#ifndef FIRST_PASS_PARSER_H
#define FIRST_PASS_PARSER_H

#include "core.h"
#include "lex/lexer.h"
#include "ast/statement.h"

ast::declaration parse_declaration(
	lex::token_pos &stream, lex::token_pos end,
	bz::vector<ctx::error> &errors
);
ast::statement parse_statement(
	lex::token_pos &stream, lex::token_pos end,
	bz::vector<ctx::error> &errors
);

#endif // FIRST_PASS_PARSER_H
