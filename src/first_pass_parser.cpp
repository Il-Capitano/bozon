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


fp_statement::fp_statement(fp_if_statement_ptr if_stm)
	: kind(statement::if_statement),
	  if_statement(std::move(if_stm))
{}

fp_statement::fp_statement(fp_while_statement_ptr while_stm)
	: kind(statement::while_statement),
	  while_statement(std::move(while_stm))
{}

fp_statement::fp_statement(fp_for_statement_ptr for_stm)
	: kind(statement::for_statement),
	  for_statement(std::move(for_stm))
{}

fp_statement::fp_statement(fp_return_statement_ptr return_stm)
	: kind(statement::return_statement),
	  return_statement(std::move(return_stm))
{}

fp_statement::fp_statement(fp_no_op_statement_ptr no_op_stm)
	: kind(statement::no_op_statement),
	  no_op_statement(std::move(no_op_stm))
{}

fp_statement::fp_statement(fp_variable_decl_statement_ptr var_decl_stm)
	: kind(statement::variable_decl_statement),
	  variable_decl_statement(std::move(var_decl_stm))
{}

fp_statement::fp_statement(fp_compound_statement_ptr compound_stm)
	: kind(statement::compound_statement),
	  compound_statement(std::move(compound_stm))
{}

fp_statement::fp_statement(fp_expression_statement_ptr expr_stm)
	: kind(statement::expression_statement),
	  expression_statement(std::move(expr_stm))
{}

fp_statement::fp_statement(fp_struct_definition_ptr struct_def)
	: kind(statement::struct_definition),
	  struct_definition(std::move(struct_def))
{}

fp_statement::fp_statement(fp_function_definition_ptr func_def)
	: kind(statement::function_definition),
	  function_definition(std::move(func_def))
{}

fp_statement::fp_statement(fp_operator_definition_ptr op_def)
	: kind(statement::operator_definition),
	  operator_definition(std::move(op_def))
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

	// variable declaration
	case token::kw_let:
	{
		stream.step(); // 'let'
		auto id = assert_token(stream, token::identifier).value;
		auto type_and_init = get_fp_expression_or_type<token::semi_colon>(stream);
		assert_token(stream, token::semi_colon);

		return make_fp_statement(
			make_fp_variable_decl_statement(
				std::move(id), std::move(type_and_init)
			)
		);
	}

	// TODO: could also be a tuple
	// compound statement
	case token::curly_open:
	{
		return make_fp_statement(
			get_fp_compound_statement(stream)
		);
	}

	// struct definition
	case token::kw_struct:
	{
		// TODO: implement
		std::cerr << "struct definition statement not yet implemented\n";
		exit(1);
	}

	// function definition
	case token::kw_function:
	{
		stream.step(); // 'function'
		auto id = assert_token(stream, token::identifier).value;
		auto params = get_fp_parameters(stream);

		// explicit return type
		if (stream.current().kind == token::arrow)
		{
			stream.step(); // '->'
			auto ret_type = get_fp_expression_or_type<token::curly_open>(stream);
			auto body = get_fp_compound_statement(stream);

			return make_fp_statement(
				make_fp_function_definition(
					std::move(id), std::move(params),
					std::move(ret_type), std::move(body)
				)
			);
		}
		// no explicit return type
		else
		{
			if (stream.current().kind != token::curly_open)
			{
				bad_token(stream, "Expected '->' or '{'");
			}
			auto body = get_fp_compound_statement(stream);

			return make_fp_statement(
				make_fp_function_definition(
					std::move(id), std::move(params),
					std::vector<token>{}, std::move(body)
				)
			);
		}
	}

	// operator definition
	case token::kw_operator:
	{
		stream.step(); // 'operator'
		if (!is_overloadable_operator(stream.current().kind))
		{
			bad_token(stream, "Expected overloadable operator");
		}
		auto op = stream.get().kind;
		auto params = get_fp_parameters(stream);

		// explicit return type
		if (stream.current().kind == token::arrow)
		{
			stream.step(); // '->'
			auto ret_type = get_fp_expression_or_type<token::curly_open>(stream);
			auto body = get_fp_compound_statement(stream);

			return make_fp_statement(
				make_fp_operator_definition(
					op, std::move(params),
					std::move(ret_type), std::move(body)
				)
			);
		}
		// no explicit return type
		else
		{
			if (stream.current().kind != token::curly_open)
			{
				bad_token(stream, "Expected '->' or '{'");
			}
			auto body = get_fp_compound_statement(stream);

			return make_fp_statement(
				make_fp_operator_definition(
					op, std::move(params),
					std::vector<token>{}, std::move(body)
				)
			);
		}
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
