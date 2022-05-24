#ifndef RESOLVE_CONSTEVAL_H
#define RESOLVE_CONSTEVAL_H

#include "ast/expression.h"
#include "ctx/parse_context.h"

namespace resolve
{

void consteval_guaranteed(ast::expression &expr, ctx::parse_context &context);
void consteval_try(ast::expression &expr, ctx::parse_context &context);
void consteval_try_without_error(ast::expression &expr, ctx::parse_context &context);

void consteval_try_without_error_decl(ast::statement &stmt, ctx::parse_context &context);

bz::vector<ctx::source_highlight> get_consteval_fail_notes(ast::expression const &expr);

} // namespace resolve

#endif // RESOLVE_CONSTEVAL_H
