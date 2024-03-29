#ifndef COMPTIME_MEMORY_H
#define COMPTIME_MEMORY_H

#include "core.h"
#include "memory_common.h"
#include "global_memory.h"
#include "lex/token.h"
#include "ast/statement_forward.h"

namespace comptime::memory
{

struct memory_properties
{
	bz::fixed_vector<uint8_t> data;

	enum property : uint8_t
	{
		is_alive       = bit_at<0>,
		is_padding     = bit_at<1>,
	};

	memory_properties(void) = default;
	memory_properties(type const *object_type, size_t size, uint8_t bits);

	bool is_all(size_t begin, size_t end, uint8_t bits, uint8_t exception_bits) const;
	bool is_none(size_t begin, size_t end, uint8_t bits, uint8_t exception_bits) const;

	void set_range(size_t begin, size_t end, uint8_t bits);
	void erase_range(size_t begin, size_t end, uint8_t bits);

	void clear(void);
};

struct copy_values_memory_and_properties_t
{
	bz::array_view<uint8_t> memory;
	bz::array_view<uint8_t> properties;
};

struct copy_overlapping_values_data_t
{
	copy_values_memory_and_properties_t dest;
	copy_values_memory_and_properties_t source;
};

struct stack_object
{
	ptr_t address;
	type const *object_type;
	bz::fixed_vector<uint8_t> memory;
	memory_properties properties;

	lex::src_tokens object_src_tokens;

	stack_object(lex::src_tokens const &object_src_tokens, ptr_t address, type const *object_type, bool is_always_initialized);

	size_t object_size(void) const;

	void start_lifetime(ptr_t begin, ptr_t end);
	void end_lifetime(ptr_t begin, ptr_t end);
	bool is_alive(ptr_t begin, ptr_t end) const;
	uint8_t *get_memory(ptr_t address);
	uint8_t const *get_memory(ptr_t address) const;

	bool check_dereference(ptr_t address, type const *subobject_type) const;
	bz::u8string get_dereference_error_reason(ptr_t address, type const *object_type) const;
	bool check_slice_construction(ptr_t begin, ptr_t end, bool end_is_one_past_the_end, type const *elem_type) const;
	bz::vector<bz::u8string> get_slice_construction_error_reason(
		ptr_t begin,
		ptr_t end,
		bool end_is_one_past_the_end,
		type const *elem_type
	) const;
	pointer_arithmetic_result_t do_pointer_arithmetic(
		ptr_t address,
		bool is_one_past_the_end,
		int64_t offset,
		type const *object_type
	) const;
	bz::vector<bz::u8string> get_pointer_arithmetic_error_reason(
		ptr_t address,
		bool is_one_past_the_end,
		int64_t offset,
		type const *object_type
	) const;
	pointer_arithmetic_result_t do_pointer_arithmetic_unchecked(
		ptr_t address,
		bool is_one_past_the_end,
		int64_t offset,
		type const *object_type
	) const;
	bz::optional<int64_t> do_pointer_difference(
		ptr_t lhs,
		ptr_t rhs,
		bool lhs_is_one_past_the_end,
		bool rhs_is_one_past_the_end,
		type const *object_type
	) const;
	bz::vector<bz::u8string> get_pointer_difference_error_reason(
		ptr_t lhs,
		ptr_t rhs,
		bool lhs_is_one_past_the_end,
		bool rhs_is_one_past_the_end,
		type const *object_type
	) const;

	copy_values_memory_t get_dest_memory(ptr_t address, size_t count, type const *elem_type);
	bz::vector<bz::u8string> get_get_dest_memory_error_reasons(ptr_t address, size_t count, type const *elem_type);

	copy_values_memory_t get_copy_source_memory(ptr_t address, size_t count, type const *elem_type);
	bz::vector<bz::u8string> get_get_copy_source_memory_error_reasons(ptr_t address, size_t count, type const *elem_type);

