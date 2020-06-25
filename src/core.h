#ifndef CORE_H
#define CORE_H

#include <fstream>
#include <iostream>
#include <vector>
#include <array>
#include <list>
#include <map>
#include <utility>
#include <tuple>
#include <memory>
#include <algorithm>
#include <chrono>

#include <cstdint>
#include <cmath>
#include <cstring>

#include <bz/variant.h>
#include <bz/string.h>
#include <bz/u8string.h>
#include <bz/optional.h>
#include <bz/format.h>
#include <bz/result.h>

#include "my_assert.h"

#undef min
#undef max

using std::int8_t;
using std::int16_t;
using std::int32_t;
using std::int64_t;
using std::uint8_t;
using std::uint16_t;
using std::uint32_t;
using std::uint64_t;
using float32_t = float;
using float64_t = double;

template<typename To, typename From>
To bit_cast(From value)
{
	static_assert(sizeof(To) == sizeof(From));
	To res;
	std::memcpy(&res, &value, sizeof(To));
	return res;
}

template<typename Array, typename Cmp, typename Swap>
constexpr void constexpr_bubble_sort(Array &arr, Cmp cmp, Swap swap)
{
	auto const size = arr.size();
	using size_type = decltype(arr.size());
	for (size_type i = 0; i < size; ++i)
	{
		for (size_type j = 0; j < size - 1; ++j)
		{
			auto &lhs = arr[j];
			auto &rhs = arr[j + 1];
			if (cmp(lhs, rhs))
			{
				// nothing
			}
			else
			{
				swap(lhs, rhs);
			}
		}
	}
}

#endif // CORE_H
