#ifndef COMPTIME_MEMORY_H
#define COMPTIME_MEMORY_H

#include "core.h"
#include "types.h"
#include "values.h"
#include "lex/token.h"
#include "ast/statement_forward.h"
#include "ast/constant_value.h"

namespace comptime::memory
{

enum class endianness_kind
{
	little,
	big,
};

template<typename Int>
Int byteswap(Int value)
{
	if constexpr (sizeof (Int) == 1)
	{
		return value;
	}
	else if constexpr (sizeof (Int) == 2)
	{
		value = ((value & uint16_t(0xff00)) >> 8) | ((value & uint16_t(0x00ff)) << 8);
		return value;
	}
	else if constexpr (sizeof (Int) == 4)
	{
		value = ((value & uint32_t(0xffff'0000)) >> 16) | ((value & uint32_t(0x0000'ffff)) << 16);
		value = ((value & uint32_t(0xff00'ff00)) >> 8) | ((value & uint32_t(0x00ff'00ff)) << 8);
		return value;
	}
	else if constexpr (sizeof (Int) == 8)
	{
		value = ((value & uint64_t(0xffff'ffff'0000'0000)) >> 32) | ((value & uint64_t(0x0000'0000'ffff'ffff)) << 32);
		value = ((value & uint64_t(0xffff'0000'ffff'0000)) >> 16) | ((value & uint64_t(0x0000'ffff'0000'ffff)) << 16);
		value = ((value & uint64_t(0xff00'ff00'ff00'ff00)) >> 8) | ((value & uint64_t(0x00ff'00ff'00ff'00ff)) << 8);
		return value;
	}
	else
	{
		static_assert(bz::meta::always_false<Int>);
	}
}

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
	uint8_t const *get_memory(ptr_t address) const;

	bool check_dereference(ptr_t address, type const *subobject_type) const;
	bool check_slice_construction(ptr_t begin, ptr_t end, type const *elem_type) const;
	pointer_arithmetic_result_t do_pointer_arithmetic(ptr_t address, int64_t amount, type const *pointer_type) const;
	bz::optional<int64_t> do_pointer_difference(ptr_t lhs, ptr_t rhs, type const *object_type) const;
};

struct bitset
{
	bz::fixed_vector<uint8_t> bits;

	bitset(void) = default;
	bitset(size_t size, bool value);

	void set_range(size_t begin, size_t end, bool value);
	bool is_all(size_t begin, size_t end) const;
	bool is_none(size_t begin, size_t end) const;

	void clear(void);
};

struct stack_object
{
	ptr_t address;
	type const *object_type;
	bz::fixed_vector<uint8_t> memory;
	bitset is_alive_bitset;

	stack_object(ptr_t address, type const *object_type, bool is_always_initialized);

	size_t object_size(void) const;

	void start_lifetime(ptr_t begin, ptr_t end);
	void end_lifetime(ptr_t begin, ptr_t end);
	bool is_alive(ptr_t begin, ptr_t end) const;
	uint8_t *get_memory(ptr_t address);
	uint8_t const *get_memory(ptr_t address) const;

	bool check_dereference(ptr_t address, type const *subobject_type) const;
	bool check_slice_construction(ptr_t begin, ptr_t end, type const *elem_type) const;
	pointer_arithmetic_result_t do_pointer_arithmetic(ptr_t address, int64_t amount, type const *pointer_type) const;
	bz::optional<int64_t> do_pointer_difference(ptr_t lhs, ptr_t rhs, type const *object_type) const;
};

struct heap_object
{
	ptr_t address;
	type const *elem_type;
	size_t count;
	bz::fixed_vector<uint8_t> memory;
	bitset is_alive_bitset;

	heap_object(ptr_t address, type const *elem_type, uint64_t count);

	size_t object_size(void) const;
	size_t elem_size(void) const;

	void start_lifetime(ptr_t begin, ptr_t end);
	void end_lifetime(ptr_t begin, ptr_t end);
	bool is_alive(ptr_t begin, ptr_t end) const;
	uint8_t *get_memory(ptr_t address);
	uint8_t const *get_memory(ptr_t address) const;

	bool check_dereference(ptr_t address, type const *subobject_type) const;
	bool check_slice_construction(ptr_t begin, ptr_t end, type const *elem_type) const;
	pointer_arithmetic_result_t do_pointer_arithmetic(ptr_t address, int64_t amount, type const *pointer_type) const;
	bz::optional<int64_t> do_pointer_difference(ptr_t lhs, ptr_t rhs, type const *object_type) const;
};

struct global_memory_manager
{
	ptr_t head;
	bz::vector<global_object> objects;

