#ifndef PARSE_CONSTEVAL_H
#define PARSE_CONSTEVAL_H

#include "ast/expression.h"
#include "ctx/parse_context.h"

namespace parse
{

void consteval_guaranteed(ast::expression &expr, ctx::parse_context &context);
void consteval_try(ast::expression &expr, ctx::parse_context &context);

bz::vector<ctx::source_highlight> get_consteval_fail_notes(ast::expression const &expr);

} // namespace parse

#endif // PARSE_CONSTEVAL_H
