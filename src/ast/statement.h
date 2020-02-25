#ifndef AST_STATEMENT_H
#define AST_STATEMENT_H

#include "core.h"

#include "node.h"
#include "expression.h"
#include "typespec.h"

namespace ast
{

#define declare_node_type(x) struct x; using x##_ptr = std::unique_ptr<x>

declare_node_type(stmt_if);
declare_node_type(stmt_while);
declare_node_type(stmt_for);
declare_node_type(stmt_return);
declare_node_type(stmt_no_op);
declare_node_type(stmt_compound);
declare_node_type(stmt_expression);

declare_node_type(decl_variable);
declare_node_type(decl_function);
declare_node_type(decl_operator);
declare_node_type(decl_struct);

#undef declare_node_type


struct declaration : node<
	decl_variable,
	decl_function,
	decl_operator,
	decl_struct
>
{
	using base_t = node<
		decl_variable,
		decl_function,
		decl_operator,
		decl_struct
	>;

	using base_t::node;
	using base_t::get;
	using base_t::kind;
};

struct statement : node<
	stmt_if,
	stmt_while,
	stmt_for,
	stmt_return,
	stmt_no_op,
	stmt_compound,
	stmt_expression,
	decl_variable,
	decl_function,
	decl_operator,
	decl_struct
>
{
	using base_t = node<
		stmt_if,
		stmt_while,
		stmt_for,
		stmt_return,
		stmt_no_op,
		stmt_compound,
		stmt_expression,
		decl_variable,
		decl_function,
		decl_operator,
		decl_struct
	>;

	using base_t::node;
	using base_t::get;
	using base_t::kind;
	using base_t::emplace;
	statement(declaration decl);
};


struct stmt_if
{
	lex::token_range        tokens;
	expression              condition;
	statement               then_block;
	bz::optional<statement> else_block;

	stmt_if(
		lex::token_range _tokens,
		expression       _condition,
		statement        _then_block
	)
		: tokens    (_tokens),
		  condition (std::move(_condition)),
		  then_block(std::move(_then_block))
	{}

	stmt_if(
		lex::token_range _tokens,
		expression       _condition,
		statement        _then_block,
		statement        _else_block
	)
		: tokens    (_tokens),
		  condition (std::move(_condition)),
		  then_block(std::move(_then_block)),
		  else_block(std::move(_else_block))
	{}

	lex::token_pos get_tokens_begin(void) const { return this->tokens.begin; }
	lex::token_pos get_tokens_pivot(void) const { return this->tokens.begin; }
	lex::token_pos get_tokens_end(void) const   { return this->tokens.end; }
};

struct stmt_while
{
	lex::token_range tokens;
	expression       condition;
	statement        while_block;

	stmt_while(
		lex::token_range _tokens,
		expression       _condition,
		statement        _while_block
	)
		: tokens     (_tokens),
		  condition  (std::move(_condition)),
		  while_block(std::move(_while_block))
	{}

	lex::token_pos get_tokens_begin(void) const { return this->tokens.begin; }
	lex::token_pos get_tokens_pivot(void) const { return this->tokens.begin; }
	lex::token_pos get_tokens_end(void) const   { return this->tokens.end; }
};

struct stmt_for
{
	// TODO

	lex::token_pos get_tokens_begin(void) const { assert(false); exit(1); }
	lex::token_pos get_tokens_pivot(void) const { assert(false); exit(1); }
	lex::token_pos get_tokens_end(void) const   { assert(false); exit(1); }
};

struct stmt_return
{
	lex::token_range tokens;
	expression       expr;

	stmt_return(lex::token_range _tokens, expression _expr)
		: tokens(_tokens), expr(std::move(_expr))
	{}

	lex::token_pos get_tokens_begin(void) const { return this->tokens.begin; }
	lex::token_pos get_tokens_pivot(void) const { return this->tokens.begin; }
	lex::token_pos get_tokens_end(void) const   { return this->tokens.end; }
};

struct stmt_no_op
{
	lex::token_range tokens;

	stmt_no_op(lex::token_range _tokens)
		: tokens(_tokens)
	{}

	lex::token_pos get_tokens_begin(void) const { return this->tokens.begin; }
	lex::token_pos get_tokens_pivot(void) const { return this->tokens.begin; }
	lex::token_pos get_tokens_end(void) const   { return this->tokens.end; }
};

struct stmt_compound
{
	lex::token_range      tokens;
	bz::vector<statement> statements;

	stmt_compound(lex::token_range _tokens)
		: tokens(_tokens)
	{}

	stmt_compound(lex::token_range _tokens, bz::vector<statement> _stms)
		: tokens(_tokens), statements(std::move(_stms))
	{}

	lex::token_pos get_tokens_begin(void) const { return this->tokens.begin; }
	lex::token_pos get_tokens_pivot(void) const { return this->tokens.begin; }
	lex::token_pos get_tokens_end(void) const   { return this->tokens.end; }
};

struct stmt_expression
{
	lex::token_range tokens;
	expression       expr;

	stmt_expression(lex::token_range _tokens, expression _expr)
		: tokens(_tokens), expr(std::move(_expr))
	{}

