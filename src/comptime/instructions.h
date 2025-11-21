#ifndef COMPTIME_INSTRUCTIONS_H
#define COMPTIME_INSTRUCTIONS_H

#include "core.h"
#include "instructions_forward.h"
#include "types.h"
#include "values.h"
#include "ctx/warnings.h"
#include "ast/typespec.h"
#include "lex/token.h"

namespace comptime
{

struct instruction_index
{
	uint32_t index;
};

struct instruction_value_index
{
	uint32_t index;
};

struct switch_info_t
{
	struct value_instruction_index_pair
	{
		uint64_t value;
		instruction_index dest;
	};

	bz::fixed_vector<value_instruction_index_pair> values;
	instruction_index default_dest;
};

struct switch_str_info_t
{
	struct value_instruction_index_pair
	{
		bz::u8string_view value;
		instruction_index dest;
	};

	bz::fixed_vector<value_instruction_index_pair> values;
	instruction_index default_dest;

	static constexpr auto compare = [](bz::u8string_view lhs, bz::u8string_view rhs) {
		auto const lhs_size = lhs.size();
		auto const rhs_size = rhs.size();
		if (lhs_size < rhs_size)
		{
			return true;
		}
		else if (lhs_size > rhs_size)
		{
			return false;
		}

		return std::memcmp(lhs.data(), rhs.data(), lhs_size) < 0;
	};
};

struct error_info_t
{
	lex::src_tokens src_tokens;
	bz::u8string message;
};

struct slice_construction_check_info_t
{
	type const *elem_type;
	ast::typespec_view slice_type;
};

struct pointer_arithmetic_check_info_t
{
	type const *object_type;
	ast::typespec_view pointer_type;
};

struct memory_access_check_info_t
{
	type const *object_type;
	ast::typespec_view object_typespec;
};

struct add_global_array_data_info_t
{
	type const *elem_type;
	lex::src_tokens src_tokens;
};

struct copy_values_info_t
{
	type const *elem_type;
	uint32_t src_tokens_index;
	bool is_trivially_destructible;
};

struct function
{
	bz::fixed_vector<instruction> instructions;
	bz::fixed_vector<type const *> arg_types;
	type const *return_type = nullptr;

	bz::fixed_vector<alloca> allocas;
	bz::fixed_vector<switch_info_t> switch_infos;
	bz::fixed_vector<switch_str_info_t> switch_str_infos;
	bz::fixed_vector<error_info_t> errors;
	bz::fixed_vector<lex::src_tokens> src_tokens;
	bz::fixed_vector<bz::fixed_vector<instruction_value_index>> call_args;
	bz::fixed_vector<slice_construction_check_info_t> slice_construction_check_infos;
	bz::fixed_vector<pointer_arithmetic_check_info_t> pointer_arithmetic_check_infos;
	bz::fixed_vector<memory_access_check_info_t> memory_access_check_infos;
	bz::fixed_vector<add_global_array_data_info_t> add_global_array_data_infos;
	bz::fixed_vector<copy_values_info_t> copy_values_infos;

	ast::function_body *func_body = nullptr;
	bool can_stack_address_leak = false;
};

namespace instructions
{

template<typename Inst>
inline constexpr size_t arg_count = []() {
	if constexpr (bz::meta::is_same<decltype(Inst::arg_types), const int>)
	{
		return 0;
	}
	else
	{
		return Inst::arg_types.size();
	}
}();


struct const_i1
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::i1;
	static inline constexpr bool invalidates_load_cache = false;

	bool value;
};

struct const_i8
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	int8_t value;
};

struct const_i16
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	int16_t value;
};

struct const_i32
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	int32_t value;
};

struct const_i64
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	int64_t value;
};

struct const_u8
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	uint8_t value;
};

struct const_u16
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	uint16_t value;
};

struct const_u32
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t value;
};

struct const_u64
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	uint64_t value;
};

struct const_f32
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	float32_t value;
};

struct const_f64
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	float64_t value;
};

struct const_ptr_null
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::ptr;
	static inline constexpr bool invalidates_load_cache = false;
};

struct const_func_ptr
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::ptr;
	static inline constexpr bool invalidates_load_cache = false;

	ptr_t value;
};

struct get_global_address
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::ptr;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t global_index;
};

struct get_function_arg
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::any;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t arg_index;
};

