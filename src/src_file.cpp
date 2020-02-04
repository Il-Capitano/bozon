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
	: _stage(constructed), _file_name(std::move(file_name)), _file(), _tokens()
{}

static void print_error(error const &err)
{
	bz::printf(
		"In file {}:{}:{}: error: {}\n",
		err.file, err.line, err.column,
		err.message
	);
	if (err.src_begin != err.src_end)
	{
		bz::print(get_highlighted_chars(err.src_begin, err.src_pivot, err.src_end));
	}
	for (auto &n : err.notes)
	{
		bz::printf(
			"In file {}:{}:{}: note: {}\n{}",
			n.file, n.line, n.column,
			n.message,
			get_highlighted_chars(n.src_begin, n.src_pivot, n.src_end)
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
		print_error(err);
	}
	this->_errors.clear();
}

[[nodiscard]] bool src_file::read_file(void)
{
	if (this->_stage == constructed)
	{
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
	}

	return true;
}

[[nodiscard]] bool src_file::tokenize(void)
{
	assert(this->_stage >= file_read);
	if (this->_stage == file_read)
	{
		this->_tokens = get_tokens(this->_file, this->_file_name, this->_errors);
		this->_stage = tokenized;
	}

	return this->_errors.size() == 0;
}

[[nodiscard]] bool src_file::first_pass_parse(void)
{
	return false;
}
