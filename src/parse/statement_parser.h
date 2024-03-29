#ifndef PARSE_STATEMENT_PARSER_H
#define PARSE_STATEMENT_PARSER_H

#include "core.h"
#include "ast/statement.h"
#include "ctx/parse_context.h"
#include "token_info.h"

namespace parse
{

ast::statement parse_global_statement(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);
ast::statement parse_local_statement(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);
ast::statement parse_struct_body_statement(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);
ast::statement parse_local_statement_without_semi_colon(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

bz::vector<ast::statement> parse_global_statements(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);
bz::vector<ast::statement> parse_local_statements(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);
bz::vector<ast::statement> parse_struct_body_statements(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

} // namespace parse

#endif // PARSE_STATEMENT_PARSER_H
