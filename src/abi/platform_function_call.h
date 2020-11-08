#ifndef ABI_PLATFORM_FUNCTION_CALL_H
#define ABI_PLATFORM_FUNCTION_CALL_H

#include "core.h"
#include "platform_abi.h"
#include "ctx/bitcode_context.h"

namespace abi
{

enum class pass_kind
{
	value,
	reference,
	one_register,
	two_registers,
};

template<platform_abi abi>
pass_kind get_pass_kind(llvm::Type *t, ctx::bitcode_context &context);

template<>
pass_kind get_pass_kind<platform_abi::generic>(llvm::Type *t, ctx::bitcode_context &context);
template<>
pass_kind get_pass_kind<platform_abi::microsoft_x64>(llvm::Type *t, ctx::bitcode_context &context);
template<>
pass_kind get_pass_kind<platform_abi::systemv_amd64>(llvm::Type *t, ctx::bitcode_context &context);


template<platform_abi abi>
llvm::Type *get_one_register_type(llvm::Type *t, ctx::bitcode_context &context);

template<>
llvm::Type *get_one_register_type<platform_abi::generic>(llvm::Type *t, ctx::bitcode_context &context);
template<>
llvm::Type *get_one_register_type<platform_abi::microsoft_x64>(llvm::Type *t, ctx::bitcode_context &context);
template<>
llvm::Type *get_one_register_type<platform_abi::systemv_amd64>(llvm::Type *t, ctx::bitcode_context &context);


template<platform_abi abi>
std::pair<llvm::Type *, llvm::Type *> get_two_register_types(llvm::Type *t, ctx::bitcode_context &context);

template<>
std::pair<llvm::Type *, llvm::Type *> get_two_register_types<platform_abi::generic>(llvm::Type *t, ctx::bitcode_context &context);
template<>
std::pair<llvm::Type *, llvm::Type *> get_two_register_types<platform_abi::microsoft_x64>(llvm::Type *t, ctx::bitcode_context &context);
template<>
std::pair<llvm::Type *, llvm::Type *> get_two_register_types<platform_abi::systemv_amd64>(llvm::Type *t, ctx::bitcode_context &context);

/*
template<platform_abi abi>
bool pass_by_reference(llvm::Type *t, ctx::bitcode_context &context);

template<>
bool pass_by_reference<platform_abi::generic>(llvm::Type *t, ctx::bitcode_context &context);
template<>
bool pass_by_reference<platform_abi::microsoft_x64>(llvm::Type *t, ctx::bitcode_context &context);
template<>
bool pass_by_reference<platform_abi::systemv_amd64>(llvm::Type *t, ctx::bitcode_context &context);


template<platform_abi abi>
bool pass_as_int_cast(llvm::Type *t, ctx::bitcode_context &context);

template<>
bool pass_as_int_cast<platform_abi::generic>(llvm::Type *t, ctx::bitcode_context &context);
template<>
bool pass_as_int_cast<platform_abi::microsoft_x64>(llvm::Type *t, ctx::bitcode_context &context);
template<>
bool pass_as_int_cast<platform_abi::systemv_amd64>(llvm::Type *t, ctx::bitcode_context &context);


template<platform_abi abi>
bool return_with_output_pointer(llvm::Type *t, ctx::bitcode_context &context);

template<>
bool return_with_output_pointer<platform_abi::generic>(llvm::Type *t, ctx::bitcode_context &context);
template<>
bool return_with_output_pointer<platform_abi::microsoft_x64>(llvm::Type *t, ctx::bitcode_context &context);
template<>
bool return_with_output_pointer<platform_abi::systemv_amd64>(llvm::Type *t, ctx::bitcode_context &context);


template<platform_abi abi>
bool return_as_int_cast(llvm::Type *t, ctx::bitcode_context &context);

template<>
bool return_as_int_cast<platform_abi::generic>(llvm::Type *t, ctx::bitcode_context &context);
template<>
bool return_as_int_cast<platform_abi::microsoft_x64>(llvm::Type *t, ctx::bitcode_context &context);
template<>
bool return_as_int_cast<platform_abi::systemv_amd64>(llvm::Type *t, ctx::bitcode_context &context);
*/

} // namespace abi

#endif // ABI_PLATFORM_FUNCTION_CALL_H
