#ifndef BC_OPTIMIZATIONS_H
#define BC_OPTIMIZATIONS_H

#include "core.h"

#include <array>
#include <bz/u8string_view.h>

namespace bc
{

enum class optimization_kind
{
	instcombine,
	mem2reg,
	simplifycfg,
	reassociate,
	gvn,
	inline_,
	dce,
	adce,
	sccp,
	aggressive_instcombine,
	memcpyopt,

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
		T{ optimization_kind::instcombine,            "instcombine",            "Combine redundant instructions"                          },
		T{ optimization_kind::mem2reg,                "mem2reg",                "Promote memory to register"                              },
		T{ optimization_kind::simplifycfg,            "simplifycfg",            "Simplify the Control Flow Graph"                         },
		T{ optimization_kind::reassociate,            "reassociate",            "Reassociate expressions for better constant propagation" },
		T{ optimization_kind::gvn,                    "gvn",                    "Global Value Numbering"                                  },
		T{ optimization_kind::inline_,                "inline",                 "Function inlining"                                       },
		T{ optimization_kind::dce,                    "dce",                    "Dead Code Elimination"                                   },
		T{ optimization_kind::adce,                   "adce",                   "Aggressive Dead Code Elimination"                        },
		T{ optimization_kind::sccp,                   "sccp",                   "Sparse Conditional Constant Propagation"                 },
		T{ optimization_kind::aggressive_instcombine, "aggressive-instcombine", "Combine expression patterns"                             },
		T{ optimization_kind::memcpyopt,              "memcpyopt",              "memcpy optimization"                                     },
		T{ optimization_kind::aggressive_consteval,   "aggressive-consteval",   "Try to evaluate all expressions at compile time"         },
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
