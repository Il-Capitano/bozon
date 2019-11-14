#ifndef AST_STATEMENT_H
#define AST_STATEMENT_H

#include "core.h"

#include "ast_expression.h"
#include "ast_type.h"


struct ast_statement;
using ast_statement_ptr = std::unique_ptr<ast_statement>;


struct ast_stmt_if
{
	ast_expression condition;
	ast_statement_ptr  then_block;
	ast_statement_ptr  else_block;

	ast_stmt_if(
		ast_expression _condition,
		ast_statement_ptr  _then_block,
		ast_statement_ptr  _else_block
	)
		: condition (std::move(_condition)),
		  then_block(std::move(_then_block)),
		  else_block(std::move(_else_block))
	{}
};

struct ast_stmt_while
{
	ast_expression condition;
	ast_statement_ptr  while_block;

	ast_stmt_while(
		ast_expression _condition,
		ast_statement_ptr  _while_block
	)
		: condition  (std::move(_condition)),
		  while_block(std::move(_while_block))
	{}
};

struct ast_stmt_for
{
	// TODO
};

struct ast_stmt_return
{
	ast_expression expr;

	ast_stmt_return(ast_expression _expr)
		: expr(std::move(_expr))
	{}
};

struct ast_stmt_no_op
{
	// nothing
};

struct ast_stmt_compound
{
	bz::vector<ast_statement_ptr> statements;

	ast_stmt_compound(bz::vector<ast_statement_ptr> _stms)
		: statements(std::move(_stms))
	{}
};

struct ast_stmt_expression
{
	ast_expression expr;

	ast_stmt_expression(ast_expression _expr)
		: expr(std::move(_expr))
	{}
};


struct ast_decl_variable
{
	src_tokens::pos    identifier;
	ast_typespec_ptr   typespec;
	bz::optional<ast_expression> init_expr;

	ast_decl_variable(
		src_tokens::pos    _id,
		ast_typespec_ptr   _typespec,
		ast_expression _init_expr
	)
		: identifier(_id),
		  typespec  (std::move(_typespec)),
		  init_expr (std::move(_init_expr))
	{}

	ast_decl_variable(
		src_tokens::pos    _id,
		ast_typespec_ptr   _typespec
	)
		: identifier(_id),
		  typespec  (std::move(_typespec)),
		  init_expr ()
	{}
};

struct ast_decl_function
{
	src_tokens::pos          identifier;
	bz::vector<ast_variable> params;
	ast_typespec_ptr         return_type;
	ast_stmt_compound        body;

	ast_decl_function(
		src_tokens::pos          _id,
		bz::vector<ast_variable> _params,
		ast_typespec_ptr         _ret_type,
		ast_stmt_compound        _body
	)
		: identifier (_id),
		  params     (std::move(_params)),
		  return_type(_ret_type),
		  body       (std::move(_body))
	{}
};

struct ast_decl_operator
{
	src_tokens::pos          op;
	bz::vector<ast_variable> params;
	ast_typespec_ptr         return_type;
	ast_stmt_compound        body;

	ast_decl_operator(
		src_tokens::pos          _op,
		bz::vector<ast_variable> _params,
		ast_typespec_ptr         _ret_type,
		ast_stmt_compound        _body
	)
		: op         (_op),
		  params     (std::move(_params)),
		  return_type(_ret_type),
		  body       (std::move(_body))
	{}
};

struct ast_decl_struct
{
	// TODO
};

struct ast_stmt_declaration :
bz::variant<
	ast_decl_variable,
	ast_decl_function,
	ast_decl_operator,
	ast_decl_struct
>
{
	using base_t = bz::variant<
		ast_decl_variable,
		ast_decl_function,
		ast_decl_operator,
		ast_decl_struct
	>;


	enum : uint32_t
	{
		variable_declaration = index_of<ast_decl_variable>,
		function_declaration = index_of<ast_decl_function>,
		operator_declaration = index_of<ast_decl_operator>,
		struct_declaration   = index_of<ast_decl_struct>,
	};

	using base_t::get;
	using base_t::variant;

	uint32_t kind(void) const
	{ return base_t::index(); }

	void resolve(void);
};


