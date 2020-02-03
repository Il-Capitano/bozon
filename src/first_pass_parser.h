#ifndef FIRST_PASS_PARSER_H
#define FIRST_PASS_PARSER_H

#include "core.h"
#include "lexer.h"
#include "ast/statement.h"


ast::statement get_ast_statement(token_pos &stream, token_pos end);
bz::vector<ast::statement> get_ast_statements(
	token_pos &stream, token_pos end
);

ast::declaration get_ast_declaration(
	token_pos &stream, token_pos end
);
bz::vector<ast::declaration> get_ast_declarations(
	token_pos &stream, token_pos end
);

#endif // FIRST_PASS_PARSER_H
