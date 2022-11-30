#include "instructions.h"
#include "executor_context.h"
#include <bit>

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

static bool execute(instructions::cmp_lt_i8 const &, uint8_t lhs, uint8_t rhs, executor_context &)
{
	return static_cast<int8_t>(lhs) < static_cast<int8_t>(rhs);
}

static bool execute(instructions::cmp_lt_i16 const &, uint16_t lhs, uint16_t rhs, executor_context &)
{
	return static_cast<int16_t>(lhs) < static_cast<int16_t>(rhs);
}

static bool execute(instructions::cmp_lt_i32 const &, uint32_t lhs, uint32_t rhs, executor_context &)
{
	return static_cast<int32_t>(lhs) < static_cast<int32_t>(rhs);
}

static bool execute(instructions::cmp_lt_i64 const &, uint64_t lhs, uint64_t rhs, executor_context &)
{
	return static_cast<int64_t>(lhs) < static_cast<int64_t>(rhs);
}

static bool execute(instructions::cmp_lt_u8 const &, uint8_t lhs, uint8_t rhs, executor_context &)
{
	return lhs < rhs;
}

static bool execute(instructions::cmp_lt_u16 const &, uint16_t lhs, uint16_t rhs, executor_context &)
{
	return lhs < rhs;
}

static bool execute(instructions::cmp_lt_u32 const &, uint32_t lhs, uint32_t rhs, executor_context &)
{
	return lhs < rhs;
}

static bool execute(instructions::cmp_lt_u64 const &, uint64_t lhs, uint64_t rhs, executor_context &)
{
	return lhs < rhs;
}

static bool execute(instructions::cmp_lt_f32 const &inst, float32_t lhs, float32_t rhs, executor_context &context)
{
	if (std::isnan(lhs) && std::isnan(rhs))
	{
		context.report_warning(
			ctx::warning_kind::nan_compare,
			inst.src_tokens_index,
			bz::format("comparing nans in expression '{} < {}' with type 'float32' evaluates to false", lhs, rhs)
		);
	}
	else if (std::isnan(lhs) || std::isnan(rhs))
	{
		context.report_warning(
			ctx::warning_kind::nan_compare,
			inst.src_tokens_index,
			bz::format("comparing against nan in expression '{} < {}' with type 'float32' evaluates to false", lhs, rhs)
		);
	}
	return lhs < rhs;
}

static bool execute(instructions::cmp_lt_f64 const &inst, float64_t lhs, float64_t rhs, executor_context &context)
{
	if (std::isnan(lhs) && std::isnan(rhs))
	{
		context.report_warning(
			ctx::warning_kind::nan_compare,
			inst.src_tokens_index,
			bz::format("comparing nans in expression '{} < {}' with type 'float64' evaluates to false", lhs, rhs)
		);
	}
	else if (std::isnan(lhs) || std::isnan(rhs))
	{
		context.report_warning(
			ctx::warning_kind::nan_compare,
			inst.src_tokens_index,
			bz::format("comparing against nan in expression '{} < {}' with type 'float64' evaluates to false", lhs, rhs)
		);
	}
	return lhs < rhs;
}

static bool execute(instructions::cmp_lt_f32_unchecked const &, float32_t lhs, float32_t rhs, executor_context &)
{
	return lhs < rhs;
}

static bool execute(instructions::cmp_lt_f64_unchecked const &, float64_t lhs, float64_t rhs, executor_context &)
{
	return lhs < rhs;
}

static bool execute(instructions::cmp_gt_i8 const &, uint8_t lhs, uint8_t rhs, executor_context &)
{
	return static_cast<int8_t>(lhs) > static_cast<int8_t>(rhs);
}

static bool execute(instructions::cmp_gt_i16 const &, uint16_t lhs, uint16_t rhs, executor_context &)
{
	return static_cast<int16_t>(lhs) > static_cast<int16_t>(rhs);
}

static bool execute(instructions::cmp_gt_i32 const &, uint32_t lhs, uint32_t rhs, executor_context &)
{
	return static_cast<int32_t>(lhs) > static_cast<int32_t>(rhs);
}

static bool execute(instructions::cmp_gt_i64 const &, uint64_t lhs, uint64_t rhs, executor_context &)
{
	return static_cast<int64_t>(lhs) > static_cast<int64_t>(rhs);
}

static bool execute(instructions::cmp_gt_u8 const &, uint8_t lhs, uint8_t rhs, executor_context &)
{
	return lhs > rhs;
}

static bool execute(instructions::cmp_gt_u16 const &, uint16_t lhs, uint16_t rhs, executor_context &)
{
	return lhs > rhs;
}

static bool execute(instructions::cmp_gt_u32 const &, uint32_t lhs, uint32_t rhs, executor_context &)
{
	return lhs > rhs;
}

static bool execute(instructions::cmp_gt_u64 const &, uint64_t lhs, uint64_t rhs, executor_context &)
{
	return lhs > rhs;
}

static bool execute(instructions::cmp_gt_f32 const &inst, float32_t lhs, float32_t rhs, executor_context &context)
{
	if (std::isnan(lhs) && std::isnan(rhs))
	{
		context.report_warning(
			ctx::warning_kind::nan_compare,
			inst.src_tokens_index,
			bz::format("comparing nans in expression '{} > {}' with type 'float32' evaluates to false", lhs, rhs)
		);
	}
	else if (std::isnan(lhs) || std::isnan(rhs))
	{
		context.report_warning(
			ctx::warning_kind::nan_compare,
			inst.src_tokens_index,
			bz::format("comparing against nan in expression '{} > {}' with type 'float32' evaluates to false", lhs, rhs)
		);
	}
	return lhs > rhs;
}

static bool execute(instructions::cmp_gt_f64 const &inst, float64_t lhs, float64_t rhs, executor_context &context)
{
	if (std::isnan(lhs) && std::isnan(rhs))
	{
		context.report_warning(
			ctx::warning_kind::nan_compare,
			inst.src_tokens_index,
			bz::format("comparing nans in expression '{} > {}' with type 'float64' evaluates to false", lhs, rhs)
		);
	}
	else if (std::isnan(lhs) || std::isnan(rhs))
	{
		context.report_warning(
			ctx::warning_kind::nan_compare,
			inst.src_tokens_index,
			bz::format("comparing against nan in expression '{} > {}' with type 'float64' evaluates to false", lhs, rhs)
		);
	}
	return lhs > rhs;
}

static bool execute(instructions::cmp_gt_f32_unchecked const &, float32_t lhs, float32_t rhs, executor_context &)
{
	return lhs > rhs;
}

static bool execute(instructions::cmp_gt_f64_unchecked const &, float64_t lhs, float64_t rhs, executor_context &)
{
	return lhs > rhs;
}

static bool execute(instructions::cmp_lte_i8 const &, uint8_t lhs, uint8_t rhs, executor_context &)
{
	return static_cast<int8_t>(lhs) <= static_cast<int8_t>(rhs);
}

static bool execute(instructions::cmp_lte_i16 const &, uint16_t lhs, uint16_t rhs, executor_context &)
{
	return static_cast<int16_t>(lhs) <= static_cast<int16_t>(rhs);
}

static bool execute(instructions::cmp_lte_i32 const &, uint32_t lhs, uint32_t rhs, executor_context &)
{
	return static_cast<int32_t>(lhs) <= static_cast<int32_t>(rhs);
}

static bool execute(instructions::cmp_lte_i64 const &, uint64_t lhs, uint64_t rhs, executor_context &)
{
	return static_cast<int64_t>(lhs) <= static_cast<int64_t>(rhs);
}

static bool execute(instructions::cmp_lte_u8 const &, uint8_t lhs, uint8_t rhs, executor_context &)
{
	return lhs <= rhs;
}

static bool execute(instructions::cmp_lte_u16 const &, uint16_t lhs, uint16_t rhs, executor_context &)
{
	return lhs <= rhs;
}

static bool execute(instructions::cmp_lte_u32 const &, uint32_t lhs, uint32_t rhs, executor_context &)
{
	return lhs <= rhs;
}

static bool execute(instructions::cmp_lte_u64 const &, uint64_t lhs, uint64_t rhs, executor_context &)
{
	return lhs <= rhs;
}

static bool execute(instructions::cmp_lte_f32 const &inst, float32_t lhs, float32_t rhs, executor_context &context)
{
	if (std::isnan(lhs) && std::isnan(rhs))
	{
		context.report_warning(
			ctx::warning_kind::nan_compare,
			inst.src_tokens_index,
			bz::format("comparing nans in expression '{} <= {}' with type 'float32' evaluates to false", lhs, rhs)
		);
	}
	else if (std::isnan(lhs) || std::isnan(rhs))
	{
		context.report_warning(
			ctx::warning_kind::nan_compare,
			inst.src_tokens_index,
			bz::format("comparing against nan in expression '{} <= {}' with type 'float32' evaluates to false", lhs, rhs)
		);
	}
	return lhs <= rhs;
}

static bool execute(instructions::cmp_lte_f64 const &inst, float64_t lhs, float64_t rhs, executor_context &context)
{
	if (std::isnan(lhs) && std::isnan(rhs))
	{
		context.report_warning(
			ctx::warning_kind::nan_compare,
			inst.src_tokens_index,
			bz::format("comparing nans in expression '{} <= {}' with type 'float64' evaluates to false", lhs, rhs)
		);
	}
	else if (std::isnan(lhs) || std::isnan(rhs))
	{
		context.report_warning(
			ctx::warning_kind::nan_compare,
			inst.src_tokens_index,
			bz::format("comparing against nan in expression '{} <= {}' with type 'float64' evaluates to false", lhs, rhs)
		);
	}
	return lhs <= rhs;
}

static bool execute(instructions::cmp_lte_f32_unchecked const &, float32_t lhs, float32_t rhs, executor_context &)
{
	return lhs <= rhs;
}

static bool execute(instructions::cmp_lte_f64_unchecked const &, float64_t lhs, float64_t rhs, executor_context &)
{
	return lhs <= rhs;
}

static bool execute(instructions::cmp_gte_i8 const &, uint8_t lhs, uint8_t rhs, executor_context &)
{
	return static_cast<int8_t>(lhs) >= static_cast<int8_t>(rhs);
}

static bool execute(instructions::cmp_gte_i16 const &, uint16_t lhs, uint16_t rhs, executor_context &)
{
	return static_cast<int16_t>(lhs) >= static_cast<int16_t>(rhs);
}

static bool execute(instructions::cmp_gte_i32 const &, uint32_t lhs, uint32_t rhs, executor_context &)
{
	return static_cast<int32_t>(lhs) >= static_cast<int32_t>(rhs);
}

static bool execute(instructions::cmp_gte_i64 const &, uint64_t lhs, uint64_t rhs, executor_context &)
{
	return static_cast<int64_t>(lhs) >= static_cast<int64_t>(rhs);
}

static bool execute(instructions::cmp_gte_u8 const &, uint8_t lhs, uint8_t rhs, executor_context &)
{
	return lhs >= rhs;
}

static bool execute(instructions::cmp_gte_u16 const &, uint16_t lhs, uint16_t rhs, executor_context &)
{
	return lhs >= rhs;
}

