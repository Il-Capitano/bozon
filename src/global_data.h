#ifndef GLOBAL_DATA_H
#define GLOBAL_DATA_H

#include "core.h"
#include "ctx/warnings.h"
#include "bc/optimizations.h"

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

inline bool display_version      = false;

inline bz::array<bool, ctx::warning_infos.size()>     warnings{};
inline bz::array<bool, ctx::warning_infos.size()>     error_warnings{};
inline bz::array<bool, bc::optimization_infos.size()> optimizations{};

inline bz::u8string output_file_name;
inline bz::u8string source_file;

inline compilation_phase compile_until = compilation_phase::link;

inline bool do_profile = false;
inline bool debug_ir_output = false;
inline bool debug_comptime_ir_output = false;
inline bool do_verbose = false;

inline bz::u8string target;
inline emit_type emit_file_type = emit_type::obj;
inline x86_asm_syntax_kind x86_asm_syntax = x86_asm_syntax_kind::att;

inline size_t tab_size = 4;
inline bool no_error_highlight = false;

inline bz::vector<bz::u8string> import_dirs;

inline size_t max_opt_iter_count = 32;

bool is_warning_enabled(ctx::warning_kind kind) noexcept;
bool is_warning_error(ctx::warning_kind kind) noexcept;
bool is_optimization_enabled(bc::optimization_kind kind) noexcept;
bool is_any_optimization_enabled(void) noexcept;

void print_version_info(void);

#endif // GLOBAL_DATA_H
