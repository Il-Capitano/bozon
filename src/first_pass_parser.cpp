#include "first_pass_parser.h"
#include "token_info.h"

static ast::statement parse_function_definition(
	lex::token_pos &stream, lex::token_pos end,
	ctx::first_pass_parse_context &context
);

static ast::statement parse_operator_definition(
	lex::token_pos &stream, lex::token_pos end,
	ctx::first_pass_parse_context &context
);


template<uint32_t ...end_tokens>
static lex::token_range get_tokens_in_curly(
	lex::token_pos &stream, lex::token_pos end,
	ctx::first_pass_parse_context &context
)
{
	bz_assert((stream - 1)->kind == lex::token::curly_open);
	auto const curly_begin = stream - 1;
	auto const begin = stream;

	auto is_valid_token = [&]()
	{
		if (stream == end || stream->kind == lex::token::eof || (
			(stream->kind == end_tokens)
			|| ...
		))
		{
			return false;
		}
		return true;
	};


	while (stream != end && is_valid_token())
	{
		if (stream->kind == lex::token::curly_open)
		{
			++stream; // '{'
			get_tokens_in_curly<lex::token::curly_close>(stream, end, context);
		}
		else
		{
			++stream;
		}
	}

	auto const curly_end = stream;
	if (stream != end && stream->kind == lex::token::curly_close)
	{
		++stream;
	}
	else
	{
		context.report_paren_match_error(stream, curly_begin);
	}

	return { begin, curly_end };
}


template<uint32_t ...end_tokens>
static lex::token_range get_expression_or_type_tokens(
	lex::token_pos &stream, lex::token_pos end,
	ctx::first_pass_parse_context &context
)
{
	auto begin = stream;

	auto is_valid_expr_token = [&]()
	{
		if (stream == end || stream->kind == lex::token::eof || (
			(stream->kind == end_tokens)
			|| ...
		))
		{
			return false;
		}
		else
		{
			return is_valid_expression_or_type_token(stream->kind);
		}
	};

	while (stream != end && is_valid_expr_token())
	{
		switch (stream->kind)
		{
		case lex::token::paren_open:
		{
			auto const paren_begin = stream;
			++stream; // '('
			get_expression_or_type_tokens<
				lex::token::paren_close, lex::token::square_close
			>(stream, end, context);
			if (stream != end && stream->kind == lex::token::paren_close)
			{
				++stream;
			}
			else
			{
				context.report_paren_match_error(stream, paren_begin);
			}
			break;
		}

		case lex::token::square_open:
		{
			auto const square_begin = stream;
			++stream; // '['
			get_expression_or_type_tokens<
				lex::token::paren_close, lex::token::square_close
			>(stream, end, context);
			if (stream != end && stream->kind == lex::token::square_close)
			{
				++stream;
			}
			else
			{
				context.report_paren_match_error(stream, square_begin);
			}
			break;
		}

		case lex::token::fat_arrow:
		{
			++stream; // '=>'
			if (stream == end || stream->kind != lex::token::curly_open)
			{
				break;
			}
			++stream; // '{'
			get_tokens_in_curly<lex::token::curly_close>(stream, end, context);
			break;
		}

		case lex::token::paren_close:
		{
			context.report_error(stream, "stray )");
			++stream; // ')'
			break;
		}

		case lex::token::square_close:
		{
			context.report_error(stream, "stray ]");
			++stream; // ']'
			break;
		}

		default:
			++stream;
			break;
		}
	}

	return { begin, stream };
}