static bool execute(instructions::cmp_gte_u32 const &, uint32_t lhs, uint32_t rhs, executor_context &)
{
	return lhs >= rhs;
}

static bool execute(instructions::cmp_gte_u64 const &, uint64_t lhs, uint64_t rhs, executor_context &)
{
	return lhs >= rhs;
}

static bool execute(instructions::cmp_gte_f32 const &inst, float32_t lhs, float32_t rhs, executor_context &context)
{
	if (std::isnan(lhs) && std::isnan(rhs))
	{
		context.report_warning(
			ctx::warning_kind::nan_compare,
			inst.src_tokens_index,
			bz::format("comparing nans in expression '{} >= {}' with type 'float32' evaluates to false", lhs, rhs)
		);
	}
	else if (std::isnan(lhs) || std::isnan(rhs))
	{
		context.report_warning(
			ctx::warning_kind::nan_compare,
			inst.src_tokens_index,
			bz::format("comparing against nan in expression '{} >= {}' with type 'float32' evaluates to false", lhs, rhs)
		);
	}
	return lhs >= rhs;
}

static bool execute(instructions::cmp_gte_f64 const &inst, float64_t lhs, float64_t rhs, executor_context &context)
{
	if (std::isnan(lhs) && std::isnan(rhs))
	{
		context.report_warning(
			ctx::warning_kind::nan_compare,
			inst.src_tokens_index,
			bz::format("comparing nans in expression '{} >= {}' with type 'float64' evaluates to false", lhs, rhs)
		);
	}
	else if (std::isnan(lhs) || std::isnan(rhs))
	{
		context.report_warning(
			ctx::warning_kind::nan_compare,
			inst.src_tokens_index,
			bz::format("comparing against nan in expression '{} >= {}' with type 'float64' evaluates to false", lhs, rhs)
		);
	}
	return lhs >= rhs;
}

static bool execute(instructions::cmp_gte_f32_unchecked const &, float32_t lhs, float32_t rhs, executor_context &)
{
	return lhs >= rhs;
}

