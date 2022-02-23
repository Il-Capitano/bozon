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
		auto const dest_info = dest.get<ast::ts_base_type>().info;
		auto const expr_info = expr_type_without_const.get<ast::ts_base_type>().info;
		if (
			(ast::is_signed_integer_kind(dest_info->kind) && ast::is_signed_integer_kind(expr_info->kind))
			|| (ast::is_unsigned_integer_kind(dest_info->kind) && ast::is_unsigned_integer_kind(expr_info->kind))
		)
		{
			return dest_info->kind >= expr_info->kind;
		}
	}
	return false;
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
	bz_assert(!dest.is<ast::ts_const>());
	bz_assert(!dest.is<ast::ts_consteval>());
	auto const [expr_type, expr_type_kind] = expr.get_expr_type_and_kind();
	return is_implicitly_convertible(dest, expr_type, expr_type_kind, context);
}

} // namespace resolve
