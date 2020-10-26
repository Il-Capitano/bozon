#ifndef PARSE_CONSTEVAL_H
#define PARSE_CONSTEVAL_H

#include "ast/expression.h"
#include "ctx/parse_context.h"

namespace parse
{

void consteval_guaranteed(ast::expression &expr, ctx::parse_context &context);
void consteval_try(ast::expression &expr, ctx::parse_context &context);

} // namespace parse

#endif // PARSE_CONSTEVAL_H
