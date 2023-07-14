#ifndef CODEGEN_LLVM_LATEST_EMIT_BITCODE_H
#define CODEGEN_LLVM_LATEST_EMIT_BITCODE_H

#include "core.h"
#include "ast/statement.h"
#include "bitcode_context.h"

namespace codegen::llvm_latest
{

void add_function_to_module(ast::function_body *func_body, bitcode_context &context);
void emit_function_bitcode(ast::function_body &func_body, bitcode_context &context);

void emit_global_variable(ast::decl_variable const &var_decl, bitcode_context &context);
void emit_global_type_symbol(ast::type_info const &info, bitcode_context &context);
void emit_global_type(ast::type_info const &info, bitcode_context &context);
void emit_necessary_functions(bitcode_context &context);

void emit_destruct_operation(
	ast::destruct_operation const &destruct_op,
	llvm::Value *condition,
	llvm::Value *move_destruct_indicator,
	bitcode_context &context
);
void emit_destruct_operation(
	ast::destruct_operation const &destruct_op,
	val_ptr value,
	llvm::Value *condition,
	llvm::Value *move_destruct_indicator,
	llvm::Value *rvalue_array_elem_ptr,
	bitcode_context &context
);

} // namespace codegen::llvm_latest

#endif // CODEGEN_LLVM_LATEST_EMIT_BITCODE_H
