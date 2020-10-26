#ifndef CTX_WARNINGS_H
#define CTX_WARNINGS_H

#include "core.h"

namespace ctx
{

enum class warning_kind
{
	int_overflow,
	int_divide_by_zero,
	float_overflow,
	float_divide_by_zero,
	unknown_attribute,
	null_pointer_dereference,
	unused_value,
	unclosed_comment,
	mismatched_brace_indent,
	unused_variable,
	greek_question_mark,
	bad_file_extension,
	unknown_target,
	invalid_unicode,
	nan_compare,
	out_of_bounds_index,

	_last,
};

struct warning_info
{
	warning_kind      kind;
	bz::u8string_view name;
	bz::u8string_view description;
};

constexpr auto warning_infos = []() {
	using result_t = std::array<warning_info, static_cast<size_t>(warning_kind::_last)>;
	using T = warning_info;
	result_t result = {
		T{ warning_kind::int_overflow,             "int-overflow",             "Integer overflow in constant expression"                                   },
		T{ warning_kind::int_divide_by_zero,       "int-divide-by-zero",       "Integer division by zero in non-constant expression"                       },
		T{ warning_kind::float_overflow,           "float-overflow",           "Floating-point inf or nan result in constant expression"                   },
		T{ warning_kind::float_divide_by_zero,     "float-divide-by-zero",     "Floating-point division by zero in non-constant expression"                },

		T{ warning_kind::unknown_attribute,        "unknown-attribute",        "Unknown attribute on statement or declaration"                             },
		T{ warning_kind::null_pointer_dereference, "null-pointer-dereference", "The dereferenced pointer is a constant expression and is null"             },
		T{ warning_kind::unused_value,             "unused-value",             "Value of expression is never used and expression has no side-effects"      },
		T{ warning_kind::unclosed_comment,         "unclosed-comment",         "Unclosed block comment"                                                    },
		T{ warning_kind::mismatched_brace_indent,  "mismatched-brace-indent",  "Opening and closing braces have different amount of indentation"           },
		T{ warning_kind::unused_variable,          "unused-variable",          "Variable is never used"                                                    },
		T{ warning_kind::greek_question_mark,      "greek-question-mark",      "Greek question mark (U+037E) in file, which looks the same as a semicolon" },
		T{ warning_kind::bad_file_extension,       "bad-file-extension",       "Output file doesn't have the usual file extension for output type"         },
		T{ warning_kind::unknown_target,           "unknown-target",           "ABI compatibility hasn't been implemented for this target yet"             },
		T{ warning_kind::invalid_unicode,          "invalid-unicode",          "'char' result of an expression is an invalid unicode codepoint"            },
		T{ warning_kind::nan_compare,              "nan-compare",              "Comparing against nan when comparing floating-point numbers"               },
		T{ warning_kind::out_of_bounds_index,      "out-of-bounds-index",      "Index is out of bounds in array subscript"                                 },
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

bz::u8string_view get_warning_name(warning_kind kind);
warning_kind get_warning_kind(bz::u8string_view name);

} // namespace ctx

#endif // CTX_WARNINGS_H
