#ifndef FIRST_PASS_PARSER_H
#define FIRST_PASS_PARSER_H

#include "core.h"
#include "lexer.h"
#include "ast/statement.h"


ast::statement get_ast_statement(src_file::token_pos &stream, src_file::token_pos end);
bz::vector<ast::statement> get_ast_statements(src_file::token_pos &stream, src_file::token_pos end);

#endif // FIRST_PASS_PARSER_H
