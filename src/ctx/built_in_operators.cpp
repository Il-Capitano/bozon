#include "built_in_operators.h"
#include "global_context.h"
#include "parse_context.h"

namespace ctx
{

static bool is_integer_kind(ast::type_info::type_kind kind)
{
	return kind >= ast::type_info::type_kind::int8_
		&& kind <= ast::type_info::type_kind::uint64_;
}

static bool is_unsigned_integer_kind(ast::type_info::type_kind kind)
{
	return kind >= ast::type_info::type_kind::uint8_
		&& kind <= ast::type_info::type_kind::uint64_;
}

static bool is_signed_integer_kind(ast::type_info::type_kind kind)
{
	return kind >= ast::type_info::type_kind::int8_
		&& kind <= ast::type_info::type_kind::int64_;
}

static bool is_floating_point_kind(ast::type_info::type_kind kind)
{
	return kind == ast::type_info::type_kind::float32_
		|| kind == ast::type_info::type_kind::float64_;
}

static bool is_arithmetic_kind(ast::type_info::type_kind kind)
{
	return kind >= ast::type_info::type_kind::int8_
		&& kind <= ast::type_info::type_kind::float64_;
}

static auto get_base_kinds(
	ast::typespec const &lhs_t,
	ast::typespec const &rhs_t
) -> std::pair<ast::type_info::type_kind, ast::type_info::type_kind>
{
	assert(lhs_t.is<ast::ts_base_type>());
	assert(rhs_t.is<ast::ts_base_type>());
	return {
		lhs_t.get<ast::ts_base_type_ptr>()->info->kind,
		rhs_t.get<ast::ts_base_type_ptr>()->info->kind
	};
};


auto get_non_overloadable_operation_type(
	ast::expression::expr_type_t const &expr,
	uint32_t op,
	parse_context &context
) -> bz::result<ast::expression::expr_type_t, bz::string>
{
	using expr_type_t = ast::expression::expr_type_t;
	switch (op)
	{
	case lex::token::address_of:
		if (
			expr.type_kind == ast::expression::lvalue
			|| expr.type_kind == ast::expression::lvalue_reference
		)
		{
			return expr_type_t{
				ast::expression::rvalue,
				ast::make_ts_pointer(expr.expr_type)
			};
		}
		else
		{
			return bz::string("cannot take address of an rvalue");
		}
	case lex::token::kw_sizeof:
	{
		return expr_type_t{ ast::expression::rvalue, ast::make_ts_base_type(context.get_type_info("uint64")) };
	}

	default:
		assert(false);
		return bz::string("");
	}
}


static ast::expression::expr_type_t get_built_in_unary_plus(
	ast::expression::expr_type_t const &expr
)
{
	using expr_type_t = ast::expression::expr_type_t;

	auto &expr_t = ast::remove_const(expr.expr_type);
	if (!expr_t.is<ast::ts_base_type>())
	{
		return {};
	}
	auto const kind = expr_t.get<ast::ts_base_type_ptr>()->info->kind;
	if (is_arithmetic_kind(kind))
	{
		return expr_type_t{ ast::expression::rvalue, expr_t };
	}
	else
	{
		return {};
	}
}

static ast::expression::expr_type_t get_built_in_unary_minus(
	ast::expression::expr_type_t const &expr
)
{
	using expr_type_t = ast::expression::expr_type_t;

	auto &expr_t = remove_const(expr.expr_type);
	if (!expr_t.is<ast::ts_base_type>())
	{
		return {};
	}
	auto const kind = expr_t.get<ast::ts_base_type_ptr>()->info->kind;
	if (is_signed_integer_kind(kind) || is_floating_point_kind(kind))
	{
		return expr_type_t{ ast::expression::rvalue, expr_t };
	}
	else
	{
		return {};
	}
}

static ast::expression::expr_type_t get_built_in_unary_dereference(
	ast::expression::expr_type_t const &expr
)
{
	using expr_type_t = ast::expression::expr_type_t;

	auto &expr_t = remove_const(expr.expr_type);
	if (!expr_t.is<ast::ts_pointer>())
	{
		return {};
	}

	return expr_type_t{
		ast::expression::lvalue_reference,
		expr_t.get<ast::ts_pointer_ptr>()->base
	};
}

static ast::expression::expr_type_t get_built_in_unary_bit_not(
	ast::expression::expr_type_t const &expr
)
{
	using expr_type_t = ast::expression::expr_type_t;

	auto &expr_t = remove_const(expr.expr_type);
	if (!expr_t.is<ast::ts_base_type>())
	{
		return {};
	}

	auto const kind = expr_t.get<ast::ts_base_type_ptr>()->info->kind;
	if (is_unsigned_integer_kind(kind))
	{
		return expr_type_t{ ast::expression::rvalue, expr_t };
	}
	else
	{
		return {};
	}
}

static ast::expression::expr_type_t get_built_in_unary_bool_not(
	ast::expression::expr_type_t const &expr
)
{
	using expr_type_t = ast::expression::expr_type_t;

	auto &expr_t = remove_const(expr.expr_type);
	if (!expr_t.is<ast::ts_base_type>())
	{
		return {};
	}

	auto const kind = expr_t.get<ast::ts_base_type_ptr>()->info->kind;
	if (kind == ast::type_info::type_kind::bool_)
	{
		return expr_type_t{ ast::expression::rvalue, expr_t };
	}
	else
	{
		return {};
	}
}

static ast::expression::expr_type_t get_built_in_unary_plus_plus(
	ast::expression::expr_type_t const &expr
)
{
	using expr_type_t = ast::expression::expr_type_t;

	if (
		expr.type_kind != ast::expression::lvalue
		&& expr.type_kind != ast::expression::lvalue_reference
	)
	{
		return {};
	}

	auto &expr_t = expr.expr_type;
	if (expr_t.is<ast::ts_pointer>())
	{
		return expr_type_t{ ast::expression::lvalue_reference, expr_t };
	}
	else if (expr_t.is<ast::ts_base_type>())
	{
		auto kind = expr_t.get<ast::ts_base_type_ptr>()->info->kind;
		if (is_integer_kind(kind))
		{
			return expr_type_t{ ast::expression::lvalue_reference, expr_t };
		}
		else
		{
			return {};
		}
	}
	else
	{
		return {};
	}
}

static ast::expression::expr_type_t get_built_in_unary_minus_minus(
	ast::expression::expr_type_t const &expr
)
{
	using expr_type_t = ast::expression::expr_type_t;

	if (
		expr.type_kind != ast::expression::lvalue
		&& expr.type_kind != ast::expression::lvalue_reference
	)
	{
		return {};
	}

	auto &expr_t = expr.expr_type;
	if (expr_t.is<ast::ts_pointer>())
	{
		return expr_type_t{ ast::expression::lvalue_reference, expr_t };
	}
	else if (expr_t.is<ast::ts_base_type>())
	{
		auto kind = expr_t.get<ast::ts_base_type_ptr>()->info->kind;
		if (is_integer_kind(kind))
		{
			return expr_type_t{ ast::expression::lvalue_reference, expr_t };
		}
		else
		{
			return {};
		}
	}
	else
	{
		return {};
	}
}

auto get_built_in_operation_type(
	ast::expression::expr_type_t const &expr,
	uint32_t op,
	parse_context &
) -> ast::expression::expr_type_t
{
	switch (op)
	{
	case lex::token::plus:
		return get_built_in_unary_plus(expr);
	case lex::token::minus:
		return get_built_in_unary_minus(expr);
	case lex::token::dereference:
		return get_built_in_unary_dereference(expr);
	case lex::token::bit_not:
		return get_built_in_unary_bit_not(expr);
	case lex::token::bool_not:
		return get_built_in_unary_bool_not(expr);
	case lex::token::plus_plus:
		return get_built_in_unary_plus_plus(expr);
	case lex::token::minus_minus:
		return get_built_in_unary_minus_minus(expr);

	default:
		assert(false);
		return {};
	}
}

static auto get_built_in_binary_plus(
	ast::expression::expr_type_t const &lhs,
	ast::expression::expr_type_t const &rhs
) -> ast::expression::expr_type_t
{
	using expr_type_t = ast::expression::expr_type_t;

	auto &lhs_t = ast::remove_const(lhs.expr_type);
	auto &rhs_t = ast::remove_const(rhs.expr_type);

	if (
		lhs_t.is<ast::ts_base_type>()
		&& rhs_t.is<ast::ts_base_type>()
	)
	{
		auto [lhs_kind, rhs_kind] = get_base_kinds(lhs_t, rhs_t);
		if (
			is_signed_integer_kind(lhs_kind)
			&& is_signed_integer_kind(rhs_kind)
		)
		{
			return lhs_kind > rhs_kind
				? expr_type_t{ ast::expression::rvalue, lhs_t }
				: expr_type_t{ ast::expression::rvalue, rhs_t };
		}
		else if (
			is_unsigned_integer_kind(lhs_kind)
			&& is_unsigned_integer_kind(rhs_kind)
		)
		{
			return lhs_kind > rhs_kind
				? expr_type_t{ ast::expression::rvalue, lhs_t }
				: expr_type_t{ ast::expression::rvalue, rhs_t };
		}
		else if (
			is_floating_point_kind(lhs_kind)
			&& is_floating_point_kind(rhs_kind)
		)
		{
			return lhs_kind > rhs_kind
				? expr_type_t{ ast::expression::rvalue, lhs_t }
				: expr_type_t{ ast::expression::rvalue, rhs_t };
		}
		// TODO: deal with chars
		else
		{
			return {};
		}
	}
	else if (
		lhs_t.is<ast::ts_pointer>()
		&& rhs_t.is<ast::ts_base_type>()
		&& is_integer_kind(rhs_t.get<ast::ts_base_type_ptr>()->info->kind)
	)
	{
		return expr_type_t{ ast::expression::rvalue, lhs_t };
	}
	else if (
		rhs_t.is<ast::ts_pointer>()
		&& lhs_t.is<ast::ts_base_type>()
		&& is_integer_kind(lhs_t.get<ast::ts_base_type_ptr>()->info->kind)
	)
	{
		return expr_type_t{ ast::expression::rvalue, rhs_t };
	}
	else
	{
		return {};
	}
}

auto get_built_in_binary_minus(
	ast::expression::expr_type_t const &lhs,
	ast::expression::expr_type_t const &rhs,
	parse_context &context
) -> ast::expression::expr_type_t
{
	using expr_type_t = ast::expression::expr_type_t;

	auto &lhs_t = ast::remove_const(lhs.expr_type);
	auto &rhs_t = ast::remove_const(rhs.expr_type);

	if (
		lhs_t.is<ast::ts_base_type>()
		&& rhs_t.is<ast::ts_base_type>()
	)
	{
		auto const [lhs_kind, rhs_kind] = get_base_kinds(lhs_t, rhs_t);
		if (
			is_signed_integer_kind(lhs_kind)
			&& is_signed_integer_kind(rhs_kind)
		)
		{
			return lhs_kind > rhs_kind
				? expr_type_t{ ast::expression::rvalue, lhs_t }
				: expr_type_t{ ast::expression::rvalue, rhs_t };
		}
		else if (
			is_unsigned_integer_kind(lhs_kind)
			&& is_unsigned_integer_kind(rhs_kind)
		)
		{
			assert(false);
			return {};
		}
		else if (
			is_floating_point_kind(lhs_kind)
			&& is_floating_point_kind(rhs_kind)
		)
		{
			return lhs_kind > rhs_kind
				? expr_type_t{ ast::expression::rvalue, lhs_t }
				: expr_type_t{ ast::expression::rvalue, rhs_t };
		}
		// TODO: deal with chars
		else
		{
			return {};
		}
	}
	else if (
		lhs_t.is<ast::ts_pointer>()
		&& rhs_t.is<ast::ts_base_type>()
	)
	{
		if (is_integer_kind(rhs_t.get<ast::ts_base_type_ptr>()->info->kind))
		{
			return expr_type_t{ ast::expression::rvalue, lhs_t };
		}
		else
		{
			return {};
		}
	}
	else if (
		lhs_t.is<ast::ts_pointer>()
		&& rhs_t.is<ast::ts_pointer>()
	)
	{
		// TODO: use some kind of are_matchable_types here
		if (lhs_t.get<ast::ts_pointer_ptr>()->base == rhs_t.get<ast::ts_pointer_ptr>()->base)
		{
			return expr_type_t{ ast::expression::rvalue, ast::make_ts_base_type(context.get_type_info("int64")) };
		}
		else
		{
			return {};
		}
	}
	else
	{
		return {};
	}
}

auto get_built_in_operation_type(
	ast::expression::expr_type_t const &lhs,
	ast::expression::expr_type_t const &rhs,
	uint32_t op,
	parse_context &context
) -> ast::expression::expr_type_t
{
	switch (op)
	{
	case lex::token::plus:               // '+'
		return get_built_in_binary_plus(lhs, rhs);
	case lex::token::minus:              // '-'
		return get_built_in_binary_minus(lhs, rhs, context);

	case lex::token::assign:             // '='
	case lex::token::plus_eq:            // '+='
	case lex::token::minus_eq:           // '-='
	case lex::token::multiply:           // '*'
	case lex::token::multiply_eq:        // '*='
	case lex::token::divide:             // '/'
	case lex::token::divide_eq:          // '/='
	case lex::token::modulo:             // '%'
	case lex::token::modulo_eq:          // '%='
	case lex::token::dot_dot:            // '..'
	case lex::token::dot_dot_eq:         // '..='
	case lex::token::equals:             // '=='
	case lex::token::not_equals:         // '!='
	case lex::token::less_than:          // '<'
	case lex::token::less_than_eq:       // '<='
	case lex::token::greater_than:       // '>'
	case lex::token::greater_than_eq:    // '>='
	case lex::token::bit_and:            // '&'
	case lex::token::bit_and_eq:         // '&='
	case lex::token::bit_xor:            // '^'
	case lex::token::bit_xor_eq:         // '^='
	case lex::token::bit_or:             // '|'
	case lex::token::bit_or_eq:          // '|='
	case lex::token::bit_left_shift:     // '<<'
	case lex::token::bit_left_shift_eq:  // '<<='
	case lex::token::bit_right_shift:    // '>>'
	case lex::token::bit_right_shift_eq: // '>>='
	case lex::token::bool_and:           // '&&'
	case lex::token::bool_xor:           // '^^'
	case lex::token::bool_or:            // '||'
	case lex::token::arrow:              // '->' ???
	case lex::token::square_open:        // '[]'
	default:
		assert(false);
		return {};
	}
}

} // namespace ctx
