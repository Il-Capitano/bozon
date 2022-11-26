#include <bit>
#include "instructions.h"
#include "executor_context.h"

namespace comptime
{

static bool execute(instructions::const_i1 const &inst, executor_context &)
{
	return inst.value;
}

static uint8_t execute(instructions::const_i8 const &inst, executor_context &)
{
	return bit_cast<uint8_t>(inst.value);
}

static uint16_t execute(instructions::const_i16 const &inst, executor_context &)
{
	return bit_cast<uint16_t>(inst.value);
}

static uint32_t execute(instructions::const_i32 const &inst, executor_context &)
{
	return bit_cast<uint32_t>(inst.value);
}

static uint64_t execute(instructions::const_i64 const &inst, executor_context &)
{
	return bit_cast<uint64_t>(inst.value);
}

static uint8_t execute(instructions::const_u8 const &inst, executor_context &)
{
	return inst.value;
}

static uint16_t execute(instructions::const_u16 const &inst, executor_context &)
{
	return inst.value;
}

static uint32_t execute(instructions::const_u32 const &inst, executor_context &)
{
	return inst.value;
}

static uint64_t execute(instructions::const_u64 const &inst, executor_context &)
{
	return inst.value;
}

static float32_t execute(instructions::const_f32 const &inst, executor_context &)
{
	return inst.value;
}

static float64_t execute(instructions::const_f64 const &inst, executor_context &)
{
	return inst.value;
}

static ptr_t execute(instructions::const_ptr_null const &, executor_context &)
{
	return 0;
}

template<typename Int>
static Int load_big_endian(uint8_t *mem)
{
	if constexpr (sizeof (Int) == 1)
	{
		return *mem;
	}
	else if constexpr (std::endian::native == std::endian::big)
	{
		Int result = 0;
		std::memcpy(&result, mem, sizeof (Int));
		return result;
	}
	else
	{
		Int result = 0;
		for (size_t i = 0; i < sizeof (Int); ++i)
		{
			result <<= 8;
			result |= static_cast<Int>(mem[i]);
		}
		return result;
	}
}

static bool execute(instructions::load_i1_be const &, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 1);
	return load_big_endian<uint8_t>(mem) != 0;
}

static uint8_t execute(instructions::load_i8_be const &, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 1);
	return load_big_endian<uint8_t>(mem);
}

static uint16_t execute(instructions::load_i16_be const &, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 2);
	return load_big_endian<uint16_t>(mem);
}

static uint32_t execute(instructions::load_i32_be const &, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 4);
	return load_big_endian<uint32_t>(mem);
}

static uint64_t execute(instructions::load_i64_be const &, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 8);
	return load_big_endian<uint64_t>(mem);
}

static float32_t execute(instructions::load_f32_be const &, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 4);
	return bit_cast<float32_t>(load_big_endian<uint32_t>(mem));
}

static float64_t execute(instructions::load_f64_be const &, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 8);
	return bit_cast<float64_t>(load_big_endian<uint64_t>(mem));
}

static ptr_t execute(instructions::load_ptr32_be const &, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 4);
	return load_big_endian<uint32_t>(mem);
}

static ptr_t execute(instructions::load_ptr64_be const &, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 8);
	return load_big_endian<uint64_t>(mem);
}

template<typename Int>
static Int load_little_endian(uint8_t *mem)
{
	if constexpr (sizeof (Int) == 1)
	{
		return *mem;
	}
	else if constexpr (std::endian::native == std::endian::little)
	{
		Int result = 0;
		std::memcpy(&result, mem, sizeof (Int));
		return result;
	}
	else
	{
		Int result = 0;
		for (size_t i = 0; i < sizeof (Int); ++i)
		{
			result |= static_cast<Int>(mem[i]) << (i * 8);
		}
		return result;
	}
}

static bool execute(instructions::load_i1_le const &, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 1);
	return load_little_endian<uint8_t>(mem) != 0;
}

static uint8_t execute(instructions::load_i8_le const &, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 1);
	return load_little_endian<uint8_t>(mem);
}

static uint16_t execute(instructions::load_i16_le const &, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 2);
	return load_little_endian<uint16_t>(mem);
}

static uint32_t execute(instructions::load_i32_le const &, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 4);
	return load_little_endian<uint32_t>(mem);
}

static uint64_t execute(instructions::load_i64_le const &, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 8);
	return load_little_endian<uint64_t>(mem);
}

static float32_t execute(instructions::load_f32_le const &, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 4);
	return bit_cast<float32_t>(load_little_endian<uint32_t>(mem));
}

static float64_t execute(instructions::load_f64_le const &, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 8);
	return bit_cast<float64_t>(load_little_endian<uint64_t>(mem));
}

static ptr_t execute(instructions::load_ptr32_le const &, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 4);
	return load_little_endian<uint32_t>(mem);
}

static ptr_t execute(instructions::load_ptr64_le const &, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 8);
	return load_little_endian<uint64_t>(mem);
}

template<typename Int>
static void store_big_endian(uint8_t *mem, Int value)
{
	if constexpr (sizeof (Int) == 1)
	{
		*mem = value;
	}
	else if constexpr (std::endian::native == std::endian::big)
	{
		std::memcpy(mem, &value, sizeof (Int));
	}
	else
	{
		for (size_t i = 0; i < sizeof (Int); ++i)
		{
			mem[i] = static_cast<uint8_t>(value >> ((sizeof (Int) - i - 1) * 8));
		}
	}
}

static void execute(instructions::store_i1_be const &, bool value, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 1);
	store_big_endian<uint8_t>(mem, value ? 1 : 0);
}

static void execute(instructions::store_i8_be const &, uint8_t value, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 1);
	store_big_endian<uint8_t>(mem, value);
}

