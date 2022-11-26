#include "executor_context.h"

namespace comptime
{

void executor_context::set_current_instruction_value(instruction_value value)
{
	this->instruction_values[this->current_instruction_index] = value;
}

instruction_value executor_context::get_instruction_value(instructions::arg_t inst_index)
{
	return this->instruction_values[inst_index];
}

void executor_context::do_jump(instruction_index dest)
{
	bz_assert(dest.index < this->instructions.size());
	this->next_instruction_index = dest.index;
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
	if (this->next_instruction_index.has_value())
	{
		bz_assert(this->next_instruction_index.get() < this->instructions.size());
		this->current_instruction_index = this->next_instruction_index.get();
	}
	else if (this->returned)
	{
		bz_unreachable;
	}
	else
	{
		bz_assert(!this->instructions[this->current_instruction_index].is_terminator());
		this->current_instruction_index += 1;
	}
}

} // namespace comptime
