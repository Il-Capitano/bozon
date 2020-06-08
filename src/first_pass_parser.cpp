#include "first_pass_parser.h"


static ast::declaration parse_function_definition(
	lex::token_pos &stream, lex::token_pos end,
	ctx::first_pass_parse_context &context
);

static ast::declaration parse_operator_definition(
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
	if (curly_end == end || curly_end->kind != lex::token::curly_close)
	{
		context.report_error(
			stream,
			stream->kind == lex::token::eof
			? bz::u8string("expected closing } before end-of-file")
			: bz::format("expected closing } before '{}'", stream->value),
			{ ctx::make_note(curly_begin, "to match this:") }
		);
	}
	else
	{
		++stream;
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

		switch (stream->kind)
		{
		// literals
		case lex::token::identifier:
		case lex::token::integer_literal:
		case lex::token::floating_point_literal:
		case lex::token::hex_literal:
		case lex::token::oct_literal:
		case lex::token::bin_literal:
		case lex::token::string_literal:
		case lex::token::character_literal:
		case lex::token::kw_true:
		case lex::token::kw_false:
		case lex::token::kw_null:
		// parenthesis/brackets
		case lex::token::paren_open:
		case lex::token::paren_close:
		case lex::token::square_open:
		case lex::token::square_close:
		// type specifiers that are not operators
		case lex::token::colon:
		case lex::token::kw_auto:
		case lex::token::kw_const:
		case lex::token::kw_function:
		// etc
		case lex::token::fat_arrow:
		case lex::token::kw_as:
			return true;

		default:
			return lex::is_operator(stream->kind);
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
			if (stream->kind != lex::token::paren_close)
			{
				auto const suggested_paren_pos = [&]() {
					switch (stream->kind)
					{
					case lex::token::square_close:
					case lex::token::semi_colon:
						return stream->src_pos.begin;
					default:
						return (stream - 1)->src_pos.end;
					}
				}();
				context.report_error(
					stream,
					stream->kind == lex::token::eof
					? bz::u8string("expected closing ) before end-of-file")
					: bz::format("expected closing ) before '{}'", stream->value),
					{ context.make_note(paren_begin, "to match this:", suggested_paren_pos, ")") }
				);
			}
			else
			{
				++stream;
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
			if (stream->kind != lex::token::square_close)
			{
				auto const suggested_square_pos = [&]() {
					switch (stream->kind)
					{
					case lex::token::paren_close:
					case lex::token::semi_colon:
						return stream->src_pos.begin;
					default:
						return (stream - 1)->src_pos.end;
					}
				}();
				context.report_error(
					stream,
					stream->kind == lex::token::eof
					? bz::u8string("expected closing ] before end-of-file")
					: bz::format("expected closing ] before '{}'", stream->value),
					{ context.make_note(square_begin, "to match this:", suggested_square_pos, "]") }
				);
			}
			else
			{
				++stream;
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
				ast::typespec(),
				ast::make_ts_unresolved(type, type)
			);
		}
		else
		{
			params.emplace_back(
				lex::token_range{ id, type.end },
				nullptr,
				ast::typespec(),
				ast::make_ts_unresolved(type, type)
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
		if (stream->kind == lex::token::paren_close)
		{
			++stream;
			return cond;
		}
		else
		{
			if (open_paren->kind == lex::token::paren_open)
			{
				context.report_error(
					stream,
					stream->kind == lex::token::eof
					? bz::u8string("expected closing ) before end-of-file")
					: bz::format("expected closing ) before '{}'", stream->value),
					{ ctx::make_note(open_paren, "to match this:") }
				);
			}
			return cond;
		}
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
		if (stream->kind == lex::token::paren_close)
		{
			++stream;
			return cond;
		}
		else
		{
			if (open_paren->kind == lex::token::paren_open)
			{
				context.report_error(
					stream,
					stream->kind == lex::token::eof
					? bz::u8string("expected closing ) before end-of-file")
					: bz::format("expected closing ) before '{}'", stream->value),
					{ ctx::make_note(open_paren, "to match this:") }
				);
			}
			return cond;
		}
	}();

	auto while_block = parse_statement(stream, end, context);

	return ast::make_stmt_while(
		lex::token_range{ begin_token, stream },
		ast::make_unresolved_expression({ condition.begin, condition.begin, condition.end }),
		std::move(while_block)
	);
}

static ast::statement parse_for_statement(
	lex::token_pos &stream, lex::token_pos,
	ctx::first_pass_parse_context &
)
{
	bz_assert(stream->kind == lex::token::kw_for);
	bz_assert(false, "for statement not yet implemented");
	return {};
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
		expr.begin == expr.end ? ast::expression() : ast::make_unresolved_expression({
			expr.begin, expr.begin, expr.end
		})
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

static ast::declaration parse_variable_declaration(
	lex::token_pos &stream, lex::token_pos end,
	ctx::first_pass_parse_context &context
)
{
	bz_assert(stream->kind == lex::token::kw_let || stream->kind == lex::token::kw_const);
	auto const tokens_begin = stream;
	ast::typespec prototype;
	if (stream->kind == lex::token::kw_const)
	{
		prototype = ast::make_ts_constant({ stream, stream + 1}, stream, ast::typespec());
	}
	++stream; // 'let' or 'const'

	auto const get_base = [](ast::typespec *ts) -> ast::typespec *
	{
		switch (ts->kind())
		{
		case ast::typespec::index<ast::ts_constant>:
			return &ts->get<ast::ts_constant_ptr>()->base;
		case ast::typespec::index<ast::ts_reference>:
			return &ts->get<ast::ts_reference_ptr>()->base;
		case ast::typespec::index<ast::ts_pointer>:
			return &ts->get<ast::ts_pointer_ptr>()->base;
		default:
			bz_assert(false);
			return nullptr;
		}
	};

	auto const add_to_prototype = [&](auto &&ts)
	{
		static_assert(
			bz::meta::is_same<ast::typespec, bz::meta::remove_cv_reference<decltype(ts)>>
		);
		ast::typespec *innermost = &prototype;
		ast::typespec *one_up = &prototype;
		while (innermost->not_null())
		{
			one_up = innermost;
			innermost = get_base(innermost);
		}
		if (
			ts.kind() == ast::typespec::index<ast::ts_reference>
			&& innermost != &prototype
		)
		{
			context.report_error(
				tokens_begin->kind == lex::token::kw_const
				? tokens_begin : tokens_begin + 1,
				stream, stream + 1,
				"reference specifier must be at top level"
			);
		}
		else if (
			ts.kind() == ast::typespec::index<ast::ts_constant>
			&& one_up->kind() == ast::typespec::index<ast::ts_constant>
		)
		{
			context.report_error(
				stream - 1, stream, stream + 1,
				"too many const specifiers"
			);
		}
		else
		{
			*innermost = std::forward<decltype(ts)>(ts);
		}
	};

	{
		bool loop = true;
		while (stream != end && loop)
		{
			switch (stream->kind)
			{
			case lex::token::kw_const:
				add_to_prototype(ast::make_ts_constant({ stream, stream + 1}, stream, ast::typespec()));
				++stream;
				break;
			case lex::token::ampersand:
				add_to_prototype(ast::make_ts_reference({ stream, stream + 1}, stream, ast::typespec()));
				++stream;
				break;
			case lex::token::star:
				add_to_prototype(ast::make_ts_pointer({ stream, stream + 1}, stream, ast::typespec()));
				++stream;
				break;
			default:
				loop = false;
				break;
			}
		}
	}

	auto id = context.assert_token(stream, lex::token::identifier);

	if (stream->kind == lex::token::colon)
	{
		++stream; // ':'
		auto type_tokens = get_expression_or_type_tokens<
			lex::token::assign, lex::token::semi_colon
		>(stream, end, context);

		auto type = ast::make_ts_unresolved(type_tokens, type_tokens);
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

static ast::declaration parse_struct_definition(
	lex::token_pos &in_stream, lex::token_pos in_end,
	ctx::first_pass_parse_context &context
)
{
	bz_assert(in_stream->kind == lex::token::kw_struct);
	++in_stream; // 'struct'

	auto const id = context.assert_token(in_stream, lex::token::identifier);
	context.assert_token(in_stream, lex::token::curly_open);
	auto [stream, end] = get_tokens_in_curly<lex::token::curly_close>(in_stream, in_end, context);

	bz::vector<ast::declaration> member_decls = {};

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
				ast::make_ts_unresolved(type, type)
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

static ast::declaration parse_function_definition(
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
		return ast::make_decl_function(id, std::move(params), ast::make_ts_unresolved(ret_type, ret_type));
	}

	if (stream->kind != lex::token::curly_open)
	{
		// if there's no opening curly bracket, we assume it was meant as a function declaration
		context.report_error(stream, "expected opening { or ';'");
		return ast::make_decl_function(id, std::move(params), ast::make_ts_unresolved(ret_type, ret_type));
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
		ast::make_ts_unresolved(ret_type, ret_type),
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
		if (op->kind != lex::token::paren_open && !lex::is_overloadable_unary_operator(op->kind))
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
		if (op->kind != lex::token::paren_open && !lex::is_overloadable_binary_operator(op->kind))
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

static ast::declaration parse_operator_definition(
	lex::token_pos &stream, lex::token_pos end,
	ctx::first_pass_parse_context &context
)
{
	bz_assert(stream->kind == lex::token::kw_operator);
	++stream; // 'operator'
	auto const op = stream;
	bool is_valid_op = true;
	if (!lex::is_operator(op->kind))
	{
		context.report_error(stream, "expected an operator");
		is_valid_op = false;
	}
	else
	{
		if (!lex::is_overloadable_operator(op->kind))
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
		return ast::make_decl_operator(op, std::move(params), ast::make_ts_unresolved(ret_type, ret_type));
	}

	if (stream->kind != lex::token::curly_open)
	{
		// if there's no opening curly bracket, we assume it was meant as a function declaration
		context.report_error(stream, "expected opening { or ';'");
		return ast::make_decl_operator(op, std::move(params), ast::make_ts_unresolved(ret_type, ret_type));
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
		ast::make_ts_unresolved(ret_type, ret_type),
		std::move(body)
	);
}

ast::declaration parse_declaration(lex::token_pos &stream, lex::token_pos end, ctx::first_pass_parse_context &context)
{
	switch (stream->kind)
	{
	// variable declaration
	case lex::token::kw_let:
	case lex::token::kw_const:
		return parse_variable_declaration(stream, end, context);

	// struct definition
	case lex::token::kw_struct:
		return parse_struct_definition(stream, end, context);

	// function definition
	case lex::token::kw_function:
		return parse_function_definition(stream, end, context);

	// operator definition
	case lex::token::kw_operator:
		return parse_operator_definition(stream, end, context);

	default:
	{
		auto begin = stream;
		while (
			stream != end
			&& stream->kind != lex::token::kw_let
			&& stream->kind != lex::token::kw_const
			&& stream->kind != lex::token::kw_struct
			&& stream->kind != lex::token::kw_function
			&& stream->kind != lex::token::kw_operator
		)
		{
			++stream;
		}
		context.report_error(begin, begin, stream, "expected a declaration");
		return parse_declaration(stream, end, context);
	}
	}
}

ast::statement parse_statement(lex::token_pos &stream, lex::token_pos end, ctx::first_pass_parse_context &context)
{
	switch (stream->kind)
	{
	// if statement
	case lex::token::kw_if:
		return parse_if_statement(stream, end, context);

	// while statement
	case lex::token::kw_while:
		return parse_while_statement(stream, end, context);

	// for statement
	case lex::token::kw_for:
		return parse_for_statement(stream, end, context);

	// return statement
	case lex::token::kw_return:
		return parse_return_statement(stream, end, context);

	// no-op statement
	case lex::token::semi_colon:
		return parse_no_op_statement(stream, end, context);

	// compound statement
	case lex::token::curly_open:
		return ast::make_statement(get_stmt_compound_ptr(stream, end, context));

	// variable declaration
	case lex::token::kw_let:
	case lex::token::kw_const:
		return parse_variable_declaration(stream, end, context);

	// struct definition
	case lex::token::kw_struct:
		return parse_struct_definition(stream, end, context);

	// function definition
	case lex::token::kw_function:
		return parse_function_definition(stream, end, context);

	// operator definition
	case lex::token::kw_operator:
		return parse_operator_definition(stream, end, context);

	// expression statement
	default:
		return parse_expression_statement(stream, end, context);
	}
}
