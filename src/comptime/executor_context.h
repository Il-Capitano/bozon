#ifndef COMPTIME_EXECUTOR_CONTEXT_H
#define COMPTIME_EXECUTOR_CONTEXT_H

#include "instructions.h"
#include "memory.h"
#include "codegen_context_forward.h"
#include "ast/typespec.h"
#include "ast/expression.h"
#include "ctx/warnings.h"
#include "ctx/error.h"

namespace comptime
{

struct execution_frame_info_t
{
	function const *func;
	instruction const *call_inst;
	uint32_t call_src_tokens_index;

	bz::fixed_vector<instruction_value> args;
	bz::fixed_vector<instruction_value> instruction_values;
};

struct executor_context
{
	instruction const *current_instruction = nullptr;
	instruction_value *current_instruction_value = nullptr;
	instruction const *next_instruction = nullptr;
	instruction_value ret_value = { .none = none_t{} };
	bool returned = false;
	bool has_error = false;

	function const *current_function = nullptr;
	bz::fixed_vector<instruction_value> args{};
	bz::fixed_vector<instruction_value> instruction_values{};
	bz::array_view<instruction const> instructions{};
	uint32_t alloca_offset = 0;
	uint32_t call_src_tokens_index = 0;

	bz::vector<execution_frame_info_t> call_stack{};
	bz::vector<ctx::error> diagnostics{};
	lex::src_tokens execution_start_src_tokens{};

	memory::memory_manager memory;

	codegen_context *codegen_ctx;


	executor_context(codegen_context *_codegen_ctx);

	uint8_t *get_memory(ptr_t address);

	void set_current_instruction_value(instruction_value value);
	instruction_value get_instruction_value(instruction_value_index index);

	void add_call_stack_notes(bz::vector<ctx::source_highlight> &notes) const;
	void report_error(uint32_t error_index);
	void report_error(
		uint32_t src_tokens_index,
		bz::u8string message,
		bz::vector<ctx::source_highlight> notes = {}
	);
	void report_warning(
		ctx::warning_kind kind,
		uint32_t src_tokens_index,
		bz::u8string message,
		bz::vector<ctx::source_highlight> notes = {}
	);
	ctx::source_highlight make_note(uint32_t src_tokens_index, bz::u8string message);
	bool is_option_set(bz::u8string_view option);

	ptr_t get_global(uint32_t index);
	ptr_t add_global_array_data(lex::src_tokens const &src_tokens, type const *elem_type, bz::array_view<uint8_t const> data);
	instruction_value get_arg(uint32_t index);
	void do_jump(instruction_index dest);
	void do_ret(instruction_value value);
	void do_ret_void(void);

	void call_function(uint32_t call_src_tokens_index, function const *func, uint32_t args_index);

	switch_info_t const &get_switch_info(uint32_t index) const;
	error_info_t const &get_error_info(uint32_t index) const;
	lex::src_tokens const &get_src_tokens(uint32_t index) const;
	slice_construction_check_info_t const &get_slice_construction_info(uint32_t index) const;
	pointer_arithmetic_check_info_t const &get_pointer_arithmetic_info(uint32_t index) const;
	memory_access_check_info_t const &get_memory_access_info(uint32_t index) const;
	add_global_array_data_info_t const &get_add_global_array_data_info(uint32_t index) const;
	copy_values_info_t const &get_copy_values_info(uint32_t index) const;

	memory::call_stack_info_t get_call_stack_info(uint32_t src_tokens_index) const;

	ptr_t malloc(uint32_t src_tokens_index, type const *type, uint64_t count);
	void free(uint32_t src_tokens_index, ptr_t address);

	void check_dereference(uint32_t src_tokens_index, ptr_t address, type const *object_type, ast::typespec_view object_typespec);
	void check_inplace_construct(uint32_t src_tokens_index, ptr_t address, type const *object_type, ast::typespec_view object_typespec);
	void check_str_construction(uint32_t src_tokens_index, ptr_t begin, ptr_t end);
	void check_slice_construction(
		uint32_t src_tokens_index,
		ptr_t begin,
		ptr_t end,
		type const *elem_type,
		ast::typespec_view slice_type
	);

	int compare_pointers(uint32_t src_tokens_index, ptr_t lhs, ptr_t rhs);
	bool compare_pointers_equal(ptr_t lhs, ptr_t rhs);
	ptr_t pointer_add_unchecked(ptr_t address, int32_t offset, type const *object_type);
	ptr_t pointer_add_signed(
		uint32_t src_tokens_index,
		ptr_t address,
		int64_t offset,
		type const *object_type,
		ast::typespec_view pointer_type
	);
	ptr_t pointer_add_unsigned(
		uint32_t src_tokens_index,
		ptr_t address,
		uint64_t offset,
		type const *object_type,
		ast::typespec_view pointer_type
	);
	ptr_t pointer_sub_signed(
		uint32_t src_tokens_index,
		ptr_t address,
		int64_t offset,
		type const *object_type,
		ast::typespec_view pointer_type
	);
	ptr_t pointer_sub_unsigned(
		uint32_t src_tokens_index,
		ptr_t address,
		uint64_t offset,
		type const *object_type,
		ast::typespec_view pointer_type
	);
	ptr_t gep(ptr_t address, type const *object_type, uint64_t index);
	int64_t pointer_difference(
		uint32_t src_tokens_index,
		ptr_t lhs,
		ptr_t rhs,
		type const *object_type,
		ast::typespec_view pointer_type
	);
	int64_t pointer_difference_unchecked(ptr_t lhs, ptr_t rhs, size_t stride);

	void copy_values(
		uint32_t src_tokens_index,
		ptr_t dest,
		ptr_t source,
		uint64_t count,
		type const *object_type,
		bool is_trivially_destructible
	);
	void copy_overlapping_values(
		uint32_t src_tokens_index,
		ptr_t dest,
		ptr_t source,
		uint64_t count,
		type const *object_type
	);
	void relocate_values(
		uint32_t src_tokens_index,
		ptr_t dest,
		ptr_t source,
		uint64_t count,
		type const *object_type,
		bool is_trivially_destructible
	);
	void set_values_i1_native(uint32_t src_tokens_index, ptr_t dest, bool value, uint64_t count);
	void set_values_i8_native(uint32_t src_tokens_index, ptr_t dest, uint8_t value, uint64_t count);
	void set_values_i16_native(uint32_t src_tokens_index, ptr_t dest, uint16_t value, uint64_t count);
	void set_values_i32_native(uint32_t src_tokens_index, ptr_t dest, uint32_t value, uint64_t count);
	void set_values_i64_native(uint32_t src_tokens_index, ptr_t dest, uint64_t value, uint64_t count);
	void set_values_f32_native(uint32_t src_tokens_index, ptr_t dest, uint32_t bits, uint64_t count);
	void set_values_f64_native(uint32_t src_tokens_index, ptr_t dest, uint64_t bits, uint64_t count);
	void set_values_ptr32_native(uint32_t src_tokens_index, ptr_t dest, uint32_t value, uint64_t count);
	void set_values_ptr64_native(uint32_t src_tokens_index, ptr_t dest, uint64_t value, uint64_t count);
	void set_values_ref(uint32_t src_tokens_index, ptr_t dest, ptr_t value_ref, uint64_t count, type const *elem_type);

	void start_lifetime(ptr_t address, size_t size);
	void end_lifetime(ptr_t address, size_t size);

	void advance(void);

	bool check_memory_leaks(void);
	ast::constant_value execute_expression(ast::expression const &expr, function const &func);
};

} // namespace comptime

#endif // COMPTIME_EXECUTOR_CONTEXT_H
