#include "statement_resolver.h"
#include "expression_resolver.h"
#include "parse/consteval.h"
#include "parse/expression_parser.h"
#include "parse/statement_parser.h"

namespace resolve
{

static void resolve_stmt(ast::stmt_while &while_stmt, ctx::parse_context &context)
{
	if (context.in_generic_function())
	{
		return;
	}
	resolve_expression(while_stmt.condition, context);
	resolve_statement(while_stmt.while_block, context);
}

static void resolve_stmt(ast::stmt_for &for_stmt, ctx::parse_context &context)
{
	if (context.in_generic_function())
	{
		return;
	}
	resolve_statement(for_stmt.init, context);
	resolve_expression(for_stmt.condition, context);
	resolve_expression(for_stmt.iteration, context);
	resolve_statement(for_stmt.for_block, context);
}

static void resolve_stmt(ast::stmt_foreach &foreach_stmt, ctx::parse_context &context)
{
	if (context.in_generic_function())
	{
		return;
	}
	resolve_statement(foreach_stmt.range_var_decl, context);
	resolve_statement(foreach_stmt.iter_var_decl, context);
	resolve_statement(foreach_stmt.end_var_decl, context);
	resolve_statement(foreach_stmt.iter_deref_var_decl, context);
	resolve_expression(foreach_stmt.condition, context);
	resolve_expression(foreach_stmt.iteration, context);
	resolve_statement(foreach_stmt.for_block, context);
}

static void resolve_stmt(ast::stmt_return &return_stmt, ctx::parse_context &context)
{
	if (return_stmt.expr.not_null() && !context.in_generic_function())
	{
		resolve_expression(return_stmt.expr, context);
		bz_assert(context.current_function != nullptr);
		context.match_expression_to_type(return_stmt.expr, context.current_function->return_type);
	}
}

static void resolve_stmt(ast::stmt_no_op &, ctx::parse_context &)
{
	// nothing
}

static bz::u8string get_static_assert_expression(ast::constant_expression const &cond)
{
	if (cond.expr.is<ast::expr_binary_op>())
	{
		auto const &binary_op = cond.expr.get<ast::expr_binary_op>();
		switch (binary_op.op)
		{
		case lex::token::equals:
		case lex::token::not_equals:
		case lex::token::less_than:
		case lex::token::less_than_eq:
		case lex::token::greater_than:
		case lex::token::greater_than_eq:
		case lex::token::bool_and:
		case lex::token::bool_xor:
		case lex::token::bool_or:
		{
			auto const op_str = token_info[binary_op.op].token_value;
			auto const &lhs = binary_op.lhs;
			bz_assert(lhs.is<ast::constant_expression>());
			auto const lhs_str = ast::get_value_string(lhs.get<ast::constant_expression>().value);
			if (lhs_str == "")
			{
				return "";
			}
			auto const &rhs = binary_op.rhs;
			bz_assert(rhs.is<ast::constant_expression>());
			auto const rhs_str = ast::get_value_string(rhs.get<ast::constant_expression>().value);
			if (rhs_str == "")
			{
				return "";
			}
			return bz::format("{} {} {}", lhs_str, op_str, rhs_str);
		}

		default:
			return "";
		}
	}
	else if (cond.expr.is<ast::expr_literal>())
	{
		return ast::get_value_string(cond.value);
	}
	else
	{
		return "";
	}
}

static void resolve_stmt(ast::stmt_static_assert &static_assert_stmt, ctx::parse_context &context)
{
	if (static_assert_stmt.condition.not_null() || context.in_generic_function())
	{
		// already has been resolved
		return;
	}
	bool good = true;
	auto const match_type_and_consteval = [&](
		ast::expression &expr,
		uint32_t base_type_kind
	) {
		if (expr.is_error())
		{
			return;
		}

		auto base_type = ast::make_base_type_typespec({}, context.get_builtin_type_info(base_type_kind));
		context.match_expression_to_type(expr, base_type);
		if (expr.not_error())
		{
			parse::consteval_try_without_error(expr, context);
		}
		else
		{
			good = false;
		}
	};

	{
		auto [stream, end] = static_assert_stmt.arg_tokens;
		auto args = parse::parse_expression_comma_list(stream, end, context);

		if (stream != end)
		{
			context.report_error({ stream, stream, end });
		}

		if (args.size() == 0)
		{
			context.report_error(static_assert_stmt.static_assert_pos, "static_assert expects exactly one or two arguments");
			static_assert_stmt.condition = ast::make_error_expression(lex::src_tokens::from_single_token(static_assert_stmt.static_assert_pos));
			return;
		}
		else if (args.size() > 2)
		{
			context.report_error(static_assert_stmt.arg_tokens, "static_assert expects exactly one or two arguments");
			// continue resolving using the first two args
		}

		static_assert_stmt.condition = std::move(args[0]);
		if (args.size() > 1)
		{
			static_assert_stmt.message = std::move(args[1]);
		}
	}

	resolve_expression(static_assert_stmt.condition, context);
	match_type_and_consteval(static_assert_stmt.condition, ast::type_info::bool_);
	if (static_assert_stmt.condition.has_consteval_failed())
	{
		good = false;
		context.report_error(
			static_assert_stmt.condition,
			"condition for static_assert must be a constant expression",
			parse::get_consteval_fail_notes(static_assert_stmt.condition)
		);
	}

	if (static_assert_stmt.message.not_null())
	{
		resolve_expression(static_assert_stmt.message, context);
		match_type_and_consteval(static_assert_stmt.message, ast::type_info::str_);
		if (static_assert_stmt.message.has_consteval_failed())
		{
			good = false;
			context.report_error(
				static_assert_stmt.message,
				"message in static_assert must be a constant expression",
				parse::get_consteval_fail_notes(static_assert_stmt.message)
			);
		}
	}

	if (!good)
	{
		bz_assert(context.has_errors());
		return;
	}

	auto &cond_const_expr = static_assert_stmt.condition.get<ast::constant_expression>();
	bz_assert(cond_const_expr.value.kind() == ast::constant_value::boolean);
	auto const cond = cond_const_expr.value.get<ast::constant_value::boolean>();

	if (!cond)
	{
		auto const expression_string = get_static_assert_expression(cond_const_expr);
		bz::u8string error_message = "static assertion failed";
		if (expression_string != "")
		{
			error_message += bz::format(" due to requirement '{}'", expression_string);
		}
		if (static_assert_stmt.message.not_null() && static_assert_stmt.message.not_error())
		{
			auto &message_const_expr = static_assert_stmt.message.get<ast::constant_expression>();
			bz_assert(message_const_expr.value.kind() == ast::constant_value::string);
			auto const message = message_const_expr.value.get<ast::constant_value::string>().as_string_view();
			error_message += bz::format(", message: '{}'", message);
		}
		context.report_error(static_assert_stmt.condition, std::move(error_message));
	}
}

static void resolve_stmt(ast::stmt_expression &expr_stmt, ctx::parse_context &context)
{
	if (context.in_generic_function())
	{
		return;
	}
	resolve_expression(expr_stmt.expr, context);
}

static void apply_prototype(
	lex::token_range prototype,
	ast::decl_variable &var_decl,
	ctx::parse_context &context
)
{
	ast::expression type = ast::make_constant_expression(
		var_decl.src_tokens,
		ast::expression_type_kind::type_name,
		ast::make_typename_typespec(nullptr),
		ast::constant_value(var_decl.get_type()),
		ast::expr_t{}
	);
	for (auto op = prototype.end; op != prototype.begin;)
	{
		--op;
		auto const src_tokens = lex::src_tokens{ op, op, var_decl.src_tokens.end };
		type = context.make_unary_operator_expression(src_tokens, op->kind, std::move(type));
	}
	if (type.is_typename())
	{
		var_decl.get_type() = std::move(type.get_typename());
	}
	else
	{
		var_decl.state = ast::resolve_state::error;
	}
}

static void resolve_variable_type(ast::decl_variable &var_decl, ctx::parse_context &context)
{
	bz_assert(var_decl.state == ast::resolve_state::resolving_symbol);
	if (var_decl.tuple_decls.not_empty())
	{
		for (auto &decl : var_decl.tuple_decls)
		{
			bz_assert(decl.state < ast::resolve_state::symbol);
			decl.state = ast::resolve_state::resolving_symbol;
			resolve_variable_type(decl, context);
			if (decl.state != ast::resolve_state::error)
			{
				decl.state = ast::resolve_state::symbol;
			}
			else
			{
				var_decl.state = ast::resolve_state::error;
			}
		}
		if (var_decl.state != ast::resolve_state::error)
		{
			var_decl.get_type() = ast::make_tuple_typespec(
				{},
				var_decl.tuple_decls.transform([](auto const &decl) -> ast::typespec {
					return decl.get_type();
				}
			).collect<bz::vector>());
			apply_prototype(var_decl.get_prototype_range(), var_decl, context);
		}
		return;
	}

	if (!var_decl.get_type().is<ast::ts_unresolved>())
	{
		return;
	}
	auto [stream, end] = var_decl.get_type().get<ast::ts_unresolved>().tokens;
	auto type = stream == end
		? ast::make_constant_expression(
			{},
			ast::expression_type_kind::type_name,
			ast::make_typename_typespec(nullptr),
			ast::constant_value(ast::make_auto_typespec(nullptr)),
			ast::make_expr_identifier(ast::identifier{})
		)
		: parse::parse_expression(stream, end, context, no_assign);
	parse::consteval_try(type, context);
	if (type.not_error() && !type.has_consteval_succeeded())
	{
		context.report_error(
			type.src_tokens,
			"variable type must be a constant expression",
			parse::get_consteval_fail_notes(type)
		);
		var_decl.clear_type();
		var_decl.state = ast::resolve_state::error;
	}
	else if (type.not_error() && !type.is_typename())
	{
		if (stream != end && is_binary_operator(stream->kind))
		{
			bz_assert(stream->kind != lex::token::assign);
			context.report_error(
				{ stream, stream, end },
				"expected ';' or '=' at the end of a type",
				{ context.make_note(
					stream,
					bz::format("'operator {}' is not allowed in a variable declaration's type", stream->value)
				) }
			);
		}
		else if (stream != end)
		{
			context.report_error({ stream, stream, end });
		}

		context.report_error(type.src_tokens, "expected a type");
		var_decl.clear_type();
		var_decl.state = ast::resolve_state::error;
	}
	else if (type.is_typename())
	{
		auto const prototype = var_decl.get_prototype_range();
		for (auto op = prototype.end; op != prototype.begin;)
		{
			--op;
			auto const src_tokens = type.src_tokens.pivot == nullptr
				? lex::src_tokens::from_single_token(op)
				: lex::src_tokens{ op, op, type.src_tokens.end };
			type = context.make_unary_operator_expression(src_tokens, op->kind, std::move(type));
		}
		if (type.is_typename())
		{
			var_decl.get_type() = std::move(type.get_typename());
		}
		else
		{
			var_decl.clear_type();
			var_decl.state = ast::resolve_state::error;
		}
	}
	else
	{
		var_decl.clear_type();
		var_decl.state = ast::resolve_state::error;
	}
}

static void resolve_variable_init_expr_and_match_type(ast::decl_variable &var_decl, ctx::parse_context &context)
{
	if (context.in_generic_function())
	{
		return;
	}
	bz_assert(!var_decl.get_type().is_empty());
	if (var_decl.init_expr.not_null())
	{
		if (var_decl.init_expr.is<ast::unresolved_expression>())
		{
			auto const begin = var_decl.init_expr.src_tokens.begin;
			auto const end   = var_decl.init_expr.src_tokens.end;
			auto stream = begin;
			var_decl.init_expr = parse::parse_expression(stream, end, context, no_comma);
			if (stream != end)
			{
				if (stream->kind == lex::token::comma)
				{
					auto const suggestion_end = (end - 1)->kind == lex::token::semi_colon ? end - 1 : end;
					context.report_error(
						stream,
						"'operator ,' is not allowed in variable initialization expression",
						{}, { context.make_suggestion_around(
							begin,          ctx::char_pos(), ctx::char_pos(), "(",
							suggestion_end, ctx::char_pos(), ctx::char_pos(), ")",
							"put parenthesis around the initialization expression"
						) }
					);
				}
				else
				{
					context.assert_token(stream, lex::token::semi_colon);
				}
			}
		}
		context.match_expression_to_variable(var_decl.init_expr, var_decl);
	}
	else if (var_decl.init_expr.src_tokens.pivot != nullptr)
	{
		if (!ast::is_complete(var_decl.get_type()))
		{
			var_decl.clear_type();
		}
		var_decl.state = ast::resolve_state::error;
	}
	else
	{
		if (!ast::is_complete(var_decl.get_type()))
		{
			context.report_error(
				var_decl.src_tokens,
				bz::format(
					"a variable with an incomplete type '{}' must be initialized",
					var_decl.get_type()
				)
			);
			var_decl.clear_type();
			var_decl.state = ast::resolve_state::error;
		}
		else if (var_decl.get_type().is<ast::ts_const>())
		{
			context.report_error(
				var_decl.src_tokens,
				"a variable with a 'const' type must be initialized"
			);
			var_decl.state = ast::resolve_state::error;
		}
		else if (var_decl.get_type().is<ast::ts_consteval>())
		{
			context.report_error(
				var_decl.src_tokens,
				"a variable with a 'consteval' type must be initialized"
			);
			var_decl.state = ast::resolve_state::error;
		}
		else if (var_decl.get_type().is<ast::ts_base_type>())
		{
			auto const info = var_decl.get_type().get<ast::ts_base_type>().info;
			auto const def_ctor = info->default_constructor != nullptr
				? info->default_constructor
				: info->default_default_constructor.get();
			if (def_ctor != nullptr)
			{
				var_decl.init_expr = ast::make_dynamic_expression(
					var_decl.src_tokens,
					ast::expression_type_kind::rvalue,
					var_decl.get_type(),
					ast::make_expr_function_call(
						var_decl.src_tokens,
						bz::vector<ast::expression>{},
						def_ctor,
						ast::resolve_order::regular
					)
				);
				parse::consteval_guaranteed(var_decl.init_expr, context);
			}
		}
	}
	if (
		!var_decl.get_type().is_empty()
		&& !context.is_instantiable(var_decl.get_type())
		&& var_decl.state != ast::resolve_state::error
	)
	{
		auto const var_decl_src_tokens = var_decl.get_type().src_tokens;
		auto const src_tokens = [&]() {
			if (var_decl_src_tokens.pivot != nullptr)
			{
				return var_decl_src_tokens;
			}
			else if (var_decl.get_id().tokens.begin != nullptr)
			{
				return lex::src_tokens::from_range(var_decl.get_id().tokens);
			}
			else if (var_decl.init_expr.src_tokens.pivot != nullptr)
			{
				return var_decl.init_expr.src_tokens;
			}
			else
			{
				return var_decl.src_tokens;
			}
		}();
		bz_assert(src_tokens.pivot != nullptr);
		context.report_error(src_tokens, bz::format("variable type '{}' is not instantiable", var_decl.get_type()));
		var_decl.state = ast::resolve_state::error;
		var_decl.clear_type();
	}
}

static void resolve_variable_symbol_impl(ast::decl_variable &var_decl, ctx::parse_context &context)
{
	var_decl.state = ast::resolve_state::resolving_symbol;
	resolve_variable_type(var_decl, context);
	if (var_decl.state == ast::resolve_state::error)
	{
		return;
	}

	if (!ast::is_complete(var_decl.get_type()) || var_decl.get_type().is<ast::ts_consteval>())
	{
		var_decl.state = ast::resolve_state::resolving_all;
		resolve_variable_init_expr_and_match_type(var_decl, context);
		if (var_decl.state == ast::resolve_state::error)
		{
			return;
		}
		var_decl.state = ast::resolve_state::all;
	}
	else
	{
		var_decl.state = ast::resolve_state::symbol;
	}
}

void resolve_variable_symbol(ast::decl_variable &var_decl, ctx::parse_context &context)
{
	if (context.in_generic_function())
	{
		return;
	}

	if (var_decl.state >= ast::resolve_state::symbol || var_decl.state == ast::resolve_state::error)
	{
		return;
	}
	else if (var_decl.state == ast::resolve_state::resolving_symbol)
	{
		context.report_circular_dependency_error(var_decl);
		var_decl.state = ast::resolve_state::error;
		return;
	}

	auto const original_file_info = context.get_current_file_info();
	auto const stmt_file_id = var_decl.src_tokens.pivot->src_pos.file_id;
	if (original_file_info.file_id != stmt_file_id)
	{
		context.set_current_file(stmt_file_id);
	}
	resolve_variable_symbol_impl(var_decl, context);
	context.set_current_file_info(original_file_info);
}

static void resolve_variable_impl(ast::decl_variable &var_decl, ctx::parse_context &context)
{
	if (var_decl.state < ast::resolve_state::symbol)
	{
		var_decl.state = ast::resolve_state::resolving_symbol;
		resolve_variable_type(var_decl, context);
		if (var_decl.state == ast::resolve_state::error)
		{
			return;
		}
	}
	var_decl.state = ast::resolve_state::resolving_all;
	resolve_variable_init_expr_and_match_type(var_decl, context);
	if (var_decl.state == ast::resolve_state::error)
	{
		return;
	}
	var_decl.state = ast::resolve_state::all;
}

void resolve_variable(ast::decl_variable &var_decl, ctx::parse_context &context)
{
	if (context.in_generic_function())
	{
		return;
	}

	if (var_decl.state >= ast::resolve_state::all || var_decl.state == ast::resolve_state::error)
	{
		return;
	}
	else if (var_decl.state == ast::resolve_state::resolving_symbol || var_decl.state == ast::resolve_state::resolving_all)
	{
		context.report_circular_dependency_error(var_decl);
		var_decl.state = ast::resolve_state::error;
		return;
	}

	auto const original_file_info = context.get_current_file_info();
	auto const stmt_file_id = var_decl.src_tokens.pivot->src_pos.file_id;
	if (original_file_info.file_id != stmt_file_id)
	{
		context.set_current_file(stmt_file_id);
	}
	resolve_variable_impl(var_decl, context);
	context.set_current_file_info(original_file_info);
}

static bool resolve_function_parameters_helper(
	ast::statement_view func_stmt,
	ast::function_body &func_body,
	ctx::parse_context &context
)
{
	bz_assert(func_body.state == ast::resolve_state::resolving_parameters || func_body.state == ast::resolve_state::resolving_symbol);
	bool good = true;
	bool is_generic = false;
	for (auto &p : func_body.params)
	{
		if (p.state == ast::resolve_state::none)
		{
			p.state = ast::resolve_state::resolving_symbol;
			resolve_variable_type(p, context);
			p.state = ast::resolve_state::symbol;
		}
		if (p.get_type().is_empty())
		{
			good = false;
		}
		else if (ast::is_generic_parameter(p))
		{
			is_generic = true;
		}
	}
	if (is_generic && !func_body.is_generic_specialization())
	{
		func_body.flags |= ast::function_body::generic;
	}

	if (!func_body.params.empty())
	{
		for (auto const &param : func_body.params.slice(0, func_body.params.size() - 1))
		{
			if (param.get_type().is<ast::ts_variadic>())
			{
				context.report_error(
					param.src_tokens,
					bz::format("a parameter with variadic type '{}' must be the last parameter", param.get_type())
				);
				good = false;
			}
		}
		if (func_body.params.back().get_type().is<ast::ts_variadic>())
		{
			func_body.params.back().flags |= ast::decl_variable::variadic;
		}
	}

	if (
		good
		&& func_stmt.is<ast::decl_operator>()
		&& func_stmt.get<ast::decl_operator>().op->kind == lex::token::assign
	)
	{
		bz_assert(func_body.params.size() == 2);
		auto const lhs_t = func_body.params[0].get_type().as_typespec_view();
		auto const rhs_t = func_body.params[1].get_type().as_typespec_view();
		if (
			lhs_t.is<ast::ts_lvalue_reference>()
			&& lhs_t.get<ast::ts_lvalue_reference>().is<ast::ts_base_type>()
			&& ((
				rhs_t.is<ast::ts_lvalue_reference>()
				&& rhs_t.get<ast::ts_lvalue_reference>().is<ast::ts_const>()
				&& rhs_t.get<ast::ts_lvalue_reference>().get<ast::ts_const>() == lhs_t.get<ast::ts_lvalue_reference>()
			) || (
				ast::remove_const_or_consteval(rhs_t) == lhs_t.get<ast::ts_lvalue_reference>()
			))
		)
		{
			auto const info = lhs_t.get<ast::ts_lvalue_reference>().get<ast::ts_base_type>().info;
			if (rhs_t.is<ast::ts_lvalue_reference>())
			{
				info->op_assign = &func_body;
			}
			else
			{
				info->op_move_assign = &func_body;
			}
		}
	}
	else if (good && func_body.is_destructor())
	{
		if (func_body.params.size() != 1)
		{
			context.report_error(
				func_body.src_tokens,
				bz::format(
					"destructor of type '{}' must have one parameter",
					ast::type_info::decode_symbol_name(func_body.get_destructor_of()->symbol_name)
				)
			);
			return false;
		}

		if (func_body.is_generic())
		{
			func_body.flags &= ~ast::function_body::generic;

			auto const param_type = func_body.params[0].get_type().as_typespec_view();
			// if the parameter is generic, then it must be &auto
			// either as `destructor(&self)` or `destructor(self: &auto)`
			if (
				param_type.nodes.size() != 2
				|| !param_type.nodes[0].is<ast::ts_lvalue_reference>()
				|| !param_type.nodes[1].is<ast::ts_auto>()
			)
			{
				auto const destructor_of_type = ast::type_info::decode_symbol_name(func_body.get_destructor_of()->symbol_name);
				context.report_error(
					func_body.params[0].src_tokens,
					bz::format(
						"invalid parameter type '{}' in destructor of type '{}'",
						param_type, destructor_of_type
					),
					{ context.make_note(bz::format("it must be either '&auto' or '&{}'", destructor_of_type)) }
				);
				return false;
			}

			func_body.params[0].get_type().nodes[1] = ast::ts_base_type{ func_body.get_destructor_of() };
		}
		else
		{
			auto const param_type = func_body.params[0].get_type().as_typespec_view();
			// if the parameter is non-generic, then it must be &<type>
			if (
				param_type.nodes.size() != 2
				|| !param_type.nodes[0].is<ast::ts_lvalue_reference>()
				|| !param_type.nodes[1].is<ast::ts_base_type>()
				|| param_type.nodes[1].get<ast::ts_base_type>().info != func_body.get_destructor_of()
			)
			{
				auto const destructor_of_type = ast::type_info::decode_symbol_name(func_body.get_destructor_of()->symbol_name);
				context.report_error(
					func_body.params[0].src_tokens,
					bz::format(
						"invalid parameter type '{}' in destructor of type '{}'",
						param_type, destructor_of_type
					),
					{ context.make_note(bz::format("it must be either '&auto' or '&{}'", destructor_of_type)) }
				);
				return false;
			}
		}
	}
	else if (good && func_body.is_constructor())
	{
		if (func_body.params.empty())
		{
			func_body.get_constructor_of()->default_constructor = &func_body;
		}
		else if (
			func_body.params.size() == 1
			&& func_body.params[0].get_type().nodes.size() == 3
			&& func_body.params[0].get_type().nodes[0].is<ast::ts_lvalue_reference>()
			&& func_body.params[0].get_type().nodes[1].is<ast::ts_const>()
			&& func_body.params[0].get_type().nodes[2].is<ast::ts_base_type>()
			&& func_body.params[0].get_type().nodes[2].get<ast::ts_base_type>().info == func_body.get_constructor_of()
		)
		{
			func_body.get_constructor_of()->copy_constructor = &func_body;
		}
	}
	return good;
}

static void resolve_function_parameters_impl(
	ast::statement_view func_stmt,
	ast::function_body &func_body,
	ctx::parse_context &context
)
{
	func_body.state = ast::resolve_state::resolving_parameters;
	if (resolve_function_parameters_helper(func_stmt, func_body, context))
	{
		func_body.state = ast::resolve_state::parameters;
	}
	else
	{
		func_body.state = ast::resolve_state::error;
	}
}

void resolve_function_parameters(ast::statement_view func_stmt, ast::function_body &func_body, ctx::parse_context &context)
{
	bz_assert(!context.in_generic_function());
	if (func_body.state >= ast::resolve_state::parameters || func_body.state == ast::resolve_state::error)
	{
		return;
	}
	else if (func_body.state == ast::resolve_state::resolving_parameters || func_body.state == ast::resolve_state::resolving_symbol)
	{
		context.report_circular_dependency_error(func_body);
		func_body.state = ast::resolve_state::error;
		return;
	}

	bz::optional<ctx::parse_context> new_context{};
	auto context_ptr = [&]() {
		if (func_body.is_local())
		{
			auto const var_count = context.scope_decls
				.transform([](auto const &decl_set) { return decl_set.var_decls.size(); })
				.sum();
			if (var_count == 0)
			{
				return &context;
			}
			else
			{
				new_context.emplace(context, ctx::parse_context::local_copy_t{});
				for (auto &decl_set : new_context->scope_decls)
				{
					decl_set.var_decls.clear();
				}
				return &new_context.get();
			}
		}
		else
		{
			if (context.scope_decls.empty())
			{
				return &context;
			}
			else
			{
				new_context.emplace(context, ctx::parse_context::global_copy_t{});
				for (auto &decl_set : new_context->scope_decls)
				{
					decl_set.var_decls.clear();
				}
				return &new_context.get();
			}
		}
	}();

	auto const original_file_info = context_ptr->get_current_file_info();
	auto const stmt_file_id = func_body.src_tokens.pivot->src_pos.file_id;
	if (original_file_info.file_id != stmt_file_id)
	{
		context_ptr->set_current_file(stmt_file_id);
	}
	resolve_function_parameters_impl(func_stmt, func_body, *context_ptr);
	context_ptr->set_current_file_info(original_file_info);
}

static void add_parameters_as_local_variables(ast::function_body &func_body, ctx::parse_context &context)
{
	auto       params_it  = func_body.params.begin();
	auto const params_end = func_body.params.end();
	for (; params_it != params_end; ++params_it)
	{
		if (params_it->is_variadic())
		{
			break;
		}
		context.add_local_variable(*params_it);
	}
	if (
		func_body.generic_parent != nullptr
		&& !func_body.generic_parent->params.empty()
		&& func_body.generic_parent->params.back().get_type().is<ast::ts_variadic>()
	)
	{
		auto variadic_params = bz::basic_range{ params_it, params_end }.transform([](auto &p) { return &p; }).collect();
		context.add_local_variable(func_body.generic_parent->params.back(), std::move(variadic_params));
	}
}

static void resolve_typespec(
	ast::typespec &ts,
	ctx::parse_context &context,
	precedence prec
)
{
	if (!ts.is<ast::ts_unresolved>())
	{
		return;
	}
	auto [stream, end] = ts.get<ast::ts_unresolved>().tokens;
	auto type = parse::parse_expression(stream, end, context, prec);
	if (stream != end)
	{
		context.report_error({ stream, stream, end });
	}

	parse::consteval_try(type, context);
	if (type.not_error() && !type.has_consteval_succeeded())
	{
		auto notes = parse::get_consteval_fail_notes(type);
		notes.push_front(context.make_note(type.src_tokens, "type must be a constant expression"));
		context.report_error(
			type.src_tokens,
			"expected a type",
			std::move(notes)
		);
		ts.clear();
	}
	else if (type.not_error() && !type.is_typename())
	{
		context.report_error(type, "expected a type");
		ts.clear();
	}
	else if (type.is_typename())
	{
		ts = std::move(type.get_typename());
	}
	else
	{
		ts.clear();
	}
}

static bool resolve_function_return_type_helper(ast::function_body &func_body, ctx::parse_context &context)
{
	bz_assert(func_body.state == ast::resolve_state::resolving_symbol);
	add_parameters_as_local_variables(func_body, context);

	resolve_typespec(func_body.return_type, context, precedence{});
	bz_assert(!func_body.return_type.is<ast::ts_unresolved>());
	if (func_body.is_destructor())
	{
		if (!func_body.return_type.is_empty() && !func_body.return_type.is<ast::ts_void>())
		{
			auto const destructor_of_type = ast::type_info::decode_symbol_name(func_body.get_destructor_of()->symbol_name);
			context.report_error(
				func_body.return_type.src_tokens,
				bz::format("return type must be 'void' for destructor of type '{}'", destructor_of_type)
			);
			return false;
		}
		return !func_body.return_type.is_empty();
	}
	else if (func_body.is_constructor())
	{
		func_body.return_type = ast::make_base_type_typespec({}, func_body.get_constructor_of());
		return true;
	}
	else
	{
		return !func_body.return_type.is_empty();
	}
}

static bool is_valid_main(ast::function_body const &body)
{
	if (body.is_generic())
	{
		return false;
	}

	if (!(
		body.return_type.is<ast::ts_void>()
		|| (
			body.return_type.is<ast::ts_base_type>()
			&& body.return_type.get<ast::ts_base_type>().info->kind == ast::type_info::int32_
		)
	))
	{
		return false;
	}

	if (body.params.size() == 0)
	{
		return true;
	}
	else if (body.params.size() > 1)
	{
		return false;
	}

	for (auto const &param : body.params)
	{
		auto const param_t = ast::remove_const_or_consteval(param.get_type());
		if (!param_t.is<ast::ts_array_slice>())
		{
			return false;
		}
		auto const slice_t = ast::remove_const_or_consteval(param_t.get<ast::ts_array_slice>().elem_type);
		if (!(
			slice_t.is<ast::ts_void>()
			|| (
				slice_t.is<ast::ts_base_type>()
				&& slice_t.get<ast::ts_base_type>().info->kind == ast::type_info::str_
			)
		))
		{
			return false;
		}
	}
	return true;
}

static void report_invalid_main_error(ast::function_body const &body, ctx::parse_context &context)
{
	if (body.is_generic())
	{
		context.report_error(
			body.src_tokens, "invalid declaration for main function",
			{ context.make_note(body.src_tokens, "main function can't be generic") }
		);
		return;
	}

	if (!(
		body.return_type.is<ast::ts_void>()
		|| (
			body.return_type.is<ast::ts_base_type>()
			&& body.return_type.get<ast::ts_base_type>().info->kind == ast::type_info::int32_
		)
	))
	{
		auto const ret_t_src_tokens = body.return_type.src_tokens;
		bz_assert(ret_t_src_tokens.pivot != nullptr);
		context.report_error(
			body.src_tokens, "invalid declaration for main function",
			{ context.make_note(ret_t_src_tokens, "main function's return type must be 'void' or 'int32'") }
		);
		return;
	}

	if (body.params.size() == 0)
	{
		bz_unreachable;
	}
	else if (body.params.size() > 1)
	{
		context.report_error(
			body.src_tokens, "invalid declaration for main function",
			{ context.make_note(body.src_tokens, "main function must have at most one parameter") }
		);
		return;
	}

	for (auto const &param : body.params)
	{
		auto const param_t = ast::remove_const_or_consteval(param.get_type());
		if (!param_t.is<ast::ts_array_slice>())
		{
			context.report_error(
				body.src_tokens, "invalid declaration for main function",
				{ context.make_note(param.src_tokens, "parameter type must be '[: const str]'") }
			);
			return;
		}
		auto const slice_t = ast::remove_const_or_consteval(param_t.get<ast::ts_array_slice>().elem_type);
		if (!(
			slice_t.is<ast::ts_void>()
			|| (
				slice_t.is<ast::ts_base_type>()
				&& slice_t.get<ast::ts_base_type>().info->kind == ast::type_info::str_
			)
		))
		{
			context.report_error(
				body.src_tokens, "invalid declaration for main function",
				{ context.make_note(param.src_tokens, "parameter type must be '[: const str]'") }
			);
			return;
		}
	}
	bz_unreachable;
}

static void add_used_flag(ast::decl_variable &decl)
{
	if (decl.tuple_decls.empty())
	{
		decl.flags |= ast::decl_variable::used;
	}
	else
	{
		for (auto &tuple_decl : decl.tuple_decls)
		{
			add_used_flag(tuple_decl);
		}
	}
}

// resolves the function symbol, but doesn't modify scope
static bool resolve_function_symbol_helper(ast::statement_view func_stmt, ast::function_body &func_body, ctx::parse_context &context)
{
	bz_assert(func_body.state == ast::resolve_state::resolving_symbol);
	auto const parameters_good = resolve_function_parameters_helper(func_stmt, func_body, context);
	if (func_body.is_generic())
	{
		return parameters_good;
	}
	auto const return_type_good = resolve_function_return_type_helper(func_body, context);
	for (auto &p : func_body.params)
	{
		add_used_flag(p);
	}
	auto const good = parameters_good && return_type_good;
	if (!good)
	{
		bz_assert(context.has_errors());
		return false;
	}

	if (func_body.is_main() && !is_valid_main(func_body))
	{
		report_invalid_main_error(func_body, context);
	}
	func_body.resolve_symbol_name();
	context.add_function_for_compilation(func_body);
	return true;
}

static void resolve_function_symbol_impl(ast::statement_view func_stmt, ast::function_body &func_body, ctx::parse_context &context)
{
	func_body.state = ast::resolve_state::resolving_symbol;
	context.add_scope();
	if (resolve_function_symbol_helper(func_stmt, func_body, context))
	{
		func_body.state = func_body.is_generic() ? ast::resolve_state::parameters : ast::resolve_state::symbol;
	}
	else
	{
		func_body.state = ast::resolve_state::error;
	}
	context.remove_scope();
}

void resolve_function_symbol(ast::statement_view func_stmt, ast::function_body &func_body, ctx::parse_context &context)
{
	bz_assert(!context.in_generic_function());
	if (func_body.state >= ast::resolve_state::symbol || func_body.state == ast::resolve_state::error)
	{
		return;
	}
	else if (func_body.state == ast::resolve_state::resolving_parameters || func_body.state == ast::resolve_state::resolving_symbol)
	{
		context.report_circular_dependency_error(func_body);
		func_body.state = ast::resolve_state::error;
		return;
	}

	bz::optional<ctx::parse_context> new_context{};
	auto context_ptr = [&]() {
		if (func_body.is_local())
		{
			auto const var_count = context.scope_decls
				.transform([](auto const &decl_set) { return decl_set.var_decls.size(); })
				.sum();
			if (var_count == 0)
			{
				return &context;
			}
			else
			{
				new_context.emplace(context, ctx::parse_context::local_copy_t{});
				for (auto &decl_set : new_context->scope_decls)
				{
					decl_set.var_decls.clear();
				}
				return &new_context.get();
			}
		}
		else
		{
			if (context.scope_decls.empty())
			{
				return &context;
			}
			else
			{
				new_context.emplace(context, ctx::parse_context::global_copy_t{});
				for (auto &decl_set : new_context->scope_decls)
				{
					decl_set.var_decls.clear();
				}
				return &new_context.get();
			}
		}
	}();

	auto const original_file_info = context_ptr->get_current_file_info();
	auto const stmt_file_id = func_body.src_tokens.pivot->src_pos.file_id;
	if (original_file_info.file_id != stmt_file_id)
	{
		context_ptr->set_current_file(stmt_file_id);
	}
	resolve_function_symbol_impl(func_stmt, func_body, *context_ptr);
	context_ptr->set_current_file_info(original_file_info);
}

static void resolve_function_impl(ast::statement_view func_stmt, ast::function_body &func_body, ctx::parse_context &context)
{
	if (func_body.state <= ast::resolve_state::parameters)
	{
		func_body.state = ast::resolve_state::resolving_symbol;
		context.add_scope();
		if (!resolve_function_symbol_helper(func_stmt, func_body, context))
		{
			func_body.state = ast::resolve_state::error;
			context.remove_scope();
			return;
		}
		else if (func_body.is_generic())
		{
			func_body.state = ast::resolve_state::parameters;
			context.remove_scope();
		}
		else
		{
			func_body.state = ast::resolve_state::symbol;
			context.remove_scope();
		}
	}

	if (func_body.body.is_null())
	{
		return;
	}

	auto const prev_function = context.current_function;
	context.current_function = &func_body;
	context.add_scope();
	add_parameters_as_local_variables(func_body, context);

	func_body.state = ast::resolve_state::resolving_all;

	bz_assert(func_body.body.is<lex::token_range>());
	auto [stream, end] = func_body.body.get<lex::token_range>();
	func_body.body = parse::parse_local_statements(stream, end, context);
	func_body.state = ast::resolve_state::all;

	context.remove_scope();
	context.current_function = prev_function;
}

void resolve_function(ast::statement_view func_stmt, ast::function_body &func_body, ctx::parse_context &context)
{
	bz_assert(!context.in_generic_function());
	if (func_body.state >= ast::resolve_state::all || func_body.state == ast::resolve_state::error || context.in_generic_function())
	{
		return;
	}
	else if (
		func_body.state == ast::resolve_state::resolving_parameters
		|| func_body.state == ast::resolve_state::resolving_symbol
		|| func_body.state == ast::resolve_state::resolving_all
	)
	{
		context.report_circular_dependency_error(func_body);
		return;
	}

	bz::optional<ctx::parse_context> new_context{};
	auto context_ptr = [&]() {
		if (func_body.is_local())
		{
			auto const var_count = context.scope_decls
				.transform([](auto const &decl_set) { return decl_set.var_decls.size(); })
				.sum();
			if (var_count == 0)
			{
				return &context;
			}
			else
			{
				new_context.emplace(context, ctx::parse_context::local_copy_t{});
				for (auto &decl_set : new_context->scope_decls)
				{
					decl_set.var_decls.clear();
				}
				return &new_context.get();
			}
		}
		else
		{
			if (context.scope_decls.empty())
			{
				return &context;
			}
			else
			{
				new_context.emplace(context, ctx::parse_context::global_copy_t{});
				for (auto &decl_set : new_context->scope_decls)
				{
					decl_set.var_decls.clear();
				}
				return &new_context.get();
			}
		}
	}();

	auto const original_file_info = context_ptr->get_current_file_info();
	// this check is needed because of generic built-in functions like __builtin_slice_size
	if (func_body.src_tokens.pivot != nullptr)
	{
		auto const stmt_file_id = func_body.src_tokens.pivot->src_pos.file_id;
		if (original_file_info.file_id != stmt_file_id)
		{
			context_ptr->set_current_file(stmt_file_id);
		}
	}
	resolve_function_impl(func_stmt, func_body, *context_ptr);
	context_ptr->set_current_file_info(original_file_info);
}

static void resolve_function_alias_impl(ast::decl_function_alias &alias_decl, ctx::parse_context &context)
{
	auto const begin = alias_decl.alias_expr.src_tokens.begin;
	auto const end   = alias_decl.alias_expr.src_tokens.end;
	auto stream = begin;
	alias_decl.state = ast::resolve_state::resolving_all;
	alias_decl.alias_expr = parse::parse_expression(stream, end, context, no_comma);
	if (stream != end)
	{
		if (stream->kind == lex::token::comma)
		{
			auto const suggestion_end = (end - 1)->kind == lex::token::semi_colon ? end - 1 : end;
			context.report_error(
				stream,
				"'operator ,' is not allowed in function alias expression",
				{}, { context.make_suggestion_around(
					begin,          ctx::char_pos(), ctx::char_pos(), "(",
					suggestion_end, ctx::char_pos(), ctx::char_pos(), ")",
					"put parenthesis around the expression"
				) }
			);
		}
		else
		{
			context.assert_token(stream, lex::token::semi_colon);
		}
	}
	parse::consteval_try(alias_decl.alias_expr, context);

	if (!alias_decl.alias_expr.has_consteval_succeeded())
	{
		context.report_error(
			alias_decl.alias_expr,
			"function alias expression must be a constant expression",
			parse::get_consteval_fail_notes(alias_decl.alias_expr)
		);
		alias_decl.state = ast::resolve_state::error;
		return;
	}

	auto const &value = alias_decl.alias_expr.get<ast::constant_expression>().value;
	if (value.is<ast::constant_value::function>())
	{
		auto const func_body = value.get<ast::constant_value::function>();
		bz_assert(alias_decl.aliased_bodies.empty());
		alias_decl.aliased_bodies = { func_body };
		alias_decl.state = ast::resolve_state::all;
	}
	else if (value.is<ast::constant_value::unqualified_function_set_id>() || value.is<ast::constant_value::qualified_function_set_id>())
	{
		auto const func_set_id = value.is<ast::constant_value::unqualified_function_set_id>()
			? value.get<ast::constant_value::unqualified_function_set_id>().as_array_view()
			: value.get<ast::constant_value::qualified_function_set_id>().as_array_view();
		bz_assert(alias_decl.aliased_bodies.empty());
		alias_decl.aliased_bodies = value.is<ast::constant_value::unqualified_function_set_id>()
			? context.get_function_bodies_from_unqualified_id(alias_decl.alias_expr.src_tokens, func_set_id)
			: context.get_function_bodies_from_qualified_id(alias_decl.alias_expr.src_tokens, func_set_id);
		if (alias_decl.state != ast::resolve_state::error && !alias_decl.aliased_bodies.empty())
		{
			alias_decl.state = ast::resolve_state::all;
		}
		else
		{
			alias_decl.state = ast::resolve_state::error;
		}
	}
	else
	{
		context.report_error(alias_decl.alias_expr, "function alias value must be a function");
		alias_decl.state = ast::resolve_state::error;
		return;
	}
}

void resolve_function_alias(ast::decl_function_alias &alias_decl, ctx::parse_context &context)
{
	if (alias_decl.state >= ast::resolve_state::all || alias_decl.state == ast::resolve_state::error)
	{
		return;
	}
	else if (alias_decl.state != ast::resolve_state::none)
	{
		bz_assert(alias_decl.state == ast::resolve_state::resolving_all);
		context.report_circular_dependency_error(alias_decl);
		alias_decl.state = ast::resolve_state::error;
		return;
	}

	auto const original_file_info = context.get_current_file_info();
	auto const stmt_file_id = alias_decl.id.tokens.begin->src_pos.file_id;
	if (original_file_info.file_id != stmt_file_id)
	{
		context.set_current_file(stmt_file_id);
	}
	resolve_function_alias_impl(alias_decl, context);
	context.set_current_file_info(original_file_info);
}

static void resolve_type_alias_impl(ast::decl_type_alias &alias_decl, ctx::parse_context &context)
{
	alias_decl.state = ast::resolve_state::resolving_all;

	bz_assert(alias_decl.alias_expr.is<ast::unresolved_expression>());
	auto const begin = alias_decl.alias_expr.src_tokens.begin;
	auto const end   = alias_decl.alias_expr.src_tokens.end;
	auto stream = begin;
	alias_decl.state = ast::resolve_state::resolving_all;
	alias_decl.alias_expr = parse::parse_expression(stream, end, context, no_comma);
	if (stream != end)
	{
		if (stream->kind == lex::token::comma)
		{
			auto const suggestion_end = (end - 1)->kind == lex::token::semi_colon ? end - 1 : end;
			context.report_error(
				stream,
				"'operator ,' is not allowed in type alias expression",
				{}, { context.make_suggestion_around(
					begin,          ctx::char_pos(), ctx::char_pos(), "(",
					suggestion_end, ctx::char_pos(), ctx::char_pos(), ")",
					"put parenthesis around the expression"
				) }
			);
		}
		else
		{
			context.assert_token(stream, lex::token::semi_colon);
		}
	}
	else if (alias_decl.alias_expr.is_error())
	{
		alias_decl.state = ast::resolve_state::error;
		return;
	}
	parse::consteval_try(alias_decl.alias_expr, context);

	if (!alias_decl.alias_expr.has_consteval_succeeded())
	{
		context.report_error(
			alias_decl.alias_expr,
			"type alias expression must be a constant expression",
			parse::get_consteval_fail_notes(alias_decl.alias_expr)
		);
		alias_decl.state = ast::resolve_state::error;
		return;
	}

	auto const &value = alias_decl.alias_expr.get<ast::constant_expression>().value;
	if (value.is<ast::constant_value::type>())
	{
		auto const &type = value.get<ast::constant_value::type>();
		if (ast::is_complete(type))
		{
			alias_decl.state = ast::resolve_state::all;
		}
		else
		{
			context.report_error(alias_decl.alias_expr, bz::format("type alias of non-complete type '{}' is not allowed", type));
			alias_decl.state = ast::resolve_state::error;
		}
	}
	else
	{
		context.report_error(alias_decl.alias_expr, "type alias value must be a type");
		alias_decl.state = ast::resolve_state::error;
		return;
	}
}

void resolve_type_alias(ast::decl_type_alias &alias_decl, ctx::parse_context &context)
{
	if (alias_decl.state >= ast::resolve_state::all || alias_decl.state == ast::resolve_state::error)
	{
		return;
	}
	else if (alias_decl.state == ast::resolve_state::resolving_all)
	{
		context.report_circular_dependency_error(alias_decl);
		alias_decl.state = ast::resolve_state::error;
		return;
	}

	auto const original_file_info = context.get_current_file_info();
	auto const stmt_file_id = alias_decl.id.tokens.begin->src_pos.file_id;
	if (original_file_info.file_id != stmt_file_id)
	{
		context.set_current_file(stmt_file_id);
	}
	if (!context.in_generic_function())
	{
		resolve_type_alias_impl(alias_decl, context);
	}
	context.set_current_file_info(original_file_info);
}

static void add_type_info_members(ast::type_info &info, ctx::parse_context &context)
{
	auto &info_body = info.body.get<bz::vector<ast::statement>>();
	for (auto &stmt : info_body)
	{
		if (stmt.is<ast::decl_function>())
		{
			auto &body = stmt.get<ast::decl_function>().body;
			if (body.is_destructor())
			{
				if (info.destructor != nullptr)
				{
					auto const type_name = ast::type_info::decode_symbol_name(info.symbol_name);
					context.report_error(
						body.src_tokens, bz::format("redefinition of destructor for type '{}'", type_name),
						{ context.make_note(info.destructor->src_tokens, "previously defined here") }
					);
				}

				body.constructor_or_destructor_of = &info;
				info.destructor = &body;
			}
			else if (body.is_constructor())
			{
				if (body.return_type.is<ast::ts_unresolved>())
				{
					auto const tokens = body.return_type.get<ast::ts_unresolved>().tokens;
					auto const constructor_of_type = ast::type_info::decode_symbol_name(info.symbol_name);
					context.report_error(
						{ tokens.begin, tokens.begin, tokens.end },
						"a return type cannot be provided for a constructor",
						{ context.make_note(
							body.src_tokens,
							bz::format("in constructor for type '{}'", constructor_of_type)
						) }
					);
				}

				body.constructor_or_destructor_of = &info;
				info.constructors.push_back(&body);
			}
		}
		else if (stmt.is<ast::decl_variable>())
		{
			auto &var_decl = stmt.get<ast::decl_variable>();
			if (var_decl.is_member())
			{
				info.member_variables.push_back(&var_decl);
			}
		}
	}

	for (auto const member : info.member_variables)
	{
		resolve_variable(*member, context);
	}
}

static void resolve_type_info_symbol_impl(
	ast::type_info &info,
	[[maybe_unused]] ctx::parse_context &context
)
{
	if (info.type_name.is_qualified)
	{
		info.symbol_name = bz::format("struct.{}", info.type_name.format_as_unqualified());
	}
	else
	{
		info.symbol_name = bz::format("non_global_struct.{}", info.type_name.format_as_unqualified());
	}
	info.state = ast::resolve_state::symbol;
}

void resolve_type_info_symbol(
	ast::type_info &info,
	ctx::parse_context &context
)
{
	if (info.state >= ast::resolve_state::symbol || info.state == ast::resolve_state::error)
	{
		return;
	}
	bz_assert(info.state != ast::resolve_state::resolving_symbol);

	auto const original_file_info = context.get_current_file_info();
	auto const stmt_file_id = info.src_tokens.pivot->src_pos.file_id;
	if (original_file_info.file_id != stmt_file_id)
	{
		context.set_current_file(stmt_file_id);
	}
	resolve_type_info_symbol_impl(info, context);
	context.set_current_file_info(original_file_info);
}

static void resolve_type_info_impl(ast::type_info &info, ctx::parse_context &context)
{
	if (info.state < ast::resolve_state::symbol)
	{
		resolve_type_info_symbol_impl(info, context);
	}
	if (info.state == ast::resolve_state::error || info.kind == ast::type_info::forward_declaration)
	{
		return;
	}

	info.state = ast::resolve_state::resolving_all;
	bz_assert(info.body.is<lex::token_range>());
	auto [stream, end] = info.body.get<lex::token_range>();

	info.body.emplace<bz::vector<ast::statement>>();
	auto &info_body = info.body.get<bz::vector<ast::statement>>();
	info_body = parse::parse_struct_body_statements(stream, end, context);

	add_type_info_members(info, context);

	if (info.state == ast::resolve_state::error)
	{
		return;
	}
	else
	{
		info.state = ast::resolve_state::all;
	}

	for (auto &stmt : info_body)
	{
		resolve_statement(stmt, context);
	}
}

void resolve_type_info(ast::type_info &info, ctx::parse_context &context)
{
	if (info.state >= ast::resolve_state::all || info.state == ast::resolve_state::error)
	{
		return;
	}
	else if (info.state == ast::resolve_state::resolving_all)
	{
		context.report_circular_dependency_error(info);
		info.state = ast::resolve_state::error;
		return;
	}

	auto const original_file_info = context.get_current_file_info();
	auto const stmt_file_id = info.src_tokens.pivot->src_pos.file_id;
	if (original_file_info.file_id != stmt_file_id)
	{
		context.set_current_file(stmt_file_id);
	}
	resolve_type_info_impl(info, context);
	context.set_current_file_info(original_file_info);
}

static void resolve_stmt(ast::decl_import &, ctx::parse_context &)
{
	// nothing
}

void resolve_statement(ast::statement &stmt, ctx::parse_context &context)
{
	if (stmt.not_null())
	{
		stmt.visit(bz::overload{
			[&context](auto &inner_stmt) {
				resolve_stmt(inner_stmt, context);
			},
			[&context](ast::decl_variable &var_decl) {
				resolve_variable(var_decl, context);
			},
			[&stmt, &context](ast::decl_function &func_decl) {
				resolve_function(stmt, func_decl.body, context);
			},
			[&stmt, &context](ast::decl_operator &op_decl) {
				resolve_function(stmt, op_decl.body, context);
			},
			[&context](ast::decl_function_alias &func_alias_decl) {
				resolve_function_alias(func_alias_decl, context);
			},
			[&context](ast::decl_type_alias &type_alias_decl) {
				resolve_type_alias(type_alias_decl, context);
			},
			[&context](ast::decl_struct &struct_decl) {
				resolve_type_info(struct_decl.info, context);
			},
		});
	}
}

} // namespace resolve
