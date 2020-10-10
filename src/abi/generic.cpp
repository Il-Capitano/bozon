#include "platform_function_call.h"

namespace abi
{

template<>
bool pass_by_reference<platform_abi::generic>(
	[[maybe_unused]] llvm::Type *t,
	[[maybe_unused]] ctx::bitcode_context &context
)
{
	return false;
}

template<>
bool pass_as_int_cast<platform_abi::generic>(
	[[maybe_unused]] llvm::Type *t,
	[[maybe_unused]] ctx::bitcode_context &context
)
{
	return false;
}

template<>
bool return_with_output_pointer<platform_abi::generic>(
	[[maybe_unused]] llvm::Type *t,
	[[maybe_unused]] ctx::bitcode_context &context
)
{
	return false;
}

template<>
bool return_as_int_cast<platform_abi::generic>(
	[[maybe_unused]] llvm::Type *t,
	[[maybe_unused]] ctx::bitcode_context &context
)
{
	return false;
}

} // namespace abi
