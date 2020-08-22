#include "statement_parser.h"
#include "token_info.h"
#include "expression_parser.h"
#include "parse_common.h"

namespace parse
{

// parse functions can't be static, because they are referenced in parse_common.h

static void resolve_attributes(
	ast::statement &stmt,
	ctx::parse_context &context
);

static void resolve_stmt_static_assert(
	ast::stmt_static_assert &static_assert_stmt,
	ctx::parse_context &context
)
{
	bz_assert(static_assert_stmt.condition.is_null());
	bz_assert(static_assert_stmt.condition.src_tokens.begin == nullptr);
	bz_assert(static_assert_stmt.message.is_null());
	bz_assert(static_assert_stmt.message.src_tokens.begin == nullptr);

	auto const [begin, end] = static_assert_stmt.arg_tokens;
	auto stream = begin;
	auto args = parse_expression_comma_list(stream, end, context);
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
		context.report_error(
			{ begin, begin, end },
			bz::format(
				"static_assert expects 1 or 2 arguments, but was given {}",
				args.size()
			)
		);
		return;
	}

	{
		bool good = true;
		auto const check_type = [&](
			ast::expression const &expr,
			uint32_t base_type_kind,
			bz::u8string_view message
		) {
			if (!expr.is_constant_or_dynamic())
			{
				return;
			}

			auto const [type, _] = expr.get_expr_type_and_kind();
			auto const without_const = ast::remove_const_or_consteval(type);
			if (
				!without_const.is<ast::ts_base_type>()
				|| without_const.get<ast::ts_base_type>().info->kind != base_type_kind
			)
			{
				good = false;
				context.report_error(expr, message);
			}
		};

		static_assert_stmt.condition = std::move(args[0]);
		if (static_assert_stmt.condition.is_null())
		{
			good = false;
		}
		else if (!static_assert_stmt.condition.is<ast::constant_expression>())
		{
			good = false;
			context.report_error(
				static_assert_stmt.condition,
				"condition for static_assert must be a constant expression"
			);
		}
		check_type(
			static_assert_stmt.condition,
			ast::type_info::bool_,
			"condition for static_assert must have type 'bool'"
		);

		if (args.size() == 2)
		{
			static_assert_stmt.message = std::move(args[1]);
			if (static_assert_stmt.message.is_null())
			{
				good = false;
			}
			else if (!static_assert_stmt.message.is<ast::constant_expression>())
			{
				good = false;
				context.report_error(
					static_assert_stmt.message,
					"message in static_assert must be a constant expression"
				);
			}
			check_type(
				static_assert_stmt.message,
				ast::type_info::str_,
				"message in static_assert must have type 'str'"
			);
		}

		if (!good)
		{
			return;
		}
	}

	auto &cond_const_expr = static_assert_stmt.condition.get<ast::constant_expression>();
	bz_assert(cond_const_expr.value.kind() == ast::constant_value::boolean);
	auto const cond = cond_const_expr.value.get<ast::constant_value::boolean>();

	if (!cond)
	{
		if (static_assert_stmt.message.is_null())
		{
			context.report_error(
				static_assert_stmt.condition,
				"static assertion failed"
			);
		}
		else
		{
			auto &message_const_expr = static_assert_stmt.message.get<ast::constant_expression>();
			bz_assert(message_const_expr.value.kind() == ast::constant_value::string);
			auto const message = message_const_expr.value.get<ast::constant_value::string>().as_string_view();
			context.report_error(
				static_assert_stmt.condition,
				bz::format(
					"static assertion failed, message: '{}'",
					ctx::convert_string_for_message(message)
				)
			);
		}
	}
}

