#include "match_expression.h"
#include "match_common.h"
#include "match_to_type.h"
#include "consteval.h"
#include "type_match_generic.h"

namespace resolve
{

static void match_expression_to_type_impl(
	ast::expression &expr,
	ast::typespec &dest_container,
	ast::typespec_view dest,
	ctx::parse_context &context
)
{
	auto const is_good = generic_type_match(match_context_t<type_match_function_kind::match_expression>{
		.expr = expr,
		.dest_container = dest_container,
		.dest = dest,
		.context = context,
	});
	if (!is_good)
	{
		expr.to_error();
		if (!ast::is_complete(dest_container))
		{
			dest_container.clear();
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
	else if (expr.is<ast::expanded_variadic_expression>())
	{
		context.report_error(expr.src_tokens, "expanded variadic expression is not allowed here");
		expr.to_error();
		if (!ast::is_complete(dest_type))
		{
			dest_type.clear();
		}
	}
	else if (expr.is_placeholder_literal())
	{
		context.report_error(expr.src_tokens, "placeholder literal is not allowed here");
		expr.to_error();
		if (!ast::is_complete(dest_type))
		{
			dest_type.clear();
		}
	}
	else
	{
		match_expression_to_type_impl(expr, dest_type, dest_type, context);
		if (dest_type.not_empty() && !dest_type.is_typename() && !dest_type.is<ast::ts_void>() && !context.is_instantiable(expr.src_tokens, dest_type))
		{
			context.report_error(expr.src_tokens, bz::format("expression type '{}' is not instantiable", dest_type));
			expr.to_error();
			if (!ast::is_complete(dest_type))
			{
				dest_type.clear();
			}
		}
		else if (dest_type.is_typename() || dest_type.is<ast::ts_consteval>())
		{
			resolve::consteval_try(expr, context);
			if (!expr.is_constant())
			{
				context.report_error(expr, "expression must be a constant expression");
				if (!ast::is_complete(dest_type))
				{
					dest_type.clear();
				}
				expr.to_error();
			}
		}
		else
		{
			resolve::consteval_guaranteed(expr, context);
		}
	}
}

static void set_type(ast::decl_variable &var_decl, ast::typespec_view type, bool is_mut, bool is_reference)
{
	if (type.is_empty())
	{
		for (auto &inner_decl : var_decl.tuple_decls)
		{
			inner_decl.get_type() = type;
			set_type(inner_decl, type, false, false);
		}
	}
	else if (var_decl.tuple_decls.empty())
	{
		if (is_mut && !var_decl.get_type().is<ast::ts_lvalue_reference>() && !var_decl.get_type().is<ast::ts_mut>())
		{
			var_decl.get_type().add_layer<ast::ts_mut>();
		}
		if (is_reference)
		{
			var_decl.flags |= ast::decl_variable::tuple_outer_ref;
		}
	}
	else if (type.remove_any_reference().is<ast::ts_tuple>())
	{
		auto const &inner_types = type.remove_any_reference().get<ast::ts_tuple>().types;
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
				var_decl.tuple_decls.reserve(inner_types.size()); // !!! resize can cause a reallocation, which invalidates .back()
				var_decl.tuple_decls.resize(inner_types.size(), var_decl.tuple_decls.back());
			}
		}
		bz_assert(inner_types.size() == var_decl.tuple_decls.size());
		for (auto const &[inner_decl, inner_type] : bz::zip(var_decl.tuple_decls, inner_types))
		{
			inner_decl.get_type() = inner_type;
			auto const inner_is_ref = inner_type.is<ast::ts_lvalue_reference>();
			auto const inner_is_mut = inner_is_ref && inner_type.get<ast::ts_lvalue_reference>().is<ast::ts_mut>();
			set_type(inner_decl, inner_type, is_mut || inner_is_mut, is_reference || inner_is_ref);
		}
	}
	else
	{
		bz_assert(type.remove_any_reference().is<ast::ts_array>());
		auto const &array_type = type.remove_any_reference().get<ast::ts_array>();
		if (var_decl.tuple_decls.back().get_type().is<ast::ts_variadic>())
		{
			var_decl.tuple_decls.back().flags |= ast::decl_variable::variadic;
			if (array_type.size == var_decl.tuple_decls.size() - 1)
			{
				var_decl.original_tuple_variadic_decl = ast::make_ast_unique<ast::decl_variable>(
					std::move(var_decl.tuple_decls.back())
				);
				var_decl.tuple_decls.pop_back();
			}
			else
			{
				bz_assert(array_type.size >= var_decl.tuple_decls.size());
				var_decl.tuple_decls.reserve(array_type.size); // !!! resize can cause a reallocation, which invalidates .back()
				var_decl.tuple_decls.resize(array_type.size, var_decl.tuple_decls.back());
			}
		}
		bz_assert(array_type.size == var_decl.tuple_decls.size());
		for (auto &inner_decl : var_decl.tuple_decls)
		{
			inner_decl.get_type() = array_type.elem_type;
			set_type(inner_decl, array_type.elem_type, is_mut, is_reference);
		}
	}
}

static void set_default_types_for_tuple_decomposition(ast::decl_variable &var_decl, bool is_outer_variadic)
{
	bz_assert(var_decl.tuple_decls.not_empty());
	bz_assert(var_decl.get_type().terminator != nullptr);
	var_decl.get_type().terminator->emplace<ast::ts_tuple>(
		var_decl.tuple_decls.transform([is_outer_variadic](auto &inner_decl) -> ast::typespec {
			if (inner_decl.tuple_decls.not_empty())
			{
				set_default_types_for_tuple_decomposition(
					inner_decl,
					is_outer_variadic || inner_decl.get_type().template is<ast::ts_variadic>()
				);
			}
			return inner_decl.get_type();
		}).collect<ast::arena_vector>()
	);
}

// the logic is duplicated in 'match_to_type.cpp' and needs to be in sync
static bool set_types_for_tuple_decomposition(
	lex::src_tokens const &src_tokens,
	ast::decl_variable &var_decl,
	bool is_outer_variadic,
	ast::expression const *expr,
	ast::typespec_view expr_type,
	ctx::parse_context &context
)
{
	bz_assert(var_decl.tuple_decls.not_empty());
	if (is_outer_variadic && var_decl.tuple_decls.back().get_type().is<ast::ts_variadic>())
	{
		var_decl.tuple_decls.back().get_type().remove_layer();
		context.report_error(var_decl.src_tokens, "variable is doubly variadic");
	}

	if (expr != nullptr && expr->is_tuple())
	{
		auto const &tuple_expr = expr->get_tuple();
		if (
			var_decl.tuple_decls.size() != tuple_expr.elems.size()
			&& !(
				var_decl.tuple_decls.back().get_type().is<ast::ts_variadic>()
				&& var_decl.tuple_decls.size() - 1 <= tuple_expr.elems.size()
			)
		)
		{
			return false;
		}

		for (auto const i : bz::iota(0, var_decl.tuple_decls.size()))
		{
			if (
				var_decl.tuple_decls[i].tuple_decls.not_empty()
				&& !set_types_for_tuple_decomposition(
					src_tokens,
					var_decl.tuple_decls[i],
					is_outer_variadic || var_decl.tuple_decls[i].get_type().is<ast::ts_variadic>(),
					&tuple_expr.elems[i],
					tuple_expr.elems[i].get_expr_type().remove_any_mut_reference(),
					context
				))
			{
				return false;
			}
		}

		bz_assert(var_decl.get_type().terminator != nullptr);
		var_decl.get_type().terminator->emplace<ast::ts_tuple>(
			var_decl.tuple_decls.transform([](auto const &inner_decl) -> ast::typespec {
				return inner_decl.get_type();
			}).collect<ast::arena_vector>()
		);
	}
	else if (expr_type.is<ast::ts_tuple>())
	{
		auto const &tuple_type = expr_type.get<ast::ts_tuple>();
		if (
			var_decl.tuple_decls.size() != tuple_type.types.size()
			&& !(
				var_decl.tuple_decls.back().get_type().is<ast::ts_variadic>()
				&& var_decl.tuple_decls.size() - 1 <= tuple_type.types.size()
			)
		)
		{
			return false;
		}

		for (auto const i : bz::iota(0, var_decl.tuple_decls.size()))
		{
			if (
				var_decl.tuple_decls[i].tuple_decls.not_empty()
				&& !set_types_for_tuple_decomposition(
					src_tokens,
					var_decl.tuple_decls[i],
					is_outer_variadic || var_decl.tuple_decls[i].get_type().is<ast::ts_variadic>(),
					nullptr,
					tuple_type.types[i],
					context
				)
			)
			{
				return false;
			}
		}

		bz_assert(var_decl.get_type().terminator != nullptr);
		var_decl.get_type().terminator->emplace<ast::ts_tuple>(
			var_decl.tuple_decls.transform([](auto const &inner_decl) -> ast::typespec {
				return inner_decl.get_type();
			}).collect<ast::arena_vector>()
		);
	}
	else if (expr_type.is<ast::ts_array>())
	{
		auto const &array_type = expr_type.get<ast::ts_array>();
		if (
			var_decl.tuple_decls.size() != array_type.size
			&& !(
				var_decl.tuple_decls.back().get_type().is<ast::ts_variadic>()
				&& var_decl.tuple_decls.size() - 1 <= array_type.size
			)
		)
		{
			auto const is_variadic = var_decl.tuple_decls.back().get_type().is<ast::ts_variadic>();
			auto const variable_count = var_decl.tuple_decls.size() - (is_variadic ? 1 : 0);
			context.report_error(
				src_tokens,
				bz::format(
					"unable to decompose an array of size {} into {} variable{}",
					array_type.size, variable_count, variable_count == 1 ? "" : "s"
				)
			);
			return false;
		}

		for (auto &inner_decl : var_decl.tuple_decls)
		{
			if (
				inner_decl.tuple_decls.not_empty()
				&& !set_types_for_tuple_decomposition(
					src_tokens,
					inner_decl,
					is_outer_variadic || inner_decl.get_type().is<ast::ts_variadic>(),
					nullptr,
					array_type.elem_type,
					context
				)
			)
			{
				return false;
			}
		}

		auto const first_decl_type = var_decl.tuple_decls[0].get_type().remove_mut();
		for (auto const &inner_decl : var_decl.tuple_decls.slice(1))
		{
			auto const inner_type = inner_decl.get_type().is<ast::ts_variadic>()
				? inner_decl.get_type().get<ast::ts_variadic>()
				: inner_decl.get_type().as_typespec_view();
			if (first_decl_type != inner_type)
			{
				return false;
			}
		}

		bz_assert(var_decl.get_type().terminator != nullptr);
		if (first_decl_type.is<ast::ts_variadic>())
		{
			var_decl.get_type().terminator->emplace<ast::ts_array>(array_type.size, first_decl_type.get<ast::ts_variadic>());
		}
		else
		{
			var_decl.get_type().terminator->emplace<ast::ts_array>(array_type.size, first_decl_type);
		}
	}
	else
	{
		return false;
	}

	return true;
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
		if (!set_types_for_tuple_decomposition(
			expr.src_tokens,
			var_decl,
			var_decl.get_type().is<ast::ts_variadic>(),
			&expr,
			expr.get_expr_type().remove_any_mut_reference(),
			context
		))
		{
			set_default_types_for_tuple_decomposition(var_decl, var_decl.get_type().is<ast::ts_variadic>());
		}

		match_expression_to_type(expr, var_decl.get_type(), context);
		auto const decl_type = var_decl.get_type().remove_mut_reference();
		if (decl_type.is<ast::ts_tuple>() || decl_type.is<ast::ts_array>())
		{
			auto const var_type_without_lvalue_ref = var_decl.get_type().remove_reference();
			set_type(
				var_decl, var_type_without_lvalue_ref.remove_any_mut(),
				var_type_without_lvalue_ref.is<ast::ts_mut>(),
				var_decl.get_type().is<ast::ts_lvalue_reference>()
			);
		}
		else
		{
			context.report_error(
				var_decl.src_tokens,
				bz::format("invalid type '{}' for tuple decomposition", var_decl.get_type())
			);
			var_decl.clear_type();
			return;
		}
	}
}

} // namespace resolve
