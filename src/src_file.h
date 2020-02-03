#ifndef SRC_FILE_H
#define SRC_FILE_H

#include "core.h"

#include "lexer.h"
#include "context.h"

struct error
{
	bz::string file;
	size_t line;
	size_t column;
	char_pos src_begin;
	char_pos src_pivot;
	char_pos src_end;
	bz::string message;
};

struct src_file
{
	enum src_file_stage
	{
		constructed,
		file_read,
		tokenized,
		first_pass_parsed,
	};

private:
	src_file_stage _stage;

	bz::vector<error> _errors;

	bz::string        _file_name;
	bz::string        _file;
	bz::vector<token> _tokens;

	parse_context                _context;
	bz::vector<ast::declaration> _declarations;

public:
	src_file(bz::string file_name);

	void report_errors_if_any(void);

	void read_file(void);
	void tokenize(void);
	void first_pass_parse(void);

	auto tokens_begin(void) const
	{ assert(this->_stage >= tokenized); return this->_tokens.begin(); }

	auto tokens_end(void) const
	{ assert(this->_stage >= tokenized); return this->_tokens.end(); }
};

#endif // SRC_FILE_H