template<bool is_global>
ast::statement parse_stmt_static_assert(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream != end);
	bz_assert(stream->kind == lex::token::kw_static_assert);
	++stream; // 'static_assert'
	auto const open_paren = context.assert_token(stream, lex::token::paren_open);
	auto const args = get_expression_tokens<
		lex::token::paren_close
	>(stream, end, context);
	if (stream != end && stream->kind == lex::token::paren_close)
	{
		++stream; // ')'
	}
	else
	{
		if (open_paren->kind == lex::token::paren_open)
		{
			context.report_paren_match_error(stream, open_paren);
		}
	}
	context.assert_token(stream, lex::token::semi_colon);

	if constexpr (is_global)
	{
		return ast::make_stmt_static_assert(args);
	}
	else
	{
		auto result = ast::make_stmt_static_assert(args);
		bz_assert(result.is<ast::stmt_static_assert>());
		resolve_stmt_static_assert(*result.get<ast::stmt_static_assert_ptr>(), context);
		return result;
	}
}

template ast::statement parse_stmt_static_assert<false>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template ast::statement parse_stmt_static_assert<true>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

static std::tuple<lex::token_range, lex::token_pos, lex::token_range>
parse_decl_variable_id_and_type(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context,
	bool needs_id = true
)
{
	auto const prototype_begin = stream;
	while (stream != end && is_unary_type_op(stream->kind))
	{
		++stream;
	}
	auto const prototype = lex::token_range{ prototype_begin, stream };
	if (stream == end)
	{
		return { {}, nullptr, {} };
	}

	auto const id = [&]() {
		if (needs_id)
		{
			return context.assert_token(stream, lex::token::identifier);
		}
		else if (stream->kind == lex::token::identifier)
		{
			auto const id = stream;
			++stream;
			return id;
		}
		else
		{
			return stream;
		}
	}();

	if (stream == end || stream->kind != lex::token::colon)
	{
		return { prototype, id, {} };
	}

	++stream; // ':'
	auto const type = get_expression_tokens<
		lex::token::assign,
		lex::token::comma,
		lex::token::paren_close,
		lex::token::square_close
	>(stream, end, context);

	return { prototype, id, type };
}

