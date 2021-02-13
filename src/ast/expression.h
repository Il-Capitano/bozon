#ifndef AST_EXPRESSION_H
#define AST_EXPRESSION_H

#include "core.h"

#include "node.h"
#include "typespec.h"
#include "constant_value.h"

namespace ast
{

struct expression;

struct expr_identifier;
struct expr_literal;
struct expr_tuple;
struct expr_unary_op;
struct expr_binary_op;
struct expr_subscript;
struct expr_function_call;
struct expr_cast;
struct expr_struct_init;
struct expr_member_access;
struct expr_compound;
struct expr_if;


using expr_t = node<
	expr_identifier,
	expr_literal,
	expr_tuple,
	expr_unary_op,
	expr_binary_op,
	expr_subscript,
	expr_function_call,
	expr_cast,
	expr_struct_init,
	expr_member_access,
	expr_compound,
	expr_if
>;

enum class expression_type_kind
{
	lvalue,
	lvalue_reference,
	rvalue,
	rvalue_reference,
	function_name,
	type_name,
	tuple,
	none,
};


constexpr bool is_lvalue(expression_type_kind kind)
{
	return kind == expression_type_kind::lvalue
		|| kind == expression_type_kind::lvalue_reference;
}

constexpr bool is_rvalue(expression_type_kind kind)
{
	return kind == expression_type_kind::rvalue
		|| kind == expression_type_kind::rvalue_reference;
}

struct unresolved_expression
{};

struct constant_expression
{
	expression_type_kind kind;
	typespec             type;
	constant_value       value;
	expr_t               expr;

	declare_default_5(constant_expression)
};

struct dynamic_expression
{
	expression_type_kind kind;
	typespec             type;
	expr_t               expr;

	declare_default_5(dynamic_expression)
};

struct expression : bz::variant<
	unresolved_expression,
	constant_expression,
	dynamic_expression
>
{
	using base_t = bz::variant<
		unresolved_expression,
		constant_expression,
		dynamic_expression
	>;

	using base_t::get;

	auto kind(void) const noexcept
	{ return this->base_t::index(); }

	template<typename T>
	bool is(void) const noexcept
	{ return this->kind() == base_t::index_of<T>; }

	enum : int
	{
		consteval_failed,
		consteval_never_tried,
		consteval_succeeded,
	};

	lex::src_tokens src_tokens;
	int consteval_state = consteval_never_tried;
	int paren_level = 0;

	declare_default_5(expression)

	template<typename ...Ts>
	expression(lex::src_tokens _src_tokens, Ts &&...ts)
		: base_t(std::forward<Ts>(ts)...),
		  src_tokens(_src_tokens),
		  consteval_state(this->is<ast::constant_expression>() ? consteval_succeeded : consteval_never_tried),
		  paren_level(0)
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

	typespec &get_typename(void) noexcept
	{
		bz_assert(this->is_typename());
		return this->get<constant_expression>().value.get<constant_value::type>();
	}

	typespec const &get_typename(void) const noexcept
	{
		bz_assert(this->is_typename());
		return this->get<constant_expression>().value.get<constant_value::type>();
	}

	bool is_tuple(void) const noexcept
	{
		return (this->is<constant_expression>() && this->get<constant_expression>().kind == expression_type_kind::tuple)
			|| (this->is<dynamic_expression>() && this->get<dynamic_expression>().kind == expression_type_kind::tuple);
	}

	expr_tuple &get_tuple(void) noexcept
	{
		bz_assert(this->is_tuple());
		if (this->is<constant_expression>())
		{
			return this->get<constant_expression>().expr.get<expr_tuple>();
		}
		else
		{
			return this->get<dynamic_expression>().expr.get<expr_tuple>();
		}
	}

	std::pair<typespec const &, expression_type_kind> get_expr_type_and_kind(void) const noexcept
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
		default:
			bz_unreachable;
			return { ast::typespec(), static_cast<ast::expression_type_kind>(0) };
		}
	}

	std::pair<typespec &, expression_type_kind> get_expr_type_and_kind(void) noexcept
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
		default:
		{
			bz_unreachable;
			auto t = ast::typespec();
			return { t, static_cast<expression_type_kind>(0) };
		}
		}
	}

	bool is_constant_or_dynamic(void) const noexcept
	{
		return this->is<constant_expression>() || this->is<dynamic_expression>();
	}

	bool is_none(void) const noexcept
	{
		return this->is_constant_or_dynamic()
			&& this->get_expr_type_and_kind().second == expression_type_kind::none;
	}

	bool has_consteval_succeeded(void) const noexcept
	{
		return this->consteval_state == consteval_succeeded;
	}

	bool has_consteval_failed(void) const noexcept
	{
		return this->consteval_state == consteval_failed;
	}

	expr_t &get_expr(void)
	{
		bz_assert(this->is<constant_expression>() || this->is<dynamic_expression>());
		if (this->is<constant_expression>())
		{
			return this->get<constant_expression>().expr;
		}
		else
		{
			return this->get<dynamic_expression>().expr;
		}
	}

	expr_t const &get_expr(void) const
	{
		bz_assert(this->is<constant_expression>() || this->is<dynamic_expression>());
		if (this->is<constant_expression>())
		{
			return this->get<constant_expression>().expr;
		}
		else
		{
			return this->get<dynamic_expression>().expr;
		}
	}

	bool is_top_level_compound_or_if(void) const noexcept
	{
		if (!this->is_constant_or_dynamic())
		{
			return false;
		}
		auto &expr = this->get_expr();
		return (expr.is<expr_compound>() && this->src_tokens.begin->kind == lex::token::curly_open)
			|| (expr.is<expr_if>() && this->src_tokens.begin->kind == lex::token::kw_if);
	}
};


