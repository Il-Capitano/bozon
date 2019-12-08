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
	src_tokens::pos &stream,
	src_tokens::pos  end
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
	src_tokens::pos &stream,
	src_tokens::pos  end
)
{
	assert_token(stream, token::paren_open);
	bz::vector<ast::variable> params;

	if (stream->kind == token::paren_close)
	{
		return params;
	}

	do
	{
		intern_string id;
		if (stream->kind == token::identifier)
		{
			id = stream->value;
			++stream;
		}
		else
		{
			id = ""_is;
		}

		assert_token(stream, token::colon);

		auto type = get_expression_or_type<token::paren_close, token::comma>(stream, end);

		params.push_back(
			{
				id,
				ast::make_typespec(
					ast::ts_unresolved(type)
				)
			}
		);
	} while (
		stream != end
		&& stream->kind == token::comma
		&& (++stream, true)
	);

	assert_token(stream, token::paren_close);

	return params;
}


static ast::stmt_compound_ptr get_stmt_compound(
	src_tokens::pos &stream,
	src_tokens::pos end
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

ast::statement get_ast_statement(
	src_tokens::pos &stream,
	src_tokens::pos  end
)
{
	switch (stream->kind)
	{
	// if statement
	case token::kw_if:
	{
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

	// while statement
	case token::kw_while:
	{
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

	// for statement
	case token::kw_for:
	{
		// TODO: implement
		assert(false);
	}

	// return statement
	case token::kw_return:
	{
		auto const begin_token = stream;
		++stream; // 'return'

		auto expr = get_expression_or_type<token::semi_colon>(stream, end);
		assert_token(stream, token::semi_colon);

		return ast::make_stmt_return(
			token_range{ begin_token, stream },
			ast::make_expr_unresolved(expr)
		);
	}

	// no-op statement
	case token::semi_colon:
	{
		auto const begin_token = stream;
		++stream; // ';'
		return ast::make_stmt_no_op(token_range{ begin_token, stream });
	}

	// compound statement
	case token::curly_open:
	{
		return ast::make_statement(get_stmt_compound(stream, end));
	}

	// variable declaration
	case token::kw_let:
	{
		++stream; // 'let'
		assert(stream != end);

		auto id = assert_token(stream, token::identifier);

		ast::typespec_ptr type = nullptr;

		if (stream->kind == token::colon)
		{
			++stream; // ':'
			auto type_tokens = get_expression_or_type<
				token::assign, token::semi_colon
			>(stream, end);

			type = ast::make_typespec(ast::ts_unresolved(type_tokens));

			if (stream->kind == token::semi_colon)
			{
				++stream; // ';'
				return ast::make_decl_variable(id, type);
			}
		}
		else if (stream->kind != token::assign)
		{
			bad_token(stream, "Expected '=' or ':'");
		}

		assert_token(stream, token::assign, token::semi_colon);

		auto init = get_expression_or_type<token::semi_colon>(stream, end);

		assert_token(stream, token::semi_colon);
		return ast::make_decl_variable(
			id,
			type,
			ast::make_expr_unresolved(init)
		);
	}

	// struct definition
	case token::kw_struct:
	{
		assert(false);
	}

	// function definition
	case token::kw_function:
	{
		++stream; // 'function'
		auto id = assert_token(stream, token::identifier);

		auto params = get_function_params(stream, end);

		assert_token(stream, token::arrow);
		auto ret_type = get_expression_or_type<token::curly_open>(stream, end);

		return ast::make_decl_function(
			id,
			std::move(params),
			ast::make_typespec(
				ast::ts_unresolved(ret_type)
			),
			get_stmt_compound(stream, end)
		);
	}

	// operator definition
	case token::kw_operator:
	{
		++stream; // 'operator'
		auto op = stream;
		if (!is_overloadable_operator(op->kind))
		{
			bad_token(stream, "Expected overloadable operator");
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

		assert_token(stream, token::arrow);
		auto ret_type = get_expression_or_type<token::curly_open>(stream, end);

		return ast::make_decl_operator(
			op,
			std::move(params),
			ast::make_typespec(
				ast::ts_unresolved(ret_type)
			),
			get_stmt_compound(stream, end)
		);
	}

	// expression statement
	default:
	{
		auto const begin_token = stream;
		auto expr = get_expression_or_type<token::semi_colon>(stream, end);
		assert_token(stream, token::semi_colon);

		return ast::make_stmt_expression(
			token_range{ begin_token, stream },
			ast::make_expr_unresolved(expr)
		);
	}
	}
}


bz::vector<ast::statement> get_ast_statements(src_tokens::pos &stream, src_tokens::pos end)
{
	bz::vector<ast::statement> statements = {};
	while (stream->kind != token::eof)
	{
		statements.emplace_back(get_ast_statement(stream, end));
	}
	return statements;
}