static void resolve_typespec(
	ast::typespec &ts,
	ctx::parse_context &context,
	precedence prec
)
{
	bz_assert(ts.is<ast::ts_unresolved>());
	auto [stream, end] = ts.get<ast::ts_unresolved>().tokens;
	auto type = parse_expression(stream, end, context, prec, true);
	if (stream != end)
	{
		context.report_error({ stream, stream, end });
	}
	if (type.not_null() && !type.is_typename())
	{
		ts.clear();
		context.report_error(type, "expected a type");
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

static void resolve_var_decl_type(
	ast::decl_variable &var_decl,
	ctx::parse_context &context
)
{
	bz_assert(var_decl.var_type.is<ast::ts_unresolved>());
	auto [stream, end] = var_decl.var_type.get<ast::ts_unresolved>().tokens;
	auto type = stream == end
		? ast::make_constant_expression(
			{},
			ast::expression_type_kind::type_name,
			ast::make_typename_typespec(nullptr),
			ast::constant_value(ast::make_auto_typespec(nullptr)),
			ast::make_expr_identifier(nullptr)
		)
		: parse_expression(stream, end, context, no_assign, true);
	if (type.not_null() && !type.is_typename())
	{
		bz_assert(stream < end);
		if (is_binary_operator(stream->kind))
		{
			bz_assert(stream->kind != lex::token::assign);
			context.report_error(
				{ stream, stream, end },
				"expected ';' or '=' at the end of a type",
				{ context.make_note(
					stream,
					bz::format("operator {} is not allowed in a variable declaration's type", stream->value)
				) }
			);
		}
		else
		{
			context.report_error({ stream, stream, end });
		}
		var_decl.var_type.clear();
	}
	else if (type.is_typename())
	{
		auto const prototype = var_decl.prototype_range;
		for (auto op = prototype.end; op != prototype.begin;)
		{
			--op;
			auto const src_tokens = lex::src_tokens{ op, op, type.src_tokens.end };
			type = context.make_unary_operator_expression(src_tokens, op, std::move(type));
		}
		if (type.is_typename())
		{
			var_decl.var_type = std::move(type.get_typename());
		}
		else
		{
			var_decl.var_type.clear();
		}
	}
	else
	{
		var_decl.var_type.clear();
	}
}

static void resolve_decl_variable(
	ast::decl_variable &var_decl,
	ctx::parse_context &context
)
{
	resolve_var_decl_type(var_decl, context);
	if (var_decl.init_expr.not_null())
	{
		bz_assert(var_decl.init_expr.is<ast::unresolved_expression>());
		auto stream = var_decl.init_expr.src_tokens.begin;
		auto const end = var_decl.init_expr.src_tokens.end;
		context.parenthesis_suppressed_value = 0;
		var_decl.init_expr = parse_expression(stream, end, context, no_comma, false);
		if (stream != end)
		{
			if (stream->kind == lex::token::comma)
			{
				context.report_error(
					stream,
					"operator , is not allowed in variable initialization expression"
				);
			}
			else
			{
				context.assert_token(stream, lex::token::semi_colon);
			}
		}
		context.match_expression_to_type(var_decl.init_expr, var_decl.var_type);
	}
	else
	{
		if (var_decl.var_type.is_empty())
		{
			bz_assert(context.has_errors());
		}
		else if (!ast::is_complete(var_decl.var_type))
		{
			context.report_error(
				var_decl.identifier,
				bz::format(
					"a variable with an incomplete type '{}' must be initialized",
					var_decl.var_type
				)
			);
			var_decl.var_type.clear();
		}
		else if (var_decl.var_type.is<ast::ts_const>())
		{
			context.report_error(
				var_decl.identifier,
				"a variable with a 'const' type must be initialized"
			);
			var_decl.var_type.clear();
		}
		else if (var_decl.var_type.is<ast::ts_consteval>())
		{
			context.report_error(
				var_decl.identifier,
				"a variable with a 'consteval' type must be initialized"
			);
			var_decl.var_type.clear();
		}
	}
}

template<bool is_global>
ast::statement parse_decl_variable(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream != end);
	bz_assert(
		stream->kind == lex::token::kw_let
		|| stream->kind == lex::token::kw_const
		|| stream->kind == lex::token::kw_consteval
	);
	auto const begin = stream;
	if (stream->kind == lex::token::kw_let)
	{
		++stream;
	}

	auto const [prototype, id, type] = parse_decl_variable_id_and_type(stream, end, context);
	if (stream != end && stream->kind == lex::token::assign)
	{
		++stream; // '='
		auto const init_expr = get_expression_tokens<>(stream, end, context);
		context.assert_token(stream, lex::token::semi_colon);
		if constexpr (is_global)
		{
			return ast::make_decl_variable(
				lex::token_range{ begin, stream },
				id, prototype,
				ast::make_unresolved_typespec(type),
				ast::make_unresolved_expression({ init_expr.begin, init_expr.begin, init_expr.end })
			);
		}
		else
		{
			auto result = ast::make_decl_variable(
				lex::token_range{ begin, stream },
				id, prototype,
				ast::make_unresolved_typespec(type),
				ast::make_unresolved_expression({ init_expr.begin, init_expr.begin, init_expr.end })
			);
			bz_assert(result.is<ast::decl_variable>());
			auto &var_decl = *result.get<ast::decl_variable_ptr>();
			resolve_decl_variable(var_decl, context);
			context.add_local_variable(var_decl);
			return result;
		}
	}
	else
	{
		if constexpr (is_global)
		{
			return ast::make_decl_variable(
				lex::token_range{ begin, stream },
				id, prototype,
				ast::make_unresolved_typespec(type)
			);
		}
		else
		{
			auto result = ast::make_decl_variable(
				lex::token_range{ begin, stream },
				id, prototype,
				ast::make_unresolved_typespec(type)
			);
			bz_assert(result.is<ast::decl_variable>());
			auto &var_decl = *result.get<ast::decl_variable_ptr>();
			resolve_decl_variable(var_decl, context);
			context.add_local_variable(var_decl);
			return result;
		}
	}
}

template ast::statement parse_decl_variable<false>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template ast::statement parse_decl_variable<true>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

// resolves the function symbol, but doesn't modify scope
static bool resolve_function_symbol_helper(
	ast::function_body &func_body,
	ctx::parse_context &context
)
{
	bz_assert(func_body.state == ast::resolve_state::resolving_symbol);
	bool good = true;
	for (auto &p : func_body.params)
	{
		resolve_var_decl_type(p, context);
		if (p.var_type.is_empty())
		{
			good = false;
		}
	}
	for (auto &p : func_body.params)
	{
		context.add_local_variable(p);
	}

	resolve_typespec(func_body.return_type, context, precedence{});
	bz_assert(!func_body.return_type.is<ast::ts_unresolved>());
	if (!good || func_body.return_type.is_empty())
	{
		return false;
	}

	context.add_function_for_compilation(func_body);
	return true;
}

void resolve_function_symbol(
	ast::function_body &func_body,
	ctx::parse_context &context
)
{
	if (func_body.state >= ast::resolve_state::symbol || func_body.state == ast::resolve_state::error)
	{
		return;
	}
	else if (func_body.state == ast::resolve_state::resolving_symbol)
	{
		context.report_circular_dependency_error(func_body);
		func_body.state = ast::resolve_state::error;
		return;
	}

	func_body.state = ast::resolve_state::resolving_symbol;
	context.add_scope();
	if (resolve_function_symbol_helper(func_body, context))
	{
		func_body.state = ast::resolve_state::symbol;
	}
	else
	{
		func_body.state = ast::resolve_state::error;
	}
	for (auto &var_decl : func_body.params)
	{
		var_decl.is_used = true;
	}
	context.remove_scope();
}

void resolve_function(
	ast::function_body &func_body,
	ctx::parse_context &context
)
{
	if (func_body.state >= ast::resolve_state::all || func_body.state == ast::resolve_state::error)
	{
		return;
	}
	else if (
		func_body.state == ast::resolve_state::resolving_symbol
		|| func_body.state == ast::resolve_state::resolving_all
	)
	{
		context.report_circular_dependency_error(func_body);
		return;
	}

	if (func_body.state == ast::resolve_state::none)
	{
		func_body.state = ast::resolve_state::resolving_symbol;
		context.add_scope();
		if (!resolve_function_symbol_helper(func_body, context))
		{
			func_body.state = ast::resolve_state::error;
			context.remove_scope();
			return;
		}
	}
	else if (func_body.body.is_null())
	{
		return;
	}
	else
	{
		context.add_scope();
		for (auto &p : func_body.params)
		{
			context.add_local_variable(p);
		}
	}

	if (func_body.body.is_null())
	{
		func_body.state = ast::resolve_state::symbol;
		context.remove_scope();
		return;
	}

	func_body.state = ast::resolve_state::resolving_all;

	bz_assert(func_body.body.is<lex::token_range>());
	auto [stream, end] = func_body.body.get<lex::token_range>();
	func_body.body = parse_local_statements(stream, end, context);

	context.remove_scope();
}

static ast::function_body parse_function_body(
	lex::src_tokens src_tokens,
	bz::u8string function_name,
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	auto const open_paren = context.assert_token(stream, lex::token::paren_open);
	auto [param_stream, param_end] = get_expression_tokens<
		lex::token::paren_close
	>(stream, end, context);

	if (param_end != end && param_end->kind == lex::token::paren_close)
	{
		++stream; // ')'
	}
	else if (open_paren->kind == lex::token::paren_open)
	{
		context.report_paren_match_error(param_end, open_paren);
	}

	ast::function_body result = {};
	result.src_tokens = src_tokens;
	result.function_name = std::move(function_name);
	while (param_stream != param_end)
	{
		auto const begin = param_stream;
		auto const [prototype, id, type] = parse_decl_variable_id_and_type(
			param_stream, param_end,
			context, false
		);
		result.params.emplace_back(
			lex::token_range{ begin, param_stream },
			id, prototype,
			ast::make_unresolved_typespec(type)
		);
		if (param_stream != param_end)
		{
			context.assert_token(param_stream, lex::token::comma, lex::token::paren_close);
		}
		if (param_stream == begin)
		{
			context.report_error(param_stream);
			++param_stream;
		}
	}

	if (stream != end && stream->kind == lex::token::arrow)
	{
		++stream; // '->'
		auto const ret_type = get_expression_tokens<
			lex::token::curly_open
		>(stream, end, context);
		result.return_type = ast::make_unresolved_typespec(ret_type);
	}

	if (stream != end && stream->kind == lex::token::curly_open)
	{
		++stream; // '{'
		auto const body_tokens = get_tokens_in_curly<>(stream, end, context);
		result.body = body_tokens;
	}
	else if (stream == end || stream->kind != lex::token::semi_colon)
	{
		for (auto &var_decl : result.params)
		{
			var_decl.is_used = true;
		}
		context.assert_token(stream, lex::token::curly_open, lex::token::semi_colon);
	}
	else
	{
		for (auto &var_decl : result.params)
		{
			var_decl.is_used = true;
		}
		++stream; // ';'
	}

	return result;
}

template<bool is_global>
ast::statement parse_decl_function(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream != end);
	bz_assert(stream->kind == lex::token::kw_function);
	auto const begin = stream;
	++stream; // 'function'
	auto const id = context.assert_token(stream, lex::token::identifier);
	auto const src_tokens = id->kind == lex::token::identifier
		? lex::src_tokens{ begin, id, stream }
		: lex::src_tokens{ begin, begin, stream };
	auto body = parse_function_body(src_tokens, id->value, stream, end, context);

	if constexpr (is_global)
	{
		return ast::make_decl_function(id, std::move(body));
	}
	else
	{
		auto result = ast::make_decl_function(id, std::move(body));
		bz_assert(result.is<ast::decl_function>());
		auto &func_decl = *result.get<ast::decl_function_ptr>();
		resolve_function(func_decl.body, context);
		if (func_decl.body.state != ast::resolve_state::error)
		{
			context.add_local_function(func_decl);
		}
		return result;
	}
}

