#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "../core.h"

#include "node.h"
#include "type.h"
#include "../bytecode.h"

namespace ast
{

#define declare_node_type(x) struct x; using x##_ptr = std::unique_ptr<x>

declare_node_type(expr_unresolved);
declare_node_type(expr_identifier);
declare_node_type(expr_literal);
declare_node_type(expr_tuple);
declare_node_type(expr_unary_op);
declare_node_type(expr_binary_op);
declare_node_type(expr_function_call);

#undef declare_node_type

struct expression : node<
	expr_unresolved,
	expr_identifier,
	expr_literal,
	expr_tuple,
	expr_unary_op,
	expr_binary_op,
	expr_function_call
>
{
	using base_t = node<
		expr_unresolved,
		expr_identifier,
		expr_literal,
		expr_tuple,
		expr_unary_op,
		expr_binary_op,
		expr_function_call
	>;

	using base_t::get;
	using base_t::kind;
	void resolve(void);
	void emit_bytecode(
		bz::vector<bytecode::instruction> &out,
		bz::optional<bytecode::value_pos_t> ret_pos
	);

	enum expr_type_kind
	{
		lvalue,
		lvalue_reference,
		rvalue,
		rvalue_reference,
	};

	struct expr_type_t
	{
		expr_type_kind type_kind;
		typespec expr_type;
	};

	expr_type_t expr_type;

	template<typename ...Ts>
	expression(Ts &&...ts)
		: base_t(std::forward<Ts>(ts)...)
	{}
};



struct expr_unresolved
{
	token_range expr;

	expr_unresolved(token_range _expr)
		: expr(_expr)
	{}

	token_pos get_tokens_begin(void) const
	{ return this->expr.begin; }

	token_pos get_tokens_pivot(void) const
	{ return this->expr.begin; }

	token_pos get_tokens_end(void) const
	{ return this->expr.end; }
};

struct expr_identifier
{
	token_pos identifier;

	expr_identifier(token_pos _id)
		: identifier(_id)
	{}

	token_pos get_tokens_begin(void) const
	{ return this->identifier; }

	token_pos get_tokens_pivot(void) const
	{ return this->identifier; }

	token_pos get_tokens_end(void) const
	{ return this->identifier + 1; }
};

struct expr_literal
{
	struct _bool_true {};
	struct _bool_false {};
	struct _null {};

	using value_t = bz::variant<
		uint64_t, double, bz::string, uint32_t,
		_bool_true, _bool_false, _null
	>;
	enum : uint32_t
	{
		integer_number        = value_t::index_of<uint64_t>,
		floating_point_number = value_t::index_of<double>,
		string                = value_t::index_of<bz::string>,
		character             = value_t::index_of<uint32_t>,
		bool_true             = value_t::index_of<_bool_true>,
		bool_false            = value_t::index_of<_bool_false>,
		null                  = value_t::index_of<_null>,
	};

	value_t   value;
	token_pos src_pos;

	expr_literal(token_pos stream);

	token_pos get_tokens_begin(void) const
	{ return this->src_pos; }

	token_pos get_tokens_pivot(void) const
	{ return this->src_pos; }

	token_pos get_tokens_end(void) const
	{ return this->src_pos + 1; }
};

struct expr_tuple
{
	bz::vector<expression> elems;
	token_range            tokens;

	expr_tuple(bz::vector<expression> _elems, token_range _tokens)
		: elems(std::move(_elems)), tokens(_tokens)
	{}

	token_pos get_tokens_begin(void) const
	{ return this->tokens.begin; }

	token_pos get_tokens_pivot(void) const
	{ return this->tokens.begin; }

	token_pos get_tokens_end(void) const
	{ return this->tokens.end; }
};

struct expr_unary_op
{
	token_pos  op;
	expression expr;

	expr_unary_op(token_pos _op, expression _expr)
		: op  (_op),
		  expr(std::move(_expr))
	{}

	token_pos get_tokens_begin(void) const;
	token_pos get_tokens_pivot(void) const;
	token_pos get_tokens_end(void) const;
};

struct expr_binary_op
{
	token_pos  op;
	expression lhs;
	expression rhs;

	expr_binary_op(token_pos _op, expression _lhs, expression _rhs)
		: op (_op),
		  lhs(std::move(_lhs)),
		  rhs(std::move(_rhs))
	{}

	token_pos get_tokens_begin(void) const;
	token_pos get_tokens_pivot(void) const;
	token_pos get_tokens_end(void) const;
};

struct expr_function_call
{
	token_pos              op;
	expression             called;
	bz::vector<expression> params;

	expr_function_call(token_pos _op, expression _called, bz::vector<expression> _params)
		: op    (_op),
		  called(std::move(_called)),
		  params(std::move(_params))
	{}

	token_pos get_tokens_begin() const;
	token_pos get_tokens_pivot() const;
	token_pos get_tokens_end() const;
};


typespec get_typespec(expression const &expr);


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
expression make_expr_tuple(Args &&...args)
{ return expression(std::make_unique<expr_tuple>(std::forward<Args>(args)...)); }

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
