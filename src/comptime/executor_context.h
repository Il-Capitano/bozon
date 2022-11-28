#ifndef COMPTIME_EXECUTOR_CONTEXT_H
#define COMPTIME_EXECUTOR_CONTEXT_H

#include "instructions.h"
#include "ctx/warnings.h"

namespace comptime
{

struct executor_context
{
	ptr_t function_stack_base;
	function *current_function;
	bz::array_view<instruction const> instructions;
	bz::array_view<instruction_value> instruction_values;

	uint32_t alloca_offset;
	instruction const *current_instruction;
	instruction_value *current_instruction_value;
	instruction const *next_instruction;

	instruction_value ret_value;
	bool returned;

	uint8_t *get_memory(ptr_t ptr, size_t size);

	void set_current_instruction_value(instruction_value value);
	instruction_value get_instruction_value(instruction_value_index index);

	void do_jump(instruction_index dest);
	void do_ret(instruction_value value);
	void do_ret_void(void);
	void report_error(uint32_t error_index);
	void report_error(uint32_t src_tokens_index, bz::u8string message);
	void report_warning(ctx::warning_kind kind, uint32_t src_tokens_index, bz::u8string message);

	void advance(void);
};

} // namespace comptime

#endif // COMPTIME_EXECUTOR_CONTEXT_H
