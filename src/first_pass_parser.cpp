#include "first_pass_parser.h"


template<typename T>
void append_vector(std::vector<T> &base, std::vector<T> new_elems)
{
	base.reserve(base.size() + new_elems.size());
	for (auto &elem : new_elems)
	{
		base.emplace_back(std::move(elem));
	}
}



fp_statement::fp_statement(fp_if_statement_ptr if_stmt)
	: base_t(std::move(if_stmt))
{}

fp_statement::fp_statement(fp_while_statement_ptr while_stmt)
	: base_t(std::move(while_stmt))
{}

fp_statement::fp_statement(fp_for_statement_ptr for_stmt)
	: base_t(std::move(for_stmt))
{}

fp_statement::fp_statement(fp_return_statement_ptr return_stmt)
	: base_t(std::move(return_stmt))
{}

fp_statement::fp_statement(fp_no_op_statement_ptr no_op_stmt)
	: base_t(std::move(no_op_stmt))
{}

fp_statement::fp_statement(fp_compound_statement_ptr compound_stmt)
	: base_t(std::move(compound_stmt))
{}

fp_statement::fp_statement(fp_expression_statement_ptr expr_stmt)
	: base_t(std::move(expr_stmt))
{}

fp_statement::fp_statement(fp_declaration_statement_ptr decl_stmt)
	: base_t(std::move(decl_stmt))
{}



template<uint32_t ...end_tokens>
static token_range get_fp_expression_or_type(src_tokens::pos &stream, src_tokens::pos end)
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
		// open parenthesis
		case token::paren_open:
		case token::curly_open:
		case token::square_open:
		// type specifiers that are not operators
		case token::colon:
		case token::kw_auto:
		case token::kw_const:
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
			get_fp_expression_or_type<token::paren_close>(stream, end);
			assert_token(stream, token::paren_close);
			break;
		}

		case token::curly_open:
		{
			++stream; // '{'
			get_fp_expression_or_type<token::curly_close>(stream, end);
			assert_token(stream, token::curly_close);
			break;
		}

		case token::square_open:
		{
			++stream; // '['
			get_fp_expression_or_type<token::square_close>(stream, end);
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


static token_range get_fp_parameters(src_tokens::pos &stream, src_tokens::pos end)
{
	return get_fp_expression_or_type<token::paren_close>(stream, end);
}

static fp_compound_statement_ptr get_fp_compound_statement(
	src_tokens::pos &stream,
	src_tokens::pos end
)
{
	assert_token(stream, token::curly_open);
	std::vector<fp_statement_ptr> stms;

	while (stream != end && stream->kind != token::curly_close)
	{
		stms.emplace_back(get_fp_statement(stream, end));
	}
	assert(stream != end);
	++stream; // '}'

	return make_fp_compound_statement(std::move(stms));
}

fp_statement_ptr get_fp_statement(src_tokens::pos &stream, src_tokens::pos end)
{
	switch (stream->kind)
	{
	// if statement
	case token::kw_if:
	{
		++stream; // 'if'

		assert_token(stream, token::paren_open);
		auto condition = get_fp_expression_or_type<token::paren_close>(stream, end);
		assert_token(stream, token::paren_close);

		auto if_block = get_fp_statement(stream, end);

		if (stream->kind == token::kw_else)
		{
			++stream; // 'else'
			auto else_block = get_fp_statement(stream, end);

			return make_fp_statement(
				make_fp_if_statement(
					condition,
					std::move(if_block),
					std::move(else_block)
				)
			);
		}
		else
		{
			return make_fp_statement(
				make_fp_if_statement(
					condition,
					std::move(if_block),
					nullptr
				)
			);
		}
	}

	// while statement
	case token::kw_while:
	{
		++stream; // 'while'

		assert_token(stream, token::paren_open);
		auto condition = get_fp_expression_or_type<token::paren_close>(stream, end);
		assert_token(stream, token::paren_close);

		auto while_block = get_fp_statement(stream, end);

		return make_fp_statement(
			make_fp_while_statement(
				condition,
				std::move(while_block)
			)
		);
	}

	// for statement
	case token::kw_for:
	{
		// TODO: implement
		assert(false);
		return nullptr;
	}

	// return statement
	case token::kw_return:
	{
		++stream; // 'return'

		auto expr = get_fp_expression_or_type<token::semi_colon>(stream, end);
		assert_token(stream, token::semi_colon);

		return make_fp_statement(
			make_fp_return_statement(expr)
		);
	}

	// no-op statement
	case token::semi_colon:
	{
		++stream; // ';'
		return make_fp_statement(make_fp_no_op_statement());
	}

	// compound statement
	case token::curly_open:
	{
		return make_fp_statement(
			get_fp_compound_statement(stream, end)
		);
	}

	// variable declaration
	case token::kw_let:
	{
		++stream; // 'let'
		assert(stream != end);

		auto id = assert_token(stream, token::identifier).value;
		auto type_and_init = get_fp_expression_or_type<token::semi_colon>(stream, end);
		assert_token(stream, token::semi_colon);

		return make_fp_statement(
			make_fp_declaration_statement(
				make_fp_variable_decl(id, type_and_init)
			)
		);
	}

	// struct definition
	case token::kw_struct:
	{
		assert(false);
		return nullptr;
	}

	// function definition
	case token::kw_function:
	{
		++stream; // 'function'
		auto id = assert_token(stream, token::identifier).value;
		auto params = get_fp_parameters(stream, end);
		token_range ret_type = { stream, stream };

		if (stream->kind == token::arrow)
		{
			++stream; // '->'
			ret_type = get_fp_expression_or_type<token::curly_open>(stream, end);
		}

		auto body = get_fp_compound_statement(stream, end);
		return make_fp_statement(
			make_fp_declaration_statement(
				make_fp_function_decl(
					id, params, ret_type, std::move(body)
				)
			)
		);
	}

	// operator definition
	case token::kw_operator:
	{
		assert(false);
		return nullptr;
	}

	// expression statement
	default:
	{
		auto expr = get_fp_expression_or_type<token::semi_colon>(stream, end);
		assert_token(stream, token::semi_colon);

		return make_fp_statement(
			make_fp_expression_statement(expr)
		);
	}
	}
}


std::vector<fp_statement_ptr> get_fp_statements(src_tokens::pos &stream, src_tokens::pos end)
{
	std::vector<fp_statement_ptr> statements = {};
	while (stream->kind != token::eof)
	{
		statements.emplace_back(get_fp_statement(stream, end));
	}
	return std::move(statements);
}