template ast::statement parse_decl_function<false>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template ast::statement parse_decl_function<true>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template<bool is_global>
ast::statement parse_decl_operator(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream != end);
	bz_assert(stream->kind == lex::token::kw_operator);
	auto const begin = stream;
	++stream; // 'operator'
	auto const op = stream;
	if (!is_overloadable_operator(op->kind))
	{
		context.report_error(
			op,
			is_operator(op->kind)
			? bz::format("operator {} is not overloadable", op->value)
			: bz::u8string("expected an overloadable operator")
		);
	}
	else
	{
		++stream;
	}

	if (op->kind == lex::token::paren_open)
	{
		context.assert_token(stream, lex::token::paren_close);
	}
	else if (op->kind == lex::token::square_open)
	{
		context.assert_token(stream, lex::token::square_close);
	}

	auto const src_tokens = is_operator(op->kind)
		? lex::src_tokens{ begin, op, stream }
		: lex::src_tokens{ begin, begin, begin + 1 };

	auto body = parse_function_body(src_tokens, bz::format("{}", op->kind), stream, end, context);

	if constexpr (is_global)
	{
		return ast::make_decl_operator(op, std::move(body));
	}
	else
	{
		auto result = ast::make_decl_operator(op, std::move(body));
		bz_assert(result.is<ast::decl_operator>());
		auto &op_decl = *result.get<ast::decl_operator_ptr>();
		resolve_function(op_decl.body, context);
		if (op_decl.body.state != ast::resolve_state::error)
		{
			context.add_local_operator(op_decl);
		}
		return result;
	}
}

