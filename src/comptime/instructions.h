#ifndef COMPTIME_INSTRUCTIONS_H
#define COMPTIME_INSTRUCTIONS_H

#include "core.h"
#include "types.h"
#include "values.h"
#include "ctx/warnings.h"

namespace comptime
{

struct instruction;

struct basic_block
{
	bz::vector<instruction> instructions;
	uint32_t instruction_value_offset;
};

struct instruction_index
{
	uint32_t index;
};

struct instruction_value_index
{
	uint32_t index;
};

struct alloca
{
	size_t size;
	size_t align;
};

struct function
{
	bz::fixed_vector<instruction> instructions;
	bz::fixed_vector<bz::fixed_vector<instruction_index>> call_args;
	bz::fixed_vector<type const *> arg_types;
	type const *return_type;
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

namespace internal
{

template<typename Inst, size_t ArgsCount>
struct instruction_with_args
{
	bz::array<instruction_value_index, ArgsCount> args;
	Inst inst;
};

template<typename Inst>
struct instruction_with_args<Inst, 0>
{
	Inst inst;
};

} // namespace internal

template<typename Inst>
using instruction_with_args = internal::instruction_with_args<Inst, arg_count<Inst>>;


struct const_i1
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::i1;

	bool value;
};

struct const_i8
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::i8;

	int8_t value;
};

struct const_i16
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::i16;

	int16_t value;
};

struct const_i32
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::i32;

	int32_t value;
};

struct const_i64
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::i64;

	int64_t value;
};

struct const_u8
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::i8;

	uint8_t value;
};

struct const_u16
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::i16;

	uint16_t value;
};

struct const_u32
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::i32;

	uint32_t value;
};

struct const_u64
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::i64;

	uint64_t value;
};

struct const_f32
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::f32;

	float32_t value;
};

struct const_f64
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::f64;

	float64_t value;
};

struct const_ptr_null
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::ptr;
};

struct load_i1_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i1;
};

struct load_i8_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i8;
};

struct load_i16_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i16;
};

struct load_i32_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i32;
};

struct load_i64_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i64;
};

struct load_f32_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::f32;
};

struct load_f64_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::f64;
};

struct load_ptr32_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::ptr;
};

struct load_ptr64_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::ptr;
};

struct load_i1_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i1;
};

struct load_i8_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i8;
};

struct load_i16_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i16;
};

struct load_i32_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i32;
};

struct load_i64_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i64;
};

struct load_f32_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::f32;
};

struct load_f64_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::f64;
};

struct load_ptr32_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::ptr;
};

struct load_ptr64_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::ptr;
};

struct store_i1_be
{
	static inline constexpr bz::array arg_types = { value_type::i1, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
};

struct store_i8_be
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
};

struct store_i16_be
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
};

struct store_i32_be
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
};

struct store_i64_be
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
};

struct store_f32_be
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
};

struct store_f64_be
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
};

struct store_ptr32_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
};

struct store_ptr64_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
};

struct store_i1_le
{
	static inline constexpr bz::array arg_types = { value_type::i1, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
};

struct store_i8_le
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
};

struct store_i16_le
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
};

struct store_i32_le
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
};

struct store_i64_le
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
};

struct store_f32_le
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
};

struct store_f64_le
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
};

struct store_ptr32_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
};

struct store_ptr64_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;
};

struct cast_zext_i1_to_i8
{
	static inline constexpr bz::array arg_types = { value_type::i1 };
	static inline constexpr value_type result_type = value_type::i8;
};

struct cast_zext_i1_to_i16
{
	static inline constexpr bz::array arg_types = { value_type::i1 };
	static inline constexpr value_type result_type = value_type::i16;
};

struct cast_zext_i1_to_i32
{
	static inline constexpr bz::array arg_types = { value_type::i1 };
	static inline constexpr value_type result_type = value_type::i32;
};

struct cast_zext_i1_to_i64
{
	static inline constexpr bz::array arg_types = { value_type::i1 };
	static inline constexpr value_type result_type = value_type::i64;
};

struct cast_zext_i8_to_i16
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::i16;
};

struct cast_zext_i8_to_i32
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::i32;
};

struct cast_zext_i8_to_i64
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::i64;
};

struct cast_zext_i16_to_i32
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::i32;
};

struct cast_zext_i16_to_i64
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::i64;
};

struct cast_zext_i32_to_i64
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::i64;
};

struct cast_sext_i8_to_i16
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::i16;
};

struct cast_sext_i8_to_i32
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::i32;
};

struct cast_sext_i8_to_i64
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::i64;
};

struct cast_sext_i16_to_i32
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::i32;
};

struct cast_sext_i16_to_i64
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::i64;
};

struct cast_sext_i32_to_i64
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::i64;
};

struct cast_trunc_i64_to_i8
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::i8;
};

struct cast_trunc_i64_to_i16
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::i16;
};

struct cast_trunc_i64_to_i32
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::i32;
};

struct cast_trunc_i32_to_i8
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::i8;
};

struct cast_trunc_i32_to_i16
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::i16;
};

struct cast_trunc_i16_to_i8
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::i8;
};

struct cast_f32_to_f64
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct cast_f64_to_f32
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct cast_f32_to_i8
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::i8;
};

struct cast_f32_to_i16
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::i16;
};

struct cast_f32_to_i32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::i32;
};

struct cast_f32_to_i64
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::i64;
};

struct cast_f32_to_u8
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::i8;
};

struct cast_f32_to_u16
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::i16;
};

struct cast_f32_to_u32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::i32;
};

struct cast_f32_to_u64
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::i64;
};

struct cast_f64_to_i8
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::i8;
};

struct cast_f64_to_i16
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::i16;
};

struct cast_f64_to_i32
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::i32;
};

struct cast_f64_to_i64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::i64;
};

struct cast_f64_to_u8
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::i8;
};

struct cast_f64_to_u16
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::i16;
};

struct cast_f64_to_u32
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::i32;
};

struct cast_f64_to_u64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::i64;
};

struct cast_i8_to_f32
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct cast_i16_to_f32
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct cast_i32_to_f32
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct cast_i64_to_f32
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct cast_u8_to_f32
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct cast_u16_to_f32
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct cast_u32_to_f32
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct cast_u64_to_f32
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct cast_i8_to_f64
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct cast_i16_to_f64
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct cast_i32_to_f64
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct cast_i64_to_f64
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct cast_u8_to_f64
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct cast_u16_to_f64
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct cast_u32_to_f64
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct cast_u64_to_f64
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct cmp_eq_i1
{
	static inline constexpr bz::array arg_types = { value_type::i1, value_type::i1 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_eq_i8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_eq_i16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_eq_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_eq_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_eq_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_eq_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_eq_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct cmp_eq_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct cmp_eq_ptr
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_neq_i1
{
	static inline constexpr bz::array arg_types = { value_type::i1, value_type::i1 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_neq_i8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_neq_i16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_neq_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_neq_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_neq_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_neq_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_neq_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct cmp_neq_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct cmp_neq_ptr
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_lt_i8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_lt_i16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_lt_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_lt_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_lt_u8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_lt_u16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_lt_u32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_lt_u64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_lt_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_lt_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_lt_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct cmp_lt_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct cmp_lt_ptr
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::i1;

	uint32_t src_tokens_index;
};

struct cmp_gt_i8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_gt_i16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_gt_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_gt_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_gt_u8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_gt_u16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_gt_u32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_gt_u64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_gt_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_gt_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_gt_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct cmp_gt_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct cmp_gt_ptr
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::i1;

	uint32_t src_tokens_index;
};

struct cmp_lte_i8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_lte_i16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_lte_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_lte_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_lte_u8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_lte_u16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_lte_u32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_lte_u64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_lte_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_lte_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_lte_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct cmp_lte_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct cmp_lte_ptr
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::i1;

	uint32_t src_tokens_index;
};

struct cmp_gte_i8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_gte_i16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_gte_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_gte_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_gte_u8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_gte_u16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_gte_u32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_gte_u64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_gte_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_gte_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_gte_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct cmp_gte_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct cmp_gte_ptr
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::i1;

	uint32_t src_tokens_index;
};

