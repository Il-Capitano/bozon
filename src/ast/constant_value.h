#ifndef AST_CONSTANT_VALUE_H
#define AST_CONSTANT_VALUE_H

#include "core.h"
#include "typespec.h"

namespace ctx
{

struct function_overload_set;

} // namespace ctx

namespace ast
{

struct function_body;

struct constant_value;
namespace internal { struct null_t {}; }

struct constant_value : bz::variant<
	int64_t, uint64_t,
	float32_t, float64_t,
	bz::u8char, bz::u8string,
	bool, internal::null_t,

	function_body *,
	bz::u8string_view,

	bz::vector<constant_value>
>
{
	using base_t = bz::variant<
		int64_t, uint64_t,
		float32_t, float64_t,
		bz::u8char, bz::u8string,
		bool, internal::null_t,

		function_body *,
		bz::u8string_view,

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
		function        = base_t::index_of<function_body *>,
		function_set_id = base_t::index_of<bz::u8string_view>,
		aggregate       = base_t::index_of<bz::vector<constant_value>>,
	};

	using base_t::variant;
	using base_t::operator =;

	size_t kind(void) const noexcept
	{ return this->base_t::index(); }

	~constant_value(void) noexcept = default;
	constant_value(constant_value const &)     = default;
	constant_value(constant_value &&) noexcept = default;
	constant_value &operator = (constant_value const &)     = default;
	constant_value &operator = (constant_value &&) noexcept = default;
};

} // namespace ast

#endif // AST_CONSTANT_VALUE_H
