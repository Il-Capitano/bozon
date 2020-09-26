#ifndef GLOBAL_DATA_H
#define GLOBAL_DATA_H

#include "core.h"
#include "ctx/warnings.h"

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
	object,
	asm_,
	llvm_bc,
	llvm_ir,
};

inline bz::optional<emit_type> parse_emit_type(bz::u8string_view arg)
{
	if (arg == "object")
	{
		return emit_type::object;
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

inline bool display_help = false;
inline bool display_version = false;
inline bool display_warning_help = false;

inline bz::u8string output_file_name;
inline std::array<bool, static_cast<size_t>(ctx::warning_kind::_last)> warnings{};
inline bz::u8string source_file;
inline bool do_profile = false;
inline compilation_phase compile_until = compilation_phase::link;
inline bool do_verbose = false;
inline bz::u8string target;
inline emit_type emit_file_type = emit_type::object;

inline size_t tab_size = 4;
inline bool no_error_highlight = false;

inline bz::vector<bz::u8string> import_dirs;

constexpr bz::u8string_view version_info = "bozon 0.0.0\n";


void enable_warning(ctx::warning_kind kind);
void disable_warning(ctx::warning_kind kind);
void enable_Wall(void);

bool is_warning_enabled(ctx::warning_kind kind);

#endif // GLOBAL_DATA_H
