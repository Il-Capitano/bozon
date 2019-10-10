#ifndef AST_EXPRESSION_H
#define AST_EXPRESSION_H

#include "core.h"

#include "ast_type.h"

struct ast_expression;
using ast_expression_ptr = std::unique_ptr<ast_expression>;


struct ast_expr_unresolved
{
	token_range expr;

	ast_expr_unresolved(token_range _expr)
		: expr(_expr)
	{}

	src_tokens::pos get_tokens_begin(void) const;
	src_tokens::pos get_tokens_pivot(void) const;
	src_tokens::pos get_tokens_end(void) const;
};

struct ast_expr_identifier
{
	intern_string   value;
	src_tokens::pos src_pos;

	ast_expr_identifier(src_tokens::pos _token)
		: value  (_token->value),
		  src_pos(_token)
	{}

	src_tokens::pos get_tokens_begin(void) const;
	src_tokens::pos get_tokens_pivot(void) const;
	src_tokens::pos get_tokens_end(void) const;
};

struct ast_expr_literal
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

	uint32_t _kind;

	uint32_t kind(void) const
	{ return this->_kind; }

	union
	{
		intern_string string_value;
		char          char_value;
		uint64_t      integer_value;
		double        floating_point_value;
	};
	src_tokens::pos src_pos;

	ast_expr_literal(src_tokens::pos stream);

	src_tokens::pos get_tokens_begin(void) const;
	src_tokens::pos get_tokens_pivot(void) const;
	src_tokens::pos get_tokens_end(void) const;
};

struct ast_expr_unary_op
{
	uint32_t op;
	ast_expression_ptr expr;
	src_tokens::pos op_src_pos;

	ast_expr_unary_op(src_tokens::pos _op, ast_expression_ptr _expr)
		: op        (_op->kind),
		  expr      (std::move(_expr)),
		  op_src_pos(_op)
	{}

	src_tokens::pos get_tokens_begin(void) const;
	src_tokens::pos get_tokens_pivot(void) const;
	src_tokens::pos get_tokens_end(void) const;
};

struct ast_expr_binary_op
{
	uint32_t op;
	ast_expression_ptr lhs;
	ast_expression_ptr rhs;
	src_tokens::pos op_src_pos;

	ast_expr_binary_op(src_tokens::pos _op, ast_expression_ptr _lhs, ast_expression_ptr _rhs)
		: op        (_op->kind),
		  lhs       (std::move(_lhs)),
		  rhs       (std::move(_rhs)),
		  op_src_pos(_op)
	{}

	src_tokens::pos get_tokens_begin(void) const;
	src_tokens::pos get_tokens_pivot(void) const;
	src_tokens::pos get_tokens_end(void) const;
};

struct ast_expr_function_call
{
	ast_expression_ptr             called   = nullptr;
	bz::vector<ast_expression_ptr> params   = {};

	ast_expr_function_call(ast_expression_ptr _called, bz::vector<ast_expression_ptr> _params)
		: called(std::move(_called)), params(std::move(_params))
	{}

	src_tokens::pos get_tokens_begin() const;
	src_tokens::pos get_tokens_pivot() const;
	src_tokens::pos get_tokens_end() const;
};

struct ast_expression :
bz::variant<
	ast_expr_unresolved,
	ast_expr_identifier,
	ast_expr_literal,
	ast_expr_unary_op,
	ast_expr_binary_op,
	ast_expr_function_call
>
{
	using base_t = bz::variant<
		ast_expr_unresolved,
		ast_expr_identifier,
		ast_expr_literal,
		ast_expr_unary_op,
		ast_expr_binary_op,
		ast_expr_function_call
	>;

	enum : uint32_t
	{
		unresolved       = index_of<ast_expr_unresolved>,
		identifier       = index_of<ast_expr_identifier>,
		literal          = index_of<ast_expr_literal>,
		unary_op         = index_of<ast_expr_unary_op>,
		binary_op        = index_of<ast_expr_binary_op>,
		function_call_op = index_of<ast_expr_function_call>,
	};

	uint32_t kind(void) const
	{
		return base_t::index();
	}

	using base_t::get;

	ast_typespec_ptr typespec = nullptr;

	ast_expression(ast_expr_unresolved     _unresolved  );
	ast_expression(ast_expr_identifier     _id          );
	ast_expression(ast_expr_literal        _literal     );
	ast_expression(ast_expr_unary_op       _unary_op    );
	ast_expression(ast_expr_binary_op      _binary_op   );
	ast_expression(ast_expr_function_call _func_call_op);

	void resolve(void);

	src_tokens::pos get_tokens_begin() const
	{
		src_tokens::pos pos = nullptr;
		this->visit([&pos](auto const &elem)
		{
			pos = elem.get_tokens_begin();
		});
		return pos;
	}

	src_tokens::pos get_tokens_pivot() const
	{
		src_tokens::pos pos = nullptr;
		this->visit([&pos](auto const &elem)
		{
			pos = elem.get_tokens_pivot();
		});
		return pos;
	}

	src_tokens::pos get_tokens_end() const
	{
		src_tokens::pos pos = nullptr;
		this->visit([&pos](auto const &elem)
		{
			pos = elem.get_tokens_end();
		});
		return pos;
	}
};

