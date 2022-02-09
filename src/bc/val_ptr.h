#ifndef BC_VAL_PTR_H
#define BC_VAL_PTR_H

#include "core.h"

#include <llvm/IR/Value.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/Debug.h>

namespace bc
{

struct value_and_type_pair
{
	llvm::Value *ptr;
	llvm::Type *type;
};

struct val_ptr
{
	enum : uintptr_t
	{
		reference,
		value,
	};
	uintptr_t kind;
	llvm::Value *val;
	llvm::Type *type;
	llvm::Value *consteval_val;

	explicit val_ptr(uintptr_t _kind, llvm::Value *_val, llvm::Type *_type, llvm::Value *_consteval_val)
		: kind(_kind), val(_val), type(_type), consteval_val(_consteval_val)
	{}

	static val_ptr get_reference(llvm::Value *ptr, llvm::Type *type, llvm::Value *consteval_val = nullptr)
	{
		bz_assert(ptr->getType()->isPointerTy());
		return val_ptr(reference, ptr, type, consteval_val);
	}

	static val_ptr get_value(llvm::Value *val, llvm::Value *consteval_val = nullptr)
	{
		return val_ptr(value, val, val->getType(), consteval_val);
	}

	static val_ptr get_none(void)
	{
		return val_ptr(0, nullptr, nullptr, nullptr);
	}

	bool has_value(void) const noexcept
	{
		return this->val != nullptr || this->consteval_val != nullptr;
	}

	llvm::Value *get_value(llvm::IRBuilder<> &builder) const
	{
		if (this->consteval_val)
		{
			return this->consteval_val;
		}

		if (this->val == nullptr)
		{
			return this->val;
		}

		if (this->kind == reference)
		{
			auto const loaded_val = builder.CreateLoad(this->type, this->val, "load_tmp");
			return loaded_val;
		}
		else
		{
			return this->val;
		}
	}

	value_and_type_pair get_value_and_type(llvm::IRBuilder<> &builder) const
	{
		return { this->get_value(builder), this->get_type() };
	}

	llvm::Type *get_type(void) const
	{
		// bz_assert(this->type != nullptr);
		return this->type;
	}
};

} // namespace bc

#endif // BC_VAL_PTR_H
