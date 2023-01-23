#include "executor_context.h"
#include "execute.h"
#include "codegen.h"
#include "global_data.h"
#include "ast/statement.h"
#include "resolve/consteval.h"

namespace comptime
{

static ctx::source_highlight make_source_highlight(
	lex::src_tokens const &src_tokens,
	bz::u8string message
)
{
	return ctx::source_highlight{
		.file_id = src_tokens.pivot->src_pos.file_id,
		.line = src_tokens.pivot->src_pos.line,

		.src_begin = src_tokens.begin->src_pos.begin,
		.src_pivot = src_tokens.pivot->src_pos.begin,
		.src_end = (src_tokens.end - 1)->src_pos.end,

		.first_suggestion = ctx::suggestion_range{},
		.second_suggestion = ctx::suggestion_range{},

		.message = std::move(message),
	};
}

static ctx::error make_diagnostic(
	ctx::warning_kind kind,
	lex::src_tokens const &src_tokens,
	bz::u8string message,
	bz::vector<ctx::source_highlight> notes
)
{
	return ctx::error{
		.kind = kind,
		.src_highlight = make_source_highlight(src_tokens, std::move(message)),
		.notes = std::move(notes),
		.suggestions = {},
	};
}

executor_context::executor_context(codegen_context *_codegen_ctx)
	: memory(
		  _codegen_ctx->get_memory_segment_info(),
		  &_codegen_ctx->global_memory,
		  _codegen_ctx->is_64_bit(),
		  _codegen_ctx->is_little_endian() == (std::endian::native == std::endian::little)
	  ),
	  codegen_ctx(_codegen_ctx)
{}

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

void executor_context::add_call_stack_notes(bz::vector<ctx::source_highlight> &notes) const
{
	notes.reserve(notes.size() + this->call_stack.size() + 1);
	auto call_src_tokens = this->call_src_tokens_index;
	auto func_body = this->current_function->func_body;
	for (auto const &info : this->call_stack.reversed())
	{
		notes.push_back(make_source_highlight(
			info.func->src_tokens[call_src_tokens],
			bz::format("in call to '{}'", func_body->get_signature())
		));
		call_src_tokens = info.call_src_tokens_index;
		func_body = info.func->func_body;
	}
	bz_assert(func_body == nullptr);
	notes.push_back(make_source_highlight(
		this->execution_start_src_tokens,
		"while evaluating expression at compile time"
	));
}

void executor_context::report_error(uint32_t error_index)
{
	this->has_error = true;
	auto const &info = this->get_error_info(error_index);
	bz::vector<ctx::source_highlight> notes = {};
	this->add_call_stack_notes(notes);

	this->diagnostics.push_back(make_diagnostic(
		ctx::warning_kind::_last,
		info.src_tokens,
		info.message,
		std::move(notes)
	));
}

void executor_context::report_error(
	uint32_t src_tokens_index,
	bz::u8string message,
	bz::vector<ctx::source_highlight> notes
)
{
	this->has_error = true;
	this->add_call_stack_notes(notes);
	auto const &src_tokens = this->get_src_tokens(src_tokens_index);
	this->diagnostics.push_back(make_diagnostic(
		ctx::warning_kind::_last,
		src_tokens,
		std::move(message),
		std::move(notes)
	));
}

void executor_context::report_warning(
	ctx::warning_kind kind,
	uint32_t src_tokens_index,
	bz::u8string message,
	bz::vector<ctx::source_highlight> notes
)
{
	this->add_call_stack_notes(notes);
	auto const &src_tokens = this->get_src_tokens(src_tokens_index);
	this->diagnostics.push_back(make_diagnostic(
		kind,
		src_tokens,
		std::move(message),
		std::move(notes)
	));
}

ctx::source_highlight executor_context::make_note(uint32_t src_tokens_index, bz::u8string message)
{
	return make_source_highlight(this->get_src_tokens(src_tokens_index), std::move(message));
}

bool executor_context::is_option_set(bz::u8string_view option)
{
	return defines.contains(option);
}

ptr_t executor_context::get_global(uint32_t index)
{
	bz_assert(index < this->memory.global_memory->objects.size());
	return this->memory.global_memory->objects[index].address;
}

ptr_t executor_context::add_global_array_data(type const *elem_type, bz::array_view<uint8_t const> data)
{
	bz_assert(data.size() % elem_type->size == 0);
	auto const size = data.size() / elem_type->size;
	auto const array_type = this->codegen_ctx->get_array_type(elem_type, size);
	auto const index = this->memory.global_memory->add_object(array_type, data);
	return this->get_global(index);
}

instruction_value executor_context::get_arg(uint32_t index)
{
	return this->args[index];
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

static constexpr size_t max_call_depth = 1024;

void executor_context::call_function(uint32_t call_src_tokens_index, function const *func, uint32_t args_index)
{
	if (this->memory.stack.stack_frames.size() >= max_call_depth)
	{
		this->report_error(call_src_tokens_index, bz::format("maximum call depth ({}) exceeded in compile time execution", max_call_depth));
		return;
	}

	if (func->instructions.empty())
	{
		this->codegen_ctx->resolve_function(this->get_src_tokens(call_src_tokens_index), *func->func_body);
		if (func->instructions.empty())
		{
			generate_code(*const_cast<function *>(func), *this->codegen_ctx);
		}
	}

	auto const good = this->memory.push_stack_frame(func->allocas);
	if (!good)
	{
		this->memory.pop_stack_frame();
		this->report_error(call_src_tokens_index, "stack overflow in compile time execution");
		return;
	}

	// bz::log("{}", to_string(*func));
	this->call_stack.push_back({
		.func = this->current_function,
		.call_inst = this->current_instruction,
		.call_src_tokens_index = this->call_src_tokens_index,
		.args = std::move(this->args),
		.instruction_values = std::move(this->instruction_values),
	});

	auto const &prev_instruction_values = this->call_stack.back().instruction_values;

	auto const &call_args = this->current_function->call_args[args_index];
	this->current_function = func;
	this->args = bz::fixed_vector<instruction_value>(
		call_args.transform([&prev_instruction_values](auto const index) {
			return prev_instruction_values[index.index];
		})
	);
	this->instruction_values = bz::fixed_vector<instruction_value>(func->allocas.size() + func->instructions.size());
	bz_assert(func->instructions.not_empty());
	this->instructions = func->instructions;
	this->alloca_offset = static_cast<uint32_t>(func->allocas.size());
	this->call_src_tokens_index = call_src_tokens_index;

	auto const &alloca_objects = this->memory.stack.stack_frames.back().objects;
	bz_assert(alloca_objects.size() == this->alloca_offset);
	for (auto const i : bz::iota(0, this->alloca_offset))
	{
		this->instruction_values[i].ptr = alloca_objects[i].address;
	}
	this->next_instruction = this->instructions.data();
}

switch_info_t const &executor_context::get_switch_info(uint32_t index) const
{
	bz_assert(index < this->current_function->switch_infos.size());
	return this->current_function->switch_infos[index];
}

error_info_t const &executor_context::get_error_info(uint32_t index) const
{
	bz_assert(index < this->current_function->errors.size());
	return this->current_function->errors[index];
}

lex::src_tokens const &executor_context::get_src_tokens(uint32_t index) const
{
	bz_assert(index < this->current_function->src_tokens.size());
	return this->current_function->src_tokens[index];
}

slice_construction_check_info_t const &executor_context::get_slice_construction_info(uint32_t index) const
{
	bz_assert(index < this->current_function->slice_construction_check_infos.size());
	return this->current_function->slice_construction_check_infos[index];
}

pointer_arithmetic_check_info_t const &executor_context::get_pointer_arithmetic_info(uint32_t index) const
{
	bz_assert(index < this->current_function->pointer_arithmetic_check_infos.size());
	return this->current_function->pointer_arithmetic_check_infos[index];
}

memory_access_check_info_t const &executor_context::get_memory_access_info(uint32_t index) const
{
	bz_assert(index < this->current_function->memory_access_check_infos.size());
	return this->current_function->memory_access_check_infos[index];
}

memory::call_stack_info_t executor_context::get_call_stack_info(uint32_t src_tokens_index) const
{
	return {
		.src_tokens = this->get_src_tokens(src_tokens_index),
		.call_stack = bz::fixed_vector<memory::call_stack_info_t::src_tokens_function_pair_t>(
			bz::iota(0, this->call_stack.size() + 1)
				.transform([&](auto const i) -> memory::call_stack_info_t::src_tokens_function_pair_t {
					if (i == 0)
					{
						return {
							.src_tokens = this->execution_start_src_tokens,
							.body = nullptr,
						};
					}
					else if (i == this->call_stack.size())
					{
						auto const &info = this->call_stack[i - 1];
						return {
							.src_tokens = info.func->src_tokens[this->call_src_tokens_index],
							.body = this->current_function->func_body,
						};
					}
					else
					{
						auto const &info = this->call_stack[i - 1];
						auto const &next_info = this->call_stack[i];
						auto result = memory::call_stack_info_t::src_tokens_function_pair_t{
							.src_tokens = info.func->src_tokens[next_info.call_src_tokens_index],
							.body = next_info.func->func_body,
						};
						return result;
					}
				}
			)
		),
	};
}

ptr_t executor_context::malloc(uint32_t src_tokens_index, type const *type, uint64_t count)
{
	auto const result = this->memory.heap.allocate(this->get_call_stack_info(src_tokens_index), type, count);
	if (result == 0)
	{
		this->report_error(src_tokens_index, bz::format("unable to allocate a region of size {}", type->size * count));
	}
	return result;
}

void executor_context::free(uint32_t src_tokens_index, ptr_t address)
{
	auto const result = this->memory.heap.free(this->get_call_stack_info(src_tokens_index), address);
	switch (result)
	{
	case memory::free_result::good:
		return;
	case memory::free_result::double_free:
		this->report_error(src_tokens_index, "double free detected");
		break;
	case memory::free_result::unknown_address:
	case memory::free_result::address_inside_object:
		this->report_error(src_tokens_index, "invalid free");
		break;
	}
}

void executor_context::check_dereference(uint32_t src_tokens_index, ptr_t address, type const *object_type, ast::typespec_view object_typespec)
{
	auto const is_good = this->memory.check_dereference(address, object_type);
	if (!is_good)
	{
		this->report_error(
			src_tokens_index,
			bz::format("invalid memory access of an object of type '{}'", object_typespec)
		);
	}
}

void executor_context::check_str_construction(uint32_t src_tokens_index, ptr_t begin, ptr_t end)
{
	auto const elem_type = this->codegen_ctx->get_builtin_type(builtin_type_kind::i8);
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

ptr_t executor_context::pointer_add_unchecked(ptr_t address, int32_t offset, type const *object_type)
{
	auto const result = this->memory.do_pointer_arithmetic(address, offset, object_type);
	bz_assert(result != 0);
	return result;
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

ptr_t executor_context::gep(ptr_t address, type const *object_type, uint64_t index)
{
	return this->memory.do_gep(address, object_type, index);
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

void executor_context::start_lifetime(ptr_t address)
{
	this->memory.start_lifetime(address);
}

void executor_context::end_lifetime(ptr_t address)
{
	this->memory.end_lifetime(address);
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
		if (this->call_stack.empty())
		{
			return;
		}

		auto &prev_call = this->call_stack.back();

		this->current_function = prev_call.func;
		this->args = std::move(prev_call.args);
		this->instruction_values = std::move(prev_call.instruction_values);
		this->instructions = this->current_function->instructions;
		this->alloca_offset = static_cast<uint32_t>(this->current_function->allocas.size());
		this->call_src_tokens_index = prev_call.call_src_tokens_index;

		this->current_instruction = prev_call.call_inst + 1;
		auto const instruction_index = this->current_instruction - this->instructions.data();
		this->current_instruction_value = this->instruction_values.data() + instruction_index + this->alloca_offset;
		this->returned = false;
		*(this->current_instruction_value - 1) = this->ret_value;

		this->call_stack.pop_back();
		this->memory.pop_stack_frame();
	}
	else
	{
		bz_assert(!this->current_instruction->is_terminator());
		bz_assert(this->current_instruction != &this->instructions.back());
		bz_assert(this->current_instruction_value != &this->instruction_values.back());
		this->current_instruction += 1;
		this->current_instruction_value += 1;
	}
}

bool executor_context::check_memory_leaks(void)
{
	bool result = false;
	for (auto const &allocation : this->memory.heap.allocations)
	{
		if (!allocation.is_freed)
		{
			bz_assert(allocation.alloc_info.call_stack.not_empty());

			auto notes = bz::vector<ctx::source_highlight>();
			notes.reserve(allocation.alloc_info.call_stack.size());
			if (allocation.alloc_info.call_stack.back().body != nullptr)
			{
				notes.push_back(make_source_highlight(
					allocation.alloc_info.call_stack.back().src_tokens,
					bz::format("allocation was made in call to '{}'", allocation.alloc_info.call_stack.back().body->get_signature())
				));
			}
			else
			{
				notes.push_back(make_source_highlight(
					allocation.alloc_info.call_stack.back().src_tokens,
					"allocation was made while evaluating expression at compile time"
				));
			}
			for (auto const &call : allocation.alloc_info.call_stack.slice(0, allocation.alloc_info.call_stack.size() - 1).reversed())
			{
				if (call.body != nullptr)
				{
					notes.push_back(make_source_highlight(
						call.src_tokens,
						bz::format("in call to '{}'", call.body->get_signature())
					));
				}
				else
				{
					notes.push_back(make_source_highlight(
						call.src_tokens,
						"while evaluating expression at compile time"
					));
				}
			}

			result = true;
			this->diagnostics.push_back(make_diagnostic(
				ctx::warning_kind::_last,
				allocation.alloc_info.src_tokens,
				"allocation was never freed",
				std::move(notes)
			));
		}
	}
	return result;
}

ast::constant_value executor_context::execute_expression(ast::expression const &expr, function const &func)
{
	this->current_function = &func;

	this->instruction_values = bz::fixed_vector<instruction_value>(func.allocas.size() + func.instructions.size());
	this->instructions = func.instructions;
	this->alloca_offset = static_cast<uint32_t>(func.allocas.size());
	this->execution_start_src_tokens = expr.src_tokens;

	this->current_instruction = func.instructions.data();
	this->current_instruction_value = this->instruction_values.data() + this->alloca_offset;

	[[maybe_unused]] auto const good = this->memory.push_stack_frame(func.allocas);
	bz_assert(good);
	auto const &alloca_objects = this->memory.stack.stack_frames.back().objects;
	bz_assert(alloca_objects.size() == this->alloca_offset);
	for (auto const i : bz::iota(0, this->alloca_offset))
	{
		this->instruction_values[i].ptr = alloca_objects[i].address;
	}

	while (!this->returned && !this->has_error)
	{
		execute_current_instruction(*this);
		this->advance();
	}

	if (this->has_error)
	{
		return ast::constant_value();
	}
	else if (this->check_memory_leaks())
	{
		return ast::constant_value();
	}

	if (func.return_type->is_void())
	{
		if (expr.get_expr_type().is_typename())
		{
			bz_assert(expr.is_typename());
			return ast::constant_value(expr.get_typename());
		}
		else
		{
			return ast::constant_value::get_void();
		}
	}
	else
	{
		bz_assert(func.return_type->is_pointer());
		auto const result_address = this->ret_value.ptr;
		auto const result_object = this->memory.stack.get_stack_object(result_address);
		bz_assert(result_object != nullptr);

		return memory::constant_value_from_object(
			result_object->object_type,
			result_object->memory.data(),
			expr.get_expr_type(),
			this->codegen_ctx->machine_parameters.endianness,
			this->memory
		);
	}
}

} // namespace comptime
