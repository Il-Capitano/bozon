#ifndef CTX_PARSE_CONTEXT_H
#define CTX_PARSE_CONTEXT_H

#include "core.h"

#include "error.h"
#include "context_forward.h"
#include "lex/token.h"
#include "ast/typespec.h"
#include "ast/expression.h"
#include "ast/statement.h"
#include "ast/scope.h"

namespace ctx
{

struct parse_context
{
	struct resolve_queue_t
	{
		lex::src_tokens requester;
		bz::variant<
			ast::function_body *,
			ast::decl_variable *,
			ast::decl_function_alias *,
			ast::decl_operator_alias *,
			ast::decl_type_alias *,
			ast::type_info *,
			ast::decl_enum *
		> requested;
	};

	struct consteval_call_stack_t
	{
		lex::src_tokens tokens;
		bz::u8string    mesage;
	};

	struct variadic_resolve_info_t
	{
		bool is_resolving_variadic;
		bool found_variadic;
		uint32_t variadic_index;
		uint32_t variadic_size;
		lex::src_tokens first_variadic_src_tokens;
	};

	global_context &global_ctx;

	bz::vector<ast::function_body *> generic_functions{};
	bz::vector<std::size_t>          generic_function_scope_start{};

	ast::scope_t                 *current_global_scope = nullptr;
	ast::enclosing_scope_t        current_local_scope  = {};
	bz::vector<bz::u8string_view> current_unresolved_locals = {};
	ast::function_body           *current_function = nullptr;

	struct move_scope_t
	{
		lex::src_tokens src_tokens;
		bz::vector<bz::vector<ast::decl_variable *>> move_branches;
	};

	bz::vector<move_scope_t> move_scopes = {};

	bz::vector<resolve_queue_t> resolve_queue{};

	variadic_resolve_info_t variadic_info = { false, false, 0, 0, {} };

	bool is_aggressive_consteval_enabled = false;
	bool in_loop = false;
	bool parsing_variadic_expansion = false;
	bool in_unevaluated_context = false;
	bool in_unresolved_context = false;
	int parsing_template_argument = 0;


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
	ast::decl_function *get_builtin_function(uint32_t kind) const;
	ast::decl_operator *get_builtin_operator(uint32_t op_kind, uint8_t expr_type_kind) const;
	ast::decl_operator *get_builtin_operator(uint32_t op_kind, uint8_t lhs_type_kind, uint8_t rhs_type_kind) const;
	bz::array_view<uint32_t const> get_builtin_universal_functions(bz::u8string_view id);
	ast::type_prototype_set_t &get_type_prototype_set(void);

	struct loop_info_t
	{
		bool in_loop;
	};

	[[nodiscard]] loop_info_t push_loop(void) noexcept;
	void pop_loop(loop_info_t prev_info) noexcept;
	bool is_in_loop(void) const noexcept;

	[[nodiscard]] variadic_resolve_info_t push_variadic_resolver(void) noexcept;
	void pop_variadic_resolver(variadic_resolve_info_t const &prev_info) noexcept;

	[[nodiscard]] bool push_parsing_variadic_expansion(void) noexcept;
	void pop_parsing_variadic_expansion(bool prev_value) noexcept;

	[[nodiscard]] bool push_unevaluated_context(void) noexcept;
	void pop_unevaluated_context(bool prev_value) noexcept;

	[[nodiscard]] bool push_unresolved_context(void) noexcept;
	void pop_unresolved_context(bool prev_value) noexcept;

	void push_parsing_template_argument(void) noexcept;
	void pop_parsing_template_argument(void) noexcept;
	bool is_parsing_template_argument(void) const noexcept;

	bool register_variadic(lex::src_tokens const &src_tokens, ast::variadic_var_decl_ref variadic_decl);
	bool register_variadic(lex::src_tokens const &src_tokens, ast::variadic_var_decl const &variadic_decl);
	uint32_t get_variadic_index(void) const;

	[[nodiscard]] ast::function_body *push_current_function(ast::function_body *new_function) noexcept;
	void pop_current_function(ast::function_body *prev_function) noexcept;

	struct global_local_scope_pair_t
	{
		ast::scope_t *global_scope;
		ast::enclosing_scope_t local_scope;
		bz::vector<bz::u8string_view> unresolved_locals;
	};

	[[nodiscard]] global_local_scope_pair_t push_global_scope(ast::scope_t *new_scope) noexcept;
	void pop_global_scope(global_local_scope_pair_t prev_scopes) noexcept;

	void push_local_scope(ast::scope_t *new_scope) noexcept;
	void pop_local_scope(bool report_unused) noexcept;

	[[nodiscard]] global_local_scope_pair_t push_enclosing_scope(ast::enclosing_scope_t new_scope) noexcept;
	void pop_enclosing_scope(global_local_scope_pair_t prev_scopes) noexcept;

	ast::enclosing_scope_t get_current_enclosing_scope(void) const noexcept;

