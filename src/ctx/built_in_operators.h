#ifndef CTX_BUILT_IN_OPERATORS_H
#define CTX_BUILT_IN_OPERATORS_H

#include "ast/expression.h"
#include "parse_context.h"

namespace ctx
{

inline bool is_integer_kind(uint32_t kind)
{
	return kind >= ast::type_info::int8_
		&& kind <= ast::type_info::uint64_;
}

inline bool is_unsigned_integer_kind(uint32_t kind)
{
	return kind >= ast::type_info::uint8_
		&& kind <= ast::type_info::uint64_;
}

inline bool is_signed_integer_kind(uint32_t kind)
{
	return kind >= ast::type_info::int8_
		&& kind <= ast::type_info::int64_;
}

inline bool is_floating_point_kind(uint32_t kind)
{
	return kind == ast::type_info::float32_
		|| kind == ast::type_info::float64_;
}

inline bool is_arithmetic_kind(uint32_t kind)
{
	return kind >= ast::type_info::int8_
		&& kind <= ast::type_info::float64_;
}

auto get_non_overloadable_operation_type(
	ast::expression::expr_type_t const &expr,
	uint32_t op,
	parse_context &context
) -> bz::result<ast::expression::expr_type_t, bz::u8string>;

auto get_non_overloadable_operation_type(
	ast::expression::expr_type_t const &lhs,
	ast::expression::expr_type_t const &rhs,
	uint32_t op,
	parse_context &context
) -> bz::result<ast::expression::expr_type_t, bz::u8string>;


auto get_built_in_operation_type(
	ast::expression::expr_type_t const &expr,
	uint32_t op,
	parse_context &context
) -> ast::expression::expr_type_t;

auto get_built_in_operation_type(
	ast::expression::expr_type_t const &lhs,
	ast::expression::expr_type_t const &rhs,
	uint32_t op,
	parse_context &context
) -> ast::expression::expr_type_t;

auto get_built_in_cast_type(
	ast::expression::expr_type_t const &expr,
	ast::typespec const &dest,
	parse_context &context
) -> ast::expression::expr_type_t;

} // namespace ctx

#endif // CTX_BUILT_IN_OPERATORS_H
