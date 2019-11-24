#ifndef AST_EXPRESSION_H
#define AST_EXPRESSION_H

#include "core.h"

#include "ast_node.h"
#include "ast_type.h"

#define declare_node_type(x) struct x; using x##_ptr = std::unique_ptr<x>

declare_node_type(ast_expr_unresolved);
declare_node_type(ast_expr_identifier);
declare_node_type(ast_expr_literal);
declare_node_type(ast_expr_unary_op);
declare_node_type(ast_expr_binary_op);
declare_node_type(ast_expr_function_call);

#undef declare_node_type

using ast_expression = ast_node<
	ast_expr_unresolved,
	ast_expr_identifier,
	ast_expr_literal,
	ast_expr_unary_op,
	ast_expr_binary_op,
	ast_expr_function_call
>;

template<>
void ast_expression::resolve(void);

struct ast_expr_unresolved
{
	token_range expr;

	ast_expr_unresolved(token_range _expr)
		: expr(_expr)
	{}

	src_tokens::pos get_tokens_begin(void) const
	{ return this->expr.begin; }

	src_tokens::pos get_tokens_pivot(void) const
	{ return this->expr.begin; }

	src_tokens::pos get_tokens_end(void) const
	{ return this->expr.end; }
};

struct ast_expr_identifier
{
	src_tokens::pos  identifier;
	ast_typespec_ptr typespec = nullptr;

	ast_expr_identifier(src_tokens::pos _id)
		: identifier(_id),
		  typespec  (nullptr)
	{}

	src_tokens::pos get_tokens_begin(void) const
	{ return this->identifier; }

	src_tokens::pos get_tokens_pivot(void) const
	{ return this->identifier; }

	src_tokens::pos get_tokens_end(void) const
	{ return this->identifier; }

	void resolve(void);
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
		uint32_t      char_value;
		uint64_t      integer_value;
		double        floating_point_value;
	};
	src_tokens::pos src_pos;
	ast_typespec_ptr typespec = nullptr;

	ast_expr_literal(src_tokens::pos stream);

	src_tokens::pos get_tokens_begin(void) const
	{ return this->src_pos; }

	src_tokens::pos get_tokens_pivot(void) const
	{ return this->src_pos; }

	src_tokens::pos get_tokens_end(void) const
	{ return this->src_pos; }

	void resolve(void);
};

struct ast_expr_unary_op
{
	src_tokens::pos  op;
	ast_expression   expr;
	ast_typespec_ptr typespec = nullptr;

	ast_expr_unary_op(src_tokens::pos _op, ast_expression _expr)
		: op(_op),
		  expr(std::move(_expr))
	{}

	src_tokens::pos get_tokens_begin(void) const;
	src_tokens::pos get_tokens_pivot(void) const;
	src_tokens::pos get_tokens_end(void) const;

	void resolve(void);
};

struct ast_expr_binary_op
{
	src_tokens::pos op;
	ast_expression  lhs;
	ast_expression  rhs;
	ast_typespec_ptr typespec = nullptr;

	ast_expr_binary_op(src_tokens::pos _op, ast_expression _lhs, ast_expression _rhs)
		: op(_op),
		  lhs (std::move(_lhs)),
		  rhs (std::move(_rhs))
	{}

	src_tokens::pos get_tokens_begin(void) const;
	src_tokens::pos get_tokens_pivot(void) const;
	src_tokens::pos get_tokens_end(void) const;

	void resolve(void);
};

struct ast_expr_function_call
{
	ast_expression             called;
	bz::vector<ast_expression> params;
	ast_typespec_ptr typespec = nullptr;

	ast_expr_function_call(ast_expression _called, bz::vector<ast_expression> _params)
		: called(std::move(_called)),
		  params(std::move(_params))
	{}

	src_tokens::pos get_tokens_begin() const;
	src_tokens::pos get_tokens_pivot() const;
	src_tokens::pos get_tokens_end() const;

	void resolve(void);
};


ast_typespec_ptr get_typespec(ast_expression const &expr);


template<typename ...Args>
ast_expression make_ast_expression(Args &&...args)
{ return ast_expression(std::forward<Args>(args)...); }

template<typename ...Args>
ast_expression make_ast_expr_unresolved(Args &&...args)
{ return ast_expression(std::make_unique<ast_expr_unresolved>(std::forward<Args>(args)...)); }

template<typename ...Args>
ast_expression make_ast_expr_identifier(Args &&...args)
{ return ast_expression(std::make_unique<ast_expr_identifier>(std::forward<Args>(args)...)); }

template<typename ...Args>
ast_expression make_ast_expr_literal(Args &&...args)
{ return ast_expression(std::make_unique<ast_expr_literal>(std::forward<Args>(args)...)); }

template<typename ...Args>
ast_expression make_ast_expr_unary_op(Args &&...args)
{ return ast_expression(std::make_unique<ast_expr_unary_op>(std::forward<Args>(args)...)); }

template<typename ...Args>
ast_expression make_ast_expr_binary_op(Args &&...args)
{ return ast_expression(std::make_unique<ast_expr_binary_op>(std::forward<Args>(args)...)); }

template<typename ...Args>
ast_expression make_ast_expr_function_call(Args &&...args)
{ return ast_expression(std::make_unique<ast_expr_function_call>(std::forward<Args>(args)...)); }

#endif // AST_EXPRESSION_H
