#ifndef COMPTIME_INSTRUCTIONS_H
#define COMPTIME_INSTRUCTIONS_H

#include "core.h"
#include "types.h"
#include "ctx/warnings.h"

namespace comptime
{

enum class value_type
{
	i1, i8, i16, i32, i64,
	f32, f64,
	ptr,
	none, any,
};

struct none_t
{};

using ptr_t = uint64_t;

union instruction_value
{
	bool i1;
	uint8_t i8;
	uint16_t i16;
	uint32_t i32;
	uint64_t i64;
	float32_t f32;
	float64_t f64;
	ptr_t ptr;
	none_t none;
};

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

	uint32_t src_tokens_index;
};

struct cmp_eq_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::i1;

	uint32_t src_tokens_index;
};

struct cmp_eq_f32_unchecked
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_eq_f64_unchecked
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::i1;
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

	uint32_t src_tokens_index;
};

struct cmp_neq_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::i1;

	uint32_t src_tokens_index;
};

struct cmp_neq_f32_unchecked
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_neq_f64_unchecked
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::i1;
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

	uint32_t src_tokens_index;
};

struct cmp_lt_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::i1;

	uint32_t src_tokens_index;
};

struct cmp_lt_f32_unchecked
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_lt_f64_unchecked
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::i1;
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

	uint32_t src_tokens_index;
};

struct cmp_gt_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::i1;

	uint32_t src_tokens_index;
};

struct cmp_gt_f32_unchecked
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_gt_f64_unchecked
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::i1;
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

	uint32_t src_tokens_index;
};

struct cmp_lte_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::i1;

	uint32_t src_tokens_index;
};

struct cmp_lte_f32_unchecked
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_lte_f64_unchecked
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::i1;
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

	uint32_t src_tokens_index;
};

struct cmp_gte_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::i1;

	uint32_t src_tokens_index;
};

struct cmp_gte_f32_unchecked
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct cmp_gte_f64_unchecked
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::i1;
};

struct add_i8_unchecked
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
};

struct add_i16_unchecked
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
};

struct add_i32_unchecked
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
};

struct add_i64_unchecked
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
};

struct sub_i8_unchecked
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
};

struct sub_i16_unchecked
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
};

struct sub_i32_unchecked
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
};

struct sub_i64_unchecked
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
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

struct abs_i8
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;

	uint32_t src_tokens_index;
};

struct abs_i16
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;

	uint32_t src_tokens_index;
};

struct abs_i32
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;

	uint32_t src_tokens_index;
};

struct abs_i64
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;

	uint32_t src_tokens_index;
};

struct abs_f32
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;

	uint32_t src_tokens_index;
};

struct abs_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;

	uint32_t src_tokens_index;
};

struct abs_i8_unchecked
{
	static inline constexpr bz::array arg_types = { value_type::i8 };
	static inline constexpr value_type result_type = value_type::i8;
};

struct abs_i16_unchecked
{
	static inline constexpr bz::array arg_types = { value_type::i16 };
	static inline constexpr value_type result_type = value_type::i16;
};

struct abs_i32_unchecked
{
	static inline constexpr bz::array arg_types = { value_type::i32 };
	static inline constexpr value_type result_type = value_type::i32;
};

struct abs_i64_unchecked
{
	static inline constexpr bz::array arg_types = { value_type::i64 };
	static inline constexpr value_type result_type = value_type::i64;
};

struct abs_f32_unchecked
{
	static inline constexpr bz::array arg_types = { value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct abs_f64_unchecked
{
	static inline constexpr bz::array arg_types = { value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
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

	uint32_t src_tokens_index;
};

struct min_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;

	uint32_t src_tokens_index;
};

struct min_f32_unchecked
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct min_f64_unchecked
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
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

	uint32_t src_tokens_index;
};

struct max_f64
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;

	uint32_t src_tokens_index;
};

struct max_f32_unchecked
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::f32 };
	static inline constexpr value_type result_type = value_type::f32;
};

