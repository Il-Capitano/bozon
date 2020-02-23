#ifndef CTX_PARSE_CONTEXT_H
#define CTX_PARSE_CONTEXT_H

#include "core.h"

#include "lex/lexer.h"
#include "ast/typespec.h"
#include "ast/expression.h"
#include "ast/statement.h"

namespace ctx
{

struct global_context;

struct parse_context
{
	bz::string      scope;
	global_context *global_ctx;

	bz::vector<bz::vector<ast::variable>> scope_variables;

	parse_context(bz::string_view _scope, global_context *_global_ctx)
		: scope(_scope), global_ctx(_global_ctx)
	{}

	void add_scope(void);
	void remove_scope(void);

	void add_global_declaration(ast::declaration &decl, bz::vector<error> &errors);
	void add_global_variable(ast::decl_variable &var_decl, bz::vector<error> &errors);
	void add_global_function(ast::decl_function &func_decl, bz::vector<error> &errors);
	void add_global_operator(ast::decl_operator &op_decl, bz::vector<error> &errors);
	void add_global_struct(ast::decl_struct &struct_decl, bz::vector<error> &errors);

	void add_local_variable(ast::variable var_decl);

	ast::typespec get_identifier_type(
		lex::token_pos id,
		bz::vector<error> &errors
	) const;
	ast::typespec get_operation_type(
		ast::expr_unary_op const &unary_op,
		bz::vector<error> &errors
	) const;
	ast::typespec get_operation_type(
		ast::expr_binary_op const &binary_op,
		bz::vector<error> &errors
	) const;
	ast::typespec get_function_call_type(
		ast::expr_function_call const &func_call,
		bz::vector<error> &errors
	) const;

	ast::type_info const *get_type_info(bz::string_view id) const;
};

} // namespace ctx

#endif // CTX_PARSE_CONTEXT_H
