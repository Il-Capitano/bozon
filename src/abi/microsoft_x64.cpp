#include "platform_function_call.h"

namespace abi
{

template<>
bool pass_by_reference<platform_abi::microsoft_x64>(
	llvm::Type *t,
	ctx::bitcode_context &context
)
{
	auto const size = context.get_size(t);
	size_t const register_size = 8;
	bz_assert(context.get_register_size() == register_size);
	return size > register_size || (size != 1 && size != 2 && size != 4 && size != 8);
}

template<>
bool pass_as_int_cast<platform_abi::microsoft_x64>(
	llvm::Type *t,
	ctx::bitcode_context &context
)
{
	bz_assert(!pass_by_reference<platform_abi::microsoft_x64>(t, context));
	return !t->isVoidTy() && !t->isIntegerTy() && !t->isFloatingPointTy() && !t->isPointerTy();
}

template<>
bool return_with_output_pointer<platform_abi::microsoft_x64>(
	llvm::Type *t,
	ctx::bitcode_context &context
)
{
	auto const size = context.get_size(t);
	size_t const register_size = 8;
	bz_assert(context.get_register_size() == register_size);
	return size > register_size || (size != 1 && size != 2 && size != 4 && size != 8);
}

template<>
bool return_as_int_cast<platform_abi::microsoft_x64>(
	llvm::Type *t,
	ctx::bitcode_context &context
)
{
	bz_assert(!return_with_output_pointer<platform_abi::microsoft_x64>(t, context));
	return !t->isVoidTy() && !t->isIntegerTy() && !t->isFloatingPointTy() && !t->isPointerTy();
}

} // namespace abi
