#include "expression_resolver.h"
#include "statement_resolver.h"
#include "global_data.h"
#include "parse/consteval.h"
#include "escape_sequences.h"

namespace resolve
{

static ast::expression resolve_expr(
	lex::src_tokens,
	ast::expr_identifier id_expr,
	ctx::parse_context &context
)
{
	bz_assert(id_expr.decl == nullptr);
	return context.make_identifier_expression(std::move(id_expr.id));
}

static ast::expression resolve_expr(
	lex::src_tokens src_tokens,
	ast::expr_tuple tuple_expr,
	ctx::parse_context &context
)
{
	for (auto &elem : tuple_expr.elems)
	{
		resolve_expression(elem, context);
	}
	return context.make_tuple(src_tokens, std::move(tuple_expr.elems));
}

static ast::expression resolve_expr(
	lex::src_tokens src_tokens,
	ast::expr_unary_op unary_op,
	ctx::parse_context &context
)
{
	// special case for variadic expansion
	if (unary_op.op == lex::token::dot_dot_dot)
	{
		auto const info = context.push_variadic_resolver_pause();
		resolve_expression(unary_op.expr, context);
		context.pop_variadic_resolver_pause(info);
	}
	else
	{
		resolve_expression(unary_op.expr, context);
	}
	return context.make_unary_operator_expression(src_tokens, unary_op.op, std::move(unary_op.expr));
}

static ast::expression resolve_expr(
	lex::src_tokens src_tokens,
	ast::expr_binary_op binary_op,
	ctx::parse_context &context
)
{
	resolve_expression(binary_op.lhs, context);
	resolve_expression(binary_op.rhs, context);
	return context.make_binary_operator_expression(src_tokens, binary_op.op, std::move(binary_op.lhs), std::move(binary_op.rhs));
}

static ast::expression resolve_expr(
	lex::src_tokens src_tokens,
	ast::expr_unresolved_subscript subscript_expr,
	ctx::parse_context &context
)
{
	resolve_expression(subscript_expr.base, context);
	for (auto &index : subscript_expr.indices)
	{
		resolve_expression(index, context);
	}
	return context.make_subscript_operator_expression(src_tokens, std::move(subscript_expr.base), std::move(subscript_expr.indices));
}

static ast::expression resolve_expr(
	lex::src_tokens src_tokens,
	ast::expr_unresolved_function_call func_call,
	ctx::parse_context &context
)
{
	resolve_expression(func_call.callee, context);
	for (auto &arg : func_call.args)
	{
		resolve_expression(arg, context);
	}
	return context.make_function_call_expression(src_tokens, std::move(func_call.callee), std::move(func_call.args));
}

static ast::expression resolve_expr(
	lex::src_tokens src_tokens,
	ast::expr_unresolved_universal_function_call func_call,
	ctx::parse_context &context
)
{
	resolve_expression(func_call.base, context);
	for (auto &arg : func_call.args)
	{
		resolve_expression(arg, context);
	}
	return context.make_universal_function_call_expression(
		src_tokens,
		std::move(func_call.base),
		std::move(func_call.fn_id),
		std::move(func_call.args)
	);
}

static ast::expression resolve_expr(
	lex::src_tokens src_tokens,
	ast::expr_unresolved_cast cast_expr,
	ctx::parse_context &context
)
{
	resolve_expression(cast_expr.expr, context);
	resolve_expression(cast_expr.type, context);
	if (cast_expr.type.is_typename())
	{
		return context.make_cast_expression(src_tokens, std::move(cast_expr.expr), std::move(cast_expr.type.get_typename()));
	}
	else
	{
		return ast::make_error_expression(src_tokens, ast::make_expr_cast(std::move(cast_expr.expr), ast::typespec()));
	}
}

static ast::expression resolve_expr(
	lex::src_tokens src_tokens,
	ast::expr_unresolved_member_access member_access,
	ctx::parse_context &context
)
{
	resolve_expression(member_access.base, context);
	return context.make_member_access_expression(src_tokens, std::move(member_access.base), member_access.member);
}

static bool is_statement_noreturn(ast::statement const &stmt)
{
	return stmt.visit(bz::overload{
		[](ast::stmt_while const &) {
			return false;
		},
		[](ast::stmt_for const &) {
			return false;
		},
		[](ast::stmt_foreach const &) {
			return false;
		},
		[](ast::stmt_return const &) {
			return true;
		},
		[](ast::stmt_no_op const &) {
			return false;
		},
		[](ast::stmt_static_assert const &) {
			return false;
		},
		[](ast::stmt_expression const &expr_stmt) {
			return expr_stmt.expr.is_noreturn();
		},
		[](ast::decl_variable const &var_decl) {
			return var_decl.init_expr.not_null() && var_decl.init_expr.is_noreturn();
		},
		[](ast::decl_function const &) {
			return false;
		},
		[](ast::decl_operator const &) {
			return false;
		},
		[](ast::decl_function_alias const &) {
			return false;
		},
		[](ast::decl_type_alias const &) {
			return false;
		},
		[](ast::decl_struct const &) {
			return false;
		},
		[](ast::decl_import const &) {
			return false;
		},
	});
}

static ast::expression resolve_expr(
	lex::src_tokens src_tokens,
	ast::expr_compound compound_expr_,
	ctx::parse_context &context
)
{
	auto result_node = ast::make_ast_unique<ast::expr_compound>(std::move(compound_expr_));
	auto &compound_expr = *result_node;
	bool is_noreturn = false;
	for (auto &stmt : compound_expr_.statements)
	{
		resolve_statement(stmt, context);
		is_noreturn |= is_statement_noreturn(stmt);
	}
	resolve_expression(compound_expr.final_expr, context);
	if (compound_expr.final_expr.is_error())
	{
		return ast::make_error_expression(src_tokens, std::move(result_node));
	}
	else if (is_noreturn && (compound_expr.final_expr.is_null() || compound_expr.final_expr.is_noreturn()))
	{
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::noreturn, ast::make_void_typespec(nullptr),
			std::move(result_node)
		);
	}
	else if (compound_expr.final_expr.is_null())
	{
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::none, ast::make_void_typespec(nullptr),
			std::move(result_node)
		);
	}
	else
	{
		auto const [result_type, result_kind] = compound_expr.final_expr.get_expr_type_and_kind();
		return ast::make_dynamic_expression(
			src_tokens,
			result_kind, result_type,
			std::move(result_node)
		);
	}
}

