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
	instruction_value ret_value;
	bool returned;

	uint8_t *get_memory(ptr_t ptr, size_t size);
	ptr_t do_alloca(size_t size, size_t align);

	void set_current_instruction_value(instruction_value value);
	instruction_value get_instruction_value(instructions::arg_t inst_index);

	void do_jump(uint32_t next_bb_index);
	void do_ret(instruction_value value);
	void do_ret_void(void);
	void report_error(uint32_t error_index);

	void advance(void);
};

} // namespace comptime

#endif // COMPTIME_EXECUTOR_CONTEXT_H
