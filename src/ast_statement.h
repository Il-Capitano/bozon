#ifndef AST_STATEMENT_H
#define AST_STATEMENT_H

#include "core.h"

#include "first_pass_parser.h"
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
struct ast_variable_decl_statement;
using ast_variable_decl_statement_ptr = std::unique_ptr<ast_variable_decl_statement>;
struct ast_compound_statement;
using ast_compound_statement_ptr = std::unique_ptr<ast_compound_statement>;
struct ast_expression_statement;
using ast_expression_statement_ptr = std::unique_ptr<ast_expression_statement>;
struct ast_struct_definition;
using ast_struct_definition_ptr = std::unique_ptr<ast_struct_definition>;
struct ast_function_definition;
using ast_function_definition_ptr = std::unique_ptr<ast_function_definition>;
struct ast_operator_definition;
using ast_operator_definition_ptr = std::unique_ptr<ast_operator_definition>;


struct ast_statement
{
	uint32_t kind;

	ast_if_statement_ptr            if_statement            = nullptr;
	ast_while_statement_ptr         while_statement         = nullptr;
	ast_for_statement_ptr           for_statement           = nullptr;
	ast_return_statement_ptr        return_statement        = nullptr;
	ast_no_op_statement_ptr         no_op_statement         = nullptr;
	ast_variable_decl_statement_ptr variable_decl_statement = nullptr;
	ast_compound_statement_ptr      compound_statement      = nullptr;
	ast_expression_statement_ptr    expression_statement    = nullptr;
	ast_struct_definition_ptr       struct_definition       = nullptr;
	ast_function_definition_ptr     function_definition     = nullptr;
	ast_operator_definition_ptr     operator_definition     = nullptr;

	ast_statement(fp_statement_ptr const &stmt);

	ast_statement(ast_if_statement_ptr            if_stmt        );
	ast_statement(ast_while_statement_ptr         while_stmt     );
	ast_statement(ast_for_statement_ptr           for_stmt       );
	ast_statement(ast_return_statement_ptr        return_stmt    );
	ast_statement(ast_no_op_statement_ptr         no_op_stmt     );
	ast_statement(ast_variable_decl_statement_ptr var_decl_stmt  );
	ast_statement(ast_compound_statement_ptr      compound_stmt  );
	ast_statement(ast_expression_statement_ptr    expression_stmt);
	ast_statement(ast_struct_definition_ptr       struct_def     );
	ast_statement(ast_function_definition_ptr     func_def       );
	ast_statement(ast_operator_definition_ptr     op_def         );
};

using ast_statement_ptr = std::unique_ptr<ast_statement>;


struct ast_if_statement
{
	ast_expression_ptr condition;
	ast_statement_ptr  if_block;
	ast_statement_ptr  else_block;

	ast_if_statement(
		ast_expression_ptr _cond,
		ast_statement_ptr  _if_block,
		ast_statement_ptr  _else_block
	)
		: condition (std::move(_cond)),
		  if_block  (std::move(_if_block)),
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

struct ast_variable_decl_statement
{
	intern_string      identifier;
	ast_typespec_ptr   typespec;
	ast_expression_ptr init_expr;

	ast_variable_decl_statement(
		intern_string      _id,
		ast_typespec_ptr   _typespec,
		ast_expression_ptr _init_expr
	)
		: identifier(_id),
		  typespec  (_typespec ? std::move(_typespec) : _init_expr->typespec->clone()),
		  init_expr (std::move(_init_expr))
	{}
};

struct ast_compound_statement
{
	std::vector<ast_statement_ptr> statements;

	ast_compound_statement(std::vector<ast_statement_ptr> _stms)
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

struct ast_struct_definition
{
	// TODO: implement
};

struct ast_function_definition
{
	intern_string                 identifier;
	std::vector<ast_variable_ptr> params;
	ast_typespec_ptr              return_typespec;
	ast_compound_statement_ptr    body;

	ast_function_definition(
		intern_string                 _id,
		std::vector<ast_variable_ptr> _params,
		ast_typespec_ptr              _ret_typespec,
		ast_compound_statement_ptr    _body
	)
		: identifier     (_id),
		  params         (std::move(_params)),
		  return_typespec(std::move(_ret_typespec)),
		  body           (std::move(_body))
	{}
};

struct ast_operator_definition
{
	uint32_t                      op;
	std::vector<ast_variable_ptr> params;
	ast_typespec_ptr              return_typespec;
	ast_compound_statement_ptr    body;

	ast_operator_definition(
		uint32_t                      _op,
		std::vector<ast_variable_ptr> _params,
		ast_typespec_ptr              _ret_typespec,
		ast_compound_statement_ptr    _body
	)
		: op             (_op),
		  params         (std::move(_params)),
		  return_typespec(std::move(_ret_typespec)),
		  body           (std::move(_body))
	{}
};


template<typename... Args>
ast_statement_ptr make_ast_statement(Args &&... args)
{
	return std::make_unique<ast_statement>(std::forward<Args>(args)...);
}

template<typename... Args>
ast_if_statement_ptr make_ast_if_statement(Args &&... args)
{
	return std::make_unique<ast_if_statement>(std::forward<Args>(args)...);
}

template<typename... Args>
ast_while_statement_ptr make_ast_while_statement(Args &&... args)
{
	return std::make_unique<ast_while_statement>(std::forward<Args>(args)...);
}

template<typename... Args>
ast_for_statement_ptr make_ast_for_statement(Args &&... args)
{
	return std::make_unique<ast_for_statement>(std::forward<Args>(args)...);
}

template<typename... Args>
ast_return_statement_ptr make_ast_return_statement(Args &&... args)
{
	return std::make_unique<ast_return_statement>(std::forward<Args>(args)...);
}

template<typename... Args>
ast_no_op_statement_ptr make_ast_no_op_statement(Args &&... args)
{
	return std::make_unique<ast_no_op_statement>(std::forward<Args>(args)...);
}

template<typename... Args>
ast_variable_decl_statement_ptr make_ast_variable_decl_statement(Args &&... args)
{
	return std::make_unique<ast_variable_decl_statement>(std::forward<Args>(args)...);
}

template<typename... Args>
ast_compound_statement_ptr make_ast_compound_statement(Args &&... args)
{
	return std::make_unique<ast_compound_statement>(std::forward<Args>(args)...);
}

template<typename... Args>
ast_expression_statement_ptr make_ast_expression_statement(Args &&... args)
{
	return std::make_unique<ast_expression_statement>(std::forward<Args>(args)...);
}

template<typename... Args>
ast_struct_definition_ptr make_ast_struct_definition(Args &&... args)
{
	return std::make_unique<ast_struct_definition>(std::forward<Args>(args)...);
}

template<typename... Args>
ast_function_definition_ptr make_ast_function_definition(Args &&... args)
{
	return std::make_unique<ast_function_definition>(std::forward<Args>(args)...);
}

template<typename... Args>
ast_operator_definition_ptr make_ast_operator_definition(Args &&... args)
{
	return std::make_unique<ast_operator_definition>(std::forward<Args>(args)...);
}





#endif // AST_STATEMENT_H
