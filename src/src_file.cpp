#include "src_file.h"

#include "colors.h"

static bz::string get_highlighted_chars(
	char_pos const file_begin,
	char_pos const file_end,
	char_pos const char_begin,
	char_pos const char_pivot,
	char_pos const char_end,
	size_t const pivot_line,
	char const *const highlight_color
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

	for (auto it = line_begin; it != line_end; ++it)
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

			if (*it == '\n')
			{
				break;
			}
			++it;
		}

		result += bz::format("{:>{}} | ", max_line_size, line_num);
		result += file_line;
		result += colors::clear;
		result += bz::format("{:>{}} | ", max_line_size, "");
		result += highlight_line;
		result += colors::clear;
		result += '\n';
		++line_num;
	}

	return result;
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

src_file::src_file(bz::string file_name)
	: _stage(constructed), _file_name(std::move(file_name)), _file(), _tokens()
{}

static void print_error(char_pos file_begin, char_pos file_end, error const &err)
{
	bz::printf(
		"{}{}:{}:{}:{} {}error{}: {}\n{}",
		colors::bright_white, err.file, err.line, err.column, colors::clear,
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
			"{}:{}:{}: {}note{}: {}\n{}",
			n.file, n.line, n.column,
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
}

void src_file::report_and_clear_errors(void)
{
	if (this->_errors.size() == 0)
	{
		return;
	}

	for (auto &err : this->_errors)
	{
		print_error(this->_file.begin(), this->_file.end(), err);
	}
	this->_errors.clear();
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
	if (this->_stage == file_read)
	{
		this->_tokens = get_tokens(this->_file, this->_file_name, this->_errors);
		this->_stage = tokenized;
	}

	return this->_errors.size() == 0;
}

[[nodiscard]] bool src_file::first_pass_parse(void)
{
	assert(this->_stage == tokenized);
	assert(this->_tokens.size() != 0);
	assert(this->_tokens.back().kind == token::eof);
	auto stream = this->_tokens.cbegin();
	auto end    = this->_tokens.cend() - 1;

	while (stream != end)
	{
		this->_declarations.emplace_back(parse_declaration(stream, end, this->_errors));
	}

	for (auto &decl : this->_declarations)
	{
		this->_context.add_global_declaration(decl, this->_errors);
	}

	return this->_errors.size() == 0;
}
