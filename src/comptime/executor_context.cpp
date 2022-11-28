#include "executor_context.h"

namespace comptime
{

void executor_context::set_current_instruction_value(instruction_value value)
{
	*this->current_instruction_value = value;
}

instruction_value executor_context::get_instruction_value(instruction_value_index index)
{
	return this->instruction_values[index.index];
}

void executor_context::do_jump(instruction_index dest)
{
	bz_assert(dest.index < this->instructions.size());
	this->next_instruction = &this->instructions[dest.index];
}

void executor_context::do_ret(instruction_value value)
{
	this->ret_value = value;
	this->returned = true;
}

void executor_context::do_ret_void(void)
{
	this->returned = true;
}

void executor_context::advance(void)
{
	if (this->next_instruction != nullptr)
	{
		auto const next_instruction_index = this->next_instruction - this->instructions.data();
		this->current_instruction = this->next_instruction;
		this->current_instruction_value = this->instruction_values.data() + next_instruction_index + this->alloca_offset;
		this->next_instruction = nullptr;
	}
	else if (this->returned)
	{
		bz_unreachable;
	}
	else
	{
		bz_assert(!this->current_instruction->is_terminator());
		this->current_instruction += 1;
	}
}

} // namespace comptime
