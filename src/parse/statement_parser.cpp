#include "statement_parser.h"
#include "token_info.h"
#include "expression_parser.h"
#include "parse_common.h"

namespace parse
{

// parse functions can't be static, because they are referenced in parse_common.h

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
	return ast::make_stmt_static_assert(args);
}

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
		lex::token::comma,
		lex::token::colon,
		lex::token::paren_close,
		lex::token::square_close
	>(stream, end, context);

	return { prototype, id, type };
}

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
	if (stream != end && stream->kind == lex::token::equals)
	{
		++stream; // '='
		auto const init_expr = get_expression_tokens<>(stream, end, context);
		return ast::make_decl_variable(
			lex::token_range{ begin, stream },
			id, prototype,
			ast::make_unresolved_typespec(type),
			ast::make_unresolved_expression({ init_expr.begin, init_expr.begin, init_expr.end })
		);
	}
	else
	{
		return ast::make_decl_variable(
			lex::token_range{ begin, stream },
			id, prototype,
			ast::make_unresolved_typespec(type)
		);
	}
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
		context.report_error(
			{ stream, stream, end },
			end - stream == 1
			? "unexpected token"
			: "unexpected tokens"
		);
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
	auto type = parse_expression(stream, end, context, no_assign, true);
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
			context.report_error(
				{ stream, stream, end },
				end - stream == 1
				? "unexpected token"
				: "unexpected tokens"
			);
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

	if (func_body.symbol_name == "" && func_body.function_name == "main")
	{
		func_body.symbol_name = "main";
	}
	else if (func_body.symbol_name == "")
	{
		func_body.symbol_name = func_body.get_symbol_name();
	}
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
		context.assert_token(stream, lex::token::curly_open, lex::token::semi_colon);
	}
	else
	{
		++stream; // ';'
	}

	return result;
}

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

	return ast::make_decl_function(id, std::move(body));
}

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

	return ast::make_decl_operator(op, std::move(body));
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
	statement.set_attributes(std::move(attributes));
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
		? (++stream, ast::expression())
		: parse_expression(stream, end, context, precedence{}, true);
	if (context.assert_token(stream, lex::token::semi_colon)->kind != lex::token::semi_colon)
	{
		get_expression_tokens<
			lex::token::curly_open,
			lex::token::kw_if,
			lex::token::paren_close
		>(stream, end, context);
	}

	if (stream == end)
	{
		context.report_error(stream, "expected iteration expression or closing )");
		return {};
	}
	auto iteration = stream->kind == lex::token::semi_colon
		? (++stream, ast::expression())
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

ast::statement parse_stmt_expression_without_semi_colon(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	switch (stream->kind)
	{
	// top level compound expression
	case lex::token::curly_open:
		return ast::make_stmt_expression(parse_compound_expression(stream, end, context));
	// top level if expression
	case lex::token::kw_if:
		bz_assert(false);
		return {};
	default:
		return ast::make_stmt_expression(parse_expression(stream, end, context, precedence{}, true));
	}
}

void consume_semi_colon_at_end_of_expression(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context,
	ast::expression const &expression
)
{
	if (!expression.is_constant_or_dynamic())
	{
		context.assert_token(stream, lex::token::semi_colon);
		return;
	}

	auto &expr = expression.get_expr();
	expr.visit(bz::overload{
		[&](ast::expr_compound const &compound_expr) {
			if (compound_expr.final_expr.src_tokens.begin != nullptr)
			{
				auto dummy_stream = compound_expr.final_expr.src_tokens.end;
				consume_semi_colon_at_end_of_expression(
					dummy_stream, end, context,
					compound_expr.final_expr
				);
			}
		},
		[&](auto const &) {
			context.assert_token(stream, lex::token::semi_colon);
		}
	});
}

ast::statement parse_stmt_expression(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	auto expr = parse_stmt_expression_without_semi_colon(stream, end, context);
	bz_assert(expr.is<ast::stmt_expression>());
	consume_semi_colon_at_end_of_expression(
		stream, end, context,
		expr.get<ast::stmt_expression_ptr>()->expr
	);
	return expr;
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

ast::statement parse_local_statement_without_semi_colon(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	constexpr auto parse_fn = create_parse_fn<
		lex::token_pos, ctx::parse_context,
		local_statement_parsers, &parse_stmt_expression_without_semi_colon
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

void resolve_global_statement(
	ast::statement &stmt,
	ctx::parse_context &context
)
{
	stmt.visit(bz::overload{
		[&](ast::decl_function &func_decl) {
			context.add_to_resolve_queue({}, func_decl.body);
			resolve_function(func_decl.body, context);
		},
		[&](ast::decl_operator &op_decl) {
			context.add_to_resolve_queue({}, op_decl.body);
			resolve_function(op_decl.body, context);
		},
		[&](auto &) {
			bz_assert(false);
		}
	});
}

} // namespace parse
