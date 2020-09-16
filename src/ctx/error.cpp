#include "error.h"
#include "colors.h"
#include "global_context.h"
#include "global_data.h"

namespace ctx
{

static uint32_t get_column_number(ctx::char_pos const file_begin, ctx::char_pos const pivot)
{
	if (pivot == file_begin)
	{
		return 1;
	}

	auto it = pivot.data();
	do
	{
		--it;
	} while (it != file_begin.data() && *it != '\n');
	auto const len = bz::u8string_view(it, pivot.data()).length();
	return static_cast<uint32_t>(it == file_begin.data() ? len + 1 : len);
}

static bz::u8string get_highlighted_error_or_warning(
	ctx::char_pos const file_begin,
	ctx::char_pos const file_end,
	ctx::error const &err
)
{
	auto const begin_pos = err.src_begin;
	auto const pivot_pos = err.src_pivot;
	auto const end_pos   = err.src_end;

	if (begin_pos == end_pos)
	{
		return "";
	}

	bz_assert(file_begin.data() != nullptr);
	bz_assert(file_end.data() != nullptr);

	bz_assert(begin_pos.data() != nullptr);
	bz_assert(pivot_pos.data() != nullptr);
	bz_assert(end_pos.data() != nullptr);
	bz_assert(begin_pos <= pivot_pos);
	bz_assert(pivot_pos < end_pos);

	auto const highlight_color = err.is_error() ? colors::error_color : colors::warning_color;

	auto const line_begin = [&]() {
		auto begin_ptr = begin_pos.data();
		while (begin_ptr != file_begin.data() && *(begin_ptr - 1) != '\n')
		{
			--begin_ptr;
		}
		return ctx::char_pos(begin_ptr);
	}();

	auto const line_end = [&]() {
		// the minus one is needed, because end_pos is a one-past-the-end iterator,
		// which means if the end of the range is a newline character, the iterator
		// will point to the first character on the next line
		// the minus one will make sure we don't highlight that line
		auto end_ptr = end_pos.data() - 1;
		while (end_ptr != file_end.data() && *end_ptr != '\n')
		{
			++end_ptr;
		}
		return ctx::char_pos(end_ptr);
	}();

	auto const start_line_num = [&]() {
		auto line = err.line;
		auto it_ptr = pivot_pos.data();
		while (it_ptr != line_begin.data())
		{
			--it_ptr;
			if (*it_ptr == '\n')
			{
				--line;
			}
		}
		return line;
	}();

	auto const max_line_num_width = [&]() {
		auto line = err.line;
		auto it_ptr = pivot_pos.data();
		while (it_ptr != line_end.data())
		{
			if (*it_ptr == '\n')
			{
				++line;
			}
			++it_ptr;
		}
		return bz::internal::lg_uint(line);
	}();

	bz::u8string result{};

	auto put_line = [
		&result,
		line_end,
		begin_pos, pivot_pos, end_pos,
		highlight_color,
		max_line_num_width,
		is_in_highlight = false,
		line = start_line_num
	](ctx::char_pos start_it) mutable {
		size_t column = 0;
		bz::u8string file_line      = is_in_highlight ? highlight_color : "";
		bz::u8string highlight_line = is_in_highlight ? highlight_color : "";

		auto const find_next_stop = [
			line_end,
			begin_pos, pivot_pos, end_pos
		](ctx::char_pos it) {
			auto it_ptr = it.data();
			while (
				it_ptr != line_end.data()
				&& it_ptr != begin_pos.data()
				&& it_ptr != pivot_pos.data()
				&& it_ptr != end_pos.data()
				&& *it_ptr != '\n'
				&& *it_ptr != '\t'
			)
			{
				++it_ptr;
			}
			return ctx::char_pos(it_ptr);
		};

		auto const put_char = [
			&column,
			&file_line, &highlight_line,
			&is_in_highlight,
			pivot_pos
		](ctx::char_pos &it) {
			if (*it.data() == '\t')
			{
				auto const spaces_unique = std::make_unique<uint8_t[]>(tab_size);
				auto const tildes_unique = std::make_unique<uint8_t[]>(tab_size);
				auto const spaces = spaces_unique.get();
				auto const tildes = tildes_unique.get();
				std::memset(spaces, ' ', tab_size);
				std::memset(tildes, '~', tab_size);
				if (it == pivot_pos)
				{
					bz_assert(is_in_highlight);
					tildes[0] = '^';
				}
				auto const char_count = tab_size - column % tab_size;

				file_line += bz::u8string_view(spaces, spaces + char_count);
				auto const highlight_line_tab = is_in_highlight ? tildes : spaces;
				highlight_line += bz::u8string_view(highlight_line_tab, highlight_line_tab + char_count);
				++it;
				column += char_count;
			}
			else
			{
				file_line += *it;
				bz::u8char const c = it == pivot_pos
					? '^'
					: (is_in_highlight ? '~' : ' ');
				highlight_line += c;
				++it;
				++column;
			}
		};

		auto it = start_it;
		while (true)
		{
			auto const stop = find_next_stop(it);
			auto const str = bz::u8string_view(it, stop);
			auto const str_len = str.length();

			file_line += str;
			highlight_line += bz::u8string(str_len, is_in_highlight ? '~' : ' ');
			column += str_len;
			it = stop;

			if (it == pivot_pos)
			{
				if (it == begin_pos)
				{
					is_in_highlight = true;
					file_line += highlight_color;
					highlight_line += highlight_color;
				}
				if (*it.data() == '\n')
				{
					highlight_line += '^';
					break;
				}
			}
			else if (it == begin_pos)
			{
				is_in_highlight = true;
				file_line += highlight_color;
				highlight_line += highlight_color;
			}
			else if (it == end_pos)
			{
				is_in_highlight = false;
				file_line += colors::clear;
				highlight_line += colors::clear;
			}

			if (it == line_end || *it.data() == '\n')
			{
				break;
			}

			put_char(it);
		}

		if (is_in_highlight)
		{
			file_line += colors::clear;
			highlight_line += colors::clear;
		}

		result += bz::format("{:{}} | {}\n", max_line_num_width, line, file_line);
		if (highlight_line.contains_any("^~"))
		{
			result += bz::format("{:{}} | {}\n", max_line_num_width, "", highlight_line);
		}
		++line;

		return it;
	};

	auto it = line_begin;
	while (true)
	{
		it = put_line(it);
		if (it == line_end)
		{
			break;
		}
		else
		{
			bz_assert(*it.data() == '\n');
			++it;
		}
	}

	return result;
}

static bz::u8string get_highlighted_note(
	ctx::char_pos const file_begin,
	ctx::char_pos const file_end,
	ctx::note const &note
)
{
	auto const begin_pos = note.src_begin;
	auto const pivot_pos = note.src_pivot;
	auto const end_pos   = note.src_end;

	auto const first_str_pos         = note.first_suggestion.suggestion_pos;
	auto const first_erase_begin_pos = note.first_suggestion.erase_begin;
	auto const first_erase_end_pos   = note.first_suggestion.erase_end;

	auto const second_str_pos         = note.second_suggestion.suggestion_pos;
	auto const second_erase_begin_pos = note.second_suggestion.erase_begin;
	auto const second_erase_end_pos   = note.second_suggestion.erase_end;

	auto const first_str  = note.first_suggestion.suggestion_str;
	auto const second_str = note.second_suggestion.suggestion_str;

	if (begin_pos == end_pos && first_str_pos.data() == nullptr)
	{
		return "";
	}

	bz_assert(file_begin.data() != nullptr);
	bz_assert(file_end.data() != nullptr);

	// either there's a note range or not
	bz_assert(
		(
			begin_pos.data() != nullptr
			&& pivot_pos.data() != nullptr
			&& end_pos.data()   != nullptr
			&& begin_pos <= pivot_pos
			&& pivot_pos < end_pos
		)
		|| (
			begin_pos.data() == nullptr
			&& pivot_pos.data() == nullptr
			&& end_pos.data()   == nullptr
		)
	);

	// both erase ranges have to be valid
	bz_assert(
		// the range is not nothing, so it has to be not empty (begin < end)
		(
			first_erase_begin_pos.data() != nullptr
			&& first_erase_end_pos.data() != nullptr
			&& first_erase_begin_pos < first_erase_end_pos
		)
		// the range is nothing (empty)
		|| (
			first_erase_begin_pos.data() == nullptr
			&& first_erase_end_pos.data() == nullptr
		)
	);
	bz_assert(
		// the range is not nothing, so it has to be not empty (begin < end)
		(
			second_erase_begin_pos.data() != nullptr
			&& second_erase_end_pos.data() != nullptr
			&& second_erase_begin_pos < second_erase_end_pos
		)
		// the range is nothing (empty)
		|| (
			second_erase_begin_pos.data() == nullptr
			&& second_erase_end_pos.data() == nullptr
		)
	);

	// if there's an erase range, there has to be a suggestion string as well
	bz_assert(first_erase_begin_pos.data() == nullptr || first_str_pos.data() != nullptr);
	bz_assert(second_erase_begin_pos.data() == nullptr || second_str_pos.data() != nullptr);

	// str_pos can't be in the erase range
	bz_assert(
		(first_str_pos <= first_erase_begin_pos && first_erase_begin_pos <= first_erase_end_pos)
		|| (first_str_pos > first_erase_end_pos && first_erase_begin_pos <= first_erase_end_pos)
	);
	bz_assert(
		(second_str_pos <= second_erase_begin_pos && second_erase_begin_pos <= second_erase_end_pos)
		|| (second_str_pos > second_erase_end_pos && second_erase_begin_pos <= second_erase_end_pos)
	);

	// first suggestion must be before the second suggestion
	bz_assert(second_str_pos.data() == nullptr || first_str_pos < second_str_pos);

	// if str_pos is not null, the suggestion string has to be not empty
	bz_assert(first_str_pos.data()  == nullptr || first_str  != "");
	bz_assert(second_str_pos.data() == nullptr || second_str != "");

	// suggestion strings shouldn't contain '\n' or '\t'
	bz_assert(first_str.find('\n')  == first_str.end());
	bz_assert(first_str.find('\t')  == first_str.end());
	bz_assert(second_str.find('\n') == second_str.end());
	bz_assert(second_str.find('\t') == second_str.end());

	// if there's no note range, there has to be at least one suggestion
	bz_assert(begin_pos.data() != nullptr || first_str_pos.data() != nullptr);

	auto const line_begin = [&]() {
		auto const min_pos = [&]() {
			if (begin_pos.data() == nullptr)
			{
				if (first_erase_begin_pos.data() == nullptr)
				{
					return first_str_pos;
				}
				else
				{
					return std::min(first_str_pos, first_erase_begin_pos);
				}
			}
			else if (first_str_pos.data() == nullptr)
			{
				return begin_pos;
			}
			else
			{
				if (first_erase_begin_pos.data() == nullptr)
				{
					return std::min(begin_pos, first_str_pos);
				}
				else
				{
					return std::min(begin_pos, std::min(first_str_pos, first_erase_begin_pos));
				}
			}
		}();

		auto begin_ptr = min_pos.data();
		while (begin_ptr != file_begin.data() && *(begin_ptr - 1) != '\n')
		{
			--begin_ptr;
		}
		return ctx::char_pos(begin_ptr);
	}();
	bz_assert(line_begin.data() != nullptr);

	auto const line_end = [&]() {
		// one-past-the-end iterators are mixed with regular iterators here,
		// which means we should have end_pos.data() - 1 for one-past-the-end,
		// and first_str_pos.data() for regular iterators
		// therefore max_pos is calculated to be a pointer and not a ctx::char_pos
		// for a more detailed explanation regarding the -1, see
		// get_highlighted_error_or_warning's line_end calculation
		auto const max_pos = [&]() {
			auto const minus_one = [](ctx::char_pos it) -> decltype(it.data()) {
				return it.data() == nullptr ? nullptr : it.data() - 1;
			};
			if (end_pos.data() == nullptr)
			{
				if (second_str_pos.data() == nullptr)
				{
					return std::max(first_str_pos.data(), minus_one(first_erase_end_pos));
				}
				else
				{
					return std::max(second_str_pos.data(), minus_one(second_erase_end_pos));
				}
			}
			else if (first_str_pos.data() == nullptr)
			{
				bz_assert(end_pos.data() != nullptr);
				return end_pos.data() - 1;
			}
			else if (second_str_pos.data() == nullptr)
			{
				return std::max(minus_one(end_pos), std::max(first_str_pos.data(), minus_one(first_erase_end_pos)));
			}
			else
			{
				return std::max(minus_one(end_pos), std::max(second_str_pos.data(), minus_one(second_erase_end_pos)));
			}
		}();

		auto end_ptr = max_pos;
		while (end_ptr != file_end.data() && *end_ptr != '\n')
		{
			++end_ptr;
		}
		return ctx::char_pos(end_ptr);
	}();
	bz_assert(line_end.data() != nullptr);

	auto const start_line_num = [&]() {
		auto line = note.line;
		auto it_ptr = (pivot_pos.data() == nullptr ? first_str_pos : pivot_pos).data();
		while (it_ptr != line_begin.data())
		{
			--it_ptr;
			if (*it_ptr == '\n')
			{
				--line;
			}
		}
		return line;
	}();

	auto const max_line_num_width = [&]() {
		auto line = note.line;
		auto it_ptr = (pivot_pos.data() == nullptr ? first_str_pos : pivot_pos).data();
		while (it_ptr != line_end.data())
		{
			if (*it_ptr == '\n')
			{
				++line;
			}
			++it_ptr;
		}
		return bz::internal::lg_uint(line);
	}();

	bz::u8string result{};

	auto put_line = [
		&result,
		line_end,
		begin_pos, pivot_pos, end_pos,
		first_str_pos, first_erase_begin_pos, first_erase_end_pos,
		second_str_pos, second_erase_begin_pos, second_erase_end_pos,
		first_str, second_str,
		max_line_num_width,
		is_in_highlight = false,
		line = start_line_num
	](ctx::char_pos start_it) mutable {
		size_t column = 0;
		bz::u8string file_line      = is_in_highlight ? colors::note_color : "";
		bz::u8string highlight_line = is_in_highlight ? colors::note_color : "";

		auto const find_next_stop = [
			line_end,
			begin_pos, pivot_pos, end_pos,
			first_str_pos, first_erase_begin_pos,
			second_str_pos, second_erase_begin_pos
		](ctx::char_pos it) {
			auto it_ptr = it.data();
			while (
				it_ptr != line_end.data()
				&& it_ptr != begin_pos.data()
				&& it_ptr != pivot_pos.data()
				&& it_ptr != end_pos.data()
				&& it_ptr != first_str_pos.data()
				&& it_ptr != first_erase_begin_pos.data()
				&& it_ptr != second_str_pos.data()
				&& it_ptr != second_erase_begin_pos.data()
				&& *it_ptr != '\n'
				&& *it_ptr != '\t'
			)
			{
				++it_ptr;
			}
			return ctx::char_pos(it_ptr);
		};

		auto const put_char = [
			&column,
			&file_line, &highlight_line,
			&is_in_highlight,
			pivot_pos
		](ctx::char_pos &it) {
			if (*it.data() == '\t')
			{
				auto const spaces_unique = std::make_unique<uint8_t[]>(tab_size);
				auto const tildes_unique = std::make_unique<uint8_t[]>(tab_size);
				auto const spaces = spaces_unique.get();
				auto const tildes = tildes_unique.get();
				std::memset(spaces, ' ', tab_size);
				std::memset(tildes, '~', tab_size);
				if (it == pivot_pos)
				{
					bz_assert(is_in_highlight);
					tildes[0] = '^';
				}
				auto const char_count = tab_size - column % tab_size;

				file_line += bz::u8string_view(spaces, spaces + char_count);
				auto const highlight_line_tab = is_in_highlight ? tildes : spaces;
				highlight_line += bz::u8string_view(highlight_line_tab, highlight_line_tab + char_count);
				++it;
				column += char_count;
			}
			else
			{
				file_line += *it;
				bz::u8char const c = it == pivot_pos
					? '^'
					: (is_in_highlight ? '~' : ' ');
				highlight_line += c;
				++it;
				++column;
			}
		};

		auto it = start_it;
		while (true)
		{
			auto const stop = find_next_stop(it);
			auto const str = bz::u8string_view(it, stop);
			auto const str_len = str.length();

			file_line += str;
			highlight_line += bz::u8string(str_len, is_in_highlight ? '~' : ' ');
			column += str_len;
			it = stop;

			if (it == end_pos)
			{
				file_line += colors::clear;
				highlight_line += colors::clear;
				is_in_highlight = false;
			}

			if (it == first_str_pos)
			{
				bz_assert(!is_in_highlight);
				auto const first_str_len = first_str.length();
				file_line += bz::format(
					"{}{}{}",
					colors::suggestion_color, first_str, colors::clear
				);
				highlight_line += bz::format(
					"{}{:~<{}}{}",
					colors::suggestion_color, first_str_len, "", colors::clear
				);
				column += first_str_len;
			}
			else if (it == second_str_pos)
			{
				bz_assert(!is_in_highlight);
				auto const second_str_len = second_str.length();
				file_line += bz::format(
					"{}{}{}",
					colors::suggestion_color, second_str, colors::clear
				);
				highlight_line += bz::format(
					"{}{:~<{}}{}",
					colors::suggestion_color, second_str_len, "", colors::clear
				);
				column += second_str_len;
			}

			if (it == first_erase_begin_pos)
			{
				bz_assert(!is_in_highlight);
				it = first_erase_end_pos;
				auto const erased_str = bz::u8string_view(first_erase_begin_pos, first_erase_end_pos);
				auto const erase_length = erased_str.length();
				file_line += bz::format("{}{:-<{}}{}", colors::bright_red, erase_length, "", colors::clear);
				highlight_line += bz::format("{:{}}", erase_length, "");
				column += erase_length;
			}
			else if (it == second_erase_begin_pos)
			{
				bz_assert(!is_in_highlight);
				it = second_erase_end_pos;
				auto const erased_str = bz::u8string_view(second_erase_begin_pos, second_erase_end_pos);
				auto const erase_length = erased_str.length();
				file_line += bz::format("{}{:-<{}}{}", colors::bright_red, erase_length, "", colors::clear);
				highlight_line += bz::format("{:{}}", erase_length, "");
				column += erase_length;
			}

			if (it == pivot_pos)
			{
				if (it == begin_pos)
				{
					is_in_highlight = true;
					file_line += colors::note_color;
					highlight_line += colors::note_color;
				}
				if (*it.data() == '\n')
				{
					highlight_line += '^';
					break;
				}
			}
			else if (it == begin_pos)
			{
				is_in_highlight = true;
				file_line += colors::note_color;
				highlight_line += colors::note_color;
			}

			if (it == line_end || *it.data() == '\n')
			{
				break;
			}

			put_char(it);
		}

		if (is_in_highlight)
		{
			file_line += colors::clear;
			highlight_line += colors::clear;
		}

		result += bz::format("{:{}} | {}\n", max_line_num_width, line, file_line);
		if (highlight_line.contains_any("^~"))
		{
			result += bz::format("{:{}} | {}\n", max_line_num_width, "", highlight_line);
		}
		++line;

		return it;
	};

	auto it = line_begin;
	while (true)
	{
		it = put_line(it);
		if (it == line_end)
		{
			break;
		}
		else
		{
			bz_assert(*it.data() == '\n');
			++it;
		}
	}

	return result;
}

static bz::u8string get_highlighted_suggestion(
	ctx::char_pos const file_begin,
	ctx::char_pos const file_end,
	ctx::suggestion const &suggestion
)
{
	bz_assert(file_begin.data() != nullptr);
	bz_assert(file_end.data() != nullptr);

	auto const first_str_pos         = suggestion.first_suggestion.suggestion_pos;
	auto const first_erase_begin_pos = suggestion.first_suggestion.erase_begin;
	auto const first_erase_end_pos   = suggestion.first_suggestion.erase_end;

	auto const second_str_pos         = suggestion.second_suggestion.suggestion_pos;
	auto const second_erase_begin_pos = suggestion.second_suggestion.erase_begin;
	auto const second_erase_end_pos   = suggestion.second_suggestion.erase_end;

	auto const first_str  = suggestion.first_suggestion.suggestion_str;
	auto const second_str = suggestion.second_suggestion.suggestion_str;

	// there has to be at least one suggestion
	bz_assert(first_str_pos.data() != nullptr);

	// both erase ranges have to be valid
	bz_assert(
		// the range is not nothing, so it has to be not empty (begin < end)
		(
			first_erase_begin_pos.data() != nullptr
			&& first_erase_end_pos.data() != nullptr
			&& first_erase_begin_pos < first_erase_end_pos
		)
		// the range is nothing (empty)
		|| (
			first_erase_begin_pos.data() == nullptr
			&& first_erase_end_pos.data() == nullptr
		)
	);
	bz_assert(
		// the range is not nothing, so it has to be not empty (begin < end)
		(
			second_erase_begin_pos.data() != nullptr
			&& second_erase_end_pos.data() != nullptr
			&& second_erase_begin_pos < second_erase_end_pos
		)
		// the range is nothing (empty)
		|| (
			second_erase_begin_pos.data() == nullptr
			&& second_erase_end_pos.data() == nullptr
		)
	);

	// str_pos can't be in the erase range
	bz_assert(
		(first_str_pos <= first_erase_begin_pos && first_erase_begin_pos <= first_erase_end_pos)
		|| (first_str_pos > first_erase_end_pos && first_erase_begin_pos <= first_erase_end_pos)
	);
	bz_assert(
		(second_str_pos <= second_erase_begin_pos && second_erase_begin_pos <= second_erase_end_pos)
		|| (second_str_pos > second_erase_end_pos && second_erase_begin_pos <= second_erase_end_pos)
	);

	// if there's a second suggestion it must have a suggestion position and not just an erase
	bz_assert(second_erase_begin_pos.data() == nullptr || first_str_pos.data() != nullptr);

	// first suggestion must be before the second suggestion
	bz_assert(second_str_pos.data() == nullptr || first_str_pos < second_str_pos);

	// if str_pos is not null, the suggestion string has to be not empty
	bz_assert(first_str != "");
	bz_assert(second_str_pos.data() == nullptr || second_str != "");

	// suggestion strings shouldn't contain '\n' or '\t'
	bz_assert(first_str.find('\n')  == first_str.end());
	bz_assert(first_str.find('\t')  == first_str.end());
	bz_assert(second_str.find('\n') == second_str.end());
	bz_assert(second_str.find('\t') == second_str.end());

	auto const line_begin = [&]() {
		auto const min_pos = [&]() {
			if (first_erase_begin_pos.data() == nullptr)
			{
				return first_str_pos;
			}
			else
			{
				return std::min(first_str_pos, first_erase_begin_pos);
			}
		}();

		auto begin_ptr = min_pos.data();
		while (begin_ptr != file_begin.data() && *(begin_ptr - 1) != '\n')
		{
			--begin_ptr;
		}
		return ctx::char_pos(begin_ptr);
	}();
	bz_assert(line_begin.data() != nullptr);

	auto const line_end = [&]() {
		auto const max_pos = [&]() {
			if (second_str_pos.data() == nullptr)
			{
				return std::max(first_str_pos, first_erase_end_pos);
			}
			else
			{
				return std::max(second_str_pos, second_erase_end_pos);
			}
		}();

		auto end_ptr = max_pos.data() - 1;
		while (end_ptr != file_end.data() && *end_ptr != '\n')
		{
			++end_ptr;
		}
		return ctx::char_pos(end_ptr);
	}();
	bz_assert(line_end.data() != nullptr);

	auto const start_line_num = [&]() {
		auto line = suggestion.line;
		auto it_ptr = first_str_pos.data();
		while (it_ptr != line_begin.data())
		{
			--it_ptr;
			if (*it_ptr == '\n')
			{
				--line;
			}
		}
		return line;
	}();

	auto const max_line_num_width = [&]() {
		auto line = suggestion.line;
		auto it_ptr = first_str_pos.data();
		while (it_ptr != line_end.data())
		{
			if (*it_ptr == '\n')
			{
				++line;
			}
			++it_ptr;
		}
		return bz::internal::lg_uint(line);
	}();

	bz::u8string result{};

	auto put_line = [
		&result,
		line_end,
		first_str_pos, first_erase_begin_pos, first_erase_end_pos,
		second_str_pos, second_erase_begin_pos, second_erase_end_pos,
		first_str, second_str,
		max_line_num_width,
		line = start_line_num
	](ctx::char_pos start_it) mutable {
		size_t column = 0;
		bz::u8string file_line{};
		bz::u8string highlight_line{};

		auto const find_next_stop = [
			line_end,
			first_str_pos, first_erase_begin_pos,
			second_str_pos, second_erase_begin_pos
		](ctx::char_pos it) {
			auto it_ptr = it.data();
			while (
				it_ptr != line_end.data()
				&& it_ptr != first_str_pos.data()
				&& it_ptr != first_erase_begin_pos.data()
				&& it_ptr != second_str_pos.data()
				&& it_ptr != second_erase_begin_pos.data()
				&& *it_ptr != '\n'
				&& *it_ptr != '\t'
			)
			{
				++it_ptr;
			}
			return ctx::char_pos(it_ptr);
		};

		auto const put_char = [
			&column,
			&file_line, &highlight_line
		](ctx::char_pos &it) {
			if (*it.data() == '\t')
			{
				auto const spaces_unique = std::make_unique<uint8_t[]>(tab_size);
				auto const spaces = spaces_unique.get();
				std::memset(spaces, ' ', tab_size);
				auto const char_count = tab_size - column % tab_size;

				auto const tab_str = bz::u8string_view(spaces, spaces + char_count);
				file_line += tab_str;
				highlight_line += tab_str;
				++it;
				column += char_count;
			}
			else
			{
				file_line += *it;
				highlight_line += ' ';
				++it;
				++column;
			}
		};

		auto it = start_it;
		while (true)
		{
			auto const stop = find_next_stop(it);
			auto const str = bz::u8string_view(it, stop);
			auto const str_len = str.length();

			file_line += str;
			highlight_line += bz::u8string(str_len, ' ');
			column += str_len;
			it = stop;

			if (it == first_str_pos)
			{
				auto const first_str_len = first_str.length();
				file_line += bz::format(
					"{}{}{}",
					colors::suggestion_color, first_str, colors::clear
				);
				highlight_line += bz::format(
					"{}{:~<{}}{}",
					colors::suggestion_color, first_str_len, "", colors::clear
				);
				column += first_str_len;
			}
			else if (it == second_str_pos)
			{
				auto const second_str_len = second_str.length();
				file_line += bz::format(
					"{}{}{}",
					colors::suggestion_color, second_str, colors::clear
				);
				highlight_line += bz::format(
					"{}{:~<{}}{}",
					colors::suggestion_color, second_str_len, "", colors::clear
				);
				column += second_str_len;
			}

			if (it == first_erase_begin_pos)
			{
				it = first_erase_end_pos;
				auto const erased_str = bz::u8string_view(first_erase_begin_pos, first_erase_end_pos);
				auto const erase_length = erased_str.length();
				file_line += bz::format("{}{:-<{}}{}", colors::bright_red, erase_length, "", colors::clear);
				highlight_line += bz::format("{:{}}", erase_length, "");
				column += erase_length;
			}
			else if (it == second_erase_begin_pos)
			{
				it = second_erase_end_pos;
				auto const erased_str = bz::u8string_view(second_erase_begin_pos, second_erase_end_pos);
				auto const erase_length = erased_str.length();
				file_line += bz::format("{}{:-<{}}{}", colors::bright_red, erase_length, "", colors::clear);
				highlight_line += bz::format("{:{}}", erase_length, "");
				column += erase_length;
			}

			if (it == line_end || *it.data() == '\n')
			{
				break;
			}

			put_char(it);
		}

		result += bz::format("{:{}} | {}\n", max_line_num_width, line, file_line);
		if (highlight_line.contains_any("^~"))
		{
			result += bz::format("{:{}} | {}\n", max_line_num_width, "", highlight_line);
		}
		++line;

		return it;
	};

	auto it = line_begin;
	while (true)
	{
		it = put_line(it);
		if (it == line_end)
		{
			break;
		}
		else
		{
			bz_assert(*it.data() == '\n');
			++it;
		}
	}

	return result;
}

void print_error_or_warning(error const &err, global_context &context)
{
	auto const [err_file_begin, err_file_end] = context.get_file_begin_and_end(err.file_id);
	auto const src_pos = [&, err_file_begin = err_file_begin]() {
		if (err.file_id == global_context::compiler_file_id)
		{
			return bz::format("{}bozon:{}", colors::bright_white, colors::clear);
		}
		else if (err.src_begin == err.src_end)
		{
			return bz::format(
				"{}{}:{}:{}",
				colors::bright_white,
				context.get_file_name(err.file_id), err.line,
				colors::clear
			);
		}
		else
		{
			auto const file_name = context.get_file_name(err.file_id);
			return bz::format(
				"{}{}:{}:{}:{}",
				colors::bright_white,
				file_name, err.line, get_column_number(err_file_begin, err.src_pivot),
				colors::clear
			);
		}
	}();
	auto const error_or_warning_line = err.is_error()
		? bz::format(
			"{}error:{} {}",
			colors::error_color, colors::clear, err.message
		)
		: bz::format(
			"{}warning:{} {} {}[-W{}]{}",
			colors::warning_color, colors::clear,
			err.message,
			colors::bright_white, get_warning_name(err.kind), colors::clear
		);

	if (no_error_highlight)
	{
		bz::print(
			"{} {}\n",
			src_pos, error_or_warning_line
		);
	}
	else
	{
		bz::print(
			"{} {}\n{}",
			src_pos, error_or_warning_line,
			get_highlighted_error_or_warning(err_file_begin, err_file_end, err)
		);
	}

	for (auto &n : err.notes)
	{
		auto const [note_file_begin, note_file_end] = context.get_file_begin_and_end(n.file_id);
		auto const is_empty = n.src_begin == n.src_end
			&& n.first_suggestion.erase_begin == n.first_suggestion.erase_end
			&& n.first_suggestion.suggestion_pos.data() == nullptr;
		auto const column = [&, note_file_begin = note_file_begin]() -> size_t {
			if (is_empty)
			{
				return 0;
			}
			else
			{
				return n.src_pivot.data() == nullptr
					? get_column_number(note_file_begin, n.first_suggestion.suggestion_pos)
					: get_column_number(note_file_begin, n.src_pivot);
			}
		}();
		auto const note_src_pos = is_empty
			? bz::format(
				"{}{}:{}{}",
				colors::bright_white,
				context.get_file_name(n.file_id), n.line,
				colors::clear
			)
			: bz::format(
				"{}{}:{}:{}{}",
				colors::bright_white,
				context.get_file_name(n.file_id), n.line, column,
				colors::clear
			);

		if (no_error_highlight)
		{
			bz::print(
				"{}: {}note:{} {}\n",
				note_src_pos, colors::note_color, colors::clear,
				n.message
			);
		}
		else
		{
			bz::print(
				"{}: {}note:{} {}\n{}",
				note_src_pos, colors::note_color, colors::clear,
				n.message,
				get_highlighted_note(note_file_begin, note_file_end, n)
			);
		}
	}
	for (auto &s : err.suggestions)
	{
		auto [suggestion_file_begin, suggestion_file_end] = context.get_file_begin_and_end(s.file_id);
		auto const report_pos = s.first_suggestion.suggestion_pos;
		auto const report_pos_erase_begin = s.first_suggestion.erase_begin;
		auto const report_pos_erase_end = s.first_suggestion.erase_end;
		auto const column = get_column_number(suggestion_file_begin, report_pos);
		auto const actual_column = report_pos <= report_pos_erase_begin
			? column
			: column - bz::u8string_view(report_pos_erase_begin, report_pos_erase_end).length();

		if (no_error_highlight)
		{
			bz::print(
				"{}{}:{}:{}:{} {}suggestion:{} {}\n",
				colors::bright_white,
				context.get_file_name(s.file_id), s.line, actual_column,
				colors::clear,
				colors::suggestion_color, colors::clear,
				s.message
			);
		}
		else
		{
			bz::print(
				"{}{}:{}:{}:{} {}suggestion:{} {}\n{}",
				colors::bright_white,
				context.get_file_name(s.file_id), s.line, actual_column,
				colors::clear,
				colors::suggestion_color, colors::clear,
				s.message,
				get_highlighted_suggestion(suggestion_file_begin, suggestion_file_end, s)
			);
		}
	}
}

bz::u8string convert_string_for_message(bz::u8string_view str)
{
	bz::u8string result = "";

	auto it = str.begin();
	auto const end = str.end();

	auto begin = it;
	for (; it != end; ++it)
	{
		auto const c = *it;
		if (c < ' ')
		{
			result += bz::u8string_view(begin, it);
			switch (c)
			{
			case '\t':
				result += bz::format("{}\\t{}", colors::bright_black, colors::clear);
				break;
			case '\n':
				result += bz::format("{}\\n{}", colors::bright_black, colors::clear);
				break;
			case '\r':
				result += bz::format("{}\\r{}", colors::bright_black, colors::clear);
				break;
			default:
				result += bz::format("{}\\x{:02x}{}", colors::bright_black, c, colors::clear);
				break;
			}
			begin = it + 1;
		}
	}
	result += bz::u8string_view(begin, it);

	return result;
}

} // namespace ctx
