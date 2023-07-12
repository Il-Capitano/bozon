#ifndef CODEGEN_LLVM_LATEST_ABI_PLATFORM_FUNCTION_CALL_H
#define CODEGEN_LLVM_LATEST_ABI_PLATFORM_FUNCTION_CALL_H

#include "core.h"
#include "platform_abi.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Type.h>

namespace codegen::llvm_latest::abi
{

enum class pass_kind
{
	value,
	reference,
	one_register,
	two_registers,
	non_trivial,
};

template<platform_abi>
bz::array_view<llvm::Attribute::AttrKind const> get_pass_by_reference_attributes(void);

template<>
bz::array_view<llvm::Attribute::AttrKind const> get_pass_by_reference_attributes<platform_abi::generic>(void);
template<>
bz::array_view<llvm::Attribute::AttrKind const> get_pass_by_reference_attributes<platform_abi::microsoft_x64>(void);
template<>
bz::array_view<llvm::Attribute::AttrKind const> get_pass_by_reference_attributes<platform_abi::systemv_amd64>(void);

inline bz::array_view<llvm::Attribute::AttrKind const> get_pass_by_reference_attributes(platform_abi abi)
{
	switch (abi)
	{
	case platform_abi::generic:
		return get_pass_by_reference_attributes<platform_abi::generic>();
	case platform_abi::microsoft_x64:
		return get_pass_by_reference_attributes<platform_abi::microsoft_x64>();
	case platform_abi::systemv_amd64:
		return get_pass_by_reference_attributes<platform_abi::systemv_amd64>();
	}
}


template<platform_abi abi>
pass_kind get_pass_kind(llvm::Type *t, llvm::DataLayout const &data_layout, llvm::LLVMContext &context);

template<>
pass_kind get_pass_kind<platform_abi::generic>(llvm::Type *t, llvm::DataLayout const &data_layout, llvm::LLVMContext &context);
template<>
pass_kind get_pass_kind<platform_abi::microsoft_x64>(llvm::Type *t, llvm::DataLayout const &data_layout, llvm::LLVMContext &context);
template<>
pass_kind get_pass_kind<platform_abi::systemv_amd64>(llvm::Type *t, llvm::DataLayout const &data_layout, llvm::LLVMContext &context);

inline pass_kind get_pass_kind(platform_abi abi, llvm::Type *t, llvm::DataLayout const &data_layout, llvm::LLVMContext &context)
{
	switch (abi)
	{
	case platform_abi::generic:
		return get_pass_kind<platform_abi::generic>(t, data_layout, context);
	case platform_abi::microsoft_x64:
		return get_pass_kind<platform_abi::microsoft_x64>(t, data_layout, context);
	case platform_abi::systemv_amd64:
		return get_pass_kind<platform_abi::systemv_amd64>(t, data_layout, context);
	}
}


template<platform_abi abi>
llvm::Type *get_one_register_type(llvm::Type *t, llvm::DataLayout const &data_layout, llvm::LLVMContext &context);

template<>
llvm::Type *get_one_register_type<platform_abi::generic>(llvm::Type *t, llvm::DataLayout const &data_layout, llvm::LLVMContext &context);
template<>
llvm::Type *get_one_register_type<platform_abi::microsoft_x64>(llvm::Type *t, llvm::DataLayout const &data_layout, llvm::LLVMContext &context);
template<>
llvm::Type *get_one_register_type<platform_abi::systemv_amd64>(llvm::Type *t, llvm::DataLayout const &data_layout, llvm::LLVMContext &context);

inline llvm::Type *get_one_register_type(platform_abi abi, llvm::Type *t, llvm::DataLayout const &data_layout, llvm::LLVMContext &context)
{
	switch (abi)
	{
	case platform_abi::generic:
		return get_one_register_type<platform_abi::generic>(t, data_layout, context);
	case platform_abi::microsoft_x64:
		return get_one_register_type<platform_abi::microsoft_x64>(t, data_layout, context);
	case platform_abi::systemv_amd64:
		return get_one_register_type<platform_abi::systemv_amd64>(t, data_layout, context);
	}
}


template<platform_abi abi>
std::pair<llvm::Type *, llvm::Type *> get_two_register_types(llvm::Type *t, llvm::DataLayout const &data_layout, llvm::LLVMContext &context);

template<>
std::pair<llvm::Type *, llvm::Type *> get_two_register_types<platform_abi::generic>(llvm::Type *t, llvm::DataLayout const &data_layout, llvm::LLVMContext &context);
template<>
std::pair<llvm::Type *, llvm::Type *> get_two_register_types<platform_abi::microsoft_x64>(llvm::Type *t, llvm::DataLayout const &data_layout, llvm::LLVMContext &context);
template<>
std::pair<llvm::Type *, llvm::Type *> get_two_register_types<platform_abi::systemv_amd64>(llvm::Type *t, llvm::DataLayout const &data_layout, llvm::LLVMContext &context);

inline std::pair<llvm::Type *, llvm::Type *> get_two_register_types(platform_abi abi, llvm::Type *t, llvm::DataLayout const &data_layout, llvm::LLVMContext &context)
{
	switch (abi)
	{
	case platform_abi::generic:
		return get_two_register_types<platform_abi::generic>(t, data_layout, context);
	case platform_abi::microsoft_x64:
		return get_two_register_types<platform_abi::microsoft_x64>(t, data_layout, context);
	case platform_abi::systemv_amd64:
		return get_two_register_types<platform_abi::systemv_amd64>(t, data_layout, context);
	}
}

} // namespace codegen::llvm_latest::abi

#endif // CODEGEN_LLVM_LATEST_ABI_PLATFORM_FUNCTION_CALL_H
