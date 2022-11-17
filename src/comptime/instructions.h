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


struct alloca
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::ptr;

	size_t size;
	size_t align;
};


struct load1_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i1;

	bz::array<arg_t, arg_types.size()> args;
};

struct load8_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i8;

	bz::array<arg_t, arg_types.size()> args;
};

struct load16_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i16;

	bz::array<arg_t, arg_types.size()> args;
};

struct load32_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i32;

	bz::array<arg_t, arg_types.size()> args;
};

struct load64_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i64;

	bz::array<arg_t, arg_types.size()> args;
};

struct load_ptr32_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::ptr;

	bz::array<arg_t, arg_types.size()> args;
};

struct load_ptr64_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::ptr;

	bz::array<arg_t, arg_types.size()> args;
};

struct load1_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i1;

	bz::array<arg_t, arg_types.size()> args;
};

struct load8_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i8;

	bz::array<arg_t, arg_types.size()> args;
};

struct load16_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i16;

	bz::array<arg_t, arg_types.size()> args;
};

struct load32_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i32;

	bz::array<arg_t, arg_types.size()> args;
};

struct load64_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i64;

	bz::array<arg_t, arg_types.size()> args;
};

struct load_ptr32_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::ptr;

	bz::array<arg_t, arg_types.size()> args;
};

struct load_ptr64_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::ptr;

	bz::array<arg_t, arg_types.size()> args;
};


struct store1_be
{
	static inline constexpr bz::array arg_types = { value_type::i1, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;

	bz::array<arg_t, arg_types.size()> args;
};

struct store8_be
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;

	bz::array<arg_t, arg_types.size()> args;
};

struct store16_be
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;

	bz::array<arg_t, arg_types.size()> args;
};

struct store32_be
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;

	bz::array<arg_t, arg_types.size()> args;
};

struct store64_be
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;

	bz::array<arg_t, arg_types.size()> args;
};

struct store_ptr32_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;

	bz::array<arg_t, arg_types.size()> args;
};

struct store_ptr64_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;

	bz::array<arg_t, arg_types.size()> args;
};

struct store1_le
{
	static inline constexpr bz::array arg_types = { value_type::i1, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;

	bz::array<arg_t, arg_types.size()> args;
};

struct store8_le
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;

	bz::array<arg_t, arg_types.size()> args;
};

struct store16_le
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;

	bz::array<arg_t, arg_types.size()> args;
};

struct store32_le
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;

	bz::array<arg_t, arg_types.size()> args;
};

struct store64_le
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;

	bz::array<arg_t, arg_types.size()> args;
};

struct store_ptr32_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;

	bz::array<arg_t, arg_types.size()> args;
};

struct store_ptr64_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;

	bz::array<arg_t, arg_types.size()> args;
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

	bz::array<arg_t, arg_types.size()> args;

	uint32_t true_bb_index;
	uint32_t false_bb_index;
};

struct ret
{
	static inline constexpr bz::array arg_types = { value_type::any };
	static inline constexpr value_type result_type = value_type::none;

	bz::array<arg_t, arg_types.size()> args;
};

struct ret_void
{
	static inline constexpr int arg_types = 0;
	static inline constexpr value_type result_type = value_type::none;
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
	instructions::alloca,
	instructions::load1_be,
	instructions::load8_be,
	instructions::load16_be,
	instructions::load32_be,
	instructions::load64_be,
	instructions::load_ptr32_be,
	instructions::load_ptr64_be,
	instructions::load1_le,
	instructions::load8_le,
	instructions::load16_le,
	instructions::load32_le,
	instructions::load64_le,
	instructions::load_ptr32_le,
	instructions::load_ptr64_le,
	instructions::store1_be,
	instructions::store8_be,
	instructions::store16_be,
	instructions::store32_be,
	instructions::store64_be,
	instructions::store_ptr32_be,
	instructions::store_ptr64_be,
	instructions::store1_le,
	instructions::store8_le,
	instructions::store16_le,
	instructions::store32_le,
	instructions::store64_le,
	instructions::store_ptr32_le,
	instructions::store_ptr64_le,
	instructions::jump,
	instructions::conditional_jump,
	instructions::ret,
	instructions::ret_void
>;

using instruction_base_t = bz::meta::apply_type_pack<bz::variant, instruction_list>;

struct instruction : instruction_base_t
{
	using base_t = instruction_base_t;

	static_assert(variant_count == 42);
	enum : base_t::index_t
	{
		const_i1          = base_t::index_of<instructions::const_i1>,
		const_i8          = base_t::index_of<instructions::const_i8>,
		const_i16         = base_t::index_of<instructions::const_i16>,
		const_i32         = base_t::index_of<instructions::const_i32>,
		const_i64         = base_t::index_of<instructions::const_i64>,
		const_u8          = base_t::index_of<instructions::const_u8>,
		const_u16         = base_t::index_of<instructions::const_u16>,
		const_u32         = base_t::index_of<instructions::const_u32>,
		const_u64         = base_t::index_of<instructions::const_u64>,
		alloca            = base_t::index_of<instructions::alloca>,
		load1_be          = base_t::index_of<instructions::load1_be>,
		load8_be          = base_t::index_of<instructions::load8_be>,
		load16_be         = base_t::index_of<instructions::load16_be>,
		load32_be         = base_t::index_of<instructions::load32_be>,
		load64_be         = base_t::index_of<instructions::load64_be>,
		load_ptr32_be     = base_t::index_of<instructions::load_ptr32_be>,
		load_ptr64_be     = base_t::index_of<instructions::load_ptr64_be>,
		load1_le          = base_t::index_of<instructions::load1_le>,
		load8_le          = base_t::index_of<instructions::load8_le>,
		load16_le         = base_t::index_of<instructions::load16_le>,
		load32_le         = base_t::index_of<instructions::load32_le>,
		load64_le         = base_t::index_of<instructions::load64_le>,
		load_ptr32_le     = base_t::index_of<instructions::load_ptr32_le>,
		load_ptr64_le     = base_t::index_of<instructions::load_ptr64_le>,
		store1_be         = base_t::index_of<instructions::store1_be>,
		store8_be         = base_t::index_of<instructions::store8_be>,
		store16_be        = base_t::index_of<instructions::store16_be>,
		store32_be        = base_t::index_of<instructions::store32_be>,
		store64_be        = base_t::index_of<instructions::store64_be>,
		store_ptr32_be    = base_t::index_of<instructions::store_ptr32_be>,
		store_ptr64_be    = base_t::index_of<instructions::store_ptr64_be>,
		store1_le         = base_t::index_of<instructions::store1_le>,
		store8_le         = base_t::index_of<instructions::store8_le>,
		store16_le        = base_t::index_of<instructions::store16_le>,
		store32_le        = base_t::index_of<instructions::store32_le>,
		store64_le        = base_t::index_of<instructions::store64_le>,
		store_ptr32_le    = base_t::index_of<instructions::store_ptr32_le>,
		store_ptr64_le    = base_t::index_of<instructions::store_ptr64_le>,
		jump              = base_t::index_of<instructions::jump>,
		conditional_jump  = base_t::index_of<instructions::conditional_jump>,
		ret               = base_t::index_of<instructions::ret>,
		ret_void          = base_t::index_of<instructions::ret_void>,
	};

	bool is_terminator(void) const
	{
		switch (this->index())
		{
		case jump:
		case conditional_jump:
		case ret:
		case ret_void:
			return true;
		default:
			return false;
		}
	}
};


} // namespace comptime

#endif // COMPTIME_INSTRUCTIONS_H