	copy_overlapping_values_data_t get_copy_overlapping_memory(ptr_t dest, ptr_t source, size_t count, type const *elem_type);
	bz::vector<bz::u8string> get_get_copy_overlapping_memory_error_reasons(ptr_t dest, ptr_t source, size_t count, type const *elem_type);
};

struct heap_object
{
	ptr_t address;
	type const *elem_type;
	size_t count;
	bz::fixed_vector<uint8_t> memory;
	memory_properties properties;

	heap_object(ptr_t address, type const *elem_type, uint64_t count);

	size_t object_size(void) const;
	size_t elem_size(void) const;

	void start_lifetime(ptr_t begin, ptr_t end);
	void end_lifetime(ptr_t begin, ptr_t end);
	bool is_alive(ptr_t begin, ptr_t end) const;
	bool is_none_alive(ptr_t begin, ptr_t end) const;
	uint8_t *get_memory(ptr_t address);
	uint8_t const *get_memory(ptr_t address) const;

	bool check_dereference(ptr_t address, type const *subobject_type) const;
	bz::u8string get_dereference_error_reason(ptr_t address, type const *object_type) const;
	bool check_inplace_construct(ptr_t address, type const *object_type) const;
	bz::u8string get_inplace_construct_error_reason(ptr_t address, type const *object_type) const;
	bool check_destruct_value(ptr_t address, type const *object_type) const;
	bz::u8string get_destruct_value_error_reason(ptr_t address, type const *object_type) const;
	bool check_slice_construction(ptr_t begin, ptr_t end, bool end_is_one_past_the_end, type const *elem_type) const;
	bz::vector<bz::u8string> get_slice_construction_error_reason(
		ptr_t begin,
		ptr_t end,
		bool end_is_one_past_the_end,
		type const *elem_type
	) const;
	pointer_arithmetic_result_t do_pointer_arithmetic(
		ptr_t address,
		bool is_one_past_the_end,
		int64_t offset,
		type const *object_type
	) const;
	bz::vector<bz::u8string> get_pointer_arithmetic_error_reason(
		ptr_t address,
		bool is_one_past_the_end,
		int64_t offset,
		type const *object_type
	) const;
	pointer_arithmetic_result_t do_pointer_arithmetic_unchecked(
		ptr_t address,
		bool is_one_past_the_end,
		int64_t offset,
		type const *object_type
	) const;
	bz::optional<int64_t> do_pointer_difference(
		ptr_t lhs,
		ptr_t rhs,
		bool lhs_is_one_past_the_end,
		bool rhs_is_one_past_the_end,
		type const *object_type
	) const;
	bz::vector<bz::u8string> get_pointer_difference_error_reason(
		ptr_t lhs,
		ptr_t rhs,
		bool lhs_is_one_past_the_end,
		bool rhs_is_one_past_the_end,
		type const *object_type
	) const;

	copy_values_memory_and_properties_t get_dest_memory(ptr_t address, size_t count, type const *elem_type, bool is_trivial);
	bz::vector<bz::u8string> get_get_dest_memory_error_reasons(ptr_t address, size_t count, type const *elem_type, bool is_trivial);

	copy_values_memory_t get_copy_source_memory(ptr_t address, size_t count, type const *elem_type);
	bz::vector<bz::u8string> get_get_copy_source_memory_error_reasons(ptr_t address, size_t count, type const *elem_type);

	copy_overlapping_values_data_t get_copy_overlapping_memory(ptr_t dest, ptr_t source, size_t count, type const *elem_type);
	bz::vector<bz::u8string> get_get_copy_overlapping_memory_error_reasons(ptr_t dest, ptr_t source, size_t count, type const *elem_type);

	copy_values_memory_and_properties_t get_relocate_source_memory(ptr_t address, size_t count, type const *elem_type);
	bz::vector<bz::u8string> get_get_relocate_source_memory_error_reasons(ptr_t address, size_t count, type const *elem_type);

