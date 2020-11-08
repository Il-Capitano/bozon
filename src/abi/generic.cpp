#include "platform_function_call.h"

namespace abi
{

template<>
pass_kind get_pass_kind<platform_abi::generic>(
	llvm::Type *t,
	ctx::bitcode_context &context
)
{
	auto const size = context.get_size(t);
	auto const register_size = context.get_register_size();
	if (size > register_size)
	{
		return pass_kind::reference;
	}
	else
	{
		return pass_kind::one_register;
	}
}

template<>
llvm::Type *get_one_register_type<platform_abi::generic>(
	llvm::Type *t,
	ctx::bitcode_context &context
)
{
	auto const size = context.get_size(t);
	return llvm::IntegerType::get(context.get_llvm_context(), size * 8);
}

template<>
std::pair<llvm::Type *, llvm::Type *> get_two_register_types<platform_abi::generic>(
	[[maybe_unused]] llvm::Type *t,
	[[maybe_unused]] ctx::bitcode_context &context
)
{
	bz_unreachable;
}

} // namespace abi
