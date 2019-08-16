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





static fp_statement_ptr get_fp_statement(token_stream &stream);


fp_statement::fp_statement(fp_if_statement_ptr if_stmt)
	: base_t(std::move(if_stmt)),
	  kind(if_statement)
{}

fp_statement::fp_statement(fp_while_statement_ptr while_stmt)
	: base_t(std::move(while_stmt)),
	  kind(while_statement)
{}

fp_statement::fp_statement(fp_for_statement_ptr for_stmt)
	: base_t(std::move(for_stmt)),
	  kind(for_statement)
{}

fp_statement::fp_statement(fp_return_statement_ptr return_stmt)
	: base_t(std::move(return_stmt)),
	  kind(return_statement)
{}

fp_statement::fp_statement(fp_no_op_statement_ptr no_op_stmt)
	: base_t(std::move(no_op_stmt)),
	  kind(no_op_statement)
{}

fp_statement::fp_statement(fp_compound_statement_ptr compound_stmt)
	: base_t(std::move(compound_stmt)),
	  kind(compound_statement)
{}

fp_statement::fp_statement(fp_expression_statement_ptr expr_stmt)
	: base_t(std::move(expr_stmt)),
	  kind(expression_statement)
{}

fp_statement::fp_statement(fp_declaration_statement_ptr decl_stmt)
	: base_t(std::move(decl_stmt)),
	  kind(declaration_statement)
{}



template<uint32_t... end_tokens>
static std::vector<token> get_fp_expression_or_type(token_stream &stream)
{
	std::vector<token> tokens = {};

	auto is_valid_expr_token = [&]()
	{
		if ((
			(stream.current().kind == end_tokens)
			|| ...
		))
		{
			return false;
		}

		switch (stream.current().kind)
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
		// type specifiers
		case token::colon:
		case token::kw_auto:
		case token::kw_const:
			return true;

		default:
			return is_operator(stream.current().kind);
		}
	};


	while (is_valid_expr_token())
	{
		switch (stream.current().kind)
		{
		case token::paren_open:
		{
			tokens.emplace_back(stream.get());
			auto new_tokens = get_fp_expression_or_type<token::paren_close>(stream);
			append_vector(tokens, std::move(new_tokens));
			tokens.emplace_back(assert_token(stream, token::paren_close));
			break;
		}

		case token::curly_open:
		{
			tokens.emplace_back(stream.get());
			auto new_tokens = get_fp_expression_or_type<token::curly_close>(stream);
			append_vector(tokens, std::move(new_tokens));
			tokens.emplace_back(assert_token(stream, token::curly_close));
			break;
		}

		case token::square_open:
		{
			tokens.emplace_back(stream.get());
			auto new_tokens = get_fp_expression_or_type<token::square_close>(stream);
			append_vector(tokens, std::move(new_tokens));
			tokens.emplace_back(assert_token(stream, token::square_close));
			break;
		}

		default:
			tokens.emplace_back(stream.get());
			break;
		}
	}

	return std::move(tokens);
}


static std::vector<token> get_fp_parameters(token_stream &stream)
{
	assert_token(stream, token::paren_open);
	if (stream.current().kind == token::paren_close)
	{
		stream.step(); // ')'
		return {};
	}

	auto params = get_fp_expression_or_type<token::paren_close>(stream);
	assert_token(stream, token::paren_close);

	return std::move(params);
}

static fp_compound_statement_ptr get_fp_compound_statement(token_stream &stream)
{
	assert_token(stream, token::curly_open);
	std::vector<fp_statement_ptr> stms;
	while (stream.current().kind != token::curly_close)
	{
		stms.emplace_back(get_fp_statement(stream));
	}
	stream.step(); // '}'
	return make_fp_compound_statement(std::move(stms));
}

static fp_statement_ptr get_fp_statement(token_stream &stream)
{
	switch (stream.current().kind)
	{
	// if statement
	case token::kw_if:
	{
		stream.step(); // 'if'

		assert_token(stream, token::paren_open);
		auto condition = get_fp_expression_or_type<token::paren_close>(stream);
		assert_token(stream, token::paren_close);

		auto if_block = get_fp_statement(stream);

		if (stream.current().kind == token::kw_else)
		{
			stream.step(); // 'else'
			auto else_block = get_fp_statement(stream);

			return make_fp_statement(
				make_fp_if_statement(
					std::move(condition),
					std::move(if_block),
					std::move(else_block)
				)
			);
		}
		else
		{
			return make_fp_statement(
				make_fp_if_statement(
					std::move(condition),
					std::move(if_block),
					nullptr
				)
			);
		}
	}

	// while statement
	case token::kw_while:
	{
		stream.step(); // 'while'

		assert_token(stream, token::paren_open);
		auto condition = get_fp_expression_or_type<token::paren_close>(stream);
		assert_token(stream, token::paren_close);

		auto while_block = get_fp_statement(stream);

		return make_fp_statement(
			make_fp_while_statement(
				std::move(condition),
				std::move(while_block)
			)
		);
	}

	// for statement
	case token::kw_for:
	{
		// TODO: implement
		std::cerr << "for statement not yet implemented\n";
		exit(1);
	}

	// return statement
	case token::kw_return:
	{
		stream.step(); // 'return'
		// empty return statement
		if (stream.current().kind == token::semi_colon)
		{
			stream.step(); // ';'
			return make_fp_statement(
				make_fp_return_statement(std::vector<token>{})
			);
		}

		auto expr = get_fp_expression_or_type<token::semi_colon>(stream);
		assert_token(stream, token::semi_colon);

		return make_fp_statement(
			make_fp_return_statement(std::move(expr))
		);
	}

	// no-op statement
	case token::semi_colon:
	{
		stream.step(); // ';'

		return make_fp_statement(make_fp_no_op_statement());
	}

	// TODO: could also be a tuple
	// compound statement
	case token::curly_open:
	{
		return make_fp_statement(
			get_fp_compound_statement(stream)
		);
	}

	// variable declaration
	case token::kw_let:
	{
		stream.step(); // 'let'
		auto id = assert_token(stream, token::identifier).value;
		auto type_and_init = get_fp_expression_or_type<token::semi_colon>(stream);
		return make_fp_statement(
			make_fp_declaration_statement(
				make_fp_variable_decl(
					id, std::move(type_and_init)
				)
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
		stream.step(); // 'function'
		auto id = assert_token(stream, token::identifier).value;
		auto params = get_fp_parameters(stream);
		std::vector<token> ret_type = {};

		if (stream.current().kind == token::arrow)
		{
			ret_type = get_fp_expression_or_type<token::curly_open>(stream);
		}

		auto body = get_fp_compound_statement(stream);
		return make_fp_statement(
			make_fp_declaration_statement(
				make_fp_function_decl(
					id, std::move(params), std::move(ret_type), std::move(body)
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
		auto expr = get_fp_expression_or_type<token::semi_colon>(stream);
		assert_token(stream, token::semi_colon);

		return make_fp_statement(
			make_fp_expression_statement(
				std::move(expr)
			)
		);
	}
	}
}


std::vector<fp_statement_ptr> get_fp_statements(token_stream &stream)
{
	std::vector<fp_statement_ptr> statements = {};
	while (stream.current().kind != token::eof)
	{
		statements.emplace_back(get_fp_statement(stream));
	}
	return std::move(statements);
}
