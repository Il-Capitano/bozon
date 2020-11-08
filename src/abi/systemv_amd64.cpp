#include "platform_function_call.h"

namespace abi
{

template<>
pass_kind get_pass_kind<platform_abi::systemv_amd64>(
	llvm::Type *t,
	ctx::bitcode_context &context
)
{
	size_t const register_size = 8;
	bz_assert(context.get_register_size() == register_size);
	switch (t->getTypeID())
	{
	case llvm::Type::TypeID::IntegerTyID:
	{
		bz_assert(static_cast<llvm::IntegerType *>(t)->getBitWidth() <= 64);
		return pass_kind::value;
	}
	case llvm::Type::TypeID::PointerTyID:
	case llvm::Type::TypeID::VoidTyID:
	case llvm::Type::TypeID::DoubleTyID:
	case llvm::Type::TypeID::FloatTyID:
		return pass_kind::value;
	case llvm::Type::ArrayTyID:
	case llvm::Type::TypeID::StructTyID:
	{
		auto const size = context.get_size(t);
		if (size <= register_size)
		{
			return pass_kind::one_register;
		}
		else if (size <= 2 * register_size)
		{
			return pass_kind::two_registers;
		}
		else
		{
			return pass_kind::reference;
		}
	}

	default:
		bz_unreachable;
	}
}

template<>
llvm::Type *get_one_register_type<platform_abi::systemv_amd64>(
	llvm::Type *t,
	ctx::bitcode_context &context
)
{
	size_t const register_size = 8;
	bz_assert(context.get_register_size() == register_size);
	switch (t->getTypeID())
	{
	case llvm::Type::ArrayTyID:
	{
		auto const element_type  = t->getArrayElementType();
		auto const element_count = t->getArrayNumElements();
		if (element_type->isFloatTy())
		{
			bz_assert(element_count == 1 || element_count == 2);
			if (element_count == 1)
			{
				return element_type;
			}
			else
			{
				llvm::FixedVectorType::get(element_type, element_count);
			}
		}
		else if (element_type->isDoubleTy())
		{
			bz_assert(element_count == 1);
			return element_type;
		}
		else if (element_type->isPointerTy())
		{
			bz_assert(element_count == 1);
			return element_type;
		}

		auto const size = context.get_size(t);
		bz_assert(size <= register_size);
		return llvm::IntegerType::get(context.get_llvm_context(), size * 8);
	}
	case llvm::Type::TypeID::StructTyID:
	{
		auto const struct_t = static_cast<llvm::StructType *>(t);
		auto const elem_count = struct_t->getNumElements();
		auto const get_pass_type = [&context](llvm::Type *type) -> llvm::Type * {
			auto const pass_kind = get_pass_kind<platform_abi::systemv_amd64>(type, context);
			switch (pass_kind)
			{
			case pass_kind::value:
				return type;
			case pass_kind::one_register:
				return get_one_register_type<platform_abi::systemv_amd64>(type, context);
			default:
				bz_unreachable;
			}
		};
		if (elem_count == 1)
		{
			// { T } gets reduced to T
			auto const elem_type = struct_t->getElementType(0);
			return get_pass_type(elem_type);
		}
		else if (elem_count == 2)
		{
			// the only special case here is { float, float } should become <2 x float>
			// this applies to { { float }, float } too
			auto const first_elem_pass_type  = get_pass_type(struct_t->getElementType(0));
			auto const second_elem_pass_type = get_pass_type(struct_t->getElementType(1));
			if (first_elem_pass_type->isFloatTy() && second_elem_pass_type->isFloatTy())
			{
				return llvm::FixedVectorType::get(first_elem_pass_type, 2);
			}
		}

		auto const size = context.get_size(t);
		return llvm::IntegerType::get(context.get_llvm_context(), size * 8);
	}

	default:
		bz_unreachable;
	}
}

template<>
std::pair<llvm::Type *, llvm::Type *> get_two_register_types<platform_abi::systemv_amd64>(
	llvm::Type *t,
	ctx::bitcode_context &context
)
{
	bz_unreachable;
}

} // namespace abi
