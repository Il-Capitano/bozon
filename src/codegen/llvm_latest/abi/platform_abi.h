#ifndef CODEGEN_LLVM_LATEST_ABI_PLATFORM_ABI_H
#define CODEGEN_LLVM_LATEST_ABI_PLATFORM_ABI_H

#include "core.h"

namespace codegen::llvm_latest::abi
{

enum class platform_abi
{
	generic,
	// https://docs.microsoft.com/en-us/cpp/build/x64-calling-convention?view=vs-2019
	microsoft_x64,
	// https://refspecs.linuxfoundation.org/elf/x86_64-abi-0.99.pdf
	systemv_amd64,
};

} // namespace codegen::llvm_latest::abi

#endif // CODEGEN_LLVM_LATEST_ABI_PLATFORM_ABI_H
