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

static bz::u8string get_static_assert_expression(ast::constant_expression const &cond)
{
	auto const get_const_expr_value_string = [](ast::constant_value const &value) -> bz::u8string {
		switch (value.kind())
		{
		case ast::constant_value::sint:
			return bz::format("{}", value.get<ast::constant_value::sint>());
		case ast::constant_value::uint:
			return bz::format("{}", value.get<ast::constant_value::uint>());
		case ast::constant_value::float32:
			return bz::format("{}", value.get<ast::constant_value::float32>());
		case ast::constant_value::float64:
			return bz::format("{}", value.get<ast::constant_value::float64>());
		case ast::constant_value::u8char:
			return bz::format("'{:c}'", value.get<ast::constant_value::u8char>());
		case ast::constant_value::string:
			return bz::format("\"{}\"", value.get<ast::constant_value::string>());
		case ast::constant_value::boolean:
			return bz::format("{}", value.get<ast::constant_value::boolean>());
		case ast::constant_value::null:
			return "null";
		case ast::constant_value::array:
		case ast::constant_value::tuple:
		case ast::constant_value::function:
		case ast::constant_value::function_set_id:
			return "";
		case ast::constant_value::type:
			return bz::format("{}", value.get<ast::constant_value::type>());
		case ast::constant_value::aggregate:
		default:
			return "";
		}
	};

	if (cond.expr.is<ast::expr_binary_op>())
	{
		auto const &binary_op = *cond.expr.get<ast::expr_binary_op_ptr>();
		switch (binary_op.op->kind)
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
			auto const op_str = token_info[binary_op.op->kind].token_value;
			auto const &lhs = binary_op.lhs;
			bz_assert(lhs.is<ast::constant_expression>());
			auto const lhs_str = get_const_expr_value_string(lhs.get<ast::constant_expression>().value);
			if (lhs_str == "")
			{
				return "";
			}
			auto const &rhs = binary_op.rhs;
			bz_assert(rhs.is<ast::constant_expression>());
			auto const rhs_str = get_const_expr_value_string(rhs.get<ast::constant_expression>().value);
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
		return get_const_expr_value_string(cond.value);
	}
	else
	{
		return "";
	}
}

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
		auto const expression_string = get_static_assert_expression(cond_const_expr);
		bz::u8string error_message = "static assertion failed";
		if (expression_string != "")
		{
			error_message += bz::format(" due to requirement '{}'", expression_string);
		}
		if (static_assert_stmt.message.not_null())
		{
			auto &message_const_expr = static_assert_stmt.message.get<ast::constant_expression>();
			bz_assert(message_const_expr.value.kind() == ast::constant_value::string);
			auto const message = message_const_expr.value.get<ast::constant_value::string>().as_string_view();
			error_message += bz::format(", message: '{}'", message);
		}
		context.report_error(static_assert_stmt.condition, std::move(error_message));
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
	auto const args = get_expression_tokens_without_error<
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
	if (!ts.is<ast::ts_unresolved>())
	{
		return;
	}
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
	else if (stream != end)
	{
		result.return_type = ast::make_void_typespec(stream);
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
		result.flags |= ast::function_body::external_linkage;
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

ast::statement parse_decl_import(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream != end);
	bz_assert(stream->kind == lex::token::kw_import);
	++stream; // import

	auto const id = context.assert_token(stream, lex::token::identifier);
	context.assert_token(stream, lex::token::semi_colon);
	return ast::make_decl_import(id);
}

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

ast::statement parse_export_statement(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream != end);
	bz_assert(stream->kind == lex::token::kw_export);
	++stream; // 'export'
	auto const after_export_token = stream;

	auto result = parse_global_statement(stream, end, context);
	if (result.not_null())
	{
		result.visit(bz::overload{
			[](ast::decl_function &func_decl) {
				func_decl.body.flags |= ast::function_body::module_export;
				func_decl.body.flags |= ast::function_body::external_linkage;
			},
			[](ast::decl_operator &op_decl) {
				op_decl.body.flags |= ast::function_body::module_export;
				op_decl.body.flags |= ast::function_body::external_linkage;
			},
			[&](auto const &) {
				context.report_error(after_export_token, "only a function or an operator can be exported");
			}
		});
	}
	return result;
}

