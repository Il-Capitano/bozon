#include "global_data.h"
#include "config.h"

#ifdef BOZON_CONFIG_BACKEND_LLVM
#include <llvm/Config/llvm-config.h>
#endif // BOZON_CONFIG_BACKEND_LLVM

bz::optional<emit_type> parse_emit_type(bz::u8string_view arg)
{
	if (config::backend_llvm && arg == "obj")
	{
		return emit_type::obj;
	}
	else if (config::backend_llvm && arg == "asm")
	{
		return emit_type::asm_;
	}
	else if (config::backend_llvm && arg == "llvm-bc")
	{
		return emit_type::llvm_bc;
	}
	else if (config::backend_llvm && arg == "llvm-ir")
	{
		return emit_type::llvm_ir;
	}
	else if (config::backend_c && arg == "c")
	{
		return emit_type::c;
	}
	else if (arg == "null")
	{
		return emit_type::null;
	}
	else
	{
		return {};
	}
}

namespace global_data
{

emit_type emit_file_type = config::backend_llvm ? emit_type::obj
	: config::backend_c ? emit_type::c
	: emit_type::null;

} // namespace global_data

bool is_warning_enabled(ctx::warning_kind kind) noexcept
{
	bz_assert(kind != ctx::warning_kind::_last);
	return global_data::warnings[static_cast<size_t>(kind)];
}

bool is_warning_error(ctx::warning_kind kind) noexcept
{
	bz_assert(kind != ctx::warning_kind::_last);
	return global_data::warnings[static_cast<size_t>(kind)] && global_data::error_warnings[static_cast<size_t>(kind)];
}

static constexpr bz::u8string_view bozon_version = "0.0.0";
#ifdef BOZON_CONFIG_BACKEND_LLVM
static constexpr bz::u8string_view llvm_version = LLVM_VERSION_STRING;
#else
static constexpr bz::u8string_view llvm_version = "";
#endif // BOZON_CONFIG_BACKEND_LLVM

void print_version_info(void)
{
	if (global_data::do_verbose)
	{
		if constexpr (config::backend_llvm)
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
			auto const default_target = BOZON_CONFIG_NATIVE_TARGET;
			bz::print(
				"bozon {}\n"
				"default target: {}\n",
				bozon_version,
				default_target
			);
		}
	}
	else
	{
		bz::print("bozon {}\n", bozon_version);
	}
}