static void execute(instructions::store_i16_be const &, uint16_t value, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 2);
	store_big_endian<uint16_t>(mem, value);
}

static void execute(instructions::store_i32_be const &, uint32_t value, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 4);
	store_big_endian<uint32_t>(mem, value);
}

static void execute(instructions::store_i64_be const &, uint64_t value, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 8);
	store_big_endian<uint64_t>(mem, value);
}

static void execute(instructions::store_f32_be const &, float32_t value, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 4);
	store_big_endian<uint32_t>(mem, bit_cast<uint32_t>(value));
}

static void execute(instructions::store_f64_be const &, float64_t value, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 8);
	store_big_endian<uint64_t>(mem, bit_cast<uint64_t>(value));
}

static void execute(instructions::store_ptr32_be const &, ptr_t value, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 4);
	store_big_endian<uint32_t>(mem, static_cast<uint32_t>(value));
}

static void execute(instructions::store_ptr64_be const &, ptr_t value, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 8);
	store_big_endian<uint64_t>(mem, static_cast<uint64_t>(value));
}

template<typename Int>
static void store_little_endian(uint8_t *mem, Int value)
{
	if constexpr (sizeof (Int) == 1)
	{
		*mem = value;
	}
	else if constexpr (std::endian::native == std::endian::little)
	{
		std::memcpy(mem, &value, sizeof (Int));
	}
	else
	{
		for (size_t i = 0; i < sizeof (Int); ++i)
		{
			mem[i] = static_cast<uint8_t>(value >> (i * 8));
		}
	}
}

static void execute(instructions::store_i1_le const &, bool value, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 1);
	store_little_endian<uint8_t>(mem, value ? 1 : 0);
}

static void execute(instructions::store_i8_le const &, uint8_t value, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 1);
	store_little_endian<uint8_t>(mem, value);
}

static void execute(instructions::store_i16_le const &, uint16_t value, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 2);
	store_little_endian<uint16_t>(mem, value);
}

static void execute(instructions::store_i32_le const &, uint32_t value, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 4);
	store_little_endian<uint32_t>(mem, value);
}

static void execute(instructions::store_i64_le const &, uint64_t value, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 8);
	store_little_endian<uint64_t>(mem, value);
}

static void execute(instructions::store_f32_le const &, float32_t value, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 4);
	store_little_endian<uint32_t>(mem, bit_cast<uint32_t>(value));
}

static void execute(instructions::store_f64_le const &, float64_t value, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 8);
	store_little_endian<uint64_t>(mem, bit_cast<uint64_t>(value));
}

static void execute(instructions::store_ptr32_le const &, ptr_t value, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 4);
	store_little_endian<uint32_t>(mem, static_cast<uint32_t>(value));
}

static void execute(instructions::store_ptr64_le const &, ptr_t value, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 8);
	store_little_endian<uint64_t>(mem, static_cast<uint64_t>(value));
}

static uint8_t execute(instructions::cast_zext_i1_to_i8 const &, bool value, executor_context &)
{
	return value ? 1 : 0;
}

static uint16_t execute(instructions::cast_zext_i1_to_i16 const &, bool value, executor_context &)
{
	return value ? 1 : 0;
}

static uint32_t execute(instructions::cast_zext_i1_to_i32 const &, bool value, executor_context &)
{
	return value ? 1 : 0;
}

static uint64_t execute(instructions::cast_zext_i1_to_i64 const &, bool value, executor_context &)
{
	return value ? 1 : 0;
}

static uint16_t execute(instructions::cast_zext_i8_to_i16 const &, uint8_t value, executor_context &)
{
	return static_cast<uint16_t>(value);
}

static uint32_t execute(instructions::cast_zext_i8_to_i32 const &, uint8_t value, executor_context &)
{
	return static_cast<uint32_t>(value);
}

static uint64_t execute(instructions::cast_zext_i8_to_i64 const &, uint8_t value, executor_context &)
{
	return static_cast<uint64_t>(value);
}

static uint32_t execute(instructions::cast_zext_i16_to_i32 const &, uint16_t value, executor_context &)
{
	return static_cast<uint32_t>(value);
}

static uint64_t execute(instructions::cast_zext_i16_to_i64 const &, uint16_t value, executor_context &)
{
	return static_cast<uint64_t>(value);
}

static uint64_t execute(instructions::cast_zext_i32_to_i64 const &, uint32_t value, executor_context &)
{
	return static_cast<uint64_t>(value);
}

static uint16_t execute(instructions::cast_sext_i8_to_i16 const &, uint8_t value, executor_context &)
{
	return static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(value)));
}

static uint32_t execute(instructions::cast_sext_i8_to_i32 const &, uint8_t value, executor_context &)
{
	return static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(value)));
}

static uint64_t execute(instructions::cast_sext_i8_to_i64 const &, uint8_t value, executor_context &)
{
	return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int8_t>(value)));
}

static uint32_t execute(instructions::cast_sext_i16_to_i32 const &, uint16_t value, executor_context &)
{
	return static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(value)));
}

static uint64_t execute(instructions::cast_sext_i16_to_i64 const &, uint16_t value, executor_context &)
{
	return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(value)));
}

static uint64_t execute(instructions::cast_sext_i32_to_i64 const &, uint32_t value, executor_context &)
{
	return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(value)));
}

static uint8_t execute(instructions::cast_trunc_i64_to_i8 const &, uint64_t value, executor_context &)
{
	return static_cast<uint8_t>(value);
}

static uint16_t execute(instructions::cast_trunc_i64_to_i16 const &, uint64_t value, executor_context &)
{
	return static_cast<uint16_t>(value);
}

static uint32_t execute(instructions::cast_trunc_i64_to_i32 const &, uint64_t value, executor_context &)
{
	return static_cast<uint32_t>(value);
}

