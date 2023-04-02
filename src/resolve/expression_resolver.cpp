#include "expression_resolver.h"
#include "statement_resolver.h"
#include "match_expression.h"
#include "consteval.h"
#include "global_data.h"
#include "parse/expression_parser.h"
#include "escape_sequences.h"
#include "colors.h"

namespace resolve
{

static ast::expression resolve_expr(
	lex::src_tokens const &,
	ast::expr_unresolved_identifier id_expr,
	ctx::parse_context &context
)
{
	return context.make_identifier_expression(std::move(id_expr.id));
}

static ast::expression resolve_expr(
	lex::src_tokens const &src_tokens,
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

static ast::expression resolve_variadic_expr(
	lex::src_tokens const &src_tokens,
	ast::expr_unary_op &unary_op,
	ctx::parse_context &context
)
{
	auto const info = context.push_variadic_resolver();
	ast::arena_vector<ast::expression> variadic_exprs;
	variadic_exprs.push_back(unary_op.expr);
	resolve_expression(variadic_exprs[0], context);
	if (!context.variadic_info.found_variadic && variadic_exprs[0].is_typename())
	{
		context.pop_variadic_resolver(info);
		return context.make_unary_operator_expression(src_tokens, unary_op.op, std::move(variadic_exprs[0]));
	}
	else if (!context.variadic_info.found_variadic)
	{
		context.report_error(unary_op.expr.src_tokens, "unable to expand non-variadic expression");
		context.pop_variadic_resolver(info);
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(unary_op.op, std::move(variadic_exprs[0])));
	}
	else if (variadic_exprs[0].is_error())
	{
		context.pop_variadic_resolver(info);
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(unary_op.op, std::move(variadic_exprs[0])));
	}
	else
	{
		auto const variadic_size = context.variadic_info.variadic_size;
		if (variadic_size == 0)
		{
			context.pop_variadic_resolver(info);
			return ast::make_expanded_variadic_expression(src_tokens, ast::arena_vector<ast::expression>{});
		}
		else if (variadic_size == 1)
		{
			context.pop_variadic_resolver(info);
			return ast::make_expanded_variadic_expression(src_tokens, std::move(variadic_exprs));
		}
		else
		{
			variadic_exprs.reserve(variadic_size);
			for ([[maybe_unused]] auto const _ : bz::iota(0, variadic_size - 2))
			{
				variadic_exprs.push_back(unary_op.expr);
			}
			// the last element is moved in and not copied
			variadic_exprs.push_back(std::move(unary_op.expr));

			// the first one has already been resolved
			for (auto &expr : variadic_exprs.slice(1))
			{
				context.variadic_info.variadic_index += 1;
				resolve_expression(expr, context);
				// return early if there's an error
				if (expr.is_error())
				{
					context.pop_variadic_resolver(info);
					return ast::make_error_expression(
						src_tokens,
						ast::make_expr_unary_op(unary_op.op, std::move(variadic_exprs.back()))
					);
				}
			}
			context.pop_variadic_resolver(info);
			return ast::make_expanded_variadic_expression(src_tokens, std::move(variadic_exprs));
		}
	}
}

static ast::expression resolve_expr(
	lex::src_tokens const &src_tokens,
	ast::expr_unary_op unary_op,
	ctx::parse_context &context
)
{
	// special case for variadic expansion
	if (unary_op.op == lex::token::dot_dot_dot)
	{
		return resolve_variadic_expr(src_tokens, unary_op, context);
	}
	else if (is_unary_has_unevaluated_context(unary_op.op))
	{
		auto const prev_value = context.push_unevaluated_context();
		resolve_expression(unary_op.expr, context);
		context.pop_unevaluated_context(prev_value);
	}
	else
	{
		resolve_expression(unary_op.expr, context);
	}
	return context.make_unary_operator_expression(src_tokens, unary_op.op, std::move(unary_op.expr));
}

