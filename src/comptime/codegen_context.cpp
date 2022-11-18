#include "codegen_context.h"
#include "ast/statement.h"

namespace comptime
{

type const *codegen_context::get_builtin_type(builtin_type_kind kind)
{
	return this->global_codegen_ctx->get_builtin_type(kind);
}

type const *codegen_context::get_pointer_type(void)
{
	return this->global_codegen_ctx->get_pointer_type();
}

type const *codegen_context::get_aggregate_type(bz::array_view<type const * const> elem_types)
{
	return this->global_codegen_ctx->get_aggregate_type(elem_types);
}

type const *codegen_context::get_array_type(type const *elem_type, size_t size)
{
	return this->global_codegen_ctx->get_array_type(elem_type, size);
}

type const *codegen_context::get_str_t(void)
{
	return this->global_codegen_ctx->get_str_t();
}

type const *codegen_context::get_null_t(void)
{
	return this->global_codegen_ctx->get_null_t();
}

type const *codegen_context::get_slice_t(void)
{
	return this->global_codegen_ctx->get_slice_t();
}

type const *codegen_context::get_optional_type(type const *value_type)
{
	return this->global_codegen_ctx->get_optional_type(value_type);
}

basic_block_ref codegen_context::get_current_basic_block(void)
{
	return this->current_bb;
}

basic_block_ref codegen_context::add_basic_block(void)
{
	this->current_function->blocks.emplace_back();
	return basic_block_ref{ static_cast<uint32_t>(this->current_function->blocks.size() - 1) };
}

void codegen_context::set_current_basic_block(basic_block_ref bb)
{
	this->current_bb = bb;
}

[[nodiscard]] codegen_context::expression_scope_info_t codegen_context::push_expression_scope(void)
{
	return { this->destructor_calls.size() };
}

void codegen_context::pop_expression_scope(expression_scope_info_t prev_info)
{
	this->emit_destruct_operations(prev_info.destructor_calls_size);
	this->destructor_calls.resize(prev_info.destructor_calls_size);
}

[[nodiscard]] codegen_context::loop_info_t codegen_context::push_loop(basic_block_ref break_bb, basic_block_ref continue_bb)
{
	auto const result = this->loop_info;
	this->loop_info.destructor_stack_begin = this->destructor_calls.size();
	this->loop_info.break_bb = break_bb;
	this->loop_info.continue_bb = continue_bb;
	return result;
}

void codegen_context::pop_loop(loop_info_t prev_info)
{
	this->loop_info = prev_info;
}

[[nodiscard]] expr_value codegen_context::push_value_reference(expr_value new_value)
{
	auto const index = this->current_value_reference_stack_size % this->current_value_references.size();
	this->current_value_reference_stack_size += 1;
	auto const result = this->current_value_references[index];
	this->current_value_references[index] = new_value;
	return result;
}

void codegen_context::pop_value_reference(expr_value prev_value)
{
	bz_assert(this->current_value_reference_stack_size > 0);
	this->current_value_reference_stack_size -= 1;
	auto const index = this->current_value_reference_stack_size % this->current_value_references.size();
	this->current_value_references[index] = prev_value;
}

expr_value codegen_context::get_value_reference(size_t index)
{
	bz_assert(index < this->current_value_reference_stack_size);
	bz_assert(index < this->current_value_references.size());
	auto const stack_index = (this->current_value_reference_stack_size - index - 1) % this->current_value_references.size();
	return this->current_value_references[stack_index];
}

// instruction_ref codegen_context::add_move_destruct_indicator(ast::decl_variable const *decl)
// {
// // make sure the returned value is not { 0, 0 } !!
// }

bz::optional<instruction_ref> codegen_context::get_move_destruct_indicator(ast::decl_variable const *decl) const
{
	if (decl == nullptr)
	{
		return {};
	}

	auto const it = this->move_destruct_indicators.find(decl);
	if (it == this->move_destruct_indicators.end())
	{
		return {};
	}
	else
	{
		return it->second;
	}
}

void codegen_context::push_destruct_operation(ast::destruct_operation const &destruct_op)
{
	auto const move_destruct_indicator = this->get_move_destruct_indicator(destruct_op.move_destructed_decl);
	if (move_destruct_indicator.has_value() || destruct_op.not_null())
	{
		this->destructor_calls.push_back({
			.destruct_op = &destruct_op,
			.value = expr_value::get_none(),
			.condition = {},
			.move_destruct_indicator = move_destruct_indicator,
			.rvalue_array_elem_ptr = {},
		});
	}
}

void codegen_context::push_variable_destruct_operation(
	ast::destruct_operation const &destruct_op,
	bz::optional<instruction_ref> move_destruct_indicator
)
{
	if (destruct_op.not_null())
	{
		this->destructor_calls.push_back({
			.destruct_op = &destruct_op,
			.value = expr_value::get_none(),
			.condition = move_destruct_indicator,
			.move_destruct_indicator = {},
			.rvalue_array_elem_ptr = {},
		});
	}
}

void codegen_context::push_self_destruct_operation(
	ast::destruct_operation const &destruct_op,
	expr_value value
)
{
	auto const move_destruct_indicator = this->get_move_destruct_indicator(destruct_op.move_destructed_decl);
	if (move_destruct_indicator.has_value() || destruct_op.not_null())
	{
		this->destructor_calls.push_back({
			.destruct_op = &destruct_op,
			.value = value,
			.condition = {},
			.move_destruct_indicator = move_destruct_indicator,
			.rvalue_array_elem_ptr = {},
		});
	}
}

void codegen_context::push_rvalue_array_destruct_operation(
	ast::destruct_operation const &destruct_op,
	expr_value value,
	instruction_ref rvalue_array_elem_ptr
)
{
	auto const move_destruct_indicator = this->get_move_destruct_indicator(destruct_op.move_destructed_decl);
	if (move_destruct_indicator.has_value() || destruct_op.not_null())
	{
		this->destructor_calls.push_back({
			.destruct_op = &destruct_op,
			.value = value,
			.condition = {},
			.move_destruct_indicator = move_destruct_indicator,
			.rvalue_array_elem_ptr = rvalue_array_elem_ptr,
		});
	}
}

static void emit_destruct_operation(codegen_context::destruct_operation_info_t const &info, codegen_context &context);

void codegen_context::emit_destruct_operations(size_t start_index)
{
	for (auto const &info : this->destructor_calls.slice(start_index).reversed())
	{
		emit_destruct_operation(info, *this);
	}
}

void codegen_context::emit_loop_destruct_operations(void)
{
	for (auto const &info : this->destructor_calls.slice(this->loop_info.destructor_stack_begin).reversed())
	{
		emit_destruct_operation(info, *this);
	}
}

void codegen_context::emit_all_destruct_operations(void)
{
	for (auto const &info : this->destructor_calls.reversed())
	{
		emit_destruct_operation(info, *this);
	}
}


expr_value codegen_context::create_alloca(type const *type)
{
	bz_assert(this->current_function->blocks.not_empty());
	this->current_function->blocks[0].instructions.emplace_back(instructions::alloca{ .size = type->size, .align = type->align });
	auto const alloca_ref = instruction_ref{
		.bb_index = 0,
		.inst_index = static_cast<uint32_t>(this->current_function->blocks[0].instructions.size() - 1),
	};
	return expr_value::get_reference(alloca_ref, type);
}

instruction_ref codegen_context::create_jump(basic_block_ref bb)
{
	return this->add_instruction(instructions::jump{ .next_bb_index = bb.bb_index });
}

instruction_ref codegen_context::create_conditional_jump(
	instruction_ref condition,
	basic_block_ref true_bb,
	basic_block_ref false_bb
)
{
	return this->add_instruction(instructions::conditional_jump{
		.args = {},
		.true_bb_index = true_bb.bb_index,
		.false_bb_index = false_bb.bb_index,
	}, condition);
}

instruction_ref codegen_context::create_ret(instruction_ref value)
{
	return this->add_instruction(instructions::ret{ .args= {} }, value);
}

instruction_ref codegen_context::create_ret_void(void)
{
	return this->add_instruction(instructions::ret_void{});
}

expr_value codegen_context::create_struct_gep(expr_value value, size_t index)
{
	bz_assert(value.is_reference());
	auto const type = value.get_type();
	if (type->is_array())
	{
		bz_assert(index < type->get_array_size());
		auto const offset = index * type->get_array_element_type()->size;
		auto const result_ptr = this->add_instruction(instructions::const_gep{
			.args = {},
			.offset = offset
		}, value.get_reference());
		return expr_value::get_reference(result_ptr, type->get_array_element_type());
	}
	else
	{
		bz_assert(type->is_aggregate());
		auto const types = type->get_aggregate_types();
		auto const offsets = type->get_aggregate_offsets();
		bz_assert(index < types.size());
		auto const result_ptr = this->add_instruction(instructions::const_gep{
			.args = {},
			.offset = offsets[index]
		}, value.get_reference());
		return expr_value::get_reference(result_ptr, types[index]);
	}
}

} // namespace comptime