static uint8_t execute(instructions::cast_trunc_i32_to_i8 const &, uint32_t value, executor_context &)
{
	return static_cast<uint8_t>(value);
}

static uint16_t execute(instructions::cast_trunc_i32_to_i16 const &, uint32_t value, executor_context &)
{
	return static_cast<uint16_t>(value);
}

static uint8_t execute(instructions::cast_trunc_i16_to_i8 const &, uint16_t value, executor_context &)
{
	return static_cast<uint8_t>(value);
}

static float64_t execute(instructions::cast_f32_to_f64 const &, float32_t value, executor_context &)
{
	return static_cast<float64_t>(value);
}

static float32_t execute(instructions::cast_f64_to_f32 const &, float64_t value, executor_context &)
{
	return static_cast<float32_t>(value);
}

static uint8_t execute(instructions::cast_f32_to_i8 const &, float32_t value, executor_context &)
{
	return static_cast<uint8_t>(static_cast<int8_t>(value));
}

static uint16_t execute(instructions::cast_f32_to_i16 const &, float32_t value, executor_context &)
{
	return static_cast<uint16_t>(static_cast<int16_t>(value));
}

static uint32_t execute(instructions::cast_f32_to_i32 const &, float32_t value, executor_context &)
{
	return static_cast<uint32_t>(static_cast<int32_t>(value));
}

static uint64_t execute(instructions::cast_f32_to_i64 const &, float32_t value, executor_context &)
{
	return static_cast<uint64_t>(static_cast<int64_t>(value));
}

static uint8_t execute(instructions::cast_f32_to_u8 const &, float32_t value, executor_context &)
{
	return static_cast<uint8_t>(value);
}

static uint16_t execute(instructions::cast_f32_to_u16 const &, float32_t value, executor_context &)
{
	return static_cast<uint16_t>(value);
}

static uint32_t execute(instructions::cast_f32_to_u32 const &, float32_t value, executor_context &)
{
	return static_cast<uint32_t>(value);
}

static uint64_t execute(instructions::cast_f32_to_u64 const &, float32_t value, executor_context &)
{
	return static_cast<uint64_t>(value);
}

static uint8_t execute(instructions::cast_f64_to_i8 const &, float64_t value, executor_context &)
{
	return static_cast<uint8_t>(static_cast<int8_t>(value));
}

static uint16_t execute(instructions::cast_f64_to_i16 const &, float64_t value, executor_context &)
{
	return static_cast<uint16_t>(static_cast<int16_t>(value));
}

static uint32_t execute(instructions::cast_f64_to_i32 const &, float64_t value, executor_context &)
{
	return static_cast<uint32_t>(static_cast<int32_t>(value));
}

static uint64_t execute(instructions::cast_f64_to_i64 const &, float64_t value, executor_context &)
{
	return static_cast<uint64_t>(static_cast<int64_t>(value));
}

static uint8_t execute(instructions::cast_f64_to_u8 const &, float64_t value, executor_context &)
{
	return static_cast<uint8_t>(value);
}

static uint16_t execute(instructions::cast_f64_to_u16 const &, float64_t value, executor_context &)
{
	return static_cast<uint16_t>(value);
}

static uint32_t execute(instructions::cast_f64_to_u32 const &, float64_t value, executor_context &)
{
	return static_cast<uint32_t>(value);
}

static uint64_t execute(instructions::cast_f64_to_u64 const &, float64_t value, executor_context &)
{
	return static_cast<uint64_t>(value);
}

static float32_t execute(instructions::cast_i8_to_f32 const &, uint8_t value, executor_context &)
{
	return static_cast<float32_t>(static_cast<int8_t>(value));
}

static float32_t execute(instructions::cast_i16_to_f32 const &, uint16_t value, executor_context &)
{
	return static_cast<float32_t>(static_cast<int16_t>(value));
}

static float32_t execute(instructions::cast_i32_to_f32 const &, uint32_t value, executor_context &)
{
	return static_cast<float32_t>(static_cast<int32_t>(value));
}

static float32_t execute(instructions::cast_i64_to_f32 const &, uint64_t value, executor_context &)
{
	return static_cast<float32_t>(static_cast<int64_t>(value));
}

static float32_t execute(instructions::cast_u8_to_f32 const &, uint8_t value, executor_context &)
{
	return static_cast<float32_t>(value);
}

static float32_t execute(instructions::cast_u16_to_f32 const &, uint16_t value, executor_context &)
{
	return static_cast<float32_t>(value);
}

static float32_t execute(instructions::cast_u32_to_f32 const &, uint32_t value, executor_context &)
{
	return static_cast<float32_t>(value);
}

static float32_t execute(instructions::cast_u64_to_f32 const &, uint64_t value, executor_context &)
{
	return static_cast<float32_t>(value);
}

static float64_t execute(instructions::cast_i8_to_f64 const &, uint8_t value, executor_context &)
{
	return static_cast<float64_t>(static_cast<int8_t>(value));
}

static float64_t execute(instructions::cast_i16_to_f64 const &, uint16_t value, executor_context &)
{
	return static_cast<float64_t>(static_cast<int16_t>(value));
}

static float64_t execute(instructions::cast_i32_to_f64 const &, uint32_t value, executor_context &)
{
	return static_cast<float64_t>(static_cast<int32_t>(value));
}

static float64_t execute(instructions::cast_i64_to_f64 const &, uint64_t value, executor_context &)
{
	return static_cast<float64_t>(static_cast<int64_t>(value));
}

static float64_t execute(instructions::cast_u8_to_f64 const &, uint8_t value, executor_context &)
{
	return static_cast<float64_t>(value);
}

static float64_t execute(instructions::cast_u16_to_f64 const &, uint16_t value, executor_context &)
{
	return static_cast<float64_t>(value);
}

