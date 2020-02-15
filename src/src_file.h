#ifndef SRC_FILE_H
#define SRC_FILE_H

#include "core.h"

#include "lexer.h"
#include "first_pass_parser.h"
#include "parse_context.h"
#include "parser.h"

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

	void report_and_clear_errors(void);

	[[nodiscard]] bool read_file(void);
	[[nodiscard]] bool tokenize(void);
	[[nodiscard]] bool first_pass_parse(void);



	bz::string const &get_file_name() const
	{ return this->_file_name; }

	bool has_errors() const
	{ return this->_errors.size() != 0; }

	size_t get_error_count() const
	{ return this->_errors.size(); }

	auto tokens_begin(void) const
	{ assert(this->_stage >= tokenized); return this->_tokens.begin(); }

	auto tokens_end(void) const
	{ assert(this->_stage >= tokenized); return this->_tokens.end(); }
};

#endif // SRC_FILE_H
