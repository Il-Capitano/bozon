#include "codegen_context.h"
#include "ast/statement.h"

namespace comptime
{

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

void codegen_context::add_variable(ast::decl_variable const *decl, expr_value value)
{
	bz_assert(!this->variables.contains(decl));
	this->variables.insert({ decl, value });
}

expr_value codegen_context::get_variable(ast::decl_variable const *decl)
{
	auto const it = this->variables.find(decl);
	if (it == this->variables.end())
	{
		return expr_value::get_none();
	}
	else
	{
		return it->second;
	}
}

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
	this->blocks.emplace_back();
	return basic_block_ref{ static_cast<uint32_t>(this->blocks.size() - 1) };
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

uint32_t codegen_context::add_src_tokens(lex::src_tokens const &src_tokens)
{
	auto const result = this->global_codegen_ctx->src_tokens.size();
	this->global_codegen_ctx->src_tokens.push_back(src_tokens);
	return static_cast<uint32_t>(result);
}

expr_value codegen_context::get_dummy_value(type const *t)
{
	return expr_value::get_reference(instruction_ref{}, t);
}


expr_value codegen_context::create_const_i1(bool value)
{
	auto const inst_ref = this->add_instruction(instructions::const_i1{ .value = value });
	return expr_value::get_value(inst_ref, this->get_builtin_type(builtin_type_kind::i1));
}

expr_value codegen_context::create_const_i8(int8_t value)
{
	auto const inst_ref = this->add_instruction(instructions::const_i8{ .value = value });
	return expr_value::get_value(inst_ref, this->get_builtin_type(builtin_type_kind::i8));
}

expr_value codegen_context::create_const_i16(int16_t value)
{
	auto const inst_ref = this->add_instruction(instructions::const_i16{ .value = value });
	return expr_value::get_value(inst_ref, this->get_builtin_type(builtin_type_kind::i16));
}

expr_value codegen_context::create_const_i32(int32_t value)
{
	auto const inst_ref = this->add_instruction(instructions::const_i32{ .value = value });
	return expr_value::get_value(inst_ref, this->get_builtin_type(builtin_type_kind::i32));
}

expr_value codegen_context::create_const_i64(int64_t value)
{
	auto const inst_ref = this->add_instruction(instructions::const_i64{ .value = value });
	return expr_value::get_value(inst_ref, this->get_builtin_type(builtin_type_kind::i64));
}

expr_value codegen_context::create_const_u8(uint8_t value)
{
	auto const inst_ref = this->add_instruction(instructions::const_u8{ .value = value });
	return expr_value::get_value(inst_ref, this->get_builtin_type(builtin_type_kind::i8));
}

expr_value codegen_context::create_const_u16(uint16_t value)
{
	auto const inst_ref = this->add_instruction(instructions::const_u16{ .value = value });
	return expr_value::get_value(inst_ref, this->get_builtin_type(builtin_type_kind::i16));
}

expr_value codegen_context::create_const_u32(uint32_t value)
{
	auto const inst_ref = this->add_instruction(instructions::const_u32{ .value = value });
	return expr_value::get_value(inst_ref, this->get_builtin_type(builtin_type_kind::i32));
}

expr_value codegen_context::create_const_u64(uint64_t value)
{
	auto const inst_ref = this->add_instruction(instructions::const_u64{ .value = value });
	return expr_value::get_value(inst_ref, this->get_builtin_type(builtin_type_kind::i64));
}

expr_value codegen_context::create_const_f32(float32_t value)
{
	auto const inst_ref = this->add_instruction(instructions::const_f32{ .value = value });
	return expr_value::get_value(inst_ref, this->get_builtin_type(builtin_type_kind::f32));
}

expr_value codegen_context::create_const_f64(float64_t value)
{
	auto const inst_ref = this->add_instruction(instructions::const_f64{ .value = value });
	return expr_value::get_value(inst_ref, this->get_builtin_type(builtin_type_kind::f64));
}

expr_value codegen_context::create_const_ptr_null(void)
{
	auto const inst_ref = this->add_instruction(instructions::const_ptr_null{});
	return expr_value::get_value(inst_ref, this->get_pointer_type());
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
					this->add_instruction(instructions::load_ptr64_le{}, ptr_),
					type
				);
			}
			else
			{
				return expr_value::get_value(
					this->add_instruction(instructions::load_ptr32_le{}, ptr_),
					type
				);
			}
		}
		else
		{
			if (this->is_64_bit())
			{
				return expr_value::get_value(
					this->add_instruction(instructions::load_ptr64_be{}, ptr_),
					type
				);
			}
			else
			{
				return expr_value::get_value(
					this->add_instruction(instructions::load_ptr32_be{}, ptr_),
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
					this->add_instruction(instructions::load_i1_le{}, ptr_),
					type
				);
			case builtin_type_kind::i8:
				return expr_value::get_value(
					this->add_instruction(instructions::load_i8_le{}, ptr_),
					type
				);
			case builtin_type_kind::i16:
				return expr_value::get_value(
					this->add_instruction(instructions::load_i16_le{}, ptr_),
					type
				);
			case builtin_type_kind::i32:
				return expr_value::get_value(
					this->add_instruction(instructions::load_i32_le{}, ptr_),
					type
				);
			case builtin_type_kind::i64:
				return expr_value::get_value(
					this->add_instruction(instructions::load_i64_le{}, ptr_),
					type
				);
			case builtin_type_kind::f32:
				return expr_value::get_value(
					this->add_instruction(instructions::load_f32_le{}, ptr_),
					type
				);
			case builtin_type_kind::f64:
				return expr_value::get_value(
					this->add_instruction(instructions::load_f64_le{}, ptr_),
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
					this->add_instruction(instructions::load_i1_be{}, ptr_),
					type
				);
			case builtin_type_kind::i8:
				return expr_value::get_value(
					this->add_instruction(instructions::load_i8_be{}, ptr_),
					type
				);
			case builtin_type_kind::i16:
				return expr_value::get_value(
					this->add_instruction(instructions::load_i16_be{}, ptr_),
					type
				);
			case builtin_type_kind::i32:
				return expr_value::get_value(
					this->add_instruction(instructions::load_i32_be{}, ptr_),
					type
				);
			case builtin_type_kind::i64:
				return expr_value::get_value(
					this->add_instruction(instructions::load_i64_be{}, ptr_),
					type
				);
			case builtin_type_kind::f32:
				return expr_value::get_value(
					this->add_instruction(instructions::load_f32_be{}, ptr_),
					type
				);
			case builtin_type_kind::f64:
				return expr_value::get_value(
					this->add_instruction(instructions::load_f64_be{}, ptr_),
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
				return this->add_instruction(instructions::store_ptr64_le{}, value_, ptr_);
			}
			else
			{
				return this->add_instruction(instructions::store_ptr32_le{}, value_, ptr_);
			}
		}
		else
		{
			if (this->is_64_bit())
			{
				return this->add_instruction(instructions::store_ptr64_be{}, value_, ptr_);
			}
			else
			{
				return this->add_instruction(instructions::store_ptr32_be{}, value_, ptr_);
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
				return this->add_instruction(instructions::store_i1_le{}, value_, ptr_);
			case builtin_type_kind::i8:
				return this->add_instruction(instructions::store_i8_le{}, value_, ptr_);
			case builtin_type_kind::i16:
				return this->add_instruction(instructions::store_i16_le{}, value_, ptr_);
			case builtin_type_kind::i32:
				return this->add_instruction(instructions::store_i32_le{}, value_, ptr_);
			case builtin_type_kind::i64:
				return this->add_instruction(instructions::store_i64_le{}, value_, ptr_);
			case builtin_type_kind::f32:
				return this->add_instruction(instructions::store_f32_le{}, value_, ptr_);
			case builtin_type_kind::f64:
				return this->add_instruction(instructions::store_f64_le{}, value_, ptr_);
			case builtin_type_kind::void_:
				bz_unreachable;
			}
		}
		else
		{
			switch (type->get_builtin_kind())
			{
			case builtin_type_kind::i1:
				return this->add_instruction(instructions::store_i1_be{}, value_, ptr_);
			case builtin_type_kind::i8:
				return this->add_instruction(instructions::store_i8_be{}, value_, ptr_);
			case builtin_type_kind::i16:
				return this->add_instruction(instructions::store_i16_be{}, value_, ptr_);
			case builtin_type_kind::i32:
				return this->add_instruction(instructions::store_i32_be{}, value_, ptr_);
			case builtin_type_kind::i64:
				return this->add_instruction(instructions::store_i64_be{}, value_, ptr_);
			case builtin_type_kind::f32:
				return this->add_instruction(instructions::store_f32_be{}, value_, ptr_);
			case builtin_type_kind::f64:
				return this->add_instruction(instructions::store_f64_be{}, value_, ptr_);
			case builtin_type_kind::void_:
				bz_unreachable;
			}
		}
	}
}