static float64_t execute(instructions::cast_u32_to_f64 const &, uint32_t value, executor_context &)
{
	return static_cast<float64_t>(value);
}

static float64_t execute(instructions::cast_u64_to_f64 const &, uint64_t value, executor_context &)
{
	return static_cast<float64_t>(value);
}

static bool execute(instructions::cmp_eq_i1 const &, bool lhs, bool rhs, executor_context &)
{
	return lhs == rhs;
}

static bool execute(instructions::cmp_eq_i8 const &, uint8_t lhs, uint8_t rhs, executor_context &)
{
	return lhs == rhs;
}

static bool execute(instructions::cmp_eq_i16 const &, uint16_t lhs, uint16_t rhs, executor_context &)
{
	return lhs == rhs;
}

static bool execute(instructions::cmp_eq_i32 const &, uint32_t lhs, uint32_t rhs, executor_context &)
{
	return lhs == rhs;
}

static bool execute(instructions::cmp_eq_i64 const &, uint64_t lhs, uint64_t rhs, executor_context &)
{
	return lhs == rhs;
}

static bool execute(instructions::cmp_eq_f32 const &inst, float32_t lhs, float32_t rhs, executor_context &context)
{
	if (std::isnan(lhs) && std::isnan(rhs))
	{
		context.report_warning(
			ctx::warning_kind::nan_compare,
			inst.src_tokens_index,
			bz::format("comparing nans in expression '{} == {}' with type 'float32' evaluates to false", lhs, rhs)
		);
	}
	else if (std::isnan(lhs) || std::isnan(rhs))
	{
		context.report_warning(
			ctx::warning_kind::nan_compare,
			inst.src_tokens_index,
			bz::format("comparing against nan in expression '{} == {}' with type 'float32' evaluates to false", lhs, rhs)
		);
	}
	return lhs == rhs;
}

static bool execute(instructions::cmp_eq_f64 const &inst, float64_t lhs, float64_t rhs, executor_context &context)
{
	if (std::isnan(lhs) && std::isnan(rhs))
	{
		context.report_warning(
			ctx::warning_kind::nan_compare,
			inst.src_tokens_index,
			bz::format("comparing nans in expression '{} == {}' with type 'float64' evaluates to false", lhs, rhs)
		);
	}
	else if (std::isnan(lhs) || std::isnan(rhs))
	{
		context.report_warning(
			ctx::warning_kind::nan_compare,
			inst.src_tokens_index,
			bz::format("comparing against nan in expression '{} == {}' with type 'float64' evaluates to false", lhs, rhs)
		);
	}
	return lhs == rhs;
}

static bool execute(instructions::cmp_eq_f32_unchecked const &, float32_t lhs, float32_t rhs, executor_context &)
{
	return lhs == rhs;
}

static bool execute(instructions::cmp_eq_f64_unchecked const &, float64_t lhs, float64_t rhs, executor_context &)
{
	return lhs == rhs;
}

static bool execute(instructions::cmp_eq_ptr const &, ptr_t lhs, ptr_t rhs, executor_context &)
{
	return lhs == rhs;
}

static bool execute(instructions::cmp_neq_i1 const &, bool lhs, bool rhs, executor_context &)
{
	return lhs != rhs;
}

static bool execute(instructions::cmp_neq_i8 const &, uint8_t lhs, uint8_t rhs, executor_context &)
{
	return lhs != rhs;
}

static bool execute(instructions::cmp_neq_i16 const &, uint16_t lhs, uint16_t rhs, executor_context &)
{
	return lhs != rhs;
}

static bool execute(instructions::cmp_neq_i32 const &, uint32_t lhs, uint32_t rhs, executor_context &)
{
	return lhs != rhs;
}

static bool execute(instructions::cmp_neq_i64 const &, uint64_t lhs, uint64_t rhs, executor_context &)
{
	return lhs != rhs;
}

static bool execute(instructions::cmp_neq_f32 const &inst, float32_t lhs, float32_t rhs, executor_context &context)
{
	if (std::isnan(lhs) && std::isnan(rhs))
	{
		context.report_warning(
			ctx::warning_kind::nan_compare,
			inst.src_tokens_index,
			bz::format("comparing nans in expression '{} != {}' with type 'float32' evaluates to true", lhs, rhs)
		);
	}
	else if (std::isnan(lhs) || std::isnan(rhs))
	{
		context.report_warning(
			ctx::warning_kind::nan_compare,
			inst.src_tokens_index,
			bz::format("comparing against nan in expression '{} != {}' with type 'float32' evaluates to true", lhs, rhs)
		);
	}
	return lhs != rhs;
}

static bool execute(instructions::cmp_neq_f64 const &inst, float64_t lhs, float64_t rhs, executor_context &context)
{
	if (std::isnan(lhs) && std::isnan(rhs))
	{
		context.report_warning(
			ctx::warning_kind::nan_compare,
			inst.src_tokens_index,
			bz::format("comparing nans in expression '{} != {}' with type 'float64' evaluates to true", lhs, rhs)
		);
	}
	else if (std::isnan(lhs) || std::isnan(rhs))
	{
		context.report_warning(
			ctx::warning_kind::nan_compare,
			inst.src_tokens_index,
			bz::format("comparing against nan in expression '{} != {}' with type 'float64' evaluates to true", lhs, rhs)
		);
	}
	return lhs != rhs;
}

static bool execute(instructions::cmp_neq_f32_unchecked const &, float32_t lhs, float32_t rhs, executor_context &)
{
	return lhs != rhs;
}

static bool execute(instructions::cmp_neq_f64_unchecked const &, float64_t lhs, float64_t rhs, executor_context &)
{
	return lhs != rhs;
}

static bool execute(instructions::cmp_neq_ptr const &, ptr_t lhs, ptr_t rhs, executor_context &)
{
	return lhs != rhs;
}

