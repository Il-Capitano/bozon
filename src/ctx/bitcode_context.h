#ifndef CTX_BITCODE_CONTEXT_H
#define CTX_BITCODE_CONTEXT_H

#include "core.h"

#include "ast/typespec.h"
#include "ast/expression.h"
#include "ast/statement.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Value.h>
#include <unordered_map>

namespace ctx
{

struct global_context;

struct bitcode_context
{
	bitcode_context(global_context &_global_ctx);

	llvm::Value *get_variable(ast::decl_variable const *var_decl) const;
	void add_variable(ast::decl_variable const *var_decl, llvm::Value *val);

	llvm::Function *get_function(ast::function_body const *func_body) const;

	llvm::LLVMContext &get_llvm_context(void) const noexcept;
	llvm::Module &get_module(void) const noexcept;

	llvm::BasicBlock *add_basic_block(bz::u8string_view name);

	llvm::Type *get_built_in_type(uint32_t kind) const;
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

	std::unordered_map<ast::decl_variable const *, llvm::Value    *> vars_;
	std::unordered_map<ast::type_info     const *, llvm::Type     *> types_;

	ast::function_body const *current_function;

	llvm::IRBuilder<> builder;
};

} // namespace ctx

#endif // CTX_BITCODE_CONTEXT_H