	bool has_common_global_scope(ast::enclosing_scope_t scope) const noexcept;

	void push_move_scope(lex::src_tokens const &src_tokens) noexcept;
	void pop_move_scope(void) noexcept;
	void push_new_move_branch(void) noexcept;
	void register_move(lex::src_tokens const &src_tokens, ast::decl_variable *decl) noexcept;
	void register_move_construction(ast::decl_variable *decl) noexcept;

	void report_error(lex::token_pos it) const;
	void report_error(
		lex::token_pos it, bz::u8string message,
		bz::vector<source_highlight> notes = {},
		bz::vector<source_highlight> suggestions = {}
	) const;
	void report_error(
		lex::src_tokens const &src_tokens, bz::u8string message,
		bz::vector<source_highlight> notes = {},
		bz::vector<source_highlight> suggestions = {}
	) const;
	void report_error(lex::src_tokens const &src_tokens) const
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
	void report_circular_dependency_error(ast::decl_operator_alias &alias_decl) const;
	void report_circular_dependency_error(ast::decl_type_alias &alias_decl) const;
	void report_circular_dependency_error(ast::type_info &info) const;
	void report_circular_dependency_error(ast::decl_enum &enum_decl) const;
	void report_circular_dependency_error(ast::decl_variable &var_decl) const;

