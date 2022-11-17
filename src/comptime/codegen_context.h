#ifndef COMPTIME_CODEGEN_CONTEXT_H
#define COMPTIME_CODEGEN_CONTEXT_H

#include "instructions.h"
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
	instruction_ref inst;
	expr_value_kind kind;

	instruction_ref get_value(codegen_context &context);

	static expr_value get_none(void)
	{
		return { .inst = {}, .kind = expr_value_kind::none };
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
	instruction_ref create_jump(basic_block_ref bb);
	instruction_ref create_conditional_jump(instruction_ref condition, basic_block_ref true_bb, basic_block_ref false_bb);

	void finalize_function(void);
};

} // namespace comptime

#endif // COMPTIME_CODEGEN_CONTEXT_H
