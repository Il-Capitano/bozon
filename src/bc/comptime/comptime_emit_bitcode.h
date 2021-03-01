#ifndef BC_EMIT_BITCODE_H
#define BC_EMIT_BITCODE_H

#include "ast/typespec.h"
#include "ast/expression.h"
#include "ast/statement.h"
#include "ctx/comptime_executor.h"

namespace bc::comptime
{

llvm::Function *add_function_to_module(ast::function_body *func_body, ctx::comptime_executor_context &context);
void emit_function_bitcode(ast::function_body &func_body, ctx::comptime_executor_context &context);

void resolve_global_type(ast::type_info *info, llvm::Type *type, ctx::comptime_executor_context &context);
void add_builtin_functions(ctx::comptime_executor_context &context);
void emit_necessary_functions(ctx::comptime_executor_context &context);

llvm::Function *create_function_for_comptime_execution(ast::function_body *body, bz::array_view<ast::constant_value const> params);

} // namespace bc::comptime

#endif // BC_EMIT_BITCODE_H
