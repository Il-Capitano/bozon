#ifndef AST_EXPRESSION_H
#define AST_EXPRESSION_H

#include "core.h"

#include "first_pass_parser.h"
#include "ast_type.h"

namespace expression
{
enum : uint32_t
{
	identifier,
	literal,
	unary_op,
	binary_op,
	function_call_op,
};
}

struct ast_identifier;
using ast_identifier_ptr = std::unique_ptr<ast_identifier>;
struct ast_literal;
using ast_literal_ptr = std::unique_ptr<ast_literal>;
struct ast_unary_op;
using ast_unary_op_ptr = std::unique_ptr<ast_unary_op>;
struct ast_binary_op;
using ast_binary_op_ptr = std::unique_ptr<ast_binary_op>;
struct ast_function_call_op;
using ast_function_call_op_ptr = std::unique_ptr<ast_function_call_op>;

struct ast_expression
{
	uint32_t kind;

	ast_typespec_ptr typespec = nullptr;

	ast_identifier_ptr       identifier       = nullptr;
	ast_literal_ptr          literal          = nullptr;
	ast_unary_op_ptr         unary_op         = nullptr;
	ast_binary_op_ptr        binary_op        = nullptr;
	ast_function_call_op_ptr function_call_op = nullptr;

	ast_expression(token                    _t           );
	ast_expression(ast_unary_op_ptr         _unary_op    );
	ast_expression(ast_binary_op_ptr        _binary_op   );
	ast_expression(ast_function_call_op_ptr _func_call_op);
};

using ast_expression_ptr = std::unique_ptr<ast_expression>;


struct ast_identifier
{
	intern_string value;

	ast_identifier(intern_string _value)
		: value(std::move(_value))
	{}
};

namespace literal
{
enum : uint32_t
{
	integer_number,
	floating_point_number,
	string,
	character,
	bool_true,
	bool_false,
	null,
};
}

struct ast_literal
{
	uint32_t kind;

	union
	{
		intern_string string_value;
		char     char_value;
		uint64_t integer_value;
		double   floating_point_value;
	};

	ast_literal(token t);
};

struct ast_unary_op
{
	uint32_t op;
	ast_expression_ptr expr = nullptr;

	ast_unary_op(uint32_t _op, ast_expression_ptr _expr)
		: op(_op), expr(std::move(_expr))
	{}
};

struct ast_binary_op
{
	uint32_t op;
	ast_expression_ptr lhs = nullptr;
	ast_expression_ptr rhs = nullptr;

	ast_binary_op(uint32_t _op, ast_expression_ptr _lhs, ast_expression_ptr _rhs)
		: op(std::move(_op)), lhs(std::move(_lhs)), rhs(std::move(_rhs))
	{}
};

struct ast_function_call_op
{
	ast_expression_ptr              called = nullptr;
	std::vector<ast_expression_ptr> params = {};

	ast_function_call_op(ast_expression_ptr _called, std::vector<ast_expression_ptr> _params)
		: called(std::move(_called)), params(std::move(_params))
	{}
};




template<typename... Args>
ast_identifier_ptr make_ast_identifier(Args &&... args)
{
	return std::make_unique<ast_identifier>(std::forward<Args>(args)...);
}

template<typename... Args>
ast_literal_ptr make_ast_literal(Args &&... args)
{
	return std::make_unique<ast_literal>(std::forward<Args>(args)...);
}

template<typename... Args>
ast_unary_op_ptr make_ast_unary_op(Args &&... args)
{
	return std::make_unique<ast_unary_op>(std::forward<Args>(args)...);
}

template<typename... Args>
ast_binary_op_ptr make_ast_binary_op(Args &&... args)
{
	return std::make_unique<ast_binary_op>(std::forward<Args>(args)...);
}

template<typename... Args>
ast_function_call_op_ptr make_ast_function_call_op(Args &&... args)
{
	return std::make_unique<ast_function_call_op>(std::forward<Args>(args)...);
}

template<typename... Args>
ast_expression_ptr make_ast_expression(Args &&... args)
{
	return std::make_unique<ast_expression>(std::forward<Args>(args)...);
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


ast_expression_ptr parse_ast_expression(std::vector<token> const &expr);

ast_expression_ptr parse_ast_expression(
	std::vector<token>::const_iterator &stream,
	std::vector<token>::const_iterator &end
);

std::vector<ast_expression_ptr> parse_ast_expression_comma_list(std::vector<token> const &expr);

#endif // AST_EXPRESSION_H
