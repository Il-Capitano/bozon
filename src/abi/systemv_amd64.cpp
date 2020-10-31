#include "platform_function_call.h"

namespace abi
{

template<>
pass_kind get_pass_kind<platform_abi::systemv_amd64>(
	[[maybe_unused]] llvm::Type *t,
	[[maybe_unused]] ctx::bitcode_context &context
)
{
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
