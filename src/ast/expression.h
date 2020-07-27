#ifndef AST_EXPRESSION_H
#define AST_EXPRESSION_H

#include "core.h"

#include "node.h"
#include "typespec.h"
#include "constant_value.h"

namespace ast
{

struct expression;

#define declare_node_type(x) struct x; using x##_ptr = std::unique_ptr<x>

declare_node_type(expr_identifier);
declare_node_type(expr_literal);
declare_node_type(expr_unary_op);
declare_node_type(expr_binary_op);
declare_node_type(expr_subscript);
declare_node_type(expr_function_call);
declare_node_type(expr_cast);

#undef declare_node_type

using expr_t = node<
	expr_identifier,
	expr_literal,
	expr_unary_op,
	expr_binary_op,
	expr_subscript,
	expr_function_call,
	expr_cast
>;

enum class expression_type_kind
{
	lvalue,
	lvalue_reference,
	rvalue,
	rvalue_reference,
	function_name,
	type_name,
};

struct unresolved_expression
{};

struct constant_expression
{
	expression_type_kind kind;
	typespec             type;
	constant_value       value;
	expr_t               expr;
};

struct dynamic_expression
{
	expression_type_kind kind;
	typespec             type;
	expr_t               expr;
};

struct tuple_expression
{
	bz::vector<expression> elems;
	~tuple_expression(void) noexcept = default;
};

struct expression : bz::variant<
	unresolved_expression,
	constant_expression,
	dynamic_expression,
	tuple_expression
>
{
	using base_t = bz::variant<
		unresolved_expression,
		constant_expression,
		dynamic_expression,
		tuple_expression
	>;

	using base_t::get;

	auto kind(void) const noexcept
	{ return this->base_t::index(); }

	template<typename T>
	bool is(void) const noexcept
	{ return this->kind() == base_t::index_of<T>; }

	lex::src_tokens src_tokens;

	template<typename ...Ts>
	expression(lex::src_tokens _src_tokens, Ts &&...ts)
		: base_t(std::forward<Ts>(ts)...),
		  src_tokens(_src_tokens)
	{}

	expression(void)
		: base_t(),
		  src_tokens{}
	{}

	lex::token_pos get_tokens_begin(void) const { return this->src_tokens.begin; }
	lex::token_pos get_tokens_pivot(void) const { return this->src_tokens.pivot; }
	lex::token_pos get_tokens_end(void) const   { return this->src_tokens.end; }

	bool is_function(void) const noexcept
	{
		auto const const_expr = this->get_if<constant_expression>();
		return const_expr
			&& const_expr->kind == expression_type_kind::function_name;
	}

	bool is_overloaded_function(void) const noexcept
	{
		auto const const_expr = this->get_if<constant_expression>();
		return const_expr
			&& const_expr->kind == expression_type_kind::function_name
			&& const_expr->type.is_empty();
	}

	bool is_typename(void) const noexcept
	{
		auto const const_expr = this->get_if<constant_expression>();
		return const_expr
			&& const_expr->kind == expression_type_kind::type_name;
	}

	ast::typespec &get_typename(void) noexcept
	{
		bz_assert(this->is_typename());
		return this->get<constant_expression>().value.get<constant_value::type>();
	}

	ast::typespec const &get_typename(void) const noexcept
	{
		bz_assert(this->is_typename());
		return this->get<constant_expression>().value.get<constant_value::type>();
	}

	std::pair<typespec const &, expression_type_kind> get_expr_type_and_kind(void) const
	{
		switch (this->kind())
		{
		case ast::expression::index_of<ast::constant_expression>:
		{
			auto &const_expr = this->get<ast::constant_expression>();
			return { const_expr.type, const_expr.kind };
		}
		case ast::expression::index_of<ast::dynamic_expression>:
		{
			auto &dyn_expr = this->get<ast::dynamic_expression>();
			return { dyn_expr.type, dyn_expr.kind };
		}

		case ast::expression::index_of<ast::tuple_expression>:
			bz_assert(false);
		default:
			return { ast::typespec(), static_cast<ast::expression_type_kind>(0) };
		}
	}

	std::pair<typespec &, expression_type_kind> get_expr_type_and_kind(void)
	{
		switch (this->kind())
		{
		case ast::expression::index_of<ast::constant_expression>:
		{
			auto &const_expr = this->get<ast::constant_expression>();
			return { const_expr.type, const_expr.kind };
		}
		case ast::expression::index_of<ast::dynamic_expression>:
		{
			auto &dyn_expr = this->get<ast::dynamic_expression>();
			return { dyn_expr.type, dyn_expr.kind };
		}

		case ast::expression::index_of<ast::tuple_expression>:
		default:
		{
			bz_assert(false);
			auto t = ast::typespec();
			return { t, static_cast<expression_type_kind>(0) };
		}
		}
	}
};


struct decl_variable;
struct decl_function;
struct function_body;


struct expr_identifier
{
	lex::token_pos identifier;
	decl_variable const *decl;

