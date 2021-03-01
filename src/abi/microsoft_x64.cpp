#include "platform_function_call.h"

namespace abi
{

template<>
pass_kind get_pass_kind<platform_abi::microsoft_x64>(
	llvm::Type *t,
	llvm::DataLayout &data_layout,
	llvm::LLVMContext &context
)
{
	if (t->isVoidTy())
	{
		return pass_kind::value;
	}

	auto const size = data_layout.getTypeAllocSize(t);
	size_t const register_size = 8;
	bz_assert(data_layout.getTypeAllocSize(data_layout.getIntPtrType(context)) == register_size);
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
		return pass_kind::one_register;
	}
}

template<>
llvm::Type *get_one_register_type<platform_abi::microsoft_x64>(
	llvm::Type *t,
	llvm::DataLayout &data_layout,
	llvm::LLVMContext &context
)
{
	auto const size = data_layout.getTypeAllocSize(t);
	bz_assert(size == 1 || size == 2 || size == 4 || size == 8);
	return llvm::IntegerType::get(context, size * 8);
}

template<>
std::pair<llvm::Type *, llvm::Type *> get_two_register_types<platform_abi::microsoft_x64>(
	[[maybe_unused]] llvm::Type *t,
	[[maybe_unused]] llvm::DataLayout &data_layout,
	[[maybe_unused]] llvm::LLVMContext &context
)
{
	bz_unreachable;
}

} // namespace abi
