#ifndef CTX_GLOBAL_CONTEXT_H
#define CTX_GLOBAL_CONTEXT_H

#include "lex/lexer.h"
#include "ast/typespec.h"
#include "ast/expression.h"
#include "ast/statement.h"

namespace ctx
{
struct function_overload_sets
{
	bz::string id;
	bz::vector<ast::decl_function *> func_decls;
};

struct operator_overload_sets
{
	uint32_t op;
	bz::vector<ast::decl_operator *> op_decls;
};

struct src_local_decls
{
	bz::vector<ast::decl_variable *>   var_decls;
	bz::vector<function_overload_sets> func_sets;
	bz::vector<operator_overload_sets> op_sets;
	bz::vector<ast::decl_struct *>     struct_decls;
};

struct global_context
{
private:
	std::map<bz::string, src_local_decls> _decls;
	std::list<ast::type_info> _types;
	bz::vector<error> _errors;

public:
	global_context(void);

	void report_error(error err);
	bool has_errors(void) const;

	auto add_global_declaration(bz::string_view scope, ast::declaration &decl)
		-> bz::result<int, error>;
	auto add_global_variable(bz::string_view scope, ast::decl_variable &var_decl)
		-> bz::result<int, error>;
	auto add_global_function(bz::string_view scope, ast::decl_function &func_decl)
		-> bz::result<int, error>;
	auto add_global_operator(bz::string_view scope, ast::decl_operator &op_decl)
		-> bz::result<int, error>;
	auto add_global_struct(bz::string_view scope, ast::decl_struct &struct_decl)
		-> bz::result<int, error>;


	auto get_identifier_type(bz::string_view scope, lex::token_pos id)
		-> bz::result<ast::typespec, error>;

	auto get_operation_type(bz::string_view scope, ast::expr_unary_op const &unary_op)
		-> bz::result<ast::typespec, error>;
	auto get_operation_type(bz::string_view scope, ast::expr_binary_op const &binary_op)
		-> bz::result<ast::typespec, error>;

	auto get_function_call_type(bz::string_view scope, ast::expr_function_call const &func_call)
		-> bz::result<ast::typespec, error>;

	bool is_convertible(
		bz::string_view scope,
		ast::expression::expr_type_t const &from,
		ast::typespec const &to
	);

	ast::type_info const *get_type_info(bz::string_view scope, bz::string_view id);
};

} // namespace ctx

#endif // CTX_GLOBAL_CONTEXT_H
