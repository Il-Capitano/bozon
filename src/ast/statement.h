#ifndef STATEMENT_H
#define STATEMENT_H

#include "../core.h"

#include "node.h"
#include "expression.h"
#include "type.h"

namespace ast
{

#define declare_node_type(x) struct x; using x##_ptr = std::unique_ptr<x>

declare_node_type(decl_variable);
declare_node_type(decl_function);
declare_node_type(decl_operator);
declare_node_type(decl_struct);

using stmt_declaration = node<
	decl_variable,
	decl_function,
	decl_operator,
	decl_struct
>;
using stmt_declaration_ptr = std::unique_ptr<stmt_declaration>;

template<>
void stmt_declaration::resolve(void);

declare_node_type(stmt_if);
declare_node_type(stmt_while);
declare_node_type(stmt_for);
declare_node_type(stmt_return);
declare_node_type(stmt_no_op);
declare_node_type(stmt_compound);
declare_node_type(stmt_expression);

#undef declare_node_type


using statement = node<
	stmt_if,
	stmt_while,
	stmt_for,
	stmt_return,
	stmt_no_op,
	stmt_compound,
	stmt_expression,
	stmt_declaration
>;

template<>
void statement::resolve(void);


struct stmt_if
{
	token_range                 tokens;
	expression              condition;
	statement               then_block;
	bz::optional<statement> else_block;

	stmt_if(
		token_range    _tokens,
		expression _condition,
		statement  _then_block
	)
		: tokens(_tokens),
		  condition (std::move(_condition)),
		  then_block(std::move(_then_block))
	{}

	stmt_if(
		token_range    _tokens,
		expression _condition,
		statement  _then_block,
		statement  _else_block
	)
		: tokens(_tokens),
		  condition (std::move(_condition)),
		  then_block(std::move(_then_block)),
		  else_block(std::move(_else_block))
	{}

	src_tokens::pos get_tokens_begin(void) const { return this->tokens.begin; }
	src_tokens::pos get_tokens_pivot(void) const { return this->tokens.begin; }
	src_tokens::pos get_tokens_end(void) const   { return this->tokens.end; }

	void resolve(void);
};

struct stmt_while
{
	token_range    tokens;
	expression condition;
	statement  while_block;

	stmt_while(
		token_range    _tokens,
		expression _condition,
		statement  _while_block
	)
		: tokens(_tokens),
		  condition  (std::move(_condition)),
		  while_block(std::move(_while_block))
	{}

	src_tokens::pos get_tokens_begin(void) const { return this->tokens.begin; }
	src_tokens::pos get_tokens_pivot(void) const { return this->tokens.begin; }
	src_tokens::pos get_tokens_end(void) const   { return this->tokens.end; }

	void resolve(void);
};

struct stmt_for
{
	// TODO

	src_tokens::pos get_tokens_begin(void) const { assert(false); return nullptr; }
	src_tokens::pos get_tokens_pivot(void) const { assert(false); return nullptr; }
	src_tokens::pos get_tokens_end(void) const   { assert(false); return nullptr; }

	void resolve(void);
};

struct stmt_return
{
	token_range    tokens;
	expression expr;

	stmt_return(token_range _tokens, expression _expr)
		: tokens(_tokens), expr(std::move(_expr))
	{}

	src_tokens::pos get_tokens_begin(void) const { return this->tokens.begin; }
	src_tokens::pos get_tokens_pivot(void) const { return this->tokens.begin; }
	src_tokens::pos get_tokens_end(void) const   { return this->tokens.end; }

	void resolve(void);
};

struct stmt_no_op
{
	token_range tokens;

	stmt_no_op(token_range _tokens)
		: tokens(_tokens)
	{}

	src_tokens::pos get_tokens_begin(void) const { return this->tokens.begin; }
	src_tokens::pos get_tokens_pivot(void) const { return this->tokens.begin; }
	src_tokens::pos get_tokens_end(void) const   { return this->tokens.end; }

	void resolve(void) {}
};

struct stmt_compound
{
	token_range               tokens;
	bz::vector<statement> statements;

	stmt_compound(token_range _tokens)
		: tokens(_tokens)
	{}

	stmt_compound(token_range _tokens, bz::vector<statement> _stms)
		: tokens(_tokens), statements(std::move(_stms))
	{}

