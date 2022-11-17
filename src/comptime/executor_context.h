#ifndef COMPTIME_EXECUTOR_CONTEXT_H
#define COMPTIME_EXECUTOR_CONTEXT_H

#include "instructions.h"

namespace comptime
{

struct executor_context
{
	instruction const *current_instruction;
	function *current_function;
	ptr_t function_stack_base;

	size_t instruction_value_offset;
	basic_block const *next_bb;

	uint8_t *get_memory(ptr_t ptr, size_t size);
	ptr_t do_alloca(size_t size, size_t align);

	void set_current_instruction_value(instruction_value value);
	instruction_value get_instruction_value(instructions::arg_t inst_index);

	void do_jump(uint32_t next_bb_index);

	void advance(void);
};

} // namespace comptime

#endif // COMPTIME_EXECUTOR_CONTEXT_H
