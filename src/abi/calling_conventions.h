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

} // namespace abi

#endif // ABI_CALLING_CONVENTIONS_H
