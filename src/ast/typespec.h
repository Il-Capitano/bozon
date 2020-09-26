#ifndef AST_TYPESPEC_H
#define AST_TYPESPEC_H

#include "core.h"
#include "lex/token.h"

namespace ast
{

struct ts_unresolved;

struct ts_base_type;
struct ts_void;
struct ts_function;
struct ts_array;
struct ts_tuple;
struct ts_auto;
struct ts_typename;

struct ts_const;
struct ts_consteval;
struct ts_pointer;
struct ts_lvalue_reference;

using typespec_types = bz::meta::type_pack<
	ts_unresolved,
	ts_base_type,
	ts_void,
	ts_function,
	ts_array,
	ts_tuple,
	ts_auto,
	ts_typename,
	ts_const,
	ts_consteval,
	ts_pointer,
	ts_lvalue_reference
>;

using terminator_typespec_types = bz::meta::type_pack<
	ts_unresolved,
	ts_base_type,
	ts_void,
	ts_function,
	ts_array,
	ts_tuple,
	ts_auto,
	ts_typename
>;

template<typename T>
constexpr bool is_terminator_typespec = bz::meta::is_in_type_pack<T, terminator_typespec_types>;

using typespec_node_t = bz::meta::apply_type_pack<bz::variant, typespec_types>;

struct typespec_view
{
	bz::array_view<const typespec_node_t> nodes;

	lex::src_tokens get_src_tokens(void) const noexcept;

	uint64_t kind(void) const noexcept;

	bool is_empty(void) const noexcept
	{ return this->nodes.empty(); }

	bool has_value(void) const noexcept
	{ return !this->nodes.empty(); }

	template<typename T>
	bool is(void) const noexcept;

	template<typename T>
	auto get(void) const noexcept -> bz::meta::conditional<is_terminator_typespec<T>, T const &, typespec_view>;

	typespec_view blind_get(void) const noexcept;

	template<typename Fn>
	decltype(auto) visit(Fn &&fn) const;


	bz::u8string get_symbol_name(void) const;
};

struct typespec
{
	bz::vector<typespec_node_t> nodes;

	declare_default_5(typespec)

	typespec(bz::vector<typespec_node_t> _nodes);
	typespec(typespec_view ts);

	template<typename T, typename ...Args>
	void add_layer(Args &&...args);
	void remove_layer(void);


	operator typespec_view (void) const noexcept
	{ return typespec_view{ this->nodes }; }

	typespec_view as_typespec_view(void) const noexcept
	{ return typespec_view{ this->nodes }; }

	lex::src_tokens get_src_tokens() const noexcept
	{ return this->as_typespec_view().get_src_tokens(); }

	bool is_unresolved(void) const noexcept;

	bool is_empty(void) const noexcept
	{ return this->nodes.empty(); }

	bool has_value(void) const noexcept
	{ return !this->nodes.empty(); }

	template<typename T>
	auto is(void) const noexcept
	{ return this->as_typespec_view().is<T>(); }

	template<typename T>
	auto get(void) const noexcept
	{ return this->as_typespec_view().get<T>(); }

	uint64_t kind(void) const noexcept
	{ return this->as_typespec_view().kind(); }

	template<typename Fn>
	decltype(auto) visit(Fn &&fn) const
	{ return this->as_typespec_view().visit(std::forward<Fn>(fn)); }

	void clear(void) noexcept;

	void copy_from(typespec_view pos, typespec_view source);
	void move_from(typespec_view pos, typespec_view source);


	bz::u8string get_symbol_name(void) const
	{ return this->as_typespec_view().get_symbol_name(); }

	static bz::u8string decode_symbol_name(
		bz::u8string_view::const_iterator &it,
		bz::u8string_view::const_iterator end
	);
};


struct type_info;

struct ts_unresolved
{
	lex::token_range tokens;
};

struct ts_base_type
{
	lex::src_tokens src_tokens;
	type_info *info;
};

struct ts_void
{
	lex::token_pos void_pos;
};

struct ts_function
{
	lex::src_tokens src_tokens;
	bz::vector<typespec> param_types;
	typespec return_type;
};

struct ts_array
{
	lex::src_tokens      src_tokens;
	bz::vector<uint64_t> sizes;
	typespec             elem_type;
};

struct ts_tuple
{
	lex::src_tokens src_tokens;
	bz::vector<typespec> types;
};

struct ts_auto
{
	lex::token_pos auto_pos;
};

struct ts_typename
{
	lex::token_pos typename_pos;
};

struct ts_const
{
	lex::token_pos const_pos;
};

struct ts_consteval
{
	lex::token_pos consteval_pos;
};

struct ts_pointer
{
	lex::token_pos pointer_pos;
};

struct ts_lvalue_reference
{
	lex::token_pos reference_pos;
};


typespec_view remove_lvalue_reference(typespec_view ts) noexcept;
typespec_view remove_const_or_consteval(typespec_view ts) noexcept;

bool is_complete(typespec_view ts) noexcept;
bool is_instantiable(typespec_view ts) noexcept;

bool operator == (typespec_view lhs, typespec_view rhs);
inline bool operator != (typespec_view lhs, typespec_view rhs)
{ return !(lhs == rhs); }


inline typespec make_auto_typespec(lex::token_pos auto_pos)
{
	return typespec{ { ts_auto{ auto_pos } } };
}

inline typespec make_typename_typespec(lex::token_pos typename_pos)
{
	return typespec{ { ts_typename{ typename_pos } } };
}

inline typespec make_unresolved_typespec(lex::token_range range)
{
	return typespec{ { ts_unresolved{ range } } };
}

inline typespec make_base_type_typespec(lex::src_tokens src_tokens, type_info *info)
{
	return typespec{ { ts_base_type{ src_tokens, info } } };
}

inline typespec make_void_typespec(lex::token_pos void_pos)
{
	return typespec{ { ts_void{ void_pos } } };
}

inline typespec make_array_typespec(
	lex::src_tokens src_tokens,
	bz::vector<uint64_t> sizes,
	typespec elem_type
)
{
	return typespec{ { ts_array{ src_tokens, std::move(sizes), std::move(elem_type) } } };
}

inline typespec make_tuple_typespec(
	lex::src_tokens src_tokens,
	bz::vector<typespec> types
)
{
	return typespec{ { ts_tuple{ src_tokens, std::move(types) } } };
}


template<typename T>
bool typespec_view::is(void) const noexcept
{
	return this->nodes.size() != 0 && this->nodes.front().index() == typespec_node_t::index_of<T>;
}

template<typename T>
auto typespec_view::get(void) const noexcept -> bz::meta::conditional<is_terminator_typespec<T>, T const &, typespec_view>
{
	bz_assert(this->is<T>());
	if constexpr (is_terminator_typespec<T>)
	{
		return this->nodes.front().get<T>();
	}
	else
	{
		return typespec_view{ { this->nodes.begin() + 1, this->nodes.end() } };
	}
}

template<typename Fn>
decltype(auto) typespec_view::visit(Fn &&fn) const
{
	bz_assert(this->nodes.size() != 0);
	return this->nodes.front().visit(std::forward<Fn>(fn));
}

template<typename T, typename ...Args>
void typespec::add_layer(Args &&...args)
{
	static_assert(!is_terminator_typespec<T>);
	this->nodes.emplace_front(T{ std::forward<Args>(args)... });
}

inline void typespec::remove_layer(void)
{
	this->nodes.pop_front();
}

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
