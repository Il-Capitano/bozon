#include "executor_context.h"

namespace comptime
{

uint8_t *executor_context::get_memory(ptr_t address)
{
	return this->memory.get_memory(address);
}

void executor_context::set_current_instruction_value(instruction_value value)
{
	*this->current_instruction_value = value;
}

instruction_value executor_context::get_instruction_value(instruction_value_index index)
{
	return this->instruction_values[index.index];
}

void executor_context::do_jump(instruction_index dest)
{
	bz_assert(dest.index < this->instructions.size());
	this->next_instruction = &this->instructions[dest.index];
}

void executor_context::do_ret(instruction_value value)
{
	this->ret_value = value;
	this->returned = true;
}

void executor_context::do_ret_void(void)
{
	this->returned = true;
}

void executor_context::check_str_construction(uint32_t src_tokens_index, ptr_t begin, ptr_t end)
{
	auto const elem_type = this->global_context->get_builtin_type(builtin_type_kind::i8);
	auto const is_good = this->memory.check_slice_construction(begin, end, elem_type);
	if (!is_good)
	{
		this->report_error(
			src_tokens_index,
			"invalid memory range for 'str'",
			{ this->make_note(src_tokens_index, this->memory.get_slice_construction_error_reason(begin, end, elem_type)) }
		);
	}
}

void executor_context::check_slice_construction(
	uint32_t src_tokens_index,
	ptr_t begin,
	ptr_t end,
	type const *elem_type,
	ast::typespec_view slice_type
)
{
	auto const is_good = this->memory.check_slice_construction(begin, end, elem_type);
	if (!is_good)
	{
		this->report_error(
			src_tokens_index,
			bz::format("invalid memory range for a slice of type '{}'", slice_type),
			{ this->make_note(src_tokens_index, this->memory.get_slice_construction_error_reason(begin, end, elem_type)) }
		);
	}
}

int executor_context::compare_pointers(uint32_t src_tokens_index, ptr_t lhs, ptr_t rhs)
{
	auto const compare_result = this->memory.compare_pointers(lhs, rhs);
	if (!compare_result.has_value())
	{
		this->report_error(src_tokens_index, "comparing unrelated pointers");
		return lhs < rhs ? -1 : lhs == rhs ? 0 : 1;
	}
	else
	{
		return compare_result.get();
	}
}

bool executor_context::compare_pointers_equal(ptr_t lhs, ptr_t rhs)
{
	auto const compare_result = this->memory.compare_pointers(lhs, rhs);
	return compare_result.has_value() && compare_result.get() == 0;
}

ptr_t executor_context::pointer_add_signed(
	uint32_t src_tokens_index,
	ptr_t address,
	int64_t offset,
	type const *object_type,
	ast::typespec_view pointer_type
)
{
	auto const result = this->memory.do_pointer_arithmetic(address, offset, object_type);
	if (result == 0)
	{
		this->report_error(
			src_tokens_index,
			bz::format("invalid pointer arithmetic operation with type '{}' and offset {}", pointer_type, offset)
		);
	}
	return result;
}

ptr_t executor_context::pointer_add_unsigned(
	uint32_t src_tokens_index,
	ptr_t address,
	uint64_t offset,
	type const *object_type,
	ast::typespec_view pointer_type
)
{
	auto const result = offset > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())
		? 0
		: this->memory.do_pointer_arithmetic(address, static_cast<int64_t>(offset), object_type);
	if (result == 0)
	{
		this->report_error(
			src_tokens_index,
			bz::format("invalid pointer arithmetic operation with type '{}' and offset {}", pointer_type, offset)
		);
	}
	return result;
}

ptr_t executor_context::pointer_sub_signed(
	uint32_t src_tokens_index,
	ptr_t address,
	int64_t offset,
	type const *object_type,
	ast::typespec_view pointer_type
)
{
	auto const result = offset == std::numeric_limits<int64_t>::min()
		? 0
		: this->memory.do_pointer_arithmetic(address, -offset, object_type);
	if (result == 0)
	{
		this->report_error(
			src_tokens_index,
			bz::format("invalid pointer arithmetic operation with type '{}' and offset {}", pointer_type, offset)
		);
	}
	return result;
}

ptr_t executor_context::pointer_sub_unsigned(
	uint32_t src_tokens_index,
	ptr_t address,
	uint64_t offset,
	type const *object_type,
	ast::typespec_view pointer_type
)
{
	constexpr auto max_value = static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + 1;
	auto const result = offset > max_value
		? 0
		: this->memory.do_pointer_arithmetic(
			address,
			offset == max_value ? std::numeric_limits<int64_t>::min() : -static_cast<int64_t>(offset),
			object_type
		);
	if (result == 0)
	{
		this->report_error(
			src_tokens_index,
			bz::format("invalid pointer arithmetic operation with type '{}' and offset {}", pointer_type, offset)
		);
	}
	return result;
}

int64_t executor_context::pointer_difference(
	uint32_t src_tokens_index,
	ptr_t lhs,
	ptr_t rhs,
	type const *object_type,
	ast::typespec_view pointer_type
)
{
	auto const result = this->memory.do_pointer_difference(lhs, rhs, object_type);
	if (!result.has_value())
	{
		this->report_error(
			src_tokens_index,
			bz::format("invalid pointer arithmetic operation with type '{}'", pointer_type)
		);
		return 0;
	}
	else
	{
		return result.get();
	}
}

int64_t executor_context::pointer_difference_unchecked(ptr_t lhs, ptr_t rhs, size_t stride)
{
	return this->memory.do_pointer_difference_unchecked(lhs, rhs, stride);
}

void executor_context::advance(void)
{
	if (this->next_instruction != nullptr)
	{
		auto const next_instruction_index = this->next_instruction - this->instructions.data();
		this->current_instruction = this->next_instruction;
		this->current_instruction_value = this->instruction_values.data() + next_instruction_index + this->alloca_offset;
		this->next_instruction = nullptr;
	}
	else if (this->returned)
	{
		bz_unreachable;
	}
	else
	{
		bz_assert(!this->current_instruction->is_terminator());
		this->current_instruction += 1;
	}
}

} // namespace comptime
