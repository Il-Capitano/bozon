#include "comptime_executor.h"
#include "global_context.h"

namespace ctx
{

comptime_executor::comptime_executor(global_context &global_ctx)
	: context(global_ctx, global_ctx._comptime_module)
{}

} // namespace ctx
