#include "platform_function_call.h"

namespace abi
{

template<>
pass_kind get_pass_kind<platform_abi::generic>(
	llvm::Type *t,
	ctx::bitcode_context &context
)
{
	return get_pass_kind<platform_abi::systemv_amd64>(t, context);
}

template<>
llvm::Type *get_int_cast_type<platform_abi::generic>(
	llvm::Type *t,
	ctx::bitcode_context &context
)
{
	return get_int_cast_type<platform_abi::systemv_amd64>(t, context);
}

} // namespace abi