static bz::vector<ast::decl_variable> get_function_params(
	lex::token_pos &in_stream, lex::token_pos in_end,
	ctx::first_pass_parse_context &context
)
{
	context.assert_token(in_stream, lex::token::paren_open);
	bz::vector<ast::decl_variable> params = {};

	if (in_stream != in_end && in_stream->kind == lex::token::paren_close)
	{
		++in_stream;
		return params;
	}

	auto [stream, end] = get_expression_or_type_tokens<lex::token::paren_close>(
		in_stream, in_end, context
	);

	if (stream == end)
	{
		context.assert_token(in_stream, lex::token::paren_close);
		return params;
	}

	do
	{
		auto id = stream;
		if (id->kind == lex::token::identifier)
		{
			++stream;
		}

		if (stream->kind != lex::token::colon)
		{
			context.report_error(stream, "expected identifier or ':'");
		}
		else
		{
			++stream;
		}

		auto type = get_expression_or_type_tokens<lex::token::paren_close, lex::token::comma>(stream, end, context);

		if (id->kind == lex::token::identifier)
		{
			params.emplace_back(
				lex::token_range{ id, type.end },
				id,
				lex::token_range{},
				ast::make_unresolved_typespec(type)
			);
		}
		else
		{
			params.emplace_back(
				lex::token_range{ id, type.end },
				nullptr,
				lex::token_range{},
				ast::make_unresolved_typespec(type)
			);
		}
	} while (
		stream != end
		&& stream->kind == lex::token::comma
		&& (++stream, true) // skip comma
	);

	in_stream = stream;
	context.assert_token(in_stream, lex::token::paren_close);
	return params;
}

/*
static ast::stmt_compound get_stmt_compound(
	lex::token_pos &in_stream, lex::token_pos in_end,
	ctx::first_pass_parse_context &context
)
{
	bz_assert(in_stream != in_end);
	bz_assert(in_stream->kind == lex::token::curly_open);
	auto comp_stmt = ast::stmt_compound(lex::token_range{ in_stream, in_stream });
	++in_stream; // '{'

	auto [stream, end] = get_tokens_in_curly<lex::token::curly_close>(in_stream, in_end, context);

	while (stream != end)
	{
		comp_stmt.statements.emplace_back(parse_statement(stream, end, context));
	}
	comp_stmt.tokens.end = in_stream;

	return comp_stmt;
}
*/

static ast::stmt_compound_ptr get_stmt_compound_ptr(
	lex::token_pos &in_stream, lex::token_pos in_end,
	ctx::first_pass_parse_context &context
)
{
	bz_assert(in_stream != in_end);
	bz_assert(in_stream->kind == lex::token::curly_open);
	auto comp_stmt = std::make_unique<ast::stmt_compound>(lex::token_range{ in_stream, in_stream });
	++in_stream; // '{'

	auto [stream, end] = get_tokens_in_curly<lex::token::curly_close>(in_stream, in_end, context);

	while (stream != end)
	{
		comp_stmt->statements.emplace_back(parse_statement(stream, end, context));
	}
	comp_stmt->tokens.end = in_stream;

	return comp_stmt;
}

static ast::statement parse_if_statement(
	lex::token_pos &stream, lex::token_pos end,
	ctx::first_pass_parse_context &context
)
{
	bz_assert(stream->kind == lex::token::kw_if);
	auto begin_token = stream;
	++stream; // 'if'

	auto const condition = [&]()
	{
		auto const open_paren = context.assert_token(stream, lex::token::paren_open);
		auto const cond = get_expression_or_type_tokens<lex::token::paren_close>(stream, end, context);
		if (stream != end && stream->kind == lex::token::paren_close)
		{
			++stream;
		}
		else if (open_paren->kind == lex::token::paren_open)
		{
			context.report_paren_match_error(stream, open_paren);
		}
		return cond;
	}();

	auto if_block = parse_statement(stream, end, context);

	if (stream->kind == lex::token::kw_else)
	{
		++stream; // 'else'
		auto else_block = parse_statement(stream, end, context);

		return ast::make_stmt_if(
			lex::token_range{ begin_token, stream },
			ast::make_unresolved_expression({ condition.begin, condition.begin, condition.end }),
			std::move(if_block),
			std::move(else_block)
		);
	}
	else
	{
		return ast::make_stmt_if(
			lex::token_range{ begin_token, stream },
			ast::make_unresolved_expression({ condition.begin, condition.begin, condition.end }),
			std::move(if_block)
		);
	}
}

