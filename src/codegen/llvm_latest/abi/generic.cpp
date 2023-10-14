#include "platform_function_call.h"

namespace codegen::llvm_latest::abi
{

static constexpr bz::array pass_by_reference_attributes_generic = {
	llvm::Attribute::NoAlias,
	llvm::Attribute::NoCapture,
	llvm::Attribute::NonNull,
};

template<>
bz::array_view<llvm::Attribute::AttrKind const> get_pass_by_reference_attributes<platform_abi::generic>(void)
{
	return pass_by_reference_attributes_generic;
}

template<>
pass_kind get_pass_kind<platform_abi::generic>(
	llvm::Type *t,
	llvm::DataLayout const &data_layout,
	llvm::LLVMContext &
)
{
	if (t->isVoidTy())
	{
		return pass_kind::value;
	}

	auto const size = data_layout.getTypeAllocSize(t);
	auto const register_size = data_layout.getPointerSize();
	if (t->isIntegerTy() || t->isFloatingPointTy() || t->isPointerTy())
	{
		return pass_kind::value;
	}
	else if (size > register_size)
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
	llvm::DataLayout const &data_layout,
	llvm::LLVMContext &context
)
{
	auto const size = data_layout.getTypeAllocSize(t);
	return llvm::IntegerType::get(context, size * 8);
}

template<>
std::pair<llvm::Type *, llvm::Type *> get_two_register_types<platform_abi::generic>(
	[[maybe_unused]] llvm::Type *t,
	[[maybe_unused]] llvm::DataLayout const &data_layout,
	[[maybe_unused]] llvm::LLVMContext &context
)
{
	bz_unreachable;
}

} // namespace codegen::llvm_latest::abi
