#ifndef CODEGEN_C_EXPR_VALUE_H
#define CODEGEN_C_EXPR_VALUE_H

#include "core.h"
#include "types.h"

namespace codegen::c
{

enum class precedence : uint8_t
{
	// https://en.cppreference.com/w/c/language/operator_precedence
	literal,
	identifier = literal,
	suffix, // '.', '->', '(...)', etc...
	prefix, // '+', '-', '&', '*', etc...
	multiply,
	divide = multiply,
	remainder = multiply,
	addition,
	subtraction = addition,
	bitshift,
	relational, // '<', '<=', '>', '>='
	equality, // '==', '!='
	bitwise_and,
	bitwise_xor,
	bitwise_or,
	logical_and,
	logical_or,
	assignment, // '=', '+=', etc...
	comma,
};

struct expr_value
{
	uint32_t value_index;
	bool needs_dereference: 1;
	bool is_const: 1;
	bool is_temporary: 1;
	bool is_variable: 1;
	bool is_rvalue: 1;
	precedence prec;
	type value_type;

	type get_type(void) const
	{
		return this->value_type;
	}
};

inline bool needs_parenthesis_unary(expr_value const &expr, precedence op_prec)
{
	// prefix unary operators are right associative, and postfix unary operators are left associative,
	// which means that we always should use '>' to compare them
	auto const expr_prec = expr.needs_dereference ? precedence::prefix : expr.prec;
	return expr_prec > op_prec;
}

struct needs_parenthesis_binary_result_t
{
	bool lhs;
	bool rhs;
};

inline needs_parenthesis_binary_result_t needs_parenthesis_binary(expr_value const &lhs, expr_value const &rhs, precedence op_prec)
{
	auto const lhs_prec = lhs.needs_dereference ? precedence::prefix : lhs.prec;
	auto const rhs_prec = rhs.needs_dereference ? precedence::prefix : rhs.prec;
	switch (op_prec)
	{
	case precedence::literal:
	case precedence::prefix:
		bz_unreachable;
	case precedence::suffix:
		bz_unreachable;

	// right associative
	case precedence::assignment:
		return {
			.lhs = lhs_prec >= op_prec,
			.rhs = rhs_prec > op_prec,
		};

	// left associative
	case precedence::multiply:
	// case precedence::divide:
	// case precedence::remainder:
	case precedence::addition:
	// case precedence::subtraction:
	case precedence::bitshift:
	case precedence::relational:
	case precedence::equality:
	case precedence::bitwise_and:
	case precedence::bitwise_xor:
	case precedence::bitwise_or:
	case precedence::logical_and:
	case precedence::logical_or:
	case precedence::comma:
		return {
			.lhs = lhs_prec > op_prec,
			.rhs = rhs_prec >= op_prec,
		};
	}
}

inline bool needs_parenthesis_binary_lhs(expr_value const &lhs, precedence op_prec)
{
	auto const lhs_prec = lhs.needs_dereference ? precedence::prefix : lhs.prec;
	switch (op_prec)
	{
	case precedence::literal:
	case precedence::prefix:
		bz_unreachable;
	case precedence::suffix:
		bz_unreachable;

	// right associative
	case precedence::assignment:
		return lhs_prec >= op_prec;

	// left associative
	case precedence::multiply:
	// case precedence::divide:
	// case precedence::remainder:
	case precedence::addition:
	// case precedence::subtraction:
	case precedence::bitshift:
	case precedence::relational:
	case precedence::equality:
	case precedence::bitwise_and:
	case precedence::bitwise_xor:
	case precedence::bitwise_or:
	case precedence::logical_and:
	case precedence::logical_or:
	case precedence::comma:
		return lhs_prec > op_prec;
	}
}

inline bool needs_parenthesis_binary_rhs(expr_value const &rhs, precedence op_prec)
{
	auto const rhs_prec = rhs.needs_dereference ? precedence::prefix : rhs.prec;
	switch (op_prec)
	{
	case precedence::literal:
	case precedence::prefix:
		bz_unreachable;
	case precedence::suffix:
		bz_unreachable;

	// right associative
	case precedence::assignment:
		return rhs_prec > op_prec;

	// left associative
	case precedence::multiply:
	// case precedence::divide:
	// case precedence::remainder:
	case precedence::addition:
	// case precedence::subtraction:
	case precedence::bitshift:
	case precedence::relational:
	case precedence::equality:
	case precedence::bitwise_and:
	case precedence::bitwise_xor:
	case precedence::bitwise_or:
	case precedence::logical_and:
	case precedence::logical_or:
	case precedence::comma:
		return rhs_prec >= op_prec;
	}
}

inline bool needs_parenthesis_for_initialization(expr_value const &init_expr, precedence op_prec)
{
	auto const init_expr_prec = init_expr.needs_dereference ? precedence::prefix : init_expr.prec;
	return init_expr_prec > op_prec;
}

} // namespace codegen::c

#endif // CODEGEN_C_EXPR_VALUE_H
