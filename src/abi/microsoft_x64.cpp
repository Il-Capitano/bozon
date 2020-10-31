#include "platform_function_call.h"

namespace abi
{

template<>
pass_kind get_pass_kind<platform_abi::microsoft_x64>(llvm::Type *t, ctx::bitcode_context &context)
{
	if (t->isVoidTy())
	{
		return pass_kind::value;
	}

	auto const size = context.get_size(t);
	size_t const register_size = 8;
	bz_assert(context.get_register_size() == register_size);
	if (size > register_size || (size != 1 && size != 2 && size != 4 && size != 8))
	{
		return pass_kind::reference;
	}
	else if (t->isIntegerTy() || t->isFloatingPointTy() || t->isPointerTy())
	{
		return pass_kind::value;
	}
	else
	{
		return pass_kind::int_cast;
	}
}

template<>
llvm::Type *get_int_cast_type<platform_abi::microsoft_x64>(llvm::Type *t, ctx::bitcode_context &context)
{
	auto const size = context.get_size(t);
	bz_assert(size == 1 || size == 2 || size == 4 || size == 8);
	return llvm::IntegerType::get(context.get_llvm_context(), size * 8);
}

} // namespace abi
