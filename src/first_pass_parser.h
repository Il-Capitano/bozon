#ifndef FIRST_PASS_PARSER_H
#define FIRST_PASS_PARSER_H

#include "core.h"

#include "lex/token.h"
#include "ast/statement.h"
#include "ctx/first_pass_parse_context.h"

ast::declaration parse_declaration(
	lex::token_pos &stream, lex::token_pos end,
	ctx::first_pass_parse_context &context
);
ast::statement parse_statement(
	lex::token_pos &stream, lex::token_pos end,
	ctx::first_pass_parse_context &context
);

#endif // FIRST_PASS_PARSER_H
