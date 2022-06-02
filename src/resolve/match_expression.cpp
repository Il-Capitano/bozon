#include "match_expression.h"
#include "match_common.h"
#include "match_to_type.h"
#include "consteval.h"

namespace resolve
{

enum class type_match_result
{
	good, needs_cast, error
};

[[nodiscard]] static type_match_result strict_match_types(
	ast::typespec &dest_container,
	ast::typespec_view source,
	ast::typespec_view dest,
	bool accept_void,
	bool propagate_const,
	bool top_level = true
)
{
	bz_assert(ast::is_complete(source));
	while (true)
	{
		// remove consts if there are any
		{
			auto const dest_is_const = dest.is<ast::ts_const>();
			auto const source_is_const = source.is<ast::ts_const>();

			if (
				(!dest_is_const && source_is_const)
				|| (!propagate_const && dest_is_const && !source_is_const)
			)
			{
				return type_match_result::error;
			}


			if (top_level)
			{
				top_level = false;
			}
			else if (!dest_is_const)
			{
				propagate_const = false;
			}

			if (dest_is_const)
			{
				dest = dest.blind_get();
			}
			if (source_is_const)
			{
				source = source.blind_get();
			}
		}

		if (dest.kind() == source.kind() && dest.is_safe_blind_get())
		{
			bz_assert(source.is_safe_blind_get());
			dest = dest.blind_get();
			source = source.blind_get();
		}
		else
		{
			break;
		}
	}
	bz_assert(!dest.is<ast::ts_unresolved>());
	bz_assert(!source.is<ast::ts_unresolved>());

	if (dest.is<ast::ts_optional>() && dest.get<ast::ts_optional>().is<ast::ts_auto>() && source.is<ast::ts_optional_pointer>())
	{
		dest_container.copy_from(dest, source);
		return type_match_result::good;
	}
	else if (dest.is<ast::ts_auto>() && !source.is<ast::ts_const>())
	{
		bz_assert(!source.is<ast::ts_consteval>());
		dest_container.copy_from(dest, source);
		return type_match_result::good;
	}
	else if (dest == source)
	{
		return type_match_result::good;
	}
	else if (accept_void && dest.is<ast::ts_void>())
	{
		return type_match_result::needs_cast;
	}
	else if (
		dest.is<ast::ts_base_type>() && dest.get<ast::ts_base_type>().info->is_generic()
		&& source.is<ast::ts_base_type>() && source.get<ast::ts_base_type>().info->is_generic_instantiation()
		&& source.get<ast::ts_base_type>().info->generic_parent == dest.get<ast::ts_base_type>().info
	)
	{
		bz_assert(dest_container.nodes.back().is<ast::ts_base_type>());
		dest_container.nodes.back().get<ast::ts_base_type>() = source.get<ast::ts_base_type>();
		return type_match_result::good;
	}
	else if (dest.is<ast::ts_tuple>() && source.is<ast::ts_tuple>())
	{
		bz_assert(dest_container.nodes.back().is<ast::ts_tuple>());
		auto &dest_types = dest_container.nodes.back().get<ast::ts_tuple>().types;
		auto &source_types = source.get<ast::ts_tuple>().types;
		if (dest_types.not_empty() && dest_types.back().is<ast::ts_variadic>())
		{
			expand_variadic_tuple_type(dest_types, source_types.size());
		}
		if (dest_types.size() != source_types.size())
		{
			return type_match_result::error;
		}
		type_match_result result = type_match_result::good;
		for (auto const [dest_elem, source_elem] : bz::zip(dest_types, source_types))
		{
			result = std::max(result, strict_match_types(dest_elem, source_elem, dest_elem, false, propagate_const, false));
		}
		return result;
	}
	else if (dest.is<ast::ts_array_slice>() && source.is<ast::ts_array_slice>())
	{
		bz_assert(dest_container.nodes.back().is<ast::ts_array_slice>());
		return strict_match_types(
			dest_container.nodes.back().get<ast::ts_array_slice>().elem_type,
			source.get<ast::ts_array_slice>().elem_type,
			dest.get<ast::ts_array_slice>().elem_type,
			false, propagate_const, false
		);
	}
	else if (dest.is<ast::ts_array>() && source.is<ast::ts_array>())
	{
		if (dest.get<ast::ts_array>().size != source.get<ast::ts_array>().size)
		{
			return type_match_result::error;
		}

		bz_assert(dest_container.nodes.back().is<ast::ts_array>());
		return strict_match_types(
			dest_container.nodes.back().get<ast::ts_array>().elem_type,
			source.get<ast::ts_array>().elem_type,
			dest.get<ast::ts_array>().elem_type,
			false, propagate_const, false
		);
	}
	else
	{
		return type_match_result::error;
	}
}

static bool match_typename_to_type_impl(
	lex::src_tokens const &src_tokens,
	ast::typespec_view original_source,
	ast::typespec_view original_dest,
	ast::typespec_view source,
	ast::typespec_view dest,
	ctx::parse_context &context
)
{
	bz_assert(dest.is_typename());

	if (!ast::is_complete(source))
	{
		context.report_error(
			src_tokens,
			bz::format("couldn't match non-complete type '{}' to typename type '{}'", original_source, original_dest)
		);
		return false;
	}

	while (dest.kind() == source.kind() && dest.is_safe_blind_get() && source.is_safe_blind_get())
	{
		dest = dest.blind_get();
		source = source.blind_get();
	}

	if (dest.is<ast::ts_typename>())
	{
		return true;
	}
	else if (dest.kind() != source.kind())
	{
		context.report_error(src_tokens, bz::format("couldn't match type '{}' to typename type '{}'", original_source, original_dest));
		return false;
	}
	else if (dest.is<ast::ts_array>())
	{
		if (dest.get<ast::ts_array>().size != source.get<ast::ts_array>().size)
		{
			context.report_error(
				src_tokens,
				bz::format("couldn't match type '{}' to typename type '{}'", original_source, original_dest),
				{ context.make_note(
					src_tokens,
					bz::format("array types '{}' and '{}' have different sizes", source, dest)
				) }
			);
			return false;
		}

		return match_typename_to_type_impl(
			src_tokens,
			original_source, original_dest,
			source.get<ast::ts_array>().elem_type,
			dest.get<ast::ts_array>().elem_type,
			context
		);
	}
	else if (dest.is<ast::ts_array_slice>())
	{
		return match_typename_to_type_impl(
			src_tokens,
			original_source, original_dest,
			source.get<ast::ts_array_slice>().elem_type, dest.get<ast::ts_array_slice>().elem_type,
			context
		);
	}
	else if (dest.is<ast::ts_tuple>())
	{
		auto &dest_types = dest.get<ast::ts_tuple>().types;
		auto const &source_types = source.get<ast::ts_tuple>().types;
		if (dest_types.size() != source_types.size())
		{
			context.report_error(
				src_tokens,
				bz::format("couldn't match type '{}' to typename type '{}'", original_source, original_dest),
				{ context.make_note(
					src_tokens,
					bz::format("tuple types '{}' and '{}' have different element counts", source, dest)
				) }
			);
			return false;
		}

		auto const size = dest_types.size();
		auto const non_typename_good = bz::iota(0, size)
			.filter([&](auto const i) { return !dest_types[i].is_typename(); })
			.is_all([&](auto const i) {
				return dest_types[i] == source_types[i];
			});
		if (!non_typename_good)
		{
			auto notes = bz::iota(0, size)
				.filter([&](auto const i) { return !dest_types[i].is_typename(); })
				.transform([&](auto const i) {
					return context.make_note(
						src_tokens,
						bz::format("different types '{}' and '{}' at position {}", source_types[i], dest_types[i], i)
					);
				})
				.collect();
			context.report_error(
				src_tokens,
				bz::format("couldn't match type '{}' to typename type '{}'", original_source, original_dest),
				std::move(notes)
			);
			return false;
		}
		return bz::iota(0, size)
			.filter([&](auto const i) { return dest_types[i].is_typename(); })
			.is_all([&](auto const i) {
				return match_typename_to_type_impl(
					src_tokens,
					source_types[i], dest_types[i],
					source_types[i], dest_types[i],
					context
				);
			});
	}
	else
	{
		bz_unreachable;
	}
}

[[nodiscard]] static type_match_result match_types(
	ast::typespec &dest_container,
	ast::typespec_view expr_type,
	ast::typespec_view dest,
	ast::expression_type_kind expr_type_kind,
	ctx::parse_context &context
)
{
	dest = ast::remove_const_or_consteval(dest);

	bz_assert(ast::is_complete(expr_type));
	auto const expr_type_without_const = ast::remove_const_or_consteval(expr_type);

	if (dest.is<ast::ts_optional_pointer>() && expr_type_without_const.is<ast::ts_pointer>())
	{
		auto const match_result = strict_match_types(
			dest_container,
			expr_type_without_const,
			dest,
			true, true
		);
		return std::max(match_result, type_match_result::needs_cast);
	}
	else if (
		(dest.is<ast::ts_pointer>() && expr_type_without_const.is<ast::ts_pointer>())
		|| (dest.is<ast::ts_optional_pointer>() && expr_type_without_const.is<ast::ts_optional_pointer>())
	)
	{
		return strict_match_types(
			dest_container,
			expr_type_without_const,
			dest,
			true, true
		);
	}
	else if (dest.is<ast::ts_lvalue_reference>())
	{
		if (!ast::is_lvalue(expr_type_kind))
		{
			return type_match_result::error;
		}

		auto const inner_dest = dest.get<ast::ts_lvalue_reference>();
		if (!inner_dest.is<ast::ts_const>() && expr_type.is<ast::ts_const>())
		{
			return type_match_result::error;
		}

		return strict_match_types(
			dest_container,
			expr_type,
			inner_dest,
			false, true, false
		);
	}
	else if (dest.is<ast::ts_move_reference>())
	{
		if (!ast::is_rvalue(expr_type_kind))
		{
			return type_match_result::error;
		}

		auto const inner_dest = dest.get<ast::ts_move_reference>();
		return strict_match_types(dest_container, expr_type_without_const, inner_dest, false, true, false);
	}
	else if (dest.is<ast::ts_auto_reference>())
	{
		bz_assert(dest_container.nodes.front().is<ast::ts_auto_reference>());
		if (ast::is_lvalue(expr_type_kind))
		{
			dest_container.nodes.front().emplace<ast::ts_lvalue_reference>();
			dest = dest_container;

			if (!dest.get<ast::ts_lvalue_reference>().is<ast::ts_const>() && expr_type.is<ast::ts_const>())
			{
				return type_match_result::error;
			}
		}
		else
		{
			dest_container.remove_layer();
			dest = dest_container;
		}
		return strict_match_types(dest_container, expr_type, ast::remove_lvalue_reference(dest), false, true, false);
	}
	else if (dest.is<ast::ts_auto_reference_const>())
	{
		bz_assert(dest_container.nodes.front().is<ast::ts_auto_reference_const>());
		auto const inner_dest = dest.get<ast::ts_auto_reference_const>();
		bz_assert(!inner_dest.is<ast::ts_const>());
		if (ast::is_lvalue(expr_type_kind) && expr_type.is<ast::ts_const>())
		{
			dest_container.nodes.front().emplace<ast::ts_const>();
			dest_container.add_layer<ast::ts_lvalue_reference>();
			dest = dest_container;
		}
		else if (ast::is_lvalue(expr_type_kind))
		{
			dest_container.nodes.front().emplace<ast::ts_lvalue_reference>();
			dest = dest_container;
		}
		else
		{
			dest_container.remove_layer();
			dest = dest_container;
		}
		return strict_match_types(dest_container, expr_type, ast::remove_lvalue_reference(dest), false, true, false);
	}

	if (dest.is<ast::ts_auto>())
	{
		dest_container.copy_from(dest, expr_type_without_const);
		dest = ast::remove_const_or_consteval(dest_container);
		return type_match_result::good;
	}
	else if (
		dest.is<ast::ts_base_type>()
		&& dest.get<ast::ts_base_type>().info->is_generic()
		&& expr_type_without_const.is<ast::ts_base_type>()
		&& expr_type_without_const.get<ast::ts_base_type>().info->is_generic_instantiation()
		&& expr_type_without_const.get<ast::ts_base_type>().info->generic_parent == dest.get<ast::ts_base_type>().info
	)
	{
		bz_assert(dest_container.nodes.back().is<ast::ts_base_type>());
		dest_container.nodes.back().get<ast::ts_base_type>() = expr_type_without_const.get<ast::ts_base_type>();
		return type_match_result::good;
	}
	else if (dest == expr_type_without_const)
	{
		return type_match_result::good;
	}
	else if (dest.is<ast::ts_tuple>() && expr_type_without_const.is<ast::ts_tuple>())
	{
		bz_assert(dest_container.nodes.back().is<ast::ts_tuple>());
		auto &dest_tuple_types = dest_container.nodes.back().get<ast::ts_tuple>().types;
		auto const &expr_tuple_types = expr_type_without_const.get<ast::ts_tuple>().types;
		type_match_result result = type_match_result::good;
		if (dest_tuple_types.not_empty() && dest_tuple_types.back().is<ast::ts_variadic>())
		{
			expand_variadic_tuple_type(dest_tuple_types, expr_tuple_types.size());
		}

		if (dest_tuple_types.size() != expr_tuple_types.size())
		{
			return type_match_result::error;
		}

		for (auto const &[expr_elem_t, dest_elem_t] : bz::zip(expr_tuple_types, dest_tuple_types))
		{
			if (
				(
					dest_elem_t.is<ast::ts_lvalue_reference>()
					&& !dest_elem_t.get<ast::ts_lvalue_reference>().is<ast::ts_const>()
					&& expr_type.is<ast::ts_const>()
				)
				|| (
					ast::is_lvalue(expr_type_kind)
					&& dest_elem_t.is<ast::ts_auto_reference_const>()
					&& !dest_elem_t.get<ast::ts_lvalue_reference>().is<ast::ts_const>()
					&& expr_type.is<ast::ts_const>()
				)
			)
			{
				result = type_match_result::error;
			}
			else if (expr_elem_t.is<ast::ts_lvalue_reference>())
			{
				if (!dest_elem_t.is<ast::ts_lvalue_reference>())
				{
					result = std::max(result, type_match_result::needs_cast);
				}
				result = std::max(result, match_types(
					dest_elem_t,
					expr_elem_t.get<ast::ts_lvalue_reference>(),
					dest_elem_t,
					ast::expression_type_kind::lvalue_reference,
					context
				));
			}
			else
			{
				if (dest_elem_t.is<ast::ts_lvalue_reference>())
				{
					result = std::max(result, type_match_result::needs_cast);
				}
				result = std::max(result, match_types(
					dest_elem_t,
					expr_elem_t,
					dest_elem_t,
					expr_type_kind,
					context
				));
			}
		}
		return result;
	}
	else if (dest.is<ast::ts_array_slice>() && expr_type_without_const.is<ast::ts_array_slice>())
	{
		bz_assert(dest_container.nodes.back().is<ast::ts_array_slice>());
		auto &inner_container = dest_container.nodes.back().get<ast::ts_array_slice>().elem_type;
		return strict_match_types(
			inner_container,
			expr_type_without_const.get<ast::ts_array_slice>().elem_type,
			dest.get<ast::ts_array_slice>().elem_type,
			false, true
		);
	}
	else if (dest.is<ast::ts_array_slice>() && expr_type_without_const.is<ast::ts_array>())
	{
		bz_assert(dest_container.nodes.back().is<ast::ts_array_slice>());
		auto &inner_container = dest_container.nodes.back().get<ast::ts_array_slice>().elem_type;
		auto const dest_elem_t = dest.get<ast::ts_array_slice>().elem_type.as_typespec_view();
		auto const expr_elem_t = expr_type_without_const.get<ast::ts_array>().elem_type.as_typespec_view();
		auto const is_const_dest_elem_t = dest_elem_t.is<ast::ts_const>();
		auto const is_const_expr_elem_t = expr_type.is<ast::ts_const>();

		if (is_const_expr_elem_t && !is_const_dest_elem_t)
		{
			return type_match_result::error;
		}
		else
		{
			auto const strict_result = strict_match_types(
				inner_container,
				expr_elem_t,
				dest_elem_t,
				false, is_const_dest_elem_t
			);
			return std::max(strict_result, type_match_result::needs_cast);
		}
	}
	else if (dest.is<ast::ts_array>() && expr_type_without_const.is<ast::ts_array>())
	{
		bz_assert(dest_container.nodes.back().is<ast::ts_array>());
		auto &inner_container = dest_container.nodes.back().get<ast::ts_array>().elem_type;
		auto const &dest_array_type = dest.get<ast::ts_array>();
		auto const &expr_array_type = expr_type_without_const.get<ast::ts_array>();
		if (dest_array_type.size != expr_array_type.size)
		{
			return type_match_result::error;
		}
		return strict_match_types(
			inner_container,
			expr_array_type.elem_type,
			dest_array_type.elem_type,
			false, true
		);
	}
	else if (is_implicitly_convertible(dest, expr_type, expr_type_kind, context))
	{
		return type_match_result::needs_cast;
	}
	else
	{
		return type_match_result::error;
	}
}

static void match_type_base_case(
	ast::expression &expr,
	ast::typespec &dest_container,
	ast::typespec_view dest,
	ctx::parse_context &context
)
{
	auto const [expr_type, expr_type_kind] = expr.get_expr_type_and_kind();
	auto const match_result = match_types(dest_container, expr_type, ast::remove_const_or_consteval(dest), expr_type_kind, context);

	switch (match_result)
	{
	case type_match_result::good:
		// nothing
		break;
	case type_match_result::needs_cast:
		expr = context.make_cast_expression(expr.src_tokens, std::move(expr), dest_container);
		break;
	case type_match_result::error:
		context.report_error(expr, bz::format("cannot implicitly convert expression from type '{}' to '{}'", expr_type, dest_container));
		if (!ast::is_complete(dest_container))
		{
			dest_container.clear();
		}
		expr.to_error();
		return;
	}
}

static void match_integer_literal_to_type(
	ast::expression &expr,
	ast::typespec &dest_container,
	ast::typespec_view dest,
	ctx::parse_context &context
)
{
	bz_assert(expr.is_integer_literal());

	if (!dest.is<ast::ts_base_type>())
	{
		// return to base case if the matched type isn't a base type
		match_type_base_case(expr, dest_container, dest, context);
		return;
	}

	auto const dest_kind = dest.get<ast::ts_base_type>().info->kind;
	if (!ast::is_integer_kind(dest_kind))
	{
		// return to base case if the matched type isn't an integer type
		match_type_base_case(expr, dest_container, dest, context);
		return;
	}

	auto const [kind, value] = expr.get_integer_literal_kind_and_value();
	bz_assert((value.is_any<ast::constant_value::sint, ast::constant_value::uint>()));
	auto const is_convertible = value.is<ast::constant_value::sint>()
		? is_integer_implicitly_convertible(dest_kind, kind, value.get<ast::constant_value::sint>())
		: is_integer_implicitly_convertible(dest_kind, kind, value.get<ast::constant_value::uint>());

	if (kind == ast::literal_kind::signed_integer)
	{
		if (!ast::is_signed_integer_kind(dest_kind))
		{
			context.report_error(
				expr,
				bz::format("cannot implicitly convert signed integer literal to unsigned integer type '{}'", dest_container)
			);
			bz_assert(ast::is_complete(dest_container));
			expr.to_error();
			return;
		}
	}
	else if (kind == ast::literal_kind::unsigned_integer)
	{
		if (!ast::is_unsigned_integer_kind(dest_kind))
		{
			context.report_error(
				expr,
				bz::format("cannot implicitly convert unsigned integer literal to signed integer type '{}'", dest_container)
			);
			bz_assert(ast::is_complete(dest_container));
			expr.to_error();
			return;
		}
	}

	if (!is_convertible)
	{
		bz::vector<ctx::source_highlight> notes = {};
		if (value.is<ast::constant_value::sint>())
		{
			notes.push_back(context.make_note(
				expr,
				bz::format(
					"value of {} is outside the range for type '{}'",
					value.get<ast::constant_value::sint>(), dest_container
				)
			));
		}
		else
		{
			notes.push_back(context.make_note(
				expr,
				bz::format(
					"value of {} is outside the range for type '{}'",
					value.get<ast::constant_value::uint>(), dest_container
				)
			));
		}
		context.report_error(
			expr,
			bz::format(
				"cannot implicitly convert expression from type '{}' to '{}'",
				expr.get_expr_type(), dest_container
			),
			std::move(notes)
		);
		bz_assert(ast::is_complete(dest_container));
		expr.to_error();
		return;
	}
	else if (
		((kind == ast::literal_kind::integer || kind == ast::literal_kind::signed_integer) && dest_kind == ast::type_info::int32_)
		|| (kind == ast::literal_kind::unsigned_integer && dest_kind == ast::type_info::uint32_)
	)
	{
		// default literal type doesn't need cast
		bz_assert(dest == expr.get_expr_type());
	}
	else
	{
		expr = context.make_cast_expression(expr.src_tokens, std::move(expr), dest_container);
	}
}

static void match_null_literal_to_type(
	ast::expression &expr,
	ast::typespec &dest_container,
	ast::typespec_view dest,
	ctx::parse_context &context
)
{
	bz_assert(expr.is_null_literal());

	if (dest.is<ast::ts_optional_pointer>() || dest.is<ast::ts_optional>())
	{
		expr = ast::make_constant_expression(
			expr.src_tokens,
			ast::expression_type_kind::rvalue, dest,
			ast::constant_value(ast::internal::null_t{}),
			ast::make_expr_builtin_default_construct(dest)
		);
	}
	else
	{
		match_type_base_case(expr, dest_container, dest, context);
	}
}

static void match_expression_to_type_impl(
	ast::expression &expr,
	ast::typespec &dest_container,
	ast::typespec_view dest,
	ctx::parse_context &context
)
{
	dest = ast::remove_const_or_consteval(dest);

	bz_assert(!dest.is<ast::ts_unresolved>());
	// basically a slightly different implementation of get_type_match_level
	if (expr.is_if_expr())
	{
		auto &if_expr = expr.get_if_expr();
		if (if_expr.then_block.is_noreturn() && !if_expr.else_block.is_noreturn())
		{
			match_expression_to_type_impl(if_expr.else_block, dest_container, dest, context);
			return;
		}
		else if (!if_expr.then_block.is_noreturn() && if_expr.else_block.is_noreturn())
		{
			match_expression_to_type_impl(if_expr.then_block, dest_container, dest, context);
			return;
		}
		else if (ast::is_complete(dest))
		{
			match_expression_to_type_impl(if_expr.then_block, dest_container, dest, context);
			match_expression_to_type_impl(if_expr.else_block, dest_container, dest, context);
			return;
		}
		else
		{
			ast::typespec then_matched_type = dest_container;
			ast::typespec else_matched_type = dest_container;
			match_expression_to_type(if_expr.then_block, then_matched_type, context);
			match_expression_to_type(if_expr.else_block, else_matched_type, context);

			if (then_matched_type.is_empty() || else_matched_type.is_empty())
			{
				expr.to_error();
				bz_assert(!ast::is_complete(dest_container));
				dest_container.clear();
				return;
			}

			if (then_matched_type == else_matched_type)
			{
				match_expression_to_type_impl(if_expr.then_block, dest_container, dest_container, context);
				match_expression_to_type_impl(if_expr.else_block, dest_container, dest_container, context);
				expr.set_type(std::move(then_matched_type));
				return;
			}

			auto after_match_then_result = get_type_match_level(else_matched_type, if_expr.then_block, context);
			auto after_match_else_result = get_type_match_level(then_matched_type, if_expr.else_block, context);
			if (after_match_then_result.is_null() && after_match_else_result.is_null())
			{
				context.report_error(
					expr,
					bz::format("couldn't match the two branches of the if expression at the same time to type '{}'", dest_container),
					{
						context.make_note(
							if_expr.then_block,
							bz::format("resulting type from matching the then branch is '{}'", then_matched_type)
						),
						context.make_note(
							if_expr.else_block,
							bz::format("resulting type from matching the else branch is '{}'", else_matched_type)
						)
					}
				);
				expr.to_error();
				if (!ast::is_complete(dest_container))
				{
					dest_container.clear();
				}
				return;
			}
			else if (after_match_then_result.is_null())
			{
				// match to the then branch first
				match_expression_to_type_impl(if_expr.then_block, dest_container, dest_container, context);
				match_expression_to_type_impl(if_expr.else_block, dest_container, dest_container, context);
				expr.set_type(std::move(then_matched_type));
				return;
			}
			else if (after_match_else_result.is_null())
			{
				// match to the else branch first
				match_expression_to_type_impl(if_expr.else_block, dest_container, dest_container, context);
				match_expression_to_type_impl(if_expr.then_block, dest_container, dest_container, context);
				expr.set_type(std::move(else_matched_type));
				return;
			}
			else
			{
				context.report_error(
					expr,
					bz::format("matching the two branches of the if expression to type '{}' is ambiguous", dest_container),
					{
						context.make_note(
							if_expr.then_block,
							bz::format("resulting type from matching the then branch is '{}'", then_matched_type)
						),
						context.make_note(
							if_expr.else_block,
							bz::format("resulting type from matching the else branch is '{}'", else_matched_type)
						)
					}
				);
				expr.to_error();
				if (!ast::is_complete(dest_container))
				{
					dest_container.clear();
				}
				return;
			}
		}
	}
	else if (expr.is_switch_expr())
	{
		auto &switch_expr = expr.get_switch_expr();
		if (switch_expr.cases.empty() && switch_expr.default_case.is_null())
		{
			context.report_error(expr, bz::format("unable to match empty switch expression to type '{}'", dest_container));
			expr.to_error();
			if (!ast::is_complete(dest_container))
			{
				dest_container.clear();
			}
			return;
		}
		else if (switch_expr.cases.empty())
		{
			match_expression_to_type_impl(switch_expr.default_case, dest_container, dest, context);
			if (switch_expr.default_case.is_error())
			{
				expr.to_error();
				// dest_container should have been cleared by match_expression_to_type_impl if necessary
				return;
			}
		}
		else if (ast::is_complete(dest))
		{
			for (auto &[_, case_expr] : switch_expr.cases)
			{
				if (case_expr.is_noreturn())
				{
					continue;
				}
				match_expression_to_type_impl(case_expr, dest_container, dest_container, context);
			}
		}
		else
		{
			size_t first_matched_index = size_t(-1); // the first iteration sets it to 0 by overflowing this value
			bool is_matched = false;
			bool is_error = false;
			ast::typespec matched_type;
			for (auto &[_, case_expr] : switch_expr.cases)
			{
				if (!is_matched)
				{
					first_matched_index += 1;
				}
				if (case_expr.is_noreturn())
				{
					continue;
				}
				auto dest_container_copy = dest_container;
				match_expression_to_type_impl(case_expr, dest_container_copy, dest_container_copy, context);
				if (case_expr.is_error())
				{
					is_error = true;
				}
				else if (!is_matched)
				{
					is_matched = true;
					matched_type = dest_container_copy;
				}
				else if (matched_type != dest_container_copy)
				{
					context.report_error(
						expr, "different types deduced for different cases in switch expression",
						{
							context.make_note(switch_expr.cases[first_matched_index].expr, bz::format("type was first deduced as '{}'", matched_type)),
							context.make_note(case_expr, bz::format("type was later deduced as '{}'", dest_container_copy)),
						}
					);
				}
			}
			if (switch_expr.default_case.not_null() && !switch_expr.default_case.is_noreturn())
			{
				auto dest_container_copy = dest_container;
				match_expression_to_type_impl(switch_expr.default_case, dest_container_copy, dest_container_copy, context);
				if (switch_expr.default_case.is_error())
				{
					is_error = true;
				}
				else if (!is_matched)
				{
					is_matched = true;
					matched_type = dest_container_copy;
				}
				else if (matched_type != dest_container_copy)
				{
					context.report_error(
						expr, "different types deduced for different cases in switch expression",
						{
							context.make_note(switch_expr.cases[first_matched_index].expr, bz::format("type was first deduced as '{}'", matched_type)),
							context.make_note(switch_expr.default_case, bz::format("type was later deduced as '{}'", dest_container_copy)),
						}
					);
				}
			}

			if (is_error)
			{
				expr.to_error();
				dest_container.clear();
				return;
			}
			else if (is_matched)
			{
				expr.set_type(ast::remove_lvalue_reference(ast::remove_const_or_consteval(matched_type)));
				expr.set_type_kind(dest_container.is<ast::ts_lvalue_reference>() ? ast::expression_type_kind::lvalue_reference : ast::expression_type_kind::rvalue);
				dest_container = std::move(matched_type);
			}
			else
			{
				match_expression_to_type_impl(
					switch_expr.cases.not_empty() ? switch_expr.cases.front().expr : switch_expr.default_case,
					dest_container, dest_container, context
				);
			}
		}
	}
	else if (expr.is_typename())
	{
		if (!dest.is_typename())
		{
			context.report_error(expr, bz::format("cannot match type '{}' to a non-typename type '{}'", expr.get_typename(), dest_container));
			expr.to_error();
			if (!ast::is_complete(dest_container))
			{
				dest_container.clear();
			}
			return;
		}

		auto const is_good = match_typename_to_type_impl(expr.src_tokens, expr.get_typename(), dest_container, expr.get_typename(), dest, context);
		if (!is_good)
		{
			expr.to_error();
			if (!ast::is_complete(dest_container))
			{
				dest_container.clear();
			}
			return;
		}
	}
	else if (expr.is_tuple())
	{
		if (expr.is_constant())
		{
			auto &const_expr = expr.get_constant();
			auto kind = const_expr.kind;
			auto type = std::move(const_expr.type);
			auto inner_expr = std::move(const_expr.expr);
			expr.emplace<ast::dynamic_expression>(kind, std::move(type), std::move(inner_expr));
			expr.consteval_state = ast::expression::consteval_never_tried;
		}

		if (dest_container.is<ast::ts_auto_reference>() || dest_container.is<ast::ts_auto_reference_const>())
		{
			dest_container.remove_layer();
			dest = dest_container;
		}
		auto const dest_without_const = ast::remove_const_or_consteval(dest);
		if (dest_without_const.is<ast::ts_auto>())
		{
			auto &tuple_expr = expr.get_tuple();
			ast::typespec tuple_type = ast::make_tuple_typespec(dest_without_const.src_tokens, {});
			auto &tuple_type_vec = tuple_type.nodes.front().get<ast::ts_tuple>().types;
			tuple_type_vec.resize(tuple_expr.elems.size());
			for (auto &type : tuple_type_vec)
			{
				type = ast::make_auto_typespec(nullptr);
			}
			for (auto const &[expr, type] : bz::zip(tuple_expr.elems, tuple_type_vec))
			{
				match_expression_to_type_impl(expr, type, type, context);
			}

			if (tuple_expr.elems.is_any([](auto const &elem) { return elem.is_error(); }))
			{
				if (!ast::is_complete(dest_container))
				{
					dest_container.clear();
				}
				expr.to_error();
				return;
			}

			expr.set_type(tuple_type);
			dest_container.move_from(dest_without_const, tuple_type);
		}
		else if (dest_without_const.is<ast::ts_tuple>())
		{
			bz_assert(dest_container.nodes.back().is<ast::ts_tuple>());
			auto &dest_tuple_types = dest_container.nodes.back().get<ast::ts_tuple>().types;
			auto &expr_tuple_elems = expr.get_tuple().elems;

			if (dest_tuple_types.not_empty() && dest_tuple_types.back().is<ast::ts_variadic>())
			{
				expand_variadic_tuple_type(dest_tuple_types, expr_tuple_elems.size());
			}

			if (dest_tuple_types.size() == expr_tuple_elems.size())
			{
				for (auto const &[expr, type] : bz::zip(expr_tuple_elems, dest_tuple_types))
				{
					match_expression_to_type_impl(expr, type, type, context);
				}
				expr.set_type(dest_without_const);
			}

			if (expr_tuple_elems.is_any([](auto const &elem) { return elem.is_error(); }))
			{
				if (!ast::is_complete(dest_container))
				{
					dest_container.clear();
				}
				expr.to_error();
				return;
			}
		}
		else if (dest_without_const.is<ast::ts_array>())
		{
			bz_assert(dest_container.nodes.back().is<ast::ts_array>());
			auto &dest_array_type = dest_container.nodes.back().get<ast::ts_array>().elem_type;
			auto const array_size = dest_container.nodes.back().get<ast::ts_array>().size;
			auto &expr_tuple_elems = expr.get_tuple().elems;
			if (array_size != expr_tuple_elems.size())
			{
				context.report_error(
					expr.src_tokens,
					bz::format(
						"unable to match tuple expression to type '{}', mismatched number of elements: {} and {}",
						dest_container, array_size, expr_tuple_elems.size()
					)
				);
			}

			bool error = array_size != expr_tuple_elems.size();
			for (auto &expr : expr_tuple_elems)
			{
				match_expression_to_type_impl(expr, dest_array_type, dest_array_type, context);
				if (expr.is_error())
				{
					error = true;
				}
			}
			if (error)
			{
				if (!ast::is_complete(dest_container))
				{
					dest_container.clear();
				}
				expr.to_error();
				return;
			}
			else
			{
				expr = ast::make_dynamic_expression(
					expr.src_tokens,
					ast::expression_type_kind::rvalue, dest_without_const,
					ast::make_expr_array_init(std::move(expr.get_tuple().elems), dest_without_const)
				);
			}
		}
		else
		{
			context.report_error(expr.src_tokens, bz::format("unable to match tuple expression to type '{}'", dest_container));
			if (!ast::is_complete(dest_container))
			{
				dest_container.clear();
			}
			expr.to_error();
			return;
		}
	}
	else if (expr.is_integer_literal())
	{
		match_integer_literal_to_type(expr, dest_container, dest, context);
		if (expr.is_error())
		{
			return;
		}
	}
	else if (expr.is_null_literal())
	{
		match_null_literal_to_type(expr, dest_container, dest, context);
		if (expr.is_error())
		{
			return;
		}
	}
	else
	{
		auto const [expr_type, expr_type_kind] = expr.get_expr_type_and_kind();
		auto const match_result = match_types(dest_container, expr_type, ast::remove_const_or_consteval(dest), expr_type_kind, context);

		switch (match_result)
		{
		case type_match_result::good:
			// nothing
			break;
		case type_match_result::needs_cast:
			expr = context.make_cast_expression(expr.src_tokens, std::move(expr), dest_container);
			break;
		case type_match_result::error:
			context.report_error(expr, bz::format("cannot implicitly convert expression from type '{}' to '{}'", expr_type, dest_container));
			if (!ast::is_complete(dest_container))
			{
				dest_container.clear();
			}
			expr.to_error();
			return;
		}
	}

	if (
		!dest_container.is_typename()
		&& dest_container.is<ast::ts_lvalue_reference>()
		&& expr.get_expr_type_and_kind().second != ast::expression_type_kind::lvalue_reference
	)
	{
		expr = ast::make_dynamic_expression(
			expr.src_tokens,
			ast::expression_type_kind::lvalue_reference,
			ast::remove_lvalue_reference(dest_container),
			ast::make_expr_take_reference(std::move(expr))
		);
	}

	if (!dest_container.is_typename() && dest_container.is<ast::ts_consteval>())
	{
		resolve::consteval_try(expr, context);
		if (!expr.is_constant())
		{
			context.report_error(expr, "expression must be a constant expression", resolve::get_consteval_fail_notes(expr));
			if (!ast::is_complete(dest_container))
			{
				dest_container.clear();
			}
			expr.to_error();
		}
	}
}

void match_expression_to_type(ast::expression &expr, ast::typespec &dest_type, ctx::parse_context &context)
{
	if (dest_type.is_empty())
	{
		expr.to_error();
	}
	else if (expr.is_error())
	{
		if (!ast::is_complete(dest_type))
		{
			dest_type.clear();
		}
	}
	else
	{
		match_expression_to_type_impl(expr, dest_type, dest_type, context);
		resolve::consteval_guaranteed(expr, context);
	}
}

static void set_type(ast::decl_variable &var_decl, ast::typespec_view type, bool is_const, bool is_reference)
{
	if (var_decl.tuple_decls.empty())
	{
		var_decl.get_type() = type;
		if (is_const && !var_decl.get_type().is<ast::ts_lvalue_reference>() && !var_decl.get_type().is<ast::ts_const>())
		{
			var_decl.get_type().add_layer<ast::ts_const>();
		}
		if (is_reference)
		{
			var_decl.flags |= ast::decl_variable::tuple_outer_ref;
		}
	}
	else if (type.is_empty())
	{
		for (auto &inner_decl : var_decl.tuple_decls)
		{
			set_type(inner_decl, type, false, false);
		}
	}
	else
	{
		bz_assert(type.is<ast::ts_tuple>());
		auto const &inner_types = type.get<ast::ts_tuple>().types;
		if (var_decl.tuple_decls.back().get_type().is<ast::ts_variadic>())
		{
			var_decl.tuple_decls.back().flags |= ast::decl_variable::variadic;
			if (inner_types.size() == var_decl.tuple_decls.size() - 1)
			{
				var_decl.original_tuple_variadic_decl = ast::make_ast_unique<ast::decl_variable>(
					std::move(var_decl.tuple_decls.back())
				);
				var_decl.tuple_decls.pop_back();
			}
			else
			{
				bz_assert(inner_types.size() >= var_decl.tuple_decls.size());
				var_decl.tuple_decls.reserve(inner_types.size());
				var_decl.tuple_decls.resize(inner_types.size(), var_decl.tuple_decls.back());
			}
		}
		bz_assert(inner_types.size() == var_decl.tuple_decls.size());
		for (auto const &[inner_decl, inner_type] : bz::zip(var_decl.tuple_decls, inner_types))
		{
			auto const inner_is_ref = inner_type.is<ast::ts_lvalue_reference>();
			auto const inner_is_const = ast::remove_lvalue_reference(inner_type).is<ast::ts_const>();
			set_type(inner_decl, inner_type, is_const || inner_is_const, is_reference || inner_is_ref);
		}
	}
}

void match_expression_to_variable(
	ast::expression &expr,
	ast::decl_variable &var_decl,
	ctx::parse_context &context
)
{
	if (var_decl.tuple_decls.empty())
	{
		match_expression_to_type(expr, var_decl.get_type(), context);
	}
	else
	{
		match_expression_to_type(expr, var_decl.get_type(), context);
		auto const var_type_without_lvalue_ref = ast::remove_lvalue_reference(var_decl.get_type());
		set_type(
			var_decl, ast::remove_const_or_consteval(var_type_without_lvalue_ref),
			var_type_without_lvalue_ref.is<ast::ts_const>() || var_type_without_lvalue_ref.is<ast::ts_consteval>(),
			var_decl.get_type().is<ast::ts_lvalue_reference>()
		);
	}
}

} // namespace resolve
