#ifndef AST_EXPRESSION_H
#define AST_EXPRESSION_H

#include "core.h"

#include "ast_type.h"

struct ast_expr_unresolved;
using ast_expr_unresolved_ptr = std::unique_ptr<ast_expr_unresolved>;
struct ast_expr_identifier;
using ast_expr_identifier_ptr = std::unique_ptr<ast_expr_identifier>;
struct ast_expr_literal;
using ast_expr_literal_ptr = std::unique_ptr<ast_expr_literal>;
struct ast_expr_unary_op;
using ast_expr_unary_op_ptr = std::unique_ptr<ast_expr_unary_op>;
struct ast_expr_binary_op;
using ast_expr_binary_op_ptr = std::unique_ptr<ast_expr_binary_op>;
struct ast_expr_function_call_op;
using ast_expr_function_call_op_ptr = std::unique_ptr<ast_expr_function_call_op>;

struct ast_expression :
bz::variant<
	ast_expr_unresolved_ptr,
	ast_expr_identifier_ptr,
	ast_expr_literal_ptr,
	ast_expr_unary_op_ptr,
	ast_expr_binary_op_ptr,
	ast_expr_function_call_op_ptr
>
{
	using base_t = bz::variant<
		ast_expr_unresolved_ptr,
		ast_expr_identifier_ptr,
		ast_expr_literal_ptr,
		ast_expr_unary_op_ptr,
		ast_expr_binary_op_ptr,
		ast_expr_function_call_op_ptr
	>;

	enum : uint32_t
	{
		unresolved       = index_of<ast_expr_unresolved_ptr>,
		identifier       = index_of<ast_expr_identifier_ptr>,
		literal          = index_of<ast_expr_literal_ptr>,
		unary_op         = index_of<ast_expr_unary_op_ptr>,
		binary_op        = index_of<ast_expr_binary_op_ptr>,
		function_call_op = index_of<ast_expr_function_call_op_ptr>,
	};

	uint32_t kind(void) const
	{
		return base_t::index();
	}

	using base_t::get;

	ast_typespec_ptr typespec = nullptr;

	ast_expression(ast_expr_unresolved_ptr _unresolved)
		: base_t(std::move(_unresolved))
	{}

	ast_expression(ast_expr_identifier_ptr       _id          );
	ast_expression(ast_expr_literal_ptr          _literal     );
	ast_expression(ast_expr_unary_op_ptr         _unary_op    );
	ast_expression(ast_expr_binary_op_ptr        _binary_op   );
	ast_expression(ast_expr_function_call_op_ptr _func_call_op);

	void resolve(void);

	src_tokens::pos get_tokens_begin() const;
	src_tokens::pos get_tokens_pivot() const;
	src_tokens::pos get_tokens_end() const;
};

using ast_expression_ptr = std::unique_ptr<ast_expression>;


struct ast_expr_unresolved
{
	token_range expr;

	ast_expr_unresolved(token_range _expr)
		: expr(_expr)
	{}

	src_tokens::pos get_tokens_begin() const
	{ return this->expr.begin; }

	src_tokens::pos get_tokens_pivot() const
	{ return this->expr.begin; }

	src_tokens::pos get_tokens_end() const
	{ return this->expr.end; }
};

struct ast_expr_identifier
{
	intern_string   value;
	src_tokens::pos src_pos;

	ast_expr_identifier(src_tokens::pos _token)
		: value  (_token->value),
		  src_pos(_token)
	{}

	src_tokens::pos get_tokens_begin() const
	{ return this->src_pos; }

	src_tokens::pos get_tokens_pivot() const
	{ return this->src_pos; }

	src_tokens::pos get_tokens_end() const
	{ return this->src_pos; }
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

	src_tokens::pos get_tokens_begin() const
	{ return this->src_pos; }

	src_tokens::pos get_tokens_pivot() const
	{ return this->src_pos; }

	src_tokens::pos get_tokens_end() const
	{ return this->src_pos; }
};

struct ast_expr_unary_op
{
	uint32_t op;
	ast_expression_ptr expr = nullptr;
	src_tokens::pos op_src_pos;

	ast_expr_unary_op(src_tokens::pos _op, ast_expression_ptr _expr)
		: op        (_op->kind),
		  expr      (std::move(_expr)),
		  op_src_pos(_op)
	{}

	src_tokens::pos get_tokens_begin() const
	{ return this->op_src_pos; }

	src_tokens::pos get_tokens_pivot() const
	{ return this->op_src_pos; }

	src_tokens::pos get_tokens_end() const
	{ return this->expr->get_tokens_end(); }
};

struct ast_expr_binary_op
{
	uint32_t op;
	ast_expression_ptr lhs = nullptr;
	ast_expression_ptr rhs = nullptr;
	src_tokens::pos op_src_pos;

	ast_expr_binary_op(src_tokens::pos _op, ast_expression_ptr _lhs, ast_expression_ptr _rhs)
		: op        (_op->kind),
		  lhs       (std::move(_lhs)),
		  rhs       (std::move(_rhs)),
		  op_src_pos(_op)
	{}

	src_tokens::pos get_tokens_begin() const
	{ return this->lhs->get_tokens_begin(); }

	src_tokens::pos get_tokens_pivot() const
	{ return this->op_src_pos; }

	src_tokens::pos get_tokens_end() const
	{ return this->rhs->get_tokens_end(); }
};

struct ast_expr_function_call_op
{
	ast_expression_ptr             called   = nullptr;
	bz::vector<ast_expression_ptr> params   = {};

	ast_expr_function_call_op(ast_expression_ptr _called, bz::vector<ast_expression_ptr> _params)
		: called(std::move(_called)), params(std::move(_params))
	{}

	src_tokens::pos get_tokens_begin() const
	{ return this->called->get_tokens_begin(); }

	src_tokens::pos get_tokens_pivot() const
	{ return this->called->get_tokens_end(); }

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


template<typename ...Args>
ast_expr_unresolved_ptr make_ast_expr_unresolved(Args &&...args)
{
	return std::make_unique<ast_expr_unresolved>(std::forward<Args>(args)...);
}

template<typename ...Args>
ast_expr_identifier_ptr make_ast_expr_identifier(Args &&...args)
{
	return std::make_unique<ast_expr_identifier>(std::forward<Args>(args)...);
}

template<typename ...Args>
ast_expr_literal_ptr make_ast_expr_literal(Args &&...args)
{
	return std::make_unique<ast_expr_literal>(std::forward<Args>(args)...);
}

template<typename ...Args>
ast_expr_unary_op_ptr make_ast_expr_unary_op(Args &&...args)
{
	return std::make_unique<ast_expr_unary_op>(std::forward<Args>(args)...);
}

template<typename ...Args>
ast_expr_binary_op_ptr make_ast_expr_binary_op(Args &&...args)
{
	return std::make_unique<ast_expr_binary_op>(std::forward<Args>(args)...);
}

template<typename ...Args>
ast_expr_function_call_op_ptr make_ast_expr_function_call_op(Args &&...args)
{
	return std::make_unique<ast_expr_function_call_op>(std::forward<Args>(args)...);
}

template<typename ...Args>
ast_expression_ptr make_ast_expression(Args &&...args)
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

#endif // AST_EXPRESSION_H