static ast::statement parse_while_statement(
	lex::token_pos &stream, lex::token_pos end,
	ctx::first_pass_parse_context &context
)
{
	bz_assert(stream->kind == lex::token::kw_while);
	auto const begin_token = stream;
	++stream; // 'while'

	auto const condition = [&]()
	{
		auto const open_paren = context.assert_token(stream, lex::token::paren_open);
		auto const cond = get_expression_or_type_tokens<lex::token::paren_close>(stream, end, context);
		if (stream != end && stream->kind == lex::token::paren_close)
		{
			++stream;
		}
		else if (open_paren->kind == lex::token::paren_open)
		{
			context.report_paren_match_error(stream, open_paren);
		}
		return cond;
	}();

	auto while_block = parse_statement(stream, end, context);

	return ast::make_stmt_while(
		lex::token_range{ begin_token, stream },
		ast::make_unresolved_expression({ condition.begin, condition.begin, condition.end }),
		std::move(while_block)
	);
}

static ast::statement parse_for_statement(
	lex::token_pos &stream, lex::token_pos end,
	ctx::first_pass_parse_context &context
)
{
	bz_assert(stream->kind == lex::token::kw_for);

	++stream; // 'for'
	auto const open_paren = context.assert_token(stream, lex::token::paren_open);
	if (stream == end)
	{
		context.report_error(stream, "expected initialization statement or ';'");
		return ast::statement();
	}
	auto init = stream->kind == lex::token::semi_colon
		? (++stream, ast::statement())
		: parse_statement(stream, end, context);
	if (stream == end)
	{
		context.report_error(stream, "expected loop condition or ';'");
		return ast::statement();
	}
	auto condition = stream->kind == lex::token::semi_colon
		? (++stream, ast::expression({}))
		: [&]() {
			auto const expr = get_expression_or_type_tokens<lex::token::semi_colon>(stream, end, context);
			context.assert_token(stream, lex::token::semi_colon);
			return ast::make_unresolved_expression({ expr.begin, expr.begin, expr.end });
		}();
	if (stream == end)
	{
		context.report_error(stream, "expected iteration expression or closing )");
		return ast::statement();
	}
	auto iteration = stream->kind == lex::token::paren_close
		? ast::expression({})
		: [&]() {
			auto const expr = get_expression_or_type_tokens<lex::token::paren_close>(stream, end, context);
			return ast::make_unresolved_expression({ expr.begin, expr.begin, expr.end });
		}();

	if (stream != end && stream->kind == lex::token::paren_close)
	{
		++stream;
	}
	else if (open_paren->kind == lex::token::paren_open)
	{
		context.report_paren_match_error(stream, open_paren);
	}

	auto for_body = parse_statement(stream, end, context);

	return ast::make_stmt_for(
		std::move(init),
		std::move(condition),
		std::move(iteration),
		std::move(for_body)
	);
}

static ast::statement parse_return_statement(
	lex::token_pos &stream, lex::token_pos end,
	ctx::first_pass_parse_context &context
)
{
	bz_assert(stream->kind == lex::token::kw_return);
	auto const begin_token = stream;
	++stream; // 'return'

	auto expr = get_expression_or_type_tokens<lex::token::semi_colon>(stream, end, context);
	context.assert_token(stream, lex::token::semi_colon);

	return ast::make_stmt_return(
		lex::token_range{ begin_token, stream },
		expr.begin == expr.end
			? ast::expression({ expr.begin, expr.begin, expr.end})
			: ast::make_unresolved_expression({ expr.begin, expr.begin, expr.end })
	);
}

static ast::statement parse_no_op_statement(
	lex::token_pos &stream, lex::token_pos,
	ctx::first_pass_parse_context &
)
{
	bz_assert(stream->kind == lex::token::semi_colon);
	auto const begin_token = stream;
	++stream; // ';'
	return ast::make_stmt_no_op(lex::token_range{ begin_token, stream });
}