static ast::expression resolve_expr(
	lex::src_tokens src_tokens,
	ast::expr_if if_expr_,
	ctx::parse_context &context
)
{
	auto result_node = ast::make_ast_unique<ast::expr_if>(std::move(if_expr_));
	auto &if_expr = *result_node;
	resolve_expression(if_expr.condition, context);
	resolve_expression(if_expr.then_block, context);
	resolve_expression(if_expr.else_block, context);

	{
		auto bool_type = ast::make_base_type_typespec({}, context.get_builtin_type_info(ast::type_info::bool_));
		context.match_expression_to_type(if_expr.condition, bool_type);
	}

	if (if_expr.condition.is_error() || if_expr.then_block.is_error() || if_expr.else_block.is_error())
	{
		return ast::make_error_expression(src_tokens, std::move(result_node));
	}
	else if (if_expr.then_block.is_noreturn() && if_expr.else_block.is_noreturn())
	{
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::noreturn, ast::make_void_typespec(nullptr),
			std::move(result_node)
		);
	}
	else if (if_expr.then_block.is_none() || if_expr.else_block.is_none())
	{
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::none, ast::make_void_typespec(nullptr),
			std::move(result_node)
		);
	}
	else
	{
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::if_expr, ast::typespec(),
			std::move(result_node)
		);
	}
}

static void check_switch_type(
	ast::expression &matched_expr,
	ast::typespec_view match_type,
	ctx::parse_context &context
)
{
	if (matched_expr.is_error())
	{
		return;
	}

	if (!match_type.is<ast::ts_base_type>())
	{
		if (do_verbose)
		{
			context.report_error(
				matched_expr, bz::format("invalid type '{}' for switch expression", match_type),
				{ context.make_note("only integral types can be used in switch expressions") }
			);
		}
		else
		{
			context.report_error(matched_expr, bz::format("invalid type '{}' for switch expression", match_type));
		}
		matched_expr.to_error();
	}
	else if (
		auto const info = match_type.get<ast::ts_base_type>().info;
		!ast::is_integer_kind(info->kind)
		&& info->kind != ast::type_info::char_
		&& info->kind != ast::type_info::bool_
	)
	{
		if (do_verbose)
		{
			context.report_error(
				matched_expr, bz::format("invalid type '{}' for switch expression", match_type),
				{ context.make_note("only integral types can be used in switch expressions") }
			);
		}
		else
		{
			context.report_error(matched_expr, bz::format("invalid type '{}' for switch expression", match_type));
		}
		matched_expr.to_error();
	}
}