	expr_identifier(lex::token_pos _id, decl_variable const *var_decl)
		: identifier(_id), decl(var_decl)
	{}

	expr_identifier(lex::token_pos _id)
		: identifier(_id), decl(nullptr)
	{}
};

struct expr_literal
{
	lex::token_range tokens;

	expr_literal(lex::token_range _tokens)
		: tokens(_tokens)
	{}

	expr_literal(lex::token_pos it)
		: tokens{ it, it + 1 }
	{}
};

struct expr_unary_op
{
	lex::token_pos op;
	expression     expr;

	expr_unary_op(
		lex::token_pos _op,
		expression     _expr
	)
		: op     (_op),
		  expr   (std::move(_expr))
	{}
};

struct expr_binary_op
{
	lex::token_pos op;
	expression     lhs;
	expression     rhs;

	expr_binary_op(
		lex::token_pos _op,
		expression     _lhs,
		expression     _rhs
	)
		: op     (_op),
		  lhs    (std::move(_lhs)),
		  rhs    (std::move(_rhs))
	{}
};

struct expr_subscript
{
	expression             base;
	bz::vector<expression> indicies;

	expr_subscript(
		expression             _base,
		bz::vector<expression> _indicies
	)
		: base    (std::move(_base)),
		  indicies(std::move(_indicies))
	{}
};

struct expr_function_call
{
	lex::src_tokens        src_tokens;
	bz::vector<expression> params;
	function_body         *func_body;

	expr_function_call(
		lex::src_tokens        _src_tokens,
		bz::vector<expression> _params,
		function_body         *_func_body
	)
		: src_tokens(_src_tokens),
		  params    (std::move(_params)),
		  func_body (_func_body)
	{}
};

struct expr_cast
{
	lex::token_pos as_pos;
	expression     expr;
	typespec       type;

	expr_cast(
		lex::token_pos _as_pos,
		expression     _expr,
		typespec       _type
	)
		: as_pos(_as_pos),
		  expr  (std::move(_expr)),
		  type  (std::move(_type))
	{}
};


inline expression make_unresolved_expression(lex::src_tokens tokens)
{ return expression(tokens, unresolved_expression()); }

template<typename ...Args>
expression make_dynamic_expression(lex::src_tokens tokens, Args &&...args)
{ return expression(tokens, dynamic_expression{ std::forward<Args>(args)... }); }

template<typename ...Args>
expression make_constant_expression(lex::src_tokens tokens, Args &&...args)
{ return expression(tokens, constant_expression{ std::forward<Args>(args)... }); }

template<typename ...Args>
expression make_tuple_expression(lex::src_tokens tokens, Args &&...args)
{ return expression(tokens, tuple_expression{ std::forward<Args>(args)... }); }


template<typename ...Args>
expr_t make_expr_identifier(Args &&...args)
{ return expr_t(std::make_unique<expr_identifier>(std::forward<Args>(args)...)); }

template<typename ...Args>
expr_t make_expr_literal(Args &&...args)
{ return expr_t(std::make_unique<expr_literal>(std::forward<Args>(args)...)); }

template<typename ...Args>
expr_t make_expr_unary_op(Args &&...args)
{ return expr_t(std::make_unique<expr_unary_op>(std::forward<Args>(args)...)); }

template<typename ...Args>
expr_t make_expr_binary_op(Args &&...args)
{ return expr_t(std::make_unique<expr_binary_op>(std::forward<Args>(args)...)); }

template<typename ...Args>
expr_t make_expr_subscript(Args &&...args)
{ return expr_t(std::make_unique<expr_subscript>(std::forward<Args>(args)...)); }

template<typename ...Args>
expr_t make_expr_function_call(Args &&...args)
{ return expr_t(std::make_unique<expr_function_call>(std::forward<Args>(args)...)); }

template<typename ...Args>
expr_t make_expr_cast(Args &&...args)
{ return expr_t(std::make_unique<expr_cast>(std::forward<Args>(args)...)); }



} // namespace ast

#endif // AST_EXPRESSION_H
