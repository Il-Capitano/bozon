#include "src_file.h"

static bz::string read_file(std::ifstream &file)
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
	: _file_name(std::move(file_name)), _file(), _tokens()
{
	this->_file_name.reserve(this->_file_name.size() + 1);
	*(this->_file_name.end()) = '\0';
	std::ifstream file(this->_file_name.data());

	this->_file   = read_file(file);
	this->_tokens = get_tokens(this->_file, this->_file_name);
}
