#ifndef COMPTIME_MEMORY_H
#define COMPTIME_MEMORY_H

#include "core.h"
#include "types.h"
#include "values.h"
#include "lex/token.h"

namespace comptime::memory
{

struct pointer_arithmetic_result_t
{
	ptr_t address;
	bool is_on_past_the_end;
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
};

struct heap_object
{
	ptr_t address;
	type const *elem_type;
	size_t count;
	bz::fixed_vector<uint8_t> memory;
	bz::fixed_vector<uint8_t> is_initialized;

	heap_object(ptr_t address, type const *elem_type, size_t count);

	size_t object_size(void) const;
	size_t elem_size(void) const;

	void initialize_region(ptr_t begin, ptr_t end);
	bool is_region_initialized(ptr_t begin, ptr_t end) const;
	uint8_t *get_memory(ptr_t address);

	bool check_dereference(ptr_t address, type const *subobject_type) const;
	bool check_slice_construction(ptr_t begin, ptr_t end, type const *elem_type) const;
	pointer_arithmetic_result_t do_pointer_arithmetic(ptr_t address, int64_t amount, type const *pointer_type) const;
};

struct stack_frame
{
	bz::fixed_vector<stack_object> objects;
};

struct stack_manager
{
	ptr_t head;
	bz::vector<stack_frame> stack_frames;
};

struct allocation
{
	heap_object object;
	lex::src_tokens malloc_src_tokens;
	lex::src_tokens free_src_tokens;
	bool is_freed;

	bool free(lex::src_tokens const &free_src_tokens);
};

struct heap_manager
{
	ptr_t head;
	bz::vector<allocation> allocations;

	ptr_t allocate(type const *object_type);
	void free(lex::src_tokens const &free_src_tokens, ptr_t address);

	uint8_t *get_memory(ptr_t address);
};

struct stack_object_pointer
{
	ptr_t stack_address;
	size_t stack_frame_index;
};

struct one_past_the_end_pointer
{
	ptr_t address;
};

struct meta_memory_manager
{
	ptr_t begin_address;
};

struct memory_manager
{
	stack_manager stack;
	heap_manager heap;
	meta_memory_manager meta_memory;

	bool check_dereference(ptr_t address, type const *object_type);
	bool check_slice_construction(ptr_t begin, ptr_t end, type const *elem_type);
	bz::u8string get_slice_construction_error_reason(ptr_t begin, ptr_t end, type const *elem_type);

	bz::optional<int> compare_pointers(ptr_t lhs, ptr_t rhs);
	ptr_t do_pointer_arithmetic(ptr_t address, int64_t offset, type const *object_type);

	uint8_t *get_memory(ptr_t address);
};

} // namespace comptime::memory

#endif // COMPTIME_MEMORY_H
