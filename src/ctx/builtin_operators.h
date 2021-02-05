#ifndef CTX_BUILT_IN_OPERATORS_H
#define CTX_BUILT_IN_OPERATORS_H

#include "ast/expression.h"
#include "parse_context.h"

namespace ctx
{

constexpr bool is_integer_kind(uint32_t kind)
{
	return kind >= ast::type_info::int8_
		&& kind <= ast::type_info::uint64_;
}

constexpr bool is_unsigned_integer_kind(uint32_t kind)
{
	return kind >= ast::type_info::uint8_
		&& kind <= ast::type_info::uint64_;
}

constexpr bool is_signed_integer_kind(uint32_t kind)
{
	return kind >= ast::type_info::int8_
		&& kind <= ast::type_info::int64_;
}

constexpr bool is_floating_point_kind(uint32_t kind)
{
	return kind == ast::type_info::float32_
		|| kind == ast::type_info::float64_;
}

constexpr bool is_arithmetic_kind(uint32_t kind)
{
	return kind >= ast::type_info::int8_
		&& kind <= ast::type_info::float64_;
}


ast::expression make_builtin_operation(
	lex::token_pos op,
	ast::expression expr,
	parse_context &context
);

ast::expression make_builtin_type_operation(
	lex::token_pos op,
	ast::expression expr,
	parse_context &context
);

ast::expression make_builtin_operation(
	lex::token_pos op,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
);

ast::expression make_builtin_type_operation(
	lex::token_pos op,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
);

ast::expression make_builtin_cast(
	lex::src_tokens src_tokens,
	ast::expression expr,
	ast::typespec dest_type,
	parse_context &context
);

ast::expression make_builtin_subscript_operator(
	lex::src_tokens src_tokens,
	ast::expression called,
	ast::expression arg,
	parse_context &context
);

} // namespace ctx

#endif // CTX_BUILT_IN_OPERATORS_H
