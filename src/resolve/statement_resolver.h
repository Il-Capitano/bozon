#ifndef RESOLVE_STATEMENT_RESOLVER_H
#define RESOLVE_STATEMENT_RESOLVER_H

#include "core.h"
#include "ast/statement.h"
#include "ctx/parse_context.h"

namespace resolve
{

void resolve_typespec(ast::typespec &ts, ctx::parse_context &context, precedence prec);

void resolve_stmt_static_assert(ast::stmt_static_assert &static_assert_stmt, ctx::parse_context &context);

void resolve_variable_symbol(ast::decl_variable &var_decl, ctx::parse_context &context);
void resolve_variable(ast::decl_variable &var_decl, ctx::parse_context &context);

void resolve_type_alias(ast::decl_type_alias &alias_decl, ctx::parse_context &context);
void resolve_function_alias(ast::decl_function_alias &alias_decl, ctx::parse_context &context);

void resolve_function_parameters(
	ast::statement_view func_stmt,
	ast::function_body &func_body,
	ctx::parse_context &context
);
void resolve_function_symbol(
	ast::statement_view func_stmt,
	ast::function_body &func_body,
	ctx::parse_context &context
);
void resolve_function(
	ast::statement_view func_stmt,
	ast::function_body &func_body,
	ctx::parse_context &context
);

void resolve_type_info_parameters(ast::type_info &info, ctx::parse_context &context);
void resolve_type_info_symbol(ast::type_info &info, ctx::parse_context &context);
void resolve_type_info_members(ast::type_info &info, ctx::parse_context &context);
void resolve_type_info(ast::type_info &info, ctx::parse_context &context);
void resolve_enum(ast::decl_enum &enum_decl, ctx::parse_context &context);

void resolve_global_statement(ast::statement &stmt, ctx::parse_context &context);

void resolve_statement(ast::statement &stmt, ctx::parse_context &context);

} // namespace resolve

#endif // RESOLVE_STATEMENT_RESOLVER_H
