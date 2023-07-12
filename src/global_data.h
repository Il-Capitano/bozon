#ifndef GLOBAL_DATA_H
#define GLOBAL_DATA_H

#include "core.h"
#include "ctx/warnings.h"
#include "codegen/optimizations.h"

enum class compilation_phase
{
	parse_command_line,
	parse_global_symbols,
	parse,
	emit_bitcode,
	compile,
	link,
};

enum class emit_type
{
	obj,
	asm_,
	llvm_bc,
	llvm_ir,
	null,
};

inline bz::optional<emit_type> parse_emit_type(bz::u8string_view arg)
{
	if (arg == "obj")
	{
		return emit_type::obj;
	}
	else if (arg == "asm")
	{
		return emit_type::asm_;
	}
	else if (arg == "llvm-bc")
	{
		return emit_type::llvm_bc;
	}
	else if (arg == "llvm-ir")
	{
		return emit_type::llvm_ir;
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

enum class x86_asm_syntax_kind
{
	att,
	intel,
};

inline bz::optional<x86_asm_syntax_kind> parse_x86_asm_syntax(bz::u8string_view arg)
{
	if (arg == "att")
	{
		return x86_asm_syntax_kind::att;
	}
	else if (arg == "intel")
	{
		return x86_asm_syntax_kind::intel;
	}
	else
	{
		return {};
	}
}

enum class target_endianness_kind
{
	little,
	big,
};

inline bz::optional<target_endianness_kind> parse_target_endianness(bz::u8string_view arg)
{
	if (arg == "little")
	{
		return target_endianness_kind::little;
	}
	else if (arg == "big")
	{
		return target_endianness_kind::big;
	}
	else
	{
		return {};
	}
}

inline bool display_version = false;

inline bz::array<bool, ctx::warning_infos.size()> warnings{};
inline bz::array<bool, ctx::warning_infos.size()> error_warnings{};

inline bz::u8string output_file_name;
inline bz::u8string source_file;

inline compilation_phase compile_until = compilation_phase::link;

inline bool do_profile = false;
inline bool no_main = false;
#ifndef NDEBUG
inline bool debug_ir_output = false;
inline bool debug_comptime_print_functions = false;
inline bool debug_comptime_print_instructions = false;
inline bool debug_no_emit_file = false;
#endif // !NDEBUG
inline bool do_verbose = false;
inline bool enable_comptime_print = false;
inline bool panic_on_unreachable = true;
inline bool panic_on_null_dereference = true;
inline bool panic_on_null_pointer_arithmetic = true;
inline bool panic_on_null_get_value = true;
inline bool panic_on_invalid_switch = true;
inline bool discard_llvm_value_names = true;
inline bool return_zero_on_error = false;
inline bool freestanding = false;
inline uint64_t target_pointer_size = 0;
inline target_endianness_kind target_endianness = target_endianness_kind::little;

inline bz::u8string target;
inline emit_type emit_file_type = emit_type::obj;
inline x86_asm_syntax_kind x86_asm_syntax = x86_asm_syntax_kind::att;

inline size_t tab_size = 4;
inline bool no_error_highlight = false;

inline bz::vector<bz::u8string> import_dirs;
inline bz::vector<bz::u8string> defines;
inline bz::u8string stdlib_dir;

inline size_t max_opt_iter_count = 1;
inline uint32_t opt_level = 0;
inline uint32_t size_opt_level = 0;
inline uint32_t machine_code_opt_level = 0;

bool is_warning_enabled(ctx::warning_kind kind) noexcept;
bool is_warning_error(ctx::warning_kind kind) noexcept;

void print_version_info(void);

#ifdef BOZON_PROFILE_COMPTIME
inline size_t comptime_executed_instructions_count = 0;
inline size_t comptime_emitted_instructions_count = 0;
inline bool debug_comptime_print_instruction_counts = false;
#endif // BOZON_PROFILE_COMPTIME

#endif // GLOBAL_DATA_H
