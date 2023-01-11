#ifndef BC_OPTIMIZATIONS_H
#define BC_OPTIMIZATIONS_H

#include "core.h"

#include <array>
#include <bz/u8string_view.h>

namespace bc
{

enum class optimization_kind
{
	aggressive_consteval,

	_last,
};

struct optimization_info
{
	optimization_kind kind;
	bz::u8string_view name;
	bz::u8string_view description;
};

constexpr bz::array optimization_infos = []() {
	using result_t = bz::array<optimization_info, static_cast<size_t>(optimization_kind::_last)>;
	using T = optimization_info;
	result_t result = {
		T{ optimization_kind::aggressive_consteval, "aggressive-consteval", "Try to evaluate all expressions at compile time" },
	};

	constexpr_bubble_sort(
		result,
		[](auto const &lhs, auto const &rhs) {
			return lhs.kind < rhs.kind;
		},
		[](auto &lhs, auto &rhs) {
			auto const tmp = lhs;
			lhs = rhs;
			rhs = tmp;
		}
	);

	return result;
}();

} // namespace bc

#endif // BC_OPTIMIZATIONS_H
