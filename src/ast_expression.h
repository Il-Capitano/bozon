#ifndef AST_EXPRESSION_H
#define AST_EXPRESSION_H

#include "core.h"

#include "first_pass_parser.h"
#include "ast_type.h"

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

struct ast_expression :
bz::variant<
	ast_identifier_ptr,
	ast_literal_ptr,
	ast_unary_op_ptr,
	ast_binary_op_ptr,
	ast_function_call_op_ptr
>
{
	using base_t = bz::variant<
		ast_identifier_ptr,
		ast_literal_ptr,
		ast_unary_op_ptr,
		ast_binary_op_ptr,
		ast_function_call_op_ptr
	>;

	enum : uint32_t
	{
		identifier       = index_of<ast_identifier_ptr>,
		literal          = index_of<ast_literal_ptr>,
		unary_op         = index_of<ast_unary_op_ptr>,
		binary_op        = index_of<ast_binary_op_ptr>,
		function_call_op = index_of<ast_function_call_op_ptr>,
	};

	uint32_t kind(void) const
	{
		return base_t::index();
	}

	ast_typespec_ptr typespec = nullptr;

	ast_expression(src_tokens::pos          stream      );
	ast_expression(ast_unary_op_ptr         unary_op    );
	ast_expression(ast_binary_op_ptr        binary_op   );
	ast_expression(ast_function_call_op_ptr func_call_op);

	src_tokens::pos get_tokens_begin() const;
	src_tokens::pos get_tokens_pivot() const;
	src_tokens::pos get_tokens_end() const;
};

using ast_expression_ptr = std::unique_ptr<ast_expression>;


struct ast_identifier
{
	intern_string   value;
	src_tokens::pos src_pos;

	ast_identifier(src_tokens::pos _token)
		: value  (_token->value),
		  src_pos(_token)
	{}

	src_tokens::pos get_tokens_begin() const
	{
		return this->src_pos;
	}

	src_tokens::pos get_tokens_pivot() const
	{
		return this->src_pos;
	}

	src_tokens::pos get_tokens_end() const
	{
		return this->src_pos;
	}
};

struct ast_literal
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

	uint32_t kind;

	union
	{
		intern_string string_value;
		char     char_value;
		uint64_t integer_value;
		double   floating_point_value;
	};
	src_tokens::pos src_pos;

	ast_literal(src_tokens::pos stream);

	src_tokens::pos get_tokens_begin() const
	{
		return this->src_pos;
	}

	src_tokens::pos get_tokens_pivot() const
	{
		return this->src_pos;
	}

	src_tokens::pos get_tokens_end() const
	{
		return this->src_pos;
	}
};

struct ast_unary_op
{
	uint32_t op;
	ast_expression_ptr expr = nullptr;
	src_tokens::pos op_src_pos;

	ast_unary_op(src_tokens::pos _op, ast_expression_ptr _expr)
		: op        (_op->kind),
		  expr      (std::move(_expr)),
		  op_src_pos(_op)
	{}

	src_tokens::pos get_tokens_begin() const
	{
		return this->op_src_pos;
	}

	src_tokens::pos get_tokens_pivot() const
	{
		return this->op_src_pos;
	}

	src_tokens::pos get_tokens_end() const
	{
		return this->expr->get_tokens_end();
	}
};

struct ast_binary_op
{
	uint32_t op;
	ast_expression_ptr lhs = nullptr;
	ast_expression_ptr rhs = nullptr;
	src_tokens::pos op_src_pos;

	ast_binary_op(src_tokens::pos _op, ast_expression_ptr _lhs, ast_expression_ptr _rhs)
		: op        (_op->kind),
		  lhs       (std::move(_lhs)),
		  rhs       (std::move(_rhs)),
		  op_src_pos(_op)
	{}

	src_tokens::pos get_tokens_begin() const
	{
		return this->lhs->get_tokens_begin();
	}

	src_tokens::pos get_tokens_pivot() const
	{
		return this->op_src_pos;
	}

	src_tokens::pos get_tokens_end() const
	{
		return this->rhs->get_tokens_end();
	}
};

struct ast_function_call_op
{
	ast_expression_ptr             called = nullptr;
	bz::vector<ast_expression_ptr> params = {};

	ast_function_call_op(ast_expression_ptr _called, bz::vector<ast_expression_ptr> _params)
		: called(std::move(_called)), params(std::move(_params))
	{}

	src_tokens::pos get_tokens_begin() const
	{
		return this->called->get_tokens_begin();
	}

	src_tokens::pos get_tokens_pivot() const
	{
		return this->called->get_tokens_end();
	}

	src_tokens::pos get_tokens_end() const
	{
		if (this->params.size() == 0)
		{
			return this->called->get_tokens_end() + 2;
		}
		else
		{
			return this->params.back()->get_tokens_end() + 1;
		}
	}
};



inline src_tokens::pos ast_expression::get_tokens_begin() const
{
	src_tokens::pos pos = nullptr;
	this->visit([&pos](auto const &elem)
	{
		pos = elem->get_tokens_begin();
	});
	return pos;
}

inline src_tokens::pos ast_expression::get_tokens_pivot() const
{
	src_tokens::pos pos = nullptr;
	this->visit([&pos](auto const &elem)
	{
		pos = elem->get_tokens_pivot();
	});
	return pos;
}

inline src_tokens::pos ast_expression::get_tokens_end() const
{
	src_tokens::pos pos = nullptr;
	this->visit([&pos](auto const &elem)
	{
		pos = elem->get_tokens_end();
	});
	return pos;
}


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


ast_expression_ptr parse_ast_expression(token_range expr);

ast_expression_ptr parse_ast_expression(
	src_tokens::pos &stream,
	src_tokens::pos  end
);

//std::vector<ast_expression_ptr> parse_ast_expression_comma_list(std::vector<token> const &expr);

#endif // AST_EXPRESSION_H
