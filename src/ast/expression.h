#ifndef AST_EXPRESSION_H
#define AST_EXPRESSION_H

#include "core.h"

#include "node.h"
#include "typespec.h"
#include "constant_value.h"
#include "identifier.h"
#include "scope.h"

namespace ast
{

struct expression;

struct expr_identifier;
struct expr_integer_literal;
struct expr_typed_literal;
struct expr_tuple;
struct expr_unary_op;
struct expr_binary_op;
struct expr_subscript;
struct expr_function_call;
struct expr_cast;
struct expr_take_reference;
struct expr_struct_init;
struct expr_array_default_construct;
struct expr_builtin_default_construct;
struct expr_member_access;
struct expr_type_member_access;
struct expr_compound;
struct expr_if;
struct expr_if_consteval;
struct expr_switch;
struct expr_break;
struct expr_continue;
struct expr_unreachable;
struct expr_generic_type_instantiation;

struct expr_unresolved_subscript;
struct expr_unresolved_function_call;
struct expr_unresolved_universal_function_call;
struct expr_unresolved_cast;
struct expr_unresolved_member_access;
struct expr_unresolved_array_type;
struct expr_unresolved_generic_type_instantiation;


using expr_t = node<
	expr_identifier,
	expr_integer_literal,
	expr_typed_literal,
	expr_tuple,
	expr_unary_op,
	expr_binary_op,
	expr_subscript,
	expr_function_call,
	expr_cast,
	expr_take_reference,
	expr_struct_init,
	expr_array_default_construct,
	expr_builtin_default_construct,
	expr_member_access,
	expr_type_member_access,
	expr_compound,
	expr_if,
	expr_if_consteval,
	expr_switch,
	expr_break,
	expr_continue,
	expr_unreachable,
	expr_generic_type_instantiation
>;

using unresolved_expr_t = node<
	expr_identifier,
	expr_tuple,
	expr_unary_op,
	expr_binary_op,
	expr_unresolved_subscript,
	expr_unresolved_function_call,
	expr_unresolved_universal_function_call,
	expr_unresolved_cast,
	expr_unresolved_member_access,
	expr_compound,
	expr_if,
	expr_if_consteval,
	expr_switch,
	expr_unresolved_array_type,
	expr_unresolved_generic_type_instantiation
>;

enum class expression_type_kind
{
	lvalue,
	lvalue_reference,
	rvalue,
	moved_lvalue,
	integer_literal,
	function_name,
	type_name,
	tuple,
	if_expr,
	switch_expr,
	none,
	noreturn,
};


constexpr bool is_lvalue(expression_type_kind kind)
{
	return kind == expression_type_kind::lvalue
		|| kind == expression_type_kind::lvalue_reference;
}

constexpr bool is_rvalue(expression_type_kind kind)
{
	return kind == expression_type_kind::rvalue
		|| kind == expression_type_kind::moved_lvalue
		|| kind == expression_type_kind::integer_literal;
}

constexpr bool is_rvalue_or_integer_literal(expression_type_kind kind)
{
	return kind == expression_type_kind::rvalue
		|| kind == expression_type_kind::integer_literal;
}

struct unresolved_expression
{
	unresolved_expr_t expr;

	unresolved_expression(void) = delete;
	unresolved_expression(unresolved_expression const &) = default;
	unresolved_expression(unresolved_expression &&)      = default;
	unresolved_expression &operator = (unresolved_expression const &) = default;
	unresolved_expression &operator = (unresolved_expression &&)      = default;
};

struct constant_expression
{
	expression_type_kind kind;
	typespec             type;
	constant_value       value;
	expr_t               expr;

	constant_expression(void) = delete;
	constant_expression(constant_expression const &) = default;
	constant_expression(constant_expression &&)      = default;
	constant_expression &operator = (constant_expression const &) = default;
	constant_expression &operator = (constant_expression &&)      = default;
};

struct dynamic_expression
{
	expression_type_kind kind;
	typespec             type;
	expr_t               expr;

