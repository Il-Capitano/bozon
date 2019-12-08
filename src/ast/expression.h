#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "../core.h"

#include "node.h"
#include "type.h"

namespace ast
{

#define declare_node_type(x) struct x; using x##_ptr = std::unique_ptr<x>

declare_node_type(expr_unresolved);
declare_node_type(expr_identifier);
declare_node_type(expr_literal);
declare_node_type(expr_unary_op);
declare_node_type(expr_binary_op);
declare_node_type(expr_function_call);

#undef declare_node_type

using expression = node<
	expr_unresolved,
	expr_identifier,
	expr_literal,
	expr_unary_op,
	expr_binary_op,
	expr_function_call
>;

template<>
void expression::resolve(void);

struct expr_unresolved
{
	token_range expr;

	expr_unresolved(token_range _expr)
		: expr(_expr)
	{}

	src_tokens::pos get_tokens_begin(void) const
	{ return this->expr.begin; }

	src_tokens::pos get_tokens_pivot(void) const
	{ return this->expr.begin; }

	src_tokens::pos get_tokens_end(void) const
	{ return this->expr.end; }
};

struct expr_identifier
{
	src_tokens::pos  identifier;
	typespec_ptr typespec = nullptr;

	expr_identifier(src_tokens::pos _id)
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

struct expr_literal
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
	typespec_ptr typespec = nullptr;

	expr_literal(src_tokens::pos stream);

	src_tokens::pos get_tokens_begin(void) const
	{ return this->src_pos; }

	src_tokens::pos get_tokens_pivot(void) const
	{ return this->src_pos; }

	src_tokens::pos get_tokens_end(void) const
	{ return this->src_pos; }

	void resolve(void);
};

struct expr_unary_op
{
	src_tokens::pos  op;
	expression   expr;
	typespec_ptr typespec = nullptr;

	expr_unary_op(src_tokens::pos _op, expression _expr)
		: op(_op),
		  expr(std::move(_expr))
	{}

	src_tokens::pos get_tokens_begin(void) const;
	src_tokens::pos get_tokens_pivot(void) const;
	src_tokens::pos get_tokens_end(void) const;

	void resolve(void);
};

struct expr_binary_op
{
	src_tokens::pos op;
	expression  lhs;
	expression  rhs;
	typespec_ptr typespec = nullptr;

	expr_binary_op(src_tokens::pos _op, expression _lhs, expression _rhs)
		: op(_op),
		  lhs (std::move(_lhs)),
		  rhs (std::move(_rhs))
	{}

	src_tokens::pos get_tokens_begin(void) const;
	src_tokens::pos get_tokens_pivot(void) const;
	src_tokens::pos get_tokens_end(void) const;

	void resolve(void);
};

struct expr_function_call
{
	expression             called;
	bz::vector<expression> params;
	typespec_ptr typespec = nullptr;

	expr_function_call(expression _called, bz::vector<expression> _params)
		: called(std::move(_called)),
		  params(std::move(_params))
	{}

	src_tokens::pos get_tokens_begin() const;
	src_tokens::pos get_tokens_pivot() const;
	src_tokens::pos get_tokens_end() const;

	void resolve(void);
};


typespec_ptr get_typespec(expression const &expr);


template<typename ...Args>
expression make_expression(Args &&...args)
{ return expression(std::forward<Args>(args)...); }

template<typename ...Args>
expression make_expr_unresolved(Args &&...args)
{ return expression(std::make_unique<expr_unresolved>(std::forward<Args>(args)...)); }

template<typename ...Args>
expression make_expr_identifier(Args &&...args)
{ return expression(std::make_unique<expr_identifier>(std::forward<Args>(args)...)); }

template<typename ...Args>
expression make_expr_literal(Args &&...args)
{ return expression(std::make_unique<expr_literal>(std::forward<Args>(args)...)); }

template<typename ...Args>
expression make_expr_unary_op(Args &&...args)
{ return expression(std::make_unique<expr_unary_op>(std::forward<Args>(args)...)); }

template<typename ...Args>
expression make_expr_binary_op(Args &&...args)
{ return expression(std::make_unique<expr_binary_op>(std::forward<Args>(args)...)); }

template<typename ...Args>
expression make_expr_function_call(Args &&...args)
{ return expression(std::make_unique<expr_function_call>(std::forward<Args>(args)...)); }

} // namespace ast

#endif // EXPRESSION_H
