#ifndef BC_EMIT_BITCODE_H
#define BC_EMIT_BITCODE_H

#include "core.h"
#include "ast/statement.h"
#include "ctx/bitcode_context.h"
#include "ctx/comptime_executor.h"

namespace bc
{

void add_function_to_module(ast::function_body *func_body, ctx::bitcode_context &context);
void emit_function_bitcode(ast::function_body &func_body, ctx::bitcode_context &context);

void emit_global_variable(ast::decl_variable const &var_decl, ctx::bitcode_context &context);
void emit_global_type_symbol(ast::type_info const &info, ctx::bitcode_context &context);
void emit_global_type(ast::type_info const &info, ctx::bitcode_context &context);
void emit_necessary_functions(ctx::bitcode_context &context);


llvm::Function *add_function_to_module(ast::function_body *func_body, ctx::comptime_executor_context &context);
void emit_function_bitcode(ast::function_body &func_body, ctx::comptime_executor_context &context);
void emit_global_variable(ast::decl_variable const &var_decl, ctx::comptime_executor_context &context);

void resolve_global_type(ast::type_info *info, llvm::Type *type, ctx::comptime_executor_context &context);
[[nodiscard]] bool emit_necessary_functions(size_t start_index, ctx::comptime_executor_context &context);

std::pair<llvm::Function *, bz::vector<llvm::Function *>> create_function_for_comptime_execution(
	ast::function_body *body,
	bz::array_view<ast::expression const> params,
	ctx::comptime_executor_context &context
);

std::pair<llvm::Function *, bz::vector<llvm::Function *>> create_function_for_comptime_execution(
	ast::expr_compound &expr,
	ctx::comptime_executor_context &context
);

[[nodiscard]] llvm::Value *emit_push_call(
	lex::src_tokens const &src_tokens,
	ast::function_body const *func_body,
	ctx::comptime_executor_context &context
);

void emit_pop_call(llvm::Value *prec_call_error_count, ctx::comptime_executor_context &context);


} // namespace bc

#endif // BC_EMIT_BITCODE_H