	void report_warning(
		warning_kind kind,
		lex::token_pos it, bz::u8string message,
		bz::vector<source_highlight> notes = {},
		bz::vector<source_highlight> suggestions = {}
	) const;
	void report_warning(
		warning_kind kind,
		lex::src_tokens const &src_tokens, bz::u8string message,
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
		lex::src_tokens const &src_tokens, bz::u8string message,
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
	[[nodiscard]] static source_highlight make_note(lex::src_tokens const &src_tokens, bz::u8string message);
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
	[[nodiscard]] static source_highlight make_note_with_suggestion_before(
		lex::src_tokens const &src_tokens,
		lex::token_pos it, bz::u8string suggestion,
		bz::u8string message
	);
	[[nodiscard]] static source_highlight make_note_with_suggestion_around(
		lex::src_tokens const &src_tokens,
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

	bool has_errors(void) const;
	lex::token_pos assert_token(lex::token_pos &stream, uint32_t kind) const;
	lex::token_pos assert_token(lex::token_pos &stream, uint32_t kind1, uint32_t kind2) const;

	void report_ambiguous_id_error(lex::token_pos id) const;

	bool has_main(void) const;
	ast::function_body *get_main(void) const;
	void set_main(ast::function_body *body);

	[[nodiscard]] size_t add_unresolved_scope(void);
	void remove_unresolved_scope(size_t prev_size);

	void add_unresolved_local(ast::identifier const &id);
	void add_unresolved_var_decl(ast::decl_variable const &var_decl);

	void add_local_variable(ast::decl_variable &var_decl);
	void add_local_variable(ast::decl_variable &original_decl, ast::arena_vector<ast::decl_variable *> variadic_decls);
	void add_local_function(ast::decl_function &func_decl);
	void add_local_operator(ast::decl_operator &op_decl);
	void add_local_struct(ast::decl_struct &struct_decl);
	void add_local_type_alias(ast::decl_type_alias &type_alias);

	void add_function_for_compilation(ast::function_body &func_body) const;

	ast::expression make_identifier_expression(ast::identifier id);
	ast::expression make_literal(lex::token_pos literal) const;
	ast::expression make_string_literal(lex::token_pos begin, lex::token_pos end) const;
	ast::expression make_tuple(lex::src_tokens const &src_tokens, ast::arena_vector<ast::expression> elems) const;
	ast::expression make_unreachable(lex::token_pos t);
	ast::expression make_integer_range_expression(lex::src_tokens const &src_tokens, ast::expression begin, ast::expression end);
	ast::expression make_integer_range_inclusive_expression(lex::src_tokens const &src_tokens, ast::expression begin, ast::expression end);
	ast::expression make_integer_range_from_expression(lex::src_tokens const &src_tokens, ast::expression begin);
	ast::expression make_integer_range_to_expression(lex::src_tokens const &src_tokens, ast::expression end);
	ast::expression make_integer_range_to_inclusive_expression(lex::src_tokens const &src_tokens, ast::expression end);
	ast::expression make_range_unbounded_expression(lex::src_tokens const &src_tokens);

	ast::expression make_unary_operator_expression(
		lex::src_tokens const &src_tokens,
		uint32_t op_kind,
		ast::expression expr
	);
	ast::expression make_binary_operator_expression(
		lex::src_tokens const &src_tokens,
		uint32_t op_kind,
		ast::expression lhs,
		ast::expression rhs
	);
	ast::expression make_function_call_expression(
		lex::src_tokens const &src_tokens,
		ast::expression called,
		ast::arena_vector<ast::expression> args
	);
	ast::expression make_universal_function_call_expression(
		lex::src_tokens const &src_tokens,
		ast::expression base,
		ast::identifier id,
		ast::arena_vector<ast::expression> args
	);
	ast::expression make_subscript_operator_expression(
		lex::src_tokens const &src_tokens,
		ast::expression called,
		ast::arena_vector<ast::expression> args
	);
	ast::expression make_cast_expression(
		lex::src_tokens const &src_tokens,
		ast::expression expr,
		ast::typespec type
	);
	ast::expression make_bit_cast_expression(
		lex::src_tokens const &src_tokens,
		ast::expression expr,
		ast::typespec result_type
	);
	ast::expression make_optional_cast_expression(ast::expression expr);
	ast::expression make_member_access_expression(
		lex::src_tokens const &src_tokens,
		ast::expression base,
		lex::token_pos member
	);
	ast::expression make_generic_type_instantiation_expression(
		lex::src_tokens const &src_tokens,
		ast::expression base,
		ast::arena_vector<ast::expression> args
	);

	ast::expression make_default_construction(lex::src_tokens const &src_tokens, ast::typespec_view type);
	ast::expression make_copy_construction(ast::expression expr);
	ast::expression make_move_construction(ast::expression expr);
	ast::expression make_default_assignment(lex::src_tokens const &src_tokens, ast::expression lhs, ast::expression rhs);

	void add_self_destruction(ast::expression &expr);
	void add_self_move_destruction(ast::expression &expr);
	ast::destruct_operation make_variable_destruction(ast::decl_variable *var_decl);
	ast::destruct_operation make_rvalue_array_destruction(lex::src_tokens const &src_tokens, ast::typespec_view type);

	void resolve_type(lex::src_tokens const &src_tokens, ast::type_info *info);
	void resolve_type_members(lex::src_tokens const &src_tokens, ast::type_info *info);
	void resolve_type(lex::src_tokens const &src_tokens, ast::decl_enum *decl);
	void resolve_typespec_members(lex::src_tokens const &src_tokens, ast::typespec_view ts);
	bool is_default_constructible(lex::src_tokens const &src_tokens, ast::typespec_view ts);
	bool is_copy_constructible(lex::src_tokens const &src_tokens, ast::typespec_view ts);
	bool is_trivially_copy_constructible(lex::src_tokens const &src_tokens, ast::typespec_view ts);
	bool is_move_constructible(lex::src_tokens const &src_tokens, ast::typespec_view ts);
	bool is_trivially_move_constructible(lex::src_tokens const &src_tokens, ast::typespec_view ts);
	bool is_trivially_destructible(lex::src_tokens const &src_tokens, ast::typespec_view ts);
	bool is_trivially_move_destructible(lex::src_tokens const &src_tokens, ast::typespec_view ts);
	bool is_trivially_relocatable(lex::src_tokens const &src_tokens, ast::typespec_view ts);
	bool is_trivial(lex::src_tokens const &src_tokens, ast::typespec_view ts);
	bool is_instantiable(lex::src_tokens const &src_tokens, ast::typespec_view ts);
	size_t get_sizeof(ast::typespec_view ts);

	ast::identifier make_qualified_identifier(lex::token_pos id);

	ast::constant_value execute_expression(ast::expression &expr);
	ast::constant_value execute_expression_without_error(ast::expression &expr);

	// bool is_implicitly_convertible(ast::expression const &from, ast::typespec_view to);
	// bool is_explicitly_convertible(ast::expression const &from, ast::typespec_view to);

	void add_to_resolve_queue(lex::src_tokens const &tokens, ast::function_body &func_body)
	{ this->resolve_queue.emplace_back(tokens, &func_body); }
	void add_to_resolve_queue(lex::src_tokens const &tokens, ast::decl_function_alias &alias_decl)
	{ this->resolve_queue.emplace_back(tokens, &alias_decl); }
	void add_to_resolve_queue(lex::src_tokens const &tokens, ast::decl_operator_alias &alias_decl)
	{ this->resolve_queue.emplace_back(tokens, &alias_decl); }
	void add_to_resolve_queue(lex::src_tokens const &tokens, ast::decl_type_alias &alias_decl)
	{ this->resolve_queue.emplace_back(tokens, &alias_decl); }
	void add_to_resolve_queue(lex::src_tokens const &tokens, ast::type_info &info)
	{ this->resolve_queue.emplace_back(tokens, &info); }
	void add_to_resolve_queue(lex::src_tokens const &tokens, ast::decl_enum &enum_decl)
	{ this->resolve_queue.emplace_back(tokens, &enum_decl); }
	void add_to_resolve_queue(lex::src_tokens const &tokens, ast::decl_variable &var_decl)
	{ this->resolve_queue.emplace_back(tokens, &var_decl); }

	void pop_resolve_queue(void)
	{ this->resolve_queue.pop_back(); }
};

} // namespace ctx

#endif // CTX_PARSE_CONTEXT_H