struct neg_i8
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
};

struct neg_i16
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
};

struct neg_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
};

struct neg_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
};

struct neg_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct neg_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct neg_i8_check
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct neg_i16_check
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct neg_i32_check
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct neg_i64_check
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct add_i8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
};

struct add_i16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
};

struct add_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
};

struct add_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
};

struct add_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct add_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct add_ptr_i32
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::i32 };
	static inline constexpr value_type result_type = value_type::ptr;

	uint32_t src_tokens_index;
	uint32_t pointer_arithmetic_check_info_index;
};

struct add_ptr_u32
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::i32 };
	static inline constexpr value_type result_type = value_type::ptr;

	uint32_t src_tokens_index;
	uint32_t pointer_arithmetic_check_info_index;
};

struct add_ptr_i64
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::i64 };
	static inline constexpr value_type result_type = value_type::ptr;

	uint32_t src_tokens_index;
	uint32_t pointer_arithmetic_check_info_index;
};

struct add_ptr_u64
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::i64 };
	static inline constexpr value_type result_type = value_type::ptr;

	uint32_t src_tokens_index;
	uint32_t pointer_arithmetic_check_info_index;
};

struct add_i8_check
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct add_i16_check
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct add_i32_check
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct add_i64_check
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct add_u8_check
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct add_u16_check
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct add_u32_check
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct add_u64_check
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct add_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct add_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct sub_i8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
};

struct sub_i16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
};

struct sub_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
};

struct sub_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
};

struct sub_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct sub_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct sub_ptr_i32
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::i32 };
	static inline constexpr value_type result_type = value_type::ptr;

	uint32_t src_tokens_index;
	uint32_t pointer_arithmetic_check_info_index;
};

struct sub_ptr_u32
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::i32 };
	static inline constexpr value_type result_type = value_type::ptr;

	uint32_t src_tokens_index;
	uint32_t pointer_arithmetic_check_info_index;
};

struct sub_ptr_i64
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::i64 };
	static inline constexpr value_type result_type = value_type::ptr;

	uint32_t src_tokens_index;
	uint32_t pointer_arithmetic_check_info_index;
};

struct sub_ptr_u64
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::i64 };
	static inline constexpr value_type result_type = value_type::ptr;

	uint32_t src_tokens_index;
	uint32_t pointer_arithmetic_check_info_index;
};

struct sub_i8_check
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct sub_i16_check
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct sub_i32_check
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct sub_i64_check
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct sub_u8_check
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct sub_u16_check
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct sub_u32_check
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct sub_u64_check
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct sub_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct sub_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct ptr32_diff
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::i32;

	size_t stride;
};

struct ptr64_diff
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::i64;

	size_t stride;
};

struct mul_i8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
};

struct mul_i16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
};

struct mul_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
};

struct mul_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
};

struct mul_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct mul_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct mul_i8_check
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct mul_i16_check
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct mul_i32_check
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct mul_i64_check
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct mul_u8_check
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct mul_u16_check
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct mul_u32_check
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct mul_u64_check
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct mul_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct mul_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct div_i8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;

	uint32_t src_tokens_index;
};

struct div_i16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;

	uint32_t src_tokens_index;
};

struct div_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;

	uint32_t src_tokens_index;
};

struct div_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;

	uint32_t src_tokens_index;
};

struct div_u8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;

	uint32_t src_tokens_index;
};

struct div_u16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;

	uint32_t src_tokens_index;
};

struct div_u32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;

	uint32_t src_tokens_index;
};

struct div_u64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;

	uint32_t src_tokens_index;
};

struct div_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct div_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct div_i8_check
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct div_i16_check
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct div_i32_check
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct div_i64_check
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct div_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct div_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct rem_i8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;

	uint32_t src_tokens_index;
};

struct rem_i16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;

	uint32_t src_tokens_index;
};

struct rem_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;

	uint32_t src_tokens_index;
};

struct rem_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;

	uint32_t src_tokens_index;
};

struct rem_u8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;

	uint32_t src_tokens_index;
};

struct rem_u16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;

	uint32_t src_tokens_index;
};

struct rem_u32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;

	uint32_t src_tokens_index;
};

struct rem_u64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;

	uint32_t src_tokens_index;
};

struct not_i1
{
	static inline constexpr bz::array arg_types = { value_type::i1 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct not_i8
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
};

struct not_i16
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
};

struct not_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
};

struct not_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
};

struct and_i1
{
	static inline constexpr bz::array arg_types = { value_type::i1, value_type::i1 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct and_i8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
};

struct and_i16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
};

struct and_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
};

struct and_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
};

struct xor_i1
{
	static inline constexpr bz::array arg_types = { value_type::i1, value_type::i1 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct xor_i8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
};

struct xor_i16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
};

struct xor_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
};

struct xor_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
};

struct or_i1
{
	static inline constexpr bz::array arg_types = { value_type::i1, value_type::i1 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct or_i8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
};

struct or_i16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
};

struct or_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
};

struct or_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
};

struct shl_i8_signed
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i8;

	uint32_t src_tokens_index;
};

struct shl_i16_signed
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i16;

	uint32_t src_tokens_index;
};

struct shl_i32_signed
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i32;

	uint32_t src_tokens_index;
};

struct shl_i64_signed
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;

	uint32_t src_tokens_index;
};

struct shl_i8_unsigned
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i8;

	uint32_t src_tokens_index;
};

struct shl_i16_unsigned
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i16;

	uint32_t src_tokens_index;
};

struct shl_i32_unsigned
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i32;

	uint32_t src_tokens_index;
};

struct shl_i64_unsigned
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;

	uint32_t src_tokens_index;
};

struct shr_i8_signed
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i8;

	uint32_t src_tokens_index;
};

struct shr_i16_signed
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i16;

	uint32_t src_tokens_index;
};

struct shr_i32_signed
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i32;

	uint32_t src_tokens_index;
};

struct shr_i64_signed
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;

	uint32_t src_tokens_index;
};

struct shr_i8_unsigned
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i8;

	uint32_t src_tokens_index;
};

struct shr_i16_unsigned
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i16;

	uint32_t src_tokens_index;
};

struct shr_i32_unsigned
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i32;

	uint32_t src_tokens_index;
};

struct shr_i64_unsigned
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;

	uint32_t src_tokens_index;
};

struct abs_i8
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
};

struct abs_i16
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
};

struct abs_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
};

struct abs_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
};

struct abs_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct abs_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct abs_i8_check
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct abs_i16_check
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct abs_i32_check
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct abs_i64_check
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct abs_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct abs_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct min_i8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
};

struct min_i16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
};

struct min_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
};

struct min_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
};

struct min_u8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
};

struct min_u16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
};

struct min_u32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
};

