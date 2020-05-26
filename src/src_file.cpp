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

static bz::u8string get_highlighted_chars(
	ctx::char_pos const file_begin,
	ctx::char_pos const file_end,
	ctx::char_pos const char_begin,
	ctx::char_pos const char_pivot,
	ctx::char_pos const char_end,
	size_t const pivot_line,
	bz::u8string_view const highlight_color
)
{
	if (char_begin == char_end)
	{
		return "";
	}

	bz_assert(file_begin <= char_begin);
	bz_assert(char_begin < char_end);
	bz_assert(char_begin <= char_pivot);
	bz_assert(char_pivot < char_end);
	bz_assert(char_end <= file_end);

	auto line_begin_ptr = char_begin.data();

	while (line_begin_ptr != file_begin.data() && *(line_begin_ptr - 1) != '\n')
	{
		--line_begin_ptr;
	}

	auto const line_begin = bz::u8iterator(line_begin_ptr);

	auto line_end_ptr = char_end.data();

	while (line_end_ptr != file_end.data() && *(line_end_ptr - 1) != '\n')
	{
		++line_end_ptr;
	}

	auto const line_end = bz::u8iterator(line_end_ptr);

	auto const begin_line_num = [&]() {
		auto line = pivot_line;
		for (auto it = char_pivot.data(); it > char_begin.data(); --it)
		{
			if (*it == '\n')
			{
				--line;
			}
		}
		return line;
	}();

	auto const max_line_size = [&]() {
		auto line = pivot_line;
		for (auto it = char_pivot; it < char_end; ++it)
		{
			if (*it == '\n')
			{
				++line;
			}
		}
		// internal function is used here...
		return bz::internal::lg_uint(line);
	}();

	bz::u8string file_line = "";
	bz::u8string highlight_line = "";
	auto line_num = begin_line_num;

	bz::u8string result = "";

	auto it = line_begin;
	while (it != line_end)
	{
		size_t file_line_size = 0;
		file_line = "";
		highlight_line = "";

		if (it > char_begin && it < char_end)
		{
			file_line += highlight_color;
			highlight_line += highlight_color;
		}

		while (true)
		{
			if (it == char_begin)
			{
				file_line += highlight_color;
				highlight_line += highlight_color;
			}
			else if (it == char_end)
			{
				file_line += colors::clear;
				highlight_line += colors::clear;
			}

			if (it == file_end || *it == '\n')
			{
				if (it == char_pivot)
				{
					highlight_line += '^';
				}
				else if (it >= char_begin && it < char_end)
				{
					highlight_line += '~';
				}
				break;
			}

			if (*it == '\t')
			{
				if (it == char_pivot)
				{
					file_line += ' ';
					++file_line_size;
					highlight_line += '^';
					while (file_line_size % 4 != 0)
					{
						file_line += ' ';
						++file_line_size;
						highlight_line += '~';
					}
				}
				else
				{
					char highlight_char = it >= char_begin && it < char_end ? '~' : ' ';
					do
					{
						file_line += ' ';
						++file_line_size;
						highlight_line += highlight_char;
					} while (file_line_size % 4 != 0);
				}
			}
			else
			{
				file_line += *it;
				++file_line_size;
				if (it == char_pivot)
				{
					highlight_line += '^';
				}
				else if (it >= char_begin && it < char_end)
				{
					highlight_line += '~';
				}
				else
				{
					highlight_line += ' ';
				}
			}

			++it;
		}

		result += bz::format("{:>{}} | {}{}\n", max_line_size, line_num, file_line, colors::clear);
		result += bz::format("{:>{}} | {}{}\n", max_line_size, "", highlight_line, colors::clear);

		if (it == line_end)
		{
			break;
		}

		bz_assert(*it == '\n');
		++it;
		++line_num;
	}

	return result;
}

