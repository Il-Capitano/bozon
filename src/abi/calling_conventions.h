#ifndef ABI_CALLING_CONVENTIONS_H
#define ABI_CALLING_CONVENTIONS_H

#include "core.h"

namespace abi
{

enum class calling_convention : int8_t
{
	c,
	fast,
	std,
	_last,
};

static constexpr calling_convention default_cc = calling_convention::c;

inline bz::u8string_view to_string(calling_convention cc)
{
	switch (cc)
	{
	case calling_convention::c:
		return "c";
	case calling_convention::fast:
		return "fast";
	case calling_convention::std:
		return "std";
	default:
		return "<unknown>";
	}
}

} // namespace abi

#endif // ABI_CALLING_CONVENTIONS_H
