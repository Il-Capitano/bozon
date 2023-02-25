#ifndef COMPTIME_VALUES_H
#define COMPTIME_VALUES_H

#include "core.h"

namespace comptime
{

enum class value_type
{
	i1, i8, i16, i32, i64,
	f32, f64,
	ptr,
	none, any,
};

struct none_t
{};

using ptr_t = uint64_t;

union instruction_value
{
	bool i1;
	uint8_t i8;
	uint16_t i16;
	uint32_t i32;
	uint64_t i64;
	float32_t f32;
	float64_t f64;
	ptr_t ptr;
	none_t none;
};

} // namespace comptime

template<>
struct bz::formatter<comptime::instruction_value>
{
	static bz::u8string format(comptime::instruction_value value, bz::u8string_view)
	{
		return bz::format(
			"(i1={}, i8={}, i16={}, i32={}, i64={}, f32={}, f64={}, ptr=0x{:x})",
			value.i1, value.i8, value.i16, value.i32, value.i64, value.f32, value.f64, value.ptr
		);
	}
};

#endif // COMPTIME_VALUES_H