static bz::u8string get_highlighted_note(
	ctx::char_pos const file_begin,
	ctx::char_pos const file_end,
	ctx::note const &note
)
{
	auto const pivot_pos = note.src_pivot;
	auto const begin_pos = note.src_begin;
	auto const end_pos   = note.src_end;

	bz_assert(begin_pos <= pivot_pos);
	bz_assert(pivot_pos < end_pos);

	auto const first_suggestion_pos = note.first_suggestion.suggestion_pos;
	auto const first_erase_begin = note.first_suggestion.erase_begin;
	auto const first_erase_end = note.first_suggestion.erase_end;
	auto const second_suggestion_pos = note.second_suggestion.suggestion_pos;
	auto const second_erase_begin = note.second_suggestion.erase_begin;
	auto const second_erase_end = note.second_suggestion.erase_end;

	auto const first_suggestion_str = note.first_suggestion.suggestion_str.as_string_view();
	auto const second_suggestion_str = note.second_suggestion.suggestion_str.as_string_view();

	bz_assert(first_erase_begin <= first_erase_end);
	bz_assert(second_erase_begin <= second_erase_end);
	bz_assert(first_erase_begin.data() == nullptr || first_erase_begin <= begin_pos || first_erase_begin >= end_pos);
	bz_assert(second_erase_begin.data() == nullptr || second_erase_begin <= begin_pos || second_erase_begin >= end_pos);
	bz_assert(second_suggestion_pos.data() == nullptr || first_suggestion_pos <= second_suggestion_pos);
	bz_assert(second_suggestion_pos.data() != nullptr ? first_suggestion_pos.data() != nullptr : true);

	bz_assert(second_erase_begin.data() == nullptr || first_erase_end <= second_erase_begin);
	bz_assert(second_erase_begin.data() == nullptr || first_suggestion_pos <= second_erase_begin);
	bz_assert(second_suggestion_pos.data() == nullptr || second_suggestion_pos >= first_erase_end);

	auto const smallest_pos = first_suggestion_pos.data() == nullptr
		? begin_pos
		: (
			first_erase_begin.data() == nullptr
			? std::min(begin_pos, first_suggestion_pos)
			: std::min(begin_pos, std::min(first_suggestion_pos, first_erase_begin))
		);

	auto const largest_pos = std::max(
		end_pos,
		std::max(
			std::max(first_suggestion_pos, first_erase_end),
			std::max(second_suggestion_pos, second_erase_end)
		)
	);

	bz_assert(smallest_pos >= file_begin);
	bz_assert(largest_pos <= file_end);

	auto line_begin_ptr = smallest_pos.data();
	while (line_begin_ptr != file_begin.data() && *(line_begin_ptr - 1) != '\n')
	{
		--line_begin_ptr;
	}
	auto const line_begin = bz::u8iterator(line_begin_ptr);

	auto line_end_ptr = largest_pos.data();
	while (line_end_ptr != file_end.data() && *line_end_ptr != '\n')
	{
		++line_end_ptr;
	}
	auto const line_end = bz::u8iterator(line_end_ptr);

	auto const begin_line_num = [&]() {
		auto line = note.line;
		for (auto it = first_suggestion_pos.data(); it > line_begin.data(); --it)
		{
			if (*it == '\n')
			{
				--line;
			}
		}
		return line;
	}();

	auto const max_line_size = [&]() {
		auto line = note.line;
		for (auto it = first_suggestion_pos.data(); it < line_end.data(); ++it)
		{
			if (*it == '\n')
			{
				++line;
			}
		}
		// internal function is used here...
		return bz::internal::lg_uint(line);
	}();

	bz::u8string result = "";
	size_t line = begin_line_num;
	auto it = line_begin;
	while (it != line_end)
	{
		size_t column = 0;
		bz::u8string file_line = "";
		bz::u8string highlight_line = "";

		if (it > begin_pos && it < end_pos)
		{
			file_line += colors::note_color;
			highlight_line += colors::note_color;
		}

		while (true)
		{
			if (it == first_suggestion_pos)
			{
				file_line += colors::suggestion_color;
				file_line += first_suggestion_str;
				file_line += colors::clear;

				highlight_line += colors::suggestion_color;
				highlight_line += bz::u8string(first_suggestion_str.length(), '~');
				highlight_line += colors::clear;

				column += first_suggestion_str.length();
			}
			else if (it == second_suggestion_pos)
			{
				file_line += colors::suggestion_color;
				file_line += second_suggestion_str;
				file_line += colors::clear;

				highlight_line += colors::suggestion_color;
				highlight_line += bz::u8string(second_suggestion_str.length(), '~');
				highlight_line += colors::clear;

				column += second_suggestion_str.length();
			}

			if (it == begin_pos)
			{
				file_line += colors::note_color;
				highlight_line += colors::note_color;
			}
			else if (it == end_pos)
			{
				file_line += colors::clear;
				highlight_line += colors::clear;
			}

			if (it == first_erase_begin)
			{
				it = first_erase_end;
			}
			else if (it == second_erase_begin)
			{
				it = second_erase_end;
			}

			if (it == file_end || *it == '\n')
			{
				if (it == pivot_pos)
				{
					highlight_line += '^';
				}
				else if (it >= begin_pos && it < end_pos)
				{
					highlight_line += '~';
				}
				break;
			}

			if (*it == '\t')
			{
				if (it == pivot_pos)
				{
					file_line += ' ';
					++column;
					highlight_line += '^';
					while (column % 4 != 0)
					{
						file_line += ' ';
						++column;
						highlight_line += '~';
					}
				}
				else
				{
					char highlight_char = it >= begin_pos && it < end_pos ? '~' : ' ';
					do
					{
						file_line += ' ';
						++column;
						highlight_line += highlight_char;
					} while (column % 4 != 0);
				}
			}
			else
			{
				file_line += *it;
				++column;
				if (it == pivot_pos)
				{
					highlight_line += '^';
				}
				else if (it >= begin_pos && it < end_pos)
				{
					highlight_line += '~';
				}
				else
				{
					highlight_line += ' ';
				}
			}

			++it;
		}

		result += bz::format("{:>{}} | {}{}\n", max_line_size, line, file_line, colors::clear);
		result += bz::format("{:>{}} | {}{}\n", max_line_size, "", highlight_line, colors::clear);

		if (it == line_end)
		{
			break;
		}

		bz_assert(*it == '\n');
		++it;
		++line;
	}

	return result;
}

