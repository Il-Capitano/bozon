#ifndef AST_CONSTANT_VALUE_H
#define AST_CONSTANT_VALUE_H

#include "core.h"
#include "typespec.h"
#include "allocator.h"

namespace ast
{

struct function_body;

struct constant_value;
namespace internal
{

struct null_t {};
struct void_t {};

} // namespace internal

struct constant_value : bz::variant<
	int64_t, uint64_t,
	float32_t, float64_t,
	bz::u8char, bz::u8string,
	bool, internal::null_t,
	internal::void_t,

	// arrays
	bz::vector<constant_value>,
	// tuples
	bz::vector<constant_value>,

	function_body *,
	arena_vector<bz::u8string_view>,
	arena_vector<bz::u8string_view>,

	ast::typespec,

	// structs
	bz::vector<constant_value>
>
{
	using base_t = bz::variant<
		int64_t, uint64_t,
		float32_t, float64_t,
		bz::u8char, bz::u8string,
		bool, internal::null_t,
		internal::void_t,

		// arrays
		bz::vector<constant_value>,
		// tuples
		bz::vector<constant_value>,

		function_body *,
		arena_vector<bz::u8string_view>,
		arena_vector<bz::u8string_view>,

		ast::typespec,

		// structs
		bz::vector<constant_value>
	>;

	enum : size_t
	{
		sint            = base_t::index_of<int64_t>,
		uint            = base_t::index_of<uint64_t>,
		float32         = base_t::index_of<float32_t>,
		float64         = base_t::index_of<float64_t>,
		u8char          = base_t::index_of<bz::u8char>,
		string          = base_t::index_of<bz::u8string>,
		boolean         = base_t::index_of<bool>,
		null            = base_t::index_of<internal::null_t>,
		void_           = base_t::index_of<internal::void_t>,
		array,
		tuple,
		function        = base_t::index_of<function_body *>,
		unqualified_function_set_id,
		qualified_function_set_id,
		type            = base_t::index_of<ast::typespec>,
		aggregate,
	};

	static_assert(bz::meta::is_same<base_t::value_type<array>, bz::vector<constant_value>>);
	static_assert(bz::meta::is_same<base_t::value_type<tuple>, bz::vector<constant_value>>);
	static_assert(bz::meta::is_same<base_t::value_type<aggregate>, bz::vector<constant_value>>);
	static_assert(array != tuple && array != aggregate && tuple != aggregate);
	static_assert(bz::meta::is_same<base_t::value_type<unqualified_function_set_id>, arena_vector<bz::u8string_view>>);
	static_assert(bz::meta::is_same<base_t::value_type<qualified_function_set_id>, arena_vector<bz::u8string_view>>);
	static_assert(unqualified_function_set_id != qualified_function_set_id);

	using base_t::operator =;

	size_t kind(void) const noexcept
	{ return this->base_t::index(); }

	declare_default_5(constant_value)

	template<typename T, bz::meta::enable_if<!bz::meta::is_same<bz::meta::remove_cv_reference<T>, constant_value>, int> = 0>
	explicit constant_value(T &&t)
		: base_t(std::forward<T>(t))
	{}

	static constant_value get_void(void)
	{ return constant_value(internal::void_t{}); }

	static void encode_for_symbol_name(bz::u8string &out, constant_value const &value);
	static bz::u8string decode_from_symbol_name(bz::u8string_view::const_iterator &it, bz::u8string_view::const_iterator end);
	static bz::u8string decode_from_symbol_name(bz::u8string_view str)
	{
		auto it = str.begin();
		auto const end = str.end();
		return decode_from_symbol_name(it, end);
	}
};

bz::u8string get_value_string(constant_value const &value);

bool operator == (constant_value const &lhs, constant_value const &rhs) noexcept;
bool operator != (constant_value const &lhs, constant_value const &rhs) noexcept;

} // namespace ast

#endif // AST_CONSTANT_VALUE_H
