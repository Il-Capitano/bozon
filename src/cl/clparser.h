#ifndef CL_CLPARSER_H
#define CL_CLPARSER_H

#include "core.h"
#include "ctx/command_parse_context.h"

namespace cl
{

bz::vector<bz::u8string_view> get_args(int argc, char const **argv);
void parse_command_line(ctx::command_parse_context &context);

void print_help_screen(void);
void print_version_info(void);
void print_warning_info(void);

} // namespace cl

#endif // CL_CLPARSER_H
