#include "src_file.h"
#include "ctx/global_context.h"
#include "bc/emit_bitcode.h"

#include "colors.h"

static uint32_t get_column_number(ctx::char_pos const file_begin, ctx::char_pos pivot)
{
	uint32_t column = 0;
	do
	{
		if (pivot == file_begin)
		{
			return column + 1;
		}

		--pivot;
		++column;
	} while (*pivot != '\n');
	return column;
}

static bz::string get_highlighted_chars(
	ctx::char_pos const file_begin,
	ctx::char_pos const file_end,
	ctx::char_pos const char_begin,
	ctx::char_pos const char_pivot,
	ctx::char_pos const char_end,
	size_t const pivot_line,
	bz::string_view const highlight_color
)
{
	if (char_begin == char_end)
	{
		return "";
	}

	assert(file_begin <= char_begin);
	assert(char_begin < char_end);
	assert(char_begin <= char_pivot);
	assert(char_pivot < char_end);
	assert(char_end <= file_end);

	auto line_begin = char_begin;

	while (line_begin != file_begin && *(line_begin - 1) != '\n')
	{
		--line_begin;
	}

	auto line_end = char_end;

	while (line_end != file_end && *(line_end - 1) != '\n')
	{
		++line_end;
	}

	auto const begin_line_num = [&]() {
		auto line = pivot_line;
		for (auto it = char_pivot; it > char_begin; --it)
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

	bz::string file_line = "";
	bz::string highlight_line = "";
	auto line_num = begin_line_num;

	bz::string result = "";

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

		assert(*it == '\n');
		++it;
		++line_num;
	}

	return result;
}

static bz::string get_highlighted_suggestion(
	ctx::char_pos const file_begin,
	ctx::char_pos const file_end,
	ctx::char_pos const char_place,
	bz::string_view const suggestion_str,
	size_t const line
)
{
	assert(file_begin < file_end);
	assert(file_begin <= char_place);
	assert(char_place <= file_end);

	auto line_begin = char_place;
	while (line_begin != file_begin && *(line_begin - 1) != '\n')
	{
		--line_begin;
	}

	auto line_end = char_place;
	while (line_end != file_end && *line_end != '\n')
	{
		++line_end;
	}

	bz::string file_line = "";
	bz::string highlight_line = "";

	size_t column = 0;

	auto it = line_begin;
	while (true)
	{
		if (it == char_place)
		{
			file_line += colors::suggestion_color;
			file_line += suggestion_str;
			file_line += colors::clear;

			highlight_line += colors::suggestion_color;
			highlight_line += bz::string(suggestion_str.length(), '~');
			highlight_line += colors::clear;

			column += suggestion_str.length();
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
}

static bz::string read_text_from_file(std::ifstream &file)
{
	file.seekg(std::ios::end);
	size_t const size = file.tellg();
	file.seekg(std::ios::beg);

	bz::string file_str = "";

	if (size == 0)
	{
		return file_str;
	}

	file_str.reserve(size);

	while (true)
	{
		char c = file.get();
		if (file.eof())
		{
			break;
		}

		// we use '\n' for line endings and not '\r\n' or '\r'
		if (c == '\r')
		{
			if (file.peek() == '\n')
			{
				file.get();
			}

			c = '\n';
		}

		file_str += c;
	}

	return file_str;
}

src_file::src_file(bz::string_view file_name, ctx::global_context &global_ctx)
	: _stage(constructed), _file_name(file_name), _file(), _tokens(), _global_ctx(global_ctx)
{}

static void print_error(ctx::char_pos file_begin, ctx::char_pos file_end, ctx::error const &err)
{
	bz::printf(
		"{}{}:{}:{}:{} {}error:{} {}\n{}",
		colors::bright_white,
		err.file, err.line, get_column_number(file_begin, err.src_pivot),
		colors::clear,
		colors::error_color, colors::clear,
		err.message,
		get_highlighted_chars(
			file_begin, file_end,
			err.src_begin, err.src_pivot, err.src_end,
			err.line,
			colors::error_color
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
			get_highlighted_chars(
				file_begin, file_end,
				n.src_begin, n.src_pivot, n.src_end,
				n.line,
				colors::note_color
			)
		);
	}
	for (auto &s : err.suggestions)
	{
		bz::printf(
			"{}{}:{}:{}:{} {}suggestion:{} {}\n{}",
			colors::bright_white,
			s.file, s.line, get_column_number(file_begin, s.place),
			colors::clear,
			colors::suggestion_color, colors::clear,
			s.message,
			get_highlighted_suggestion(
				file_begin, file_end,
				s.place, s.suggestion_str, s.line
			)
		);
	}
}

void src_file::report_and_clear_errors(void)
{
	if (this->_global_ctx.has_errors())
	{
		for (auto &err : this->_global_ctx.get_errors())
		{
			print_error(this->_file.begin(), this->_file.end(), err);
		}
		this->_global_ctx.clear_errors();
	}
}

[[nodiscard]] bool src_file::read_file(void)
{
	assert(this->_stage == constructed);
	auto file_name = this->_file_name;
	file_name.reserve(file_name.size() + 1);
	*(file_name.end()) = '\0';
	std::ifstream file(file_name.data());

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
	assert(this->_stage == file_read);

	ctx::lex_context context(this->_global_ctx);
	this->_tokens = lex::get_tokens(this->_file, this->_file_name, context);
	this->_stage = tokenized;

	return !this->_global_ctx.has_errors();
}

[[nodiscard]] bool src_file::first_pass_parse(void)
{
	assert(this->_stage == tokenized);
	assert(this->_tokens.size() != 0);
	assert(this->_tokens.back().kind == lex::token::eof);
	auto stream = this->_tokens.cbegin();
	auto end    = this->_tokens.cend() - 1;

	ctx::first_pass_parse_context context(this->_global_ctx);

	while (stream != end)
	{
		this->_declarations.emplace_back(parse_declaration(stream, end, context));
	}

	for (auto &decl : this->_declarations)
	{
		this->_global_ctx.add_export_declaration(this->_global_ctx.get_file_id(this->_file_name), decl);
	}

	this->_stage = first_pass_parsed;
	return !this->_global_ctx.has_errors();
}

[[nodiscard]] bool src_file::resolve(void)
{
	assert(this->_stage == first_pass_parsed);

	ctx::parse_context context(this->_global_ctx.get_file_id(this->_file_name), this->_global_ctx);

	for (auto &decl : this->_declarations)
	{
		context.add_global_declaration(decl);
	}

	for (auto &decl : this->_declarations)
	{
		::resolve(decl, context);
	}

	this->_stage = resolved;
	return !this->_global_ctx.has_errors();
}

[[nodiscard]] bool src_file::emit_bitcode(ctx::bitcode_context &context)
{
	// add the declarations to the module
	for (auto &decl : this->_declarations)
	{
		switch (decl.kind())
		{
		case ast::declaration::index<ast::decl_variable>:
			assert(false);
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
			assert(false);
			break;
		default:
			assert(false);
			break;
		}
	}
	// add the definitions to the module
	for (auto &decl : this->_declarations)
	{
		switch (decl.kind())
		{
		case ast::declaration::index<ast::decl_variable>:
			assert(false);
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
			assert(false);
			break;
		default:
			assert(false);
			break;
		}
	}

	return true;
}