static bool execute(instructions::cmp_gte_f64_unchecked const &, float64_t lhs, float64_t rhs, executor_context &)
{
	return lhs >= rhs;
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

static bool execute(instructions::and_i1 const &, bool lhs, bool rhs, executor_context &)
{
	return lhs && rhs;
}

static uint8_t execute(instructions::and_i8 const &, uint8_t lhs, uint8_t rhs, executor_context &)
{
	return lhs & rhs;
}

static uint16_t execute(instructions::and_i16 const &, uint16_t lhs, uint16_t rhs, executor_context &)
{
	return lhs & rhs;
}

static uint32_t execute(instructions::and_i32 const &, uint32_t lhs, uint32_t rhs, executor_context &)
{
	return lhs & rhs;
}

static uint64_t execute(instructions::and_i64 const &, uint64_t lhs, uint64_t rhs, executor_context &)
{
	return lhs & rhs;
}

static bool execute(instructions::xor_i1 const &, bool lhs, bool rhs, executor_context &)
{
	return lhs != rhs;
}

static uint8_t execute(instructions::xor_i8 const &, uint8_t lhs, uint8_t rhs, executor_context &)
{
	return lhs ^ rhs;
}

static uint16_t execute(instructions::xor_i16 const &, uint16_t lhs, uint16_t rhs, executor_context &)
{
	return lhs ^ rhs;
}

static uint32_t execute(instructions::xor_i32 const &, uint32_t lhs, uint32_t rhs, executor_context &)
{
	return lhs ^ rhs;
}

static uint64_t execute(instructions::xor_i64 const &, uint64_t lhs, uint64_t rhs, executor_context &)
{
	return lhs ^ rhs;
}

static bool execute(instructions::or_i1 const &, bool lhs, bool rhs, executor_context &)
{
	return lhs || rhs;
}

static uint8_t execute(instructions::or_i8 const &, uint8_t lhs, uint8_t rhs, executor_context &)
{
	return lhs | rhs;
}

static uint16_t execute(instructions::or_i16 const &, uint16_t lhs, uint16_t rhs, executor_context &)
{
	return lhs | rhs;
}

static uint32_t execute(instructions::or_i32 const &, uint32_t lhs, uint32_t rhs, executor_context &)
{
	return lhs | rhs;
}

static uint64_t execute(instructions::or_i64 const &, uint64_t lhs, uint64_t rhs, executor_context &)
{
	return lhs | rhs;
}

static uint8_t execute(instructions::abs_i8 const &inst, uint8_t uvalue, executor_context &context)
{
	auto const value = static_cast<int8_t>(uvalue);
	if (value == std::numeric_limits<int8_t>::min())
	{
		context.report_warning(
			ctx::warning_kind::math_domain_error,
			inst.src_tokens_index,
			bz::format("calling 'abs' with {} of type 'int8' results in {}", value, value)
		);
		return static_cast<uint8_t>(value);
	}
	return static_cast<uint8_t>(value < 0 ? -value : value);
}

static uint16_t execute(instructions::abs_i16 const &inst, uint16_t uvalue, executor_context &context)
{
	auto const value = static_cast<int16_t>(uvalue);
	if (value == std::numeric_limits<int16_t>::min())
	{
		context.report_warning(
			ctx::warning_kind::math_domain_error,
			inst.src_tokens_index,
			bz::format("calling 'abs' with {} of type 'int16' results in {}", value, value)
		);
		return static_cast<uint16_t>(value);
	}
	return static_cast<uint8_t>(value < 0 ? -value : value);
}

static uint32_t execute(instructions::abs_i32 const &inst, uint32_t uvalue, executor_context &context)
{
	auto const value = static_cast<int32_t>(uvalue);
	if (value == std::numeric_limits<int32_t>::min())
	{
		context.report_warning(
			ctx::warning_kind::math_domain_error,
			inst.src_tokens_index,
			bz::format("calling 'abs' with {} of type 'int32' results in {}", value, value)
		);
		return static_cast<uint32_t>(value);
	}
	return static_cast<uint8_t>(value < 0 ? -value : value);
}

static uint64_t execute(instructions::abs_i64 const &inst, uint64_t uvalue, executor_context &context)
{
	auto const value = static_cast<int64_t>(uvalue);
	if (value == std::numeric_limits<int64_t>::min())
	{
		context.report_warning(
			ctx::warning_kind::math_domain_error,
			inst.src_tokens_index,
			bz::format("calling 'abs' with {} of type 'int64' results in {}", value, value)
		);
		return static_cast<uint64_t>(value);
	}
	return static_cast<uint8_t>(value < 0 ? -value : value);
}

static float32_t execute(instructions::abs_f32 const &inst, float32_t value, executor_context &context)
{
	if (std::isnan(value))
	{
		context.report_warning(
			ctx::warning_kind::math_domain_error,
			inst.src_tokens_index,
			"calling 'abs' with nan of type 'float32' results in nan"
		);
	}
	return std::fabs(value);
}

static float64_t execute(instructions::abs_f64 const &inst, float64_t value, executor_context &context)
{
	if (std::isnan(value))
	{
		context.report_warning(
			ctx::warning_kind::math_domain_error,
			inst.src_tokens_index,
			"calling 'abs' with nan of type 'float64' results in nan"
		);
	}
	return std::fabs(value);
}

static uint8_t execute(instructions::abs_i8_unchecked const &, uint8_t uvalue, executor_context &)
{
	auto const value = static_cast<int8_t>(uvalue);
	return static_cast<uint8_t>(
		value == std::numeric_limits<int8_t>::min() ? value :
		value < 0 ? -value :
		value
	);
}

static uint16_t execute(instructions::abs_i16_unchecked const &, uint16_t uvalue, executor_context &)
{
	auto const value = static_cast<int16_t>(uvalue);
	return static_cast<uint16_t>(
		value == std::numeric_limits<int16_t>::min() ? value :
		value < 0 ? -value :
		value
	);
}

static uint32_t execute(instructions::abs_i32_unchecked const &, uint32_t uvalue, executor_context &)
{
	auto const value = static_cast<int32_t>(uvalue);
	return static_cast<uint32_t>(
		value == std::numeric_limits<int32_t>::min() ? value :
		value < 0 ? -value :
		value
	);
}

static uint64_t execute(instructions::abs_i64_unchecked const &, uint64_t uvalue, executor_context &)
{
	auto const value = static_cast<int64_t>(uvalue);
	return static_cast<uint64_t>(
		value == std::numeric_limits<int64_t>::min() ? value :
		value < 0 ? -value :
		value
	);
}

static float32_t execute(instructions::abs_f32_unchecked const &, float32_t value, executor_context &)
{
	return std::fabs(value);
}

static float64_t execute(instructions::abs_f64_unchecked const &, float64_t value, executor_context &)
{
	return std::fabs(value);
}

static uint8_t execute(instructions::min_i8 const &, uint8_t a, uint8_t b, executor_context &)
{
	return static_cast<int8_t>(a) < static_cast<int8_t>(b) ? a : b;
}

static uint16_t execute(instructions::min_i16 const &, uint16_t a, uint16_t b, executor_context &)
{
	return static_cast<int16_t>(a) < static_cast<int16_t>(b) ? a : b;
}

static uint32_t execute(instructions::min_i32 const &, uint32_t a, uint32_t b, executor_context &)
{
	return static_cast<int32_t>(a) < static_cast<int32_t>(b) ? a : b;
}

static uint64_t execute(instructions::min_i64 const &, uint64_t a, uint64_t b, executor_context &)
{
	return static_cast<int64_t>(a) < static_cast<int64_t>(b) ? a : b;
}

static uint8_t execute(instructions::min_u8 const &, uint8_t a, uint8_t b, executor_context &)
{
	return a < b ? a : b;
}

static uint16_t execute(instructions::min_u16 const &, uint16_t a, uint16_t b, executor_context &)
{
	return a < b ? a : b;
}

static uint32_t execute(instructions::min_u32 const &, uint32_t a, uint32_t b, executor_context &)
{
	return a < b ? a : b;
}

static uint64_t execute(instructions::min_u64 const &, uint64_t a, uint64_t b, executor_context &)
{
	return a < b ? a : b;
}

static float32_t execute(instructions::min_f32 const &inst, float32_t x, float32_t y, executor_context &context)
{
	if (std::isnan(x) || std::isnan(y))
	{
		context.report_warning(
			ctx::warning_kind::math_domain_error,
			inst.src_tokens_index,
			bz::format("calling 'min' with {} and {} of type 'float32'")
		);
	}
	return std::fmin(x, y);
}

static float64_t execute(instructions::min_f64 const &inst, float64_t x, float64_t y, executor_context &context)
{
	if (std::isnan(x) || std::isnan(y))
	{
		context.report_warning(
			ctx::warning_kind::math_domain_error,
			inst.src_tokens_index,
			bz::format("calling 'min' with {} and {} of type 'float64'")
		);
	}
	return std::fmin(x, y);
}

static float32_t execute(instructions::min_f32_unchecked const &, float32_t x, float32_t y, executor_context &)
{
	return std::fmin(x, y);
}

static float64_t execute(instructions::min_f64_unchecked const &, float64_t x, float64_t y, executor_context &)
{
	return std::fmin(x, y);
}

static uint8_t execute(instructions::max_i8 const &, uint8_t a, uint8_t b, executor_context &)
{
	return static_cast<int8_t>(a) < static_cast<int8_t>(b) ? a : b;
}

static uint16_t execute(instructions::max_i16 const &, uint16_t a, uint16_t b, executor_context &)
{
	return static_cast<int16_t>(a) < static_cast<int16_t>(b) ? a : b;
}

static uint32_t execute(instructions::max_i32 const &, uint32_t a, uint32_t b, executor_context &)
{
	return static_cast<int32_t>(a) < static_cast<int32_t>(b) ? a : b;
}

static uint64_t execute(instructions::max_i64 const &, uint64_t a, uint64_t b, executor_context &)
{
	return static_cast<int64_t>(a) < static_cast<int64_t>(b) ? a : b;
}

static uint8_t execute(instructions::max_u8 const &, uint8_t a, uint8_t b, executor_context &)
{
	return a < b ? a : b;
}

static uint16_t execute(instructions::max_u16 const &, uint16_t a, uint16_t b, executor_context &)
{
	return a < b ? a : b;
}

static uint32_t execute(instructions::max_u32 const &, uint32_t a, uint32_t b, executor_context &)
{
	return a < b ? a : b;
}

static uint64_t execute(instructions::max_u64 const &, uint64_t a, uint64_t b, executor_context &)
{
	return a < b ? a : b;
}

static float32_t execute(instructions::max_f32 const &inst, float32_t x, float32_t y, executor_context &context)
{
	if (std::isnan(x) || std::isnan(y))
	{
		context.report_warning(
			ctx::warning_kind::math_domain_error,
			inst.src_tokens_index,
			bz::format("calling 'max' with {} and {} of type 'float32'")
		);
	}
	return std::fmax(x, y);
}

static float64_t execute(instructions::max_f64 const &inst, float64_t x, float64_t y, executor_context &context)
{
	if (std::isnan(x) || std::isnan(y))
	{
		context.report_warning(
			ctx::warning_kind::math_domain_error,
			inst.src_tokens_index,
			bz::format("calling 'max' with {} and {} of type 'float64'")
		);
	}
	return std::fmax(x, y);
}

static float32_t execute(instructions::max_f32_unchecked const &, float32_t x, float32_t y, executor_context &)
{
	return std::fmax(x, y);
}

static float64_t execute(instructions::max_f64_unchecked const &, float64_t x, float64_t y, executor_context &)
{
	return std::fmax(x, y);
}

static void report_regular_math_error(
	bz::u8string_view func_name,
	uint32_t src_tokens_index,
	float32_t x,
	float32_t result,
	executor_context &context
)
{
	context.report_warning(
		ctx::warning_kind::math_domain_error,
		src_tokens_index,
		bz::format("calling '{}' with {} results in {}", func_name, x, result)
	);
}

static void report_regular_math_error(
	bz::u8string_view func_name,
	uint32_t src_tokens_index,
	float64_t x,
	float64_t result,
	executor_context &context
)
{
	context.report_warning(
		ctx::warning_kind::math_domain_error,
		src_tokens_index,
		bz::format("calling '{}' with {} results in {}", func_name, x, result)
	);
}

static void report_negative_math_error(
	bz::u8string_view func_name,
	uint32_t src_tokens_index,
	float32_t x,
	float32_t result,
	executor_context &context
)
{
	context.report_warning(
		ctx::warning_kind::math_domain_error,
		src_tokens_index,
		bz::format("calling '{}' with a negative value {} results in {}", func_name, x, result)
	);
}

static void report_negative_math_error(
	bz::u8string_view func_name,
	uint32_t src_tokens_index,
	float64_t x,
	float64_t result,
	executor_context &context
)
{
	context.report_warning(
		ctx::warning_kind::math_domain_error,
		src_tokens_index,
		bz::format("calling '{}' with a negative value {} results in {}", func_name, x, result)
	);
}

static void report_negative_integer_math_error(
	bz::u8string_view func_name,
	uint32_t src_tokens_index,
	float32_t x,
	float32_t result,
	executor_context &context
)
{
	context.report_warning(
		ctx::warning_kind::math_domain_error,
		src_tokens_index,
		bz::format("calling '{}' with a negative integer {} results in {}", func_name, x, result)
	);
}

static void report_negative_integer_math_error(
	bz::u8string_view func_name,
	uint32_t src_tokens_index,
	float64_t x,
	float64_t result,
	executor_context &context
)
{
	context.report_warning(
		ctx::warning_kind::math_domain_error,
		src_tokens_index,
		bz::format("calling '{}' with a negative integer {} results in {}", func_name, x, result)
	);
}

static void check_for_nan(
	bz::u8string_view func_name,
	uint32_t src_tokens_index,
	float32_t x,
	executor_context &context
)
{
	if (std::isnan(x))
	{
		context.report_warning(
			ctx::warning_kind::math_domain_error,
			src_tokens_index,
			bz::format("calling '{}' with nan results in nan", func_name)
		);
	}
}

static void check_for_nan(
	bz::u8string_view func_name,
	uint32_t src_tokens_index,
	float64_t x,
	executor_context &context
)
{
	if (std::isnan(x))
	{
		context.report_warning(
			ctx::warning_kind::math_domain_error,
			src_tokens_index,
			bz::format("calling '{}' with nan results in nan", func_name)
		);
	}
}

static void check_for_nan_or_inf(
	bz::u8string_view func_name,
	uint32_t src_tokens_index,
	float32_t x,
	float32_t result,
	executor_context &context
)
{
	if (std::isnan(x) || std::isinf(x))
	{
		report_regular_math_error(func_name, src_tokens_index, x, result, context);
	}
}

static void check_for_nan_or_inf(
	bz::u8string_view func_name,
	uint32_t src_tokens_index,
	float64_t x,
	float64_t result,
	executor_context &context
)
{
	if (std::isnan(x) || std::isinf(x))
	{
		report_regular_math_error(func_name, src_tokens_index, x, result, context);
	}
}

static void check_for_negative(
	bz::u8string_view func_name,
	uint32_t src_tokens_index,
	float32_t x,
	float32_t result,
	executor_context &context
)
{
	if (x < 0.0f)
	{
		report_negative_math_error(func_name, src_tokens_index, x, result, context);
	}
}

static void check_for_negative(
	bz::u8string_view func_name,
	uint32_t src_tokens_index,
	float64_t x,
	float64_t result,
	executor_context &context
)
{
	if (x < 0.0)
	{
		report_negative_math_error(func_name, src_tokens_index, x, result, context);
	}
}

static bool isint(float32_t x)
{
	auto const bits = bit_cast<uint32_t>(x);
	constexpr uint32_t exponent_mask = 0x7f80'0000;
	constexpr uint32_t mantissa_mask = 0x007f'ffff;
	constexpr uint32_t exponent_bias = 127;
	constexpr uint32_t exponent_inf = exponent_mask >> std::countr_zero(exponent_mask);
	constexpr uint32_t mantissa_size = std::popcount(mantissa_mask);

	// special case for +-0.0
	if ((bits & (mantissa_mask | exponent_mask)) == 0)
	{
		return true;
	}

	auto const exponent = (bits & exponent_mask) >> std::countr_zero(exponent_mask);
	auto const mantissa_non_zero = mantissa_size - std::countr_zero((bits & mantissa_mask) | (uint32_t(1) << mantissa_size));
	return exponent != exponent_inf
		&& exponent >= exponent_bias
		&& exponent - exponent_bias >= mantissa_non_zero;
}

static bool isint(float64_t x)
{
	auto const bits = bit_cast<uint64_t>(x);
	constexpr uint64_t exponent_mask = 0x7ff0'0000'0000'0000;
	constexpr uint64_t mantissa_mask = 0x000f'ffff'ffff'ffff;
	constexpr uint64_t exponent_bias = 1023;
	constexpr uint64_t exponent_inf = exponent_mask >> std::countr_zero(exponent_mask);
	constexpr uint64_t mantissa_size = std::popcount(mantissa_mask);

	// special case for +-0.0
	if ((bits & (mantissa_mask | exponent_mask)) == 0)
	{
		return true;
	}

	auto const exponent = (bits & exponent_mask) >> std::countr_zero(exponent_mask);
	auto const mantissa_non_zero = mantissa_size - std::countr_zero((bits & mantissa_mask) | (uint64_t(1) << mantissa_size));
	return exponent != exponent_inf
		&& exponent >= exponent_bias
		&& exponent - exponent_bias >= mantissa_non_zero;
}

static float32_t execute(instructions::exp_f32 const &inst, float32_t x, executor_context &context)
{
	check_for_nan("exp", inst.src_tokens_index, x, context);
	return std::exp(x);
}

static float64_t execute(instructions::exp_f64 const &inst, float64_t x, executor_context &context)
{
	check_for_nan("exp", inst.src_tokens_index, x, context);
	return std::exp(x);
}

static float32_t execute(instructions::exp_f32_unchecked const &, float32_t x, executor_context &)
{
	return std::exp(x);
}

static float64_t execute(instructions::exp_f64_unchecked const &, float64_t x, executor_context &)
{
	return std::exp(x);
}

static float32_t execute(instructions::exp2_f32 const &inst, float32_t x, executor_context &context)
{
	check_for_nan("exp2", inst.src_tokens_index, x, context);
	return std::exp2(x);
}

static float64_t execute(instructions::exp2_f64 const &inst, float64_t x, executor_context &context)
{
	check_for_nan("exp2", inst.src_tokens_index, x, context);
	return std::exp2(x);
}

static float32_t execute(instructions::exp2_f32_unchecked const &, float32_t x, executor_context &)
{
	return std::exp2(x);
}

static float64_t execute(instructions::exp2_f64_unchecked const &, float64_t x, executor_context &)
{
	return std::exp2(x);
}

static float32_t execute(instructions::expm1_f32 const &inst, float32_t x, executor_context &context)
{
	check_for_nan("expm1", inst.src_tokens_index, x, context);
	return std::expm1(x);
}

static float64_t execute(instructions::expm1_f64 const &inst, float64_t x, executor_context &context)
{
	check_for_nan("expm1", inst.src_tokens_index, x, context);
	return std::expm1(x);
}

static float32_t execute(instructions::expm1_f32_unchecked const &, float32_t x, executor_context &)
{
	return std::expm1(x);
}

static float64_t execute(instructions::expm1_f64_unchecked const &, float64_t x, executor_context &)
{
	return std::expm1(x);
}

static float32_t execute(instructions::log_f32 const &inst, float32_t x, executor_context &context)
{
	auto const result = std::log(x);
	if (std::isnan(x) || x == 0.0f)
	{
		context.report_warning(
			ctx::warning_kind::math_domain_error,
			inst.src_tokens_index,
			bz::format("calling 'log' with {} results in {}", x, result)
		);
	}
	check_for_negative("log", inst.src_tokens_index, x, result, context);
	return result;
}

static float64_t execute(instructions::log_f64 const &inst, float64_t x, executor_context &context)
{
	auto const result = std::log(x);
	if (std::isnan(x) || x == 0.0)
	{
		context.report_warning(
			ctx::warning_kind::math_domain_error,
			inst.src_tokens_index,
			bz::format("calling 'log' with {} results in {}", x, result)
		);
	}
	check_for_negative("log", inst.src_tokens_index, x, result, context);
	return result;
}

static float32_t execute(instructions::log_f32_unchecked const &, float32_t x, executor_context &)
{
	return std::log(x);
}

static float64_t execute(instructions::log_f64_unchecked const &, float64_t x, executor_context &)
{
	return std::log(x);
}

static float32_t execute(instructions::log10_f32 const &inst, float32_t x, executor_context &context)
{
	auto const result = std::log10(x);
	if (std::isnan(x) || x == 0.0f)
	{
		context.report_warning(
			ctx::warning_kind::math_domain_error,
			inst.src_tokens_index,
			bz::format("calling 'log10' with {} results in {}", x, result)
		);
	}
	check_for_negative("log10", inst.src_tokens_index, x, result, context);
	return result;
}

static float64_t execute(instructions::log10_f64 const &inst, float64_t x, executor_context &context)
{
	auto const result = std::log10(x);
	if (std::isnan(x) || x == 0.0)
	{
		context.report_warning(
			ctx::warning_kind::math_domain_error,
			inst.src_tokens_index,
			bz::format("calling 'log10' with {} results in {}", x, result)
		);
	}
	check_for_negative("log10", inst.src_tokens_index, x, result, context);
	return result;
}

static float32_t execute(instructions::log10_f32_unchecked const &, float32_t x, executor_context &)
{
	return std::log10(x);
}

static float64_t execute(instructions::log10_f64_unchecked const &, float64_t x, executor_context &)
{
	return std::log10(x);
}

static float32_t execute(instructions::log2_f32 const &inst, float32_t x, executor_context &context)
{
	auto const result = std::log2(x);
	if (std::isnan(x) || x == 0.0f)
	{
		context.report_warning(
			ctx::warning_kind::math_domain_error,
			inst.src_tokens_index,
			bz::format("calling 'log2' with {} results in {}", x, result)
		);
	}
	check_for_negative("log2", inst.src_tokens_index, x, result, context);
	return result;
}

static float64_t execute(instructions::log2_f64 const &inst, float64_t x, executor_context &context)
{
	auto const result = std::log2(x);
	if (std::isnan(x) || x == 0.0)
	{
		context.report_warning(
			ctx::warning_kind::math_domain_error,
			inst.src_tokens_index,
			bz::format("calling 'log2' with {} results in {}", x, result)
		);
	}
	check_for_negative("log2", inst.src_tokens_index, x, result, context);
	return result;
}

static float32_t execute(instructions::log2_f32_unchecked const &, float32_t x, executor_context &)
{
	return std::log2(x);
}

static float64_t execute(instructions::log2_f64_unchecked const &, float64_t x, executor_context &)
{
	return std::log2(x);
}

static float32_t execute(instructions::log1p_f32 const &inst, float32_t x, executor_context &context)
{
	auto const result = std::log1p(x);
	if (std::isnan(x) || x <= -1.0f)
	{
		context.report_warning(
			ctx::warning_kind::math_domain_error,
			inst.src_tokens_index,
			bz::format("calling 'log1p' with {} results in {}", x, result)
		);
	}
	return result;
}

static float64_t execute(instructions::log1p_f64 const &inst, float64_t x, executor_context &context)
{
	auto const result = std::log1p(x);
	if (std::isnan(x) || x <= -1.0)
	{
		context.report_warning(
			ctx::warning_kind::math_domain_error,
			inst.src_tokens_index,
			bz::format("calling 'log1p' with {} results in {}", x, result)
		);
	}
	return result;
}

static float32_t execute(instructions::log1p_f32_unchecked const &, float32_t x, executor_context &)
{
	return std::log1p(x);
}

static float64_t execute(instructions::log1p_f64_unchecked const &, float64_t x, executor_context &)
{
	return std::log1p(x);
}

static float32_t execute(instructions::sqrt_f32 const &inst, float32_t x, executor_context &context)
{
	auto const result = std::sqrt(x);
	check_for_nan("sqrt", inst.src_tokens_index, x, context);
	check_for_negative("sqrt", inst.src_tokens_index, x, result, context);
	return result;
}

static float64_t execute(instructions::sqrt_f64 const &inst, float64_t x, executor_context &context)
{
	auto const result = std::sqrt(x);
	check_for_nan("sqrt", inst.src_tokens_index, x, context);
	check_for_negative("sqrt", inst.src_tokens_index, x, result, context);
	return result;
}

static float32_t execute(instructions::sqrt_f32_unchecked const &, float32_t x, executor_context &)
{
	return std::sqrt(x);
}

static float64_t execute(instructions::sqrt_f64_unchecked const &, float64_t x, executor_context &)
{
	return std::sqrt(x);
}

static float32_t execute(instructions::pow_f32 const &inst, float32_t x, float32_t y, executor_context &context)
{
	auto const result = std::pow(x, y);
	if (x == 0.0f && y < 0.0f)
	{
		context.report_warning(
			ctx::warning_kind::math_domain_error,
			inst.src_tokens_index,
			bz::format("calling 'pow' with base {} and exponent {} results in {}", x, y, result)
		);
	}
	else if (std::isfinite(x) && x < 0.0f && std::isfinite(y) && !isint(y))
	{
		context.report_warning(
			ctx::warning_kind::math_domain_error,
			inst.src_tokens_index,
			bz::format("calling 'pow' with a negative base {} and a non-integer exponent {} results in {}", x, y, result)
		);
	}
	else if (x != 0.0f && y != 0.0f && (std::isnan(x) || std::isnan(y)))
	{
		context.report_warning(
			ctx::warning_kind::math_domain_error,
			inst.src_tokens_index,
			bz::format("calling 'pow' with base {} and exponent {} results in {}", x, y, result)
		);
	}
	return result;
}

static float64_t execute(instructions::pow_f64 const &inst, float64_t x, float64_t y, executor_context &context)
{
	auto const result = std::pow(x, y);
	if (x == 0.0 && y < 0.0)
	{
		context.report_warning(
			ctx::warning_kind::math_domain_error,
			inst.src_tokens_index,
			bz::format("calling 'pow' with base {} and exponent {} results in {}", x, y, result)
		);
	}
	else if (std::isfinite(x) && x < 0.0 && std::isfinite(y) && !isint(y))
	{
		context.report_warning(
			ctx::warning_kind::math_domain_error,
			inst.src_tokens_index,
			bz::format("calling 'pow' with a negative base {} and a non-integer exponent {} results in {}", x, y, result)
		);
	}
	else if (x != 0.0 && y != 0.0 && (std::isnan(x) || std::isnan(y)))
	{
		context.report_warning(
			ctx::warning_kind::math_domain_error,
			inst.src_tokens_index,
			bz::format("calling 'pow' with base {} and exponent {} results in {}", x, y, result)
		);
	}
	return result;
}

static float32_t execute(instructions::pow_f32_unchecked const &, float32_t x, float32_t y, executor_context &)
{
	return std::pow(x, y);
}

static float64_t execute(instructions::pow_f64_unchecked const &, float64_t x, float64_t y, executor_context &)
{
	return std::pow(x, y);
}

static float32_t execute(instructions::cbrt_f32 const &inst, float32_t x, executor_context &context)
{
	check_for_nan("cbrt", inst.src_tokens_index, x, context);
	return std::cbrt(x);
}

static float64_t execute(instructions::cbrt_f64 const &inst, float64_t x, executor_context &context)
{
	check_for_nan("cbrt", inst.src_tokens_index, x, context);
	return std::cbrt(x);
}

static float32_t execute(instructions::cbrt_f32_unchecked const &, float32_t x, executor_context &)
{
	return std::cbrt(x);
}

static float64_t execute(instructions::cbrt_f64_unchecked const &, float64_t x, executor_context &)
{
	return std::cbrt(x);
}

static float32_t execute(instructions::hypot_f32 const &inst, float32_t x, float32_t y, executor_context &context)
{
	auto const result = std::hypot(x, y);
	if (!std::isinf(x) && !std::isinf(y) && (std::isnan(x) || std::isnan(y)))
	{
		context.report_warning(
			ctx::warning_kind::math_domain_error,
			inst.src_tokens_index,
			bz::format("calling 'hypot' with {} and {} results in {}", x, y, result)
		);
	}
	return result;
}

static float64_t execute(instructions::hypot_f64 const &inst, float64_t x, float64_t y, executor_context &context)
{
	auto const result = std::hypot(x, y);
	if (!std::isinf(x) && !std::isinf(y) && (std::isnan(x) || std::isnan(y)))
	{
		context.report_warning(
			ctx::warning_kind::math_domain_error,
			inst.src_tokens_index,
			bz::format("calling 'hypot' with {} and {} results in {}", x, y, result)
		);
	}
	return result;
}

static float32_t execute(instructions::hypot_f32_unchecked const &, float32_t x, float32_t y, executor_context &)
{
	return std::hypot(x, y);
}

static float64_t execute(instructions::hypot_f64_unchecked const &, float64_t x, float64_t y, executor_context &)
{
	return std::hypot(x, y);
}

static float32_t execute(instructions::sin_f32 const &inst, float32_t x, executor_context &context)
{
	auto const result = std::sin(x);
	check_for_nan_or_inf("sin", inst.src_tokens_index, x, result, context);
	return result;
}

static float64_t execute(instructions::sin_f64 const &inst, float64_t x, executor_context &context)
{
	auto const result = std::sin(x);
	check_for_nan_or_inf("sin", inst.src_tokens_index, x, result, context);
	return result;
}

static float32_t execute(instructions::sin_f32_unchecked const &, float32_t x, executor_context &)
{
	return std::sin(x);
}

static float64_t execute(instructions::sin_f64_unchecked const &, float64_t x, executor_context &)
{
	return std::sin(x);
}

static float32_t execute(instructions::cos_f32 const &inst, float32_t x, executor_context &context)
{
	auto const result = std::cos(x);
	check_for_nan_or_inf("cos", inst.src_tokens_index, x, result, context);
	return result;
}

static float64_t execute(instructions::cos_f64 const &inst, float64_t x, executor_context &context)
{
	auto const result = std::cos(x);
	check_for_nan_or_inf("cos", inst.src_tokens_index, x, result, context);
	return result;
}

static float32_t execute(instructions::cos_f32_unchecked const &, float32_t x, executor_context &)
{
	return std::cos(x);
}

static float64_t execute(instructions::cos_f64_unchecked const &, float64_t x, executor_context &)
{
	return std::cos(x);
}

static float32_t execute(instructions::tan_f32 const &inst, float32_t x, executor_context &context)
{
	auto const result = std::tan(x);
	check_for_nan_or_inf("tan", inst.src_tokens_index, x, result, context);
	return result;
}

static float64_t execute(instructions::tan_f64 const &inst, float64_t x, executor_context &context)
{
	auto const result = std::tan(x);
	check_for_nan_or_inf("tan", inst.src_tokens_index, x, result, context);
	return result;
}

static float32_t execute(instructions::tan_f32_unchecked const &, float32_t x, executor_context &)
{
	return std::tan(x);
}

static float64_t execute(instructions::tan_f64_unchecked const &, float64_t x, executor_context &)
{
	return std::tan(x);
}

static float32_t execute(instructions::asin_f32 const &inst, float32_t x, executor_context &context)
{
	auto const result = std::asin(x);
	if (std::isnan(x) || std::fabs(x) > 1.0f)
	{
		report_regular_math_error("asin", inst.src_tokens_index, x, result, context);
	}
	return result;
}

static float64_t execute(instructions::asin_f64 const &inst, float64_t x, executor_context &context)
{
	auto const result = std::asin(x);
	if (std::isnan(x) || std::fabs(x) > 1.0)
	{
		report_regular_math_error("asin", inst.src_tokens_index, x, result, context);
	}
	return result;
}

static float32_t execute(instructions::asin_f32_unchecked const &, float32_t x, executor_context &)
{
	return std::asin(x);
}

static float64_t execute(instructions::asin_f64_unchecked const &, float64_t x, executor_context &)
{
	return std::asin(x);
}

static float32_t execute(instructions::acos_f32 const &inst, float32_t x, executor_context &context)
{
	auto const result = std::acos(x);
	if (std::isnan(x) || std::fabs(x) > 1.0f)
	{
		report_regular_math_error("acos", inst.src_tokens_index, x, result, context);
	}
	return result;
}

static float64_t execute(instructions::acos_f64 const &inst, float64_t x, executor_context &context)
{
	auto const result = std::acos(x);
	if (std::isnan(x) || std::fabs(x) > 1.0)
	{
		report_regular_math_error("acos", inst.src_tokens_index, x, result, context);
	}
	return result;
}

static float32_t execute(instructions::acos_f32_unchecked const &, float32_t x, executor_context &)
{
	return std::acos(x);
}

static float64_t execute(instructions::acos_f64_unchecked const &, float64_t x, executor_context &)
{
	return std::acos(x);
}

static float32_t execute(instructions::atan_f32 const &inst, float32_t x, executor_context &context)
{
	check_for_nan("atan", inst.src_tokens_index, x, context);
	return std::atan(x);
}

static float64_t execute(instructions::atan_f64 const &inst, float64_t x, executor_context &context)
{
	check_for_nan("atan", inst.src_tokens_index, x, context);
	return std::atan(x);
}

static float32_t execute(instructions::atan_f32_unchecked const &, float32_t x, executor_context &)
{
	return std::atan(x);
}

static float64_t execute(instructions::atan_f64_unchecked const &, float64_t x, executor_context &)
{
	return std::atan(x);
}

static float32_t execute(instructions::atan2_f32 const &inst, float32_t y, float32_t x, executor_context &context)
{
	auto const result = std::atan2(y, x);
	if (std::isnan(y) || std::isnan(x))
	{
		context.report_warning(
			ctx::warning_kind::math_domain_error,
			inst.src_tokens_index,
			bz::format("calling 'atan2' with {} and {} results in {}", y, x, result)
		);
	}
	return result;
}

static float64_t execute(instructions::atan2_f64 const &inst, float64_t y, float64_t x, executor_context &context)
{
	auto const result = std::atan2(y, x);
	if (std::isnan(y) || std::isnan(x))
	{
		context.report_warning(
			ctx::warning_kind::math_domain_error,
			inst.src_tokens_index,
			bz::format("calling 'atan2' with {} and {} results in {}", y, x, result)
		);
	}
	return result;
}

static float32_t execute(instructions::atan2_f32_unchecked const &, float32_t y, float32_t x, executor_context &)
{
	return std::atan2(y, x);
}

static float64_t execute(instructions::atan2_f64_unchecked const &, float64_t y, float64_t x, executor_context &)
{
	return std::atan2(y, x);
}

static float32_t execute(instructions::sinh_f32 const &inst, float32_t x, executor_context &context)
{
	check_for_nan("sinh", inst.src_tokens_index, x, context);
	return std::sinh(x);
}

static float64_t execute(instructions::sinh_f64 const &inst, float64_t x, executor_context &context)
{
	check_for_nan("sinh", inst.src_tokens_index, x, context);
	return std::sinh(x);
}

static float32_t execute(instructions::sinh_f32_unchecked const &, float32_t x, executor_context &)
{
	return std::sinh(x);
}

static float64_t execute(instructions::sinh_f64_unchecked const &, float64_t x, executor_context &)
{
	return std::sinh(x);
}

static float32_t execute(instructions::cosh_f32 const &inst, float32_t x, executor_context &context)
{
	check_for_nan("cosh", inst.src_tokens_index, x, context);
	return std::cosh(x);
}

static float64_t execute(instructions::cosh_f64 const &inst, float64_t x, executor_context &context)
{
	check_for_nan("cosh", inst.src_tokens_index, x, context);
	return std::cosh(x);
}

static float32_t execute(instructions::cosh_f32_unchecked const &, float32_t x, executor_context &)
{
	return std::cosh(x);
}

static float64_t execute(instructions::cosh_f64_unchecked const &, float64_t x, executor_context &)
{
	return std::cosh(x);
}

static float32_t execute(instructions::tanh_f32 const &inst, float32_t x, executor_context &context)
{
	check_for_nan("tanh", inst.src_tokens_index, x, context);
	return std::tanh(x);
}

static float64_t execute(instructions::tanh_f64 const &inst, float64_t x, executor_context &context)
{
	check_for_nan("tanh", inst.src_tokens_index, x, context);
	return std::tanh(x);
}

static float32_t execute(instructions::tanh_f32_unchecked const &, float32_t x, executor_context &)
{
	return std::tanh(x);
}

static float64_t execute(instructions::tanh_f64_unchecked const &, float64_t x, executor_context &)
{
	return std::tanh(x);
}

static float32_t execute(instructions::asinh_f32 const &inst, float32_t x, executor_context &context)
{
	check_for_nan("asinh", inst.src_tokens_index, x, context);
	return std::asinh(x);
}

static float64_t execute(instructions::asinh_f64 const &inst, float64_t x, executor_context &context)
{
	check_for_nan("asinh", inst.src_tokens_index, x, context);
	return std::asinh(x);
}

static float32_t execute(instructions::asinh_f32_unchecked const &, float32_t x, executor_context &)
{
	return std::asinh(x);
}

static float64_t execute(instructions::asinh_f64_unchecked const &, float64_t x, executor_context &)
{
	return std::asinh(x);
}

static float32_t execute(instructions::acosh_f32 const &inst, float32_t x, executor_context &context)
{
	auto const result = std::acosh(x);
	if (std::isnan(x) || x < 1.0f)
	{
		report_regular_math_error("acosh", inst.src_tokens_index, x, result, context);
	}
	return result;
}

static float64_t execute(instructions::acosh_f64 const &inst, float64_t x, executor_context &context)
{
	auto const result = std::acosh(x);
	if (std::isnan(x) || x < 1.0)
	{
		report_regular_math_error("acosh", inst.src_tokens_index, x, result, context);
	}
	return result;
}

static float32_t execute(instructions::acosh_f32_unchecked const &, float32_t x, executor_context &)
{
	return std::acosh(x);
}

static float64_t execute(instructions::acosh_f64_unchecked const &, float64_t x, executor_context &)
{
	return std::acosh(x);
}

static float32_t execute(instructions::atanh_f32 const &inst, float32_t x, executor_context &context)
{
	auto const result = std::atanh(x);
	if (std::isnan(x) || std::fabs(x) >= 1.0f)
	{
		report_regular_math_error("atanh", inst.src_tokens_index, x, result, context);
	}
	return result;
}

static float64_t execute(instructions::atanh_f64 const &inst, float64_t x, executor_context &context)
{
	auto const result = std::atanh(x);
	if (std::isnan(x) || std::fabs(x) >= 1.0)
	{
		report_regular_math_error("atanh", inst.src_tokens_index, x, result, context);
	}
	return result;
}

static float32_t execute(instructions::atanh_f32_unchecked const &, float32_t x, executor_context &)
{
	return std::atanh(x);
}

static float64_t execute(instructions::atanh_f64_unchecked const &, float64_t x, executor_context &)
{
	return std::atanh(x);
}

static float32_t execute(instructions::erf_f32 const &inst, float32_t x, executor_context &context)
{
	check_for_nan("erf", inst.src_tokens_index, x, context);
	return std::erf(x);
}

static float64_t execute(instructions::erf_f64 const &inst, float64_t x, executor_context &context)
{
	check_for_nan("erf", inst.src_tokens_index, x, context);
	return std::erf(x);
}

static float32_t execute(instructions::erf_f32_unchecked const &, float32_t x, executor_context &)
{
	return std::erf(x);
}

static float64_t execute(instructions::erf_f64_unchecked const &, float64_t x, executor_context &)
{
	return std::erf(x);
}

static float32_t execute(instructions::erfc_f32 const &inst, float32_t x, executor_context &context)
{
	check_for_nan("erfc", inst.src_tokens_index, x, context);
	return std::erfc(x);
}

static float64_t execute(instructions::erfc_f64 const &inst, float64_t x, executor_context &context)
{
	check_for_nan("erfc", inst.src_tokens_index, x, context);
	return std::erfc(x);
}

static float32_t execute(instructions::erfc_f32_unchecked const &, float32_t x, executor_context &)
{
	return std::erfc(x);
}

static float64_t execute(instructions::erfc_f64_unchecked const &, float64_t x, executor_context &)
{
	return std::erfc(x);
}

static float32_t execute(instructions::tgamma_f32 const &inst, float32_t x, executor_context &context)
{
	auto const result = std::tgamma(x);
	if (std::isnan(x) || x == 0.0f || x == -std::numeric_limits<float32_t>::infinity())
	{
		report_regular_math_error("tgamma", inst.src_tokens_index, x, result, context);
	}
	else if (x < 0.0f && isint(x))
	{
		report_negative_integer_math_error("tgamma", inst.src_tokens_index, x, result, context);
	}
	return result;
}

static float64_t execute(instructions::tgamma_f64 const &inst, float64_t x, executor_context &context)
{
	auto const result = std::tgamma(x);
	if (std::isnan(x) || x == 0.0 || x == -std::numeric_limits<float64_t>::infinity())
	{
		report_regular_math_error("tgamma", inst.src_tokens_index, x, result, context);
	}
	else if (x < 0.0 && isint(x))
	{
		report_negative_integer_math_error("tgamma", inst.src_tokens_index, x, result, context);
	}
	return result;
}

static float32_t execute(instructions::tgamma_f32_unchecked const &, float32_t x, executor_context &)
{
	return std::tgamma(x);
}

static float64_t execute(instructions::tgamma_f64_unchecked const &, float64_t x, executor_context &)
{
	return std::tgamma(x);
}

static float32_t execute(instructions::lgamma_f32 const &inst, float32_t x, executor_context &context)
{
	auto const result = std::lgamma(x);
	if (std::isnan(x) || x == 0.0f)
	{
		report_regular_math_error("lgamma", inst.src_tokens_index, x, result, context);
	}
	else if (x < 0.0f && isint(x))
	{
		report_negative_integer_math_error("lgamma", inst.src_tokens_index, x, result, context);
	}
	return result;
}

static float64_t execute(instructions::lgamma_f64 const &inst, float64_t x, executor_context &context)
{
	auto const result = std::lgamma(x);
	if (std::isnan(x) || x == 0.0)
	{
		report_regular_math_error("lgamma", inst.src_tokens_index, x, result, context);
	}
	else if (x < 0.0 && isint(x))
	{
		report_negative_integer_math_error("lgamma", inst.src_tokens_index, x, result, context);
	}
	return result;
}

static float32_t execute(instructions::lgamma_f32_unchecked const &, float32_t x, executor_context &)
{
	return std::lgamma(x);
}

static float64_t execute(instructions::lgamma_f64_unchecked const &, float64_t x, executor_context &)
{
	return std::lgamma(x);
}

static uint8_t execute(instructions::bitreverse_u8 const &, uint8_t value, executor_context &)
{
	value = ((value & uint8_t(0b1111'0000)) >> 4) | ((value & uint8_t(0b0000'1111)) << 4);
	value = ((value & uint8_t(0b1100'1100)) >> 2) | ((value & uint8_t(0b0011'0011)) << 2);
	value = ((value & uint8_t(0b1010'1010)) >> 1) | ((value & uint8_t(0b0101'0101)) << 1);
	return value;
}

static uint16_t execute(instructions::bitreverse_u16 const &, uint16_t value, executor_context &)
{
	value = ((value & uint16_t(0xff00)) >> 8) | ((value & uint16_t(0x00ff)) << 8);
	value = ((value & uint16_t(0b1111'0000'1111'0000)) >> 4) | ((value & uint16_t(0b0000'1111'0000'1111)) << 4);
	value = ((value & uint16_t(0b1100'1100'1100'1100)) >> 2) | ((value & uint16_t(0b0011'0011'0011'0011)) << 2);
	value = ((value & uint16_t(0b1010'1010'1010'1010)) >> 1) | ((value & uint16_t(0b0101'0101'0101'0101)) << 1);
	return value;
}

static uint32_t execute(instructions::bitreverse_u32 const &, uint32_t value, executor_context &)
{
	value = ((value & uint32_t(0xffff'0000)) >> 16) | ((value & uint32_t(0x0000'ffff)) << 16);
	value = ((value & uint32_t(0xff00'ff00)) >> 8) | ((value & uint32_t(0x00ff'00ff)) << 8);
	value = ((value & uint32_t(0b1111'0000'1111'0000'1111'0000'1111'0000)) >> 4)
		| ((value & uint32_t(0b0000'1111'0000'1111'0000'1111'0000'1111)) << 4);
	value = ((value & uint32_t(0b1100'1100'1100'1100'1100'1100'1100'1100)) >> 2)
		| ((value & uint32_t(0b0011'0011'0011'0011'0011'0011'0011'0011)) << 2);
	value = ((value & uint32_t(0b1010'1010'1010'1010'1010'1010'1010'1010)) >> 1)
		| ((value & uint32_t(0b0101'0101'0101'0101'0101'0101'0101'0101)) << 1);
	return value;
}

static uint64_t execute(instructions::bitreverse_u64 const &, uint64_t value, executor_context &)
{
	value = ((value & uint64_t(0xffff'ffff'0000'0000)) >> 32) | ((value & uint64_t(0x0000'0000'ffff'ffff)) << 32);
	value = ((value & uint64_t(0xffff'0000'ffff'0000)) >> 16) | ((value & uint64_t(0x0000'ffff'0000'ffff)) << 16);
	value = ((value & uint64_t(0xff00'ff00'ff00'ff00)) >> 8) | ((value & uint64_t(0x00ff'00ff'00ff'00ff)) << 8);
	value = ((value & uint64_t(0b1111'0000'1111'0000'1111'0000'1111'0000'1111'0000'1111'0000'1111'0000'1111'0000)) >> 4)
		| ((value & uint64_t(0b0000'1111'0000'1111'0000'1111'0000'1111'0000'1111'0000'1111'0000'1111'0000'1111)) << 4);
	value = ((value & uint64_t(0b1100'1100'1100'1100'1100'1100'1100'1100'1100'1100'1100'1100'1100'1100'1100'1100)) >> 2)
		| ((value & uint64_t(0b0011'0011'0011'0011'0011'0011'0011'0011'0011'0011'0011'0011'0011'0011'0011'0011)) << 2);
	value = ((value & uint64_t(0b1010'1010'1010'1010'1010'1010'1010'1010'1010'1010'1010'1010'1010'1010'1010'1010)) >> 1)
		| ((value & uint64_t(0b0101'0101'0101'0101'0101'0101'0101'0101'0101'0101'0101'0101'0101'0101'0101'0101)) << 1);
	return value;
}

static uint8_t execute(instructions::popcount_u8 const &, uint8_t value, executor_context &)
{
	return std::popcount(value);
}

static uint16_t execute(instructions::popcount_u16 const &, uint16_t value, executor_context &)
{
	return std::popcount(value);
}

static uint32_t execute(instructions::popcount_u32 const &, uint32_t value, executor_context &)
{
	return std::popcount(value);
}

static uint64_t execute(instructions::popcount_u64 const &, uint64_t value, executor_context &)
{
	return std::popcount(value);
}

static uint16_t execute(instructions::byteswap_u16 const &, uint16_t value, executor_context &)
{
	value = ((value & uint16_t(0xff00)) >> 8) | ((value & uint16_t(0x00ff)) << 8);
	return value;
}

static uint32_t execute(instructions::byteswap_u32 const &, uint32_t value, executor_context &)
{
	value = ((value & uint32_t(0xffff'0000)) >> 16) | ((value & uint32_t(0x0000'ffff)) << 16);
	value = ((value & uint32_t(0xff00'ff00)) >> 8) | ((value & uint32_t(0x00ff'00ff)) << 8);
	return value;
}

static uint64_t execute(instructions::byteswap_u64 const &, uint64_t value, executor_context &)
{
	value = ((value & uint64_t(0xffff'ffff'0000'0000)) >> 32) | ((value & uint64_t(0x0000'0000'ffff'ffff)) << 32);
	value = ((value & uint64_t(0xffff'0000'ffff'0000)) >> 16) | ((value & uint64_t(0x0000'ffff'0000'ffff)) << 16);
	value = ((value & uint64_t(0xff00'ff00'ff00'ff00)) >> 8) | ((value & uint64_t(0x00ff'00ff'00ff'00ff)) << 8);
	return value;
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

static instruction_value execute(instructions::function_call const &inst, executor_context &context);

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

static void execute(instructions::unreachable const &, executor_context &)
{
	bz_unreachable;
}

static void execute(instructions::error const &error, executor_context &context)
{
	context.report_error(error.error_index);
}

static void execute(instructions::diagnostic_str const &inst, ptr_t begin, ptr_t end, executor_context &context)
{
	auto const begin_ptr = context.get_memory(begin, 0);
	auto const end_ptr = context.get_memory(end, 0);
	auto const message = bz::u8string_view(begin_ptr, end_ptr);
	if (inst.kind == ctx::warning_kind::_last)
	{
		context.report_error(inst.src_tokens_index, message);
	}
	else
	{
		context.report_warning(inst.kind, inst.src_tokens_index, message);
	}
}

static void execute(instructions::array_bounds_check_i32 const &inst, uint32_t uindex, uint32_t size, executor_context &context)
{
	auto const index = static_cast<int32_t>(uindex);
	if (index < 0)
	{
		context.report_error(
			inst.src_tokens_index,
			bz::format("negative index {} in subscript for an array of size {}", index, size)
		);
	}
	else if (uindex >= size)
	{
		context.report_error(
			inst.src_tokens_index,
			bz::format("index {} is out-of-bounds for an array of size {}", uindex, size)
		);
	}
}

static void execute(instructions::array_bounds_check_u32 const &inst, uint32_t index, uint32_t size, executor_context &context)
{
	if (index >= size)
	{
		context.report_error(
			inst.src_tokens_index,
			bz::format("index {} is out-of-bounds for an array of size {}", index, size)
		);
	}
}

static void execute(instructions::array_bounds_check_i64 const &inst, uint64_t uindex, uint64_t size, executor_context &context)
{
	auto const index = static_cast<int64_t>(uindex);
	if (index < 0)
	{
		context.report_error(
			inst.src_tokens_index,
			bz::format("negative index {} in subscript for an array of size {}", index, size)
		);
	}
	else if (uindex >= size)
	{
		context.report_error(
			inst.src_tokens_index,
			bz::format("index {} is out-of-bounds for an array of size {}", uindex, size)
		);
	}
}

static void execute(instructions::array_bounds_check_u64 const &inst, uint64_t index, uint64_t size, executor_context &context)
{
	if (index >= size)
	{
		context.report_error(
			inst.src_tokens_index,
			bz::format("index {} is out-of-bounds for an array of size {}", index, size)
		);
	}
}

static void execute(instructions::optional_get_value_check const &inst, bool has_value, executor_context &context)
{
	if (!has_value)
	{
		context.report_error(inst.src_tokens_index, "getting value of a null optional");
	}
}

static void execute(instructions::str_construction_check const &inst, ptr_t begin_ptr, ptr_t end_ptr, executor_context &context);
static void execute(instructions::slice_construction_check const &inst, ptr_t begin_ptr, ptr_t end_ptr, executor_context &context);


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
	else if constexpr (type == value_type::any)
	{
		return value;
	}
	else
	{
		static_assert(bz::meta::always_false<get_value_type<type>>);
	}
}


template<typename Inst>
static void execute(executor_context &context)
{
	auto const &inst = *context.current_instruction;
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
	switch (context.current_instruction->index())
	{
		static_assert(instruction::variant_count == 376);
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
		case instruction::cmp_lt_i8:
			execute<instructions::cmp_lt_i8>(context);
			break;
		case instruction::cmp_lt_i16:
			execute<instructions::cmp_lt_i16>(context);
			break;
		case instruction::cmp_lt_i32:
			execute<instructions::cmp_lt_i32>(context);
			break;
		case instruction::cmp_lt_i64:
			execute<instructions::cmp_lt_i64>(context);
			break;
		case instruction::cmp_lt_u8:
			execute<instructions::cmp_lt_u8>(context);
			break;
		case instruction::cmp_lt_u16:
			execute<instructions::cmp_lt_u16>(context);
			break;
		case instruction::cmp_lt_u32:
			execute<instructions::cmp_lt_u32>(context);
			break;
		case instruction::cmp_lt_u64:
			execute<instructions::cmp_lt_u64>(context);
			break;
		case instruction::cmp_lt_f32:
			execute<instructions::cmp_lt_f32>(context);
			break;
		case instruction::cmp_lt_f64:
			execute<instructions::cmp_lt_f64>(context);
			break;
		case instruction::cmp_lt_f32_unchecked:
			execute<instructions::cmp_lt_f32_unchecked>(context);
			break;
		case instruction::cmp_lt_f64_unchecked:
			execute<instructions::cmp_lt_f64_unchecked>(context);
			break;
		case instruction::cmp_gt_i8:
			execute<instructions::cmp_gt_i8>(context);
			break;
		case instruction::cmp_gt_i16:
			execute<instructions::cmp_gt_i16>(context);
			break;
		case instruction::cmp_gt_i32:
			execute<instructions::cmp_gt_i32>(context);
			break;
		case instruction::cmp_gt_i64:
			execute<instructions::cmp_gt_i64>(context);
			break;
		case instruction::cmp_gt_u8:
			execute<instructions::cmp_gt_u8>(context);
			break;
		case instruction::cmp_gt_u16:
			execute<instructions::cmp_gt_u16>(context);
			break;
		case instruction::cmp_gt_u32:
			execute<instructions::cmp_gt_u32>(context);
			break;
		case instruction::cmp_gt_u64:
			execute<instructions::cmp_gt_u64>(context);
			break;
		case instruction::cmp_gt_f32:
			execute<instructions::cmp_gt_f32>(context);
			break;
		case instruction::cmp_gt_f64:
			execute<instructions::cmp_gt_f64>(context);
			break;
		case instruction::cmp_gt_f32_unchecked:
			execute<instructions::cmp_gt_f32_unchecked>(context);
			break;
		case instruction::cmp_gt_f64_unchecked:
			execute<instructions::cmp_gt_f64_unchecked>(context);
			break;
		case instruction::cmp_lte_i8:
			execute<instructions::cmp_lte_i8>(context);
			break;
		case instruction::cmp_lte_i16:
			execute<instructions::cmp_lte_i16>(context);
			break;
		case instruction::cmp_lte_i32:
			execute<instructions::cmp_lte_i32>(context);
			break;
		case instruction::cmp_lte_i64:
			execute<instructions::cmp_lte_i64>(context);
			break;
		case instruction::cmp_lte_u8:
			execute<instructions::cmp_lte_u8>(context);
			break;
		case instruction::cmp_lte_u16:
			execute<instructions::cmp_lte_u16>(context);
			break;
		case instruction::cmp_lte_u32:
			execute<instructions::cmp_lte_u32>(context);
			break;
		case instruction::cmp_lte_u64:
			execute<instructions::cmp_lte_u64>(context);
			break;
		case instruction::cmp_lte_f32:
			execute<instructions::cmp_lte_f32>(context);
			break;
		case instruction::cmp_lte_f64:
			execute<instructions::cmp_lte_f64>(context);
			break;
		case instruction::cmp_lte_f32_unchecked:
			execute<instructions::cmp_lte_f32_unchecked>(context);
			break;
		case instruction::cmp_lte_f64_unchecked:
			execute<instructions::cmp_lte_f64_unchecked>(context);
			break;
		case instruction::cmp_gte_i8:
			execute<instructions::cmp_gte_i8>(context);
			break;
		case instruction::cmp_gte_i16:
			execute<instructions::cmp_gte_i16>(context);
			break;
		case instruction::cmp_gte_i32:
			execute<instructions::cmp_gte_i32>(context);
			break;
		case instruction::cmp_gte_i64:
			execute<instructions::cmp_gte_i64>(context);
			break;
		case instruction::cmp_gte_u8:
			execute<instructions::cmp_gte_u8>(context);
			break;
		case instruction::cmp_gte_u16:
			execute<instructions::cmp_gte_u16>(context);
			break;
		case instruction::cmp_gte_u32:
			execute<instructions::cmp_gte_u32>(context);
			break;
		case instruction::cmp_gte_u64:
			execute<instructions::cmp_gte_u64>(context);
			break;
		case instruction::cmp_gte_f32:
			execute<instructions::cmp_gte_f32>(context);
			break;
		case instruction::cmp_gte_f64:
			execute<instructions::cmp_gte_f64>(context);
			break;
		case instruction::cmp_gte_f32_unchecked:
			execute<instructions::cmp_gte_f32_unchecked>(context);
			break;
		case instruction::cmp_gte_f64_unchecked:
			execute<instructions::cmp_gte_f64_unchecked>(context);
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
		case instruction::and_i1:
			execute<instructions::and_i1>(context);
			break;
		case instruction::and_i8:
			execute<instructions::and_i8>(context);
			break;
		case instruction::and_i16:
			execute<instructions::and_i16>(context);
			break;
		case instruction::and_i32:
			execute<instructions::and_i32>(context);
			break;
		case instruction::and_i64:
			execute<instructions::and_i64>(context);
			break;
		case instruction::xor_i1:
			execute<instructions::xor_i1>(context);
			break;
		case instruction::xor_i8:
			execute<instructions::xor_i8>(context);
			break;
		case instruction::xor_i16:
			execute<instructions::xor_i16>(context);
			break;
		case instruction::xor_i32:
			execute<instructions::xor_i32>(context);
			break;
		case instruction::xor_i64:
			execute<instructions::xor_i64>(context);
			break;
		case instruction::or_i1:
			execute<instructions::or_i1>(context);
			break;
		case instruction::or_i8:
			execute<instructions::or_i8>(context);
			break;
		case instruction::or_i16:
			execute<instructions::or_i16>(context);
			break;
		case instruction::or_i32:
			execute<instructions::or_i32>(context);
			break;
		case instruction::or_i64:
			execute<instructions::or_i64>(context);
			break;
		case instruction::abs_i8:
			execute<instructions::abs_i8>(context);
			break;
		case instruction::abs_i16:
			execute<instructions::abs_i16>(context);
			break;
		case instruction::abs_i32:
			execute<instructions::abs_i32>(context);
			break;
		case instruction::abs_i64:
			execute<instructions::abs_i64>(context);
			break;
		case instruction::abs_f32:
			execute<instructions::abs_f32>(context);
			break;
		case instruction::abs_f64:
			execute<instructions::abs_f64>(context);
			break;
		case instruction::abs_i8_unchecked:
			execute<instructions::abs_i8_unchecked>(context);
			break;
		case instruction::abs_i16_unchecked:
			execute<instructions::abs_i16_unchecked>(context);
			break;
		case instruction::abs_i32_unchecked:
			execute<instructions::abs_i32_unchecked>(context);
			break;
		case instruction::abs_i64_unchecked:
			execute<instructions::abs_i64_unchecked>(context);
			break;
		case instruction::abs_f32_unchecked:
			execute<instructions::abs_f32_unchecked>(context);
			break;
		case instruction::abs_f64_unchecked:
			execute<instructions::abs_f64_unchecked>(context);
			break;
		case instruction::min_i8:
			execute<instructions::min_i8>(context);
			break;
		case instruction::min_i16:
			execute<instructions::min_i16>(context);
			break;
		case instruction::min_i32:
			execute<instructions::min_i32>(context);
			break;
		case instruction::min_i64:
			execute<instructions::min_i64>(context);
			break;
		case instruction::min_u8:
			execute<instructions::min_u8>(context);
			break;
		case instruction::min_u16:
			execute<instructions::min_u16>(context);
			break;
		case instruction::min_u32:
			execute<instructions::min_u32>(context);
			break;
		case instruction::min_u64:
			execute<instructions::min_u64>(context);
			break;
		case instruction::min_f32:
			execute<instructions::min_f32>(context);
			break;
		case instruction::min_f64:
			execute<instructions::min_f64>(context);
			break;
		case instruction::min_f32_unchecked:
			execute<instructions::min_f32_unchecked>(context);
			break;
		case instruction::min_f64_unchecked:
			execute<instructions::min_f64_unchecked>(context);
			break;
		case instruction::max_i8:
			execute<instructions::max_i8>(context);
			break;
		case instruction::max_i16:
			execute<instructions::max_i16>(context);
			break;
		case instruction::max_i32:
			execute<instructions::max_i32>(context);
			break;
		case instruction::max_i64:
			execute<instructions::max_i64>(context);
			break;
		case instruction::max_u8:
			execute<instructions::max_u8>(context);
			break;
		case instruction::max_u16:
			execute<instructions::max_u16>(context);
			break;
		case instruction::max_u32:
			execute<instructions::max_u32>(context);
			break;
		case instruction::max_u64:
			execute<instructions::max_u64>(context);
			break;
		case instruction::max_f32:
			execute<instructions::max_f32>(context);
			break;
		case instruction::max_f64:
			execute<instructions::max_f64>(context);
			break;
		case instruction::max_f32_unchecked:
			execute<instructions::max_f32_unchecked>(context);
			break;
		case instruction::max_f64_unchecked:
			execute<instructions::max_f64_unchecked>(context);
			break;
		case instruction::exp_f32:
			execute<instructions::exp_f32>(context);
			break;
		case instruction::exp_f64:
			execute<instructions::exp_f64>(context);
			break;
		case instruction::exp_f32_unchecked:
			execute<instructions::exp_f32_unchecked>(context);
			break;
		case instruction::exp_f64_unchecked:
			execute<instructions::exp_f64_unchecked>(context);
			break;
		case instruction::exp2_f32:
			execute<instructions::exp2_f32>(context);
			break;
		case instruction::exp2_f64:
			execute<instructions::exp2_f64>(context);
			break;
		case instruction::exp2_f32_unchecked:
			execute<instructions::exp2_f32_unchecked>(context);
			break;
		case instruction::exp2_f64_unchecked:
			execute<instructions::exp2_f64_unchecked>(context);
			break;
		case instruction::expm1_f32:
			execute<instructions::expm1_f32>(context);
			break;
		case instruction::expm1_f64:
			execute<instructions::expm1_f64>(context);
			break;
		case instruction::expm1_f32_unchecked:
			execute<instructions::expm1_f32_unchecked>(context);
			break;
		case instruction::expm1_f64_unchecked:
			execute<instructions::expm1_f64_unchecked>(context);
			break;
		case instruction::log_f32:
			execute<instructions::log_f32>(context);
			break;
		case instruction::log_f64:
			execute<instructions::log_f64>(context);
			break;
		case instruction::log_f32_unchecked:
			execute<instructions::log_f32_unchecked>(context);
			break;
		case instruction::log_f64_unchecked:
			execute<instructions::log_f64_unchecked>(context);
			break;
		case instruction::log10_f32:
			execute<instructions::log10_f32>(context);
			break;
		case instruction::log10_f64:
			execute<instructions::log10_f64>(context);
			break;
		case instruction::log10_f32_unchecked:
			execute<instructions::log10_f32_unchecked>(context);
			break;
		case instruction::log10_f64_unchecked:
			execute<instructions::log10_f64_unchecked>(context);
			break;
		case instruction::log2_f32:
			execute<instructions::log2_f32>(context);
			break;
		case instruction::log2_f64:
			execute<instructions::log2_f64>(context);
			break;
		case instruction::log2_f32_unchecked:
			execute<instructions::log2_f32_unchecked>(context);
			break;
		case instruction::log2_f64_unchecked:
			execute<instructions::log2_f64_unchecked>(context);
			break;
		case instruction::log1p_f32:
			execute<instructions::log1p_f32>(context);
			break;
		case instruction::log1p_f64:
			execute<instructions::log1p_f64>(context);
			break;
		case instruction::log1p_f32_unchecked:
			execute<instructions::log1p_f32_unchecked>(context);
			break;
		case instruction::log1p_f64_unchecked:
			execute<instructions::log1p_f64_unchecked>(context);
			break;
		case instruction::sqrt_f32:
			execute<instructions::sqrt_f32>(context);
			break;
		case instruction::sqrt_f64:
			execute<instructions::sqrt_f64>(context);
			break;
		case instruction::sqrt_f32_unchecked:
			execute<instructions::sqrt_f32_unchecked>(context);
			break;
		case instruction::sqrt_f64_unchecked:
			execute<instructions::sqrt_f64_unchecked>(context);
			break;
		case instruction::pow_f32:
			execute<instructions::pow_f32>(context);
			break;
		case instruction::pow_f64:
			execute<instructions::pow_f64>(context);
			break;
		case instruction::pow_f32_unchecked:
			execute<instructions::pow_f32_unchecked>(context);
			break;
		case instruction::pow_f64_unchecked:
			execute<instructions::pow_f64_unchecked>(context);
			break;
		case instruction::cbrt_f32:
			execute<instructions::cbrt_f32>(context);
			break;
		case instruction::cbrt_f64:
			execute<instructions::cbrt_f64>(context);
			break;
		case instruction::cbrt_f32_unchecked:
			execute<instructions::cbrt_f32_unchecked>(context);
			break;
		case instruction::cbrt_f64_unchecked:
			execute<instructions::cbrt_f64_unchecked>(context);
			break;
		case instruction::hypot_f32:
			execute<instructions::hypot_f32>(context);
			break;
		case instruction::hypot_f64:
			execute<instructions::hypot_f64>(context);
			break;
		case instruction::hypot_f32_unchecked:
			execute<instructions::hypot_f32_unchecked>(context);
			break;
		case instruction::hypot_f64_unchecked:
			execute<instructions::hypot_f64_unchecked>(context);
			break;
		case instruction::sin_f32:
			execute<instructions::sin_f32>(context);
			break;
		case instruction::sin_f64:
			execute<instructions::sin_f64>(context);
			break;
		case instruction::sin_f32_unchecked:
			execute<instructions::sin_f32_unchecked>(context);
			break;
		case instruction::sin_f64_unchecked:
			execute<instructions::sin_f64_unchecked>(context);
			break;
		case instruction::cos_f32:
			execute<instructions::cos_f32>(context);
			break;
		case instruction::cos_f64:
			execute<instructions::cos_f64>(context);
			break;
		case instruction::cos_f32_unchecked:
			execute<instructions::cos_f32_unchecked>(context);
			break;
		case instruction::cos_f64_unchecked:
			execute<instructions::cos_f64_unchecked>(context);
			break;
		case instruction::tan_f32:
			execute<instructions::tan_f32>(context);
			break;
		case instruction::tan_f64:
			execute<instructions::tan_f64>(context);
			break;
		case instruction::tan_f32_unchecked:
			execute<instructions::tan_f32_unchecked>(context);
			break;
		case instruction::tan_f64_unchecked:
			execute<instructions::tan_f64_unchecked>(context);
			break;
		case instruction::asin_f32:
			execute<instructions::asin_f32>(context);
			break;
		case instruction::asin_f64:
			execute<instructions::asin_f64>(context);
			break;
		case instruction::asin_f32_unchecked:
			execute<instructions::asin_f32_unchecked>(context);
			break;
		case instruction::asin_f64_unchecked:
			execute<instructions::asin_f64_unchecked>(context);
			break;
		case instruction::acos_f32:
			execute<instructions::acos_f32>(context);
			break;
		case instruction::acos_f64:
			execute<instructions::acos_f64>(context);
			break;
		case instruction::acos_f32_unchecked:
			execute<instructions::acos_f32_unchecked>(context);
			break;
		case instruction::acos_f64_unchecked:
			execute<instructions::acos_f64_unchecked>(context);
			break;
		case instruction::atan_f32:
			execute<instructions::atan_f32>(context);
			break;
		case instruction::atan_f64:
			execute<instructions::atan_f64>(context);
			break;
		case instruction::atan_f32_unchecked:
			execute<instructions::atan_f32_unchecked>(context);
			break;
		case instruction::atan_f64_unchecked:
			execute<instructions::atan_f64_unchecked>(context);
			break;
		case instruction::atan2_f32:
			execute<instructions::atan2_f32>(context);
			break;
		case instruction::atan2_f64:
			execute<instructions::atan2_f64>(context);
			break;
		case instruction::atan2_f32_unchecked:
			execute<instructions::atan2_f32_unchecked>(context);
			break;
		case instruction::atan2_f64_unchecked:
			execute<instructions::atan2_f64_unchecked>(context);
			break;
		case instruction::sinh_f32:
			execute<instructions::sinh_f32>(context);
			break;
		case instruction::sinh_f64:
			execute<instructions::sinh_f64>(context);
			break;
		case instruction::sinh_f32_unchecked:
			execute<instructions::sinh_f32_unchecked>(context);
			break;
		case instruction::sinh_f64_unchecked:
			execute<instructions::sinh_f64_unchecked>(context);
			break;
		case instruction::cosh_f32:
			execute<instructions::cosh_f32>(context);
			break;
		case instruction::cosh_f64:
			execute<instructions::cosh_f64>(context);
			break;
		case instruction::cosh_f32_unchecked:
			execute<instructions::cosh_f32_unchecked>(context);
			break;
		case instruction::cosh_f64_unchecked:
			execute<instructions::cosh_f64_unchecked>(context);
			break;
		case instruction::tanh_f32:
			execute<instructions::tanh_f32>(context);
			break;
		case instruction::tanh_f64:
			execute<instructions::tanh_f64>(context);
			break;
		case instruction::tanh_f32_unchecked:
			execute<instructions::tanh_f32_unchecked>(context);
			break;
		case instruction::tanh_f64_unchecked:
			execute<instructions::tanh_f64_unchecked>(context);
			break;
		case instruction::asinh_f32:
			execute<instructions::asinh_f32>(context);
			break;
		case instruction::asinh_f64:
			execute<instructions::asinh_f64>(context);
			break;
		case instruction::asinh_f32_unchecked:
			execute<instructions::asinh_f32_unchecked>(context);
			break;
		case instruction::asinh_f64_unchecked:
			execute<instructions::asinh_f64_unchecked>(context);
			break;
		case instruction::acosh_f32:
			execute<instructions::acosh_f32>(context);
			break;
		case instruction::acosh_f64:
			execute<instructions::acosh_f64>(context);
			break;
		case instruction::acosh_f32_unchecked:
			execute<instructions::acosh_f32_unchecked>(context);
			break;
		case instruction::acosh_f64_unchecked:
			execute<instructions::acosh_f64_unchecked>(context);
			break;
		case instruction::atanh_f32:
			execute<instructions::atanh_f32>(context);
			break;
		case instruction::atanh_f64:
			execute<instructions::atanh_f64>(context);
			break;
		case instruction::atanh_f32_unchecked:
			execute<instructions::atanh_f32_unchecked>(context);
			break;
		case instruction::atanh_f64_unchecked:
			execute<instructions::atanh_f64_unchecked>(context);
			break;
		case instruction::erf_f32:
			execute<instructions::erf_f32>(context);
			break;
		case instruction::erf_f64:
			execute<instructions::erf_f64>(context);
			break;
		case instruction::erf_f32_unchecked:
			execute<instructions::erf_f32_unchecked>(context);
			break;
		case instruction::erf_f64_unchecked:
			execute<instructions::erf_f64_unchecked>(context);
			break;
		case instruction::erfc_f32:
			execute<instructions::erfc_f32>(context);
			break;
		case instruction::erfc_f64:
			execute<instructions::erfc_f64>(context);
			break;
		case instruction::erfc_f32_unchecked:
			execute<instructions::erfc_f32_unchecked>(context);
			break;
		case instruction::erfc_f64_unchecked:
			execute<instructions::erfc_f64_unchecked>(context);
			break;
		case instruction::tgamma_f32:
			execute<instructions::tgamma_f32>(context);
			break;
		case instruction::tgamma_f64:
			execute<instructions::tgamma_f64>(context);
			break;
		case instruction::tgamma_f32_unchecked:
			execute<instructions::tgamma_f32_unchecked>(context);
			break;
		case instruction::tgamma_f64_unchecked:
			execute<instructions::tgamma_f64_unchecked>(context);
			break;
		case instruction::lgamma_f32:
			execute<instructions::lgamma_f32>(context);
			break;
		case instruction::lgamma_f64:
			execute<instructions::lgamma_f64>(context);
			break;
		case instruction::lgamma_f32_unchecked:
			execute<instructions::lgamma_f32_unchecked>(context);
			break;
		case instruction::lgamma_f64_unchecked:
			execute<instructions::lgamma_f64_unchecked>(context);
			break;
		case instruction::bitreverse_u8:
			execute<instructions::bitreverse_u8>(context);
			break;
		case instruction::bitreverse_u16:
			execute<instructions::bitreverse_u16>(context);
			break;
		case instruction::bitreverse_u32:
			execute<instructions::bitreverse_u32>(context);
			break;
		case instruction::bitreverse_u64:
			execute<instructions::bitreverse_u64>(context);
			break;
		case instruction::popcount_u8:
			execute<instructions::popcount_u8>(context);
			break;
		case instruction::popcount_u16:
			execute<instructions::popcount_u16>(context);
			break;
		case instruction::popcount_u32:
			execute<instructions::popcount_u32>(context);
			break;
		case instruction::popcount_u64:
			execute<instructions::popcount_u64>(context);
			break;
		case instruction::byteswap_u16:
			execute<instructions::byteswap_u16>(context);
			break;
		case instruction::byteswap_u32:
			execute<instructions::byteswap_u32>(context);
			break;
		case instruction::byteswap_u64:
			execute<instructions::byteswap_u64>(context);
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
		case instruction::function_call:
			execute<instructions::function_call>(context);
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
		case instruction::unreachable:
			execute<instructions::unreachable>(context);
			break;
		case instruction::error:
			execute<instructions::error>(context);
			break;
		case instruction::diagnostic_str:
			execute<instructions::diagnostic_str>(context);
			break;
		case instruction::array_bounds_check_i32:
			execute<instructions::array_bounds_check_i32>(context);
			break;
		case instruction::array_bounds_check_u32:
			execute<instructions::array_bounds_check_u32>(context);
			break;
		case instruction::array_bounds_check_i64:
			execute<instructions::array_bounds_check_i64>(context);
			break;
		case instruction::array_bounds_check_u64:
			execute<instructions::array_bounds_check_u64>(context);
			break;
		case instruction::optional_get_value_check:
			execute<instructions::optional_get_value_check>(context);
			break;
		case instruction::str_construction_check:
			execute<instructions::str_construction_check>(context);
			break;
		case instruction::slice_construction_check:
			execute<instructions::slice_construction_check>(context);
			break;
		default:
			bz_unreachable;
	}
}

} // namespace comptime
