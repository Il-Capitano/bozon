#include "executor_context.h"

namespace comptime
{

uint8_t *executor_context::get_memory(ptr_t ptr, size_t size)
{
	bz_unreachable;
}

ptr_t executor_context::do_alloca(size_t size, size_t align)
{
	bz_unreachable;
}

void executor_context::set_current_instruction_value(instruction_value value)
{
	this->current_function->instruction_values[this->instruction_value_offset] = value;
}

instruction_value executor_context::get_instruction_value(instructions::arg_t inst_index)
{
	return this->current_function->instruction_values[inst_index];
}

void executor_context::do_jump(uint32_t next_bb_index)
{
	bz_assert(next_bb_index < this->current_function->blocks.size());
	this->next_bb = &this->current_function->blocks[next_bb_index];
}

void executor_context::advance(void)
{
	if (this->next_bb != nullptr)
	{
		bz_assert(this->next_bb >= this->current_function->blocks.data() && this->next_bb < this->current_function->blocks.data_end());
		this->current_instruction = this->next_bb->instructions.data();
		this->instruction_value_offset = this->next_bb->instruction_value_offset;
	}
	else
	{
		bz_assert(!this->current_instruction->is_terminator());
		++this->current_instruction;
	}
}

} // namespace comptime