static ast::statement parse_compound_statement(
	lex::token_pos &stream, lex::token_pos end,
	ctx::first_pass_parse_context &context
)
{
	bz_assert(stream->kind == lex::token::curly_open);
	return ast::make_statement(get_stmt_compound_ptr(stream, end, context));
}

static ast::statement parse_static_assert_statement(
	lex::token_pos &stream, lex::token_pos end,
	ctx::first_pass_parse_context &context
)
{
	bz_assert(stream->kind == lex::token::kw_static_assert);
	++stream;

	auto const paren_begin = context.assert_token(stream, lex::token::paren_open);
	auto const args_range = get_expression_or_type_tokens<lex::token::paren_close>(stream, end, context);
	if (stream != end && stream->kind == lex::token::paren_close)
	{
		++stream;
	}
	else if (paren_begin->kind == lex::token::paren_open)
	{
		context.report_paren_match_error(stream, paren_begin);
	}
	context.assert_token(stream, lex::token::semi_colon);

	return ast::make_stmt_static_assert(args_range);
}

static ast::statement parse_expression_statement(
	lex::token_pos &stream, lex::token_pos end,
	ctx::first_pass_parse_context &context
)
{
	bz_assert(stream != end);
	auto const begin_token = stream;
	auto const expr = get_expression_or_type_tokens<lex::token::semi_colon>(stream, end, context);
	if (expr.begin == expr.end) // && stream->kind != lex::token::semi_colon (should always be true)
	{
		context.report_error(stream);
		++stream;
		if (stream == end)
		{
			return ast::statement();
		}
		else
		{
			return parse_statement(stream, end, context);
		}
	}
	context.assert_token(stream, lex::token::semi_colon);

	return ast::make_stmt_expression(
		lex::token_range{ begin_token, stream },
		ast::make_unresolved_expression({ expr.begin, expr.begin, expr.end })
	);
}

static ast::statement parse_variable_declaration(
	lex::token_pos &stream, lex::token_pos end,
	ctx::first_pass_parse_context &context
)
{
	bz_assert(
		stream->kind == lex::token::kw_let
		|| stream->kind == lex::token::kw_const
		|| stream->kind == lex::token::kw_consteval
	);
	auto const tokens_begin = stream;
	if (stream->kind == lex::token::kw_let)
	{
		++stream; // 'let'
	}

	auto const prototype_begin = stream;

	while (stream != end && is_unary_type_op(stream->kind))
	{
		++stream;
	}

	auto const prototype = lex::token_range{ prototype_begin, stream };

	auto id = context.assert_token(stream, lex::token::identifier);

	if (stream->kind == lex::token::colon)
	{
		++stream; // ':'
		auto type_tokens = get_expression_or_type_tokens<
			lex::token::assign, lex::token::semi_colon
		>(stream, end, context);

		auto type = ast::make_unresolved_typespec(type_tokens);
		if (stream->kind == lex::token::semi_colon)
		{
			++stream; // ';'
			auto const tokens_end = stream;
			return ast::make_decl_variable(
				lex::token_range{tokens_begin, tokens_end},
				id, prototype, type
			);
		}

		context.assert_token(stream, lex::token::assign, lex::token::semi_colon);

		auto init = get_expression_or_type_tokens<lex::token::semi_colon>(stream, end, context);

		context.assert_token(stream, lex::token::semi_colon);
		auto const tokens_end = stream;
		return ast::make_decl_variable(
			lex::token_range{tokens_begin, tokens_end},
			id,
			prototype,
			type,
			ast::make_unresolved_expression({ init.begin, init.begin, init.end })
		);
	}
	else if (stream->kind == lex::token::assign)
	{
		++stream;
	}
	else
	{
		context.report_error(stream, "expected '=' or ':'");
	}

	auto init = get_expression_or_type_tokens<lex::token::semi_colon>(stream, end, context);

	context.assert_token(stream, lex::token::semi_colon);
	auto const tokens_end = stream;
	return ast::make_decl_variable(
		lex::token_range{tokens_begin, tokens_end},
		id,
		prototype,
		ast::make_unresolved_expression({ init.begin, init.begin, init.end })
	);
}