	copy_overlapping_values_data_t get_relocate_overlapping_memory(ptr_t dest, ptr_t source, size_t count, bool is_trivial);
	bz::vector<bz::u8string> get_get_relocate_overlapping_memory_error_reasons(ptr_t dest, ptr_t source, size_t count, bool is_trivial);
};

struct stack_frame
{
	bz::fixed_vector<stack_object> objects;
	ptr_t begin_address;
	size_t total_size;
	uint32_t id;
};

struct stack_manager
{
	ptr_t head;
	bz::vector<stack_frame> stack_frames;
	uint32_t stack_frame_id;

	explicit stack_manager(ptr_t stack_begin);

	void push_stack_frame(bz::array_view<alloca const> types);
	void pop_stack_frame(void);

	stack_frame *get_stack_frame(ptr_t address);
	stack_frame const *get_stack_frame(ptr_t address) const;
	stack_object *get_stack_object(ptr_t address);
	stack_object const *get_stack_object(ptr_t address) const;

	bool check_dereference(ptr_t address, type const *object_type) const;
	bz::vector<error_reason_t> get_dereference_error_reason(ptr_t address, type const *object_type) const;
	bool check_slice_construction(ptr_t begin, ptr_t end, bool end_is_one_past_the_end, type const *elem_type) const;
	bz::vector<error_reason_t> get_slice_construction_error_reason(
		ptr_t begin,
		ptr_t end,
		bool end_is_one_past_the_end,
		type const *elem_type
	) const;

	bz::optional<int> compare_pointers(ptr_t lhs, ptr_t rhs) const;
	bz::vector<error_reason_t> get_compare_pointers_error_reason(ptr_t lhs, ptr_t rhs) const;
	pointer_arithmetic_result_t do_pointer_arithmetic(
		ptr_t address,
		bool is_one_past_the_end,
		int64_t offset,
		type const *object_type
	) const;
	bz::vector<error_reason_t> get_pointer_arithmetic_error_reason(
		ptr_t address,
		bool is_one_past_the_end,
		int64_t offset,
		type const *object_type
	) const;
	pointer_arithmetic_result_t do_pointer_arithmetic_unchecked(
		ptr_t address,
		bool is_one_past_the_end,
		int64_t offset,
		type const *object_type
	) const;
	bz::optional<int64_t> do_pointer_difference(
		ptr_t lhs,
		ptr_t rhs,
		bool lhs_is_one_past_the_end,
		bool rhs_is_one_past_the_end,
		type const *object_type
	) const;
	bz::vector<error_reason_t> get_pointer_difference_error_reason(
		ptr_t lhs,
		ptr_t rhs,
		bool lhs_is_one_past_the_end,
		bool rhs_is_one_past_the_end,
		type const *object_type
	) const;

	uint8_t *get_memory(ptr_t address);
	uint8_t const *get_memory(ptr_t address) const;
};

enum class free_result
{
	good,
	double_free,
	invalid_pointer,
};

struct call_stack_info_t
{
	struct src_tokens_function_pair_t
	{
		lex::src_tokens src_tokens;
		ast::function_body const *body;
	};

	lex::src_tokens src_tokens;
	bz::fixed_vector<src_tokens_function_pair_t> call_stack;
};

struct allocation
{
	heap_object object;
	call_stack_info_t alloc_info;
	call_stack_info_t free_info;
	bool is_freed;

	allocation(call_stack_info_t alloc_info, ptr_t address, type const *elem_type, uint64_t count);

	free_result free(call_stack_info_t free_info);
};

struct heap_manager
{
	ptr_t head;
	bz::vector<allocation> allocations;

	explicit heap_manager(ptr_t heap_begin);

	allocation *get_allocation(ptr_t address);
	allocation const *get_allocation(ptr_t address) const;

	ptr_t allocate(call_stack_info_t alloc_info, type const *object_type, uint64_t count);
	free_result free(call_stack_info_t free_info, ptr_t address);

