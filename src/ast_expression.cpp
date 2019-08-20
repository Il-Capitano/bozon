#include "ast_expression.h"
#include "context.h"



ast_literal::ast_literal(token t)
{
	switch(t.kind)
	{
	case token::number_literal:
	{
		size_t pos;
		std::string value = static_cast<std::string>(t.value);
		while ((pos = value.find('\'')) != std::string::npos)
		{
			value.erase(std::remove(value.begin(), value.end(), '\''));
		}

		double num = 0;
		size_t i;
		for (i = 0; i < value.length() && value[i] != '.'; ++i)
		{
			assert(value[i] >= '0' && value[i] <= '9');
			num *= 10;
			num += value[i] - '0';
		}

		if (i == value.length())
		{
			this->kind = ast_literal::integer_number;
			this->integer_value = static_cast<uint64_t>(num);
		}
		else
		{
			assert(value[i] == '.');
			++i;
			this->kind = ast_literal::floating_point_number;

			double level = 1;
			while (i < value.length())
			{
				assert(value[i] >= '0' && value[i] <= '9');
				level *= 0.1;
				num += level * (value[i] - '0');
				++i;
			}

			this->floating_point_value = num;
		}
		break;
	}

	case token::string_literal:
		this->kind = ast_literal::string;
		this->string_value = static_cast<std::string>(t.value);
		break;

	case token::character_literal:
		this->kind = ast_literal::character;
		assert(t.value.length() == 1);
		this->char_value = t.value[0];
		break;

	case token::kw_true:
		this->kind = ast_literal::bool_true;
		break;

	case token::kw_false:
		this->kind = ast_literal::bool_false;
		break;

	case token::kw_null:
		this->kind = ast_literal::null;
		break;

	default:
		assert(false);
		break;
	}
}

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

ast_expression::ast_expression(token _t)
	: base_t()
{
	switch (_t.kind)
	{
	case token::identifier:
		this->typespec = context.get_variable_typespec(_t.value);
		if (!this->typespec)
		{
			bad_token(_t, "Undefined identifier");
		}
		this->emplace<identifier>(make_ast_identifier(_t.value));
		return;

	case token::number_literal:
		this->emplace<literal>(make_ast_literal(std::move(_t)));
		this->typespec = make_ast_typespec(
			this->get<literal>()->kind == ast_literal::integer_number
			? "int32"
			: "float64"
		);
		return;

	case token::kw_true:
	case token::kw_false:
		this->emplace<literal>(make_ast_literal(std::move(_t)));
		this->typespec = make_ast_typespec("bool");
		return;

	case token::string_literal:
	case token::character_literal:
	case token::kw_null:
	default:
		assert(false);
		return;
	}
}

ast_expression::ast_expression(ast_unary_op_ptr _unary_op)
	: base_t(std::move(_unary_op))
{
	auto &op = this->get<unary_op>();
	this->typespec = context.get_unary_operation_typespec(
		op->op,
		op->expr->typespec
	);
}

ast_expression::ast_expression(ast_binary_op_ptr _binary_op)
	: base_t(std::move(_binary_op))
{
	auto &op = this->get<binary_op>();
	this->typespec = context.get_binary_operation_typespec(
		op->op,
		op->lhs->typespec,
		op->rhs->typespec
	);
}

ast_expression::ast_expression(ast_function_call_op_ptr _func_call_op)
	: base_t(std::move(_func_call_op))
{}




static ast_expression_ptr parse_expression_internal(
	std::vector<token>::const_iterator &stream,
	std::vector<token>::const_iterator &end,
	precedence p
);

static std::vector<ast_expression_ptr> parse_expression_comma_list_internal(
	std::vector<token>::const_iterator &stream,
	std::vector<token>::const_iterator &end
);

static ast_expression_ptr parse_primary_expression(
	std::vector<token>::const_iterator &stream,
	std::vector<token>::const_iterator &end
)
{
	if (stream == end)
	{
		std::cerr << "Internal error: unqualified call to parse_primary_expression()\n";
		exit(1);
	}

	switch (stream->kind)
	{
	case token::identifier:
	case token::number_literal:
	case token::string_literal:
	case token::character_literal:
	case token::kw_true:
	case token::kw_false:
	case token::kw_null:
	{
		auto expr = make_ast_expression(*stream);
		++stream;
		return std::move(expr);
	}

	case token::paren_open:
	{
		++stream;
		auto expr = parse_expression_internal(stream, end, precedence{});
		if (stream->kind != token::paren_close)
		{
			bad_token(*stream, "Expected ')'");
		}
		++stream;
		return std::move(expr);
	}

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
		auto op = stream->kind;
		++stream;
		auto prec = get_unary_precedence(op);
		auto expr = parse_expression_internal(stream, end, prec);

		return make_ast_expression(make_ast_unary_op(op, std::move(expr)));
	}

	default:
		assert(false);
		return nullptr;
	}
}

