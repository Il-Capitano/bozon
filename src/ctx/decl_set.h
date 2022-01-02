#ifndef CTX_DECL_SET_H
#define CTX_DECL_SET_H

#include "ast/typespec.h"
#include "ast/expression.h"
#include "ast/statement.h"

namespace ctx
{

/*

struct function_overload_set
{
	ast::identifier id;
	bz::vector<ast::statement_view> func_decls;
	bz::vector<ast::statement_view> alias_decls;
};

struct operator_overload_set
{
	bz::array_view<bz::u8string_view const> scope;
	uint32_t op;
	// will always be ast::decl_operator
	bz::vector<ast::statement_view> op_decls;
};

struct variadic_var_decl
{
	ast::decl_variable *original_decl;
	bz::vector<ast::decl_variable *> var_decls;
};

using symbol_t = bz::variant<
	ast::decl_variable *,
	variadic_var_decl,
	function_overload_set,
	ast::decl_type_alias *,
	ast::decl_struct *,
	ast::identifier
>;

ast::identifier const &get_symbol_id(symbol_t const &symbol);
lex::src_tokens get_symbol_src_tokens(symbol_t const &symbol);

struct decl_set
{
	bz::vector<symbol_t> symbols;
	bz::vector<operator_overload_set> op_sets;

	auto var_decl_range(void) const
	{
		return this->symbols
			.filter([](auto const &symbol) { return symbol.template is<ast::decl_variable *>(); })
			.transform([](auto const &symbol) -> auto const & { return symbol.template get<ast::decl_variable *>(); });
	}

	auto variadic_var_decl_range(void) const
	{
		return this->symbols
			.filter([](auto const &symbol) { return symbol.template is<variadic_var_decl>(); })
			.transform([](auto const &symbol) -> auto const & { return symbol.template get<variadic_var_decl>(); });
	}

	auto function_overload_set_range(void) const
	{
		return this->symbols
			.filter([](auto const &symbol) { return symbol.template is<function_overload_set>(); })
			.transform([](auto const &symbol) -> auto const & { return symbol.template get<function_overload_set>(); });
	}

	auto type_alias_range(void) const
	{
		return this->symbols
			.filter([](auto const &symbol) { return symbol.template is<ast::decl_type_alias *>(); })
			.transform([](auto const &symbol) -> auto const & { return symbol.template get<ast::decl_type_alias *>(); });
	}

	auto type_range(void) const
	{
		return this->symbols
			.filter([](auto const &symbol) { return symbol.template is<ast::decl_struct *>(); })
			.transform([](auto const &symbol) -> auto const & { return symbol.template get<ast::decl_struct *>(); });
	}

	symbol_t *find_by_id(ast::identifier const &id);
	bz::vector<symbol_t *> find_by_unqualified_id(
		ast::identifier const &id,
		bz::array_view<bz::u8string_view const> current_scope
	);
	// used for universal function calls
	bz::vector<symbol_t *> find_by_unqualified_id(
		ast::identifier const &id,
		bz::array_view<bz::u8string_view const> current_scope,
		bz::array_view<bz::u8string_view const> base_scope
	);

	// these functions return a non-null symbol_t * if there's a redeclaration

	[[nodiscard]] symbol_t *add_symbol(symbol_t const &symbol);

	[[nodiscard]] symbol_t *add_function(ast::statement &stmt);
	[[nodiscard]] symbol_t *add_function(ast::decl_function &func_decl);
	[[nodiscard]] symbol_t *add_function_set(function_overload_set const &func_set);

	[[nodiscard]] symbol_t *add_operator(ast::statement &stmt);
	[[nodiscard]] symbol_t *add_operator(ast::decl_operator &op_decl);
	[[nodiscard]] symbol_t *add_operator_set(operator_overload_set const &op_set);

	[[nodiscard]] symbol_t *add_function_alias(ast::decl_function_alias &alias_decl);
	[[nodiscard]] symbol_t *add_type_alias(ast::decl_type_alias &alias_decl);
	[[nodiscard]] symbol_t *add_type(ast::decl_struct &struct_decl);
	[[nodiscard]] symbol_t *add_variable(ast::decl_variable &var_decl);
	[[nodiscard]] symbol_t *add_variadic_variable(ast::decl_variable &original_decl, bz::vector<ast::decl_variable *> variadic_decls);

	void add_local_function(ast::statement &stmt);
	void add_local_function(ast::decl_function &func_decl);
	void add_local_function_set(function_overload_set const &func_set);

	void add_local_function_alias(ast::decl_function_alias &alias_decl);
	void add_local_type_alias(ast::decl_type_alias &alias_decl);
	void add_local_type(ast::decl_struct &struct_decl);
	void add_local_variable(ast::decl_variable &var_decl);
	void add_local_variadic_variable(ast::decl_variable &original_decl, bz::vector<ast::decl_variable *> variadic_decls);

	void add_unresolved_id(ast::identifier id);
};

*/

} // namespace ctx

#endif // CTX_DECL_SET_H
