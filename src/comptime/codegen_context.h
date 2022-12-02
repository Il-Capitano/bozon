#ifndef COMPTIME_CODEGEN_CONTEXT_H
#define COMPTIME_CODEGEN_CONTEXT_H

#include "instructions.h"
#include "types.h"
#include "global_codegen_context.h"
#include "ast/expression.h"

namespace comptime
{

struct codegen_context;

struct basic_block_ref
{
	uint32_t bb_index;
};

struct instruction_ref
{
	uint32_t bb_index;
	uint32_t inst_index;

	bool operator == (instruction_ref const &rhs) const = default;

	static inline constexpr uint32_t alloca_bb_index = std::numeric_limits<uint32_t>::max();
};

enum class expr_value_kind
{
	none,
	reference,
	value,
};

struct expr_value
{
	instruction_ref value;
	expr_value_kind kind;
	type const *value_type;

	bool is_value(void) const;
	bool is_reference(void) const;
	bool is_none(void) const;

	expr_value get_value(codegen_context &context) const;
	instruction_ref get_value_as_instruction(codegen_context &context) const;
	instruction_ref get_reference(void) const;
	type const *get_type(void) const;

	static expr_value get_none(void)
	{
		return { .value = {}, .kind = expr_value_kind::none, .value_type = nullptr };
	}

	static expr_value get_reference(instruction_ref value, type const *value_type)
	{
		return { .value = value, .kind = expr_value_kind::reference, .value_type = value_type };
	}

	static expr_value get_value(instruction_ref value, type const *value_type)
	{
		return { .value = value, .kind = expr_value_kind::value, .value_type = value_type };
	}

	bool operator == (expr_value const &rhs) const = default;
};

struct unresolved_instruction
{
	instruction_ref inst;
	bz::array<instruction_ref, 3> args;
};

struct unresolved_jump
{
	instruction_ref inst;
	bz::array<basic_block_ref, 2> dests;
};

struct codegen_context
{
	basic_block_ref current_bb = {};
	bz::optional<expr_value> function_return_address;
	bz::vector<alloca> allocas;
	bz::vector<basic_block> blocks;
	bz::vector<bz::fixed_vector<instruction_ref>> call_args;
	bz::vector<unresolved_instruction> unresolved_instructions;
	bz::vector<unresolved_jump> unresolved_jumps;

	struct destruct_operation_info_t
	{
		ast::destruct_operation const *destruct_op;
		expr_value value;
		bz::optional<instruction_ref> condition;
		bz::optional<instruction_ref> move_destruct_indicator;
		bz::optional<instruction_ref> rvalue_array_elem_ptr;
	};

	bz::vector<destruct_operation_info_t> destructor_calls;
	std::unordered_map<ast::decl_variable const *, instruction_ref> move_destruct_indicators;
	std::unordered_map<ast::decl_variable const *, expr_value> variables;

	struct loop_info_t
	{
		basic_block_ref break_bb = {};
		basic_block_ref continue_bb = {};
		size_t destructor_stack_begin = 0;
		bool in_loop = false;
	};

	loop_info_t loop_info = {};
	bz::array<expr_value, 4> current_value_references = {};
	size_t current_value_reference_stack_size = 0;

	global_codegen_context *global_codegen_ctx;

	bool is_little_endian(void) const;
	bool is_big_endian(void) const;
	bool is_64_bit(void) const;
	bool is_32_bit(void) const;

	void add_variable(ast::decl_variable const *decl, expr_value value);
	expr_value get_variable(ast::decl_variable const *decl);
	function const *get_function(ast::function_body const *body);

	type const *get_builtin_type(builtin_type_kind kind);
	type const *get_pointer_type(void);
	type const *get_aggregate_type(bz::array_view<type const * const> elem_types);
	type const *get_array_type(type const *elem_type, size_t size);
	type const *get_str_t(void);
	type const *get_null_t(void);
	type const *get_slice_t(void);
	type const *get_optional_type(type const *value_type);

	// control flow structure
	basic_block_ref get_current_basic_block(void);
	basic_block_ref add_basic_block(void);
	void set_current_basic_block(basic_block_ref bb_index);

	struct expression_scope_info_t
	{
		size_t destructor_calls_size;
	};

	// RAII helpers
	[[nodiscard]] expression_scope_info_t push_expression_scope(void);
	void pop_expression_scope(expression_scope_info_t prev_info);

