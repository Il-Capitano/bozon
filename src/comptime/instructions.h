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

	bz::array<arg_t, arg_types.size()> args;
};

struct load_i8_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i8;

	bz::array<arg_t, arg_types.size()> args;
};

struct load_i16_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i16;

	bz::array<arg_t, arg_types.size()> args;
};

struct load_i32_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i32;

	bz::array<arg_t, arg_types.size()> args;
};

struct load_i64_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i64;

	bz::array<arg_t, arg_types.size()> args;
};

struct load_f32_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::f32;

	bz::array<arg_t, arg_types.size()> args;
};

struct load_f64_be
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::f64;

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

struct load_i1_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i1;

	bz::array<arg_t, arg_types.size()> args;
};

struct load_i8_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i8;

	bz::array<arg_t, arg_types.size()> args;
};

struct load_i16_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i16;

	bz::array<arg_t, arg_types.size()> args;
};

struct load_i32_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i32;

	bz::array<arg_t, arg_types.size()> args;
};

struct load_i64_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::i64;

	bz::array<arg_t, arg_types.size()> args;
};

struct load_f32_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::f32;

	bz::array<arg_t, arg_types.size()> args;
};

struct load_f64_le
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::f64;

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


struct store_i1_be
{
	static inline constexpr bz::array arg_types = { value_type::i1, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;

	bz::array<arg_t, arg_types.size()> args;
};

struct store_i8_be
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;

	bz::array<arg_t, arg_types.size()> args;
};

struct store_i16_be
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;

	bz::array<arg_t, arg_types.size()> args;
};

struct store_i32_be
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;

	bz::array<arg_t, arg_types.size()> args;
};

struct store_i64_be
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;

	bz::array<arg_t, arg_types.size()> args;
};

struct store_f32_be
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;

	bz::array<arg_t, arg_types.size()> args;
};

struct store_f64_be
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::ptr };
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

struct store_i1_le
{
	static inline constexpr bz::array arg_types = { value_type::i1, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;

	bz::array<arg_t, arg_types.size()> args;
};

struct store_i8_le
{
	static inline constexpr bz::array arg_types = { value_type::i8, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;

	bz::array<arg_t, arg_types.size()> args;
};

struct store_i16_le
{
	static inline constexpr bz::array arg_types = { value_type::i16, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;

	bz::array<arg_t, arg_types.size()> args;
};

struct store_i32_le
{
	static inline constexpr bz::array arg_types = { value_type::i32, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;

	bz::array<arg_t, arg_types.size()> args;
};

struct store_i64_le
{
	static inline constexpr bz::array arg_types = { value_type::i64, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;

	bz::array<arg_t, arg_types.size()> args;
};

struct store_f32_le
{
	static inline constexpr bz::array arg_types = { value_type::f32, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;

	bz::array<arg_t, arg_types.size()> args;
};

struct store_f64_le
{
	static inline constexpr bz::array arg_types = { value_type::f64, value_type::ptr };
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

struct const_gep
{
	static inline constexpr bz::array arg_types = { value_type::ptr };
	static inline constexpr value_type result_type = value_type::ptr;

	bz::array<arg_t, arg_types.size()> args;

	size_t offset;
};

struct const_memcpy
{
	static inline constexpr bz::array arg_types = { value_type::ptr, value_type::ptr };
	static inline constexpr value_type result_type = value_type::none;

	bz::array<arg_t, arg_types.size()> args;

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
	instructions::const_gep,
	instructions::const_memcpy,
	instructions::jump,
	instructions::conditional_jump,
	instructions::ret,
	instructions::ret_void
>;

using instruction_base_t = bz::meta::apply_type_pack<bz::variant, instruction_list>;

struct instruction : instruction_base_t
{
	using base_t = instruction_base_t;

	static_assert(variant_count == 53);
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
		const_ptr_null    = base_t::index_of<instructions::const_ptr_null>,
		alloca            = base_t::index_of<instructions::alloca>,
		load_i1_be        = base_t::index_of<instructions::load_i1_be>,
		load_i8_be        = base_t::index_of<instructions::load_i8_be>,
		load_i16_be       = base_t::index_of<instructions::load_i16_be>,
		load_i32_be       = base_t::index_of<instructions::load_i32_be>,
		load_i64_be       = base_t::index_of<instructions::load_i64_be>,
		load_f32_be       = base_t::index_of<instructions::load_f32_be>,
		load_f64_be       = base_t::index_of<instructions::load_f64_be>,
		load_ptr32_be     = base_t::index_of<instructions::load_ptr32_be>,
		load_ptr64_be     = base_t::index_of<instructions::load_ptr64_be>,
		load_i1_le        = base_t::index_of<instructions::load_i1_le>,
		load_i8_le        = base_t::index_of<instructions::load_i8_le>,
		load_i16_le       = base_t::index_of<instructions::load_i16_le>,
		load_i32_le       = base_t::index_of<instructions::load_i32_le>,
		load_i64_le       = base_t::index_of<instructions::load_i64_le>,
		load_f32_le       = base_t::index_of<instructions::load_f32_le>,
		load_f64_le       = base_t::index_of<instructions::load_f64_le>,
		load_ptr32_le     = base_t::index_of<instructions::load_ptr32_le>,
		load_ptr64_le     = base_t::index_of<instructions::load_ptr64_le>,
		store_i1_be       = base_t::index_of<instructions::store_i1_be>,
		store_i8_be       = base_t::index_of<instructions::store_i8_be>,
		store_i16_be      = base_t::index_of<instructions::store_i16_be>,
		store_i32_be      = base_t::index_of<instructions::store_i32_be>,
		store_i64_be      = base_t::index_of<instructions::store_i64_be>,
		store_f32_be      = base_t::index_of<instructions::store_f32_be>,
		store_f64_be      = base_t::index_of<instructions::store_f64_be>,
		store_ptr32_be    = base_t::index_of<instructions::store_ptr32_be>,
		store_ptr64_be    = base_t::index_of<instructions::store_ptr64_be>,
		store_i1_le       = base_t::index_of<instructions::store_i1_le>,
		store_i8_le       = base_t::index_of<instructions::store_i8_le>,
		store_i16_le      = base_t::index_of<instructions::store_i16_le>,
		store_i32_le      = base_t::index_of<instructions::store_i32_le>,
		store_i64_le      = base_t::index_of<instructions::store_i64_le>,
		store_f32_le      = base_t::index_of<instructions::store_f32_le>,
		store_f64_le      = base_t::index_of<instructions::store_f64_le>,
		store_ptr32_le    = base_t::index_of<instructions::store_ptr32_le>,
		store_ptr64_le    = base_t::index_of<instructions::store_ptr64_le>,
		const_gep         = base_t::index_of<instructions::const_gep>,
		const_memcpy      = base_t::index_of<instructions::const_memcpy>,
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