struct min_u64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
};

struct min_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct min_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct min_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct min_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct max_i8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
};

struct max_i16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
};

struct max_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
};

struct max_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
};

struct max_u8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
};

struct max_u16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
};

struct max_u32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
};

struct max_u64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
};

struct max_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct max_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct max_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct max_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct exp_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct exp_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct exp_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct exp_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct exp2_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct exp2_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct exp2_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct exp2_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct expm1_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct expm1_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct expm1_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct expm1_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct log_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct log_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct log_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct log_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct log10_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct log10_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct log10_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct log10_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct log2_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct log2_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct log2_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct log2_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct log1p_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct log1p_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct log1p_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct log1p_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct sqrt_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct sqrt_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct sqrt_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct sqrt_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct pow_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct pow_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct pow_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct pow_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct cbrt_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct cbrt_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct cbrt_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct cbrt_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct hypot_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct hypot_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct hypot_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct hypot_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct sin_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct sin_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct sin_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct sin_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct cos_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct cos_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct cos_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct cos_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct tan_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct tan_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct tan_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct tan_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct asin_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct asin_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct asin_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct asin_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct acos_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct acos_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct acos_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct acos_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct atan_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct atan_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct atan_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct atan_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct atan2_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct atan2_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct atan2_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct atan2_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct sinh_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct sinh_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct sinh_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct sinh_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct cosh_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct cosh_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct cosh_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct cosh_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct tanh_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct tanh_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct tanh_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct tanh_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct asinh_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct asinh_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct asinh_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct asinh_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct acosh_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct acosh_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct acosh_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct acosh_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct atanh_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct atanh_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct atanh_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct atanh_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct erf_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct erf_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct erf_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct erf_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct erfc_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct erfc_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct erfc_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct erfc_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct tgamma_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct tgamma_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct tgamma_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct tgamma_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct lgamma_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct lgamma_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
};

struct lgamma_f32_check
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct lgamma_f64_check
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct bitreverse_u8
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
};

struct bitreverse_u16
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
};

struct bitreverse_u32
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
};

struct bitreverse_u64
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
};

struct popcount_u8
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
};

struct popcount_u16
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
};

struct popcount_u32
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
};

struct popcount_u64
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
};

struct byteswap_u16
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
};

struct byteswap_u32
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
};

struct byteswap_u64
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
};

struct clz_u8
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
};

struct clz_u16
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
};

struct clz_u32
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
};

struct clz_u64
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
};

struct ctz_u8
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
};

struct ctz_u16
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
};

struct ctz_u32
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
};

struct ctz_u64
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
};

struct fshl_u8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
};

struct fshl_u16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
};

struct fshl_u32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
};

struct fshl_u64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
};

struct fshr_u8
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
};

struct fshr_u16
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
};

struct fshr_u32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
};

struct fshr_u64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
};

struct const_gep
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::ptr;

	size_t offset;
};

struct array_gep_i32
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::i32 };
	static inline constexpr value_type result_type = value_type::ptr;

	size_t stride;
};

struct array_gep_i64
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::i64 };
	static inline constexpr value_type result_type = value_type::ptr;

	size_t stride;
};

struct const_memcpy
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;

	size_t size;
};

struct const_memset_zero
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;

	size_t size;
};

struct function_call
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::any;

	function const *func;
	size_t args_index;
};

struct jump
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::none;

	instruction_index dest;
};

struct conditional_jump
{
	static inline constexpr bz::array arg_types = { value_type::i1 };
	static inline constexpr value_type result_type = value_type::none;

	instruction_index true_dest;
	instruction_index false_dest;
};

struct ret
{
	static inline constexpr bz::array arg_types = { value_type::any };
	static inline constexpr value_type result_type = value_type::none;
};

struct ret_void
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::none;
};

struct unreachable
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::none;
};

struct error
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::none;

	uint32_t error_index;
};

struct diagnostic_str
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
	ctx::warning_kind kind;
};

struct array_bounds_check_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct array_bounds_check_u32
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct array_bounds_check_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct array_bounds_check_u64
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct optional_get_value_check
{
	static inline constexpr bz::array arg_types = { value_type::i1 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct str_construction_check
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
};

struct slice_construction_check
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t src_tokens_index;
	uint32_t slice_construction_check_info_index;
};

} // namespace instructions

using instruction_list = bz::meta::type_pack<
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
	instructions::cmp_eq_i1,
	instructions::cmp_eq_i8,
	instructions::cmp_eq_i16,
	instructions::cmp_eq_i32,
	instructions::cmp_eq_i64,
	instructions::cmp_eq_f32,
	instructions::cmp_eq_f64,
	instructions::cmp_eq_f32_check,
	instructions::cmp_eq_f64_check,
	instructions::cmp_eq_ptr,
	instructions::cmp_neq_i1,
	instructions::cmp_neq_i8,
	instructions::cmp_neq_i16,
	instructions::cmp_neq_i32,
	instructions::cmp_neq_i64,
	instructions::cmp_neq_f32,
	instructions::cmp_neq_f64,
	instructions::cmp_neq_f32_check,
	instructions::cmp_neq_f64_check,
	instructions::cmp_neq_ptr,
	instructions::cmp_lt_i8,
	instructions::cmp_lt_i16,
	instructions::cmp_lt_i32,
	instructions::cmp_lt_i64,
	instructions::cmp_lt_u8,
	instructions::cmp_lt_u16,
	instructions::cmp_lt_u32,
	instructions::cmp_lt_u64,
	instructions::cmp_lt_f32,
	instructions::cmp_lt_f64,
	instructions::cmp_lt_f32_check,
	instructions::cmp_lt_f64_check,
	instructions::cmp_lt_ptr,
	instructions::cmp_gt_i8,
	instructions::cmp_gt_i16,
	instructions::cmp_gt_i32,
	instructions::cmp_gt_i64,
	instructions::cmp_gt_u8,
	instructions::cmp_gt_u16,
	instructions::cmp_gt_u32,
	instructions::cmp_gt_u64,
	instructions::cmp_gt_f32,
	instructions::cmp_gt_f64,
	instructions::cmp_gt_f32_check,
	instructions::cmp_gt_f64_check,
	instructions::cmp_gt_ptr,
	instructions::cmp_lte_i8,
	instructions::cmp_lte_i16,
	instructions::cmp_lte_i32,
	instructions::cmp_lte_i64,
	instructions::cmp_lte_u8,
	instructions::cmp_lte_u16,
	instructions::cmp_lte_u32,
	instructions::cmp_lte_u64,
	instructions::cmp_lte_f32,
	instructions::cmp_lte_f64,
	instructions::cmp_lte_f32_check,
	instructions::cmp_lte_f64_check,
	instructions::cmp_lte_ptr,
	instructions::cmp_gte_i8,
	instructions::cmp_gte_i16,
	instructions::cmp_gte_i32,
	instructions::cmp_gte_i64,
	instructions::cmp_gte_u8,
	instructions::cmp_gte_u16,
	instructions::cmp_gte_u32,
	instructions::cmp_gte_u64,
	instructions::cmp_gte_f32,
	instructions::cmp_gte_f64,
	instructions::cmp_gte_f32_check,
	instructions::cmp_gte_f64_check,
	instructions::cmp_gte_ptr,
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
	instructions::min_f32_check,
	instructions::min_f64_check,
	instructions::min_f32,
	instructions::min_f64,
	instructions::max_i8,
	instructions::max_i16,
	instructions::max_i32,
	instructions::max_i64,
	instructions::max_u8,
	instructions::max_u16,
	instructions::max_u32,
	instructions::max_u64,
	instructions::max_f32_check,
	instructions::max_f64_check,
	instructions::max_f32,
	instructions::max_f64,
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
	instructions::const_gep,
	instructions::array_gep_i32,
	instructions::array_gep_i64,
	instructions::const_memcpy,
	instructions::const_memset_zero,
	instructions::function_call,
	instructions::jump,
	instructions::conditional_jump,
	instructions::ret,
	instructions::ret_void,
	instructions::unreachable,
	instructions::error,
	instructions::diagnostic_str,
	instructions::array_bounds_check_i32,
	instructions::array_bounds_check_u32,
	instructions::array_bounds_check_i64,
	instructions::array_bounds_check_u64,
	instructions::optional_get_value_check,
	instructions::str_construction_check,
	instructions::slice_construction_check
