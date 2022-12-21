#ifndef COMPTIME_EXECUTOR_CONTEXT_H
#define COMPTIME_EXECUTOR_CONTEXT_H

#include "instructions.h"
#include "memory.h"
#include "global_codegen_context.h"
#include "ctx/warnings.h"

namespace comptime
{

struct executor_context
{
	instruction const *current_instruction;
	instruction_value *current_instruction_value;
	instruction const *next_instruction;
	uint32_t alloca_offset;
	bool returned;
	instruction_value ret_value;

	function *current_function;
	bz::array_view<instruction const> instructions;
	bz::array_view<instruction_value> instruction_values;

	memory::memory_manager memory;

	type const *get_builtin_type(builtin_type_kind kind);
	type const *get_pointer_type(void);

	uint8_t *get_memory(ptr_t ptr, type const *object_type);
	uint8_t *get_memory_raw(ptr_t ptr);

	void set_current_instruction_value(instruction_value value);
	instruction_value get_instruction_value(instruction_value_index index);

	void do_jump(instruction_index dest);
	void do_ret(instruction_value value);
	void do_ret_void(void);
	void report_error(uint32_t error_index);
	void report_error(uint32_t src_tokens_index, bz::u8string message);
	void report_warning(ctx::warning_kind kind, uint32_t src_tokens_index, bz::u8string message);

	slice_construction_check_info_t const &get_slice_construction_info(uint32_t index) const;

	void do_str_construction_check(uint32_t src_tokens_index, ptr_t begin, ptr_t end);
	void do_slice_construction_check(uint32_t src_tokens_index, ptr_t begin, ptr_t end, type const *elem_type);

	void advance(void);
};

} // namespace comptime

#endif // COMPTIME_EXECUTOR_CONTEXT_H
