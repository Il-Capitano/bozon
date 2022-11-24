#ifndef COMPTIME_INSTRUCTIONS_H
#define COMPTIME_INSTRUCTIONS_H

#include "core.h"

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
	size_t instruction_value_offset;
};

struct function
{
	bz::vector<basic_block> blocks;
	bz::vector<instruction_value> instruction_values;
	bz::vector<uint32_t> parameter_offsets;
};

namespace instructions
{

using arg_t = uint32_t;

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
	bz::array<arg_t, ArgsCount> args;
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


struct alloca
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::ptr;

	size_t size;
	size_t align;
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

struct const_gep
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::ptr;

	size_t offset;
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


struct jump
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::none;

	uint32_t next_bb_index;
};

struct conditional_jump
{
	static inline constexpr bz::array arg_types = { value_type::i1 };
	static inline constexpr value_type result_type = value_type::none;

	uint32_t true_bb_index;
	uint32_t false_bb_index;
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

struct error
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::none;

	uint32_t error_index;
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
	instructions::alloca,
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
	instructions::const_gep,
	instructions::const_memcpy,
	instructions::const_memset_zero,
	instructions::jump,
	instructions::conditional_jump,
	instructions::ret,
	instructions::ret_void,
	instructions::error
>;

using instruction_with_args_list = bz::meta::transform_type_pack<instructions::instruction_with_args, instruction_list>;
using instruction_base_t = bz::meta::apply_type_pack<bz::variant, instruction_with_args_list>;

struct instruction : instruction_base_t
{
	using base_t = instruction_base_t;
	template<typename Inst>
	static inline constexpr base_t::index_t index_of = base_t::index_of<instructions::instruction_with_args<Inst>>;

