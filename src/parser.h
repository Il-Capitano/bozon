#ifndef PARSER_H
#define PARSER_H

#include "ast/expression.h"
#include "ast/statement.h"
#include "ctx/parse_context.h"


// void resolve(
// 	ast::typespec &ts,
// 	ctx::parse_context &context
// );

void resolve(
	ast::expression &expr,
	ctx::parse_context &context
);

// void resolve(
// 	ast::decl_variable &var_decl,
// 	ctx::parse_context &context
// );

void resolve_symbol(
	ast::function_body &func_body,
	ctx::parse_context &context
);

// void resolve(
// 	ast::decl_function &func_decl,
// 	bz::string_view scope,
// 	ctx::global_context &global_ctx
// );
// void resolve(
// 	ast::decl_operator &decl,
// 	bz::string_view scope,
// 	ctx::global_context &global_ctx
// );
// void resolve(
// 	ast::decl_struct &decl,
// 	bz::string_view scope,
// 	ctx::global_context &global_ctx
// );

void resolve(
	ast::declaration &decl,
	ctx::parse_context &context
);
void resolve(
	ast::statement &stmt,
	ctx::parse_context &context
);

#endif // PARSER_H
