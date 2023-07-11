#include "global_data.h"
#include "config.h"
#include <llvm/Config/llvm-config.h>

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
		auto const default_target = BOZON_CONFIG_NATIVE_TARGET;
		bz::print(
			"bozon {}\n"
			"default target: {}\n"
			"LLVM version: {}\n",
			bozon_version,
			default_target,
			llvm_version
		);
	}
	else
	{
		bz::print("bozon {}\n", bozon_version);
	}
}
