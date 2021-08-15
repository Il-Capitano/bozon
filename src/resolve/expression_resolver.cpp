#include "expression_resolver.h"
#include "statement_resolver.h"
#include "parse/consteval.h"
#include "ctx/builtin_operators.h"
#include "global_data.h"

namespace resolve
{

static ast::expression resolve_expr(
	[[maybe_unused]] ast::expression &original_expr,
	ast::expr_identifier &id_expr,
	ctx::parse_context &context
)
{
	bz_assert(!context.in_generic_function());
	return context.make_identifier_expression(std::move(id_expr.id));
}

static ast::expression resolve_expr(
	ast::expression &original_expr,
	ast::expr_tuple &tuple_expr,
	ctx::parse_context &context
)
{
	for (auto &elem : tuple_expr.elems)
	{
		resolve_expression(elem, context);
	}
	return context.make_tuple(original_expr.src_tokens, std::move(tuple_expr.elems));
}

static ast::expression resolve_expr(
	ast::expression &original_expr,
	ast::expr_unary_op &unary_op,
	ctx::parse_context &context
)
{
	resolve_expression(unary_op.expr, context);
	return context.make_unary_operator_expression(original_expr.src_tokens, unary_op.op, std::move(unary_op.expr));
}

static ast::expression resolve_expr(
	ast::expression &original_expr,
	ast::expr_binary_op &binary_op,
	ctx::parse_context &context
)
{
	resolve_expression(binary_op.lhs, context);
	resolve_expression(binary_op.rhs, context);
	return context.make_binary_operator_expression(
		original_expr.src_tokens, binary_op.op,
		std::move(binary_op.lhs), std::move(binary_op.rhs)
	);
}

static ast::expression resolve_expr(
	ast::expression &original_expr,
	ast::expr_unresolved_subscript &subscript,
	ctx::parse_context &context
)
{
	resolve_expression(subscript.base, context);
	for (auto &index : subscript.indices)
	{
		resolve_expression(index, context);
	}
	return context.make_subscript_operator_expression(original_expr.src_tokens, std::move(subscript.base), std::move(subscript.indices));
}

static ast::expression resolve_expr(
	ast::expression &original_expr,
	ast::expr_unresolved_function_call &func_call,
	ctx::parse_context &context
)
{
	resolve_expression(func_call.func, context);
	for (auto &arg : func_call.args)
	{
		resolve_expression(arg, context);
	}
	return context.make_function_call_expression(original_expr.src_tokens, std::move(func_call.func), std::move(func_call.args));
}

static ast::expression resolve_expr(
	ast::expression &original_expr,
	ast::expr_unresolved_cast &cast,
	ctx::parse_context &context
)
{
	resolve_expression(cast.expr, context);
	resolve_expression(cast.type, context);
	if (!cast.type.is_typename() && cast.type.not_error())
	{
		context.report_error(cast.type.src_tokens, "expected a type in cast expression");
		return ast::make_error_expression(original_expr.src_tokens, ast::make_expr_cast(std::move(cast.expr), ast::typespec()));
	}

	return context.make_cast_expression(original_expr.src_tokens, std::move(cast.expr), std::move(cast.type.get_typename()));
}

static ast::expression resolve_expr(
	ast::expression &original_expr,
	ast::expr_unresolved_member_access &member_access,
	ctx::parse_context &context
)
{
	resolve_expression(member_access.base, context);
	return context.make_member_access_expression(original_expr.src_tokens, std::move(member_access.base), member_access.member);
}

static ast::expression resolve_expr(
	ast::expression &original_expr,
	ast::expr_unresolved_universal_function_call &universal_func_call,
	ctx::parse_context &context
)
{
	resolve_expression(universal_func_call.base, context);
	for (auto &arg : universal_func_call.args)
	{
		resolve_expression(arg, context);
	}
	return context.make_universal_function_call_expression(
		original_expr.src_tokens,
		std::move(universal_func_call.base),
		std::move(universal_func_call.func_name),
		std::move(universal_func_call.args)
	);
}

static ast::expression resolve_expr(
	ast::expression &original_expr,
	ast::expr_compound &compound,
	ctx::parse_context &context
)
{
	for (auto &stmt : compound.statements)
	{
		resolve_statement(stmt, context);
	}
	if (compound.final_expr.not_null())
	{
		resolve_expression(compound.final_expr, context);
	}

	if (compound.final_expr.is_null())
	{
		return ast::make_dynamic_expression(
			original_expr.src_tokens,
			ast::expression_type_kind::none, ast::make_void_typespec(nullptr),
			ast::make_expr_compound(std::move(compound.statements), ast::expression())
		);
	}
	else
	{
		auto const [type, kind] = compound.final_expr.get_expr_type_and_kind();
		ast::typespec result_type = type;
		return ast::make_dynamic_expression(
			original_expr.src_tokens,
			kind, std::move(result_type),
			ast::make_expr_compound(std::move(compound))
		);
	}
}

static ast::expression resolve_expr(
	ast::expression &original_expr,
	ast::expr_if &if_expr,
	ctx::parse_context &context
)
{
	resolve_expression(if_expr.condition, context);
	resolve_expression(if_expr.then_block, context);
	if (if_expr.else_block.not_null())
	{
		resolve_expression(if_expr.else_block, context);
	}

	if (if_expr.then_block.is_noreturn() && if_expr.else_block.is_noreturn())
	{
		return ast::make_dynamic_expression(
			original_expr.src_tokens,
			ast::expression_type_kind::noreturn,
			ast::make_void_typespec(nullptr),
			ast::make_expr_if(std::move(if_expr))
		);
	}
	else if (if_expr.then_block.is_none() || if_expr.else_block.is_null() || if_expr.else_block.is_none())
	{
		return ast::make_dynamic_expression(
			original_expr.src_tokens,
			ast::expression_type_kind::none,
			ast::make_void_typespec(nullptr),
			ast::make_expr_if(std::move(if_expr))
		);
	}
	else if (if_expr.then_block.not_error() && if_expr.else_block.not_error())
	{
		return ast::make_dynamic_expression(
			original_expr.src_tokens,
			ast::expression_type_kind::if_expr,
			ast::typespec(),
			ast::make_expr_if(std::move(if_expr))
		);
	}
	else
	{
		bz_assert(context.has_errors());
		return ast::make_error_expression(original_expr.src_tokens, ast::make_expr_if(std::move(if_expr)));
	}
}

static bool are_all_cases_unique(
	bz::array_view<ast::switch_case> cases,
	ctx::parse_context &context
)
{
	ast::arena_vector<ast::expression const *> case_values;
	for (auto const &switch_case : cases)
	{
		for (auto const &value : switch_case.values)
		{
			if (value.is_error())
			{
				return true;
			}
			bz_assert(value.is<ast::constant_expression>());
			case_values.push_back(&value);
		}
	}
	for (size_t i = 0; i < case_values.size() - 1; ++i)
	{
		auto const &lhs = *case_values[i];
		auto const &lhs_value = lhs.get<ast::constant_expression>().value;
		for (size_t j = i + 1; j < case_values.size(); ++j)
		{
			auto const &rhs = *case_values[j];
			auto const &rhs_value = rhs.get<ast::constant_expression>().value;
			if (lhs_value == rhs_value)
			{
				switch (rhs_value.kind())
				{
				case ast::constant_value::sint:
					context.report_error(
						rhs,
						bz::format("duplicate value '{}' in switch expression", lhs_value.get<ast::constant_value::sint>()),
						{ context.make_note(lhs, "value previously used here") }
					);
					break;
				case ast::constant_value::uint:
					context.report_error(
						rhs,
						bz::format("duplicate value '{}' in switch expression", lhs_value.get<ast::constant_value::uint>()),
						{ context.make_note(lhs, "value previously used here") }
					);
					break;
				case ast::constant_value::u8char:
					context.report_error(
						rhs,
						bz::format(
							"duplicate value '{}' in switch expression",
							lhs_value.get<ast::constant_value::u8char>()
						),
						{ context.make_note(lhs, "value previously used here") }
					);
					break;
				case ast::constant_value::boolean:
					context.report_error(
						rhs,
						bz::format("duplicate value '{}' in switch expression", lhs_value.get<ast::constant_value::boolean>()),
						{ context.make_note(lhs, "value previously used here") }
					);
					break;
				default:
					bz_unreachable;
				}
				return false;
			}
		}
	}
	return true;
}

static ast::expression resolve_expr(
	ast::expression &original_expr,
	ast::expr_switch &switch_expr,
	ctx::parse_context &context
)
{
	resolve_expression(switch_expr.matched_expr, context);
	if (switch_expr.matched_expr.is_error())
	{
		bz_assert(context.has_errors());
		return ast::make_error_expression(original_expr.src_tokens, ast::make_expr_switch(std::move(switch_expr)));
	}
	auto const match_type_view = ast::remove_const_or_consteval(switch_expr.matched_expr.get_expr_type_and_kind().first);
	if (!match_type_view.is<ast::ts_base_type>() || [&]() {
		auto const info = match_type_view.get<ast::ts_base_type>().info;
		return !ctx::is_integer_kind(info->kind) && info->kind != ast::type_info::char_ && info->kind != ast::type_info::bool_;
	}())
	{
		if (do_verbose)
		{
			context.report_error(
				switch_expr.matched_expr.src_tokens,
				bz::format("invalid type '{}' for switch expression", match_type_view),
				{ context.make_note("only integral types can be used in switch expressions") }
			);
		}
		else
		{
			context.report_error(
				switch_expr.matched_expr.src_tokens,
				bz::format("invalid type '{}' for switch expression", match_type_view)
			);
		}
		return ast::make_error_expression(original_expr.src_tokens, ast::make_expr_switch(std::move(switch_expr)));
	}

	ast::typespec match_type = match_type_view;
	bool good = true;
	for (auto &[values, expr] : switch_expr.cases)
	{
		for (auto &value : values)
		{
			resolve_expression(value, context);
			context.match_expression_to_type(value, match_type);
			good &= value.not_error();
			parse::consteval_try_without_error(value, context);
			if (value.has_consteval_failed())
			{
				context.report_error(
					value.src_tokens, "case value in switch expression must be a constant expression",
					parse::get_consteval_fail_notes(value)
				);
				good = false;
			}
		}
		resolve_expression(expr, context);
		good &= expr.not_error();
	}

	if (!good || !are_all_cases_unique(switch_expr.cases, context))
	{
		return ast::make_error_expression(original_expr.src_tokens, ast::make_expr_switch(std::move(switch_expr)));
	}

	auto const case_count = switch_expr.cases.empty() ? 0 :
		switch_expr.cases.transform([](auto const &case_) {
			return case_.values.size();
		}).sum();
	auto const match_type_info = match_type.get<ast::ts_base_type>().info;
	auto const max_case_count = [&]() -> uint64_t {
		switch (match_type_info->kind)
		{
		case ast::type_info::bool_:
			return 2;
		case ast::type_info::int8_:
		case ast::type_info::uint8_:
			return std::numeric_limits<uint8_t>::max();
		case ast::type_info::int16_:
		case ast::type_info::uint16_:
			return std::numeric_limits<uint16_t>::max();
		case ast::type_info::int32_:
		case ast::type_info::uint32_:
		case ast::type_info::char_:
			return std::numeric_limits<uint32_t>::max();
		case ast::type_info::int64_:
		case ast::type_info::uint64_:
			return std::numeric_limits<uint64_t>::max();
		default:
			bz_unreachable;
		}
	}();

	if (case_count < max_case_count && switch_expr.default_case.is_null())
	{
		context.report_warning(
			ctx::warning_kind::non_exhaustive_switch,
			original_expr.src_tokens,
			"switch expression doesn't cover all possible values and doesn't have an else case"
		);
		return ast::make_dynamic_expression(
			original_expr.src_tokens,
			ast::expression_type_kind::none,
			ast::make_void_typespec(nullptr),
			ast::make_expr_switch(std::move(switch_expr))
		);
	}
	else if (case_count == max_case_count && switch_expr.default_case.not_null())
	{
		context.report_warning(
			ctx::warning_kind::unneeded_else,
			switch_expr.default_case.src_tokens,
			"else case is not needed as all possible values are already covered"
		);
	}

	auto const expr_kind = switch_expr.cases
		.transform([](auto const &case_) {
			bz_assert(case_.expr.is_constant_or_dynamic());
			auto const kind = case_.expr.template is<ast::constant_expression>()
				? case_.expr.template get<ast::constant_expression>().kind
				: case_.expr.template get<ast::dynamic_expression>().kind;
			switch (kind)
			{
			case ast::expression_type_kind::none:
			case ast::expression_type_kind::noreturn:
				return kind;
			default:
				return ast::expression_type_kind::switch_expr;
			}
		})
		.reduce(ast::expression_type_kind::noreturn, [](auto const lhs, auto const rhs) {
			switch (lhs)
			{
			case ast::expression_type_kind::switch_expr:
				return lhs;
			case ast::expression_type_kind::none:
				switch (rhs)
				{
				case ast::expression_type_kind::switch_expr:
				case ast::expression_type_kind::none:
					return rhs;
				default:
					return lhs;
				}
			case ast::expression_type_kind::noreturn:
				return rhs;
			default:
				bz_unreachable;
			}
		});

	if (expr_kind == ast::expression_type_kind::switch_expr)
	{
		return ast::make_dynamic_expression(
			original_expr.src_tokens,
			expr_kind, ast::typespec(),
			ast::make_expr_switch(std::move(switch_expr))
		);
	}
	else
	{
		return ast::make_dynamic_expression(
			original_expr.src_tokens,
			expr_kind, ast::make_void_typespec(nullptr),
			ast::make_expr_switch(std::move(switch_expr))
		);
	}
}

void resolve_expression(ast::expression &expr, ctx::parse_context &context)
{
	if (expr.is_unresolved())
	{
		expr = expr.get<ast::unresolved_expression>().expr.visit([&](auto &inner_expr) {
			return resolve_expr(expr, inner_expr, context);
		});
		parse::consteval_guaranteed(expr, context);
		bz_assert(!expr.is_unresolved());
	}
}

} // namespace resolve