template ast::statement parse_decl_operator<false>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template ast::statement parse_decl_operator<true>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template<bool is_global>
ast::statement parse_attribute_statement(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream != end);
	bz_assert(stream->kind == lex::token::at);
	bz::vector<ast::attribute> attributes = {};
	while (stream != end && stream->kind == lex::token::at)
	{
		++stream; // '@'
		auto const name = context.assert_token(stream, lex::token::identifier);
		if (stream->kind == lex::token::paren_open)
		{
			auto const paren_open = stream;
			++stream;
			auto const args_range = get_expression_tokens<
				lex::token::paren_close
			>(stream, end, context);
			if (stream != end && stream->kind == lex::token::paren_close)
			{
				++stream;
			}
			else
			{
				context.report_paren_match_error(stream, paren_open);
			}
			attributes.emplace_back(name, args_range, bz::vector<ast::expression>{});
		}
		else
		{
			attributes.emplace_back(name, lex::token_range{}, bz::vector<ast::expression>{});
		}
	}

	constexpr auto parser_fn = is_global ? &parse_global_statement : &parse_local_statement;
	auto statement = parser_fn(stream, end, context);
	if constexpr (is_global)
	{
		statement.set_attributes_without_resolve(std::move(attributes));
	}
	else
	{
		statement.set_attributes_without_resolve(std::move(attributes));
		resolve_attributes(statement, context);
	}
	return statement;
}

