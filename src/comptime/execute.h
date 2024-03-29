#ifndef COMPTIME_EXECUTE_H
#define COMPTIME_EXECUTE_H

#include "instructions.h"
#include "executor_context.h"

namespace comptime
{

void execute_current_instruction(executor_context &context);

} // namespace comptime

#endif // COMPTIME_EXECUTE_H
