#include "expression.h"
#include "../context.h"

namespace ast
{

src_file::token_pos expr_unary_op::get_tokens_begin(void) const
{ return this->op; }

src_file::token_pos expr_unary_op::get_tokens_pivot(void) const
{ return this->op; }

src_file::token_pos expr_unary_op::get_tokens_end(void) const
{ return this->expr.get_tokens_end(); }


src_file::token_pos expr_binary_op::get_tokens_begin(void) const
{ return this->lhs.get_tokens_begin(); }

src_file::token_pos expr_binary_op::get_tokens_pivot(void) const
{ return this->op; }

src_file::token_pos expr_binary_op::get_tokens_end(void) const
{ return this->rhs.get_tokens_end(); }


src_file::token_pos expr_function_call::get_tokens_begin(void) const
{ return this->called.get_tokens_begin(); }

src_file::token_pos expr_function_call::get_tokens_pivot(void) const
{ return this->op; }

src_file::token_pos expr_function_call::get_tokens_end(void) const
{
	if (this->params.size() == 0)
	{
		return this->called.get_tokens_end() + 2;
	}
	else
	{
		return this->params.back().get_tokens_end() + 1;
	}
}


void expr_identifier::resolve(void)
{
	this->expr_type = context.get_identifier_type(this->identifier);
}

void expr_literal::resolve(void)
{}

void expr_unary_op::resolve(void)
{
	this->expr.resolve();
	this->expr_type = context.get_operator_type(*this);
}

void expr_binary_op::resolve(void)
{
	this->lhs.resolve();
	this->rhs.resolve();
	this->expr_type = context.get_operator_type(*this);
}

void expr_function_call::resolve(void)
{
	if (
		this->called.kind() != expression::index<expr_identifier>
		|| context.is_variable(this->called.get<expr_identifier_ptr>()->identifier->value)
	)
	{
		this->called.resolve();
	}
	for (auto &p : this->params)
	{
		p.resolve();
	}

	this->expr_type = context.get_function_call_type(*this);
}



expr_literal::expr_literal(src_file::token_pos stream)
	: src_pos(stream)
{
	switch(stream->kind)
	{
	case token::number_literal:
	{
		bz::string value = stream->value;
		value.erase('\'');

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
			this->value.emplace<integer_number>(static_cast<uint64_t>(num));
			this->expr_type = ast::typespec(std::make_unique<ast::ts_base_type>(ast::int32_));
		}
		else
		{
			assert(value[i] == '.');
			++i;

			double level = 1;
			while (i < value.length())
			{
				assert(value[i] >= '0' && value[i] <= '9');
				level *= 0.1;
				num += level * (value[i] - '0');
				++i;
			}

			this->value.emplace<floating_point_number>(num);
			this->expr_type = ast::typespec(std::make_unique<ast::ts_base_type>(ast::float64_));
		}
		break;
	}

	case token::string_literal:
		this->value.emplace<string>(stream->value);
		this->expr_type = ast::typespec(std::make_unique<ast::ts_base_type>(ast::str_));
		break;

	case token::character_literal:
		assert(stream->value.length() == 1);
		this->value.emplace<character>(stream->value[0]);
		this->expr_type = ast::typespec(std::make_unique<ast::ts_base_type>(ast::char_));
		break;

	case token::kw_true:
		this->value.emplace<bool_true>();
		this->expr_type = ast::typespec(std::make_unique<ast::ts_base_type>(ast::bool_));
		break;

	case token::kw_false:
		this->value.emplace<bool_false>();
		this->expr_type = ast::typespec(std::make_unique<ast::ts_base_type>(ast::bool_));
		break;

	case token::kw_null:
		this->value.emplace<null>();
		this->expr_type = ast::typespec(std::make_unique<ast::ts_base_type>(ast::null_t_));
		break;

	default:
		assert(false);
		break;
	}
}


struct precedence
{
	int value;
	bool is_left_associative;

	precedence(void)
		: value(-1), is_left_associative(true)
	{}

	constexpr precedence(int val, bool assoc)
		: value(val), is_left_associative(assoc)
	{}
};

inline bool operator < (precedence lhs, precedence rhs)
{
	if (lhs.value == -1)
	{
		return false;
	}
	else if (rhs.value == -1)
	{
		return true;
	}
	else
	{
		return rhs.is_left_associative ? lhs.value < rhs.value : lhs.value <= rhs.value;
	}
}

