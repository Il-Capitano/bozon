#ifndef ABI_PLATFORM_FUNCTION_CALL_H
#define ABI_PLATFORM_FUNCTION_CALL_H

#include "core.h"
#include "platform_abi.h"
#include "ctx/bitcode_context.h"

namespace abi
{

template<platform_abi abi>
bool pass_by_reference(llvm::Type *t, ctx::bitcode_context &context);

template<>
bool pass_by_reference<platform_abi::generic>(llvm::Type *t, ctx::bitcode_context &context);
template<>
bool pass_by_reference<platform_abi::microsoft_x64>(llvm::Type *t, ctx::bitcode_context &context);

template<platform_abi abi>
bool pass_as_int_cast(llvm::Type *t, ctx::bitcode_context &context);

template<>
bool pass_as_int_cast<platform_abi::generic>(llvm::Type *t, ctx::bitcode_context &context);
template<>
bool pass_as_int_cast<platform_abi::microsoft_x64>(llvm::Type *t, ctx::bitcode_context &context);

template<platform_abi abi>
bool return_with_output_pointer(llvm::Type *t, ctx::bitcode_context &context);

template<>
bool return_with_output_pointer<platform_abi::generic>(llvm::Type *t, ctx::bitcode_context &context);
template<>
bool return_with_output_pointer<platform_abi::microsoft_x64>(llvm::Type *t, ctx::bitcode_context &context);

template<platform_abi abi>
bool return_as_int_cast(llvm::Type *t, ctx::bitcode_context &context);

template<>
bool return_as_int_cast<platform_abi::generic>(llvm::Type *t, ctx::bitcode_context &context);
template<>
bool return_as_int_cast<platform_abi::microsoft_x64>(llvm::Type *t, ctx::bitcode_context &context);

} // namespace abi

#endif // ABI_PLATFORM_FUNCTION_CALL_H
