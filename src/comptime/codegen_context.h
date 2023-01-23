#ifndef COMPTIME_CODEGEN_CONTEXT_H
#define COMPTIME_CODEGEN_CONTEXT_H

#include "instructions.h"
#include "types.h"
#include "memory.h"
#include "ast/expression.h"
#include "ctx/parse_context.h"

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

struct unresolved_switch
{
	struct value_bb_pair
	{
		uint64_t value;
		basic_block_ref bb;
	};

	instruction_ref inst;
	bz::vector<value_bb_pair> values;
	basic_block_ref default_bb;
};

struct destruct_operation_info_t
{
	ast::destruct_operation const *destruct_op;
	expr_value value;
	bz::optional<instruction_ref> condition;
	bz::optional<instruction_ref> move_destruct_indicator;
	bz::optional<instruction_ref> rvalue_array_elem_ptr;
};

struct current_function_info_t
{
	function *func = nullptr;

	bz::optional<expr_value> return_address;
	bz::vector<alloca> allocas;
	bz::vector<basic_block> blocks;
	bz::vector<error_info_t> errors;
	bz::vector<lex::src_tokens> src_tokens;
	bz::vector<bz::fixed_vector<instruction_ref>> call_args;
	bz::vector<slice_construction_check_info_t> slice_construction_check_infos;
	bz::vector<pointer_arithmetic_check_info_t> pointer_arithmetic_check_infos;
	bz::vector<memory_access_check_info_t> memory_access_check_infos;
	bz::vector<unresolved_instruction> unresolved_instructions;
	bz::vector<unresolved_jump> unresolved_jumps;
	bz::vector<unresolved_switch> unresolved_switches;

	bz::vector<instruction_ref> lifetimes{};
	bz::vector<destruct_operation_info_t> destructor_calls{};
	std::unordered_map<ast::decl_variable const *, instruction_ref> move_destruct_indicators{};
	std::unordered_map<ast::decl_variable const *, expr_value> variables{};

	void finalize_function(void);
};

struct machine_parameters_t
{
	size_t pointer_size;
	memory::endianness_kind endianness;
};

struct codegen_context
{
	basic_block_ref current_bb = {};
	current_function_info_t current_function_info{};

	machine_parameters_t machine_parameters;
	memory::global_memory_manager global_memory;
	type_set_t type_set;
	type const *pointer_pair_t = nullptr;
	type const *null_t = nullptr;

	std::unordered_map<ast::decl_variable const *, uint32_t> global_variables{};
	std::unordered_map<ast::function_body *, std::unique_ptr<function>> functions{};

	struct loop_info_t
	{
		basic_block_ref break_bb = {};
		basic_block_ref continue_bb = {};
		size_t destructor_stack_begin = 0;
		size_t lifetimes_stack_begin = 0;
		bool in_loop = false;
	};

	loop_info_t loop_info = {};
	bz::array<expr_value, 4> current_value_references = {};
	size_t current_value_reference_stack_size = 0;

	ctx::parse_context *parse_ctx = nullptr;

	codegen_context(machine_parameters_t _machine_parameters);

	void resolve_function(lex::src_tokens const &src_tokens, ast::function_body &body);

	memory::endianness_kind get_endianness(void) const;
	bool is_little_endian(void) const;
	bool is_big_endian(void) const;
	bool is_64_bit(void) const;
	bool is_32_bit(void) const;
	memory::memory_segment_info_t get_memory_segment_info(void) const;

	void add_variable(ast::decl_variable const *decl, expr_value value);
	void add_global_variable(ast::decl_variable const *decl, uint32_t global_index);
	expr_value get_variable(ast::decl_variable const *decl);
	bz::optional<uint32_t> get_global_variable(ast::decl_variable const *decl);
	function *get_non_const_function(ast::function_body *body);
	function const *get_function(ast::function_body *body);

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
	bool has_terminator(void);

	struct expression_scope_info_t
	{
		size_t destructor_calls_size;
		size_t lifetimes_size;
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
	void emit_destruct_operations(size_t destruct_calls_start_index, size_t lifetimes_start_index);
	void emit_loop_destruct_operations(void);
	void emit_all_destruct_operations(void);

