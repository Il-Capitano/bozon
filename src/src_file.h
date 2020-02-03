#ifndef SRC_FILE_H
#define SRC_FILE_H

#include "core.h"

#include "lexer.h"
#include "context.h"

class src_file
{
private:
	bz::string        _file_name;
	bz::string        _file;
	bz::vector<token> _tokens;

public:
	src_file(bz::string file_name);

	bz::string_view file(void) const
	{ return this->_file; }

	auto tokens_begin(void) const
	{ return this->_tokens.begin(); }

	auto tokens_end(void) const
	{ return this->_tokens.end(); }
};

#endif // SRC_FILE_H