static ast::expression resolve_expr(
	lex::src_tokens const &src_tokens,
	ast::expr_binary_op binary_op,
	ctx::parse_context &context
)
{
	if (token_info[binary_op.op].binary_prec.is_left_associative)
	{
		resolve_expression(binary_op.lhs, context);
		resolve_expression(binary_op.rhs, context);
	}
	else
	{
		resolve_expression(binary_op.rhs, context);
		resolve_expression(binary_op.lhs, context);
	}
	return context.make_binary_operator_expression(src_tokens, binary_op.op, std::move(binary_op.lhs), std::move(binary_op.rhs));
}

static ast::expression resolve_expr(
	lex::src_tokens const &src_tokens,
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
	lex::src_tokens const &src_tokens,
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
	lex::src_tokens const &src_tokens,
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
	lex::src_tokens const &src_tokens,
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
	lex::src_tokens const &src_tokens,
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
		[](ast::stmt_defer const &) {
			return false;
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
		[](ast::decl_operator_alias const &) {
			return false;
		},
		[](ast::decl_type_alias const &) {
			return false;
		},
		[](ast::decl_struct const &) {
			return false;
		},
		[](ast::decl_enum const &) {
			return false;
		},
		[](ast::decl_import const &) {
			return false;
		},
	});
}

static ast::expression resolve_expr(
	lex::src_tokens const &src_tokens,
	ast::expr_compound compound_expr_,
	ctx::parse_context &context
)
{
	auto result_node = ast::make_ast_unique<ast::expr_compound>(std::move(compound_expr_));
	auto &compound_expr = *result_node;
	bool is_noreturn = false;
	if (compound_expr.scope.is_null())
	{
		compound_expr.scope = ast::make_local_scope(context.get_current_enclosing_scope(), false);
	}
	context.push_local_scope(&compound_expr.scope);
	for (auto &stmt : compound_expr.statements)
	{
		resolve_statement(stmt, context);
		is_noreturn |= is_statement_noreturn(stmt);
	}
	resolve_expression(compound_expr.final_expr, context);
	context.pop_local_scope(true);
	if (compound_expr.final_expr.is_error())
	{
		return ast::make_error_expression(src_tokens, std::move(result_node));
	}
	else if (is_noreturn && (compound_expr.final_expr.is_null() || compound_expr.final_expr.is_noreturn()))
	{
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::noreturn, ast::make_void_typespec(nullptr),
			std::move(result_node),
			ast::destruct_operation()
		);
	}
	else if (compound_expr.final_expr.is_null())
	{
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::none, ast::make_void_typespec(nullptr),
			std::move(result_node),
			ast::destruct_operation()
		);
	}
	else
	{
		auto const [result_type, result_kind] = compound_expr.final_expr.get_expr_type_and_kind();
		return ast::make_dynamic_expression(
			src_tokens,
			result_kind, result_type,
			std::move(result_node),
			ast::destruct_operation()
		);
	}
}

static ast::expression resolve_expr(
	lex::src_tokens const &src_tokens,
	ast::expr_if if_expr_,
	ctx::parse_context &context
)
{
	auto result_node = ast::make_ast_unique<ast::expr_if>(std::move(if_expr_));
	auto &if_expr = *result_node;

	resolve_expression(if_expr.condition, context);
	{
		auto bool_type = ast::make_base_type_typespec({}, context.get_builtin_type_info(ast::type_info::bool_));
		match_expression_to_type(if_expr.condition, bool_type, context);
	}

	context.push_move_scope(src_tokens);
	context.push_new_move_branch();
	resolve_expression(if_expr.then_block, context);
	context.push_new_move_branch();
	resolve_expression(if_expr.else_block, context);
	context.pop_move_scope();

	if (if_expr.condition.is_error() || if_expr.then_block.is_error() || if_expr.else_block.is_error())
	{
		return ast::make_error_expression(src_tokens, std::move(result_node));
	}
	else if (if_expr.then_block.is_noreturn() && if_expr.else_block.is_noreturn())
	{
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::noreturn, ast::make_void_typespec(nullptr),
			std::move(result_node),
			ast::destruct_operation()
		);
	}
	else if (if_expr.then_block.is_none() || if_expr.else_block.is_null() || if_expr.else_block.is_none())
	{
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::none, ast::make_void_typespec(nullptr),
			std::move(result_node),
			ast::destruct_operation()
		);
	}
	else if (if_expr.then_block.is_typename() && if_expr.else_block.is_typename())
	{
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::if_expr, ast::make_typename_typespec(nullptr),
			std::move(result_node),
			ast::destruct_operation()
		);
	}
	else
	{
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::if_expr, ast::typespec(),
			std::move(result_node),
			ast::destruct_operation()
		);
	}
}

