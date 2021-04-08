#ifndef COLORS_H
#define COLORS_H

#include <bz/u8string_view.h>

namespace colors
{

constexpr bz::u8string_view clear = "\033[0m";

constexpr bz::u8string_view black   = "\033[30m";
constexpr bz::u8string_view red     = "\033[31m";
constexpr bz::u8string_view green   = "\033[32m";
constexpr bz::u8string_view yellow  = "\033[33m";
constexpr bz::u8string_view blue    = "\033[34m";
constexpr bz::u8string_view magenta = "\033[35m";
constexpr bz::u8string_view cyan    = "\033[36m";
constexpr bz::u8string_view white   = "\033[37m";

constexpr bz::u8string_view bright_black   = "\033[90m";
constexpr bz::u8string_view bright_red     = "\033[91m";
constexpr bz::u8string_view bright_green   = "\033[92m";
constexpr bz::u8string_view bright_yellow  = "\033[93m";
constexpr bz::u8string_view bright_blue    = "\033[94m";
constexpr bz::u8string_view bright_magenta = "\033[95m";
constexpr bz::u8string_view bright_cyan    = "\033[96m";
constexpr bz::u8string_view bright_white   = "\033[97m";

} // namespace colors

#endif // COLORS_H
