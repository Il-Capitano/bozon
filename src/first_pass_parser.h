#ifndef FIRST_PASS_PARSER_H
#define FIRST_PASS_PARSER_H

#include "core.h"
#include "lexer.h"
#include "ast/statement.h"


ast::statement get_ast_statement(src_tokens::pos &stream, src_tokens::pos end);
bz::vector<ast::statement> get_ast_statements(src_tokens::pos &stream, src_tokens::pos end);

#endif // FIRST_PASS_PARSER_H
