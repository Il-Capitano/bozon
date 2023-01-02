#ifndef COMPTIME_EXECUTOR_CONTEXT_H
#define COMPTIME_EXECUTOR_CONTEXT_H

#include "instructions.h"
#include "memory.h"
#include "global_codegen_context.h"
#include "ast/typespec.h"
#include "ctx/warnings.h"
#include "ctx/error.h"

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

	global_codegen_context *global_context;


	uint8_t *get_memory(ptr_t address);

	void set_current_instruction_value(instruction_value value);
	instruction_value get_instruction_value(instruction_value_index index);

	void do_jump(instruction_index dest);
	void do_ret(instruction_value value);
	void do_ret_void(void);
	void report_error(uint32_t error_index);
	void report_error(uint32_t src_tokens_index, bz::u8string message, bz::vector<ctx::source_highlight> notes = {});
	void report_warning(ctx::warning_kind kind, uint32_t src_tokens_index, bz::u8string message);
	ctx::source_highlight make_note(uint32_t src_tokens_index, bz::u8string message);

	slice_construction_check_info_t const &get_slice_construction_info(uint32_t index) const;

	void do_str_construction_check(uint32_t src_tokens_index, ptr_t begin, ptr_t end);
	void do_slice_construction_check(uint32_t src_tokens_index, ptr_t begin, ptr_t end, type const *elem_type, ast::typespec_view slice_type);

	int compare_pointers(uint32_t src_tokens_index, ptr_t lhs, ptr_t rhs);
	bool compare_pointers_equal(ptr_t lhs, ptr_t rhs);

	void advance(void);
};

} // namespace comptime

#endif // COMPTIME_EXECUTOR_CONTEXT_H
