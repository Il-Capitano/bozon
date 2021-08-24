#ifndef RESOLVE_EXPRESSION_RESOLVER
#define RESOLVE_EXPRESSION_RESOLVER

#include "core.h"
#include "ast/expression.h"
#include "ctx/parse_context.h"

namespace resolve
{

void resolve_expression(ast::expression &expr, ctx::parse_context &context);

} // namespace resolve

#endif // RESOLVE_EXPRESSION_RESOLVER