/*
static ast::statement parse_struct_definition(
	lex::token_pos &in_stream, lex::token_pos in_end,
	ctx::first_pass_parse_context &context
)
{
	bz_assert(in_stream->kind == lex::token::kw_struct);
	++in_stream; // 'struct'

	auto const id = context.assert_token(in_stream, lex::token::identifier);
	context.assert_token(in_stream, lex::token::curly_open);
	auto [stream, end] = get_tokens_in_curly<lex::token::curly_close>(in_stream, in_end, context);

	bz::vector<ast::statement> member_decls = {};

	auto parse_member_decl = [&](auto &stream, auto end)
	{
		switch (stream->kind)
		{
		case lex::token::kw_function:
			member_decls.emplace_back(parse_function_definition(stream, end, context));
			return;
		case lex::token::kw_operator:
			member_decls.emplace_back(parse_operator_definition(stream, end, context));
			return;
		case lex::token::identifier:
		{
			auto const id = stream;
			++stream;
			context.assert_token(stream, lex::token::colon);
			auto type = get_expression_or_type_tokens<lex::token::semi_colon>(stream, end, context);
			auto const end_token = stream;
			context.assert_token(stream, lex::token::semi_colon);
			member_decls.emplace_back(ast::make_decl_variable(
				lex::token_range{ id, end_token },
				id,
				ast::typespec(),
				ast::make_ts_unresolved({ type.begin, type.begin, type.end }, type)
			));
			return;
		}
		default:
			context.report_error(stream);
			++stream;
			return;
		}
	};

	while (stream != end)
	{
		parse_member_decl(stream, end);
	}

	return ast::make_decl_struct(id, std::move(member_decls));
}
*/

static ast::statement parse_function_definition(
	lex::token_pos &stream, lex::token_pos end,
	ctx::first_pass_parse_context &context
)
{
	bz_assert(stream->kind == lex::token::kw_function);
	++stream; // 'function'
	auto id = context.assert_token(stream, lex::token::identifier);

	auto params = get_function_params(stream, end, context);

	context.assert_token(stream, lex::token::arrow);
	auto ret_type = get_expression_or_type_tokens<lex::token::curly_open, lex::token::semi_colon>(stream, end, context);

	if (stream->kind == lex::token::semi_colon)
	{
		++stream; // ';'
		return ast::make_decl_function(id, std::move(params), ast::make_unresolved_typespec(ret_type));
	}

	if (stream->kind != lex::token::curly_open)
	{
		// if there's no opening curly bracket, we assume it was meant as a function declaration
		context.report_error(stream, "expected opening { or ';'");
		return ast::make_decl_function(id, std::move(params), ast::make_unresolved_typespec(ret_type));
	}

	++stream; // '{'

	bz::vector<ast::statement> body = {};

	auto [body_stream, body_end] = get_tokens_in_curly<lex::token::curly_close>(stream, end, context);
	while (body_stream != body_end && body_stream->kind != lex::token::curly_close)
	{
		body.emplace_back(parse_statement(body_stream, body_end, context));
	}

	return ast::make_decl_function(
		id,
		std::move(params),
		ast::make_unresolved_typespec(ret_type),
		std::move(body)
	);
}

