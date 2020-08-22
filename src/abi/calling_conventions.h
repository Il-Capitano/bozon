#ifndef ABI_CALLING_CONVENTIONS_H
#define ABI_CALLING_CONVENTIONS_H

#include "core.h"

namespace abi
{

enum class calling_convention
{
	bozon,
	c,
	fast,
	std,
};

enum class platform_abi
{
	generic,
	microsoft_x64,
};

} // namespace abi

#endif // ABI_CALLING_CONVENTIONS_H