	static_assert(variant_count == 110);
	enum : base_t::index_t
	{
		const_i1              = index_of<instructions::const_i1>,
		const_i8              = index_of<instructions::const_i8>,
		const_i16             = index_of<instructions::const_i16>,
		const_i32             = index_of<instructions::const_i32>,
		const_i64             = index_of<instructions::const_i64>,
		const_u8              = index_of<instructions::const_u8>,
		const_u16             = index_of<instructions::const_u16>,
		const_u32             = index_of<instructions::const_u32>,
		const_u64             = index_of<instructions::const_u64>,
		const_f32             = index_of<instructions::const_f32>,
		const_f64             = index_of<instructions::const_f64>,
		const_ptr_null        = index_of<instructions::const_ptr_null>,
		alloca                = index_of<instructions::alloca>,
		load_i1_be            = index_of<instructions::load_i1_be>,
		load_i8_be            = index_of<instructions::load_i8_be>,
		load_i16_be           = index_of<instructions::load_i16_be>,
		load_i32_be           = index_of<instructions::load_i32_be>,
		load_i64_be           = index_of<instructions::load_i64_be>,
		load_f32_be           = index_of<instructions::load_f32_be>,
		load_f64_be           = index_of<instructions::load_f64_be>,
		load_ptr32_be         = index_of<instructions::load_ptr32_be>,
		load_ptr64_be         = index_of<instructions::load_ptr64_be>,
		load_i1_le            = index_of<instructions::load_i1_le>,
		load_i8_le            = index_of<instructions::load_i8_le>,
		load_i16_le           = index_of<instructions::load_i16_le>,
		load_i32_le           = index_of<instructions::load_i32_le>,
		load_i64_le           = index_of<instructions::load_i64_le>,
		load_f32_le           = index_of<instructions::load_f32_le>,
		load_f64_le           = index_of<instructions::load_f64_le>,
		load_ptr32_le         = index_of<instructions::load_ptr32_le>,
		load_ptr64_le         = index_of<instructions::load_ptr64_le>,
		store_i1_be           = index_of<instructions::store_i1_be>,
		store_i8_be           = index_of<instructions::store_i8_be>,
		store_i16_be          = index_of<instructions::store_i16_be>,
		store_i32_be          = index_of<instructions::store_i32_be>,
		store_i64_be          = index_of<instructions::store_i64_be>,
		store_f32_be          = index_of<instructions::store_f32_be>,
		store_f64_be          = index_of<instructions::store_f64_be>,
		store_ptr32_be        = index_of<instructions::store_ptr32_be>,
		store_ptr64_be        = index_of<instructions::store_ptr64_be>,
		store_i1_le           = index_of<instructions::store_i1_le>,
		store_i8_le           = index_of<instructions::store_i8_le>,
		store_i16_le          = index_of<instructions::store_i16_le>,
		store_i32_le          = index_of<instructions::store_i32_le>,
		store_i64_le          = index_of<instructions::store_i64_le>,
		store_f32_le          = index_of<instructions::store_f32_le>,
		store_f64_le          = index_of<instructions::store_f64_le>,
		store_ptr32_le        = index_of<instructions::store_ptr32_le>,
		store_ptr64_le        = index_of<instructions::store_ptr64_le>,
		cast_zext_i1_to_i8    = index_of<instructions::cast_zext_i1_to_i8>,
		cast_zext_i1_to_i16   = index_of<instructions::cast_zext_i1_to_i16>,
		cast_zext_i1_to_i32   = index_of<instructions::cast_zext_i1_to_i32>,
		cast_zext_i1_to_i64   = index_of<instructions::cast_zext_i1_to_i64>,
		cast_zext_i8_to_i16   = index_of<instructions::cast_zext_i8_to_i16>,
		cast_zext_i8_to_i32   = index_of<instructions::cast_zext_i8_to_i32>,
		cast_zext_i8_to_i64   = index_of<instructions::cast_zext_i8_to_i64>,
		cast_zext_i16_to_i32  = index_of<instructions::cast_zext_i16_to_i32>,
		cast_zext_i16_to_i64  = index_of<instructions::cast_zext_i16_to_i64>,
		cast_zext_i32_to_i64  = index_of<instructions::cast_zext_i32_to_i64>,
		cast_sext_i8_to_i16   = index_of<instructions::cast_sext_i8_to_i16>,
		cast_sext_i8_to_i32   = index_of<instructions::cast_sext_i8_to_i32>,
		cast_sext_i8_to_i64   = index_of<instructions::cast_sext_i8_to_i64>,
		cast_sext_i16_to_i32  = index_of<instructions::cast_sext_i16_to_i32>,
		cast_sext_i16_to_i64  = index_of<instructions::cast_sext_i16_to_i64>,
		cast_sext_i32_to_i64  = index_of<instructions::cast_sext_i32_to_i64>,
		cast_trunc_i64_to_i8  = index_of<instructions::cast_trunc_i64_to_i8>,
		cast_trunc_i64_to_i16 = index_of<instructions::cast_trunc_i64_to_i16>,
		cast_trunc_i64_to_i32 = index_of<instructions::cast_trunc_i64_to_i32>,
		cast_trunc_i32_to_i8  = index_of<instructions::cast_trunc_i32_to_i8>,
		cast_trunc_i32_to_i16 = index_of<instructions::cast_trunc_i32_to_i16>,
		cast_trunc_i16_to_i8  = index_of<instructions::cast_trunc_i16_to_i8>,
		cast_f32_to_f64       = index_of<instructions::cast_f32_to_f64>,
		cast_f64_to_f32       = index_of<instructions::cast_f64_to_f32>,
		cast_f32_to_i8        = index_of<instructions::cast_f32_to_i8>,
		cast_f32_to_i16       = index_of<instructions::cast_f32_to_i16>,
		cast_f32_to_i32       = index_of<instructions::cast_f32_to_i32>,
		cast_f32_to_i64       = index_of<instructions::cast_f32_to_i64>,
		cast_f32_to_u8        = index_of<instructions::cast_f32_to_u8>,
		cast_f32_to_u16       = index_of<instructions::cast_f32_to_u16>,
		cast_f32_to_u32       = index_of<instructions::cast_f32_to_u32>,
		cast_f32_to_u64       = index_of<instructions::cast_f32_to_u64>,
		cast_f64_to_i8        = index_of<instructions::cast_f64_to_i8>,
		cast_f64_to_i16       = index_of<instructions::cast_f64_to_i16>,
		cast_f64_to_i32       = index_of<instructions::cast_f64_to_i32>,
		cast_f64_to_i64       = index_of<instructions::cast_f64_to_i64>,
		cast_f64_to_u8        = index_of<instructions::cast_f64_to_u8>,
		cast_f64_to_u16       = index_of<instructions::cast_f64_to_u16>,
		cast_f64_to_u32       = index_of<instructions::cast_f64_to_u32>,
		cast_f64_to_u64       = index_of<instructions::cast_f64_to_u64>,
		cast_i8_to_f32        = index_of<instructions::cast_i8_to_f32>,
		cast_i16_to_f32       = index_of<instructions::cast_i16_to_f32>,
		cast_i32_to_f32       = index_of<instructions::cast_i32_to_f32>,
		cast_i64_to_f32       = index_of<instructions::cast_i64_to_f32>,
		cast_u8_to_f32        = index_of<instructions::cast_u8_to_f32>,
		cast_u16_to_f32       = index_of<instructions::cast_u16_to_f32>,
		cast_u32_to_f32       = index_of<instructions::cast_u32_to_f32>,
		cast_u64_to_f32       = index_of<instructions::cast_u64_to_f32>,
		cast_i8_to_f64        = index_of<instructions::cast_i8_to_f64>,
		cast_i16_to_f64       = index_of<instructions::cast_i16_to_f64>,
		cast_i32_to_f64       = index_of<instructions::cast_i32_to_f64>,
		cast_i64_to_f64       = index_of<instructions::cast_i64_to_f64>,
		cast_u8_to_f64        = index_of<instructions::cast_u8_to_f64>,
		cast_u16_to_f64       = index_of<instructions::cast_u16_to_f64>,
		cast_u32_to_f64       = index_of<instructions::cast_u32_to_f64>,
		cast_u64_to_f64       = index_of<instructions::cast_u64_to_f64>,
		const_gep             = index_of<instructions::const_gep>,
		const_memcpy          = index_of<instructions::const_memcpy>,
		const_memset_zero     = index_of<instructions::const_memset_zero>,
		jump                  = index_of<instructions::jump>,
		conditional_jump      = index_of<instructions::conditional_jump>,
		ret                   = index_of<instructions::ret>,
		ret_void              = index_of<instructions::ret_void>,
		error                 = index_of<instructions::error>,
	};

	bool is_terminator(void) const
	{
		switch (this->index())
		{
		case jump:
		case conditional_jump:
		case ret:
		case ret_void:
		case error:
			return true;
		default:
			return false;
		}
	}
};


} // namespace comptime

#endif // COMPTIME_INSTRUCTIONS_H
