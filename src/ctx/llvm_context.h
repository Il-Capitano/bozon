#ifndef CTX_LLVM_CONTEXT_H
#define CTX_LLVM_CONTEXT_H

#include "core.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Value.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/Target/TargetMachine.h>

#include "ast/statement.h"

namespace ctx
{

struct llvm_context
{
	llvm_context(global_context &global_ctx, bool &error);

	llvm::LLVMContext _llvm_context;
	llvm::Module      _module;
	llvm::Target const *_target;
	std::unique_ptr<llvm::TargetMachine> _target_machine;
	bz::optional<llvm::DataLayout>       _data_layout;
	bz::array<llvm::Type *, static_cast<int>(ast::type_info::null_t_) + 1> _llvm_builtin_types;

	llvm::DataLayout const &get_data_layout(void) const
	{
		bz_assert(this->_data_layout.has_value());
		return this->_data_layout.get();
	}

	std::string const &get_target_triple(void) const
	{
		bz_assert(this->_target_machine != nullptr);
		return this->_target_machine->getTargetTriple().str();
	}

	[[nodiscard]] bool emit_bitcode(global_context &global_ctx);
	[[nodiscard]] bool optimize(void);
	[[nodiscard]] bool emit_file(global_context &global_ctx);
	[[nodiscard]] bool emit_obj(global_context &global_ctx);
	[[nodiscard]] bool emit_asm(global_context &global_ctx);
	[[nodiscard]] bool emit_llvm_bc(global_context &global_ctx);
	[[nodiscard]] bool emit_llvm_ir(global_context &global_ctx);
};

} // namespace ctx

#endif // CTX_LLVM_CONTEXT_H