static bz::u8string get_highlighted_suggestion(
	ctx::char_pos const file_begin,
	ctx::char_pos const file_end,
	ctx::suggestion const &suggestion
)
{
	auto const first_suggestion_pos = suggestion.first_suggestion.suggestion_pos;
	auto const first_erase_begin = suggestion.first_suggestion.erase_begin;
	auto const first_erase_end = suggestion.first_suggestion.erase_end;
	auto const second_suggestion_pos = suggestion.second_suggestion.suggestion_pos;
	auto const second_erase_begin = suggestion.second_suggestion.erase_begin;
	auto const second_erase_end = suggestion.second_suggestion.erase_end;

	auto const first_suggestion_str = suggestion.first_suggestion.suggestion_str.as_string_view();
	auto const second_suggestion_str = suggestion.second_suggestion.suggestion_str.as_string_view();

	bz_assert(first_erase_begin <= first_erase_end);
	bz_assert(second_erase_begin <= second_erase_end);
	bz_assert(second_suggestion_pos.data() == nullptr || first_suggestion_pos <= second_suggestion_pos);

	bz_assert(second_erase_begin.data() == nullptr || first_erase_end <= second_erase_begin);
	bz_assert(second_erase_begin.data() == nullptr || first_suggestion_pos <= second_erase_begin);
	bz_assert(second_suggestion_pos.data() == nullptr || second_suggestion_pos >= first_erase_end);
	bz_assert(second_suggestion_pos.data() != nullptr ? first_suggestion_pos.data() != nullptr : true);

	auto const smallest_pos = first_erase_begin.data() == nullptr
		? first_suggestion_pos
		: std::min(first_suggestion_pos, first_erase_begin);
	auto const largest_pos = second_suggestion_pos.data() == nullptr
		? std::max(first_suggestion_pos, first_erase_end)
		: std::max(second_suggestion_pos, second_erase_end);

	bz_assert(smallest_pos >= file_begin);
	bz_assert(largest_pos <= file_end);

	auto line_begin_ptr = smallest_pos.data();
	while (line_begin_ptr != file_begin.data() && *(line_begin_ptr - 1) != '\n')
	{
		--line_begin_ptr;
	}
	auto const line_begin = bz::u8iterator(line_begin_ptr);

	auto line_end_ptr = largest_pos.data();
	while (line_end_ptr != file_end.data() && *line_end_ptr != '\n')
	{
		++line_end_ptr;
	}
	auto const line_end = bz::u8iterator(line_end_ptr);

	auto const begin_line_num = [&]() {
		auto line = suggestion.line;
		for (auto it = first_suggestion_pos.data(); it > line_begin.data(); --it)
		{
			if (*it == '\n')
			{
				--line;
			}
		}
		return line;
	}();

	auto const max_line_size = [&]() {
		auto line = suggestion.line;
		for (auto it = first_suggestion_pos.data(); it < line_end.data(); ++it)
		{
			if (*it == '\n')
			{
				++line;
			}
		}
		// internal function is used here...
		return bz::internal::lg_uint(line);
	}();

	bz::u8string result = "";
	size_t line = begin_line_num;
	auto it = line_begin;
	while (true)
	{
		bz::u8string file_line = "";
		bz::u8string highlight_line = "";
		size_t column = 0;

		while (it != line_end && *it != '\n')
		{
			if (it == first_suggestion_pos)
			{
				file_line += colors::suggestion_color;
				file_line += first_suggestion_str;
				file_line += colors::clear;

				highlight_line += colors::suggestion_color;
				highlight_line += bz::u8string(first_suggestion_str.length(), '~');
				highlight_line += colors::clear;

				column += first_suggestion_str.length();
			}
			else if (it == second_suggestion_pos)
			{
				file_line += colors::suggestion_color;
				file_line += second_suggestion_str;
				file_line += colors::clear;

				highlight_line += colors::suggestion_color;
				highlight_line += bz::u8string(second_suggestion_str.length(), '~');
				highlight_line += colors::clear;

				column += second_suggestion_str.length();
			}

			if (it == first_erase_begin)
			{
				it = first_erase_end;
			}
			else if (it == second_erase_begin)
			{
				it = second_erase_end;
			}

			if (it == line_end)
			{
				break;
			}

			if (*it == '\t')
			{
				do
				{
					file_line += ' ';
					highlight_line += ' ';
					++column;
				} while (column % 4 != 0);
			}
			else
			{
				file_line += *it;
				highlight_line += ' ';
				++column;
			}
			++it;
		}

		result += bz::format("{:>{}} | {}\n", max_line_size, line, file_line);
		result += bz::format("{:>{}} | {}\n", max_line_size, "", highlight_line);

		if (it == line_end)
		{
			break;
		}
		else
		{
			++it;
			++line;
		}
	}

	return result;

/*
	auto it = line_begin;
	while (true)
	{
		if (it == char_place)
		{
			file_line += colors::suggestion_color;
			file_line += suggestion_str;
			file_line += colors::clear;

			highlight_line += colors::suggestion_color;
			highlight_line += bz::u8string(suggestion_str.length(), '~');
			highlight_line += colors::clear;

			column += suggestion_str.length();
		}

		if (it == erase_begin)
		{
			it = erase_end;
		}

		if (it == line_end)
		{
			break;
		}

		if (*it == '\t')
		{
			do
			{
				file_line += ' ';
				highlight_line += ' ';
				++column;
			} while (column % 4 != 0);
		}
		else
		{
			file_line += *it;
			highlight_line += ' ';
			++column;
		}

		++it;
	}

	return bz::format(
		"{} | {}\n"
		"{:{}} | {}\n",
		line, file_line,
		bz::internal::lg_uint(line), "", highlight_line
	);
*/
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
	auto const highlight_color = err.is_warning ? colors::warning_color : colors::error_color;

	bz::printf(
		"{}{}:{}:{}:{} {} {}\n{}",
		colors::bright_white,
		err.file, err.line, get_column_number(file_begin, err.src_pivot),
		colors::clear,
		error_or_warning,
		err.message,
		get_highlighted_chars(
			file_begin, file_end,
			err.src_begin, err.src_pivot, err.src_end,
			err.line,
			highlight_color
		)
	);
	for (auto &n : err.notes)
	{
		bz::printf(
			"{}{}:{}:{}:{} {}note:{} {}\n{}",
			colors::bright_white,
			n.file, n.line, get_column_number(file_begin, n.src_pivot),
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
