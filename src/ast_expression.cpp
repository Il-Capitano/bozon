#include "ast_expression.h"
#include "context.h"

src_tokens::pos ast_expr_unary_op::get_tokens_begin(void) const
{ return this->__op; }

src_tokens::pos ast_expr_unary_op::get_tokens_pivot(void) const
{ return this->__op; }

src_tokens::pos ast_expr_unary_op::get_tokens_end(void) const
{ return this->expr.get_tokens_end(); }


src_tokens::pos ast_expr_binary_op::get_tokens_begin(void) const
{ return this->lhs.get_tokens_begin(); }

src_tokens::pos ast_expr_binary_op::get_tokens_pivot(void) const
{ return this->__op; }

src_tokens::pos ast_expr_binary_op::get_tokens_end(void) const
{ return this->rhs.get_tokens_end(); }


src_tokens::pos ast_expr_function_call::get_tokens_begin(void) const
{ return this->called.get_tokens_begin(); }

src_tokens::pos ast_expr_function_call::get_tokens_pivot(void) const
{ return this->called.get_tokens_end(); }

src_tokens::pos ast_expr_function_call::get_tokens_end(void) const
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


void ast_expr_identifier::resolve(void)
{
	this->typespec = context.get_identifier_type(this->identifier);
}

void ast_expr_literal::resolve(void)
{}

void ast_expr_unary_op::resolve(void)
{
	this->expr.resolve();
	this->typespec = context.get_operator_type(this->__op->kind, { get_typespec(this->expr) });
}

void ast_expr_binary_op::resolve(void)
{
	this->lhs.resolve();
	this->rhs.resolve();
	this->typespec = context.get_operator_type(
		this->__op->kind,
		{ get_typespec(this->lhs), get_typespec(this->rhs) }
	);
}

void ast_expr_function_call::resolve(void)
{
	this->called.resolve();
	for (auto &p : this->params)
	{
		p.resolve();
	}

	if (this->called.kind() == ast_expression::index<ast_expr_identifier>)
	{
		bz::vector<ast_typespec_ptr> param_types = {};
		param_types.reserve(this->params.size());

		for (auto &p : this->params)
		{
			param_types.push_back(get_typespec(p));
		}

		this->typespec = context.get_function_type(
			this->called.get<ast_expr_identifier_ptr>()->identifier->value,
			param_types
		);
	}
	else
	{
		bz::vector<ast_typespec_ptr> param_types = {};
		param_types.reserve(this->params.size() + 1);

		param_types.push_back(get_typespec(this->called));
		for (auto &p : this->params)
		{
			param_types.push_back(get_typespec(p));
		}

		this->typespec = context.get_operator_type(
			token::paren_open,
			param_types
		);
	}
}



