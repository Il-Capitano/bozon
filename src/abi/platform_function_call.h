#ifndef ABI_PLATFORM_FUNCTION_CALL_H
#define ABI_PLATFORM_FUNCTION_CALL_H

#include "core.h"
#include "platform_abi.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Type.h>

namespace abi
{

enum class pass_kind
{
	value,
	reference,
	one_register,
	two_registers,
	non_trivial,
};

template<platform_abi abi>
pass_kind get_pass_kind(llvm::Type *t, llvm::DataLayout &data_layout, llvm::LLVMContext &context);

template<>
pass_kind get_pass_kind<platform_abi::generic>(llvm::Type *t, llvm::DataLayout &data_layout, llvm::LLVMContext &context);
template<>
pass_kind get_pass_kind<platform_abi::microsoft_x64>(llvm::Type *t, llvm::DataLayout &data_layout, llvm::LLVMContext &context);
template<>
pass_kind get_pass_kind<platform_abi::systemv_amd64>(llvm::Type *t, llvm::DataLayout &data_layout, llvm::LLVMContext &context);


template<platform_abi abi>
llvm::Type *get_one_register_type(llvm::Type *t, llvm::DataLayout &data_layout, llvm::LLVMContext &context);

template<>
llvm::Type *get_one_register_type<platform_abi::generic>(llvm::Type *t, llvm::DataLayout &data_layout, llvm::LLVMContext &context);
template<>
llvm::Type *get_one_register_type<platform_abi::microsoft_x64>(llvm::Type *t, llvm::DataLayout &data_layout, llvm::LLVMContext &context);
template<>
llvm::Type *get_one_register_type<platform_abi::systemv_amd64>(llvm::Type *t, llvm::DataLayout &data_layout, llvm::LLVMContext &context);


template<platform_abi abi>
std::pair<llvm::Type *, llvm::Type *> get_two_register_types(llvm::Type *t, llvm::DataLayout &data_layout, llvm::LLVMContext &context);

template<>
std::pair<llvm::Type *, llvm::Type *> get_two_register_types<platform_abi::generic>(llvm::Type *t, llvm::DataLayout &data_layout, llvm::LLVMContext &context);
template<>
std::pair<llvm::Type *, llvm::Type *> get_two_register_types<platform_abi::microsoft_x64>(llvm::Type *t, llvm::DataLayout &data_layout, llvm::LLVMContext &context);
template<>
std::pair<llvm::Type *, llvm::Type *> get_two_register_types<platform_abi::systemv_amd64>(llvm::Type *t, llvm::DataLayout &data_layout, llvm::LLVMContext &context);

} // namespace abi

#endif // ABI_PLATFORM_FUNCTION_CALL_H
