#ifndef SRC_FILE_H
#define SRC_FILE_H

#include "core.h"

#include "ctx/lex_context.h"
#include "lex/lexer.h"
#include "ctx/first_pass_parse_context.h"
#include "ctx/parse_context.h"
#include "ctx/bitcode_context.h"
#include "ctx/decl_set.h"

struct src_file
{
	enum src_file_stage
	{
		constructed,
		file_read,
		tokenized,
		parsed_global_symbols,
		parsed,
	};

public:
	src_file_stage _stage;

	uint32_t               _file_id;
	bz::u8string           _file_name;
	bz::u8string           _file;
	bz::vector<lex::token> _tokens;

	ctx::global_context       &_global_ctx;
	bz::vector<ast::statement> _declarations;
	ctx::decl_set              _global_decls;
	ctx::decl_set              _export_decls;

public:
	src_file(bz::u8string_view file_name, ctx::global_context &global_ctx);

	void report_and_clear_errors_and_warnings(void);
	void add_to_global_decls(ctx::decl_set const &set);

	[[nodiscard]] bool read_file(void);
	[[nodiscard]] bool tokenize(void);
	[[nodiscard]] bool parse_global_symbols(void);
	[[nodiscard]] bool parse(void);


	bz::u8string_view get_file_name() const
	{ return this->_file_name; }

	auto tokens_begin(void) const
	{ bz_assert(this->_stage >= tokenized); return this->_tokens.begin(); }

	auto tokens_end(void) const
	{ bz_assert(this->_stage >= tokenized); return this->_tokens.end(); }
};

#endif // SRC_FILE_H
