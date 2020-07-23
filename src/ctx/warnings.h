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
	no_side_effect,
	no_comment_end,

	_last,
};

bz::u8string_view get_warning_name(warning_kind kind);

} // namespace ctx

#endif // CTX_WARNINGS_H