	dynamic_expression(void) = delete;
	dynamic_expression(dynamic_expression const &) = default;
	dynamic_expression(dynamic_expression &&)      = default;
	dynamic_expression &operator = (dynamic_expression const &) = default;
	dynamic_expression &operator = (dynamic_expression &&)      = default;
};

struct expanded_variadic_expression
{
	arena_vector<expression> exprs;

	expanded_variadic_expression(void) = delete;
	expanded_variadic_expression(expanded_variadic_expression const &) = default;
	expanded_variadic_expression(expanded_variadic_expression &&)      = default;
	expanded_variadic_expression &operator = (expanded_variadic_expression const &) = default;
	expanded_variadic_expression &operator = (expanded_variadic_expression &&)      = default;
};

struct error_expression
{
	expr_t expr;
};

struct expression : bz::variant<
	unresolved_expression,
	constant_expression,
	dynamic_expression,
	expanded_variadic_expression,
	error_expression
>
{
	using base_t = bz::variant<
		unresolved_expression,
		constant_expression,
		dynamic_expression,
		expanded_variadic_expression,
		error_expression
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
		consteval_guaranteed_failed,
		consteval_never_tried,
		consteval_succeeded,
	};

	lex::src_tokens src_tokens;
	int consteval_state = consteval_never_tried;
	int paren_level = 0;

	declare_default_5(expression)

	expression(lex::src_tokens const &_src_tokens) = delete;

	template<typename ...Ts>
	expression(lex::src_tokens const &_src_tokens, Ts &&...ts)
		: base_t(std::forward<Ts>(ts)...),
		  src_tokens(_src_tokens),
		  consteval_state(this->is<ast::constant_expression>() ? consteval_succeeded : consteval_never_tried),
		  paren_level(0)
	{}

	lex::token_pos get_tokens_begin(void) const { return this->src_tokens.begin; }
	lex::token_pos get_tokens_pivot(void) const { return this->src_tokens.pivot; }
	lex::token_pos get_tokens_end(void) const   { return this->src_tokens.end; }

	void to_error(void);

	bool is_error(void) const;
	bool not_error(void) const;

	bool is_function(void) const noexcept;
	bool is_overloaded_function(void) const noexcept;

	bool is_typename(void) const noexcept;
	typespec &get_typename(void) noexcept;
	typespec const &get_typename(void) const noexcept;

	bool is_tuple(void) const noexcept;
	expr_tuple &get_tuple(void) noexcept;
	expr_tuple const &get_tuple(void) const noexcept;

	bool is_if_expr(void) const noexcept;
	expr_if &get_if_expr(void) noexcept;
	expr_if const &get_if_expr(void) const noexcept;

	bool is_switch_expr(void) const noexcept;
	expr_switch &get_switch_expr(void) noexcept;
	expr_switch const &get_switch_expr(void) const noexcept;

	bool is_integer_literal(void) const noexcept;
	expr_integer_literal &get_integer_literal(void) noexcept;
	expr_integer_literal const &get_integer_literal(void) const noexcept;

	constant_value &get_integer_literal_value(void) noexcept;
	constant_value const &get_integer_literal_value(void) const noexcept;

	bool is_generic_type(void) const noexcept;
	type_info *get_generic_type(void) const noexcept;

	void set_type(ast::typespec new_type);
	void set_type_kind(expression_type_kind new_kind);

	std::pair<typespec_view, expression_type_kind> get_expr_type_and_kind(void) const noexcept;

	bool is_constant_or_dynamic(void) const noexcept;
	bool is_unresolved(void) const noexcept;

	bool is_none(void) const noexcept;
	bool is_noreturn(void) const noexcept;

	bool has_consteval_succeeded(void) const noexcept;
	bool has_consteval_failed(void) const noexcept;

	expr_t &get_expr(void);
	expr_t const &get_expr(void) const;

	unresolved_expr_t &get_unresolved_expr(void);
	unresolved_expr_t const &get_unresolved_expr(void) const;

	bool is_special_top_level(void) const noexcept;
};


struct decl_variable;
struct decl_function;
struct function_body;


struct expr_identifier
{
	identifier id;
	decl_variable const *decl;

	declare_default_5(expr_identifier)