	src_tokens::pos get_tokens_begin(void) const { return this->tokens.begin; }
	src_tokens::pos get_tokens_pivot(void) const { return this->tokens.begin; }
	src_tokens::pos get_tokens_end(void) const   { return this->tokens.end; }

	void resolve(void);
};

struct stmt_expression
{
	token_range    tokens;
	expression expr;

	stmt_expression(token_range _tokens, expression _expr)
		: tokens(_tokens), expr(std::move(_expr))
	{}

	src_tokens::pos get_tokens_begin(void) const { return this->tokens.begin; }
	src_tokens::pos get_tokens_pivot(void) const { return this->tokens.begin; }
	src_tokens::pos get_tokens_end(void) const   { return this->tokens.end; }

	void resolve(void);
};


struct decl_variable
{
	src_tokens::pos              identifier;
	typespec_ptr             typespec;
	bz::optional<expression> init_expr;

	decl_variable(
		src_tokens::pos  _id,
		typespec_ptr _typespec,
		expression   _init_expr
	)
		: identifier(_id),
		  typespec  (std::move(_typespec)),
		  init_expr (std::move(_init_expr))
	{}

	decl_variable(
		src_tokens::pos  _id,
		typespec_ptr _typespec
	)
		: identifier(_id),
		  typespec  (std::move(_typespec)),
		  init_expr ()
	{}

	src_tokens::pos get_tokens_begin(void) const;
	src_tokens::pos get_tokens_pivot(void) const;
	src_tokens::pos get_tokens_end(void) const;

	void resolve(void);
};

struct decl_function
{
	src_tokens::pos          identifier;
	bz::vector<variable> params;
	typespec_ptr         return_type;
	stmt_compound_ptr    body;

	decl_function(
		src_tokens::pos          _id,
		bz::vector<variable> _params,
		typespec_ptr         _ret_type,
		stmt_compound_ptr    _body
	)
		: identifier (_id),
		  params     (std::move(_params)),
		  return_type(_ret_type),
		  body       (std::move(_body))
	{}

	src_tokens::pos get_tokens_begin(void) const;
	src_tokens::pos get_tokens_pivot(void) const;
	src_tokens::pos get_tokens_end(void) const;

	void resolve(void);
};

struct decl_operator
{
	src_tokens::pos          op;
	bz::vector<variable> params;
	typespec_ptr         return_type;
	stmt_compound_ptr    body;

	decl_operator(
		src_tokens::pos          _op,
		bz::vector<variable> _params,
		typespec_ptr         _ret_type,
		stmt_compound_ptr    _body
	)
		: op         (_op),
		  params     (std::move(_params)),
		  return_type(_ret_type),
		  body       (std::move(_body))
	{}

	src_tokens::pos get_tokens_begin(void) const;
	src_tokens::pos get_tokens_pivot(void) const;
	src_tokens::pos get_tokens_end(void) const;

	void resolve(void);
};

struct decl_struct
{
	// TODO

	src_tokens::pos get_tokens_begin(void) const { assert(false); return nullptr; }
	src_tokens::pos get_tokens_pivot(void) const { assert(false); return nullptr; }
	src_tokens::pos get_tokens_end(void) const { assert(false); return nullptr; }

	void resolve(void) { assert(false); }
};


template<typename ...Args>
statement make_statement(Args &&...args)
{ return statement(std::forward<Args>(args)...); }


template<typename ...Args>
statement make_decl_variable(Args &&...args)
{
	return statement(
		std::make_unique<stmt_declaration>(
			std::make_unique<decl_variable>(std::forward<Args>(args)...)
		)
	);
}

template<typename ...Args>
statement make_decl_function(Args &&...args)
{
	return statement(
		std::make_unique<stmt_declaration>(
			std::make_unique<decl_function>(std::forward<Args>(args)...)
		)
	);
}

template<typename ...Args>
statement make_decl_operator(Args &&...args)
{
	return statement(
		std::make_unique<stmt_declaration>(
			std::make_unique<decl_operator>(std::forward<Args>(args)...)
		)
	);
}

template<typename ...Args>
statement make_decl_struct(Args &&...args)
{
	return statement(
		std::make_unique<stmt_declaration>(
			std::make_unique<decl_struct>(std::forward<Args>(args)...)
		)
	);
}


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

template<typename ...Args>
statement make_stmt_declaration(Args &&...args)
{ return statement(std::make_unique<stmt_declaration>(std::forward<Args>(args)...)); }

} // namespace ast

#endif // STATEMENT_H
