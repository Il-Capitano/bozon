#ifndef CL_CLPARSER_H
#define CL_CLPARSER_H

#include "core.h"

namespace cl
{

bz::vector<bz::u8string_view> get_args(int argc, char const **argv);
void parse_command_line(bz::array_view<bz::u8string_view const> args);

} // namespace cl

#endif // CL_CLPARSER_H
