#include "statement_resolver.h"
#include "expression_resolver.h"
#include "parse/statement_parser.h"
#include "parse/expression_parser.h"
#include "parse/consteval.h"
#include "ctx/global_context.h"

namespace resolve
{

static void resolve_stmt(ast::stmt_while &while_stmt, ctx::parse_context &context)
{
	resolve_expression(while_stmt.condition, context);
	resolve_expression(while_stmt.while_block, context);

	auto bool_type = ast::make_base_type_typespec({}, context.get_builtin_type_info(ast::type_info::bool_));
	context.match_expression_to_type(while_stmt.condition, bool_type);
}

static void resolve_stmt(ast::stmt_for &for_stmt, ctx::parse_context &context)
{
	resolve_statement(for_stmt.init, context);
	resolve_expression(for_stmt.condition, context);
	resolve_expression(for_stmt.iteration, context);
	resolve_expression(for_stmt.for_block, context);

	auto bool_type = ast::make_base_type_typespec({}, context.get_builtin_type_info(ast::type_info::bool_));
	if (for_stmt.condition.not_null())
	{
		context.match_expression_to_type(for_stmt.condition, bool_type);
	}
}

static void resolve_stmt(ast::stmt_foreach &foreach_stmt, ctx::parse_context &context)
{
	if (foreach_stmt.iter_var_decl.not_null())
	{
		// already resolved
		return;
	}
	resolve_statement(foreach_stmt.range_var_decl, context);
	bz_assert(foreach_stmt.range_var_decl.is<ast::decl_variable>());
	auto &range_var_decl = foreach_stmt.range_var_decl.get<ast::decl_variable>();
	range_var_decl.flags |= ast::decl_variable::used;
	if (range_var_decl.get_type().is_empty())
	{
		return;
	}

	auto const &range_expr_src_tokens = range_var_decl.init_expr.src_tokens;

	auto range_begin_expr = [&]() {
		if (range_var_decl.get_type().is_empty())
		{
			return ast::make_error_expression(range_expr_src_tokens);
		}
		auto const type_kind = range_var_decl.get_type().is<ast::ts_lvalue_reference>()
			? ast::expression_type_kind::lvalue_reference
			: ast::expression_type_kind::lvalue;
		auto const type = ast::remove_lvalue_reference(range_var_decl.get_type());

		auto range_var_expr = ast::make_dynamic_expression(
			range_expr_src_tokens,
			type_kind, type,
			ast::make_expr_identifier(ast::identifier{}, &range_var_decl)
		);
		return context.make_universal_function_call_expression(
			range_expr_src_tokens,
			std::move(range_var_expr),
			ast::make_identifier("begin"), {}
		);
	}();
	foreach_stmt.iter_var_decl = ast::make_decl_variable(
		range_expr_src_tokens,
		lex::token_range{},
		ast::var_id_and_type(ast::identifier{}, ast::type_as_expression(ast::make_auto_typespec(nullptr))),
		std::move(range_begin_expr)
	);
	bz_assert(foreach_stmt.iter_var_decl.is<ast::decl_variable>());
	auto &iter_var_decl = foreach_stmt.iter_var_decl.get<ast::decl_variable>();
	iter_var_decl.id_and_type.id.tokens = { range_expr_src_tokens.begin, range_expr_src_tokens.end };
	iter_var_decl.id_and_type.id.values = { "" };
	iter_var_decl.id_and_type.id.is_qualified = false;
	resolve_statement(foreach_stmt.iter_var_decl, context);
	context.add_local_variable(iter_var_decl);
	iter_var_decl.flags |= ast::decl_variable::used;

	auto range_end_expr = [&]() {
		if (range_var_decl.get_type().is_empty())
		{
			return ast::make_error_expression(range_expr_src_tokens);
		}
		auto const type_kind = range_var_decl.get_type().is<ast::ts_lvalue_reference>()
			? ast::expression_type_kind::lvalue_reference
			: ast::expression_type_kind::lvalue;
		auto const type = ast::remove_lvalue_reference(range_var_decl.get_type());

		auto range_var_expr = ast::make_dynamic_expression(
			range_expr_src_tokens,
			type_kind, type,
			ast::make_expr_identifier(ast::identifier{}, &range_var_decl)
		);
		return context.make_universal_function_call_expression(
			range_expr_src_tokens,
			std::move(range_var_expr),
			ast::make_identifier("end"), {}
		);
	}();

	foreach_stmt.end_var_decl = ast::make_decl_variable(
		range_expr_src_tokens,
		lex::token_range{},
		ast::var_id_and_type(ast::identifier{}, ast::type_as_expression(ast::make_auto_typespec(nullptr))),
		std::move(range_end_expr)
	);
	bz_assert(foreach_stmt.end_var_decl.is<ast::decl_variable>());
	auto &end_var_decl = foreach_stmt.end_var_decl.get<ast::decl_variable>();
	end_var_decl.id_and_type.id.tokens = { range_expr_src_tokens.begin, range_expr_src_tokens.end };
	end_var_decl.id_and_type.id.values = { "" };
	end_var_decl.id_and_type.id.is_qualified = false;
	resolve_statement(foreach_stmt.end_var_decl, context);
	context.add_local_variable(end_var_decl);
	end_var_decl.flags |= ast::decl_variable::used;

	foreach_stmt.condition = [&]() {
		if (iter_var_decl.get_type().is_empty() || end_var_decl.get_type().is_empty())
		{
			return ast::make_error_expression(range_expr_src_tokens);
		}
		auto iter_var_expr = ast::make_dynamic_expression(
			range_expr_src_tokens,
			ast::expression_type_kind::lvalue, iter_var_decl.get_type(),
			ast::make_expr_identifier(ast::identifier{}, &iter_var_decl)
		);
		auto end_var_expr = ast::make_dynamic_expression(
			range_expr_src_tokens,
			ast::expression_type_kind::lvalue, end_var_decl.get_type(),
			ast::make_expr_identifier(ast::identifier{}, &end_var_decl)
		);
		return context.make_binary_operator_expression(
			range_expr_src_tokens,
			lex::token::not_equals,
			std::move(iter_var_expr),
			std::move(end_var_expr)
		);
	}();

	{
		auto bool_type = ast::make_base_type_typespec({}, context.get_builtin_type_info(ast::type_info::bool_));
		context.match_expression_to_type(foreach_stmt.condition, bool_type);
	}

	foreach_stmt.iteration = [&]() {
		if (iter_var_decl.get_type().is_empty())
		{
			return ast::make_error_expression(range_expr_src_tokens);
		}
		auto iter_var_expr = ast::make_dynamic_expression(
			range_expr_src_tokens,
			ast::expression_type_kind::lvalue, iter_var_decl.get_type(),
			ast::make_expr_identifier(ast::identifier{}, &iter_var_decl)
		);
		return context.make_unary_operator_expression(
			range_expr_src_tokens,
			lex::token::plus_plus,
			std::move(iter_var_expr)
		);
	}();

	bz_assert(foreach_stmt.iter_deref_var_decl.is<ast::decl_variable>());
	auto &iter_deref_var_decl = foreach_stmt.iter_deref_var_decl.get<ast::decl_variable>();
	bz_assert(iter_deref_var_decl.init_expr.is_null());
	iter_deref_var_decl.init_expr = [&]() {
		if (iter_var_decl.get_type().is_empty())
		{
			return ast::make_error_expression(range_expr_src_tokens);
		}
		auto iter_var_expr = ast::make_dynamic_expression(
			range_expr_src_tokens,
			ast::expression_type_kind::lvalue, iter_var_decl.get_type(),
			ast::make_expr_identifier(ast::identifier{}, &iter_var_decl)
		);
		return context.make_unary_operator_expression(
			range_expr_src_tokens,
			lex::token::dereference,
			std::move(iter_var_expr)
		);
	}();
	resolve_statement(foreach_stmt.iter_deref_var_decl, context);
	context.add_local_variable(iter_deref_var_decl);

	resolve_expression(foreach_stmt.for_block, context);
}

static void resolve_stmt(ast::stmt_return &return_stmt, ctx::parse_context &context)
{
	if (context.current_function == nullptr)
	{
		context.report_error(return_stmt.return_pos, "a return statement can only appear inside of a function");
	}
	else if (return_stmt.expr.is_null())
	{
		if (!context.current_function->return_type.is<ast::ts_void>())
		{
			context.report_error(return_stmt.return_pos, "a function with a non-void return type must return a value");
		}
	}
	else
	{
		resolve_expression(return_stmt.expr, context);
		bz_assert(context.current_function != nullptr);
		bz_assert(ast::is_complete(context.current_function->return_type));
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
	bz_assert(static_assert_stmt.condition.is_null());
	bz_assert(static_assert_stmt.condition.src_tokens.begin == nullptr);
	bz_assert(static_assert_stmt.message.is_null());
	bz_assert(static_assert_stmt.message.src_tokens.begin == nullptr);

	auto const static_assert_pos = static_assert_stmt.static_assert_pos;
	auto const [begin, end] = static_assert_stmt.arg_tokens;
	auto stream = begin;
	auto args = parse::parse_expression_comma_list(stream, end, context);
	if (stream != end)
	{
		auto const open_paren = begin - 1;
		if (open_paren->kind == lex::token::paren_open)
		{
			context.assert_token(stream, lex::token::paren_close);
		}
		else
		{
			context.report_error(stream);
		}
	}
	if (args.size() != 1 && args.size() != 2)
	{
		auto const src_tokens = begin == end
			? lex::src_tokens{ static_assert_pos, static_assert_pos, static_assert_pos + 1 }
			: lex::src_tokens{ begin, begin, end };
		context.report_error(
			src_tokens,
			bz::format(
				"static_assert expects 1 or 2 arguments, but was given {}",
				args.size()
			)
		);
		return;
	}

	{
		bool good = true;
		auto const match_type_and_consteval = [&](
			ast::expression &expr,
			uint32_t base_type_kind
		) {
			resolve_expression(expr, context);

			if (expr.is_error())
			{
				good = false;
				return;
			}

			auto base_type = ast::make_base_type_typespec({}, context.get_builtin_type_info(base_type_kind));
			context.match_expression_to_type(expr, base_type);
			if (!expr.is_error())
			{
				parse::consteval_try(expr, context);
			}
			good &= expr.not_error();
		};

		static_assert_stmt.condition = std::move(args[0]);
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

		if (args.size() == 2)
		{
			static_assert_stmt.message = std::move(args[1]);
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
	resolve_expression(expr_stmt.expr, context);
}

static void resolve_stmt(ast::decl_variable &var_decl, ctx::parse_context &context)
{
	resolve_variable(var_decl, context);
	context.add_local_variable(var_decl);
}

static void resolve_stmt(ast::decl_function &func_decl, ctx::parse_context &context)
{
	resolve_function(&func_decl, func_decl.body, context);
	context.add_local_function(func_decl);
}

static void resolve_stmt(ast::decl_operator &op_decl, ctx::parse_context &context)
{
	resolve_function(&op_decl, op_decl.body, context);
}

static void resolve_stmt(ast::decl_function_alias &func_alias_decl, ctx::parse_context &context)
{
	resolve_function_alias(func_alias_decl, context);
}

static void resolve_stmt(ast::decl_type_alias &type_alias_decl, ctx::parse_context &context)
{
	resolve_type_alias(type_alias_decl, context);
	context.add_local_type_alias(type_alias_decl);
}

static void resolve_stmt(ast::decl_struct &struct_decl, ctx::parse_context &context)
{
	resolve_type_info(struct_decl.info, context);
}

static void resolve_stmt(ast::decl_import &, ctx::parse_context &)
{
	bz_unreachable;
}

void resolve_statement(ast::statement &stmt, ctx::parse_context &context)
{
	if (stmt.not_null())
	{
		stmt.visit([&context](auto &inner_stmt) {
			resolve_stmt(inner_stmt, context);
		});
	}
}

void resolve_typespec(ast::typespec &ts, ctx::parse_context &context, precedence prec)
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
	resolve_expression(type, context);

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

void resolve_stmt_static_assert(ast::stmt_static_assert &static_assert_stmt, ctx::parse_context &context)
{
	auto const original_file_info = context.get_current_file_info();
	auto const stmt_file_id = static_assert_stmt.static_assert_pos->src_pos.file_id;
	if (original_file_info.file_id != stmt_file_id)
	{
		context.set_current_file(stmt_file_id);
	}
	resolve_stmt(static_assert_stmt, context);
	context.set_current_file_info(original_file_info);
}

static void apply_prototype(
	lex::token_range prototype,
	ast::decl_variable &var_decl,
	ctx::parse_context &context
)
{
	auto &type = var_decl.id_and_type.var_type;
	for (auto op = prototype.end; op != prototype.begin;)
	{
		--op;
		auto const src_tokens = lex::src_tokens{ op, op, var_decl.src_tokens.end };
		type = context.make_unary_operator_expression(src_tokens, op->kind, std::move(type));
	}
	if (!type.is_typename())
	{
		var_decl.clear_type();
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

	if (var_decl.get_type().is<ast::ts_unresolved>())
	{
		auto [stream, end] = var_decl.get_type().get<ast::ts_unresolved>().tokens;
		bz_assert(stream != end);
		var_decl.id_and_type.var_type = parse::parse_expression(stream, end, context, no_assign);
		resolve_expression(var_decl.id_and_type.var_type, context);
		auto &type = var_decl.id_and_type.var_type;
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
			return;
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
			return;
		}
		else if (!type.is_typename())
		{
			var_decl.clear_type();
			var_decl.state = ast::resolve_state::error;
			return;
		}
	}
	apply_prototype(var_decl.get_prototype_range(), var_decl, context);
}

static void resolve_variable_init_expr_and_match_type(ast::decl_variable &var_decl, ctx::parse_context &context)
{
	bz_assert(!var_decl.get_type().is_empty());
	if (var_decl.init_expr.not_null())
	{
		if (var_decl.init_expr.is_unresolved() && var_decl.init_expr.get_unresolved_expr().is_null())
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
		resolve_expression(var_decl.init_expr, context);
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
						ast::arena_vector<ast::expression>{},
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
	resolve_type_alias_impl(alias_decl, context);
	context.set_current_file_info(original_file_info);
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
			if (p.state != ast::resolve_state::error)
			{
				p.state = ast::resolve_state::symbol;
			}
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
		&& !func_body.is_generic()
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
				bz_unreachable;
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

static ctx::parse_context *get_context_ptr(bool is_local, bz::optional<ctx::parse_context> &new_context, ctx::parse_context &context)
{
	if (is_local)
	{
		auto const var_count = context.scope_decls
			.transform([](auto const &decl_set) { return decl_set.var_decl_range().count(); })
			.sum();
		if (var_count == 0)
		{
			return &context;
		}
		else
		{
			new_context.emplace(context, ctx::parse_context::local_copy_t{});
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
			return &new_context.get();
		}
	}
}

void resolve_function_parameters(
	ast::statement_view func_stmt,
	ast::function_body &func_body,
	ctx::parse_context &context
)
{
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
	auto const context_ptr = get_context_ptr(func_body.is_local(), new_context, context);

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
static bool resolve_function_symbol_helper(
	ast::statement_view func_stmt,
	ast::function_body &func_body,
	ctx::parse_context &context
)
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

static void resolve_function_symbol_impl(
	ast::statement_view func_stmt,
	ast::function_body &func_body,
	ctx::parse_context &context
)
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

void resolve_function_symbol(
	ast::statement_view func_stmt,
	ast::function_body &func_body,
	ctx::parse_context &context
)
{
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
	auto const context_ptr = get_context_ptr(func_body.is_local(), new_context, context);

	auto const original_file_info = context_ptr->get_current_file_info();
	auto const stmt_file_id = func_body.src_tokens.pivot->src_pos.file_id;
	if (original_file_info.file_id != stmt_file_id)
	{
		context_ptr->set_current_file(stmt_file_id);
	}
	resolve_function_symbol_impl(func_stmt, func_body, *context_ptr);
	context_ptr->set_current_file_info(original_file_info);
}

static void resolve_local_statements(
	bz::array_view<ast::statement> stmts,
	ctx::parse_context &context
)
{
	for (auto &stmt : stmts)
	{
		resolve_statement(stmt, context);
	}
}

static void resolve_function_impl(
	ast::statement_view func_stmt,
	ast::function_body &func_body,
	ctx::parse_context &context
)
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
			return;
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
	context.add_scope();
	func_body.body = parse::parse_local_statements(stream, end, context);
	context.remove_scope();
	context.add_scope();
	resolve_local_statements(func_body.body.get<bz::vector<ast::statement>>(), context);
	context.remove_scope();

	func_body.state = ast::resolve_state::all;
	context.remove_scope();
	context.current_function = prev_function;
}

void resolve_function(
	ast::statement_view func_stmt,
	ast::function_body &func_body,
	ctx::parse_context &context
)
{
	if (func_body.state >= ast::resolve_state::all || func_body.state == ast::resolve_state::error)
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
	auto const context_ptr = get_context_ptr(func_body.is_local(), new_context, context);

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

static void resolve_type_info_symbol_impl(ast::type_info &info, [[maybe_unused]] ctx::parse_context &context)
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

void resolve_type_info_symbol(ast::type_info &info, ctx::parse_context &context)
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

static void add_type_info_members(
	ast::type_info &info,
	ctx::parse_context &context
)
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
		resolve::resolve_variable(*member, context);
	}
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
		resolve_global_statement(stmt, context);
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

static void apply_attribute(
	ast::stmt_while &,
	ast::attribute const &attribute,
	ctx::parse_context &context
)
{
	context.report_warning(
		ctx::warning_kind::unknown_attribute,
		attribute.name,
		bz::format("unknown attribute '{}'", attribute.name->value)
	);
}

static void apply_attribute(
	ast::stmt_for &,
	ast::attribute const &attribute,
	ctx::parse_context &context
)
{
	context.report_warning(
		ctx::warning_kind::unknown_attribute,
		attribute.name,
		bz::format("unknown attribute '{}'", attribute.name->value)
	);
}

static void apply_attribute(
	ast::stmt_foreach &,
	ast::attribute const &attribute,
	ctx::parse_context &context
)
{
	context.report_warning(
		ctx::warning_kind::unknown_attribute,
		attribute.name,
		bz::format("unknown attribute '{}'", attribute.name->value)
	);
}

static void apply_attribute(
	ast::stmt_return &,
	ast::attribute const &attribute,
	ctx::parse_context &context
)
{
	context.report_warning(
		ctx::warning_kind::unknown_attribute,
		attribute.name,
		bz::format("unknown attribute '{}'", attribute.name->value)
	);
}

static void apply_attribute(
	ast::stmt_no_op &,
	ast::attribute const &attribute,
	ctx::parse_context &context
)
{
	context.report_warning(
		ctx::warning_kind::unknown_attribute,
		attribute.name,
		bz::format("unknown attribute '{}'", attribute.name->value)
	);
}

static void apply_attribute(
	ast::stmt_static_assert &,
	ast::attribute const &attribute,
	ctx::parse_context &context
)
{
	context.report_warning(
		ctx::warning_kind::unknown_attribute,
		attribute.name,
		bz::format("unknown attribute '{}'", attribute.name->value)
	);
}

static void apply_attribute(
	ast::stmt_expression &,
	ast::attribute const &attribute,
	ctx::parse_context &context
)
{
	context.report_warning(
		ctx::warning_kind::unknown_attribute,
		attribute.name,
		bz::format("unknown attribute '{}'", attribute.name->value)
	);
}

static void apply_maybe_unused_to_var_decl(ast::decl_variable &var_decl)
{
	var_decl.flags |= ast::decl_variable::maybe_unused;
	for (auto &decl : var_decl.tuple_decls)
	{
		apply_maybe_unused_to_var_decl(decl);
	}
}

static void apply_attribute(
	ast::decl_variable &var_decl,
	ast::attribute &attribute,
	ctx::parse_context &context
)
{
	if (attribute.name->value == "maybe_unused")
	{
		if (attribute.args.size() != 0)
		{
			context.report_error(
				{ attribute.arg_tokens.begin, attribute.arg_tokens.begin, attribute.arg_tokens.end },
				"@maybe_unused expects no arguments"
			);
		}
		apply_maybe_unused_to_var_decl(var_decl);
	}
	else if (attribute.name->value == "comptime_error_checking")
	{
		if (attribute.args.size() != 1)
		{
			context.report_error(attribute.name, "@comptime_error_checking expects exactly one argument");
			return;
		}

		{
			parse::consteval_try(attribute.args[0], context);
			auto const [type, _] = attribute.args[0].get_expr_type_and_kind();
			auto const type_without_const = ast::remove_const_or_consteval(type);
			if (
				!type_without_const.is<ast::ts_base_type>()
				|| type_without_const.get<ast::ts_base_type>().info->kind != ast::type_info::str_
			)
			{
				context.report_error(attribute.args[0], "kind in @comptime_error_checking must have type 'str'");
				return;
			}
		}

		auto const kind = attribute.args[0]
			.get<ast::constant_expression>().value
			.get<ast::constant_value::string>().as_string_view();

		if (!context.global_ctx.add_comptime_checking_variable(kind, &var_decl))
		{
			context.report_error(attribute.args[0], bz::format("invalid kind '{}' for @comptime_error_checking", kind));
		}
	}
	else if (attribute.name->value == "no_runtime_emit")
	{
		if (attribute.args.size() != 0)
		{
			context.report_error(
				{ attribute.arg_tokens.begin, attribute.arg_tokens.begin, attribute.arg_tokens.end },
				"@no_runtime_emit expects no arguments"
			);
		}
		var_decl.flags |= ast::decl_variable::no_runtime_emit;
	}
	else
	{
		context.report_warning(
			ctx::warning_kind::unknown_attribute,
			attribute.name,
			bz::format("unknown attribute '{}'", attribute.name->value)
		);
	}
}

static void apply_symbol_name(
	ast::function_body &func_body,
	ast::attribute &attribute,
	ctx::parse_context &context
)
{
	bz_assert(attribute.name->value == "symbol_name");
	if (attribute.args.size() != 1)
	{
		context.report_error(attribute.name, "@symbol_name expects exactly one argument");
		return;
	}

	bz_assert(func_body.state >= ast::resolve_state::parameters);
	if (func_body.is_generic())
	{
		context.report_error(attribute.name, "@symbol_name cannot be applied to generic functions");
		return;
	}

	// symbol name
	{
		parse::consteval_try(attribute.args[0], context);
		auto const [type, _] = attribute.args[0].get_expr_type_and_kind();
		auto const type_without_const = ast::remove_const_or_consteval(type);
		if (
			!type_without_const.is<ast::ts_base_type>()
			|| type_without_const.get<ast::ts_base_type>().info->kind != ast::type_info::str_
		)
		{
			context.report_error(attribute.args[0], "name in @symbol_name must have type 'str'");
			return;
		}
	}

	auto const symbol_name = attribute.args[0]
		.get<ast::constant_expression>().value
		.get<ast::constant_value::string>().as_string_view();

	func_body.symbol_name = symbol_name;
	func_body.flags |= ast::function_body::external_linkage;
}

static void apply_no_comptime_checking(
	ast::function_body &func_body,
	ast::attribute &attribute,
	ctx::parse_context &context
)
{
	if (!attribute.args.empty())
	{
		context.report_error(attribute.name, "@no_comptime_checking expects no arguments");
	}

	func_body.flags |= ast::function_body::no_comptime_checking;
	for (auto const &specialization : func_body.generic_specializations)
	{
		specialization->flags |= ast::function_body::no_comptime_checking;
	}
}

static void apply_comptime_error_checking(
	ast::function_body &func_body,
	ast::attribute &attribute,
	ctx::parse_context &context
)
{
	if (attribute.args.size() != 1)
	{
		context.report_error(attribute.name, "@comptime_error_checking expects exactly one argument");
		return;
	}

	{
		parse::consteval_try(attribute.args[0], context);
		auto const [type, _] = attribute.args[0].get_expr_type_and_kind();
		auto const type_without_const = ast::remove_const_or_consteval(type);
		if (
			!type_without_const.is<ast::ts_base_type>()
			|| type_without_const.get<ast::ts_base_type>().info->kind != ast::type_info::str_
		)
		{
			context.report_error(attribute.args[0], "kind in @comptime_error_checking must have type 'str'");
			return;
		}
	}

	auto const kind = attribute.args[0]
		.get<ast::constant_expression>().value
		.get<ast::constant_value::string>().as_string_view();

	if (!context.global_ctx.add_comptime_checking_function(kind, &func_body))
	{
		context.report_error(attribute.args[0], bz::format("invalid kind '{}' for @comptime_error_checking", kind));
	}
	func_body.flags |= ast::function_body::no_comptime_checking;
}

static void apply_builtin(
	ast::function_body &func_body,
	ast::attribute &attribute,
	ctx::parse_context &context
)
{
	if (attribute.args.size() != 1)
	{
		context.report_error(attribute.name, "@__builtin expects exactly one argument");
		return;
	}

	{
		parse::consteval_try(attribute.args[0], context);
		auto const [type, _] = attribute.args[0].get_expr_type_and_kind();
		auto const type_without_const = ast::remove_const_or_consteval(type);
		if (
			!type_without_const.is<ast::ts_base_type>()
			|| type_without_const.get<ast::ts_base_type>().info->kind != ast::type_info::str_
		)
		{
			context.report_error(attribute.args[0], "kind in @__builtin must have type 'str'");
			return;
		}
	}

	auto const kind = attribute.args[0]
		.get<ast::constant_expression>().value
		.get<ast::constant_value::string>().as_string_view();

	if (!context.global_ctx.add_builtin_function(kind, &func_body))
	{
		context.report_error(attribute.args[0], bz::format("invalid kind '{}' for @__builtin", kind));
	}
	func_body.flags |= ast::function_body::intrinsic;
}

static void apply_attribute(
	ast::decl_function &func_decl,
	ast::attribute &attribute,
	ctx::parse_context &context
)
{
	auto const attr_name = attribute.name->value;
	if (attr_name == "symbol_name")
	{
		apply_symbol_name(func_decl.body, attribute, context);
	}
	else if (attr_name == "no_comptime_checking")
	{
		apply_no_comptime_checking(func_decl.body, attribute, context);
	}
	else if (attr_name == "comptime_error_checking")
	{
		apply_comptime_error_checking(func_decl.body, attribute, context);
	}
	else if (attr_name == "__builtin")
	{
		apply_builtin(func_decl.body, attribute, context);
	}
	else
	{
		context.report_warning(
			ctx::warning_kind::unknown_attribute,
			attribute.name,
			bz::format("unknown attribute '{}'", attr_name)
		);
	}
}

static void apply_attribute(
	ast::decl_operator &op_decl,
	ast::attribute &attribute,
	ctx::parse_context &context
)
{
	auto const attr_name = attribute.name->value;
	if (attr_name == "symbol_name")
	{
		apply_symbol_name(op_decl.body, attribute, context);
	}
	else if (attr_name == "no_comptime_checking")
	{
		apply_no_comptime_checking(op_decl.body, attribute, context);
	}
	else if (attr_name == "comptime_error_checking")
	{
		apply_comptime_error_checking(op_decl.body, attribute, context);
	}
	else if (attr_name == "builtin")
	{
		apply_builtin(op_decl.body, attribute, context);
	}
	else
	{
		context.report_warning(
			ctx::warning_kind::unknown_attribute,
			attribute.name,
			bz::format("unknown attribute '{}'", attr_name)
		);
	}
}

static void apply_attribute(
	ast::decl_function_alias &,
	ast::attribute const &attribute,
	ctx::parse_context &context
)
{
	context.report_warning(
		ctx::warning_kind::unknown_attribute,
		attribute.name,
		bz::format("unknown attribute '{}'", attribute.name->value)
	);
}

static void apply_attribute(
	ast::decl_type_alias &,
	ast::attribute const &attribute,
	ctx::parse_context &context
)
{
	context.report_warning(
		ctx::warning_kind::unknown_attribute,
		attribute.name,
		bz::format("unknown attribute '{}'", attribute.name->value)
	);
}

static void apply_attribute(
	ast::decl_struct &,
	ast::attribute const &attribute,
	ctx::parse_context &context
)
{
	context.report_warning(
		ctx::warning_kind::unknown_attribute,
		attribute.name,
		bz::format("unknown attribute '{}'", attribute.name->value)
	);
}

static void apply_attribute(
	ast::decl_import &,
	ast::attribute const &attribute,
	ctx::parse_context &context
)
{
	context.report_warning(
		ctx::warning_kind::unknown_attribute,
		attribute.name,
		bz::format("unknown attribute '{}'", attribute.name->value)
	);
}


static void apply_attribute(
	ast::statement_view stmt,
	ast::attribute &attribute,
	ctx::parse_context &context
)
{
	stmt.visit([&](auto &s) {
		apply_attribute(s, attribute, context);
	});
}

void resolve_attributes(ast::statement_view stmt, ctx::parse_context &context)
{
	auto const attributes = stmt.get_attributes();
	for (auto &attribute : attributes)
	{
		if (attribute.args.size() != 0)
		{
			// attributes have already been resolved
			return;
		}
		auto [stream, end] = attribute.arg_tokens;
		if (stream != end)
		{
			attribute.args = parse::parse_expression_comma_list(stream, end, context);
			if (stream != end)
			{
				context.report_error({ stream, stream, end });
			}

			for (auto &arg : attribute.args)
			{
				parse::consteval_try(arg, context);
				if (arg.not_error() && !arg.is<ast::constant_expression>())
				{
					context.report_error(arg, "attribute argument must be a constant expression");
				}
			}
		}

		apply_attribute(stmt, attribute, context);
	}
}

void resolve_global_statement(ast::statement &stmt, ctx::parse_context &context)
{
	stmt.visit(bz::overload{
		[&](ast::decl_function &func_decl) {
			context.add_to_resolve_queue({}, func_decl.body);
			resolve_function(stmt, func_decl.body, context);
			context.pop_resolve_queue();
		},
		[&](ast::decl_operator &op_decl) {
			context.add_to_resolve_queue({}, op_decl.body);
			resolve_function(stmt, op_decl.body, context);
			context.pop_resolve_queue();
		},
		[&](ast::decl_function_alias &alias_decl) {
			context.add_to_resolve_queue({}, alias_decl);
			resolve_function_alias(alias_decl, context);
			context.pop_resolve_queue();
		},
		[&](ast::decl_type_alias &alias_decl) {
			context.add_to_resolve_queue({}, alias_decl);
			resolve_type_alias(alias_decl, context);
			context.pop_resolve_queue();
		},
		[&](ast::decl_struct &struct_decl) {
			context.add_to_resolve_queue({}, struct_decl.info);
			resolve_type_info(struct_decl.info, context);
			context.pop_resolve_queue();
		},
		[&](ast::decl_variable &var_decl) {
			context.add_to_resolve_queue({}, var_decl);
			resolve_variable(var_decl, context);
			context.pop_resolve_queue();
			if (!var_decl.is_member() && var_decl.state != ast::resolve_state::error && var_decl.init_expr.not_null())
			{
				parse::consteval_try(var_decl.init_expr, context);
				if (var_decl.init_expr.not_error() && !var_decl.init_expr.has_consteval_succeeded())
				{
					context.report_error(
						var_decl.src_tokens,
						"a global variable must be initialized by a constant expression",
						parse::get_consteval_fail_notes(var_decl.init_expr)
					);
				}
			}
		},
		[&](ast::stmt_static_assert &static_assert_stmt) {
			resolve_stmt_static_assert(static_assert_stmt, context);
		},
		[](ast::decl_import) {
			// nothing
		},
		[&](auto &) {
			bz_unreachable;
		}
	});
	resolve_attributes(stmt, context);
}

} // namespace resolve