static void check_operator_param_count(
	lex::token_pos op, lex::token_pos params_end, size_t param_count,
	ctx::first_pass_parse_context &context
)
{
	if (param_count == 0)
	{
		bz::u8string_view operator_name;

		if (op->kind == lex::token::paren_open)
		{
			operator_name = "()";
		}
		else if (op->kind == lex::token::square_open)
		{
			operator_name = "[]";
		}
		else
		{
			operator_name = op->value;
		}

		context.report_error(
			op - 1, op, params_end,
			bz::format("operator {} cannot take 0 arguments", operator_name)
		);
	}
	if (param_count == 1)
	{
		if (op->kind != lex::token::paren_open && !is_unary_overloadable_operator(op->kind))
		{
			bz::u8string_view const operator_name = op->kind == lex::token::square_open ? "[]" : op->value;
			context.report_error(
				op - 1, op, params_end,
				bz::format("operator {} cannot take 1 argument", operator_name)
			);
		}
	}
	else if (param_count == 2)
	{
		if (op->kind != lex::token::paren_open && !is_binary_overloadable_operator(op->kind))
		{
			context.report_error(
				op - 1, op, params_end,
				bz::format("operator {} cannot take 2 arguments", op->value)
			);
		}
	}
	else if (op->kind != lex::token::paren_open)
	{
		bz::u8string_view const operator_name = op->kind == lex::token::square_open ? "[]" : op->value;
		context.report_error(
			op - 1, op, params_end,
			bz::format("operator {} cannot take {} arguments", operator_name, param_count)
		);
	}
}

static ast::statement parse_operator_definition(
	lex::token_pos &stream, lex::token_pos end,
	ctx::first_pass_parse_context &context
)
{
	bz_assert(stream->kind == lex::token::kw_operator);
	++stream; // 'operator'
	auto const op = stream;
	bool is_valid_op = true;
	if (!is_operator(op->kind))
	{
		context.report_error(stream, "expected an operator");
		is_valid_op = false;
	}
	else
	{
		if (!is_overloadable_operator(op->kind))
		{
			context.report_error(
				stream, bz::format("operator {} is not overloadable", stream->value)
			);
			is_valid_op = false;
		}
		++stream;
		if (op->kind == lex::token::paren_open)
		{
			context.assert_token(stream, lex::token::paren_close);
		}
		else if (op->kind == lex::token::square_open)
		{
			context.assert_token(stream, lex::token::square_close);
		}
	}

	auto params = get_function_params(stream, end, context);

	if (is_valid_op)
	{
		check_operator_param_count(op, stream, params.size(), context);
	}

	context.assert_token(stream, lex::token::arrow);
	auto ret_type = get_expression_or_type_tokens<lex::token::curly_open, lex::token::semi_colon>(stream, end, context);

	if (stream->kind == lex::token::semi_colon)
	{
		++stream; // ';'
		return ast::make_decl_operator(op, std::move(params), ast::make_unresolved_typespec(ret_type));
	}

	if (stream->kind != lex::token::curly_open)
	{
		// if there's no opening curly bracket, we assume it was meant as a function declaration
		context.report_error(stream, "expected opening { or ';'");
		return ast::make_decl_operator(op, std::move(params), ast::make_unresolved_typespec(ret_type));
	}

	++stream; // '{'

	bz::vector<ast::statement> body = {};

	auto [body_stream, body_end] = get_tokens_in_curly<lex::token::curly_close>(stream, end, context);
	while (body_stream != body_end && body_stream->kind != lex::token::curly_close)
	{
		body.emplace_back(parse_statement(body_stream, body_end, context));
	}

	return ast::make_decl_operator(
		op,
		std::move(params),
		ast::make_unresolved_typespec(ret_type),
		std::move(body)
	);
}

static ast::statement parse_const_declaration(
	lex::token_pos &stream, lex::token_pos end,
	ctx::first_pass_parse_context &context
)
{
	return parse_variable_declaration(stream, end, context);
}

static ast::statement parse_consteval_declaration(
	lex::token_pos &stream, lex::token_pos end,
	ctx::first_pass_parse_context &context
)
{
	return parse_variable_declaration(stream, end, context);
}

