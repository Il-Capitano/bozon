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
			ast::decl_variable *,
			ast::decl_function_alias *,
			ast::decl_type_alias *,
			ast::type_info *
		> requested;
	};

	struct consteval_call_stack_t
	{
		lex::src_tokens tokens;
		bz::u8string    mesage;
	};

	global_context      &global_ctx;
	decl_set            *global_decls = nullptr;
	bz::vector<decl_set> scope_decls{};
	bz::vector<ast::function_body *> generic_functions{};
	bz::vector<std::size_t>          generic_function_scope_start{};
	uint32_t                                current_file_id = std::numeric_limits<uint32_t>::max();
	bz::array_view<bz::u8string_view const> current_scope{};
	ast::function_body                     *current_function = nullptr;

	bz::vector<resolve_queue_t> resolve_queue{};


	struct local_copy_t {};
	struct global_copy_t {};

	parse_context(global_context &_global_ctx);
	parse_context(parse_context const &other, local_copy_t);
	parse_context(parse_context const &other, global_copy_t);
	parse_context(parse_context const &other) = delete;
	parse_context(parse_context &&)           = delete;
	parse_context &operator = (parse_context const &) = delete;
	parse_context &operator = (parse_context &&)      = delete;

	ast::type_info *get_builtin_type_info(uint32_t kind) const;
	ast::type_info *get_usize_type_info(void) const;
	ast::type_info *get_isize_type_info(void) const;
	ast::typespec_view get_builtin_type(bz::u8string_view name) const;
	ast::function_body *get_builtin_function(uint32_t kind) const;
	bz::array_view<uint32_t const> get_builtin_universal_functions(bz::u8string_view id);

	void report_error(lex::token_pos it) const;
	void report_error(
		lex::token_pos it, bz::u8string message,
		bz::vector<source_highlight> notes = {},
		bz::vector<source_highlight> suggestions = {}
	) const;
	void report_error(
		lex::src_tokens src_tokens, bz::u8string message,
		bz::vector<source_highlight> notes = {},
		bz::vector<source_highlight> suggestions = {}
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
		lex::token_range range, bz::u8string message,
		bz::vector<source_highlight> notes = {},
		bz::vector<source_highlight> suggestions = {}
	)
	{
		this->report_error(
			{ range.begin, range.begin, range.end },
			std::move(message),
			std::move(notes), std::move(suggestions)
		);
	}
	void report_error(
		bz::u8string message,
		bz::vector<source_highlight> notes = {},
		bz::vector<source_highlight> suggestions = {}
	) const;
	template<typename T>
	void report_error(
		T const &tokens, bz::u8string message,
		bz::vector<source_highlight> notes = {},
		bz::vector<source_highlight> suggestions = {}
	) const
	{
		this->report_error(
			{ tokens.get_tokens_begin(), tokens.get_tokens_pivot(), tokens.get_tokens_end() },
			std::move(message), std::move(notes), std::move(suggestions)
		);
	}

	void report_paren_match_error(
		lex::token_pos it, lex::token_pos open_paren_it,
		bz::vector<source_highlight> notes = {}, bz::vector<source_highlight> suggestions = {}
	) const;

	void report_circular_dependency_error(ast::function_body &func_body) const;
	void report_circular_dependency_error(ast::decl_function_alias &alias_decl) const;
	void report_circular_dependency_error(ast::decl_type_alias &alias_decl) const;
	void report_circular_dependency_error(ast::type_info &info) const;
	void report_circular_dependency_error(ast::decl_variable &var_decl) const;

	void report_warning(
		warning_kind kind,
		lex::token_pos it, bz::u8string message,
		bz::vector<source_highlight> notes = {},
		bz::vector<source_highlight> suggestions = {}
	) const;
	void report_warning(
		warning_kind kind,
		lex::src_tokens src_tokens, bz::u8string message,
		bz::vector<source_highlight> notes = {},
		bz::vector<source_highlight> suggestions = {}
	) const;
	template<typename T>
	void report_warning(
		warning_kind kind,
		T const &tokens, bz::u8string message,
		bz::vector<source_highlight> notes = {},
		bz::vector<source_highlight> suggestions = {}
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
		bz::vector<source_highlight> notes = {},
		bz::vector<source_highlight> suggestions = {}
	) const;
	void report_parenthesis_suppressed_warning(
		int parens_count,
		warning_kind kind,
		lex::src_tokens src_tokens, bz::u8string message,
		bz::vector<source_highlight> notes = {},
		bz::vector<source_highlight> suggestions = {}
	) const;
	template<typename T>
	void report_parenthesis_suppressed_warning(
		int parens_count,
		warning_kind kind,
		T const &tokens, bz::u8string message,
		bz::vector<source_highlight> notes = {},
		bz::vector<source_highlight> suggestions = {}
	) const
	{
		this->report_parenthesis_suppressed_warning(
			parens_count, kind,
			{ tokens.get_tokens_begin(), tokens.get_tokens_pivot(), tokens.get_tokens_end() },
			std::move(message), std::move(notes), std::move(suggestions)
		);
	}

	[[nodiscard]] static source_highlight make_note(uint32_t file_id, uint32_t line, bz::u8string message);
	[[nodiscard]] static source_highlight make_note(lex::token_pos it, bz::u8string message);
	[[nodiscard]] static source_highlight make_note(lex::src_tokens src_tokens, bz::u8string message);
	[[nodiscard]] static source_highlight make_note(lex::token_range range, bz::u8string message)
	{
		return make_note({ range.begin, range.begin, range.end }, std::move(message));
	}
	template<typename T>
	[[nodiscard]] static source_highlight make_note(T const &tokens, bz::u8string message)
	{
		return make_note(
			{ tokens.get_tokens_begin(), tokens.get_tokens_pivot(), tokens.get_tokens_end() },
			std::move(message)
		);
	}

	[[nodiscard]] static source_highlight make_note(
		lex::token_pos it, bz::u8string message,
		char_pos suggestion_pos, bz::u8string suggestion_str
	);
	[[nodiscard]] static source_highlight make_paren_match_note(
		lex::token_pos it, lex::token_pos open_paren_it
	);
	// should only be used for generic intrinsic instantiation reporting
	[[nodiscard]] static source_highlight make_note(bz::u8string message);
	[[nodiscard]] static source_highlight make_note_with_suggestion_around(
		lex::src_tokens src_tokens,
		lex::token_pos begin, bz::u8string first_suggestion,
		lex::token_pos end, bz::u8string second_suggestion,
		bz::u8string message
	);

	[[nodiscard]] static source_highlight make_suggestion_before(
		lex::token_pos it,
		char_pos erase_begin, char_pos erase_end,
		bz::u8string suggestion_str,
		bz::u8string message
	);
	[[nodiscard]] static source_highlight make_suggestion_before(
		lex::token_pos first_it,
		char_pos first_erase_begin, char_pos first_erase_end,
		bz::u8string first_suggestion_str,
		lex::token_pos second_it,
		char_pos second_erase_begin, char_pos second_erase_end,
		bz::u8string second_suggestion_str,
		bz::u8string message
	);
	[[nodiscard]] static source_highlight make_suggestion_after(
		lex::token_pos it,
		char_pos erase_begin, char_pos erase_end,
		bz::u8string suggestion_str,
		bz::u8string message
	);
	[[nodiscard]] static source_highlight make_suggestion_around(
		lex::token_pos first,
		char_pos first_erase_begin, char_pos first_erase_end,
		bz::u8string first_suggestion_str,
		lex::token_pos last,
		char_pos second_erase_begin, char_pos second_erase_end,
		bz::u8string last_suggestion_str,
		bz::u8string message
	);

	[[nodiscard]] static source_highlight make_suggestion_before(
		lex::token_pos it, bz::u8string suggestion_str,
		bz::u8string message
	)
	{
		return make_suggestion_before(
			it, char_pos(), char_pos(),
			std::move(suggestion_str), std::move(message)
		);
	}

	[[nodiscard]] static source_highlight make_suggestion_after(
		lex::token_pos it, bz::u8string suggestion_str,
		bz::u8string message
	)
	{
		return make_suggestion_after(
			it, char_pos(), char_pos(),
			std::move(suggestion_str), std::move(message)
		);
	}

	[[nodiscard]] static source_highlight make_suggestion_around(
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

	void set_current_file(uint32_t file_id);

	struct src_file_info_t
	{
		uint32_t file_id;
		bz::array_view<bz::u8string_view const> scope;
		decl_set *global_decls;
	};

	src_file_info_t get_current_file_info(void) const noexcept
	{
		return { this->current_file_id, this->current_scope, this->global_decls };
	}

	void set_current_file_info(src_file_info_t info) noexcept
	{
		this->current_file_id = info.file_id;
		this->current_scope   = info.scope;
		this->global_decls    = info.global_decls;
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
	void add_local_type_alias(ast::decl_type_alias &type_alias);

	void add_function_for_compilation(ast::function_body &func_body) const;

	ast::expression make_identifier_expression(ast::identifier id);
	ast::expression make_literal(lex::token_pos literal) const;
	ast::expression make_string_literal(lex::token_pos begin, lex::token_pos end) const;
	ast::expression make_tuple(lex::src_tokens src_tokens, bz::vector<ast::expression> elems) const;

	ast::expression make_unary_operator_expression(
		lex::src_tokens src_tokens,
		uint32_t op_kind,
		ast::expression expr
	);
	ast::expression make_binary_operator_expression(
		lex::src_tokens src_tokens,
		uint32_t op_kind,
		ast::expression lhs,
		ast::expression rhs
	);
	ast::expression make_function_call_expression(
		lex::src_tokens src_tokens,
		ast::expression called,
		bz::vector<ast::expression> params
	);
	ast::expression make_universal_function_call_expression(
		lex::src_tokens src_tokens,
		ast::expression base,
		ast::identifier id,
		bz::vector<ast::expression> params
	);
	ast::expression make_subscript_operator_expression(
		lex::src_tokens src_tokens,
		ast::expression called,
		bz::vector<ast::expression> params
	);
	ast::expression make_cast_expression(
		lex::src_tokens src_tokens,
		ast::expression expr,
		ast::typespec type
	);
	ast::expression make_member_access_expression(
		lex::src_tokens src_tokens,
		ast::expression base,
		lex::token_pos member
	);

	void match_expression_to_type(
		ast::expression &expr,
		ast::typespec &type
	);
	void match_expression_to_variable(
		ast::expression &expr,
		ast::decl_variable &var_decl
	);

	bool is_instantiable(ast::typespec_view ts);
	size_t get_sizeof(ast::typespec_view ts);

	bz::vector<ast::function_body *> get_function_bodies_from_unqualified_id(
		lex::src_tokens requester,
		bz::array_view<bz::u8string_view const> id
	);
	bz::vector<ast::function_body *> get_function_bodies_from_qualified_id(
		lex::src_tokens requester,
		bz::array_view<bz::u8string_view const> id
	);

	ast::identifier make_qualified_identifier(lex::token_pos id);

	ast::constant_value execute_function(
		lex::src_tokens src_tokens,
		ast::function_body *body,
		bz::array_view<ast::expression const> params
	);

	ast::constant_value execute_compound_expression(
		lex::src_tokens src_tokens,
		ast::expr_compound &expr
	);

	// bool is_implicitly_convertible(ast::expression const &from, ast::typespec_view to);
	// bool is_explicitly_convertible(ast::expression const &from, ast::typespec_view to);

	void add_to_resolve_queue(lex::src_tokens tokens, ast::function_body &func_body)
	{ this->resolve_queue.emplace_back(tokens, &func_body); }
	void add_to_resolve_queue(lex::src_tokens tokens, ast::decl_function_alias &alias_decl)
	{ this->resolve_queue.emplace_back(tokens, &alias_decl); }
	void add_to_resolve_queue(lex::src_tokens tokens, ast::decl_type_alias &alias_decl)
	{ this->resolve_queue.emplace_back(tokens, &alias_decl); }
	void add_to_resolve_queue(lex::src_tokens tokens, ast::type_info &info)
	{ this->resolve_queue.emplace_back(tokens, &info); }
	void add_to_resolve_queue(lex::src_tokens tokens, ast::decl_variable &var_decl)
	{ this->resolve_queue.emplace_back(tokens, &var_decl); }

	void pop_resolve_queue(void)
	{ this->resolve_queue.pop_back(); }
};

} // namespace ctx

#endif // CTX_PARSE_CONTEXT_H