ast::statement parse_local_export_statement(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream != end);
	bz_assert(stream->kind == lex::token::kw_export);
	context.report_error(stream, "'export' is not allowed for local declarations");
	++stream; // 'export'
	return parse_local_statement(stream, end, context);
}

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
		context.report_error(
			stream,
			stream->kind == lex::token::eof
			? "expected a statement before end-of-file"
			: "expected a statement"
		);
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

static void apply_extern(
	ast::function_body &func_body,
	ast::attribute const &attribute,
	ctx::parse_context &context
)
{
	bz_assert(attribute.name->value == "extern");
	if (attribute.args.size() != 1 && attribute.args.size() != 2)
	{
		context.report_error(attribute.name, "@extern expects one or two arguments");
		return;
	}

	bool good = true;
	// symbol name
	{
		auto const [type, _] = attribute.args[0].get_expr_type_and_kind();
		auto const type_without_const = ast::remove_const_or_consteval(type);
		if (
			!type_without_const.is<ast::ts_base_type>()
			|| type_without_const.get<ast::ts_base_type>().info->kind != ast::type_info::str_
		)
		{
			context.report_error(attribute.args[0], "symbol name in @extern must have type 'str'");
			good = false;
		}
	}

	// calling convention
	if (attribute.args.size() == 2)
	{
		auto const [type, _] = attribute.args[1].get_expr_type_and_kind();
		auto const type_without_const = ast::remove_const_or_consteval(type);
		if (
			!type_without_const.is<ast::ts_base_type>()
			|| type_without_const.get<ast::ts_base_type>().info->kind != ast::type_info::str_
		)
		{
			context.report_error(attribute.args[0], "calling convention in @extern must have type 'str'");
			good = false;
		}
	}

	if (!good)
	{
		return;
	}

	auto const extern_name = attribute.args[0]
		.get<ast::constant_expression>().value
		.get<ast::constant_value::string>().as_string_view();
	auto const cc = [&]() {
		if (attribute.args.size() != 2)
		{
			return abi::calling_convention::c;
		}
		auto const cc_name = attribute.args[1]
			.get<ast::constant_expression>().value
			.get<ast::constant_value::string>().as_string_view();
		if (cc_name == "c")
		{
			return abi::calling_convention::c;
		}
		else if (cc_name == "std")
		{
			return abi::calling_convention::std;
		}
		else if (cc_name == "fast")
		{
			return abi::calling_convention::fast;
		}
		else
		{
			context.report_error(
				attribute.args[1],
				bz::format("unknown calling convention '{}'", cc_name)
			);
			return abi::calling_convention::c;
		}
	}();
	func_body.flags |= ast::function_body::external_linkage;
	func_body.symbol_name = extern_name;
	func_body.cc = cc;
}

struct intrinsic_info_t
{
	bz::u8string_view name;
	bz::u8string_view symbol_name;
	uint32_t kind;
};