static ast::expression resolve_expr(
	lex::src_tokens const &src_tokens,
	ast::expr_if_consteval if_expr_,
	ctx::parse_context &context
)
{
	auto result_node = ast::make_ast_unique<ast::expr_if_consteval>(std::move(if_expr_));
	auto &if_expr = *result_node;
	resolve_expression(if_expr.condition, context);

	{
		auto bool_type = ast::make_base_type_typespec({}, context.get_builtin_type_info(ast::type_info::bool_));
		match_expression_to_type(if_expr.condition, bool_type, context);
	}

	if (if_expr.condition.is_error())
	{
		return ast::make_error_expression(src_tokens, std::move(result_node));
	}

	consteval_try(if_expr.condition, context);
	if (if_expr.condition.has_consteval_failed())
	{
		context.report_error(if_expr.condition.src_tokens, "condition for an if consteval expression must be a constant expression");
		return ast::make_error_expression(src_tokens, std::move(result_node));
	}

	auto const &condition_value = if_expr.condition.get_constant_value();
	bz_assert(condition_value.is_boolean());
	if (condition_value.get_boolean())
	{
		resolve_expression(if_expr.then_block, context);
		if (if_expr.then_block.is_error())
		{
			return ast::make_error_expression(src_tokens, std::move(result_node));
		}
		auto const [type, kind] = if_expr.then_block.get_expr_type_and_kind();
		return ast::make_dynamic_expression(src_tokens, kind, type, std::move(result_node), ast::destruct_operation());
	}
	else if (if_expr.else_block.not_null())
	{
		resolve_expression(if_expr.else_block, context);
		if (if_expr.else_block.is_error())
		{
			return ast::make_error_expression(src_tokens, std::move(result_node));
		}
		auto const [type, kind] = if_expr.else_block.get_expr_type_and_kind();
		return ast::make_dynamic_expression(src_tokens, kind, type, std::move(result_node), ast::destruct_operation());
	}
	else
	{
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::none, ast::typespec(),
			ast::constant_value::get_void(),
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

	if (match_type.is<ast::ts_enum>())
	{
		// this is a valid type
		return;
	}

	if (match_type.is<ast::ts_base_type>())
	{
		auto const info = match_type.get<ast::ts_base_type>().info;
		if (
			ast::is_integer_kind(info->kind)
			|| info->kind == ast::type_info::char_
			|| info->kind == ast::type_info::bool_
			|| info->kind == ast::type_info::str_
		)
		{
			// integers, char, bool and str are valid
			return;
		}
	}

	bz::vector<ctx::source_highlight> notes = {};
	if (do_verbose)
	{
		notes.push_back(context.make_note("only integer types, enum types, 'char', 'bool' and 'str' can be used in switch expressions"));
	}
	context.report_error(matched_expr, bz::format("invalid type '{}' for switch expression", match_type), std::move(notes));
	matched_expr.to_error();
}

static ast::expression resolve_expr(
	lex::src_tokens const &src_tokens,
	ast::expr_switch switch_expr_,
	ctx::parse_context &context
)
{
	auto result_node = ast::make_ast_unique<ast::expr_switch>(std::move(switch_expr_));
	auto &switch_expr = *result_node;

	resolve_expression(switch_expr.matched_expr, context);
	ast::typespec match_type = ast::make_auto_typespec(nullptr);
	match_expression_to_type(switch_expr.matched_expr, match_type, context);
	check_switch_type(switch_expr.matched_expr, match_type, context);

	context.push_move_scope(src_tokens);
	for (auto &[case_values, case_expr] : switch_expr.cases)
	{
		for (auto &case_value : case_values)
		{
			resolve_expression(case_value, context);
		}
		context.push_new_move_branch();
		resolve_expression(case_expr, context);
	}
	context.push_new_move_branch();
	resolve_expression(switch_expr.default_case, context);
	context.pop_move_scope();

	if (switch_expr.matched_expr.is_error())
	{
		return ast::make_error_expression(src_tokens, std::move(result_node));
	}

	for (auto &[case_values, case_expr] : switch_expr.cases)
	{
		for (auto &case_value : case_values)
		{
			match_expression_to_type(case_value, match_type, context);
			consteval_try(case_value, context);
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
				bz_assert(value.is_constant());
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
			auto const &lhs_value = lhs.get_constant_value();
			for (size_t j = i + 1; j < case_values.size(); ++j)
			{
				auto const &rhs = *case_values[j];
				auto const &rhs_value = rhs.get_constant_value();
				if (lhs_value == rhs_value)
				{
					switch (rhs_value.kind())
					{
					static_assert(ast::constant_value::variant_count == 19);
					case ast::constant_value::sint:
						context.report_error(
							rhs.src_tokens,
							bz::format("duplicate value {} in switch expression", lhs_value.get_sint()),
							{ context.make_note(lhs.src_tokens, "value previously used here") }
						);
						break;
					case ast::constant_value::uint:
						context.report_error(
							rhs.src_tokens,
							bz::format("duplicate value {} in switch expression", lhs_value.get_uint()),
							{ context.make_note(lhs.src_tokens, "value previously used here") }
						);
						break;
					case ast::constant_value::u8char:
						context.report_error(
							rhs.src_tokens,
							bz::format(
								"duplicate value '{}' in switch expression",
								add_escape_sequences(lhs_value.get_u8char())
							),
							{ context.make_note(lhs.src_tokens, "value previously used here") }
						);
						break;
					case ast::constant_value::boolean:
						context.report_error(
							rhs.src_tokens,
							bz::format("duplicate value '{}' in switch expression", lhs_value.get_boolean()),
							{ context.make_note(lhs.src_tokens, "value previously used here") }
						);
						break;
					case ast::constant_value::enum_:
						context.report_error(
							rhs.src_tokens,
							bz::format("duplicate value '{}' in switch expression", get_value_string(lhs_value)),
							{ context.make_note(lhs.src_tokens, "value previously used here") }
						);
						break;
					case ast::constant_value::string:
						context.report_error(
							rhs.src_tokens,
							bz::format("duplicate value {} in switch expression", get_value_string(lhs_value)),
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
	auto const max_case_count = [&]() -> uint64_t {
		if (match_type.is<ast::ts_base_type>())
		{
			auto const match_type_info = match_type.get<ast::ts_base_type>().info;

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
			case ast::type_info::str_:
				// just return a basically infinite value
				return std::numeric_limits<uint64_t>::max();
			default:
				bz_unreachable;
			}
		}
		else
		{
			bz_assert(match_type.is<ast::ts_enum>());
			return match_type.get<ast::ts_enum>().decl->get_unique_values_count();
		}
	}();

	if (case_count < max_case_count && switch_expr.default_case.is_null())
	{
		context.report_warning(
			ctx::warning_kind::non_exhaustive_switch,
			src_tokens,
			"switch expression doesn't cover all possible values and doesn't have an else case"
		);
		result_node->is_complete = false;
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::none,
			ast::make_void_typespec(nullptr),
			std::move(result_node),
			ast::destruct_operation()
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

	result_node->is_complete = true;

	auto const get_expr_kind = [](auto const &expr) {
		bz_assert(expr.is_constant_or_dynamic());
		auto const kind = expr.get_expr_type_and_kind().second;
		switch (kind)
		{
		case ast::expression_type_kind::none:
		case ast::expression_type_kind::noreturn:
			return kind;
		default:
			return ast::expression_type_kind::switch_expr;
		}
	};

	auto const expr_kind_reduce = [](auto const lhs, auto const rhs) {
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
	};

	auto const start_kind = switch_expr.default_case.not_null()
		? expr_kind_reduce(ast::expression_type_kind::noreturn, get_expr_kind(switch_expr.default_case))
		: ast::expression_type_kind::noreturn;

	auto const expr_kind = switch_expr.cases
		.member<&ast::switch_case::expr>()
		.transform(get_expr_kind)
		.reduce(start_kind, expr_kind_reduce);

	if (expr_kind == ast::expression_type_kind::switch_expr)
	{
		return ast::make_dynamic_expression(
			src_tokens,
			expr_kind,
			ast::typespec(),
			std::move(result_node),
			ast::destruct_operation()
		);
	}
	else
	{
		return ast::make_dynamic_expression(
			src_tokens,
			expr_kind,
			ast::make_void_typespec(nullptr),
			std::move(result_node),
			ast::destruct_operation()
		);
	}
}

static ast::expression resolve_expr(
	lex::src_tokens const &src_tokens,
	ast::expr_unresolved_array_type array_type,
	ctx::parse_context &context
)
{
	for (auto &size : array_type.sizes)
	{
		resolve_expression(size, context);
	}
	resolve_expression(array_type.type, context);

	bool good = true;
	auto const sizes = array_type.sizes
		.transform([&good, &context](auto &size) -> uint64_t {
			if (size.is_placeholder_literal())
			{
				consteval_try(size, context);
				return 0;
			}

			auto auto_type = ast::make_auto_typespec(nullptr);
			match_expression_to_type(size, auto_type, context);
			consteval_try(size, context);
			if (size.is_error())
			{
				good = false;
				return 0;
			}
			else if (!size.is_constant())
			{
				good = false;
				context.report_error(size.src_tokens, "array size must be a constant expression");
				return 0;
			}
			else
			{
				ast::constant_value const &size_value = size.get_constant_value();
				switch (size_value.kind())
				{
				static_assert(ast::constant_value::variant_count == 19);
				case ast::constant_value::sint:
				{
					auto const value = size_value.get_sint();
					if (value <= 0)
					{
						good = false;
						context.report_error(
							size.src_tokens,
							bz::format("invalid array size {}, it must be a positive integer", value)
						);
					}
					return static_cast<uint64_t>(value);
				}
				case ast::constant_value::uint:
				{
					auto const value = size_value.get_uint();
					if (value == 0)
					{
						good = false;
						context.report_error(
							size.src_tokens,
							bz::format("invalid array size {}, it must be a positive integer", value)
						);
					}
					return value;
				}
				default:
					good = false;
					context.report_error(
						size.src_tokens,
						bz::format("invalid type '{}' as array size", size.get_expr_type())
					);
					return 0;
				}
			}
		})
		.collect();

	if (!array_type.type.is_typename())
	{
		good = false;
		context.report_error(array_type.type.src_tokens, "expected a type as the array element type");
	}

	if (!good)
	{
		return ast::make_error_expression(src_tokens);
	}
	else if (array_type.sizes.empty())
	{
		auto &elem_type = array_type.type.get_typename();
		if (elem_type.is<ast::ts_void>())
		{
			context.report_error(array_type.type.src_tokens, "array element type cannot be 'void'");
			return ast::make_error_expression(src_tokens);
		}
		else if (elem_type.is<ast::ts_consteval>())
		{
			auto const consteval_pos = array_type.type.src_tokens.pivot != nullptr
				&& array_type.type.src_tokens.pivot->kind == lex::token::kw_consteval
					? array_type.type.src_tokens.pivot
					: lex::token_pos(nullptr);
			auto const [consteval_begin, consteval_end] = consteval_pos == nullptr
				? std::make_pair(ctx::char_pos(), ctx::char_pos())
				: std::make_pair(consteval_pos->src_pos.begin, consteval_pos->src_pos.end);
			context.report_error(
				array_type.type.src_tokens, "array slice element type cannot be 'consteval'",
				{}, { context.make_suggestion_before(
					src_tokens.begin, ctx::char_pos(), ctx::char_pos(), "consteval ",
					consteval_pos, consteval_begin, consteval_end, "const",
					"make the array slice type 'consteval'"
				) }
			);
			return ast::make_error_expression(src_tokens);
		}
		else if (elem_type.is<ast::ts_lvalue_reference>())
		{
			context.report_error(array_type.type.src_tokens, "array element type cannot be a reference type");
			return ast::make_error_expression(src_tokens);
		}
		else if (elem_type.is<ast::ts_move_reference>())
		{
			context.report_error(array_type.type.src_tokens, "array element type cannot be a move reference type");
			return ast::make_error_expression(src_tokens);
		}
		else if (elem_type.is<ast::ts_auto_reference>())
		{
			context.report_error(array_type.type.src_tokens, "array element type cannot be an auto reference type");
			return ast::make_error_expression(src_tokens);
		}
		else if (elem_type.is<ast::ts_auto_reference_const>())
		{
			context.report_error(array_type.type.src_tokens, "array element type cannot be an auto reference-const type");
			return ast::make_error_expression(src_tokens);
		}
		else if (elem_type.is<ast::ts_variadic>())
		{
			context.report_error(array_type.type.src_tokens, "array element type cannot be a variadic type");
			return ast::make_error_expression(src_tokens);
		}
		else
		{
			return ast::type_as_expression(src_tokens, ast::make_array_slice_typespec(src_tokens, std::move(elem_type)));
		}
	}
	else
	{
		auto &elem_type = array_type.type.get_typename();
		if (elem_type.is<ast::ts_void>())
		{
			context.report_error(array_type.type.src_tokens, "array element type cannot be 'void'");
			return ast::make_error_expression(src_tokens);
		}
		else if (elem_type.is<ast::ts_const>())
		{
			good = false;
			auto const const_pos = array_type.type.src_tokens.pivot != nullptr
				&& array_type.type.src_tokens.pivot->kind == lex::token::kw_const
					? array_type.type.src_tokens.pivot
					: lex::token_pos(nullptr);
			auto const [const_begin, const_end] = const_pos == nullptr
				? std::make_pair(ctx::char_pos(), ctx::char_pos())
				: std::make_pair(const_pos->src_pos.begin, (const_pos + 1)->src_pos.begin);
			context.report_error(
				array_type.type.src_tokens, "array element type cannot be 'const'",
				{}, { context.make_suggestion_before(
					src_tokens.begin, const_begin, const_end,
					"const ", "make the array type 'const'"
				) }
			);
			return ast::make_error_expression(src_tokens);
		}
		else if (elem_type.is<ast::ts_consteval>())
		{
			good = false;
			auto const consteval_pos = array_type.type.src_tokens.pivot != nullptr
				&& array_type.type.src_tokens.pivot->kind == lex::token::kw_consteval
					? array_type.type.src_tokens.pivot
					: lex::token_pos(nullptr);
			auto const [consteval_begin, consteval_end] = consteval_pos == nullptr
				? std::make_pair(ctx::char_pos(), ctx::char_pos())
				: std::make_pair(consteval_pos->src_pos.begin, (consteval_pos + 1)->src_pos.begin);
			context.report_error(
				array_type.type.src_tokens, "array element type cannot be 'consteval'",
				{}, { context.make_suggestion_before(
					src_tokens.begin, consteval_begin, consteval_end,
					"consteval ", "make the array type 'consteval'"
				) }
			);
			return ast::make_error_expression(src_tokens);
		}
		else if (elem_type.is<ast::ts_lvalue_reference>())
		{
			context.report_error(array_type.type.src_tokens, "array element type cannot be a reference type");
			return ast::make_error_expression(src_tokens);
		}
		else if (elem_type.is<ast::ts_move_reference>())
		{
			context.report_error(array_type.type.src_tokens, "array element type cannot be a move reference type");
			return ast::make_error_expression(src_tokens);
		}
		else if (elem_type.is<ast::ts_auto_reference>())
		{
			context.report_error(array_type.type.src_tokens, "array element type cannot be an auto reference type");
			return ast::make_error_expression(src_tokens);
		}
		else if (elem_type.is<ast::ts_auto_reference_const>())
		{
			context.report_error(array_type.type.src_tokens, "array element type cannot be an auto reference-const type");
			return ast::make_error_expression(src_tokens);
		}
		else if (elem_type.is<ast::ts_variadic>())
		{
			context.report_error(array_type.type.src_tokens, "array element type cannot be a variadic type");
			return ast::make_error_expression(src_tokens);
		}
		else
		{
			for (auto const size : sizes.reversed())
			{
				elem_type = ast::make_array_typespec(src_tokens, size, std::move(elem_type));
			}
			return ast::type_as_expression(src_tokens, std::move(elem_type));
		}
	}
}

static ast::expression resolve_expr(
	lex::src_tokens const &src_tokens,
	ast::expr_unresolved_generic_type_instantiation generic_instantiation,
	ctx::parse_context &context
)
{
	resolve_expression(generic_instantiation.base, context);
	for (auto &arg : generic_instantiation.args)
	{
		resolve_expression(arg, context);
	}
	return context.make_generic_type_instantiation_expression(
		src_tokens,
		std::move(generic_instantiation.base),
		std::move(generic_instantiation.args)
	);
}

static ast::expression resolve_expr(
	lex::src_tokens const &src_tokens,
	ast::expr_unresolved_function_type function_type,
	ctx::parse_context &context
)
{
	bool good = true;
	for (auto &param_type : function_type.param_types)
	{
		resolve_expression(param_type, context);
		if (!param_type.is_typename())
		{
			context.report_error(param_type.src_tokens, "expected a type");
			good = false;
		}
	}
	resolve_expression(function_type.return_type, context);
	if (!function_type.return_type.is_typename())
	{
		context.report_error(function_type.return_type.src_tokens, "expected a type");
		good = false;
	}

	if (!good)
	{
		return ast::make_error_expression(src_tokens);
	}

	auto fn_type = ast::make_function_typespec(
		src_tokens,
		function_type.param_types.transform([](auto &param_type) {
			return std::move(param_type.get_typename());
		}).collect<ast::arena_vector>(),
		std::move(function_type.return_type.get_typename()),
		function_type.cc
	);
	return ast::make_constant_expression(
		src_tokens,
		ast::expression_type_kind::type_name,
		ast::make_typename_typespec(nullptr),
		ast::constant_value(std::move(fn_type)),
		ast::make_expr_typename_literal()
	);
}

static ast::expression resolve_expr(
	lex::src_tokens const &src_tokens,
	ast::expr_unresolved_integer_range range,
	ctx::parse_context &context
)
{
	resolve_expression(range.begin, context);
	resolve_expression(range.end, context);

	return context.make_integer_range_expression(src_tokens, std::move(range.begin), std::move(range.end));
}

static ast::expression resolve_expr(
	lex::src_tokens const &src_tokens,
	ast::expr_unresolved_integer_range_from range_from,
	ctx::parse_context &context
)
{
	resolve_expression(range_from.begin, context);

	return context.make_integer_range_from_expression(src_tokens, std::move(range_from.begin));
}

static ast::expression resolve_expr(
	lex::src_tokens const &src_tokens,
	ast::expr_unresolved_integer_range_to range_to,
	ctx::parse_context &context
)
{
	resolve_expression(range_to.end, context);

	return context.make_integer_range_to_expression(src_tokens, std::move(range_to.end));
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
		// bz_assert(!expr.is_unresolved());
		expr.consteval_state = expr_consteval_state;
		expr.paren_level = expr_paren_level;
	}
}

} // namespace resolve
