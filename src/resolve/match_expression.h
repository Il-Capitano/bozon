#ifndef RESOLVE_MATCH_EXPRESSION_H
#define RESOLVE_MATCH_EXPRESSION_H

#include "core.h"
#include "ast/typespec.h"
#include "ast/expression.h"
#include "ast/statement.h"
#include "ctx/parse_context.h"

namespace resolve
{

void match_expression_to_type(ast::expression &expr, ast::typespec &dest_type, ctx::parse_context &context);
void match_expression_to_variable(ast::expression &expr, ast::decl_variable &var_decl, ctx::parse_context &context);

} // namespace resolve

#endif // RESOLVE_MATCH_EXPRESSION_H