constexpr auto intrinsic_info = []() {
	using T = intrinsic_info_t;

	std::array result = {
		T{ "memcpy_i32",  "llvm.memcpy.p0i8.p0i8.i32",  ast::function_body::memcpy  },
		T{ "memcpy_i64",  "llvm.memcpy.p0i8.p0i8.i64",  ast::function_body::memcpy  },
		T{ "memmove_i32", "llvm.memmove.p0i8.p0i8.i32", ast::function_body::memmove },
		T{ "memmove_i64", "llvm.memmove.p0i8.p0i8.i64", ast::function_body::memmove },
		T{ "memset_i32",  "llvm.memset.p0i8.i32",       ast::function_body::memset  },
		T{ "memset_i64",  "llvm.memset.p0i8.i64",       ast::function_body::memset  },

		T{ "exp_f32",   "llvm.exp.f32",   ast::function_body::exp_f32   },
		T{ "exp_f64",   "llvm.exp.f64",   ast::function_body::exp_f64   },
		T{ "exp2_f32",  "llvm.exp2.f32",  ast::function_body::exp2_f32  },
		T{ "exp2_f64",  "llvm.exp2.f64",  ast::function_body::exp2_f64  },
		T{ "expm1_f32", "expm1f",         ast::function_body::expm1_f32 },
		T{ "expm1_f64", "expm1",          ast::function_body::expm1_f64 },
		T{ "log_f32",   "llvm.log.f32",   ast::function_body::log_f32   },
		T{ "log_f64",   "llvm.log.f64",   ast::function_body::log_f64   },
		T{ "log10_f32", "llvm.log10.f32", ast::function_body::log10_f32 },
		T{ "log10_f64", "llvm.log10.f64", ast::function_body::log10_f64 },
		T{ "log2_f32",  "llvm.log2.f32",  ast::function_body::log2_f32  },
		T{ "log2_f64",  "llvm.log2.f64",  ast::function_body::log2_f64  },
		T{ "log1p_f32", "log1pf",         ast::function_body::log1p_f32 },
		T{ "log1p_f64", "log1p",          ast::function_body::log1p_f64 },

		T{ "pow_f32",   "llvm.pow.f32",  ast::function_body::pow_f32   },
		T{ "pow_f64",   "llvm.pow.f64",  ast::function_body::pow_f64   },
		T{ "sqrt_f32",  "llvm.sqrt.f32", ast::function_body::sqrt_f32  },
		T{ "sqrt_f64",  "llvm.sqrt.f64", ast::function_body::sqrt_f64  },
		T{ "cbrt_f32",  "cbrtf",         ast::function_body::cbrt_f32  },
		T{ "cbrt_f64",  "cbrt",          ast::function_body::cbrt_f64  },
		T{ "hypot_f32", "hypotf",        ast::function_body::hypot_f32 },
		T{ "hypot_f64", "hypot",         ast::function_body::hypot_f64 },

		T{ "sin_f32",   "llvm.sin.f32", ast::function_body::sin_f32   },
		T{ "sin_f64",   "llvm.sin.f64", ast::function_body::sin_f64   },
		T{ "cos_f32",   "llvm.cos.f32", ast::function_body::cos_f32   },
		T{ "cos_f64",   "llvm.cos.f64", ast::function_body::cos_f64   },
		T{ "tan_f32",   "tanf",         ast::function_body::tan_f32   },
		T{ "tan_f64",   "tan",          ast::function_body::tan_f64   },
		T{ "asin_f32",  "asinf",        ast::function_body::asin_f32  },
		T{ "asin_f64",  "asin",         ast::function_body::asin_f64  },
		T{ "acos_f32",  "acosf",        ast::function_body::acos_f32  },
		T{ "acos_f64",  "acos",         ast::function_body::acos_f64  },
		T{ "atan_f32",  "atanf",        ast::function_body::atan_f32  },
		T{ "atan_f64",  "atan",         ast::function_body::atan_f64  },
		T{ "atan2_f32", "atan2f",       ast::function_body::atan2_f32 },
		T{ "atan2_f64", "atan2",        ast::function_body::atan2_f64 },

		T{ "sinh_f32",  "sinhf",  ast::function_body::sinh_f32  },
		T{ "sinh_f64",  "sinh",   ast::function_body::sinh_f64  },
		T{ "cosh_f32",  "coshf",  ast::function_body::cosh_f32  },
		T{ "cosh_f64",  "cosh",   ast::function_body::cosh_f64  },
		T{ "tanh_f32",  "tanhf",  ast::function_body::tanh_f32  },
		T{ "tanh_f64",  "tanh",   ast::function_body::tanh_f64  },
		T{ "asinh_f32", "asinhf", ast::function_body::asinh_f32 },
		T{ "asinh_f64", "asinh",  ast::function_body::asinh_f64 },
		T{ "acosh_f32", "acoshf", ast::function_body::acosh_f32 },
		T{ "acosh_f64", "acosh",  ast::function_body::acosh_f64 },
		T{ "atanh_f32", "atanhf", ast::function_body::atanh_f32 },
		T{ "atanh_f64", "atanh",  ast::function_body::atanh_f64 },

		T{ "erf_f32",    "erff",    ast::function_body::erf_f32    },
		T{ "erf_f64",    "erf",     ast::function_body::erf_f64    },
		T{ "erfc_f32",   "erfcf",   ast::function_body::erfc_f32   },
		T{ "erfc_f64",   "erfc",    ast::function_body::erfc_f64   },
		T{ "tgamma_f32", "tgammaf", ast::function_body::tgamma_f32 },
		T{ "tgamma_f64", "tgamma",  ast::function_body::tgamma_f64 },
		T{ "lgamma_f32", "lgammaf", ast::function_body::lgamma_f32 },
		T{ "lgamma_f64", "lgamma",  ast::function_body::lgamma_f64 },
	};

	return result;
}();