	struct one_past_the_end_pointer_info_t
	{
		uint32_t object_index;
		uint32_t offset;
		ptr_t address;
	};

	bz::vector<one_past_the_end_pointer_info_t> one_past_the_end_pointer_infos;

	explicit global_memory_manager(ptr_t global_memory_begin);

	uint32_t add_object(type const *object_type, bz::fixed_vector<uint8_t> data);
	void add_one_past_the_end_pointer_info(one_past_the_end_pointer_info_t info);

	global_object *get_global_object(ptr_t address);
	global_object const *get_global_object(ptr_t address) const;

	bool check_dereference(ptr_t address, type const *object_type) const;
	bool check_slice_construction(ptr_t begin, ptr_t end, type const *elem_type) const;
	bz::u8string get_slice_construction_error_reason(ptr_t begin, ptr_t end, type const *elem_type) const;

	pointer_arithmetic_result_t do_pointer_arithmetic(ptr_t address, int64_t offset, type const *object_type) const;
	bz::optional<int64_t> do_pointer_difference(ptr_t lhs, ptr_t rhs, type const *object_type) const;

	uint8_t *get_memory(ptr_t address);
	uint8_t const *get_memory(ptr_t address) const;
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
	bool check_slice_construction(ptr_t begin, ptr_t end, type const *elem_type) const;
	bz::u8string get_slice_construction_error_reason(ptr_t begin, ptr_t end, type const *elem_type) const;

	pointer_arithmetic_result_t do_pointer_arithmetic(ptr_t address, int64_t offset, type const *object_type) const;
	bz::optional<int64_t> do_pointer_difference(ptr_t lhs, ptr_t rhs, type const *object_type) const;

	uint8_t *get_memory(ptr_t address);
	uint8_t const *get_memory(ptr_t address) const;
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
	allocation const *get_allocation(ptr_t address) const;

	ptr_t allocate(call_stack_info_t alloc_info, type const *object_type, uint64_t count);
	free_result free(call_stack_info_t free_info, ptr_t address);

	bool check_dereference(ptr_t address, type const *object_type) const;
	bool check_slice_construction(ptr_t begin, ptr_t end, type const *elem_type) const;
	bz::u8string get_slice_construction_error_reason(ptr_t begin, ptr_t end, type const *elem_type) const;

	pointer_arithmetic_result_t do_pointer_arithmetic(ptr_t address, int64_t offset, type const *object_type) const;
	bz::optional<int64_t> do_pointer_difference(ptr_t lhs, ptr_t rhs, type const *object_type) const;

	uint8_t *get_memory(ptr_t address);
	uint8_t const *get_memory(ptr_t address) const;
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

	explicit memory_manager(
		memory_segment_info_t _segment_info,
		global_memory_manager *_global_memory,
		bool is_64_bit,
		bool is_native_endianness
	);

	ptr_t get_non_meta_address(ptr_t address);

	[[nodiscard]] bool push_stack_frame(bz::array_view<alloca const> types);
	void pop_stack_frame(void);

	bool check_dereference(ptr_t address, type const *object_type) const;
	bool check_slice_construction(ptr_t begin, ptr_t end, type const *elem_type) const;
	bz::u8string get_slice_construction_error_reason(ptr_t begin, ptr_t end, type const *elem_type) const;

	bz::optional<int> compare_pointers(ptr_t lhs, ptr_t rhs);
	ptr_t do_pointer_arithmetic(ptr_t address, int64_t offset, type const *object_type);
	ptr_t do_gep(ptr_t address, type const *object_type, uint64_t index);
	bz::optional<int64_t> do_pointer_difference(ptr_t lhs, ptr_t rhs, type const *object_type);
	int64_t do_pointer_difference_unchecked(ptr_t lhs, ptr_t rhs, size_t stride);

	void start_lifetime(ptr_t address, size_t size);
	void end_lifetime(ptr_t address, size_t size);

	bool is_global(ptr_t address) const;

	uint8_t *get_memory(ptr_t address);
	uint8_t const *get_memory(ptr_t address) const;
};

ast::constant_value constant_value_from_object(
	type const *object_type,
	uint8_t const *mem,
	ast::typespec_view ts,
	endianness_kind endianness,
	memory_manager const &manager
);

struct object_from_constant_value_result_t
{
	bz::fixed_vector<uint8_t> data;
	bz::vector<global_memory_manager::one_past_the_end_pointer_info_t> one_past_the_end_infos;
};

object_from_constant_value_result_t object_from_constant_value(
	ast::constant_value const &value,
	type const *object_type,
	endianness_kind endianness,
	global_memory_manager &manager,
	type_set_t &type_set
);

} // namespace comptime::memory

#endif // COMPTIME_MEMORY_H
