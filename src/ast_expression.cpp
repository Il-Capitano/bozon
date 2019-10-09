#include "ast_expression.h"
#include "context.h"


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
		}
		break;
	}

	case token::string_literal:
		this->_kind = ast_expr_literal::string;
		this->string_value = stream->value;
		break;

	case token::character_literal:
		this->_kind = ast_expr_literal::character;
		assert(stream->value.length() == 1);
		this->char_value = stream->value[0];
		break;

	case token::kw_true:
		this->_kind = ast_expr_literal::bool_true;
		break;

	case token::kw_false:
		this->_kind = ast_expr_literal::bool_false;
		break;

	case token::kw_null:
		this->_kind = ast_expr_literal::null;
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

ast_expression::ast_expression(ast_expr_identifier_ptr _id)
	: base_t(std::move(_id))
{
	this->resolve();
}

ast_expression::ast_expression(ast_expr_literal_ptr _literal)
	: base_t(std::move(_literal))
{
	this->resolve();
}

ast_expression::ast_expression(ast_expr_unary_op_ptr _unary_op)
	: base_t(std::move(_unary_op))
{
	this->resolve();
}

ast_expression::ast_expression(ast_expr_binary_op_ptr _binary_op)
	: base_t(std::move(_binary_op))
{
	this->resolve();
}

ast_expression::ast_expression(ast_expr_function_call_op_ptr _func_call_op)
	: base_t(std::move(_func_call_op))
{
	this->resolve();
}



static ast_expression_ptr parse_expression(
	src_tokens::pos &stream,
	src_tokens::pos  end,
	precedence       prec = precedence{}
);

static bz::vector<ast_expression_ptr> parse_expression_comma_list(
	src_tokens::pos &stream,
	src_tokens::pos  end
);

static ast_expression_ptr parse_primary_expression(
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
		return make_ast_expression(std::move(id));
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
		return make_ast_expression(std::move(literal));
	}

	case token::paren_open:
	{
		++stream;
		auto paren_begin = stream;

		for (size_t paren_level = 1; stream != end && paren_level != 0; ++stream)
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

		if (stream == end)
		{
			bad_token(stream, "Expected ')'");
		}

		return make_ast_expression(
			make_ast_expr_unresolved(token_range{ paren_begin, stream })
		);
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

		return make_ast_expression(make_ast_expr_unary_op(op, std::move(expr)));
	}

	default:
		bad_token(stream, "Expected primary expression");
		return nullptr;
	}
};

static ast_expression_ptr parse_expression_helper(
	ast_expression_ptr lhs,
	src_tokens::pos   &stream,
	src_tokens::pos    end,
	precedence         prec
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
				lhs = make_ast_expression(
					make_ast_expr_function_call_op(
						std::move(lhs), bz::vector<ast_expression_ptr>{}
					)
				);
				break;
			}

			auto params = parse_expression_comma_list(stream, end);
			assert_token(stream, token::paren_close);

			lhs = make_ast_expression(
				make_ast_expr_function_call_op(
					std::move(lhs), std::move(params)
				)
			);
			break;
		}

		case token::square_open:
		{
			auto rhs = parse_expression(stream, end);
			assert_token(stream, token::square_close);

			lhs = make_ast_expression(
				make_ast_expr_binary_op(op, std::move(lhs), std::move(rhs))
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

			lhs = make_ast_expression(
				make_ast_expr_binary_op(op, std::move(lhs), std::move(rhs))
			);
			break;
		}
		}
	}

	return std::move(lhs);
}

static bz::vector<ast_expression_ptr> parse_expression_comma_list(
	src_tokens::pos &stream,
	src_tokens::pos  end
)
{
	bz::vector<ast_expression_ptr> exprs = {};

	exprs.emplace_back(parse_expression(stream, end, no_comma));

	while (stream != end && stream->kind == token::comma)
	{
		exprs.emplace_back(parse_expression(stream, end, no_comma));
	}

	return exprs;
}

static ast_expression_ptr parse_expression(
	src_tokens::pos &stream,
	src_tokens::pos  end,
	precedence       prec
)
{
	auto lhs = parse_primary_expression(stream, end);
	return parse_expression_helper(std::move(lhs), stream, end, prec);
}

void ast_expression::resolve(void)
{
	if (this->typespec != nullptr)
	{
		return;
	}

	switch (this->kind())
	{
	case unresolved:
	{
		auto &unresolved_expr = this->get<unresolved>();

		auto begin = unresolved_expr->expr.begin;
		auto end   = unresolved_expr->expr.end;

		auto expr = parse_expression(begin, end);
		*this = std::move(*expr);

		break;
	}

	case identifier:
		this->typespec = context.get_identifier_type(
			this->get<identifier>()->src_pos
		);
		break;

	case literal:
		this->typespec = [this]() -> ast_typespec_ptr
		{
			switch (this->get<literal>()->kind())
			{
			case ast_expr_literal::integer_number:
				return make_ast_typespec(ast_ts_name("int32"_is));
			case ast_expr_literal::floating_point_number:
				return make_ast_typespec(ast_ts_name("float64"_is));
			case ast_expr_literal::string:
				return make_ast_typespec(ast_ts_name("str"_is));
			case ast_expr_literal::character:
				return make_ast_typespec(ast_ts_name("char"_is));
			case ast_expr_literal::bool_true:
			case ast_expr_literal::bool_false:
				return make_ast_typespec(ast_ts_name("bool"_is));
			case ast_expr_literal::null:
				return make_ast_typespec(ast_ts_name("null_t"_is));
			default:
				assert(false);
				return nullptr;
			}
		}();
		break;

	case unary_op:
	{
		auto &unary = this->get<unary_op>();
		unary->expr->resolve();

		this->typespec = context.get_operator_type(
			unary->op, { unary->expr->typespec }
		);
		break;
	}

	case binary_op:
	{
		auto &binary = this->get<binary_op>();
		binary->lhs->resolve();
		binary->rhs->resolve();

		this->typespec = context.get_operator_type(
			binary->op, { binary->lhs->typespec, binary->rhs->typespec }
		);
		break;
	}

	case function_call_op:
	{
		auto &fn_call = this->get<function_call_op>();
		fn_call->called->resolve();

		for (auto &param : fn_call->params)
		{
			param->resolve();
		}

		if (
			fn_call->called->kind() == identifier
			&& context.is_function(fn_call->called->get<identifier>()->value)
		)
		{
			bz::vector<ast_typespec_ptr> typespecs = {};
			typespecs.reserve(fn_call->params.size());

			for (auto &param : fn_call->params)
			{
				typespecs.emplace_back(param->typespec);
			}

			this->typespec = context.get_function_type(
				fn_call->called->get<identifier>()->value, typespecs
			);
		}
		else
		{
			bz::vector<ast_typespec_ptr> typespecs = {};
			typespecs.reserve(fn_call->params.size() + 1);
			typespecs.emplace_back(fn_call->called->typespec);

			for (auto &param : fn_call->params)
			{
				typespecs.emplace_back(param->typespec);
			}

			this->typespec = context.get_operator_type(
				token::paren_open, typespecs
			);
		}
		break;
	}

	default:
		assert(false);
		break;
	}
}
