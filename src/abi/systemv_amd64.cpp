#include "platform_function_call.h"

namespace abi
{

template<>
pass_kind get_pass_kind<platform_abi::systemv_amd64>(llvm::Type *t, ctx::bitcode_context &context)
{
	if (t->isVoidTy())
	{
		return pass_kind::value;
	}

	auto const size = context.get_size(t);
	size_t const register_size = 8;
	bz_assert(context.get_register_size() == register_size);
	if (size > 2 * register_size)
	{
		return pass_kind::reference;
	}

	return pass_kind::value;
}

template<>
llvm::Type *get_int_cast_type<platform_abi::systemv_amd64>(
	[[maybe_unused]] llvm::Type *t,
	[[maybe_unused]] ctx::bitcode_context &context
)
{
	bz_unreachable;
}

} // namespace abi
