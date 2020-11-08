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

static void get_types_helper(llvm::Type *t, bz::vector<llvm::Type *> &types)
{
	switch (t->getTypeID())
	{
	case llvm::Type::TypeID::ArrayTyID:
	{
		auto const elem_type = t->getArrayElementType();
		auto const elem_count = t->getArrayNumElements();
		for (unsigned i = 0; i < elem_count; ++i)
		{
			get_types_helper(elem_type, types);
		}
		break;
	}
	case llvm::Type::TypeID::StructTyID:
	{
		for (auto const elem_type : static_cast<llvm::StructType *>(t)->elements())
		{
			get_types_helper(elem_type, types);
		}
		break;
	}
	default:
		types.push_back(t);
		break;
	}
}

static bz::vector<llvm::Type *> get_types(llvm::Type *t)
{
	bz::vector<llvm::Type *> result;
	get_types_helper(t, result);
	return result;
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
	case llvm::Type::TypeID::StructTyID:
	{
		auto const contained_types = get_types(t);
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
		if (contained_types.size() == 1)
		{
			// { T } gets reduced to T
			return get_pass_type(contained_types[0]);
		}
		else if (contained_types.size() == 2)
		{
			// the only special case here is { float, float } should become <2 x float>
			// this applies to { { float }, float } too
			if (contained_types[0]->isFloatTy() && contained_types[1]->isFloatTy())
			{
				return llvm::FixedVectorType::get(contained_types[0], 2);
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
	size_t const register_size = 8;
	bz_assert(context.get_register_size() == register_size);
	std::pair<llvm::Type *, llvm::Type *> result{};
	auto const size = context.get_size(t);
	auto const contained_types = get_types(t);
	bz_assert(contained_types.size() > 1);
	auto const first_type = contained_types.front();
	auto const last_type = contained_types.back();
	if (first_type->isPointerTy() || first_type->isDoubleTy())
	{
		result.first = first_type;
	}
	if (last_type->isPointerTy() || last_type->isDoubleTy())
	{
		result.second = last_type;
	}

	if (result.first != nullptr && result.second != nullptr)
	{
		bz_assert(contained_types.size() == 2);
		return result;
	}
	else
	{
		bz_unreachable;
	}
}

} // namespace abi
