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

inline bool display_help = false;
inline bool display_version = false;
inline bool display_warning_help = false;

inline bz::u8string output_file_name;
inline std::array<bool, static_cast<size_t>(ctx::warning_kind::_last)> warnings{};
inline bz::u8string source_file;
inline bool do_profile = false;
inline compilation_phase compile_until = compilation_phase::link;
// inline bool do_verbose_error = false;
inline bz::u8string target;

inline size_t tab_size = 4;
inline bool no_error_highlight = false;

constexpr bz::u8string_view version_info = "bozon 0.0.0\n";


void enable_warning(ctx::warning_kind kind);
void disable_warning(ctx::warning_kind kind);
void enable_Wall(void);

bool is_warning_enabled(ctx::warning_kind kind);

#endif // GLOBAL_DATA_H
