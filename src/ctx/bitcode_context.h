#ifndef CTX_BITCODE_CONTEXT_H
#define CTX_BITCODE_CONTEXT_H

#include "core.h"

#include "ast/typespec.h"
#include "ast/expression.h"
#include "ast/statement.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Value.h>

namespace ctx
{

struct global_context;

struct bitcode_context
{
	bitcode_context(global_context &_global_ctx);

	llvm::Value *get_variable_val(ast::decl_variable const *var_decl) const;
	llvm::BasicBlock *add_basic_block(bz::string_view name);

	llvm::Type *get_built_in_type(ast::type_info::type_kind kind) const;
	llvm::Type *get_int8_t(void) const;
	llvm::Type *get_int16_t(void) const;
	llvm::Type *get_int32_t(void) const;
	llvm::Type *get_int64_t(void) const;
	llvm::Type *get_uint8_t(void) const;
	llvm::Type *get_uint16_t(void) const;
	llvm::Type *get_uint32_t(void) const;
	llvm::Type *get_uint64_t(void) const;
	llvm::Type *get_float32_t(void) const;
	llvm::Type *get_float64_t(void) const;
	llvm::Type *get_str_t(void) const;
	llvm::Type *get_char_t(void) const;
	llvm::Type *get_bool_t(void) const;

	bool has_terminator(void) const;
	static bool has_terminator(llvm::BasicBlock *bb);


	global_context &global_ctx;
	bz::vector<std::pair<ast::decl_variable const *, llvm::Value *>> vars;
	bz::vector<std::pair<ast::function_body const *, llvm::Function *>> funcs;
	llvm::LLVMContext llvm_context;
	llvm::IRBuilder<> builder;
	llvm::Module module;
	llvm::Function *current_function;
	llvm::Type *built_in_types[static_cast<int>(ast::type_info::type_kind::bool_) + 1];
};

} // namespace ctx

#endif // CTX_BITCODE_CONTEXT_H
