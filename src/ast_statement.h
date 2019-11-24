#ifndef AST_STATEMENT_H
#define AST_STATEMENT_H

#include "core.h"

#include "ast_node.h"
#include "ast_expression.h"
#include "ast_type.h"

#define declare_node_type(x) struct x; using x##_ptr = std::unique_ptr<x>

declare_node_type(ast_decl_variable);
declare_node_type(ast_decl_function);
declare_node_type(ast_decl_operator);
declare_node_type(ast_decl_struct);

using ast_stmt_declaration = ast_node<
	ast_decl_variable,
	ast_decl_function,
	ast_decl_operator,
	ast_decl_struct
>;
using ast_stmt_declaration_ptr = std::unique_ptr<ast_stmt_declaration>;

template<>
void ast_stmt_declaration::resolve(void);

declare_node_type(ast_stmt_if);
declare_node_type(ast_stmt_while);
declare_node_type(ast_stmt_for);
declare_node_type(ast_stmt_return);
declare_node_type(ast_stmt_no_op);
declare_node_type(ast_stmt_compound);
declare_node_type(ast_stmt_expression);

#undef declare_node_type


using ast_statement = ast_node<
	ast_stmt_if,
	ast_stmt_while,
	ast_stmt_for,
	ast_stmt_return,
	ast_stmt_no_op,
	ast_stmt_compound,
	ast_stmt_expression,
	ast_stmt_declaration
>;

template<>
void ast_statement::resolve(void);


struct ast_stmt_if
{
	ast_expression              condition;
	ast_statement               then_block;
	bz::optional<ast_statement> else_block;

	ast_stmt_if(
		ast_expression _condition,
		ast_statement  _then_block
	)
		: condition (std::move(_condition)),
		  then_block(std::move(_then_block))
	{}

	ast_stmt_if(
		ast_expression _condition,
		ast_statement  _then_block,
		ast_statement  _else_block
	)
		: condition (std::move(_condition)),
		  then_block(std::move(_then_block)),
		  else_block(std::move(_else_block))
	{}

	src_tokens::pos get_tokens_begin(void) const;
	src_tokens::pos get_tokens_pivot(void) const;
	src_tokens::pos get_tokens_end(void) const;

	void resolve(void);
};

struct ast_stmt_while
{
	ast_expression condition;
	ast_statement  while_block;

	ast_stmt_while(
		ast_expression _condition,
		ast_statement  _while_block
	)
		: condition  (std::move(_condition)),
		  while_block(std::move(_while_block))
	{}

	src_tokens::pos get_tokens_begin(void) const;
	src_tokens::pos get_tokens_pivot(void) const;
	src_tokens::pos get_tokens_end(void) const;

	void resolve(void);
};

struct ast_stmt_for
{
	// TODO

	src_tokens::pos get_tokens_begin(void) const { assert(false); return nullptr; }
	src_tokens::pos get_tokens_pivot(void) const { assert(false); return nullptr; }
	src_tokens::pos get_tokens_end(void) const { assert(false); return nullptr; }

	void resolve(void) { assert(false); }
};

struct ast_stmt_return
{
	ast_expression expr;

	ast_stmt_return(ast_expression _expr)
		: expr(std::move(_expr))
	{}

	src_tokens::pos get_tokens_begin(void) const;
	src_tokens::pos get_tokens_pivot(void) const;
	src_tokens::pos get_tokens_end(void) const;

	void resolve(void);
};

struct ast_stmt_no_op
{
	// nothing

	src_tokens::pos get_tokens_begin(void) const;
	src_tokens::pos get_tokens_pivot(void) const;
	src_tokens::pos get_tokens_end(void) const;

	void resolve(void) {}
};

struct ast_stmt_compound
{
	bz::vector<ast_statement> statements;

	ast_stmt_compound(void) = default;

	ast_stmt_compound(bz::vector<ast_statement> _stms)
		: statements(std::move(_stms))
	{}

	src_tokens::pos get_tokens_begin(void) const;
	src_tokens::pos get_tokens_pivot(void) const;
	src_tokens::pos get_tokens_end(void) const;

	void resolve(void);
};

struct ast_stmt_expression
{
	ast_expression expr;

	ast_stmt_expression(ast_expression _expr)
		: expr(std::move(_expr))
	{}

	src_tokens::pos get_tokens_begin(void) const;
	src_tokens::pos get_tokens_pivot(void) const;
	src_tokens::pos get_tokens_end(void) const;

	void resolve(void);
};


