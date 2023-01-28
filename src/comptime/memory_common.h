#ifndef COMPTIME_MEMORY_COMMON_H
#define COMPTIME_MEMORY_COMMON_H

#include "core.h"
#include "types.h"
#include "values.h"

namespace comptime::memory
{

inline constexpr uint8_t max_object_align = 8;

enum class endianness_kind
{
	little,
	big,
};

template<auto segments>
struct memory_segment_info_t
{
	using segment_type = typename decltype(segments)::value_type;
	static constexpr size_t N = segments.size();

	bz::array<ptr_t, N> segment_begins;

	segment_type get_segment(ptr_t address) const
	{
		bz_assert(address >= this->segment_begins[0]);
		auto const it = std::upper_bound(
			this->segment_begins.begin(), this->segment_begins.end(),
			address,
			[](ptr_t p, ptr_t address_begin) {
				return p < address_begin;
			}
		);
		auto const index = (it - 1) - this->segment_begins.begin();
		return segments[index];
	}

	template<segment_type kind>
	ptr_t get_segment_begin(void) const
	{
		constexpr auto index = []() {
			auto const it = std::find_if(
				segments.begin(), segments.end(),
				[](segment_type segment) {
					return kind == segment;
				}
			);
			return it - segments.begin();
		}();
		static_assert(index >= 0 && index < N);
		return this->segment_begins[index];
	}
};

enum class memory_segment
{
	global,
	stack,
	heap,
	meta,
};

using global_segment_info_t = memory_segment_info_t<bz::array{
	memory_segment::global,
	memory_segment::stack,
	memory_segment::heap,
	memory_segment::meta,
}>;

template<typename Int>
Int byteswap(Int value)
{
	if constexpr (sizeof (Int) == 1)
	{
		return value;
	}
	else if constexpr (sizeof (Int) == 2)
	{
		value = ((value & uint16_t(0xff00)) >> 8) | ((value & uint16_t(0x00ff)) << 8);
		return value;
	}
	else if constexpr (sizeof (Int) == 4)
	{
		value = ((value & uint32_t(0xffff'0000)) >> 16) | ((value & uint32_t(0x0000'ffff)) << 16);
		value = ((value & uint32_t(0xff00'ff00)) >> 8) | ((value & uint32_t(0x00ff'00ff)) << 8);
		return value;
	}
	else if constexpr (sizeof (Int) == 8)
	{
		value = ((value & uint64_t(0xffff'ffff'0000'0000)) >> 32) | ((value & uint64_t(0x0000'0000'ffff'ffff)) << 32);
		value = ((value & uint64_t(0xffff'0000'ffff'0000)) >> 16) | ((value & uint64_t(0x0000'ffff'0000'ffff)) << 16);
		value = ((value & uint64_t(0xff00'ff00'ff00'ff00)) >> 8) | ((value & uint64_t(0x00ff'00ff'00ff'00ff)) << 8);
		return value;
	}
	else
	{
		static_assert(bz::meta::always_false<Int>);
	}
}

struct pointer_arithmetic_result_t
{
	ptr_t address;
	bool is_one_past_the_end;
};

struct error_reason_t
{
	lex::src_tokens src_tokens;
	bz::u8string message;
};

enum class pointer_arithmetic_check_result
{
	fail,
	good,
	one_past_the_end,
};

bool contained_in_object(type const *object_type, size_t offset, type const *subobject_type);
bool slice_contained_in_object(type const *object_type, size_t offset, type const *elem_type, size_t total_size, bool end_is_one_past_the_end);
pointer_arithmetic_check_result check_pointer_arithmetic(
	type const *object_type,
	size_t offset,
	size_t result_offset,
	bool is_one_past_the_end,
	type const *pointer_type
);

type const *get_multi_dimensional_array_elem_type(type const *arr_type);
bool is_native(endianness_kind endianness);

template<std::size_t Size>
using uint_t = bz::meta::conditional<
	Size == 1, uint8_t, bz::meta::conditional<
	Size == 2, uint16_t, bz::meta::conditional<
	Size == 4, uint32_t, bz::meta::conditional<
	Size == 8, uint64_t,
	void
>>>>;

} // namespace comptime::memory

#endif // COMPTIME_MEMORY_COMMON_H
