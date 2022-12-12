#ifndef AST_CONSTANT_VALUE_H
#define AST_CONSTANT_VALUE_H

#include "core.h"
#include "typespec.h"
#include "allocator.h"
#include "statement_forward.h"

namespace ast
{

struct statement_view;

struct constant_value;
namespace internal
{

struct null_t {};
struct void_t {};

struct enum_t
{
	decl_enum *decl;
	uint64_t value;
};

} // namespace internal

using constant_value_base_t = bz::variant<
	int64_t, uint64_t,
	float32_t, float64_t,
	bz::u8char, bz::u8string,
	bool, internal::null_t,
	internal::void_t,
	internal::enum_t,

	// arrays
	arena_vector<constant_value>,
	arena_vector<int64_t>,
	arena_vector<uint64_t>,
	arena_vector<float32_t>,
	arena_vector<float64_t>,
	// tuples
	arena_vector<constant_value>,

	function_body *,

	typespec,

	// structs
	arena_vector<constant_value>
>;

struct constant_value : constant_value_base_t
{
	using base_t = constant_value_base_t;

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
		enum_           = base_t::index_of<internal::enum_t>,
		array,
		sint_array      = base_t::index_of<arena_vector<int64_t>>,
		uint_array      = base_t::index_of<arena_vector<uint64_t>>,
		float32_array   = base_t::index_of<arena_vector<float32_t>>,
		float64_array   = base_t::index_of<arena_vector<float64_t>>,
		tuple,
		function        = base_t::index_of<function_body *>,
		type            = base_t::index_of<typespec>,
		aggregate,
	};

	static_assert(bz::meta::is_same<base_t::value_type<array>, arena_vector<constant_value>>);
	static_assert(bz::meta::is_same<base_t::value_type<tuple>, arena_vector<constant_value>>);
	static_assert(bz::meta::is_same<base_t::value_type<aggregate>, arena_vector<constant_value>>);
	static_assert(array != tuple && array != aggregate && tuple != aggregate);

	using base_t::operator =;

	size_t kind(void) const noexcept
	{ return this->base_t::index(); }


	bool is_sint(void) const noexcept
	{ return this->is<sint>(); }

	bool is_uint(void) const noexcept
	{ return this->is<uint>(); }

	bool is_float32(void) const noexcept
	{ return this->is<float32>(); }

	bool is_float64(void) const noexcept
	{ return this->is<float64>(); }

	bool is_u8char(void) const noexcept
	{ return this->is<u8char>(); }

	bool is_string(void) const noexcept
	{ return this->is<string>(); }

	bool is_boolean(void) const noexcept
	{ return this->is<boolean>(); }

	bool is_null_constant(void) const noexcept
	{ return this->is<null>(); }

	bool is_void(void) const noexcept
	{ return this->is<void_>(); }

	bool is_enum(void) const noexcept
	{ return this->is<enum_>(); }

	bool is_array(void) const noexcept
	{ return this->is<array>(); }

	bool is_sint_array(void) const noexcept
	{ return this->is<sint_array>(); }

	bool is_uint_array(void) const noexcept
	{ return this->is<uint_array>(); }

	bool is_float32_array(void) const noexcept
	{ return this->is<float32_array>(); }

	bool is_float64_array(void) const noexcept
	{ return this->is<float64_array>(); }

	bool is_tuple(void) const noexcept
	{ return this->is<tuple>(); }

	bool is_function(void) const noexcept
	{ return this->is<function>(); }

	bool is_type(void) const noexcept
	{ return this->is<type>(); }

	bool is_aggregate(void) const noexcept
	{ return this->is<aggregate>(); }


	int64_t get_sint(void) const noexcept
	{ return this->get<sint>(); }

	uint64_t get_uint(void) const noexcept
	{ return this->get<uint>(); }

	float32_t get_float32(void) const noexcept
	{ return this->get<float32>(); }

	float64_t get_float64(void) const noexcept
	{ return this->get<float64>(); }

	bz::u8char get_u8char(void) const noexcept
	{ return this->get<u8char>(); }

	bz::u8string_view get_string(void) const noexcept
	{ return this->get<string>(); }

	bool get_boolean(void) const noexcept
	{ return this->get<boolean>(); }

	internal::enum_t get_enum(void) const noexcept
	{ return this->get<enum_>(); }

	bz::array_view<constant_value const> get_array(void) const noexcept
	{ return this->get<array>(); }

	bz::array_view<int64_t const> get_sint_array(void) const noexcept
	{ return this->get<sint_array>(); }

	bz::array_view<uint64_t const> get_uint_array(void) const noexcept
	{ return this->get<uint_array>(); }

	bz::array_view<float32_t const> get_float32_array(void) const noexcept
	{ return this->get<float32_array>(); }

	bz::array_view<float64_t const> get_float64_array(void) const noexcept
	{ return this->get<float64_array>(); }

	bz::array_view<constant_value const> get_tuple(void) const noexcept
	{ return this->get<tuple>(); }

	function_body *get_function(void) const noexcept
	{ return this->get<function>(); }

	typespec_view get_type(void) const noexcept
	{ return this->get<type>(); }

	bz::array_view<constant_value const> get_aggregate(void) const noexcept
	{ return this->get<aggregate>(); }


	constant_value(void) = default;

	template<typename T, bz::meta::enable_if<!bz::meta::is_same<bz::meta::remove_cv_reference<T>, constant_value>, int> = 0>
	explicit constant_value(T &&t)
		: base_t(std::forward<T>(t))
	{}

	static constant_value get_void(void)
	{ return constant_value(internal::void_t{}); }

	static constant_value get_null(void)
	{ return constant_value(internal::null_t{}); }

	static constant_value get_enum(decl_enum *decl, int64_t value)
	{ return constant_value(internal::enum_t{ decl, bit_cast<uint64_t>(value) }); }

	static constant_value get_enum(decl_enum *decl, uint64_t value)
	{ return constant_value(internal::enum_t{ decl, value }); }

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
