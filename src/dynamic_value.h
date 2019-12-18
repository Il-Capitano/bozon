#ifndef DYNAMIC_VALUE_H
#define DYNAMIC_VALUE_H

#include "core.h"

using str = bz::string_view;

struct dynamic_value;

struct dv_array
{
	bz::vector<dynamic_value> array;
};

struct dv_pointer
{
	dynamic_value *ptr;
};

struct dynamic_value : bz::variant<
	int8_t, int16_t, int32_t, int64_t,
	uint8_t, uint16_t, uint32_t, uint64_t,
	char, bool,
	dv_array, dv_pointer
>
{
	using base_t = bz::variant<
		int8_t, int16_t, int32_t, int64_t,
		uint8_t, uint16_t, uint32_t, uint64_t,
		char, bool,
		dv_array, dv_pointer
	>;

	using base_t::variant;
	using base_t::get;
};


#endif // DYNAMIC_VALUE_H
