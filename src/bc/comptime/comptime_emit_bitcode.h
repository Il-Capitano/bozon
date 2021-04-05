#ifndef BC_COMPTIME_EMIT_BITCODE_H
#define BC_COMPTIME_EMIT_BITCODE_H

#include "ast/typespec.h"
#include "ast/expression.h"
#include "ast/statement.h"
#include "ctx/comptime_executor.h"
#include "bc/common.h"

namespace bc::comptime
{

llvm::Function *add_function_to_module(ast::function_body *func_body, ctx::comptime_executor_context &context);
void emit_function_bitcode(ast::function_body &func_body, ctx::comptime_executor_context &context);
void emit_global_variable(ast::decl_variable const &var_decl, ctx::comptime_executor_context &context);

void resolve_global_type(ast::type_info *info, llvm::Type *type, ctx::comptime_executor_context &context);
void add_builtin_functions(ctx::comptime_executor_context &context);
[[nodiscard]] bool emit_necessary_functions(ctx::comptime_executor_context &context);

std::pair<llvm::Function *, bz::vector<llvm::Function *>> create_function_for_comptime_execution(
	ast::function_body *body,
	bz::array_view<ast::constant_value const> params,
	ctx::comptime_executor_context &context
);

std::pair<llvm::Function *, bz::vector<llvm::Function *>> create_function_for_comptime_execution(
	ast::expr_compound &expr,
	ctx::comptime_executor_context &context
);

void emit_push_call(
	lex::src_tokens src_tokens,
	ast::function_body const *func_body,
	ctx::comptime_executor_context &context
);

void emit_pop_call(ctx::comptime_executor_context &context);

} // namespace bc::comptime

#endif // BC_COMPTIME_EMIT_BITCODE_H
