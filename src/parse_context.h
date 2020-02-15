#ifndef PARSE_CONTEXT_H
#define PARSE_CONTEXT_H

#include "core.h"

#include "ast/type.h"
#include "ast/expression.h"
#include "ast/statement.h"

using ctx_variable = ast::decl_variable *;
using ctx_function = ast::decl_function *;
using ctx_operator = ast::decl_operator *;

struct function_overload_set
{
	bz::string               id;
	bz::vector<ctx_function> functions;
};

struct operator_overload_set
{
	uint32_t                 op;
	bz::vector<ctx_operator> operators;
};

struct parse_context
{
	bz::vector<ctx_variable>             global_variables;
	bz::vector<bz::vector<ctx_variable>> scope_variables;

	bz::vector<function_overload_set> global_functions;
	bz::vector<operator_overload_set> global_operators;

	void add_scope(void);
	void remove_scope(void);

	void add_global_declaration(ast::declaration &decl, bz::vector<error> &errors);
	void add_global_variable(ast::decl_variable &var_decl, bz::vector<error> &errors);
	void add_global_function(ast::decl_function &func_decl, bz::vector<error> &errors);
	void add_global_operator(ast::decl_operator &op_decl, bz::vector<error> &errors);
	void add_global_struct(ast::decl_struct &struct_decl, bz::vector<error> &errors);

	void add_local_variable(ast::decl_variable &var_decl);

	ast::typespec get_identifier_type(token_pos id) const;
	ast::typespec get_operation_type(ast::expr_unary_op const &unary_op);
	ast::typespec get_operation_type(ast::expr_binary_op const &binary_op);
};

#endif // PARSE_CONTEXT_H
