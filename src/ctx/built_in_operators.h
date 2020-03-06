#ifndef CTX_BUILT_IN_OPERATORS_H
#define CTX_BUILT_IN_OPERATORS_H

#include "ast/expression.h"

namespace ctx
{

struct global_context;

auto get_non_overloadable_operation_type(
	ast::expression::expr_type_t const &expr,
	uint32_t op,
	bz::string_view scope,
	global_context &context
) -> bz::result<ast::expression::expr_type_t, bz::string>;


auto get_built_in_operation_type(
	ast::expression::expr_type_t const &expr,
	uint32_t op
) -> ast::expression::expr_type_t;

auto get_built_in_operation_type(
	ast::expression::expr_type_t const &lhs,
	ast::expression::expr_type_t const &rhs,
	uint32_t op
) -> ast::expression::expr_type_t;

} // namespace ctx

#endif // CTX_BUILT_IN_OPERATORS_H
