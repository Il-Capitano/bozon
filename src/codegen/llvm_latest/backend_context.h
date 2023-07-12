#ifndef CODEGEN_LLVM_LATEST_LLVM_CONTEXT_H
#define CODEGEN_LLVM_LATEST_LLVM_CONTEXT_H

#include "core.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Value.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/Target/TargetMachine.h>

#include "ast/statement.h"
#include "abi/platform_abi.h"

namespace codegen::llvm_latest
{

enum class output_code_kind
{
	obj,
	asm_,
	llvm_bc,
	llvm_ir,
	null,
};

struct backend_context
{
	backend_context(ctx::global_context &global_ctx, bz::u8string_view target_triple, output_code_kind output_code, bool &error);

	llvm::LLVMContext _llvm_context;
	llvm::Module      _module;
	llvm::Target const *_target;
	std::unique_ptr<llvm::TargetMachine> _target_machine;
	bz::optional<llvm::DataLayout>       _data_layout;
	bz::array<llvm::Type *, static_cast<int>(ast::type_info::null_t_) + 1> _llvm_builtin_types;
	abi::platform_abi _platform_abi;
	output_code_kind _output_code;

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

	[[nodiscard]] bool emit_bitcode(ctx::global_context &global_ctx);
	[[nodiscard]] bool optimize(void);
	[[nodiscard]] bool emit_file(ctx::global_context &global_ctx);
	[[nodiscard]] bool emit_obj(ctx::global_context &global_ctx);
	[[nodiscard]] bool emit_asm(ctx::global_context &global_ctx);
	[[nodiscard]] bool emit_llvm_bc(ctx::global_context &global_ctx);
	[[nodiscard]] bool emit_llvm_ir(ctx::global_context &global_ctx);
};

} // namespace codegen::llvm_latest

#endif // CODEGEN_LLVM_LATEST_LLVM_CONTEXT_H
