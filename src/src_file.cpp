#include "src_file.h"

static bz::string read_text_from_file(std::ifstream &file)
{
	file.seekg(std::ios::end);
	size_t const size = file.tellg();
	file.seekg(std::ios::beg);

	bz::string file_str = "\n";

	if (size == 0)
	{
		return file_str;
	}

	file_str.reserve(size + 2);

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

		file_str.push_back(c);
	}

	file_str.push_back('\n');

	return file_str;
}

src_file::src_file(bz::string file_name)
	: _file_name(std::move(file_name)), _file(), _tokens(), _stage(constructed)
{}

void src_file::report_errors_if_any(void)
{
	if (this->_errors.size() == 0)
	{
		return;
	}

	for (auto &err : this->_errors)
	{
		bz::printf(
			"In file {}:{}:{}: error: {}\n{}",
			err.file, err.line, err.column,
			err.message,
			get_highlighted_chars(err.src_begin, err.src_pivot, err.src_end)
		);
	}

	exit(1);
}

void src_file::read_file(void)
{
	if (this->_stage == constructed)
	{
		auto file_name = this->_file_name;
		file_name.reserve(file_name.size() + 1);
		*(file_name.end()) = '\0';
		std::ifstream file(file_name.data());
		this->_file = read_text_from_file(file);
		this->_stage = file_read;
	}
}

void src_file::tokenize(void)
{
	assert(this->_stage >= file_read);
	if (this->_stage == file_read)
	{
		this->_tokens = get_tokens(this->_file, this->_file_name);
		this->_stage = tokenized;
	}
}

void src_file::first_pass_parse(void)
{
	assert(false);
}
