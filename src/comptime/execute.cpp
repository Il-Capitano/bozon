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

static ptr_t execute(instructions::alloca const &inst, executor_context &context)
{
	return context.do_alloca(inst.size, inst.align);
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

static bool execute(instructions::load1_be const &, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 1);
	return load_big_endian<uint8_t>(mem) != 0;
}

static uint8_t execute(instructions::load8_be const &, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 1);
	return load_big_endian<uint8_t>(mem);
}

static uint16_t execute(instructions::load16_be const &, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 2);
	return load_big_endian<uint16_t>(mem);
}

static uint32_t execute(instructions::load32_be const &, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 4);
	return load_big_endian<uint32_t>(mem);
}

static uint64_t execute(instructions::load64_be const &, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 8);
	return load_big_endian<uint64_t>(mem);
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

static bool execute(instructions::load1_le const &, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 1);
	return load_little_endian<uint8_t>(mem) != 0;
}

static uint8_t execute(instructions::load8_le const &, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 1);
	return load_little_endian<uint8_t>(mem);
}

static uint16_t execute(instructions::load16_le const &, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 2);
	return load_little_endian<uint16_t>(mem);
}

static uint32_t execute(instructions::load32_le const &, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 4);
	return load_little_endian<uint32_t>(mem);
}

static uint64_t execute(instructions::load64_le const &, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 8);
	return load_little_endian<uint64_t>(mem);
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

static void execute(instructions::store1_be const &, bool value, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 1);
	store_big_endian<uint8_t>(mem, value ? 1 : 0);
}

static void execute(instructions::store8_be const &, uint8_t value, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 1);
	store_big_endian<uint8_t>(mem, value);
}

static void execute(instructions::store16_be const &, uint16_t value, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 2);
	store_big_endian<uint16_t>(mem, value);
}

static void execute(instructions::store32_be const &, uint32_t value, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 4);
	store_big_endian<uint32_t>(mem, value);
}

static void execute(instructions::store64_be const &, uint64_t value, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 8);
	store_big_endian<uint64_t>(mem, value);
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

static void execute(instructions::store1_le const &, bool value, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 1);
	store_little_endian<uint8_t>(mem, value ? 1 : 0);
}

static void execute(instructions::store8_le const &, uint8_t value, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 1);
	store_little_endian<uint8_t>(mem, value);
}

static void execute(instructions::store16_le const &, uint16_t value, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 2);
	store_little_endian<uint16_t>(mem, value);
}

static void execute(instructions::store32_le const &, uint32_t value, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 4);
	store_little_endian<uint32_t>(mem, value);
}

static void execute(instructions::store64_le const &, uint64_t value, ptr_t ptr, executor_context &context)
{
	auto const mem = context.get_memory(ptr, 8);
	store_little_endian<uint64_t>(mem, value);
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

static void execute(instructions::jump const &inst, executor_context &context)
{
	context.do_jump(inst.next_bb_index);
}

static void execute(instructions::conditional_jump const &inst, bool condition, executor_context &context)
{
	if (condition)
	{
		context.do_jump(inst.true_bb_index);
	}
	else
	{
		context.do_jump(inst.false_bb_index);
	}
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
	auto const inst = context.current_instruction;
	instruction_value result;
	if constexpr (instructions::arg_count<Inst> == 0)
	{
		if constexpr (Inst::result_type != value_type::none)
		{
			get_value_ref<Inst::result_type>(result) = execute(inst->get<Inst>(), context);
		}
		else
		{
			execute(inst->get<Inst>(), context);
			result.none = none_t();
		}
	}
	else
	{
		[&]<size_t ...Is>(bz::meta::index_sequence<Is...>) {
			if constexpr (Inst::result_type != value_type::none)
			{
				get_value_ref<Inst::result_type>(result) = execute(
					inst->get<Inst>(),
					get_value<Inst::arg_types[Is]>(context.get_instruction_value(inst->get<Inst>().args[Is]))...,
					context
				);
			}
			else
			{
				execute(
					inst->get<Inst>(),
					get_value<Inst::arg_types[Is]>(context.get_instruction_value(inst->get<Inst>().args[Is]))...,
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
		static_assert(instruction::variant_count == 40);
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
		case instruction::load1_be:
			execute<instructions::load1_be>(context);
			break;
		case instruction::load8_be:
			execute<instructions::load8_be>(context);
			break;
		case instruction::load16_be:
			execute<instructions::load16_be>(context);
			break;
		case instruction::load32_be:
			execute<instructions::load32_be>(context);
			break;
		case instruction::load64_be:
			execute<instructions::load64_be>(context);
			break;
		case instruction::load_ptr32_be:
			execute<instructions::load_ptr32_be>(context);
			break;
		case instruction::load_ptr64_be:
			execute<instructions::load_ptr64_be>(context);
			break;
		case instruction::load1_le:
			execute<instructions::load1_le>(context);
			break;
		case instruction::load8_le:
			execute<instructions::load8_le>(context);
			break;
		case instruction::load16_le:
			execute<instructions::load16_le>(context);
			break;
		case instruction::load32_le:
			execute<instructions::load32_le>(context);
			break;
		case instruction::load64_le:
			execute<instructions::load64_le>(context);
			break;
		case instruction::load_ptr32_le:
			execute<instructions::load_ptr32_le>(context);
			break;
		case instruction::load_ptr64_le:
			execute<instructions::load_ptr64_le>(context);
			break;
		case instruction::store1_be:
			execute<instructions::store1_be>(context);
			break;
		case instruction::store8_be:
			execute<instructions::store8_be>(context);
			break;
		case instruction::store16_be:
			execute<instructions::store16_be>(context);
			break;
		case instruction::store32_be:
			execute<instructions::store32_be>(context);
			break;
		case instruction::store64_be:
			execute<instructions::store64_be>(context);
			break;
		case instruction::store_ptr32_be:
			execute<instructions::store_ptr32_be>(context);
			break;
		case instruction::store_ptr64_be:
			execute<instructions::store_ptr64_be>(context);
			break;
		case instruction::store1_le:
			execute<instructions::store1_le>(context);
			break;
		case instruction::store8_le:
			execute<instructions::store8_le>(context);
			break;
		case instruction::store16_le:
			execute<instructions::store16_le>(context);
			break;
		case instruction::store32_le:
			execute<instructions::store32_le>(context);
			break;
		case instruction::store64_le:
			execute<instructions::store64_le>(context);
			break;
		case instruction::store_ptr32_le:
			execute<instructions::store_ptr32_le>(context);
			break;
		case instruction::store_ptr64_le:
			execute<instructions::store_ptr64_le>(context);
			break;
		case instruction::jump:
			execute<instructions::jump>(context);
			break;
		case instruction::conditional_jump:
			execute<instructions::conditional_jump>(context);
			break;
		default:
			bz_unreachable;
	}
}

} // namespace comptime