static uint8_t execute(instructions::add_i8_unchecked const &, uint8_t lhs, uint8_t rhs, executor_context &)
{
	return lhs + rhs;
}

static uint16_t execute(instructions::add_i16_unchecked const &, uint16_t lhs, uint16_t rhs, executor_context &)
{
	return lhs + rhs;
}

static uint32_t execute(instructions::add_i32_unchecked const &, uint32_t lhs, uint32_t rhs, executor_context &)
{
	return lhs + rhs;
}

static uint64_t execute(instructions::add_i64_unchecked const &, uint64_t lhs, uint64_t rhs, executor_context &)
{
	return lhs + rhs;
}

static uint8_t execute(instructions::sub_i8_unchecked const &, uint8_t lhs, uint8_t rhs, executor_context &)
{
	return lhs - rhs;
}

static uint16_t execute(instructions::sub_i16_unchecked const &, uint16_t lhs, uint16_t rhs, executor_context &)
{
	return lhs - rhs;
}

static uint32_t execute(instructions::sub_i32_unchecked const &, uint32_t lhs, uint32_t rhs, executor_context &)
{
	return lhs - rhs;
}

static uint64_t execute(instructions::sub_i64_unchecked const &, uint64_t lhs, uint64_t rhs, executor_context &)
{
	return lhs - rhs;
}

static uint32_t execute(instructions::ptr32_diff const &inst, ptr_t lhs, ptr_t rhs, executor_context &)
{
	auto const result = static_cast<int32_t>(lhs - rhs);
	auto const stride = static_cast<int32_t>(inst.stride);
	bz_assert(result % stride == 0);
	return static_cast<uint32_t>(result / stride);
}

static uint64_t execute(instructions::ptr64_diff const &inst, ptr_t lhs, ptr_t rhs, executor_context &)
{
	auto const result = static_cast<int64_t>(lhs - rhs);
	auto const stride = static_cast<int64_t>(inst.stride);
	bz_assert(result % stride == 0);
	return static_cast<uint64_t>(result / stride);
}

static ptr_t execute(instructions::const_gep const &inst, ptr_t ptr, executor_context &)
{
	return ptr + inst.offset;
}

static ptr_t execute(instructions::array_gep_i32 const &inst, ptr_t ptr, uint32_t index, executor_context &)
{
	return ptr + inst.stride * index;
}

static ptr_t execute(instructions::array_gep_i64 const &inst, ptr_t ptr, uint64_t index, executor_context &)
{
	return ptr + inst.stride * index;
}

static void execute(instructions::const_memcpy const &inst, ptr_t dest, ptr_t src, executor_context &context)
{
	auto const dest_mem = context.get_memory(dest, inst.size);
	auto const src_mem  = context.get_memory(src, inst.size);
	std::memcpy(dest_mem, src_mem, inst.size);
}

static void execute(instructions::const_memset_zero const &inst, ptr_t dest, executor_context &context)
{
	auto const dest_mem = context.get_memory(dest, inst.size);
	std::memset(dest_mem, 0, inst.size);
}

static void execute(instructions::jump const &inst, executor_context &context)
{
	context.do_jump(inst.dest);
}

static void execute(instructions::conditional_jump const &inst, bool condition, executor_context &context)
{
	if (condition)
	{
		context.do_jump(inst.true_dest);
	}
	else
	{
		context.do_jump(inst.false_dest);
	}
}

static void execute(instructions::ret const &, instruction_value value, executor_context &context)
{
	context.do_ret(value);
}

static void execute(instructions::ret_void const &, executor_context &context)
{
	context.do_ret_void();
}

static void execute(instructions::error const &error, executor_context &context)
{
	context.report_error(error.error_index);
}


template<value_type type>
struct get_value_type;

template<>
struct get_value_type<value_type::i1>
{
	using type = bool;
};

template<>
struct get_value_type<value_type::i8>
{
	using type = uint8_t;
};

template<>
struct get_value_type<value_type::i16>
{
	using type = uint16_t;
};

template<>
struct get_value_type<value_type::i32>
{
	using type = uint32_t;
};

template<>
struct get_value_type<value_type::i64>
{
	using type = uint64_t;
};

template<>
struct get_value_type<value_type::f32>
{
	using type = float32_t;
};

template<>
struct get_value_type<value_type::f64>
{
	using type = float64_t;
};

template<>
struct get_value_type<value_type::ptr>
{
	using type = ptr_t;
};

template<>
struct get_value_type<value_type::any>
{
	using type = instruction_value;
};

template<value_type type>
using get_value_type_t = typename get_value_type<type>::type;

template<value_type type>
static get_value_type_t<type> get_value(instruction_value value)
{
	if constexpr (type == value_type::i1)
	{
		return value.i1;
	}
	else if constexpr (type == value_type::i8)
	{
		return value.i8;
	}
	else if constexpr (type == value_type::i16)
	{
		return value.i16;
	}
	else if constexpr (type == value_type::i32)
	{
		return value.i32;
	}
	else if constexpr (type == value_type::i64)
	{
		return value.i64;
	}
	else if constexpr (type == value_type::f32)
	{
		return value.f32;
	}
	else if constexpr (type == value_type::f64)
	{
		return value.f64;
	}
	else if constexpr (type == value_type::ptr)
	{
		return value.ptr;
	}
	else if constexpr (type == value_type::any)
	{
		return value;
	}
	else
	{
		static_assert(bz::meta::always_false<get_value_type<type>>);
	}
}

