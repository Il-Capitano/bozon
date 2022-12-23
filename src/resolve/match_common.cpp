#include "match_common.h"

namespace resolve
{

void expand_variadic_tuple_type(ast::arena_vector<ast::typespec> &tuple_types, size_t new_size)
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

bool is_integer_implicitly_convertible(uint8_t dest_kind, ast::literal_kind kind, uint64_t value)
{
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

	if (kind == ast::literal_kind::integer)
	{
		return value <= dest_max_value;
	}
	else if (kind == ast::literal_kind::signed_integer)
	{
		return ast::is_signed_integer_kind(dest_kind) && value <= dest_max_value;
	}
	else if (kind == ast::literal_kind::unsigned_integer)
	{
		return ast::is_unsigned_integer_kind(dest_kind) && value <= dest_max_value;
	}
	else
	{
		return false;
	}
}

bool is_integer_implicitly_convertible(uint8_t dest_kind, ast::literal_kind kind, int64_t value)
{
	auto const [dest_min_value, dest_max_value] = [&]() -> std::pair<int64_t, int64_t> {
		switch (dest_kind)
		{
		case ast::type_info::int8_:
			return {
				static_cast<int64_t>(std::numeric_limits<int8_t>::min()),
				static_cast<int64_t>(std::numeric_limits<int8_t>::max())
			};
		case ast::type_info::int16_:
			return {
				static_cast<int64_t>(std::numeric_limits<int16_t>::min()),
				static_cast<int64_t>(std::numeric_limits<int16_t>::max())
			};
		case ast::type_info::int32_:
			return {
				static_cast<int64_t>(std::numeric_limits<int32_t>::min()),
				static_cast<int64_t>(std::numeric_limits<int32_t>::max())
			};
		case ast::type_info::int64_:
			return {
				static_cast<int64_t>(std::numeric_limits<int64_t>::min()),
				static_cast<int64_t>(std::numeric_limits<int64_t>::max())
			};
		case ast::type_info::uint8_:
			return {
				static_cast<int64_t>(std::numeric_limits<uint8_t>::min()),
				static_cast<int64_t>(std::numeric_limits<uint8_t>::max())
			};
		case ast::type_info::uint16_:
			return {
				static_cast<int64_t>(std::numeric_limits<uint16_t>::min()),
				static_cast<int64_t>(std::numeric_limits<uint16_t>::max())
			};
		case ast::type_info::uint32_:
			return {
				static_cast<int64_t>(std::numeric_limits<uint32_t>::min()),
				static_cast<int64_t>(std::numeric_limits<uint32_t>::max())
			};
		case ast::type_info::uint64_:
			return {
				static_cast<int64_t>(std::numeric_limits<uint64_t>::min()),
				static_cast<int64_t>(std::numeric_limits<int64_t>::max()) // avoid overflow
			};
		default:
			bz_unreachable;
		}
	}();

	if (kind == ast::literal_kind::integer)
	{
		return value >= dest_min_value && value <= dest_max_value;
	}
	else if (kind == ast::literal_kind::signed_integer)
	{
		return ast::is_signed_integer_kind(dest_kind) && value >= dest_min_value && value <= dest_max_value;
	}
	else if (kind == ast::literal_kind::unsigned_integer)
	{
		return ast::is_unsigned_integer_kind(dest_kind) && value >= dest_min_value && value <= dest_max_value;
	}
	else
	{
		return false;
	}
}

static bool is_integer_literal_implicitly_convertible(
	ast::typespec_view dest,
	ast::expression const &expr
)
{
	bz_assert(expr.is_integer_literal());
	auto const [kind, value] = expr.get_integer_literal_kind_and_value();

	if (!dest.is<ast::ts_base_type>())
	{
		return false;
	}

	auto const dest_kind = dest.get<ast::ts_base_type>().info->kind;
	if (!ast::is_integer_kind(dest_kind) || (!value.is_sint() && !value.is_uint()))
	{
		return false;
	}

	return value.is_sint()
		? is_integer_implicitly_convertible(dest_kind, kind, value.get_sint())
		: is_integer_implicitly_convertible(dest_kind, kind, value.get_uint());
}

static bool is_null_literal_implicitly_convertible(ast::typespec_view dest)
{
	return ast::is_complete(dest) && dest.is<ast::ts_optional>();
}

static bool is_enum_literal_implicitly_convertible(
	ast::typespec_view dest,
	ast::expression const &expr
)
{
	bz_assert(expr.is_enum_literal());
	auto const &enum_literal = expr.get_enum_literal();

	if (!dest.is<ast::ts_enum>())
	{
		return false;
	}

	auto const dest_enum_values = dest.get<ast::ts_enum>().decl->values.as_array_view();
	return dest_enum_values.is_any([name = enum_literal.id->value](auto const &name_and_value) {
		return name == name_and_value.id->value;
	});
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
		auto const is_default_convertible = switch_expr.default_case.is_null()
			|| is_implicitly_convertible(dest, switch_expr.default_case, context);
		return is_default_convertible
			&& switch_expr.cases
				.member<&ast::switch_case::expr>()
				.filter([](auto const &expr) { return expr.not_null(); })
				.is_all([&](auto const &) { return is_implicitly_convertible(dest, expr, context); });
	}
	else if (expr.is_integer_literal())
	{
		return is_integer_literal_implicitly_convertible(dest, expr);
	}
	else if (expr.is_null_literal())
	{
		return is_null_literal_implicitly_convertible(dest);
	}
	else if (expr.is_enum_literal())
	{
		return is_enum_literal_implicitly_convertible(dest, expr);
	}

	bz_assert(!dest.is<ast::ts_const>());
	bz_assert(!dest.is<ast::ts_consteval>());
	auto const [expr_type, expr_type_kind] = expr.get_expr_type_and_kind();
	return is_implicitly_convertible(dest, expr_type, expr_type_kind, context);
}

} // namespace resolve
