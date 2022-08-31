#include "match_expression.h"
#include "match_common.h"
#include "match_to_type.h"
#include "consteval.h"
#include "type_match_generic.h"

#include "ctx/global_context.h"

namespace resolve
{

static void match_expression_to_type_impl(
	ast::expression &expr,
	ast::typespec &dest_container,
	ast::typespec_view dest,
	ctx::parse_context &context
);

enum class type_match_result
{
	good, needs_copy, needs_move, needs_cast, error
};

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
		if (!dest_type.is_typename() && dest_type.is<ast::ts_consteval>())
		{
			resolve::consteval_try(expr, context);
			if (!expr.is_constant())
			{
				context.report_error(expr, "expression must be a constant expression", resolve::get_consteval_fail_notes(expr));
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
