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
#include <bz/array.h>

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

template<size_t N>
constexpr uint64_t bit_at = uint64_t(1) << N;

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

template<
	typename Stream,
	typename Context,
	auto const &parsers,
	auto default_parser
>
constexpr auto create_parse_fn() -> decltype(default_parser)
{
	static_assert(bz::meta::is_same<decltype(parsers[0].parse_fn), decltype(default_parser)>);
	using parse_fn_t = decltype(default_parser);
	using ret_t = bz::meta::fn_return_type<parse_fn_t>;
	using args_t = bz::meta::fn_param_types<parse_fn_t>;
	static_assert(bz::meta::is_same<args_t, bz::meta::type_pack<Stream &, Stream, Context &>>);
	return [&]<size_t ...Ns>(bz::meta::index_sequence<Ns...>) {
		return +[](Stream &stream, Stream end, Context &context) -> ret_t {
			auto const kind = stream->kind;
			auto parse_fn = default_parser;
			((
				kind == parsers[Ns].kind
				? (void)(parse_fn = parsers[Ns].parse_fn)
				: (void)0
			), ...);
			return parse_fn(stream, end, context);
		};
	}(bz::meta::make_index_sequence<parsers.size()>{});
}

#define declare_default_5(type)            \
type(void)                      = default; \
type(type const &)              = default; \
type(type &&)                   = default; \
type &operator = (type const &) = default; \
type &operator = (type &&)      = default; \
~type(void) noexcept            = default;

#endif // CORE_H
