#ifndef CTX_COMPTIME_EXECUTOR_H
#define CTX_COMPTIME_EXECUTOR_H

#include "bitcode_context.h"
#include "ast/constant_value.h"
#include "ast/expression.h"
#include "ast/statement.h"
#include <llvm/IR/Module.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>

namespace ctx
{

struct comptime_executor
{
	comptime_executor(global_context &global_ctx);

	ast::constant_value execute_function(ast::function_body &body);

	void compile_functions(void);

	bitcode_context context;
	std::unique_ptr<llvm::ExecutionEngine> engine;
};

} // namespace ctx

#endif // CTX_COMPTIME_EXECUTOR_H
