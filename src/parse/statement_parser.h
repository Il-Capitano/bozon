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

void resolve_global_statement(
	ast::statement &stmt,
	ctx::parse_context &context
);

void resolve_type_alias(
	ast::decl_type_alias &alias_decl,
	ctx::parse_context &context
);

void resolve_variable_symbol(
	ast::decl_variable &var_decl,
	ctx::parse_context &context
);
void resolve_variable(
	ast::decl_variable &var_decl,
	ctx::parse_context &context
);

void resolve_function_alias(
	ast::decl_function_alias &alias_decl,
	ctx::parse_context &context
);

void resolve_function_parameters(
	ast::statement_view func_stmt,
	ast::function_body &func_body,
	ctx::parse_context &context
);
void resolve_function_symbol(
	ast::statement_view func_stmt,
	ast::function_body &func_body,
	ctx::parse_context &context
);
void resolve_function(
	ast::statement_view func_stmt,
	ast::function_body &func_body,
	ctx::parse_context &context
);

} // namespace parse

#endif // PARSE_STATEMENT_PARSER_H
