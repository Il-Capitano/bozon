#include "first_pass_parser.h"


template<typename T>
void append_vector(bz::vector<T> &base, bz::vector<T> new_elems)
{
	base.reserve(base.size() + new_elems.size());
	for (auto &elem : new_elems)
	{
		base.emplace_back(std::move(elem));
	}
}

template<uint32_t ...end_tokens>
static token_range get_expression_or_type(
	token_pos &stream,
	token_pos  end
)
{
	auto begin = stream;

	auto is_valid_expr_token = [&]()
	{
		if ((
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
		// parenthesis
		case token::paren_open:
		case token::curly_open:
		case token::square_open:
		// type specifiers that are not operators
		case token::colon:
		case token::kw_auto:
		case token::kw_const:
		case token::kw_function:
			return true;

		default:
			return is_operator(stream->kind);
		}
	};


	while (is_valid_expr_token())
	{
		switch (stream->kind)
		{
		case token::paren_open:
		{
			++stream; // '('
			get_expression_or_type<token::paren_close>(stream, end);
			assert_token(stream, token::paren_close);
			break;
		}

		case token::curly_open:
		{
			++stream; // '{'
			get_expression_or_type<token::curly_close>(stream, end);
			assert_token(stream, token::curly_close);
			break;
		}

		case token::square_open:
		{
			++stream; // '['
			get_expression_or_type<token::square_close>(stream, end);
			assert_token(stream, token::square_close);
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
	token_pos &stream,
	token_pos  end
)
{
	assert_token(stream, token::paren_open);
	bz::vector<ast::variable> params;

	if (stream->kind == token::paren_close)
	{
		++stream;
		return params;
	}

	do
	{
		auto id = stream;
		if (id->kind == token::identifier)
		{
			++stream;
		}

		assert_token(stream, token::colon);

		auto type = get_expression_or_type<token::paren_close, token::comma>(stream, end);

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

	assert_token(stream, token::paren_close);

	return params;
}


static ast::stmt_compound get_stmt_compound(
	token_pos &stream,
	token_pos end
)
{
	auto const begin_token = stream;
	assert_token(stream, token::curly_open);
	auto comp_stmt = ast::stmt_compound(token_range{ begin_token, stream });

	while (stream != end && stream->kind != token::curly_close)
	{
		comp_stmt.statements.emplace_back(get_ast_statement(stream, end));
	}
	assert(stream != end);
	++stream; // '}'
	comp_stmt.tokens.end = stream;

	return comp_stmt;
}

static ast::stmt_compound_ptr get_stmt_compound_ptr(
	token_pos &stream,
	token_pos end
)
{
	auto const begin_token = stream;
	assert_token(stream, token::curly_open);
	auto comp_stmt = std::make_unique<ast::stmt_compound>(token_range{ begin_token, stream });

	while (stream != end && stream->kind != token::curly_close)
	{
		comp_stmt->statements.emplace_back(get_ast_statement(stream, end));
	}
	assert(stream != end);
	++stream; // '}'
	comp_stmt->tokens.end = stream;

	return comp_stmt;
}

ast::statement parse_if_statement(token_pos &stream, token_pos end)
{
	assert(stream->kind == token::kw_if);
	auto begin_token = stream;
	++stream; // 'if'

	assert_token(stream, token::paren_open);
	auto condition = get_expression_or_type<token::paren_close>(stream, end);
	assert_token(stream, token::paren_close);

	auto if_block = get_ast_statement(stream, end);

	if (stream->kind == token::kw_else)
	{
		++stream; // 'else'
		auto else_block = get_ast_statement(stream, end);

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

ast::statement parse_while_statement(token_pos &stream, token_pos end)
{
	assert(stream->kind == token::kw_while);
	auto const begin_token = stream;
	++stream; // 'while'

	assert_token(stream, token::paren_open);
	auto condition = get_expression_or_type<token::paren_close>(stream, end);
	assert_token(stream, token::paren_close);

	auto while_block = get_ast_statement(stream, end);

	return ast::make_stmt_while(
		token_range{ begin_token, stream },
		ast::make_expr_unresolved(condition),
		std::move(while_block)
	);
}

ast::statement parse_for_statement(token_pos &stream, token_pos)
{
	assert(stream->kind == token::kw_for);
	bad_token(stream, "Error: for statement not yet implemented");
}

ast::statement parse_return_statement(token_pos &stream, token_pos end)
{
	assert(stream->kind == token::kw_return);
	auto const begin_token = stream;
	++stream; // 'return'

	auto expr = get_expression_or_type<token::semi_colon>(stream, end);
	assert_token(stream, token::semi_colon);

	return ast::make_stmt_return(
		token_range{ begin_token, stream },
		ast::make_expr_unresolved(expr)
	);
}

ast::statement parse_no_op_statement(token_pos &stream, token_pos)
{
	assert(stream->kind == token::semi_colon);
	auto const begin_token = stream;
	++stream; // ';'
	return ast::make_stmt_no_op(token_range{ begin_token, stream });
}

ast::declaration parse_variable_declaration(token_pos &stream, token_pos end)
{
	assert(stream->kind == token::kw_let || stream->kind == token::kw_const);
	auto const tokens_begin = stream;
	bool is_const = stream->kind == token::kw_const;
	++stream; // 'let' or 'const'
	assert(stream != end);
	bool is_ref = stream->kind == token::ampersand;
	if (is_ref)
	{
		if (is_const)
		{
			bad_tokens(
				tokens_begin,
				stream,
				stream + 1,
				"Error: A reference cannot be const"
			);
		}
		++stream; // '&'
	}

	auto id = assert_token(stream, token::identifier);

	if (stream->kind == token::colon)
	{
		++stream; // ':'
		auto type_tokens = get_expression_or_type<
			token::assign, token::semi_colon
		>(stream, end);

		auto type = ast::make_ts_unresolved(type_tokens);
		if (is_const)
		{
			type = ast::add_const(std::move(type));
		}
		else if (is_ref)
		{
			type = ast::add_lvalue_reference(std::move(type));
		}

		if (stream->kind == token::semi_colon)
		{
			++stream; // ';'
			auto const tokens_end = stream;
			return ast::make_decl_variable(token_range{tokens_begin, tokens_end}, id, type);
		}

		assert_token(stream, token::assign, token::semi_colon);

		auto init = get_expression_or_type<token::semi_colon>(stream, end);

		assert_token(stream, token::semi_colon);
		auto const tokens_end = stream;
		return ast::make_decl_variable(
			token_range{tokens_begin, tokens_end},
			id,
			type,
			ast::make_expr_unresolved(init)
		);
	}
	else if (stream->kind != token::assign)
	{
		bad_token(stream, "Error: Expected '=' or ':'");
	}
	++stream;

	auto init = get_expression_or_type<token::semi_colon>(stream, end);

	ast::typespec type;
	if (is_const)
	{
		type = ast::make_ts_constant(ast::typespec());
	}
	else if (is_ref)
	{
		type = ast::make_ts_reference(ast::typespec());
	}

	assert_token(stream, token::semi_colon);
	auto const tokens_end = stream;
	return ast::make_decl_variable(
		token_range{tokens_begin, tokens_end},
		id,
		type,
		ast::make_expr_unresolved(init)
	);
}

ast::declaration parse_struct_definition(token_pos &stream, token_pos end)
{
	assert(stream->kind == token::kw_struct);
	++stream; // 'struct'

	auto parse_member_variable = [&]()
	{
		assert(stream->kind == token::identifier);
		auto id = stream;
		++stream;
		assert_token(stream, token::colon);
		auto type = get_expression_or_type<token::semi_colon>(stream, end);
		assert_token(stream, token::semi_colon);
		return ast::variable(
			id, ast::make_ts_unresolved(type)
		);
	};

	auto id = assert_token(stream, token::identifier);
	assert_token(stream, token::curly_open);

	bz::vector<ast::variable> member_variables = {};
	while (stream != end && stream->kind == token::identifier)
	{
		member_variables.push_back(parse_member_variable());
	}

	if (stream->kind != token::curly_close)
	{
		bad_token(stream, "Error: Expected '}' or member variable");
	}
	++stream; // '}'
	return ast::make_decl_struct(id, std::move(member_variables));
}

ast::declaration parse_function_definition(token_pos &stream, token_pos end)
{
	assert(stream->kind == token::kw_function);
	++stream; // 'function'
	auto id = assert_token(stream, token::identifier);

	auto params = get_function_params(stream, end);

	assert_token(stream, token::arrow);
	auto ret_type = get_expression_or_type<token::curly_open>(stream, end);

	return ast::make_decl_function(
		id,
		std::move(params),
		ast::make_ts_unresolved(ret_type),
		get_stmt_compound(stream, end)
	);
}

ast::declaration parse_operator_definition(token_pos &stream, token_pos end)
{
	assert(stream->kind == token::kw_operator);
	++stream; // 'operator'
	auto op = stream;
	if (!is_overloadable_operator(op->kind))
	{
		bad_token(stream, "Error: Expected overloadable operator");
	}

	++stream;
	if (op->kind == token::paren_open)
	{
		assert_token(stream, token::paren_close);
	}
	else if (op->kind == token::square_open)
	{
		assert_token(stream, token::square_close);
	}

	auto params = get_function_params(stream, end);

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

		bad_tokens(
			op - 1, op, stream,
			bz::format("Error: operator {} cannot take 0 arguments", operator_name)
		);
	}
	if (params.size() == 1)
	{
		if (op->kind != token::paren_open && !is_unary_operator(op->kind))
		{
			bz::string operator_name = op->kind == token::square_open ? "[]" : op->value;
			bad_tokens(
				op - 1, op, stream,
				bz::format("Error: operator {} cannot take 1 argument", operator_name)
			);
		}
	}
	else if (params.size() == 2)
	{
		if (op->kind != token::paren_open && !is_binary_operator(op->kind))
		{
			bad_tokens(
				op - 1, op, stream,
				bz::format("Error: operator {} cannot take 2 arguments", op->value)
			);
		}
	}
	else if (op->kind != token::paren_open)
	{
		bz::string operator_name = op->kind == token::square_open ? "[]" : op->value;
		bad_tokens(
			op - 1, op, stream,
			bz::format("Error: operator {} cannot take {} arguments", operator_name, params.size())
		);
	}

	assert_token(stream, token::arrow);
	auto ret_type = get_expression_or_type<token::curly_open>(stream, end);

	return ast::make_decl_operator(
		op,
		std::move(params),
		ast::make_ts_unresolved(ret_type),
		get_stmt_compound(stream, end)
	);
}

ast::statement parse_expression_statement(token_pos &stream, token_pos end)
{
	auto const begin_token = stream;
	auto expr = get_expression_or_type<token::semi_colon>(stream, end);
	assert_token(stream, token::semi_colon);

	return ast::make_stmt_expression(
		token_range{ begin_token, stream },
		ast::make_expr_unresolved(expr)
	);
}

ast::statement get_ast_statement(
	token_pos &stream,
	token_pos  end
)
{
	switch (stream->kind)
	{
	// if statement
	case token::kw_if:
		return parse_if_statement(stream, end);

	// while statement
	case token::kw_while:
		return parse_while_statement(stream, end);

	// for statement
	case token::kw_for:
		// TODO: implement
		assert(false);

	// return statement
	case token::kw_return:
		return parse_return_statement(stream, end);

	// no-op statement
	case token::semi_colon:
		return parse_no_op_statement(stream, end);

	// compound statement
	case token::curly_open:
		return ast::make_statement(get_stmt_compound_ptr(stream, end));

	// variable declaration
	case token::kw_let:
	case token::kw_const:
		return parse_variable_declaration(stream, end);

	// struct definition
	case token::kw_struct:
		return parse_struct_definition(stream, end);

	// function definition
	case token::kw_function:
		return parse_function_definition(stream, end);

	// operator definition
	case token::kw_operator:
		return parse_operator_definition(stream, end);

	// expression statement
	default:
		return parse_expression_statement(stream, end);
	}
}


bz::vector<ast::statement> get_ast_statements(
	token_pos &stream, token_pos end
)
{
	bz::vector<ast::statement> statements = {};
	while (stream->kind != token::eof)
	{
		statements.emplace_back(get_ast_statement(stream, end));
	}
	return statements;
}

ast::declaration get_ast_declaration(
	token_pos &stream, token_pos end
)
{
	switch (stream->kind)
	{
	// variable declaration
	case token::kw_let:
	case token::kw_const:
		return parse_variable_declaration(stream, end);

	// struct definition
	case token::kw_struct:
		return parse_struct_definition(stream, end);

	// function definition
	case token::kw_function:
		return parse_function_definition(stream, end);

	// operator definition
	case token::kw_operator:
		return parse_operator_definition(stream, end);

	default:
		bad_token(stream, "Expected a declaration");
	}
}

bz::vector<ast::declaration> get_ast_declarations(
	token_pos &stream, token_pos end
)
{
	bz::vector<ast::declaration> declarations = {};
	while (stream->kind != token::eof)
	{
		declarations.emplace_back(get_ast_declaration(stream, end));
	}
	return declarations;
}
