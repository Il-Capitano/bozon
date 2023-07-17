#ifndef AST_TYPESPEC_H
#define AST_TYPESPEC_H

#include "core.h"
#include "lex/token.h"
#include "allocator.h"
#include "statement_forward.h"
#include "type_prototype.h"
#include "abi/calling_conventions.h"

namespace ast
{

struct ts_unresolved;

struct ts_base_type;
struct ts_enum;
struct ts_void;
struct ts_function;
struct ts_array;
struct ts_array_slice;
struct ts_tuple;
struct ts_auto;
struct ts_typename;

struct ts_mut;
struct ts_consteval;
struct ts_pointer;
struct ts_optional;
struct ts_lvalue_reference;
struct ts_move_reference;
struct ts_auto_reference;
struct ts_auto_reference_mut;
struct ts_variadic;

using typespec_types = bz::meta::type_pack<
	ts_unresolved,
	ts_base_type,
	ts_enum,
	ts_void,
	ts_function,
	ts_array,
	ts_array_slice,
	ts_tuple,
	ts_auto,
	ts_typename,
	ts_mut,
	ts_consteval,
	ts_pointer,
	ts_optional,
	ts_lvalue_reference,
	ts_move_reference,
	ts_auto_reference,
	ts_auto_reference_mut,
	ts_variadic
>;

using modifier_typespec_types = bz::meta::type_pack<
	ts_mut,
	ts_consteval,
	ts_pointer,
	ts_optional,
	ts_lvalue_reference,
	ts_move_reference,
	ts_auto_reference,
	ts_auto_reference_mut,
	ts_variadic
>;

using terminator_typespec_types = bz::meta::type_pack<
	ts_unresolved,
	ts_base_type,
	ts_enum,
	ts_void,
	ts_function,
	ts_array,
	ts_array_slice,
	ts_tuple,
	ts_auto,
	ts_typename
>;

static_assert(typespec_types::size() == modifier_typespec_types::size() + terminator_typespec_types::size());

template<typename T>
constexpr bool is_terminator_typespec = bz::meta::is_in_type_pack<T, terminator_typespec_types>;

using modifier_typespec_node_t = bz::meta::apply_type_pack<bz::variant, modifier_typespec_types>;
using terminator_typespec_node_t = bz::meta::apply_type_pack<bz::variant, terminator_typespec_types>;

struct typespec_view
{
	lex::src_tokens src_tokens = {};
	bz::array_view<const modifier_typespec_node_t> modifiers = {};
	terminator_typespec_node_t const *terminator = nullptr;

	uint64_t modifier_kind(void) const noexcept;
	uint64_t terminator_kind(void) const noexcept;
	bool same_kind_as(typespec_view const &other) const noexcept;

	bool is_empty(void) const noexcept
	{ return this->terminator == nullptr; }

	bool not_empty(void) const noexcept
	{ return this->terminator != nullptr; }

	template<typename T>
	bool is(void) const noexcept;

	template<typename T>
	auto get(void) const noexcept -> bz::meta::conditional<is_terminator_typespec<T>, T const &, typespec_view>;

	bool is_safe_blind_get(void) const noexcept;
	typespec_view blind_get(void) const noexcept;
	bool is_typename(void) const noexcept;
	bool is_optional_pointer_like(void) const noexcept;
	bool is_optional_pointer(void) const noexcept;
	typespec_view get_optional_pointer(void) const noexcept;
	bool is_optional_reference(void) const noexcept;
	typespec_view get_optional_reference(void) const noexcept;
	bool is_optional_function(void) const noexcept;
	ts_function const &get_optional_function(void) const noexcept;

	bool is_reference(void) const noexcept;
	typespec_view get_reference(void) const noexcept;
	bool is_mut_reference(void) const noexcept;
	typespec_view get_mut_reference(void) const noexcept;

	typespec_view remove_reference(void) const noexcept;
	typespec_view remove_mut_reference(void) const noexcept;

	bool is_any_reference(void) const noexcept;
	typespec_view get_any_reference(void) const noexcept;

	template<typename Fn>
	decltype(auto) visit(Fn &&fn) const;


	bz::u8string get_symbol_name(void) const;
};

struct typespec
{
	lex::src_tokens src_tokens;
	arena_vector<modifier_typespec_node_t> modifiers;
	ast_unique_ptr<terminator_typespec_node_t> terminator;

	typespec(void) = default;
	typespec(
		lex::src_tokens const &_src_tokens,
		arena_vector<modifier_typespec_node_t> _modifiers,
		ast_unique_ptr<terminator_typespec_node_t> _terminator
	);
	typespec(typespec_view ts);

	typespec(typespec const &other);
	typespec(typespec &&other) = default;
	typespec &operator = (typespec const &other);
	typespec &operator = (typespec &&other) = default;

	template<typename T, typename ...Args>
	void add_layer(Args &&...args);
	void remove_layer(void);


