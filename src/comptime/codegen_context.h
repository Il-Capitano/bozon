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

	instruction_ref get_value(codegen_context &context) const;
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
};

struct unresolved_instruction
{
	instruction_ref inst;
	bz::array<instruction_ref, 2> args;
};

struct codegen_context
{
	function *current_function = nullptr;
	basic_block_ref current_bb = {};
	bz::optional<expr_value> function_return_address;
	bz::vector<unresolved_instruction> unresolved_instructions;

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

	struct loop_info_t
	{
		basic_block_ref break_bb;
		basic_block_ref continue_bb;
		size_t destructor_stack_begin;
	};

	loop_info_t loop_info = {};
	bz::array<expr_value, 4> current_value_references = {};
	size_t current_value_reference_stack_size = 0;

	global_codegen_context *global_codegen_ctx;

	bool is_little_endian(void) const;
	bool is_big_endian(void) const;
	bool is_64_bit(void) const;
	bool is_32_bit(void) const;

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
	instruction_ref add_instruction(Inst &&inst)
	{
		this->current_function->blocks[this->current_bb.bb_index].instructions.emplace_back(std::forward<Inst>(inst));
		auto const result = instruction_ref{
			.bb_index   = this->current_bb.bb_index,
			.inst_index = static_cast<uint32_t>(this->current_function->blocks[this->current_bb.bb_index].instructions.size() - 1),
		};
		return result;
	}

	template<typename Inst>
	instruction_ref add_instruction(Inst &&inst, instruction_ref arg)
	{
		this->current_function->blocks[this->current_bb.bb_index].instructions.emplace_back(std::forward<Inst>(inst));
		auto const result = instruction_ref{
			.bb_index   = this->current_bb.bb_index,
			.inst_index = static_cast<uint32_t>(this->current_function->blocks[this->current_bb.bb_index].instructions.size() - 1),
		};
		this->unresolved_instructions.push_back({
			.inst = result,
			.args = { arg, {} },
		});
		return result;
	}

	template<typename Inst>
	instruction_ref add_instruction(Inst &&inst, instruction_ref arg1, instruction_ref arg2)
	{
		this->current_function->blocks[this->current_bb.bb_index].instructions.emplace_back(std::forward<Inst>(inst));
		auto const result = instruction_ref{
			.bb_index   = this->current_bb.bb_index,
			.inst_index = static_cast<uint32_t>(this->current_function->blocks[this->current_bb.bb_index].instructions.size() - 1),
		};
		this->unresolved_instructions.push_back({
			.inst = result,
			.args = { arg1, arg2 },
		});
		return result;
	}

	expr_value create_const_ptr_null(void);
	expr_value create_load(expr_value ptr);
	instruction_ref create_store(expr_value value, expr_value ptr);
	expr_value create_alloca(type const *type);
	instruction_ref create_jump(basic_block_ref bb);
	instruction_ref create_conditional_jump(instruction_ref condition, basic_block_ref true_bb, basic_block_ref false_bb);
	instruction_ref create_ret(instruction_ref value);
	instruction_ref create_ret_void(void);
	expr_value create_struct_gep(expr_value value, size_t index);

	void finalize_function(void);
};

} // namespace comptime

#endif // COMPTIME_CODEGEN_CONTEXT_H