struct ast_decl_variable
{
	src_tokens::pos              identifier;
	ast_typespec_ptr             typespec;
	bz::optional<ast_expression> init_expr;

	ast_decl_variable(
		src_tokens::pos  _id,
		ast_typespec_ptr _typespec,
		ast_expression   _init_expr
	)
		: identifier(_id),
		  typespec  (std::move(_typespec)),
		  init_expr (std::move(_init_expr))
	{}

	ast_decl_variable(
		src_tokens::pos  _id,
		ast_typespec_ptr _typespec
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

struct ast_decl_function
{
	src_tokens::pos          identifier;
	bz::vector<ast_variable> params;
	ast_typespec_ptr         return_type;
	ast_stmt_compound_ptr    body;

	ast_decl_function(
		src_tokens::pos          _id,
		bz::vector<ast_variable> _params,
		ast_typespec_ptr         _ret_type,
		ast_stmt_compound_ptr    _body
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

struct ast_decl_operator
{
	src_tokens::pos          op;
	bz::vector<ast_variable> params;
	ast_typespec_ptr         return_type;
	ast_stmt_compound_ptr    body;

	ast_decl_operator(
		src_tokens::pos          _op,
		bz::vector<ast_variable> _params,
		ast_typespec_ptr         _ret_type,
		ast_stmt_compound_ptr    _body
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

struct ast_decl_struct
{
	// TODO

	src_tokens::pos get_tokens_begin(void) const { assert(false); return nullptr; }
	src_tokens::pos get_tokens_pivot(void) const { assert(false); return nullptr; }
	src_tokens::pos get_tokens_end(void) const { assert(false); return nullptr; }

	void resolve(void) { assert(false); }
};


template<typename ...Args>
ast_statement make_ast_statement(Args &&...args)
{ return ast_statement(std::forward<Args>(args)...); }


template<typename ...Args>
ast_statement make_ast_decl_variable(Args &&...args)
{
	return ast_statement(
		std::make_unique<ast_stmt_declaration>(
			std::make_unique<ast_decl_variable>(std::forward<Args>(args)...)
		)
	);
}

template<typename ...Args>
ast_statement make_ast_decl_function(Args &&...args)
{
	return ast_statement(
		std::make_unique<ast_stmt_declaration>(
			std::make_unique<ast_decl_function>(std::forward<Args>(args)...)
		)
	);
}

template<typename ...Args>
ast_statement make_ast_decl_operator(Args &&...args)
{
	return ast_statement(
		std::make_unique<ast_stmt_declaration>(
			std::make_unique<ast_decl_operator>(std::forward<Args>(args)...)
		)
	);
}

template<typename ...Args>
ast_statement make_ast_decl_struct(Args &&...args)
{
	return ast_statement(
		std::make_unique<ast_stmt_declaration>(
			std::make_unique<ast_decl_struct>(std::forward<Args>(args)...)
		)
	);
}


template<typename ...Args>
ast_statement make_ast_stmt_if(Args &&...args)
{ return ast_statement(std::make_unique<ast_stmt_if>(std::forward<Args>(args)...)); }

template<typename ...Args>
ast_statement make_ast_stmt_while(Args &&...args)
{ return ast_statement(std::make_unique<ast_stmt_while>(std::forward<Args>(args)...)); }

template<typename ...Args>
ast_statement make_ast_stmt_for(Args &&...args)
{ return ast_statement(std::make_unique<ast_stmt_for>(std::forward<Args>(args)...)); }

template<typename ...Args>
ast_statement make_ast_stmt_return(Args &&...args)
{ return ast_statement(std::make_unique<ast_stmt_return>(std::forward<Args>(args)...)); }

template<typename ...Args>
ast_statement make_ast_stmt_no_op(Args &&...args)
{ return ast_statement(std::make_unique<ast_stmt_no_op>(std::forward<Args>(args)...)); }

template<typename ...Args>
ast_statement make_ast_stmt_compound(Args &&...args)
{ return ast_statement(std::make_unique<ast_stmt_compound>(std::forward<Args>(args)...)); }

template<typename ...Args>
ast_statement make_ast_stmt_expression(Args &&...args)
{ return ast_statement(std::make_unique<ast_stmt_expression>(std::forward<Args>(args)...)); }

template<typename ...Args>
ast_statement make_ast_stmt_declaration(Args &&...args)
{ return ast_statement(std::make_unique<ast_stmt_declaration>(std::forward<Args>(args)...)); }


#endif // AST_STATEMENT_H