	// instruction creation functions
	template<typename Inst>
	instruction_ref add_instruction(Inst inst)
	{
		if (this->has_terminator())
		{
			this->set_current_basic_block(this->add_basic_block());
		}
		static_assert(instructions::arg_count<Inst> == 0);
		instruction new_inst = instruction();
		new_inst.emplace<Inst>(inst);
		this->current_function_info.blocks[this->current_bb.bb_index].instructions.push_back(std::move(new_inst));
		auto const result = instruction_ref{
			.bb_index   = this->current_bb.bb_index,
			.inst_index = static_cast<uint32_t>(this->current_function_info.blocks[this->current_bb.bb_index].instructions.size() - 1),
		};
		return result;
	}

	template<typename Inst>
	instruction_ref add_instruction(Inst inst, instruction_ref arg)
	{
		if (this->has_terminator())
		{
			this->set_current_basic_block(this->add_basic_block());
		}
		static_assert(instructions::arg_count<Inst> == 1);
		instruction new_inst = instruction();
		new_inst.emplace<Inst>(inst);
		this->current_function_info.blocks[this->current_bb.bb_index].instructions.push_back(std::move(new_inst));
		auto const result = instruction_ref{
			.bb_index   = this->current_bb.bb_index,
			.inst_index = static_cast<uint32_t>(this->current_function_info.blocks[this->current_bb.bb_index].instructions.size() - 1),
		};
		this->current_function_info.unresolved_instructions.push_back({
			.inst = result,
			.args = { arg, {}, {} },
		});
		return result;
	}

	template<typename Inst>
	instruction_ref add_instruction(Inst inst, instruction_ref arg1, instruction_ref arg2)
	{
		if (this->has_terminator())
		{
			this->set_current_basic_block(this->add_basic_block());
		}
		static_assert(instructions::arg_count<Inst> == 2);
		instruction new_inst = instruction();
		new_inst.emplace<Inst>(inst);
		this->current_function_info.blocks[this->current_bb.bb_index].instructions.push_back(std::move(new_inst));
		auto const result = instruction_ref{
			.bb_index   = this->current_bb.bb_index,
			.inst_index = static_cast<uint32_t>(this->current_function_info.blocks[this->current_bb.bb_index].instructions.size() - 1),
		};
		this->current_function_info.unresolved_instructions.push_back({
			.inst = result,
			.args = { arg1, arg2, {} },
		});
		return result;
	}

	template<typename Inst>
	instruction_ref add_instruction(Inst inst, instruction_ref arg1, instruction_ref arg2, instruction_ref arg3)
	{
		if (this->has_terminator())
		{
			this->set_current_basic_block(this->add_basic_block());
		}
		static_assert(instructions::arg_count<Inst> == 3);
		instruction new_inst = instruction();
		new_inst.emplace<Inst>(inst);
		this->current_function_info.blocks[this->current_bb.bb_index].instructions.push_back(std::move(new_inst));
		auto const result = instruction_ref{
			.bb_index   = this->current_bb.bb_index,
			.inst_index = static_cast<uint32_t>(this->current_function_info.blocks[this->current_bb.bb_index].instructions.size() - 1),
		};
		this->current_function_info.unresolved_instructions.push_back({
			.inst = result,
			.args = { arg1, arg2, arg3 },
		});
		return result;
	}

	uint32_t add_src_tokens(lex::src_tokens const &src_tokens);
	uint32_t add_slice_construction_check_info(slice_construction_check_info_t info);
	uint32_t add_pointer_arithmetic_check_info(pointer_arithmetic_check_info_t info);
	uint32_t add_memory_access_check_info(memory_access_check_info_t info);

	expr_value get_dummy_value(type const *t);

	expr_value create_const_int(type const *int_type, int64_t value);
	expr_value create_const_int(type const *int_type, uint64_t value);
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
	void create_string(bz::u8string_view str, expr_value result_address);
	expr_value create_string(bz::u8string_view str);
	struct global_object_result
	{
		expr_value value;
		uint32_t index;
	};
	global_object_result create_global_object(type const *object_type, bz::fixed_vector<uint8_t> data);
	expr_value create_add_global_array_data(type const *elem_type, expr_value begin_ptr, expr_value end_ptr);

