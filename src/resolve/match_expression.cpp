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
		if (!ast::remove_const_or_consteval(ast::remove_lvalue_reference(var_decl.get_type())).is<ast::ts_tuple>())
		{
			context.report_error(
				var_decl.src_tokens,
				bz::format("invalid type '{}' for tuple decomposition", var_decl.get_type())
			);
			var_decl.get_type().clear();
			return;
		}

		auto const var_type_without_lvalue_ref = ast::remove_lvalue_reference(var_decl.get_type());
		set_type(
			var_decl, ast::remove_const_or_consteval(var_type_without_lvalue_ref),
			var_type_without_lvalue_ref.is<ast::ts_const>() || var_type_without_lvalue_ref.is<ast::ts_consteval>(),
			var_decl.get_type().is<ast::ts_lvalue_reference>()
		);
	}
}

} // namespace resolve
