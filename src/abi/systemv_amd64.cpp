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
	{
		auto const size = context.get_size(t);
		if (size > 2 * register_size)
		{
			return pass_kind::reference;
		}

		auto const elem_type = t->getArrayElementType();
		auto const elem_pass_kind = get_pass_kind<platform_abi::systemv_amd64>(elem_type, context);
		bz_assert(elem_pass_kind != pass_kind::reference);

		auto const elem_count = t->getArrayNumElements();
		auto const elem_size = context.get_size(elem_type);
		if (elem_size > register_size)
		{
			bz_assert(elem_count == 1);
			return elem_pass_kind;
		}
		else if (elem_size == register_size)
		{
			bz_assert(elem_count == 1 || elem_count == 2);
			return elem_pass_kind;
		}
		else
		{
			return pass_kind::int_cast;
		}
	}
	case llvm::Type::TypeID::StructTyID:
	{
		auto const size = context.get_size(t);
		if (size > 2 * register_size)
		{
			return pass_kind::reference;
		}

		if (t->getStructNumElements() > 2)
		{
			return pass_kind::int_cast;
		}
		for (auto const elem_type : static_cast<llvm::StructType *>(t)->elements())
		{
			auto const elem_pass_kind = get_pass_kind<platform_abi::systemv_amd64>(elem_type, context);
			if (elem_pass_kind == pass_kind::int_cast)
			{
				return pass_kind::int_cast;
			}
		}
		return pass_kind::value;
	}

	default:
		bz_unreachable;
	}
}

template<>
llvm::Type *get_int_cast_type<platform_abi::systemv_amd64>(
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
		auto const size = context.get_size(t);
		bz_assert(size <= 2 * register_size);
		switch (size)
		{
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
		case 8:
			return llvm::IntegerType::get(context.get_llvm_context(), size * 8);
		case 9:
		case 10:
		case 11:
		case 12:
		case 13:
		case 14:
		case 15:
		case 16:
			return llvm::StructType::get(
				context.get_int64_t(),
				llvm::IntegerType::get(context.get_llvm_context(), (size - 8) * 8)
			);
		default:
			bz_unreachable;
		}
	}
	case llvm::Type::TypeID::StructTyID:
	{
		auto const size = context.get_size(t);
		auto const struct_t = static_cast<llvm::StructType *>(t);
		if (size <= 8)
		{
			return llvm::IntegerType::get(context.get_llvm_context(), size * 8);
		}

		auto const first_type = struct_t->elements().front();
		auto const last_type  = struct_t->elements().back();
		bz_assert(!(first_type->isPointerTy() && last_type->isPointerTy()));

		if (first_type->isPointerTy())
		{
			return llvm::StructType::get(
				first_type,
				llvm::IntegerType::get(context.get_llvm_context(), (size - 8) * 8)
			);
		}
		else if (last_type->isPointerTy())
		{
			return llvm::StructType::get(
				llvm::IntegerType::get(context.get_llvm_context(), (size - 8) * 8),
				last_type
			);
		}
		else
		{
			return llvm::StructType::get(
				context.get_int64_t(),
				llvm::IntegerType::get(context.get_llvm_context(), (size - 8) * 8)
			);
		}
	}

	default:
		bz_unreachable;
	}
}

} // namespace abi