struct decl_variable;
struct decl_function;
struct function_body;


struct expr_identifier
{
	lex::token_pos identifier;
	decl_variable const *decl;

	declare_default_5(expr_identifier)

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

	declare_default_5(expr_literal)

	expr_literal(lex::token_range _tokens)
		: tokens(_tokens)
	{}

	expr_literal(lex::token_pos it)
		: tokens{ it, it + 1 }
	{}
};

struct expr_tuple
{
	bz::vector<expression> elems;

	declare_default_5(expr_tuple)

	expr_tuple(bz::vector<expression> _elems)
		: elems(std::move(_elems))
	{}
};

struct expr_unary_op
{
	lex::token_pos op;
	expression     expr;

	declare_default_5(expr_unary_op)

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

	declare_default_5(expr_binary_op)

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
	expression base;
	expression index;

	declare_default_5(expr_subscript)

	expr_subscript(
		expression _base,
		expression _index
	)
		: base (std::move(_base)),
		  index(std::move(_index))
	{}
};

enum class resolve_order
{
	regular, reversed,
};

struct expr_function_call
{
	lex::src_tokens        src_tokens;
	bz::vector<expression> params;
	function_body         *func_body;
	resolve_order          param_resolve_order;

	declare_default_5(expr_function_call)

	expr_function_call(
		lex::src_tokens        _src_tokens,
		bz::vector<expression> _params,
		function_body         *_func_body,
		resolve_order          _param_resolve_order
	)
		: src_tokens(_src_tokens),
		  params    (std::move(_params)),
		  func_body (_func_body),
		  param_resolve_order(_param_resolve_order)
	{}
};

struct expr_cast
{
	expression     expr;
	typespec       type;

	declare_default_5(expr_cast)

	expr_cast(
		expression _expr,
		typespec   _type
	)
		: expr(std::move(_expr)),
		  type(std::move(_type))
	{}
};

struct expr_struct_init
{
	bz::vector<expression> exprs;
	typespec               type;

	declare_default_5(expr_struct_init)

	expr_struct_init(
		bz::vector<expression> _exprs,
		typespec               _type
	)
		: exprs(std::move(_exprs)),
		  type (std::move(_type))
	{}
};

struct expr_member_access
{
	expression base;
	uint32_t   index;

	declare_default_5(expr_member_access)

	expr_member_access(
		expression _base,
		uint32_t   _index
	)
		: base (std::move(_base)),
		  index(_index)
	{}
};

struct statement;

struct expr_compound
{
	bz::vector<statement> statements;
	expression            final_expr;

	declare_default_5(expr_compound)

	expr_compound(
		bz::vector<statement> _statements,
		expression            _final_expr
	);
};

struct expr_if
{
	expression condition;
	expression then_block;
	expression else_block;

	declare_default_5(expr_if)

	expr_if(
		expression _condition,
		expression _then_block,
		expression _else_block
	)
		: condition (std::move(_condition)),
		  then_block(std::move(_then_block)),
		  else_block(std::move(_else_block))
	{}

	expr_if(
		expression _condition,
		expression _then_block
	)
		: condition (std::move(_condition)),
		  then_block(std::move(_then_block)),
		  else_block()
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


#define def_make_fn(ret_type, node_type)                                       \
template<typename ...Args>                                                     \
ret_type make_ ## node_type (Args &&...args)                                   \
{ return ret_type(make_ast_unique<node_type>(std::forward<Args>(args)...)); }

def_make_fn(expr_t, expr_identifier)
def_make_fn(expr_t, expr_literal)
def_make_fn(expr_t, expr_tuple)
def_make_fn(expr_t, expr_unary_op)
def_make_fn(expr_t, expr_binary_op)
def_make_fn(expr_t, expr_subscript)
def_make_fn(expr_t, expr_function_call)
def_make_fn(expr_t, expr_cast)
def_make_fn(expr_t, expr_struct_init)
def_make_fn(expr_t, expr_member_access)
def_make_fn(expr_t, expr_compound)
def_make_fn(expr_t, expr_if)

#undef def_make_fn

} // namespace ast

#endif // AST_EXPRESSION_H
