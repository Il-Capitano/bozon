#ifndef SRC_FILE_H
#define SRC_FILE_H

#include "core.h"

#include "ctx/lex_context.h"
#include "lex/lexer.h"
#include "ctx/first_pass_parse_context.h"
#include "first_pass_parser.h"
#include "ctx/parse_context.h"
#include "ctx/bitcode_context.h"
#include "parser.h"

struct src_file
{
	enum src_file_stage
	{
		constructed,
		file_read,
		tokenized,
		first_pass_parsed,
		resolved,
	};

public:
	src_file_stage _stage;

	bz::string             _file_name;
	bz::string             _file;
	bz::vector<lex::token> _tokens;

	ctx::global_context         &_global_ctx;
	bz::vector<ast::declaration> _declarations;

public:
	src_file(bz::string_view file_name, ctx::global_context &global_ctx);

	void report_and_clear_errors(void);

	[[nodiscard]] bool read_file(void);
	[[nodiscard]] bool tokenize(void);
	[[nodiscard]] bool first_pass_parse(void);
	[[nodiscard]] bool resolve(void);
	[[nodiscard]] bool emit_bitcode(ctx::bitcode_context &context);


	bz::string const &get_file_name() const
	{ return this->_file_name; }

	auto tokens_begin(void) const
	{ assert(this->_stage >= tokenized); return this->_tokens.begin(); }

	auto tokens_end(void) const
	{ assert(this->_stage >= tokenized); return this->_tokens.end(); }
};

#endif // SRC_FILE_H
