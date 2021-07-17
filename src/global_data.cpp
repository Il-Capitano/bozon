#include "global_data.h"
#include <llvm/ADT/StringRef.h>
#include <llvm/Config/llvm-config.h>
#include <llvm/Support/Host.h>

bool is_warning_enabled(ctx::warning_kind kind) noexcept
{
	bz_assert(kind != ctx::warning_kind::_last);
	return warnings[static_cast<size_t>(kind)];
}

bool is_warning_error(ctx::warning_kind kind) noexcept
{
	bz_assert(kind != ctx::warning_kind::_last);
	return warnings[static_cast<size_t>(kind)] && error_warnings[static_cast<size_t>(kind)];
}

static constexpr bz::u8string_view bozon_version = "0.0.0";
static constexpr bz::u8string_view llvm_version = LLVM_VERSION_STRING;

void print_version_info(void)
{
	if (do_verbose)
	{
		auto const default_target = llvm::Triple::normalize(llvm::sys::getDefaultTargetTriple());
		auto const host_cpu = llvm::sys::getHostCPUName();
		bz::print(
			"bozon {}\n"
			"default target: {}\n"
			"host CPU: {}\n"
			"LLVM version: {}\n",
			bozon_version, default_target.c_str(),
			host_cpu.str().c_str(), llvm_version
		);
	}
	else
	{
		bz::print("bozon {}\n", bozon_version);
	}
}