	[[nodiscard]] loop_info_t push_loop(basic_block_ref break_bb, basic_block_ref continue_bb);
	void pop_loop(loop_info_t prev_info);

	[[nodiscard]] expr_value push_value_reference(expr_value new_value);
	void pop_value_reference(expr_value prev_value);
	expr_value get_value_reference(size_t index);

	instruction_ref add_move_destruct_indicator(ast::decl_variable const *decl);
	bz::optional<instruction_ref> get_move_destruct_indicator(ast::decl_variable const *decl) const;

	void push_destruct_operation(ast::destruct_operation const &destruct_op);
	void push_variable_destruct_operation(
		ast::destruct_operation const &destruct_op,
		bz::optional<instruction_ref> move_destruct_indicator = {}
	);
	void push_self_destruct_operation(
		ast::destruct_operation const &destruct_op,
		expr_value value
	);
	void push_rvalue_array_destruct_operation(
		ast::destruct_operation const &destruct_op,
		expr_value value,
		instruction_ref rvalue_array_elem_ptr
	);
	void emit_destruct_operations(size_t start_index);
	void emit_loop_destruct_operations(void);
	void emit_all_destruct_operations(void);

	// instruction creation functions
	template<typename Inst>
	instruction_ref add_instruction(Inst inst)
	{
		static_assert(instructions::arg_count<Inst> == 0);
		instruction new_inst = instruction();
		new_inst.emplace<instructions::instruction_with_args<Inst>>(instructions::instruction_with_args<Inst>{
			.inst = std::move(inst)
		});
		this->blocks[this->current_bb.bb_index].instructions.push_back(std::move(new_inst));
		auto const result = instruction_ref{
			.bb_index   = this->current_bb.bb_index,
			.inst_index = static_cast<uint32_t>(this->blocks[this->current_bb.bb_index].instructions.size() - 1),
		};
		return result;
	}

	template<typename Inst>
	instruction_ref add_instruction(Inst inst, instruction_ref arg)
	{
		static_assert(instructions::arg_count<Inst> == 1);
		instruction new_inst = instruction();
		new_inst.emplace<instructions::instruction_with_args<Inst>>(instructions::instruction_with_args<Inst>{
			.args = {},
			.inst = std::move(inst)
		});
		this->blocks[this->current_bb.bb_index].instructions.push_back(std::move(new_inst));
		auto const result = instruction_ref{
			.bb_index   = this->current_bb.bb_index,
			.inst_index = static_cast<uint32_t>(this->blocks[this->current_bb.bb_index].instructions.size() - 1),
		};
		this->unresolved_instructions.push_back({
			.inst = result,
			.args = { arg, {} },
		});
		return result;
	}

	template<typename Inst>
	instruction_ref add_instruction(Inst inst, instruction_ref arg1, instruction_ref arg2)
	{
		static_assert(instructions::arg_count<Inst> == 2);
		instruction new_inst = instruction();
		new_inst.emplace<instructions::instruction_with_args<Inst>>(instructions::instruction_with_args<Inst>{
			.args = {},
			.inst = std::move(inst)
		});
		this->blocks[this->current_bb.bb_index].instructions.push_back(std::move(new_inst));
		auto const result = instruction_ref{
			.bb_index   = this->current_bb.bb_index,
			.inst_index = static_cast<uint32_t>(this->blocks[this->current_bb.bb_index].instructions.size() - 1),
		};
		this->unresolved_instructions.push_back({
			.inst = result,
			.args = { arg1, arg2 },
		});
		return result;
	}

	template<typename Inst>
	instruction_ref add_instruction(Inst inst, instruction_ref arg1, instruction_ref arg2, instruction_ref arg3)
	{
		static_assert(instructions::arg_count<Inst> == 3);
		instruction new_inst = instruction();
		new_inst.emplace<instructions::instruction_with_args<Inst>>(instructions::instruction_with_args<Inst>{
			.args = {},
			.inst = std::move(inst)
		});
		this->blocks[this->current_bb.bb_index].instructions.push_back(std::move(new_inst));
		auto const result = instruction_ref{
			.bb_index   = this->current_bb.bb_index,
			.inst_index = static_cast<uint32_t>(this->blocks[this->current_bb.bb_index].instructions.size() - 1),
		};
		this->unresolved_instructions.push_back({
			.inst = result,
			.args = { arg1, arg2, arg3 },
		});
		return result;
	}

