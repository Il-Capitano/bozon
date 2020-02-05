#ifndef PARSER_H
#define PARSER_H

#include "ast/expression.h"
#include "ast/statement.h"
#include "first_pass_parser.h"
#include "parse_context.h"


struct precedence
{
	int value;
	bool is_left_associative;

	constexpr precedence(void)
		: value(-1), is_left_associative(true)
	{}

	constexpr precedence(int val, bool assoc)
		: value(val), is_left_associative(assoc)
	{}
};

constexpr bool operator < (precedence lhs, precedence rhs)
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

constexpr bool operator <= (precedence lhs, precedence rhs)
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

ast::expression parse_expression(
	token_pos &stream, token_pos end,
	bz::vector<error> &errors,
	precedence prec = precedence{}
);

ast::typespec parse_typespec(
	token_pos &stream, token_pos end,
	bz::vector<error> &errors
);

#endif // PARSER_H
