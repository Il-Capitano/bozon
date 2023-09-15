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
	bz::u8char, bz::u8string_view,
	bool, internal::null_t,
	internal::void_t,
	internal::enum_t,

	// arrays
	bz::array_view<constant_value const>,
	bz::array_view<int64_t const>,
	bz::array_view<uint64_t const>,
	bz::array_view<float32_t const>,
	bz::array_view<float64_t const>,
	// tuples
	bz::array_view<constant_value const>,

	function_body *,

	typespec_view,

	// structs
	bz::array_view<constant_value const>
>;

enum class constant_value_kind : uint64_t
{
	sint = constant_value_base_t::index_of<int64_t>,
	uint,
	float32,
	float64,
	u8char,
	string,
	boolean,
	null,
	void_,
	enum_,
	array,
	sint_array,
	uint_array,
	float32_array,
	float64_array,
	tuple,
	function,
	type,
	aggregate,
};

struct constant_value : constant_value_base_t
{
	using base_t = constant_value_base_t;
	using base_t::operator =;

	template<constant_value_kind kind>
	decltype(auto) get(void)
	{
		return this->base_t::get<static_cast<uint64_t>(kind)>();
	}

	template<constant_value_kind kind>
	decltype(auto) get(void) const
	{
		return this->base_t::get<static_cast<uint64_t>(kind)>();
	}

	template<constant_value_kind kind>
	bool is(void) const
	{
		return this->base_t::is<static_cast<uint64_t>(kind)>();
	}

	template<constant_value_kind kind, typename ...Args>
	decltype(auto) emplace(Args &&...args)
	{
		return this->base_t::emplace<static_cast<uint64_t>(kind)>(std::forward<Args>(args)...);
	}

	constant_value_kind kind(void) const noexcept
	{ return static_cast<constant_value_kind>(this->base_t::index()); }


	bool is_sint(void) const noexcept
	{ return this->is<constant_value_kind::sint>(); }

	bool is_uint(void) const noexcept
	{ return this->is<constant_value_kind::uint>(); }

	bool is_float32(void) const noexcept
	{ return this->is<constant_value_kind::float32>(); }

	bool is_float64(void) const noexcept
	{ return this->is<constant_value_kind::float64>(); }

	bool is_u8char(void) const noexcept
	{ return this->is<constant_value_kind::u8char>(); }

	bool is_string(void) const noexcept
	{ return this->is<constant_value_kind::string>(); }

	bool is_boolean(void) const noexcept
	{ return this->is<constant_value_kind::boolean>(); }

	bool is_null_constant(void) const noexcept
	{ return this->is<constant_value_kind::null>(); }

	bool is_void(void) const noexcept
	{ return this->is<constant_value_kind::void_>(); }

	bool is_enum(void) const noexcept
	{ return this->is<constant_value_kind::enum_>(); }

	bool is_array(void) const noexcept
	{ return this->is<constant_value_kind::array>(); }

	bool is_sint_array(void) const noexcept
	{ return this->is<constant_value_kind::sint_array>(); }

	bool is_uint_array(void) const noexcept
	{ return this->is<constant_value_kind::uint_array>(); }

	bool is_float32_array(void) const noexcept
	{ return this->is<constant_value_kind::float32_array>(); }

	bool is_float64_array(void) const noexcept
	{ return this->is<constant_value_kind::float64_array>(); }

	bool is_tuple(void) const noexcept
	{ return this->is<constant_value_kind::tuple>(); }

	bool is_function(void) const noexcept
	{ return this->is<constant_value_kind::function>(); }

	bool is_type(void) const noexcept
	{ return this->is<constant_value_kind::type>(); }

	bool is_aggregate(void) const noexcept
	{ return this->is<constant_value_kind::aggregate>(); }


	int64_t get_sint(void) const noexcept
	{ return this->get<constant_value_kind::sint>(); }

	uint64_t get_uint(void) const noexcept
	{ return this->get<constant_value_kind::uint>(); }

	float32_t get_float32(void) const noexcept
	{ return this->get<constant_value_kind::float32>(); }

	float64_t get_float64(void) const noexcept
	{ return this->get<constant_value_kind::float64>(); }

	bz::u8char get_u8char(void) const noexcept
	{ return this->get<constant_value_kind::u8char>(); }

	bz::u8string_view get_string(void) const noexcept
	{ return this->get<constant_value_kind::string>(); }

	bool get_boolean(void) const noexcept
	{ return this->get<constant_value_kind::boolean>(); }

	internal::enum_t get_enum(void) const noexcept
	{ return this->get<constant_value_kind::enum_>(); }

	bz::array_view<constant_value const> get_array(void) const noexcept
	{ return this->get<constant_value_kind::array>(); }

	bz::array_view<int64_t const> get_sint_array(void) const noexcept
	{ return this->get<constant_value_kind::sint_array>(); }

	bz::array_view<uint64_t const> get_uint_array(void) const noexcept
	{ return this->get<constant_value_kind::uint_array>(); }

	bz::array_view<float32_t const> get_float32_array(void) const noexcept
	{ return this->get<constant_value_kind::float32_array>(); }

	bz::array_view<float64_t const> get_float64_array(void) const noexcept
	{ return this->get<constant_value_kind::float64_array>(); }

	bz::array_view<constant_value const> get_tuple(void) const noexcept
	{ return this->get<constant_value_kind::tuple>(); }

	function_body *get_function(void) const noexcept
	{ return this->get<constant_value_kind::function>(); }

	typespec_view get_type(void) const noexcept
	{ return this->get<constant_value_kind::type>(); }

	bz::array_view<constant_value const> get_aggregate(void) const noexcept
	{ return this->get<constant_value_kind::aggregate>(); }


	constant_value(void) = default;

	template<typename T>
	explicit constant_value(T &&t) requires(
		!bz::meta::is_same<bz::meta::remove_cv_reference<T>, constant_value>
		&& !bz::meta::is_same<bz::meta::remove_cv_reference<T>, bz::u8string>
		&& !bz::meta::is_same<bz::meta::remove_cv_reference<T>, typespec>
		&& !bz::meta::is_same<bz::meta::remove_cv_reference<T>, arena_vector<constant_value>>
		&& !bz::meta::is_same<bz::meta::remove_cv_reference<T>, arena_vector<int64_t>>
		&& !bz::meta::is_same<bz::meta::remove_cv_reference<T>, arena_vector<uint64_t>>
		&& !bz::meta::is_same<bz::meta::remove_cv_reference<T>, arena_vector<float32_t>>
		&& !bz::meta::is_same<bz::meta::remove_cv_reference<T>, arena_vector<float64_t>>
	)
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