static ast_expression_ptr parse_expression_helper(
	std::vector<token>::const_iterator &stream,
	std::vector<token>::const_iterator &end,
	ast_expression_ptr lhs,
	precedence p
)
{
	uint32_t op;
	precedence op_prec;

	while (
		stream != end
		&&
		(op_prec = get_binary_precedence(op = stream->kind)) <= p
	)
	{
		++stream;

		// TODO: check for () and []
		switch (op)
		{
		case token::paren_open:
		{
			if (stream != end && stream->kind == token::paren_close)
			{
				++stream;
				lhs = make_ast_expression(
					make_ast_function_call_op(std::move(lhs), std::vector<ast_expression_ptr>())
				);
				break;
			}
			auto params = parse_expression_comma_list_internal(stream, end);
			if (stream == end)
			{
				std::cerr << "Expected ')'\n";
				exit(1);
			}
			assert_token(*stream, token::paren_close);
			++stream; // ')'

			lhs = make_ast_expression(
				make_ast_function_call_op(std::move(lhs), std::move(params))
			);
			break;
		}

		case token::square_open:
		{
			auto rhs = parse_expression_internal(stream, end, precedence{});
			assert_token(*stream, token::square_close);
			++stream; // ']'

			lhs = make_ast_expression(make_ast_binary_op(op, std::move(lhs), std::move(rhs)));
			break;
		}

		default:
		{
			auto rhs = parse_primary_expression(stream, end);
			precedence rhs_prec;
			while (
				stream != end
				&&
				(rhs_prec = get_binary_precedence(stream->kind)) < op_prec
			)
			{
				rhs = parse_expression_helper(stream, end, std::move(rhs), rhs_prec);
			}

			lhs = make_ast_expression(make_ast_binary_op(op, std::move(lhs), std::move(rhs)));
			break;
		}
		}
	}

	return std::move(lhs);
};

static ast_expression_ptr parse_expression_internal(
	std::vector<token>::const_iterator &stream,
	std::vector<token>::const_iterator &end,
	precedence p
)
{
	if (stream == end)
	{
		std::cerr << "Internal error: unqualified call to parse_expression_internal()\n";
		exit(1);
	}

	auto lhs = parse_primary_expression(stream, end);
	return parse_expression_helper(stream, end, std::move(lhs), p);
}

static std::vector<ast_expression_ptr> parse_expression_comma_list_internal(
	std::vector<token>::const_iterator &stream,
	std::vector<token>::const_iterator &end
)
{
	if (stream == end)
	{
		return {};
	}

	std::vector<ast_expression_ptr> rv = {};
	rv.emplace_back(parse_expression_internal(stream, end, no_comma));
	while (stream != end && stream->kind == token::comma)
	{
		++stream;
		rv.emplace_back(parse_expression_internal(stream, end, no_comma));
	}
	return std::move(rv);
}

ast_expression_ptr parse_ast_expression(std::vector<token> const &expr)
{
	assert(!expr.empty());

	auto begin = expr.cbegin();
	auto end   = expr.cend();

	auto rv = parse_expression_internal(begin, end, precedence{});
	if (begin != end)
	{
		bad_token(*begin, "Expected ';'");
	}

	return std::move(rv);
}

ast_expression_ptr parse_ast_expression(
	std::vector<token>::const_iterator &stream,
	std::vector<token>::const_iterator &end
)
{
	return parse_expression_internal(stream, end, precedence{});
}

std::vector<ast_expression_ptr> parse_ast_expression_comma_list(std::vector<token> const &expr)
{
	if (expr.empty())
	{
		return {};
	}

	auto begin = expr.cbegin();
	auto end   = expr.cend();

	auto rv = parse_expression_comma_list_internal(begin, end);
	if (begin != end)
	{
		bad_token(*begin, "Expected ';'");
	}

	return std::move(rv);
}