template<bool is_top_level>
static ast::statement parse_statement_with_attribute(
	lex::token_pos &stream, lex::token_pos end,
	ctx::first_pass_parse_context &context
)
{
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
			auto const args_range = get_expression_or_type_tokens<lex::token::paren_close>(stream, end, context);
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

	constexpr auto parse_fn = is_top_level ? parse_top_level_statement : parse_statement;
	auto stmt = parse_fn(stream, end, context);
	stmt.set_attributes(std::move(attributes));
	return stmt;
}

static ast::statement default_top_level_parser(
	lex::token_pos &stream, lex::token_pos end,
	ctx::first_pass_parse_context &context
);

using statement_parse_fn_t = ast::statement (*)(lex::token_pos &stream, lex::token_pos end, ctx::first_pass_parse_context &context);

struct statement_parser
{
	uint32_t kind;
	statement_parse_fn_t parse_fn;
	bool is_top_level;
	bool is_declaration;
};

constexpr auto statement_parsers = []() {
	std::array result = {
		statement_parser{ lex::token::kw_if,            &parse_if_statement,            ast::is_top_level_statement_type<ast::stmt_if>,            ast::is_declaration_type<ast::stmt_if>            },
		statement_parser{ lex::token::kw_while,         &parse_while_statement,         ast::is_top_level_statement_type<ast::stmt_while>,         ast::is_declaration_type<ast::stmt_while>         },
		statement_parser{ lex::token::kw_for,           &parse_for_statement,           ast::is_top_level_statement_type<ast::stmt_for>,           ast::is_declaration_type<ast::stmt_for>           },
		statement_parser{ lex::token::kw_return,        &parse_return_statement,        ast::is_top_level_statement_type<ast::stmt_return>,        ast::is_declaration_type<ast::stmt_return>        },
		statement_parser{ lex::token::semi_colon,       &parse_no_op_statement,         ast::is_top_level_statement_type<ast::stmt_no_op>,         ast::is_declaration_type<ast::stmt_no_op>         },
		statement_parser{ lex::token::curly_open,       &parse_compound_statement,      ast::is_top_level_statement_type<ast::stmt_compound>,      ast::is_declaration_type<ast::stmt_compound>      },
		statement_parser{ lex::token::kw_static_assert, &parse_static_assert_statement, ast::is_top_level_statement_type<ast::stmt_static_assert>, ast::is_declaration_type<ast::stmt_static_assert> },
		statement_parser{ lex::token::kw_let,           &parse_variable_declaration,    ast::is_top_level_statement_type<ast::decl_variable>,      ast::is_declaration_type<ast::decl_variable>      },
	//	statement_parser{ lex::token::kw_struct,        &parse_struct_definition,       ast::is_top_level_statement_type<ast::decl_struct>,        ast::is_declaration_type<ast::decl_struct>        },
		statement_parser{ lex::token::kw_function,      &parse_function_definition,     ast::is_top_level_statement_type<ast::decl_function>,      ast::is_declaration_type<ast::decl_function>      },
		statement_parser{ lex::token::kw_operator,      &parse_operator_definition,     ast::is_top_level_statement_type<ast::decl_operator>,      ast::is_declaration_type<ast::decl_operator>      },
		statement_parser{ lex::token::_last,            &parse_expression_statement,    ast::is_top_level_statement_type<ast::stmt_expression>,    ast::is_declaration_type<ast::stmt_expression>    },

		statement_parser{ lex::token::kw_const,         &parse_const_declaration,     true, true },
		statement_parser{ lex::token::kw_consteval,     &parse_consteval_declaration, true, true },

		statement_parser{ lex::token::at,               &parse_statement_with_attribute<false>, false, false },
	};

	constexpr_bubble_sort(
		result,
		[](auto const &lhs, auto const rhs) {
			return lhs.kind < rhs.kind;
		},
		[](auto &lhs, auto &rhs) {
			auto const tmp = lhs;
			lhs = rhs;
			rhs = tmp;
		}
	);
	return result;
}();