struct load_i1_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i1;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct load_i8_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct load_i16_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct load_i32_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct load_i64_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct load_f32_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct load_f64_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct load_ptr32_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::ptr;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct load_ptr64_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::ptr;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct load_i1_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i1;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct load_i8_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct load_i16_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct load_i32_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct load_i64_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct load_f32_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct load_f64_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct load_ptr32_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::ptr;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct load_ptr64_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::ptr;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct store_i1_be
{
	static inline constexpr bz::array arg_types = { value_type::i1, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct store_i8_be
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct store_i16_be
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct store_i32_be
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct store_i64_be
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct store_f32_be
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct store_f64_be
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct store_ptr32_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct store_ptr64_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct store_i1_le
{
	static inline constexpr bz::array arg_types = { value_type::i1, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct store_i8_le
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct store_i16_le
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct store_i32_le
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct store_i64_le
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct store_f32_le
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct store_f64_le
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct store_ptr32_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct store_ptr64_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct check_dereference
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;
	uint32_t memory_access_check_info_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct check_inplace_construct
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;
	uint32_t memory_access_check_info_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct check_destruct_value
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;
	uint32_t memory_access_check_info_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_zext_i1_to_i8
{
	static inline constexpr bz::array arg_types = { value_type::i1 };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_zext_i1_to_i16
{
	static inline constexpr bz::array arg_types = { value_type::i1 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_zext_i1_to_i32
{
	static inline constexpr bz::array arg_types = { value_type::i1 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_zext_i1_to_i64
{
	static inline constexpr bz::array arg_types = { value_type::i1 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_zext_i8_to_i16
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_zext_i8_to_i32
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_zext_i8_to_i64
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_zext_i16_to_i32
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_zext_i16_to_i64
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_zext_i32_to_i64
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_sext_i8_to_i16
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_sext_i8_to_i32
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_sext_i8_to_i64
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_sext_i16_to_i32
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_sext_i16_to_i64
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_sext_i32_to_i64
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_trunc_i64_to_i8
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_trunc_i64_to_i16
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_trunc_i64_to_i32
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_trunc_i32_to_i8
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_trunc_i32_to_i16
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_trunc_i16_to_i8
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_f32_to_f64
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_f64_to_f32
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_f32_to_i8
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_f32_to_i16
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_f32_to_i32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_f32_to_i64
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_f32_to_u8
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_f32_to_u16
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_f32_to_u32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_f32_to_u64
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_f64_to_i8
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_f64_to_i16
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_f64_to_i32
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_f64_to_i64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_f64_to_u8
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_f64_to_u16
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_f64_to_u32
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_f64_to_u64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_i8_to_f32
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_i16_to_f32
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_i32_to_f32
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_i64_to_f32
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_u8_to_f32
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_u16_to_f32
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_u32_to_f32
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_u64_to_f32
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_i8_to_f64
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_i16_to_f64
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_i32_to_f64
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_i64_to_f64
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_u8_to_f64
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_u16_to_f64
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_u32_to_f64
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cast_u64_to_f64
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

enum class cmp_kind : uint32_t
{
	// integer comparisons
	eq,
	neq,
	slt,
	sgt,
	slte,
	sgte,
	ult,
	ugt,
	ulte,
	ugte,

	// pointer comparisons
	ptr_eq = eq,
	ptr_neq = neq,
	ptr_lt = ult,
	ptr_gt = ugt,
	ptr_lte = ulte,
	ptr_gte = ugte,

	// floating-point comparisons
	feq = eq,
	fneq = neq,
	flt = slt,
	fgt = sgt,
	flte = slte,
	fgte = sgte,
};

constexpr bool is_equality_compare(cmp_kind kind)
{
	switch (kind)
	{
	case cmp_kind::eq:
	case cmp_kind::neq:
		return true;
	default:
		return false;
	}
}

constexpr bool is_unsigned_compare(cmp_kind kind)
{
	switch (kind)
	{
	case cmp_kind::ult:
	case cmp_kind::ugt:
	case cmp_kind::ulte:
	case cmp_kind::ugte:
		return true;
	default:
		return false;
	}
}

constexpr bz::u8string_view get_compare_operator(cmp_kind kind)
{
	switch (kind)
	{
	case cmp_kind::eq:
		return "==";
	case cmp_kind::neq:
		return "!=";
	case cmp_kind::slt:
	case cmp_kind::ult:
		return "<";
	case cmp_kind::sgt:
	case cmp_kind::ugt:
		return ">";
	case cmp_kind::slte:
	case cmp_kind::ulte:
		return "<=";
	case cmp_kind::sgte:
	case cmp_kind::ugte:
		return ">=";
	}
}

struct cmp_i1
{
	static inline constexpr bz::array arg_types = { value_type::i1, value_type::i1 };
	static inline constexpr value_type result_type = value_type::i1;
	static inline constexpr bool invalidates_load_cache = false;

	cmp_kind kind;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cmp_i8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i1;
	static inline constexpr bool invalidates_load_cache = false;

	cmp_kind kind;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cmp_i16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i1;
	static inline constexpr bool invalidates_load_cache = false;

	cmp_kind kind;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cmp_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i1;
	static inline constexpr bool invalidates_load_cache = false;

	cmp_kind kind;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cmp_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i1;
	static inline constexpr bool invalidates_load_cache = false;

	cmp_kind kind;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cmp_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::i1;
	static inline constexpr bool invalidates_load_cache = false;

	cmp_kind kind;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cmp_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::i1;
	static inline constexpr bool invalidates_load_cache = false;

	cmp_kind kind;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cmp_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;
	cmp_kind kind;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cmp_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;
	cmp_kind kind;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cmp_ptr
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::i1;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;
	cmp_kind kind;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct neg_i8
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct neg_i16
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct neg_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct neg_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct neg_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct neg_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct neg_i8_check
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct neg_i16_check
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct neg_i32_check
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct neg_i64_check
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct add_i8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct add_i16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct add_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct add_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct add_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct add_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct add_ptr_i32
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::i32 };
	static inline constexpr value_type result_type = value_type::ptr;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;
	uint32_t pointer_arithmetic_check_info_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct add_ptr_u32
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::i32 };
	static inline constexpr value_type result_type = value_type::ptr;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;
	uint32_t pointer_arithmetic_check_info_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct add_ptr_i64
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::i64 };
	static inline constexpr value_type result_type = value_type::ptr;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;
	uint32_t pointer_arithmetic_check_info_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct add_ptr_u64
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::i64 };
	static inline constexpr value_type result_type = value_type::ptr;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;
	uint32_t pointer_arithmetic_check_info_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct add_ptr_const_unchecked
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::ptr;
	static inline constexpr bool invalidates_load_cache = false;

	type const *object_type;
	int32_t amount;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct add_i8_check
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct add_i16_check
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct add_i32_check
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct add_i64_check
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct add_u8_check
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct add_u16_check
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct add_u32_check
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct add_u64_check
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct add_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct add_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct sub_i8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct sub_i16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct sub_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct sub_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct sub_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct sub_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct sub_ptr_i32
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::i32 };
	static inline constexpr value_type result_type = value_type::ptr;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;
	uint32_t pointer_arithmetic_check_info_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct sub_ptr_u32
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::i32 };
	static inline constexpr value_type result_type = value_type::ptr;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;
	uint32_t pointer_arithmetic_check_info_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct sub_ptr_i64
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::i64 };
	static inline constexpr value_type result_type = value_type::ptr;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;
	uint32_t pointer_arithmetic_check_info_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct sub_ptr_u64
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::i64 };
	static inline constexpr value_type result_type = value_type::ptr;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;
	uint32_t pointer_arithmetic_check_info_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct sub_i8_check
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct sub_i16_check
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct sub_i32_check
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct sub_i64_check
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct sub_u8_check
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct sub_u16_check
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct sub_u32_check
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct sub_u64_check
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct sub_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct sub_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct ptr32_diff
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;
	uint32_t pointer_arithmetic_check_info_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct ptr64_diff
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;
	uint32_t pointer_arithmetic_check_info_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct ptr32_diff_unchecked
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	size_t stride;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct ptr64_diff_unchecked
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	size_t stride;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct mul_i8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct mul_i16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct mul_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct mul_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct mul_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct mul_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct mul_i8_check
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct mul_i16_check
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct mul_i32_check
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct mul_i64_check
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct mul_u8_check
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct mul_u16_check
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct mul_u32_check
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct mul_u64_check
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct mul_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct mul_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct div_i8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct div_i16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct div_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct div_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct div_u8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct div_u16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct div_u32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct div_u64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct div_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct div_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct div_i8_check
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct div_i16_check
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct div_i32_check
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct div_i64_check
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct div_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct div_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct rem_i8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct rem_i16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct rem_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct rem_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct rem_u8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct rem_u16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct rem_u32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct rem_u64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct not_i1
{
	static inline constexpr bz::array arg_types = { value_type::i1 };
	static inline constexpr value_type result_type = value_type::i1;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct not_i8
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct not_i16
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct not_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct not_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct and_i1
{
	static inline constexpr bz::array arg_types = { value_type::i1, value_type::i1 };
	static inline constexpr value_type result_type = value_type::i1;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct and_i8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct and_i16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct and_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct and_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct xor_i1
{
	static inline constexpr bz::array arg_types = { value_type::i1, value_type::i1 };
	static inline constexpr value_type result_type = value_type::i1;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct xor_i8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct xor_i16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct xor_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct xor_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct or_i1
{
	static inline constexpr bz::array arg_types = { value_type::i1, value_type::i1 };
	static inline constexpr value_type result_type = value_type::i1;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct or_i8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct or_i16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct or_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct or_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct shl_i8_signed
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct shl_i16_signed
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct shl_i32_signed
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct shl_i64_signed
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct shl_i8_unsigned
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct shl_i16_unsigned
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct shl_i32_unsigned
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct shl_i64_unsigned
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct shr_i8_signed
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct shr_i16_signed
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct shr_i32_signed
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct shr_i64_signed
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct shr_i8_unsigned
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct shr_i16_unsigned
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct shr_i32_unsigned
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct shr_i64_unsigned
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct isnan_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::i1;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct isnan_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::i1;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct isinf_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::i1;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct isinf_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::i1;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct isfinite_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::i1;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct isfinite_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::i1;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct isnormal_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::i1;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct isnormal_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::i1;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct issubnormal_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::i1;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct issubnormal_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::i1;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct iszero_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::i1;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct iszero_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::i1;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct nextafter_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct nextafter_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct abs_i8
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct abs_i16
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct abs_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct abs_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct abs_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct abs_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct abs_i8_check
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct abs_i16_check
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct abs_i32_check
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct abs_i64_check
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct abs_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct abs_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct min_i8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct min_i16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct min_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct min_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct min_u8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct min_u16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct min_u32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct min_u64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct min_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct min_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct min_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct min_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct max_i8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct max_i16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct max_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct max_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct max_u8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct max_u16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct max_u32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct max_u64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct max_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct max_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct max_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct max_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct exp_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct exp_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct exp_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct exp_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct exp2_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct exp2_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct exp2_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct exp2_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct expm1_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct expm1_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct expm1_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct expm1_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct log_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct log_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct log_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct log_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct log10_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct log10_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct log10_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct log10_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct log2_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct log2_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct log2_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct log2_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct log1p_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct log1p_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct log1p_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct log1p_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct sqrt_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct sqrt_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct sqrt_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct sqrt_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct pow_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct pow_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct pow_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct pow_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cbrt_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cbrt_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cbrt_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cbrt_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct hypot_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct hypot_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct hypot_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct hypot_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct sin_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct sin_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct sin_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct sin_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cos_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cos_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cos_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cos_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct tan_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct tan_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct tan_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct tan_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct asin_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct asin_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct asin_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct asin_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct acos_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct acos_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct acos_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct acos_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct atan_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct atan_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct atan_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct atan_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct atan2_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct atan2_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct atan2_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct atan2_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct sinh_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct sinh_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct sinh_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct sinh_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cosh_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cosh_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cosh_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct cosh_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct tanh_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct tanh_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct tanh_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct tanh_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct asinh_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct asinh_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct asinh_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct asinh_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct acosh_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct acosh_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct acosh_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct acosh_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct atanh_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct atanh_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct atanh_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct atanh_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct erf_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct erf_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct erf_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct erf_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct erfc_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct erfc_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct erfc_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct erfc_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct tgamma_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct tgamma_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct tgamma_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct tgamma_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct lgamma_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct lgamma_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct lgamma_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct lgamma_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct bitreverse_u8
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct bitreverse_u16
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct bitreverse_u32
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct bitreverse_u64
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct popcount_u8
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct popcount_u16
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct popcount_u32
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct popcount_u64
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct byteswap_u16
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct byteswap_u32
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct byteswap_u64
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct clz_u8
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct clz_u16
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct clz_u32
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct clz_u64
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct ctz_u8
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct ctz_u16
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct ctz_u32
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct ctz_u64
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct fshl_u8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct fshl_u16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct fshl_u32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct fshl_u64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct fshr_u8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct fshr_u16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct fshr_u32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct fshr_u64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct ashr_u8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct ashr_u16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct ashr_u32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct ashr_u64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct const_gep
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::ptr;
	static inline constexpr bool invalidates_load_cache = false;

	type const *object_type;
	uint32_t index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct array_gep_i32
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::i32 };
	static inline constexpr value_type result_type = value_type::ptr;
	static inline constexpr bool invalidates_load_cache = false;

	type const *elem_type;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct array_gep_i64
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::i64 };
	static inline constexpr value_type result_type = value_type::ptr;
	static inline constexpr bool invalidates_load_cache = false;

	type const *elem_type;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct const_memcpy
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	size_t size;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct const_memset_zero
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	size_t size;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct copy_values
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	uint32_t copy_values_info_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct copy_overlapping_values
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	uint32_t copy_values_info_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct relocate_values
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	uint32_t copy_values_info_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct set_values_i1_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::i1, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct set_values_i8_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::i8, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct set_values_i16_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::i16, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct set_values_i32_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::i32, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct set_values_i64_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct set_values_f32_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::f32, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct set_values_f64_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::f64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct set_values_ptr32_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct set_values_ptr64_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct set_values_i1_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::i1, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct set_values_i8_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::i8, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct set_values_i16_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::i16, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct set_values_i32_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::i32, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct set_values_i64_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct set_values_f32_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::f32, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct set_values_f64_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::f64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct set_values_ptr32_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct set_values_ptr64_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct set_values_ref
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	uint32_t copy_values_info_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct function_call
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	function *func;
	uint32_t args_index;
	uint32_t src_tokens_index;
};

struct indirect_function_call
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = true;

	uint32_t args_index;
	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct malloc
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::ptr;
	static inline constexpr bool invalidates_load_cache = false;

	type const *elem_type;
	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct free
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct jump
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	instruction_index dest;
};

struct conditional_jump
{
	static inline constexpr bz::array arg_types = { value_type::i1 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	instruction_index true_dest;
	instruction_index false_dest;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct switch_i1
{
	static inline constexpr bz::array arg_types = { value_type::i1 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t switch_info_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct switch_i8
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t switch_info_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct switch_i16
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t switch_info_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct switch_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t switch_info_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct switch_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t switch_info_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct switch_str
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t switch_str_info_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct ret
{
	static inline constexpr bz::array arg_types = { value_type::any };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct ret_void
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;
};

struct unreachable
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;
};

struct error
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t error_index;
};

struct diagnostic_str
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;
	ctx::warning_kind kind;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct print
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct is_option_set
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::i1;
	static inline constexpr bool invalidates_load_cache = false;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct add_global_array_data
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::ptr;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t add_global_array_data_info_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct range_bounds_check_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct range_bounds_check_u64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct array_bounds_check_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct array_bounds_check_u32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct array_bounds_check_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct array_bounds_check_u64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct array_range_bounds_check_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct array_range_bounds_check_u32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct array_range_bounds_check_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct array_range_bounds_check_u64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct array_range_begin_bounds_check_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct array_range_begin_bounds_check_u32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct array_range_begin_bounds_check_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct array_range_begin_bounds_check_u64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct array_range_end_bounds_check_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct array_range_end_bounds_check_u32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct array_range_end_bounds_check_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct array_range_end_bounds_check_u64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct optional_get_value_check
{
	static inline constexpr bz::array arg_types = { value_type::i1 };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct str_construction_check
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct slice_construction_check
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	uint32_t src_tokens_index;
	uint32_t slice_construction_check_info_index;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct start_lifetime
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	size_t size;

	bz::array<instruction_value_index, arg_types.size()> args;
};

struct end_lifetime
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
	static inline constexpr bool invalidates_load_cache = false;

	size_t size;

	bz::array<instruction_value_index, arg_types.size()> args;
};

} // namespace instructions

using instruction_list_t = bz::meta::type_pack<
	instructions::const_i1,
	instructions::const_i8,
	instructions::const_i16,
	instructions::const_i32,
	instructions::const_i64,
	instructions::const_u8,
	instructions::const_u16,
	instructions::const_u32,
	instructions::const_u64,
	instructions::const_f32,
	instructions::const_f64,
	instructions::const_ptr_null,
	instructions::const_func_ptr,
	instructions::get_global_address,
	instructions::get_function_arg,
	instructions::load_i1_be,
	instructions::load_i8_be,
	instructions::load_i16_be,
	instructions::load_i32_be,
	instructions::load_i64_be,
	instructions::load_f32_be,
	instructions::load_f64_be,
	instructions::load_ptr32_be,
	instructions::load_ptr64_be,
	instructions::load_i1_le,
	instructions::load_i8_le,
	instructions::load_i16_le,
	instructions::load_i32_le,
	instructions::load_i64_le,
	instructions::load_f32_le,
	instructions::load_f64_le,
	instructions::load_ptr32_le,
	instructions::load_ptr64_le,
	instructions::store_i1_be,
	instructions::store_i8_be,
	instructions::store_i16_be,
	instructions::store_i32_be,
	instructions::store_i64_be,
	instructions::store_f32_be,
	instructions::store_f64_be,
	instructions::store_ptr32_be,
	instructions::store_ptr64_be,
	instructions::store_i1_le,
	instructions::store_i8_le,
	instructions::store_i16_le,
	instructions::store_i32_le,
	instructions::store_i64_le,
	instructions::store_f32_le,
	instructions::store_f64_le,
	instructions::store_ptr32_le,
	instructions::store_ptr64_le,
	instructions::check_dereference,
	instructions::check_inplace_construct,
	instructions::check_destruct_value,
	instructions::cast_zext_i1_to_i8,
	instructions::cast_zext_i1_to_i16,
	instructions::cast_zext_i1_to_i32,
	instructions::cast_zext_i1_to_i64,
	instructions::cast_zext_i8_to_i16,
	instructions::cast_zext_i8_to_i32,
	instructions::cast_zext_i8_to_i64,
	instructions::cast_zext_i16_to_i32,
	instructions::cast_zext_i16_to_i64,
	instructions::cast_zext_i32_to_i64,
	instructions::cast_sext_i8_to_i16,
	instructions::cast_sext_i8_to_i32,
	instructions::cast_sext_i8_to_i64,
	instructions::cast_sext_i16_to_i32,
	instructions::cast_sext_i16_to_i64,
	instructions::cast_sext_i32_to_i64,
	instructions::cast_trunc_i64_to_i8,
	instructions::cast_trunc_i64_to_i16,
	instructions::cast_trunc_i64_to_i32,
	instructions::cast_trunc_i32_to_i8,
	instructions::cast_trunc_i32_to_i16,
	instructions::cast_trunc_i16_to_i8,
	instructions::cast_f32_to_f64,
	instructions::cast_f64_to_f32,
	instructions::cast_f32_to_i8,
	instructions::cast_f32_to_i16,
	instructions::cast_f32_to_i32,
	instructions::cast_f32_to_i64,
	instructions::cast_f32_to_u8,
	instructions::cast_f32_to_u16,
	instructions::cast_f32_to_u32,
	instructions::cast_f32_to_u64,
	instructions::cast_f64_to_i8,
	instructions::cast_f64_to_i16,
	instructions::cast_f64_to_i32,
	instructions::cast_f64_to_i64,
	instructions::cast_f64_to_u8,
	instructions::cast_f64_to_u16,
	instructions::cast_f64_to_u32,
	instructions::cast_f64_to_u64,
	instructions::cast_i8_to_f32,
	instructions::cast_i16_to_f32,
	instructions::cast_i32_to_f32,
	instructions::cast_i64_to_f32,
	instructions::cast_u8_to_f32,
	instructions::cast_u16_to_f32,
	instructions::cast_u32_to_f32,
	instructions::cast_u64_to_f32,
	instructions::cast_i8_to_f64,
	instructions::cast_i16_to_f64,
	instructions::cast_i32_to_f64,
	instructions::cast_i64_to_f64,
	instructions::cast_u8_to_f64,
	instructions::cast_u16_to_f64,
	instructions::cast_u32_to_f64,
	instructions::cast_u64_to_f64,
	instructions::cmp_i1,
	instructions::cmp_i8,
	instructions::cmp_i16,
	instructions::cmp_i32,
	instructions::cmp_i64,
	instructions::cmp_f32,
	instructions::cmp_f64,
	instructions::cmp_f32_check,
	instructions::cmp_f64_check,
	instructions::cmp_ptr,
	instructions::neg_i8,
	instructions::neg_i16,
	instructions::neg_i32,
	instructions::neg_i64,
	instructions::neg_f32,
	instructions::neg_f64,
	instructions::neg_i8_check,
	instructions::neg_i16_check,
	instructions::neg_i32_check,
	instructions::neg_i64_check,
	instructions::add_i8,
	instructions::add_i16,
	instructions::add_i32,
	instructions::add_i64,
	instructions::add_f32,
	instructions::add_f64,
	instructions::add_ptr_i32,
	instructions::add_ptr_u32,
	instructions::add_ptr_i64,
	instructions::add_ptr_u64,
	instructions::add_ptr_const_unchecked,
	instructions::add_i8_check,
	instructions::add_i16_check,
	instructions::add_i32_check,
	instructions::add_i64_check,
	instructions::add_u8_check,
	instructions::add_u16_check,
	instructions::add_u32_check,
	instructions::add_u64_check,
	instructions::add_f32_check,
	instructions::add_f64_check,
	instructions::sub_i8,
	instructions::sub_i16,
	instructions::sub_i32,
	instructions::sub_i64,
	instructions::sub_f32,
	instructions::sub_f64,
	instructions::sub_ptr_i32,
	instructions::sub_ptr_u32,
	instructions::sub_ptr_i64,
	instructions::sub_ptr_u64,
	instructions::sub_i8_check,
	instructions::sub_i16_check,
	instructions::sub_i32_check,
	instructions::sub_i64_check,
	instructions::sub_u8_check,
	instructions::sub_u16_check,
	instructions::sub_u32_check,
	instructions::sub_u64_check,
	instructions::sub_f32_check,
	instructions::sub_f64_check,
	instructions::ptr32_diff,
	instructions::ptr64_diff,
	instructions::ptr32_diff_unchecked,
	instructions::ptr64_diff_unchecked,
	instructions::mul_i8,
	instructions::mul_i16,
	instructions::mul_i32,
	instructions::mul_i64,
	instructions::mul_f32,
	instructions::mul_f64,
	instructions::mul_i8_check,
	instructions::mul_i16_check,
	instructions::mul_i32_check,
	instructions::mul_i64_check,
	instructions::mul_u8_check,
	instructions::mul_u16_check,
	instructions::mul_u32_check,
	instructions::mul_u64_check,
	instructions::mul_f32_check,
	instructions::mul_f64_check,
	instructions::div_i8,
	instructions::div_i16,
	instructions::div_i32,
	instructions::div_i64,
	instructions::div_u8,
	instructions::div_u16,
	instructions::div_u32,
	instructions::div_u64,
	instructions::div_f32,
	instructions::div_f64,
	instructions::div_i8_check,
	instructions::div_i16_check,
	instructions::div_i32_check,
	instructions::div_i64_check,
	instructions::div_f32_check,
	instructions::div_f64_check,
	instructions::rem_i8,
	instructions::rem_i16,
	instructions::rem_i32,
	instructions::rem_i64,
	instructions::rem_u8,
	instructions::rem_u16,
	instructions::rem_u32,
	instructions::rem_u64,
	instructions::not_i1,
	instructions::not_i8,
	instructions::not_i16,
	instructions::not_i32,
	instructions::not_i64,
	instructions::and_i1,
	instructions::and_i8,
	instructions::and_i16,
	instructions::and_i32,
	instructions::and_i64,
	instructions::xor_i1,
	instructions::xor_i8,
	instructions::xor_i16,
	instructions::xor_i32,
	instructions::xor_i64,
	instructions::or_i1,
	instructions::or_i8,
	instructions::or_i16,
	instructions::or_i32,
	instructions::or_i64,
	instructions::shl_i8_signed,
	instructions::shl_i16_signed,
	instructions::shl_i32_signed,
	instructions::shl_i64_signed,
	instructions::shl_i8_unsigned,
	instructions::shl_i16_unsigned,
	instructions::shl_i32_unsigned,
	instructions::shl_i64_unsigned,
	instructions::shr_i8_signed,
	instructions::shr_i16_signed,
	instructions::shr_i32_signed,
	instructions::shr_i64_signed,
	instructions::shr_i8_unsigned,
	instructions::shr_i16_unsigned,
	instructions::shr_i32_unsigned,
	instructions::shr_i64_unsigned,
	instructions::isnan_f32,
	instructions::isnan_f64,
	instructions::isinf_f32,
	instructions::isinf_f64,
	instructions::isfinite_f32,
	instructions::isfinite_f64,
	instructions::isnormal_f32,
	instructions::isnormal_f64,
	instructions::issubnormal_f32,
	instructions::issubnormal_f64,
	instructions::iszero_f32,
	instructions::iszero_f64,
	instructions::nextafter_f32,
	instructions::nextafter_f64,
	instructions::abs_i8,
	instructions::abs_i16,
	instructions::abs_i32,
	instructions::abs_i64,
	instructions::abs_f32,
	instructions::abs_f64,
	instructions::abs_i8_check,
	instructions::abs_i16_check,
	instructions::abs_i32_check,
	instructions::abs_i64_check,
	instructions::abs_f32_check,
	instructions::abs_f64_check,
	instructions::min_i8,
	instructions::min_i16,
	instructions::min_i32,
	instructions::min_i64,
	instructions::min_u8,
	instructions::min_u16,
	instructions::min_u32,
	instructions::min_u64,
	instructions::min_f32,
	instructions::min_f64,
	instructions::min_f32_check,
	instructions::min_f64_check,
	instructions::max_i8,
	instructions::max_i16,
	instructions::max_i32,
	instructions::max_i64,
	instructions::max_u8,
	instructions::max_u16,
	instructions::max_u32,
	instructions::max_u64,
	instructions::max_f32,
	instructions::max_f64,
	instructions::max_f32_check,
	instructions::max_f64_check,
	instructions::exp_f32,
	instructions::exp_f64,
	instructions::exp_f32_check,
	instructions::exp_f64_check,
	instructions::exp2_f32,
	instructions::exp2_f64,
	instructions::exp2_f32_check,
	instructions::exp2_f64_check,
	instructions::expm1_f32,
	instructions::expm1_f64,
	instructions::expm1_f32_check,
	instructions::expm1_f64_check,
	instructions::log_f32,
	instructions::log_f64,
	instructions::log_f32_check,
	instructions::log_f64_check,
	instructions::log10_f32,
	instructions::log10_f64,
	instructions::log10_f32_check,
	instructions::log10_f64_check,
	instructions::log2_f32,
	instructions::log2_f64,
	instructions::log2_f32_check,
	instructions::log2_f64_check,
	instructions::log1p_f32,
	instructions::log1p_f64,
	instructions::log1p_f32_check,
	instructions::log1p_f64_check,
	instructions::sqrt_f32,
	instructions::sqrt_f64,
	instructions::sqrt_f32_check,
	instructions::sqrt_f64_check,
	instructions::pow_f32,
	instructions::pow_f64,
	instructions::pow_f32_check,
	instructions::pow_f64_check,
	instructions::cbrt_f32,
	instructions::cbrt_f64,
	instructions::cbrt_f32_check,
	instructions::cbrt_f64_check,
	instructions::hypot_f32,
	instructions::hypot_f64,
	instructions::hypot_f32_check,
	instructions::hypot_f64_check,
	instructions::sin_f32,
	instructions::sin_f64,
	instructions::sin_f32_check,
	instructions::sin_f64_check,
	instructions::cos_f32,
	instructions::cos_f64,
	instructions::cos_f32_check,
	instructions::cos_f64_check,
	instructions::tan_f32,
	instructions::tan_f64,
	instructions::tan_f32_check,
	instructions::tan_f64_check,
	instructions::asin_f32,
	instructions::asin_f64,
	instructions::asin_f32_check,
	instructions::asin_f64_check,
	instructions::acos_f32,
	instructions::acos_f64,
	instructions::acos_f32_check,
	instructions::acos_f64_check,
	instructions::atan_f32,
	instructions::atan_f64,
	instructions::atan_f32_check,
	instructions::atan_f64_check,
	instructions::atan2_f32,
	instructions::atan2_f64,
	instructions::atan2_f32_check,
	instructions::atan2_f64_check,
	instructions::sinh_f32,
	instructions::sinh_f64,
	instructions::sinh_f32_check,
	instructions::sinh_f64_check,
	instructions::cosh_f32,
	instructions::cosh_f64,
	instructions::cosh_f32_check,
	instructions::cosh_f64_check,
	instructions::tanh_f32,
	instructions::tanh_f64,
	instructions::tanh_f32_check,
	instructions::tanh_f64_check,
	instructions::asinh_f32,
	instructions::asinh_f64,
	instructions::asinh_f32_check,
	instructions::asinh_f64_check,
	instructions::acosh_f32,
	instructions::acosh_f64,
	instructions::acosh_f32_check,
	instructions::acosh_f64_check,
	instructions::atanh_f32,
	instructions::atanh_f64,
	instructions::atanh_f32_check,
	instructions::atanh_f64_check,
	instructions::erf_f32,
	instructions::erf_f64,
	instructions::erf_f32_check,
	instructions::erf_f64_check,
	instructions::erfc_f32,
	instructions::erfc_f64,
	instructions::erfc_f32_check,
	instructions::erfc_f64_check,
	instructions::tgamma_f32,
	instructions::tgamma_f64,
	instructions::tgamma_f32_check,
	instructions::tgamma_f64_check,
	instructions::lgamma_f32,
	instructions::lgamma_f64,
	instructions::lgamma_f32_check,
	instructions::lgamma_f64_check,
	instructions::bitreverse_u8,
	instructions::bitreverse_u16,
	instructions::bitreverse_u32,
	instructions::bitreverse_u64,
	instructions::popcount_u8,
	instructions::popcount_u16,
	instructions::popcount_u32,
	instructions::popcount_u64,
	instructions::byteswap_u16,
	instructions::byteswap_u32,
	instructions::byteswap_u64,
	instructions::clz_u8,
	instructions::clz_u16,
	instructions::clz_u32,
	instructions::clz_u64,
	instructions::ctz_u8,
	instructions::ctz_u16,
	instructions::ctz_u32,
	instructions::ctz_u64,
	instructions::fshl_u8,
	instructions::fshl_u16,
	instructions::fshl_u32,
	instructions::fshl_u64,
	instructions::fshr_u8,
	instructions::fshr_u16,
	instructions::fshr_u32,
	instructions::fshr_u64,
	instructions::ashr_u8,
	instructions::ashr_u16,
	instructions::ashr_u32,
	instructions::ashr_u64,
	instructions::const_gep,
	instructions::array_gep_i32,
	instructions::array_gep_i64,
	instructions::const_memcpy,
	instructions::const_memset_zero,
	instructions::copy_values,
	instructions::copy_overlapping_values,
	instructions::relocate_values,
	instructions::set_values_i1_be,
	instructions::set_values_i8_be,
	instructions::set_values_i16_be,
	instructions::set_values_i32_be,
	instructions::set_values_i64_be,
	instructions::set_values_f32_be,
	instructions::set_values_f64_be,
	instructions::set_values_ptr32_be,
	instructions::set_values_ptr64_be,
	instructions::set_values_i1_le,
	instructions::set_values_i8_le,
	instructions::set_values_i16_le,
	instructions::set_values_i32_le,
	instructions::set_values_i64_le,
	instructions::set_values_f32_le,
	instructions::set_values_f64_le,
	instructions::set_values_ptr32_le,
	instructions::set_values_ptr64_le,
	instructions::set_values_ref,
	instructions::function_call,
	instructions::indirect_function_call,
	instructions::malloc,
	instructions::free,
	instructions::jump,
	instructions::conditional_jump,
	instructions::switch_i1,
	instructions::switch_i8,
	instructions::switch_i16,
	instructions::switch_i32,
	instructions::switch_i64,
	instructions::switch_str,
	instructions::ret,
	instructions::ret_void,
	instructions::unreachable,
	instructions::error,
	instructions::diagnostic_str,
	instructions::print,
	instructions::is_option_set,
	instructions::add_global_array_data,
	instructions::range_bounds_check_i64,
	instructions::range_bounds_check_u64,
	instructions::array_bounds_check_i32,
	instructions::array_bounds_check_u32,
	instructions::array_bounds_check_i64,
	instructions::array_bounds_check_u64,
	instructions::array_range_bounds_check_i32,
	instructions::array_range_bounds_check_u32,
	instructions::array_range_bounds_check_i64,
	instructions::array_range_bounds_check_u64,
	instructions::array_range_begin_bounds_check_i32,
	instructions::array_range_begin_bounds_check_u32,
	instructions::array_range_begin_bounds_check_i64,
	instructions::array_range_begin_bounds_check_u64,
	instructions::array_range_end_bounds_check_i32,
	instructions::array_range_end_bounds_check_u32,
	instructions::array_range_end_bounds_check_i64,
	instructions::array_range_end_bounds_check_u64,
	instructions::optional_get_value_check,
	instructions::str_construction_check,
	instructions::slice_construction_check,
	instructions::start_lifetime,
	instructions::end_lifetime
>;

namespace internal
{

template<typename T, typename Pack = instruction_list_t>
inline constexpr void *index_of = nullptr;

template<typename T, typename ...Ts>
	requires std::same_as<T, void> || bz::meta::is_in_type_pack<T, instruction_list_t>
inline constexpr size_t index_of<
	T,
	bz::meta::type_pack<Ts...>
> = bz::meta::type_index<T, void, Ts...>;

} // namespace internal

struct instruction
{
private:
	static inline constexpr size_t data_size = []<typename ...Ts>(bz::meta::type_pack<Ts...>) {
		return std::max({ sizeof (Ts)... });
	}(instruction_list_t());
	static inline constexpr size_t data_align = []<typename ...Ts>(bz::meta::type_pack<Ts...>) {
		return std::max({ alignof (Ts)... });
	}(instruction_list_t());
	static_assert(data_size <= 16);
	static_assert(data_align == 8);

public:
	template<typename T>
	static inline constexpr uint64_t index_of = internal::index_of<T>;

	static_assert(index_of<void> == 0);

private:
	alignas(data_align) uint8_t _data[data_size]{};
	uint64_t _index = index_of<void>;

public:
	static_assert(instruction_list_t::size() == 514);
	enum : uint64_t
	{
		const_i1 = index_of<instructions::const_i1>,
		const_i8,
		const_i16,
		const_i32,
		const_i64,
		const_u8,
		const_u16,
		const_u32,
		const_u64,
		const_f32,
		const_f64,
		const_ptr_null,
		const_func_ptr,
		get_global_address,
		get_function_arg,
		load_i1_be,
		load_i8_be,
		load_i16_be,
		load_i32_be,
		load_i64_be,
		load_f32_be,
		load_f64_be,
		load_ptr32_be,
		load_ptr64_be,
		load_i1_le,
		load_i8_le,
		load_i16_le,
		load_i32_le,
		load_i64_le,
		load_f32_le,
		load_f64_le,
		load_ptr32_le,
		load_ptr64_le,
		store_i1_be,
		store_i8_be,
		store_i16_be,
		store_i32_be,
		store_i64_be,
		store_f32_be,
		store_f64_be,
		store_ptr32_be,
		store_ptr64_be,
		store_i1_le,
		store_i8_le,
		store_i16_le,
		store_i32_le,
		store_i64_le,
		store_f32_le,
		store_f64_le,
		store_ptr32_le,
		store_ptr64_le,
		check_dereference,
		check_inplace_construct,
		check_destruct_value,
		cast_zext_i1_to_i8,
		cast_zext_i1_to_i16,
		cast_zext_i1_to_i32,
		cast_zext_i1_to_i64,
		cast_zext_i8_to_i16,
		cast_zext_i8_to_i32,
		cast_zext_i8_to_i64,
		cast_zext_i16_to_i32,
		cast_zext_i16_to_i64,
		cast_zext_i32_to_i64,
		cast_sext_i8_to_i16,
		cast_sext_i8_to_i32,
		cast_sext_i8_to_i64,
		cast_sext_i16_to_i32,
		cast_sext_i16_to_i64,
		cast_sext_i32_to_i64,
		cast_trunc_i64_to_i8,
		cast_trunc_i64_to_i16,
		cast_trunc_i64_to_i32,
		cast_trunc_i32_to_i8,
		cast_trunc_i32_to_i16,
		cast_trunc_i16_to_i8,
		cast_f32_to_f64,
		cast_f64_to_f32,
		cast_f32_to_i8,
		cast_f32_to_i16,
		cast_f32_to_i32,
		cast_f32_to_i64,
		cast_f32_to_u8,
		cast_f32_to_u16,
		cast_f32_to_u32,
		cast_f32_to_u64,
		cast_f64_to_i8,
		cast_f64_to_i16,
		cast_f64_to_i32,
		cast_f64_to_i64,
		cast_f64_to_u8,
		cast_f64_to_u16,
		cast_f64_to_u32,
		cast_f64_to_u64,
		cast_i8_to_f32,
		cast_i16_to_f32,
		cast_i32_to_f32,
		cast_i64_to_f32,
		cast_u8_to_f32,
		cast_u16_to_f32,
		cast_u32_to_f32,
		cast_u64_to_f32,
		cast_i8_to_f64,
		cast_i16_to_f64,
		cast_i32_to_f64,
		cast_i64_to_f64,
		cast_u8_to_f64,
		cast_u16_to_f64,
		cast_u32_to_f64,
		cast_u64_to_f64,
		cmp_i1,
		cmp_i8,
		cmp_i16,
		cmp_i32,
		cmp_i64,
		cmp_f32,
		cmp_f64,
		cmp_f32_check,
		cmp_f64_check,
		cmp_ptr,
		neg_i8,
		neg_i16,
		neg_i32,
		neg_i64,
		neg_f32,
		neg_f64,
		neg_i8_check,
		neg_i16_check,
		neg_i32_check,
		neg_i64_check,
		add_i8,
		add_i16,
		add_i32,
		add_i64,
		add_f32,
		add_f64,
		add_ptr_i32,
		add_ptr_u32,
		add_ptr_i64,
		add_ptr_u64,
		add_ptr_const_unchecked,
		add_i8_check,
		add_i16_check,
		add_i32_check,
		add_i64_check,
		add_u8_check,
		add_u16_check,
		add_u32_check,
		add_u64_check,
		add_f32_check,
		add_f64_check,
		sub_i8,
		sub_i16,
		sub_i32,
		sub_i64,
		sub_f32,
		sub_f64,
		sub_ptr_i32,
		sub_ptr_u32,
		sub_ptr_i64,
		sub_ptr_u64,
		sub_i8_check,
		sub_i16_check,
		sub_i32_check,
		sub_i64_check,
		sub_u8_check,
		sub_u16_check,
		sub_u32_check,
		sub_u64_check,
		sub_f32_check,
		sub_f64_check,
		ptr32_diff,
		ptr64_diff,
		ptr32_diff_unchecked,
		ptr64_diff_unchecked,
		mul_i8,
		mul_i16,
		mul_i32,
		mul_i64,
		mul_f32,
		mul_f64,
		mul_i8_check,
		mul_i16_check,
		mul_i32_check,
		mul_i64_check,
		mul_u8_check,
		mul_u16_check,
		mul_u32_check,
		mul_u64_check,
		mul_f32_check,
		mul_f64_check,
		div_i8,
		div_i16,
		div_i32,
		div_i64,
		div_u8,
		div_u16,
		div_u32,
		div_u64,
		div_f32,
		div_f64,
		div_i8_check,
		div_i16_check,
		div_i32_check,
		div_i64_check,
		div_f32_check,
		div_f64_check,
		rem_i8,
		rem_i16,
		rem_i32,
		rem_i64,
		rem_u8,
		rem_u16,
		rem_u32,
		rem_u64,
		not_i1,
		not_i8,
		not_i16,
		not_i32,
		not_i64,
		and_i1,
		and_i8,
		and_i16,
		and_i32,
		and_i64,
		xor_i1,
		xor_i8,
		xor_i16,
		xor_i32,
		xor_i64,
		or_i1,
		or_i8,
		or_i16,
		or_i32,
		or_i64,
		shl_i8_signed,
		shl_i16_signed,
		shl_i32_signed,
		shl_i64_signed,
		shl_i8_unsigned,
		shl_i16_unsigned,
		shl_i32_unsigned,
		shl_i64_unsigned,
		shr_i8_signed,
		shr_i16_signed,
		shr_i32_signed,
		shr_i64_signed,
		shr_i8_unsigned,
		shr_i16_unsigned,
		shr_i32_unsigned,
		shr_i64_unsigned,
		isnan_f32,
		isnan_f64,
		isinf_f32,
		isinf_f64,
		isfinite_f32,
		isfinite_f64,
		isnormal_f32,
		isnormal_f64,
		issubnormal_f32,
		issubnormal_f64,
		iszero_f32,
		iszero_f64,
		nextafter_f32,
		nextafter_f64,
		abs_i8,
		abs_i16,
		abs_i32,
		abs_i64,
		abs_f32,
		abs_f64,
		abs_i8_check,
		abs_i16_check,
		abs_i32_check,
		abs_i64_check,
		abs_f32_check,
		abs_f64_check,
		min_i8,
		min_i16,
		min_i32,
		min_i64,
		min_u8,
		min_u16,
		min_u32,
		min_u64,
		min_f32,
		min_f64,
		min_f32_check,
		min_f64_check,
		max_i8,
		max_i16,
		max_i32,
		max_i64,
		max_u8,
		max_u16,
		max_u32,
		max_u64,
		max_f32,
		max_f64,
		max_f32_check,
		max_f64_check,
		exp_f32,
		exp_f64,
		exp_f32_check,
		exp_f64_check,
		exp2_f32,
		exp2_f64,
		exp2_f32_check,
		exp2_f64_check,
		expm1_f32,
		expm1_f64,
		expm1_f32_check,
		expm1_f64_check,
		log_f32,
		log_f64,
		log_f32_check,
		log_f64_check,
		log10_f32,
		log10_f64,
		log10_f32_check,
		log10_f64_check,
		log2_f32,
		log2_f64,
		log2_f32_check,
		log2_f64_check,
		log1p_f32,
		log1p_f64,
		log1p_f32_check,
		log1p_f64_check,
		sqrt_f32,
		sqrt_f64,
		sqrt_f32_check,
		sqrt_f64_check,
		pow_f32,
		pow_f64,
		pow_f32_check,
		pow_f64_check,
		cbrt_f32,
		cbrt_f64,
		cbrt_f32_check,
		cbrt_f64_check,
		hypot_f32,
		hypot_f64,
		hypot_f32_check,
		hypot_f64_check,
		sin_f32,
		sin_f64,
		sin_f32_check,
		sin_f64_check,
		cos_f32,
		cos_f64,
		cos_f32_check,
		cos_f64_check,
		tan_f32,
		tan_f64,
		tan_f32_check,
		tan_f64_check,
		asin_f32,
		asin_f64,
		asin_f32_check,
		asin_f64_check,
		acos_f32,
		acos_f64,
		acos_f32_check,
		acos_f64_check,
		atan_f32,
		atan_f64,
		atan_f32_check,
		atan_f64_check,
		atan2_f32,
		atan2_f64,
		atan2_f32_check,
		atan2_f64_check,
		sinh_f32,
		sinh_f64,
		sinh_f32_check,
		sinh_f64_check,
		cosh_f32,
		cosh_f64,
		cosh_f32_check,
		cosh_f64_check,
		tanh_f32,
		tanh_f64,
		tanh_f32_check,
		tanh_f64_check,
		asinh_f32,
		asinh_f64,
		asinh_f32_check,
		asinh_f64_check,
		acosh_f32,
		acosh_f64,
		acosh_f32_check,
		acosh_f64_check,
		atanh_f32,
		atanh_f64,
		atanh_f32_check,
		atanh_f64_check,
		erf_f32,
		erf_f64,
		erf_f32_check,
		erf_f64_check,
		erfc_f32,
		erfc_f64,
		erfc_f32_check,
		erfc_f64_check,
		tgamma_f32,
		tgamma_f64,
		tgamma_f32_check,
		tgamma_f64_check,
		lgamma_f32,
		lgamma_f64,
		lgamma_f32_check,
		lgamma_f64_check,
		bitreverse_u8,
		bitreverse_u16,
		bitreverse_u32,
		bitreverse_u64,
		popcount_u8,
		popcount_u16,
		popcount_u32,
		popcount_u64,
		byteswap_u16,
		byteswap_u32,
		byteswap_u64,
		clz_u8,
		clz_u16,
		clz_u32,
		clz_u64,
		ctz_u8,
		ctz_u16,
		ctz_u32,
		ctz_u64,
		fshl_u8,
		fshl_u16,
		fshl_u32,
		fshl_u64,
		fshr_u8,
		fshr_u16,
		fshr_u32,
		fshr_u64,
		ashr_u8,
		ashr_u16,
		ashr_u32,
		ashr_u64,
		const_gep,
		array_gep_i32,
		array_gep_i64,
		const_memcpy,
		const_memset_zero,
		copy_values,
		copy_overlapping_values,
		relocate_values,
		set_values_i1_be,
		set_values_i8_be,
		set_values_i16_be,
		set_values_i32_be,
		set_values_i64_be,
		set_values_f32_be,
		set_values_f64_be,
		set_values_ptr32_be,
		set_values_ptr64_be,
		set_values_i1_le,
		set_values_i8_le,
		set_values_i16_le,
		set_values_i32_le,
		set_values_i64_le,
		set_values_f32_le,
		set_values_f64_le,
		set_values_ptr32_le,
		set_values_ptr64_le,
		set_values_ref,
		function_call,
		indirect_function_call,
		malloc,
		free,
		jump,
		conditional_jump,
		switch_i1,
		switch_i8,
		switch_i16,
		switch_i32,
		switch_i64,
		switch_str,
		ret,
		ret_void,
		unreachable,
		error,
		diagnostic_str,
		print,
		is_option_set,
		add_global_array_data,
		range_bounds_check_i64,
		range_bounds_check_u64,
		array_bounds_check_i32,
		array_bounds_check_u32,
		array_bounds_check_i64,
		array_bounds_check_u64,
		array_range_bounds_check_i32,
		array_range_bounds_check_u32,
		array_range_bounds_check_i64,
		array_range_bounds_check_u64,
		array_range_begin_bounds_check_i32,
		array_range_begin_bounds_check_u32,
		array_range_begin_bounds_check_i64,
		array_range_begin_bounds_check_u64,
		array_range_end_bounds_check_i32,
		array_range_end_bounds_check_u32,
		array_range_end_bounds_check_i64,
		array_range_end_bounds_check_u64,
		optional_get_value_check,
		str_construction_check,
		slice_construction_check,
		start_lifetime,
		end_lifetime,
	};

	instruction(void) = default;
	instruction(instruction const &) = default;
	instruction(instruction &&) = default;
	instruction &operator = (instruction const &) = default;
	instruction &operator = (instruction &&) = default;

	template<typename T>
	instruction(T other) = delete;

	template<typename T>
	instruction &operator = (T &&other) = delete;

	template<typename T, typename ...Args>
	T &emplace(Args &&...args) noexcept
	{
		this->_index = index_of<T>;
		return *new(this->_data) T(std::forward<Args>(args)...);
	}

	template<typename Visitor>
	void visit(Visitor visitor)
	{
		[&]<typename ...Ts>(bz::meta::type_pack<Ts...>) {
			using fn_t = void (*)(instruction *inst, Visitor *visitor);

			static constexpr fn_t fns[] = {
				nullptr,
				+[](instruction *inst, Visitor *visitor) {
					(*visitor)(inst->get<Ts>());
				}...
			};
			fns[this->_index](this, &visitor);
		}(instruction_list_t());
	}

	template<typename T>
	T &get(void) noexcept
	{
		bz_assert(this->_index == index_of<T>);
		return *std::launder(reinterpret_cast<T *>(this->_data));
	}

	template<typename T>
	T const &get(void) const noexcept
	{
		bz_assert(this->_index == index_of<T>);
		return *std::launder(reinterpret_cast<T const *>(this->_data));
	}

	uint64_t index(void) const noexcept
	{
		return this->_index;
	}

	bool is_terminator(void) const noexcept
	{
		switch (this->index())
		{
		case jump:
		case conditional_jump:
		case switch_i1:
		case switch_i8:
		case switch_i16:
		case switch_i32:
		case switch_i64:
		case switch_str:
		case ret:
		case ret_void:
		case unreachable:
			return true;
		default:
			return false;
		}
	}
};

bz::u8string to_string(instruction const &inst, function const *func);
bz::u8string to_string(function const &func);

#ifdef BOZON_PROFILE_COMPTIME
void print_instruction_counts(void);
inline bz::array<size_t, instruction_list_t::size() + 1> instruction_counts{};
#endif // BOZON_PROFILE_COMPTIME

} // namespace comptime

template<>
struct bz::formatter<comptime::instruction_value_index>
{
	static bz::u8string format(comptime::instruction_value_index index, bz::u8string_view)
	{
		return bz::format("%{}", index.index);
	}
};

template<>
struct bz::formatter<comptime::instructions::cmp_kind>
{
	static bz::u8string format(comptime::instructions::cmp_kind kind, bz::u8string_view)
	{
		switch (kind)
		{
		using enum comptime::instructions::cmp_kind;
		case eq:
			return "eq";
		case neq:
			return "neq";
		case slt:
		case ult:
			return "lt";
		case sgt:
		case ugt:
			return "gt";
		case slte:
		case ulte:
			return "lte";
		case sgte:
		case ugte:
			return "gte";
		}
		bz_unreachable;
	}
};

#endif // COMPTIME_INSTRUCTIONS_H
