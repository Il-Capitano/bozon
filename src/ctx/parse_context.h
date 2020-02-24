#ifndef CTX_PARSE_CONTEXT_H
#define CTX_PARSE_CONTEXT_H

#include "core.h"

#include "lex/token.h"
#include "error.h"
#include "ast/typespec.h"
#include "ast/expression.h"
#include "ast/statement.h"

namespace ctx
{

struct global_context;

struct parse_context
{
	bz::string      scope;
	global_context &global_ctx;

	bz::vector<bz::vector<ast::variable>> scope_variables;

	parse_context(bz::string_view _scope, global_context &_global_ctx)
		: scope(_scope), global_ctx(_global_ctx)
	{}

	void report_error(lex::token_pos it) const;
	void report_error(
		lex::token_pos it,
		bz::string message, bz::vector<ctx::note> notes = {}
	) const;
	void report_error(
		lex::token_pos begin, lex::token_pos pivot, lex::token_pos end,
		bz::string message, bz::vector<ctx::note> notes = {}
	) const;
	template<typename T>
	void report_error(
		T const &tokens,
		bz::string message, bz::vector<ctx::note> notes = {}
	) const
	{
		this->report_error(
			tokens.get_tokens_begin(), tokens.get_tokens_pivot(), tokens.get_tokens_end(),
			std::move(message), std::move(notes)
		);
	}
	bool has_errors(void) const;
	lex::token_pos assert_token(lex::token_pos &stream, uint32_t kind) const;
	lex::token_pos assert_token(lex::token_pos &stream, uint32_t kind1, uint32_t kind2) const;

	void add_scope(void);
	void remove_scope(void);

	void add_global_declaration(ast::declaration &decl);
	void add_global_variable(ast::decl_variable &var_decl);
	void add_global_function(ast::decl_function &func_decl);
	void add_global_operator(ast::decl_operator &op_decl);
	void add_global_struct(ast::decl_struct &struct_decl);

	void add_local_variable(ast::variable var_decl);

	ast::typespec get_identifier_type(lex::token_pos id) const;
	ast::typespec get_operation_type(ast::expr_unary_op const &unary_op) const;
	ast::typespec get_operation_type(ast::expr_binary_op const &binary_op) const;
	ast::typespec get_function_call_type(ast::expr_function_call const &func_call) const;

	bool is_convertible(ast::expression::expr_type_t const &from, ast::typespec const &to);

	ast::type_info const *get_type_info(bz::string_view id) const;
};

} // namespace ctx

#endif // CTX_PARSE_CONTEXT_H
