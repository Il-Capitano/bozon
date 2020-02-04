#include "parser.h"
#include "context.h"

// ================================================================
// ---------------------- expression parsing ----------------------
// ================================================================

static std::map<uint32_t, precedence> binary_op_precendences =
{
	{ token::scope,              precedence{  1, true  } },

	{ token::paren_open,         precedence{  2, true  } },
	{ token::square_open,        precedence{  2, true  } },
	{ token::dot,                precedence{  2, true  } },
	{ token::arrow,              precedence{  2, true  } },

	{ token::dot_dot,            precedence{  4, true  } },

	{ token::multiply,           precedence{  5, true  } },
	{ token::divide,             precedence{  5, true  } },
	{ token::modulo,             precedence{  5, true  } },

	{ token::plus,               precedence{  6, true  } },
	{ token::minus,              precedence{  6, true  } },

	{ token::bit_left_shift,     precedence{  7, true  } },
	{ token::bit_right_shift,    precedence{  7, true  } },

	{ token::bit_and,            precedence{  8, true  } },
	{ token::bit_xor,            precedence{  9, true  } },
	{ token::bit_or,             precedence{ 10, true  } },

	{ token::less_than,          precedence{ 11, true  } },
	{ token::less_than_eq,       precedence{ 11, true  } },
	{ token::greater_than,       precedence{ 11, true  } },
	{ token::greater_than_eq,    precedence{ 11, true  } },

	{ token::equals,             precedence{ 12, true  } },
	{ token::not_equals,         precedence{ 12, true  } },

	{ token::bool_and,           precedence{ 13, true  } },
	{ token::bool_xor,           precedence{ 14, true  } },
	{ token::bool_or,            precedence{ 15, true  } },

	// ternary ?
	{ token::assign,             precedence{ 16, false } },
	{ token::plus_eq,            precedence{ 16, false } },
	{ token::minus_eq,           precedence{ 16, false } },
	{ token::multiply_eq,        precedence{ 16, false } },
	{ token::divide_eq,          precedence{ 16, false } },
	{ token::modulo_eq,          precedence{ 16, false } },
	{ token::dot_dot_eq,         precedence{ 16, false } },
	{ token::bit_left_shift_eq,  precedence{ 16, false } },
	{ token::bit_right_shift_eq, precedence{ 16, false } },
	{ token::bit_and_eq,         precedence{ 16, false } },
	{ token::bit_xor_eq,         precedence{ 16, false } },
	{ token::bit_or_eq,          precedence{ 16, false } },

	{ token::comma,              precedence{ 18, true  } },
};

constexpr precedence no_comma(17, true);

static std::map<uint32_t, precedence> unary_op_precendences =
{
	{ token::plus,               precedence{  3, false } },
	{ token::minus,              precedence{  3, false } },
	{ token::plus_plus,          precedence{  3, false } },
	{ token::minus_minus,        precedence{  3, false } },
	{ token::bit_not,            precedence{  3, false } },
	{ token::bool_not,           precedence{  3, false } },
	{ token::address_of,         precedence{  3, false } },
	{ token::dereference,        precedence{  3, false } },
	{ token::kw_sizeof,          precedence{  3, false } },
	{ token::kw_typeof,          precedence{  3, false } },
	// new, delete
};


static precedence get_binary_precedence(uint32_t kind)
{
	auto it = binary_op_precendences.find(kind);
	if (it == binary_op_precendences.end())
	{
		return precedence{};
	}
	else
	{
		return it->second;
	}
}

static precedence get_unary_precedence(uint32_t kind)
{
	auto it = unary_op_precendences.find(kind);
	if (it == unary_op_precendences.end())
	{
		return precedence{};
	}
	else
	{
		return it->second;
	}
}

static bz::vector<ast::expression> parse_expression_comma_list(
	token_pos &stream, token_pos end,
	bz::vector<error> &errors
);

static ast::expression parse_primary_expression(
	token_pos &stream, token_pos end,
	bz::vector<error> &errors
)
{
	switch (stream->kind)
	{
	case token::identifier:
	{
		auto id = ast::make_expr_identifier(stream);
		++stream;
		return id;
	}

	// literals
	case token::number_literal:
	case token::string_literal:
	case token::character_literal:
	case token::kw_true:
	case token::kw_false:
	case token::kw_null:
	{
		auto literal = ast::make_expr_literal(stream);
		++stream;
		return literal;
	}

	case token::paren_open:
	{
		++stream;
		auto paren_begin = stream;

		size_t paren_level;
		for (paren_level = 1; stream != end && paren_level != 0; ++stream)
		{
			if (stream->kind == token::paren_open)
			{
				++paren_level;
			}
			else if (stream->kind == token::paren_close)
			{
				--paren_level;
			}
		}

		if (paren_level != 0)
		{
			errors.emplace_back(bad_token(stream, "expected ')'"));
		}

		return ast::make_expr_unresolved(token_range{ paren_begin, stream });
	}

	// tuple
	case token::square_open:
	{
		auto const begin_token = stream;
		++stream;
		auto elems = parse_expression_comma_list(stream, end, errors);
		assert_token(stream, token::square_close, errors);
		auto const end_token = stream;
		return make_expr_tuple(std::move(elems), token_range{ begin_token, end_token });
	}

	// unary operators
	case token::plus:
	case token::minus:
	case token::plus_plus:
	case token::minus_minus:
	case token::bit_not:
	case token::bool_not:
	case token::address_of:
	case token::dereference:
	case token::kw_sizeof:
	case token::kw_typeof:
	{
		auto op = stream;
		++stream;
		auto prec = get_unary_precedence(op->kind);
		auto expr = parse_expression(stream, end, errors, prec);

		auto result = make_expr_unary_op(op, std::move(expr));
		return result;
	}

	default:
		errors.emplace_back(bad_token(stream, "expected primary expression"));
		return ast::expression();
	}
};