template<value_type type>
static get_value_type_t<type> &get_value_ref(instruction_value &value)
{
	if constexpr (type == value_type::i1)
	{
		return value.i1;
	}
	else if constexpr (type == value_type::i8)
	{
		return value.i8;
	}
	else if constexpr (type == value_type::i16)
	{
		return value.i16;
	}
	else if constexpr (type == value_type::i32)
	{
		return value.i32;
	}
	else if constexpr (type == value_type::i64)
	{
		return value.i64;
	}
	else if constexpr (type == value_type::f32)
	{
		return value.f32;
	}
	else if constexpr (type == value_type::f64)
	{
		return value.f64;
	}
	else if constexpr (type == value_type::ptr)
	{
		return value.ptr;
	}
	else
	{
		static_assert(bz::meta::always_false<get_value_type<type>>);
	}
}


template<typename Inst>
static void execute(executor_context &context)
{
	auto const &inst = context.instructions[context.current_instruction_index];
	instruction_value result;
	if constexpr (instructions::arg_count<Inst> == 0)
	{
		auto const &inst_with_args = inst.get<instructions::instruction_with_args<Inst>>();
		if constexpr (Inst::result_type != value_type::none)
		{
			get_value_ref<Inst::result_type>(result) = execute(inst_with_args.inst, context);
		}
		else
		{
			execute(inst_with_args.inst, context);
			result.none = none_t();
		}
	}
	else
	{
		[&]<size_t ...Is>(bz::meta::index_sequence<Is...>) {
			auto const &inst_with_args = inst.get<instructions::instruction_with_args<Inst>>();
			if constexpr (Inst::result_type != value_type::none)
			{
				get_value_ref<Inst::result_type>(result) = execute(
					inst_with_args.inst,
					get_value<Inst::arg_types[Is]>(context.get_instruction_value(inst_with_args.args[Is]))...,
					context
				);
			}
			else
			{
				execute(
					inst_with_args.inst,
					get_value<Inst::arg_types[Is]>(context.get_instruction_value(inst_with_args.args[Is]))...,
					context
				);
				result.none = none_t();
			}
		}(bz::meta::make_index_sequence<instructions::arg_count<Inst>>());
	}
	context.set_current_instruction_value(result);
}