struct ast_statement :
bz::variant<
	ast_stmt_if,
	ast_stmt_while,
	ast_stmt_for,
	ast_stmt_return,
	ast_stmt_no_op,
	ast_stmt_compound,
	ast_stmt_expression,
	ast_stmt_declaration
>
{
	using base_t = bz::variant<
		ast_stmt_if,
		ast_stmt_while,
		ast_stmt_for,
		ast_stmt_return,
		ast_stmt_no_op,
		ast_stmt_compound,
		ast_stmt_expression,
		ast_stmt_declaration
	>;

	enum : uint32_t
	{
		if_statement          = index_of<ast_stmt_if>,
		while_statement       = index_of<ast_stmt_while>,
		for_statement         = index_of<ast_stmt_for>,
		return_statement      = index_of<ast_stmt_return>,
		no_op_statement       = index_of<ast_stmt_no_op>,
		compound_statement    = index_of<ast_stmt_compound>,
		expression_statement  = index_of<ast_stmt_expression>,
		declaration_statement = index_of<ast_stmt_declaration>,
	};

	using base_t::get;
	using base_t::variant;

	uint32_t kind(void) const
	{ return base_t::index(); }

	void resolve(void);
};



template<typename ...Args>
ast_statement_ptr make_ast_statement(Args &&...args)
{
	return std::make_unique<ast_statement>(std::forward<Args>(args)...);
}

template<typename ...Args>
ast_statement_ptr make_ast_if_statement(Args &&...args)
{
	return make_ast_statement(
		ast_stmt_if(std::forward<Args>(args)...)
	);
}

template<typename ...Args>
ast_statement_ptr make_ast_while_statement(Args &&...args)
{
	return make_ast_statement(
		ast_stmt_while(std::forward<Args>(args)...)
	);
}

template<typename ...Args>
ast_statement_ptr make_ast_for_statement(Args &&...args)
{
	return make_ast_statement(
		ast_stmt_for(std::forward<Args>(args)...)
	);
}

template<typename ...Args>
ast_statement_ptr make_ast_return_statement(Args &&...args)
{
	return make_ast_statement(
		ast_stmt_return(std::forward<Args>(args)...)
	);
}

template<typename ...Args>
ast_statement_ptr make_ast_no_op_statement(Args &&...args)
{
	return make_ast_statement(
		ast_stmt_no_op(std::forward<Args>(args)...)
	);
}

template<typename ...Args>
ast_statement_ptr make_ast_compound_statement(Args &&...args)
{
	return make_ast_statement(
		ast_stmt_compound(std::forward<Args>(args)...)
	);
}

template<typename ...Args>
ast_statement_ptr make_ast_expression_statement(Args &&...args)
{
	return make_ast_statement(
		ast_stmt_expression(std::forward<Args>(args)...)
	);
}

template<typename ...Args>
ast_statement_ptr make_ast_declaration_statement(Args &&...args)
{
	return make_ast_statement(
		ast_stmt_declaration(std::forward<Args>(args)...)
	);
}

template<typename ...Args>
ast_stmt_declaration make_ast_variable_decl(Args &&...args)
{
	return ast_stmt_declaration(
		ast_decl_variable(std::forward<Args>(args)...)
	);
}

template<typename ...Args>
ast_stmt_declaration make_ast_function_decl(Args &&...args)
{
	return ast_stmt_declaration(
		ast_decl_function(std::forward<Args>(args)...)
	);
}

template<typename ...Args>
ast_stmt_declaration make_ast_operator_decl(Args &&...args)
{
	return ast_stmt_declaration(
		ast_decl_operator(std::forward<Args>(args)...)
	);
}

template<typename ...Args>
ast_stmt_declaration make_ast_struct_decl(Args &&...args)
{
	return ast_stmt_declaration(
		ast_decl_struct(std::forward<Args>(args)...)
	);
}



#endif // AST_STATEMENT_H
