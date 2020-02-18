#ifndef PARSE_CONTEXT_H
#define PARSE_CONTEXT_H

#include "core.h"

#include "ast/typespec.h"
#include "ast/expression.h"
#include "ast/statement.h"

inline std::list<ast::type_info> get_default_types(void)
{
	return {
		ast::type_info{ "int8",    1,  1, {} },
		ast::type_info{ "int16",   2,  2, {} },
		ast::type_info{ "int32",   4,  4, {} },
		ast::type_info{ "int64",   8,  8, {} },
		ast::type_info{ "uint8",   1,  1, {} },
		ast::type_info{ "uint16",  2,  2, {} },
		ast::type_info{ "uint32",  4,  4, {} },
		ast::type_info{ "uint64",  8,  8, {} },
		ast::type_info{ "float32", 4,  4, {} },
		ast::type_info{ "float64", 8,  8, {} },
		ast::type_info{ "char",    4,  4, {} },
		ast::type_info{ "str",     16, 8, {} },
		ast::type_info{ "bool",    1,  1, {} },
		ast::type_info{ "null_t",  0,  0, {} },
		ast::type_info{ "void",    0,  0, {} },
	};
}

using ctx_variable = ast::decl_variable *;
using ctx_function = ast::decl_function *;
using ctx_operator = ast::decl_operator *;
using ctx_struct   = ast::decl_struct *;

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

	std::list<ast::type_info> types = get_default_types();

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

	ast::type_info const *get_type_info(bz::string_view id) const;
};

#endif // PARSE_CONTEXT_H