	lex::token_pos get_tokens_begin(void) const { return this->tokens.begin; }
	lex::token_pos get_tokens_pivot(void) const { return this->tokens.begin; }
	lex::token_pos get_tokens_end(void) const   { return this->tokens.end; }
};


struct decl_variable
{
	lex::token_range         tokens;
	lex::token_pos           identifier;
	typespec                 prototype;
	typespec                 var_type;
	bz::optional<expression> init_expr;

	decl_variable(
		lex::token_range _tokens,
		lex::token_pos   _id,
		typespec         _prototype,
		typespec         _var_type,
		expression       _init_expr
	)
		: tokens    (_tokens),
		  identifier(_id),
		  prototype (std::move(_prototype)),
		  var_type  (std::move(_var_type)),
		  init_expr (std::move(_init_expr))
	{}

	decl_variable(
		lex::token_range _tokens,
		lex::token_pos   _id,
		typespec         _prototype,
		typespec         _var_type
	)
		: tokens(_tokens),
		  identifier(_id),
		  prototype (std::move(_prototype)),
		  var_type  (std::move(_var_type)),
		  init_expr ()
	{}

	decl_variable(
		lex::token_range _tokens,
		lex::token_pos   _id,
		typespec         _prototype,
		expression       _init_expr
	)
		: tokens    (_tokens),
		  identifier(_id),
		  prototype (std::move(_prototype)),
		  var_type  (),
		  init_expr (std::move(_init_expr))
	{}

	lex::token_pos get_tokens_begin(void) const;
	lex::token_pos get_tokens_pivot(void) const;
	lex::token_pos get_tokens_end(void) const;
};

struct decl_function
{
	lex::token_pos        identifier;
	bz::vector<variable>  params;
	typespec              return_type;
	bz::vector<statement> body;

	decl_function(
		lex::token_pos        _id,
		bz::vector<variable>  _params,
		typespec              _ret_type,
		bz::vector<statement> _body
	)
		: identifier (_id),
		  params     (std::move(_params)),
		  return_type(std::move(_ret_type)),
		  body       (std::move(_body))
	{}

	lex::token_pos get_tokens_begin(void) const;
	lex::token_pos get_tokens_pivot(void) const;
	lex::token_pos get_tokens_end(void) const;
};

struct decl_operator
{
	lex::token_pos        op;
	bz::vector<variable>  params;
	typespec              return_type;
	bz::vector<statement> body;

	decl_operator(
		lex::token_pos        _op,
		bz::vector<variable>  _params,
		typespec              _ret_type,
		bz::vector<statement> _body
	)
		: op         (_op),
		  params     (std::move(_params)),
		  return_type(std::move(_ret_type)),
		  body       (std::move(_body))
	{}

	lex::token_pos get_tokens_begin(void) const;
	lex::token_pos get_tokens_pivot(void) const;
	lex::token_pos get_tokens_end(void) const;
};

struct decl_struct
{
	lex::token_pos            identifier;
	bz::vector<ast::variable> member_variables;

	type_info *info = nullptr;

	decl_struct(lex::token_pos _id, bz::vector<ast::variable> _members)
		: identifier      (_id),
		  member_variables(std::move(_members))
	{}

	lex::token_pos get_tokens_begin(void) const;
	lex::token_pos get_tokens_pivot(void) const;
	lex::token_pos get_tokens_end(void) const;
};


template<typename ...Args>
statement make_statement(Args &&...args)
{ return statement(std::forward<Args>(args)...); }

template<typename ...Args>
declaration make_declaration(Args &&...args)
{ return declaration(std::forward<Args>(args)...); }


template<typename ...Args>
declaration make_decl_variable(Args &&...args)
{ return declaration(std::make_unique<decl_variable>(std::forward<Args>(args)...)); }

template<typename ...Args>
declaration make_decl_function(Args &&...args)
{ return declaration(std::make_unique<decl_function>(std::forward<Args>(args)...)); }

template<typename ...Args>
declaration make_decl_operator(Args &&...args)
{ return declaration(std::make_unique<decl_operator>(std::forward<Args>(args)...)); }

template<typename ...Args>
declaration make_decl_struct(Args &&...args)
{ return declaration(std::make_unique<decl_struct>(std::forward<Args>(args)...)); }


template<typename ...Args>
statement make_stmt_if(Args &&...args)
{ return statement(std::make_unique<stmt_if>(std::forward<Args>(args)...)); }

template<typename ...Args>
statement make_stmt_while(Args &&...args)
{ return statement(std::make_unique<stmt_while>(std::forward<Args>(args)...)); }

template<typename ...Args>
statement make_stmt_for(Args &&...args)
{ return statement(std::make_unique<stmt_for>(std::forward<Args>(args)...)); }

template<typename ...Args>
statement make_stmt_return(Args &&...args)
{ return statement(std::make_unique<stmt_return>(std::forward<Args>(args)...)); }

template<typename ...Args>
statement make_stmt_no_op(Args &&...args)
{ return statement(std::make_unique<stmt_no_op>(std::forward<Args>(args)...)); }

template<typename ...Args>
statement make_stmt_compound(Args &&...args)
{ return statement(std::make_unique<stmt_compound>(std::forward<Args>(args)...)); }

template<typename ...Args>
statement make_stmt_expression(Args &&...args)
{ return statement(std::make_unique<stmt_expression>(std::forward<Args>(args)...)); }

} // namespace ast

#endif // AST_STATEMENT_H
