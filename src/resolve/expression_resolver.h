#ifndef RESOLVE_RESOLVE_EXPRESSION_H
#define RESOLVE_RESOLVE_EXPRESSION_H

#include "core.h"
#include "ast/expression.h"
#include "ctx/parse_context.h"

namespace resolve
{

void resolve_expression(ast::expression &expr, ctx::parse_context &context);

} // namespace resolve

#endif // RESOLVE_RESOLVE_EXPRESSION_H
