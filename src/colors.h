#ifndef COLORS_H
#define COLORS_H

namespace colors
{

constexpr char const *clear = "\033[0m";

constexpr char const *black   = "\033[30m";
constexpr char const *red     = "\033[31m";
constexpr char const *green   = "\033[32m";
constexpr char const *yellow  = "\033[33m";
constexpr char const *blue    = "\033[34m";
constexpr char const *magenta = "\033[35m";
constexpr char const *cyan    = "\033[36m";
constexpr char const *white   = "\033[37m";

constexpr char const *bright_black   = "\033[90m";
constexpr char const *bright_red     = "\033[91m";
constexpr char const *bright_green   = "\033[92m";
constexpr char const *bright_yellow  = "\033[93m";
constexpr char const *bright_blue    = "\033[94m";
constexpr char const *bright_magenta = "\033[95m";
constexpr char const *bright_cyan    = "\033[96m";
constexpr char const *bright_white   = "\033[97m";

constexpr auto error_color = bright_red;
constexpr auto note_color  = bright_cyan;

} // namespace colors

#endif // COLORS_H