void execute(executor_context &context)
{
	switch (context.instructions[context.current_instruction_index].index())
	{
		static_assert(instruction::variant_count == 144);
		case instruction::const_i1:
			execute<instructions::const_i1>(context);
			break;
		case instruction::const_i8:
			execute<instructions::const_i8>(context);
			break;
		case instruction::const_i16:
			execute<instructions::const_i16>(context);
			break;
		case instruction::const_i32:
			execute<instructions::const_i32>(context);
			break;
		case instruction::const_i64:
			execute<instructions::const_i64>(context);
			break;
		case instruction::const_u8:
			execute<instructions::const_u8>(context);
			break;
		case instruction::const_u16:
			execute<instructions::const_u16>(context);
			break;
		case instruction::const_u32:
			execute<instructions::const_u32>(context);
			break;
		case instruction::const_u64:
			execute<instructions::const_u64>(context);
			break;
		case instruction::const_f32:
			execute<instructions::const_f32>(context);
			break;
		case instruction::const_f64:
			execute<instructions::const_f64>(context);
			break;
		case instruction::const_ptr_null:
			execute<instructions::const_ptr_null>(context);
			break;
		case instruction::load_i1_be:
			execute<instructions::load_i1_be>(context);
			break;
		case instruction::load_i8_be:
			execute<instructions::load_i8_be>(context);
			break;
		case instruction::load_i16_be:
			execute<instructions::load_i16_be>(context);
			break;
		case instruction::load_i32_be:
			execute<instructions::load_i32_be>(context);
			break;
		case instruction::load_i64_be:
			execute<instructions::load_i64_be>(context);
			break;
		case instruction::load_f32_be:
			execute<instructions::load_f32_be>(context);
			break;
		case instruction::load_f64_be:
			execute<instructions::load_f64_be>(context);
			break;
		case instruction::load_ptr32_be:
			execute<instructions::load_ptr32_be>(context);
			break;
		case instruction::load_ptr64_be:
			execute<instructions::load_ptr64_be>(context);
			break;
		case instruction::load_i1_le:
			execute<instructions::load_i1_le>(context);
			break;
		case instruction::load_i8_le:
			execute<instructions::load_i8_le>(context);
			break;
		case instruction::load_i16_le:
			execute<instructions::load_i16_le>(context);
			break;
		case instruction::load_i32_le:
			execute<instructions::load_i32_le>(context);
			break;
		case instruction::load_i64_le:
			execute<instructions::load_i64_le>(context);
			break;
		case instruction::load_f32_le:
			execute<instructions::load_f32_le>(context);
			break;
		case instruction::load_f64_le:
			execute<instructions::load_f64_le>(context);
			break;
		case instruction::load_ptr32_le:
			execute<instructions::load_ptr32_le>(context);
			break;
		case instruction::load_ptr64_le:
			execute<instructions::load_ptr64_le>(context);
			break;
		case instruction::store_i1_be:
			execute<instructions::store_i1_be>(context);
			break;
		case instruction::store_i8_be:
			execute<instructions::store_i8_be>(context);
			break;
		case instruction::store_i16_be:
			execute<instructions::store_i16_be>(context);
			break;
		case instruction::store_i32_be:
			execute<instructions::store_i32_be>(context);
			break;
		case instruction::store_i64_be:
			execute<instructions::store_i64_be>(context);
			break;
		case instruction::store_f32_be:
			execute<instructions::store_f32_be>(context);
			break;
		case instruction::store_f64_be:
			execute<instructions::store_f64_be>(context);
			break;
		case instruction::store_ptr32_be:
			execute<instructions::store_ptr32_be>(context);
			break;
		case instruction::store_ptr64_be:
			execute<instructions::store_ptr64_be>(context);
			break;
		case instruction::store_i1_le:
			execute<instructions::store_i1_le>(context);
			break;
		case instruction::store_i8_le:
			execute<instructions::store_i8_le>(context);
			break;
		case instruction::store_i16_le:
			execute<instructions::store_i16_le>(context);
			break;
		case instruction::store_i32_le:
			execute<instructions::store_i32_le>(context);
			break;
		case instruction::store_i64_le:
			execute<instructions::store_i64_le>(context);
			break;
		case instruction::store_f32_le:
			execute<instructions::store_f32_le>(context);
			break;
		case instruction::store_f64_le:
			execute<instructions::store_f64_le>(context);
			break;
		case instruction::store_ptr32_le:
			execute<instructions::store_ptr32_le>(context);
			break;
		case instruction::store_ptr64_le:
			execute<instructions::store_ptr64_le>(context);
			break;
		case instruction::cast_zext_i1_to_i8:
			execute<instructions::cast_zext_i1_to_i8>(context);
			break;
		case instruction::cast_zext_i1_to_i16:
			execute<instructions::cast_zext_i1_to_i16>(context);
			break;
		case instruction::cast_zext_i1_to_i32:
			execute<instructions::cast_zext_i1_to_i32>(context);
			break;
		case instruction::cast_zext_i1_to_i64:
			execute<instructions::cast_zext_i1_to_i64>(context);
			break;
		case instruction::cast_zext_i8_to_i16:
			execute<instructions::cast_zext_i8_to_i16>(context);
			break;
		case instruction::cast_zext_i8_to_i32:
			execute<instructions::cast_zext_i8_to_i32>(context);
			break;
		case instruction::cast_zext_i8_to_i64:
			execute<instructions::cast_zext_i8_to_i64>(context);
			break;
		case instruction::cast_zext_i16_to_i32:
			execute<instructions::cast_zext_i16_to_i32>(context);
			break;
		case instruction::cast_zext_i16_to_i64:
			execute<instructions::cast_zext_i16_to_i64>(context);
			break;
		case instruction::cast_zext_i32_to_i64:
			execute<instructions::cast_zext_i32_to_i64>(context);
			break;
		case instruction::cast_sext_i8_to_i16:
			execute<instructions::cast_sext_i8_to_i16>(context);
			break;
		case instruction::cast_sext_i8_to_i32:
			execute<instructions::cast_sext_i8_to_i32>(context);
			break;
		case instruction::cast_sext_i8_to_i64:
			execute<instructions::cast_sext_i8_to_i64>(context);
			break;
		case instruction::cast_sext_i16_to_i32:
			execute<instructions::cast_sext_i16_to_i32>(context);
			break;
		case instruction::cast_sext_i16_to_i64:
			execute<instructions::cast_sext_i16_to_i64>(context);
			break;
		case instruction::cast_sext_i32_to_i64:
			execute<instructions::cast_sext_i32_to_i64>(context);
			break;
		case instruction::cast_trunc_i64_to_i8:
			execute<instructions::cast_trunc_i64_to_i8>(context);
			break;
		case instruction::cast_trunc_i64_to_i16:
			execute<instructions::cast_trunc_i64_to_i16>(context);
			break;
		case instruction::cast_trunc_i64_to_i32:
			execute<instructions::cast_trunc_i64_to_i32>(context);
			break;
		case instruction::cast_trunc_i32_to_i8:
			execute<instructions::cast_trunc_i32_to_i8>(context);
			break;
		case instruction::cast_trunc_i32_to_i16:
			execute<instructions::cast_trunc_i32_to_i16>(context);
			break;
		case instruction::cast_trunc_i16_to_i8:
			execute<instructions::cast_trunc_i16_to_i8>(context);
			break;
		case instruction::cast_f32_to_f64:
			execute<instructions::cast_f32_to_f64>(context);
			break;
		case instruction::cast_f64_to_f32:
			execute<instructions::cast_f64_to_f32>(context);
			break;
		case instruction::cast_f32_to_i8:
			execute<instructions::cast_f32_to_i8>(context);
			break;
		case instruction::cast_f32_to_i16:
			execute<instructions::cast_f32_to_i16>(context);
			break;
		case instruction::cast_f32_to_i32:
			execute<instructions::cast_f32_to_i32>(context);
			break;
		case instruction::cast_f32_to_i64:
			execute<instructions::cast_f32_to_i64>(context);
			break;
		case instruction::cast_f32_to_u8:
			execute<instructions::cast_f32_to_u8>(context);
			break;
		case instruction::cast_f32_to_u16:
			execute<instructions::cast_f32_to_u16>(context);
			break;
		case instruction::cast_f32_to_u32:
			execute<instructions::cast_f32_to_u32>(context);
			break;
		case instruction::cast_f32_to_u64:
			execute<instructions::cast_f32_to_u64>(context);
			break;
		case instruction::cast_f64_to_i8:
			execute<instructions::cast_f64_to_i8>(context);
			break;
		case instruction::cast_f64_to_i16:
			execute<instructions::cast_f64_to_i16>(context);
			break;
		case instruction::cast_f64_to_i32:
			execute<instructions::cast_f64_to_i32>(context);
			break;
		case instruction::cast_f64_to_i64:
			execute<instructions::cast_f64_to_i64>(context);
			break;
		case instruction::cast_f64_to_u8:
			execute<instructions::cast_f64_to_u8>(context);
			break;
		case instruction::cast_f64_to_u16:
			execute<instructions::cast_f64_to_u16>(context);
			break;
		case instruction::cast_f64_to_u32:
			execute<instructions::cast_f64_to_u32>(context);
			break;
		case instruction::cast_f64_to_u64:
			execute<instructions::cast_f64_to_u64>(context);
			break;
		case instruction::cast_i8_to_f32:
			execute<instructions::cast_i8_to_f32>(context);
			break;
		case instruction::cast_i16_to_f32:
			execute<instructions::cast_i16_to_f32>(context);
			break;
		case instruction::cast_i32_to_f32:
			execute<instructions::cast_i32_to_f32>(context);
			break;
		case instruction::cast_i64_to_f32:
			execute<instructions::cast_i64_to_f32>(context);
			break;
		case instruction::cast_u8_to_f32:
			execute<instructions::cast_u8_to_f32>(context);
			break;
		case instruction::cast_u16_to_f32:
			execute<instructions::cast_u16_to_f32>(context);
			break;
		case instruction::cast_u32_to_f32:
			execute<instructions::cast_u32_to_f32>(context);
			break;
		case instruction::cast_u64_to_f32:
			execute<instructions::cast_u64_to_f32>(context);
			break;
		case instruction::cast_i8_to_f64:
			execute<instructions::cast_i8_to_f64>(context);
			break;
		case instruction::cast_i16_to_f64:
			execute<instructions::cast_i16_to_f64>(context);
			break;
		case instruction::cast_i32_to_f64:
			execute<instructions::cast_i32_to_f64>(context);
			break;
		case instruction::cast_i64_to_f64:
			execute<instructions::cast_i64_to_f64>(context);
			break;
		case instruction::cast_u8_to_f64:
			execute<instructions::cast_u8_to_f64>(context);
			break;
		case instruction::cast_u16_to_f64:
			execute<instructions::cast_u16_to_f64>(context);
			break;
		case instruction::cast_u32_to_f64:
			execute<instructions::cast_u32_to_f64>(context);
			break;
		case instruction::cast_u64_to_f64:
			execute<instructions::cast_u64_to_f64>(context);
			break;
		case instruction::cmp_eq_i1:
			execute<instructions::cmp_eq_i1>(context);
			break;
		case instruction::cmp_eq_i8:
			execute<instructions::cmp_eq_i8>(context);
			break;
		case instruction::cmp_eq_i16:
			execute<instructions::cmp_eq_i16>(context);
			break;
		case instruction::cmp_eq_i32:
			execute<instructions::cmp_eq_i32>(context);
			break;
		case instruction::cmp_eq_i64:
			execute<instructions::cmp_eq_i64>(context);
			break;
		case instruction::cmp_eq_f32:
			execute<instructions::cmp_eq_f32>(context);
			break;
		case instruction::cmp_eq_f64:
			execute<instructions::cmp_eq_f64>(context);
			break;
		case instruction::cmp_eq_f32_unchecked:
			execute<instructions::cmp_eq_f32_unchecked>(context);
			break;
		case instruction::cmp_eq_f64_unchecked:
			execute<instructions::cmp_eq_f64_unchecked>(context);
			break;
		case instruction::cmp_eq_ptr:
			execute<instructions::cmp_eq_ptr>(context);
			break;
		case instruction::cmp_neq_i1:
			execute<instructions::cmp_neq_i1>(context);
			break;
		case instruction::cmp_neq_i8:
			execute<instructions::cmp_neq_i8>(context);
			break;
		case instruction::cmp_neq_i16:
			execute<instructions::cmp_neq_i16>(context);
			break;
		case instruction::cmp_neq_i32:
			execute<instructions::cmp_neq_i32>(context);
			break;
		case instruction::cmp_neq_i64:
			execute<instructions::cmp_neq_i64>(context);
			break;
		case instruction::cmp_neq_f32:
			execute<instructions::cmp_neq_f32>(context);
			break;
		case instruction::cmp_neq_f64:
			execute<instructions::cmp_neq_f64>(context);
			break;
		case instruction::cmp_neq_f32_unchecked:
			execute<instructions::cmp_neq_f32_unchecked>(context);
			break;
		case instruction::cmp_neq_f64_unchecked:
			execute<instructions::cmp_neq_f64_unchecked>(context);
			break;
		case instruction::cmp_neq_ptr:
			execute<instructions::cmp_neq_ptr>(context);
			break;
		case instruction::add_i8_unchecked:
			execute<instructions::add_i8_unchecked>(context);
			break;
		case instruction::add_i16_unchecked:
			execute<instructions::add_i16_unchecked>(context);
			break;
		case instruction::add_i32_unchecked:
			execute<instructions::add_i32_unchecked>(context);
			break;
		case instruction::add_i64_unchecked:
			execute<instructions::add_i64_unchecked>(context);
			break;
		case instruction::sub_i8_unchecked:
			execute<instructions::sub_i8_unchecked>(context);
			break;
		case instruction::sub_i16_unchecked:
			execute<instructions::sub_i16_unchecked>(context);
			break;
		case instruction::sub_i32_unchecked:
			execute<instructions::sub_i32_unchecked>(context);
			break;
		case instruction::sub_i64_unchecked:
			execute<instructions::sub_i64_unchecked>(context);
			break;
		case instruction::ptr32_diff:
			execute<instructions::ptr32_diff>(context);
			break;
		case instruction::ptr64_diff:
			execute<instructions::ptr64_diff>(context);
			break;
		case instruction::const_gep:
			execute<instructions::const_gep>(context);
			break;
		case instruction::array_gep_i32:
			execute<instructions::array_gep_i32>(context);
			break;
		case instruction::array_gep_i64:
			execute<instructions::array_gep_i64>(context);
			break;
		case instruction::const_memcpy:
			execute<instructions::const_memcpy>(context);
			break;
		case instruction::const_memset_zero:
			execute<instructions::const_memset_zero>(context);
			break;
		case instruction::jump:
			execute<instructions::jump>(context);
			break;
		case instruction::conditional_jump:
			execute<instructions::conditional_jump>(context);
			break;
		case instruction::ret:
			execute<instructions::ret>(context);
			break;
		case instruction::ret_void:
			execute<instructions::ret_void>(context);
			break;
		case instruction::error:
			execute<instructions::error>(context);
			break;
		default:
			bz_unreachable;
	}
}

} // namespace comptime
