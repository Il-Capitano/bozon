#ifndef AST_STATEMENT_H
#define AST_STATEMENT_H

#include "core.h"

#include "ast_expression.h"
#include "ast_type.h"

struct ast_if_statement;
using ast_if_statement_ptr = std::unique_ptr<ast_if_statement>;
struct ast_while_statement;
using ast_while_statement_ptr = std::unique_ptr<ast_while_statement>;
struct ast_for_statement;
using ast_for_statement_ptr = std::unique_ptr<ast_for_statement>;
struct ast_return_statement;
using ast_return_statement_ptr = std::unique_ptr<ast_return_statement>;
struct ast_no_op_statement;
using ast_no_op_statement_ptr = std::unique_ptr<ast_no_op_statement>;
struct ast_compound_statement;
using ast_compound_statement_ptr = std::unique_ptr<ast_compound_statement>;
struct ast_expression_statement;
using ast_expression_statement_ptr = std::unique_ptr<ast_expression_statement>;
struct ast_declaration_statement;
using ast_declaration_statement_ptr = std::unique_ptr<ast_declaration_statement>;


#define _ast_statement_types \
_o(if_statement),            \
_o(while_statement),         \
_o(for_statement),           \
_o(return_statement),        \
_o(no_op_statement),         \
_o(compound_statement),      \
_o(expression_statement),    \
_o(declaration_statement)


#define _o(x) ast_##x##_ptr

struct ast_statement :
bz::variant< _ast_statement_types >
{
	using base_t = bz::variant< _ast_statement_types >;

#undef _o

#define _o(x) x = index_of< ast_##x##_ptr >

	enum : uint32_t
	{
		_ast_statement_types
	};

#undef _o

	using base_t::get;
	using base_t::variant;

	uint32_t kind(void) const
	{ return base_t::index(); }

	void resolve(void);
};

#undef _ast_statement_types

using ast_statement_ptr = std::unique_ptr<ast_statement>;


struct ast_if_statement
{
	ast_expression_ptr condition;
	ast_statement_ptr  then_block;
	ast_statement_ptr  else_block;

	ast_if_statement(
		ast_expression_ptr _cond,
		ast_statement_ptr  _then_block,
		ast_statement_ptr  _else_block
	)
		: condition (std::move(_cond)),
		  then_block(std::move(_then_block)),
		  else_block(std::move(_else_block))
	{}
};

struct ast_while_statement
{
	ast_expression_ptr condition;
	ast_statement_ptr  while_block;

	ast_while_statement(
		ast_expression_ptr _cond,
		ast_statement_ptr  _while_block
	)
		: condition  (std::move(_cond)),
		  while_block(std::move(_while_block))
	{}
};

struct ast_for_statement
{
	// TODO: implement
};

struct ast_return_statement
{
	ast_expression_ptr expr;

	ast_return_statement(ast_expression_ptr _expr)
		: expr(std::move(_expr))
	{}
};

struct ast_no_op_statement
{
	// nothing
};

struct ast_compound_statement
{
	bz::vector<ast_statement_ptr> statements;

	ast_compound_statement(bz::vector<ast_statement_ptr> _stms)
		: statements(std::move(_stms))
	{}
};

struct ast_expression_statement
{
	ast_expression_ptr expr;

	ast_expression_statement(ast_expression_ptr _expr)
		: expr(std::move(_expr))
	{}
};



struct ast_variable_decl
{
	src_tokens::pos    identifier;
	ast_typespec_ptr   typespec;
	ast_expression_ptr init_expr;

	ast_variable_decl(
		src_tokens::pos    _id,
		ast_typespec_ptr   _typespec,
		ast_expression_ptr _init_expr
	)
		: identifier(_id),
		  typespec  (std::move(_typespec)),
		  init_expr (std::move(_init_expr))
	{}
};
using ast_variable_decl_ptr = std::unique_ptr<ast_variable_decl>;

struct ast_function_decl
{
	src_tokens::pos            identifier;
	bz::vector<ast_variable>   params;
	ast_typespec_ptr           return_type;
	ast_compound_statement_ptr body;