	uint32_t add_src_tokens(lex::src_tokens const &src_tokens);

	expr_value get_dummy_value(type const *t);

	expr_value create_const_i1(bool value);
	expr_value create_const_i8(int8_t value);
	expr_value create_const_i16(int16_t value);
	expr_value create_const_i32(int32_t value);
	expr_value create_const_i64(int64_t value);
	expr_value create_const_u8(uint8_t value);
	expr_value create_const_u16(uint16_t value);
	expr_value create_const_u32(uint32_t value);
	expr_value create_const_u64(uint64_t value);
	expr_value create_const_f32(float32_t value);
	expr_value create_const_f64(float64_t value);
	expr_value create_const_ptr_null(void);

	expr_value create_load(expr_value ptr);
	instruction_ref create_store(expr_value value, expr_value ptr);

	expr_value create_alloca(type const *type);

	instruction_ref create_jump(basic_block_ref bb);
	instruction_ref create_conditional_jump(expr_value condition, basic_block_ref true_bb, basic_block_ref false_bb);
	instruction_ref create_ret(instruction_ref value);
	instruction_ref create_ret_void(void);

	expr_value create_struct_gep(expr_value value, size_t index);
	expr_value create_array_gep(expr_value value, expr_value index);
	expr_value create_array_slice_gep(expr_value begin_ptr, expr_value index, type const *elem_type);
	instruction_ref create_const_memcpy(expr_value dest, expr_value source, size_t size);
	instruction_ref create_const_memset_zero(expr_value dest, size_t size);

	expr_value create_function_call(function const *func, bz::fixed_vector<instruction_ref> args);

	expr_value create_int_cast(expr_value value, type const *dest, bool is_value_signed);
	expr_value create_float_cast(expr_value value, type const *dest);
	expr_value create_float_to_int_cast(expr_value value, type const *dest, bool is_dest_signed);
	expr_value create_int_to_float_cast(expr_value value, type const *dest, bool is_value_signed);

	expr_value create_int_cmp_eq(expr_value lhs, expr_value rhs);
	expr_value create_int_cmp_neq(expr_value lhs, expr_value rhs);
	expr_value create_int_cmp_lt(expr_value lhs, expr_value rhs, bool is_signed);
	expr_value create_int_cmp_gt(expr_value lhs, expr_value rhs, bool is_signed);
	expr_value create_int_cmp_lte(expr_value lhs, expr_value rhs, bool is_signed);
	expr_value create_int_cmp_gte(expr_value lhs, expr_value rhs, bool is_signed);
	expr_value create_float_cmp_eq(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs);
	expr_value create_float_cmp_neq(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs);
	expr_value create_float_cmp_lt(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs);
	expr_value create_float_cmp_gt(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs);
	expr_value create_float_cmp_lte(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs);
	expr_value create_float_cmp_gte(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs);
	expr_value create_float_cmp_eq_unchecked(expr_value lhs, expr_value rhs);
	expr_value create_float_cmp_neq_unchecked(expr_value lhs, expr_value rhs);
	expr_value create_float_cmp_lt_unchecked(expr_value lhs, expr_value rhs);
	expr_value create_float_cmp_gt_unchecked(expr_value lhs, expr_value rhs);
	expr_value create_float_cmp_lte_unchecked(expr_value lhs, expr_value rhs);
	expr_value create_float_cmp_gte_unchecked(expr_value lhs, expr_value rhs);
	expr_value create_cmp_eq_ptr(expr_value lhs, expr_value rhs);
	expr_value create_cmp_neq_ptr(expr_value lhs, expr_value rhs);

	expr_value create_neg(lex::src_tokens const &src_tokens, expr_value value);
	expr_value create_neg_unchecked(expr_value value);
	expr_value create_add_unchecked(expr_value lhs, expr_value rhs);
	expr_value create_sub_unchecked(expr_value lhs, expr_value rhs);

	expr_value create_ptrdiff(expr_value lhs, expr_value rhs, type const *elem_type);

	expr_value create_not(expr_value value);
	expr_value create_and(expr_value lhs, expr_value rhs);
	expr_value create_xor(expr_value lhs, expr_value rhs);
	expr_value create_or(expr_value lhs, expr_value rhs);

