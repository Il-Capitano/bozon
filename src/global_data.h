#ifndef GLOBAL_DATA_H
#define GLOBAL_DATA_H

#include "core.h"
#include "ctx/warnings.h"

inline bz::u8string output_file_name = "output.o";
inline std::array<bool, static_cast<size_t>(ctx::warning_kind::_last)> warnings{};
inline bz::vector<bz::u8string> files_to_compile;

void enable_warning(ctx::warning_kind kind);
void disable_warning(ctx::warning_kind kind);
void enable_Wall(void);

bool is_warning_enabled(ctx::warning_kind kind);

#endif // GLOBAL_DATA_H
