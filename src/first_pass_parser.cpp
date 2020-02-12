#include "first_pass_parser.h"


template<typename T>
static void append_vector(bz::vector<T> &base, bz::vector<T> new_elems)
{
	base.reserve(base.size() + new_elems.size());
	for (auto &elem : new_elems)
	{
		base.emplace_back(std::move(elem));
	}
}

template<uint32_t ...end_tokens>
static token_range get_tokens_in_curly(
	token_pos &stream, token_pos end,
	bz::vector<error> &errors
)
{
	auto begin = stream;

	auto is_valid_token = [&]()
	{
		if (stream == end || stream->kind == token::eof || (
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
		switch (stream->kind)
		{
		case token::paren_open:
		{
			++stream; // '('
			get_tokens_in_curly<token::paren_close, token::curly_close>(stream, end, errors);
			if (stream != end && stream->kind != token::eof)
			{
				assert(stream->kind == token::paren_close || stream->kind == token::curly_close);
				if (stream->kind == token::paren_close)
				{
					++stream;
				}
			}
			break;
		}

		case token::square_open:
		{
			++stream; // '['
			get_tokens_in_curly<token::square_close, token::curly_close>(stream, end, errors);
			if (stream != end && stream->kind != token::eof)
			{
				assert(stream->kind == token::square_close || stream->kind == token::curly_close);
				if (stream->kind == token::square_close)
				{
					++stream;
				}
			}
			break;
		}

		case token::curly_open:
		{
			auto const curly_begin = stream;
			++stream; // '{'
			get_tokens_in_curly<token::curly_close>(stream, end, errors);
			if (stream->kind != token::curly_close)
			{
				errors.emplace_back(bad_token(
					stream,
					stream->kind == token::eof
					? bz::string("expected closing } before end-of-file")
					: bz::format("expected closing } before '{}'", stream->value),
					{ make_note(curly_begin, "to match this:") }
				));
			}
			else
			{
				++stream;
			}
			break;
		}

		default:
			++stream;
			break;
		}
	}

	return { begin, stream };
}


template<uint32_t ...end_tokens>
static token_range get_expression_or_type_tokens(
	token_pos &stream, token_pos end,
	bz::vector<error> &errors
)
{
	auto begin = stream;

	auto is_valid_expr_token = [&]()
	{
		if (stream == end || stream->kind == token::eof || (
			(stream->kind == end_tokens)
			|| ...
		))
		{
			return false;
		}

		switch (stream->kind)
		{
		// literals
		case token::identifier:
		case token::number_literal:
		case token::string_literal:
		case token::character_literal:
		case token::kw_true:
		case token::kw_false:
		case token::kw_null:
		// parenthesis/brackets
		case token::paren_open:
		case token::paren_close:
		case token::curly_open:
		case token::square_open:
		case token::square_close:
		// type specifiers that are not operators
		case token::colon:
		case token::kw_auto:
		case token::kw_const:
		case token::kw_function:
		// etc
		case token::fat_arrow:
			return true;

		default:
			return is_operator(stream->kind);
		}
	};


	while (stream != end && is_valid_expr_token())
	{
		switch (stream->kind)
		{
		case token::paren_open:
		{
			auto const paren_begin = stream;
			++stream; // '('
			get_expression_or_type_tokens<
				token::paren_close, token::square_close
			>(stream, end, errors);
			if (stream->kind != token::paren_close)
			{
				errors.emplace_back(bad_token(
					stream,
					stream->kind == token::eof
					? bz::string("expected closing ) before end-of-file")
					: bz::format("expected closing ) before '{}'", stream->value),
					{ make_note(paren_begin, "to match this:") }
				));
			}
			else
			{
				++stream;
			}
			break;
		}

		case token::square_open:
		{
			auto const square_begin = stream;
			++stream; // '['
			get_expression_or_type_tokens<
				token::paren_close, token::square_close
			>(stream, end, errors);
			if (stream->kind != token::square_close)
			{
				errors.emplace_back(bad_token(
					stream,
					stream->kind == token::eof
					? bz::string("expected closing ] before end-of-file")
					: bz::format("expected closing ] before '{}'", stream->value),
					{ make_note(square_begin, "to match this:") }
				));
			}
			else
			{
				++stream;
			}
			break;
		}

		case token::curly_open:
		{
			auto const curly_begin = stream;
			++stream; // '{'
			get_tokens_in_curly<token::curly_close>(stream, end, errors);
			if (stream->kind != token::curly_close)
			{
				errors.emplace_back(bad_token(
					stream,
					stream->kind == token::eof
					? bz::string("expected closing } before end-of-file")
					: bz::format("expected closing } before '{}'", stream->value),
					{ make_note(curly_begin, "to match this:") }
				));
			}
			else
			{
				++stream;
			}
			break;
		}

		case token::paren_close:
		{
			errors.emplace_back(bad_token(stream, "stray )"));
			++stream; // ')'
			break;
		}

		case token::square_close:
		{
			errors.emplace_back(bad_token(stream, "stray ]"));
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

static bz::vector<ast::variable> get_function_params(
	token_pos &in_stream, token_pos in_end,
	bz::vector<error> &errors
)
{
	assert_token(in_stream, token::paren_open, errors);
	bz::vector<ast::variable> params;

	if (in_stream != in_end && in_stream->kind == token::paren_close)
	{
		++in_stream;
		return params;
	}

	auto [stream, end] = get_expression_or_type_tokens<token::paren_close>(
		in_stream, in_end, errors
	);

	if (stream == end)
	{
		assert_token(in_stream, token::paren_close, errors);
		return params;
	}

	do
	{
		auto id = stream;
		if (id->kind == token::identifier)
		{
			++stream;
		}

		assert_token(stream, token::colon, errors);
		auto type = get_expression_or_type_tokens<token::paren_close, token::comma>(stream, end, errors);

		if (id->kind == token::identifier)
		{
			params.push_back({
				id, ast::make_ts_unresolved(type)
			});
		}
		else
		{
			params.push_back({
				nullptr, ast::make_ts_unresolved(type)
			});
		}
	} while (
		stream != end
		&& stream->kind == token::comma
		&& (++stream, true)
	);

	in_stream = stream;
	assert_token(in_stream, token::paren_close, errors);
	return params;
}


static ast::stmt_compound get_stmt_compound(
	token_pos &in_stream, token_pos in_end,
	bz::vector<error> &errors
)
{
	assert(in_stream != in_end);
	assert(in_stream->kind == token::curly_open);
	auto comp_stmt = ast::stmt_compound(token_range{ in_stream, in_stream });
	++in_stream; // '{'

	auto [stream, end] = get_tokens_in_curly<token::curly_close>(in_stream, in_end, errors);
	if (in_stream->kind != token::curly_close)
	{
		errors.emplace_back(bad_token(
			in_stream,
			in_stream->kind == token::eof
			? bz::string("expected closing } before end-of-file")
			: bz::format("expected closing } before '{}'", in_stream->value),
			{ make_note(comp_stmt.tokens.begin, "to match this:") }
		));
	}
	else
	{
		++in_stream;
	}

	while (stream != end)
	{
		comp_stmt.statements.emplace_back(parse_statement(stream, end, errors));
	}
	comp_stmt.tokens.end = in_stream;

	return comp_stmt;
}

static ast::stmt_compound_ptr get_stmt_compound_ptr(
	token_pos &in_stream, token_pos in_end,
	bz::vector<error> &errors
)
{
	assert(in_stream != in_end);
	assert(in_stream->kind == token::curly_open);
	auto comp_stmt = std::make_unique<ast::stmt_compound>(token_range{ in_stream, in_stream });
	++in_stream; // '{'

	auto [stream, end] = get_tokens_in_curly<token::curly_close>(in_stream, in_end, errors);
	assert_token(in_stream, token::curly_close, errors);

	while (stream != end)
	{
		comp_stmt->statements.emplace_back(parse_statement(stream, end, errors));
	}
	comp_stmt->tokens.end = in_stream;

	return comp_stmt;
}

static ast::statement parse_if_statement(
	token_pos &stream, token_pos end,
	bz::vector<error> &errors
)
{
	assert(stream->kind == token::kw_if);
	auto begin_token = stream;
	++stream; // 'if'

	auto const condition = [&]()
	{
		auto const open_paren = assert_token(stream, token::paren_open, errors);
		auto const cond = get_expression_or_type_tokens<token::paren_close>(stream, end, errors);
		if (stream->kind == token::paren_close)
		{
			++stream;
			return cond;
		}
		else
		{
			if (open_paren->kind == token::paren_open)
			{
				errors.emplace_back(bad_token(
					stream,
					stream->kind == token::eof
					? bz::string("expected closing ) before end-of-file")
					: bz::format("expected closing ) before '{}'", stream->value),
					{ make_note(open_paren, "to match this:") }
				));
			}
			return cond;
		}
	}();

	auto if_block = parse_statement(stream, end, errors);

	if (stream->kind == token::kw_else)
	{
		++stream; // 'else'
		auto else_block = parse_statement(stream, end, errors);

		return ast::make_stmt_if(
			token_range{ begin_token, stream },
			ast::make_expr_unresolved(condition),
			std::move(if_block),
			std::move(else_block)
		);
	}
	else
	{
		return ast::make_stmt_if(
			token_range{ begin_token, stream },
			ast::make_expr_unresolved(condition),
			std::move(if_block)
		);
	}
}

static ast::statement parse_while_statement(
	token_pos &stream, token_pos end,
	bz::vector<error> &errors
)
{
	assert(stream->kind == token::kw_while);
	auto const begin_token = stream;
	++stream; // 'while'

	auto const condition = [&]()
	{
		auto const open_paren = assert_token(stream, token::paren_open, errors);
		auto const cond = get_expression_or_type_tokens<token::paren_close>(stream, end, errors);
		if (stream->kind == token::paren_close)
		{
			++stream;
			return cond;
		}
		else
		{
			if (open_paren->kind == token::paren_open)
			{
				errors.emplace_back(bad_token(
					stream,
					stream->kind == token::eof
					? bz::string("expected closing ) before end-of-file")
					: bz::format("expected closing ) before '{}'", stream->value),
					{ make_note(open_paren, "to match this:") }
				));
			}
			return cond;
		}
	}();

	auto while_block = parse_statement(stream, end, errors);

	return ast::make_stmt_while(
		token_range{ begin_token, stream },
		ast::make_expr_unresolved(condition),
		std::move(while_block)
	);
}

static ast::statement parse_for_statement(
	token_pos &stream, token_pos,
	bz::vector<error> &
)
{
	assert(stream->kind == token::kw_for);
	assert(false, "for statement not yet implemented");
}

static ast::statement parse_return_statement(
	token_pos &stream, token_pos end,
	bz::vector<error> &errors
)
{
	assert(stream->kind == token::kw_return);
	auto const begin_token = stream;
	++stream; // 'return'

	auto expr = get_expression_or_type_tokens<token::semi_colon>(stream, end, errors);
	assert_token(stream, token::semi_colon, errors);

	return ast::make_stmt_return(
		token_range{ begin_token, stream },
		ast::make_expr_unresolved(expr)
	);
}

static ast::statement parse_no_op_statement(
	token_pos &stream, token_pos,
	bz::vector<error> &
)
{
	assert(stream->kind == token::semi_colon);
	auto const begin_token = stream;
	++stream; // ';'
	return ast::make_stmt_no_op(token_range{ begin_token, stream });
}

static ast::statement parse_expression_statement(
	token_pos &stream, token_pos end,
	bz::vector<error> &errors
)
{
	assert(stream != end);
	auto const begin_token = stream;
	auto const expr = get_expression_or_type_tokens<token::semi_colon>(stream, end, errors);
	if (expr.begin == expr.end) // && stream->kind != token::semi_colon (should always be true)
	{
		errors.emplace_back(bad_token(stream));
		++stream;
		if (stream == end)
		{
			return ast::statement();
		}
		else
		{
			return parse_statement(stream, end, errors);
		}
	}
	assert_token(stream, token::semi_colon, errors);

	return ast::make_stmt_expression(
		token_range{ begin_token, stream },
		ast::make_expr_unresolved(expr)
	);
}

static ast::declaration parse_variable_declaration(
	token_pos &stream, token_pos end,
	bz::vector<error> &errors
)
{
	assert(stream->kind == token::kw_let || stream->kind == token::kw_const);
	auto const tokens_begin = stream;
	ast::typespec prototype;
	if (stream->kind == token::kw_const)
	{
		prototype = ast::make_ts_constant(ast::typespec());
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
			assert(false);
		}
	};

	auto const add_to_prototype = [&](auto &&ts)
	{
		static_assert(
			bz::meta::is_same<ast::typespec, bz::meta::remove_cv_reference<decltype(ts)>>
		);
		ast::typespec *innermost = &prototype;
		ast::typespec *one_up = &prototype;
		while (innermost->kind() != ast::typespec::null)
		{
			one_up = innermost;
			innermost = get_base(innermost);
		}
		if (
			ts.kind() == ast::typespec::index<ast::ts_reference>
			&& innermost != &prototype
		)
		{
			errors.emplace_back(bad_tokens(
				tokens_begin->kind == token::kw_const
				? tokens_begin : tokens_begin + 1,
				stream, stream + 1,
				"reference specifier must be at top level"
			));
		}
		else if (
			ts.kind() == ast::typespec::index<ast::ts_constant>
			&& one_up->kind() == ast::typespec::index<ast::ts_constant>
		)
		{
			errors.emplace_back(bad_tokens(
				stream - 1, stream, stream + 1,
				"too many const specifiers"
			));
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
			case token::kw_const:
				add_to_prototype(ast::make_ts_constant(ast::typespec()));
				++stream;
				break;
			case token::ampersand:
				add_to_prototype(ast::make_ts_reference(ast::typespec()));
				++stream;
				break;
			case token::star:
				add_to_prototype(ast::make_ts_pointer(ast::typespec()));
				++stream;
				break;
			default:
				loop = false;
				break;
			}
		}
	}

	auto id = assert_token(stream, token::identifier, errors);

	if (stream->kind == token::colon)
	{
		++stream; // ':'
		auto type_tokens = get_expression_or_type_tokens<
			token::assign, token::semi_colon
		>(stream, end, errors);

		auto type = ast::make_ts_unresolved(type_tokens);
		if (stream->kind == token::semi_colon)
		{
			++stream; // ';'
			auto const tokens_end = stream;
			return ast::make_decl_variable(
				token_range{tokens_begin, tokens_end},
				id, prototype, type
			);
		}

		assert_token(stream, token::assign, token::semi_colon, errors);

		auto init = get_expression_or_type_tokens<token::semi_colon>(stream, end, errors);

		assert_token(stream, token::semi_colon, errors);
		auto const tokens_end = stream;
		return ast::make_decl_variable(
			token_range{tokens_begin, tokens_end},
			id,
			prototype,
			type,
			ast::make_expr_unresolved(init)
		);
	}
	else if (stream->kind == token::assign)
	{
		++stream;
	}
	else
	{
		errors.emplace_back(bad_token(stream, "expected '=' or ':'"));
	}

	auto init = get_expression_or_type_tokens<token::semi_colon>(stream, end, errors);

	assert_token(stream, token::semi_colon, errors);
	auto const tokens_end = stream;
	return ast::make_decl_variable(
		token_range{tokens_begin, tokens_end},
		id,
		prototype,
		ast::make_expr_unresolved(init)
	);
}

static ast::declaration parse_struct_definition(
	token_pos &in_stream, token_pos in_end,
	bz::vector<error> &errors
)
{
	assert(in_stream->kind == token::kw_struct);
	++in_stream; // 'struct'

	auto const id = assert_token(in_stream, token::identifier, errors);
	assert_token(in_stream, token::curly_open, errors);
	auto [stream, end] = get_tokens_in_curly<token::curly_close>(in_stream, in_end, errors);

	if (in_stream->kind == token::curly_close)
	{
		++in_stream;
	}

	auto parse_member_variable = [&](auto &stream, auto end)
	{
		assert(stream->kind == token::identifier);
		auto const id = stream;
		++stream;
		assert_token(stream, token::colon, errors);
		auto const type = token_range{stream, end};
		stream = end;
		return ast::variable(
			id, ast::make_ts_unresolved(type)
		);
	};

	bz::vector<ast::variable> member_variables = {};
	while (stream != end)
	{
		auto [inner_stream, inner_end] = get_expression_or_type_tokens<
			token::semi_colon
		>(stream, end, errors);

		if (stream != end && stream->kind == token::semi_colon)
		{
			++stream;
		}

		while (inner_stream != inner_end && inner_stream->kind != token::identifier)
		{
			errors.emplace_back(bad_token(inner_stream));
			++inner_stream;
		}
		if (inner_stream != inner_end)
		{
			member_variables.emplace_back(parse_member_variable(inner_stream, inner_end));
		}
	}

	return ast::make_decl_struct(id, std::move(member_variables));
}

static ast::declaration parse_function_definition(
	token_pos &stream, token_pos end,
	bz::vector<error> &errors
)
{
	assert(stream->kind == token::kw_function);
	++stream; // 'function'
	auto id = assert_token(stream, token::identifier, errors);

	auto params = get_function_params(stream, end, errors);

	assert_token(stream, token::arrow, errors);
	auto ret_type = get_expression_or_type_tokens<token::curly_open>(stream, end, errors);

	return ast::make_decl_function(
		id,
		std::move(params),
		ast::make_ts_unresolved(ret_type),
		get_stmt_compound(stream, end, errors)
	);
}

static ast::declaration parse_operator_definition(
	token_pos &stream, token_pos end,
	bz::vector<error> &errors
)
{
	assert(stream->kind == token::kw_operator);
	++stream; // 'operator'
	auto op = stream;
	if (!is_overloadable_operator(op->kind))
	{
		errors.emplace_back(bad_token(stream, "expected overloadable operator"));
	}

	++stream;
	if (op->kind == token::paren_open)
	{
		assert_token(stream, token::paren_close, errors);
	}
	else if (op->kind == token::square_open)
	{
		assert_token(stream, token::square_close, errors);
	}

	auto params = get_function_params(stream, end, errors);

	if (params.size() == 0)
	{
		bz::string operator_name;

		if (op->kind == token::paren_open)
		{
			operator_name = "()";
		}
		else if (op->kind == token::square_open)
		{
			operator_name = "[]";
		}
		else
		{
			operator_name = op->value;
		}

		errors.emplace_back(bad_tokens(
			op - 1, op, stream,
			bz::format("Error: operator {} cannot take 0 arguments", operator_name)
		));
	}
	if (params.size() == 1)
	{
		if (op->kind != token::paren_open && !is_unary_operator(op->kind))
		{
			bz::string operator_name = op->kind == token::square_open ? "[]" : op->value;
			errors.emplace_back(bad_tokens(
				op - 1, op, stream,
				bz::format("Error: operator {} cannot take 1 argument", operator_name)
			));
		}
	}
	else if (params.size() == 2)
	{
		if (op->kind != token::paren_open && !is_binary_operator(op->kind))
		{
			errors.emplace_back(bad_tokens(
				op - 1, op, stream,
				bz::format("Error: operator {} cannot take 2 arguments", op->value)
			));
		}
	}
	else if (op->kind != token::paren_open)
	{
		bz::string operator_name = op->kind == token::square_open ? "[]" : op->value;
		errors.emplace_back(bad_tokens(
			op - 1, op, stream,
			bz::format("Error: operator {} cannot take {} arguments", operator_name, params.size())
		));
	}

	assert_token(stream, token::arrow, errors);
	auto ret_type = get_expression_or_type_tokens<token::curly_open>(stream, end, errors);

	return ast::make_decl_operator(
		op,
		std::move(params),
		ast::make_ts_unresolved(ret_type),
		get_stmt_compound(stream, end, errors)
	);
}

ast::declaration parse_declaration(token_pos &stream, token_pos end, bz::vector<error> &errors)
{
	switch (stream->kind)
	{
	// variable declaration
	case token::kw_let:
	case token::kw_const:
		return parse_variable_declaration(stream, end, errors);

	// struct definition
	case token::kw_struct:
		return parse_struct_definition(stream, end, errors);

	// function definition
	case token::kw_function:
		return parse_function_definition(stream, end, errors);

	// operator definition
	case token::kw_operator:
		return parse_operator_definition(stream, end, errors);

	default:
		errors.emplace_back(bad_token(stream, "expected a declaration"));
		while (
			stream != end
			&& stream->kind != token::kw_let
			&& stream->kind != token::kw_const
			&& stream->kind != token::kw_struct
			&& stream->kind != token::kw_function
			&& stream->kind != token::kw_operator
		)
		{
			++stream;
		}
		return parse_declaration(stream, end, errors);
	}
}

ast::statement parse_statement(token_pos &stream, token_pos end, bz::vector<error> &errors)
{
	switch (stream->kind)
	{
	// if statement
	case token::kw_if:
		return parse_if_statement(stream, end, errors);

	// while statement
	case token::kw_while:
		return parse_while_statement(stream, end, errors);

	// for statement
	case token::kw_for:
		return parse_for_statement(stream, end, errors);

	// return statement
	case token::kw_return:
		return parse_return_statement(stream, end, errors);

	// no-op statement
	case token::semi_colon:
		return parse_no_op_statement(stream, end, errors);

	// compound statement
	case token::curly_open:
		return ast::make_statement(get_stmt_compound_ptr(stream, end, errors));

	// variable declaration
	case token::kw_let:
	case token::kw_const:
		return parse_variable_declaration(stream, end, errors);

	// struct definition
	case token::kw_struct:
		return parse_struct_definition(stream, end, errors);

	// function definition
	case token::kw_function:
		return parse_function_definition(stream, end, errors);

	// operator definition
	case token::kw_operator:
		return parse_operator_definition(stream, end, errors);

	// expression statement
	default:
		return parse_expression_statement(stream, end, errors);
	}
}