struct max_f64_unchecked
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::f64 };
	static inline constexpr value_type result_type = value_type::f64;
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
	instructions::cmp_eq_f32_unchecked,
	instructions::cmp_eq_f64_unchecked,
	instructions::cmp_eq_ptr,
	instructions::cmp_neq_i1,
	instructions::cmp_neq_i8,
	instructions::cmp_neq_i16,
	instructions::cmp_neq_i32,
	instructions::cmp_neq_i64,
	instructions::cmp_neq_f32,
	instructions::cmp_neq_f64,
	instructions::cmp_neq_f32_unchecked,
	instructions::cmp_neq_f64_unchecked,
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
	instructions::cmp_lt_f32_unchecked,
	instructions::cmp_lt_f64_unchecked,
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
	instructions::cmp_gt_f32_unchecked,
	instructions::cmp_gt_f64_unchecked,
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
	instructions::cmp_lte_f32_unchecked,
	instructions::cmp_lte_f64_unchecked,
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
	instructions::cmp_gte_f32_unchecked,
	instructions::cmp_gte_f64_unchecked,
	instructions::add_i8_unchecked,
	instructions::add_i16_unchecked,
	instructions::add_i32_unchecked,
	instructions::add_i64_unchecked,
	instructions::sub_i8_unchecked,
	instructions::sub_i16_unchecked,
	instructions::sub_i32_unchecked,
	instructions::sub_i64_unchecked,
	instructions::ptr32_diff,
	instructions::ptr64_diff,
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
	instructions::abs_i8,
	instructions::abs_i16,
	instructions::abs_i32,
	instructions::abs_i64,
	instructions::abs_f32,
	instructions::abs_f64,
	instructions::abs_i8_unchecked,
	instructions::abs_i16_unchecked,
	instructions::abs_i32_unchecked,
	instructions::abs_i64_unchecked,
	instructions::abs_f32_unchecked,
	instructions::abs_f64_unchecked,
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
	instructions::min_f32_unchecked,
	instructions::min_f64_unchecked,
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
	instructions::max_f32_unchecked,
	instructions::max_f64_unchecked,
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

	static_assert(variant_count == 253);
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
		cmp_eq_f32_unchecked     = index_of<instructions::cmp_eq_f32_unchecked>,
		cmp_eq_f64_unchecked     = index_of<instructions::cmp_eq_f64_unchecked>,
		cmp_eq_ptr               = index_of<instructions::cmp_eq_ptr>,
		cmp_neq_i1               = index_of<instructions::cmp_neq_i1>,
		cmp_neq_i8               = index_of<instructions::cmp_neq_i8>,
		cmp_neq_i16              = index_of<instructions::cmp_neq_i16>,
		cmp_neq_i32              = index_of<instructions::cmp_neq_i32>,
		cmp_neq_i64              = index_of<instructions::cmp_neq_i64>,
		cmp_neq_f32              = index_of<instructions::cmp_neq_f32>,
		cmp_neq_f64              = index_of<instructions::cmp_neq_f64>,
		cmp_neq_f32_unchecked    = index_of<instructions::cmp_neq_f32_unchecked>,
		cmp_neq_f64_unchecked    = index_of<instructions::cmp_neq_f64_unchecked>,
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
		cmp_lt_f32_unchecked     = index_of<instructions::cmp_lt_f32_unchecked>,
		cmp_lt_f64_unchecked     = index_of<instructions::cmp_lt_f64_unchecked>,
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
		cmp_gt_f32_unchecked     = index_of<instructions::cmp_gt_f32_unchecked>,
		cmp_gt_f64_unchecked     = index_of<instructions::cmp_gt_f64_unchecked>,
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
		cmp_lte_f32_unchecked    = index_of<instructions::cmp_lte_f32_unchecked>,
		cmp_lte_f64_unchecked    = index_of<instructions::cmp_lte_f64_unchecked>,
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
		cmp_gte_f32_unchecked    = index_of<instructions::cmp_gte_f32_unchecked>,
		cmp_gte_f64_unchecked    = index_of<instructions::cmp_gte_f64_unchecked>,
		add_i8_unchecked         = index_of<instructions::add_i8_unchecked>,
		add_i16_unchecked        = index_of<instructions::add_i16_unchecked>,
		add_i32_unchecked        = index_of<instructions::add_i32_unchecked>,
		add_i64_unchecked        = index_of<instructions::add_i64_unchecked>,
		sub_i8_unchecked         = index_of<instructions::sub_i8_unchecked>,
		sub_i16_unchecked        = index_of<instructions::sub_i16_unchecked>,
		sub_i32_unchecked        = index_of<instructions::sub_i32_unchecked>,
		sub_i64_unchecked        = index_of<instructions::sub_i64_unchecked>,
		ptr32_diff               = index_of<instructions::ptr32_diff>,
		ptr64_diff               = index_of<instructions::ptr64_diff>,
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
		abs_i8                   = index_of<instructions::abs_i8>,
		abs_i16                  = index_of<instructions::abs_i16>,
		abs_i32                  = index_of<instructions::abs_i32>,
		abs_i64                  = index_of<instructions::abs_i64>,
		abs_f32                  = index_of<instructions::abs_f32>,
		abs_f64                  = index_of<instructions::abs_f64>,
		abs_i8_unchecked         = index_of<instructions::abs_i8_unchecked>,
		abs_i16_unchecked        = index_of<instructions::abs_i16_unchecked>,
		abs_i32_unchecked        = index_of<instructions::abs_i32_unchecked>,
		abs_i64_unchecked        = index_of<instructions::abs_i64_unchecked>,
		abs_f32_unchecked        = index_of<instructions::abs_f32_unchecked>,
		abs_f64_unchecked        = index_of<instructions::abs_f64_unchecked>,
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
		min_f32_unchecked        = index_of<instructions::min_f32_unchecked>,
		min_f64_unchecked        = index_of<instructions::min_f64_unchecked>,
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
		max_f32_unchecked        = index_of<instructions::max_f32_unchecked>,
		max_f64_unchecked        = index_of<instructions::max_f64_unchecked>,
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
