#ifndef PARSER_H
#define PARSER_H

#include "ast/expression.h"
#include "ast/statement.h"
#include "ctx/parse_context.h"

void resolve(
	ast::expression &expr,
	ctx::parse_context &context
);

void resolve_symbol(
	ast::function_body &func_body,
	ctx::parse_context &context
);

void resolve(
	ast::statement &stmt,
	ctx::parse_context &context
);

#endif // PARSER_H
