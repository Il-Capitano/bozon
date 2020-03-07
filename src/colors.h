#ifndef COLORS_H
#define COLORS_H

namespace colors
{

constexpr char const *clear = "\033[0m";

constexpr bz::string_view black   = "\033[30m";
constexpr bz::string_view red     = "\033[31m";
constexpr bz::string_view green   = "\033[32m";
constexpr bz::string_view yellow  = "\033[33m";
constexpr bz::string_view blue    = "\033[34m";
constexpr bz::string_view magenta = "\033[35m";
constexpr bz::string_view cyan    = "\033[36m";
constexpr bz::string_view white   = "\033[37m";

constexpr bz::string_view bright_black   = "\033[90m";
constexpr bz::string_view bright_red     = "\033[91m";
constexpr bz::string_view bright_green   = "\033[92m";
constexpr bz::string_view bright_yellow  = "\033[93m";
constexpr bz::string_view bright_blue    = "\033[94m";
constexpr bz::string_view bright_magenta = "\033[95m";
constexpr bz::string_view bright_cyan    = "\033[96m";
constexpr bz::string_view bright_white   = "\033[97m";

constexpr auto error_color      = bright_red;
constexpr auto note_color       = bright_cyan;
constexpr auto suggestion_color = bright_green;

} // namespace colors

#endif // COLORS_H
