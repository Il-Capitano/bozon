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
	// https://docs.microsoft.com/en-us/cpp/build/x64-calling-convention?view=vs-2019
	microsoft_x64,
	// https://refspecs.linuxfoundation.org/elf/x86_64-abi-0.99.pdf
	systemv_amd64,
};

} // namespace abi

#endif // ABI_CALLING_CONVENTIONS_H
