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

expr_value codegen_context::create_float_cmp_eq(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs)
{
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);
	this->global_codegen_ctx->src_tokens.push_back(src_tokens);
	auto const src_tokens_index = this->global_codegen_ctx->src_tokens.size() - 1;

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			this->add_instruction(
				instructions::cmp_eq_f32{ .src_tokens_index = static_cast<uint32_t>(src_tokens_index) },
				lhs_val, rhs_val
			),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(
				instructions::cmp_eq_f64{ .src_tokens_index = static_cast<uint32_t>(src_tokens_index) },
				lhs_val, rhs_val
			),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_float_cmp_neq(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs)
{
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);
	this->global_codegen_ctx->src_tokens.push_back(src_tokens);
	auto const src_tokens_index = this->global_codegen_ctx->src_tokens.size() - 1;

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			this->add_instruction(
				instructions::cmp_neq_f32{ .src_tokens_index = static_cast<uint32_t>(src_tokens_index) },
				lhs_val, rhs_val
			),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(
				instructions::cmp_neq_f64{ .src_tokens_index = static_cast<uint32_t>(src_tokens_index) },
				lhs_val, rhs_val
			),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_float_cmp_lt(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs)
{
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);
	this->global_codegen_ctx->src_tokens.push_back(src_tokens);
	auto const src_tokens_index = this->global_codegen_ctx->src_tokens.size() - 1;

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			this->add_instruction(
				instructions::cmp_lt_f32{ .src_tokens_index = static_cast<uint32_t>(src_tokens_index) },
				lhs_val, rhs_val
			),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(
				instructions::cmp_lt_f64{ .src_tokens_index = static_cast<uint32_t>(src_tokens_index) },
				lhs_val, rhs_val
			),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_float_cmp_gt(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs)
{
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);
	this->global_codegen_ctx->src_tokens.push_back(src_tokens);
	auto const src_tokens_index = this->global_codegen_ctx->src_tokens.size() - 1;

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			this->add_instruction(
				instructions::cmp_gt_f32{ .src_tokens_index = static_cast<uint32_t>(src_tokens_index) },
				lhs_val, rhs_val
			),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(
				instructions::cmp_gt_f64{ .src_tokens_index = static_cast<uint32_t>(src_tokens_index) },
				lhs_val, rhs_val
			),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_float_cmp_lte(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs)
{
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);
	this->global_codegen_ctx->src_tokens.push_back(src_tokens);
	auto const src_tokens_index = this->global_codegen_ctx->src_tokens.size() - 1;

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			this->add_instruction(
				instructions::cmp_lte_f32{ .src_tokens_index = static_cast<uint32_t>(src_tokens_index) },
				lhs_val, rhs_val
			),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(
				instructions::cmp_lte_f64{ .src_tokens_index = static_cast<uint32_t>(src_tokens_index) },
				lhs_val, rhs_val
			),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_float_cmp_gte(lex::src_tokens const &src_tokens, expr_value lhs, expr_value rhs)
{
	bz_assert(lhs.get_type()->is_builtin());
	bz_assert(rhs.get_type()->is_builtin());
	bz_assert(lhs.get_type()->get_builtin_kind() == rhs.get_type()->get_builtin_kind());

	auto const lhs_val = lhs.get_value_as_instruction(*this);
	auto const rhs_val = rhs.get_value_as_instruction(*this);
	this->global_codegen_ctx->src_tokens.push_back(src_tokens);
	auto const src_tokens_index = this->global_codegen_ctx->src_tokens.size() - 1;

	switch (lhs.get_type()->get_builtin_kind())
	{
	case builtin_type_kind::f32:
		return expr_value::get_value(
			this->add_instruction(
				instructions::cmp_gte_f32{ .src_tokens_index = static_cast<uint32_t>(src_tokens_index) },
				lhs_val, rhs_val
			),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(
				instructions::cmp_gte_f64{ .src_tokens_index = static_cast<uint32_t>(src_tokens_index) },
				lhs_val, rhs_val
			),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_float_cmp_eq_unchecked(expr_value lhs, expr_value rhs)
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
			this->add_instruction(instructions::cmp_eq_f32_unchecked{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::cmp_eq_f64_unchecked{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_float_cmp_neq_unchecked(expr_value lhs, expr_value rhs)
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
			this->add_instruction(instructions::cmp_neq_f32_unchecked{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::cmp_neq_f64_unchecked{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_float_cmp_lt_unchecked(expr_value lhs, expr_value rhs)
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
			this->add_instruction(instructions::cmp_lt_f32_unchecked{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::cmp_lt_f64_unchecked{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_float_cmp_gt_unchecked(expr_value lhs, expr_value rhs)
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
			this->add_instruction(instructions::cmp_gt_f32_unchecked{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::cmp_gt_f64_unchecked{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_float_cmp_lte_unchecked(expr_value lhs, expr_value rhs)
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
			this->add_instruction(instructions::cmp_lte_f32_unchecked{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::cmp_lte_f64_unchecked{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_float_cmp_gte_unchecked(expr_value lhs, expr_value rhs)
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
			this->add_instruction(instructions::cmp_gte_f32_unchecked{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i1)
		);
	case builtin_type_kind::f64:
		return expr_value::get_value(
			this->add_instruction(instructions::cmp_gte_f64_unchecked{}, lhs_val, rhs_val),
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

expr_value codegen_context::create_add_unchecked(expr_value lhs, expr_value rhs)
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
			this->add_instruction(instructions::add_i8_unchecked{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i8)
		);
	case builtin_type_kind::i16:
		return expr_value::get_value(
			this->add_instruction(instructions::add_i16_unchecked{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i16)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			this->add_instruction(instructions::add_i32_unchecked{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i32)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			this->add_instruction(instructions::add_i64_unchecked{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i64)
		);
	default:
		bz_unreachable;
	}
}

expr_value codegen_context::create_sub_unchecked(expr_value lhs, expr_value rhs)
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
			this->add_instruction(instructions::sub_i8_unchecked{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i8)
		);
	case builtin_type_kind::i16:
		return expr_value::get_value(
			this->add_instruction(instructions::sub_i16_unchecked{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i16)
		);
	case builtin_type_kind::i32:
		return expr_value::get_value(
			this->add_instruction(instructions::sub_i32_unchecked{}, lhs_val, rhs_val),
			this->get_builtin_type(builtin_type_kind::i32)
		);
	case builtin_type_kind::i64:
		return expr_value::get_value(
			this->add_instruction(instructions::sub_i64_unchecked{}, lhs_val, rhs_val),
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

instruction_ref codegen_context::create_array_bounds_check(
	lex::src_tokens const &src_tokens,
	expr_value index,
	expr_value size,
	bool is_index_signed
)
{
	this->global_codegen_ctx->src_tokens.push_back(src_tokens);
	auto const src_tokens_index = static_cast<uint32_t>(this->global_codegen_ctx->src_tokens.size() - 1);

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

} // namespace comptime