	expr_value create_get_global_object(uint32_t global_index);
	expr_value create_get_function_return_address(void);
	instruction_ref create_get_function_arg(uint32_t arg_index);

	expr_value create_load(expr_value ptr);
	instruction_ref create_store(expr_value value, expr_value ptr);
	void create_memory_access_check(
		lex::src_tokens const &src_tokens,
		expr_value ptr,
		type const *object_type,
		ast::typespec_view object_typespec
	);

	expr_value create_alloca(type const *type);
	expr_value create_alloca_without_lifetime(type const *type);

	instruction_ref create_jump(basic_block_ref bb);
	instruction_ref create_conditional_jump(expr_value condition, basic_block_ref true_bb, basic_block_ref false_bb);
	instruction_ref create_switch(expr_value value, bz::vector<unresolved_switch::value_bb_pair> values, basic_block_ref default_bb);
	instruction_ref create_ret(instruction_ref value);
	instruction_ref create_ret_void(void);

	expr_value create_struct_gep(expr_value value, size_t index);
	expr_value create_array_gep(expr_value value, expr_value index);
	expr_value create_array_slice_gep(expr_value begin_ptr, expr_value index, type const *elem_type);
	instruction_ref create_const_memcpy(expr_value dest, expr_value source, size_t size);
	instruction_ref create_const_memset_zero(expr_value dest, size_t size);

	expr_value create_function_call(lex::src_tokens const &src_tokens, function const *func, bz::fixed_vector<instruction_ref> args);
	expr_value create_malloc(lex::src_tokens const &src_tokens, type const *type, expr_value count);
	void create_free(lex::src_tokens const &src_tokens, expr_value ptr);

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
	void create_float_cmp_eq_check(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs);
	void create_float_cmp_neq_check(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs);
	void create_float_cmp_lt_check(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs);
	void create_float_cmp_gt_check(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs);
	void create_float_cmp_lte_check(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs);
	void create_float_cmp_gte_check(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs);
	expr_value create_float_cmp_eq(expr_value lhs, expr_value rhs);
	expr_value create_float_cmp_neq(expr_value lhs, expr_value rhs);
	expr_value create_float_cmp_lt(expr_value lhs, expr_value rhs);
	expr_value create_float_cmp_gt(expr_value lhs, expr_value rhs);
	expr_value create_float_cmp_lte(expr_value lhs, expr_value rhs);
	expr_value create_float_cmp_gte(expr_value lhs, expr_value rhs);
	expr_value create_pointer_cmp_eq(expr_value lhs, expr_value rhs);
	expr_value create_pointer_cmp_neq(expr_value lhs, expr_value rhs);
	expr_value create_pointer_cmp_lt(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs);
	expr_value create_pointer_cmp_gt(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs);
	expr_value create_pointer_cmp_lte(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs);
	expr_value create_pointer_cmp_gte(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs);

	expr_value create_neg(expr_value value);
	void create_neg_check(lex::src_tokens const &src_tokens, expr_value value);
	expr_value create_add(expr_value lhs, expr_value rhs);
	void create_add_check(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs, bool is_signed_int);
	expr_value create_ptr_add_const_unchecked(expr_value address, int32_t amount, type const *object_type);
	expr_value create_ptr_add(
		lex::src_tokens const &src_tokens,
		expr_value address,
		expr_value offset,
		bool is_offset_signed,
		type const *object_type,
		ast::typespec_view pointer_type
	);
	expr_value create_sub(expr_value lhs, expr_value rhs);
	void create_sub_check(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs, bool is_signed_int);
	expr_value create_ptr_sub(
		lex::src_tokens const &src_tokens,
		expr_value address,
		expr_value offset,
		bool is_offset_signed,
		type const *object_type,
		ast::typespec_view pointer_type
	);
	expr_value create_ptrdiff(
		lex::src_tokens const &src_tokens,
		expr_value lhs,
		expr_value rhs,
		type const *object_type,
		ast::typespec_view pointer_type
	);
	expr_value create_ptrdiff_unchecked(expr_value lhs, expr_value rhs, type const *object_type);
	expr_value create_mul(expr_value lhs, expr_value rhs);
	void create_mul_check(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs, bool is_signed_int);
	expr_value create_div(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs, bool is_signed_int);
	void create_div_check(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs, bool is_signed_int);
	expr_value create_rem(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs, bool is_signed_int);

