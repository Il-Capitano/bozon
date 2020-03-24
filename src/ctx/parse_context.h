#ifndef CTX_PARSE_CONTEXT_H
#define CTX_PARSE_CONTEXT_H

#include "core.h"

#include "lex/token.h"
#include "error.h"
#include "ast/typespec.h"
#include "ast/expression.h"
#include "ast/statement.h"

#include "decl_set.h"

namespace ctx
{

struct global_context;

struct parse_context
{
	uint32_t        file_id;
	global_context &global_ctx;

	decl_set global_decls;

	bz::vector<decl_set> scope_decls;

	parse_context(uint32_t _file_id, global_context &_global_ctx);

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

	void report_ambiguous_id_error(lex::token_pos id) const;

	void add_scope(void);
	void remove_scope(void);

	void add_global_declaration(ast::declaration &decl);
	void add_global_variable(ast::decl_variable &var_decl);
	void add_global_function(ast::decl_function &func_decl);
	void add_global_operator(ast::decl_operator &op_decl);
	void add_global_struct(ast::decl_struct &struct_decl);

	void add_local_variable(ast::decl_variable &var_decl);
	void add_local_function(ast::decl_function &func_decl);
	void add_local_operator(ast::decl_operator &op_decl);
	void add_local_struct(ast::decl_struct &struct_decl);

	auto get_identifier_decl(lex::token_pos id) const
		-> bz::variant<ast::decl_variable const *, ast::decl_function const *>;

	ast::expression::expr_type_t get_identifier_type(lex::token_pos id) const;

	auto get_operation_body_and_type(ast::expr_unary_op const &unary_op)
		-> std::pair<ast::function_body *, ast::expression::expr_type_t>;
	auto get_operation_body_and_type(ast::expr_binary_op const &binary_op)
		-> std::pair<ast::function_body *, ast::expression::expr_type_t>;
	auto get_function_call_body_and_type(ast::expr_function_call const &func_call)
		-> std::pair<ast::function_body *, ast::expression::expr_type_t>;

//	ast::expression::expr_type_t get_operation_type(ast::expr_unary_op const &unary_op);
//	ast::expression::expr_type_t get_operation_type(ast::expr_binary_op const &binary_op);
//	ast::expression::expr_type_t get_function_call_type(ast::expr_function_call const &func_call) const;

	bool is_convertible(ast::expression::expr_type_t const &from, ast::typespec const &to);

	ast::type_info const *get_type_info(bz::string_view id) const;
};

} // namespace ctx

#endif // CTX_PARSE_CONTEXT_H
