#ifndef BC_VAL_PTR_H
#define BC_VAL_PTR_H

#include "core.h"

#include <llvm/IR/Value.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/Debug.h>

namespace bc
{

struct val_ptr
{
	enum : uintptr_t
	{
		reference,
		value,
	};
	uintptr_t kind = 0;
	llvm::Value *val = nullptr;
	llvm::Value *consteval_val = nullptr;

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
			auto const loaded_val = builder.CreateLoad(this->val, "load_tmp");
			return loaded_val;
		}
		else
		{
			return this->val;
		}
	}

	llvm::Type *get_type(void) const
	{
		if (this->consteval_val)
		{
			return this->consteval_val->getType();
		}

		if (this->kind == reference)
		{
			auto const ptr_t = llvm::dyn_cast<llvm::PointerType>(this->val->getType());
			bz_assert(ptr_t != nullptr);
			return ptr_t->getElementType();
		}
		else
		{
			return this->val->getType();
		}
	}
};

} // namespace bc

#endif // BC_VAL_PTR_H
