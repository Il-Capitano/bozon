#ifndef COMPTIME_INSTRUCTIONS_FORWARD_H
#define COMPTIME_INSTRUCTIONS_FORWARD_H

namespace comptime
{

struct instruction;
struct function;

#ifdef BOZON_PROFILE_COMPTIME
void print_instruction_counts(void);
#endif // BOZON_PROFILE_COMPTIME

} // namespace comptime

#endif // COMPTIME_INSTRUCTIONS_FORWARD_H
