#include "codegen_context.h"
#include "codegen.h"
#include "ast/statement.h"
#include "global_data.h"
#include "resolve/statement_resolver.h"

namespace comptime
{

static_assert(sizeof (memory::global_segment_info_t) == 32);

static constexpr memory::global_segment_info_t segment_info_64_bit = {
	0x0000'0000'0001'0000,
	0x4000'0000'0000'0000,
	0x8000'0000'0000'0000,
	0xff00'0000'0000'0000,
};

static constexpr memory::global_segment_info_t segment_info_32_bit = {
	0x0001'0000,
	0x4000'0000,
	0x8000'0000,
	0xff00'0000,
};

template<typename Inst>
instruction_ref add_instruction(codegen_context &context, Inst inst)
{
	if (context.has_terminator())
	{
		context.set_current_basic_block(context.add_basic_block());
	}
	static_assert(instructions::arg_count<Inst> == 0);
	instruction new_inst = instruction();
	new_inst.emplace<Inst>(inst);
	context.current_function_info.blocks[context.current_function_info.current_bb.bb_index].instructions.push_back(std::move(new_inst));
	auto const result = instruction_ref{
		.bb_index   = context.current_function_info.current_bb.bb_index,
		.inst_index = static_cast<uint32_t>(context.current_function_info.blocks[context.current_function_info.current_bb.bb_index].instructions.size() - 1),
	};
	return result;
}

template<typename Inst>
instruction_ref add_instruction(codegen_context &context, Inst inst, instruction_ref arg)
{
	if (context.has_terminator())
	{
		context.set_current_basic_block(context.add_basic_block());
	}
	static_assert(instructions::arg_count<Inst> == 1);
	instruction new_inst = instruction();
	new_inst.emplace<Inst>(inst);
	context.current_function_info.blocks[context.get_current_basic_block().bb_index].instructions.push_back(std::move(new_inst));
	auto const result = instruction_ref{
		.bb_index   = context.get_current_basic_block().bb_index,
		.inst_index = static_cast<uint32_t>(context.current_function_info.blocks[context.get_current_basic_block().bb_index].instructions.size() - 1),
	};
	context.current_function_info.unresolved_instructions.push_back({
		.inst = result,
		.args = { arg, {}, {} },
	});
	return result;
}

template<typename Inst>
instruction_ref add_instruction(codegen_context &context, Inst inst, instruction_ref arg1, instruction_ref arg2)
{
	if (context.has_terminator())
	{
		context.set_current_basic_block(context.add_basic_block());
	}
	static_assert(instructions::arg_count<Inst> == 2);
	instruction new_inst = instruction();
	new_inst.emplace<Inst>(inst);
	context.current_function_info.blocks[context.get_current_basic_block().bb_index].instructions.push_back(std::move(new_inst));
	auto const result = instruction_ref{
		.bb_index   = context.get_current_basic_block().bb_index,
		.inst_index = static_cast<uint32_t>(context.current_function_info.blocks[context.get_current_basic_block().bb_index].instructions.size() - 1),
	};
	context.current_function_info.unresolved_instructions.push_back({
		.inst = result,
		.args = { arg1, arg2, {} },
	});
	return result;
}

template<typename Inst>
instruction_ref add_instruction(codegen_context &context, Inst inst, instruction_ref arg1, instruction_ref arg2, instruction_ref arg3)
{
	if (context.has_terminator())
	{
		context.set_current_basic_block(context.add_basic_block());
	}
	static_assert(instructions::arg_count<Inst> == 3);
	instruction new_inst = instruction();
	new_inst.emplace<Inst>(inst);
	context.current_function_info.blocks[context.get_current_basic_block().bb_index].instructions.push_back(std::move(new_inst));
	auto const result = instruction_ref{
		.bb_index   = context.get_current_basic_block().bb_index,
		.inst_index = static_cast<uint32_t>(context.current_function_info.blocks[context.get_current_basic_block().bb_index].instructions.size() - 1),
	};
	context.current_function_info.unresolved_instructions.push_back({
		.inst = result,
		.args = { arg1, arg2, arg3 },
	});
	return result;
}

bool expr_value::is_value(void) const
{
	return this->kind == expr_value_kind::value;
}

bool expr_value::is_reference(void) const
{
	return this->kind == expr_value_kind::reference;
}

bool expr_value::is_none(void) const
{
	return this->kind == expr_value_kind::none;
}

expr_value expr_value::get_value(codegen_context &context) const
{
	if (this->is_value())
	{
		return *this;
	}
	else
	{
		return context.create_load(*this);
	}
}

instruction_ref expr_value::get_value_as_instruction(codegen_context &context) const
{
	if (this->is_value())
	{
		return this->value;
	}
	else
	{
		return context.create_load(*this).value;
	}
}

instruction_ref expr_value::get_reference(void) const
{
	bz_assert(this->is_reference());
	return this->value;
}

type const *expr_value::get_type(void) const
{
	bz_assert(this->value_type != nullptr);
	return this->value_type;
}

codegen_context::codegen_context(machine_parameters_t _machine_parameters)
	: machine_parameters(_machine_parameters),
	  global_memory(
		  _machine_parameters.pointer_size == 8
		  ? segment_info_64_bit.get_segment_begin<memory::memory_segment::global>()
		  : segment_info_32_bit.get_segment_begin<memory::memory_segment::global>()
	  ),
	  type_set(_machine_parameters.pointer_size)
{
	auto const pointer_type = this->get_pointer_type();
	type const *const pointer_pair_types[] = { pointer_type, pointer_type };
	this->pointer_pair_t = this->get_aggregate_type(pointer_pair_types);
	this->null_t = this->get_aggregate_type({});
}

void codegen_context::resolve_function(lex::src_tokens const &src_tokens, ast::function_body &body)
{
	if (body.state == ast::resolve_state::all || body.state == ast::resolve_state::error)
	{
		return;
	}

	bz_assert(this->parse_ctx != nullptr);
	auto &context = *this->parse_ctx;

	context.add_to_resolve_queue(src_tokens, body);
	resolve::resolve_function({}, body, context);
	context.pop_resolve_queue();
}

memory::endianness_kind codegen_context::get_endianness(void) const
{
	return this->machine_parameters.endianness;
}

bool codegen_context::is_little_endian(void) const
{
	return this->machine_parameters.endianness == memory::endianness_kind::little;
}

bool codegen_context::is_big_endian(void) const
{
	return this->machine_parameters.endianness == memory::endianness_kind::big;
}

bool codegen_context::is_64_bit(void) const
{
	return this->machine_parameters.pointer_size == 8;
}

bool codegen_context::is_32_bit(void) const
{
	return this->machine_parameters.pointer_size == 4;
}

memory::global_segment_info_t codegen_context::get_global_segment_info(void) const
{
	if (this->is_64_bit())
	{
		return segment_info_64_bit;
	}
	else
	{
		return segment_info_32_bit;
	}
}

void codegen_context::initialize_function(function *func)
{
	bz_assert(this->current_function_info.func == nullptr);
	this->current_function_info.func = func;

	this->current_function_info.global_variables_bb = this->add_basic_block();

	this->current_function_info.entry_bb = this->add_basic_block();
	this->set_current_basic_block(this->current_function_info.entry_bb);

	auto const needs_return_address = !func->return_type->is_simple_value_type();
	bz_assert(!this->current_function_info.return_address.has_value());
	if (needs_return_address)
	{
		this->current_function_info.return_address = this->create_get_function_return_address();
	}
}

void codegen_context::finalize_function(void)
{
	this->set_current_basic_block(this->current_function_info.global_variables_bb);
	this->create_jump(this->current_function_info.entry_bb);
	this->current_function_info.finalize_function();
}

void codegen_context::add_variable(ast::decl_variable const *decl, expr_value value)
{
	bz_assert(!this->current_function_info.variables.contains(decl));
	this->current_function_info.variables.insert({ decl, value });
}

void codegen_context::add_global_variable(ast::decl_variable const *decl, uint32_t global_index)
{
	bz_assert(!this->global_variables.contains(decl));
	this->global_variables.insert({ decl, global_index });
}

expr_value codegen_context::get_variable(ast::decl_variable const *decl)
{
	auto const it = this->current_function_info.variables.find(decl);
	if (it == this->current_function_info.variables.end())
	{
		if (decl->is_global_storage() && decl->get_type().is<ast::ts_consteval>())
		{
			generate_consteval_variable(*decl, *this);
			auto const new_it = this->current_function_info.variables.find(decl);
			return new_it == this->current_function_info.variables.end() ? expr_value::get_none() : new_it->second;
		}
		else
		{
			return expr_value::get_none();
		}
	}
	else
	{
		return it->second;
	}
}

bz::optional<uint32_t> codegen_context::get_global_variable(ast::decl_variable const *decl)
{
	auto const it = this->global_variables.find(decl);
	if (it == this->global_variables.end())
	{
		return {};
	}
	else
	{
		return it->second;
	}
}

function *codegen_context::get_non_const_function(ast::function_body *body)
{
	auto const it = this->functions.find(body);
	if (it == this->functions.end())
	{
		bz_assert(body->state != ast::resolve_state::error);
		bz_assert(body->state >= ast::resolve_state::symbol);
		auto const [it, inserted] = this->functions.insert({ body, generate_from_symbol(*body, *this) });
		return it->second.get();
	}
	else
	{
		return it->second.get();
	}
}

function const *codegen_context::get_function(ast::function_body *body)
{
	return this->get_non_const_function(body);
}

type const *codegen_context::get_builtin_type(builtin_type_kind kind)
{
	return this->type_set.get_builtin_type(kind);
}

type const *codegen_context::get_pointer_type(void)
{
	return this->type_set.get_pointer_type();
}

type const *codegen_context::get_aggregate_type(bz::array_view<type const * const> elem_types)
{
	return this->type_set.get_aggregate_type(elem_types);
}

type const *codegen_context::get_array_type(type const *elem_type, size_t size)
{
	return this->type_set.get_array_type(elem_type, size);
}

type const *codegen_context::get_str_t(void)
{
	return this->pointer_pair_t;
}

type const *codegen_context::get_null_t(void)
{
	return this->null_t;
}

type const *codegen_context::get_slice_t(void)
{
	return this->pointer_pair_t;
}

type const *codegen_context::get_optional_type(type const *value_type)
{
	type const * const types[] = { value_type, this->get_builtin_type(builtin_type_kind::i1) };
	return this->get_aggregate_type(types);
}

basic_block_ref codegen_context::get_current_basic_block(void)
{
	return this->current_function_info.current_bb;
}

basic_block_ref codegen_context::add_basic_block(void)
{
	this->current_function_info.blocks.emplace_back();
	return basic_block_ref{ static_cast<uint32_t>(this->current_function_info.blocks.size() - 1) };
}

void codegen_context::set_current_basic_block(basic_block_ref bb)
{
	this->current_function_info.current_bb = bb;
}

bool codegen_context::has_terminator(void)
{
	auto const &bb = this->current_function_info.blocks[this->get_current_basic_block().bb_index];
	return bb.instructions.not_empty() && bb.instructions.back().is_terminator();
}

[[nodiscard]] codegen_context::expression_scope_info_t codegen_context::push_expression_scope(void)
{
	return {
		.destructor_calls_size = this->current_function_info.destructor_calls.size(),
	};
}

void codegen_context::pop_expression_scope(expression_scope_info_t prev_info)
{
	this->emit_destruct_operations(prev_info.destructor_calls_size);
	this->current_function_info.destructor_calls.resize(prev_info.destructor_calls_size);
}

[[nodiscard]] codegen_context::loop_info_t codegen_context::push_loop(basic_block_ref break_bb, basic_block_ref continue_bb)
{
	auto const result = this->loop_info;
	this->loop_info.destructor_stack_begin = this->current_function_info.destructor_calls.size();
	this->loop_info.break_bb = break_bb;
	this->loop_info.continue_bb = continue_bb;
	this->loop_info.in_loop = true;
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

instruction_ref codegen_context::add_move_destruct_indicator(ast::decl_variable const *decl)
{
	auto const indicator = this->create_alloca_without_lifetime(this->get_builtin_type(builtin_type_kind::i1));
	[[maybe_unused]] auto const [it, inserted] = this->current_function_info.move_destruct_indicators.insert({ decl, indicator.get_reference() });
	bz_assert(inserted);
	this->create_store(this->create_const_i1(true), indicator);
	return indicator.get_reference();
}

bz::optional<instruction_ref> codegen_context::get_move_destruct_indicator(ast::decl_variable const *decl) const
{
	if (decl == nullptr)
	{
		return {};
	}

	auto const it = this->current_function_info.move_destruct_indicators.find(decl);
	if (it == this->current_function_info.move_destruct_indicators.end())
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
		this->current_function_info.destructor_calls.push_back({
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
	expr_value value,
	bz::optional<instruction_ref> move_destruct_indicator
)
{
	this->current_function_info.destructor_calls.push_back({
		.destruct_op = destruct_op.not_null() ? &destruct_op : nullptr,
		.value = value,
		.condition = move_destruct_indicator,
		.move_destruct_indicator = {},
		.rvalue_array_elem_ptr = {},
	});
}

void codegen_context::push_self_destruct_operation(
	ast::destruct_operation const &destruct_op,
	expr_value value
)
{
	auto const move_destruct_indicator = this->get_move_destruct_indicator(destruct_op.move_destructed_decl);
	if (move_destruct_indicator.has_value() || destruct_op.not_null())
	{
		this->current_function_info.destructor_calls.push_back({
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
		this->current_function_info.destructor_calls.push_back({
			.destruct_op = &destruct_op,
			.value = value,
			.condition = {},
			.move_destruct_indicator = move_destruct_indicator,
			.rvalue_array_elem_ptr = rvalue_array_elem_ptr,
		});
	}
}

void codegen_context::push_end_lifetime(expr_value value)
{
	bz_assert(value.is_reference());
	this->current_function_info.destructor_calls.push_back({
		.destruct_op = nullptr,
		.value = value,
		.condition = {},
		.move_destruct_indicator = {},
		.rvalue_array_elem_ptr = {},
	});
}

static void emit_destruct_operation(destruct_operation_info_t const &info, codegen_context &context)
{
	generate_destruct_operation(info, context);
}

void codegen_context::emit_destruct_operations(size_t destruct_calls_start_index)
{
	if (this->has_terminator())
	{
		return;
	}

	for (auto const &info : this->current_function_info.destructor_calls.slice(destruct_calls_start_index).reversed())
	{
		emit_destruct_operation(info, *this);
	}
}

void codegen_context::emit_loop_destruct_operations(void)
{
	if (this->has_terminator())
	{
		return;
	}

	for (auto const &info : this->current_function_info.destructor_calls.slice(this->loop_info.destructor_stack_begin).reversed())
	{
		emit_destruct_operation(info, *this);
	}
}

void codegen_context::emit_all_destruct_operations(void)
{
	if (this->has_terminator())
	{
		return;
	}

	for (auto const &info : this->current_function_info.destructor_calls.reversed())
	{
		emit_destruct_operation(info, *this);
	}
}

uint32_t codegen_context::add_src_tokens(lex::src_tokens const &src_tokens)
{
	auto const result = this->current_function_info.src_tokens.size();
	this->current_function_info.src_tokens.push_back(src_tokens);
	return static_cast<uint32_t>(result);
}

uint32_t codegen_context::add_slice_construction_check_info(slice_construction_check_info_t info)
{
	auto const result = this->current_function_info.slice_construction_check_infos.size();
	this->current_function_info.slice_construction_check_infos.push_back(info);
	return static_cast<uint32_t>(result);
}

uint32_t codegen_context::add_pointer_arithmetic_check_info(pointer_arithmetic_check_info_t info)
{
	auto const result = this->current_function_info.pointer_arithmetic_check_infos.size();
	this->current_function_info.pointer_arithmetic_check_infos.push_back(info);
	return static_cast<uint32_t>(result);
}

uint32_t codegen_context::add_memory_access_check_info(memory_access_check_info_t info)
{
	auto const result = this->current_function_info.memory_access_check_infos.size();
	this->current_function_info.memory_access_check_infos.push_back(info);
	return static_cast<uint32_t>(result);
}

uint32_t codegen_context::add_add_global_array_data_info(add_global_array_data_info_t info)
{
	auto const result = this->current_function_info.add_global_array_data_infos.size();
	this->current_function_info.add_global_array_data_infos.push_back(info);
	return static_cast<uint32_t>(result);
}

uint32_t codegen_context::add_copy_values_info(copy_values_info_t info)
{
	auto const result = this->current_function_info.copy_values_infos.size();
	this->current_function_info.copy_values_infos.push_back(info);
	return static_cast<uint32_t>(result);
}

uint32_t codegen_context::add_typename_result_info(typename_result_info_t info)
{
	auto const result = this->typename_result_infos.size();
	this->typename_result_infos.push_back(info);
	return static_cast<uint32_t>(result);
}

expr_value codegen_context::get_dummy_value(type const *t)
{
	return expr_value::get_reference(instruction_ref{}, t);
}


expr_value codegen_context::create_const_int(type const *int_type, int64_t value)
{
	bz_assert(int_type->is_integer_type());
	switch (int_type->get_builtin_kind())
	{
	case builtin_type_kind::i1:
		bz_assert(value >= 0 && value <= 1);
		return this->create_const_i1(value != 0);
	case builtin_type_kind::i8:
		bz_assert(value >= std::numeric_limits<int8_t>::min());
		bz_assert(value <= std::numeric_limits<int8_t>::max());
		return this->create_const_i8(static_cast<int8_t>(value));
	case builtin_type_kind::i16:
		bz_assert(value >= std::numeric_limits<int16_t>::min());
		bz_assert(value <= std::numeric_limits<int16_t>::max());
		return this->create_const_i16(static_cast<int16_t>(value));
	case builtin_type_kind::i32:
		bz_assert(value >= std::numeric_limits<int32_t>::min());
		bz_assert(value <= std::numeric_limits<int32_t>::max());
		return this->create_const_i32(static_cast<int32_t>(value));
	case builtin_type_kind::i64:
		bz_assert(value >= std::numeric_limits<int64_t>::min());
		bz_assert(value <= std::numeric_limits<int64_t>::max());
		return this->create_const_i64(static_cast<int64_t>(value));
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_const_int(type const *int_type, uint64_t value)
{
	bz_assert(int_type->is_integer_type());
	switch (int_type->get_builtin_kind())
	{
	case builtin_type_kind::i1:
		bz_assert(value >= 0 && value <= 1);
		return this->create_const_i1(value != 0);
	case builtin_type_kind::i8:
		bz_assert(value <= std::numeric_limits<uint8_t>::max());
		return this->create_const_u8(static_cast<uint8_t>(value));
	case builtin_type_kind::i16:
		bz_assert(value <= std::numeric_limits<uint16_t>::max());
		return this->create_const_u16(static_cast<uint16_t>(value));
	case builtin_type_kind::i32:
		bz_assert(value <= std::numeric_limits<uint32_t>::max());
		return this->create_const_u32(static_cast<uint32_t>(value));
	case builtin_type_kind::i64:
		bz_assert(value <= std::numeric_limits<uint64_t>::max());
		return this->create_const_u64(static_cast<uint64_t>(value));
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_const_i1(bool value)
{
	auto const inst_ref = add_instruction(*this, instructions::const_i1{ .value = value });
	return expr_value::get_value(inst_ref, this->get_builtin_type(builtin_type_kind::i1));
}

expr_value codegen_context::create_const_i8(int8_t value)
{
	auto const inst_ref = add_instruction(*this, instructions::const_i8{ .value = value });
	return expr_value::get_value(inst_ref, this->get_builtin_type(builtin_type_kind::i8));
}

expr_value codegen_context::create_const_i16(int16_t value)
{
	auto const inst_ref = add_instruction(*this, instructions::const_i16{ .value = value });
	return expr_value::get_value(inst_ref, this->get_builtin_type(builtin_type_kind::i16));
}

expr_value codegen_context::create_const_i32(int32_t value)
{
	auto const inst_ref = add_instruction(*this, instructions::const_i32{ .value = value });
	return expr_value::get_value(inst_ref, this->get_builtin_type(builtin_type_kind::i32));
}

expr_value codegen_context::create_const_i64(int64_t value)
{
	auto const inst_ref = add_instruction(*this, instructions::const_i64{ .value = value });
	return expr_value::get_value(inst_ref, this->get_builtin_type(builtin_type_kind::i64));
}

expr_value codegen_context::create_const_u8(uint8_t value)
{
	auto const inst_ref = add_instruction(*this, instructions::const_u8{ .value = value });
	return expr_value::get_value(inst_ref, this->get_builtin_type(builtin_type_kind::i8));
}

expr_value codegen_context::create_const_u16(uint16_t value)
{
	auto const inst_ref = add_instruction(*this, instructions::const_u16{ .value = value });
	return expr_value::get_value(inst_ref, this->get_builtin_type(builtin_type_kind::i16));
}

expr_value codegen_context::create_const_u32(uint32_t value)
{
	auto const inst_ref = add_instruction(*this, instructions::const_u32{ .value = value });
	return expr_value::get_value(inst_ref, this->get_builtin_type(builtin_type_kind::i32));
}

expr_value codegen_context::create_const_u64(uint64_t value)
{
	auto const inst_ref = add_instruction(*this, instructions::const_u64{ .value = value });
	return expr_value::get_value(inst_ref, this->get_builtin_type(builtin_type_kind::i64));
}

expr_value codegen_context::create_const_f32(float32_t value)
{
	auto const inst_ref = add_instruction(*this, instructions::const_f32{ .value = value });
	return expr_value::get_value(inst_ref, this->get_builtin_type(builtin_type_kind::f32));
}

expr_value codegen_context::create_const_f64(float64_t value)
{
	auto const inst_ref = add_instruction(*this, instructions::const_f64{ .value = value });
	return expr_value::get_value(inst_ref, this->get_builtin_type(builtin_type_kind::f64));
}

expr_value codegen_context::create_const_ptr_null(void)
{
	auto const inst_ref = add_instruction(*this, instructions::const_ptr_null{});
	return expr_value::get_value(inst_ref, this->get_pointer_type());
}

void codegen_context::create_string(lex::src_tokens const &src_tokens, bz::u8string_view str, expr_value result_address)
{
	auto const global_object_type = this->get_array_type(this->get_builtin_type(builtin_type_kind::i8), str.size());
	auto data = bz::fixed_vector<uint8_t>(bz::array_view(str.data(), str.size()));
	auto const index = this->global_memory.add_object(src_tokens, global_object_type, std::move(data));
	auto const str_data = this->create_get_global_object(index);
	auto const str_end = this->create_struct_gep(str_data, str.size());
	auto const str_begin_ptr = expr_value::get_value(str_data.get_reference(), this->get_pointer_type());
	auto const str_end_ptr = expr_value::get_value(str_end.get_reference(), this->get_pointer_type());

	bz_assert(result_address.is_reference());
	bz_assert(result_address.get_type() == this->get_str_t());
	this->create_store(str_begin_ptr, this->create_struct_gep(result_address, 0));
	this->create_store(str_end_ptr,   this->create_struct_gep(result_address, 1));
	this->create_start_lifetime(result_address);
}

expr_value codegen_context::create_string(lex::src_tokens const &src_tokens, bz::u8string_view str)
{
	auto const result_address = this->create_alloca(src_tokens, this->get_str_t());
	this->create_string(src_tokens, str, result_address);
	return result_address;
}

expr_value codegen_context::create_typename(ast::typespec_view type)
{
	auto const result = this->add_typename_result_info({ type });
	return this->create_const_u32(result);
}

codegen_context::global_object_result codegen_context::create_global_object(
	lex::src_tokens const &src_tokens,
	type const *object_type,
	bz::fixed_vector<uint8_t> data
)
{
	auto const index = this->global_memory.add_object(src_tokens, object_type, std::move(data));
	return { this->create_get_global_object(index), index };
}

expr_value codegen_context::create_add_global_array_data(lex::src_tokens const &src_tokens, type const *elem_type, expr_value begin_ptr, expr_value end_ptr)
{
	auto const begin_ptr_value = begin_ptr.get_value_as_instruction(*this);
	auto const end_ptr_value = end_ptr.get_value_as_instruction(*this);
	auto const info_index = this->add_add_global_array_data_info({ .elem_type = elem_type, .src_tokens = src_tokens });
	return expr_value::get_value(
		add_instruction(*this, instructions::add_global_array_data{
			.add_global_array_data_info_index = info_index
		}, begin_ptr_value, end_ptr_value),
		this->get_pointer_type()
	);
}

expr_value codegen_context::create_get_global_object(uint32_t global_index)
{
	bz_assert(global_index < this->global_memory.objects.size());
	return expr_value::get_reference(
		add_instruction(*this, instructions::get_global_address{ .global_index = global_index }),
		this->global_memory.objects[global_index].object_type
	);
}

expr_value codegen_context::create_get_function_return_address(void)
{
	auto const return_type = this->current_function_info.func->return_type;
	bz_assert(!return_type->is_simple_value_type());
	return expr_value::get_reference(
		add_instruction(*this, instructions::get_function_arg{ .arg_index = 0 }),
		return_type
	);
}

instruction_ref codegen_context::create_get_function_arg(uint32_t arg_index)
{
	auto const needs_return_address = !this->current_function_info.func->return_type->is_simple_value_type();
	return add_instruction(*this, instructions::get_function_arg{ .arg_index = arg_index + (needs_return_address ? 1 : 0) });
}

expr_value codegen_context::create_load(expr_value ptr)
{
	bz_assert(ptr.is_reference());
	auto const type = ptr.get_type();
	bz_assert(type->is_builtin() || type->is_pointer());
	auto const ptr_ = ptr.get_reference();
	if (type->is_pointer())
	{
		if (this->is_little_endian())
		{
			if (this->is_64_bit())
			{
				return expr_value::get_value(
					add_instruction(*this, instructions::load_ptr64_le{}, ptr_),
					type
				);
			}
			else
			{
				return expr_value::get_value(
					add_instruction(*this, instructions::load_ptr32_le{}, ptr_),
					type
				);
			}
		}
		else
		{
			if (this->is_64_bit())
			{
				return expr_value::get_value(
					add_instruction(*this, instructions::load_ptr64_be{}, ptr_),
					type
				);
			}
			else
			{
				return expr_value::get_value(
					add_instruction(*this, instructions::load_ptr32_be{}, ptr_),
					type
				);
			}
		}
	}
	else
	{
		if (this->is_little_endian())
		{
			switch (type->get_builtin_kind())
			{
			case builtin_type_kind::i1:
				return expr_value::get_value(
					add_instruction(*this, instructions::load_i1_le{}, ptr_),
					type
				);
			case builtin_type_kind::i8:
				return expr_value::get_value(
					add_instruction(*this, instructions::load_i8_le{}, ptr_),
					type
				);
			case builtin_type_kind::i16:
				return expr_value::get_value(
					add_instruction(*this, instructions::load_i16_le{}, ptr_),
					type
				);
			case builtin_type_kind::i32:
				return expr_value::get_value(
					add_instruction(*this, instructions::load_i32_le{}, ptr_),
					type
				);
			case builtin_type_kind::i64:
				return expr_value::get_value(
					add_instruction(*this, instructions::load_i64_le{}, ptr_),
					type
				);
			case builtin_type_kind::f32:
				return expr_value::get_value(
					add_instruction(*this, instructions::load_f32_le{}, ptr_),
					type
				);
			case builtin_type_kind::f64:
				return expr_value::get_value(
					add_instruction(*this, instructions::load_f64_le{}, ptr_),
					type
				);
			case builtin_type_kind::void_:
				bz_unreachable;
			}
		}
		else
		{
			switch (type->get_builtin_kind())
			{
			case builtin_type_kind::i1:
				return expr_value::get_value(
					add_instruction(*this, instructions::load_i1_be{}, ptr_),
					type
				);
			case builtin_type_kind::i8:
				return expr_value::get_value(
					add_instruction(*this, instructions::load_i8_be{}, ptr_),
					type
				);
			case builtin_type_kind::i16:
				return expr_value::get_value(
					add_instruction(*this, instructions::load_i16_be{}, ptr_),
					type
				);
			case builtin_type_kind::i32:
				return expr_value::get_value(
					add_instruction(*this, instructions::load_i32_be{}, ptr_),
					type
				);
			case builtin_type_kind::i64:
				return expr_value::get_value(
					add_instruction(*this, instructions::load_i64_be{}, ptr_),
					type
				);
			case builtin_type_kind::f32:
				return expr_value::get_value(
					add_instruction(*this, instructions::load_f32_be{}, ptr_),
					type
				);
			case builtin_type_kind::f64:
				return expr_value::get_value(
					add_instruction(*this, instructions::load_f64_be{}, ptr_),
					type
				);
			case builtin_type_kind::void_:
				bz_unreachable;
			}
		}
	}
}

instruction_ref codegen_context::create_store(expr_value value, expr_value ptr)
{
	bz_assert(value.get_type() == ptr.get_type());
	auto const type = value.get_type();
	bz_assert(type->is_builtin() || type->is_pointer());
	auto const value_ = value.get_value_as_instruction(*this);
	auto const ptr_ = ptr.get_reference();
	if (type->is_pointer())
	{
		if (this->is_little_endian())
		{
			if (this->is_64_bit())
			{
				return add_instruction(*this, instructions::store_ptr64_le{}, value_, ptr_);
			}
			else
			{
				return add_instruction(*this, instructions::store_ptr32_le{}, value_, ptr_);
			}
		}
		else
		{
			if (this->is_64_bit())
			{
				return add_instruction(*this, instructions::store_ptr64_be{}, value_, ptr_);
			}
			else
			{
				return add_instruction(*this, instructions::store_ptr32_be{}, value_, ptr_);
			}
		}
	}
	else
	{
		if (this->is_little_endian())
		{
			switch (type->get_builtin_kind())
			{
			case builtin_type_kind::i1:
				return add_instruction(*this, instructions::store_i1_le{}, value_, ptr_);
			case builtin_type_kind::i8:
				return add_instruction(*this, instructions::store_i8_le{}, value_, ptr_);
			case builtin_type_kind::i16:
				return add_instruction(*this, instructions::store_i16_le{}, value_, ptr_);
			case builtin_type_kind::i32:
				return add_instruction(*this, instructions::store_i32_le{}, value_, ptr_);
			case builtin_type_kind::i64:
				return add_instruction(*this, instructions::store_i64_le{}, value_, ptr_);
			case builtin_type_kind::f32:
				return add_instruction(*this, instructions::store_f32_le{}, value_, ptr_);
			case builtin_type_kind::f64:
				return add_instruction(*this, instructions::store_f64_le{}, value_, ptr_);
			case builtin_type_kind::void_:
				bz_unreachable;
			}
		}
		else
		{
			switch (type->get_builtin_kind())
			{
			case builtin_type_kind::i1:
				return add_instruction(*this, instructions::store_i1_be{}, value_, ptr_);
			case builtin_type_kind::i8:
				return add_instruction(*this, instructions::store_i8_be{}, value_, ptr_);
			case builtin_type_kind::i16:
				return add_instruction(*this, instructions::store_i16_be{}, value_, ptr_);
			case builtin_type_kind::i32:
				return add_instruction(*this, instructions::store_i32_be{}, value_, ptr_);
			case builtin_type_kind::i64:
				return add_instruction(*this, instructions::store_i64_be{}, value_, ptr_);
			case builtin_type_kind::f32:
				return add_instruction(*this, instructions::store_f32_be{}, value_, ptr_);
			case builtin_type_kind::f64:
				return add_instruction(*this, instructions::store_f64_be{}, value_, ptr_);
			case builtin_type_kind::void_:
				bz_unreachable;
			}
		}
	}
}

void codegen_context::create_memory_access_check(
	lex::src_tokens const &src_tokens,
	expr_value ptr,
	ast::typespec_view object_typespec
)
{
	bz_assert(ptr.is_reference());

	auto const src_tokens_index = this->add_src_tokens(src_tokens);
	auto const memory_access_check_info_index = this->add_memory_access_check_info({
		.object_type = ptr.get_type(),
		.object_typespec = object_typespec,
	});
	auto const ptr_value = ptr.get_reference();

	add_instruction(*this, instructions::check_dereference{
		.src_tokens_index = src_tokens_index,
		.memory_access_check_info_index = memory_access_check_info_index,
	}, ptr_value);
}

void codegen_context::create_inplace_construct_check(
	lex::src_tokens const &src_tokens,
	expr_value ptr,
	ast::typespec_view object_typespec
)
{
	bz_assert(ptr.is_reference());

	auto const src_tokens_index = this->add_src_tokens(src_tokens);
	auto const memory_access_check_info_index = this->add_memory_access_check_info({
		.object_type = ptr.get_type(),
		.object_typespec = object_typespec,
	});
	auto const ptr_value = ptr.get_reference();

	add_instruction(*this, instructions::check_inplace_construct{
		.src_tokens_index = src_tokens_index,
		.memory_access_check_info_index = memory_access_check_info_index,
	}, ptr_value);
}

expr_value codegen_context::create_alloca(lex::src_tokens const &src_tokens, type const *type)
{
	this->current_function_info.allocas.push_back({ type, false, src_tokens });
	auto const alloca_ref = instruction_ref{
		.bb_index = instruction_ref::alloca_bb_index,
		.inst_index = static_cast<uint32_t>(this->current_function_info.allocas.size() - 1),
	};
	return expr_value::get_reference(alloca_ref, type);
}

expr_value codegen_context::create_alloca_without_lifetime(type const *type)
{
	this->current_function_info.allocas.push_back({ type, true, {} });
	auto const alloca_ref = instruction_ref{
		.bb_index = instruction_ref::alloca_bb_index,
		.inst_index = static_cast<uint32_t>(this->current_function_info.allocas.size() - 1),
	};
	return expr_value::get_reference(alloca_ref, type);
}

instruction_ref codegen_context::create_jump(basic_block_ref bb)
{
	auto const result = add_instruction(*this, instructions::jump{});
	this->current_function_info.unresolved_jumps.push_back({ .inst = result, .dests = { bb, {} } });
	return result;
}

instruction_ref codegen_context::create_conditional_jump(
	expr_value condition,
	basic_block_ref true_bb,
	basic_block_ref false_bb
)
{
	auto const result = add_instruction(*this, instructions::conditional_jump{}, condition.get_value_as_instruction(*this));
	this->current_function_info.unresolved_jumps.push_back({ .inst = result, .dests = { true_bb, false_bb } });
	return result;
}

instruction_ref codegen_context::create_switch(
	expr_value value,
	bz::vector<unresolved_switch::value_bb_pair> values,
	basic_block_ref default_bb
)
{
	auto const value_ref = value.get_value_as_instruction(*this);
	auto const switch_info_index = static_cast<uint32_t>(this->current_function_info.unresolved_switches.size());
	bz_assert(value.get_type()->is_integer_type());
	auto const result = [&]() {
		switch (value.get_type()->get_builtin_kind())
		{
		case builtin_type_kind::i1:
			return add_instruction(*this, instructions::switch_i1{ .switch_info_index = switch_info_index }, value_ref);
		case builtin_type_kind::i8:
			return add_instruction(*this, instructions::switch_i8{ .switch_info_index = switch_info_index }, value_ref);
		case builtin_type_kind::i16:
			return add_instruction(*this, instructions::switch_i16{ .switch_info_index = switch_info_index }, value_ref);
		case builtin_type_kind::i32:
			return add_instruction(*this, instructions::switch_i32{ .switch_info_index = switch_info_index }, value_ref);
		case builtin_type_kind::i64:
			return add_instruction(*this, instructions::switch_i64{ .switch_info_index = switch_info_index }, value_ref);
		default:
			bz_unreachable;
		}
	}();
	this->current_function_info.unresolved_switches.push_back({
		.inst = result,
		.values = std::move(values),
		.default_bb = default_bb,
	});
	return result;
}

instruction_ref codegen_context::create_ret(instruction_ref value)
{
	return add_instruction(*this, instructions::ret{}, value);
}

instruction_ref codegen_context::create_ret_void(void)
{
	return add_instruction(*this, instructions::ret_void{});
}

expr_value codegen_context::create_struct_gep(expr_value value, size_t index)
{
	bz_assert(value.is_reference());
	auto const type = value.get_type();
	if (type->is_array())
	{
		bz_assert(index <= type->get_array_size()); // one-past-the-end is allowed
		bz_assert(index <= std::numeric_limits<uint32_t>::max());
		auto const result_ptr = add_instruction(*this, instructions::const_gep{
			.object_type = type,
			.index = static_cast<uint32_t>(index),
		}, value.get_reference());
		return expr_value::get_reference(result_ptr, type->get_array_element_type());
	}
	else
	{
		bz_assert(type->is_aggregate());
		auto const types = type->get_aggregate_types();
		bz_assert(index < types.size());
		bz_assert(index <= std::numeric_limits<uint32_t>::max());
		auto const result_ptr = add_instruction(*this, instructions::const_gep{
			.object_type = type,
			.index = static_cast<uint32_t>(index),
		}, value.get_reference());
		return expr_value::get_reference(result_ptr, types[index]);
	}
}

expr_value codegen_context::create_array_gep(expr_value value, expr_value index)
{
	bz_assert(value.is_reference());
	bz_assert(value.get_type()->is_array());

	auto const elem_type = value.get_type()->get_array_element_type();
	bz_assert(index.get_type()->is_builtin());
	switch (index.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::i8:
	case builtin_type_kind::i16:
		index = this->create_int_cast(index, this->get_builtin_type(builtin_type_kind::i32), false);
		[[fallthrough]];
	case builtin_type_kind::i32:
	{
		auto const result_ptr = add_instruction(*this,
			instructions::array_gep_i32{ .elem_type = elem_type },
			value.get_reference(), index.get_value_as_instruction(*this)
		);
		return expr_value::get_reference(result_ptr, elem_type);
	}
	case builtin_type_kind::i64:
	{
		auto const result_ptr = add_instruction(*this,
			instructions::array_gep_i64{ .elem_type = elem_type },
			value.get_reference(), index.get_value_as_instruction(*this)
		);
		return expr_value::get_reference(result_ptr, elem_type);
	}
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_array_slice_gep(expr_value begin_ptr, expr_value index, type const *elem_type)
{
	bz_assert(begin_ptr.get_type()->is_pointer());

	bz_assert(index.get_type()->is_builtin());
	switch (index.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::i8:
	case builtin_type_kind::i16:
		index = this->create_int_cast(index, this->get_builtin_type(builtin_type_kind::i32), false);
		[[fallthrough]];
	case builtin_type_kind::i32:
	{
		auto const result_ptr = add_instruction(*this,
			instructions::array_gep_i32{ .elem_type = elem_type },
			begin_ptr.get_value_as_instruction(*this), index.get_value_as_instruction(*this)
		);
		return expr_value::get_reference(result_ptr, elem_type);
	}
	case builtin_type_kind::i64:
	{
		auto const result_ptr = add_instruction(*this,
			instructions::array_gep_i64{ .elem_type = elem_type },
			begin_ptr.get_value_as_instruction(*this), index.get_value_as_instruction(*this)
		);
		return expr_value::get_reference(result_ptr, elem_type);
	}
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_const_memcpy(expr_value dest, expr_value source, size_t size)
{
	bz_assert(dest.is_reference());
	bz_assert(source.is_reference());

	return add_instruction(*this, instructions::const_memcpy{ .size = size }, dest.get_reference(), source.get_reference());
}

instruction_ref codegen_context::create_const_memset_zero(expr_value dest)
{
	bz_assert(dest.is_reference());

	return add_instruction(*this, instructions::const_memset_zero{ .size = dest.get_type()->size }, dest.get_reference());
}

void codegen_context::create_copy_values(
	lex::src_tokens const &src_tokens,
	expr_value dest,
	expr_value source,
	expr_value count,
	type const *elem_type,
	ast::typespec_view elem_typespec
)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);
	auto const copy_values_info_index = this->add_copy_values_info({
		.elem_type = elem_type,
		.src_tokens_index = src_tokens_index,
		.is_trivially_destructible = this->parse_ctx->is_trivially_destructible(src_tokens, elem_typespec),
	});

	auto const dest_value = dest.get_value_as_instruction(*this);
	auto const source_value = source.get_value_as_instruction(*this);
	auto const count_cast = this->create_int_cast(count, this->get_builtin_type(builtin_type_kind::i64), false);
	auto const count_value = count_cast.get_value_as_instruction(*this);

	add_instruction(*this, instructions::copy_values{
		.copy_values_info_index = copy_values_info_index
	}, dest_value, source_value, count_value);
}

void codegen_context::create_relocate_values(
	lex::src_tokens const &src_tokens,
	expr_value dest,
	expr_value source,
	expr_value count,
	type const *elem_type,
	ast::typespec_view elem_typespec
)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);
	auto const copy_values_info_index = this->add_copy_values_info({
		.elem_type = elem_type,
		.src_tokens_index = src_tokens_index,
		.is_trivially_destructible = this->parse_ctx->is_trivially_destructible(src_tokens, elem_typespec),
	});

	auto const dest_value = dest.get_value_as_instruction(*this);
	auto const source_value = source.get_value_as_instruction(*this);
	auto const count_cast = this->create_int_cast(count, this->get_builtin_type(builtin_type_kind::i64), false);
	auto const count_value = count_cast.get_value_as_instruction(*this);

	add_instruction(*this, instructions::relocate_values{
		.copy_values_info_index = copy_values_info_index
	}, dest_value, source_value, count_value);
}

expr_value codegen_context::create_function_call(lex::src_tokens const &src_tokens, function const *func, bz::fixed_vector<instruction_ref> args)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);
	auto const args_index = static_cast<uint32_t>(this->current_function_info.call_args.size());
	this->current_function_info.call_args.push_back(std::move(args));

	auto const inst_ref = add_instruction(*this,
		instructions::function_call{ .func = func, .args_index = args_index, .src_tokens_index = src_tokens_index }
	);
	if (func->return_type->is_simple_value_type())
	{
		return expr_value::get_value(inst_ref, func->return_type);
	}
	else
	{
		return expr_value::get_none();
	}
}

expr_value codegen_context::create_malloc(lex::src_tokens const &src_tokens, type const *type, expr_value count)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(count.get_type()->is_integer_type());
	auto const count_val = this->create_int_cast(count, this->get_builtin_type(builtin_type_kind::i64), false)
		.get_value_as_instruction(*this);

	return expr_value::get_value(
		add_instruction(*this, instructions::malloc{ .type = type, .src_tokens_index = src_tokens_index }, count_val),
		this->get_pointer_type()
	);
}

void codegen_context::create_free(lex::src_tokens const &src_tokens, expr_value ptr)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(ptr.get_type()->is_pointer());
	auto const ptr_val = ptr.get_value_as_instruction(*this);

	add_instruction(*this, instructions::free{ .src_tokens_index = src_tokens_index }, ptr_val);
}

expr_value codegen_context::create_int_cast(expr_value value, type const *dest, bool is_value_signed)
{
	auto const value_type = value.get_type();
	bz_assert(value_type->is_builtin() && dest->is_builtin());
	bz_assert(is_integer_kind(value_type->get_builtin_kind()));
	bz_assert(is_integer_kind(dest->get_builtin_kind()));

	if (value_type->get_builtin_kind() == dest->get_builtin_kind())
	{
		return value;
	}

	auto const value_ref = value.get_value_as_instruction(*this);
	switch (value_type->get_builtin_kind())
	{
	case builtin_type_kind::i1:
		switch (dest->get_builtin_kind())
		{
		// case builtin_type_kind::i1:
		case builtin_type_kind::i8:
			return expr_value::get_value(add_instruction(*this, instructions::cast_zext_i1_to_i8{}, value_ref), dest);
		case builtin_type_kind::i16:
			return expr_value::get_value(add_instruction(*this, instructions::cast_zext_i1_to_i16{}, value_ref), dest);
		case builtin_type_kind::i32:
			return expr_value::get_value(add_instruction(*this, instructions::cast_zext_i1_to_i32{}, value_ref), dest);
		case builtin_type_kind::i64:
			return expr_value::get_value(add_instruction(*this, instructions::cast_zext_i1_to_i64{}, value_ref), dest);
		default:
			bz_unreachable;
		}
	case builtin_type_kind::i8:
		if (is_value_signed)
		{
			switch (dest->get_builtin_kind())
			{
			// case builtin_type_kind::i8:
			case builtin_type_kind::i16:
				return expr_value::get_value(add_instruction(*this, instructions::cast_sext_i8_to_i16{}, value_ref), dest);
			case builtin_type_kind::i32:
				return expr_value::get_value(add_instruction(*this, instructions::cast_sext_i8_to_i32{}, value_ref), dest);
			case builtin_type_kind::i64:
				return expr_value::get_value(add_instruction(*this, instructions::cast_sext_i8_to_i64{}, value_ref), dest);
			default:
				bz_unreachable;
			}
		}
		else
		{
			switch (dest->get_builtin_kind())
			{
			// case builtin_type_kind::i8:
			case builtin_type_kind::i16:
				return expr_value::get_value(add_instruction(*this, instructions::cast_zext_i8_to_i16{}, value_ref), dest);
			case builtin_type_kind::i32:
				return expr_value::get_value(add_instruction(*this, instructions::cast_zext_i8_to_i32{}, value_ref), dest);
			case builtin_type_kind::i64:
				return expr_value::get_value(add_instruction(*this, instructions::cast_zext_i8_to_i64{}, value_ref), dest);
			default:
				bz_unreachable;
			}
		}
	case builtin_type_kind::i16:
		if (is_value_signed)
		{
			switch (dest->get_builtin_kind())
			{
			case builtin_type_kind::i8:
				return expr_value::get_value(add_instruction(*this, instructions::cast_trunc_i16_to_i8{}, value_ref), dest);
			// case builtin_type_kind::i16:
			case builtin_type_kind::i32:
				return expr_value::get_value(add_instruction(*this, instructions::cast_sext_i16_to_i32{}, value_ref), dest);
			case builtin_type_kind::i64:
				return expr_value::get_value(add_instruction(*this, instructions::cast_sext_i16_to_i64{}, value_ref), dest);
			default:
				bz_unreachable;
			}
		}
		else
		{
			switch (dest->get_builtin_kind())
			{
			case builtin_type_kind::i8:
				return expr_value::get_value(add_instruction(*this, instructions::cast_trunc_i16_to_i8{}, value_ref), dest);
			// case builtin_type_kind::i16:
			case builtin_type_kind::i32:
				return expr_value::get_value(add_instruction(*this, instructions::cast_zext_i16_to_i32{}, value_ref), dest);
			case builtin_type_kind::i64:
				return expr_value::get_value(add_instruction(*this, instructions::cast_zext_i16_to_i64{}, value_ref), dest);
			default:
				bz_unreachable;
			}
		}
	case builtin_type_kind::i32:
		if (is_value_signed)
		{
			switch (dest->get_builtin_kind())
			{
			case builtin_type_kind::i8:
				return expr_value::get_value(add_instruction(*this, instructions::cast_trunc_i32_to_i8{}, value_ref), dest);
			case builtin_type_kind::i16:
				return expr_value::get_value(add_instruction(*this, instructions::cast_trunc_i32_to_i16{}, value_ref), dest);
			// case builtin_type_kind::i32:
			case builtin_type_kind::i64:
				return expr_value::get_value(add_instruction(*this, instructions::cast_sext_i32_to_i64{}, value_ref), dest);
			default:
				bz_unreachable;
			}
		}
		else
		{
			switch (dest->get_builtin_kind())
			{
			case builtin_type_kind::i8:
				return expr_value::get_value(add_instruction(*this, instructions::cast_trunc_i32_to_i8{}, value_ref), dest);
			case builtin_type_kind::i16:
				return expr_value::get_value(add_instruction(*this, instructions::cast_trunc_i32_to_i16{}, value_ref), dest);
			// case builtin_type_kind::i32:
			case builtin_type_kind::i64:
				return expr_value::get_value(add_instruction(*this, instructions::cast_zext_i32_to_i64{}, value_ref), dest);
			default:
				bz_unreachable;
			}
		}
	case builtin_type_kind::i64:
		switch (dest->get_builtin_kind())
		{
		case builtin_type_kind::i8:
			return expr_value::get_value(add_instruction(*this, instructions::cast_trunc_i64_to_i8{}, value_ref), dest);
		case builtin_type_kind::i16:
			return expr_value::get_value(add_instruction(*this, instructions::cast_trunc_i64_to_i16{}, value_ref), dest);
		case builtin_type_kind::i32:
			return expr_value::get_value(add_instruction(*this, instructions::cast_trunc_i64_to_i32{}, value_ref), dest);
		// case builtin_type_kind::i64:
		default:
			bz_unreachable;
		}
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_float_cast(expr_value value, type const *dest)
{
	auto const value_type = value.get_type();
	bz_assert(value_type->is_builtin() && dest->is_builtin());
	bz_assert(is_floating_point_kind(value_type->get_builtin_kind()));
	bz_assert(is_floating_point_kind(dest->get_builtin_kind()));

	if (value_type->get_builtin_kind() == dest->get_builtin_kind())
	{
		return value;
	}
	else if (value_type->get_builtin_kind() == builtin_type_kind::f32)
	{
		return expr_value::get_value(add_instruction(*this,
			instructions::cast_f32_to_f64{},
			value.get_value_as_instruction(*this)
		), dest);
	}
	else
	{
		return expr_value::get_value(add_instruction(*this,
			instructions::cast_f64_to_f32{},
			value.get_value_as_instruction(*this)
		), dest);
	}
}

expr_value codegen_context::create_float_to_int_cast(expr_value value, type const *dest, bool is_dest_signed)
{
	auto const value_type = value.get_type();
	bz_assert(value_type->is_builtin() && dest->is_builtin());
	bz_assert(is_floating_point_kind(value_type->get_builtin_kind()));
	bz_assert(is_integer_kind(dest->get_builtin_kind()));

	auto const value_ref = value.get_value_as_instruction(*this);
	if (value_type->get_builtin_kind() == builtin_type_kind::f32)
	{
		if (is_dest_signed)
		{
			switch (dest->get_builtin_kind())
			{
			case builtin_type_kind::i8:
				return expr_value::get_value(add_instruction(*this, instructions::cast_f32_to_i8{}, value_ref), dest);
			case builtin_type_kind::i16:
				return expr_value::get_value(add_instruction(*this, instructions::cast_f32_to_i16{}, value_ref), dest);
			case builtin_type_kind::i32:
				return expr_value::get_value(add_instruction(*this, instructions::cast_f32_to_i32{}, value_ref), dest);
			case builtin_type_kind::i64:
				return expr_value::get_value(add_instruction(*this, instructions::cast_f32_to_i64{}, value_ref), dest);
			default:
				bz_unreachable;
			}
		}
		else
		{
			switch (dest->get_builtin_kind())
			{
			case builtin_type_kind::i8:
				return expr_value::get_value(add_instruction(*this, instructions::cast_f32_to_u8{}, value_ref), dest);
			case builtin_type_kind::i16:
				return expr_value::get_value(add_instruction(*this, instructions::cast_f32_to_u16{}, value_ref), dest);
			case builtin_type_kind::i32:
				return expr_value::get_value(add_instruction(*this, instructions::cast_f32_to_u32{}, value_ref), dest);
			case builtin_type_kind::i64:
				return expr_value::get_value(add_instruction(*this, instructions::cast_f32_to_u64{}, value_ref), dest);
			default:
				bz_unreachable;
			}
		}
	}
	else
	{
		if (is_dest_signed)
		{
			switch (dest->get_builtin_kind())
			{
			case builtin_type_kind::i8:
				return expr_value::get_value(add_instruction(*this, instructions::cast_f64_to_i8{}, value_ref), dest);
			case builtin_type_kind::i16:
				return expr_value::get_value(add_instruction(*this, instructions::cast_f64_to_i16{}, value_ref), dest);
			case builtin_type_kind::i32:
				return expr_value::get_value(add_instruction(*this, instructions::cast_f64_to_i32{}, value_ref), dest);
			case builtin_type_kind::i64:
				return expr_value::get_value(add_instruction(*this, instructions::cast_f64_to_i64{}, value_ref), dest);
			default:
				bz_unreachable;
			}
		}
		else
		{
			switch (dest->get_builtin_kind())
			{
			case builtin_type_kind::i8:
				return expr_value::get_value(add_instruction(*this, instructions::cast_f64_to_u8{}, value_ref), dest);
			case builtin_type_kind::i16:
				return expr_value::get_value(add_instruction(*this, instructions::cast_f64_to_u16{}, value_ref), dest);
			case builtin_type_kind::i32:
				return expr_value::get_value(add_instruction(*this, instructions::cast_f64_to_u32{}, value_ref), dest);
			case builtin_type_kind::i64:
				return expr_value::get_value(add_instruction(*this, instructions::cast_f64_to_u64{}, value_ref), dest);
			default:
				bz_unreachable;
			}
		}
	}
}

expr_value codegen_context::create_int_to_float_cast(expr_value value, type const *dest, bool is_value_signed)
{
	auto const value_type = value.get_type();
	bz_assert(value_type->is_builtin() && dest->is_builtin());
	bz_assert(is_integer_kind(value_type->get_builtin_kind()));
	bz_assert(is_floating_point_kind(dest->get_builtin_kind()));

	auto const value_ref = value.get_value_as_instruction(*this);
	if (dest->get_builtin_kind() == builtin_type_kind::f32)
	{
		if (is_value_signed)
		{
			switch (value_type->get_builtin_kind())
			{
			case builtin_type_kind::i8:
				return expr_value::get_value(add_instruction(*this, instructions::cast_i8_to_f32{}, value_ref), dest);
			case builtin_type_kind::i16:
				return expr_value::get_value(add_instruction(*this, instructions::cast_i16_to_f32{}, value_ref), dest);
			case builtin_type_kind::i32:
				return expr_value::get_value(add_instruction(*this, instructions::cast_i32_to_f32{}, value_ref), dest);
			case builtin_type_kind::i64:
				return expr_value::get_value(add_instruction(*this, instructions::cast_i64_to_f32{}, value_ref), dest);
			default:
				bz_unreachable;
			}
		}
		else
		{
			switch (value_type->get_builtin_kind())
			{
			case builtin_type_kind::i8:
				return expr_value::get_value(add_instruction(*this, instructions::cast_u8_to_f32{}, value_ref), dest);
			case builtin_type_kind::i16:
				return expr_value::get_value(add_instruction(*this, instructions::cast_u16_to_f32{}, value_ref), dest);
			case builtin_type_kind::i32:
				return expr_value::get_value(add_instruction(*this, instructions::cast_u32_to_f32{}, value_ref), dest);
			case builtin_type_kind::i64:
				return expr_value::get_value(add_instruction(*this, instructions::cast_u64_to_f32{}, value_ref), dest);
			default:
				bz_unreachable;
			}
		}
	}
	else
	{
		if (is_value_signed)
		{
			switch (value_type->get_builtin_kind())
			{
			case builtin_type_kind::i8:
				return expr_value::get_value(add_instruction(*this, instructions::cast_i8_to_f64{}, value_ref), dest);
			case builtin_type_kind::i16:
				return expr_value::get_value(add_instruction(*this, instructions::cast_i16_to_f64{}, value_ref), dest);
			case builtin_type_kind::i32:
				return expr_value::get_value(add_instruction(*this, instructions::cast_i32_to_f64{}, value_ref), dest);
			case builtin_type_kind::i64:
				return expr_value::get_value(add_instruction(*this, instructions::cast_i64_to_f64{}, value_ref), dest);
			default:
				bz_unreachable;
			}
		}
		else
		{
			switch (value_type->get_builtin_kind())
			{
			case builtin_type_kind::i8:
				return expr_value::get_value(add_instruction(*this, instructions::cast_u8_to_f64{}, value_ref), dest);
			case builtin_type_kind::i16:
				return expr_value::get_value(add_instruction(*this, instructions::cast_u16_to_f64{}, value_ref), dest);
			case builtin_type_kind::i32:
				return expr_value::get_value(add_instruction(*this, instructions::cast_u32_to_f64{}, value_ref), dest);
			case builtin_type_kind::i64:
				return expr_value::get_value(add_instruction(*this, instructions::cast_u64_to_f64{}, value_ref), dest);
			default:
				bz_unreachable;
			}
		}
	}
}

expr_value codegen_context::create_int_cmp_eq(expr_value lhs, expr_value rhs)
{
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::i1:
		return expr_value::get_value(
			add_instruction(*this, instructions::cmp_eq_i1{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::i8:
		return expr_value::get_value(
			add_instruction(*this, instructions::cmp_eq_i8{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::i16:
		return expr_value::get_value(
			add_instruction(*this, instructions::cmp_eq_i16{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			add_instruction(*this, instructions::cmp_eq_i32{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			add_instruction(*this, instructions::cmp_eq_i64{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_int_cmp_neq(expr_value lhs, expr_value rhs)
{
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::i1:
		return expr_value::get_value(
			add_instruction(*this, instructions::cmp_neq_i1{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::i8:
		return expr_value::get_value(
			add_instruction(*this, instructions::cmp_neq_i8{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::i16:
		return expr_value::get_value(
			add_instruction(*this, instructions::cmp_neq_i16{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			add_instruction(*this, instructions::cmp_neq_i32{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			add_instruction(*this, instructions::cmp_neq_i64{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_int_cmp_lt(expr_value lhs, expr_value rhs, bool is_signed)
{
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);

	if (is_signed)
	{
		switch (lhs.get_type()->get_builtin_kind())
		{
		case builtin_type_kind::i8:
			return expr_value::get_value(
				add_instruction(*this, instructions::cmp_lt_i8{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i16:
			return expr_value::get_value(
				add_instruction(*this, instructions::cmp_lt_i16{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i32:
			return expr_value::get_value(
				add_instruction(*this, instructions::cmp_lt_i32{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i64:
			return expr_value::get_value(
				add_instruction(*this, instructions::cmp_lt_i64{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		default:
			bz_unreachable;
		}
	}
	else
	{
		switch (lhs.get_type()->get_builtin_kind())
		{
		case builtin_type_kind::i8:
			return expr_value::get_value(
				add_instruction(*this, instructions::cmp_lt_u8{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i16:
			return expr_value::get_value(
				add_instruction(*this, instructions::cmp_lt_u16{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i32:
			return expr_value::get_value(
				add_instruction(*this, instructions::cmp_lt_u32{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i64:
			return expr_value::get_value(
				add_instruction(*this, instructions::cmp_lt_u64{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		default:
			bz_unreachable;
		}
	}
}

expr_value codegen_context::create_int_cmp_gt(expr_value lhs, expr_value rhs, bool is_signed)
{
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);

	if (is_signed)
	{
		switch (lhs.get_type()->get_builtin_kind())
		{
		case builtin_type_kind::i8:
			return expr_value::get_value(
				add_instruction(*this, instructions::cmp_gt_i8{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i16:
			return expr_value::get_value(
				add_instruction(*this, instructions::cmp_gt_i16{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i32:
			return expr_value::get_value(
				add_instruction(*this, instructions::cmp_gt_i32{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i64:
			return expr_value::get_value(
				add_instruction(*this, instructions::cmp_gt_i64{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		default:
			bz_unreachable;
		}
	}
	else
	{
		switch (lhs.get_type()->get_builtin_kind())
		{
		case builtin_type_kind::i8:
			return expr_value::get_value(
				add_instruction(*this, instructions::cmp_gt_u8{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i16:
			return expr_value::get_value(
				add_instruction(*this, instructions::cmp_gt_u16{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i32:
			return expr_value::get_value(
				add_instruction(*this, instructions::cmp_gt_u32{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i64:
			return expr_value::get_value(
				add_instruction(*this, instructions::cmp_gt_u64{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		default:
			bz_unreachable;
		}
	}
}

expr_value codegen_context::create_int_cmp_lte(expr_value lhs, expr_value rhs, bool is_signed)
{
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);

	if (is_signed)
	{
		switch (lhs.get_type()->get_builtin_kind())
		{
		case builtin_type_kind::i8:
			return expr_value::get_value(
				add_instruction(*this, instructions::cmp_lte_i8{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i16:
			return expr_value::get_value(
				add_instruction(*this, instructions::cmp_lte_i16{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i32:
			return expr_value::get_value(
				add_instruction(*this, instructions::cmp_lte_i32{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i64:
			return expr_value::get_value(
				add_instruction(*this, instructions::cmp_lte_i64{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		default:
			bz_unreachable;
		}
	}
	else
	{
		switch (lhs.get_type()->get_builtin_kind())
		{
		case builtin_type_kind::i8:
			return expr_value::get_value(
				add_instruction(*this, instructions::cmp_lte_u8{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i16:
			return expr_value::get_value(
				add_instruction(*this, instructions::cmp_lte_u16{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i32:
			return expr_value::get_value(
				add_instruction(*this, instructions::cmp_lte_u32{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i64:
			return expr_value::get_value(
				add_instruction(*this, instructions::cmp_lte_u64{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		default:
			bz_unreachable;
		}
	}
}

expr_value codegen_context::create_int_cmp_gte(expr_value lhs, expr_value rhs, bool is_signed)
{
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);

	if (is_signed)
	{
		switch (lhs.get_type()->get_builtin_kind())
		{
		case builtin_type_kind::i8:
			return expr_value::get_value(
				add_instruction(*this, instructions::cmp_gte_i8{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i16:
			return expr_value::get_value(
				add_instruction(*this, instructions::cmp_gte_i16{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i32:
			return expr_value::get_value(
				add_instruction(*this, instructions::cmp_gte_i32{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i64:
			return expr_value::get_value(
				add_instruction(*this, instructions::cmp_gte_i64{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		default:
			bz_unreachable;
		}
	}
	else
	{
		switch (lhs.get_type()->get_builtin_kind())
		{
		case builtin_type_kind::i8:
			return expr_value::get_value(
				add_instruction(*this, instructions::cmp_gte_u8{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i16:
			return expr_value::get_value(
				add_instruction(*this, instructions::cmp_gte_u16{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i32:
			return expr_value::get_value(
				add_instruction(*this, instructions::cmp_gte_u32{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i64:
			return expr_value::get_value(
				add_instruction(*this, instructions::cmp_gte_u64{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		default:
			bz_unreachable;
		}
	}
}

void codegen_context::create_float_cmp_eq_check(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs)
{
	if (!is_warning_enabled(ctx::warning_kind::nan_compare))
	{
		return;
	}
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		add_instruction(*this,
			instructions::cmp_eq_f32_check{ .src_tokens_index = src_tokens_index },
			lhs_val, rhs_val
		);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this,
			instructions::cmp_eq_f64_check{ .src_tokens_index = src_tokens_index },
			lhs_val, rhs_val
		);
		break;
	default:
		bz_unreachable;
	}
}

void codegen_context::create_float_cmp_neq_check(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs)
{
	if (!is_warning_enabled(ctx::warning_kind::nan_compare))
	{
		return;
	}
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		add_instruction(*this,
			instructions::cmp_neq_f32_check{ .src_tokens_index = src_tokens_index },
			lhs_val, rhs_val
		);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this,
			instructions::cmp_neq_f64_check{ .src_tokens_index = src_tokens_index },
			lhs_val, rhs_val
		);
		break;
	default:
		bz_unreachable;
	}
}

void codegen_context::create_float_cmp_lt_check(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs)
{
	if (!is_warning_enabled(ctx::warning_kind::nan_compare))
	{
		return;
	}
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		add_instruction(*this,
			instructions::cmp_lt_f32_check{ .src_tokens_index = src_tokens_index },
			lhs_val, rhs_val
		);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this,
			instructions::cmp_lt_f64_check{ .src_tokens_index = src_tokens_index },
			lhs_val, rhs_val
		);
		break;
	default:
		bz_unreachable;
	}
}

void codegen_context::create_float_cmp_gt_check(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs)
{
	if (!is_warning_enabled(ctx::warning_kind::nan_compare))
	{
		return;
	}
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		add_instruction(*this,
			instructions::cmp_gt_f32_check{ .src_tokens_index = src_tokens_index },
			lhs_val, rhs_val
		);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this,
			instructions::cmp_gt_f64_check{ .src_tokens_index = src_tokens_index },
			lhs_val, rhs_val
		);
		break;
	default:
		bz_unreachable;
	}
}

void codegen_context::create_float_cmp_lte_check(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs)
{
	if (!is_warning_enabled(ctx::warning_kind::nan_compare))
	{
		return;
	}
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		add_instruction(*this,
			instructions::cmp_lte_f32_check{ .src_tokens_index = src_tokens_index },
			lhs_val, rhs_val
		);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this,
			instructions::cmp_lte_f64_check{ .src_tokens_index = src_tokens_index },
			lhs_val, rhs_val
		);
		break;
	default:
		bz_unreachable;
	}
}

void codegen_context::create_float_cmp_gte_check(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs)
{
	if (!is_warning_enabled(ctx::warning_kind::nan_compare))
	{
		return;
	}
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		add_instruction(*this,
			instructions::cmp_gte_f32_check{ .src_tokens_index = src_tokens_index },
			lhs_val, rhs_val
		);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this,
			instructions::cmp_gte_f64_check{ .src_tokens_index = src_tokens_index },
			lhs_val, rhs_val
		);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_float_cmp_eq(expr_value lhs, expr_value rhs)
{
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::cmp_eq_f32{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::cmp_eq_f64{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_float_cmp_neq(expr_value lhs, expr_value rhs)
{
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::cmp_neq_f32{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::cmp_neq_f64{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_float_cmp_lt(expr_value lhs, expr_value rhs)
{
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::cmp_lt_f32{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::cmp_lt_f64{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_float_cmp_gt(expr_value lhs, expr_value rhs)
{
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::cmp_gt_f32{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::cmp_gt_f64{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_float_cmp_lte(expr_value lhs, expr_value rhs)
{
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::cmp_lte_f32{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::cmp_lte_f64{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_float_cmp_gte(expr_value lhs, expr_value rhs)
{
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::cmp_gte_f32{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::cmp_gte_f64{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_pointer_cmp_eq(expr_value lhs, expr_value rhs)
{
	bz_assert(lhs.get_type()->is_pointer() && rhs.get_type()->is_pointer());
	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);
	return expr_value::get_value(
		add_instruction(*this, instructions::cmp_eq_ptr{}, lhs_val, rhs_val),
		this->get_builtin_type(builtin_type_kind::i1)
	);
}

expr_value codegen_context::create_pointer_cmp_neq(expr_value lhs, expr_value rhs)
{
	bz_assert(lhs.get_type()->is_pointer() && rhs.get_type()->is_pointer());
	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);
	return expr_value::get_value(
		add_instruction(*this, instructions::cmp_neq_ptr{}, lhs_val, rhs_val),
		this->get_builtin_type(builtin_type_kind::i1)
	);
}

expr_value codegen_context::create_pointer_cmp_lt(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs)
{
	bz_assert(lhs.get_type()->is_pointer() && rhs.get_type()->is_pointer());
	auto const src_tokens_index = this->add_src_tokens(src_tokens);
	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);
	return expr_value::get_value(
		add_instruction(*this, instructions::cmp_lt_ptr{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val),
		this->get_builtin_type(builtin_type_kind::i1)
	);
}

expr_value codegen_context::create_pointer_cmp_gt(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs)
{
	bz_assert(lhs.get_type()->is_pointer() && rhs.get_type()->is_pointer());
	auto const src_tokens_index = this->add_src_tokens(src_tokens);
	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);
	return expr_value::get_value(
		add_instruction(*this, instructions::cmp_gt_ptr{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val),
		this->get_builtin_type(builtin_type_kind::i1)
	);
}

expr_value codegen_context::create_pointer_cmp_lte(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs)
{
	bz_assert(lhs.get_type()->is_pointer() && rhs.get_type()->is_pointer());
	auto const src_tokens_index = this->add_src_tokens(src_tokens);
	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);
	return expr_value::get_value(
		add_instruction(*this, instructions::cmp_lte_ptr{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val),
		this->get_builtin_type(builtin_type_kind::i1)
	);
}

expr_value codegen_context::create_pointer_cmp_gte(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs)
{
	bz_assert(lhs.get_type()->is_pointer() && rhs.get_type()->is_pointer());
	auto const src_tokens_index = this->add_src_tokens(src_tokens);
	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);
	return expr_value::get_value(
		add_instruction(*this, instructions::cmp_gte_ptr{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val),
		this->get_builtin_type(builtin_type_kind::i1)
	);
}

expr_value codegen_context::create_neg(expr_value value)
{
	bz_assert(value.get_type()->is_builtin());

	auto const value_ref = value.get_value_as_instruction(*this);

	switch (value.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::i8:
		return expr_value::get_value(
			add_instruction(*this, instructions::neg_i8{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i8)
		);
	case builtin_type_kind::i16:
		return expr_value::get_value(
			add_instruction(*this, instructions::neg_i16{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i16)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			add_instruction(*this, instructions::neg_i32{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i32)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			add_instruction(*this, instructions::neg_i64{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i64)
		);
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::neg_f32{}, value_ref),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::neg_f64{}, value_ref),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

void codegen_context::create_neg_check(lex::src_tokens const &src_tokens, expr_value value)
{
	if (!is_warning_enabled(ctx::warning_kind::int_overflow))
	{
		return;
	}
	bz_assert(value.get_type()->is_builtin());

	auto const value_ref = value.get_value_as_instruction(*this);

	switch (value.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::i8:
		add_instruction(*this, instructions::neg_i8_check{ .src_tokens_index = this->add_src_tokens(src_tokens) }, value_ref);
		break;
	case builtin_type_kind::i16:
		add_instruction(*this, instructions::neg_i16_check{ .src_tokens_index = this->add_src_tokens(src_tokens) }, value_ref);
		break;
	case builtin_type_kind::i32:
		add_instruction(*this, instructions::neg_i32_check{ .src_tokens_index = this->add_src_tokens(src_tokens) }, value_ref);
		break;
	case builtin_type_kind::i64:
		add_instruction(*this, instructions::neg_i64_check{ .src_tokens_index = this->add_src_tokens(src_tokens) }, value_ref);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_add(expr_value lhs, expr_value rhs)
{
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::i8:
		return expr_value::get_value(
			add_instruction(*this, instructions::add_i8{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i8)
		);
	case builtin_type_kind::i16:
		return expr_value::get_value(
			add_instruction(*this, instructions::add_i16{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i16)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			add_instruction(*this, instructions::add_i32{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i32)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			add_instruction(*this, instructions::add_i64{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i64)
		);
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::add_f32{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::add_f64{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

void codegen_context::create_add_check(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs, bool is_signed_int)
{
	if (
		(lhs.get_type()->is_integer_type() && !is_warning_enabled(ctx::warning_kind::int_overflow))
		|| (lhs.get_type()->is_floating_point_type() && !is_warning_enabled(ctx::warning_kind::float_overflow))
	)
	{
		return;
	}
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::i8:
		if (is_signed_int)
		{
			add_instruction(*this, instructions::add_i8_check{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val);
		}
		else
		{
			add_instruction(*this, instructions::add_u8_check{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val);
		}
		break;
	case builtin_type_kind::i16:
		if (is_signed_int)
		{
			add_instruction(*this, instructions::add_i16_check{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val);
		}
		else
		{
			add_instruction(*this, instructions::add_u16_check{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val);
		}
		break;
	case builtin_type_kind::i32:
		if (is_signed_int)
		{
			add_instruction(*this, instructions::add_i32_check{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val);
		}
		else
		{
			add_instruction(*this, instructions::add_u32_check{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val);
		}
		break;
	case builtin_type_kind::i64:
		if (is_signed_int)
		{
			add_instruction(*this, instructions::add_i64_check{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val);
		}
		else
		{
			add_instruction(*this, instructions::add_u64_check{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val);
		}
		break;
	case builtin_type_kind::f32:
		add_instruction(*this, instructions::add_f32_check{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this, instructions::add_f64_check{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_ptr_add_const_unchecked(expr_value address, int32_t amount, type const *object_type)
{
	auto const address_val = address.get_value_as_instruction(*this);
	return expr_value::get_value(
		add_instruction(*this, instructions::add_ptr_const_unchecked{
			.object_type = object_type,
			.amount = amount,
		}, address_val),
		this->get_pointer_type()
	);
}

expr_value codegen_context::create_ptr_add(
	lex::src_tokens const &src_tokens,
	expr_value address,
	expr_value offset,
	bool is_offset_signed,
	type const *object_type,
	ast::typespec_view pointer_type
)
{
	bz_assert(address.get_type()->is_pointer());
	bz_assert(offset.get_type()->is_integer_type());

	auto const src_tokens_index = this->add_src_tokens(src_tokens);
	auto const pointer_arithmetic_check_info_index = this->add_pointer_arithmetic_check_info({
		.object_type = object_type,
		.pointer_type = pointer_type,
	});

	auto const address_val = address.get_value_as_instruction(*this);

	if (this->is_64_bit())
	{
		auto const intptr_type = this->get_builtin_type(builtin_type_kind::i64);
		auto const offset_val = this->create_int_cast(offset, intptr_type, is_offset_signed).get_value_as_instruction(*this);
		if (is_offset_signed)
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::add_ptr_i64{
					.src_tokens_index = src_tokens_index,
					.pointer_arithmetic_check_info_index = pointer_arithmetic_check_info_index,
				}, address_val, offset_val),
				this->get_pointer_type()
			);
		}
		else
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::add_ptr_u64{
					.src_tokens_index = src_tokens_index,
					.pointer_arithmetic_check_info_index = pointer_arithmetic_check_info_index,
				}, address_val, offset_val),
				this->get_pointer_type()
			);
		}
	}
	else
	{
		auto const intptr_type = this->get_builtin_type(builtin_type_kind::i32);
		auto const offset_val = this->create_int_cast(offset, intptr_type, is_offset_signed).get_value_as_instruction(*this);
		if (is_offset_signed)
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::add_ptr_i32{
					.src_tokens_index = src_tokens_index,
					.pointer_arithmetic_check_info_index = pointer_arithmetic_check_info_index,
				}, address_val, offset_val),
				this->get_pointer_type()
			);
		}
		else
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::add_ptr_u32{
					.src_tokens_index = src_tokens_index,
					.pointer_arithmetic_check_info_index = pointer_arithmetic_check_info_index,
				}, address_val, offset_val),
				this->get_pointer_type()
			);
		}
	}
}

expr_value codegen_context::create_sub(expr_value lhs, expr_value rhs)
{
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::i8:
		return expr_value::get_value(
			add_instruction(*this, instructions::sub_i8{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i8)
		);
	case builtin_type_kind::i16:
		return expr_value::get_value(
			add_instruction(*this, instructions::sub_i16{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i16)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			add_instruction(*this, instructions::sub_i32{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i32)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			add_instruction(*this, instructions::sub_i64{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i64)
		);
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::sub_f32{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::sub_f64{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

void codegen_context::create_sub_check(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs, bool is_signed_int)
{
	if (
		(lhs.get_type()->is_integer_type() && !is_warning_enabled(ctx::warning_kind::int_overflow))
		|| (lhs.get_type()->is_floating_point_type() && !is_warning_enabled(ctx::warning_kind::float_overflow))
	)
	{
		return;
	}
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::i8:
		if (is_signed_int)
		{
			add_instruction(*this, instructions::sub_i8_check{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val);
		}
		else
		{
			add_instruction(*this, instructions::sub_u8_check{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val);
		}
		break;
	case builtin_type_kind::i16:
		if (is_signed_int)
		{
			add_instruction(*this, instructions::sub_i16_check{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val);
		}
		else
		{
			add_instruction(*this, instructions::sub_u16_check{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val);
		}
		break;
	case builtin_type_kind::i32:
		if (is_signed_int)
		{
			add_instruction(*this, instructions::sub_i32_check{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val);
		}
		else
		{
			add_instruction(*this, instructions::sub_u32_check{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val);
		}
		break;
	case builtin_type_kind::i64:
		if (is_signed_int)
		{
			add_instruction(*this, instructions::sub_i64_check{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val);
		}
		else
		{
			add_instruction(*this, instructions::sub_u64_check{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val);
		}
		break;
	case builtin_type_kind::f32:
		add_instruction(*this, instructions::sub_f32_check{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this, instructions::sub_f64_check{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_ptr_sub(
	lex::src_tokens const &src_tokens,
	expr_value address,
	expr_value offset,
	bool is_offset_signed,
	type const *object_type,
	ast::typespec_view pointer_type
)
{
	bz_assert(address.get_type()->is_pointer());
	bz_assert(offset.get_type()->is_integer_type());

	auto const src_tokens_index = this->add_src_tokens(src_tokens);
	auto const pointer_arithmetic_check_info_index = this->add_pointer_arithmetic_check_info({
		.object_type = object_type,
		.pointer_type = pointer_type,
	});

	auto const address_val = address.get_value_as_instruction(*this);

	if (this->is_64_bit())
	{
		auto const intptr_type = this->get_builtin_type(builtin_type_kind::i64);
		auto const offset_val = this->create_int_cast(offset, intptr_type, is_offset_signed).get_value_as_instruction(*this);
		if (is_offset_signed)
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::sub_ptr_i64{
					.src_tokens_index = src_tokens_index,
					.pointer_arithmetic_check_info_index = pointer_arithmetic_check_info_index,
				}, address_val, offset_val),
				this->get_pointer_type()
			);
		}
		else
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::sub_ptr_u64{
					.src_tokens_index = src_tokens_index,
					.pointer_arithmetic_check_info_index = pointer_arithmetic_check_info_index,
				}, address_val, offset_val),
				this->get_pointer_type()
			);
		}
	}
	else
	{
		auto const intptr_type = this->get_builtin_type(builtin_type_kind::i32);
		auto const offset_val = this->create_int_cast(offset, intptr_type, is_offset_signed).get_value_as_instruction(*this);
		if (is_offset_signed)
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::sub_ptr_i32{
					.src_tokens_index = src_tokens_index,
					.pointer_arithmetic_check_info_index = pointer_arithmetic_check_info_index,
				}, address_val, offset_val),
				this->get_pointer_type()
			);
		}
		else
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::sub_ptr_u32{
					.src_tokens_index = src_tokens_index,
					.pointer_arithmetic_check_info_index = pointer_arithmetic_check_info_index,
				}, address_val, offset_val),
				this->get_pointer_type()
			);
		}
	}
}

expr_value codegen_context::create_ptrdiff(
	lex::src_tokens const &src_tokens,
	expr_value lhs,
	expr_value rhs,
	type const *object_type,
	ast::typespec_view pointer_type
)
{
	bz_assert(lhs.get_type()->is_pointer());
	bz_assert(rhs.get_type()->is_pointer());

	auto const src_tokens_index = this->add_src_tokens(src_tokens);
	auto const pointer_arithmetic_check_info_index = this->add_pointer_arithmetic_check_info({
		.object_type = object_type,
		.pointer_type = pointer_type,
	});

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);

	if (this->is_64_bit())
	{
		return expr_value::get_value(
			add_instruction(*this, instructions::ptr64_diff{
				.src_tokens_index = src_tokens_index,
				.pointer_arithmetic_check_info_index = pointer_arithmetic_check_info_index,
			}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i64)
		);
	}
	else
	{
		return expr_value::get_value(
			add_instruction(*this, instructions::ptr32_diff{
				.src_tokens_index = src_tokens_index,
				.pointer_arithmetic_check_info_index = pointer_arithmetic_check_info_index,
			}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i32)
		);
	}
}

expr_value codegen_context::create_ptrdiff_unchecked(expr_value lhs, expr_value rhs, type const *object_type)
{
	bz_assert(lhs.get_type()->is_pointer());
	bz_assert(rhs.get_type()->is_pointer());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);

	if (this->is_64_bit())
	{
		return expr_value::get_value(
			add_instruction(*this, instructions::ptr64_diff_unchecked{ .stride = object_type->size }, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i64)
		);
	}
	else
	{
		return expr_value::get_value(
			add_instruction(*this, instructions::ptr32_diff_unchecked{ .stride = object_type->size }, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i32)
		);
	}
}

expr_value codegen_context::create_mul(expr_value lhs, expr_value rhs)
{
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::i8:
		return expr_value::get_value(
			add_instruction(*this, instructions::mul_i8{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i8)
		);
	case builtin_type_kind::i16:
		return expr_value::get_value(
			add_instruction(*this, instructions::mul_i16{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i16)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			add_instruction(*this, instructions::mul_i32{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i32)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			add_instruction(*this, instructions::mul_i64{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i64)
		);
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::mul_f32{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::mul_f64{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

void codegen_context::create_mul_check(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs, bool is_signed_int)
{
	if (
		(lhs.get_type()->is_integer_type() && !is_warning_enabled(ctx::warning_kind::int_overflow))
		|| (lhs.get_type()->is_floating_point_type() && !is_warning_enabled(ctx::warning_kind::float_overflow))
	)
	{
		return;
	}
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::i8:
		if (is_signed_int)
		{
			add_instruction(*this, instructions::mul_i8_check{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val);
		}
		else
		{
			add_instruction(*this, instructions::mul_u8_check{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val);
		}
		break;
	case builtin_type_kind::i16:
		if (is_signed_int)
		{
			add_instruction(*this, instructions::mul_i16_check{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val);
		}
		else
		{
			add_instruction(*this, instructions::mul_u16_check{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val);
		}
		break;
	case builtin_type_kind::i32:
		if (is_signed_int)
		{
			add_instruction(*this, instructions::mul_i32_check{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val);
		}
		else
		{
			add_instruction(*this, instructions::mul_u32_check{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val);
		}
		break;
	case builtin_type_kind::i64:
		if (is_signed_int)
		{
			add_instruction(*this, instructions::mul_i64_check{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val);
		}
		else
		{
			add_instruction(*this, instructions::mul_u64_check{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val);
		}
		break;
	case builtin_type_kind::f32:
		add_instruction(*this, instructions::mul_f32_check{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this, instructions::mul_f64_check{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_div(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs, bool is_signed_int)
{
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::i8:
		if (is_signed_int)
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::div_i8{ .src_tokens_index = this->add_src_tokens(src_tokens) }, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i8)
			);
		}
		else
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::div_u8{ .src_tokens_index = this->add_src_tokens(src_tokens) }, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i8)
			);
		}
	case builtin_type_kind::i16:
		if (is_signed_int)
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::div_i16{ .src_tokens_index = this->add_src_tokens(src_tokens) }, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i16)
			);
		}
		else
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::div_u16{ .src_tokens_index = this->add_src_tokens(src_tokens) }, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i16)
			);
		}
	case builtin_type_kind::i32:
		if (is_signed_int)
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::div_i32{ .src_tokens_index = this->add_src_tokens(src_tokens) }, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i32)
			);
		}
		else
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::div_u32{ .src_tokens_index = this->add_src_tokens(src_tokens) }, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i32)
			);
		}
	case builtin_type_kind::i64:
		if (is_signed_int)
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::div_i64{ .src_tokens_index = this->add_src_tokens(src_tokens) }, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i64)
			);
		}
		else
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::div_u64{ .src_tokens_index = this->add_src_tokens(src_tokens) }, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i64)
			);
		}
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::div_f32{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::div_f64{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

void codegen_context::create_div_check(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs, bool is_signed_int)
{
	if (
		(lhs.get_type()->is_integer_type() && (!is_signed_int || !is_warning_enabled(ctx::warning_kind::int_overflow)))
		|| (lhs.get_type()->is_floating_point_type() && !is_warning_enabled(ctx::warning_kind::float_overflow))
	)
	{
		return;
	}
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::i8:
		add_instruction(*this, instructions::div_i8_check{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val);
		break;
	case builtin_type_kind::i16:
		add_instruction(*this, instructions::div_i16_check{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val);
		break;
	case builtin_type_kind::i32:
		add_instruction(*this, instructions::div_i32_check{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val);
		break;
	case builtin_type_kind::i64:
		add_instruction(*this, instructions::div_i64_check{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val);
		break;
	case builtin_type_kind::f32:
		add_instruction(*this, instructions::div_f32_check{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this, instructions::div_f64_check{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_rem(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs, bool is_signed_int)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::i8:
		if (is_signed_int)
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::rem_i8{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i8)
			);
		}
		else
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::rem_u8{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i8)
			);
		}
	case builtin_type_kind::i16:
		if (is_signed_int)
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::rem_i16{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i16)
			);
		}
		else
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::rem_u16{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i16)
			);
		}
	case builtin_type_kind::i32:
		if (is_signed_int)
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::rem_i32{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i32)
			);
		}
		else
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::rem_u32{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i32)
			);
		}
	case builtin_type_kind::i64:
		if (is_signed_int)
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::rem_i64{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i64)
			);
		}
		else
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::rem_u64{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i64)
			);
		}
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_not(expr_value value)
{
	bz_assert(value.get_type()->is_builtin());

	auto const value_ref = value.get_value_as_instruction(*this);

	switch (value.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::i1:
		return expr_value::get_value(
			add_instruction(*this, instructions::not_i1{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::i8:
		return expr_value::get_value(
			add_instruction(*this, instructions::not_i8{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i8)
		);
	case builtin_type_kind::i16:
		return expr_value::get_value(
			add_instruction(*this, instructions::not_i16{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i16)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			add_instruction(*this, instructions::not_i32{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i32)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			add_instruction(*this, instructions::not_i64{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i64)
		);
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_and(expr_value lhs, expr_value rhs)
{
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::i1:
		return expr_value::get_value(
			add_instruction(*this, instructions::and_i1{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::i8:
		return expr_value::get_value(
			add_instruction(*this, instructions::and_i8{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i8)
		);
	case builtin_type_kind::i16:
		return expr_value::get_value(
			add_instruction(*this, instructions::and_i16{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i16)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			add_instruction(*this, instructions::and_i32{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i32)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			add_instruction(*this, instructions::and_i64{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i64)
		);
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_xor(expr_value lhs, expr_value rhs)
{
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::i1:
		return expr_value::get_value(
			add_instruction(*this, instructions::xor_i1{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::i8:
		return expr_value::get_value(
			add_instruction(*this, instructions::xor_i8{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i8)
		);
	case builtin_type_kind::i16:
		return expr_value::get_value(
			add_instruction(*this, instructions::xor_i16{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i16)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			add_instruction(*this, instructions::xor_i32{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i32)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			add_instruction(*this, instructions::xor_i64{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i64)
		);
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_or(expr_value lhs, expr_value rhs)
{
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::i1:
		return expr_value::get_value(
			add_instruction(*this, instructions::or_i1{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::i8:
		return expr_value::get_value(
			add_instruction(*this, instructions::or_i8{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i8)
		);
	case builtin_type_kind::i16:
		return expr_value::get_value(
			add_instruction(*this, instructions::or_i16{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i16)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			add_instruction(*this, instructions::or_i32{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i32)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			add_instruction(*this, instructions::or_i64{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i64)
		);
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_shl(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs, bool is_rhs_signed)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_cast = this->create_int_cast(rhs, this->get_builtin_type(builtin_type_kind::i64), is_rhs_signed);
	auto const rhs_val = rhs_cast.get_value_as_instruction(*this);

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::i8:
		if (is_rhs_signed)
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::shl_i8_signed{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i8)
			);
		}
		else
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::shl_i8_unsigned{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i8)
			);
		}
	case builtin_type_kind::i16:
		if (is_rhs_signed)
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::shl_i16_signed{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i16)
			);
		}
		else
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::shl_i16_unsigned{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i16)
			);
		}
	case builtin_type_kind::i32:
		if (is_rhs_signed)
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::shl_i32_signed{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i32)
			);
		}
		else
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::shl_i32_unsigned{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i32)
			);
		}
	case builtin_type_kind::i64:
		if (is_rhs_signed)
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::shl_i64_signed{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i64)
			);
		}
		else
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::shl_i64_unsigned{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i64)
			);
		}
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_shr(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs, bool is_rhs_signed)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_cast = this->create_int_cast(rhs, this->get_builtin_type(builtin_type_kind::i64), is_rhs_signed);
	auto const rhs_val = rhs_cast.get_value_as_instruction(*this);

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::i8:
		if (is_rhs_signed)
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::shr_i8_signed{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i8)
			);
		}
		else
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::shr_i8_unsigned{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i8)
			);
		}
	case builtin_type_kind::i16:
		if (is_rhs_signed)
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::shr_i16_signed{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i16)
			);
		}
		else
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::shr_i16_unsigned{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i16)
			);
		}
	case builtin_type_kind::i32:
		if (is_rhs_signed)
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::shr_i32_signed{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i32)
			);
		}
		else
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::shr_i32_unsigned{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i32)
			);
		}
	case builtin_type_kind::i64:
		if (is_rhs_signed)
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::shr_i64_signed{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i64)
			);
		}
		else
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::shr_i64_unsigned{ .src_tokens_index = src_tokens_index }, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i64)
			);
		}
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_abs(expr_value value)
{
	auto const value_ref = value.get_value_as_instruction(*this);

	bz_assert(value.get_type()->is_builtin());
	switch (value.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::i8:
		return expr_value::get_value(
			add_instruction(*this, instructions::abs_i8{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i8)
		);
	case builtin_type_kind::i16:
		return expr_value::get_value(
			add_instruction(*this, instructions::abs_i16{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i16)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			add_instruction(*this, instructions::abs_i32{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i32)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			add_instruction(*this, instructions::abs_i64{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i64)
		);
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::abs_f32{}, value_ref),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::abs_f64{}, value_ref),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

void codegen_context::create_abs_check(lex::src_tokens const &src_tokens, expr_value value)
{
	if (
		(value.get_type()->is_integer_type() && !is_warning_enabled(ctx::warning_kind::int_overflow))
		|| (value.get_type()->is_floating_point_type() && !is_warning_enabled(ctx::warning_kind::math_domain_error))
	)
	{
		return;
	}
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	auto const value_ref = value.get_value_as_instruction(*this);

	bz_assert(value.get_type()->is_builtin());
	switch (value.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::i8:
		add_instruction(*this, instructions::abs_i8_check{ .src_tokens_index = src_tokens_index }, value_ref);
		break;
	case builtin_type_kind::i16:
		add_instruction(*this, instructions::abs_i16_check{ .src_tokens_index = src_tokens_index }, value_ref);
		break;
	case builtin_type_kind::i32:
		add_instruction(*this, instructions::abs_i32_check{ .src_tokens_index = src_tokens_index }, value_ref);
		break;
	case builtin_type_kind::i64:
		add_instruction(*this, instructions::abs_i64_check{ .src_tokens_index = src_tokens_index }, value_ref);
		break;
	case builtin_type_kind::f32:
		add_instruction(*this, instructions::abs_f32_check{ .src_tokens_index = src_tokens_index }, value_ref);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this, instructions::abs_f64_check{ .src_tokens_index = src_tokens_index }, value_ref);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_min(expr_value a, expr_value b, bool is_signed_int)
{
	bz_assert(a.get_type()->is_builtin());
	bz_assert(b.get_type()->is_builtin());
	bz_assert(a.get_type()->get_builtin_kind() == b.get_type()->get_builtin_kind());

	auto const a_val = a.get_value_as_instruction(*this);
	auto const b_val = b.get_value_as_instruction(*this);

	switch (a.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::i8:
		if (is_signed_int)
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::min_i8{}, a_val, b_val),
				this->get_builtin_type(builtin_type_kind::i8)
			);
		}
		else
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::min_u8{}, a_val, b_val),
				this->get_builtin_type(builtin_type_kind::i8)
			);
		}
	case builtin_type_kind::i16:
		if (is_signed_int)
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::min_i16{}, a_val, b_val),
				this->get_builtin_type(builtin_type_kind::i16)
			);
		}
		else
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::min_u16{}, a_val, b_val),
				this->get_builtin_type(builtin_type_kind::i16)
			);
		}
	case builtin_type_kind::i32:
		if (is_signed_int)
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::min_i32{}, a_val, b_val),
				this->get_builtin_type(builtin_type_kind::i32)
			);
		}
		else
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::min_u32{}, a_val, b_val),
				this->get_builtin_type(builtin_type_kind::i32)
			);
		}
	case builtin_type_kind::i64:
		if (is_signed_int)
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::min_i64{}, a_val, b_val),
				this->get_builtin_type(builtin_type_kind::i64)
			);
		}
		else
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::min_u64{}, a_val, b_val),
				this->get_builtin_type(builtin_type_kind::i64)
			);
		}
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::min_f32{}, a_val, b_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::min_f64{}, a_val, b_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

void codegen_context::create_min_check(lex::src_tokens const &src_tokens, expr_value x, expr_value y)
{
	if (!x.get_type()->is_floating_point_type() || !is_warning_enabled(ctx::warning_kind::math_domain_error))
	{
		return;
	}
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());
	bz_assert(y.get_type()->is_builtin());
	bz_assert(x.get_type()->get_builtin_kind() == y.get_type()->get_builtin_kind());

	auto const x_val = x.get_value_as_instruction(*this);
	auto const y_val = y.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		add_instruction(*this, instructions::min_f32_check{ .src_tokens_index = src_tokens_index }, x_val, y_val);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this, instructions::min_f64_check{ .src_tokens_index = src_tokens_index }, x_val, y_val);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_max(expr_value a, expr_value b, bool is_signed_int)
{
	bz_assert(a.get_type()->is_builtin());
	bz_assert(b.get_type()->is_builtin());
	bz_assert(a.get_type()->get_builtin_kind() == b.get_type()->get_builtin_kind());

	auto const a_val = a.get_value_as_instruction(*this);
	auto const b_val = b.get_value_as_instruction(*this);

	switch (a.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::i8:
		if (is_signed_int)
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::max_i8{}, a_val, b_val),
				this->get_builtin_type(builtin_type_kind::i8)
			);
		}
		else
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::max_u8{}, a_val, b_val),
				this->get_builtin_type(builtin_type_kind::i8)
			);
		}
	case builtin_type_kind::i16:
		if (is_signed_int)
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::max_i16{}, a_val, b_val),
				this->get_builtin_type(builtin_type_kind::i16)
			);
		}
		else
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::max_u16{}, a_val, b_val),
				this->get_builtin_type(builtin_type_kind::i16)
			);
		}
	case builtin_type_kind::i32:
		if (is_signed_int)
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::max_i32{}, a_val, b_val),
				this->get_builtin_type(builtin_type_kind::i32)
			);
		}
		else
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::max_u32{}, a_val, b_val),
				this->get_builtin_type(builtin_type_kind::i32)
			);
		}
	case builtin_type_kind::i64:
		if (is_signed_int)
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::max_i64{}, a_val, b_val),
				this->get_builtin_type(builtin_type_kind::i64)
			);
		}
		else
		{
			return expr_value::get_value(
				add_instruction(*this, instructions::max_u64{}, a_val, b_val),
				this->get_builtin_type(builtin_type_kind::i64)
			);
		}
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::max_f32{}, a_val, b_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::max_f64{}, a_val, b_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

void codegen_context::create_max_check(lex::src_tokens const &src_tokens, expr_value x, expr_value y)
{
	if (!x.get_type()->is_floating_point_type() || !is_warning_enabled(ctx::warning_kind::math_domain_error))
	{
		return;
	}
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());
	bz_assert(y.get_type()->is_builtin());
	bz_assert(x.get_type()->get_builtin_kind() == y.get_type()->get_builtin_kind());

	auto const x_val = x.get_value_as_instruction(*this);
	auto const y_val = y.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		add_instruction(*this, instructions::max_f32_check{ .src_tokens_index = src_tokens_index }, x_val, y_val);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this, instructions::max_f64_check{ .src_tokens_index = src_tokens_index }, x_val, y_val);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_exp(expr_value x)
{
	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::exp_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::exp_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

void codegen_context::create_exp_check(lex::src_tokens const &src_tokens, expr_value x)
{
	if (!is_warning_enabled(ctx::warning_kind::math_domain_error))
	{
		return;
	}
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		add_instruction(*this, instructions::exp_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this, instructions::exp_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_exp2(expr_value x)
{
	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::exp2_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::exp2_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

void codegen_context::create_exp2_check(lex::src_tokens const &src_tokens, expr_value x)
{
	if (!is_warning_enabled(ctx::warning_kind::math_domain_error))
	{
		return;
	}
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		add_instruction(*this, instructions::exp2_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this, instructions::exp2_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_expm1(expr_value x)
{
	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::expm1_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::expm1_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

void codegen_context::create_expm1_check(lex::src_tokens const &src_tokens, expr_value x)
{
	if (!is_warning_enabled(ctx::warning_kind::math_domain_error))
	{
		return;
	}
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		add_instruction(*this, instructions::expm1_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this, instructions::expm1_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_log(expr_value x)
{
	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::log_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::log_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

void codegen_context::create_log_check(lex::src_tokens const &src_tokens, expr_value x)
{
	if (!is_warning_enabled(ctx::warning_kind::math_domain_error))
	{
		return;
	}
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		add_instruction(*this, instructions::log_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this, instructions::log_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_log10(expr_value x)
{
	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::log10_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::log10_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

void codegen_context::create_log10_check(lex::src_tokens const &src_tokens, expr_value x)
{
	if (!is_warning_enabled(ctx::warning_kind::math_domain_error))
	{
		return;
	}
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		add_instruction(*this, instructions::log10_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this, instructions::log10_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_log2(expr_value x)
{
	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::log2_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::log2_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

void codegen_context::create_log2_check(lex::src_tokens const &src_tokens, expr_value x)
{
	if (!is_warning_enabled(ctx::warning_kind::math_domain_error))
	{
		return;
	}
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		add_instruction(*this, instructions::log2_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this, instructions::log2_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_log1p(expr_value x)
{
	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::log1p_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::log1p_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

void codegen_context::create_log1p_check(lex::src_tokens const &src_tokens, expr_value x)
{
	if (!is_warning_enabled(ctx::warning_kind::math_domain_error))
	{
		return;
	}
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		add_instruction(*this, instructions::log1p_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this, instructions::log1p_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_sqrt(expr_value x)
{
	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::sqrt_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::sqrt_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

void codegen_context::create_sqrt_check(lex::src_tokens const &src_tokens, expr_value x)
{
	if (!is_warning_enabled(ctx::warning_kind::math_domain_error))
	{
		return;
	}
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		add_instruction(*this, instructions::sqrt_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this, instructions::sqrt_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_pow(expr_value x, expr_value y)
{
	bz_assert(x.get_type()->is_builtin());
	bz_assert(y.get_type()->is_builtin());
	bz_assert(x.get_type()->get_builtin_kind() == y.get_type()->get_builtin_kind());

	auto const x_val = x.get_value_as_instruction(*this);
	auto const y_val = y.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::pow_f32{}, x_val, y_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::pow_f64{}, x_val, y_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

void codegen_context::create_pow_check(lex::src_tokens const &src_tokens, expr_value x, expr_value y)
{
	if (!is_warning_enabled(ctx::warning_kind::math_domain_error))
	{
		return;
	}
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());
	bz_assert(y.get_type()->is_builtin());
	bz_assert(x.get_type()->get_builtin_kind() == y.get_type()->get_builtin_kind());

	auto const x_val = x.get_value_as_instruction(*this);
	auto const y_val = y.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		add_instruction(*this, instructions::pow_f32_check{ .src_tokens_index = src_tokens_index }, x_val, y_val);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this, instructions::pow_f64_check{ .src_tokens_index = src_tokens_index }, x_val, y_val);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_cbrt(expr_value x)
{
	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::cbrt_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::cbrt_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

void codegen_context::create_cbrt_check(lex::src_tokens const &src_tokens, expr_value x)
{
	if (!is_warning_enabled(ctx::warning_kind::math_domain_error))
	{
		return;
	}
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		add_instruction(*this, instructions::cbrt_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this, instructions::cbrt_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_hypot(expr_value x, expr_value y)
{
	bz_assert(x.get_type()->is_builtin());
	bz_assert(y.get_type()->is_builtin());
	bz_assert(x.get_type()->get_builtin_kind() == y.get_type()->get_builtin_kind());

	auto const x_val = x.get_value_as_instruction(*this);
	auto const y_val = y.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::hypot_f32{}, x_val, y_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::hypot_f64{}, x_val, y_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

void codegen_context::create_hypot_check(lex::src_tokens const &src_tokens, expr_value x, expr_value y)
{
	if (!is_warning_enabled(ctx::warning_kind::math_domain_error))
	{
		return;
	}
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());
	bz_assert(y.get_type()->is_builtin());
	bz_assert(x.get_type()->get_builtin_kind() == y.get_type()->get_builtin_kind());

	auto const x_val = x.get_value_as_instruction(*this);
	auto const y_val = y.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		add_instruction(*this, instructions::hypot_f32_check{ .src_tokens_index = src_tokens_index }, x_val, y_val);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this, instructions::hypot_f64_check{ .src_tokens_index = src_tokens_index }, x_val, y_val);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_sin(expr_value x)
{
	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::sin_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::sin_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

void codegen_context::create_sin_check(lex::src_tokens const &src_tokens, expr_value x)
{
	if (!is_warning_enabled(ctx::warning_kind::math_domain_error))
	{
		return;
	}
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		add_instruction(*this, instructions::sin_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this, instructions::sin_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_cos(expr_value x)
{
	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::cos_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::cos_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

void codegen_context::create_cos_check(lex::src_tokens const &src_tokens, expr_value x)
{
	if (!is_warning_enabled(ctx::warning_kind::math_domain_error))
	{
		return;
	}
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		add_instruction(*this, instructions::cos_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this, instructions::cos_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_tan(expr_value x)
{
	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::tan_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::tan_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

void codegen_context::create_tan_check(lex::src_tokens const &src_tokens, expr_value x)
{
	if (!is_warning_enabled(ctx::warning_kind::math_domain_error))
	{
		return;
	}
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		add_instruction(*this, instructions::tan_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this, instructions::tan_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_asin(expr_value x)
{
	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::asin_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::asin_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

void codegen_context::create_asin_check(lex::src_tokens const &src_tokens, expr_value x)
{
	if (!is_warning_enabled(ctx::warning_kind::math_domain_error))
	{
		return;
	}
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		add_instruction(*this, instructions::asin_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this, instructions::asin_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_acos(expr_value x)
{
	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::acos_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::acos_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

void codegen_context::create_acos_check(lex::src_tokens const &src_tokens, expr_value x)
{
	if (!is_warning_enabled(ctx::warning_kind::math_domain_error))
	{
		return;
	}
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		add_instruction(*this, instructions::acos_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this, instructions::acos_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_atan(expr_value x)
{
	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::atan_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::atan_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

void codegen_context::create_atan_check(lex::src_tokens const &src_tokens, expr_value x)
{
	if (!is_warning_enabled(ctx::warning_kind::math_domain_error))
	{
		return;
	}
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		add_instruction(*this, instructions::atan_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this, instructions::atan_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_atan2(expr_value y, expr_value x)
{
	bz_assert(y.get_type()->is_builtin());
	bz_assert(x.get_type()->is_builtin());
	bz_assert(y.get_type()->get_builtin_kind() == x.get_type()->get_builtin_kind());

	auto const y_val = y.get_value_as_instruction(*this);
	auto const x_val = x.get_value_as_instruction(*this);

	switch (y.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::atan2_f32{}, y_val, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::atan2_f64{}, y_val, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

void codegen_context::create_atan2_check(lex::src_tokens const &src_tokens, expr_value y, expr_value x)
{
	if (!is_warning_enabled(ctx::warning_kind::math_domain_error))
	{
		return;
	}
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(y.get_type()->is_builtin());
	bz_assert(x.get_type()->is_builtin());
	bz_assert(y.get_type()->get_builtin_kind() == x.get_type()->get_builtin_kind());

	auto const y_val = y.get_value_as_instruction(*this);
	auto const x_val = x.get_value_as_instruction(*this);

	switch (y.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		add_instruction(*this, instructions::atan2_f32_check{ .src_tokens_index = src_tokens_index }, y_val, x_val);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this, instructions::atan2_f64_check{ .src_tokens_index = src_tokens_index }, y_val, x_val);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_sinh(expr_value x)
{
	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::sinh_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::sinh_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

void codegen_context::create_sinh_check(lex::src_tokens const &src_tokens, expr_value x)
{
	if (!is_warning_enabled(ctx::warning_kind::math_domain_error))
	{
		return;
	}
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		add_instruction(*this, instructions::sinh_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this, instructions::sinh_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_cosh(expr_value x)
{
	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::cosh_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::cosh_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

void codegen_context::create_cosh_check(lex::src_tokens const &src_tokens, expr_value x)
{
	if (!is_warning_enabled(ctx::warning_kind::math_domain_error))
	{
		return;
	}
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		add_instruction(*this, instructions::cosh_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this, instructions::cosh_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_tanh(expr_value x)
{
	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::tanh_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::tanh_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

void codegen_context::create_tanh_check(lex::src_tokens const &src_tokens, expr_value x)
{
	if (!is_warning_enabled(ctx::warning_kind::math_domain_error))
	{
		return;
	}
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		add_instruction(*this, instructions::tanh_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this, instructions::tanh_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_asinh(expr_value x)
{
	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::asinh_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::asinh_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

void codegen_context::create_asinh_check(lex::src_tokens const &src_tokens, expr_value x)
{
	if (!is_warning_enabled(ctx::warning_kind::math_domain_error))
	{
		return;
	}
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		add_instruction(*this, instructions::asinh_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this, instructions::asinh_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_acosh(expr_value x)
{
	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::acosh_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::acosh_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

void codegen_context::create_acosh_check(lex::src_tokens const &src_tokens, expr_value x)
{
	if (!is_warning_enabled(ctx::warning_kind::math_domain_error))
	{
		return;
	}
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		add_instruction(*this, instructions::acosh_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this, instructions::acosh_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_atanh(expr_value x)
{
	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::atanh_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::atanh_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

void codegen_context::create_atanh_check(lex::src_tokens const &src_tokens, expr_value x)
{
	if (!is_warning_enabled(ctx::warning_kind::math_domain_error))
	{
		return;
	}
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		add_instruction(*this, instructions::atanh_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this, instructions::atanh_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_erf(expr_value x)
{
	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::erf_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::erf_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

void codegen_context::create_erf_check(lex::src_tokens const &src_tokens, expr_value x)
{
	if (!is_warning_enabled(ctx::warning_kind::math_domain_error))
	{
		return;
	}
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		add_instruction(*this, instructions::erf_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this, instructions::erf_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_erfc(expr_value x)
{
	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::erfc_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::erfc_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

void codegen_context::create_erfc_check(lex::src_tokens const &src_tokens, expr_value x)
{
	if (!is_warning_enabled(ctx::warning_kind::math_domain_error))
	{
		return;
	}
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		add_instruction(*this, instructions::erfc_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this, instructions::erfc_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_tgamma(expr_value x)
{
	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::tgamma_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::tgamma_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

void codegen_context::create_tgamma_check(lex::src_tokens const &src_tokens, expr_value x)
{
	if (!is_warning_enabled(ctx::warning_kind::math_domain_error))
	{
		return;
	}
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		add_instruction(*this, instructions::tgamma_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this, instructions::tgamma_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_lgamma(expr_value x)
{
	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			add_instruction(*this, instructions::lgamma_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			add_instruction(*this, instructions::lgamma_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

void codegen_context::create_lgamma_check(lex::src_tokens const &src_tokens, expr_value x)
{
	if (!is_warning_enabled(ctx::warning_kind::math_domain_error))
	{
		return;
	}
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		add_instruction(*this, instructions::lgamma_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	case builtin_type_kind::f64:
		add_instruction(*this, instructions::lgamma_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
		break;
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_bitreverse(expr_value value)
{
	bz_assert(value.get_type()->is_builtin());

	auto const value_ref = value.get_value_as_instruction(*this);

	switch (value.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::i8:
		return expr_value::get_value(
			add_instruction(*this, instructions::bitreverse_u8{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i8)
		);
	case builtin_type_kind::i16:
		return expr_value::get_value(
			add_instruction(*this, instructions::bitreverse_u16{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i16)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			add_instruction(*this, instructions::bitreverse_u32{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i32)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			add_instruction(*this, instructions::bitreverse_u64{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i64)
		);
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_popcount(expr_value value)
{
	bz_assert(value.get_type()->is_builtin());

	auto const value_ref = value.get_value_as_instruction(*this);

	switch (value.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::i8:
		return expr_value::get_value(
			add_instruction(*this, instructions::popcount_u8{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i8)
		);
	case builtin_type_kind::i16:
		return expr_value::get_value(
			add_instruction(*this, instructions::popcount_u16{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i16)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			add_instruction(*this, instructions::popcount_u32{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i32)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			add_instruction(*this, instructions::popcount_u64{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i64)
		);
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_byteswap(expr_value value)
{
	bz_assert(value.get_type()->is_builtin());

	auto const value_ref = value.get_value_as_instruction(*this);

	switch (value.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::i16:
		return expr_value::get_value(
			add_instruction(*this, instructions::byteswap_u16{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i16)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			add_instruction(*this, instructions::byteswap_u32{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i32)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			add_instruction(*this, instructions::byteswap_u64{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i64)
		);
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_clz(expr_value value)
{
	bz_assert(value.get_type()->is_builtin());

	auto const value_ref = value.get_value_as_instruction(*this);

	switch (value.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::i8:
		return expr_value::get_value(
			add_instruction(*this, instructions::clz_u8{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i8)
		);
	case builtin_type_kind::i16:
		return expr_value::get_value(
			add_instruction(*this, instructions::clz_u16{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i16)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			add_instruction(*this, instructions::clz_u32{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i32)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			add_instruction(*this, instructions::clz_u64{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i64)
		);
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_ctz(expr_value value)
{
	bz_assert(value.get_type()->is_builtin());

	auto const value_ref = value.get_value_as_instruction(*this);

	switch (value.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::i8:
		return expr_value::get_value(
			add_instruction(*this, instructions::ctz_u8{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i8)
		);
	case builtin_type_kind::i16:
		return expr_value::get_value(
			add_instruction(*this, instructions::ctz_u16{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i16)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			add_instruction(*this, instructions::ctz_u32{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i32)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			add_instruction(*this, instructions::ctz_u64{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i64)
		);
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_fshl(expr_value a, expr_value b, expr_value amount)
{
	bz_assert(a.get_type()->is_builtin());
	bz_assert(b.get_type()->is_builtin());
	bz_assert(amount.get_type()->is_builtin());
	bz_assert(a.get_type()->get_builtin_kind() == b.get_type()->get_builtin_kind());
	bz_assert(a.get_type()->get_builtin_kind() == amount.get_type()->get_builtin_kind());

	auto const a_val = a.get_value_as_instruction(*this);
	auto const b_val = b.get_value_as_instruction(*this);
	auto const amount_val = amount.get_value_as_instruction(*this);

	switch (a.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::i8:
		return expr_value::get_value(
			add_instruction(*this, instructions::fshl_u8{}, a_val, b_val, amount_val),
			this->get_builtin_type(builtin_type_kind::i8)
		);
	case builtin_type_kind::i16:
		return expr_value::get_value(
			add_instruction(*this, instructions::fshl_u16{}, a_val, b_val, amount_val),
			this->get_builtin_type(builtin_type_kind::i16)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			add_instruction(*this, instructions::fshl_u32{}, a_val, b_val, amount_val),
			this->get_builtin_type(builtin_type_kind::i32)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			add_instruction(*this, instructions::fshl_u64{}, a_val, b_val, amount_val),
			this->get_builtin_type(builtin_type_kind::i64)
		);
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_fshr(expr_value a, expr_value b, expr_value amount)
{
	bz_assert(a.get_type()->is_builtin());
	bz_assert(b.get_type()->is_builtin());
	bz_assert(amount.get_type()->is_builtin());
	bz_assert(a.get_type()->get_builtin_kind() == b.get_type()->get_builtin_kind());
	bz_assert(a.get_type()->get_builtin_kind() == amount.get_type()->get_builtin_kind());

	auto const a_val = a.get_value_as_instruction(*this);
	auto const b_val = b.get_value_as_instruction(*this);
	auto const amount_val = amount.get_value_as_instruction(*this);

	switch (a.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::i8:
		return expr_value::get_value(
			add_instruction(*this, instructions::fshr_u8{}, a_val, b_val, amount_val),
			this->get_builtin_type(builtin_type_kind::i8)
		);
	case builtin_type_kind::i16:
		return expr_value::get_value(
			add_instruction(*this, instructions::fshr_u16{}, a_val, b_val, amount_val),
			this->get_builtin_type(builtin_type_kind::i16)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			add_instruction(*this, instructions::fshr_u32{}, a_val, b_val, amount_val),
			this->get_builtin_type(builtin_type_kind::i32)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			add_instruction(*this, instructions::fshr_u64{}, a_val, b_val, amount_val),
			this->get_builtin_type(builtin_type_kind::i64)
		);
	default:
		bz_unreachable;
	}
}


instruction_ref codegen_context::create_unreachable(void)
{
	return add_instruction(*this, instructions::unreachable{});
}

instruction_ref codegen_context::create_error(lex::src_tokens const &src_tokens, bz::u8string message)
{
	auto const index = this->current_function_info.errors.size();
	this->current_function_info.errors.push_back({
		.src_tokens = src_tokens,
		.message = std::move(message),
	});
	bz_assert(index <= std::numeric_limits<uint32_t>::max());
	return add_instruction(*this, instructions::error{ .error_index = static_cast<uint32_t>(index) });
}

instruction_ref codegen_context::create_error_str(
	lex::src_tokens const &src_tokens,
	expr_value begin_ptr,
	expr_value end_ptr
)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	auto const begin_ptr_value = begin_ptr.get_value_as_instruction(*this);
	auto const end_ptr_value = end_ptr.get_value_as_instruction(*this);

	return add_instruction(*this,
		instructions::diagnostic_str{ .src_tokens_index = src_tokens_index, .kind = ctx::warning_kind::_last },
		begin_ptr_value,
		end_ptr_value
	);
}

instruction_ref codegen_context::create_warning_str(
	lex::src_tokens const &src_tokens,
	ctx::warning_kind kind,
	expr_value begin_ptr,
	expr_value end_ptr
)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	auto const begin_ptr_value = begin_ptr.get_value_as_instruction(*this);
	auto const end_ptr_value = end_ptr.get_value_as_instruction(*this);

	return add_instruction(*this,
		instructions::diagnostic_str{ .src_tokens_index = src_tokens_index, .kind = kind },
		begin_ptr_value,
		end_ptr_value
	);
}

expr_value codegen_context::create_is_option_set(expr_value begin_ptr, expr_value end_ptr)
{
	auto const begin_ptr_value = begin_ptr.get_value_as_instruction(*this);
	auto const end_ptr_value = end_ptr.get_value_as_instruction(*this);

	return expr_value::get_value(
		add_instruction(*this,
			instructions::is_option_set{},
			begin_ptr_value,
			end_ptr_value
		),
		this->get_builtin_type(builtin_type_kind::i1)
	);
}

instruction_ref codegen_context::create_array_bounds_check(
	lex::src_tokens const &src_tokens,
	expr_value index,
	expr_value size,
	bool is_index_signed
)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(index.get_type() == size.get_type());
	bz_assert(index.get_type()->is_builtin());

	auto const index_val = index.get_value_as_instruction(*this);
	auto const size_val = size.get_value_as_instruction(*this);

	if (index.get_type()->get_builtin_kind() == builtin_type_kind::i64)
	{
		if (is_index_signed)
		{
			return add_instruction(*this,
				instructions::array_bounds_check_i32{ .src_tokens_index = src_tokens_index },
				index_val, size_val
			);
		}
		else
		{
			return add_instruction(*this,
				instructions::array_bounds_check_u32{ .src_tokens_index = src_tokens_index },
				index_val, size_val
			);
		}
	}
	else
	{
		if (is_index_signed)
		{
			return add_instruction(*this,
				instructions::array_bounds_check_i64{ .src_tokens_index = src_tokens_index },
				index_val, size_val
			);
		}
		else
		{
			return add_instruction(*this,
				instructions::array_bounds_check_u64{ .src_tokens_index = src_tokens_index },
				index_val, size_val
			);
		}
	}
}

instruction_ref codegen_context::create_optional_get_value_check(lex::src_tokens const &src_tokens, expr_value has_value)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	auto const has_value_val = has_value.get_value_as_instruction(*this);
	return add_instruction(*this,
		instructions::optional_get_value_check{ .src_tokens_index = src_tokens_index },
		has_value_val
	);
}

instruction_ref codegen_context::create_str_construction_check(
	lex::src_tokens const &src_tokens,
	expr_value begin_ptr,
	expr_value end_ptr
)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	auto const begin_ptr_value = begin_ptr.get_value_as_instruction(*this);
	auto const end_ptr_value = end_ptr.get_value_as_instruction(*this);

	return add_instruction(*this,
		instructions::str_construction_check{ .src_tokens_index = src_tokens_index },
		begin_ptr_value,
		end_ptr_value
	);
}

instruction_ref codegen_context::create_slice_construction_check(
	lex::src_tokens const &src_tokens,
	expr_value begin_ptr,
	expr_value end_ptr,
	type const *elem_type,
	ast::typespec_view slice_type
)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);
	auto const slice_construction_check_info_index = this->add_slice_construction_check_info({
		.elem_type = elem_type,
		.slice_type = slice_type,
	});

	auto const begin_ptr_value = begin_ptr.get_value_as_instruction(*this);
	auto const end_ptr_value = end_ptr.get_value_as_instruction(*this);

	return add_instruction(*this,
		instructions::slice_construction_check{
			.src_tokens_index = src_tokens_index,
			.slice_construction_check_info_index = slice_construction_check_info_index,
		},
		begin_ptr_value,
		end_ptr_value
	);
}

void codegen_context::create_start_lifetime(expr_value ptr)
{
	bz_assert(ptr.is_reference());
	add_instruction(*this, instructions::start_lifetime{ .size = ptr.get_type()->size }, ptr.get_reference());
}

void codegen_context::create_end_lifetime(expr_value ptr)
{
	bz_assert(ptr.is_reference());
	auto const type = ptr.get_type();
	if (type->is_aggregate() && type->get_aggregate_types().empty())
	{
		// do nothing for an empty type
	}
	else
	{
		add_instruction(*this, instructions::end_lifetime{ .size = ptr.get_type()->size }, ptr.get_reference());
	}
}

static void resolve_instruction_args(instruction &inst, bz::array<instruction_ref, 3> const &args, auto get_instruction_value_index)
{
	inst.visit([&](auto &inst) {
		using inst_type = bz::meta::remove_reference<decltype(inst)>;
		if constexpr (instructions::arg_count<inst_type> == 0)
		{
			// such an instruction shouldn't be in unresolved_instructions
			bz_unreachable;
		}
		else if constexpr (instructions::arg_count<inst_type> == 1)
		{
			inst.args[0] = get_instruction_value_index(args[0]);
		}
		else if constexpr (instructions::arg_count<inst_type> == 2)
		{
			inst.args[0] = get_instruction_value_index(args[0]);
			inst.args[1] = get_instruction_value_index(args[1]);
		}
		else if constexpr (instructions::arg_count<inst_type> == 3)
		{
			inst.args[0] = get_instruction_value_index(args[0]);
			inst.args[1] = get_instruction_value_index(args[1]);
			inst.args[2] = get_instruction_value_index(args[2]);
		}
		else
		{
			static_assert(bz::meta::always_false<inst_type>);
		}
	});
}

static void resolve_jump_dests(instruction &inst, bz::array<basic_block_ref, 2> const &dests, auto get_instruction_index)
{
	switch (inst.index())
	{
	static_assert(instruction::variant_count == 519);
	case instruction::jump:
	{
		auto &jump_inst = inst.get<instruction::jump>();
		jump_inst.dest = get_instruction_index({ .bb_index = dests[0].bb_index, .inst_index = 0 });
		break;
	}
	case instruction::conditional_jump:
	{
		auto &jump_inst = inst.get<instruction::conditional_jump>();
		jump_inst.true_dest  = get_instruction_index({ .bb_index = dests[0].bb_index, .inst_index = 0 });
		jump_inst.false_dest = get_instruction_index({ .bb_index = dests[1].bb_index, .inst_index = 0 });
		break;
	}
	default:
		bz_unreachable;
	}
}

void current_function_info_t::finalize_function(void)
{
	bz_assert(this->func != nullptr);
	auto &func = *this->func;

	uint32_t instruction_value_offset = this->allocas.size();
	for (auto &bb : this->blocks)
	{
		bb.instruction_value_offset = instruction_value_offset;
		instruction_value_offset += bb.instructions.size();
	}

	auto const instructions_count = instruction_value_offset - this->blocks[0].instruction_value_offset;

	auto const get_instruction_value_index = [this](instruction_ref inst_ref) -> instruction_value_index {
		if (inst_ref.bb_index == instruction_ref::alloca_bb_index)
		{
			return { .index = inst_ref.inst_index };
		}
		else
		{
			return { .index = this->blocks[inst_ref.bb_index].instruction_value_offset + inst_ref.inst_index };
		}
	};

	auto const get_instruction_index = [this](instruction_ref inst_ref) -> instruction_index {
		bz_assert(inst_ref.bb_index != instruction_ref::alloca_bb_index);
		auto const bb_offset = this->blocks[inst_ref.bb_index].instruction_value_offset - this->blocks[0].instruction_value_offset;
		return { .index = bb_offset + inst_ref.inst_index };
	};

	auto const get_instruction = [this](instruction_ref inst_ref) -> instruction & {
		bz_assert(inst_ref.bb_index != instruction_ref::alloca_bb_index);
		return this->blocks[inst_ref.bb_index].instructions[inst_ref.inst_index];
	};

	for (auto const &[inst_ref, args] : this->unresolved_instructions)
	{
		resolve_instruction_args(get_instruction(inst_ref), args, get_instruction_value_index);
	}

	for (auto const &[inst_ref, dests] : this->unresolved_jumps)
	{
		resolve_jump_dests(get_instruction(inst_ref), dests, get_instruction_index);
	}

	// finalize switch_infos
	func.switch_infos = bz::fixed_vector<switch_info_t>(this->unresolved_switches.size());
	for (auto const i : bz::iota(0, func.switch_infos.size()))
	{
		auto const &[inst_ref, values, default_dest] = this->unresolved_switches[i];
		auto &info = func.switch_infos[i];
		info.values = bz::fixed_vector<switch_info_t::value_instruction_index_pair>(values.size());
		for (auto const j : bz::iota(0, info.values.size()))
		{
			info.values[j].value = values[j].value;
			info.values[j].dest = get_instruction_index({ .bb_index = values[j].bb.bb_index, .inst_index = 0 });
		}
		info.values.sort([](auto const &lhs, auto const &rhs) { return lhs.value < rhs.value; });
		info.default_dest = get_instruction_index({ .bb_index = default_dest.bb_index, .inst_index = 0 });
	}

	// finalize instructions
	{
		func.instructions = bz::fixed_vector<instruction>(instructions_count);
		auto it = func.instructions.begin();
		for (auto const &bb : this->blocks)
		{
			bz_assert(bb.instructions.not_empty());
			bz_assert(bb.instructions.back().is_terminator());
			std::copy_n(bb.instructions.begin(), bb.instructions.size(), it);
			it += bb.instructions.size();
		}
		bz_assert(it == func.instructions.end());
	}

	// finalize allocas
	func.allocas = bz::fixed_vector(this->allocas.as_array_view());

	// finalize errors
	{
		func.errors = bz::fixed_vector<error_info_t>(this->errors.size());
		for (auto const i : bz::iota(0, func.errors.size()))
		{
			func.errors[i] = std::move(this->errors[i]);
		}
	}

	// finalize src_tokens
	func.src_tokens = bz::fixed_vector(this->src_tokens.as_array_view());

	// finalize call_args
	{
		func.call_args = bz::fixed_vector<bz::fixed_vector<instruction_value_index>>(this->call_args.size());
		for (auto const i : bz::iota(0, func.call_args.size()))
		{
			func.call_args[i] = bz::fixed_vector<instruction_value_index>(this->call_args[i].size());
			for (auto const j : bz::iota(0, func.call_args[i].size()))
			{
				func.call_args[i][j] = get_instruction_value_index(this->call_args[i][j]);
			}
		}
	}

	// finalize slice_construction_check_infos
	func.slice_construction_check_infos = bz::fixed_vector(this->slice_construction_check_infos.as_array_view());

	// finalize pointer_arithmetic_check_infos
	func.pointer_arithmetic_check_infos = bz::fixed_vector(this->pointer_arithmetic_check_infos.as_array_view());

	// finalize memory_access_check_infos
	func.memory_access_check_infos = bz::fixed_vector(this->memory_access_check_infos.as_array_view());

	// finalize add_global_array_data_infos
	func.add_global_array_data_infos = bz::fixed_vector(this->add_global_array_data_infos.as_array_view());

	// finalize copy_values_infos
	func.copy_values_infos = bz::fixed_vector(this->copy_values_infos.as_array_view());

#ifndef NDEBUG
	if (debug_comptime_print_functions)
	{
		bz::log("{}", to_string(func));
	}
#endif // !NDEBUG

	*this = current_function_info_t();
}

} // namespace comptime