static ast::expression resolve_expr(
	lex::src_tokens src_tokens,
	ast::expr_switch switch_expr_,
	ctx::parse_context &context
)
{
	auto result_node = ast::make_ast_unique<ast::expr_switch>(std::move(switch_expr_));
	auto &switch_expr = *result_node;
	resolve_expression(switch_expr.matched_expr, context);
	for (auto &[case_values, case_expr] : switch_expr.cases)
	{
		for (auto &case_value : case_values)
		{
			resolve_expression(case_value, context);
		}
		resolve_expression(case_expr, context);
	}
	resolve_expression(switch_expr.default_case, context);

	ast::typespec match_type = ast::make_auto_typespec(nullptr);
	context.match_expression_to_type(switch_expr.matched_expr, match_type);
	check_switch_type(switch_expr.matched_expr, match_type, context);
	if (switch_expr.matched_expr.is_error())
	{
		return ast::make_error_expression(src_tokens, std::move(result_node));
	}

	for (auto &[case_values, case_expr] : switch_expr.cases)
	{
		for (auto &case_value : case_values)
		{
			context.match_expression_to_type(case_value, match_type);
			parse::consteval_try(case_value, context);
		}
	}

	bool const is_good = switch_expr.cases.is_all([](auto const &switch_case) {
		return switch_case.expr.not_error()
			&& switch_case.values.is_all([](auto const &value) {
				return value.not_error();
			});
	});

	bool const is_all_unique = [&]() {
		ast::arena_vector<ast::expression const *> case_values;
		for (auto const &[values, _] : switch_expr.cases)
		{
			for (auto const &value : values)
			{
				if (value.is_error())
				{
					return true;
				}
				bz_assert(value.is<ast::constant_expression>());
				case_values.push_back(&value);
			}
		}
		if (case_values.size() == 0)
		{
			return true;
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
							rhs.src_tokens,
							bz::format("duplicate value {} in switch expression", lhs_value.get<ast::constant_value::sint>()),
							{ context.make_note(lhs.src_tokens, "value previously used here") }
						);
						break;
					case ast::constant_value::uint:
						context.report_error(
							rhs.src_tokens,
							bz::format("duplicate value {} in switch expression", lhs_value.get<ast::constant_value::uint>()),
							{ context.make_note(lhs.src_tokens, "value previously used here") }
						);
						break;
					case ast::constant_value::u8char:
						context.report_error(
							rhs.src_tokens,
							bz::format(
								"duplicate value '{}' in switch expression",
								add_escape_sequences(lhs_value.get<ast::constant_value::u8char>())
							),
							{ context.make_note(lhs.src_tokens, "value previously used here") }
						);
						break;
					case ast::constant_value::boolean:
						context.report_error(
							rhs.src_tokens,
							bz::format("duplicate value '{}' in switch expression", lhs_value.get<ast::constant_value::boolean>()),
							{ context.make_note(lhs.src_tokens, "value previously used here") }
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
	}();

	if (!is_all_unique || !is_good)
	{
		return ast::make_error_expression(src_tokens, std::move(result_node));
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
			src_tokens,
			"switch expression doesn't cover all possible values and doesn't have an else case"
		);
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::none,
			ast::make_void_typespec(nullptr),
			std::move(result_node)
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
			src_tokens,
			expr_kind,
			ast::typespec(),
			std::move(result_node)
		);
	}
	else
	{
		return ast::make_dynamic_expression(
			src_tokens,
			expr_kind,
			ast::make_void_typespec(nullptr),
			std::move(result_node)
		);
	}
}

void resolve_expression(ast::expression &expr, ctx::parse_context &context)
{
	if (expr.is_unresolved())
	{
		auto const expr_consteval_state = expr.consteval_state;
		auto const expr_paren_level = expr.paren_level;
		expr = expr.get_unresolved_expr().visit([&](auto &inner_expr) {
			return resolve_expr(expr.src_tokens, std::move(inner_expr), context);
		});
		expr.consteval_state = expr_consteval_state;
		expr.paren_level = expr_paren_level;
		parse::consteval_guaranteed(expr, context);
	}
}

} // namespace resolve