static ast::expression parse_expression_helper(
	ast::expression lhs,
	token_pos &stream, token_pos end,
	bz::vector<error> &errors,
	precedence prec
)
{
	token_pos op = nullptr;
	precedence op_prec;

	while (
		stream != end
		&& (op_prec = get_binary_precedence((op = stream)->kind)) <= prec)
	{
		++stream;
		switch (op->kind)
		{
		case token::paren_open:
		{
			if (
				stream != end
				&& stream->kind == token::paren_close
			)
			{
				++stream;
				lhs = make_expr_function_call(
					op, std::move(lhs), bz::vector<ast::expression>{}
				);
				break;
			}

			auto params = parse_expression_comma_list(stream, end, errors);
			assert_token(stream, token::paren_close, errors);

			lhs = make_expr_function_call(
				op, std::move(lhs), std::move(params)
			);
			break;
		}

		case token::square_open:
		{
			auto rhs = parse_expression(stream, end, errors);
			assert_token(stream, token::square_close, errors);

			lhs = make_expr_binary_op(
				op, std::move(lhs), std::move(rhs)
			);
			break;
		}

		default:
		{
			auto rhs = parse_primary_expression(stream, end, errors);
			precedence rhs_prec;

			while (
				stream != end
				&& (rhs_prec = get_binary_precedence(stream->kind)) < op_prec
			)
			{
				rhs = parse_expression_helper(std::move(rhs), stream, end, errors, rhs_prec);
			}

			lhs = make_expr_binary_op(
				op, std::move(lhs), std::move(rhs)
			);
			break;
		}
		}
	}

	return lhs;
}

static bz::vector<ast::expression> parse_expression_comma_list(
	token_pos &stream, token_pos end,
	bz::vector<error> &errors
)
{
	bz::vector<ast::expression> exprs = {};

	exprs.emplace_back(parse_expression(stream, end, errors, no_comma));

	while (stream != end && stream->kind == token::comma)
	{
		++stream; // ','
		exprs.emplace_back(parse_expression(stream, end, errors, no_comma));
	}

	return exprs;
}

ast::expression parse_expression(
	token_pos &stream, token_pos end,
	bz::vector<error> &errors,
	precedence prec
)
{
	auto lhs = parse_primary_expression(stream, end, errors);
	auto result = parse_expression_helper(std::move(lhs), stream, end, errors, prec);
	return result;
}

// ================================================================
// ------------------------ type parsing --------------------------
// ================================================================

ast::typespec parse_typespec(
	token_pos &stream, token_pos end,
	bz::vector<error> &errors
)
{
	if (stream == end)
	{
		errors.emplace_back(bad_token(stream));
		return ast::typespec();
	}

	switch (stream->kind)
	{
	case token::identifier:
	{
		auto id = stream;
		++stream;
		return ast::make_ts_base_type(context.get_type(id));
	}

	case token::kw_const:
		++stream; // 'const'
		return ast::make_ts_constant(parse_typespec(stream, end, errors));

	case token::star:
		++stream; // '*'
		return ast::make_ts_pointer(parse_typespec(stream, end, errors));

	case token::ampersand:
		++stream; // '&'
		return ast::make_ts_reference(parse_typespec(stream, end, errors));

	case token::kw_function:
	{
		++stream; // 'function'
		assert_token(stream, token::paren_open, errors);

		bz::vector<ast::typespec> param_types = {};
		if (stream->kind != token::paren_close) while (stream != end)
		{
			param_types.push_back(parse_typespec(stream, end, errors));
			if (stream->kind != token::paren_close)
			{
				assert_token(stream, token::comma, errors);
			}
			else
			{
				break;
			}
		}
		assert(stream != end);
		assert_token(stream, token::paren_close, errors);
		assert_token(stream, token::arrow, errors);

		auto ret_type = parse_typespec(stream, end, errors);

		return make_ts_function(std::move(ret_type), std::move(param_types));
	}

	case token::square_open:
	{
		++stream; // '['

		bz::vector<ast::typespec> types = {};
		if (stream->kind != token::square_close) while (stream != end)
		{
			types.push_back(parse_typespec(stream, end, errors));
			if (stream->kind != token::square_close)
			{
				assert_token(stream, token::comma, errors);
			}
			else
			{
				break;
			}
		}
		assert(stream != end);
		assert_token(stream, token::square_close, errors);

		return make_ts_tuple(std::move(types));
	}

	default:
		assert(false);
		return ast::typespec();
	}
}