using ast_expression_ptr = std::unique_ptr<ast_expression>;


inline src_tokens::pos ast_expr_unresolved::get_tokens_begin(void) const
{ return this->expr.begin; }

inline src_tokens::pos ast_expr_unresolved::get_tokens_pivot(void) const
{ return this->expr.begin; }

inline src_tokens::pos ast_expr_unresolved::get_tokens_end(void) const
{ return this->expr.end; }


inline src_tokens::pos ast_expr_identifier::get_tokens_begin(void) const
{ return this->src_pos; }

inline src_tokens::pos ast_expr_identifier::get_tokens_pivot(void) const
{ return this->src_pos; }

inline src_tokens::pos ast_expr_identifier::get_tokens_end(void) const
{ return this->src_pos; }


inline src_tokens::pos ast_expr_literal::get_tokens_begin(void) const
{ return this->src_pos; }

inline src_tokens::pos ast_expr_literal::get_tokens_pivot(void) const
{ return this->src_pos; }

inline src_tokens::pos ast_expr_literal::get_tokens_end(void) const
{ return this->src_pos; }


inline src_tokens::pos ast_expr_unary_op::get_tokens_begin(void) const
{ return this->op_src_pos; }

inline src_tokens::pos ast_expr_unary_op::get_tokens_pivot(void) const
{ return this->op_src_pos; }

inline src_tokens::pos ast_expr_unary_op::get_tokens_end(void) const
{ return this->expr->get_tokens_end(); }


inline src_tokens::pos ast_expr_binary_op::get_tokens_begin(void) const
{ return this->lhs->get_tokens_begin(); }

inline src_tokens::pos ast_expr_binary_op::get_tokens_pivot(void) const
{ return this->op_src_pos; }

inline src_tokens::pos ast_expr_binary_op::get_tokens_end(void) const
{ return this->rhs->get_tokens_end(); }


inline src_tokens::pos ast_expr_function_call::get_tokens_begin(void) const
{ return this->called->get_tokens_begin(); }

inline src_tokens::pos ast_expr_function_call::get_tokens_pivot(void) const
{ return this->called->get_tokens_end(); }

inline src_tokens::pos ast_expr_function_call::get_tokens_end(void) const
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



template<typename ...Args>
auto make_ast_expression(Args &&...args)
{
	return std::make_unique<ast_expression>(std::forward<Args>(args)...);
}

template<typename ...Args>
auto make_ast_unresolved_expression(Args &&...args)
{
	return make_ast_expression(
		ast_expr_unresolved(std::forward<Args>(args)...)
	);
}

template<typename ...Args>
auto make_ast_identifier_expression(Args &&...args)
{
	return make_ast_expression(
		ast_expr_identifier(std::forward<Args>(args)...)
	);
}

template<typename ...Args>
auto make_ast_literal_expression(Args &&...args)
{
	return make_ast_expression(
		ast_expr_literal(std::forward<Args>(args)...)
	);
}

template<typename ...Args>
auto make_ast_unary_op_expression(Args &&...args)
{
	return make_ast_expression(
		ast_expr_unary_op(std::forward<Args>(args)...)
	);
}

template<typename ...Args>
auto make_ast_binary_op_expression(Args &&...args)
{
	return make_ast_expression(
		ast_expr_binary_op(std::forward<Args>(args)...)
	);
}

template<typename ...Args>
auto make_ast_function_call_expression(Args &&...args)
{
	return make_ast_expression(
		ast_expr_function_call(std::forward<Args>(args)...)
	);
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

#endif // AST_EXPRESSION_H