expr_value codegen_context::create_alloca(type const *type)
{
	this->allocas.push_back({
		.size = type->size,
		.align = type->align,
	});
	auto const alloca_ref = instruction_ref{
		.bb_index = instruction_ref::alloca_bb_index,
		.inst_index = static_cast<uint32_t>(this->allocas.size() - 1),
	};
	return expr_value::get_reference(alloca_ref, type);
}

instruction_ref codegen_context::create_jump(basic_block_ref bb)
{
	auto const result = this->add_instruction(instructions::jump{});
	this->unresolved_jumps.push_back({ .inst = result, .dests = { bb, {} } });
	return result;
}

instruction_ref codegen_context::create_conditional_jump(
	expr_value condition,
	basic_block_ref true_bb,
	basic_block_ref false_bb
)
{
	auto const result = this->add_instruction(instructions::conditional_jump{}, condition.get_value_as_instruction(*this));
	this->unresolved_jumps.push_back({ .inst = result, .dests = { true_bb, false_bb } });
	return result;
}

instruction_ref codegen_context::create_ret(instruction_ref value)
{
	return this->add_instruction(instructions::ret{}, value);
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
		bz_assert(index <= type->get_array_size()); // one-past-the-end is allowed
		auto const offset = index * type->get_array_element_type()->size;
		auto const result_ptr = this->add_instruction(instructions::const_gep{ .offset = offset }, value.get_reference());
		return expr_value::get_reference(result_ptr, type->get_array_element_type());
	}
	else
	{
		bz_assert(type->is_aggregate());
		auto const types = type->get_aggregate_types();
		auto const offsets = type->get_aggregate_offsets();
		bz_assert(index < types.size());
		auto const result_ptr = this->add_instruction(instructions::const_gep{ .offset = offsets[index] }, value.get_reference());
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
		auto const result_ptr = this->add_instruction(
			instructions::array_gep_i32{ .stride = elem_type->size },
			value.get_reference(), index.get_value_as_instruction(*this)
		);
		return expr_value::get_reference(result_ptr, elem_type);
	}
	case builtin_type_kind::i64:
	{
		auto const result_ptr = this->add_instruction(
			instructions::array_gep_i64{ .stride = elem_type->size },
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
		auto const result_ptr = this->add_instruction(
			instructions::array_gep_i32{ .stride = elem_type->size },
			begin_ptr.get_value_as_instruction(*this), index.get_value_as_instruction(*this)
		);
		return expr_value::get_reference(result_ptr, elem_type);
	}
	case builtin_type_kind::i64:
	{
		auto const result_ptr = this->add_instruction(
			instructions::array_gep_i64{ .stride = elem_type->size },
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

	return this->add_instruction(instructions::const_memcpy{ .size = size }, dest.get_reference(), source.get_reference());
}

instruction_ref codegen_context::create_const_memset_zero(expr_value dest, size_t size)
{
	bz_assert(dest.is_reference());

	return this->add_instruction(instructions::const_memset_zero{ .size = size }, dest.get_reference());
}

expr_value codegen_context::create_function_call(function const *func, bz::fixed_vector<instruction_ref> args)
{
	auto const args_index = this->call_args.size();
	this->call_args.push_back(std::move(args));

	auto const inst_ref = this->add_instruction(
		instructions::function_call{ .func = func, .args_index = args_index }
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
			return expr_value::get_value(this->add_instruction(instructions::cast_zext_i1_to_i8{}, value_ref), dest);
		case builtin_type_kind::i16:
			return expr_value::get_value(this->add_instruction(instructions::cast_zext_i1_to_i16{}, value_ref), dest);
		case builtin_type_kind::i32:
			return expr_value::get_value(this->add_instruction(instructions::cast_zext_i1_to_i32{}, value_ref), dest);
		case builtin_type_kind::i64:
			return expr_value::get_value(this->add_instruction(instructions::cast_zext_i1_to_i64{}, value_ref), dest);
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
				return expr_value::get_value(this->add_instruction(instructions::cast_sext_i8_to_i16{}, value_ref), dest);
			case builtin_type_kind::i32:
				return expr_value::get_value(this->add_instruction(instructions::cast_sext_i8_to_i32{}, value_ref), dest);
			case builtin_type_kind::i64:
				return expr_value::get_value(this->add_instruction(instructions::cast_sext_i8_to_i64{}, value_ref), dest);
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
				return expr_value::get_value(this->add_instruction(instructions::cast_zext_i8_to_i16{}, value_ref), dest);
			case builtin_type_kind::i32:
				return expr_value::get_value(this->add_instruction(instructions::cast_zext_i8_to_i32{}, value_ref), dest);
			case builtin_type_kind::i64:
				return expr_value::get_value(this->add_instruction(instructions::cast_zext_i8_to_i64{}, value_ref), dest);
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
				return expr_value::get_value(this->add_instruction(instructions::cast_trunc_i16_to_i8{}, value_ref), dest);
			// case builtin_type_kind::i16:
			case builtin_type_kind::i32:
				return expr_value::get_value(this->add_instruction(instructions::cast_sext_i16_to_i32{}, value_ref), dest);
			case builtin_type_kind::i64:
				return expr_value::get_value(this->add_instruction(instructions::cast_sext_i16_to_i64{}, value_ref), dest);
			default:
				bz_unreachable;
			}
		}
		else
		{
			switch (dest->get_builtin_kind())
			{
			case builtin_type_kind::i8:
				return expr_value::get_value(this->add_instruction(instructions::cast_trunc_i16_to_i8{}, value_ref), dest);
			// case builtin_type_kind::i16:
			case builtin_type_kind::i32:
				return expr_value::get_value(this->add_instruction(instructions::cast_zext_i16_to_i32{}, value_ref), dest);
			case builtin_type_kind::i64:
				return expr_value::get_value(this->add_instruction(instructions::cast_zext_i16_to_i64{}, value_ref), dest);
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
				return expr_value::get_value(this->add_instruction(instructions::cast_trunc_i32_to_i8{}, value_ref), dest);
			case builtin_type_kind::i16:
				return expr_value::get_value(this->add_instruction(instructions::cast_trunc_i32_to_i16{}, value_ref), dest);
			// case builtin_type_kind::i32:
			case builtin_type_kind::i64:
				return expr_value::get_value(this->add_instruction(instructions::cast_sext_i32_to_i64{}, value_ref), dest);
			default:
				bz_unreachable;
			}
		}
		else
		{
			switch (dest->get_builtin_kind())
			{
			case builtin_type_kind::i8:
				return expr_value::get_value(this->add_instruction(instructions::cast_trunc_i32_to_i8{}, value_ref), dest);
			case builtin_type_kind::i16:
				return expr_value::get_value(this->add_instruction(instructions::cast_trunc_i32_to_i16{}, value_ref), dest);
			// case builtin_type_kind::i32:
			case builtin_type_kind::i64:
				return expr_value::get_value(this->add_instruction(instructions::cast_zext_i32_to_i64{}, value_ref), dest);
			default:
				bz_unreachable;
			}
		}
	case builtin_type_kind::i64:
		switch (dest->get_builtin_kind())
		{
		case builtin_type_kind::i8:
			return expr_value::get_value(this->add_instruction(instructions::cast_trunc_i64_to_i8{}, value_ref), dest);
		case builtin_type_kind::i16:
			return expr_value::get_value(this->add_instruction(instructions::cast_trunc_i64_to_i16{}, value_ref), dest);
		case builtin_type_kind::i32:
			return expr_value::get_value(this->add_instruction(instructions::cast_trunc_i64_to_i32{}, value_ref), dest);
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
		return expr_value::get_value(this->add_instruction(
			instructions::cast_f32_to_f64{},
			value.get_value_as_instruction(*this)
		), dest);
	}
	else
	{
		return expr_value::get_value(this->add_instruction(
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
				return expr_value::get_value(this->add_instruction(instructions::cast_f32_to_i8{}, value_ref), dest);
			case builtin_type_kind::i16:
				return expr_value::get_value(this->add_instruction(instructions::cast_f32_to_i16{}, value_ref), dest);
			case builtin_type_kind::i32:
				return expr_value::get_value(this->add_instruction(instructions::cast_f32_to_i32{}, value_ref), dest);
			case builtin_type_kind::i64:
				return expr_value::get_value(this->add_instruction(instructions::cast_f32_to_i64{}, value_ref), dest);
			default:
				bz_unreachable;
			}
		}
		else
		{
			switch (dest->get_builtin_kind())
			{
			case builtin_type_kind::i8:
				return expr_value::get_value(this->add_instruction(instructions::cast_f32_to_u8{}, value_ref), dest);
			case builtin_type_kind::i16:
				return expr_value::get_value(this->add_instruction(instructions::cast_f32_to_u16{}, value_ref), dest);
			case builtin_type_kind::i32:
				return expr_value::get_value(this->add_instruction(instructions::cast_f32_to_u32{}, value_ref), dest);
			case builtin_type_kind::i64:
				return expr_value::get_value(this->add_instruction(instructions::cast_f32_to_u64{}, value_ref), dest);
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
				return expr_value::get_value(this->add_instruction(instructions::cast_f64_to_i8{}, value_ref), dest);
			case builtin_type_kind::i16:
				return expr_value::get_value(this->add_instruction(instructions::cast_f64_to_i16{}, value_ref), dest);
			case builtin_type_kind::i32:
				return expr_value::get_value(this->add_instruction(instructions::cast_f64_to_i32{}, value_ref), dest);
			case builtin_type_kind::i64:
				return expr_value::get_value(this->add_instruction(instructions::cast_f64_to_i64{}, value_ref), dest);
			default:
				bz_unreachable;
			}
		}
		else
		{
			switch (dest->get_builtin_kind())
			{
			case builtin_type_kind::i8:
				return expr_value::get_value(this->add_instruction(instructions::cast_f64_to_u8{}, value_ref), dest);
			case builtin_type_kind::i16:
				return expr_value::get_value(this->add_instruction(instructions::cast_f64_to_u16{}, value_ref), dest);
			case builtin_type_kind::i32:
				return expr_value::get_value(this->add_instruction(instructions::cast_f64_to_u32{}, value_ref), dest);
			case builtin_type_kind::i64:
				return expr_value::get_value(this->add_instruction(instructions::cast_f64_to_u64{}, value_ref), dest);
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
				return expr_value::get_value(this->add_instruction(instructions::cast_i8_to_f32{}, value_ref), dest);
			case builtin_type_kind::i16:
				return expr_value::get_value(this->add_instruction(instructions::cast_i16_to_f32{}, value_ref), dest);
			case builtin_type_kind::i32:
				return expr_value::get_value(this->add_instruction(instructions::cast_i32_to_f32{}, value_ref), dest);
			case builtin_type_kind::i64:
				return expr_value::get_value(this->add_instruction(instructions::cast_i64_to_f32{}, value_ref), dest);
			default:
				bz_unreachable;
			}
		}
		else
		{
			switch (value_type->get_builtin_kind())
			{
			case builtin_type_kind::i8:
				return expr_value::get_value(this->add_instruction(instructions::cast_u8_to_f32{}, value_ref), dest);
			case builtin_type_kind::i16:
				return expr_value::get_value(this->add_instruction(instructions::cast_u16_to_f32{}, value_ref), dest);
			case builtin_type_kind::i32:
				return expr_value::get_value(this->add_instruction(instructions::cast_u32_to_f32{}, value_ref), dest);
			case builtin_type_kind::i64:
				return expr_value::get_value(this->add_instruction(instructions::cast_u64_to_f32{}, value_ref), dest);
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
				return expr_value::get_value(this->add_instruction(instructions::cast_i8_to_f64{}, value_ref), dest);
			case builtin_type_kind::i16:
				return expr_value::get_value(this->add_instruction(instructions::cast_i16_to_f64{}, value_ref), dest);
			case builtin_type_kind::i32:
				return expr_value::get_value(this->add_instruction(instructions::cast_i32_to_f64{}, value_ref), dest);
			case builtin_type_kind::i64:
				return expr_value::get_value(this->add_instruction(instructions::cast_i64_to_f64{}, value_ref), dest);
			default:
				bz_unreachable;
			}
		}
		else
		{
			switch (value_type->get_builtin_kind())
			{
			case builtin_type_kind::i8:
				return expr_value::get_value(this->add_instruction(instructions::cast_u8_to_f64{}, value_ref), dest);
			case builtin_type_kind::i16:
				return expr_value::get_value(this->add_instruction(instructions::cast_u16_to_f64{}, value_ref), dest);
			case builtin_type_kind::i32:
				return expr_value::get_value(this->add_instruction(instructions::cast_u32_to_f64{}, value_ref), dest);
			case builtin_type_kind::i64:
				return expr_value::get_value(this->add_instruction(instructions::cast_u64_to_f64{}, value_ref), dest);
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
			this->add_instruction(instructions::cmp_eq_i1{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::i8:
		return expr_value::get_value(
			this->add_instruction(instructions::cmp_eq_i8{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::i16:
		return expr_value::get_value(
			this->add_instruction(instructions::cmp_eq_i16{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			this->add_instruction(instructions::cmp_eq_i32{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			this->add_instruction(instructions::cmp_eq_i64{}, lhs_val, rhs_val),
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
			this->add_instruction(instructions::cmp_neq_i1{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::i8:
		return expr_value::get_value(
			this->add_instruction(instructions::cmp_neq_i8{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::i16:
		return expr_value::get_value(
			this->add_instruction(instructions::cmp_neq_i16{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			this->add_instruction(instructions::cmp_neq_i32{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			this->add_instruction(instructions::cmp_neq_i64{}, lhs_val, rhs_val),
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
				this->add_instruction(instructions::cmp_lt_i8{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i16:
			return expr_value::get_value(
				this->add_instruction(instructions::cmp_lt_i16{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i32:
			return expr_value::get_value(
				this->add_instruction(instructions::cmp_lt_i32{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i64:
			return expr_value::get_value(
				this->add_instruction(instructions::cmp_lt_i64{}, lhs_val, rhs_val),
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
				this->add_instruction(instructions::cmp_lt_u8{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i16:
			return expr_value::get_value(
				this->add_instruction(instructions::cmp_lt_u16{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i32:
			return expr_value::get_value(
				this->add_instruction(instructions::cmp_lt_u32{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i64:
			return expr_value::get_value(
				this->add_instruction(instructions::cmp_lt_u64{}, lhs_val, rhs_val),
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
				this->add_instruction(instructions::cmp_gt_i8{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i16:
			return expr_value::get_value(
				this->add_instruction(instructions::cmp_gt_i16{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i32:
			return expr_value::get_value(
				this->add_instruction(instructions::cmp_gt_i32{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i64:
			return expr_value::get_value(
				this->add_instruction(instructions::cmp_gt_i64{}, lhs_val, rhs_val),
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
				this->add_instruction(instructions::cmp_gt_u8{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i16:
			return expr_value::get_value(
				this->add_instruction(instructions::cmp_gt_u16{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i32:
			return expr_value::get_value(
				this->add_instruction(instructions::cmp_gt_u32{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i64:
			return expr_value::get_value(
				this->add_instruction(instructions::cmp_gt_u64{}, lhs_val, rhs_val),
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
				this->add_instruction(instructions::cmp_lte_i8{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i16:
			return expr_value::get_value(
				this->add_instruction(instructions::cmp_lte_i16{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i32:
			return expr_value::get_value(
				this->add_instruction(instructions::cmp_lte_i32{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i64:
			return expr_value::get_value(
				this->add_instruction(instructions::cmp_lte_i64{}, lhs_val, rhs_val),
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
				this->add_instruction(instructions::cmp_lte_u8{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i16:
			return expr_value::get_value(
				this->add_instruction(instructions::cmp_lte_u16{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i32:
			return expr_value::get_value(
				this->add_instruction(instructions::cmp_lte_u32{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i64:
			return expr_value::get_value(
				this->add_instruction(instructions::cmp_lte_u64{}, lhs_val, rhs_val),
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
				this->add_instruction(instructions::cmp_gte_i8{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i16:
			return expr_value::get_value(
				this->add_instruction(instructions::cmp_gte_i16{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i32:
			return expr_value::get_value(
				this->add_instruction(instructions::cmp_gte_i32{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i64:
			return expr_value::get_value(
				this->add_instruction(instructions::cmp_gte_i64{}, lhs_val, rhs_val),
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
				this->add_instruction(instructions::cmp_gte_u8{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i16:
			return expr_value::get_value(
				this->add_instruction(instructions::cmp_gte_u16{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i32:
			return expr_value::get_value(
				this->add_instruction(instructions::cmp_gte_u32{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		case builtin_type_kind::i64:
			return expr_value::get_value(
				this->add_instruction(instructions::cmp_gte_u64{}, lhs_val, rhs_val),
				this->get_builtin_type(builtin_type_kind::i1)
			);
		default:
			bz_unreachable;
		}
	}
}

instruction_ref codegen_context::create_float_cmp_eq_check(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs)
{
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return this->add_instruction(
			instructions::cmp_eq_f32_check{ .src_tokens_index = src_tokens_index },
			lhs_val, rhs_val
		);
	case builtin_type_kind::f64:
		return this->add_instruction(
			instructions::cmp_eq_f64_check{ .src_tokens_index = src_tokens_index },
			lhs_val, rhs_val
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_float_cmp_neq_check(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs)
{
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return this->add_instruction(
			instructions::cmp_neq_f32_check{ .src_tokens_index = src_tokens_index },
			lhs_val, rhs_val
		);
	case builtin_type_kind::f64:
		return this->add_instruction(
			instructions::cmp_neq_f64_check{ .src_tokens_index = src_tokens_index },
			lhs_val, rhs_val
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_float_cmp_lt_check(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs)
{
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return this->add_instruction(
			instructions::cmp_lt_f32_check{ .src_tokens_index = src_tokens_index },
			lhs_val, rhs_val
		);
	case builtin_type_kind::f64:
		return this->add_instruction(
			instructions::cmp_lt_f64_check{ .src_tokens_index = src_tokens_index },
			lhs_val, rhs_val
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_float_cmp_gt_check(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs)
{
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return this->add_instruction(
			instructions::cmp_gt_f32_check{ .src_tokens_index = src_tokens_index },
			lhs_val, rhs_val
		);
	case builtin_type_kind::f64:
		return this->add_instruction(
			instructions::cmp_gt_f64_check{ .src_tokens_index = src_tokens_index },
			lhs_val, rhs_val
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_float_cmp_lte_check(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs)
{
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return this->add_instruction(
			instructions::cmp_lte_f32_check{ .src_tokens_index = src_tokens_index },
			lhs_val, rhs_val
		);
	case builtin_type_kind::f64:
		return this->add_instruction(
			instructions::cmp_lte_f64_check{ .src_tokens_index = src_tokens_index },
			lhs_val, rhs_val
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_float_cmp_gte_check(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs)
{
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return this->add_instruction(
			instructions::cmp_gte_f32_check{ .src_tokens_index = src_tokens_index },
			lhs_val, rhs_val
		);
	case builtin_type_kind::f64:
		return this->add_instruction(
			instructions::cmp_gte_f64_check{ .src_tokens_index = src_tokens_index },
			lhs_val, rhs_val
		);
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
			this->add_instruction(instructions::cmp_eq_f32{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::cmp_eq_f64{}, lhs_val, rhs_val),
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
			this->add_instruction(instructions::cmp_neq_f32{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::cmp_neq_f64{}, lhs_val, rhs_val),
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
			this->add_instruction(instructions::cmp_lt_f32{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::cmp_lt_f64{}, lhs_val, rhs_val),
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
			this->add_instruction(instructions::cmp_gt_f32{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::cmp_gt_f64{}, lhs_val, rhs_val),
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
			this->add_instruction(instructions::cmp_lte_f32{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::cmp_lte_f64{}, lhs_val, rhs_val),
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
			this->add_instruction(instructions::cmp_gte_f32{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::cmp_gte_f64{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_cmp_eq_ptr(expr_value lhs, expr_value rhs)
{
	bz_assert(lhs.get_type()->is_pointer() && rhs.get_type()->is_pointer());
	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);
	auto const inst_ref = this->add_instruction(instructions::cmp_eq_ptr{}, lhs_val, rhs_val);
	return expr_value::get_value(inst_ref, this->get_builtin_type(builtin_type_kind::i1));
}

expr_value codegen_context::create_cmp_neq_ptr(expr_value lhs, expr_value rhs)
{
	bz_assert(lhs.get_type()->is_pointer() && rhs.get_type()->is_pointer());
	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);
	auto const inst_ref = this->add_instruction(instructions::cmp_neq_ptr{}, lhs_val, rhs_val);
	return expr_value::get_value(inst_ref, this->get_builtin_type(builtin_type_kind::i1));
}

expr_value codegen_context::create_neg(expr_value value)
{
	bz_assert(value.get_type()->is_builtin());

	auto const value_ref = value.get_value_as_instruction(*this);

	switch (value.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::i8:
		return expr_value::get_value(
			this->add_instruction(instructions::neg_i8{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i8)
		);
	case builtin_type_kind::i16:
		return expr_value::get_value(
			this->add_instruction(instructions::neg_i16{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i16)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			this->add_instruction(instructions::neg_i32{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i32)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			this->add_instruction(instructions::neg_i64{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i64)
		);
	case builtin_type_kind::f32:
		return expr_value::get_value(
			this->add_instruction(instructions::neg_f32{}, value_ref),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::neg_f64{}, value_ref),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_neg_check(lex::src_tokens const &src_tokens, expr_value value)
{
	bz_assert(value.get_type()->is_builtin());

	auto const value_ref = value.get_value_as_instruction(*this);

	switch (value.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::i8:
		return this->add_instruction(instructions::neg_i8_check{ .src_tokens_index = this->add_src_tokens(src_tokens) }, value_ref);
	case builtin_type_kind::i16:
		return this->add_instruction(instructions::neg_i16_check{ .src_tokens_index = this->add_src_tokens(src_tokens) }, value_ref);
	case builtin_type_kind::i32:
		return this->add_instruction(instructions::neg_i32_check{ .src_tokens_index = this->add_src_tokens(src_tokens) }, value_ref);
	case builtin_type_kind::i64:
		return this->add_instruction(instructions::neg_i64_check{ .src_tokens_index = this->add_src_tokens(src_tokens) }, value_ref);
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
			this->add_instruction(instructions::add_i8{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i8)
		);
	case builtin_type_kind::i16:
		return expr_value::get_value(
			this->add_instruction(instructions::add_i16{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i16)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			this->add_instruction(instructions::add_i32{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i32)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			this->add_instruction(instructions::add_i64{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i64)
		);
	default:
		bz_unreachable;
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
			this->add_instruction(instructions::sub_i8{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i8)
		);
	case builtin_type_kind::i16:
		return expr_value::get_value(
			this->add_instruction(instructions::sub_i16{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i16)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			this->add_instruction(instructions::sub_i32{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i32)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			this->add_instruction(instructions::sub_i64{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i64)
		);
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_ptrdiff(expr_value lhs, expr_value rhs, type const *elem_type)
{
	bz_assert(lhs.get_type()->is_pointer());
	bz_assert(rhs.get_type()->is_pointer());
	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);
	if (this->is_64_bit())
	{
		return expr_value::get_value(
			this->add_instruction(instructions::ptr64_diff{ .stride = elem_type->size }, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i64)
		);
	}
	else
	{
		return expr_value::get_value(
			this->add_instruction(instructions::ptr32_diff{ .stride = elem_type->size }, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i32)
		);
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
			this->add_instruction(instructions::not_i1{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::i8:
		return expr_value::get_value(
			this->add_instruction(instructions::not_i8{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i8)
		);
	case builtin_type_kind::i16:
		return expr_value::get_value(
			this->add_instruction(instructions::not_i16{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i16)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			this->add_instruction(instructions::not_i32{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i32)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			this->add_instruction(instructions::not_i64{}, value_ref),
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
			this->add_instruction(instructions::and_i1{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::i8:
		return expr_value::get_value(
			this->add_instruction(instructions::and_i8{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i8)
		);
	case builtin_type_kind::i16:
		return expr_value::get_value(
			this->add_instruction(instructions::and_i16{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i16)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			this->add_instruction(instructions::and_i32{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i32)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			this->add_instruction(instructions::and_i64{}, lhs_val, rhs_val),
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
			this->add_instruction(instructions::xor_i1{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::i8:
		return expr_value::get_value(
			this->add_instruction(instructions::xor_i8{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i8)
		);
	case builtin_type_kind::i16:
		return expr_value::get_value(
			this->add_instruction(instructions::xor_i16{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i16)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			this->add_instruction(instructions::xor_i32{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i32)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			this->add_instruction(instructions::xor_i64{}, lhs_val, rhs_val),
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
			this->add_instruction(instructions::or_i1{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::i8:
		return expr_value::get_value(
			this->add_instruction(instructions::or_i8{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i8)
		);
	case builtin_type_kind::i16:
		return expr_value::get_value(
			this->add_instruction(instructions::or_i16{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i16)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			this->add_instruction(instructions::or_i32{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i32)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			this->add_instruction(instructions::or_i64{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i64)
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_abs_check(lex::src_tokens const &src_tokens, expr_value value)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	auto const value_ref = value.get_value_as_instruction(*this);

	bz_assert(value.get_type()->is_builtin());
	switch (value.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::i8:
		return this->add_instruction(instructions::abs_i8_check{ .src_tokens_index = src_tokens_index }, value_ref);
	case builtin_type_kind::i16:
		return this->add_instruction(instructions::abs_i16_check{ .src_tokens_index = src_tokens_index }, value_ref);
	case builtin_type_kind::i32:
		return this->add_instruction(instructions::abs_i32_check{ .src_tokens_index = src_tokens_index }, value_ref);
	case builtin_type_kind::i64:
		return this->add_instruction(instructions::abs_i64_check{ .src_tokens_index = src_tokens_index }, value_ref);
	case builtin_type_kind::f32:
		return this->add_instruction(instructions::abs_f32_check{ .src_tokens_index = src_tokens_index }, value_ref);
	case builtin_type_kind::f64:
		return this->add_instruction(instructions::abs_f64_check{ .src_tokens_index = src_tokens_index }, value_ref);
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
			this->add_instruction(instructions::abs_i8{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i8)
		);
	case builtin_type_kind::i16:
		return expr_value::get_value(
			this->add_instruction(instructions::abs_i16{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i16)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			this->add_instruction(instructions::abs_i32{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i32)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			this->add_instruction(instructions::abs_i64{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i64)
		);
	case builtin_type_kind::f32:
		return expr_value::get_value(
			this->add_instruction(instructions::abs_f32{}, value_ref),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::abs_f64{}, value_ref),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_int_min(expr_value a, expr_value b, bool is_signed)
{
	bz_assert(a.get_type()->is_builtin());
	bz_assert(b.get_type()->is_builtin());
	bz_assert(a.get_type()->get_builtin_kind() == b.get_type()->get_builtin_kind());

	auto const a_val = a.get_value_as_instruction(*this);
	auto const b_val = b.get_value_as_instruction(*this);

	if (is_signed)
	{
		switch (a.get_type()->get_builtin_kind())
		{
		case builtin_type_kind::i8:
			return expr_value::get_value(
				this->add_instruction(instructions::min_i8{}, a_val, b_val),
				this->get_builtin_type(builtin_type_kind::i8)
			);
		case builtin_type_kind::i16:
			return expr_value::get_value(
				this->add_instruction(instructions::min_i16{}, a_val, b_val),
				this->get_builtin_type(builtin_type_kind::i16)
			);
		case builtin_type_kind::i32:
			return expr_value::get_value(
				this->add_instruction(instructions::min_i32{}, a_val, b_val),
				this->get_builtin_type(builtin_type_kind::i32)
			);
		case builtin_type_kind::i64:
			return expr_value::get_value(
				this->add_instruction(instructions::min_i64{}, a_val, b_val),
				this->get_builtin_type(builtin_type_kind::i64)
			);
		default:
			bz_unreachable;
		}
	}
	else
	{
		switch (a.get_type()->get_builtin_kind())
		{
		case builtin_type_kind::i8:
			return expr_value::get_value(
				this->add_instruction(instructions::min_u8{}, a_val, b_val),
				this->get_builtin_type(builtin_type_kind::i8)
			);
		case builtin_type_kind::i16:
			return expr_value::get_value(
				this->add_instruction(instructions::min_u16{}, a_val, b_val),
				this->get_builtin_type(builtin_type_kind::i16)
			);
		case builtin_type_kind::i32:
			return expr_value::get_value(
				this->add_instruction(instructions::min_u32{}, a_val, b_val),
				this->get_builtin_type(builtin_type_kind::i32)
			);
		case builtin_type_kind::i64:
			return expr_value::get_value(
				this->add_instruction(instructions::min_u64{}, a_val, b_val),
				this->get_builtin_type(builtin_type_kind::i64)
			);
		default:
			bz_unreachable;
		}
	}
}

expr_value codegen_context::create_float_min(expr_value x, expr_value y)
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
			this->add_instruction(instructions::min_f32{}, x_val, y_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::min_f64{}, x_val, y_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_float_min_check(lex::src_tokens const &src_tokens, expr_value x, expr_value y)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());
	bz_assert(y.get_type()->is_builtin());
	bz_assert(x.get_type()->get_builtin_kind() == y.get_type()->get_builtin_kind());

	auto const x_val = x.get_value_as_instruction(*this);
	auto const y_val = y.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return this->add_instruction(instructions::min_f32_check{ .src_tokens_index = src_tokens_index }, x_val, y_val);
	case builtin_type_kind::f64:
		return this->add_instruction(instructions::min_f64_check{ .src_tokens_index = src_tokens_index }, x_val, y_val);
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_int_max(expr_value a, expr_value b, bool is_signed)
{
	bz_assert(a.get_type()->is_builtin());
	bz_assert(b.get_type()->is_builtin());
	bz_assert(a.get_type()->get_builtin_kind() == b.get_type()->get_builtin_kind());

	auto const a_val = a.get_value_as_instruction(*this);
	auto const b_val = b.get_value_as_instruction(*this);

	if (is_signed)
	{
		switch (a.get_type()->get_builtin_kind())
		{
		case builtin_type_kind::i8:
			return expr_value::get_value(
				this->add_instruction(instructions::max_i8{}, a_val, b_val),
				this->get_builtin_type(builtin_type_kind::i8)
			);
		case builtin_type_kind::i16:
			return expr_value::get_value(
				this->add_instruction(instructions::max_i16{}, a_val, b_val),
				this->get_builtin_type(builtin_type_kind::i16)
			);
		case builtin_type_kind::i32:
			return expr_value::get_value(
				this->add_instruction(instructions::max_i32{}, a_val, b_val),
				this->get_builtin_type(builtin_type_kind::i32)
			);
		case builtin_type_kind::i64:
			return expr_value::get_value(
				this->add_instruction(instructions::max_i64{}, a_val, b_val),
				this->get_builtin_type(builtin_type_kind::i64)
			);
		default:
			bz_unreachable;
		}
	}
	else
	{
		switch (a.get_type()->get_builtin_kind())
		{
		case builtin_type_kind::i8:
			return expr_value::get_value(
				this->add_instruction(instructions::max_u8{}, a_val, b_val),
				this->get_builtin_type(builtin_type_kind::i8)
			);
		case builtin_type_kind::i16:
			return expr_value::get_value(
				this->add_instruction(instructions::max_u16{}, a_val, b_val),
				this->get_builtin_type(builtin_type_kind::i16)
			);
		case builtin_type_kind::i32:
			return expr_value::get_value(
				this->add_instruction(instructions::max_u32{}, a_val, b_val),
				this->get_builtin_type(builtin_type_kind::i32)
			);
		case builtin_type_kind::i64:
			return expr_value::get_value(
				this->add_instruction(instructions::max_u64{}, a_val, b_val),
				this->get_builtin_type(builtin_type_kind::i64)
			);
		default:
			bz_unreachable;
		}
	}
}

expr_value codegen_context::create_float_max(expr_value x, expr_value y)
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
			this->add_instruction(instructions::max_f32{}, x_val, y_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::max_f64{}, x_val, y_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_float_max_check(lex::src_tokens const &src_tokens, expr_value x, expr_value y)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());
	bz_assert(y.get_type()->is_builtin());
	bz_assert(x.get_type()->get_builtin_kind() == y.get_type()->get_builtin_kind());

	auto const x_val = x.get_value_as_instruction(*this);
	auto const y_val = y.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return this->add_instruction(instructions::max_f32_check{ .src_tokens_index = src_tokens_index }, x_val, y_val);
	case builtin_type_kind::f64:
		return this->add_instruction(instructions::max_f64_check{ .src_tokens_index = src_tokens_index }, x_val, y_val);
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
			this->add_instruction(instructions::exp_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::exp_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_exp_check(lex::src_tokens const &src_tokens, expr_value x)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return this->add_instruction(instructions::exp_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
	case builtin_type_kind::f64:
		return this->add_instruction(instructions::exp_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
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
			this->add_instruction(instructions::exp2_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::exp2_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_exp2_check(lex::src_tokens const &src_tokens, expr_value x)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return this->add_instruction(instructions::exp2_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
	case builtin_type_kind::f64:
		return this->add_instruction(instructions::exp2_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
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
			this->add_instruction(instructions::expm1_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::expm1_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_expm1_check(lex::src_tokens const &src_tokens, expr_value x)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return this->add_instruction(instructions::expm1_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
	case builtin_type_kind::f64:
		return this->add_instruction(instructions::expm1_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
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
			this->add_instruction(instructions::log_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::log_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_log_check(lex::src_tokens const &src_tokens, expr_value x)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return this->add_instruction(instructions::log_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
	case builtin_type_kind::f64:
		return this->add_instruction(instructions::log_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
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
			this->add_instruction(instructions::log10_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::log10_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_log10_check(lex::src_tokens const &src_tokens, expr_value x)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return this->add_instruction(instructions::log10_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
	case builtin_type_kind::f64:
		return this->add_instruction(instructions::log10_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
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
			this->add_instruction(instructions::log2_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::log2_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_log2_check(lex::src_tokens const &src_tokens, expr_value x)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return this->add_instruction(instructions::log2_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
	case builtin_type_kind::f64:
		return this->add_instruction(instructions::log2_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
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
			this->add_instruction(instructions::log1p_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::log1p_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_log1p_check(lex::src_tokens const &src_tokens, expr_value x)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return this->add_instruction(instructions::log1p_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
	case builtin_type_kind::f64:
		return this->add_instruction(instructions::log1p_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
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
			this->add_instruction(instructions::sqrt_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::sqrt_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_sqrt_check(lex::src_tokens const &src_tokens, expr_value x)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return this->add_instruction(instructions::sqrt_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
	case builtin_type_kind::f64:
		return this->add_instruction(instructions::sqrt_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
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
			this->add_instruction(instructions::pow_f32{}, x_val, y_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::pow_f64{}, x_val, y_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_pow_check(lex::src_tokens const &src_tokens, expr_value x, expr_value y)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());
	bz_assert(y.get_type()->is_builtin());
	bz_assert(x.get_type()->get_builtin_kind() == y.get_type()->get_builtin_kind());

	auto const x_val = x.get_value_as_instruction(*this);
	auto const y_val = y.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return this->add_instruction(instructions::pow_f32_check{ .src_tokens_index = src_tokens_index }, x_val, y_val);
	case builtin_type_kind::f64:
		return this->add_instruction(instructions::pow_f64_check{ .src_tokens_index = src_tokens_index }, x_val, y_val);
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
			this->add_instruction(instructions::cbrt_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::cbrt_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_cbrt_check(lex::src_tokens const &src_tokens, expr_value x)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return this->add_instruction(instructions::cbrt_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
	case builtin_type_kind::f64:
		return this->add_instruction(instructions::cbrt_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
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
			this->add_instruction(instructions::hypot_f32{}, x_val, y_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::hypot_f64{}, x_val, y_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_hypot_check(lex::src_tokens const &src_tokens, expr_value x, expr_value y)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());
	bz_assert(y.get_type()->is_builtin());
	bz_assert(x.get_type()->get_builtin_kind() == y.get_type()->get_builtin_kind());

	auto const x_val = x.get_value_as_instruction(*this);
	auto const y_val = y.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return this->add_instruction(instructions::hypot_f32_check{ .src_tokens_index = src_tokens_index }, x_val, y_val);
	case builtin_type_kind::f64:
		return this->add_instruction(instructions::hypot_f64_check{ .src_tokens_index = src_tokens_index }, x_val, y_val);
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
			this->add_instruction(instructions::sin_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::sin_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_sin_check(lex::src_tokens const &src_tokens, expr_value x)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return this->add_instruction(instructions::sin_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
	case builtin_type_kind::f64:
		return this->add_instruction(instructions::sin_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
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
			this->add_instruction(instructions::cos_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::cos_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_cos_check(lex::src_tokens const &src_tokens, expr_value x)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return this->add_instruction(instructions::cos_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
	case builtin_type_kind::f64:
		return this->add_instruction(instructions::cos_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
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
			this->add_instruction(instructions::tan_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::tan_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_tan_check(lex::src_tokens const &src_tokens, expr_value x)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return this->add_instruction(instructions::tan_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
	case builtin_type_kind::f64:
		return this->add_instruction(instructions::tan_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
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
			this->add_instruction(instructions::asin_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::asin_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_asin_check(lex::src_tokens const &src_tokens, expr_value x)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return this->add_instruction(instructions::asin_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
	case builtin_type_kind::f64:
		return this->add_instruction(instructions::asin_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
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
			this->add_instruction(instructions::acos_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::acos_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_acos_check(lex::src_tokens const &src_tokens, expr_value x)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return this->add_instruction(instructions::acos_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
	case builtin_type_kind::f64:
		return this->add_instruction(instructions::acos_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
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
			this->add_instruction(instructions::atan_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::atan_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_atan_check(lex::src_tokens const &src_tokens, expr_value x)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return this->add_instruction(instructions::atan_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
	case builtin_type_kind::f64:
		return this->add_instruction(instructions::atan_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
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
			this->add_instruction(instructions::atan2_f32{}, y_val, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::atan2_f64{}, y_val, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_atan2_check(lex::src_tokens const &src_tokens, expr_value y, expr_value x)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(y.get_type()->is_builtin());
	bz_assert(x.get_type()->is_builtin());
	bz_assert(y.get_type()->get_builtin_kind() == x.get_type()->get_builtin_kind());

	auto const y_val = y.get_value_as_instruction(*this);
	auto const x_val = x.get_value_as_instruction(*this);

	switch (y.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return this->add_instruction(instructions::atan2_f32_check{ .src_tokens_index = src_tokens_index }, y_val, x_val);
	case builtin_type_kind::f64:
		return this->add_instruction(instructions::atan2_f64_check{ .src_tokens_index = src_tokens_index }, y_val, x_val);
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
			this->add_instruction(instructions::sinh_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::sinh_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_sinh_check(lex::src_tokens const &src_tokens, expr_value x)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return this->add_instruction(instructions::sinh_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
	case builtin_type_kind::f64:
		return this->add_instruction(instructions::sinh_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
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
			this->add_instruction(instructions::cosh_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::cosh_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_cosh_check(lex::src_tokens const &src_tokens, expr_value x)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return this->add_instruction(instructions::cosh_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
	case builtin_type_kind::f64:
		return this->add_instruction(instructions::cosh_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
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
			this->add_instruction(instructions::tanh_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::tanh_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_tanh_check(lex::src_tokens const &src_tokens, expr_value x)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return this->add_instruction(instructions::tanh_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
	case builtin_type_kind::f64:
		return this->add_instruction(instructions::tanh_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
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
			this->add_instruction(instructions::asinh_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::asinh_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_asinh_check(lex::src_tokens const &src_tokens, expr_value x)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return this->add_instruction(instructions::asinh_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
	case builtin_type_kind::f64:
		return this->add_instruction(instructions::asinh_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
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
			this->add_instruction(instructions::acosh_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::acosh_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_acosh_check(lex::src_tokens const &src_tokens, expr_value x)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return this->add_instruction(instructions::acosh_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
	case builtin_type_kind::f64:
		return this->add_instruction(instructions::acosh_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
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
			this->add_instruction(instructions::atanh_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::atanh_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_atanh_check(lex::src_tokens const &src_tokens, expr_value x)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return this->add_instruction(instructions::atanh_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
	case builtin_type_kind::f64:
		return this->add_instruction(instructions::atanh_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
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
			this->add_instruction(instructions::erf_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::erf_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_erf_check(lex::src_tokens const &src_tokens, expr_value x)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return this->add_instruction(instructions::erf_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
	case builtin_type_kind::f64:
		return this->add_instruction(instructions::erf_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
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
			this->add_instruction(instructions::erfc_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::erfc_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_erfc_check(lex::src_tokens const &src_tokens, expr_value x)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return this->add_instruction(instructions::erfc_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
	case builtin_type_kind::f64:
		return this->add_instruction(instructions::erfc_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
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
			this->add_instruction(instructions::tgamma_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::tgamma_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_tgamma_check(lex::src_tokens const &src_tokens, expr_value x)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return this->add_instruction(instructions::tgamma_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
	case builtin_type_kind::f64:
		return this->add_instruction(instructions::tgamma_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
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
			this->add_instruction(instructions::lgamma_f32{}, x_val),
			this->get_builtin_type(builtin_type_kind::f32)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::lgamma_f64{}, x_val),
			this->get_builtin_type(builtin_type_kind::f64)
		);
	default:
		bz_unreachable;
	}
}

instruction_ref codegen_context::create_lgamma_check(lex::src_tokens const &src_tokens, expr_value x)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	bz_assert(x.get_type()->is_builtin());

	auto const x_val = x.get_value_as_instruction(*this);

	switch (x.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return this->add_instruction(instructions::lgamma_f32_check{ .src_tokens_index = src_tokens_index }, x_val);
	case builtin_type_kind::f64:
		return this->add_instruction(instructions::lgamma_f64_check{ .src_tokens_index = src_tokens_index }, x_val);
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
			this->add_instruction(instructions::bitreverse_u8{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i8)
		);
	case builtin_type_kind::i16:
		return expr_value::get_value(
			this->add_instruction(instructions::bitreverse_u16{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i16)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			this->add_instruction(instructions::bitreverse_u32{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i32)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			this->add_instruction(instructions::bitreverse_u64{}, value_ref),
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
			this->add_instruction(instructions::popcount_u8{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i8)
		);
	case builtin_type_kind::i16:
		return expr_value::get_value(
			this->add_instruction(instructions::popcount_u16{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i16)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			this->add_instruction(instructions::popcount_u32{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i32)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			this->add_instruction(instructions::popcount_u64{}, value_ref),
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
			this->add_instruction(instructions::byteswap_u16{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i16)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			this->add_instruction(instructions::byteswap_u32{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i32)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			this->add_instruction(instructions::byteswap_u64{}, value_ref),
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
			this->add_instruction(instructions::clz_u8{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i8)
		);
	case builtin_type_kind::i16:
		return expr_value::get_value(
			this->add_instruction(instructions::clz_u16{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i16)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			this->add_instruction(instructions::clz_u32{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i32)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			this->add_instruction(instructions::clz_u64{}, value_ref),
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
			this->add_instruction(instructions::ctz_u8{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i8)
		);
	case builtin_type_kind::i16:
		return expr_value::get_value(
			this->add_instruction(instructions::ctz_u16{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i16)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			this->add_instruction(instructions::ctz_u32{}, value_ref),
			this->get_builtin_type(builtin_type_kind::i32)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			this->add_instruction(instructions::ctz_u64{}, value_ref),
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
			this->add_instruction(instructions::fshl_u8{}, a_val, b_val, amount_val),
			this->get_builtin_type(builtin_type_kind::i8)
		);
	case builtin_type_kind::i16:
		return expr_value::get_value(
			this->add_instruction(instructions::fshl_u16{}, a_val, b_val, amount_val),
			this->get_builtin_type(builtin_type_kind::i16)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			this->add_instruction(instructions::fshl_u32{}, a_val, b_val, amount_val),
			this->get_builtin_type(builtin_type_kind::i32)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			this->add_instruction(instructions::fshl_u64{}, a_val, b_val, amount_val),
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
			this->add_instruction(instructions::fshr_u8{}, a_val, b_val, amount_val),
			this->get_builtin_type(builtin_type_kind::i8)
		);
	case builtin_type_kind::i16:
		return expr_value::get_value(
			this->add_instruction(instructions::fshr_u16{}, a_val, b_val, amount_val),
			this->get_builtin_type(builtin_type_kind::i16)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			this->add_instruction(instructions::fshr_u32{}, a_val, b_val, amount_val),
			this->get_builtin_type(builtin_type_kind::i32)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			this->add_instruction(instructions::fshr_u64{}, a_val, b_val, amount_val),
			this->get_builtin_type(builtin_type_kind::i64)
		);
	default:
		bz_unreachable;
	}
}


instruction_ref codegen_context::create_unreachable(void)
{
	return this->add_instruction(instructions::unreachable{});
}

instruction_ref codegen_context::create_error(lex::src_tokens const &src_tokens, bz::u8string message)
{
	this->global_codegen_ctx->errors.push_back({
		.src_tokens = src_tokens,
		.message = std::move(message),
	});
	auto const index = this->global_codegen_ctx->errors.size() - 1;
	bz_assert(index <= std::numeric_limits<uint32_t>::max());
	return this->add_instruction(instructions::error{ .error_index = static_cast<uint32_t>(index) });
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

	return this->add_instruction(
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

	return this->add_instruction(
		instructions::diagnostic_str{ .src_tokens_index = src_tokens_index, .kind = kind },
		begin_ptr_value,
		end_ptr_value
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
			return this->add_instruction(
				instructions::array_bounds_check_i32{ .src_tokens_index = src_tokens_index },
				index_val, size_val
			);
		}
		else
		{
			return this->add_instruction(
				instructions::array_bounds_check_u32{ .src_tokens_index = src_tokens_index },
				index_val, size_val
			);
		}
	}
	else
	{
		if (is_index_signed)
		{
			return this->add_instruction(
				instructions::array_bounds_check_i64{ .src_tokens_index = src_tokens_index },
				index_val, size_val
			);
		}
		else
		{
			return this->add_instruction(
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
	return this->add_instruction(
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

	return this->add_instruction(
		instructions::str_construction_check{ .src_tokens_index = src_tokens_index },
		begin_ptr_value,
		end_ptr_value
	);
}

instruction_ref codegen_context::create_slice_construction_check(
	lex::src_tokens const &src_tokens,
	expr_value begin_ptr,
	expr_value end_ptr
)
{
	auto const src_tokens_index = this->add_src_tokens(src_tokens);

	auto const begin_ptr_value = begin_ptr.get_value_as_instruction(*this);
	auto const end_ptr_value = end_ptr.get_value_as_instruction(*this);

	return this->add_instruction(
		instructions::slice_construction_check{ .src_tokens_index = src_tokens_index },
		begin_ptr_value,
		end_ptr_value
	);
}

static void resolve_instruction_args(instruction &inst, bz::array<instruction_ref, 3> const &args, auto get_instruction_value_index)
{
	inst.visit([&](auto &inst) {
		using inst_type = bz::meta::remove_reference<decltype(inst.inst)>;
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
	static_assert(instruction::variant_count == 407);
	case instruction::jump:
	{
		auto &jump_inst = inst.get<instruction::jump>().inst;
		jump_inst.dest = get_instruction_index({ .bb_index = dests[0].bb_index, .inst_index = 0 });
		break;
	}
	case instruction::conditional_jump:
	{
		auto &jump_inst = inst.get<instruction::conditional_jump>().inst;
		jump_inst.true_dest  = get_instruction_index({ .bb_index = dests[0].bb_index, .inst_index = 0 });
		jump_inst.false_dest = get_instruction_index({ .bb_index = dests[1].bb_index, .inst_index = 0 });
		break;
	}
	default:
		bz_unreachable;
	}
}

void codegen_context::finalize_function(function &func)
{
	uint32_t instruction_value_offset = this->allocas.size();
	for (auto &bb : this->blocks)
	{
		bb.instruction_value_offset = instruction_value_offset;
		instruction_value_offset += bb.instructions.size();
	}

	auto const instruction_values_count = instruction_value_offset;
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

	// finalize instructions
	{
		func.instructions = bz::fixed_vector<instruction>(instructions_count);
		auto it = func.instructions.begin();
		for (auto const &bb : this->blocks)
		{
			std::copy_n(bb.instructions.begin(), bb.instructions.size(), it);
			it += bb.instructions.size();
		}
		bz_assert(it == func.instructions.end());
	}

	// finalize call_args
	{
		func.call_args = bz::fixed_vector<bz::fixed_vector<instruction_index>>(this->call_args.size());
		for (auto const i : bz::iota(0, func.call_args.size()))
		{
			func.call_args[i] = bz::fixed_vector<instruction_index>(this->call_args[i].size());
			for (auto const j : bz::iota(0, func.call_args[i].size()))
			{
				func.call_args[i][j] = get_instruction_index(this->call_args[i][j]);
			}
		}
	}
}

} // namespace comptime
