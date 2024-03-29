#ifndef AST_SCOPE_H
#define AST_SCOPE_H

#include "core.h"
#include "allocator.h"
#include "identifier.h"
#include "statement_forward.h"

namespace ast
{

struct scope_t;

struct enclosing_scope_t
{
	scope_t *scope = nullptr;
	size_t symbol_count = 0;
};

inline bool operator == (enclosing_scope_t lhs, enclosing_scope_t rhs)
{
	return lhs.scope == rhs.scope && lhs.symbol_count == rhs.symbol_count;
}

inline bool operator != (enclosing_scope_t lhs, enclosing_scope_t rhs)
{
	return !(lhs == rhs);
}

struct function_overload_set
{
	arena_vector<decl_function *>       func_decls;
	arena_vector<decl_function_alias *> alias_decls;
};

struct operator_overload_set
{
	uint32_t op;
	arena_vector<decl_operator *>       op_decls;
	arena_vector<decl_operator_alias *> alias_decls;
};

struct variadic_var_decl
{
	decl_variable *original_decl;
	arena_vector<decl_variable *> variadic_decls;
};

struct variadic_var_decl_ref
{
	decl_variable *original_decl;
	bz::array_view<decl_variable * const> variadic_decls;
};

enum class global_scope_symbol_kind : uint32_t
{
	function_set,
	variable,
	variadic_variable,
	type_alias,
	struct_,
	enum_,
	ambiguous,
	none,
};

struct global_scope_symbol_index_t
{
	global_scope_symbol_kind symbol_kind;
	uint32_t index;
};

struct identifier_hash
{
	size_t operator () (bz::array_view<bz::u8string_view const> id) const
	{
		auto hasher = std::hash<std::string_view>();
		size_t result = 0;
		for (auto const value : id)
		{
			// https://stackoverflow.com/questions/35985960/c-why-is-boosthash-combine-the-best-way-to-combine-hash-values
			result ^= hasher(std::string_view(value.data(), value.size())) + 0x9e3779b9 + (result << 6) + (result >> 2);
		}
		return result;
	}
};

struct global_scope_symbol_list_t
{
	arena_vector<function_overload_set> function_sets;
	arena_vector<operator_overload_set> operator_sets;
	arena_vector<decl_variable   *> variables;
	arena_vector<variadic_var_decl> variadic_variables;
	arena_vector<decl_type_alias *> type_aliases;
	arena_vector<decl_struct     *> structs;
	arena_vector<decl_enum       *> enums;

	using id_map_t = std::unordered_map<bz::array_view<bz::u8string_view const>, global_scope_symbol_index_t, identifier_hash>;
	using ambiguous_id_map_t = std::unordered_map<bz::array_view<bz::u8string_view const>, bz::vector<global_scope_symbol_index_t>, identifier_hash>;

	id_map_t id_map;
	ambiguous_id_map_t ambiguous_id_map;
	bz::vector<bz::vector<bz::u8string_view>> id_storage;

	std::pair<id_map_t::iterator, bool> insert(bz::array_view<bz::u8string_view const> id, global_scope_symbol_index_t index);

	void add_variable(bz::array_view<bz::u8string_view const> id, decl_variable &var_decl);
	void add_variable(bz::array_view<bz::u8string_view const> id, decl_variable &original_decl, arena_vector<decl_variable *> variadic_decls);
	void add_function(bz::array_view<bz::u8string_view const> id, decl_function &func_decl);
	void add_function_alias(bz::array_view<bz::u8string_view const> id, decl_function_alias &alias_decl);
	void add_operator(decl_operator &op_decl);
	void add_operator_alias(decl_operator_alias &alias_decl);
	void add_type_alias(bz::array_view<bz::u8string_view const> id, decl_type_alias &alias_decl);
	void add_struct(bz::array_view<bz::u8string_view const> id, decl_struct &struct_decl);
	void add_enum(bz::array_view<bz::u8string_view const> id, decl_enum &enum_decl);

	global_scope_symbol_index_t get_symbol_index_by_id(identifier const &id) const;
	bz::array_view<global_scope_symbol_index_t const> get_ambiguous_symbols_by_id(identifier const &id) const;
};

struct global_scope_t
{
	global_scope_symbol_list_t all_symbols;
	global_scope_symbol_list_t export_symbols;

