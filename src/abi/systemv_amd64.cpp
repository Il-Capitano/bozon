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
	case llvm::Type::TypeID::ArrayTyID:
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
	switch (t->getTypeID())
	{
	case llvm::Type::ArrayTyID:
	case llvm::Type::TypeID::StructTyID:
	{
		auto const contained_types = get_types(t);
		if (contained_types.size() == 1)
		{
			// { T } gets reduced to T
			return contained_types[0];
		}
		else if (
			contained_types.size() == 2
			&& contained_types[0]->isFloatTy()
			&& contained_types[1]->isFloatTy()
		)
		{
			// the only special case here is { float, float } should become <2 x float>
			// this applies to { { float }, float } too
#if LLVM_VERSION_MAJOR >= 11
			return llvm::FixedVectorType::get(contained_types[0], 2);
#else
			return llvm::VectorType::get(contained_types[0], 2);
#endif // llvm 11
		}

		auto const size = context.get_size(t);
		return llvm::IntegerType::get(context.get_llvm_context(), size * 8);
	}

	default:
		bz_unreachable;
	}
}

static void get_types_with_offset_helper(
	llvm::Type *t,
	bz::vector<std::pair<llvm::Type *, size_t>> &result,
	size_t current_offset,
	ctx::bitcode_context &context
)
{
	size_t const register_size = 8;
	bz_assert(context.get_register_size() == register_size);
	switch (t->getTypeID())
	{
	case llvm::Type::TypeID::ArrayTyID:
	{
		auto const elem_type = t->getArrayElementType();
		auto const elem_count = t->getArrayNumElements();
		auto const elem_size = context.get_size(elem_type);
		for ([[maybe_unused]] auto const _ : bz::range(elem_count))
		{
			get_types_with_offset_helper(elem_type, result, current_offset, context);
			current_offset += elem_size;
		}
		break;
	}
	case llvm::Type::TypeID::StructTyID:
	{
		auto const elem_count = t->getStructNumElements();
		for (auto const i : bz::range(elem_count))
		{
			get_types_with_offset_helper(
				t->getStructElementType(i),
				result,
				current_offset + context.get_offset(t, i),
				context
			);
		}
		break;
	}
	default:
		result.push_back({ t, current_offset });
		break;
	}
}

static bz::vector<std::pair<llvm::Type *, size_t>> get_types_with_offset(llvm::Type *t, ctx::bitcode_context &context)
{
	bz::vector<std::pair<llvm::Type *, size_t>> result;
	get_types_with_offset_helper(t, result, 0, context);
	return result;
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

	auto const contained_types = get_types_with_offset(t, context);

	auto const first_type_in_second_register_it = std::find_if(
		contained_types.begin(), contained_types.end(),
		[register_size](auto const &pair) { return pair.second == register_size; }
	);
	bz_assert(first_type_in_second_register_it != contained_types.end());
	auto const first_register_types  = bz::array_view(contained_types.begin(), first_type_in_second_register_it);
	auto const second_register_types = bz::array_view(first_type_in_second_register_it, contained_types.end());

	if (first_register_types.size() == 1)
	{
		// special case for single types (e.g. pointers or float64)
		result.first = first_register_types[0].first;
	}
	else if (
		first_register_types.size() == 2
		&& first_register_types[0].first->isFloatTy()
		&& first_register_types[1].first->isFloatTy()
	)
	{
		// special case for 2 float32's
#if LLVM_VERSION_MAJOR >= 11
		result.first = llvm::FixedVectorType::get(first_register_types[0].first, 2);
#else
		result.first = llvm::VectorType::get(first_register_types[0].first, 2);
#endif // llvm 11
	}
	else
	{
		// here, we don't care about how big the remaining types are, it's always going to be int64
		// clang does the same thing, e.g [int16, int16, int64] would become int64, int64 when
		// passing them as arguments
		result.first = context.get_int64_t();
	}

	if (second_register_types.size() == 1)
	{
		// special case for single types (e.g. pointers or float64)
		result.second = second_register_types[0].first;
	}
	else if (
		second_register_types.size() == 2
		&& second_register_types[0].first->isFloatTy()
		&& second_register_types[1].first->isFloatTy()
	)
	{
		// special case for 2 float32's
#if LLVM_VERSION_MAJOR >= 11
		result.second = llvm::FixedVectorType::get(second_register_types[0].first, 2);
#else
		result.second = llvm::VectorType::get(second_register_types[0].first, 2);
#endif // llvm 11
	}
	else
	{
		auto const max_align = contained_types
			.member<&decltype(contained_types)::value_type::first>()
			.transform([&context](auto const type) { return context.get_align(type); })
			.max(0);
		auto const [last_type, last_offset] = second_register_types.back();
		auto const last_type_size = context.get_size(last_type);
		auto const last_type_end_offset = last_offset + last_type_size;
		auto const last_type_end_aligned_offset = last_type_end_offset + max_align - ((last_type_end_offset - 1) % max_align + 1);
		auto const second_register_int_size = last_type_end_aligned_offset - register_size;
		result.second = llvm::IntegerType::get(context.get_llvm_context(), second_register_int_size * 8);
	}

	return result;
}

} // namespace abi