	operator typespec_view (void) const noexcept
	{ return typespec_view{ this->src_tokens, this->modifiers, this->terminator.get() }; }

	typespec_view as_typespec_view(void) const noexcept
	{ return typespec_view{ this->src_tokens, this->modifiers, this->terminator.get() }; }

	bool is_unresolved(void) const noexcept;
	bool is_empty(void) const noexcept;
	bool not_empty(void) const noexcept;

	template<typename T>
	bool is(void) const noexcept
	{ return this->as_typespec_view().is<T>(); }

	template<typename T>
	decltype(auto) get(void) const noexcept
	{ return this->as_typespec_view().get<T>(); }

	uint64_t modifier_kind(void) const noexcept
	{ return this->as_typespec_view().modifier_kind(); }

	uint64_t terminator_kind(void) const noexcept
	{ return this->as_typespec_view().terminator_kind(); }

	bool is_typename(void) const noexcept
	{ return this->as_typespec_view().is_typename(); }

	bool is_optional_pointer_like(void) const noexcept
	{ return this->as_typespec_view().is_optional_pointer_like(); }

	bool is_optional_pointer(void) const noexcept
	{ return this->as_typespec_view().is_optional_pointer(); }

	typespec_view get_optional_pointer(void) const noexcept
	{ return this->as_typespec_view().get_optional_pointer(); }

	bool is_optional_reference(void) const noexcept
	{ return this->as_typespec_view().is_optional_reference(); }

	typespec_view get_optional_reference(void) const noexcept
	{ return this->as_typespec_view().get_optional_reference(); }

	bool is_optional_function(void) const noexcept
	{ return this->as_typespec_view().is_optional_function(); }

	ts_function &get_optional_function(void) noexcept;

	ts_function const &get_optional_function(void) const noexcept
	{ return this->as_typespec_view().get_optional_function(); }

	bool is_reference(void) const noexcept
	{ return this->as_typespec_view().is_reference(); }

	typespec_view get_reference(void) const noexcept
	{ return this->as_typespec_view().get_reference(); }

	bool is_mut_reference(void) const noexcept
	{ return this->as_typespec_view().is_mut_reference(); }

	typespec_view get_mut_reference(void) const noexcept
	{ return this->as_typespec_view().get_mut_reference(); }

	typespec_view remove_reference(void) const noexcept
	{ return this->as_typespec_view().remove_reference(); }

	typespec_view remove_mut_reference(void) const noexcept
	{ return this->as_typespec_view().remove_mut_reference(); }

	bool is_any_reference(void) const noexcept
	{ return this->as_typespec_view().is_any_reference(); }

	typespec_view get_any_reference(void) const noexcept
	{ return this->as_typespec_view().get_any_reference(); }

	template<typename Fn>
	decltype(auto) visit(Fn &&fn) const
	{ return this->as_typespec_view().visit(std::forward<Fn>(fn)); }

	void clear(void) noexcept;

	void copy_from(typespec_view pos, typespec_view source);
	void move_from(typespec_view pos, typespec &source);
	void move_from(typespec_view pos, typespec &&source);


	bz::u8string get_symbol_name(void) const
	{ return this->as_typespec_view().get_symbol_name(); }

	static bz::u8string decode_symbol_name(
		bz::u8string_view::const_iterator &it,
		bz::u8string_view::const_iterator end
	);

