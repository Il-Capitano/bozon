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
	struct resolve_queue_t
	{
		lex::src_tokens requester;
		bz::variant<
			ast::function_body *,
			ast::decl_variable *
		> requested;
	};

	struct consteval_call_stack_t
	{
		lex::src_tokens tokens;
		bz::u8string    mesage;
	};

	global_context      &global_ctx;
	decl_set            *global_decls;
	bz::vector<decl_set> scope_decls;
	bz::vector<ast::function_body *> generic_functions;
	bz::vector<std::size_t>          generic_function_scope_start;

	bz::vector<resolve_queue_t>        resolve_queue;
	bz::vector<consteval_call_stack_t> consteval_call_stack;


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
	void report_error(lex::src_tokens src_tokens) const
	{
		if (src_tokens.end - src_tokens.begin == 1)
		{
			this->report_error(src_tokens.begin);
		}
		else
		{
			this->report_error(src_tokens, "unexpected tokens");
		}
	}
	void report_error(
		bz::u8string message,
		bz::vector<note> notes = {},
		bz::vector<suggestion> suggestions = {}
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

	void report_paren_match_error(
		lex::token_pos it, lex::token_pos open_paren_it,
		bz::vector<ctx::note> notes = {}, bz::vector<ctx::suggestion> suggestions = {}
	) const;

	void report_circular_dependency_error(ast::function_body &func_body) const;

	void report_warning(
		warning_kind kind,
		lex::token_pos it, bz::u8string message,
		bz::vector<ctx::note> notes = {},
		bz::vector<ctx::suggestion> suggestions = {}
	) const;
	void report_warning(
		warning_kind kind,
		lex::src_tokens src_tokens, bz::u8string message,
		bz::vector<ctx::note> notes = {},
		bz::vector<ctx::suggestion> suggestions = {}
	) const;
	template<typename T>
	void report_warning(
		warning_kind kind,
		T const &tokens, bz::u8string message,
		bz::vector<ctx::note> notes = {},
		bz::vector<ctx::suggestion> suggestions = {}
	) const
	{
		this->report_warning(
			kind,
			{ tokens.get_tokens_begin(), tokens.get_tokens_pivot(), tokens.get_tokens_end() },
			std::move(message), std::move(notes), std::move(suggestions)
		);
	}

	void report_parenthesis_suppressed_warning(
		int parens_count,
		warning_kind kind,
		lex::token_pos it, bz::u8string message,
		bz::vector<ctx::note> notes = {},
		bz::vector<ctx::suggestion> suggestions = {}
	) const;
	void report_parenthesis_suppressed_warning(
		int parens_count,
		warning_kind kind,
		lex::src_tokens src_tokens, bz::u8string message,
		bz::vector<ctx::note> notes = {},
		bz::vector<ctx::suggestion> suggestions = {}
	) const;
	template<typename T>
	void report_parenthesis_suppressed_warning(
		int parens_count,
		warning_kind kind,
		T const &tokens, bz::u8string message,
		bz::vector<ctx::note> notes = {},
		bz::vector<ctx::suggestion> suggestions = {}
	) const
	{
		this->report_parenthesis_suppressed_warning(
			parens_count, kind,
			{ tokens.get_tokens_begin(), tokens.get_tokens_pivot(), tokens.get_tokens_end() },
			std::move(message), std::move(notes), std::move(suggestions)
		);
	}

	[[nodiscard]] static note make_note(uint32_t file_id, uint32_t line, bz::u8string message);
	[[nodiscard]] static note make_note(lex::token_pos it, bz::u8string message);
	[[nodiscard]] static note make_note(lex::src_tokens src_tokens, bz::u8string message);
	template<typename T>
	[[nodiscard]] static note make_note(T const &tokens, bz::u8string message)
	{
		return make_note(
			{ tokens.get_tokens_begin(), tokens.get_tokens_pivot(), tokens.get_tokens_end() },
			std::move(message)
		);
	}

	[[nodiscard]] static note make_note(
		lex::token_pos it, bz::u8string message,
		char_pos suggestion_pos, bz::u8string suggestion_str
	);
	[[nodiscard]] static note make_paren_match_note(
		lex::token_pos it, lex::token_pos open_paren_it
	);
	// should only be used for generic intrinsic instantiation reporting
	[[nodiscard]] static note make_note(bz::u8string message);

	[[nodiscard]] static suggestion make_suggestion_before(
		lex::token_pos it,
		char_pos erase_begin, char_pos erase_end,
		bz::u8string suggestion_str,
		bz::u8string message
	);
	[[nodiscard]] static suggestion make_suggestion_before(
		lex::token_pos first_it,
		char_pos first_erase_begin, char_pos first_erase_end,
		bz::u8string first_suggestion_str,
		lex::token_pos second_it,
		char_pos second_erase_begin, char_pos second_erase_end,
		bz::u8string second_suggestion_str,
		bz::u8string message
	);
	[[nodiscard]] static suggestion make_suggestion_after(
		lex::token_pos it,
		char_pos erase_begin, char_pos erase_end,
		bz::u8string suggestion_str,
		bz::u8string message
	);
	[[nodiscard]] static suggestion make_suggestion_around(
		lex::token_pos first,
		char_pos first_erase_begin, char_pos first_erase_end,
		bz::u8string first_suggestion_str,
		lex::token_pos last,
		char_pos second_erase_begin, char_pos second_erase_end,
		bz::u8string last_suggestion_str,
		bz::u8string message
	);

	[[nodiscard]] static suggestion make_suggestion_before(
		lex::token_pos it, bz::u8string suggestion_str,
		bz::u8string message
	)
	{
		return make_suggestion_before(
			it, char_pos(), char_pos(),
			std::move(suggestion_str), std::move(message)
		);
	}

	[[nodiscard]] static suggestion make_suggestion_after(
		lex::token_pos it, bz::u8string suggestion_str,
		bz::u8string message
	)
	{
		return make_suggestion_after(
			it, char_pos(), char_pos(),
			std::move(suggestion_str), std::move(message)
		);
	}

	[[nodiscard]] static suggestion make_suggestion_around(
		lex::token_pos first, bz::u8string first_suggestion_str,
		lex::token_pos last, bz::u8string last_suggestion_str,
		bz::u8string message
	)
	{
		return make_suggestion_around(
			first, char_pos(), char_pos(), std::move(first_suggestion_str),
			last,  char_pos(), char_pos(), std::move(last_suggestion_str),
			std::move(message)
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

	void add_function_for_compilation(ast::function_body &func_body) const;

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
	ast::expression make_subscript_operator_expression(
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

	// bool is_implicitly_convertible(ast::expression const &from, ast::typespec_view to);
	// bool is_explicitly_convertible(ast::expression const &from, ast::typespec_view to);

	void add_to_resolve_queue(lex::src_tokens tokens, ast::function_body &func_body)
	{ this->resolve_queue.emplace_back(tokens, &func_body); }
	void add_to_resolve_queue(lex::src_tokens tokens, ast::decl_variable &var_decl)
	{ this->resolve_queue.emplace_back(tokens, &var_decl); }

	void pop_resolve_queue(void)
	{ this->resolve_queue.pop_back(); }

	void add_to_consteval_call_stack(lex::src_tokens tokens, bz::u8string message)
	{ this->consteval_call_stack.emplace_back(tokens, std::move(message)); }

	void pop_consteval_call_stack(void)
	{ this->consteval_call_stack.pop_back(); }
};

} // namespace ctx

#endif // CTX_PARSE_CONTEXT_H
