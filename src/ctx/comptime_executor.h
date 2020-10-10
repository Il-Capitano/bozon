#ifndef CTX_COMPTIME_EXECUTOR_H
#define CTX_COMPTIME_EXECUTOR_H

#include "bitcode_context.h"
#include "ast/constant_value.h"
#include "ast/expression.h"
#include "ast/statement.h"
#include <llvm/IR/Module.h>

namespace ctx
{

struct comptime_executor
{
	comptime_executor(global_context &global_ctx);

	llvm::Module module;
	bitcode_context context;
};

} // namespace ctx

#endif // CTX_COMPTIME_EXECUTOR_H
