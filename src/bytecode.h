#ifndef BYTECODE_H
#define BYTECODE_H

#include "core.h"

struct stack
{
	bz::vector<uint8_t> stack;

	void change_size(int64_t amount);

	template<typename T>
	T value_at_offset(int64_t offset)
	{
		assert(offset < 0);
		assert(static_cast<size_t>(-offset) <= this->stack.size());
		auto const end_ptr = &*this->stack.end();
		auto const ptr = end_ptr + offset;
		assert((int64_t)ptr % alignof(T) == 0);
		return *(T *)ptr;
	}

	template<typename T>
	void store_at_offset(int64_t offset, T value)
	{
		assert(offset < 0);
		assert(static_cast<size_t>(-offset) <= this->stack.size());
		auto const end_ptr = &*this->stack.end();
		auto const ptr = end_ptr + offset;
		assert((int64_t)ptr % alignof(T) == 0);
		*(T *)ptr = value;
	}
};

namespace bytecode
{

enum class type_kind
{
	int8, int16, int32, int64,
	uint8, uint16, uint32, uint64,
	float32, float64,
};

struct move_stack_pointer
{
	int64_t amount;

	void execute(stack &stack);
};

struct add
{
	int64_t res_offset;
	int64_t lhs_offset;
	int64_t rhs_offset;
	type_kind type;

	void execute(stack &stack);
};

struct sub
{
	int64_t res_offset;
	int64_t lhs_offset;
	int64_t rhs_offset;
	type_kind type;

	void execute(stack &stack);
};

struct mul
{
	int64_t res_offset;
	int64_t lhs_offset;
	int64_t rhs_offset;
	type_kind type;

	void execute(stack &stack);
};

struct div
{
	int64_t res_offset;
	int64_t lhs_offset;
	int64_t rhs_offset;
	type_kind type;

	void execute(stack &stack);
};

struct mod
{
	int64_t res_offset;
	int64_t lhs_offset;
	int64_t rhs_offset;
	type_kind type;

	void execute(stack &stack);
};

struct cast
{
	int64_t res_offset;
	int64_t arg_offset;
	type_kind src_type;
	type_kind dest_type;

	void execute(stack &stack);
};

struct mov
{
	int64_t offset;
	type_kind type;
	uint64_t value;

	void execute(stack &stack);
};

} // namespace bytecode


using instruction = bz::variant<
	bytecode::move_stack_pointer,
	bytecode::add,
	bytecode::sub,
	bytecode::mul,
	bytecode::div,
	bytecode::mod,
	bytecode::cast,
	bytecode::mov
>;

void execute_new(bz::vector<instruction> const &instructions);

#endif // BYTECODE_H