	ast_function_decl(
		src_tokens::pos            _id,
		bz::vector<ast_variable>   _params,
		ast_typespec_ptr           _ret_type,
		ast_compound_statement_ptr _body
	)
		: identifier (_id),
		  params     (std::move(_params)),
		  return_type(_ret_type),
		  body       (std::move(_body))
	{}
};
using ast_function_decl_ptr = std::unique_ptr<ast_function_decl>;

struct ast_operator_decl
{
	src_tokens::pos            op;
	bz::vector<ast_variable>   params;
	ast_typespec_ptr           return_type;
	ast_compound_statement_ptr body;

	ast_operator_decl(
		src_tokens::pos            _op,
		bz::vector<ast_variable>   _params,
		ast_typespec_ptr           _ret_type,
		ast_compound_statement_ptr _body
	)
		: op         (_op),
		  params     (std::move(_params)),
		  return_type(_ret_type),
		  body       (std::move(_body))
	{}
};
using ast_operator_decl_ptr = std::unique_ptr<ast_operator_decl>;

struct ast_struct_decl
{
	// TODO: implement
};
using ast_struct_decl_ptr = std::unique_ptr<ast_struct_decl>;

#define _ast_declaration_statement_types \
_o(variable_decl),                       \
_o(function_decl),                       \
_o(operator_decl),                       \
_o(struct_decl)

#define _o(x) ast_##x##_ptr

struct ast_declaration_statement : bz::variant< _ast_declaration_statement_types >
{
	using base_t = bz::variant< _ast_declaration_statement_types >;

#undef _o

#define _o(x) x = index_of< ast_##x##_ptr >

	enum : uint32_t
	{
		_ast_declaration_statement_types
	};

#undef _o

	using base_t::get;
	using base_t::variant;

	uint32_t kind(void) const
	{ return base_t::index(); }

	void resolve(void);
};

#undef _ast_declaration_statement_types

template<typename ...Args>
ast_statement_ptr make_ast_statement(Args &&...args)
{
	return std::make_unique<ast_statement>(std::forward<Args>(args)...);
}

template<typename ...Args>
ast_if_statement_ptr make_ast_if_statement(Args &&...args)
{
	return std::make_unique<ast_if_statement>(std::forward<Args>(args)...);
}

template<typename ...Args>
ast_while_statement_ptr make_ast_while_statement(Args &&...args)
{
	return std::make_unique<ast_while_statement>(std::forward<Args>(args)...);
}

template<typename ...Args>
ast_for_statement_ptr make_ast_for_statement(Args &&...args)
{
	return std::make_unique<ast_for_statement>(std::forward<Args>(args)...);
}

template<typename ...Args>
ast_return_statement_ptr make_ast_return_statement(Args &&...args)
{
	return std::make_unique<ast_return_statement>(std::forward<Args>(args)...);
}

template<typename ...Args>
ast_no_op_statement_ptr make_ast_no_op_statement(Args &&...args)
{
	return std::make_unique<ast_no_op_statement>(std::forward<Args>(args)...);
}

template<typename ...Args>
ast_compound_statement_ptr make_ast_compound_statement(Args &&...args)
{
	return std::make_unique<ast_compound_statement>(std::forward<Args>(args)...);
}

template<typename ...Args>
ast_expression_statement_ptr make_ast_expression_statement(Args &&...args)
{
	return std::make_unique<ast_expression_statement>(std::forward<Args>(args)...);
}

template<typename ...Args>
ast_declaration_statement_ptr make_ast_declaration_statement(Args &&...args)
{
	return std::make_unique<ast_declaration_statement>(std::forward<Args>(args)...);
}

template<typename ...Args>
ast_variable_decl_ptr make_ast_variable_decl(Args &&...args)
{
	return std::make_unique<ast_variable_decl>(std::forward<Args>(args)...);
}

template<typename ...Args>
ast_function_decl_ptr make_ast_function_decl(Args &&...args)
{
	return std::make_unique<ast_function_decl>(std::forward<Args>(args)...);
}

template<typename ...Args>
ast_operator_decl_ptr make_ast_operator_decl(Args &&...args)
{
	return std::make_unique<ast_operator_decl>(std::forward<Args>(args)...);
}

template<typename ...Args>
ast_struct_decl_ptr make_ast_struct_decl(Args &&...args)
{
	return std::make_unique<ast_struct_decl>(std::forward<Args>(args)...);
}



#endif // AST_STATEMENT_H