ast_expr_literal::ast_expr_literal(src_tokens::pos stream)
	: src_pos(stream)
{
	switch(stream->kind)
	{
	case token::number_literal:
	{
		bz::string value = stream->value.get();
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
			this->_kind = ast_expr_literal::integer_number;
			this->integer_value = static_cast<uint64_t>(num);
			this->typespec = make_ast_name_typespec("int32"_is);
		}
		else
		{
			assert(value[i] == '.');
			++i;
			this->_kind = ast_expr_literal::floating_point_number;

			double level = 1;
			while (i < value.length())
			{
				assert(value[i] >= '0' && value[i] <= '9');
				level *= 0.1;
				num += level * (value[i] - '0');
				++i;
			}

			this->floating_point_value = num;
			this->typespec = make_ast_name_typespec("float64"_is);
		}
		break;
	}

	case token::string_literal:
		this->_kind = ast_expr_literal::string;
		this->string_value = stream->value;
		this->typespec = make_ast_name_typespec("str"_is);
		break;

	case token::character_literal:
		this->_kind = ast_expr_literal::character;
		assert(stream->value.length() == 1);
		this->char_value = stream->value[0];
		this->typespec = make_ast_name_typespec("char"_is);
		break;

	case token::kw_true:
		this->_kind = ast_expr_literal::bool_true;
		this->typespec = make_ast_name_typespec("bool"_is);
		break;

	case token::kw_false:
		this->_kind = ast_expr_literal::bool_false;
		this->typespec = make_ast_name_typespec("bool"_is);
		break;

	case token::kw_null:
		this->_kind = ast_expr_literal::null;
		this->typespec = make_ast_name_typespec("null_t"_is);
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

static ast_expression parse_expression(
	src_tokens::pos &stream,
	src_tokens::pos  end,
	precedence       prec = precedence{}
);

static bz::vector<ast_expression> parse_expression_comma_list(
	src_tokens::pos &stream,
	src_tokens::pos  end
);

static ast_expression parse_primary_expression(
	src_tokens::pos &stream,
	src_tokens::pos  end
)
{
	switch (stream->kind)
	{
	case token::identifier:
	{
		auto id = make_ast_expr_identifier(stream);
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
		auto literal = make_ast_expr_literal(stream);
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

		auto result = make_ast_expr_unresolved(token_range{ paren_begin, stream });
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

		auto result = make_ast_expr_unary_op(op, std::move(expr));
		return result;
	}

	default:
		bad_token(stream, "Expected primary expression");
	}
};

static ast_expression parse_expression_helper(
	ast_expression   lhs,
	src_tokens::pos &stream,
	src_tokens::pos  end,
	precedence       prec
)
{
	src_tokens::pos op = nullptr;
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
				lhs = make_ast_expr_function_call(
					std::move(lhs), bz::vector<ast_expression>{}
				);
				break;
			}

			auto params = parse_expression_comma_list(stream, end);
			assert_token(stream, token::paren_close);

			lhs = make_ast_expr_function_call(
				std::move(lhs), std::move(params)
			);
			break;
		}

		case token::square_open:
		{
			auto rhs = parse_expression(stream, end);
			assert_token(stream, token::square_close);

			lhs = make_ast_expr_binary_op(
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

			lhs = make_ast_expr_binary_op(
				op, std::move(lhs), std::move(rhs)
			);
			break;
		}
		}
	}

	return lhs;
}

static bz::vector<ast_expression> parse_expression_comma_list(
	src_tokens::pos &stream,
	src_tokens::pos  end
)
{
	bz::vector<ast_expression> exprs = {};

	exprs.emplace_back(parse_expression(stream, end, no_comma));

	while (stream != end && stream->kind == token::comma)
	{
		exprs.emplace_back(parse_expression(stream, end, no_comma));
	}

	return exprs;
}

static ast_expression parse_expression(
	src_tokens::pos &stream,
	src_tokens::pos  end,
	precedence       prec
)
{
	auto lhs = parse_primary_expression(stream, end);
	auto result = parse_expression_helper(std::move(lhs), stream, end, prec);
	return result;
}

template<>
void ast_expression::resolve(void)
{
	switch (this->kind())
	{
	case index<ast_expr_unresolved>:
	{
		auto &unresolved_expr = this->get<ast_expr_unresolved_ptr>();

		auto begin = unresolved_expr->expr.begin;
		auto end   = unresolved_expr->expr.end;

		auto expr = parse_expression(begin, end);
		expr.resolve();
		this->assign(std::move(expr));

		break;
	}

	case index<ast_expr_identifier>:
		this->get<ast_expr_identifier_ptr>()->resolve();
		break;

	case index<ast_expr_literal>:
		this->get<ast_expr_literal_ptr>()->resolve();
		break;

	case index<ast_expr_unary_op>:
		this->get<ast_expr_unary_op_ptr>()->resolve();
		break;

	case index<ast_expr_binary_op>:
		this->get<ast_expr_binary_op_ptr>()->resolve();
		break;

	case index<ast_expr_function_call>:
		this->get<ast_expr_function_call_ptr>()->resolve();
		break;

	default:
		assert(false);
		break;
	}
}

ast_typespec_ptr get_typespec(ast_expression const &expr)
{
	switch (expr.kind())
	{
	case ast_expression::index<ast_expr_unresolved>:
		assert(false);
		return nullptr;

	case ast_expression::index<ast_expr_identifier>:
		return expr.get<ast_expr_identifier_ptr>()->typespec;

	case ast_expression::index<ast_expr_literal>:
		return expr.get<ast_expr_literal_ptr>()->typespec;

	case ast_expression::index<ast_expr_unary_op>:
		return expr.get<ast_expr_unary_op_ptr>()->typespec;

	case ast_expression::index<ast_expr_binary_op>:
		return expr.get<ast_expr_binary_op_ptr>()->typespec;

	case ast_expression::index<ast_expr_function_call>:
		return expr.get<ast_expr_function_call_ptr>()->typespec;

	default:
		assert(false);
		return nullptr;
	}
}