#ifndef PARSER_H
#define PARSER_H

#include "ast/expression.h"
#include "ast/statement.h"
#include "ctx/parse_context.h"


void resolve(
	ast::typespec &ts,
	ctx::parse_context &context,
	bz::vector<ctx::error> &errors
);
void resolve(
	ast::expression &expr,
	ctx::parse_context &context,
	bz::vector<ctx::error> &errors
);
void resolve(
	ast::declaration &decl,
	ctx::parse_context &context,
	bz::vector<ctx::error> &errors
);
void resolve(
	ast::statement &stmt,
	ctx::parse_context &context,
	bz::vector<ctx::error> &errors
);


ast::typespec parse_typespec(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context,
	bz::vector<ctx::error> &errors
);

#endif // PARSER_H