// explicit intantiation of parse_attribute_statement, because it is referenced in parse_common.h
template ast::statement parse_attribute_statement<false>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template ast::statement parse_attribute_statement<true>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

ast::statement parse_stmt_while(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream != end);
	bz_assert(stream->kind == lex::token::kw_while);
	auto const begin = stream;
	++stream; // 'while'
	auto condition = parse_parenthesized_condition(stream, end, context);
	if (!condition.is_null())
	{
		auto const [type, _] = condition.get_expr_type_and_kind();
		if (
			auto const type_without_const = ast::remove_const_or_consteval(type);
			!type_without_const.is<ast::ts_base_type>()
			|| type_without_const.get<ast::ts_base_type>().info->kind != ast::type_info::bool_
		)
		{
			context.report_error(condition, "condition for while statement must have type 'bool'");
		}
	}
	auto body = parse_local_statement(stream, end, context);
	return ast::make_stmt_while(
		lex::token_range{ begin, stream },
		std::move(condition), std::move(body)
	);
}

ast::statement parse_stmt_for(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream != end);
	bz_assert(stream->kind == lex::token::kw_for);
	++stream; // 'for'
	auto const open_paren = context.assert_token(stream, lex::token::paren_open);

	context.add_scope();

	if (stream == end)
	{
		context.report_error(stream, "expected initialization statement or ';'");
		return {};
	}
	auto init_stmt = stream->kind == lex::token::semi_colon
		? (++stream, ast::statement())
		: parse_local_statement(stream, end, context);
	if (stream == end)
	{
		context.report_error(stream, "expected loop condition or ';'");
		return {};
	}
	auto condition = stream->kind == lex::token::semi_colon
		? ast::expression()
		: parse_expression(stream, end, context, precedence{}, true);
	if (context.assert_token(stream, lex::token::semi_colon)->kind != lex::token::semi_colon)
	{
		get_expression_tokens<
			lex::token::curly_open,
			lex::token::kw_if,
			lex::token::paren_close
		>(stream, end, context);
	}
	if (!condition.is_null())
	{
		auto const [type, _] = condition.get_expr_type_and_kind();
		if (
			auto const type_without_const = ast::remove_const_or_consteval(type);
			!type_without_const.is<ast::ts_base_type>()
			|| type_without_const.get<ast::ts_base_type>().info->kind != ast::type_info::bool_
		)
		{
			context.report_error(condition, "condition for for statement must have type 'bool'");
		}
	}

	if (stream == end)
	{
		context.report_error(stream, "expected iteration expression or closing )");
		return {};
	}
	auto iteration = stream->kind == lex::token::paren_close
		? ast::expression()
		: parse_expression(stream, end, context, precedence{}, true);
	if (stream != end && stream->kind == lex::token::paren_close)
	{
		++stream;
	}
	else if (open_paren->kind == lex::token::paren_open)
	{
		context.report_paren_match_error(stream, open_paren);
		get_expression_tokens<
			lex::token::curly_open,
			lex::token::kw_if,
			lex::token::paren_close
		>(stream, end, context);
	}

	auto body = parse_local_statement(stream, end, context);

	context.remove_scope();

	return ast::make_stmt_for(
		std::move(init_stmt),
		std::move(condition),
		std::move(iteration),
		std::move(body)
	);
}

