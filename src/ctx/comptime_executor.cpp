#include "comptime_executor.h"
#include "global_context.h"

namespace ctx
{

comptime_executor::comptime_executor(global_context &global_ctx)
	: module("comptime_execution", global_ctx._llvm_context),
	  context(global_ctx, module)
{}

} // namespace ctx