	expr_identifier(identifier _id, decl_variable const *var_decl)
		: id(std::move(_id)), decl(var_decl)
	{}

	expr_identifier(identifier _id)
		: id(std::move(_id)), decl(nullptr)
	{}
};

enum class literal_kind
{
	signed_integer,
	unsigned_integer,
	integer,
};

struct expr_integer_literal
{
	literal_kind kind;

	declare_default_5(expr_integer_literal)

	expr_integer_literal(literal_kind _kind)
		: kind(_kind)
	{}
};

struct expr_typed_literal
{
	lex::token_range tokens;

	declare_default_5(expr_typed_literal)

	expr_typed_literal(lex::token_range _tokens)
		: tokens(_tokens)
	{}

	expr_typed_literal(lex::token_pos it)
		: tokens{ it, it + 1 }
	{}
};

struct expr_tuple
{
	arena_vector<expression> elems;

	declare_default_5(expr_tuple)

	expr_tuple(arena_vector<expression> _elems)
		: elems(std::move(_elems))
	{}
};

struct expr_unary_op
{
	uint32_t   op;
	expression expr;

	declare_default_5(expr_unary_op)

	expr_unary_op(
		uint32_t   _op,
		expression _expr
	)
		: op  (_op),
		  expr(std::move(_expr))
	{}
};

struct expr_binary_op
{
	uint32_t   op;
	expression lhs;
	expression rhs;

	declare_default_5(expr_binary_op)

	expr_binary_op(
		uint32_t   _op,
		expression _lhs,
		expression _rhs
	)
		: op (_op),
		  lhs(std::move(_lhs)),
		  rhs(std::move(_rhs))
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
	lex::src_tokens          src_tokens;
	arena_vector<expression> params;
	function_body           *func_body;
	resolve_order            param_resolve_order;

	declare_default_5(expr_function_call)

	expr_function_call(
		lex::src_tokens   const &_src_tokens,
		arena_vector<expression> _params,
		function_body           *_func_body,
		resolve_order            _param_resolve_order
	)
		: src_tokens(_src_tokens),
		  params    (std::move(_params)),
		  func_body (_func_body),
		  param_resolve_order(_param_resolve_order)
	{}
};

struct expr_cast
{
	expression expr;
	typespec   type;

	declare_default_5(expr_cast)

	expr_cast(
		expression _expr,
		typespec   _type
	)
		: expr(std::move(_expr)),
		  type(std::move(_type))
	{}
};

struct expr_take_reference
{
	expression expr;

	declare_default_5(expr_take_reference)

	expr_take_reference(expression _expr)
		: expr(std::move(_expr))
	{}
};

struct expr_struct_init
{
	arena_vector<expression> exprs;
	typespec                 type;

	declare_default_5(expr_struct_init)

	expr_struct_init(
		arena_vector<expression> _exprs,
		typespec                 _type
	)
		: exprs(std::move(_exprs)),
		  type (std::move(_type))
	{}
};

struct expr_array_default_construct
{
	expression elem_ctor_call;
	typespec   type;

	declare_default_5(expr_array_default_construct)

	expr_array_default_construct(
		expression _elem_ctor_call,
		typespec   _type
	)
		: elem_ctor_call(std::move(_elem_ctor_call)),
		  type(std::move(_type))
	{}
};

struct expr_builtin_default_construct
{
	typespec type;

	declare_default_5(expr_builtin_default_construct)

	expr_builtin_default_construct(typespec _type)
		: type(std::move(_type))
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

struct expr_type_member_access
{
	expression                base;
	lex::token_pos            member;
	ast::decl_variable const *var_decl;

	declare_default_5(expr_type_member_access)

	expr_type_member_access(
		expression                _base,
		lex::token_pos            _member,
		ast::decl_variable const *_var_decl
	)
		: base(std::move(_base)),
		  member(_member),
		  var_decl(_var_decl)
	{}
};

struct statement;

struct expr_compound
{
	arena_vector<statement> statements;
	expression              final_expr;
	scope_t                 scope;

	declare_default_5(expr_compound)