	static bz::u8string decode_symbol_name(bz::u8string_view symbol)
	{
		auto it = symbol.begin();
		auto const end = symbol.end();
		return decode_symbol_name(it, end);
	}
};


struct ts_unresolved
{
	lex::token_range tokens;
};

struct ts_base_type
{
	type_info *info;
};

struct ts_enum
{
	decl_enum *decl;
};

struct ts_void
{};

struct ts_function
{
	arena_vector<typespec> param_types;
	typespec return_type;
	abi::calling_convention cc;
};

struct ts_array
{
	uint64_t size; // 0 if it's a placeholder
	typespec elem_type;
};

struct ts_array_slice
{
	typespec elem_type;
};

struct ts_tuple
{
	arena_vector<typespec> types;
};

struct ts_auto
{};

struct ts_typename
{};

struct ts_mut
{};

struct ts_consteval
{};

struct ts_pointer
{};

struct ts_optional
{};

struct ts_lvalue_reference
{};

struct ts_move_reference
{};

struct ts_auto_reference
{};

struct ts_auto_reference_mut
{};

struct ts_variadic
{};


struct typespec_hash
{
	size_t operator () (typespec_view ts) const noexcept;
};


typespec_view remove_lvalue_reference(typespec_view ts) noexcept;
typespec_view remove_pointer(typespec_view ts) noexcept;
typespec_view remove_optional(typespec_view ts) noexcept;
typespec_view remove_consteval(typespec_view ts) noexcept;
typespec_view remove_mut(typespec_view ts) noexcept;
typespec_view remove_mutability_modifiers(typespec_view ts) noexcept;
typespec_view remove_lvalue_or_move_reference(typespec_view ts) noexcept;
typespec_view remove_any_reference(typespec_view ts) noexcept;

bool is_complete(typespec_view ts) noexcept;

bool is_trivially_relocatable(typespec_view ts) noexcept;

bool operator == (typespec_view lhs, typespec_view rhs);
inline bool operator != (typespec_view lhs, typespec_view rhs)
{ return !(lhs == rhs); }


inline typespec make_auto_typespec(lex::token_pos auto_pos)
{
	return typespec{ lex::src_tokens::from_single_token(auto_pos), {}, make_ast_unique<terminator_typespec_node_t>(ts_auto()) };
}

inline typespec make_typename_typespec(lex::token_pos typename_pos)
{
	return typespec{ lex::src_tokens::from_single_token(typename_pos), {}, make_ast_unique<terminator_typespec_node_t>(ts_typename()) };
}

inline typespec make_unresolved_typespec(lex::token_range range)
{
	return typespec{ lex::src_tokens::from_range(range), {}, make_ast_unique<terminator_typespec_node_t>(ts_unresolved{ range }) };
}

inline typespec make_base_type_typespec(lex::src_tokens const &src_tokens, type_info *info)
{
	return typespec{ src_tokens, {}, make_ast_unique<terminator_typespec_node_t>(ts_base_type{ info }) };
}

inline typespec make_enum_typespec(lex::src_tokens const &src_tokens, decl_enum *decl)
{
	return typespec{ src_tokens, {}, make_ast_unique<terminator_typespec_node_t>(ts_enum{ decl }) };
}

inline typespec make_void_typespec(lex::token_pos void_pos)
{
	return typespec{ lex::src_tokens::from_single_token(void_pos), {}, make_ast_unique<terminator_typespec_node_t>(ts_void()) };
}

inline typespec make_array_typespec(
	lex::src_tokens const &src_tokens,
	uint64_t size,
	typespec elem_type
)
{
	return typespec{ src_tokens, {}, make_ast_unique<terminator_typespec_node_t>(ts_array{ size, std::move(elem_type) }) };
}

inline typespec make_array_slice_typespec(
	lex::src_tokens const &src_tokens,
	typespec elem_type
)
{
	return typespec{ src_tokens, {}, make_ast_unique<terminator_typespec_node_t>(ts_array_slice{ std::move(elem_type) }) };
}

inline typespec make_tuple_typespec(
	lex::src_tokens const &src_tokens,
	arena_vector<typespec> types
)
{
	return typespec{ src_tokens, {}, make_ast_unique<terminator_typespec_node_t>(ts_tuple{ std::move(types) }) };
}

inline typespec make_function_typespec(
	lex::src_tokens const &src_tokens,
	arena_vector<typespec> param_types,
	typespec return_type,
	abi::calling_convention cc
)
{
	return typespec{
		src_tokens,
		{},
		make_ast_unique<terminator_typespec_node_t>(ts_function{ std::move(param_types), std::move(return_type), cc })
	};
}


template<typename T>
bool typespec_view::is(void) const noexcept
{
	if constexpr (is_terminator_typespec<T>)
	{
		return this->modifiers.empty() && this->terminator != nullptr && this->terminator->is<T>();
	}
	else
	{
		return this->modifiers.not_empty() && this->modifiers[0].is<T>();
	}
}

template<typename T>
auto typespec_view::get(void) const noexcept -> bz::meta::conditional<is_terminator_typespec<T>, T const &, typespec_view>
{
	bz_assert(this->is<T>());
	if constexpr (is_terminator_typespec<T>)
	{
		return this->terminator->get<T>();
	}
	else
	{
		return typespec_view{ this->src_tokens, { this->modifiers.begin() + 1, this->modifiers.end() }, this->terminator };
	}
}

template<typename Fn>
decltype(auto) typespec_view::visit(Fn &&fn) const
{
	bz_assert(!this->is_empty());
	if (this->modifiers.not_empty())
	{
		return this->modifiers[0].visit(std::forward<Fn>(fn));
	}
	else
	{
		return this->terminator->visit(std::forward<Fn>(fn));
	}
}

template<typename T, typename ...Args>
void typespec::add_layer(Args &&...args)
{
	static_assert(!is_terminator_typespec<T>);
	this->modifiers.emplace_front(T{ std::forward<Args>(args)... });
}

inline void typespec::remove_layer(void)
{
	bz_assert(this->modifiers.not_empty());
	this->modifiers.pop_front();
}

type_prototype const *get_type_prototype(ast::typespec_view type, type_prototype_set_t &type_prototype_set);

} // namespace ast

template<>
struct bz::formatter<ast::typespec_view>
{
	static bz::u8string format(ast::typespec_view typespec, bz::u8string_view);
};

template<>
struct bz::formatter<ast::typespec>
{
	static bz::u8string format(ast::typespec const &typespec, bz::u8string_view);
};

#endif // AST_TYPESPEC_H
