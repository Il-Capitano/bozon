#ifndef PARSER_H
#define PARSER_H

#include "ast/expression.h"
#include "ast/statement.h"
#include "parse_context.h"



void resolve(
	ast::declaration &decl,
	parse_context &context,
	bz::vector<error> &errors
);
void resolve(
	ast::statement &stmt,
	parse_context &context,
	bz::vector<error> &errors
);
void resolve(
	ast::expression &expr,
	parse_context &context,
	bz::vector<error> &errors
);


ast::typespec parse_typespec(
	token_pos &stream, token_pos end,
	parse_context &context,
	bz::vector<error> &errors
);

#endif // PARSER_H
