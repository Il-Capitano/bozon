#ifndef FIRST_PASS_PARSER_H
#define FIRST_PASS_PARSER_H

#include "core.h"
#include "lexer.h"
#include "ast/statement.h"

ast::declaration parse_declaration(token_pos &stream, token_pos end, bz::vector<error> &errors);
ast::statement parse_statement(token_pos &stream, token_pos end, bz::vector<error> &errors);

#endif // FIRST_PASS_PARSER_H