>;

using instruction_with_args_list = bz::meta::transform_type_pack<instructions::instruction_with_args, instruction_list>;
using instruction_base_t = bz::meta::apply_type_pack<bz::variant, instruction_with_args_list>;

struct instruction : instruction_base_t
{
	using base_t = instruction_base_t;
	template<typename Inst>
	static inline constexpr base_t::index_t index_of = base_t::index_of<instructions::instruction_with_args<Inst>>;

	static_assert(variant_count == 499);
	enum : base_t::index_t
	{
		const_i1                 = index_of<instructions::const_i1>,
		const_i8                 = index_of<instructions::const_i8>,
		const_i16                = index_of<instructions::const_i16>,
		const_i32                = index_of<instructions::const_i32>,
		const_i64                = index_of<instructions::const_i64>,
		const_u8                 = index_of<instructions::const_u8>,
		const_u16                = index_of<instructions::const_u16>,
		const_u32                = index_of<instructions::const_u32>,
		const_u64                = index_of<instructions::const_u64>,
		const_f32                = index_of<instructions::const_f32>,
		const_f64                = index_of<instructions::const_f64>,
		const_ptr_null           = index_of<instructions::const_ptr_null>,
		load_i1_be               = index_of<instructions::load_i1_be>,
		load_i8_be               = index_of<instructions::load_i8_be>,
		load_i16_be              = index_of<instructions::load_i16_be>,
		load_i32_be              = index_of<instructions::load_i32_be>,
		load_i64_be              = index_of<instructions::load_i64_be>,
		load_f32_be              = index_of<instructions::load_f32_be>,
		load_f64_be              = index_of<instructions::load_f64_be>,
		load_ptr32_be            = index_of<instructions::load_ptr32_be>,
		load_ptr64_be            = index_of<instructions::load_ptr64_be>,
		load_i1_le               = index_of<instructions::load_i1_le>,
		load_i8_le               = index_of<instructions::load_i8_le>,
		load_i16_le              = index_of<instructions::load_i16_le>,
		load_i32_le              = index_of<instructions::load_i32_le>,
		load_i64_le              = index_of<instructions::load_i64_le>,
		load_f32_le              = index_of<instructions::load_f32_le>,
		load_f64_le              = index_of<instructions::load_f64_le>,
		load_ptr32_le            = index_of<instructions::load_ptr32_le>,
		load_ptr64_le            = index_of<instructions::load_ptr64_le>,
		store_i1_be              = index_of<instructions::store_i1_be>,
		store_i8_be              = index_of<instructions::store_i8_be>,
		store_i16_be             = index_of<instructions::store_i16_be>,
		store_i32_be             = index_of<instructions::store_i32_be>,
		store_i64_be             = index_of<instructions::store_i64_be>,
		store_f32_be             = index_of<instructions::store_f32_be>,
		store_f64_be             = index_of<instructions::store_f64_be>,
		store_ptr32_be           = index_of<instructions::store_ptr32_be>,
		store_ptr64_be           = index_of<instructions::store_ptr64_be>,
		store_i1_le              = index_of<instructions::store_i1_le>,
		store_i8_le              = index_of<instructions::store_i8_le>,
		store_i16_le             = index_of<instructions::store_i16_le>,
		store_i32_le             = index_of<instructions::store_i32_le>,
		store_i64_le             = index_of<instructions::store_i64_le>,
		store_f32_le             = index_of<instructions::store_f32_le>,
		store_f64_le             = index_of<instructions::store_f64_le>,
		store_ptr32_le           = index_of<instructions::store_ptr32_le>,
		store_ptr64_le           = index_of<instructions::store_ptr64_le>,
		cast_zext_i1_to_i8       = index_of<instructions::cast_zext_i1_to_i8>,
		cast_zext_i1_to_i16      = index_of<instructions::cast_zext_i1_to_i16>,
		cast_zext_i1_to_i32      = index_of<instructions::cast_zext_i1_to_i32>,
		cast_zext_i1_to_i64      = index_of<instructions::cast_zext_i1_to_i64>,
		cast_zext_i8_to_i16      = index_of<instructions::cast_zext_i8_to_i16>,
		cast_zext_i8_to_i32      = index_of<instructions::cast_zext_i8_to_i32>,
		cast_zext_i8_to_i64      = index_of<instructions::cast_zext_i8_to_i64>,
		cast_zext_i16_to_i32     = index_of<instructions::cast_zext_i16_to_i32>,
		cast_zext_i16_to_i64     = index_of<instructions::cast_zext_i16_to_i64>,
		cast_zext_i32_to_i64     = index_of<instructions::cast_zext_i32_to_i64>,
		cast_sext_i8_to_i16      = index_of<instructions::cast_sext_i8_to_i16>,
		cast_sext_i8_to_i32      = index_of<instructions::cast_sext_i8_to_i32>,
		cast_sext_i8_to_i64      = index_of<instructions::cast_sext_i8_to_i64>,
		cast_sext_i16_to_i32     = index_of<instructions::cast_sext_i16_to_i32>,
		cast_sext_i16_to_i64     = index_of<instructions::cast_sext_i16_to_i64>,
		cast_sext_i32_to_i64     = index_of<instructions::cast_sext_i32_to_i64>,
		cast_trunc_i64_to_i8     = index_of<instructions::cast_trunc_i64_to_i8>,
		cast_trunc_i64_to_i16    = index_of<instructions::cast_trunc_i64_to_i16>,
		cast_trunc_i64_to_i32    = index_of<instructions::cast_trunc_i64_to_i32>,
		cast_trunc_i32_to_i8     = index_of<instructions::cast_trunc_i32_to_i8>,
		cast_trunc_i32_to_i16    = index_of<instructions::cast_trunc_i32_to_i16>,
		cast_trunc_i16_to_i8     = index_of<instructions::cast_trunc_i16_to_i8>,
		cast_f32_to_f64          = index_of<instructions::cast_f32_to_f64>,
		cast_f64_to_f32          = index_of<instructions::cast_f64_to_f32>,
		cast_f32_to_i8           = index_of<instructions::cast_f32_to_i8>,
		cast_f32_to_i16          = index_of<instructions::cast_f32_to_i16>,
		cast_f32_to_i32          = index_of<instructions::cast_f32_to_i32>,
		cast_f32_to_i64          = index_of<instructions::cast_f32_to_i64>,
		cast_f32_to_u8           = index_of<instructions::cast_f32_to_u8>,
		cast_f32_to_u16          = index_of<instructions::cast_f32_to_u16>,
		cast_f32_to_u32          = index_of<instructions::cast_f32_to_u32>,
		cast_f32_to_u64          = index_of<instructions::cast_f32_to_u64>,
		cast_f64_to_i8           = index_of<instructions::cast_f64_to_i8>,
		cast_f64_to_i16          = index_of<instructions::cast_f64_to_i16>,
		cast_f64_to_i32          = index_of<instructions::cast_f64_to_i32>,
		cast_f64_to_i64          = index_of<instructions::cast_f64_to_i64>,
		cast_f64_to_u8           = index_of<instructions::cast_f64_to_u8>,
		cast_f64_to_u16          = index_of<instructions::cast_f64_to_u16>,
		cast_f64_to_u32          = index_of<instructions::cast_f64_to_u32>,
		cast_f64_to_u64          = index_of<instructions::cast_f64_to_u64>,
		cast_i8_to_f32           = index_of<instructions::cast_i8_to_f32>,
		cast_i16_to_f32          = index_of<instructions::cast_i16_to_f32>,
		cast_i32_to_f32          = index_of<instructions::cast_i32_to_f32>,
		cast_i64_to_f32          = index_of<instructions::cast_i64_to_f32>,
		cast_u8_to_f32           = index_of<instructions::cast_u8_to_f32>,
		cast_u16_to_f32          = index_of<instructions::cast_u16_to_f32>,
		cast_u32_to_f32          = index_of<instructions::cast_u32_to_f32>,
		cast_u64_to_f32          = index_of<instructions::cast_u64_to_f32>,
		cast_i8_to_f64           = index_of<instructions::cast_i8_to_f64>,
		cast_i16_to_f64          = index_of<instructions::cast_i16_to_f64>,
		cast_i32_to_f64          = index_of<instructions::cast_i32_to_f64>,
		cast_i64_to_f64          = index_of<instructions::cast_i64_to_f64>,
		cast_u8_to_f64           = index_of<instructions::cast_u8_to_f64>,
		cast_u16_to_f64          = index_of<instructions::cast_u16_to_f64>,
		cast_u32_to_f64          = index_of<instructions::cast_u32_to_f64>,
		cast_u64_to_f64          = index_of<instructions::cast_u64_to_f64>,
		cmp_eq_i1                = index_of<instructions::cmp_eq_i1>,
		cmp_eq_i8                = index_of<instructions::cmp_eq_i8>,
		cmp_eq_i16               = index_of<instructions::cmp_eq_i16>,
		cmp_eq_i32               = index_of<instructions::cmp_eq_i32>,
		cmp_eq_i64               = index_of<instructions::cmp_eq_i64>,
		cmp_eq_f32               = index_of<instructions::cmp_eq_f32>,
		cmp_eq_f64               = index_of<instructions::cmp_eq_f64>,
		cmp_eq_f32_check         = index_of<instructions::cmp_eq_f32_check>,
		cmp_eq_f64_check         = index_of<instructions::cmp_eq_f64_check>,
		cmp_eq_ptr               = index_of<instructions::cmp_eq_ptr>,
		cmp_neq_i1               = index_of<instructions::cmp_neq_i1>,
		cmp_neq_i8               = index_of<instructions::cmp_neq_i8>,
		cmp_neq_i16              = index_of<instructions::cmp_neq_i16>,
		cmp_neq_i32              = index_of<instructions::cmp_neq_i32>,
		cmp_neq_i64              = index_of<instructions::cmp_neq_i64>,
		cmp_neq_f32              = index_of<instructions::cmp_neq_f32>,
		cmp_neq_f64              = index_of<instructions::cmp_neq_f64>,
		cmp_neq_f32_check        = index_of<instructions::cmp_neq_f32_check>,
		cmp_neq_f64_check        = index_of<instructions::cmp_neq_f64_check>,
		cmp_neq_ptr              = index_of<instructions::cmp_neq_ptr>,
		cmp_lt_i8                = index_of<instructions::cmp_lt_i8>,
		cmp_lt_i16               = index_of<instructions::cmp_lt_i16>,
		cmp_lt_i32               = index_of<instructions::cmp_lt_i32>,
		cmp_lt_i64               = index_of<instructions::cmp_lt_i64>,
		cmp_lt_u8                = index_of<instructions::cmp_lt_u8>,
		cmp_lt_u16               = index_of<instructions::cmp_lt_u16>,
		cmp_lt_u32               = index_of<instructions::cmp_lt_u32>,
		cmp_lt_u64               = index_of<instructions::cmp_lt_u64>,
		cmp_lt_f32               = index_of<instructions::cmp_lt_f32>,
		cmp_lt_f64               = index_of<instructions::cmp_lt_f64>,
		cmp_lt_f32_check         = index_of<instructions::cmp_lt_f32_check>,
		cmp_lt_f64_check         = index_of<instructions::cmp_lt_f64_check>,
		cmp_lt_ptr               = index_of<instructions::cmp_lt_ptr>,
		cmp_gt_i8                = index_of<instructions::cmp_gt_i8>,
		cmp_gt_i16               = index_of<instructions::cmp_gt_i16>,
		cmp_gt_i32               = index_of<instructions::cmp_gt_i32>,
		cmp_gt_i64               = index_of<instructions::cmp_gt_i64>,
		cmp_gt_u8                = index_of<instructions::cmp_gt_u8>,
		cmp_gt_u16               = index_of<instructions::cmp_gt_u16>,
		cmp_gt_u32               = index_of<instructions::cmp_gt_u32>,
		cmp_gt_u64               = index_of<instructions::cmp_gt_u64>,
		cmp_gt_f32               = index_of<instructions::cmp_gt_f32>,
		cmp_gt_f64               = index_of<instructions::cmp_gt_f64>,
		cmp_gt_f32_check         = index_of<instructions::cmp_gt_f32_check>,
		cmp_gt_f64_check         = index_of<instructions::cmp_gt_f64_check>,
		cmp_gt_ptr               = index_of<instructions::cmp_gt_ptr>,
		cmp_lte_i8               = index_of<instructions::cmp_lte_i8>,
		cmp_lte_i16              = index_of<instructions::cmp_lte_i16>,
		cmp_lte_i32              = index_of<instructions::cmp_lte_i32>,
		cmp_lte_i64              = index_of<instructions::cmp_lte_i64>,
		cmp_lte_u8               = index_of<instructions::cmp_lte_u8>,
		cmp_lte_u16              = index_of<instructions::cmp_lte_u16>,
		cmp_lte_u32              = index_of<instructions::cmp_lte_u32>,
		cmp_lte_u64              = index_of<instructions::cmp_lte_u64>,
		cmp_lte_f32              = index_of<instructions::cmp_lte_f32>,
		cmp_lte_f64              = index_of<instructions::cmp_lte_f64>,
		cmp_lte_f32_check        = index_of<instructions::cmp_lte_f32_check>,
		cmp_lte_f64_check        = index_of<instructions::cmp_lte_f64_check>,
		cmp_lte_ptr              = index_of<instructions::cmp_lte_ptr>,
		cmp_gte_i8               = index_of<instructions::cmp_gte_i8>,
		cmp_gte_i16              = index_of<instructions::cmp_gte_i16>,
		cmp_gte_i32              = index_of<instructions::cmp_gte_i32>,
		cmp_gte_i64              = index_of<instructions::cmp_gte_i64>,
		cmp_gte_u8               = index_of<instructions::cmp_gte_u8>,
		cmp_gte_u16              = index_of<instructions::cmp_gte_u16>,
		cmp_gte_u32              = index_of<instructions::cmp_gte_u32>,
		cmp_gte_u64              = index_of<instructions::cmp_gte_u64>,
		cmp_gte_f32              = index_of<instructions::cmp_gte_f32>,
		cmp_gte_f64              = index_of<instructions::cmp_gte_f64>,
		cmp_gte_f32_check        = index_of<instructions::cmp_gte_f32_check>,
		cmp_gte_f64_check        = index_of<instructions::cmp_gte_f64_check>,
		cmp_gte_ptr              = index_of<instructions::cmp_gte_ptr>,
		neg_i8                   = index_of<instructions::neg_i8>,
		neg_i16                  = index_of<instructions::neg_i16>,
		neg_i32                  = index_of<instructions::neg_i32>,
		neg_i64                  = index_of<instructions::neg_i64>,
		neg_f32                  = index_of<instructions::neg_f32>,
		neg_f64                  = index_of<instructions::neg_f64>,
		neg_i8_check             = index_of<instructions::neg_i8_check>,
		neg_i16_check            = index_of<instructions::neg_i16_check>,
		neg_i32_check            = index_of<instructions::neg_i32_check>,
		neg_i64_check            = index_of<instructions::neg_i64_check>,
		add_i8                   = index_of<instructions::add_i8>,
		add_i16                  = index_of<instructions::add_i16>,
		add_i32                  = index_of<instructions::add_i32>,
		add_i64                  = index_of<instructions::add_i64>,
		add_f32                  = index_of<instructions::add_f32>,
		add_f64                  = index_of<instructions::add_f64>,
		add_ptr_i32              = index_of<instructions::add_ptr_i32>,
		add_ptr_u32              = index_of<instructions::add_ptr_u32>,
		add_ptr_i64              = index_of<instructions::add_ptr_i64>,
		add_ptr_u64              = index_of<instructions::add_ptr_u64>,
		add_i8_check             = index_of<instructions::add_i8_check>,
		add_i16_check            = index_of<instructions::add_i16_check>,
		add_i32_check            = index_of<instructions::add_i32_check>,
		add_i64_check            = index_of<instructions::add_i64_check>,
		add_u8_check             = index_of<instructions::add_u8_check>,
		add_u16_check            = index_of<instructions::add_u16_check>,
		add_u32_check            = index_of<instructions::add_u32_check>,
		add_u64_check            = index_of<instructions::add_u64_check>,
		add_f32_check            = index_of<instructions::add_f32_check>,
		add_f64_check            = index_of<instructions::add_f64_check>,
		sub_i8                   = index_of<instructions::sub_i8>,
		sub_i16                  = index_of<instructions::sub_i16>,
		sub_i32                  = index_of<instructions::sub_i32>,
		sub_i64                  = index_of<instructions::sub_i64>,
		sub_f32                  = index_of<instructions::sub_f32>,
		sub_f64                  = index_of<instructions::sub_f64>,
		sub_ptr_i32              = index_of<instructions::sub_ptr_i32>,
		sub_ptr_u32              = index_of<instructions::sub_ptr_u32>,
		sub_ptr_i64              = index_of<instructions::sub_ptr_i64>,
		sub_ptr_u64              = index_of<instructions::sub_ptr_u64>,
		sub_i8_check             = index_of<instructions::sub_i8_check>,
		sub_i16_check            = index_of<instructions::sub_i16_check>,
		sub_i32_check            = index_of<instructions::sub_i32_check>,
		sub_i64_check            = index_of<instructions::sub_i64_check>,
		sub_u8_check             = index_of<instructions::sub_u8_check>,
		sub_u16_check            = index_of<instructions::sub_u16_check>,
		sub_u32_check            = index_of<instructions::sub_u32_check>,
		sub_u64_check            = index_of<instructions::sub_u64_check>,
		sub_f32_check            = index_of<instructions::sub_f32_check>,
		sub_f64_check            = index_of<instructions::sub_f64_check>,
		ptr32_diff               = index_of<instructions::ptr32_diff>,
		ptr64_diff               = index_of<instructions::ptr64_diff>,
		mul_i8                   = index_of<instructions::mul_i8>,
		mul_i16                  = index_of<instructions::mul_i16>,
		mul_i32                  = index_of<instructions::mul_i32>,
		mul_i64                  = index_of<instructions::mul_i64>,
		mul_f32                  = index_of<instructions::mul_f32>,
		mul_f64                  = index_of<instructions::mul_f64>,
		mul_i8_check             = index_of<instructions::mul_i8_check>,
		mul_i16_check            = index_of<instructions::mul_i16_check>,
		mul_i32_check            = index_of<instructions::mul_i32_check>,
		mul_i64_check            = index_of<instructions::mul_i64_check>,
		mul_u8_check             = index_of<instructions::mul_u8_check>,
		mul_u16_check            = index_of<instructions::mul_u16_check>,
		mul_u32_check            = index_of<instructions::mul_u32_check>,
		mul_u64_check            = index_of<instructions::mul_u64_check>,
		mul_f32_check            = index_of<instructions::mul_f32_check>,
		mul_f64_check            = index_of<instructions::mul_f64_check>,
		div_i8                   = index_of<instructions::div_i8>,
		div_i16                  = index_of<instructions::div_i16>,
		div_i32                  = index_of<instructions::div_i32>,
		div_i64                  = index_of<instructions::div_i64>,
		div_u8                   = index_of<instructions::div_u8>,
		div_u16                  = index_of<instructions::div_u16>,
		div_u32                  = index_of<instructions::div_u32>,
		div_u64                  = index_of<instructions::div_u64>,
		div_f32                  = index_of<instructions::div_f32>,
		div_f64                  = index_of<instructions::div_f64>,
		div_i8_check             = index_of<instructions::div_i8_check>,
		div_i16_check            = index_of<instructions::div_i16_check>,
		div_i32_check            = index_of<instructions::div_i32_check>,
		div_i64_check            = index_of<instructions::div_i64_check>,
		div_f32_check            = index_of<instructions::div_f32_check>,
		div_f64_check            = index_of<instructions::div_f64_check>,
		rem_i8                   = index_of<instructions::rem_i8>,
		rem_i16                  = index_of<instructions::rem_i16>,
		rem_i32                  = index_of<instructions::rem_i32>,
		rem_i64                  = index_of<instructions::rem_i64>,
		rem_u8                   = index_of<instructions::rem_u8>,
		rem_u16                  = index_of<instructions::rem_u16>,
		rem_u32                  = index_of<instructions::rem_u32>,
		rem_u64                  = index_of<instructions::rem_u64>,
		not_i1                   = index_of<instructions::not_i1>,
		not_i8                   = index_of<instructions::not_i8>,
		not_i16                  = index_of<instructions::not_i16>,
		not_i32                  = index_of<instructions::not_i32>,
		not_i64                  = index_of<instructions::not_i64>,
		and_i1                   = index_of<instructions::and_i1>,
		and_i8                   = index_of<instructions::and_i8>,
		and_i16                  = index_of<instructions::and_i16>,
		and_i32                  = index_of<instructions::and_i32>,
		and_i64                  = index_of<instructions::and_i64>,
		xor_i1                   = index_of<instructions::xor_i1>,
		xor_i8                   = index_of<instructions::xor_i8>,
		xor_i16                  = index_of<instructions::xor_i16>,
		xor_i32                  = index_of<instructions::xor_i32>,
		xor_i64                  = index_of<instructions::xor_i64>,
		or_i1                    = index_of<instructions::or_i1>,
		or_i8                    = index_of<instructions::or_i8>,
		or_i16                   = index_of<instructions::or_i16>,
		or_i32                   = index_of<instructions::or_i32>,
		or_i64                   = index_of<instructions::or_i64>,
		shl_i8_signed            = index_of<instructions::shl_i8_signed>,
		shl_i16_signed           = index_of<instructions::shl_i16_signed>,
		shl_i32_signed           = index_of<instructions::shl_i32_signed>,
		shl_i64_signed           = index_of<instructions::shl_i64_signed>,
		shl_i8_unsigned          = index_of<instructions::shl_i8_unsigned>,
		shl_i16_unsigned         = index_of<instructions::shl_i16_unsigned>,
		shl_i32_unsigned         = index_of<instructions::shl_i32_unsigned>,
		shl_i64_unsigned         = index_of<instructions::shl_i64_unsigned>,
		shr_i8_signed            = index_of<instructions::shr_i8_signed>,
		shr_i16_signed           = index_of<instructions::shr_i16_signed>,
		shr_i32_signed           = index_of<instructions::shr_i32_signed>,
		shr_i64_signed           = index_of<instructions::shr_i64_signed>,
		shr_i8_unsigned          = index_of<instructions::shr_i8_unsigned>,
		shr_i16_unsigned         = index_of<instructions::shr_i16_unsigned>,
		shr_i32_unsigned         = index_of<instructions::shr_i32_unsigned>,
		shr_i64_unsigned         = index_of<instructions::shr_i64_unsigned>,
		abs_i8                   = index_of<instructions::abs_i8>,
		abs_i16                  = index_of<instructions::abs_i16>,
		abs_i32                  = index_of<instructions::abs_i32>,
		abs_i64                  = index_of<instructions::abs_i64>,
		abs_f32                  = index_of<instructions::abs_f32>,
		abs_f64                  = index_of<instructions::abs_f64>,
		abs_i8_check             = index_of<instructions::abs_i8_check>,
		abs_i16_check            = index_of<instructions::abs_i16_check>,
		abs_i32_check            = index_of<instructions::abs_i32_check>,
		abs_i64_check            = index_of<instructions::abs_i64_check>,
		abs_f32_check            = index_of<instructions::abs_f32_check>,
		abs_f64_check            = index_of<instructions::abs_f64_check>,
		min_i8                   = index_of<instructions::min_i8>,
		min_i16                  = index_of<instructions::min_i16>,
		min_i32                  = index_of<instructions::min_i32>,
		min_i64                  = index_of<instructions::min_i64>,
		min_u8                   = index_of<instructions::min_u8>,
		min_u16                  = index_of<instructions::min_u16>,
		min_u32                  = index_of<instructions::min_u32>,
		min_u64                  = index_of<instructions::min_u64>,
		min_f32                  = index_of<instructions::min_f32>,
		min_f64                  = index_of<instructions::min_f64>,
		min_f32_check            = index_of<instructions::min_f32_check>,
		min_f64_check            = index_of<instructions::min_f64_check>,
		max_i8                   = index_of<instructions::max_i8>,
		max_i16                  = index_of<instructions::max_i16>,
		max_i32                  = index_of<instructions::max_i32>,
		max_i64                  = index_of<instructions::max_i64>,
		max_u8                   = index_of<instructions::max_u8>,
		max_u16                  = index_of<instructions::max_u16>,
		max_u32                  = index_of<instructions::max_u32>,
		max_u64                  = index_of<instructions::max_u64>,
		max_f32                  = index_of<instructions::max_f32>,
		max_f64                  = index_of<instructions::max_f64>,
		max_f32_check            = index_of<instructions::max_f32_check>,
		max_f64_check            = index_of<instructions::max_f64_check>,
		exp_f32                  = index_of<instructions::exp_f32>,
		exp_f64                  = index_of<instructions::exp_f64>,
		exp_f32_check            = index_of<instructions::exp_f32_check>,
		exp_f64_check            = index_of<instructions::exp_f64_check>,
		exp2_f32                 = index_of<instructions::exp2_f32>,
		exp2_f64                 = index_of<instructions::exp2_f64>,
		exp2_f32_check           = index_of<instructions::exp2_f32_check>,
		exp2_f64_check           = index_of<instructions::exp2_f64_check>,
		expm1_f32                = index_of<instructions::expm1_f32>,
		expm1_f64                = index_of<instructions::expm1_f64>,
		expm1_f32_check          = index_of<instructions::expm1_f32_check>,
		expm1_f64_check          = index_of<instructions::expm1_f64_check>,
		log_f32                  = index_of<instructions::log_f32>,
		log_f64                  = index_of<instructions::log_f64>,
		log_f32_check            = index_of<instructions::log_f32_check>,
		log_f64_check            = index_of<instructions::log_f64_check>,
		log10_f32                = index_of<instructions::log10_f32>,
		log10_f64                = index_of<instructions::log10_f64>,
		log10_f32_check          = index_of<instructions::log10_f32_check>,
		log10_f64_check          = index_of<instructions::log10_f64_check>,
		log2_f32                 = index_of<instructions::log2_f32>,
		log2_f64                 = index_of<instructions::log2_f64>,
		log2_f32_check           = index_of<instructions::log2_f32_check>,
		log2_f64_check           = index_of<instructions::log2_f64_check>,
		log1p_f32                = index_of<instructions::log1p_f32>,
		log1p_f64                = index_of<instructions::log1p_f64>,
		log1p_f32_check          = index_of<instructions::log1p_f32_check>,
		log1p_f64_check          = index_of<instructions::log1p_f64_check>,
		sqrt_f32                 = index_of<instructions::sqrt_f32>,
		sqrt_f64                 = index_of<instructions::sqrt_f64>,
		sqrt_f32_check           = index_of<instructions::sqrt_f32_check>,
		sqrt_f64_check           = index_of<instructions::sqrt_f64_check>,
		pow_f32                  = index_of<instructions::pow_f32>,
		pow_f64                  = index_of<instructions::pow_f64>,
		pow_f32_check            = index_of<instructions::pow_f32_check>,
		pow_f64_check            = index_of<instructions::pow_f64_check>,
		cbrt_f32                 = index_of<instructions::cbrt_f32>,
		cbrt_f64                 = index_of<instructions::cbrt_f64>,
		cbrt_f32_check           = index_of<instructions::cbrt_f32_check>,
		cbrt_f64_check           = index_of<instructions::cbrt_f64_check>,
		hypot_f32                = index_of<instructions::hypot_f32>,
		hypot_f64                = index_of<instructions::hypot_f64>,
		hypot_f32_check          = index_of<instructions::hypot_f32_check>,
		hypot_f64_check          = index_of<instructions::hypot_f64_check>,
		sin_f32                  = index_of<instructions::sin_f32>,
		sin_f64                  = index_of<instructions::sin_f64>,
		sin_f32_check            = index_of<instructions::sin_f32_check>,
		sin_f64_check            = index_of<instructions::sin_f64_check>,
		cos_f32                  = index_of<instructions::cos_f32>,
		cos_f64                  = index_of<instructions::cos_f64>,
		cos_f32_check            = index_of<instructions::cos_f32_check>,
		cos_f64_check            = index_of<instructions::cos_f64_check>,
		tan_f32                  = index_of<instructions::tan_f32>,
		tan_f64                  = index_of<instructions::tan_f64>,
		tan_f32_check            = index_of<instructions::tan_f32_check>,
		tan_f64_check            = index_of<instructions::tan_f64_check>,
		asin_f32                 = index_of<instructions::asin_f32>,
		asin_f64                 = index_of<instructions::asin_f64>,
		asin_f32_check           = index_of<instructions::asin_f32_check>,
		asin_f64_check           = index_of<instructions::asin_f64_check>,
		acos_f32                 = index_of<instructions::acos_f32>,
		acos_f64                 = index_of<instructions::acos_f64>,
		acos_f32_check           = index_of<instructions::acos_f32_check>,
		acos_f64_check           = index_of<instructions::acos_f64_check>,
		atan_f32                 = index_of<instructions::atan_f32>,
		atan_f64                 = index_of<instructions::atan_f64>,
		atan_f32_check           = index_of<instructions::atan_f32_check>,
		atan_f64_check           = index_of<instructions::atan_f64_check>,
		atan2_f32                = index_of<instructions::atan2_f32>,
		atan2_f64                = index_of<instructions::atan2_f64>,
		atan2_f32_check          = index_of<instructions::atan2_f32_check>,
		atan2_f64_check          = index_of<instructions::atan2_f64_check>,
		sinh_f32                 = index_of<instructions::sinh_f32>,
		sinh_f64                 = index_of<instructions::sinh_f64>,
		sinh_f32_check           = index_of<instructions::sinh_f32_check>,
		sinh_f64_check           = index_of<instructions::sinh_f64_check>,
		cosh_f32                 = index_of<instructions::cosh_f32>,
		cosh_f64                 = index_of<instructions::cosh_f64>,
		cosh_f32_check           = index_of<instructions::cosh_f32_check>,
		cosh_f64_check           = index_of<instructions::cosh_f64_check>,
		tanh_f32                 = index_of<instructions::tanh_f32>,
		tanh_f64                 = index_of<instructions::tanh_f64>,
		tanh_f32_check           = index_of<instructions::tanh_f32_check>,
		tanh_f64_check           = index_of<instructions::tanh_f64_check>,
		asinh_f32                = index_of<instructions::asinh_f32>,
		asinh_f64                = index_of<instructions::asinh_f64>,
		asinh_f32_check          = index_of<instructions::asinh_f32_check>,
		asinh_f64_check          = index_of<instructions::asinh_f64_check>,
		acosh_f32                = index_of<instructions::acosh_f32>,
		acosh_f64                = index_of<instructions::acosh_f64>,
		acosh_f32_check          = index_of<instructions::acosh_f32_check>,
		acosh_f64_check          = index_of<instructions::acosh_f64_check>,
		atanh_f32                = index_of<instructions::atanh_f32>,
		atanh_f64                = index_of<instructions::atanh_f64>,
		atanh_f32_check          = index_of<instructions::atanh_f32_check>,
		atanh_f64_check          = index_of<instructions::atanh_f64_check>,
		erf_f32                  = index_of<instructions::erf_f32>,
		erf_f64                  = index_of<instructions::erf_f64>,
		erf_f32_check            = index_of<instructions::erf_f32_check>,
		erf_f64_check            = index_of<instructions::erf_f64_check>,
		erfc_f32                 = index_of<instructions::erfc_f32>,
		erfc_f64                 = index_of<instructions::erfc_f64>,
		erfc_f32_check           = index_of<instructions::erfc_f32_check>,
		erfc_f64_check           = index_of<instructions::erfc_f64_check>,
		tgamma_f32               = index_of<instructions::tgamma_f32>,
		tgamma_f64               = index_of<instructions::tgamma_f64>,
		tgamma_f32_check         = index_of<instructions::tgamma_f32_check>,
		tgamma_f64_check         = index_of<instructions::tgamma_f64_check>,
		lgamma_f32               = index_of<instructions::lgamma_f32>,
		lgamma_f64               = index_of<instructions::lgamma_f64>,
		lgamma_f32_check         = index_of<instructions::lgamma_f32_check>,
		lgamma_f64_check         = index_of<instructions::lgamma_f64_check>,
		bitreverse_u8            = index_of<instructions::bitreverse_u8>,
		bitreverse_u16           = index_of<instructions::bitreverse_u16>,
		bitreverse_u32           = index_of<instructions::bitreverse_u32>,
		bitreverse_u64           = index_of<instructions::bitreverse_u64>,
		popcount_u8              = index_of<instructions::popcount_u8>,
		popcount_u16             = index_of<instructions::popcount_u16>,
		popcount_u32             = index_of<instructions::popcount_u32>,
		popcount_u64             = index_of<instructions::popcount_u64>,
		byteswap_u16             = index_of<instructions::byteswap_u16>,
		byteswap_u32             = index_of<instructions::byteswap_u32>,
		byteswap_u64             = index_of<instructions::byteswap_u64>,
		clz_u8                   = index_of<instructions::clz_u8>,
		clz_u16                  = index_of<instructions::clz_u16>,
		clz_u32                  = index_of<instructions::clz_u32>,
		clz_u64                  = index_of<instructions::clz_u64>,
		ctz_u8                   = index_of<instructions::ctz_u8>,
		ctz_u16                  = index_of<instructions::ctz_u16>,
		ctz_u32                  = index_of<instructions::ctz_u32>,
		ctz_u64                  = index_of<instructions::ctz_u64>,
		fshl_u8                  = index_of<instructions::fshl_u8>,
		fshl_u16                 = index_of<instructions::fshl_u16>,
		fshl_u32                 = index_of<instructions::fshl_u32>,
		fshl_u64                 = index_of<instructions::fshl_u64>,
		fshr_u8                  = index_of<instructions::fshr_u8>,
		fshr_u16                 = index_of<instructions::fshr_u16>,
		fshr_u32                 = index_of<instructions::fshr_u32>,
		fshr_u64                 = index_of<instructions::fshr_u64>,
		const_gep                = index_of<instructions::const_gep>,
		array_gep_i32            = index_of<instructions::array_gep_i32>,
		array_gep_i64            = index_of<instructions::array_gep_i64>,
		const_memcpy             = index_of<instructions::const_memcpy>,
		const_memset_zero        = index_of<instructions::const_memset_zero>,
		function_call            = index_of<instructions::function_call>,
		jump                     = index_of<instructions::jump>,
		conditional_jump         = index_of<instructions::conditional_jump>,
		ret                      = index_of<instructions::ret>,
		ret_void                 = index_of<instructions::ret_void>,
		unreachable              = index_of<instructions::unreachable>,
		error                    = index_of<instructions::error>,
		diagnostic_str           = index_of<instructions::diagnostic_str>,
		array_bounds_check_i32   = index_of<instructions::array_bounds_check_i32>,
		array_bounds_check_u32   = index_of<instructions::array_bounds_check_u32>,
		array_bounds_check_i64   = index_of<instructions::array_bounds_check_i64>,
		array_bounds_check_u64   = index_of<instructions::array_bounds_check_u64>,
		optional_get_value_check = index_of<instructions::optional_get_value_check>,
		str_construction_check   = index_of<instructions::str_construction_check>,
		slice_construction_check = index_of<instructions::slice_construction_check>,
	};

	bool is_terminator(void) const
	{
		switch (this->index())
		{
		case jump:
		case conditional_jump:
		case ret:
		case ret_void:
		case unreachable:
			return true;
		default:
			return false;
		}
	}
};

static_assert(std::is_trivially_copy_constructible_v<instruction>);
static_assert(std::is_trivially_destructible_v<instruction>);


} // namespace comptime

#endif // COMPTIME_INSTRUCTIONS_H
