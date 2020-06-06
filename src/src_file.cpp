#include "src_file.h"
#include "ctx/global_context.h"
#include "bc/emit_bitcode.h"

#include "colors.h"

static uint32_t get_column_number(ctx::char_pos const file_begin, ctx::char_pos const pivot)
{
	if (pivot == file_begin)
	{
		return 1;
	}

	auto it = pivot.data();
	do
	{
		if (pivot == file_begin)
		{
		}

		--it;
	} while (it != file_begin.data() && *it != '\n');
	return bz::u8string_view(it, pivot.data()).length();
}

constexpr size_t tab_size = 4;

static bz::u8string get_highlighted_error_or_warning(
	ctx::char_pos const file_begin,
	ctx::char_pos const file_end,
	ctx::error const &err
)
{
	bz_assert(file_begin.data() != nullptr);
	bz_assert(file_end.data() != nullptr);

	auto const begin_pos = err.src_begin;
	auto const pivot_pos = err.src_pivot;
	auto const end_pos   = err.src_end;

	if (begin_pos == end_pos)
	{
		return "";
	}

	bz_assert(begin_pos.data() != nullptr);
	bz_assert(pivot_pos.data() != nullptr);
	bz_assert(end_pos.data() != nullptr);
	bz_assert(begin_pos <= pivot_pos);
	bz_assert(pivot_pos < end_pos);

	auto const highlight_color = err.is_warning ? colors::warning_color : colors::error_color;

	auto const line_begin = [&]() {
		auto begin_ptr = begin_pos.data();
		while (begin_ptr != file_begin.data() && *(begin_ptr - 1) != '\n')
		{
			--begin_ptr;
		}
		return ctx::char_pos(begin_ptr);
	}();

	auto const line_end = [&]() {
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
				uint8_t spaces[tab_size];
				uint8_t tildes[tab_size];
				std::memset(spaces, ' ', sizeof spaces);
				std::memset(tildes, '~', sizeof tildes);
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
				auto const c = it == pivot_pos
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
		result += bz::format("{:{}} | {}\n", max_line_num_width, "", highlight_line);
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
	bz_assert(file_begin.data() != nullptr);
	bz_assert(file_end.data() != nullptr);

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
		auto const max_pos = [&]() {
			if (end_pos.data() == nullptr)
			{
				if (second_str_pos.data() == nullptr)
				{
					return std::max(first_str_pos, first_erase_end_pos);
				}
				else
				{
					return std::max(second_str_pos, second_erase_end_pos);
				}
			}
			else if (first_str_pos.data() == nullptr)
			{
				return end_pos;
			}
			else if (second_str_pos.data() == nullptr)
			{
				return std::max(end_pos, std::max(first_str_pos, first_erase_end_pos));
			}
			else
			{
				return std::max(end_pos, std::max(second_str_pos, second_erase_end_pos));
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
				uint8_t spaces[tab_size];
				uint8_t tildes[tab_size];
				std::memset(spaces, ' ', sizeof spaces);
				std::memset(tildes, '~', sizeof tildes);
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
				auto const c = it == pivot_pos
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
			}
			else if (it == second_erase_begin_pos)
			{
				bz_assert(!is_in_highlight);
				it = second_erase_end_pos;
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
		result += bz::format("{:{}} | {}\n", max_line_num_width, "", highlight_line);
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
				uint8_t spaces[tab_size];
				std::memset(spaces, ' ', sizeof spaces);
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
					"{}{:~{}}{}",
					colors::suggestion_color, second_str_len, "", colors::clear
				);
				column += second_str_len;
			}

			if (it == first_erase_begin_pos)
			{
				it = first_erase_end_pos;
			}
			else if (it == second_erase_begin_pos)
			{
				it = second_erase_end_pos;
			}

			if (it == line_end || *it.data() == '\n')
			{
				break;
			}

			put_char(it);
		}

		result += bz::format("{:{}} | {}\n", max_line_num_width, line, file_line);
		result += bz::format("{:{}} | {}\n", max_line_num_width, "", highlight_line);
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

static bz::u8string read_text_from_file(std::ifstream &file)
{
	std::string file_content{
		std::istreambuf_iterator<char>(file),
		std::istreambuf_iterator<char>()
	};
	auto const file_content_view = bz::u8string_view(
		file_content.data(),
		file_content.data() + file_content.size()
	);

	bz::u8string file_str;
	file_str.reserve(file_content_view.size());
	for (auto it = file_content_view.begin(); it != file_content_view.end();  ++it)
	{
		// we use '\n' for line endings and not '\r\n' or '\r'
		if (*it == '\r')
		{
			file_str += '\n';
			if (it + 1 != file_content_view.end() && *(it + 1) == '\n')
			{
				++it;
			}
		}
		else
		{
			file_str += *it;
		}
	}

	bz_assert(file_str.verify());
	return file_str;
}


src_file::src_file(bz::u8string_view file_name, ctx::global_context &global_ctx)
	: _stage(constructed), _file_name(file_name), _file(), _tokens(), _global_ctx(global_ctx)
{}

static void print_error_or_warning(ctx::char_pos file_begin, ctx::char_pos file_end, ctx::error const &err)
{
	auto const error_or_warning = err.is_warning
		? bz::format("{}warning:{}", colors::warning_color, colors::clear)
		: bz::format("{}error:{}", colors::error_color, colors::clear);

	bz::printf(
		"{}{}:{}:{}:{} {} {}\n{}",
		colors::bright_white,
		err.file, err.line, get_column_number(file_begin, err.src_pivot),
		colors::clear,
		error_or_warning,
		err.message,
		get_highlighted_error_or_warning(
			file_begin, file_end, err
		)
	);
	for (auto &n : err.notes)
	{
		auto const column = n.src_pivot.data() == nullptr
			? get_column_number(file_begin, n.first_suggestion.suggestion_pos)
			: get_column_number(file_begin, n.src_pivot);
		bz::printf(
			"{}{}:{}:{}:{} {}note:{} {}\n{}",
			colors::bright_white,
			n.file, n.line, column,
			colors::clear,
			colors::note_color, colors::clear,
			n.message,
			get_highlighted_note(file_begin, file_end, n)
		);
	}
	for (auto &s : err.suggestions)
	{
		auto const report_pos = s.first_suggestion.suggestion_pos;
		auto const report_pos_erase_begin = s.first_suggestion.erase_begin;
		auto const report_pos_erase_end = s.first_suggestion.erase_end;
		auto const column = get_column_number(file_begin, report_pos);
		auto const actual_column = report_pos <= report_pos_erase_begin
			? column
			: column - bz::u8string_view(report_pos_erase_begin, report_pos_erase_end).length();

		bz::printf(
			"{}{}:{}:{}:{} {}suggestion:{} {}\n{}",
			colors::bright_white,
			s.file, s.line, actual_column,
			colors::clear,
			colors::suggestion_color, colors::clear,
			s.message,
			get_highlighted_suggestion(file_begin, file_end, s)
		);
	}
}

void src_file::report_and_clear_errors_and_warnings(void)
{
	if (this->_global_ctx.has_errors_or_warnings())
	{
		for (auto &err : this->_global_ctx.get_errors_and_warnings())
		{
			print_error_or_warning(this->_file.begin(), this->_file.end(), err);
		}
		this->_global_ctx.clear_errors_and_warnings();
	}
}

[[nodiscard]] bool src_file::read_file(void)
{
	bz_assert(this->_stage == constructed);
	auto file_name = this->_file_name;
	file_name += '\0';
	std::ifstream file(reinterpret_cast<char const *>(file_name.data()));

	if (!file.good())
	{
		return false;
	}

	this->_file = read_text_from_file(file);
	this->_stage = file_read;
	return true;
}

[[nodiscard]] bool src_file::tokenize(void)
{
	bz_assert(this->_stage == file_read);

	ctx::lex_context context(this->_global_ctx);
	this->_tokens = lex::get_tokens(this->_file, this->_file_name, context);
	this->_stage = tokenized;

	return !this->_global_ctx.has_errors();
}

[[nodiscard]] bool src_file::first_pass_parse(void)
{
	bz_assert(this->_stage == tokenized);
	bz_assert(this->_tokens.size() != 0);
	bz_assert(this->_tokens.back().kind == lex::token::eof);
	auto stream = this->_tokens.cbegin();
	auto end    = this->_tokens.cend() - 1;

	ctx::first_pass_parse_context context(this->_global_ctx);

	while (stream != end)
	{
		this->_declarations.emplace_back(parse_declaration(stream, end, context));
	}

	for (auto &decl : this->_declarations)
	{
		this->_global_ctx.add_export_declaration(decl);
	}

	this->_stage = first_pass_parsed;
	return !this->_global_ctx.has_errors();
}

[[nodiscard]] bool src_file::resolve(void)
{
	bz_assert(this->_stage == first_pass_parsed);

	ctx::parse_context context(this->_global_ctx);

	for (auto &decl : this->_declarations)
	{
		::resolve(decl, context);
	}

	this->_stage = resolved;
	return !this->_global_ctx.has_errors();
}

/*
[[nodiscard]] bool src_file::emit_bitcode(ctx::bitcode_context &context)
{
	// add the declarations to the module
	for (auto &decl : this->_declarations)
	{
		switch (decl.kind())
		{
		case ast::declaration::index<ast::decl_variable>:
			bz_assert(false);
			break;
		case ast::declaration::index<ast::decl_function>:
		{
			auto &func_decl = *decl.get<ast::decl_function_ptr>();
			bc::add_function_to_module(func_decl.body, func_decl.identifier->value, context);
			break;
		}
		case ast::declaration::index<ast::decl_operator>:
		{
			auto &op_decl = *decl.get<ast::decl_operator_ptr>();
			bc::add_function_to_module(op_decl.body, bz::format("__operator_{}", op_decl.op->kind), context);
			break;
		}
		case ast::declaration::index<ast::decl_struct>:
			bz_assert(false);
			break;
		default:
			bz_assert(false);
			break;
		}
	}
	// add the definitions to the module
	for (auto &decl : this->_declarations)
	{
		switch (decl.kind())
		{
		case ast::declaration::index<ast::decl_variable>:
			bz_assert(false);
			break;
		case ast::declaration::index<ast::decl_function>:
		{
			auto &func_decl = *decl.get<ast::decl_function_ptr>();
			if (func_decl.body.body.has_value())
			{
				bc::emit_function_bitcode(func_decl.body, context);
			}
			break;
		}
		case ast::declaration::index<ast::decl_operator>:
		{
			auto &op_decl = *decl.get<ast::decl_operator_ptr>();
			if (op_decl.body.body.has_value())
			{
				bc::emit_function_bitcode(op_decl.body, context);
			}
			break;
		}
		case ast::declaration::index<ast::decl_struct>:
			bz_assert(false);
			break;
		default:
			bz_assert(false);
			break;
		}
	}

	return true;
}
*/