	bool check_dereference(ptr_t address, type const *object_type) const;
	bz::vector<error_reason_t> get_dereference_error_reason(ptr_t address, type const *object_type) const;
	bool check_inplace_construct(ptr_t address, type const *object_type) const;
	bz::vector<error_reason_t> get_inplace_construct_error_reason(ptr_t address, type const *object_type) const;
	bool check_destruct_value(ptr_t address, type const *object_type) const;
	bz::vector<error_reason_t> get_destruct_value_error_reason(ptr_t address, type const *object_type) const;
	bool check_slice_construction(ptr_t begin, ptr_t end, bool end_is_one_past_the_end, type const *elem_type) const;
	bz::vector<error_reason_t> get_slice_construction_error_reason(
		ptr_t begin,
		ptr_t end,
		bool end_is_one_past_the_end,
		type const *elem_type
	) const;

	bz::optional<int> compare_pointers(ptr_t lhs, ptr_t rhs) const;
	bz::vector<error_reason_t> get_compare_pointers_error_reason(ptr_t lhs, ptr_t rhs) const;
	pointer_arithmetic_result_t do_pointer_arithmetic(
		ptr_t address,
		bool is_one_past_the_end,
		int64_t offset,
		type const *object_type
	) const;
	bz::vector<error_reason_t> get_pointer_arithmetic_error_reason(
		ptr_t address,
		bool is_one_past_the_end,
		int64_t offset,
		type const *object_type
	) const;
	pointer_arithmetic_result_t do_pointer_arithmetic_unchecked(
		ptr_t address,
		bool is_one_past_the_end,
		int64_t offset,
		type const *object_type
	) const;
	bz::optional<int64_t> do_pointer_difference(
		ptr_t lhs,
		ptr_t rhs,
		bool lhs_is_one_past_the_end,
		bool rhs_is_one_past_the_end,
		type const *object_type
	) const;
	bz::vector<error_reason_t> get_pointer_difference_error_reason(
		ptr_t lhs,
		ptr_t rhs,
		bool lhs_is_one_past_the_end,
		bool rhs_is_one_past_the_end,
		type const *object_type
	) const;

	uint8_t *get_memory(ptr_t address);
	uint8_t const *get_memory(ptr_t address) const;
};

struct stack_object_pointer
{
	ptr_t stack_address;
	uint32_t stack_frame_depth;
	uint32_t stack_frame_id;
	lex::src_tokens object_src_tokens;
};

struct one_past_the_end_pointer
{
	ptr_t address;
};

enum class meta_memory_segment
{
	stack_object,
	one_past_the_end,
};

using meta_segment_info_t = memory_segment_info_t<bz::array{
	meta_memory_segment::stack_object,
	meta_memory_segment::one_past_the_end,
}>;

struct meta_memory_manager
{
	meta_segment_info_t segment_info;

	bz::vector<stack_object_pointer> stack_object_pointers;
	bz::vector<one_past_the_end_pointer> one_past_the_end_pointers;

	explicit meta_memory_manager(ptr_t meta_begin);

	size_t get_stack_object_index(ptr_t address) const;
	size_t get_one_past_the_end_index(ptr_t address) const;
	stack_object_pointer const &get_stack_object_pointer(ptr_t address) const;
	one_past_the_end_pointer const &get_one_past_the_end_pointer(ptr_t address) const;

	ptr_t make_stack_object_address(
		ptr_t address,
		uint32_t stack_frame_depth,
		uint32_t stack_frame_id,
		lex::src_tokens const &object_src_tokens
	);
	ptr_t make_inherited_stack_object_address(ptr_t address, ptr_t inherit_from);
	ptr_t make_one_past_the_end_address(ptr_t address);

	ptr_t get_real_address(ptr_t address) const;
};

struct memory_manager
{
	global_segment_info_t segment_info;

	global_memory_manager *global_memory;
	stack_manager stack;
	heap_manager heap;
	meta_memory_manager meta_memory;

	explicit memory_manager(global_segment_info_t _segment_info, global_memory_manager *_global_memory);

	[[nodiscard]] bool push_stack_frame(bz::array_view<alloca const> types);
	void pop_stack_frame(void);

