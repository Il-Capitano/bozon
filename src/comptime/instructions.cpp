#include "instructions.h"
#include "ast/statement.h"

namespace comptime
{

static_assert(sizeof (instruction) == 24);
static_assert(std::is_trivially_copy_constructible_v<instruction>);
static_assert(std::is_trivially_destructible_v<instruction>);
static_assert([]<typename ...Ts>(bz::meta::type_pack<Ts...>) {
	return bz::meta::is_all<std::is_trivial_v<Ts>...>;
}(instruction_list_t()));

static_assert(instruction_list_t::size() == 544);
static_assert(instruction::const_i1                 == instruction::index_of<instructions::const_i1>);
static_assert(instruction::const_i8                 == instruction::index_of<instructions::const_i8>);
static_assert(instruction::const_i16                == instruction::index_of<instructions::const_i16>);
static_assert(instruction::const_i32                == instruction::index_of<instructions::const_i32>);
static_assert(instruction::const_i64                == instruction::index_of<instructions::const_i64>);
static_assert(instruction::const_u8                 == instruction::index_of<instructions::const_u8>);
static_assert(instruction::const_u16                == instruction::index_of<instructions::const_u16>);
static_assert(instruction::const_u32                == instruction::index_of<instructions::const_u32>);
static_assert(instruction::const_u64                == instruction::index_of<instructions::const_u64>);
static_assert(instruction::const_f32                == instruction::index_of<instructions::const_f32>);
static_assert(instruction::const_f64                == instruction::index_of<instructions::const_f64>);
static_assert(instruction::const_ptr_null           == instruction::index_of<instructions::const_ptr_null>);
static_assert(instruction::const_func_ptr           == instruction::index_of<instructions::const_func_ptr>);
static_assert(instruction::get_global_address       == instruction::index_of<instructions::get_global_address>);
static_assert(instruction::get_function_arg         == instruction::index_of<instructions::get_function_arg>);
static_assert(instruction::load_i1_be               == instruction::index_of<instructions::load_i1_be>);
static_assert(instruction::load_i8_be               == instruction::index_of<instructions::load_i8_be>);
static_assert(instruction::load_i16_be              == instruction::index_of<instructions::load_i16_be>);
static_assert(instruction::load_i32_be              == instruction::index_of<instructions::load_i32_be>);
static_assert(instruction::load_i64_be              == instruction::index_of<instructions::load_i64_be>);
static_assert(instruction::load_f32_be              == instruction::index_of<instructions::load_f32_be>);
static_assert(instruction::load_f64_be              == instruction::index_of<instructions::load_f64_be>);
static_assert(instruction::load_ptr32_be            == instruction::index_of<instructions::load_ptr32_be>);
static_assert(instruction::load_ptr64_be            == instruction::index_of<instructions::load_ptr64_be>);
static_assert(instruction::load_i1_le               == instruction::index_of<instructions::load_i1_le>);
static_assert(instruction::load_i8_le               == instruction::index_of<instructions::load_i8_le>);
static_assert(instruction::load_i16_le              == instruction::index_of<instructions::load_i16_le>);
static_assert(instruction::load_i32_le              == instruction::index_of<instructions::load_i32_le>);
static_assert(instruction::load_i64_le              == instruction::index_of<instructions::load_i64_le>);
static_assert(instruction::load_f32_le              == instruction::index_of<instructions::load_f32_le>);
static_assert(instruction::load_f64_le              == instruction::index_of<instructions::load_f64_le>);
static_assert(instruction::load_ptr32_le            == instruction::index_of<instructions::load_ptr32_le>);
static_assert(instruction::load_ptr64_le            == instruction::index_of<instructions::load_ptr64_le>);
static_assert(instruction::store_i1_be              == instruction::index_of<instructions::store_i1_be>);
static_assert(instruction::store_i8_be              == instruction::index_of<instructions::store_i8_be>);
static_assert(instruction::store_i16_be             == instruction::index_of<instructions::store_i16_be>);
static_assert(instruction::store_i32_be             == instruction::index_of<instructions::store_i32_be>);
static_assert(instruction::store_i64_be             == instruction::index_of<instructions::store_i64_be>);
static_assert(instruction::store_f32_be             == instruction::index_of<instructions::store_f32_be>);
static_assert(instruction::store_f64_be             == instruction::index_of<instructions::store_f64_be>);
static_assert(instruction::store_ptr32_be           == instruction::index_of<instructions::store_ptr32_be>);
static_assert(instruction::store_ptr64_be           == instruction::index_of<instructions::store_ptr64_be>);
static_assert(instruction::store_i1_le              == instruction::index_of<instructions::store_i1_le>);
static_assert(instruction::store_i8_le              == instruction::index_of<instructions::store_i8_le>);
static_assert(instruction::store_i16_le             == instruction::index_of<instructions::store_i16_le>);
static_assert(instruction::store_i32_le             == instruction::index_of<instructions::store_i32_le>);
static_assert(instruction::store_i64_le             == instruction::index_of<instructions::store_i64_le>);
static_assert(instruction::store_f32_le             == instruction::index_of<instructions::store_f32_le>);
static_assert(instruction::store_f64_le             == instruction::index_of<instructions::store_f64_le>);
static_assert(instruction::store_ptr32_le           == instruction::index_of<instructions::store_ptr32_le>);
static_assert(instruction::store_ptr64_le           == instruction::index_of<instructions::store_ptr64_le>);
static_assert(instruction::check_dereference        == instruction::index_of<instructions::check_dereference>);
static_assert(instruction::check_inplace_construct  == instruction::index_of<instructions::check_inplace_construct>);
static_assert(instruction::check_destruct_value     == instruction::index_of<instructions::check_destruct_value>);
static_assert(instruction::cast_zext_i1_to_i8       == instruction::index_of<instructions::cast_zext_i1_to_i8>);
static_assert(instruction::cast_zext_i1_to_i16      == instruction::index_of<instructions::cast_zext_i1_to_i16>);
static_assert(instruction::cast_zext_i1_to_i32      == instruction::index_of<instructions::cast_zext_i1_to_i32>);
static_assert(instruction::cast_zext_i1_to_i64      == instruction::index_of<instructions::cast_zext_i1_to_i64>);
static_assert(instruction::cast_zext_i8_to_i16      == instruction::index_of<instructions::cast_zext_i8_to_i16>);
static_assert(instruction::cast_zext_i8_to_i32      == instruction::index_of<instructions::cast_zext_i8_to_i32>);
static_assert(instruction::cast_zext_i8_to_i64      == instruction::index_of<instructions::cast_zext_i8_to_i64>);
static_assert(instruction::cast_zext_i16_to_i32     == instruction::index_of<instructions::cast_zext_i16_to_i32>);
static_assert(instruction::cast_zext_i16_to_i64     == instruction::index_of<instructions::cast_zext_i16_to_i64>);
static_assert(instruction::cast_zext_i32_to_i64     == instruction::index_of<instructions::cast_zext_i32_to_i64>);
static_assert(instruction::cast_sext_i8_to_i16      == instruction::index_of<instructions::cast_sext_i8_to_i16>);
static_assert(instruction::cast_sext_i8_to_i32      == instruction::index_of<instructions::cast_sext_i8_to_i32>);
static_assert(instruction::cast_sext_i8_to_i64      == instruction::index_of<instructions::cast_sext_i8_to_i64>);
static_assert(instruction::cast_sext_i16_to_i32     == instruction::index_of<instructions::cast_sext_i16_to_i32>);
static_assert(instruction::cast_sext_i16_to_i64     == instruction::index_of<instructions::cast_sext_i16_to_i64>);
static_assert(instruction::cast_sext_i32_to_i64     == instruction::index_of<instructions::cast_sext_i32_to_i64>);
static_assert(instruction::cast_trunc_i64_to_i8     == instruction::index_of<instructions::cast_trunc_i64_to_i8>);
static_assert(instruction::cast_trunc_i64_to_i16    == instruction::index_of<instructions::cast_trunc_i64_to_i16>);
static_assert(instruction::cast_trunc_i64_to_i32    == instruction::index_of<instructions::cast_trunc_i64_to_i32>);
static_assert(instruction::cast_trunc_i32_to_i8     == instruction::index_of<instructions::cast_trunc_i32_to_i8>);
static_assert(instruction::cast_trunc_i32_to_i16    == instruction::index_of<instructions::cast_trunc_i32_to_i16>);
static_assert(instruction::cast_trunc_i16_to_i8     == instruction::index_of<instructions::cast_trunc_i16_to_i8>);
static_assert(instruction::cast_f32_to_f64          == instruction::index_of<instructions::cast_f32_to_f64>);
static_assert(instruction::cast_f64_to_f32          == instruction::index_of<instructions::cast_f64_to_f32>);
static_assert(instruction::cast_f32_to_i8           == instruction::index_of<instructions::cast_f32_to_i8>);
static_assert(instruction::cast_f32_to_i16          == instruction::index_of<instructions::cast_f32_to_i16>);
static_assert(instruction::cast_f32_to_i32          == instruction::index_of<instructions::cast_f32_to_i32>);
static_assert(instruction::cast_f32_to_i64          == instruction::index_of<instructions::cast_f32_to_i64>);
static_assert(instruction::cast_f32_to_u8           == instruction::index_of<instructions::cast_f32_to_u8>);
static_assert(instruction::cast_f32_to_u16          == instruction::index_of<instructions::cast_f32_to_u16>);
static_assert(instruction::cast_f32_to_u32          == instruction::index_of<instructions::cast_f32_to_u32>);
static_assert(instruction::cast_f32_to_u64          == instruction::index_of<instructions::cast_f32_to_u64>);
static_assert(instruction::cast_f64_to_i8           == instruction::index_of<instructions::cast_f64_to_i8>);
static_assert(instruction::cast_f64_to_i16          == instruction::index_of<instructions::cast_f64_to_i16>);
static_assert(instruction::cast_f64_to_i32          == instruction::index_of<instructions::cast_f64_to_i32>);
static_assert(instruction::cast_f64_to_i64          == instruction::index_of<instructions::cast_f64_to_i64>);
static_assert(instruction::cast_f64_to_u8           == instruction::index_of<instructions::cast_f64_to_u8>);
static_assert(instruction::cast_f64_to_u16          == instruction::index_of<instructions::cast_f64_to_u16>);
static_assert(instruction::cast_f64_to_u32          == instruction::index_of<instructions::cast_f64_to_u32>);
static_assert(instruction::cast_f64_to_u64          == instruction::index_of<instructions::cast_f64_to_u64>);
static_assert(instruction::cast_i8_to_f32           == instruction::index_of<instructions::cast_i8_to_f32>);
static_assert(instruction::cast_i16_to_f32          == instruction::index_of<instructions::cast_i16_to_f32>);
static_assert(instruction::cast_i32_to_f32          == instruction::index_of<instructions::cast_i32_to_f32>);
static_assert(instruction::cast_i64_to_f32          == instruction::index_of<instructions::cast_i64_to_f32>);
static_assert(instruction::cast_u8_to_f32           == instruction::index_of<instructions::cast_u8_to_f32>);
static_assert(instruction::cast_u16_to_f32          == instruction::index_of<instructions::cast_u16_to_f32>);
static_assert(instruction::cast_u32_to_f32          == instruction::index_of<instructions::cast_u32_to_f32>);
static_assert(instruction::cast_u64_to_f32          == instruction::index_of<instructions::cast_u64_to_f32>);
static_assert(instruction::cast_i8_to_f64           == instruction::index_of<instructions::cast_i8_to_f64>);
static_assert(instruction::cast_i16_to_f64          == instruction::index_of<instructions::cast_i16_to_f64>);
static_assert(instruction::cast_i32_to_f64          == instruction::index_of<instructions::cast_i32_to_f64>);
static_assert(instruction::cast_i64_to_f64          == instruction::index_of<instructions::cast_i64_to_f64>);
static_assert(instruction::cast_u8_to_f64           == instruction::index_of<instructions::cast_u8_to_f64>);
static_assert(instruction::cast_u16_to_f64          == instruction::index_of<instructions::cast_u16_to_f64>);
static_assert(instruction::cast_u32_to_f64          == instruction::index_of<instructions::cast_u32_to_f64>);
static_assert(instruction::cast_u64_to_f64          == instruction::index_of<instructions::cast_u64_to_f64>);
static_assert(instruction::cmp_eq_i1                == instruction::index_of<instructions::cmp_eq_i1>);
static_assert(instruction::cmp_eq_i8                == instruction::index_of<instructions::cmp_eq_i8>);
static_assert(instruction::cmp_eq_i16               == instruction::index_of<instructions::cmp_eq_i16>);
static_assert(instruction::cmp_eq_i32               == instruction::index_of<instructions::cmp_eq_i32>);
static_assert(instruction::cmp_eq_i64               == instruction::index_of<instructions::cmp_eq_i64>);
static_assert(instruction::cmp_eq_f32               == instruction::index_of<instructions::cmp_eq_f32>);
static_assert(instruction::cmp_eq_f64               == instruction::index_of<instructions::cmp_eq_f64>);
static_assert(instruction::cmp_eq_f32_check         == instruction::index_of<instructions::cmp_eq_f32_check>);
static_assert(instruction::cmp_eq_f64_check         == instruction::index_of<instructions::cmp_eq_f64_check>);
static_assert(instruction::cmp_eq_ptr               == instruction::index_of<instructions::cmp_eq_ptr>);
static_assert(instruction::cmp_neq_i1               == instruction::index_of<instructions::cmp_neq_i1>);
static_assert(instruction::cmp_neq_i8               == instruction::index_of<instructions::cmp_neq_i8>);
static_assert(instruction::cmp_neq_i16              == instruction::index_of<instructions::cmp_neq_i16>);
static_assert(instruction::cmp_neq_i32              == instruction::index_of<instructions::cmp_neq_i32>);
static_assert(instruction::cmp_neq_i64              == instruction::index_of<instructions::cmp_neq_i64>);
static_assert(instruction::cmp_neq_f32              == instruction::index_of<instructions::cmp_neq_f32>);
static_assert(instruction::cmp_neq_f64              == instruction::index_of<instructions::cmp_neq_f64>);
static_assert(instruction::cmp_neq_f32_check        == instruction::index_of<instructions::cmp_neq_f32_check>);
static_assert(instruction::cmp_neq_f64_check        == instruction::index_of<instructions::cmp_neq_f64_check>);
static_assert(instruction::cmp_neq_ptr              == instruction::index_of<instructions::cmp_neq_ptr>);
static_assert(instruction::cmp_lt_i8                == instruction::index_of<instructions::cmp_lt_i8>);
static_assert(instruction::cmp_lt_i16               == instruction::index_of<instructions::cmp_lt_i16>);
static_assert(instruction::cmp_lt_i32               == instruction::index_of<instructions::cmp_lt_i32>);
static_assert(instruction::cmp_lt_i64               == instruction::index_of<instructions::cmp_lt_i64>);
static_assert(instruction::cmp_lt_u8                == instruction::index_of<instructions::cmp_lt_u8>);
static_assert(instruction::cmp_lt_u16               == instruction::index_of<instructions::cmp_lt_u16>);
static_assert(instruction::cmp_lt_u32               == instruction::index_of<instructions::cmp_lt_u32>);
static_assert(instruction::cmp_lt_u64               == instruction::index_of<instructions::cmp_lt_u64>);
static_assert(instruction::cmp_lt_f32               == instruction::index_of<instructions::cmp_lt_f32>);
static_assert(instruction::cmp_lt_f64               == instruction::index_of<instructions::cmp_lt_f64>);
static_assert(instruction::cmp_lt_f32_check         == instruction::index_of<instructions::cmp_lt_f32_check>);
static_assert(instruction::cmp_lt_f64_check         == instruction::index_of<instructions::cmp_lt_f64_check>);
static_assert(instruction::cmp_lt_ptr               == instruction::index_of<instructions::cmp_lt_ptr>);
static_assert(instruction::cmp_gt_i8                == instruction::index_of<instructions::cmp_gt_i8>);
static_assert(instruction::cmp_gt_i16               == instruction::index_of<instructions::cmp_gt_i16>);
static_assert(instruction::cmp_gt_i32               == instruction::index_of<instructions::cmp_gt_i32>);
static_assert(instruction::cmp_gt_i64               == instruction::index_of<instructions::cmp_gt_i64>);
static_assert(instruction::cmp_gt_u8                == instruction::index_of<instructions::cmp_gt_u8>);
static_assert(instruction::cmp_gt_u16               == instruction::index_of<instructions::cmp_gt_u16>);
static_assert(instruction::cmp_gt_u32               == instruction::index_of<instructions::cmp_gt_u32>);
static_assert(instruction::cmp_gt_u64               == instruction::index_of<instructions::cmp_gt_u64>);
static_assert(instruction::cmp_gt_f32               == instruction::index_of<instructions::cmp_gt_f32>);
static_assert(instruction::cmp_gt_f64               == instruction::index_of<instructions::cmp_gt_f64>);
static_assert(instruction::cmp_gt_f32_check         == instruction::index_of<instructions::cmp_gt_f32_check>);
static_assert(instruction::cmp_gt_f64_check         == instruction::index_of<instructions::cmp_gt_f64_check>);
static_assert(instruction::cmp_gt_ptr               == instruction::index_of<instructions::cmp_gt_ptr>);
static_assert(instruction::cmp_lte_i8               == instruction::index_of<instructions::cmp_lte_i8>);
static_assert(instruction::cmp_lte_i16              == instruction::index_of<instructions::cmp_lte_i16>);
static_assert(instruction::cmp_lte_i32              == instruction::index_of<instructions::cmp_lte_i32>);
static_assert(instruction::cmp_lte_i64              == instruction::index_of<instructions::cmp_lte_i64>);
static_assert(instruction::cmp_lte_u8               == instruction::index_of<instructions::cmp_lte_u8>);
static_assert(instruction::cmp_lte_u16              == instruction::index_of<instructions::cmp_lte_u16>);
static_assert(instruction::cmp_lte_u32              == instruction::index_of<instructions::cmp_lte_u32>);
static_assert(instruction::cmp_lte_u64              == instruction::index_of<instructions::cmp_lte_u64>);
static_assert(instruction::cmp_lte_f32              == instruction::index_of<instructions::cmp_lte_f32>);
static_assert(instruction::cmp_lte_f64              == instruction::index_of<instructions::cmp_lte_f64>);
static_assert(instruction::cmp_lte_f32_check        == instruction::index_of<instructions::cmp_lte_f32_check>);
static_assert(instruction::cmp_lte_f64_check        == instruction::index_of<instructions::cmp_lte_f64_check>);
static_assert(instruction::cmp_lte_ptr              == instruction::index_of<instructions::cmp_lte_ptr>);
static_assert(instruction::cmp_gte_i8               == instruction::index_of<instructions::cmp_gte_i8>);
static_assert(instruction::cmp_gte_i16              == instruction::index_of<instructions::cmp_gte_i16>);
static_assert(instruction::cmp_gte_i32              == instruction::index_of<instructions::cmp_gte_i32>);
static_assert(instruction::cmp_gte_i64              == instruction::index_of<instructions::cmp_gte_i64>);
static_assert(instruction::cmp_gte_u8               == instruction::index_of<instructions::cmp_gte_u8>);
static_assert(instruction::cmp_gte_u16              == instruction::index_of<instructions::cmp_gte_u16>);
static_assert(instruction::cmp_gte_u32              == instruction::index_of<instructions::cmp_gte_u32>);
static_assert(instruction::cmp_gte_u64              == instruction::index_of<instructions::cmp_gte_u64>);
static_assert(instruction::cmp_gte_f32              == instruction::index_of<instructions::cmp_gte_f32>);
static_assert(instruction::cmp_gte_f64              == instruction::index_of<instructions::cmp_gte_f64>);
static_assert(instruction::cmp_gte_f32_check        == instruction::index_of<instructions::cmp_gte_f32_check>);
static_assert(instruction::cmp_gte_f64_check        == instruction::index_of<instructions::cmp_gte_f64_check>);
static_assert(instruction::cmp_gte_ptr              == instruction::index_of<instructions::cmp_gte_ptr>);
static_assert(instruction::neg_i8                   == instruction::index_of<instructions::neg_i8>);
static_assert(instruction::neg_i16                  == instruction::index_of<instructions::neg_i16>);
static_assert(instruction::neg_i32                  == instruction::index_of<instructions::neg_i32>);
static_assert(instruction::neg_i64                  == instruction::index_of<instructions::neg_i64>);
static_assert(instruction::neg_f32                  == instruction::index_of<instructions::neg_f32>);
static_assert(instruction::neg_f64                  == instruction::index_of<instructions::neg_f64>);
static_assert(instruction::neg_i8_check             == instruction::index_of<instructions::neg_i8_check>);
static_assert(instruction::neg_i16_check            == instruction::index_of<instructions::neg_i16_check>);
static_assert(instruction::neg_i32_check            == instruction::index_of<instructions::neg_i32_check>);
static_assert(instruction::neg_i64_check            == instruction::index_of<instructions::neg_i64_check>);
static_assert(instruction::add_i8                   == instruction::index_of<instructions::add_i8>);
static_assert(instruction::add_i16                  == instruction::index_of<instructions::add_i16>);
static_assert(instruction::add_i32                  == instruction::index_of<instructions::add_i32>);
static_assert(instruction::add_i64                  == instruction::index_of<instructions::add_i64>);
static_assert(instruction::add_f32                  == instruction::index_of<instructions::add_f32>);
static_assert(instruction::add_f64                  == instruction::index_of<instructions::add_f64>);
static_assert(instruction::add_ptr_i32              == instruction::index_of<instructions::add_ptr_i32>);
static_assert(instruction::add_ptr_u32              == instruction::index_of<instructions::add_ptr_u32>);
static_assert(instruction::add_ptr_i64              == instruction::index_of<instructions::add_ptr_i64>);
static_assert(instruction::add_ptr_u64              == instruction::index_of<instructions::add_ptr_u64>);
static_assert(instruction::add_ptr_const_unchecked  == instruction::index_of<instructions::add_ptr_const_unchecked>);
static_assert(instruction::add_i8_check             == instruction::index_of<instructions::add_i8_check>);
static_assert(instruction::add_i16_check            == instruction::index_of<instructions::add_i16_check>);
static_assert(instruction::add_i32_check            == instruction::index_of<instructions::add_i32_check>);
static_assert(instruction::add_i64_check            == instruction::index_of<instructions::add_i64_check>);
static_assert(instruction::add_u8_check             == instruction::index_of<instructions::add_u8_check>);
static_assert(instruction::add_u16_check            == instruction::index_of<instructions::add_u16_check>);
static_assert(instruction::add_u32_check            == instruction::index_of<instructions::add_u32_check>);
static_assert(instruction::add_u64_check            == instruction::index_of<instructions::add_u64_check>);
static_assert(instruction::add_f32_check            == instruction::index_of<instructions::add_f32_check>);
static_assert(instruction::add_f64_check            == instruction::index_of<instructions::add_f64_check>);
static_assert(instruction::sub_i8                   == instruction::index_of<instructions::sub_i8>);
static_assert(instruction::sub_i16                  == instruction::index_of<instructions::sub_i16>);
static_assert(instruction::sub_i32                  == instruction::index_of<instructions::sub_i32>);
static_assert(instruction::sub_i64                  == instruction::index_of<instructions::sub_i64>);
static_assert(instruction::sub_f32                  == instruction::index_of<instructions::sub_f32>);
static_assert(instruction::sub_f64                  == instruction::index_of<instructions::sub_f64>);
static_assert(instruction::sub_ptr_i32              == instruction::index_of<instructions::sub_ptr_i32>);
static_assert(instruction::sub_ptr_u32              == instruction::index_of<instructions::sub_ptr_u32>);
static_assert(instruction::sub_ptr_i64              == instruction::index_of<instructions::sub_ptr_i64>);
static_assert(instruction::sub_ptr_u64              == instruction::index_of<instructions::sub_ptr_u64>);
static_assert(instruction::sub_i8_check             == instruction::index_of<instructions::sub_i8_check>);
static_assert(instruction::sub_i16_check            == instruction::index_of<instructions::sub_i16_check>);
static_assert(instruction::sub_i32_check            == instruction::index_of<instructions::sub_i32_check>);
static_assert(instruction::sub_i64_check            == instruction::index_of<instructions::sub_i64_check>);
static_assert(instruction::sub_u8_check             == instruction::index_of<instructions::sub_u8_check>);
static_assert(instruction::sub_u16_check            == instruction::index_of<instructions::sub_u16_check>);
static_assert(instruction::sub_u32_check            == instruction::index_of<instructions::sub_u32_check>);
static_assert(instruction::sub_u64_check            == instruction::index_of<instructions::sub_u64_check>);
static_assert(instruction::sub_f32_check            == instruction::index_of<instructions::sub_f32_check>);
static_assert(instruction::sub_f64_check            == instruction::index_of<instructions::sub_f64_check>);
static_assert(instruction::ptr32_diff               == instruction::index_of<instructions::ptr32_diff>);
static_assert(instruction::ptr64_diff               == instruction::index_of<instructions::ptr64_diff>);
static_assert(instruction::ptr32_diff_unchecked     == instruction::index_of<instructions::ptr32_diff_unchecked>);
static_assert(instruction::ptr64_diff_unchecked     == instruction::index_of<instructions::ptr64_diff_unchecked>);
static_assert(instruction::mul_i8                   == instruction::index_of<instructions::mul_i8>);
static_assert(instruction::mul_i16                  == instruction::index_of<instructions::mul_i16>);
static_assert(instruction::mul_i32                  == instruction::index_of<instructions::mul_i32>);
static_assert(instruction::mul_i64                  == instruction::index_of<instructions::mul_i64>);
static_assert(instruction::mul_f32                  == instruction::index_of<instructions::mul_f32>);
static_assert(instruction::mul_f64                  == instruction::index_of<instructions::mul_f64>);
static_assert(instruction::mul_i8_check             == instruction::index_of<instructions::mul_i8_check>);
static_assert(instruction::mul_i16_check            == instruction::index_of<instructions::mul_i16_check>);
static_assert(instruction::mul_i32_check            == instruction::index_of<instructions::mul_i32_check>);
static_assert(instruction::mul_i64_check            == instruction::index_of<instructions::mul_i64_check>);
static_assert(instruction::mul_u8_check             == instruction::index_of<instructions::mul_u8_check>);
static_assert(instruction::mul_u16_check            == instruction::index_of<instructions::mul_u16_check>);
static_assert(instruction::mul_u32_check            == instruction::index_of<instructions::mul_u32_check>);
static_assert(instruction::mul_u64_check            == instruction::index_of<instructions::mul_u64_check>);
static_assert(instruction::mul_f32_check            == instruction::index_of<instructions::mul_f32_check>);
static_assert(instruction::mul_f64_check            == instruction::index_of<instructions::mul_f64_check>);
static_assert(instruction::div_i8                   == instruction::index_of<instructions::div_i8>);
static_assert(instruction::div_i16                  == instruction::index_of<instructions::div_i16>);
static_assert(instruction::div_i32                  == instruction::index_of<instructions::div_i32>);
static_assert(instruction::div_i64                  == instruction::index_of<instructions::div_i64>);
static_assert(instruction::div_u8                   == instruction::index_of<instructions::div_u8>);
static_assert(instruction::div_u16                  == instruction::index_of<instructions::div_u16>);
static_assert(instruction::div_u32                  == instruction::index_of<instructions::div_u32>);
static_assert(instruction::div_u64                  == instruction::index_of<instructions::div_u64>);
static_assert(instruction::div_f32                  == instruction::index_of<instructions::div_f32>);
static_assert(instruction::div_f64                  == instruction::index_of<instructions::div_f64>);
static_assert(instruction::div_i8_check             == instruction::index_of<instructions::div_i8_check>);
static_assert(instruction::div_i16_check            == instruction::index_of<instructions::div_i16_check>);
static_assert(instruction::div_i32_check            == instruction::index_of<instructions::div_i32_check>);
static_assert(instruction::div_i64_check            == instruction::index_of<instructions::div_i64_check>);
static_assert(instruction::div_f32_check            == instruction::index_of<instructions::div_f32_check>);
static_assert(instruction::div_f64_check            == instruction::index_of<instructions::div_f64_check>);
static_assert(instruction::rem_i8                   == instruction::index_of<instructions::rem_i8>);
static_assert(instruction::rem_i16                  == instruction::index_of<instructions::rem_i16>);
static_assert(instruction::rem_i32                  == instruction::index_of<instructions::rem_i32>);
static_assert(instruction::rem_i64                  == instruction::index_of<instructions::rem_i64>);
static_assert(instruction::rem_u8                   == instruction::index_of<instructions::rem_u8>);
static_assert(instruction::rem_u16                  == instruction::index_of<instructions::rem_u16>);
static_assert(instruction::rem_u32                  == instruction::index_of<instructions::rem_u32>);
static_assert(instruction::rem_u64                  == instruction::index_of<instructions::rem_u64>);
static_assert(instruction::not_i1                   == instruction::index_of<instructions::not_i1>);
static_assert(instruction::not_i8                   == instruction::index_of<instructions::not_i8>);
static_assert(instruction::not_i16                  == instruction::index_of<instructions::not_i16>);
static_assert(instruction::not_i32                  == instruction::index_of<instructions::not_i32>);
static_assert(instruction::not_i64                  == instruction::index_of<instructions::not_i64>);
static_assert(instruction::and_i1                   == instruction::index_of<instructions::and_i1>);
static_assert(instruction::and_i8                   == instruction::index_of<instructions::and_i8>);
static_assert(instruction::and_i16                  == instruction::index_of<instructions::and_i16>);
static_assert(instruction::and_i32                  == instruction::index_of<instructions::and_i32>);
static_assert(instruction::and_i64                  == instruction::index_of<instructions::and_i64>);
static_assert(instruction::xor_i1                   == instruction::index_of<instructions::xor_i1>);
static_assert(instruction::xor_i8                   == instruction::index_of<instructions::xor_i8>);
static_assert(instruction::xor_i16                  == instruction::index_of<instructions::xor_i16>);
static_assert(instruction::xor_i32                  == instruction::index_of<instructions::xor_i32>);
static_assert(instruction::xor_i64                  == instruction::index_of<instructions::xor_i64>);
static_assert(instruction::or_i1                    == instruction::index_of<instructions::or_i1>);
static_assert(instruction::or_i8                    == instruction::index_of<instructions::or_i8>);
static_assert(instruction::or_i16                   == instruction::index_of<instructions::or_i16>);
static_assert(instruction::or_i32                   == instruction::index_of<instructions::or_i32>);
static_assert(instruction::or_i64                   == instruction::index_of<instructions::or_i64>);
static_assert(instruction::shl_i8_signed            == instruction::index_of<instructions::shl_i8_signed>);
static_assert(instruction::shl_i16_signed           == instruction::index_of<instructions::shl_i16_signed>);
static_assert(instruction::shl_i32_signed           == instruction::index_of<instructions::shl_i32_signed>);
static_assert(instruction::shl_i64_signed           == instruction::index_of<instructions::shl_i64_signed>);
static_assert(instruction::shl_i8_unsigned          == instruction::index_of<instructions::shl_i8_unsigned>);
static_assert(instruction::shl_i16_unsigned         == instruction::index_of<instructions::shl_i16_unsigned>);
static_assert(instruction::shl_i32_unsigned         == instruction::index_of<instructions::shl_i32_unsigned>);
static_assert(instruction::shl_i64_unsigned         == instruction::index_of<instructions::shl_i64_unsigned>);
static_assert(instruction::shr_i8_signed            == instruction::index_of<instructions::shr_i8_signed>);
static_assert(instruction::shr_i16_signed           == instruction::index_of<instructions::shr_i16_signed>);
static_assert(instruction::shr_i32_signed           == instruction::index_of<instructions::shr_i32_signed>);
static_assert(instruction::shr_i64_signed           == instruction::index_of<instructions::shr_i64_signed>);
static_assert(instruction::shr_i8_unsigned          == instruction::index_of<instructions::shr_i8_unsigned>);
static_assert(instruction::shr_i16_unsigned         == instruction::index_of<instructions::shr_i16_unsigned>);
static_assert(instruction::shr_i32_unsigned         == instruction::index_of<instructions::shr_i32_unsigned>);
static_assert(instruction::shr_i64_unsigned         == instruction::index_of<instructions::shr_i64_unsigned>);
static_assert(instruction::abs_i8                   == instruction::index_of<instructions::abs_i8>);
static_assert(instruction::abs_i16                  == instruction::index_of<instructions::abs_i16>);
static_assert(instruction::abs_i32                  == instruction::index_of<instructions::abs_i32>);
static_assert(instruction::abs_i64                  == instruction::index_of<instructions::abs_i64>);
static_assert(instruction::abs_f32                  == instruction::index_of<instructions::abs_f32>);
static_assert(instruction::abs_f64                  == instruction::index_of<instructions::abs_f64>);
static_assert(instruction::abs_i8_check             == instruction::index_of<instructions::abs_i8_check>);
static_assert(instruction::abs_i16_check            == instruction::index_of<instructions::abs_i16_check>);
static_assert(instruction::abs_i32_check            == instruction::index_of<instructions::abs_i32_check>);
static_assert(instruction::abs_i64_check            == instruction::index_of<instructions::abs_i64_check>);
static_assert(instruction::abs_f32_check            == instruction::index_of<instructions::abs_f32_check>);
static_assert(instruction::abs_f64_check            == instruction::index_of<instructions::abs_f64_check>);
static_assert(instruction::min_i8                   == instruction::index_of<instructions::min_i8>);
static_assert(instruction::min_i16                  == instruction::index_of<instructions::min_i16>);
static_assert(instruction::min_i32                  == instruction::index_of<instructions::min_i32>);
static_assert(instruction::min_i64                  == instruction::index_of<instructions::min_i64>);
static_assert(instruction::min_u8                   == instruction::index_of<instructions::min_u8>);
static_assert(instruction::min_u16                  == instruction::index_of<instructions::min_u16>);
static_assert(instruction::min_u32                  == instruction::index_of<instructions::min_u32>);
static_assert(instruction::min_u64                  == instruction::index_of<instructions::min_u64>);
static_assert(instruction::min_f32                  == instruction::index_of<instructions::min_f32>);
static_assert(instruction::min_f64                  == instruction::index_of<instructions::min_f64>);
static_assert(instruction::min_f32_check            == instruction::index_of<instructions::min_f32_check>);
static_assert(instruction::min_f64_check            == instruction::index_of<instructions::min_f64_check>);
static_assert(instruction::max_i8                   == instruction::index_of<instructions::max_i8>);
static_assert(instruction::max_i16                  == instruction::index_of<instructions::max_i16>);
static_assert(instruction::max_i32                  == instruction::index_of<instructions::max_i32>);
static_assert(instruction::max_i64                  == instruction::index_of<instructions::max_i64>);
static_assert(instruction::max_u8                   == instruction::index_of<instructions::max_u8>);
static_assert(instruction::max_u16                  == instruction::index_of<instructions::max_u16>);
static_assert(instruction::max_u32                  == instruction::index_of<instructions::max_u32>);
static_assert(instruction::max_u64                  == instruction::index_of<instructions::max_u64>);
static_assert(instruction::max_f32                  == instruction::index_of<instructions::max_f32>);
static_assert(instruction::max_f64                  == instruction::index_of<instructions::max_f64>);
static_assert(instruction::max_f32_check            == instruction::index_of<instructions::max_f32_check>);
static_assert(instruction::max_f64_check            == instruction::index_of<instructions::max_f64_check>);
static_assert(instruction::exp_f32                  == instruction::index_of<instructions::exp_f32>);
static_assert(instruction::exp_f64                  == instruction::index_of<instructions::exp_f64>);
static_assert(instruction::exp_f32_check            == instruction::index_of<instructions::exp_f32_check>);
static_assert(instruction::exp_f64_check            == instruction::index_of<instructions::exp_f64_check>);
static_assert(instruction::exp2_f32                 == instruction::index_of<instructions::exp2_f32>);
static_assert(instruction::exp2_f64                 == instruction::index_of<instructions::exp2_f64>);
static_assert(instruction::exp2_f32_check           == instruction::index_of<instructions::exp2_f32_check>);
static_assert(instruction::exp2_f64_check           == instruction::index_of<instructions::exp2_f64_check>);
static_assert(instruction::expm1_f32                == instruction::index_of<instructions::expm1_f32>);
static_assert(instruction::expm1_f64                == instruction::index_of<instructions::expm1_f64>);
static_assert(instruction::expm1_f32_check          == instruction::index_of<instructions::expm1_f32_check>);
static_assert(instruction::expm1_f64_check          == instruction::index_of<instructions::expm1_f64_check>);
static_assert(instruction::log_f32                  == instruction::index_of<instructions::log_f32>);
static_assert(instruction::log_f64                  == instruction::index_of<instructions::log_f64>);
static_assert(instruction::log_f32_check            == instruction::index_of<instructions::log_f32_check>);
static_assert(instruction::log_f64_check            == instruction::index_of<instructions::log_f64_check>);
static_assert(instruction::log10_f32                == instruction::index_of<instructions::log10_f32>);
static_assert(instruction::log10_f64                == instruction::index_of<instructions::log10_f64>);
static_assert(instruction::log10_f32_check          == instruction::index_of<instructions::log10_f32_check>);
static_assert(instruction::log10_f64_check          == instruction::index_of<instructions::log10_f64_check>);
static_assert(instruction::log2_f32                 == instruction::index_of<instructions::log2_f32>);
static_assert(instruction::log2_f64                 == instruction::index_of<instructions::log2_f64>);
static_assert(instruction::log2_f32_check           == instruction::index_of<instructions::log2_f32_check>);
static_assert(instruction::log2_f64_check           == instruction::index_of<instructions::log2_f64_check>);
static_assert(instruction::log1p_f32                == instruction::index_of<instructions::log1p_f32>);
static_assert(instruction::log1p_f64                == instruction::index_of<instructions::log1p_f64>);
static_assert(instruction::log1p_f32_check          == instruction::index_of<instructions::log1p_f32_check>);
static_assert(instruction::log1p_f64_check          == instruction::index_of<instructions::log1p_f64_check>);
static_assert(instruction::sqrt_f32                 == instruction::index_of<instructions::sqrt_f32>);
static_assert(instruction::sqrt_f64                 == instruction::index_of<instructions::sqrt_f64>);
static_assert(instruction::sqrt_f32_check           == instruction::index_of<instructions::sqrt_f32_check>);
static_assert(instruction::sqrt_f64_check           == instruction::index_of<instructions::sqrt_f64_check>);
static_assert(instruction::pow_f32                  == instruction::index_of<instructions::pow_f32>);
static_assert(instruction::pow_f64                  == instruction::index_of<instructions::pow_f64>);
static_assert(instruction::pow_f32_check            == instruction::index_of<instructions::pow_f32_check>);
static_assert(instruction::pow_f64_check            == instruction::index_of<instructions::pow_f64_check>);
static_assert(instruction::cbrt_f32                 == instruction::index_of<instructions::cbrt_f32>);
static_assert(instruction::cbrt_f64                 == instruction::index_of<instructions::cbrt_f64>);
static_assert(instruction::cbrt_f32_check           == instruction::index_of<instructions::cbrt_f32_check>);
static_assert(instruction::cbrt_f64_check           == instruction::index_of<instructions::cbrt_f64_check>);
static_assert(instruction::hypot_f32                == instruction::index_of<instructions::hypot_f32>);
static_assert(instruction::hypot_f64                == instruction::index_of<instructions::hypot_f64>);
static_assert(instruction::hypot_f32_check          == instruction::index_of<instructions::hypot_f32_check>);
static_assert(instruction::hypot_f64_check          == instruction::index_of<instructions::hypot_f64_check>);
static_assert(instruction::sin_f32                  == instruction::index_of<instructions::sin_f32>);
static_assert(instruction::sin_f64                  == instruction::index_of<instructions::sin_f64>);
static_assert(instruction::sin_f32_check            == instruction::index_of<instructions::sin_f32_check>);
static_assert(instruction::sin_f64_check            == instruction::index_of<instructions::sin_f64_check>);
static_assert(instruction::cos_f32                  == instruction::index_of<instructions::cos_f32>);
static_assert(instruction::cos_f64                  == instruction::index_of<instructions::cos_f64>);
static_assert(instruction::cos_f32_check            == instruction::index_of<instructions::cos_f32_check>);
static_assert(instruction::cos_f64_check            == instruction::index_of<instructions::cos_f64_check>);
static_assert(instruction::tan_f32                  == instruction::index_of<instructions::tan_f32>);
static_assert(instruction::tan_f64                  == instruction::index_of<instructions::tan_f64>);
static_assert(instruction::tan_f32_check            == instruction::index_of<instructions::tan_f32_check>);
static_assert(instruction::tan_f64_check            == instruction::index_of<instructions::tan_f64_check>);
static_assert(instruction::asin_f32                 == instruction::index_of<instructions::asin_f32>);
static_assert(instruction::asin_f64                 == instruction::index_of<instructions::asin_f64>);
static_assert(instruction::asin_f32_check           == instruction::index_of<instructions::asin_f32_check>);
static_assert(instruction::asin_f64_check           == instruction::index_of<instructions::asin_f64_check>);
static_assert(instruction::acos_f32                 == instruction::index_of<instructions::acos_f32>);
static_assert(instruction::acos_f64                 == instruction::index_of<instructions::acos_f64>);
static_assert(instruction::acos_f32_check           == instruction::index_of<instructions::acos_f32_check>);
static_assert(instruction::acos_f64_check           == instruction::index_of<instructions::acos_f64_check>);
static_assert(instruction::atan_f32                 == instruction::index_of<instructions::atan_f32>);
static_assert(instruction::atan_f64                 == instruction::index_of<instructions::atan_f64>);
static_assert(instruction::atan_f32_check           == instruction::index_of<instructions::atan_f32_check>);
static_assert(instruction::atan_f64_check           == instruction::index_of<instructions::atan_f64_check>);
static_assert(instruction::atan2_f32                == instruction::index_of<instructions::atan2_f32>);
static_assert(instruction::atan2_f64                == instruction::index_of<instructions::atan2_f64>);
static_assert(instruction::atan2_f32_check          == instruction::index_of<instructions::atan2_f32_check>);
static_assert(instruction::atan2_f64_check          == instruction::index_of<instructions::atan2_f64_check>);
static_assert(instruction::sinh_f32                 == instruction::index_of<instructions::sinh_f32>);
static_assert(instruction::sinh_f64                 == instruction::index_of<instructions::sinh_f64>);
static_assert(instruction::sinh_f32_check           == instruction::index_of<instructions::sinh_f32_check>);
static_assert(instruction::sinh_f64_check           == instruction::index_of<instructions::sinh_f64_check>);
static_assert(instruction::cosh_f32                 == instruction::index_of<instructions::cosh_f32>);
static_assert(instruction::cosh_f64                 == instruction::index_of<instructions::cosh_f64>);
static_assert(instruction::cosh_f32_check           == instruction::index_of<instructions::cosh_f32_check>);
static_assert(instruction::cosh_f64_check           == instruction::index_of<instructions::cosh_f64_check>);
static_assert(instruction::tanh_f32                 == instruction::index_of<instructions::tanh_f32>);
static_assert(instruction::tanh_f64                 == instruction::index_of<instructions::tanh_f64>);
static_assert(instruction::tanh_f32_check           == instruction::index_of<instructions::tanh_f32_check>);
static_assert(instruction::tanh_f64_check           == instruction::index_of<instructions::tanh_f64_check>);
static_assert(instruction::asinh_f32                == instruction::index_of<instructions::asinh_f32>);
static_assert(instruction::asinh_f64                == instruction::index_of<instructions::asinh_f64>);
static_assert(instruction::asinh_f32_check          == instruction::index_of<instructions::asinh_f32_check>);
static_assert(instruction::asinh_f64_check          == instruction::index_of<instructions::asinh_f64_check>);
static_assert(instruction::acosh_f32                == instruction::index_of<instructions::acosh_f32>);
static_assert(instruction::acosh_f64                == instruction::index_of<instructions::acosh_f64>);
static_assert(instruction::acosh_f32_check          == instruction::index_of<instructions::acosh_f32_check>);
static_assert(instruction::acosh_f64_check          == instruction::index_of<instructions::acosh_f64_check>);
static_assert(instruction::atanh_f32                == instruction::index_of<instructions::atanh_f32>);
static_assert(instruction::atanh_f64                == instruction::index_of<instructions::atanh_f64>);
static_assert(instruction::atanh_f32_check          == instruction::index_of<instructions::atanh_f32_check>);
static_assert(instruction::atanh_f64_check          == instruction::index_of<instructions::atanh_f64_check>);
static_assert(instruction::erf_f32                  == instruction::index_of<instructions::erf_f32>);
static_assert(instruction::erf_f64                  == instruction::index_of<instructions::erf_f64>);
static_assert(instruction::erf_f32_check            == instruction::index_of<instructions::erf_f32_check>);
static_assert(instruction::erf_f64_check            == instruction::index_of<instructions::erf_f64_check>);
static_assert(instruction::erfc_f32                 == instruction::index_of<instructions::erfc_f32>);
static_assert(instruction::erfc_f64                 == instruction::index_of<instructions::erfc_f64>);
static_assert(instruction::erfc_f32_check           == instruction::index_of<instructions::erfc_f32_check>);
static_assert(instruction::erfc_f64_check           == instruction::index_of<instructions::erfc_f64_check>);
static_assert(instruction::tgamma_f32               == instruction::index_of<instructions::tgamma_f32>);
static_assert(instruction::tgamma_f64               == instruction::index_of<instructions::tgamma_f64>);
static_assert(instruction::tgamma_f32_check         == instruction::index_of<instructions::tgamma_f32_check>);
static_assert(instruction::tgamma_f64_check         == instruction::index_of<instructions::tgamma_f64_check>);
static_assert(instruction::lgamma_f32               == instruction::index_of<instructions::lgamma_f32>);
static_assert(instruction::lgamma_f64               == instruction::index_of<instructions::lgamma_f64>);
static_assert(instruction::lgamma_f32_check         == instruction::index_of<instructions::lgamma_f32_check>);
static_assert(instruction::lgamma_f64_check         == instruction::index_of<instructions::lgamma_f64_check>);
static_assert(instruction::bitreverse_u8            == instruction::index_of<instructions::bitreverse_u8>);
static_assert(instruction::bitreverse_u16           == instruction::index_of<instructions::bitreverse_u16>);
static_assert(instruction::bitreverse_u32           == instruction::index_of<instructions::bitreverse_u32>);
static_assert(instruction::bitreverse_u64           == instruction::index_of<instructions::bitreverse_u64>);
static_assert(instruction::popcount_u8              == instruction::index_of<instructions::popcount_u8>);
static_assert(instruction::popcount_u16             == instruction::index_of<instructions::popcount_u16>);
static_assert(instruction::popcount_u32             == instruction::index_of<instructions::popcount_u32>);
static_assert(instruction::popcount_u64             == instruction::index_of<instructions::popcount_u64>);
static_assert(instruction::byteswap_u16             == instruction::index_of<instructions::byteswap_u16>);
static_assert(instruction::byteswap_u32             == instruction::index_of<instructions::byteswap_u32>);
static_assert(instruction::byteswap_u64             == instruction::index_of<instructions::byteswap_u64>);
static_assert(instruction::clz_u8                   == instruction::index_of<instructions::clz_u8>);
static_assert(instruction::clz_u16                  == instruction::index_of<instructions::clz_u16>);
static_assert(instruction::clz_u32                  == instruction::index_of<instructions::clz_u32>);
static_assert(instruction::clz_u64                  == instruction::index_of<instructions::clz_u64>);
static_assert(instruction::ctz_u8                   == instruction::index_of<instructions::ctz_u8>);
static_assert(instruction::ctz_u16                  == instruction::index_of<instructions::ctz_u16>);
static_assert(instruction::ctz_u32                  == instruction::index_of<instructions::ctz_u32>);
static_assert(instruction::ctz_u64                  == instruction::index_of<instructions::ctz_u64>);
static_assert(instruction::fshl_u8                  == instruction::index_of<instructions::fshl_u8>);
static_assert(instruction::fshl_u16                 == instruction::index_of<instructions::fshl_u16>);
static_assert(instruction::fshl_u32                 == instruction::index_of<instructions::fshl_u32>);
static_assert(instruction::fshl_u64                 == instruction::index_of<instructions::fshl_u64>);
static_assert(instruction::fshr_u8                  == instruction::index_of<instructions::fshr_u8>);
static_assert(instruction::fshr_u16                 == instruction::index_of<instructions::fshr_u16>);
static_assert(instruction::fshr_u32                 == instruction::index_of<instructions::fshr_u32>);
static_assert(instruction::fshr_u64                 == instruction::index_of<instructions::fshr_u64>);
static_assert(instruction::const_gep                == instruction::index_of<instructions::const_gep>);
static_assert(instruction::array_gep_i32            == instruction::index_of<instructions::array_gep_i32>);
static_assert(instruction::array_gep_i64            == instruction::index_of<instructions::array_gep_i64>);
static_assert(instruction::const_memcpy             == instruction::index_of<instructions::const_memcpy>);
static_assert(instruction::const_memset_zero        == instruction::index_of<instructions::const_memset_zero>);
static_assert(instruction::copy_values              == instruction::index_of<instructions::copy_values>);
static_assert(instruction::copy_overlapping_values  == instruction::index_of<instructions::copy_overlapping_values>);
static_assert(instruction::relocate_values          == instruction::index_of<instructions::relocate_values>);
static_assert(instruction::set_values_i1_be         == instruction::index_of<instructions::set_values_i1_be>);
static_assert(instruction::set_values_i8_be         == instruction::index_of<instructions::set_values_i8_be>);
static_assert(instruction::set_values_i16_be        == instruction::index_of<instructions::set_values_i16_be>);
static_assert(instruction::set_values_i32_be        == instruction::index_of<instructions::set_values_i32_be>);
static_assert(instruction::set_values_i64_be        == instruction::index_of<instructions::set_values_i64_be>);
static_assert(instruction::set_values_f32_be        == instruction::index_of<instructions::set_values_f32_be>);
static_assert(instruction::set_values_f64_be        == instruction::index_of<instructions::set_values_f64_be>);
static_assert(instruction::set_values_ptr32_be      == instruction::index_of<instructions::set_values_ptr32_be>);
static_assert(instruction::set_values_ptr64_be      == instruction::index_of<instructions::set_values_ptr64_be>);
static_assert(instruction::set_values_i1_le         == instruction::index_of<instructions::set_values_i1_le>);
static_assert(instruction::set_values_i8_le         == instruction::index_of<instructions::set_values_i8_le>);
static_assert(instruction::set_values_i16_le        == instruction::index_of<instructions::set_values_i16_le>);
static_assert(instruction::set_values_i32_le        == instruction::index_of<instructions::set_values_i32_le>);
static_assert(instruction::set_values_i64_le        == instruction::index_of<instructions::set_values_i64_le>);
static_assert(instruction::set_values_f32_le        == instruction::index_of<instructions::set_values_f32_le>);
static_assert(instruction::set_values_f64_le        == instruction::index_of<instructions::set_values_f64_le>);
static_assert(instruction::set_values_ptr32_le      == instruction::index_of<instructions::set_values_ptr32_le>);
static_assert(instruction::set_values_ptr64_le      == instruction::index_of<instructions::set_values_ptr64_le>);
static_assert(instruction::set_values_ref           == instruction::index_of<instructions::set_values_ref>);
static_assert(instruction::function_call            == instruction::index_of<instructions::function_call>);
static_assert(instruction::indirect_function_call   == instruction::index_of<instructions::indirect_function_call>);
static_assert(instruction::malloc                   == instruction::index_of<instructions::malloc>);
static_assert(instruction::free                     == instruction::index_of<instructions::free>);
static_assert(instruction::jump                     == instruction::index_of<instructions::jump>);
static_assert(instruction::conditional_jump         == instruction::index_of<instructions::conditional_jump>);
static_assert(instruction::switch_i1                == instruction::index_of<instructions::switch_i1>);
static_assert(instruction::switch_i8                == instruction::index_of<instructions::switch_i8>);
static_assert(instruction::switch_i16               == instruction::index_of<instructions::switch_i16>);
static_assert(instruction::switch_i32               == instruction::index_of<instructions::switch_i32>);
static_assert(instruction::switch_i64               == instruction::index_of<instructions::switch_i64>);
static_assert(instruction::switch_str               == instruction::index_of<instructions::switch_str>);
static_assert(instruction::ret                      == instruction::index_of<instructions::ret>);
static_assert(instruction::ret_void                 == instruction::index_of<instructions::ret_void>);
static_assert(instruction::unreachable              == instruction::index_of<instructions::unreachable>);
static_assert(instruction::error                    == instruction::index_of<instructions::error>);
static_assert(instruction::diagnostic_str           == instruction::index_of<instructions::diagnostic_str>);
static_assert(instruction::print                    == instruction::index_of<instructions::print>);
static_assert(instruction::is_option_set            == instruction::index_of<instructions::is_option_set>);
static_assert(instruction::add_global_array_data    == instruction::index_of<instructions::add_global_array_data>);
static_assert(instruction::array_bounds_check_i32   == instruction::index_of<instructions::array_bounds_check_i32>);
static_assert(instruction::array_bounds_check_u32   == instruction::index_of<instructions::array_bounds_check_u32>);
static_assert(instruction::array_bounds_check_i64   == instruction::index_of<instructions::array_bounds_check_i64>);
static_assert(instruction::array_bounds_check_u64   == instruction::index_of<instructions::array_bounds_check_u64>);
static_assert(instruction::optional_get_value_check == instruction::index_of<instructions::optional_get_value_check>);
static_assert(instruction::str_construction_check   == instruction::index_of<instructions::str_construction_check>);
static_assert(instruction::slice_construction_check == instruction::index_of<instructions::slice_construction_check>);
static_assert(instruction::start_lifetime           == instruction::index_of<instructions::start_lifetime>);
static_assert(instruction::end_lifetime             == instruction::index_of<instructions::end_lifetime>);

bz::u8string to_string(instruction const &inst_, function const *func)
{
	switch (inst_.index())
	{
	static_assert(instruction_list_t::size() == 544);
	case instruction::const_i1:
	{
		auto const &inst = inst_.get<instructions::const_i1>();
		return bz::format("const i1 {}", inst.value);
	}
	case instruction::const_i8:
	{
		auto const &inst = inst_.get<instructions::const_i8>();
		return bz::format("const i8 {}", inst.value);
	}
	case instruction::const_i16:
	{
		auto const &inst = inst_.get<instructions::const_i16>();
		return bz::format("const i16 {}", inst.value);
	}
	case instruction::const_i32:
	{
		auto const &inst = inst_.get<instructions::const_i32>();
		return bz::format("const i32 {}", inst.value);
	}
	case instruction::const_i64:
	{
		auto const &inst = inst_.get<instructions::const_i64>();
		return bz::format("const i64 {}", inst.value);
	}
	case instruction::const_u8:
	{
		auto const &inst = inst_.get<instructions::const_u8>();
		return bz::format("const u8 {}", inst.value);
	}
	case instruction::const_u16:
	{
		auto const &inst = inst_.get<instructions::const_u16>();
		return bz::format("const u16 {}", inst.value);
	}
	case instruction::const_u32:
	{
		auto const &inst = inst_.get<instructions::const_u32>();
		return bz::format("const u32 {}", inst.value);
	}
	case instruction::const_u64:
	{
		auto const &inst = inst_.get<instructions::const_u64>();
		return bz::format("const u64 {}", inst.value);
	}
	case instruction::const_f32:
	{
		auto const &inst = inst_.get<instructions::const_f32>();
		return bz::format("const f32 {}", inst.value);
	}
	case instruction::const_f64:
	{
		auto const &inst = inst_.get<instructions::const_f64>();
		return bz::format("const f64 {}", inst.value);
	}
	case instruction::const_ptr_null:
	{
		return "const ptr null";
	}
	case instruction::const_func_ptr:
	{
		auto const &inst = inst_.get<instructions::const_func_ptr>();
		return bz::format("const func-ptr 0x{:x}", inst.value);
	}
	case instruction::get_global_address:
	{
		auto const &inst = inst_.get<instructions::get_global_address>();
		return bz::format("get_global_address {}", inst.global_index);
	}
	case instruction::get_function_arg:
	{
		auto const &inst = inst_.get<instructions::get_function_arg>();
		return bz::format("get_function_arg {}", inst.arg_index);
	}
	case instruction::load_i1_be:
	{
		auto const &inst = inst_.get<instructions::load_i1_be>();
		return bz::format("load i1 be {}", inst.args[0]);
	}
	case instruction::load_i8_be:
	{
		auto const &inst = inst_.get<instructions::load_i8_be>();
		return bz::format("load i8 be {}", inst.args[0]);
	}
	case instruction::load_i16_be:
	{
		auto const &inst = inst_.get<instructions::load_i16_be>();
		return bz::format("load i16 be {}", inst.args[0]);
	}
	case instruction::load_i32_be:
	{
		auto const &inst = inst_.get<instructions::load_i32_be>();
		return bz::format("load i32 be {}", inst.args[0]);
	}
	case instruction::load_i64_be:
	{
		auto const &inst = inst_.get<instructions::load_i64_be>();
		return bz::format("load i64 be {}", inst.args[0]);
	}
	case instruction::load_f32_be:
	{
		auto const &inst = inst_.get<instructions::load_f32_be>();
		return bz::format("load f32 be {}", inst.args[0]);
	}
	case instruction::load_f64_be:
	{
		auto const &inst = inst_.get<instructions::load_f64_be>();
		return bz::format("load f64 be {}", inst.args[0]);
	}
	case instruction::load_ptr32_be:
	{
		auto const &inst = inst_.get<instructions::load_ptr32_be>();
		return bz::format("load ptr32 be {}", inst.args[0]);
	}
	case instruction::load_ptr64_be:
	{
		auto const &inst = inst_.get<instructions::load_ptr64_be>();
		return bz::format("load ptr64 be {}", inst.args[0]);
	}
	case instruction::load_i1_le:
	{
		auto const &inst = inst_.get<instructions::load_i1_le>();
		return bz::format("load i1 le {}", inst.args[0]);
	}
	case instruction::load_i8_le:
	{
		auto const &inst = inst_.get<instructions::load_i8_le>();
		return bz::format("load i8 le {}", inst.args[0]);
	}
	case instruction::load_i16_le:
	{
		auto const &inst = inst_.get<instructions::load_i16_le>();
		return bz::format("load i16 le {}", inst.args[0]);
	}
	case instruction::load_i32_le:
	{
		auto const &inst = inst_.get<instructions::load_i32_le>();
		return bz::format("load i32 le {}", inst.args[0]);
	}
	case instruction::load_i64_le:
	{
		auto const &inst = inst_.get<instructions::load_i64_le>();
		return bz::format("load i64 le {}", inst.args[0]);
	}
	case instruction::load_f32_le:
	{
		auto const &inst = inst_.get<instructions::load_f32_le>();
		return bz::format("load f32 le {}", inst.args[0]);
	}
	case instruction::load_f64_le:
	{
		auto const &inst = inst_.get<instructions::load_f64_le>();
		return bz::format("load f64 le {}", inst.args[0]);
	}
	case instruction::load_ptr32_le:
	{
		auto const &inst = inst_.get<instructions::load_ptr32_le>();
		return bz::format("load ptr32 le {}", inst.args[0]);
	}
	case instruction::load_ptr64_le:
	{
		auto const &inst = inst_.get<instructions::load_ptr64_le>();
		return bz::format("load ptr64 le {}", inst.args[0]);
	}
	case instruction::store_i1_be:
	{
		auto const &inst = inst_.get<instructions::store_i1_be>();
		return bz::format("store i1 be {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::store_i8_be:
	{
		auto const &inst = inst_.get<instructions::store_i8_be>();
		return bz::format("store i8 be {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::store_i16_be:
	{
		auto const &inst = inst_.get<instructions::store_i16_be>();
		return bz::format("store i16 be {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::store_i32_be:
	{
		auto const &inst = inst_.get<instructions::store_i32_be>();
		return bz::format("store i32 be {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::store_i64_be:
	{
		auto const &inst = inst_.get<instructions::store_i64_be>();
		return bz::format("store i64 be {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::store_f32_be:
	{
		auto const &inst = inst_.get<instructions::store_f32_be>();
		return bz::format("store f32 be {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::store_f64_be:
	{
		auto const &inst = inst_.get<instructions::store_f64_be>();
		return bz::format("store f64 be {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::store_ptr32_be:
	{
		auto const &inst = inst_.get<instructions::store_ptr32_be>();
		return bz::format("store ptr32 be {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::store_ptr64_be:
	{
		auto const &inst = inst_.get<instructions::store_ptr64_be>();
		return bz::format("store ptr64 be {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::store_i1_le:
	{
		auto const &inst = inst_.get<instructions::store_i1_le>();
		return bz::format("store i1 le {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::store_i8_le:
	{
		auto const &inst = inst_.get<instructions::store_i8_le>();
		return bz::format("store i8 le {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::store_i16_le:
	{
		auto const &inst = inst_.get<instructions::store_i16_le>();
		return bz::format("store i16 le {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::store_i32_le:
	{
		auto const &inst = inst_.get<instructions::store_i32_le>();
		return bz::format("store i32 le {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::store_i64_le:
	{
		auto const &inst = inst_.get<instructions::store_i64_le>();
		return bz::format("store i64 le {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::store_f32_le:
	{
		auto const &inst = inst_.get<instructions::store_f32_le>();
		return bz::format("store f32 le {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::store_f64_le:
	{
		auto const &inst = inst_.get<instructions::store_f64_le>();
		return bz::format("store f64 le {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::store_ptr32_le:
	{
		auto const &inst = inst_.get<instructions::store_ptr32_le>();
		return bz::format("store ptr32 le {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::store_ptr64_le:
	{
		auto const &inst = inst_.get<instructions::store_ptr64_le>();
		return bz::format("store ptr64 le {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::check_dereference:
	{
		auto const &inst = inst_.get<instructions::check_dereference>();
		return bz::format("check-dereference {} ({}, {})", inst.args[0], inst.src_tokens_index, inst.memory_access_check_info_index);
	}
	case instruction::check_inplace_construct:
	{
		auto const &inst = inst_.get<instructions::check_inplace_construct>();
		return bz::format("check-inplace-construct {} ({}, {})", inst.args[0], inst.src_tokens_index, inst.memory_access_check_info_index);
	}
	case instruction::check_destruct_value:
	{
		auto const &inst = inst_.get<instructions::check_destruct_value>();
		return bz::format("check-destruct-value {} ({}, {})", inst.args[0], inst.src_tokens_index, inst.memory_access_check_info_index);
	}
	case instruction::cast_zext_i1_to_i8:
	{
		auto const &inst = inst_.get<instructions::cast_zext_i1_to_i8>();
		return bz::format("zext i1 to i8 {}", inst.args[0]);
	}
	case instruction::cast_zext_i1_to_i16:
	{
		auto const &inst = inst_.get<instructions::cast_zext_i1_to_i16>();
		return bz::format("zext i1 to i16 {}", inst.args[0]);
	}
	case instruction::cast_zext_i1_to_i32:
	{
		auto const &inst = inst_.get<instructions::cast_zext_i1_to_i32>();
		return bz::format("zext i1 to i32 {}", inst.args[0]);
	}
	case instruction::cast_zext_i1_to_i64:
	{
		auto const &inst = inst_.get<instructions::cast_zext_i1_to_i64>();
		return bz::format("zext i1 to i64 {}", inst.args[0]);
	}
	case instruction::cast_zext_i8_to_i16:
	{
		auto const &inst = inst_.get<instructions::cast_zext_i8_to_i16>();
		return bz::format("zext i8 to i16 {}", inst.args[0]);
	}
	case instruction::cast_zext_i8_to_i32:
	{
		auto const &inst = inst_.get<instructions::cast_zext_i8_to_i32>();
		return bz::format("zext i8 to i32 {}", inst.args[0]);
	}
	case instruction::cast_zext_i8_to_i64:
	{
		auto const &inst = inst_.get<instructions::cast_zext_i8_to_i64>();
		return bz::format("zext i8 to i64 {}", inst.args[0]);
	}
	case instruction::cast_zext_i16_to_i32:
	{
		auto const &inst = inst_.get<instructions::cast_zext_i16_to_i32>();
		return bz::format("zext i16 to i32 {}", inst.args[0]);
	}
	case instruction::cast_zext_i16_to_i64:
	{
		auto const &inst = inst_.get<instructions::cast_zext_i16_to_i64>();
		return bz::format("zext i16 to i64 {}", inst.args[0]);
	}
	case instruction::cast_zext_i32_to_i64:
	{
		auto const &inst = inst_.get<instructions::cast_zext_i32_to_i64>();
		return bz::format("zext i32 to i64 {}", inst.args[0]);
	}
	case instruction::cast_sext_i8_to_i16:
	{
		auto const &inst = inst_.get<instructions::cast_sext_i8_to_i16>();
		return bz::format("sext i8 to i16 {}", inst.args[0]);
	}
	case instruction::cast_sext_i8_to_i32:
	{
		auto const &inst = inst_.get<instructions::cast_sext_i8_to_i32>();
		return bz::format("sext i8 to i32 {}", inst.args[0]);
	}
	case instruction::cast_sext_i8_to_i64:
	{
		auto const &inst = inst_.get<instructions::cast_sext_i8_to_i64>();
		return bz::format("sext i8 to i64 {}", inst.args[0]);
	}
	case instruction::cast_sext_i16_to_i32:
	{
		auto const &inst = inst_.get<instructions::cast_sext_i16_to_i32>();
		return bz::format("sext i16 to i32 {}", inst.args[0]);
	}
	case instruction::cast_sext_i16_to_i64:
	{
		auto const &inst = inst_.get<instructions::cast_sext_i16_to_i64>();
		return bz::format("sext i16 to i64 {}", inst.args[0]);
	}
	case instruction::cast_sext_i32_to_i64:
	{
		auto const &inst = inst_.get<instructions::cast_sext_i32_to_i64>();
		return bz::format("sext i32 to i64 {}", inst.args[0]);
	}
	case instruction::cast_trunc_i64_to_i8:
	{
		auto const &inst = inst_.get<instructions::cast_trunc_i64_to_i8>();
		return bz::format("trunc i64 to i8 {}", inst.args[0]);
	}
	case instruction::cast_trunc_i64_to_i16:
	{
		auto const &inst = inst_.get<instructions::cast_trunc_i64_to_i16>();
		return bz::format("trunc i64 to i16 {}", inst.args[0]);
	}
	case instruction::cast_trunc_i64_to_i32:
	{
		auto const &inst = inst_.get<instructions::cast_trunc_i64_to_i32>();
		return bz::format("trunc i64 to i32 {}", inst.args[0]);
	}
	case instruction::cast_trunc_i32_to_i8:
	{
		auto const &inst = inst_.get<instructions::cast_trunc_i32_to_i8>();
		return bz::format("trunc i32 to i8 {}", inst.args[0]);
	}
	case instruction::cast_trunc_i32_to_i16:
	{
		auto const &inst = inst_.get<instructions::cast_trunc_i32_to_i16>();
		return bz::format("trunc i32 to i16 {}", inst.args[0]);
	}
	case instruction::cast_trunc_i16_to_i8:
	{
		auto const &inst = inst_.get<instructions::cast_trunc_i16_to_i8>();
		return bz::format("trunc i16 to i8 {}", inst.args[0]);
	}
	case instruction::cast_f32_to_f64:
	{
		auto const &inst = inst_.get<instructions::cast_f32_to_f64>();
		return bz::format("fp-cast f32 to f64 {}", inst.args[0]);
	}
	case instruction::cast_f64_to_f32:
	{
		auto const &inst = inst_.get<instructions::cast_f64_to_f32>();
		return bz::format("fp-cast f64 to f32 {}", inst.args[0]);
	}
	case instruction::cast_f32_to_i8:
	{
		auto const &inst = inst_.get<instructions::cast_f32_to_i8>();
		return bz::format("fp-to-int f32 to i8 {}", inst.args[0]);
	}
	case instruction::cast_f32_to_i16:
	{
		auto const &inst = inst_.get<instructions::cast_f32_to_i16>();
		return bz::format("fp-to-int f32 to i16 {}", inst.args[0]);
	}
	case instruction::cast_f32_to_i32:
	{
		auto const &inst = inst_.get<instructions::cast_f32_to_i32>();
		return bz::format("fp-to-int f32 to i32 {}", inst.args[0]);
	}
	case instruction::cast_f32_to_i64:
	{
		auto const &inst = inst_.get<instructions::cast_f32_to_i64>();
		return bz::format("fp-to-int f32 to i64 {}", inst.args[0]);
	}
	case instruction::cast_f32_to_u8:
	{
		auto const &inst = inst_.get<instructions::cast_f32_to_u8>();
		return bz::format("fp-to-int f32 to u8 {}", inst.args[0]);
	}
	case instruction::cast_f32_to_u16:
	{
		auto const &inst = inst_.get<instructions::cast_f32_to_u16>();
		return bz::format("fp-to-int f32 to u16 {}", inst.args[0]);
	}
	case instruction::cast_f32_to_u32:
	{
		auto const &inst = inst_.get<instructions::cast_f32_to_u32>();
		return bz::format("fp-to-int f32 to u32 {}", inst.args[0]);
	}
	case instruction::cast_f32_to_u64:
	{
		auto const &inst = inst_.get<instructions::cast_f32_to_u64>();
		return bz::format("fp-to-int f32 to u64 {}", inst.args[0]);
	}
	case instruction::cast_f64_to_i8:
	{
		auto const &inst = inst_.get<instructions::cast_f64_to_i8>();
		return bz::format("fp-to-int f64 to i8 {}", inst.args[0]);
	}
	case instruction::cast_f64_to_i16:
	{
		auto const &inst = inst_.get<instructions::cast_f64_to_i16>();
		return bz::format("fp-to-int f64 to i16 {}", inst.args[0]);
	}
	case instruction::cast_f64_to_i32:
	{
		auto const &inst = inst_.get<instructions::cast_f64_to_i32>();
		return bz::format("fp-to-int f64 to i32 {}", inst.args[0]);
	}
	case instruction::cast_f64_to_i64:
	{
		auto const &inst = inst_.get<instructions::cast_f64_to_i64>();
		return bz::format("fp-to-int f64 to i64 {}", inst.args[0]);
	}
	case instruction::cast_f64_to_u8:
	{
		auto const &inst = inst_.get<instructions::cast_f64_to_u8>();
		return bz::format("fp-to-int f64 to u8 {}", inst.args[0]);
	}
	case instruction::cast_f64_to_u16:
	{
		auto const &inst = inst_.get<instructions::cast_f64_to_u16>();
		return bz::format("fp-to-int f64 to u16 {}", inst.args[0]);
	}
	case instruction::cast_f64_to_u32:
	{
		auto const &inst = inst_.get<instructions::cast_f64_to_u32>();
		return bz::format("fp-to-int f64 to u32 {}", inst.args[0]);
	}
	case instruction::cast_f64_to_u64:
	{
		auto const &inst = inst_.get<instructions::cast_f64_to_u64>();
		return bz::format("fp-to-int f64 to u64 {}", inst.args[0]);
	}
	case instruction::cast_i8_to_f32:
	{
		auto const &inst = inst_.get<instructions::cast_i8_to_f32>();
		return bz::format("int-to-fp i8 to f32 {}", inst.args[0]);
	}
	case instruction::cast_i16_to_f32:
	{
		auto const &inst = inst_.get<instructions::cast_i16_to_f32>();
		return bz::format("int-to-fp i16 to f32 {}", inst.args[0]);
	}
	case instruction::cast_i32_to_f32:
	{
		auto const &inst = inst_.get<instructions::cast_i32_to_f32>();
		return bz::format("int-to-fp i32 to f32 {}", inst.args[0]);
	}
	case instruction::cast_i64_to_f32:
	{
		auto const &inst = inst_.get<instructions::cast_i64_to_f32>();
		return bz::format("int-to-fp i64 to f32 {}", inst.args[0]);
	}
	case instruction::cast_u8_to_f32:
	{
		auto const &inst = inst_.get<instructions::cast_u8_to_f32>();
		return bz::format("int-to-fp u8 to f32 {}", inst.args[0]);
	}
	case instruction::cast_u16_to_f32:
	{
		auto const &inst = inst_.get<instructions::cast_u16_to_f32>();
		return bz::format("int-to-fp u16 to f32 {}", inst.args[0]);
	}
	case instruction::cast_u32_to_f32:
	{
		auto const &inst = inst_.get<instructions::cast_u32_to_f32>();
		return bz::format("int-to-fp u32 to f32 {}", inst.args[0]);
	}
	case instruction::cast_u64_to_f32:
	{
		auto const &inst = inst_.get<instructions::cast_u64_to_f32>();
		return bz::format("int-to-fp u64 to f32 {}", inst.args[0]);
	}
	case instruction::cast_i8_to_f64:
	{
		auto const &inst = inst_.get<instructions::cast_i8_to_f64>();
		return bz::format("int-to-fp i8 to f64 {}", inst.args[0]);
	}
	case instruction::cast_i16_to_f64:
	{
		auto const &inst = inst_.get<instructions::cast_i16_to_f64>();
		return bz::format("int-to-fp i16 to f64 {}", inst.args[0]);
	}
	case instruction::cast_i32_to_f64:
	{
		auto const &inst = inst_.get<instructions::cast_i32_to_f64>();
		return bz::format("int-to-fp i32 to f64 {}", inst.args[0]);
	}
	case instruction::cast_i64_to_f64:
	{
		auto const &inst = inst_.get<instructions::cast_i64_to_f64>();
		return bz::format("int-to-fp i64 to f64 {}", inst.args[0]);
	}
	case instruction::cast_u8_to_f64:
	{
		auto const &inst = inst_.get<instructions::cast_u8_to_f64>();
		return bz::format("int-to-fp u8 to f64 {}", inst.args[0]);
	}
	case instruction::cast_u16_to_f64:
	{
		auto const &inst = inst_.get<instructions::cast_u16_to_f64>();
		return bz::format("int-to-fp u16 to f64 {}", inst.args[0]);
	}
	case instruction::cast_u32_to_f64:
	{
		auto const &inst = inst_.get<instructions::cast_u32_to_f64>();
		return bz::format("int-to-fp u32 to f64 {}", inst.args[0]);
	}
	case instruction::cast_u64_to_f64:
	{
		auto const &inst = inst_.get<instructions::cast_u64_to_f64>();
		return bz::format("int-to-fp u64 to f64 {}", inst.args[0]);
	}
	case instruction::cmp_eq_i1:
	{
		auto const &inst = inst_.get<instructions::cmp_eq_i1>();
		return bz::format("cmp eq i1 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_eq_i8:
	{
		auto const &inst = inst_.get<instructions::cmp_eq_i8>();
		return bz::format("cmp eq i8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_eq_i16:
	{
		auto const &inst = inst_.get<instructions::cmp_eq_i16>();
		return bz::format("cmp eq i16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_eq_i32:
	{
		auto const &inst = inst_.get<instructions::cmp_eq_i32>();
		return bz::format("cmp eq i32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_eq_i64:
	{
		auto const &inst = inst_.get<instructions::cmp_eq_i64>();
		return bz::format("cmp eq i64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_eq_f32:
	{
		auto const &inst = inst_.get<instructions::cmp_eq_f32>();
		return bz::format("cmp eq f32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_eq_f64:
	{
		auto const &inst = inst_.get<instructions::cmp_eq_f64>();
		return bz::format("cmp eq f64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_eq_f32_check:
	{
		auto const &inst = inst_.get<instructions::cmp_eq_f32_check>();
		return bz::format("cmp eq f32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::cmp_eq_f64_check:
	{
		auto const &inst = inst_.get<instructions::cmp_eq_f64_check>();
		return bz::format("cmp eq f64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::cmp_eq_ptr:
	{
		auto const &inst = inst_.get<instructions::cmp_eq_ptr>();
		return bz::format("cmp eq ptr {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_neq_i1:
	{
		auto const &inst = inst_.get<instructions::cmp_neq_i1>();
		return bz::format("cmp neq i1 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_neq_i8:
	{
		auto const &inst = inst_.get<instructions::cmp_neq_i8>();
		return bz::format("cmp neq i8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_neq_i16:
	{
		auto const &inst = inst_.get<instructions::cmp_neq_i16>();
		return bz::format("cmp neq i16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_neq_i32:
	{
		auto const &inst = inst_.get<instructions::cmp_neq_i32>();
		return bz::format("cmp neq i32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_neq_i64:
	{
		auto const &inst = inst_.get<instructions::cmp_neq_i64>();
		return bz::format("cmp neq i64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_neq_f32:
	{
		auto const &inst = inst_.get<instructions::cmp_neq_f32>();
		return bz::format("cmp neq f32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_neq_f64:
	{
		auto const &inst = inst_.get<instructions::cmp_neq_f64>();
		return bz::format("cmp neq f64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_neq_f32_check:
	{
		auto const &inst = inst_.get<instructions::cmp_neq_f32_check>();
		return bz::format("cmp neq f32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::cmp_neq_f64_check:
	{
		auto const &inst = inst_.get<instructions::cmp_neq_f64_check>();
		return bz::format("cmp neq f64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::cmp_neq_ptr:
	{
		auto const &inst = inst_.get<instructions::cmp_neq_ptr>();
		return bz::format("cmp neq ptr {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lt_i8:
	{
		auto const &inst = inst_.get<instructions::cmp_lt_i8>();
		return bz::format("cmp lt i8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lt_i16:
	{
		auto const &inst = inst_.get<instructions::cmp_lt_i16>();
		return bz::format("cmp lt i16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lt_i32:
	{
		auto const &inst = inst_.get<instructions::cmp_lt_i32>();
		return bz::format("cmp lt i32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lt_i64:
	{
		auto const &inst = inst_.get<instructions::cmp_lt_i64>();
		return bz::format("cmp lt i64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lt_u8:
	{
		auto const &inst = inst_.get<instructions::cmp_lt_u8>();
		return bz::format("cmp lt u8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lt_u16:
	{
		auto const &inst = inst_.get<instructions::cmp_lt_u16>();
		return bz::format("cmp lt u16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lt_u32:
	{
		auto const &inst = inst_.get<instructions::cmp_lt_u32>();
		return bz::format("cmp lt u32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lt_u64:
	{
		auto const &inst = inst_.get<instructions::cmp_lt_u64>();
		return bz::format("cmp lt u64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lt_f32:
	{
		auto const &inst = inst_.get<instructions::cmp_lt_f32>();
		return bz::format("cmp lt f32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lt_f64:
	{
		auto const &inst = inst_.get<instructions::cmp_lt_f64>();
		return bz::format("cmp lt f64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lt_f32_check:
	{
		auto const &inst = inst_.get<instructions::cmp_lt_f32_check>();
		return bz::format("cmp lt f32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::cmp_lt_f64_check:
	{
		auto const &inst = inst_.get<instructions::cmp_lt_f64_check>();
		return bz::format("cmp lt f64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::cmp_lt_ptr:
	{
		auto const &inst = inst_.get<instructions::cmp_lt_ptr>();
		return bz::format("cmp lt ptr {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::cmp_gt_i8:
	{
		auto const &inst = inst_.get<instructions::cmp_gt_i8>();
		return bz::format("cmp gt i8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gt_i16:
	{
		auto const &inst = inst_.get<instructions::cmp_gt_i16>();
		return bz::format("cmp gt i16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gt_i32:
	{
		auto const &inst = inst_.get<instructions::cmp_gt_i32>();
		return bz::format("cmp gt i32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gt_i64:
	{
		auto const &inst = inst_.get<instructions::cmp_gt_i64>();
		return bz::format("cmp gt i64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gt_u8:
	{
		auto const &inst = inst_.get<instructions::cmp_gt_u8>();
		return bz::format("cmp gt u8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gt_u16:
	{
		auto const &inst = inst_.get<instructions::cmp_gt_u16>();
		return bz::format("cmp gt u16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gt_u32:
	{
		auto const &inst = inst_.get<instructions::cmp_gt_u32>();
		return bz::format("cmp gt u32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gt_u64:
	{
		auto const &inst = inst_.get<instructions::cmp_gt_u64>();
		return bz::format("cmp gt u64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gt_f32:
	{
		auto const &inst = inst_.get<instructions::cmp_gt_f32>();
		return bz::format("cmp gt f32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gt_f64:
	{
		auto const &inst = inst_.get<instructions::cmp_gt_f64>();
		return bz::format("cmp gt f64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gt_f32_check:
	{
		auto const &inst = inst_.get<instructions::cmp_gt_f32_check>();
		return bz::format("cmp gt f32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::cmp_gt_f64_check:
	{
		auto const &inst = inst_.get<instructions::cmp_gt_f64_check>();
		return bz::format("cmp gt f64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::cmp_gt_ptr:
	{
		auto const &inst = inst_.get<instructions::cmp_gt_ptr>();
		return bz::format("cmp gt ptr {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::cmp_lte_i8:
	{
		auto const &inst = inst_.get<instructions::cmp_lte_i8>();
		return bz::format("cmp lte i8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lte_i16:
	{
		auto const &inst = inst_.get<instructions::cmp_lte_i16>();
		return bz::format("cmp lte i16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lte_i32:
	{
		auto const &inst = inst_.get<instructions::cmp_lte_i32>();
		return bz::format("cmp lte i32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lte_i64:
	{
		auto const &inst = inst_.get<instructions::cmp_lte_i64>();
		return bz::format("cmp lte i64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lte_u8:
	{
		auto const &inst = inst_.get<instructions::cmp_lte_u8>();
		return bz::format("cmp lte u8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lte_u16:
	{
		auto const &inst = inst_.get<instructions::cmp_lte_u16>();
		return bz::format("cmp lte u16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lte_u32:
	{
		auto const &inst = inst_.get<instructions::cmp_lte_u32>();
		return bz::format("cmp lte u32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lte_u64:
	{
		auto const &inst = inst_.get<instructions::cmp_lte_u64>();
		return bz::format("cmp lte u64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lte_f32:
	{
		auto const &inst = inst_.get<instructions::cmp_lte_f32>();
		return bz::format("cmp lte f32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lte_f64:
	{
		auto const &inst = inst_.get<instructions::cmp_lte_f64>();
		return bz::format("cmp lte f64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_lte_f32_check:
	{
		auto const &inst = inst_.get<instructions::cmp_lte_f32_check>();
		return bz::format("cmp lte f32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::cmp_lte_f64_check:
	{
		auto const &inst = inst_.get<instructions::cmp_lte_f64_check>();
		return bz::format("cmp lte f64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::cmp_lte_ptr:
	{
		auto const &inst = inst_.get<instructions::cmp_lte_ptr>();
		return bz::format("cmp lte ptr {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::cmp_gte_i8:
	{
		auto const &inst = inst_.get<instructions::cmp_gte_i8>();
		return bz::format("cmp gte i8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gte_i16:
	{
		auto const &inst = inst_.get<instructions::cmp_gte_i16>();
		return bz::format("cmp gte i16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gte_i32:
	{
		auto const &inst = inst_.get<instructions::cmp_gte_i32>();
		return bz::format("cmp gte i32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gte_i64:
	{
		auto const &inst = inst_.get<instructions::cmp_gte_i64>();
		return bz::format("cmp gte i64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gte_u8:
	{
		auto const &inst = inst_.get<instructions::cmp_gte_u8>();
		return bz::format("cmp gte u8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gte_u16:
	{
		auto const &inst = inst_.get<instructions::cmp_gte_u16>();
		return bz::format("cmp gte u16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gte_u32:
	{
		auto const &inst = inst_.get<instructions::cmp_gte_u32>();
		return bz::format("cmp gte u32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gte_u64:
	{
		auto const &inst = inst_.get<instructions::cmp_gte_u64>();
		return bz::format("cmp gte u64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gte_f32:
	{
		auto const &inst = inst_.get<instructions::cmp_gte_f32>();
		return bz::format("cmp gte f32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gte_f64:
	{
		auto const &inst = inst_.get<instructions::cmp_gte_f64>();
		return bz::format("cmp gte f64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::cmp_gte_f32_check:
	{
		auto const &inst = inst_.get<instructions::cmp_gte_f32_check>();
		return bz::format("cmp gte f32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::cmp_gte_f64_check:
	{
		auto const &inst = inst_.get<instructions::cmp_gte_f64_check>();
		return bz::format("cmp gte f64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::cmp_gte_ptr:
	{
		auto const &inst = inst_.get<instructions::cmp_gte_ptr>();
		return bz::format("cmp gte ptr {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::neg_i8:
	{
		auto const &inst = inst_.get<instructions::neg_i8>();
		return bz::format("neg i8 {}", inst.args[0]);
	}
	case instruction::neg_i16:
	{
		auto const &inst = inst_.get<instructions::neg_i16>();
		return bz::format("neg i16 {}", inst.args[0]);
	}
	case instruction::neg_i32:
	{
		auto const &inst = inst_.get<instructions::neg_i32>();
		return bz::format("neg i32 {}", inst.args[0]);
	}
	case instruction::neg_i64:
	{
		auto const &inst = inst_.get<instructions::neg_i64>();
		return bz::format("neg i64 {}", inst.args[0]);
	}
	case instruction::neg_f32:
	{
		auto const &inst = inst_.get<instructions::neg_f32>();
		return bz::format("neg f32 {}", inst.args[0]);
	}
	case instruction::neg_f64:
	{
		auto const &inst = inst_.get<instructions::neg_f64>();
		return bz::format("neg f64 {}", inst.args[0]);
	}
	case instruction::neg_i8_check:
	{
		auto const &inst = inst_.get<instructions::neg_i8_check>();
		return bz::format("neg i8 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::neg_i16_check:
	{
		auto const &inst = inst_.get<instructions::neg_i16_check>();
		return bz::format("neg i16 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::neg_i32_check:
	{
		auto const &inst = inst_.get<instructions::neg_i32_check>();
		return bz::format("neg i32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::neg_i64_check:
	{
		auto const &inst = inst_.get<instructions::neg_i64_check>();
		return bz::format("neg i64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::add_i8:
	{
		auto const &inst = inst_.get<instructions::add_i8>();
		return bz::format("add i8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::add_i16:
	{
		auto const &inst = inst_.get<instructions::add_i16>();
		return bz::format("add i16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::add_i32:
	{
		auto const &inst = inst_.get<instructions::add_i32>();
		return bz::format("add i32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::add_i64:
	{
		auto const &inst = inst_.get<instructions::add_i64>();
		return bz::format("add i64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::add_f32:
	{
		auto const &inst = inst_.get<instructions::add_f32>();
		return bz::format("add f32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::add_f64:
	{
		auto const &inst = inst_.get<instructions::add_f64>();
		return bz::format("add f64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::add_ptr_i32:
	{
		auto const &inst = inst_.get<instructions::add_ptr_i32>();
		return bz::format("add ptr i32 {}, {} ({}, {})", inst.args[0], inst.args[1], inst.src_tokens_index, inst.pointer_arithmetic_check_info_index);
	}
	case instruction::add_ptr_u32:
	{
		auto const &inst = inst_.get<instructions::add_ptr_u32>();
		return bz::format("add ptr u32 {}, {} ({}, {})", inst.args[0], inst.args[1], inst.src_tokens_index, inst.pointer_arithmetic_check_info_index);
	}
	case instruction::add_ptr_i64:
	{
		auto const &inst = inst_.get<instructions::add_ptr_i64>();
		return bz::format("add ptr i64 {}, {} ({}, {})", inst.args[0], inst.args[1], inst.src_tokens_index, inst.pointer_arithmetic_check_info_index);
	}
	case instruction::add_ptr_u64:
	{
		auto const &inst = inst_.get<instructions::add_ptr_u64>();
		return bz::format("add ptr u64 {}, {} ({}, {})", inst.args[0], inst.args[1], inst.src_tokens_index, inst.pointer_arithmetic_check_info_index);
	}
	case instruction::add_ptr_const_unchecked:
	{
		auto const &inst = inst_.get<instructions::add_ptr_const_unchecked>();
		return bz::format("add ptr {} const {} {}", inst.object_type->to_string(), inst.amount, inst.args[0]);
	}
	case instruction::add_i8_check:
	{
		auto const &inst = inst_.get<instructions::add_i8_check>();
		return bz::format("add i8 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::add_i16_check:
	{
		auto const &inst = inst_.get<instructions::add_i16_check>();
		return bz::format("add i16 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::add_i32_check:
	{
		auto const &inst = inst_.get<instructions::add_i32_check>();
		return bz::format("add i32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::add_i64_check:
	{
		auto const &inst = inst_.get<instructions::add_i64_check>();
		return bz::format("add i64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::add_u8_check:
	{
		auto const &inst = inst_.get<instructions::add_u8_check>();
		return bz::format("add u8 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::add_u16_check:
	{
		auto const &inst = inst_.get<instructions::add_u16_check>();
		return bz::format("add u16 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::add_u32_check:
	{
		auto const &inst = inst_.get<instructions::add_u32_check>();
		return bz::format("add u32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::add_u64_check:
	{
		auto const &inst = inst_.get<instructions::add_u64_check>();
		return bz::format("add u64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::add_f32_check:
	{
		auto const &inst = inst_.get<instructions::add_f32_check>();
		return bz::format("add f32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::add_f64_check:
	{
		auto const &inst = inst_.get<instructions::add_f64_check>();
		return bz::format("add f64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::sub_i8:
	{
		auto const &inst = inst_.get<instructions::sub_i8>();
		return bz::format("sub i8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::sub_i16:
	{
		auto const &inst = inst_.get<instructions::sub_i16>();
		return bz::format("sub i16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::sub_i32:
	{
		auto const &inst = inst_.get<instructions::sub_i32>();
		return bz::format("sub i32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::sub_i64:
	{
		auto const &inst = inst_.get<instructions::sub_i64>();
		return bz::format("sub i64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::sub_f32:
	{
		auto const &inst = inst_.get<instructions::sub_f32>();
		return bz::format("sub f32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::sub_f64:
	{
		auto const &inst = inst_.get<instructions::sub_f64>();
		return bz::format("sub f64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::sub_ptr_i32:
	{
		auto const &inst = inst_.get<instructions::sub_ptr_i32>();
		return bz::format("sub ptr i32 {}, {} ({}, {})", inst.args[0], inst.args[1], inst.src_tokens_index, inst.pointer_arithmetic_check_info_index);
	}
	case instruction::sub_ptr_u32:
	{
		auto const &inst = inst_.get<instructions::sub_ptr_u32>();
		return bz::format("sub ptr u32 {}, {} ({}, {})", inst.args[0], inst.args[1], inst.src_tokens_index, inst.pointer_arithmetic_check_info_index);
	}
	case instruction::sub_ptr_i64:
	{
		auto const &inst = inst_.get<instructions::sub_ptr_i64>();
		return bz::format("sub ptr i64 {}, {} ({}, {})", inst.args[0], inst.args[1], inst.src_tokens_index, inst.pointer_arithmetic_check_info_index);
	}
	case instruction::sub_ptr_u64:
	{
		auto const &inst = inst_.get<instructions::sub_ptr_u64>();
		return bz::format("sub ptr u64 {}, {} ({}, {})", inst.args[0], inst.args[1], inst.src_tokens_index, inst.pointer_arithmetic_check_info_index);
	}
	case instruction::sub_i8_check:
	{
		auto const &inst = inst_.get<instructions::sub_i8_check>();
		return bz::format("sub i8 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::sub_i16_check:
	{
		auto const &inst = inst_.get<instructions::sub_i16_check>();
		return bz::format("sub i16 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::sub_i32_check:
	{
		auto const &inst = inst_.get<instructions::sub_i32_check>();
		return bz::format("sub i32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::sub_i64_check:
	{
		auto const &inst = inst_.get<instructions::sub_i64_check>();
		return bz::format("sub i64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::sub_u8_check:
	{
		auto const &inst = inst_.get<instructions::sub_u8_check>();
		return bz::format("sub u8 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::sub_u16_check:
	{
		auto const &inst = inst_.get<instructions::sub_u16_check>();
		return bz::format("sub u16 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::sub_u32_check:
	{
		auto const &inst = inst_.get<instructions::sub_u32_check>();
		return bz::format("sub u32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::sub_u64_check:
	{
		auto const &inst = inst_.get<instructions::sub_u64_check>();
		return bz::format("sub u64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::sub_f32_check:
	{
		auto const &inst = inst_.get<instructions::sub_f32_check>();
		return bz::format("sub f32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::sub_f64_check:
	{
		auto const &inst = inst_.get<instructions::sub_f64_check>();
		return bz::format("sub f64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::ptr32_diff:
	{
		auto const &inst = inst_.get<instructions::ptr32_diff>();
		return bz::format("ptr32-diff {}, {} ({}, {})", inst.args[0], inst.args[1], inst.src_tokens_index, inst.pointer_arithmetic_check_info_index);
	}
	case instruction::ptr64_diff:
	{
		auto const &inst = inst_.get<instructions::ptr64_diff>();
		return bz::format("ptr64-diff {}, {} ({}, {})", inst.args[0], inst.args[1], inst.src_tokens_index, inst.pointer_arithmetic_check_info_index);
	}
	case instruction::ptr32_diff_unchecked:
	{
		auto const &inst = inst_.get<instructions::ptr32_diff_unchecked>();
		return bz::format("ptr32-diff {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::ptr64_diff_unchecked:
	{
		auto const &inst = inst_.get<instructions::ptr64_diff_unchecked>();
		return bz::format("ptr64-diff {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::mul_i8:
	{
		auto const &inst = inst_.get<instructions::mul_i8>();
		return bz::format("mul i8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::mul_i16:
	{
		auto const &inst = inst_.get<instructions::mul_i16>();
		return bz::format("mul i16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::mul_i32:
	{
		auto const &inst = inst_.get<instructions::mul_i32>();
		return bz::format("mul i32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::mul_i64:
	{
		auto const &inst = inst_.get<instructions::mul_i64>();
		return bz::format("mul i64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::mul_f32:
	{
		auto const &inst = inst_.get<instructions::mul_f32>();
		return bz::format("mul f32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::mul_f64:
	{
		auto const &inst = inst_.get<instructions::mul_f64>();
		return bz::format("mul f64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::mul_i8_check:
	{
		auto const &inst = inst_.get<instructions::mul_i8_check>();
		return bz::format("mul i8 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::mul_i16_check:
	{
		auto const &inst = inst_.get<instructions::mul_i16_check>();
		return bz::format("mul i16 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::mul_i32_check:
	{
		auto const &inst = inst_.get<instructions::mul_i32_check>();
		return bz::format("mul i32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::mul_i64_check:
	{
		auto const &inst = inst_.get<instructions::mul_i64_check>();
		return bz::format("mul i64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::mul_u8_check:
	{
		auto const &inst = inst_.get<instructions::mul_u8_check>();
		return bz::format("mul u8 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::mul_u16_check:
	{
		auto const &inst = inst_.get<instructions::mul_u16_check>();
		return bz::format("mul u16 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::mul_u32_check:
	{
		auto const &inst = inst_.get<instructions::mul_u32_check>();
		return bz::format("mul u32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::mul_u64_check:
	{
		auto const &inst = inst_.get<instructions::mul_u64_check>();
		return bz::format("mul u64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::mul_f32_check:
	{
		auto const &inst = inst_.get<instructions::mul_f32_check>();
		return bz::format("mul f32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::mul_f64_check:
	{
		auto const &inst = inst_.get<instructions::mul_f64_check>();
		return bz::format("mul f64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::div_i8:
	{
		auto const &inst = inst_.get<instructions::div_i8>();
		return bz::format("div i8 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::div_i16:
	{
		auto const &inst = inst_.get<instructions::div_i16>();
		return bz::format("div i16 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::div_i32:
	{
		auto const &inst = inst_.get<instructions::div_i32>();
		return bz::format("div i32 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::div_i64:
	{
		auto const &inst = inst_.get<instructions::div_i64>();
		return bz::format("div i64 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::div_u8:
	{
		auto const &inst = inst_.get<instructions::div_u8>();
		return bz::format("div u8 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::div_u16:
	{
		auto const &inst = inst_.get<instructions::div_u16>();
		return bz::format("div u16 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::div_u32:
	{
		auto const &inst = inst_.get<instructions::div_u32>();
		return bz::format("div u32 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::div_u64:
	{
		auto const &inst = inst_.get<instructions::div_u64>();
		return bz::format("div u64 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::div_f32:
	{
		auto const &inst = inst_.get<instructions::div_f32>();
		return bz::format("div f32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::div_f64:
	{
		auto const &inst = inst_.get<instructions::div_f64>();
		return bz::format("div f64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::div_i8_check:
	{
		auto const &inst = inst_.get<instructions::div_i8_check>();
		return bz::format("div i8 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::div_i16_check:
	{
		auto const &inst = inst_.get<instructions::div_i16_check>();
		return bz::format("div i16 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::div_i32_check:
	{
		auto const &inst = inst_.get<instructions::div_i32_check>();
		return bz::format("div i32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::div_i64_check:
	{
		auto const &inst = inst_.get<instructions::div_i64_check>();
		return bz::format("div i64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::div_f32_check:
	{
		auto const &inst = inst_.get<instructions::div_f32_check>();
		return bz::format("div f32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::div_f64_check:
	{
		auto const &inst = inst_.get<instructions::div_f64_check>();
		return bz::format("div f64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::rem_i8:
	{
		auto const &inst = inst_.get<instructions::rem_i8>();
		return bz::format("rem i8 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::rem_i16:
	{
		auto const &inst = inst_.get<instructions::rem_i16>();
		return bz::format("rem i16 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::rem_i32:
	{
		auto const &inst = inst_.get<instructions::rem_i32>();
		return bz::format("rem i32 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::rem_i64:
	{
		auto const &inst = inst_.get<instructions::rem_i64>();
		return bz::format("rem i64 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::rem_u8:
	{
		auto const &inst = inst_.get<instructions::rem_u8>();
		return bz::format("rem u8 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::rem_u16:
	{
		auto const &inst = inst_.get<instructions::rem_u16>();
		return bz::format("rem u16 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::rem_u32:
	{
		auto const &inst = inst_.get<instructions::rem_u32>();
		return bz::format("rem u32 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::rem_u64:
	{
		auto const &inst = inst_.get<instructions::rem_u64>();
		return bz::format("rem u64 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::not_i1:
	{
		auto const &inst = inst_.get<instructions::not_i1>();
		return bz::format("not i1 {}", inst.args[0]);
	}
	case instruction::not_i8:
	{
		auto const &inst = inst_.get<instructions::not_i8>();
		return bz::format("not i8 {}", inst.args[0]);
	}
	case instruction::not_i16:
	{
		auto const &inst = inst_.get<instructions::not_i16>();
		return bz::format("not i16 {}", inst.args[0]);
	}
	case instruction::not_i32:
	{
		auto const &inst = inst_.get<instructions::not_i32>();
		return bz::format("not i32 {}", inst.args[0]);
	}
	case instruction::not_i64:
	{
		auto const &inst = inst_.get<instructions::not_i64>();
		return bz::format("not i64 {}", inst.args[0]);
	}
	case instruction::and_i1:
	{
		auto const &inst = inst_.get<instructions::and_i1>();
		return bz::format("and i1 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::and_i8:
	{
		auto const &inst = inst_.get<instructions::and_i8>();
		return bz::format("and i8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::and_i16:
	{
		auto const &inst = inst_.get<instructions::and_i16>();
		return bz::format("and i16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::and_i32:
	{
		auto const &inst = inst_.get<instructions::and_i32>();
		return bz::format("and i32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::and_i64:
	{
		auto const &inst = inst_.get<instructions::and_i64>();
		return bz::format("and i64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::xor_i1:
	{
		auto const &inst = inst_.get<instructions::xor_i1>();
		return bz::format("xor i1 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::xor_i8:
	{
		auto const &inst = inst_.get<instructions::xor_i8>();
		return bz::format("xor i8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::xor_i16:
	{
		auto const &inst = inst_.get<instructions::xor_i16>();
		return bz::format("xor i16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::xor_i32:
	{
		auto const &inst = inst_.get<instructions::xor_i32>();
		return bz::format("xor i32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::xor_i64:
	{
		auto const &inst = inst_.get<instructions::xor_i64>();
		return bz::format("xor i64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::or_i1:
	{
		auto const &inst = inst_.get<instructions::or_i1>();
		return bz::format("or i1 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::or_i8:
	{
		auto const &inst = inst_.get<instructions::or_i8>();
		return bz::format("or i8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::or_i16:
	{
		auto const &inst = inst_.get<instructions::or_i16>();
		return bz::format("or i16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::or_i32:
	{
		auto const &inst = inst_.get<instructions::or_i32>();
		return bz::format("or i32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::or_i64:
	{
		auto const &inst = inst_.get<instructions::or_i64>();
		return bz::format("or i64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::shl_i8_signed:
	{
		auto const &inst = inst_.get<instructions::shl_i8_signed>();
		return bz::format("shl i8 signed {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::shl_i16_signed:
	{
		auto const &inst = inst_.get<instructions::shl_i16_signed>();
		return bz::format("shl i16 signed {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::shl_i32_signed:
	{
		auto const &inst = inst_.get<instructions::shl_i32_signed>();
		return bz::format("shl i32 signed {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::shl_i64_signed:
	{
		auto const &inst = inst_.get<instructions::shl_i64_signed>();
		return bz::format("shl i64 signed {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::shl_i8_unsigned:
	{
		auto const &inst = inst_.get<instructions::shl_i8_unsigned>();
		return bz::format("shl i8 unsigned {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::shl_i16_unsigned:
	{
		auto const &inst = inst_.get<instructions::shl_i16_unsigned>();
		return bz::format("shl i16 unsigned {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::shl_i32_unsigned:
	{
		auto const &inst = inst_.get<instructions::shl_i32_unsigned>();
		return bz::format("shl i32 unsigned {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::shl_i64_unsigned:
	{
		auto const &inst = inst_.get<instructions::shl_i64_unsigned>();
		return bz::format("shl i64 unsigned {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::shr_i8_signed:
	{
		auto const &inst = inst_.get<instructions::shr_i8_signed>();
		return bz::format("shr i8 signed {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::shr_i16_signed:
	{
		auto const &inst = inst_.get<instructions::shr_i16_signed>();
		return bz::format("shr i16 signed {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::shr_i32_signed:
	{
		auto const &inst = inst_.get<instructions::shr_i32_signed>();
		return bz::format("shr i32 signed {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::shr_i64_signed:
	{
		auto const &inst = inst_.get<instructions::shr_i64_signed>();
		return bz::format("shr i64 signed {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::shr_i8_unsigned:
	{
		auto const &inst = inst_.get<instructions::shr_i8_unsigned>();
		return bz::format("shr i8 unsigned {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::shr_i16_unsigned:
	{
		auto const &inst = inst_.get<instructions::shr_i16_unsigned>();
		return bz::format("shr i16 unsigned {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::shr_i32_unsigned:
	{
		auto const &inst = inst_.get<instructions::shr_i32_unsigned>();
		return bz::format("shr i32 unsigned {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::shr_i64_unsigned:
	{
		auto const &inst = inst_.get<instructions::shr_i64_unsigned>();
		return bz::format("shr i64 unsigned {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::abs_i8:
	{
		auto const &inst = inst_.get<instructions::abs_i8>();
		return bz::format("abs i8 {}", inst.args[0]);
	}
	case instruction::abs_i16:
	{
		auto const &inst = inst_.get<instructions::abs_i16>();
		return bz::format("abs i16 {}", inst.args[0]);
	}
	case instruction::abs_i32:
	{
		auto const &inst = inst_.get<instructions::abs_i32>();
		return bz::format("abs i32 {}", inst.args[0]);
	}
	case instruction::abs_i64:
	{
		auto const &inst = inst_.get<instructions::abs_i64>();
		return bz::format("abs i64 {}", inst.args[0]);
	}
	case instruction::abs_f32:
	{
		auto const &inst = inst_.get<instructions::abs_f32>();
		return bz::format("abs f32 {}", inst.args[0]);
	}
	case instruction::abs_f64:
	{
		auto const &inst = inst_.get<instructions::abs_f64>();
		return bz::format("abs f64 {}", inst.args[0]);
	}
	case instruction::abs_i8_check:
	{
		auto const &inst = inst_.get<instructions::abs_i8_check>();
		return bz::format("abs i8 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::abs_i16_check:
	{
		auto const &inst = inst_.get<instructions::abs_i16_check>();
		return bz::format("abs i16 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::abs_i32_check:
	{
		auto const &inst = inst_.get<instructions::abs_i32_check>();
		return bz::format("abs i32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::abs_i64_check:
	{
		auto const &inst = inst_.get<instructions::abs_i64_check>();
		return bz::format("abs i64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::abs_f32_check:
	{
		auto const &inst = inst_.get<instructions::abs_f32_check>();
		return bz::format("abs f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::abs_f64_check:
	{
		auto const &inst = inst_.get<instructions::abs_f64_check>();
		return bz::format("abs f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::min_i8:
	{
		auto const &inst = inst_.get<instructions::min_i8>();
		return bz::format("min i8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::min_i16:
	{
		auto const &inst = inst_.get<instructions::min_i16>();
		return bz::format("min i16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::min_i32:
	{
		auto const &inst = inst_.get<instructions::min_i32>();
		return bz::format("min i32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::min_i64:
	{
		auto const &inst = inst_.get<instructions::min_i64>();
		return bz::format("min i64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::min_u8:
	{
		auto const &inst = inst_.get<instructions::min_u8>();
		return bz::format("min u8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::min_u16:
	{
		auto const &inst = inst_.get<instructions::min_u16>();
		return bz::format("min u16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::min_u32:
	{
		auto const &inst = inst_.get<instructions::min_u32>();
		return bz::format("min u32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::min_u64:
	{
		auto const &inst = inst_.get<instructions::min_u64>();
		return bz::format("min u64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::min_f32:
	{
		auto const &inst = inst_.get<instructions::min_f32>();
		return bz::format("min f32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::min_f64:
	{
		auto const &inst = inst_.get<instructions::min_f64>();
		return bz::format("min f64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::min_f32_check:
	{
		auto const &inst = inst_.get<instructions::min_f32_check>();
		return bz::format("min f32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::min_f64_check:
	{
		auto const &inst = inst_.get<instructions::min_f64_check>();
		return bz::format("min f64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::max_i8:
	{
		auto const &inst = inst_.get<instructions::max_i8>();
		return bz::format("max i8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::max_i16:
	{
		auto const &inst = inst_.get<instructions::max_i16>();
		return bz::format("max i16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::max_i32:
	{
		auto const &inst = inst_.get<instructions::max_i32>();
		return bz::format("max i32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::max_i64:
	{
		auto const &inst = inst_.get<instructions::max_i64>();
		return bz::format("max i64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::max_u8:
	{
		auto const &inst = inst_.get<instructions::max_u8>();
		return bz::format("max u8 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::max_u16:
	{
		auto const &inst = inst_.get<instructions::max_u16>();
		return bz::format("max u16 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::max_u32:
	{
		auto const &inst = inst_.get<instructions::max_u32>();
		return bz::format("max u32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::max_u64:
	{
		auto const &inst = inst_.get<instructions::max_u64>();
		return bz::format("max u64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::max_f32:
	{
		auto const &inst = inst_.get<instructions::max_f32>();
		return bz::format("max f32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::max_f64:
	{
		auto const &inst = inst_.get<instructions::max_f64>();
		return bz::format("max f64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::max_f32_check:
	{
		auto const &inst = inst_.get<instructions::max_f32_check>();
		return bz::format("max f32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::max_f64_check:
	{
		auto const &inst = inst_.get<instructions::max_f64_check>();
		return bz::format("max f64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::exp_f32:
	{
		auto const &inst = inst_.get<instructions::exp_f32>();
		return bz::format("exp f32 {}", inst.args[0]);
	}
	case instruction::exp_f64:
	{
		auto const &inst = inst_.get<instructions::exp_f64>();
		return bz::format("exp f64 {}", inst.args[0]);
	}
	case instruction::exp_f32_check:
	{
		auto const &inst = inst_.get<instructions::exp_f32_check>();
		return bz::format("exp f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::exp_f64_check:
	{
		auto const &inst = inst_.get<instructions::exp_f64_check>();
		return bz::format("exp f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::exp2_f32:
	{
		auto const &inst = inst_.get<instructions::exp2_f32>();
		return bz::format("exp2 f32 {}", inst.args[0]);
	}
	case instruction::exp2_f64:
	{
		auto const &inst = inst_.get<instructions::exp2_f64>();
		return bz::format("exp2 f64 {}", inst.args[0]);
	}
	case instruction::exp2_f32_check:
	{
		auto const &inst = inst_.get<instructions::exp2_f32_check>();
		return bz::format("exp2 f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::exp2_f64_check:
	{
		auto const &inst = inst_.get<instructions::exp2_f64_check>();
		return bz::format("exp2 f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::expm1_f32:
	{
		auto const &inst = inst_.get<instructions::expm1_f32>();
		return bz::format("expm1 f32 {}", inst.args[0]);
	}
	case instruction::expm1_f64:
	{
		auto const &inst = inst_.get<instructions::expm1_f64>();
		return bz::format("expm1 f64 {}", inst.args[0]);
	}
	case instruction::expm1_f32_check:
	{
		auto const &inst = inst_.get<instructions::expm1_f32_check>();
		return bz::format("expm1 f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::expm1_f64_check:
	{
		auto const &inst = inst_.get<instructions::expm1_f64_check>();
		return bz::format("expm1 f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::log_f32:
	{
		auto const &inst = inst_.get<instructions::log_f32>();
		return bz::format("log f32 {}", inst.args[0]);
	}
	case instruction::log_f64:
	{
		auto const &inst = inst_.get<instructions::log_f64>();
		return bz::format("log f64 {}", inst.args[0]);
	}
	case instruction::log_f32_check:
	{
		auto const &inst = inst_.get<instructions::log_f32_check>();
		return bz::format("log f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::log_f64_check:
	{
		auto const &inst = inst_.get<instructions::log_f64_check>();
		return bz::format("log f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::log10_f32:
	{
		auto const &inst = inst_.get<instructions::log10_f32>();
		return bz::format("log10 f32 {}", inst.args[0]);
	}
	case instruction::log10_f64:
	{
		auto const &inst = inst_.get<instructions::log10_f64>();
		return bz::format("log10 f64 {}", inst.args[0]);
	}
	case instruction::log10_f32_check:
	{
		auto const &inst = inst_.get<instructions::log10_f32_check>();
		return bz::format("log10 f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::log10_f64_check:
	{
		auto const &inst = inst_.get<instructions::log10_f64_check>();
		return bz::format("log10 f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::log2_f32:
	{
		auto const &inst = inst_.get<instructions::log2_f32>();
		return bz::format("log2 f32 {}", inst.args[0]);
	}
	case instruction::log2_f64:
	{
		auto const &inst = inst_.get<instructions::log2_f64>();
		return bz::format("log2 f64 {}", inst.args[0]);
	}
	case instruction::log2_f32_check:
	{
		auto const &inst = inst_.get<instructions::log2_f32_check>();
		return bz::format("log2 f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::log2_f64_check:
	{
		auto const &inst = inst_.get<instructions::log2_f64_check>();
		return bz::format("log2 f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::log1p_f32:
	{
		auto const &inst = inst_.get<instructions::log1p_f32>();
		return bz::format("log1p f32 {}", inst.args[0]);
	}
	case instruction::log1p_f64:
	{
		auto const &inst = inst_.get<instructions::log1p_f64>();
		return bz::format("log1p f64 {}", inst.args[0]);
	}
	case instruction::log1p_f32_check:
	{
		auto const &inst = inst_.get<instructions::log1p_f32_check>();
		return bz::format("log1p f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::log1p_f64_check:
	{
		auto const &inst = inst_.get<instructions::log1p_f64_check>();
		return bz::format("log1p f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::sqrt_f32:
	{
		auto const &inst = inst_.get<instructions::sqrt_f32>();
		return bz::format("sqrt f32 {}", inst.args[0]);
	}
	case instruction::sqrt_f64:
	{
		auto const &inst = inst_.get<instructions::sqrt_f64>();
		return bz::format("sqrt f64 {}", inst.args[0]);
	}
	case instruction::sqrt_f32_check:
	{
		auto const &inst = inst_.get<instructions::sqrt_f32_check>();
		return bz::format("sqrt f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::sqrt_f64_check:
	{
		auto const &inst = inst_.get<instructions::sqrt_f64_check>();
		return bz::format("sqrt f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::pow_f32:
	{
		auto const &inst = inst_.get<instructions::pow_f32>();
		return bz::format("pow f32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::pow_f64:
	{
		auto const &inst = inst_.get<instructions::pow_f64>();
		return bz::format("pow f64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::pow_f32_check:
	{
		auto const &inst = inst_.get<instructions::pow_f32_check>();
		return bz::format("pow f32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::pow_f64_check:
	{
		auto const &inst = inst_.get<instructions::pow_f64_check>();
		return bz::format("pow f64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::cbrt_f32:
	{
		auto const &inst = inst_.get<instructions::cbrt_f32>();
		return bz::format("cbrt f32 {}", inst.args[0]);
	}
	case instruction::cbrt_f64:
	{
		auto const &inst = inst_.get<instructions::cbrt_f64>();
		return bz::format("cbrt f64 {}", inst.args[0]);
	}
	case instruction::cbrt_f32_check:
	{
		auto const &inst = inst_.get<instructions::cbrt_f32_check>();
		return bz::format("cbrt f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::cbrt_f64_check:
	{
		auto const &inst = inst_.get<instructions::cbrt_f64_check>();
		return bz::format("cbrt f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::hypot_f32:
	{
		auto const &inst = inst_.get<instructions::hypot_f32>();
		return bz::format("hypot f32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::hypot_f64:
	{
		auto const &inst = inst_.get<instructions::hypot_f64>();
		return bz::format("hypot f64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::hypot_f32_check:
	{
		auto const &inst = inst_.get<instructions::hypot_f32_check>();
		return bz::format("hypot f32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::hypot_f64_check:
	{
		auto const &inst = inst_.get<instructions::hypot_f64_check>();
		return bz::format("hypot f64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::sin_f32:
	{
		auto const &inst = inst_.get<instructions::sin_f32>();
		return bz::format("sin f32 {}", inst.args[0]);
	}
	case instruction::sin_f64:
	{
		auto const &inst = inst_.get<instructions::sin_f64>();
		return bz::format("sin f64 {}", inst.args[0]);
	}
	case instruction::sin_f32_check:
	{
		auto const &inst = inst_.get<instructions::sin_f32_check>();
		return bz::format("sin f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::sin_f64_check:
	{
		auto const &inst = inst_.get<instructions::sin_f64_check>();
		return bz::format("sin f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::cos_f32:
	{
		auto const &inst = inst_.get<instructions::cos_f32>();
		return bz::format("cos f32 {}", inst.args[0]);
	}
	case instruction::cos_f64:
	{
		auto const &inst = inst_.get<instructions::cos_f64>();
		return bz::format("cos f64 {}", inst.args[0]);
	}
	case instruction::cos_f32_check:
	{
		auto const &inst = inst_.get<instructions::cos_f32_check>();
		return bz::format("cos f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::cos_f64_check:
	{
		auto const &inst = inst_.get<instructions::cos_f64_check>();
		return bz::format("cos f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::tan_f32:
	{
		auto const &inst = inst_.get<instructions::tan_f32>();
		return bz::format("tan f32 {}", inst.args[0]);
	}
	case instruction::tan_f64:
	{
		auto const &inst = inst_.get<instructions::tan_f64>();
		return bz::format("tan f64 {}", inst.args[0]);
	}
	case instruction::tan_f32_check:
	{
		auto const &inst = inst_.get<instructions::tan_f32_check>();
		return bz::format("tan f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::tan_f64_check:
	{
		auto const &inst = inst_.get<instructions::tan_f64_check>();
		return bz::format("tan f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::asin_f32:
	{
		auto const &inst = inst_.get<instructions::asin_f32>();
		return bz::format("asin f32 {}", inst.args[0]);
	}
	case instruction::asin_f64:
	{
		auto const &inst = inst_.get<instructions::asin_f64>();
		return bz::format("asin f64 {}", inst.args[0]);
	}
	case instruction::asin_f32_check:
	{
		auto const &inst = inst_.get<instructions::asin_f32_check>();
		return bz::format("asin f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::asin_f64_check:
	{
		auto const &inst = inst_.get<instructions::asin_f64_check>();
		return bz::format("asin f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::acos_f32:
	{
		auto const &inst = inst_.get<instructions::acos_f32>();
		return bz::format("acos f32 {}", inst.args[0]);
	}
	case instruction::acos_f64:
	{
		auto const &inst = inst_.get<instructions::acos_f64>();
		return bz::format("acos f64 {}", inst.args[0]);
	}
	case instruction::acos_f32_check:
	{
		auto const &inst = inst_.get<instructions::acos_f32_check>();
		return bz::format("acos f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::acos_f64_check:
	{
		auto const &inst = inst_.get<instructions::acos_f64_check>();
		return bz::format("acos f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::atan_f32:
	{
		auto const &inst = inst_.get<instructions::atan_f32>();
		return bz::format("atan f32 {}", inst.args[0]);
	}
	case instruction::atan_f64:
	{
		auto const &inst = inst_.get<instructions::atan_f64>();
		return bz::format("atan f64 {}", inst.args[0]);
	}
	case instruction::atan_f32_check:
	{
		auto const &inst = inst_.get<instructions::atan_f32_check>();
		return bz::format("atan f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::atan_f64_check:
	{
		auto const &inst = inst_.get<instructions::atan_f64_check>();
		return bz::format("atan f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::atan2_f32:
	{
		auto const &inst = inst_.get<instructions::atan2_f32>();
		return bz::format("atan2 f32 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::atan2_f64:
	{
		auto const &inst = inst_.get<instructions::atan2_f64>();
		return bz::format("atan2 f64 {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::atan2_f32_check:
	{
		auto const &inst = inst_.get<instructions::atan2_f32_check>();
		return bz::format("atan2 f32 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::atan2_f64_check:
	{
		auto const &inst = inst_.get<instructions::atan2_f64_check>();
		return bz::format("atan2 f64 check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::sinh_f32:
	{
		auto const &inst = inst_.get<instructions::sinh_f32>();
		return bz::format("sinh f32 {}", inst.args[0]);
	}
	case instruction::sinh_f64:
	{
		auto const &inst = inst_.get<instructions::sinh_f64>();
		return bz::format("sinh f64 {}", inst.args[0]);
	}
	case instruction::sinh_f32_check:
	{
		auto const &inst = inst_.get<instructions::sinh_f32_check>();
		return bz::format("sinh f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::sinh_f64_check:
	{
		auto const &inst = inst_.get<instructions::sinh_f64_check>();
		return bz::format("sinh f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::cosh_f32:
	{
		auto const &inst = inst_.get<instructions::cosh_f32>();
		return bz::format("cosh f32 {}", inst.args[0]);
	}
	case instruction::cosh_f64:
	{
		auto const &inst = inst_.get<instructions::cosh_f64>();
		return bz::format("cosh f64 {}", inst.args[0]);
	}
	case instruction::cosh_f32_check:
	{
		auto const &inst = inst_.get<instructions::cosh_f32_check>();
		return bz::format("cosh f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::cosh_f64_check:
	{
		auto const &inst = inst_.get<instructions::cosh_f64_check>();
		return bz::format("cosh f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::tanh_f32:
	{
		auto const &inst = inst_.get<instructions::tanh_f32>();
		return bz::format("tanh f32 {}", inst.args[0]);
	}
	case instruction::tanh_f64:
	{
		auto const &inst = inst_.get<instructions::tanh_f64>();
		return bz::format("tanh f64 {}", inst.args[0]);
	}
	case instruction::tanh_f32_check:
	{
		auto const &inst = inst_.get<instructions::tanh_f32_check>();
		return bz::format("tanh f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::tanh_f64_check:
	{
		auto const &inst = inst_.get<instructions::tanh_f64_check>();
		return bz::format("tanh f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::asinh_f32:
	{
		auto const &inst = inst_.get<instructions::asinh_f32>();
		return bz::format("asinh f32 {}", inst.args[0]);
	}
	case instruction::asinh_f64:
	{
		auto const &inst = inst_.get<instructions::asinh_f64>();
		return bz::format("asinh f64 {}", inst.args[0]);
	}
	case instruction::asinh_f32_check:
	{
		auto const &inst = inst_.get<instructions::asinh_f32_check>();
		return bz::format("asinh f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::asinh_f64_check:
	{
		auto const &inst = inst_.get<instructions::asinh_f64_check>();
		return bz::format("asinh f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::acosh_f32:
	{
		auto const &inst = inst_.get<instructions::acosh_f32>();
		return bz::format("acosh f32 {}", inst.args[0]);
	}
	case instruction::acosh_f64:
	{
		auto const &inst = inst_.get<instructions::acosh_f64>();
		return bz::format("acosh f64 {}", inst.args[0]);
	}
	case instruction::acosh_f32_check:
	{
		auto const &inst = inst_.get<instructions::acosh_f32_check>();
		return bz::format("acosh f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::acosh_f64_check:
	{
		auto const &inst = inst_.get<instructions::acosh_f64_check>();
		return bz::format("acosh f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::atanh_f32:
	{
		auto const &inst = inst_.get<instructions::atanh_f32>();
		return bz::format("atanh f32 {}", inst.args[0]);
	}
	case instruction::atanh_f64:
	{
		auto const &inst = inst_.get<instructions::atanh_f64>();
		return bz::format("atanh f64 {}", inst.args[0]);
	}
	case instruction::atanh_f32_check:
	{
		auto const &inst = inst_.get<instructions::atanh_f32_check>();
		return bz::format("atanh f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::atanh_f64_check:
	{
		auto const &inst = inst_.get<instructions::atanh_f64_check>();
		return bz::format("atanh f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::erf_f32:
	{
		auto const &inst = inst_.get<instructions::erf_f32>();
		return bz::format("erf f32 {}", inst.args[0]);
	}
	case instruction::erf_f64:
	{
		auto const &inst = inst_.get<instructions::erf_f64>();
		return bz::format("erf f64 {}", inst.args[0]);
	}
	case instruction::erf_f32_check:
	{
		auto const &inst = inst_.get<instructions::erf_f32_check>();
		return bz::format("erf f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::erf_f64_check:
	{
		auto const &inst = inst_.get<instructions::erf_f64_check>();
		return bz::format("erf f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::erfc_f32:
	{
		auto const &inst = inst_.get<instructions::erfc_f32>();
		return bz::format("erfc f32 {}", inst.args[0]);
	}
	case instruction::erfc_f64:
	{
		auto const &inst = inst_.get<instructions::erfc_f64>();
		return bz::format("erfc f64 {}", inst.args[0]);
	}
	case instruction::erfc_f32_check:
	{
		auto const &inst = inst_.get<instructions::erfc_f32_check>();
		return bz::format("erfc f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::erfc_f64_check:
	{
		auto const &inst = inst_.get<instructions::erfc_f64_check>();
		return bz::format("erfc f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::tgamma_f32:
	{
		auto const &inst = inst_.get<instructions::tgamma_f32>();
		return bz::format("tgamma f32 {}", inst.args[0]);
	}
	case instruction::tgamma_f64:
	{
		auto const &inst = inst_.get<instructions::tgamma_f64>();
		return bz::format("tgamma f64 {}", inst.args[0]);
	}
	case instruction::tgamma_f32_check:
	{
		auto const &inst = inst_.get<instructions::tgamma_f32_check>();
		return bz::format("tgamma f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::tgamma_f64_check:
	{
		auto const &inst = inst_.get<instructions::tgamma_f64_check>();
		return bz::format("tgamma f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::lgamma_f32:
	{
		auto const &inst = inst_.get<instructions::lgamma_f32>();
		return bz::format("lgamma f32 {}", inst.args[0]);
	}
	case instruction::lgamma_f64:
	{
		auto const &inst = inst_.get<instructions::lgamma_f64>();
		return bz::format("lgamma f64 {}", inst.args[0]);
	}
	case instruction::lgamma_f32_check:
	{
		auto const &inst = inst_.get<instructions::lgamma_f32_check>();
		return bz::format("lgamma f32 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::lgamma_f64_check:
	{
		auto const &inst = inst_.get<instructions::lgamma_f64_check>();
		return bz::format("lgamma f64 check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::bitreverse_u8:
	{
		auto const &inst = inst_.get<instructions::bitreverse_u8>();
		return bz::format("bitreverse u8 {}", inst.args[0]);
	}
	case instruction::bitreverse_u16:
	{
		auto const &inst = inst_.get<instructions::bitreverse_u16>();
		return bz::format("bitreverse u16 {}", inst.args[0]);
	}
	case instruction::bitreverse_u32:
	{
		auto const &inst = inst_.get<instructions::bitreverse_u32>();
		return bz::format("bitreverse u32 {}", inst.args[0]);
	}
	case instruction::bitreverse_u64:
	{
		auto const &inst = inst_.get<instructions::bitreverse_u64>();
		return bz::format("bitreverse u64 {}", inst.args[0]);
	}
	case instruction::popcount_u8:
	{
		auto const &inst = inst_.get<instructions::popcount_u8>();
		return bz::format("popcount u8 {}", inst.args[0]);
	}
	case instruction::popcount_u16:
	{
		auto const &inst = inst_.get<instructions::popcount_u16>();
		return bz::format("popcount u16 {}", inst.args[0]);
	}
	case instruction::popcount_u32:
	{
		auto const &inst = inst_.get<instructions::popcount_u32>();
		return bz::format("popcount u32 {}", inst.args[0]);
	}
	case instruction::popcount_u64:
	{
		auto const &inst = inst_.get<instructions::popcount_u64>();
		return bz::format("popcount u64 {}", inst.args[0]);
	}
	case instruction::byteswap_u16:
	{
		auto const &inst = inst_.get<instructions::byteswap_u16>();
		return bz::format("byteswap u16 {}", inst.args[0]);
	}
	case instruction::byteswap_u32:
	{
		auto const &inst = inst_.get<instructions::byteswap_u32>();
		return bz::format("byteswap u32 {}", inst.args[0]);
	}
	case instruction::byteswap_u64:
	{
		auto const &inst = inst_.get<instructions::byteswap_u64>();
		return bz::format("byteswap u64 {}", inst.args[0]);
	}
	case instruction::clz_u8:
	{
		auto const &inst = inst_.get<instructions::clz_u8>();
		return bz::format("clz u8 {}", inst.args[0]);
	}
	case instruction::clz_u16:
	{
		auto const &inst = inst_.get<instructions::clz_u16>();
		return bz::format("clz u16 {}", inst.args[0]);
	}
	case instruction::clz_u32:
	{
		auto const &inst = inst_.get<instructions::clz_u32>();
		return bz::format("clz u32 {}", inst.args[0]);
	}
	case instruction::clz_u64:
	{
		auto const &inst = inst_.get<instructions::clz_u64>();
		return bz::format("clz u64 {}", inst.args[0]);
	}
	case instruction::ctz_u8:
	{
		auto const &inst = inst_.get<instructions::ctz_u8>();
		return bz::format("ctz u8 {}", inst.args[0]);
	}
	case instruction::ctz_u16:
	{
		auto const &inst = inst_.get<instructions::ctz_u16>();
		return bz::format("ctz u16 {}", inst.args[0]);
	}
	case instruction::ctz_u32:
	{
		auto const &inst = inst_.get<instructions::ctz_u32>();
		return bz::format("ctz u32 {}", inst.args[0]);
	}
	case instruction::ctz_u64:
	{
		auto const &inst = inst_.get<instructions::ctz_u64>();
		return bz::format("ctz u64 {}", inst.args[0]);
	}
	case instruction::fshl_u8:
	{
		auto const &inst = inst_.get<instructions::fshl_u8>();
		return bz::format("fshl u8 {}, {}, {}", inst.args[0], inst.args[1], inst.args[2]);
	}
	case instruction::fshl_u16:
	{
		auto const &inst = inst_.get<instructions::fshl_u16>();
		return bz::format("fshl u16 {}, {}, {}", inst.args[0], inst.args[1], inst.args[2]);
	}
	case instruction::fshl_u32:
	{
		auto const &inst = inst_.get<instructions::fshl_u32>();
		return bz::format("fshl u32 {}, {}, {}", inst.args[0], inst.args[1], inst.args[2]);
	}
	case instruction::fshl_u64:
	{
		auto const &inst = inst_.get<instructions::fshl_u64>();
		return bz::format("fshl u64 {}, {}, {}", inst.args[0], inst.args[1], inst.args[2]);
	}
	case instruction::fshr_u8:
	{
		auto const &inst = inst_.get<instructions::fshr_u8>();
		return bz::format("fshr u8 {}, {}, {}", inst.args[0], inst.args[1], inst.args[2]);
	}
	case instruction::fshr_u16:
	{
		auto const &inst = inst_.get<instructions::fshr_u16>();
		return bz::format("fshr u16 {}, {}, {}", inst.args[0], inst.args[1], inst.args[2]);
	}
	case instruction::fshr_u32:
	{
		auto const &inst = inst_.get<instructions::fshr_u32>();
		return bz::format("fshr u32 {}, {}, {}", inst.args[0], inst.args[1], inst.args[2]);
	}
	case instruction::fshr_u64:
	{
		auto const &inst = inst_.get<instructions::fshr_u64>();
		return bz::format("fshr u64 {}, {}, {}", inst.args[0], inst.args[1], inst.args[2]);
	}
	case instruction::const_gep:
	{
		auto const &inst = inst_.get<instructions::const_gep>();
		return bz::format("gep {} {} {}", inst.object_type->to_string(), inst.index, inst.args[0]);
	}
	case instruction::array_gep_i32:
	{
		auto const &inst = inst_.get<instructions::array_gep_i32>();
		return bz::format("array-gep i32 {} {} {}", inst.elem_type->to_string(), inst.args[0], inst.args[1]);
	}
	case instruction::array_gep_i64:
	{
		auto const &inst = inst_.get<instructions::array_gep_i64>();
		return bz::format("array-gep i64 {} {} {}", inst.elem_type->to_string(), inst.args[0], inst.args[1]);
	}
	case instruction::const_memcpy:
	{
		auto const &inst = inst_.get<instructions::const_memcpy>();
		return bz::format("const-memcpy {} {}, {}", inst.size, inst.args[0], inst.args[1]);
	}
	case instruction::const_memset_zero:
	{
		auto const &inst = inst_.get<instructions::const_memset_zero>();
		return bz::format("const-memset-zero {} {}", inst.size, inst.args[0]);
	}
	case instruction::copy_values:
	{
		auto const &inst = inst_.get<instructions::copy_values>();
		return bz::format("copy-values {}, {}, {} ({})", inst.args[0], inst.args[1], inst.args[2], inst.copy_values_info_index);
	}
	case instruction::copy_overlapping_values:
	{
		auto const &inst = inst_.get<instructions::copy_overlapping_values>();
		return bz::format("copy-overlapping-values {}, {}, {} ({})", inst.args[0], inst.args[1], inst.args[2], inst.copy_values_info_index);
	}
	case instruction::relocate_values:
	{
		auto const &inst = inst_.get<instructions::relocate_values>();
		return bz::format("relocate-values {}, {}, {} ({})", inst.args[0], inst.args[1], inst.args[2], inst.copy_values_info_index);
	}
	case instruction::set_values_i1_be:
	{
		auto const &inst = inst_.get<instructions::set_values_i1_be>();
		return bz::format("set-values i1 be {}, {}, {} ({})", inst.args[0], inst.args[1], inst.args[2], inst.src_tokens_index);
	}
	case instruction::set_values_i8_be:
	{
		auto const &inst = inst_.get<instructions::set_values_i8_be>();
		return bz::format("set-values i8 be {}, {}, {} ({})", inst.args[0], inst.args[1], inst.args[2], inst.src_tokens_index);
	}
	case instruction::set_values_i16_be:
	{
		auto const &inst = inst_.get<instructions::set_values_i16_be>();
		return bz::format("set-values i16 be {}, {}, {} ({})", inst.args[0], inst.args[1], inst.args[2], inst.src_tokens_index);
	}
	case instruction::set_values_i32_be:
	{
		auto const &inst = inst_.get<instructions::set_values_i32_be>();
		return bz::format("set-values i32 be {}, {}, {} ({})", inst.args[0], inst.args[1], inst.args[2], inst.src_tokens_index);
	}
	case instruction::set_values_i64_be:
	{
		auto const &inst = inst_.get<instructions::set_values_i64_be>();
		return bz::format("set-values i64 be {}, {}, {} ({})", inst.args[0], inst.args[1], inst.args[2], inst.src_tokens_index);
	}
	case instruction::set_values_f32_be:
	{
		auto const &inst = inst_.get<instructions::set_values_f32_be>();
		return bz::format("set-values f32 be {}, {}, {} ({})", inst.args[0], inst.args[1], inst.args[2], inst.src_tokens_index);
	}
	case instruction::set_values_f64_be:
	{
		auto const &inst = inst_.get<instructions::set_values_f64_be>();
		return bz::format("set-values f64 be {}, {}, {} ({})", inst.args[0], inst.args[1], inst.args[2], inst.src_tokens_index);
	}
	case instruction::set_values_ptr32_be:
	{
		auto const &inst = inst_.get<instructions::set_values_ptr32_be>();
		return bz::format("set-values ptr32 be {}, {}, {} ({})", inst.args[0], inst.args[1], inst.args[2], inst.src_tokens_index);
	}
	case instruction::set_values_ptr64_be:
	{
		auto const &inst = inst_.get<instructions::set_values_ptr64_be>();
		return bz::format("set-values ptr64 be {}, {}, {} ({})", inst.args[0], inst.args[1], inst.args[2], inst.src_tokens_index);
	}
	case instruction::set_values_i1_le:
	{
		auto const &inst = inst_.get<instructions::set_values_i1_le>();
		return bz::format("set-values i1 le {}, {}, {} ({})", inst.args[0], inst.args[1], inst.args[2], inst.src_tokens_index);
	}
	case instruction::set_values_i8_le:
	{
		auto const &inst = inst_.get<instructions::set_values_i8_le>();
		return bz::format("set-values i8 le {}, {}, {} ({})", inst.args[0], inst.args[1], inst.args[2], inst.src_tokens_index);
	}
	case instruction::set_values_i16_le:
	{
		auto const &inst = inst_.get<instructions::set_values_i16_le>();
		return bz::format("set-values i16 le {}, {}, {} ({})", inst.args[0], inst.args[1], inst.args[2], inst.src_tokens_index);
	}
	case instruction::set_values_i32_le:
	{
		auto const &inst = inst_.get<instructions::set_values_i32_le>();
		return bz::format("set-values i32 le {}, {}, {} ({})", inst.args[0], inst.args[1], inst.args[2], inst.src_tokens_index);
	}
	case instruction::set_values_i64_le:
	{
		auto const &inst = inst_.get<instructions::set_values_i64_le>();
		return bz::format("set-values i64 le {}, {}, {} ({})", inst.args[0], inst.args[1], inst.args[2], inst.src_tokens_index);
	}
	case instruction::set_values_f32_le:
	{
		auto const &inst = inst_.get<instructions::set_values_f32_le>();
		return bz::format("set-values f32 le {}, {}, {} ({})", inst.args[0], inst.args[1], inst.args[2], inst.src_tokens_index);
	}
	case instruction::set_values_f64_le:
	{
		auto const &inst = inst_.get<instructions::set_values_f64_le>();
		return bz::format("set-values f64 le {}, {}, {} ({})", inst.args[0], inst.args[1], inst.args[2], inst.src_tokens_index);
	}
	case instruction::set_values_ptr32_le:
	{
		auto const &inst = inst_.get<instructions::set_values_ptr32_le>();
		return bz::format("set-values ptr32 le {}, {}, {} ({})", inst.args[0], inst.args[1], inst.args[2], inst.src_tokens_index);
	}
	case instruction::set_values_ptr64_le:
	{
		auto const &inst = inst_.get<instructions::set_values_ptr64_le>();
		return bz::format("set-values ptr64 le {}, {}, {} ({})", inst.args[0], inst.args[1], inst.args[2], inst.src_tokens_index);
	}
	case instruction::set_values_ref:
	{
		auto const &inst = inst_.get<instructions::set_values_ref>();
		return bz::format("set-values ref {}, {}, {} ({})", inst.args[0], inst.args[1], inst.args[2], inst.copy_values_info_index);
	}
	case instruction::function_call:
	{
		auto const &inst = inst_.get<instructions::function_call>();
		if (func != nullptr)
		{
			bz::u8string result = bz::format("call '{}'(", inst.func->func_body->get_signature());
			auto const args = func->call_args[inst.args_index].as_array_view();
			if (args.not_empty())
			{
				result += bz::format("{}", args[0]);
				for (auto const arg : args.slice(1))
				{
					result += bz::format(", {}", arg);
				}
			}
			result += bz::format(") ({})", inst.src_tokens_index);
			return result;
		}
		else
		{
			return bz::format("call '{}' ({}) ({})", inst.func->func_body->get_signature(), inst.args_index, inst.src_tokens_index);
		}
	}
	case instruction::indirect_function_call:
	{
		auto const &inst = inst_.get<instructions::indirect_function_call>();
		if (func != nullptr)
		{
			bz::u8string result = bz::format("call {}(", inst.args[0]);
			auto const args = func->call_args[inst.args_index].as_array_view();
			if (args.not_empty())
			{
				result += bz::format("{}", args[0]);
				for (auto const arg : args.slice(1))
				{
					result += bz::format(", {}", arg);
				}
			}
			result += bz::format(") ({})", inst.src_tokens_index);
			return result;
		}
		else
		{
			return bz::format("call {} ({}) ({})", inst.args[0], inst.args_index, inst.src_tokens_index);
		}
	}
	case instruction::malloc:
	{
		auto const &inst = inst_.get<instructions::malloc>();
		return bz::format("malloc {} {} ({})", inst.elem_type->to_string(), inst.args[0], inst.src_tokens_index);
	}
	case instruction::free:
	{
		auto const &inst = inst_.get<instructions::free>();
		return bz::format("free {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::jump:
	{
		auto const &inst = inst_.get<instructions::jump>();
		if (func != nullptr)
		{
			return bz::format("jump {}", instruction_value_index{ inst.dest.index + static_cast<uint32_t>(func->allocas.size()) });
		}
		else
		{
			return bz::format("jump {}", inst.dest.index);
		}
	}
	case instruction::conditional_jump:
	{
		auto const &inst = inst_.get<instructions::conditional_jump>();
		if (func != nullptr)
		{
			return bz::format(
				"jump {}, {}, {}",
				inst.args[0],
				instruction_value_index{ inst.true_dest.index + static_cast<uint32_t>(func->allocas.size()) },
				instruction_value_index{ inst.false_dest.index + static_cast<uint32_t>(func->allocas.size()) }
			);
		}
		else
		{
			return bz::format("jump {}, {}, {}", inst.args[0], inst.true_dest.index, inst.false_dest.index);
		}
	}
	case instruction::switch_i1:
	{
		auto const &inst = inst_.get<instructions::switch_i1>();
		return bz::format("switch i1 {} ({})", inst.args[0], inst.switch_info_index);
	}
	case instruction::switch_i8:
	{
		auto const &inst = inst_.get<instructions::switch_i8>();
		return bz::format("switch i8 {} ({})", inst.args[0], inst.switch_info_index);
	}
	case instruction::switch_i16:
	{
		auto const &inst = inst_.get<instructions::switch_i16>();
		return bz::format("switch i16 {} ({})", inst.args[0], inst.switch_info_index);
	}
	case instruction::switch_i32:
	{
		auto const &inst = inst_.get<instructions::switch_i32>();
		return bz::format("switch i32 {} ({})", inst.args[0], inst.switch_info_index);
	}
	case instruction::switch_i64:
	{
		auto const &inst = inst_.get<instructions::switch_i64>();
		return bz::format("switch i64 {} ({})", inst.args[0], inst.switch_info_index);
	}
	case instruction::switch_str:
	{
		auto const &inst = inst_.get<instructions::switch_str>();
		return bz::format("switch str {}, {} ({})", inst.args[0], inst.args[1], inst.switch_str_info_index);
	}
	case instruction::ret:
	{
		auto const &inst = inst_.get<instructions::ret>();
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
		auto const &inst = inst_.get<instructions::error>();
		return bz::format("error {}", inst.error_index);
	}
	case instruction::print:
	{
		auto const &inst = inst_.get<instructions::print>();
		return bz::format("print {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::diagnostic_str:
	{
		auto const &inst = inst_.get<instructions::diagnostic_str>();
		return bz::format("diagnostic {} {}, {} ({})", (int)inst.kind, inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::is_option_set:
	{
		auto const &inst = inst_.get<instructions::is_option_set>();
		return bz::format("is-option-set {}, {}", inst.args[0], inst.args[1]);
	}
	case instruction::add_global_array_data:
	{
		auto const &inst = inst_.get<instructions::add_global_array_data>();
		return bz::format("add-global-array-data {}, {} ({})", inst.args[0], inst.args[1], inst.add_global_array_data_info_index);
	}
	case instruction::array_bounds_check_i32:
	{
		auto const &inst = inst_.get<instructions::array_bounds_check_i32>();
		return bz::format("array-bounds-check i32 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::array_bounds_check_u32:
	{
		auto const &inst = inst_.get<instructions::array_bounds_check_u32>();
		return bz::format("array-bounds-check u32 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::array_bounds_check_i64:
	{
		auto const &inst = inst_.get<instructions::array_bounds_check_i64>();
		return bz::format("array-bounds-check i64 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::array_bounds_check_u64:
	{
		auto const &inst = inst_.get<instructions::array_bounds_check_u64>();
		return bz::format("array-bounds-check u64 {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::optional_get_value_check:
	{
		auto const &inst = inst_.get<instructions::optional_get_value_check>();
		return bz::format("optional-get_value-check {} ({})", inst.args[0], inst.src_tokens_index);
	}
	case instruction::str_construction_check:
	{
		auto const &inst = inst_.get<instructions::str_construction_check>();
		return bz::format("str-construction-check {}, {} ({})", inst.args[0], inst.args[1], inst.src_tokens_index);
	}
	case instruction::slice_construction_check:
	{
		auto const &inst = inst_.get<instructions::slice_construction_check>();
		return bz::format("slice-construction-check {}, {} ({}, {})", inst.args[0], inst.args[1], inst.src_tokens_index, inst.slice_construction_check_info_index);
	}
	case instruction::start_lifetime:
	{
		auto const &inst = inst_.get<instructions::start_lifetime>();
		return bz::format("start-lifetime {} {}", inst.size, inst.args[0]);
	}
	case instruction::end_lifetime:
	{
		auto const &inst = inst_.get<instructions::end_lifetime>();
		return bz::format("end-lifetime {} {}", inst.size, inst.args[0]);
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
		result += bz::format("'{}'(", func.func_body->get_signature());
	}
	else
	{
		result += "anon-function(";
	}

	if (func.arg_types.not_empty())
	{
		result += func.arg_types[0]->to_string();
		for (auto const type : func.arg_types.slice(1))
		{
			result += ", ";
			result += type->to_string();
		}
	}
	result += bz::format(") -> {}:\n", func.return_type->to_string());

	uint32_t i = 0;

	for (auto const &alloca : func.allocas)
	{
		result += bz::format(
			"  {} = alloca {} ({})\n",
			instruction_value_index{ .index = i },
			alloca.object_type->to_string(),
			alloca.is_always_initialized
		);
		++i;
	}

	for (auto const &inst : func.instructions)
	{
		result += bz::format("  {} = {}\n", instruction_value_index{ .index = i }, to_string(inst, &func));
		++i;
	}

	return result;
}

} // namespace comptime
