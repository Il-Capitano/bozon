#ifndef AST_EXPRESSION_H
#define AST_EXPRESSION_H

#include "core.h"

#include "node.h"
#include "typespec.h"

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

	enum expr_type_kind
	{
		lvalue,
		lvalue_reference,
		rvalue,
		rvalue_reference,
		function_name,
	};

	struct expr_type_t
	{
		expr_type_kind type_kind = rvalue;
		typespec       expr_type;
	};

	expr_type_t      expr_type;
	lex::token_range tokens;

	template<typename ...Ts>
	expression(lex::token_range _tokens, Ts &&...ts)
		: base_t   (std::forward<Ts>(ts)...),
		  expr_type{},
		  tokens   (_tokens)
	{}

	expression(void)
		: base_t   (),
		  expr_type{},
		  tokens   { nullptr, nullptr }
	{}

	lex::token_pos get_tokens_begin(void) const;
	lex::token_pos get_tokens_pivot(void) const;
	lex::token_pos get_tokens_end(void) const;
};


struct decl_variable;
struct decl_function;
struct function_body;


struct expr_unresolved
{
	lex::token_range expr;

	expr_unresolved(lex::token_range _expr)
		: expr(_expr)
	{}

	lex::token_pos get_tokens_begin(void) const
	{ return this->expr.begin; }

	lex::token_pos get_tokens_pivot(void) const
	{ return this->expr.begin; }

	lex::token_pos get_tokens_end(void) const
	{ return this->expr.end; }
};

struct expr_identifier
{
	lex::token_pos identifier;
	bz::variant<decl_variable const *, decl_function const *> decl;

	expr_identifier(lex::token_pos _id, decl_variable const *var_decl)
		: identifier(_id), decl(var_decl)
	{}

	expr_identifier(lex::token_pos _id, decl_function const *func_decl)
		: identifier(_id), decl(func_decl)
	{}

	expr_identifier(lex::token_pos _id, bz::variant<decl_variable const *, decl_function const *> _decl)
		: identifier(_id), decl(_decl)
	{}

	lex::token_pos get_tokens_begin(void) const
	{ return this->identifier; }

	lex::token_pos get_tokens_pivot(void) const
	{ return this->identifier; }

	lex::token_pos get_tokens_end(void) const
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

	value_t              value;
	lex::token_pos       src_pos;
	type_info::type_kind type_kind = type_info::type_kind::int8_;

	expr_literal(lex::token_pos stream);

	lex::token_pos get_tokens_begin(void) const
	{ return this->src_pos; }

	lex::token_pos get_tokens_pivot(void) const
	{ return this->src_pos; }

	lex::token_pos get_tokens_end(void) const
	{ return this->src_pos + 1; }
};

struct expr_tuple
{
	bz::vector<expression> elems;

	expr_tuple(bz::vector<expression> _elems)
		: elems(std::move(_elems))
	{}

	lex::token_pos get_tokens_begin(void) const
	{ assert(false); return nullptr; }

	lex::token_pos get_tokens_pivot(void) const
	{ assert(false); return nullptr; }

	lex::token_pos get_tokens_end(void) const
	{ assert(false); return nullptr; }
};

struct expr_unary_op
{
	lex::token_pos op;
	expression     expr;
	function_body *op_body;

	expr_unary_op(
		lex::token_pos _op,
		expression     _expr
	)
		: op     (_op),
		  expr   (std::move(_expr)),
		  op_body(nullptr)
	{}

	lex::token_pos get_tokens_begin(void) const;
	lex::token_pos get_tokens_pivot(void) const;
	lex::token_pos get_tokens_end(void) const;
};

struct expr_binary_op
{
	lex::token_pos op;
	expression     lhs;
	expression     rhs;
	function_body *op_body;

	expr_binary_op(
		lex::token_pos _op,
		expression     _lhs,
		expression     _rhs
	)
		: op     (_op),
		  lhs    (std::move(_lhs)),
		  rhs    (std::move(_rhs)),
		  op_body(nullptr)
	{}

	lex::token_pos get_tokens_begin(void) const;
	lex::token_pos get_tokens_pivot(void) const;
	lex::token_pos get_tokens_end(void) const;
};

struct expr_function_call
{
	lex::token_pos         op;
	expression             called;
	bz::vector<expression> params;
	function_body         *func_body;

	expr_function_call(
		lex::token_pos         _op,
		expression             _called,
		bz::vector<expression> _params
	)
		: op       (_op),
		  called   (std::move(_called)),
		  params   (std::move(_params)),
		  func_body(nullptr)
	{}

	lex::token_pos get_tokens_begin() const;
	lex::token_pos get_tokens_pivot() const;
	lex::token_pos get_tokens_end() const;
};


typespec get_typespec(expression const &expr);


template<typename ...Args>
expression make_expression(Args &&...args)
{ return expression(std::forward<Args>(args)...); }

template<typename ...Args>
expression make_expr_unresolved(lex::token_range tokens, Args &&...args)
{ return expression(tokens, std::make_unique<expr_unresolved>(std::forward<Args>(args)...)); }

template<typename ...Args>
expression make_expr_identifier(lex::token_range tokens, Args &&...args)
{ return expression(tokens, std::make_unique<expr_identifier>(std::forward<Args>(args)...)); }

template<typename ...Args>
expression make_expr_literal(lex::token_range tokens, Args &&...args)
{ return expression(tokens, std::make_unique<expr_literal>(std::forward<Args>(args)...)); }

template<typename ...Args>
expression make_expr_tuple(lex::token_range tokens, Args &&...args)
{ return expression(tokens, std::make_unique<expr_tuple>(std::forward<Args>(args)...)); }

template<typename ...Args>
expression make_expr_unary_op(lex::token_range tokens, Args &&...args)
{ return expression(tokens, std::make_unique<expr_unary_op>(std::forward<Args>(args)...)); }

template<typename ...Args>
expression make_expr_binary_op(lex::token_range tokens, Args &&...args)
{ return expression(tokens, std::make_unique<expr_binary_op>(std::forward<Args>(args)...)); }

template<typename ...Args>
expression make_expr_function_call(lex::token_range tokens, Args &&...args)
{ return expression(tokens, std::make_unique<expr_function_call>(std::forward<Args>(args)...)); }

} // namespace ast

#endif // AST_EXPRESSION_H