	ptr_t make_current_frame_stack_object_address(stack_object const &object);

	ptr_t allocate(call_stack_info_t alloc_info, type const *object_type, uint64_t count);
	free_result free(call_stack_info_t free_info, ptr_t address);
	bz::vector<error_reason_t> get_free_error_reason(ptr_t address);

	bool check_dereference(ptr_t address, type const *object_type) const;
	bz::vector<error_reason_t> get_dereference_error_reason(ptr_t address, type const *object_type) const;
	bool check_inplace_construct(ptr_t address, type const *object_type) const;
	bz::vector<error_reason_t> get_inplace_construct_error_reason(ptr_t address, type const *object_type) const;
	bool check_destruct_value(ptr_t address, type const *object_type) const;
	bz::vector<error_reason_t> get_destruct_value_error_reason(ptr_t address, type const *object_type) const;
	bool check_slice_construction(ptr_t begin, ptr_t end, type const *elem_type) const;
	bz::vector<error_reason_t> get_slice_construction_error_reason(ptr_t begin, ptr_t end, type const *elem_type) const;

	bz::optional<int> compare_pointers(ptr_t lhs, ptr_t rhs) const;
	bz::vector<error_reason_t> get_compare_pointers_error_reason(ptr_t lhs, ptr_t rhs) const;
	ptr_t do_pointer_arithmetic(ptr_t address, int64_t offset, type const *object_type);
	bz::vector<error_reason_t> get_pointer_arithmetic_error_reason(ptr_t address, int64_t offset, type const *object_type) const;
	ptr_t do_pointer_arithmetic_unchecked(ptr_t address, int64_t offset, type const *object_type);
	ptr_t do_gep(ptr_t address, type const *object_type, uint64_t index);
	bz::optional<int64_t> do_pointer_difference(ptr_t lhs, ptr_t rhs, type const *object_type) const;
	bz::vector<error_reason_t> get_pointer_difference_error_reason(ptr_t lhs, ptr_t rhs, type const *object_type) const;
	int64_t do_pointer_difference_unchecked(ptr_t lhs, ptr_t rhs, size_t stride) const;

	bool copy_values(ptr_t dest, ptr_t source, size_t count, type const *elem_type, bool is_trivial);
	bz::vector<error_reason_t> get_copy_values_error_reason(
		ptr_t dest,
		ptr_t source,
		size_t count,
		type const *elem_type,
		bool is_trivial
	);

	bool copy_overlapping_values(ptr_t dest, ptr_t source, size_t count, type const *elem_type);
	bz::vector<error_reason_t> get_copy_overlapping_values_error_reason(
		ptr_t dest,
		ptr_t source,
		size_t count,
		type const *elem_type
	);

	bool relocate_values(ptr_t dest, ptr_t source, size_t count, type const *elem_type, bool is_trivial);
	bz::vector<error_reason_t> get_relocate_values_error_reason(
		ptr_t dest,
		ptr_t source,
		size_t count,
		type const *elem_type,
		bool is_trivial
	);

	bool set_values_i8_native(ptr_t dest, uint8_t value, size_t count, type const *elem_type);
	bool set_values_i16_native(ptr_t dest, uint16_t value, size_t count, type const *elem_type);
	bool set_values_i32_native(ptr_t dest, uint32_t value, size_t count, type const *elem_type);
	bool set_values_i64_native(ptr_t dest, uint64_t value, size_t count, type const *elem_type);
	bool set_values_ref(ptr_t dest, uint8_t const *value_mem, size_t count, type const *elem_type);
	bz::vector<error_reason_t> get_set_values_error_reason(ptr_t dest, size_t count, type const *elem_type);

	void start_lifetime(ptr_t address, size_t size);
	void end_lifetime(ptr_t address, size_t size);

	bool is_global(ptr_t address) const;

	uint8_t *get_memory(ptr_t address);
	uint8_t const *get_memory(ptr_t address) const;
};

} // namespace comptime::memory

#endif // COMPTIME_MEMORY_H