	expr_value create_not(expr_value value);
	expr_value create_and(expr_value lhs, expr_value rhs);
	expr_value create_xor(expr_value lhs, expr_value rhs);
	expr_value create_or(expr_value lhs, expr_value rhs);
	expr_value create_shl(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs, bool is_rhs_signed);
	expr_value create_shr(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs, bool is_rhs_signed);

	expr_value create_abs(expr_value value);
	void create_abs_check(lex::src_tokens const &src_tokens, expr_value value);
	expr_value create_min(expr_value a, expr_value b, bool is_signed_int);
	void create_min_check(lex::src_tokens const &src_tokens, expr_value x, expr_value y);
	expr_value create_max(expr_value a, expr_value b, bool is_signed_int);
	void create_max_check(lex::src_tokens const &src_tokens, expr_value x, expr_value y);
	expr_value create_exp(expr_value x);
	void create_exp_check(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_exp2(expr_value x);
	void create_exp2_check(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_expm1(expr_value x);
	void create_expm1_check(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_log(expr_value x);
	void create_log_check(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_log10(expr_value x);
	void create_log10_check(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_log2(expr_value x);
	void create_log2_check(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_log1p(expr_value x);
	void create_log1p_check(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_sqrt(expr_value x);
	void create_sqrt_check(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_pow(expr_value x, expr_value y);
	void create_pow_check(lex::src_tokens const &src_tokens, expr_value x, expr_value y);
	expr_value create_cbrt(expr_value x);
	void create_cbrt_check(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_hypot(expr_value x, expr_value y);
	void create_hypot_check(lex::src_tokens const &src_tokens, expr_value x, expr_value y);
	expr_value create_sin(expr_value x);
	void create_sin_check(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_cos(expr_value x);
	void create_cos_check(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_tan(expr_value x);
	void create_tan_check(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_asin(expr_value x);
	void create_asin_check(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_acos(expr_value x);
	void create_acos_check(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_atan(expr_value x);
	void create_atan_check(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_atan2(expr_value y, expr_value x);
	void create_atan2_check(lex::src_tokens const &src_tokens, expr_value y, expr_value x);
	expr_value create_sinh(expr_value x);
	void create_sinh_check(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_cosh(expr_value x);
	void create_cosh_check(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_tanh(expr_value x);
	void create_tanh_check(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_asinh(expr_value x);
	void create_asinh_check(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_acosh(expr_value x);
	void create_acosh_check(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_atanh(expr_value x);
	void create_atanh_check(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_erf(expr_value x);
	void create_erf_check(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_erfc(expr_value x);
	void create_erfc_check(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_tgamma(expr_value x);
	void create_tgamma_check(lex::src_tokens const &src_tokens, expr_value x);
	expr_value create_lgamma(expr_value x);
	void create_lgamma_check(lex::src_tokens const &src_tokens, expr_value x);
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
	expr_value create_is_option_set(expr_value begin_ptr, expr_value end_ptr);
	instruction_ref create_array_bounds_check(lex::src_tokens const &src_tokens, expr_value index, expr_value size, bool is_index_signed);
	instruction_ref create_optional_get_value_check(lex::src_tokens const &src_tokens, expr_value has_value);
	instruction_ref create_str_construction_check(lex::src_tokens const &src_tokens, expr_value begin_ptr, expr_value end_ptr);
	instruction_ref create_slice_construction_check(
		lex::src_tokens const &src_tokens,
		expr_value begin_ptr,
		expr_value end_ptr,
		type const *elem_type,
		ast::typespec_view slice_type
	);
	void create_start_lifetime(instruction_ref ptr);
	void create_end_lifetime(instruction_ref ptr);
};

} // namespace comptime

#endif // COMPTIME_CODEGEN_CONTEXT_H
