#include "error.h"
#include "colors.h"
#include "global_context.h"
#include "global_data.h"

static constexpr auto error_color      = colors::bright_red;
static constexpr auto warning_color    = colors::bright_magenta;
static constexpr auto note_color       = colors::bright_black;
static constexpr auto suggestion_color = colors::bright_green;
static constexpr uint32_t shown_adjacent_lines = 1;

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

static bz::u8string convert_string_for_message(bz::u8string_view str)
{
	bz::u8string result = "";

	auto it = str.begin();
	auto const end = str.end();

	auto begin = it;
	for (; it != end; ++it)
	{
		auto const c = *it;
		if (c < ' ' || c == '\u007f') // '\u007f': DEL character, which can't be printed
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

static bz::u8string get_highlighted_text(
	bz::u8string_view highlight_color,
	char_pos file_begin_it, char_pos file_end_it,
	source_highlight const &src_highlight
)
{
	auto const file_begin = file_begin_it.data();
	auto const file_end   = file_end_it.data();
	auto const src_begin = src_highlight.src_begin.data();
	auto const src_pivot = src_highlight.src_pivot.data();
	auto const src_end   = src_highlight.src_end.data();
	auto const first_erase_begin = src_highlight.first_suggestion.erase_begin.data();
	auto const first_erase_end   = src_highlight.first_suggestion.erase_end.data();
	auto const first_str_pos     = src_highlight.first_suggestion.suggestion_pos.data();
	auto const second_erase_begin = src_highlight.second_suggestion.erase_begin.data();
	auto const second_erase_end   = src_highlight.second_suggestion.erase_end.data();
	auto const second_str_pos     = src_highlight.second_suggestion.suggestion_pos.data();

	if ((src_begin == nullptr || src_begin == src_end) && first_erase_begin == nullptr && first_str_pos == nullptr)
	{
		return "";
	}

	// verify file_begin and file_end
	bz_assert(file_begin != nullptr && file_end != nullptr);
	bz_assert(file_begin < file_end);

	// verify src_begin, src_pivot and src_end
	bz_assert(
		(
			src_begin    != nullptr
			&& src_pivot != nullptr
			&& src_end   != nullptr
			&& src_begin <= src_pivot
			&& src_pivot < src_end
		)
		|| (
			src_begin    == nullptr
			&& src_pivot == nullptr
			&& src_end   == nullptr
		)
	);

	// verify first_suggestion
	bz_assert(
		(first_erase_begin == nullptr && first_erase_end == nullptr)
		|| (
			first_erase_begin != nullptr
			&& first_erase_end != nullptr
			&& first_erase_begin < first_erase_end
		)
	);

	// verify second_suggestion
	bz_assert(
		(second_erase_begin == nullptr && second_erase_end == nullptr)
		|| (
			second_erase_begin != nullptr
			&& second_erase_end != nullptr
			&& second_erase_begin < second_erase_end
		)
	);

	// verify the relation between the different ranges
	bz_assert(([&]() -> bool {
		auto const overlaps = [](
			char const *lhs_begin, char const *lhs_end,
			char const *rhs_begin, char const *rhs_end
		) {
			return !(lhs_end <= rhs_begin || rhs_end <= lhs_begin);
		};

		return (!overlaps(src_begin, src_end, first_erase_begin, first_erase_end))
			&& (!overlaps(src_begin, src_end, second_erase_begin, second_erase_end))
			&& (!overlaps(first_erase_begin, first_erase_end, second_erase_begin, second_erase_end));
	}()));

	auto const first_line_begin = [&]() {
		auto const suggestion_min =
			first_erase_begin == nullptr ? first_str_pos :
			std::min(first_erase_begin, first_str_pos);
		auto it = suggestion_min == nullptr ? src_begin :
			src_begin == nullptr ? suggestion_min :
			std::min(suggestion_min, src_begin);
		while (it != file_begin && *(it - 1) != '\n')
		{
			--it;
		}
		return it;
	}();

	auto const last_line_end = [&]() {
		auto const has_second_suggestion = second_erase_begin != nullptr || second_str_pos != nullptr;
		auto const suggestion_max = has_second_suggestion ? (
			second_erase_begin == nullptr ? second_str_pos :
			std::max(second_erase_begin, second_str_pos)
		) : (
			first_erase_begin == nullptr ? first_str_pos :
			std::max(first_erase_begin, first_str_pos)
		);
		// the minus one is needed, because if the end pointer points to a new line character,
		// we don't want to highlight the next line, because there would be nothing on it
		auto it = suggestion_max == nullptr ? src_end - 1 :
			src_end == nullptr ? suggestion_max - 1 :
			std::max(suggestion_max, src_end) - 1;
		while (it != file_end && *it != '\n')
		{
			++it;
		}
		return it;
	}();

	auto const first_line_num = [&]() {
		auto it = src_pivot == nullptr ? first_str_pos : src_pivot;
		auto line_num = src_highlight.line;
		while (it != first_line_begin)
		{
			--it;
			if (*it == '\n')
			{
				--line_num;
			}
			bz_assert(line_num != 0);
		}
		return line_num;
	}();

	auto const max_line_chars_width = [&]() {
		auto it = src_pivot == nullptr ? first_str_pos : src_pivot;
		auto line_num = src_highlight.line;
		while (it != last_line_end)
		{
			if (*it == '\n')
			{
				++line_num;
			}
			++it;
		}
		auto const result = bz::internal::lg_uint(line_num);
		return result > 4 ? result : 4;
	}();

	bz::u8string file_line;
	// may be empty, if the highlight shouldn't be shown,
	// because it's either empty, or all tildes
	bz::u8string highlight_line;
	size_t column = 0; // 0-based column number
	auto it = first_line_begin;

	auto const try_put_error = [&](char const *line_begin) {
		if (it < src_begin || it >= src_end)
		{
			return false;
		}
		file_line      += highlight_color;
		highlight_line += highlight_color;
		auto u8it = bz::u8iterator(it);
		while (u8it.data() != src_end && *u8it.data() != '\n')
		{
			auto const c = *u8it;
			if (c == '\t')
			{
				auto const tab_size = global_data::tab_size == 0 ? 4 : std::min(global_data::tab_size, size_t(1024));
				auto const chars_to_put = tab_size - column % tab_size;
				if (chars_to_put <= 16)
				{
					char spaces[16];
					char tildes[16];
					std::memset(spaces, ' ', sizeof spaces);
					std::memset(tildes, '~', sizeof tildes);
					if (u8it.data() == src_pivot)
					{
						tildes[0] = '^';
					}
					file_line      += bz::u8string_view(spaces, spaces + chars_to_put);
					highlight_line += bz::u8string_view(tildes, tildes + chars_to_put);
				}
				else
				{
					auto const spaces = std::make_unique<char[]>(chars_to_put);
					auto const tildes = std::make_unique<char[]>(chars_to_put);
					std::memset(spaces.get(), ' ', chars_to_put);
					std::memset(tildes.get(), '~', chars_to_put);
					if (u8it.data() == src_pivot)
					{
						tildes[0] = '^';
					}
					file_line      += bz::u8string_view(spaces.get(), spaces.get() + chars_to_put);
					highlight_line += bz::u8string_view(tildes.get(), tildes.get() + chars_to_put);
				}
				column += chars_to_put;
			}
			else
			{
				file_line += c;
				highlight_line += u8it.data() == src_pivot ? '^' : '~';
				++column;
			}
			++u8it;
		}
		it = u8it.data();
		auto const end = it;
		if (it == src_pivot)
		{
			highlight_line += '^';
		}
		else if (line_begin >= src_begin && end < src_end && (src_pivot < line_begin || src_pivot > end))
		{
			highlight_line.clear();
		}
		file_line += colors::clear;
		if (highlight_line.size() != 0)
		{
			highlight_line += colors::clear;
		}
		return true;
	};

	auto const try_put_first_suggestion = [&]() {
		bool ret_val = false;
		if (it == first_str_pos)
		{
			ret_val = true;
			auto const len = src_highlight.first_suggestion.suggestion_str.length();
			file_line      += suggestion_color;
			highlight_line += suggestion_color;
			file_line += src_highlight.first_suggestion.suggestion_str;
			highlight_line += bz::format("{:~<{}}", len, "");
			file_line      += colors::clear;
			highlight_line += colors::clear;
			column += len;
		}
		if (it == first_erase_begin)
		{
			ret_val = true;
			it = first_erase_end;
			if (first_erase_begin != first_str_pos)
			{
				auto const erased_str = bz::u8string_view(first_erase_begin, first_erase_end);
				auto const erased_len = erased_str.length();
				file_line      += colors::bright_red;
				file_line      += bz::format("{:-<{}}", erased_len, "");
				highlight_line += bz::format("{: <{}}", erased_len, "");
				file_line      += colors::clear;
				column += erased_len;
			}
		}
		return ret_val;
	};

	auto const try_put_second_suggestion = [&]() {
		bool ret_val = false;
		if (it == second_str_pos)
		{
			ret_val = true;
			auto const len = src_highlight.second_suggestion.suggestion_str.length();
			file_line      += suggestion_color;
			highlight_line += suggestion_color;
			file_line += src_highlight.second_suggestion.suggestion_str;
			highlight_line += bz::format("{:~<{}}", len, "");
			file_line      += colors::clear;
			highlight_line += colors::clear;
			column += len;
		}
		if (it == second_erase_begin)
		{
			ret_val = true;
			it = second_erase_end;
			if (second_erase_begin != second_str_pos)
			{
				auto const erased_str = bz::u8string_view(second_erase_begin, second_erase_end);
				auto const erased_len = erased_str.length();
				file_line      += colors::bright_red;
				file_line      += bz::format("{:-<{}}", erased_len, "");
				highlight_line += bz::format("{: <{}}", erased_len, "");
				file_line      += colors::clear;
				column += erased_len;
			}
		}
		return ret_val;
	};

	auto const find_next_stop = [&](char const *iter) {
		// iter is not to be confused with it,
		// this function does not mutate its
		while (
			iter != last_line_end
			&& *iter != '\n'
			&& *iter != '\t'
			&& (iter < src_begin || iter >= src_end)
			&& iter != first_erase_begin
			&& iter != first_str_pos
			&& iter != second_erase_begin
			&& iter != second_str_pos
		)
		{
			++iter;
		}
		return iter;
	};

	bz::u8string result;

	auto const interesting_line_numbers = [&]() {
		bz::vector<uint32_t> result = {};

		if (global_data::do_verbose)
		{
			return result;
		}

		auto line_num = first_line_num;
		for (auto const it : bz::iota(first_line_begin, last_line_end))
		{
			if (
				it == src_begin
				|| it == src_pivot
				|| it == src_end
				|| it == first_erase_begin
				|| it == first_erase_end
				|| it == first_str_pos
				|| it == second_erase_begin
				|| it == second_erase_end
				|| it == second_str_pos
			)
			{
				result.push_back(line_num);
			}

			if (*it == '\n')
			{
				line_num += 1;
			}
		}

		result.sort();
		return result;
	}();

	auto const should_skip_line = [&interesting_line_numbers](uint32_t line_num) {
		if (interesting_line_numbers.size() < 2)
		{
			return false;
		}

		auto const lb = std::lower_bound(interesting_line_numbers.begin(), interesting_line_numbers.end(), line_num);
		bz_assert(lb != interesting_line_numbers.end());
		bz_assert(*lb - line_num <= shown_adjacent_lines || lb != interesting_line_numbers.begin());
		if (*lb - line_num <= shown_adjacent_lines || line_num - *(lb - 1) <= shown_adjacent_lines)
		{
			return false;
		}
		else
		{
			// we shouldn't skip a single line, because it wouldn't reduce the error message length
			return (*lb - line_num != shown_adjacent_lines + 1) || (line_num - *(lb - 1) != shown_adjacent_lines + 1);
		}
	};

	bool skipped_lines = false;
	auto line_num = first_line_num;
	while (it != last_line_end)
	{
		if (!global_data::do_verbose && should_skip_line(line_num))
		{
			while (*it != '\n')
			{
				bz_assert(it != last_line_end);
				++it;
			}
			++it; // skip new line
			++line_num;
			skipped_lines = true;
			continue;
		}

		if (skipped_lines)
		{
			skipped_lines = false;
			result += "    ...\n";
		}

		file_line.clear();
		highlight_line.clear();
		column = 0;
		auto const line_begin = it;
		while (it != last_line_end && *it != '\n')
		{
			auto const stop = find_next_stop(it);
			auto const str = bz::u8string_view(it, stop);
			auto const str_len = str.length();
			it = stop;
			file_line += str;
			highlight_line += bz::u8string(str_len, ' ');
			column += str_len;
			try_put_error(line_begin);
			if (
				(try_put_first_suggestion() && it == first_str_pos)
				// this is deliberately not a '||', because we need both expressions
				// to be evaluated
				| (try_put_second_suggestion() && it == second_str_pos)
			)
			{
				if (it == src_begin)
				{
					continue;
				}
				else if (it == last_line_end || *it == '\n')
				{
					break;
				}
				else if (*it == '\t')
				{
					auto const tab_size = global_data::tab_size == 0 ? 4 : global_data::tab_size;
					auto const chars_to_put = tab_size - column % tab_size;
					auto const spaces = bz::u8string(chars_to_put, ' ');
					file_line      += spaces;
					highlight_line += spaces;
					column += chars_to_put;
					++it;
				}
				else
				{
					auto utf8_it = bz::u8string_view::const_iterator(it);
					file_line += *utf8_it;
					highlight_line += ' ';
					++column;
					++utf8_it;
					it = utf8_it.data();
				}
			}
			if (it != last_line_end && *it == '\t')
			{
				auto const tab_size = global_data::tab_size == 0 ? 4 : global_data::tab_size;
				auto const chars_to_put = tab_size - column % tab_size;
				auto const spaces = bz::u8string(chars_to_put, ' ');
				file_line      += spaces;
				highlight_line += spaces;
				column += chars_to_put;
				++it;
			}
		}

		auto const line_color = highlight_line.contains('^') ? colors::clear : colors::bright_black;
		result += bz::format("{}{:{}}{} | {}\n", line_color, max_line_chars_width, line_num, colors::clear, file_line);
		if (highlight_line.size() != 0 && !highlight_line.is_all(bz::u8char(' ')))
		{
			result += bz::format("{:{}} | {}\n", max_line_chars_width, "", highlight_line);
		}
		if (it != last_line_end)
		{
			++it;
		}
		++line_num;
	}
	return result;
}

void print_error_or_warning(error const &err, global_context &context)
{
	auto const [err_file_begin, err_file_end] = context.get_file_begin_and_end(err.src_highlight.file_id);
	auto const src_pos = [&, err_file_begin = err_file_begin]() {
		if (err.src_highlight.file_id == global_context::compiler_file_id)
		{
			return bz::format("{}bozon:{}", colors::bright_white, colors::clear);
		}
		else if (err.src_highlight.src_begin == err.src_highlight.src_end)
		{
			return bz::format(
				"{}{}:{}:{}",
				colors::bright_white,
				context.get_file_name(err.src_highlight.file_id), err.src_highlight.line,
				colors::clear
			);
		}
		else
		{
			auto const file_name = context.get_file_name(err.src_highlight.file_id);
			return bz::format(
				"{}{}:{}:{}:{}",
				colors::bright_white,
				file_name, err.src_highlight.line, get_column_number(err_file_begin, err.src_highlight.src_pivot),
				colors::clear
			);
		}
	}();
	auto const error_or_warning_line = [&]() {
		if (err.is_error())
		{
			return bz::format(
				"{}error:{} {}",
				error_color, colors::clear, convert_string_for_message(err.src_highlight.message)
			);
		}
		else if (is_warning_error(err.kind))
		{
			return bz::format(
				"{}error:{} {} {}[-W error={}]{}",
				error_color, colors::clear,
				convert_string_for_message(err.src_highlight.message),
				colors::bright_white, get_warning_name(err.kind), colors::clear
			);
		}
		else
		{
			return bz::format(
				"{}warning:{} {} {}[-W {}]{}",
				warning_color, colors::clear,
				convert_string_for_message(err.src_highlight.message),
				colors::bright_white, get_warning_name(err.kind), colors::clear
			);
		}
	}();

	if (global_data::no_error_highlight)
	{
		bz::print(
			stderr,
			"{} {}\n",
			src_pos, error_or_warning_line
		);
	}
	else
	{
		bz::print(
			stderr,
			"{} {}\n{}",
			src_pos, error_or_warning_line,
			get_highlighted_text(
				err.is_error() || is_warning_error(err.kind) ? error_color : warning_color,
				err_file_begin, err_file_end, err.src_highlight
			)
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
		auto const note_src_pos = [&]() {
			if (n.file_id == global_context::compiler_file_id)
			{
				return bz::format("{}bozon:{}", colors::bright_white, colors::clear);
			}
			else if (is_empty)
			{
				return bz::format(
					"{}{}:{}:{}",
					colors::bright_white,
					context.get_file_name(n.file_id), n.line,
					colors::clear
				);
			}
			else
			{
				return bz::format(
					"{}{}:{}:{}:{}",
					colors::bright_white,
					context.get_file_name(n.file_id), n.line, column,
					colors::clear
				);
			}
		}();

		if (global_data::no_error_highlight || (!global_data::do_verbose && context.is_library_file(n.file_id)))
		{
			bz::print(
				stderr,
				"{} {}note:{} {}\n",
				note_src_pos, note_color, colors::clear,
				convert_string_for_message(n.message)
			);
		}
		else
		{
			bz::print(
				stderr,
				"{} {}note:{} {}\n{}",
				note_src_pos, note_color, colors::clear,
				convert_string_for_message(n.message),
				get_highlighted_text(note_color, note_file_begin, note_file_end, n)
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

		if (global_data::no_error_highlight)
		{
			bz::print(
				stderr,
				"{}{}:{}:{}:{} {}suggestion:{} {}\n",
				colors::bright_white,
				context.get_file_name(s.file_id), s.line, actual_column,
				colors::clear,
				suggestion_color, colors::clear,
				convert_string_for_message(s.message)
			);
		}
		else
		{
			bz::print(
				stderr,
				"{}{}:{}:{}:{} {}suggestion:{} {}\n{}",
				colors::bright_white,
				context.get_file_name(s.file_id), s.line, actual_column,
				colors::clear,
				suggestion_color, colors::clear,
				convert_string_for_message(s.message),
				get_highlighted_text("", suggestion_file_begin, suggestion_file_end, s)
			);
		}
	}
}

} // namespace ctx
