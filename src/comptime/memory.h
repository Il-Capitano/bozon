#ifndef COMPTIME_MEMORY_H
#define COMPTIME_MEMORY_H

#include "core.h"
#include "types.h"
#include "values.h"
#include "lex/token.h"
#include "ast/statement_forward.h"

namespace comptime::memory
{

struct pointer_arithmetic_result_t
{
	ptr_t address;
	bool is_one_past_the_end;
};

struct global_object
{
	ptr_t address;
	type const *object_type;
	bz::fixed_vector<uint8_t> memory;

	global_object(ptr_t address, type const *object_type, bz::fixed_vector<uint8_t> data);

	size_t object_size(void) const;

	uint8_t *get_memory(ptr_t address);

	bool check_dereference(ptr_t address, type const *subobject_type) const;
	bool check_slice_construction(ptr_t begin, ptr_t end, type const *elem_type) const;
	pointer_arithmetic_result_t do_pointer_arithmetic(ptr_t address, int64_t amount, type const *pointer_type) const;
	bz::optional<int64_t> do_pointer_difference(ptr_t lhs, ptr_t rhs, type const *object_type);
};

struct stack_object
{
	ptr_t address;
	type const *object_type;
	bz::fixed_vector<uint8_t> memory;
	bool is_initialized: 1;

	stack_object(ptr_t address, type const *object_type);

	size_t object_size(void) const;

	void initialize(void);
	void deinitialize(void);
	uint8_t *get_memory(ptr_t address);

	bool check_dereference(ptr_t address, type const *subobject_type) const;
	bool check_slice_construction(ptr_t begin, ptr_t end, type const *elem_type) const;
	pointer_arithmetic_result_t do_pointer_arithmetic(ptr_t address, int64_t amount, type const *pointer_type) const;
	bz::optional<int64_t> do_pointer_difference(ptr_t lhs, ptr_t rhs, type const *object_type);
};

struct heap_object
{
	ptr_t address;
	type const *elem_type;
	size_t count;
	bz::fixed_vector<uint8_t> memory;
	bz::fixed_vector<uint8_t> is_initialized;

	heap_object(ptr_t address, type const *elem_type, uint64_t count);

	size_t object_size(void) const;
	size_t elem_size(void) const;

	void initialize_region(ptr_t begin, ptr_t end);
	bool is_region_initialized(ptr_t begin, ptr_t end) const;
	uint8_t *get_memory(ptr_t address);

	bool check_dereference(ptr_t address, type const *subobject_type) const;
	bool check_slice_construction(ptr_t begin, ptr_t end, type const *elem_type) const;
	pointer_arithmetic_result_t do_pointer_arithmetic(ptr_t address, int64_t amount, type const *pointer_type) const;
	bz::optional<int64_t> do_pointer_difference(ptr_t lhs, ptr_t rhs, type const *object_type);
};

struct global_memory_manager
{
	ptr_t head;
	bz::vector<global_object> objects;

	explicit global_memory_manager(ptr_t global_memory_begin);

	uint32_t add_object(type const *object_type, bz::fixed_vector<uint8_t> data);

	global_object *get_global_object(ptr_t address);

	bool check_dereference(ptr_t address, type const *object_type);
	bool check_slice_construction(ptr_t begin, ptr_t end, type const *elem_type);
	bz::u8string get_slice_construction_error_reason(ptr_t begin, ptr_t end, type const *elem_type);

	pointer_arithmetic_result_t do_pointer_arithmetic(ptr_t address, int64_t offset, type const *object_type);
	bz::optional<int64_t> do_pointer_difference(ptr_t lhs, ptr_t rhs, type const *object_type);

	uint8_t *get_memory(ptr_t address);
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

	void push_stack_frame(bz::array_view<type const *const> types);
	void pop_stack_frame(void);

	stack_frame *get_stack_frame(ptr_t address);
	stack_object *get_stack_object(ptr_t address);

	bool check_dereference(ptr_t address, type const *object_type);
	bool check_slice_construction(ptr_t begin, ptr_t end, type const *elem_type);
	bz::u8string get_slice_construction_error_reason(ptr_t begin, ptr_t end, type const *elem_type);

	pointer_arithmetic_result_t do_pointer_arithmetic(ptr_t address, int64_t offset, type const *object_type);
	bz::optional<int64_t> do_pointer_difference(ptr_t lhs, ptr_t rhs, type const *object_type);

	uint8_t *get_memory(ptr_t address);
};

enum class free_result
{
	good,
	double_free,
	unknown_address,
	address_inside_object,
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

	ptr_t allocate(call_stack_info_t alloc_info, type const *object_type, uint64_t count);
	free_result free(call_stack_info_t free_info, ptr_t address);

	bool check_dereference(ptr_t address, type const *object_type);
	bool check_slice_construction(ptr_t begin, ptr_t end, type const *elem_type);
	bz::u8string get_slice_construction_error_reason(ptr_t begin, ptr_t end, type const *elem_type);

	pointer_arithmetic_result_t do_pointer_arithmetic(ptr_t address, int64_t offset, type const *object_type);
	bz::optional<int64_t> do_pointer_difference(ptr_t lhs, ptr_t rhs, type const *object_type);

	uint8_t *get_memory(ptr_t address);
};

struct stack_object_pointer
{
	ptr_t stack_address;
	uint32_t stack_frame_depth;
	uint32_t stack_frame_id;
};

struct one_past_the_end_pointer
{
	ptr_t address;
};

struct meta_memory_manager
{
	ptr_t stack_object_begin_address;
	ptr_t one_past_the_end_begin_address;

	bz::vector<stack_object_pointer> stack_object_pointers;
	bz::vector<one_past_the_end_pointer> one_past_the_end_pointers;

	explicit meta_memory_manager(ptr_t meta_begin);

	ptr_t get_real_address(ptr_t address) const;
	bool is_valid(ptr_t address, bz::array_view<stack_frame const> current_stack_frames) const;

	ptr_t make_one_past_the_end_address(ptr_t address);
};

enum class memory_segment
{
	invalid,
	global,
	stack,
	heap,
	meta,
};

struct memory_segment_info_t
{
	ptr_t global_begin;
	ptr_t stack_begin;
	ptr_t heap_begin;
	ptr_t meta_begin;

	memory_segment get_segment(ptr_t address) const;
};

struct memory_manager
{
	memory_segment_info_t segment_info;

	global_memory_manager *global_memory;
	stack_manager stack;
	heap_manager heap;
	meta_memory_manager meta_memory;

	explicit memory_manager(memory_segment_info_t _segment_info, global_memory_manager *_global_memory);

	[[nodiscard]] bool push_stack_frame(bz::array_view<type const *const> types);
	void pop_stack_frame(void);

	bool check_dereference(ptr_t address, type const *object_type);
	bool check_slice_construction(ptr_t begin, ptr_t end, type const *elem_type);
	bz::u8string get_slice_construction_error_reason(ptr_t begin, ptr_t end, type const *elem_type);

	bz::optional<int> compare_pointers(ptr_t lhs, ptr_t rhs);
	ptr_t do_pointer_arithmetic(ptr_t address, int64_t offset, type const *object_type);
	ptr_t do_gep(ptr_t address, type const *object_type, uint64_t index);
	bz::optional<int64_t> do_pointer_difference(ptr_t lhs, ptr_t rhs, type const *object_type);
	int64_t do_pointer_difference_unchecked(ptr_t lhs, ptr_t rhs, size_t stride);

	uint8_t *get_memory(ptr_t address);
};

} // namespace comptime::memory

#endif // COMPTIME_MEMORY_H