	expr_value create_abs(lex::src_tokens const &src_tokens, expr_value value);
	expr_value create_abs_unchecked(expr_value value);
	expr_value create_int_min(expr_value a, expr_value b, bool is_signed);
	expr_value create_float_min(lex::src_tokens const &src_tokens, expr_value x, expr_value y);
	expr_value create_float_min_unchecked(expr_value x, expr_value y);
	expr_value create_int_max(expr_value a, expr_value b, bool is_signed);
	expr_value create_float_max(lex::src_tokens const &src_tokens, expr_value x, expr_value y);
	expr_value create_float_max_unchecked(expr_value x, expr_value y);
	expr_value create_exp(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_exp_unchecked(expr_value x);
	expr_value create_exp2(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_exp2_unchecked(expr_value x);
	expr_value create_expm1(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_expm1_unchecked(expr_value x);
	expr_value create_log(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_log_unchecked(expr_value x);
	expr_value create_log10(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_log10_unchecked(expr_value x);
	expr_value create_log2(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_log2_unchecked(expr_value x);
	expr_value create_log1p(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_log1p_unchecked(expr_value x);
	expr_value create_sqrt(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_sqrt_unchecked(expr_value x);
	expr_value create_pow(lex::src_tokens const &src_tokens, expr_value x, expr_value y);
	expr_value create_pow_unchecked(expr_value x, expr_value y);
	expr_value create_cbrt(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_cbrt_unchecked(expr_value x);
	expr_value create_hypot(lex::src_tokens const &src_tokens, expr_value x, expr_value y);
	expr_value create_hypot_unchecked(expr_value x, expr_value y);
	expr_value create_sin(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_sin_unchecked(expr_value x);
	expr_value create_cos(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_cos_unchecked(expr_value x);
	expr_value create_tan(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_tan_unchecked(expr_value x);
	expr_value create_asin(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_asin_unchecked(expr_value x);
	expr_value create_acos(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_acos_unchecked(expr_value x);
	expr_value create_atan(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_atan_unchecked(expr_value x);
	expr_value create_atan2(lex::src_tokens const &src_tokens, expr_value y, expr_value x);
	expr_value create_atan2_unchecked(expr_value y, expr_value x);
	expr_value create_sinh(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_sinh_unchecked(expr_value x);
	expr_value create_cosh(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_cosh_unchecked(expr_value x);
	expr_value create_tanh(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_tanh_unchecked(expr_value x);
	expr_value create_asinh(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_asinh_unchecked(expr_value x);
	expr_value create_acosh(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_acosh_unchecked(expr_value x);
	expr_value create_atanh(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_atanh_unchecked(expr_value x);
	expr_value create_erf(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_erf_unchecked(expr_value x);
	expr_value create_erfc(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_erfc_unchecked(expr_value x);
	expr_value create_tgamma(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_tgamma_unchecked(expr_value x);
	expr_value create_lgamma(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_lgamma_unchecked(expr_value x);
	expr_value create_bitreverse(expr_value value);
	expr_value create_popcount(expr_value value);
	expr_value create_byteswap(expr_value value);
	expr_value create_clz(expr_value value);
	expr_value create_ctz(expr_value value);
	expr_value create_fshl(expr_value a, expr_value b, expr_value amount);
	expr_value create_fshr(expr_value a, expr_value b, expr_value amount);

	instruction_ref create_unreachable(void);
	instruction_ref create_error(lex::src_tokens const &src_tokens, bz::u8string message);
	instruction_ref create_error_str(lex::src_tokens const &src_tokens, expr_value begin_ptr, expr_value end_ptr);
	instruction_ref create_warning_str(lex::src_tokens const &src_tokens, ctx::warning_kind kind, expr_value begin_ptr, expr_value end_ptr);
	instruction_ref create_array_bounds_check(lex::src_tokens const &src_tokens, expr_value index, expr_value size, bool is_index_signed);
	instruction_ref create_optional_get_value_check(lex::src_tokens const &src_tokens, expr_value has_value);
	instruction_ref create_str_construction_check(lex::src_tokens const &src_tokens, expr_value begin_ptr, expr_value end_ptr);
	instruction_ref create_slice_construction_check(lex::src_tokens const &src_tokens, expr_value begin_ptr, expr_value end_ptr);

	void finalize_function(function &func);
};

} // namespace comptime

#endif // COMPTIME_CODEGEN_CONTEXT_H
