#include "instructions.h"
#include "ast/statement.h"

namespace comptime
{

bz::u8string to_string(instruction const &inst_)
{
	switch (inst_.index())
	{
	static_assert(instruction::variant_count == 513);
	case instruction::const_i1:
	{
		auto const &inst = inst_.get<instruction::const_i1>();
		return bz::format("const i1 {}", inst.value);
	}
	case instruction::const_i8:
	{
		auto const &inst = inst_.get<instruction::const_i8>();
		return bz::format("const i8 {}", inst.value);
	}
	case instruction::const_i16:
	{
		auto const &inst = inst_.get<instruction::const_i16>();
		return bz::format("const i16 {}", inst.value);
	}
	case instruction::const_i32:
	{
		auto const &inst = inst_.get<instruction::const_i32>();
		return bz::format("const i32 {}", inst.value);
	}
	case instruction::const_i64:
	{
		auto const &inst = inst_.get<instruction::const_i64>();
		return bz::format("const i64 {}", inst.value);
	}
	case instruction::const_u8:
	{
		auto const &inst = inst_.get<instruction::const_u8>();
		return bz::format("const u8 {}", inst.value);
	}
	case instruction::const_u16:
	{
		auto const &inst = inst_.get<instruction::const_u16>();
		return bz::format("const u16 {}", inst.value);
	}
	case instruction::const_u32:
	{
		auto const &inst = inst_.get<instruction::const_u32>();
		return bz::format("const u32 {}", inst.value);
	}
	case instruction::const_u64:
	{
		auto const &inst = inst_.get<instruction::const_u64>();
		return bz::format("const u64 {}", inst.value);
	}
	case instruction::const_f32:
	{
		auto const &inst = inst_.get<instruction::const_f32>();
		return bz::format("const f32 {}", inst.value);
	}
	case instruction::const_f64:
	{
		auto const &inst = inst_.get<instruction::const_f64>();
		return bz::format("const f64 {}", inst.value);
	}
	case instruction::const_ptr_null:
	{
		return "const ptr null";
	}
	case instruction::get_global_address:
	{
		auto const &inst = inst_.get<instruction::get_global_address>();
		return bz::format("get_global_address {}", inst.global_index);
	}
	case instruction::get_function_arg:
	{
		auto const &inst = inst_.get<instruction::get_function_arg>();
		return bz::format("get_function_arg {}", inst.arg_index);
	}
	case instruction::load_i1_be:
	{
		auto const &inst = inst_.get<instruction::load_i1_be>();
		return bz::format("load i1 be {}", inst.args[0]);
	}
	case instruction::load_i8_be:
	{
		auto const &inst = inst_.get<instruction::load_i8_be>();
		return bz::format("load i8 be {}", inst.args[0]);
	}
	case instruction::load_i16_be:
	{
		auto const &inst = inst_.get<instruction::load_i16_be>();
		return bz::format("load i16 be {}", inst.args[0]);
	}
	case instruction::load_i32_be:
	{
		auto const &inst = inst_.get<instruction::load_i32_be>();
		return bz::format("load i32 be {}", inst.args[0]);
	}
	case instruction::load_i64_be:
	{
		auto const &inst = inst_.get<instruction::load_i64_be>();
		return bz::format("load i64 be {}", inst.args[0]);
	}
	case instruction::load_f32_be:
	{
		auto const &inst = inst_.get<instruction::load_f32_be>();
		return bz::format("load f32 be {}", inst.args[0]);
	}
	case instruction::load_f64_be:
	{
		auto const &inst = inst_.get<instruction::load_f64_be>();
		return bz::format("load f64 be {}", inst.args[0]);
	}
	case instruction::load_ptr32_be:
	{
		auto const &inst = inst_.get<instruction::load_ptr32_be>();
		return bz::format("load ptr32 be {}", inst.args[0]);
	}
	case instruction::load_ptr64_be:
	{
		auto const &inst = inst_.get<instruction::load_ptr64_be>();
		return bz::format("load ptr64 be {}", inst.args[0]);
	}
	case instruction::load_i1_le:
	{
		auto const &inst = inst_.get<instruction::load_i1_le>();
		return bz::format("load i1 le {}", inst.args[0]);
	}
	case instruction::load_i8_le:
	{
		auto const &inst = inst_.get<instruction::load_i8_le>();
		return bz::format("load i8 le {}", inst.args[0]);
	}
	case instruction::load_i16_le:
	{
		auto const &inst = inst_.get<instruction::load_i16_le>();
		return bz::format("load i16 le {}", inst.args[0]);
	}
	case instruction::load_i32_le:
	{
		auto const &inst = inst_.get<instruction::load_i32_le>();
		return bz::format("load i32 le {}", inst.args[0]);
	}
	case instruction::load_i64_le:
	{
		auto const &inst = inst_.get<instruction::load_i64_le>();
		return bz::format("load i64 le {}", inst.args[0]);
	}
	case instruction::load_f32_le:
	{
		auto const &inst = inst_.get<instruction::load_f32_le>();
		return bz::format("load f32 le {}", inst.args[0]);
	}
	case instruction::load_f64_le:
	{
		auto const &inst = inst_.get<instruction::load_f64_le>();
		return bz::format("load f64 le {}", inst.args[0]);
	}
	case instruction::load_ptr32_le:
	{
		auto const &inst = inst_.get<instruction::load_ptr32_le>();
		return bz::format("load ptr32 le {}", inst.args[0]);
	}
	case instruction::load_ptr64_le:
	{
		auto const &inst = inst_.get<instruction::load_ptr64_le>();
		return bz::format("load ptr64 le {}", inst.args[0]);
	}
	case instruction::store_i1_be:
	{
		auto const &inst = inst_.get<instruction::store_i1_be>();
		return bz::format("store i1 be {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::store_i8_be:
	{
		auto const &inst = inst_.get<instruction::store_i8_be>();
		return bz::format("store i8 be {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::store_i16_be:
	{
		auto const &inst = inst_.get<instruction::store_i16_be>();
		return bz::format("store i16 be {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::store_i32_be:
	{
		auto const &inst = inst_.get<instruction::store_i32_be>();
		return bz::format("store i32 be {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::store_i64_be:
	{
		auto const &inst = inst_.get<instruction::store_i64_be>();
		return bz::format("store i64 be {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::store_f32_be:
	{
		auto const &inst = inst_.get<instruction::store_f32_be>();
		return bz::format("store f32 be {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::store_f64_be:
	{
		auto const &inst = inst_.get<instruction::store_f64_be>();
		return bz::format("store f64 be {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::store_ptr32_be:
	{
		auto const &inst = inst_.get<instruction::store_ptr32_be>();
		return bz::format("store ptr32 be {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::store_ptr64_be:
	{
		auto const &inst = inst_.get<instruction::store_ptr64_be>();
		return bz::format("store ptr64 be {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::store_i1_le:
	{
		auto const &inst = inst_.get<instruction::store_i1_le>();
		return bz::format("store i1 le {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::store_i8_le:
	{
		auto const &inst = inst_.get<instruction::store_i8_le>();
		return bz::format("store i8 le {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::store_i16_le:
	{
		auto const &inst = inst_.get<instruction::store_i16_le>();
		return bz::format("store i16 le {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::store_i32_le:
	{
		auto const &inst = inst_.get<instruction::store_i32_le>();
		return bz::format("store i32 le {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::store_i64_le:
	{
		auto const &inst = inst_.get<instruction::store_i64_le>();
		return bz::format("store i64 le {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::store_f32_le:
	{
		auto const &inst = inst_.get<instruction::store_f32_le>();
		return bz::format("store f32 le {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::store_f64_le:
	{
		auto const &inst = inst_.get<instruction::store_f64_le>();
		return bz::format("store f64 le {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::store_ptr32_le:
	{
		auto const &inst = inst_.get<instruction::store_ptr32_le>();
		return bz::format("store ptr32 le {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::store_ptr64_le:
	{
		auto const &inst = inst_.get<instruction::store_ptr64_le>();
		return bz::format("store ptr64 le {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::check_dereference:
	{
		auto const &inst = inst_.get<instruction::check_dereference>();
		return bz::format("check_dereference {} ({}, {})", inst.args[0], inst.src_tokens_index, inst.memory_access_check_info_index);
	}
	case instruction::cast_zext_i1_to_i8:
	{
		auto const &inst = inst_.get<instruction::cast_zext_i1_to_i8>();
		return bz::format("zext i1 to i8 {}", inst.args[0]);
	}
	case instruction::cast_zext_i1_to_i16:
	{
		auto const &inst = inst_.get<instruction::cast_zext_i1_to_i16>();
		return bz::format("zext i1 to i16 {}", inst.args[0]);
	}
	case instruction::cast_zext_i1_to_i32:
	{
		auto const &inst = inst_.get<instruction::cast_zext_i1_to_i32>();
		return bz::format("zext i1 to i32 {}", inst.args[0]);
	}
	case instruction::cast_zext_i1_to_i64:
	{
		auto const &inst = inst_.get<instruction::cast_zext_i1_to_i64>();
		return bz::format("zext i1 to i64 {}", inst.args[0]);
	}
	case instruction::cast_zext_i8_to_i16:
	{
		auto const &inst = inst_.get<instruction::cast_zext_i8_to_i16>();
		return bz::format("zext i8 to i16 {}", inst.args[0]);
	}
	case instruction::cast_zext_i8_to_i32:
	{
		auto const &inst = inst_.get<instruction::cast_zext_i8_to_i32>();
		return bz::format("zext i8 to i32 {}", inst.args[0]);
	}
	case instruction::cast_zext_i8_to_i64:
	{
		auto const &inst = inst_.get<instruction::cast_zext_i8_to_i64>();
		return bz::format("zext i8 to i64 {}", inst.args[0]);
	}
	case instruction::cast_zext_i16_to_i32:
	{
		auto const &inst = inst_.get<instruction::cast_zext_i16_to_i32>();
		return bz::format("zext i16 to i32 {}", inst.args[0]);
	}
	case instruction::cast_zext_i16_to_i64:
	{
		auto const &inst = inst_.get<instruction::cast_zext_i16_to_i64>();
		return bz::format("zext i16 to i64 {}", inst.args[0]);
	}
	case instruction::cast_zext_i32_to_i64:
	{
		auto const &inst = inst_.get<instruction::cast_zext_i32_to_i64>();
		return bz::format("zext i32 to i64 {}", inst.args[0]);
	}
	case instruction::cast_sext_i8_to_i16:
	{
		auto const &inst = inst_.get<instruction::cast_sext_i8_to_i16>();
		return bz::format("sext i8 to i16 {}", inst.args[0]);
	}
	case instruction::cast_sext_i8_to_i32:
	{
		auto const &inst = inst_.get<instruction::cast_sext_i8_to_i32>();
		return bz::format("sext i8 to i32 {}", inst.args[0]);
	}
	case instruction::cast_sext_i8_to_i64:
	{
		auto const &inst = inst_.get<instruction::cast_sext_i8_to_i64>();
		return bz::format("sext i8 to i64 {}", inst.args[0]);
	}
	case instruction::cast_sext_i16_to_i32:
	{
		auto const &inst = inst_.get<instruction::cast_sext_i16_to_i32>();
		return bz::format("sext i16 to i32 {}", inst.args[0]);
	}
	case instruction::cast_sext_i16_to_i64:
	{
		auto const &inst = inst_.get<instruction::cast_sext_i16_to_i64>();
		return bz::format("sext i16 to i64 {}", inst.args[0]);
	}
	case instruction::cast_sext_i32_to_i64:
	{
		auto const &inst = inst_.get<instruction::cast_sext_i32_to_i64>();
		return bz::format("sext i32 to i64 {}", inst.args[0]);
	}
	case instruction::cast_trunc_i64_to_i8:
	{
		auto const &inst = inst_.get<instruction::cast_trunc_i64_to_i8>();
		return bz::format("trunc i64 to i8 {}", inst.args[0]);
	}
	case instruction::cast_trunc_i64_to_i16:
	{
		auto const &inst = inst_.get<instruction::cast_trunc_i64_to_i16>();
		return bz::format("trunc i64 to i16 {}", inst.args[0]);
	}
	case instruction::cast_trunc_i64_to_i32:
	{
		auto const &inst = inst_.get<instruction::cast_trunc_i64_to_i32>();
		return bz::format("trunc i64 to i32 {}", inst.args[0]);
	}
	case instruction::cast_trunc_i32_to_i8:
	{
		auto const &inst = inst_.get<instruction::cast_trunc_i32_to_i8>();
		return bz::format("trunc i32 to i8 {}", inst.args[0]);
	}
	case instruction::cast_trunc_i32_to_i16:
	{
		auto const &inst = inst_.get<instruction::cast_trunc_i32_to_i16>();
		return bz::format("trunc i32 to i16 {}", inst.args[0]);
	}
	case instruction::cast_trunc_i16_to_i8:
	{
		auto const &inst = inst_.get<instruction::cast_trunc_i16_to_i8>();
		return bz::format("trunc i16 to i8 {}", inst.args[0]);
	}
	case instruction::cast_f32_to_f64:
	{
		auto const &inst = inst_.get<instruction::cast_f32_to_f64>();
		return bz::format("fp-cast f32 to f64 {}", inst.args[0]);
	}
	case instruction::cast_f64_to_f32:
	{
		auto const &inst = inst_.get<instruction::cast_f64_to_f32>();
		return bz::format("fp-cast f64 to f32 {}", inst.args[0]);
	}
	case instruction::cast_f32_to_i8:
	{
		auto const &inst = inst_.get<instruction::cast_f32_to_i8>();
		return bz::format("fp-to-int f32 to i8 {}", inst.args[0]);
	}
	case instruction::cast_f32_to_i16:
	{
		auto const &inst = inst_.get<instruction::cast_f32_to_i16>();
		return bz::format("fp-to-int f32 to i16 {}", inst.args[0]);
	}
	case instruction::cast_f32_to_i32:
	{
		auto const &inst = inst_.get<instruction::cast_f32_to_i32>();
		return bz::format("fp-to-int f32 to i32 {}", inst.args[0]);
	}
	case instruction::cast_f32_to_i64:
	{
		auto const &inst = inst_.get<instruction::cast_f32_to_i64>();
		return bz::format("fp-to-int f32 to i64 {}", inst.args[0]);
	}
	case instruction::cast_f32_to_u8:
	{
		auto const &inst = inst_.get<instruction::cast_f32_to_u8>();
		return bz::format("fp-to-int f32 to u8 {}", inst.args[0]);
	}
	case instruction::cast_f32_to_u16:
	{
		auto const &inst = inst_.get<instruction::cast_f32_to_u16>();
		return bz::format("fp-to-int f32 to u16 {}", inst.args[0]);
	}
	case instruction::cast_f32_to_u32:
	{
		auto const &inst = inst_.get<instruction::cast_f32_to_u32>();
		return bz::format("fp-to-int f32 to u32 {}", inst.args[0]);
	}
	case instruction::cast_f32_to_u64:
	{
		auto const &inst = inst_.get<instruction::cast_f32_to_u64>();
		return bz::format("fp-to-int f32 to u64 {}", inst.args[0]);
	}
	case instruction::cast_f64_to_i8:
	{
		auto const &inst = inst_.get<instruction::cast_f64_to_i8>();
		return bz::format("fp-to-int f64 to i8 {}", inst.args[0]);
	}
	case instruction::cast_f64_to_i16:
	{
		auto const &inst = inst_.get<instruction::cast_f64_to_i16>();
		return bz::format("fp-to-int f64 to i16 {}", inst.args[0]);
	}
	case instruction::cast_f64_to_i32:
	{
		auto const &inst = inst_.get<instruction::cast_f64_to_i32>();
		return bz::format("fp-to-int f64 to i32 {}", inst.args[0]);
	}
	case instruction::cast_f64_to_i64:
	{
		auto const &inst = inst_.get<instruction::cast_f64_to_i64>();
		return bz::format("fp-to-int f64 to i64 {}", inst.args[0]);
	}
	case instruction::cast_f64_to_u8:
	{
		auto const &inst = inst_.get<instruction::cast_f64_to_u8>();
		return bz::format("fp-to-int f64 to u8 {}", inst.args[0]);
	}
	case instruction::cast_f64_to_u16:
	{
		auto const &inst = inst_.get<instruction::cast_f64_to_u16>();
		return bz::format("fp-to-int f64 to u16 {}", inst.args[0]);
	}
	case instruction::cast_f64_to_u32:
	{
		auto const &inst = inst_.get<instruction::cast_f64_to_u32>();
		return bz::format("fp-to-int f64 to u32 {}", inst.args[0]);
	}
	case instruction::cast_f64_to_u64:
	{
		auto const &inst = inst_.get<instruction::cast_f64_to_u64>();
		return bz::format("fp-to-int f64 to u64 {}", inst.args[0]);
	}
	case instruction::cast_i8_to_f32:
	{
		auto const &inst = inst_.get<instruction::cast_i8_to_f32>();
		return bz::format("int-to-fp i8 to f32 {}", inst.args[0]);
	}
	case instruction::cast_i16_to_f32:
	{
		auto const &inst = inst_.get<instruction::cast_i16_to_f32>();
		return bz::format("int-to-fp i16 to f32 {}", inst.args[0]);
	}
	case instruction::cast_i32_to_f32:
	{
		auto const &inst = inst_.get<instruction::cast_i32_to_f32>();
		return bz::format("int-to-fp i32 to f32 {}", inst.args[0]);
	}
	case instruction::cast_i64_to_f32:
	{
		auto const &inst = inst_.get<instruction::cast_i64_to_f32>();
		return bz::format("int-to-fp i64 to f32 {}", inst.args[0]);
	}
	case instruction::cast_u8_to_f32:
	{
		auto const &inst = inst_.get<instruction::cast_u8_to_f32>();
		return bz::format("int-to-fp u8 to f32 {}", inst.args[0]);
	}
	case instruction::cast_u16_to_f32:
	{
		auto const &inst = inst_.get<instruction::cast_u16_to_f32>();
		return bz::format("int-to-fp u16 to f32 {}", inst.args[0]);
	}
	case instruction::cast_u32_to_f32:
	{
		auto const &inst = inst_.get<instruction::cast_u32_to_f32>();
		return bz::format("int-to-fp u32 to f32 {}", inst.args[0]);
	}
	case instruction::cast_u64_to_f32:
	{
		auto const &inst = inst_.get<instruction::cast_u64_to_f32>();
		return bz::format("int-to-fp u64 to f32 {}", inst.args[0]);
	}
	case instruction::cast_i8_to_f64:
	{
		auto const &inst = inst_.get<instruction::cast_i8_to_f64>();
		return bz::format("int-to-fp i8 to f64 {}", inst.args[0]);
	}
	case instruction::cast_i16_to_f64:
	{
		auto const &inst = inst_.get<instruction::cast_i16_to_f64>();
		return bz::format("int-to-fp i16 to f64 {}", inst.args[0]);
	}
	case instruction::cast_i32_to_f64:
	{
		auto const &inst = inst_.get<instruction::cast_i32_to_f64>();
		return bz::format("int-to-fp i32 to f64 {}", inst.args[0]);
	}
	case instruction::cast_i64_to_f64:
	{
		auto const &inst = inst_.get<instruction::cast_i64_to_f64>();
		return bz::format("int-to-fp i64 to f64 {}", inst.args[0]);
	}
	case instruction::cast_u8_to_f64:
	{
		auto const &inst = inst_.get<instruction::cast_u8_to_f64>();
		return bz::format("int-to-fp u8 to f64 {}", inst.args[0]);
	}
	case instruction::cast_u16_to_f64:
	{
		auto const &inst = inst_.get<instruction::cast_u16_to_f64>();
		return bz::format("int-to-fp u16 to f64 {}", inst.args[0]);
	}
	case instruction::cast_u32_to_f64:
	{
		auto const &inst = inst_.get<instruction::cast_u32_to_f64>();
		return bz::format("int-to-fp u32 to f64 {}", inst.args[0]);
	}
	case instruction::cast_u64_to_f64:
	{
		auto const &inst = inst_.get<instruction::cast_u64_to_f64>();
		return bz::format("int-to-fp u64 to f64 {}", inst.args[0]);
	}
	case instruction::cmp_eq_i1:
	{
		auto const &inst = inst_.get<instruction::cmp_eq_i1>();
		return bz::format("cmp eq i1 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_eq_i8:
	{
		auto const &inst = inst_.get<instruction::cmp_eq_i8>();
		return bz::format("cmp eq i8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_eq_i16:
	{
		auto const &inst = inst_.get<instruction::cmp_eq_i16>();
		return bz::format("cmp eq i16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_eq_i32:
	{
		auto const &inst = inst_.get<instruction::cmp_eq_i32>();
		return bz::format("cmp eq i32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_eq_i64:
	{
		auto const &inst = inst_.get<instruction::cmp_eq_i64>();
		return bz::format("cmp eq i64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_eq_f32:
	{
		auto const &inst = inst_.get<instruction::cmp_eq_f32>();
		return bz::format("cmp eq f32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_eq_f64:
	{
		auto const &inst = inst_.get<instruction::cmp_eq_f64>();
		return bz::format("cmp eq f64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_eq_f32_check:
	{
		auto const &inst = inst_.get<instruction::cmp_eq_f32_check>();
		return bz::format("cmp eq f32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::cmp_eq_f64_check:
	{
		auto const &inst = inst_.get<instruction::cmp_eq_f64_check>();
		return bz::format("cmp eq f64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::cmp_eq_ptr:
	{
		auto const &inst = inst_.get<instruction::cmp_eq_ptr>();
		return bz::format("cmp eq ptr {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_neq_i1:
	{
		auto const &inst = inst_.get<instruction::cmp_neq_i1>();
		return bz::format("cmp neq i1 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_neq_i8:
	{
		auto const &inst = inst_.get<instruction::cmp_neq_i8>();
		return bz::format("cmp neq i8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_neq_i16:
	{
		auto const &inst = inst_.get<instruction::cmp_neq_i16>();
		return bz::format("cmp neq i16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_neq_i32:
	{
		auto const &inst = inst_.get<instruction::cmp_neq_i32>();
		return bz::format("cmp neq i32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_neq_i64:
	{
		auto const &inst = inst_.get<instruction::cmp_neq_i64>();
		return bz::format("cmp neq i64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_neq_f32:
	{
		auto const &inst = inst_.get<instruction::cmp_neq_f32>();
		return bz::format("cmp neq f32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_neq_f64:
	{
		auto const &inst = inst_.get<instruction::cmp_neq_f64>();
		return bz::format("cmp neq f64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_neq_f32_check:
	{
		auto const &inst = inst_.get<instruction::cmp_neq_f32_check>();
		return bz::format("cmp neq f32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::cmp_neq_f64_check:
	{
		auto const &inst = inst_.get<instruction::cmp_neq_f64_check>();
		return bz::format("cmp neq f64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::cmp_neq_ptr:
	{
		auto const &inst = inst_.get<instruction::cmp_neq_ptr>();
		return bz::format("cmp neq ptr {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lt_i8:
	{
		auto const &inst = inst_.get<instruction::cmp_lt_i8>();
		return bz::format("cmp lt i8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lt_i16:
	{
		auto const &inst = inst_.get<instruction::cmp_lt_i16>();
		return bz::format("cmp lt i16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lt_i32:
	{
		auto const &inst = inst_.get<instruction::cmp_lt_i32>();
		return bz::format("cmp lt i32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lt_i64:
	{
		auto const &inst = inst_.get<instruction::cmp_lt_i64>();
		return bz::format("cmp lt i64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lt_u8:
	{
		auto const &inst = inst_.get<instruction::cmp_lt_u8>();
		return bz::format("cmp lt u8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lt_u16:
	{
		auto const &inst = inst_.get<instruction::cmp_lt_u16>();
		return bz::format("cmp lt u16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lt_u32:
	{
		auto const &inst = inst_.get<instruction::cmp_lt_u32>();
		return bz::format("cmp lt u32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lt_u64:
	{
		auto const &inst = inst_.get<instruction::cmp_lt_u64>();
		return bz::format("cmp lt u64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lt_f32:
	{
		auto const &inst = inst_.get<instruction::cmp_lt_f32>();
		return bz::format("cmp lt f32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lt_f64:
	{
		auto const &inst = inst_.get<instruction::cmp_lt_f64>();
		return bz::format("cmp lt f64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lt_f32_check:
	{
		auto const &inst = inst_.get<instruction::cmp_lt_f32_check>();
		return bz::format("cmp lt f32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::cmp_lt_f64_check:
	{
		auto const &inst = inst_.get<instruction::cmp_lt_f64_check>();
		return bz::format("cmp lt f64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::cmp_lt_ptr:
	{
		auto const &inst = inst_.get<instruction::cmp_lt_ptr>();
		return bz::format("cmp lt ptr {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::cmp_gt_i8:
	{
		auto const &inst = inst_.get<instruction::cmp_gt_i8>();
		return bz::format("cmp gt i8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gt_i16:
	{
		auto const &inst = inst_.get<instruction::cmp_gt_i16>();
		return bz::format("cmp gt i16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gt_i32:
	{
		auto const &inst = inst_.get<instruction::cmp_gt_i32>();
		return bz::format("cmp gt i32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gt_i64:
	{
		auto const &inst = inst_.get<instruction::cmp_gt_i64>();
		return bz::format("cmp gt i64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gt_u8:
	{
		auto const &inst = inst_.get<instruction::cmp_gt_u8>();
		return bz::format("cmp gt u8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gt_u16:
	{
		auto const &inst = inst_.get<instruction::cmp_gt_u16>();
		return bz::format("cmp gt u16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gt_u32:
	{
		auto const &inst = inst_.get<instruction::cmp_gt_u32>();
		return bz::format("cmp gt u32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gt_u64:
	{
		auto const &inst = inst_.get<instruction::cmp_gt_u64>();
		return bz::format("cmp gt u64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gt_f32:
	{
		auto const &inst = inst_.get<instruction::cmp_gt_f32>();
		return bz::format("cmp gt f32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gt_f64:
	{
		auto const &inst = inst_.get<instruction::cmp_gt_f64>();
		return bz::format("cmp gt f64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gt_f32_check:
	{
		auto const &inst = inst_.get<instruction::cmp_gt_f32_check>();
		return bz::format("cmp gt f32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::cmp_gt_f64_check:
	{
		auto const &inst = inst_.get<instruction::cmp_gt_f64_check>();
		return bz::format("cmp gt f64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::cmp_gt_ptr:
	{
		auto const &inst = inst_.get<instruction::cmp_gt_ptr>();
		return bz::format("cmp gt ptr {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::cmp_lte_i8:
	{
		auto const &inst = inst_.get<instruction::cmp_lte_i8>();
		return bz::format("cmp lte i8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lte_i16:
	{
		auto const &inst = inst_.get<instruction::cmp_lte_i16>();
		return bz::format("cmp lte i16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lte_i32:
	{
		auto const &inst = inst_.get<instruction::cmp_lte_i32>();
		return bz::format("cmp lte i32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lte_i64:
	{
		auto const &inst = inst_.get<instruction::cmp_lte_i64>();
		return bz::format("cmp lte i64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lte_u8:
	{
		auto const &inst = inst_.get<instruction::cmp_lte_u8>();
		return bz::format("cmp lte u8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lte_u16:
	{
		auto const &inst = inst_.get<instruction::cmp_lte_u16>();
		return bz::format("cmp lte u16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lte_u32:
	{
		auto const &inst = inst_.get<instruction::cmp_lte_u32>();
		return bz::format("cmp lte u32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lte_u64:
	{
		auto const &inst = inst_.get<instruction::cmp_lte_u64>();
		return bz::format("cmp lte u64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lte_f32:
	{
		auto const &inst = inst_.get<instruction::cmp_lte_f32>();
		return bz::format("cmp lte f32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lte_f64:
	{
		auto const &inst = inst_.get<instruction::cmp_lte_f64>();
		return bz::format("cmp lte f64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lte_f32_check:
	{
		auto const &inst = inst_.get<instruction::cmp_lte_f32_check>();
		return bz::format("cmp lte f32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::cmp_lte_f64_check:
	{
		auto const &inst = inst_.get<instruction::cmp_lte_f64_check>();
		return bz::format("cmp lte f64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::cmp_lte_ptr:
	{
		auto const &inst = inst_.get<instruction::cmp_lte_ptr>();
		return bz::format("cmp lte ptr {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::cmp_gte_i8:
	{
		auto const &inst = inst_.get<instruction::cmp_gte_i8>();
		return bz::format("cmp gte i8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gte_i16:
	{
		auto const &inst = inst_.get<instruction::cmp_gte_i16>();
		return bz::format("cmp gte i16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gte_i32:
	{
		auto const &inst = inst_.get<instruction::cmp_gte_i32>();
		return bz::format("cmp gte i32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gte_i64:
	{
		auto const &inst = inst_.get<instruction::cmp_gte_i64>();
		return bz::format("cmp gte i64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gte_u8:
	{
		auto const &inst = inst_.get<instruction::cmp_gte_u8>();
		return bz::format("cmp gte u8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gte_u16:
	{
		auto const &inst = inst_.get<instruction::cmp_gte_u16>();
		return bz::format("cmp gte u16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gte_u32:
	{
		auto const &inst = inst_.get<instruction::cmp_gte_u32>();
		return bz::format("cmp gte u32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gte_u64:
	{
		auto const &inst = inst_.get<instruction::cmp_gte_u64>();
		return bz::format("cmp gte u64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gte_f32:
	{
		auto const &inst = inst_.get<instruction::cmp_gte_f32>();
		return bz::format("cmp gte f32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gte_f64:
	{
		auto const &inst = inst_.get<instruction::cmp_gte_f64>();
		return bz::format("cmp gte f64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gte_f32_check:
	{
		auto const &inst = inst_.get<instruction::cmp_gte_f32_check>();
		return bz::format("cmp gte f32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::cmp_gte_f64_check:
	{
		auto const &inst = inst_.get<instruction::cmp_gte_f64_check>();
		return bz::format("cmp gte f64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::cmp_gte_ptr:
	{
		auto const &inst = inst_.get<instruction::cmp_gte_ptr>();
		return bz::format("cmp gte ptr {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::neg_i8:
	{
		auto const &inst = inst_.get<instruction::neg_i8>();
		return bz::format("neg i8 {}", inst.args[0]);
	}
	case instruction::neg_i16:
	{
		auto const &inst = inst_.get<instruction::neg_i16>();
		return bz::format("neg i16 {}", inst.args[0]);
	}
	case instruction::neg_i32:
	{
		auto const &inst = inst_.get<instruction::neg_i32>();
		return bz::format("neg i32 {}", inst.args[0]);
	}
	case instruction::neg_i64:
	{
		auto const &inst = inst_.get<instruction::neg_i64>();
		return bz::format("neg i64 {}", inst.args[0]);
	}
	case instruction::neg_f32:
	{
		auto const &inst = inst_.get<instruction::neg_f32>();
		return bz::format("neg f32 {}", inst.args[0]);
	}
	case instruction::neg_f64:
	{
		auto const &inst = inst_.get<instruction::neg_f64>();
		return bz::format("neg f64 {}", inst.args[0]);
	}
	case instruction::neg_i8_check:
	{
		auto const &inst = inst_.get<instruction::neg_i8_check>();
		return bz::format("neg i8 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::neg_i16_check:
	{
		auto const &inst = inst_.get<instruction::neg_i16_check>();
		return bz::format("neg i16 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::neg_i32_check:
	{
		auto const &inst = inst_.get<instruction::neg_i32_check>();
		return bz::format("neg i32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::neg_i64_check:
	{
		auto const &inst = inst_.get<instruction::neg_i64_check>();
		return bz::format("neg i64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::add_i8:
	{
		auto const &inst = inst_.get<instruction::add_i8>();
		return bz::format("add i8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::add_i16:
	{
		auto const &inst = inst_.get<instruction::add_i16>();
		return bz::format("add i16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::add_i32:
	{
		auto const &inst = inst_.get<instruction::add_i32>();
		return bz::format("add i32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::add_i64:
	{
		auto const &inst = inst_.get<instruction::add_i64>();
		return bz::format("add i64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::add_f32:
	{
		auto const &inst = inst_.get<instruction::add_f32>();
		return bz::format("add f32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::add_f64:
	{
		auto const &inst = inst_.get<instruction::add_f64>();
		return bz::format("add f64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::add_ptr_i32:
	{
		auto const &inst = inst_.get<instruction::add_ptr_i32>();
		return bz::format("add ptr i32 {}, {} ({}, {})", inst.args[0], inst.args[1], inst.src_tokens_index, inst.pointer_arithmetic_check_info_index);
	}
	case instruction::add_ptr_u32:
	{
		auto const &inst = inst_.get<instruction::add_ptr_u32>();
		return bz::format("add ptr u32 {}, {} ({}, {})", inst.args[0], inst.args[1], inst.src_tokens_index, inst.pointer_arithmetic_check_info_index);
	}
	case instruction::add_ptr_i64:
	{
		auto const &inst = inst_.get<instruction::add_ptr_i64>();
		return bz::format("add ptr i64 {}, {} ({}, {})", inst.args[0], inst.args[1], inst.src_tokens_index, inst.pointer_arithmetic_check_info_index);
	}
	case instruction::add_ptr_u64:
	{
		auto const &inst = inst_.get<instruction::add_ptr_u64>();
		return bz::format("add ptr u64 {}, {} ({}, {})", inst.args[0], inst.args[1], inst.src_tokens_index, inst.pointer_arithmetic_check_info_index);
	}
	case instruction::add_ptr_const_unchecked:
	{
		auto const &inst = inst_.get<instruction::add_ptr_const_unchecked>();
		return bz::format("add ptr {} const {} {}", inst.object_type->to_string(), inst.amount, inst.args[0]);
	}
	case instruction::add_i8_check:
	{
		auto const &inst = inst_.get<instruction::add_i8_check>();
		return bz::format("add i8 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::add_i16_check:
	{
		auto const &inst = inst_.get<instruction::add_i16_check>();
		return bz::format("add i16 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::add_i32_check:
	{
		auto const &inst = inst_.get<instruction::add_i32_check>();
		return bz::format("add i32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::add_i64_check:
	{
		auto const &inst = inst_.get<instruction::add_i64_check>();
		return bz::format("add i64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::add_u8_check:
	{
		auto const &inst = inst_.get<instruction::add_u8_check>();
		return bz::format("add u8 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::add_u16_check:
	{
		auto const &inst = inst_.get<instruction::add_u16_check>();
		return bz::format("add u16 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::add_u32_check:
	{
		auto const &inst = inst_.get<instruction::add_u32_check>();
		return bz::format("add u32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::add_u64_check:
	{
		auto const &inst = inst_.get<instruction::add_u64_check>();
		return bz::format("add u64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::add_f32_check:
	{
		auto const &inst = inst_.get<instruction::add_f32_check>();
		return bz::format("add f32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::add_f64_check:
	{
		auto const &inst = inst_.get<instruction::add_f64_check>();
		return bz::format("add f64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::sub_i8:
	{
		auto const &inst = inst_.get<instruction::sub_i8>();
		return bz::format("sub i8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::sub_i16:
	{
		auto const &inst = inst_.get<instruction::sub_i16>();
		return bz::format("sub i16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::sub_i32:
	{
		auto const &inst = inst_.get<instruction::sub_i32>();
		return bz::format("sub i32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::sub_i64:
	{
		auto const &inst = inst_.get<instruction::sub_i64>();
		return bz::format("sub i64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::sub_f32:
	{
		auto const &inst = inst_.get<instruction::sub_f32>();
		return bz::format("sub f32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::sub_f64:
	{
		auto const &inst = inst_.get<instruction::sub_f64>();
		return bz::format("sub f64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::sub_ptr_i32:
	{
		auto const &inst = inst_.get<instruction::sub_ptr_i32>();
		return bz::format("sub ptr i32 {}, {} ({}, {})", inst.args[0], inst.args[1], inst.src_tokens_index, inst.pointer_arithmetic_check_info_index);
	}
	case instruction::sub_ptr_u32:
	{
		auto const &inst = inst_.get<instruction::sub_ptr_u32>();
		return bz::format("sub ptr u32 {}, {} ({}, {})", inst.args[0], inst.args[1], inst.src_tokens_index, inst.pointer_arithmetic_check_info_index);
	}
	case instruction::sub_ptr_i64:
	{
		auto const &inst = inst_.get<instruction::sub_ptr_i64>();
		return bz::format("sub ptr i64 {}, {} ({}, {})", inst.args[0], inst.args[1], inst.src_tokens_index, inst.pointer_arithmetic_check_info_index);
	}
	case instruction::sub_ptr_u64:
	{
		auto const &inst = inst_.get<instruction::sub_ptr_u64>();
		return bz::format("sub ptr u64 {}, {} ({}, {})", inst.args[0], inst.args[1], inst.src_tokens_index, inst.pointer_arithmetic_check_info_index);
	}
	case instruction::sub_i8_check:
	{
		auto const &inst = inst_.get<instruction::sub_i8_check>();
		return bz::format("sub i8 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::sub_i16_check:
	{
		auto const &inst = inst_.get<instruction::sub_i16_check>();
		return bz::format("sub i16 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::sub_i32_check:
	{
		auto const &inst = inst_.get<instruction::sub_i32_check>();
		return bz::format("sub i32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::sub_i64_check:
	{
		auto const &inst = inst_.get<instruction::sub_i64_check>();
		return bz::format("sub i64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::sub_u8_check:
	{
		auto const &inst = inst_.get<instruction::sub_u8_check>();
		return bz::format("sub u8 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::sub_u16_check:
	{
		auto const &inst = inst_.get<instruction::sub_u16_check>();
		return bz::format("sub u16 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::sub_u32_check:
	{
		auto const &inst = inst_.get<instruction::sub_u32_check>();
		return bz::format("sub u32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::sub_u64_check:
	{
		auto const &inst = inst_.get<instruction::sub_u64_check>();
		return bz::format("sub u64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::sub_f32_check:
	{
		auto const &inst = inst_.get<instruction::sub_f32_check>();
		return bz::format("sub f32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::sub_f64_check:
	{
		auto const &inst = inst_.get<instruction::sub_f64_check>();
		return bz::format("sub f64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::ptr32_diff:
	{
		auto const &inst = inst_.get<instruction::ptr32_diff>();
		return bz::format("ptr32-diff {}, {} ({}, {})", inst.args[0], inst.args[1], inst.src_tokens_index, inst.pointer_arithmetic_check_info_index);
	}
	case instruction::ptr64_diff:
	{
		auto const &inst = inst_.get<instruction::ptr64_diff>();
		return bz::format("ptr64-diff {}, {} ({}, {})", inst.args[0], inst.args[1], inst.src_tokens_index, inst.pointer_arithmetic_check_info_index);
	}
	case instruction::ptr32_diff_unchecked:
	{
		auto const &inst = inst_.get<instruction::ptr32_diff_unchecked>();
		return bz::format("ptr32-diff {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::ptr64_diff_unchecked:
	{
		auto const &inst = inst_.get<instruction::ptr64_diff_unchecked>();
		return bz::format("ptr64-diff {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::mul_i8:
	{
		auto const &inst = inst_.get<instruction::mul_i8>();
		return bz::format("mul i8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::mul_i16:
	{
		auto const &inst = inst_.get<instruction::mul_i16>();
		return bz::format("mul i16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::mul_i32:
	{
		auto const &inst = inst_.get<instruction::mul_i32>();
		return bz::format("mul i32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::mul_i64:
	{
		auto const &inst = inst_.get<instruction::mul_i64>();
		return bz::format("mul i64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::mul_f32:
	{
		auto const &inst = inst_.get<instruction::mul_f32>();
		return bz::format("mul f32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::mul_f64:
	{
		auto const &inst = inst_.get<instruction::mul_f64>();
		return bz::format("mul f64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::mul_i8_check:
	{
		auto const &inst = inst_.get<instruction::mul_i8_check>();
		return bz::format("mul i8 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::mul_i16_check:
	{
		auto const &inst = inst_.get<instruction::mul_i16_check>();
		return bz::format("mul i16 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::mul_i32_check:
	{
		auto const &inst = inst_.get<instruction::mul_i32_check>();
		return bz::format("mul i32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::mul_i64_check:
	{
		auto const &inst = inst_.get<instruction::mul_i64_check>();
		return bz::format("mul i64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::mul_u8_check:
	{
		auto const &inst = inst_.get<instruction::mul_u8_check>();
		return bz::format("mul u8 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::mul_u16_check:
	{
		auto const &inst = inst_.get<instruction::mul_u16_check>();
		return bz::format("mul u16 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::mul_u32_check:
	{
		auto const &inst = inst_.get<instruction::mul_u32_check>();
		return bz::format("mul u32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::mul_u64_check:
	{
		auto const &inst = inst_.get<instruction::mul_u64_check>();
		return bz::format("mul u64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::mul_f32_check:
	{
		auto const &inst = inst_.get<instruction::mul_f32_check>();
		return bz::format("mul f32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::mul_f64_check:
	{
		auto const &inst = inst_.get<instruction::mul_f64_check>();
		return bz::format("mul f64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::div_i8:
	{
		auto const &inst = inst_.get<instruction::div_i8>();
		return bz::format("div i8 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::div_i16:
	{
		auto const &inst = inst_.get<instruction::div_i16>();
		return bz::format("div i16 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::div_i32:
	{
		auto const &inst = inst_.get<instruction::div_i32>();
		return bz::format("div i32 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::div_i64:
	{
		auto const &inst = inst_.get<instruction::div_i64>();
		return bz::format("div i64 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::div_u8:
	{
		auto const &inst = inst_.get<instruction::div_u8>();
		return bz::format("div u8 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::div_u16:
	{
		auto const &inst = inst_.get<instruction::div_u16>();
		return bz::format("div u16 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::div_u32:
	{
		auto const &inst = inst_.get<instruction::div_u32>();
		return bz::format("div u32 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::div_u64:
	{
		auto const &inst = inst_.get<instruction::div_u64>();
		return bz::format("div u64 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::div_f32:
	{
		auto const &inst = inst_.get<instruction::div_f32>();
		return bz::format("div f32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::div_f64:
	{
		auto const &inst = inst_.get<instruction::div_f64>();
		return bz::format("div f64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::div_i8_check:
	{
		auto const &inst = inst_.get<instruction::div_i8_check>();
		return bz::format("div i8 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::div_i16_check:
	{
		auto const &inst = inst_.get<instruction::div_i16_check>();
		return bz::format("div i16 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::div_i32_check:
	{
		auto const &inst = inst_.get<instruction::div_i32_check>();
		return bz::format("div i32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::div_i64_check:
	{
		auto const &inst = inst_.get<instruction::div_i64_check>();
		return bz::format("div i64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::div_f32_check:
	{
		auto const &inst = inst_.get<instruction::div_f32_check>();
		return bz::format("div f32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::div_f64_check:
	{
		auto const &inst = inst_.get<instruction::div_f64_check>();
		return bz::format("div f64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::rem_i8:
	{
		auto const &inst = inst_.get<instruction::rem_i8>();
		return bz::format("rem i8 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::rem_i16:
	{
		auto const &inst = inst_.get<instruction::rem_i16>();
		return bz::format("rem i16 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::rem_i32:
	{
		auto const &inst = inst_.get<instruction::rem_i32>();
		return bz::format("rem i32 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::rem_i64:
	{
		auto const &inst = inst_.get<instruction::rem_i64>();
		return bz::format("rem i64 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::rem_u8:
	{
		auto const &inst = inst_.get<instruction::rem_u8>();
		return bz::format("rem u8 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::rem_u16:
	{
		auto const &inst = inst_.get<instruction::rem_u16>();
		return bz::format("rem u16 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::rem_u32:
	{
		auto const &inst = inst_.get<instruction::rem_u32>();
		return bz::format("rem u32 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::rem_u64:
	{
		auto const &inst = inst_.get<instruction::rem_u64>();
		return bz::format("rem u64 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::not_i1:
	{
		auto const &inst = inst_.get<instruction::not_i1>();
		return bz::format("not i1 {}", inst.args[0]);
	}
	case instruction::not_i8:
	{
		auto const &inst = inst_.get<instruction::not_i8>();
		return bz::format("not i8 {}", inst.args[0]);
	}
	case instruction::not_i16:
	{
		auto const &inst = inst_.get<instruction::not_i16>();
		return bz::format("not i16 {}", inst.args[0]);
	}
	case instruction::not_i32:
	{
		auto const &inst = inst_.get<instruction::not_i32>();
		return bz::format("not i32 {}", inst.args[0]);
	}
	case instruction::not_i64:
	{
		auto const &inst = inst_.get<instruction::not_i64>();
		return bz::format("not i64 {}", inst.args[0]);
	}
	case instruction::and_i1:
	{
		auto const &inst = inst_.get<instruction::and_i1>();
		return bz::format("and i1 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::and_i8:
	{
		auto const &inst = inst_.get<instruction::and_i8>();
		return bz::format("and i8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::and_i16:
	{
		auto const &inst = inst_.get<instruction::and_i16>();
		return bz::format("and i16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::and_i32:
	{
		auto const &inst = inst_.get<instruction::and_i32>();
		return bz::format("and i32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::and_i64:
	{
		auto const &inst = inst_.get<instruction::and_i64>();
		return bz::format("and i64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::xor_i1:
	{
		auto const &inst = inst_.get<instruction::xor_i1>();
		return bz::format("xor i1 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::xor_i8:
	{
		auto const &inst = inst_.get<instruction::xor_i8>();
		return bz::format("xor i8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::xor_i16:
	{
		auto const &inst = inst_.get<instruction::xor_i16>();
		return bz::format("xor i16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::xor_i32:
	{
		auto const &inst = inst_.get<instruction::xor_i32>();
		return bz::format("xor i32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::xor_i64:
	{
		auto const &inst = inst_.get<instruction::xor_i64>();
		return bz::format("xor i64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::or_i1:
	{
		auto const &inst = inst_.get<instruction::or_i1>();
		return bz::format("or i1 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::or_i8:
	{
		auto const &inst = inst_.get<instruction::or_i8>();
		return bz::format("or i8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::or_i16:
	{
		auto const &inst = inst_.get<instruction::or_i16>();
		return bz::format("or i16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::or_i32:
	{
		auto const &inst = inst_.get<instruction::or_i32>();
		return bz::format("or i32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::or_i64:
	{
		auto const &inst = inst_.get<instruction::or_i64>();
		return bz::format("or i64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::shl_i8_signed:
	{
		auto const &inst = inst_.get<instruction::shl_i8_signed>();
		return bz::format("shl i8 signed {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::shl_i16_signed:
	{
		auto const &inst = inst_.get<instruction::shl_i16_signed>();
		return bz::format("shl i16 signed {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::shl_i32_signed:
	{
		auto const &inst = inst_.get<instruction::shl_i32_signed>();
		return bz::format("shl i32 signed {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::shl_i64_signed:
	{
		auto const &inst = inst_.get<instruction::shl_i64_signed>();
		return bz::format("shl i64 signed {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::shl_i8_unsigned:
	{
		auto const &inst = inst_.get<instruction::shl_i8_unsigned>();
		return bz::format("shl i8 unsigned {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::shl_i16_unsigned:
	{
		auto const &inst = inst_.get<instruction::shl_i16_unsigned>();
		return bz::format("shl i16 unsigned {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::shl_i32_unsigned:
	{
		auto const &inst = inst_.get<instruction::shl_i32_unsigned>();
		return bz::format("shl i32 unsigned {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::shl_i64_unsigned:
	{
		auto const &inst = inst_.get<instruction::shl_i64_unsigned>();
		return bz::format("shl i64 unsigned {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::shr_i8_signed:
	{
		auto const &inst = inst_.get<instruction::shr_i8_signed>();
		return bz::format("shr i8 signed {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::shr_i16_signed:
	{
		auto const &inst = inst_.get<instruction::shr_i16_signed>();
		return bz::format("shr i16 signed {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::shr_i32_signed:
	{
		auto const &inst = inst_.get<instruction::shr_i32_signed>();
		return bz::format("shr i32 signed {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::shr_i64_signed:
	{
		auto const &inst = inst_.get<instruction::shr_i64_signed>();
		return bz::format("shr i64 signed {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::shr_i8_unsigned:
	{
		auto const &inst = inst_.get<instruction::shr_i8_unsigned>();
		return bz::format("shr i8 unsigned {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::shr_i16_unsigned:
	{
		auto const &inst = inst_.get<instruction::shr_i16_unsigned>();
		return bz::format("shr i16 unsigned {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::shr_i32_unsigned:
	{
		auto const &inst = inst_.get<instruction::shr_i32_unsigned>();
		return bz::format("shr i32 unsigned {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::shr_i64_unsigned:
	{
		auto const &inst = inst_.get<instruction::shr_i64_unsigned>();
		return bz::format("shr i64 unsigned {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::abs_i8:
	{
		auto const &inst = inst_.get<instruction::abs_i8>();
		return bz::format("abs i8 {}", inst.args[0]);
	}
	case instruction::abs_i16:
	{
		auto const &inst = inst_.get<instruction::abs_i16>();
		return bz::format("abs i16 {}", inst.args[0]);
	}
	case instruction::abs_i32:
	{
		auto const &inst = inst_.get<instruction::abs_i32>();
		return bz::format("abs i32 {}", inst.args[0]);
	}
	case instruction::abs_i64:
	{
		auto const &inst = inst_.get<instruction::abs_i64>();
		return bz::format("abs i64 {}", inst.args[0]);
	}
	case instruction::abs_f32:
	{
		auto const &inst = inst_.get<instruction::abs_f32>();
		return bz::format("abs f32 {}", inst.args[0]);
	}
	case instruction::abs_f64:
	{
		auto const &inst = inst_.get<instruction::abs_f64>();
		return bz::format("abs f64 {}", inst.args[0]);
	}
	case instruction::abs_i8_check:
	{
		auto const &inst = inst_.get<instruction::abs_i8_check>();
		return bz::format("abs i8 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::abs_i16_check:
	{
		auto const &inst = inst_.get<instruction::abs_i16_check>();
		return bz::format("abs i16 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::abs_i32_check:
	{
		auto const &inst = inst_.get<instruction::abs_i32_check>();
		return bz::format("abs i32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::abs_i64_check:
	{
		auto const &inst = inst_.get<instruction::abs_i64_check>();
		return bz::format("abs i64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::abs_f32_check:
	{
		auto const &inst = inst_.get<instruction::abs_f32_check>();
		return bz::format("abs f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::abs_f64_check:
	{
		auto const &inst = inst_.get<instruction::abs_f64_check>();
		return bz::format("abs f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::min_i8:
	{
		auto const &inst = inst_.get<instruction::min_i8>();
		return bz::format("min i8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::min_i16:
	{
		auto const &inst = inst_.get<instruction::min_i16>();
		return bz::format("min i16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::min_i32:
	{
		auto const &inst = inst_.get<instruction::min_i32>();
		return bz::format("min i32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::min_i64:
	{
		auto const &inst = inst_.get<instruction::min_i64>();
		return bz::format("min i64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::min_u8:
	{
		auto const &inst = inst_.get<instruction::min_u8>();
		return bz::format("min u8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::min_u16:
	{
		auto const &inst = inst_.get<instruction::min_u16>();
		return bz::format("min u16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::min_u32:
	{
		auto const &inst = inst_.get<instruction::min_u32>();
		return bz::format("min u32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::min_u64:
	{
		auto const &inst = inst_.get<instruction::min_u64>();
		return bz::format("min u64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::min_f32:
	{
		auto const &inst = inst_.get<instruction::min_f32>();
		return bz::format("min f32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::min_f64:
	{
		auto const &inst = inst_.get<instruction::min_f64>();
		return bz::format("min f64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::min_f32_check:
	{
		auto const &inst = inst_.get<instruction::min_f32_check>();
		return bz::format("min f32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::min_f64_check:
	{
		auto const &inst = inst_.get<instruction::min_f64_check>();
		return bz::format("min f64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::max_i8:
	{
		auto const &inst = inst_.get<instruction::max_i8>();
		return bz::format("max i8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::max_i16:
	{
		auto const &inst = inst_.get<instruction::max_i16>();
		return bz::format("max i16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::max_i32:
	{
		auto const &inst = inst_.get<instruction::max_i32>();
		return bz::format("max i32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::max_i64:
	{
		auto const &inst = inst_.get<instruction::max_i64>();
		return bz::format("max i64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::max_u8:
	{
		auto const &inst = inst_.get<instruction::max_u8>();
		return bz::format("max u8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::max_u16:
	{
		auto const &inst = inst_.get<instruction::max_u16>();
		return bz::format("max u16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::max_u32:
	{
		auto const &inst = inst_.get<instruction::max_u32>();
		return bz::format("max u32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::max_u64:
	{
		auto const &inst = inst_.get<instruction::max_u64>();
		return bz::format("max u64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::max_f32:
	{
		auto const &inst = inst_.get<instruction::max_f32>();
		return bz::format("max f32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::max_f64:
	{
		auto const &inst = inst_.get<instruction::max_f64>();
		return bz::format("max f64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::max_f32_check:
	{
		auto const &inst = inst_.get<instruction::max_f32_check>();
		return bz::format("max f32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::max_f64_check:
	{
		auto const &inst = inst_.get<instruction::max_f64_check>();
		return bz::format("max f64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::exp_f32:
	{
		auto const &inst = inst_.get<instruction::exp_f32>();
		return bz::format("exp f32 {}", inst.args[0]);
	}
	case instruction::exp_f64:
	{
		auto const &inst = inst_.get<instruction::exp_f64>();
		return bz::format("exp f64 {}", inst.args[0]);
	}
	case instruction::exp_f32_check:
	{
		auto const &inst = inst_.get<instruction::exp_f32_check>();
		return bz::format("exp f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::exp_f64_check:
	{
		auto const &inst = inst_.get<instruction::exp_f64_check>();
		return bz::format("exp f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::exp2_f32:
	{
		auto const &inst = inst_.get<instruction::exp2_f32>();
		return bz::format("exp2 f32 {}", inst.args[0]);
	}
	case instruction::exp2_f64:
	{
		auto const &inst = inst_.get<instruction::exp2_f64>();
		return bz::format("exp2 f64 {}", inst.args[0]);
	}
	case instruction::exp2_f32_check:
	{
		auto const &inst = inst_.get<instruction::exp2_f32_check>();
		return bz::format("exp2 f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::exp2_f64_check:
	{
		auto const &inst = inst_.get<instruction::exp2_f64_check>();
		return bz::format("exp2 f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::expm1_f32:
	{
		auto const &inst = inst_.get<instruction::expm1_f32>();
		return bz::format("expm1 f32 {}", inst.args[0]);
	}
	case instruction::expm1_f64:
	{
		auto const &inst = inst_.get<instruction::expm1_f64>();
		return bz::format("expm1 f64 {}", inst.args[0]);
	}
	case instruction::expm1_f32_check:
	{
		auto const &inst = inst_.get<instruction::expm1_f32_check>();
		return bz::format("expm1 f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::expm1_f64_check:
	{
		auto const &inst = inst_.get<instruction::expm1_f64_check>();
		return bz::format("expm1 f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::log_f32:
	{
		auto const &inst = inst_.get<instruction::log_f32>();
		return bz::format("log f32 {}", inst.args[0]);
	}
	case instruction::log_f64:
	{
		auto const &inst = inst_.get<instruction::log_f64>();
		return bz::format("log f64 {}", inst.args[0]);
	}
	case instruction::log_f32_check:
	{
		auto const &inst = inst_.get<instruction::log_f32_check>();
		return bz::format("log f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::log_f64_check:
	{
		auto const &inst = inst_.get<instruction::log_f64_check>();
		return bz::format("log f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::log10_f32:
	{
		auto const &inst = inst_.get<instruction::log10_f32>();
		return bz::format("log10 f32 {}", inst.args[0]);
	}
	case instruction::log10_f64:
	{
		auto const &inst = inst_.get<instruction::log10_f64>();
		return bz::format("log10 f64 {}", inst.args[0]);
	}
	case instruction::log10_f32_check:
	{
		auto const &inst = inst_.get<instruction::log10_f32_check>();
		return bz::format("log10 f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::log10_f64_check:
	{
		auto const &inst = inst_.get<instruction::log10_f64_check>();
		return bz::format("log10 f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::log2_f32:
	{
		auto const &inst = inst_.get<instruction::log2_f32>();
		return bz::format("log2 f32 {}", inst.args[0]);
	}
	case instruction::log2_f64:
	{
		auto const &inst = inst_.get<instruction::log2_f64>();
		return bz::format("log2 f64 {}", inst.args[0]);
	}
	case instruction::log2_f32_check:
	{
		auto const &inst = inst_.get<instruction::log2_f32_check>();
		return bz::format("log2 f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::log2_f64_check:
	{
		auto const &inst = inst_.get<instruction::log2_f64_check>();
		return bz::format("log2 f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::log1p_f32:
	{
		auto const &inst = inst_.get<instruction::log1p_f32>();
		return bz::format("log1p f32 {}", inst.args[0]);
	}
	case instruction::log1p_f64:
	{
		auto const &inst = inst_.get<instruction::log1p_f64>();
		return bz::format("log1p f64 {}", inst.args[0]);
	}
	case instruction::log1p_f32_check:
	{
		auto const &inst = inst_.get<instruction::log1p_f32_check>();
		return bz::format("log1p f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::log1p_f64_check:
	{
		auto const &inst = inst_.get<instruction::log1p_f64_check>();
		return bz::format("log1p f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::sqrt_f32:
	{
		auto const &inst = inst_.get<instruction::sqrt_f32>();
		return bz::format("sqrt f32 {}", inst.args[0]);
	}
	case instruction::sqrt_f64:
	{
		auto const &inst = inst_.get<instruction::sqrt_f64>();
		return bz::format("sqrt f64 {}", inst.args[0]);
	}
	case instruction::sqrt_f32_check:
	{
		auto const &inst = inst_.get<instruction::sqrt_f32_check>();
		return bz::format("sqrt f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::sqrt_f64_check:
	{
		auto const &inst = inst_.get<instruction::sqrt_f64_check>();
		return bz::format("sqrt f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::pow_f32:
	{
		auto const &inst = inst_.get<instruction::pow_f32>();
		return bz::format("pow f32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::pow_f64:
	{
		auto const &inst = inst_.get<instruction::pow_f64>();
		return bz::format("pow f64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::pow_f32_check:
	{
		auto const &inst = inst_.get<instruction::pow_f32_check>();
		return bz::format("pow f32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::pow_f64_check:
	{
		auto const &inst = inst_.get<instruction::pow_f64_check>();
		return bz::format("pow f64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::cbrt_f32:
	{
		auto const &inst = inst_.get<instruction::cbrt_f32>();
		return bz::format("cbrt f32 {}", inst.args[0]);
	}
	case instruction::cbrt_f64:
	{
		auto const &inst = inst_.get<instruction::cbrt_f64>();
		return bz::format("cbrt f64 {}", inst.args[0]);
	}
	case instruction::cbrt_f32_check:
	{
		auto const &inst = inst_.get<instruction::cbrt_f32_check>();
		return bz::format("cbrt f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::cbrt_f64_check:
	{
		auto const &inst = inst_.get<instruction::cbrt_f64_check>();
		return bz::format("cbrt f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::hypot_f32:
	{
		auto const &inst = inst_.get<instruction::hypot_f32>();
		return bz::format("hypot f32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::hypot_f64:
	{
		auto const &inst = inst_.get<instruction::hypot_f64>();
		return bz::format("hypot f64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::hypot_f32_check:
	{
		auto const &inst = inst_.get<instruction::hypot_f32_check>();
		return bz::format("hypot f32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::hypot_f64_check:
	{
		auto const &inst = inst_.get<instruction::hypot_f64_check>();
		return bz::format("hypot f64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::sin_f32:
	{
		auto const &inst = inst_.get<instruction::sin_f32>();
		return bz::format("sin f32 {}", inst.args[0]);
	}
	case instruction::sin_f64:
	{
		auto const &inst = inst_.get<instruction::sin_f64>();
		return bz::format("sin f64 {}", inst.args[0]);
	}
	case instruction::sin_f32_check:
	{
		auto const &inst = inst_.get<instruction::sin_f32_check>();
		return bz::format("sin f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::sin_f64_check:
	{
		auto const &inst = inst_.get<instruction::sin_f64_check>();
		return bz::format("sin f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::cos_f32:
	{
		auto const &inst = inst_.get<instruction::cos_f32>();
		return bz::format("cos f32 {}", inst.args[0]);
	}
	case instruction::cos_f64:
	{
		auto const &inst = inst_.get<instruction::cos_f64>();
		return bz::format("cos f64 {}", inst.args[0]);
	}
	case instruction::cos_f32_check:
	{
		auto const &inst = inst_.get<instruction::cos_f32_check>();
		return bz::format("cos f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::cos_f64_check:
	{
		auto const &inst = inst_.get<instruction::cos_f64_check>();
		return bz::format("cos f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::tan_f32:
	{
		auto const &inst = inst_.get<instruction::tan_f32>();
		return bz::format("tan f32 {}", inst.args[0]);
	}
	case instruction::tan_f64:
	{
		auto const &inst = inst_.get<instruction::tan_f64>();
		return bz::format("tan f64 {}", inst.args[0]);
	}
	case instruction::tan_f32_check:
	{
		auto const &inst = inst_.get<instruction::tan_f32_check>();
		return bz::format("tan f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::tan_f64_check:
	{
		auto const &inst = inst_.get<instruction::tan_f64_check>();
		return bz::format("tan f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::asin_f32:
	{
		auto const &inst = inst_.get<instruction::asin_f32>();
		return bz::format("asin f32 {}", inst.args[0]);
	}
	case instruction::asin_f64:
	{
		auto const &inst = inst_.get<instruction::asin_f64>();
		return bz::format("asin f64 {}", inst.args[0]);
	}
	case instruction::asin_f32_check:
	{
		auto const &inst = inst_.get<instruction::asin_f32_check>();
		return bz::format("asin f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::asin_f64_check:
	{
		auto const &inst = inst_.get<instruction::asin_f64_check>();
		return bz::format("asin f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::acos_f32:
	{
		auto const &inst = inst_.get<instruction::acos_f32>();
		return bz::format("acos f32 {}", inst.args[0]);
	}
	case instruction::acos_f64:
	{
		auto const &inst = inst_.get<instruction::acos_f64>();
		return bz::format("acos f64 {}", inst.args[0]);
	}
	case instruction::acos_f32_check:
	{
		auto const &inst = inst_.get<instruction::acos_f32_check>();
		return bz::format("acos f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::acos_f64_check:
	{
		auto const &inst = inst_.get<instruction::acos_f64_check>();
		return bz::format("acos f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::atan_f32:
	{
		auto const &inst = inst_.get<instruction::atan_f32>();
		return bz::format("atan f32 {}", inst.args[0]);
	}
	case instruction::atan_f64:
	{
		auto const &inst = inst_.get<instruction::atan_f64>();
		return bz::format("atan f64 {}", inst.args[0]);
	}
	case instruction::atan_f32_check:
	{
		auto const &inst = inst_.get<instruction::atan_f32_check>();
		return bz::format("atan f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::atan_f64_check:
	{
		auto const &inst = inst_.get<instruction::atan_f64_check>();
		return bz::format("atan f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::atan2_f32:
	{
		auto const &inst = inst_.get<instruction::atan2_f32>();
		return bz::format("atan2 f32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::atan2_f64:
	{
		auto const &inst = inst_.get<instruction::atan2_f64>();
		return bz::format("atan2 f64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::atan2_f32_check:
	{
		auto const &inst = inst_.get<instruction::atan2_f32_check>();
		return bz::format("atan2 f32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::atan2_f64_check:
	{
		auto const &inst = inst_.get<instruction::atan2_f64_check>();
		return bz::format("atan2 f64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::sinh_f32:
	{
		auto const &inst = inst_.get<instruction::sinh_f32>();
		return bz::format("sinh f32 {}", inst.args[0]);
	}
	case instruction::sinh_f64:
	{
		auto const &inst = inst_.get<instruction::sinh_f64>();
		return bz::format("sinh f64 {}", inst.args[0]);
	}
	case instruction::sinh_f32_check:
	{
		auto const &inst = inst_.get<instruction::sinh_f32_check>();
		return bz::format("sinh f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::sinh_f64_check:
	{
		auto const &inst = inst_.get<instruction::sinh_f64_check>();
		return bz::format("sinh f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::cosh_f32:
	{
		auto const &inst = inst_.get<instruction::cosh_f32>();
		return bz::format("cosh f32 {}", inst.args[0]);
	}
	case instruction::cosh_f64:
	{
		auto const &inst = inst_.get<instruction::cosh_f64>();
		return bz::format("cosh f64 {}", inst.args[0]);
	}
	case instruction::cosh_f32_check:
	{
		auto const &inst = inst_.get<instruction::cosh_f32_check>();
		return bz::format("cosh f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::cosh_f64_check:
	{
		auto const &inst = inst_.get<instruction::cosh_f64_check>();
		return bz::format("cosh f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::tanh_f32:
	{
		auto const &inst = inst_.get<instruction::tanh_f32>();
		return bz::format("tanh f32 {}", inst.args[0]);
	}
	case instruction::tanh_f64:
	{
		auto const &inst = inst_.get<instruction::tanh_f64>();
		return bz::format("tanh f64 {}", inst.args[0]);
	}
	case instruction::tanh_f32_check:
	{
		auto const &inst = inst_.get<instruction::tanh_f32_check>();
		return bz::format("tanh f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::tanh_f64_check:
	{
		auto const &inst = inst_.get<instruction::tanh_f64_check>();
		return bz::format("tanh f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::asinh_f32:
	{
		auto const &inst = inst_.get<instruction::asinh_f32>();
		return bz::format("asinh f32 {}", inst.args[0]);
	}
	case instruction::asinh_f64:
	{
		auto const &inst = inst_.get<instruction::asinh_f64>();
		return bz::format("asinh f64 {}", inst.args[0]);
	}
	case instruction::asinh_f32_check:
	{
		auto const &inst = inst_.get<instruction::asinh_f32_check>();
		return bz::format("asinh f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::asinh_f64_check:
	{
		auto const &inst = inst_.get<instruction::asinh_f64_check>();
		return bz::format("asinh f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::acosh_f32:
	{
		auto const &inst = inst_.get<instruction::acosh_f32>();
		return bz::format("acosh f32 {}", inst.args[0]);
	}
	case instruction::acosh_f64:
	{
		auto const &inst = inst_.get<instruction::acosh_f64>();
		return bz::format("acosh f64 {}", inst.args[0]);
	}
	case instruction::acosh_f32_check:
	{
		auto const &inst = inst_.get<instruction::acosh_f32_check>();
		return bz::format("acosh f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::acosh_f64_check:
	{
		auto const &inst = inst_.get<instruction::acosh_f64_check>();
		return bz::format("acosh f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::atanh_f32:
	{
		auto const &inst = inst_.get<instruction::atanh_f32>();
		return bz::format("atanh f32 {}", inst.args[0]);
	}
	case instruction::atanh_f64:
	{
		auto const &inst = inst_.get<instruction::atanh_f64>();
		return bz::format("atanh f64 {}", inst.args[0]);
	}
	case instruction::atanh_f32_check:
	{
		auto const &inst = inst_.get<instruction::atanh_f32_check>();
		return bz::format("atanh f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::atanh_f64_check:
	{
		auto const &inst = inst_.get<instruction::atanh_f64_check>();
		return bz::format("atanh f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::erf_f32:
	{
		auto const &inst = inst_.get<instruction::erf_f32>();
		return bz::format("erf f32 {}", inst.args[0]);
	}
	case instruction::erf_f64:
	{
		auto const &inst = inst_.get<instruction::erf_f64>();
		return bz::format("erf f64 {}", inst.args[0]);
	}
	case instruction::erf_f32_check:
	{
		auto const &inst = inst_.get<instruction::erf_f32_check>();
		return bz::format("erf f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::erf_f64_check:
	{
		auto const &inst = inst_.get<instruction::erf_f64_check>();
		return bz::format("erf f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::erfc_f32:
	{
		auto const &inst = inst_.get<instruction::erfc_f32>();
		return bz::format("erfc f32 {}", inst.args[0]);
	}
	case instruction::erfc_f64:
	{
		auto const &inst = inst_.get<instruction::erfc_f64>();
		return bz::format("erfc f64 {}", inst.args[0]);
	}
	case instruction::erfc_f32_check:
	{
		auto const &inst = inst_.get<instruction::erfc_f32_check>();
		return bz::format("erfc f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::erfc_f64_check:
	{
		auto const &inst = inst_.get<instruction::erfc_f64_check>();
		return bz::format("erfc f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::tgamma_f32:
	{
		auto const &inst = inst_.get<instruction::tgamma_f32>();
		return bz::format("tgamma f32 {}", inst.args[0]);
	}
	case instruction::tgamma_f64:
	{
		auto const &inst = inst_.get<instruction::tgamma_f64>();
		return bz::format("tgamma f64 {}", inst.args[0]);
	}
	case instruction::tgamma_f32_check:
	{
		auto const &inst = inst_.get<instruction::tgamma_f32_check>();
		return bz::format("tgamma f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::tgamma_f64_check:
	{
		auto const &inst = inst_.get<instruction::tgamma_f64_check>();
		return bz::format("tgamma f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::lgamma_f32:
	{
		auto const &inst = inst_.get<instruction::lgamma_f32>();
		return bz::format("lgamma f32 {}", inst.args[0]);
	}
	case instruction::lgamma_f64:
	{
		auto const &inst = inst_.get<instruction::lgamma_f64>();
		return bz::format("lgamma f64 {}", inst.args[0]);
	}
	case instruction::lgamma_f32_check:
	{
		auto const &inst = inst_.get<instruction::lgamma_f32_check>();
		return bz::format("lgamma f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::lgamma_f64_check:
	{
		auto const &inst = inst_.get<instruction::lgamma_f64_check>();
		return bz::format("lgamma f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::bitreverse_u8:
	{
		auto const &inst = inst_.get<instruction::bitreverse_u8>();
		return bz::format("bitreverse u8 {}", inst.args[0]);
	}
	case instruction::bitreverse_u16:
	{
		auto const &inst = inst_.get<instruction::bitreverse_u16>();
		return bz::format("bitreverse u16 {}", inst.args[0]);
	}
	case instruction::bitreverse_u32:
	{
		auto const &inst = inst_.get<instruction::bitreverse_u32>();
		return bz::format("bitreverse u32 {}", inst.args[0]);
	}
	case instruction::bitreverse_u64:
	{
		auto const &inst = inst_.get<instruction::bitreverse_u64>();
		return bz::format("bitreverse u64 {}", inst.args[0]);
	}
	case instruction::popcount_u8:
	{
		auto const &inst = inst_.get<instruction::popcount_u8>();
		return bz::format("popcount u8 {}", inst.args[0]);
	}
	case instruction::popcount_u16:
	{
		auto const &inst = inst_.get<instruction::popcount_u16>();
		return bz::format("popcount u16 {}", inst.args[0]);
	}
	case instruction::popcount_u32:
	{
		auto const &inst = inst_.get<instruction::popcount_u32>();
		return bz::format("popcount u32 {}", inst.args[0]);
	}
	case instruction::popcount_u64:
	{
		auto const &inst = inst_.get<instruction::popcount_u64>();
		return bz::format("popcount u64 {}", inst.args[0]);
	}
	case instruction::byteswap_u16:
	{
		auto const &inst = inst_.get<instruction::byteswap_u16>();
		return bz::format("byteswap u16 {}", inst.args[0]);
	}
	case instruction::byteswap_u32:
	{
		auto const &inst = inst_.get<instruction::byteswap_u32>();
		return bz::format("byteswap u32 {}", inst.args[0]);
	}
	case instruction::byteswap_u64:
	{
		auto const &inst = inst_.get<instruction::byteswap_u64>();
		return bz::format("byteswap u64 {}", inst.args[0]);
	}
	case instruction::clz_u8:
	{
		auto const &inst = inst_.get<instruction::clz_u8>();
		return bz::format("clz u8 {}", inst.args[0]);
	}
	case instruction::clz_u16:
	{
		auto const &inst = inst_.get<instruction::clz_u16>();
		return bz::format("clz u16 {}", inst.args[0]);
	}
	case instruction::clz_u32:
	{
		auto const &inst = inst_.get<instruction::clz_u32>();
		return bz::format("clz u32 {}", inst.args[0]);
	}
	case instruction::clz_u64:
	{
		auto const &inst = inst_.get<instruction::clz_u64>();
		return bz::format("clz u64 {}", inst.args[0]);
	}
	case instruction::ctz_u8:
	{
		auto const &inst = inst_.get<instruction::ctz_u8>();
		return bz::format("ctz u8 {}", inst.args[0]);
	}
	case instruction::ctz_u16:
	{
		auto const &inst = inst_.get<instruction::ctz_u16>();
		return bz::format("ctz u16 {}", inst.args[0]);
	}
	case instruction::ctz_u32:
	{
		auto const &inst = inst_.get<instruction::ctz_u32>();
		return bz::format("ctz u32 {}", inst.args[0]);
	}
	case instruction::ctz_u64:
	{
		auto const &inst = inst_.get<instruction::ctz_u64>();
		return bz::format("ctz u64 {}", inst.args[0]);
	}
	case instruction::fshl_u8:
	{
		auto const &inst = inst_.get<instruction::fshl_u8>();
		return bz::format("fshl u8 {}, {}, {}", inst.args[0], inst.args[1], inst.args[2]);
	}
	case instruction::fshl_u16:
	{
		auto const &inst = inst_.get<instruction::fshl_u16>();
		return bz::format("fshl u16 {}, {}, {}", inst.args[0], inst.args[1], inst.args[2]);
	}
	case instruction::fshl_u32:
	{
		auto const &inst = inst_.get<instruction::fshl_u32>();
		return bz::format("fshl u32 {}, {}, {}", inst.args[0], inst.args[1], inst.args[2]);
	}
	case instruction::fshl_u64:
	{
		auto const &inst = inst_.get<instruction::fshl_u64>();
		return bz::format("fshl u64 {}, {}, {}", inst.args[0], inst.args[1], inst.args[2]);
	}
	case instruction::fshr_u8:
	{
		auto const &inst = inst_.get<instruction::fshr_u8>();
		return bz::format("fshr u8 {}, {}, {}", inst.args[0], inst.args[1], inst.args[2]);
	}
	case instruction::fshr_u16:
	{
		auto const &inst = inst_.get<instruction::fshr_u16>();
		return bz::format("fshr u16 {}, {}, {}", inst.args[0], inst.args[1], inst.args[2]);
	}
	case instruction::fshr_u32:
	{
		auto const &inst = inst_.get<instruction::fshr_u32>();
		return bz::format("fshr u32 {}, {}, {}", inst.args[0], inst.args[1], inst.args[2]);
	}
	case instruction::fshr_u64:
	{
		auto const &inst = inst_.get<instruction::fshr_u64>();
		return bz::format("fshr u64 {}, {}, {}", inst.args[0], inst.args[1], inst.args[2]);
	}
	case instruction::const_gep:
	{
		auto const &inst = inst_.get<instruction::const_gep>();
		return bz::format("gep {} {} {}", inst.object_type->to_string(), inst.index, inst.args[0]);
	}
	case instruction::array_gep_i32:
	{
		auto const &inst = inst_.get<instruction::array_gep_i32>();
		return bz::format("array-gep i32 {} {} {}", inst.elem_type->to_string(), inst.args[0], inst.args[1]);
	}
	case instruction::array_gep_i64:
	{
		auto const &inst = inst_.get<instruction::array_gep_i64>();
		return bz::format("array-gep i64 {} {} {}", inst.elem_type->to_string(), inst.args[0], inst.args[1]);
	}
	case instruction::const_memcpy:
	{
		auto const &inst = inst_.get<instruction::const_memcpy>();
		return bz::format("const memcpy {} {}, {}", inst.size, inst.args[0], inst.args[1]);
	}
	case instruction::const_memset_zero:
	{
		auto const &inst = inst_.get<instruction::const_memset_zero>();
		return bz::format("const memset-zero {} {}", inst.size, inst.args[0]);
	}
	case instruction::function_call:
	{
		auto const &inst = inst_.get<instruction::function_call>();
		return bz::format("call '{}' ({}) ({})", inst.func->func_body->get_signature(), inst.args_index, inst.src_tokens_index);
	}
	case instruction::malloc:
	{
		auto const &inst = inst_.get<instruction::malloc>();
		return bz::format("malloc {} {} ({})", inst.type->to_string(), inst.args[0], inst.src_tokens_index);
	}
	case instruction::free:
	{
		auto const &inst = inst_.get<instruction::free>();
		return bz::format("free {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::jump:
	{
		auto const &inst = inst_.get<instruction::jump>();
		return bz::format("jump {}", inst.dest.index);
	}
	case instruction::conditional_jump:
	{
		auto const &inst = inst_.get<instruction::conditional_jump>();
		return bz::format("jump {}, {}, {}", inst.args[0], inst.true_dest.index, inst.false_dest.index);
	}
	case instruction::switch_i1:
	{
		auto const &inst = inst_.get<instruction::switch_i1>();
		return bz::format("switch i1 {} ({})", inst.args[0], inst.switch_info_index);
	}
	case instruction::switch_i8:
	{
		auto const &inst = inst_.get<instruction::switch_i8>();
		return bz::format("switch i8 {} ({})", inst.args[0], inst.switch_info_index);
	}
	case instruction::switch_i16:
	{
		auto const &inst = inst_.get<instruction::switch_i16>();
		return bz::format("switch i16 {} ({})", inst.args[0], inst.switch_info_index);
	}
	case instruction::switch_i32:
	{
		auto const &inst = inst_.get<instruction::switch_i32>();
		return bz::format("switch i32 {} ({})", inst.args[0], inst.switch_info_index);
	}
	case instruction::switch_i64:
	{
		auto const &inst = inst_.get<instruction::switch_i64>();
		return bz::format("switch i64 {} ({})", inst.args[0], inst.switch_info_index);
	}
	case instruction::ret:
	{
		auto const &inst = inst_.get<instruction::ret>();
		return bz::format("ret {}", inst.args[0]);
	}
	case instruction::ret_void:
	{
		return "ret";
	}
	case instruction::unreachable:
	{
		return "unreachable";
	}
	case instruction::error:
	{
		auto const &inst = inst_.get<instruction::error>();
		return bz::format("error {}", inst.error_index);
	}
	case instruction::diagnostic_str:
	{
		auto const &inst = inst_.get<instruction::diagnostic_str>();
		return bz::format("diagnostic {} {}, {} ({})", (int)inst.kind, inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::is_option_set:
	{
		auto const &inst = inst_.get<instruction::is_option_set>();
		return bz::format("is-option-set {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::array_bounds_check_i32:
	{
		auto const &inst = inst_.get<instruction::array_bounds_check_i32>();
		return bz::format("array bounds check i32 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::array_bounds_check_u32:
	{
		auto const &inst = inst_.get<instruction::array_bounds_check_u32>();
		return bz::format("array bounds check u32 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::array_bounds_check_i64:
	{
		auto const &inst = inst_.get<instruction::array_bounds_check_i64>();
		return bz::format("array bounds check i64 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::array_bounds_check_u64:
	{
		auto const &inst = inst_.get<instruction::array_bounds_check_u64>();
		return bz::format("array bounds check u64 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::optional_get_value_check:
	{
		auto const &inst = inst_.get<instruction::optional_get_value_check>();
		return bz::format("optional get_value check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::str_construction_check:
	{
		auto const &inst = inst_.get<instruction::str_construction_check>();
		return bz::format("str construction check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::slice_construction_check:
	{
		auto const &inst = inst_.get<instruction::slice_construction_check>();
		return bz::format("slice construction check {}, {} ({}, {})", inst.args[0], inst.args[1], inst.src_tokens_index, inst.slice_construction_check_info_index);
	}
	default:
		bz_unreachable;
	}
}

bz::u8string to_string(function const &func)
{
	bz::u8string result;

	if (func.func_body != nullptr)
	{
		result += bz::format("'{}':\n", func.func_body->get_signature());
	}
	else
	{
		result += "anon-function:\n";
	}

	uint32_t i = 0;

	for (auto const type : func.allocas)
	{
		result += bz::format("  {} = alloca {}\n", instruction_value_index{ .index = i }, type->to_string());
		++i;
	}

	for (auto const &inst : func.instructions)
	{
		result += bz::format("  {} = {}\n", instruction_value_index{ .index = i }, to_string(inst));
		++i;
	}

	result += "call_args:\n";
	for (auto const i : bz::iota(0, func.call_args.size()))
	{
		result += bz::format("  {}: (", i);
		auto const &args = func.call_args[i];
		if (args.size() == 0)
		{
			result += ")\n";
		}
		else
		{
			result += bz::format("{}", args[0]);
			for (auto const arg : args.slice(1))
			{
				result += bz::format(", {}", arg);
			}
			result += ")\n";
		}
	}

	return result;
}

} // namespace comptime
