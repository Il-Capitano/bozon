#include "comptime_executor.h"
#include "global_context.h"
#include "bc/comptime/comptime_emit_bitcode.h"
#include <llvm/ExecutionEngine/ExecutionEngine.h>

namespace ctx
{

comptime_executor::comptime_executor(global_context &global_ctx)
	: context(global_ctx, global_ctx._comptime_module), engine()
{
	// LLVM doesn't have a built-in interpreter, so we can't easily
	// implement this.  LLVM-13 seems to add one though.
	bz_unreachable;
}

} // namespace ctx
