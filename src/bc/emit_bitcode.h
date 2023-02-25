#ifndef BC_EMIT_BITCODE_H
#define BC_EMIT_BITCODE_H

#include "core.h"
#include "ast/statement.h"
#include "ctx/bitcode_context.h"

namespace bc
{

void add_function_to_module(ast::function_body *func_body, ctx::bitcode_context &context);
void emit_function_bitcode(ast::function_body &func_body, ctx::bitcode_context &context);

void emit_global_variable(ast::decl_variable const &var_decl, ctx::bitcode_context &context);
void emit_global_type_symbol(ast::type_info const &info, ctx::bitcode_context &context);
void emit_global_type(ast::type_info const &info, ctx::bitcode_context &context);
void emit_necessary_functions(ctx::bitcode_context &context);

void emit_destruct_operation(
	ast::destruct_operation const &destruct_op,
	llvm::Value *condition,
	llvm::Value *move_destruct_indicator,
	ctx::bitcode_context &context
);
void emit_destruct_operation(
	ast::destruct_operation const &destruct_op,
	val_ptr value,
	llvm::Value *condition,
	llvm::Value *move_destruct_indicator,
	llvm::Value *rvalue_array_elem_ptr,
	ctx::bitcode_context &context
);

} // namespace bc

#endif // BC_EMIT_BITCODE_H
