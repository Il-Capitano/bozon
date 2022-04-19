#include "match_common.h"

namespace resolve
{

void expand_variadic_tuple_type(bz::vector<ast::typespec> &tuple_types, size_t new_size)
{
	bz_assert(tuple_types.not_empty() && tuple_types.back().is<ast::ts_variadic>());
	auto const non_variadic_count = tuple_types.size() - 1;
	if (non_variadic_count <= new_size)
	{
		auto const variadic_type = tuple_types.back().get<ast::ts_variadic>();
		tuple_types.resize(new_size, variadic_type);
		if (non_variadic_count < new_size)
		{
			tuple_types[non_variadic_count].remove_layer();
		}
	}
}

bool is_implicitly_convertible(
	ast::typespec_view dest,
	ast::typespec_view expr_type,
	[[maybe_unused]] ast::expression_type_kind expr_type_kind,
	[[maybe_unused]] ctx::parse_context &context
)
{
	auto const expr_type_without_const = ast::remove_const_or_consteval(expr_type);
	if (dest.is<ast::ts_base_type>() && expr_type_without_const.is<ast::ts_base_type>())
	{
		auto const dest_kind = dest.get<ast::ts_base_type>().info->kind;
		auto const expr_kind = expr_type_without_const.get<ast::ts_base_type>().info->kind;
		if (
			(ast::is_signed_integer_kind(dest_kind) && ast::is_signed_integer_kind(expr_kind))
			|| (ast::is_unsigned_integer_kind(dest_kind) && ast::is_unsigned_integer_kind(expr_kind))
		)
		{
			return dest_kind >= expr_kind;
		}
	}
	return false;
}

static bool is_literal_implicitly_convertible(
	ast::typespec_view dest,
	ast::expression const &expr
)
{
	bz_assert(expr.is_literal());
	auto &literal = expr.get_literal();
	auto const literal_kind = literal.tokens.begin->kind;
	auto const postfix = (literal.tokens.end - 1)->postfix;

	auto const &literal_value = expr.get_literal_value();

	if (!dest.is<ast::ts_base_type>())
	{
		return false;
	}

	auto const dest_kind = dest.get<ast::ts_base_type>().info->kind;
	if (!ast::is_integer_kind(dest_kind) || !literal_value.is_any<ast::constant_value::sint, ast::constant_value::uint>())
	{
		return false;
	}

	bz_assert(literal_value.is<ast::constant_value::uint>() || literal_value.get<ast::constant_value::sint>() >= 0);
	auto const literal_int_value = literal_value.is<ast::constant_value::sint>()
		? static_cast<uint64_t>(literal_value.get<ast::constant_value::sint>())
		: literal_value.get<ast::constant_value::uint>();
	// other kinds of literals are signed by default
	auto const is_any = literal_kind == lex::token::integer_literal && postfix == "";
	// the postfix 'i' can be used to interpret hex, octal and binary literals as signed integers
	auto const is_any_signed   = is_any || postfix == "i";
	auto const is_any_unsigned = is_any || (literal_kind != lex::token::integer_literal && postfix == "") || postfix == "u";
	bz_assert(is_any || !(is_any_signed && is_any_unsigned));

	auto const dest_max_value = [&]() -> uint64_t {
		switch (dest_kind)
		{
		case ast::type_info::int8_:
			return static_cast<uint64_t>(std::numeric_limits<int8_t>::max());
		case ast::type_info::int16_:
			return static_cast<uint64_t>(std::numeric_limits<int16_t>::max());
		case ast::type_info::int32_:
			return static_cast<uint64_t>(std::numeric_limits<int32_t>::max());
		case ast::type_info::int64_:
			return static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
		case ast::type_info::uint8_:
			return static_cast<uint64_t>(std::numeric_limits<uint8_t>::max());
		case ast::type_info::uint16_:
			return static_cast<uint64_t>(std::numeric_limits<uint16_t>::max());
		case ast::type_info::uint32_:
			return static_cast<uint64_t>(std::numeric_limits<uint32_t>::max());
		case ast::type_info::uint64_:
			return static_cast<uint64_t>(std::numeric_limits<uint64_t>::max());
		default:
			bz_unreachable;
		}
	}();

	if (is_any)
	{
		return literal_int_value <= dest_max_value;
	}
	else if (is_any_signed)
	{
		return ast::is_signed_integer_kind(dest_kind) && literal_int_value <= dest_max_value;
	}
	else if (is_any_unsigned)
	{
		return ast::is_unsigned_integer_kind(dest_kind) && literal_int_value <= dest_max_value;
	}
	else
	{
		return false;
	}
}

bool is_implicitly_convertible(
	ast::typespec_view dest,
	ast::expression const &expr,
	ctx::parse_context &context
)
{
	if (expr.is_if_expr())
	{
		auto const &if_expr = expr.get_if_expr();
		return is_implicitly_convertible(dest, if_expr.then_block, context)
			&& is_implicitly_convertible(dest, if_expr.else_block, context);
	}
	else if (expr.is_switch_expr())
	{
		auto const &switch_expr = expr.get_switch_expr();
		auto const is_default_convirtible = switch_expr.default_case.is_null()
			|| is_implicitly_convertible(dest, switch_expr.default_case, context);
		return is_default_convirtible
			&& switch_expr.cases
				.member<&ast::switch_case::expr>()
				.filter([](auto const &expr) { return expr.not_null(); })
				.is_all([&](auto const &) { return is_implicitly_convertible(dest, expr, context); });
	}
	else if (expr.is_literal())
	{
		return is_literal_implicitly_convertible(dest, expr);
	}

	bz_assert(!dest.is<ast::ts_const>());
	bz_assert(!dest.is<ast::ts_consteval>());
	auto const [expr_type, expr_type_kind] = expr.get_expr_type_and_kind();
	return is_implicitly_convertible(dest, expr_type, expr_type_kind, context);
}

} // namespace resolve
