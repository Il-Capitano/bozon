#ifndef COMPTIME_GLOBAL_MEMORY_H
#define COMPTIME_GLOBAL_MEMORY_H

#include "core.h"
#include "memory_common.h"
#include "instructions_forward.h"
#include "ast/constant_value.h"

namespace comptime::memory
{

struct global_object
{
	ptr_t address;
	type const *object_type;
	bz::fixed_vector<uint8_t> memory;

	lex::src_tokens object_src_tokens;

	global_object(lex::src_tokens const &object_src_tokens, ptr_t address, type const *object_type, bz::fixed_vector<uint8_t> data);

	size_t object_size(void) const;

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

	copy_values_memory_t get_copy_source_memory(ptr_t address, size_t count, type const *elem_type);
	bz::vector<bz::u8string> get_get_copy_source_memory_error_reasons(ptr_t address, size_t count, type const *elem_type);
};

enum class global_meta_memory_segment
{
	one_past_the_end,
	functions,
	objects,
};

using global_meta_segment_info_t = memory_segment_info_t<bz::array{
	global_meta_memory_segment::one_past_the_end,
	global_meta_memory_segment::functions,
	global_meta_memory_segment::objects,
}>;

struct global_memory_manager
{
	struct one_past_the_end_pointer
	{
		ptr_t address;
	};

	struct function_pointer
	{
		function *func;
	};

	global_meta_segment_info_t segment_info;
	ptr_t head;

	bz::vector<one_past_the_end_pointer> one_past_the_end_pointers;
	bz::vector<function_pointer> function_pointers;
	bz::vector<global_object> objects;

	explicit global_memory_manager(ptr_t global_memory_begin);

	ptr_t make_global_one_past_the_end_address(ptr_t address);
	one_past_the_end_pointer const &get_one_past_the_end_pointer(ptr_t address) const;

	ptr_t make_unique_function_pointer(function *func);
	function_pointer const &get_function_pointer(ptr_t address) const;

	uint32_t add_object(lex::src_tokens const &object_src_tokens, type const *object_type, bz::fixed_vector<uint8_t> data);

	global_object *get_global_object(ptr_t address);
	global_object const *get_global_object(ptr_t address) const;

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

	uint8_t *get_memory(ptr_t address);
	uint8_t const *get_memory(ptr_t address) const;
};

bz::fixed_vector<uint8_t> object_from_constant_value(
	lex::src_tokens const &src_tokens,
	ast::constant_value const &value,
	type const *object_type,
	endianness_kind endianness,
	global_memory_manager &manager,
	type_set_t &type_set
);

} // namespace comptime::memory

#endif // COMPTIME_GLOBAL_MEMORY_H