ast::statement parse_stmt_return(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream != end);
	bz_assert(stream->kind == lex::token::kw_return);
	++stream; // 'return'
	if (stream != end && stream->kind == lex::token::semi_colon)
	{
		return ast::make_stmt_return();
	}
	auto expr = parse_expression(stream, end, context, precedence{}, true);
	context.assert_token(stream, lex::token::semi_colon);
	return ast::make_stmt_return(std::move(expr));
}

ast::statement parse_stmt_no_op(
	lex::token_pos &stream, lex::token_pos end,
	[[maybe_unused]] ctx::parse_context &context
)
{
	bz_assert(stream != end);
	bz_assert(stream->kind == lex::token::semi_colon);
	++stream; // ';'
	return ast::make_stmt_no_op();
}

ast::statement parse_stmt_expression(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	auto expr = parse_top_level_expression(stream, end, context);
	return ast::make_stmt_expression(std::move(expr));
}

static ast::statement default_global_statement_parser(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	context.report_error(stream);
	++stream;
	return parse_global_statement(stream, end, context);
}

static ast::statement default_local_statement_parser(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	auto const begin = stream;
	auto expr_stmt = parse_stmt_expression(stream, end, context);
	if (stream == begin)
	{
		context.report_error(stream);
		++stream;
		return parse_local_statement(stream, end, context);
	}
	else
	{
		return expr_stmt;
	}
}


ast::statement parse_global_statement(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	constexpr auto parse_fn = create_parse_fn<
		lex::token_pos, ctx::parse_context,
		global_statement_parsers, &default_global_statement_parser
	>();
	if (stream == end)
	{
		context.report_error(stream, "expected a statement");
		return {};
	}
	else
	{
		return parse_fn(stream, end, context);
	}
}

ast::statement parse_local_statement(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	constexpr auto parse_fn = create_parse_fn<
		lex::token_pos, ctx::parse_context,
		local_statement_parsers, &default_local_statement_parser
	>();
	if (stream == end)
	{
		context.report_error(stream, "expected a statement");
		return {};
	}
	else
	{
		return parse_fn(stream, end, context);
	}
}

static constexpr auto parse_local_statement_without_semi_colon_default_parser = +[](
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	auto const begin = stream;
	auto const result = ast::make_stmt_expression(
		parse_expression_without_semi_colon(stream, end, context)
	);
	if (stream == begin)
	{
		context.report_error(stream);
		++stream;
	}
	return result;
};

ast::statement parse_local_statement_without_semi_colon(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	constexpr auto parse_fn = create_parse_fn<
		lex::token_pos, ctx::parse_context,
		local_statement_parsers, parse_local_statement_without_semi_colon_default_parser
	>();
	if (stream == end)
	{
		context.report_error(stream, "expected a statement");
		return {};
	}
	else
	{
		return parse_fn(stream, end, context);
	}
}

bz::vector<ast::statement> parse_global_statements(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz::vector<ast::statement> result = {};
	while (stream != end)
	{
		result.emplace_back(parse_global_statement(stream, end, context));
	}
	return result;
}

