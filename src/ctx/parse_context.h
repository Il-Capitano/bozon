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
	global_context &global_ctx;
	bz::vector<decl_set> scope_decls;

	bool is_parenthesis_suppressed = false;

	parse_context(global_context &_global_ctx);

	void report_error(lex::token_pos it) const;
	void report_error(
		lex::token_pos it, bz::u8string message,
		bz::vector<ctx::note> notes = {},
		bz::vector<ctx::suggestion> suggestions = {}
	) const;
	void report_error(
		lex::src_tokens src_tokens, bz::u8string message,
		bz::vector<ctx::note> notes = {},
		bz::vector<ctx::suggestion> suggestions = {}
	) const;
	template<typename T>
	void report_error(
		T const &tokens, bz::u8string message,
		bz::vector<ctx::note> notes = {},
		bz::vector<ctx::suggestion> suggestions = {}
	) const
	{
		this->report_error(
			{ tokens.get_tokens_begin(), tokens.get_tokens_pivot(), tokens.get_tokens_end() },
			std::move(message), std::move(notes), std::move(suggestions)
		);
	}

	void report_warning(
		lex::token_pos it, bz::u8string message,
		bz::vector<ctx::note> notes = {},
		bz::vector<ctx::suggestion> suggestions = {}
	) const;
	void report_warning(
		lex::src_tokens src_tokens, bz::u8string message,
		bz::vector<ctx::note> notes = {},
		bz::vector<ctx::suggestion> suggestions = {}
	) const;
	template<typename T>
	void report_warning(
		T const &tokens, bz::u8string message,
		bz::vector<ctx::note> notes = {},
		bz::vector<ctx::suggestion> suggestions = {}
	) const
	{
		this->report_warning(
			{ tokens.get_tokens_begin(), tokens.get_tokens_pivot(), tokens.get_tokens_end() },
			std::move(message), std::move(notes), std::move(suggestions)
		);
	}

	void report_parenthesis_suppressed_warning(
		lex::token_pos it, bz::u8string message,
		bz::vector<ctx::note> notes = {},
		bz::vector<ctx::suggestion> suggestions = {}
	) const;
	void report_parenthesis_suppressed_warning(
		lex::src_tokens src_tokens, bz::u8string message,
		bz::vector<ctx::note> notes = {},
		bz::vector<ctx::suggestion> suggestions = {}
	) const;
	template<typename T>
	void report_parenthesis_suppressed_warning(
		T const &tokens, bz::u8string message,
		bz::vector<ctx::note> notes = {},
		bz::vector<ctx::suggestion> suggestions = {}
	) const
	{
		if (this->is_parenthesis_suppressed)
		{
			return;
		}
		this->report_parenthesis_suppressed_warning(
			{ tokens.get_tokens_begin(), tokens.get_tokens_pivot(), tokens.get_tokens_end() },
			std::move(message), std::move(notes), std::move(suggestions)
		);
	}

	bool has_errors(void) const;
	lex::token_pos assert_token(lex::token_pos &stream, uint32_t kind) const;
	lex::token_pos assert_token(lex::token_pos &stream, uint32_t kind1, uint32_t kind2) const;

	void report_ambiguous_id_error(lex::token_pos id) const;

	void add_scope(void);
	void remove_scope(void);

	void add_local_variable(ast::decl_variable &var_decl);
	void add_local_function(ast::decl_function &func_decl);
	void add_local_operator(ast::decl_operator &op_decl);
	void add_local_struct(ast::decl_struct &struct_decl);

	ast::expression make_identifier_expression(lex::token_pos id) const;
	ast::expression make_literal(lex::token_pos literal) const;
	ast::expression make_string_literal(lex::token_pos begin, lex::token_pos end) const;
	ast::expression make_tuple(lex::src_tokens src_tokens, bz::vector<ast::expression> elems) const;

	ast::expression make_unary_operator_expression(
		lex::src_tokens src_tokens,
		lex::token_pos op,
		ast::expression expr
	);
	ast::expression make_binary_operator_expression(
		lex::src_tokens src_tokens,
		lex::token_pos op,
		ast::expression lhs,
		ast::expression rhs
	);
	ast::expression make_function_call_expression(
		lex::src_tokens src_tokens,
		ast::expression called,
		bz::vector<ast::expression> params
	);
	ast::expression make_cast_expression(
		lex::src_tokens src_tokens,
		lex::token_pos op,
		ast::expression expr,
		ast::typespec type
	);

	void match_expression_to_type(
		ast::expression &expr,
		ast::typespec &type
	);

	bool is_implicitly_convertible(ast::expression const &from, ast::typespec const &to);
	bool is_explicitly_convertible(ast::expression const &from, ast::typespec const &to);

	ast::type_info *get_type_info(bz::u8string_view id) const;

	ast::type_info *get_base_type_info(uint32_t kind) const;
};

} // namespace ctx

#endif // CTX_PARSE_CONTEXT_H