inline bool operator <= (precedence lhs, precedence rhs)
{
	if (lhs.value == -1)
	{
		return false;
	}
	else if (rhs.value == -1)
	{
		return true;
	}
	else
	{
		return lhs.value <= rhs.value;
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

static expression parse_expression(
	src_file::token_pos &stream,
	src_file::token_pos  end,
	precedence       prec = precedence{}
);

static bz::vector<expression> parse_expression_comma_list(
	src_file::token_pos &stream,
	src_file::token_pos  end
);

static expression parse_primary_expression(
	src_file::token_pos &stream,
	src_file::token_pos  end
)
{
	switch (stream->kind)
	{
	case token::identifier:
	{
		auto id = make_expr_identifier(stream);
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
		auto literal = make_expr_literal(stream);
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
			bad_token(stream, "Expected ')'");
		}

		auto result = make_expr_unresolved(token_range{ paren_begin, stream });
		return result;
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
		auto expr = parse_expression(stream, end, prec);

		auto result = make_expr_unary_op(op, std::move(expr));
		return result;
	}

	default:
		bad_token(stream, "Expected primary expression");
	}
};

static expression parse_expression_helper(
	expression   lhs,
	src_file::token_pos &stream,
	src_file::token_pos  end,
	precedence       prec
)
{
	src_file::token_pos op = nullptr;
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
					op, std::move(lhs), bz::vector<expression>{}
				);
				break;
			}

			auto params = parse_expression_comma_list(stream, end);
			assert_token(stream, token::paren_close);

			lhs = make_expr_function_call(
				op, std::move(lhs), std::move(params)
			);
			break;
		}

		case token::square_open:
		{
			auto rhs = parse_expression(stream, end);
			assert_token(stream, token::square_close);

			lhs = make_expr_binary_op(
				op, std::move(lhs), std::move(rhs)
			);
			break;
		}

		default:
		{
			auto rhs = parse_primary_expression(stream, end);
			precedence rhs_prec;

			while (
				stream != end
				&& (rhs_prec = get_binary_precedence(stream->kind)) < op_prec
			)
			{
				rhs = parse_expression_helper(std::move(rhs), stream, end, rhs_prec);
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

static bz::vector<expression> parse_expression_comma_list(
	src_file::token_pos &stream,
	src_file::token_pos  end
)
{
	bz::vector<expression> exprs = {};

	exprs.emplace_back(parse_expression(stream, end, no_comma));

	while (stream != end && stream->kind == token::comma)
	{
		exprs.emplace_back(parse_expression(stream, end, no_comma));
	}

	return exprs;
}

static expression parse_expression(
	src_file::token_pos &stream,
	src_file::token_pos  end,
	precedence       prec
)
{
	auto lhs = parse_primary_expression(stream, end);
	auto result = parse_expression_helper(std::move(lhs), stream, end, prec);
	return result;
}

template<>
void expression::resolve(void)
{
	if (this->kind() == index<expr_unresolved>)
	{
		auto &unresolved_expr = this->get<expr_unresolved_ptr>();

		auto begin = unresolved_expr->expr.begin;
		auto end   = unresolved_expr->expr.end;

		auto expr = parse_expression(begin, end);
		expr.resolve();
		this->assign(std::move(expr));
	}

	switch (this->kind())
	{
	case index<expr_identifier>:
		this->get<expr_identifier_ptr>()->resolve();
		break;

	case index<expr_literal>:
		this->get<expr_literal_ptr>()->resolve();
		break;

	case index<expr_unary_op>:
		this->get<expr_unary_op_ptr>()->resolve();
		break;

	case index<expr_binary_op>:
		this->get<expr_binary_op_ptr>()->resolve();
		break;

	case index<expr_function_call>:
		this->get<expr_function_call_ptr>()->resolve();
		break;

	default:
		assert(false);
		break;
	}
}

typespec get_typespec(expression const &expr)
{
	switch (expr.kind())
	{
	case expression::index<expr_unresolved>:
		assert(false);
		return typespec();

	case expression::index<expr_identifier>:
		return expr.get<expr_identifier_ptr>()->expr_type;

	case expression::index<expr_literal>:
		return expr.get<expr_literal_ptr>()->expr_type;

	case expression::index<expr_unary_op>:
		return expr.get<expr_unary_op_ptr>()->expr_type;

	case expression::index<expr_binary_op>:
		return expr.get<expr_binary_op_ptr>()->expr_type;

	case expression::index<expr_function_call>:
		return expr.get<expr_function_call_ptr>()->expr_type;

	default:
		assert(false);
		fatal_error("");
	}
}

} // namespace ast
