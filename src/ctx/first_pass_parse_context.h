#ifndef CTX_FIRST_PASS_PARSE_CONTEXT_H
#define CTX_FIRST_PASS_PARSE_CONTEXT_H

#include "core.h"

namespace ctx
{

struct global_context;

struct first_pass_parse_context
{
	global_context &global_ctx;

	first_pass_parse_context(global_context &_global_ctx)
		: global_ctx(_global_ctx)
	{}

	void report_error(lex::token_pos it) const;
	void report_error(
		lex::token_pos it,
		bz::string message, bz::vector<ctx::note> notes = {}
	) const;
	void report_error(
		lex::token_pos begin, lex::token_pos pivot, lex::token_pos end,
		bz::string message, bz::vector<ctx::note> notes = {}
	) const;

	lex::token_pos assert_token(lex::token_pos &stream, uint32_t kind) const;
	lex::token_pos assert_token(lex::token_pos &stream, uint32_t kind1, uint32_t kind2) const;
};

} // namespace ctx


#endif // CTX_FIRST_PASS_PARSE_CONTEXT_H