bz::vector<ast::statement> parse_local_statements(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz::vector<ast::statement> result = {};
	while (stream != end)
	{
		result.emplace_back(parse_local_statement(stream, end, context));
	}
	return result;
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

static void apply_attribute(
	ast::decl_variable &var_decl,
	ast::attribute const &attribute,
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
		var_decl.is_used = true;
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

static void apply_attribute(
	ast::decl_function &func_decl,
	ast::attribute const &attribute,
	ctx::parse_context &context
)
{
	if (attribute.name->value == "extern")
	{
		if (attribute.args.size() != 1)
		{
			context.report_error(attribute.name, "@extern expects exactly one argument");
			return;
		}
		auto const [type, _] = attribute.args[0].get_expr_type_and_kind();
		auto const type_without_const = ast::remove_const_or_consteval(type);
		if (
			!type_without_const.is<ast::ts_base_type>()
			|| type_without_const.get<ast::ts_base_type>().info->kind != ast::type_info::str_
		)
		{
			context.report_error(attribute.args[0], "argument of @extern must have type 'str'");
			return;
		}

		auto const extern_name = attribute.args[0]
			.get<ast::constant_expression>().value
			.get<ast::constant_value::string>().as_string_view();
		func_decl.body.symbol_name = extern_name;
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

static void apply_attribute(
	ast::decl_operator &op_decl,
	ast::attribute const &attribute,
	ctx::parse_context &context
)
{
	if (attribute.name->value == "extern")
	{
		if (attribute.args.size() != 1)
		{
			context.report_error(attribute.name, "@extern expects exactly one argument");
			return;
		}
		auto const [type, _] = attribute.args[0].get_expr_type_and_kind();
		auto const type_without_const = ast::remove_const_or_consteval(type);
		if (
			!type_without_const.is<ast::ts_base_type>()
			|| type_without_const.get<ast::ts_base_type>().info->kind != ast::type_info::str_
		)
		{
			context.report_error(attribute.args[0], "argument of @extern must have type 'str'");
			return;
		}

		auto const extern_name = attribute.args[0]
			.get<ast::constant_expression>().value
			.get<ast::constant_value::string>().as_string_view();
		op_decl.body.symbol_name = extern_name;
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
	ast::statement &stmt,
	ast::attribute const &attribute,
	ctx::parse_context &context
)
{
	stmt.visit([&](auto &s) {
		apply_attribute(s, attribute, context);
	});
}

static void resolve_attributes(
	ast::statement &stmt,
	ctx::parse_context &context
)
{
	auto &attributes = stmt.get_attributes();
	for (auto &attribute : attributes)
	{
		bz_assert(attribute.args.size() == 0);
		auto [stream, end] = attribute.arg_tokens;
		attribute.args = parse_expression_comma_list(stream, end, context);
		if (stream != end)
		{
			context.report_error({ stream, stream, end });
		}

		for (auto &arg : attribute.args)
		{
			if (arg.not_null() && !arg.is<ast::constant_expression>())
			{
				context.report_error(arg, "attribute argument must be a constant expression");
			}
		}

		apply_attribute(stmt, attribute, context);
	}
}

void resolve_global_statement(
	ast::statement &stmt,
	ctx::parse_context &context
)
{
	stmt.visit(bz::overload{
		[&](ast::decl_function &func_decl) {
			context.add_to_resolve_queue({}, func_decl.body);
			resolve_function(func_decl.body, context);
			context.pop_resolve_queue();
		},
		[&](ast::decl_operator &op_decl) {
			context.add_to_resolve_queue({}, op_decl.body);
			resolve_function(op_decl.body, context);
			context.pop_resolve_queue();
		},
		[&](ast::decl_variable &var_decl) {
			context.add_to_resolve_queue({}, var_decl);
			resolve_decl_variable(var_decl, context);
			context.pop_resolve_queue();
		},
		[&](ast::stmt_static_assert &static_assert_stmt) {
			resolve_stmt_static_assert(static_assert_stmt, context);
		},
		[&](auto &) {
			bz_unreachable;
		}
	});
	resolve_attributes(stmt, context);
}

} // namespace parse