constexpr auto top_level_statement_parsers = []() {
	auto const get_count = []() {
		size_t count = 0;
		for (auto &parser : statement_parsers)
		{
			if (parser.is_top_level)
			{
				++count;
			}
		}
		return count;
	};
	// + 2 because of the deafult parser and the attribute parser
	using result_t = std::array<statement_parser, get_count() + 2>;

	result_t result{};

	size_t i = 0;
	for (auto &parser : statement_parsers)
	{
		if (parser.is_top_level)
		{
			result[i] = parser;
			++i;
		}
	}
	result[i] = { lex::token::at, &parse_statement_with_attribute<true>, true, true };
	++i;
	result[i] = { lex::token::_last, &default_top_level_parser, true, false };
	++i;
	bz_assert(i == result.size());

	constexpr_bubble_sort(
		result,
		[](auto const &lhs, auto const rhs) {
			return lhs.kind < rhs.kind;
		},
		[](auto &lhs, auto &rhs) {
			auto const tmp = lhs;
			lhs = rhs;
			rhs = tmp;
		}
	);
	return result;
}();

static ast::statement default_top_level_parser(
	lex::token_pos &stream, lex::token_pos end,
	ctx::first_pass_parse_context &context
)
{
	bz_assert(stream != end);
	bz_assert(
		[&]<size_t ...Ns>(bz::meta::index_sequence<Ns...>) {
			return ((
				!statement_parsers[Ns].is_top_level
				|| stream->kind != statement_parsers[Ns].kind
			) && ...);
		}(bz::meta::make_index_sequence<statement_parsers.size()>{})
	);
	auto begin = stream;
	while (
		stream != end
		&& [&]<size_t ...Ns>(bz::meta::index_sequence<Ns...>) {
			return ((
				!statement_parsers[Ns].is_top_level
				|| stream->kind != statement_parsers[Ns].kind
			) && ...);
		}(bz::meta::make_index_sequence<statement_parsers.size()>{})
	)
	{
		++stream;
	}
	context.report_error(begin, begin, stream, "expected a top level statement");
	if (stream == end)
	{
		return ast::statement();
	}
	else
	{
		return parse_top_level_statement(stream, end, context);
	}
}


ast::statement parse_statement(
	lex::token_pos &stream, lex::token_pos end,
	ctx::first_pass_parse_context &context
)
{
	if (stream == end)
	{
		context.report_error(stream, "expected a statement");
		return ast::statement();
	}
	ast::statement result;
	auto const stream_kind = stream->kind;
	[&]<size_t ...Ns>(bz::meta::index_sequence<Ns...>) {
		bool good = false;
		((
			// lex::token::_last represents the default statement parser
			stream_kind == statement_parsers[Ns].kind
			? (void)((result = statement_parsers[Ns].parse_fn(stream, end, context)), good = true)
			: (void)0
		), ...);
		if (!good)
		{
			constexpr auto parse_fn = statement_parsers.back().parse_fn;
			result = parse_fn(stream, end, context);
		}
	}(bz::meta::make_index_sequence<statement_parsers.size() - 1>{}); // - 1 because of the default parser function
	return result;
}

ast::statement parse_top_level_statement(
	lex::token_pos &stream, lex::token_pos end,
	ctx::first_pass_parse_context &context
)
{
	if (stream == end)
	{
		context.report_error(stream, "expected a top level statement");
		return ast::statement();
	}
	ast::statement result;
	auto const stream_kind = stream->kind;
	[&]<size_t ...Ns>(bz::meta::index_sequence<Ns...>) {
		bool good = false;
		((
			// lex::token::_last represents the default statement parser
			stream_kind == top_level_statement_parsers[Ns].kind
			? (void)((result = top_level_statement_parsers[Ns].parse_fn(stream, end, context)), good = true)
			: (void)0
		), ...);
		if (!good)
		{
			constexpr auto default_parse_fn = top_level_statement_parsers.back().parse_fn;
			result = default_parse_fn(stream, end, context);
		}
	}(bz::meta::make_index_sequence<top_level_statement_parsers.size() - 1>{}); // - 1 because of the default parser function
	return result;
}