	expr_compound(
		arena_vector<statement> _statements,
		expression              _final_expr
	);

	expr_compound(
		arena_vector<statement> _statements,
		expression              _final_expr,
		enclosing_scope_t       _enclosing_scope
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

struct expr_if_consteval
{
	expression condition;
	expression then_block;
	expression else_block;

	declare_default_5(expr_if_consteval)

	expr_if_consteval(
		expression _condition,
		expression _then_block,
		expression _else_block
	)
		: condition (std::move(_condition)),
		  then_block(std::move(_then_block)),
		  else_block(std::move(_else_block))
	{}

	expr_if_consteval(
		expression _condition,
		expression _then_block
	)
		: condition (std::move(_condition)),
		  then_block(std::move(_then_block)),
		  else_block()
	{}
};

struct switch_case
{
	arena_vector<expression> values;
	expression expr;
};

struct expr_switch
{
	expression                matched_expr;
	expression                default_case;
	arena_vector<switch_case> cases;

	declare_default_5(expr_switch)

	expr_switch(
		expression                _matched_expr,
		expression                _default_case,
		arena_vector<switch_case> _cases
	)
		: matched_expr(std::move(_matched_expr)),
		  default_case(std::move(_default_case)),
		  cases       (std::move(_cases))
	{}
};

struct expr_break
{
};

struct expr_continue
{
};

struct expr_unreachable
{
};

struct expr_generic_type_instantiation
{
	type_info *info;

	expr_generic_type_instantiation(type_info *_info)
		: info(_info)
	{}
};


struct expr_unresolved_subscript
{
	expression               base;
	arena_vector<expression> indices;

	declare_default_5(expr_unresolved_subscript)

	expr_unresolved_subscript(
		expression               _base,
		arena_vector<expression> _indices
	)
		: base   (std::move(_base)),
		  indices(std::move(_indices))
	{}
};

struct expr_unresolved_function_call
{
	expression               callee;
	arena_vector<expression> args;

	declare_default_5(expr_unresolved_function_call)

	expr_unresolved_function_call(
		expression               _callee,
		arena_vector<expression> _args
	)
		: callee(std::move(_callee)),
		  args  (std::move(_args))
	{}
};

struct expr_unresolved_universal_function_call
{
	expression               base;
	identifier               fn_id;
	arena_vector<expression> args;

	declare_default_5(expr_unresolved_universal_function_call)

	expr_unresolved_universal_function_call(
		expression               _base,
		identifier               _fn_id,
		arena_vector<expression> _args
	)
		: base (std::move(_base)),
		  fn_id(std::move(_fn_id)),
		  args (std::move(_args))
	{}
};

struct expr_unresolved_cast
{
	expression expr;
	expression type;

	declare_default_5(expr_unresolved_cast)

	expr_unresolved_cast(
		expression _expr,
		expression _type
	)
		: expr(std::move(_expr)),
		  type(std::move(_type))
	{}
};

struct expr_unresolved_member_access
{
	expression     base;
	lex::token_pos member;

	declare_default_5(expr_unresolved_member_access)

	expr_unresolved_member_access(
		expression     _base,
		lex::token_pos _member
	)
		: base(std::move(_base)),
		  member(_member)
	{}
};

struct expr_unresolved_array_type
{
	arena_vector<expression> sizes;
	expression               type;

	declare_default_5(expr_unresolved_array_type)

	expr_unresolved_array_type(
		arena_vector<expression> _sizes,
		expression               _type
	)
		: sizes(std::move(_sizes)),
		  type (std::move(_type))
	{}
};

struct expr_unresolved_generic_type_instantiation
{
	expression               base;
	arena_vector<expression> args;

	declare_default_5(expr_unresolved_generic_type_instantiation)

	expr_unresolved_generic_type_instantiation(
		expression               _base,
		arena_vector<expression> _args
	)
		: base(std::move(_base)),
		  args(std::move(_args))
	{}
};


template<typename ...Args>
expression make_unresolved_expression(lex::src_tokens const &tokens, Args &&...args)
{ return expression(tokens, unresolved_expression{ std::forward<Args>(args)... }); }

template<typename ...Args>
expression make_dynamic_expression(lex::src_tokens const &tokens, Args &&...args)
{ return expression(tokens, dynamic_expression{ std::forward<Args>(args)... }); }

template<typename ...Args>
expression make_constant_expression(lex::src_tokens const &tokens, Args &&...args)
{ return expression(tokens, constant_expression{ std::forward<Args>(args)... }); }

template<typename ...Args>
expression make_expanded_variadic_expression(lex::src_tokens const &tokens, Args &&...args)
{ return expression(tokens, expanded_variadic_expression{ std::forward<Args>(args)... }); }

template<typename ...Args>
expression make_error_expression(lex::src_tokens const &tokens, Args &&...args)
{ return expression(tokens, error_expression{ std::forward<Args>(args)... }); }


#define def_make_fn(ret_type, node_type)                                       \
template<typename ...Args>                                                     \
ret_type make_ ## node_type (Args &&...args)                                   \
{ return ret_type(make_ast_unique<node_type>(std::forward<Args>(args)...)); }

def_make_fn(expr_t, expr_identifier)
def_make_fn(expr_t, expr_integer_literal)
def_make_fn(expr_t, expr_typed_literal)
def_make_fn(expr_t, expr_tuple)
def_make_fn(expr_t, expr_unary_op)
def_make_fn(expr_t, expr_binary_op)
def_make_fn(expr_t, expr_subscript)
def_make_fn(expr_t, expr_function_call)
def_make_fn(expr_t, expr_cast)
def_make_fn(expr_t, expr_take_reference)
def_make_fn(expr_t, expr_struct_init)
def_make_fn(expr_t, expr_array_default_construct)
def_make_fn(expr_t, expr_builtin_default_construct)
def_make_fn(expr_t, expr_member_access)
def_make_fn(expr_t, expr_type_member_access)
def_make_fn(expr_t, expr_compound)
def_make_fn(expr_t, expr_if)
def_make_fn(expr_t, expr_if_consteval)
def_make_fn(expr_t, expr_switch)
def_make_fn(expr_t, expr_break)
def_make_fn(expr_t, expr_continue)
def_make_fn(expr_t, expr_unreachable)
def_make_fn(expr_t, expr_generic_type_instantiation)

#undef def_make_fn

#define def_make_unresolved_fn(ret_type, node_type)                            \
template<typename ...Args>                                                     \
ret_type make_unresolved_ ## node_type (Args &&...args)                        \
{ return ret_type(make_ast_unique<node_type>(std::forward<Args>(args)...)); }

def_make_unresolved_fn(unresolved_expr_t, expr_identifier)
def_make_unresolved_fn(unresolved_expr_t, expr_tuple)
def_make_unresolved_fn(unresolved_expr_t, expr_unary_op)
def_make_unresolved_fn(unresolved_expr_t, expr_binary_op)
def_make_unresolved_fn(unresolved_expr_t, expr_unresolved_subscript)
def_make_unresolved_fn(unresolved_expr_t, expr_unresolved_function_call)
def_make_unresolved_fn(unresolved_expr_t, expr_unresolved_universal_function_call)
def_make_unresolved_fn(unresolved_expr_t, expr_unresolved_cast)
def_make_unresolved_fn(unresolved_expr_t, expr_unresolved_member_access)
def_make_unresolved_fn(unresolved_expr_t, expr_compound)
def_make_unresolved_fn(unresolved_expr_t, expr_if)
def_make_unresolved_fn(unresolved_expr_t, expr_if_consteval)
def_make_unresolved_fn(unresolved_expr_t, expr_switch)
def_make_unresolved_fn(unresolved_expr_t, expr_unresolved_array_type)
def_make_unresolved_fn(unresolved_expr_t, expr_unresolved_generic_type_instantiation)

#undef def_make_unresolved_fn


inline expression type_as_expression(typespec type)
{
	return make_constant_expression(
		{},
		expression_type_kind::type_name,
		make_typename_typespec(nullptr),
		constant_value(std::move(type)),
		expr_t{}
	);
}

} // namespace ast

#endif // AST_EXPRESSION_H