	void add_variable(bz::array_view<bz::u8string_view const> id, decl_variable &var_decl);
	void add_variable(bz::array_view<bz::u8string_view const> id, decl_variable &original_decl, arena_vector<decl_variable *> variadic_decls);
	void add_function(bz::array_view<bz::u8string_view const> id, decl_function &func_decl);
	void add_function_alias(bz::array_view<bz::u8string_view const> id, decl_function_alias &alias_decl);
	void add_operator(decl_operator &op_decl);
	void add_operator_alias(decl_operator_alias &alias_decl);
	void add_type_alias(bz::array_view<bz::u8string_view const> id, decl_type_alias &alias_decl);
	void add_struct(bz::array_view<bz::u8string_view const> id, decl_struct &struct_decl);
	void add_enum(bz::array_view<bz::u8string_view const> id, decl_enum &enum_decl);

	enclosing_scope_t parent = {};
};

struct local_symbol_t : bz::variant<
	decl_variable *,
	variadic_var_decl,
	decl_function *,
	decl_function_alias *,
	decl_type_alias *,
	decl_struct *,
	decl_enum *
>
{
	identifier const &get_id(void) const;
	lex::src_tokens const &get_src_tokens(void) const;

	bool is_variable(void) const noexcept
	{ return this->is<decl_variable *>(); }

	bool is_variadic_variable(void) const noexcept
	{ return this->is<variadic_var_decl>(); }

	bool is_function(void) const noexcept
	{ return this->is<decl_function *>(); }

	bool is_function_alias(void) const noexcept
	{ return this->is<decl_function_alias *>(); }

	bool is_type_alias(void) const noexcept
	{ return this->is<decl_type_alias *>(); }

	bool is_struct(void) const noexcept
	{ return this->is<decl_struct *>(); }

	bool is_enum(void) const noexcept
	{ return this->is<decl_enum *>(); }

	decl_variable *get_variable(void) const noexcept
	{ bz_assert(this->is_variable()); return this->get<decl_variable *>(); }

	variadic_var_decl const &get_variadic_variable(void) const noexcept
	{ bz_assert(this->is_variadic_variable()); return this->get<variadic_var_decl>(); }

	decl_function *get_function(void) const noexcept
	{ bz_assert(this->is_function()); return this->get<decl_function *>(); }

	decl_function_alias *get_function_alias(void) const noexcept
	{ bz_assert(this->is_function_alias()); return this->get<decl_function_alias *>(); }

	decl_type_alias *get_type_alias(void) const noexcept
	{ bz_assert(this->is_type_alias()); return this->get<decl_type_alias *>(); }

	decl_struct *get_struct(void) const noexcept
	{ bz_assert(this->is_struct()); return this->get<decl_struct *>(); }

	decl_enum *get_enum(void) const noexcept
	{ bz_assert(this->is_enum()); return this->get<decl_enum *>(); }
};

struct local_scope_t
{
	arena_vector<local_symbol_t> symbols{};
	enclosing_scope_t parent = {};
	bool is_loop_scope = false;

	local_symbol_t *find_by_id(identifier const &id, size_t bound) noexcept;

	void add_variable(decl_variable &var_decl);
	void add_variable(decl_variable &original_decl, arena_vector<decl_variable *> variadic_decls);
	void add_function(decl_function &func_decl);
	void add_type_alias(decl_type_alias &alias_decl);

	auto var_decl_range(void) const noexcept
	{
		return this->symbols
			.filter([](auto const &symbol) { return symbol.is_variable(); })
			.transform([](auto const &symbol) { return symbol.get_variable(); });
	}
};

struct scope_t : bz::variant<global_scope_t, local_scope_t>
{
	bool is_global(void) const noexcept
	{ return this->is<global_scope_t>(); }

	bool is_local(void) const noexcept
	{ return this->is<local_scope_t>(); }

	global_scope_t &get_global(void) noexcept
	{ bz_assert(this->is_global()); return this->get<global_scope_t>(); }

	global_scope_t const &get_global(void) const noexcept
	{ bz_assert(this->is_global()); return this->get<global_scope_t>(); }

	local_scope_t &get_local(void) noexcept
	{ bz_assert(this->is_local()); return this->get<local_scope_t>(); }

	local_scope_t const &get_local(void) const noexcept
	{ bz_assert(this->is_local()); return this->get<local_scope_t>(); }
};

inline scope_t make_global_scope(enclosing_scope_t enclosing_scope)
{
	scope_t result;
	auto &global_scope = result.emplace<global_scope_t>();
	global_scope.parent = enclosing_scope;
	return result;
}

inline scope_t make_local_scope(enclosing_scope_t enclosing_scope, bool is_loop_scope)
{
	scope_t result;
	auto &local_scope = result.emplace<local_scope_t>();
	local_scope.parent = enclosing_scope;
	local_scope.is_loop_scope = is_loop_scope;
	return result;
}

void add_global_variable(ast::global_scope_t &scope, ast::decl_variable &var_decl);

} // namespace ast

#endif // AST_SCOPE_H
