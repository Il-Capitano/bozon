#include "warnings.h"

namespace ctx
{

struct warning_info
{
	warning_kind      kind;
	bz::u8string_view name;
};

static constexpr auto warning_infos = []() {
	using result_t = std::array<warning_info, static_cast<size_t>(warning_kind::_last)>;
	using T = warning_info;
	result_t result = {
		T{ warning_kind::int_overflow,             "int-overflow"             },
		T{ warning_kind::int_divide_by_zero,       "int-divide-by-zero"       },
		T{ warning_kind::float_overflow,           "float-overflow"           },
		T{ warning_kind::float_divide_by_zero,     "float-divide-by-zero"     },

		T{ warning_kind::unknown_attribute,        "unknown-attribute"        },
		T{ warning_kind::null_pointer_dereference, "null-pointer-dereference" },
		T{ warning_kind::unused_value,             "unused-value"             },
		T{ warning_kind::unclosed_comment,         "unclosed-comment"         },
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

static_assert([]() {
	for (auto const &info : warning_infos)
	{
		if (info.name.starts_with("no-"))
		{
			return false;
		}
	}
	return true;
}(), "a warning name starts with 'no-'");

static_assert([]() {
	for (size_t i = 0; i < warning_infos.size(); ++i)
	{
		if (static_cast<size_t>(warning_infos[i].kind) != i)
		{
			return false;
		}
	}
	return true;
}(), "warning_infos is not sorted");


bz::u8string_view get_warning_name(warning_kind kind)
{
	return warning_infos[static_cast<size_t>(kind)].name;
}

warning_kind get_warning_kind(bz::u8string_view name)
{
	for (auto &info : warning_infos)
	{
		if (info.name == name)
		{
			return info.kind;
		}
	}
	return warning_kind::_last;
}

} // namespace ctx