static void apply_intrinsic(
	ast::function_body &func_body,
	ast::attribute const &attribute,
	ctx::parse_context &context
)
{
	bz_assert(attribute.name->value == "intrinsic");
	if (attribute.args.size() != 1)
	{
		context.report_error(attribute.name, "@intrinsic expects exactly one argument");
		return;
	}

	// symbol name
	{
		auto const [type, _] = attribute.args[0].get_expr_type_and_kind();
		auto const type_without_const = ast::remove_const_or_consteval(type);
		if (
			!type_without_const.is<ast::ts_base_type>()
			|| type_without_const.get<ast::ts_base_type>().info->kind != ast::type_info::str_
		)
		{
			context.report_error(attribute.args[0], "name in @intrinsic must have type 'str'");
			return;
		}
	}

	if ((func_body.flags & ast::function_body::intrinsic) != 0)
	{
		context.report_error(attribute.name, "declaration has already been marked as @intrinsic");
		return;
	}

	auto const name = attribute.args[0]
		.get<ast::constant_expression>().value
		.get<ast::constant_value::string>().as_string_view();

	auto const info = std::find_if(
		intrinsic_info.begin(), intrinsic_info.end(),
		[&](auto const &info) {
			return name == info.name;
		}
	);

	if (info == intrinsic_info.end())
	{
		context.report_error(
			attribute.args[0],
			bz::format("unknown intrinsic '{}'", name)
		);
		return;
	}

	func_body.flags |= ast::function_body::intrinsic;
	func_body.intrinsic_kind = info->kind;
	func_body.symbol_name = info->symbol_name;
	func_body.cc = abi::calling_convention::c;
}

static void apply_attribute(
	ast::decl_function &func_decl,
	ast::attribute const &attribute,
	ctx::parse_context &context
)
{
	auto const attr_name = attribute.name->value;
	if (attr_name == "extern")
	{
		apply_extern(func_decl.body, attribute, context);
	}
	else if (attr_name == "intrinsic")
	{
		apply_intrinsic(func_decl.body, attribute, context);
	}
	else if (attr_name == "cdecl")
	{
		if (attribute.args.size() != 0)
		{
			context.report_error(
				{ attribute.arg_tokens.begin, attribute.arg_tokens.begin, attribute.arg_tokens.end },
				"@cdecl expects no arguments"
			);
		}
		if (func_decl.body.cc != abi::calling_convention::bozon)
		{
			context.report_error(
				attribute.name,
				"calling convention has already been set for this function"
			);
		}
		else
		{
			func_decl.body.cc = abi::calling_convention::c;
		}
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
	ast::attribute const &attribute,
	ctx::parse_context &context
)
{
	auto const attr_name = attribute.name->value;
	if (attr_name == "extern")
	{
		apply_extern(op_decl.body, attribute, context);
	}
	else if (attr_name == "intrinsic")
	{
		apply_intrinsic(op_decl.body, attribute, context);
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
		[](ast::decl_import) {
			// nothing
		},
		[&](auto &) {
			bz_unreachable;
		}
	});
	resolve_attributes(stmt, context);
}

} // namespace parse
