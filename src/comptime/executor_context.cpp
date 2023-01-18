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
	: memory(_codegen_ctx->get_memory_segment_info()),
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
		generate_code(*const_cast<function *>(func), *this->codegen_ctx);
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

ptr_t executor_context::malloc(uint32_t src_tokens_index, type const *type, uint64_t count)
{
	auto const result = this->memory.heap.allocate(this->get_src_tokens(src_tokens_index), type, count);
	if (result == 0)
	{
		this->report_error(src_tokens_index, bz::format("unable to allocate a region of size {}", type->size * count));
	}
	return result;
}

void executor_context::free(uint32_t src_tokens_index, ptr_t address)
{
	auto const result = this->memory.heap.free(this->get_src_tokens(src_tokens_index), address);
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

static bool is_native(endianness_kind endianness)
{
	return (endianness == endianness_kind::little) == (std::endian::native == std::endian::little);
}

template<std::size_t Size>
using uint_t = bz::meta::conditional<
	Size == 1, uint8_t, bz::meta::conditional<
	Size == 2, uint16_t, bz::meta::conditional<
	Size == 4, uint32_t, bz::meta::conditional<
	Size == 8, uint64_t,
	void
>>>>;

template<typename Int>
static Int byteswap(Int value)
{
	if constexpr (sizeof (Int) == 1)
	{
		return value;
	}
	else if constexpr (sizeof (Int) == 2)
	{
		value = ((value & uint16_t(0xff00)) >> 8) | ((value & uint16_t(0x00ff)) << 8);
		return value;
	}
	else if constexpr (sizeof (Int) == 4)
	{
		value = ((value & uint32_t(0xffff'0000)) >> 16) | ((value & uint32_t(0x0000'ffff)) << 16);
		value = ((value & uint32_t(0xff00'ff00)) >> 8) | ((value & uint32_t(0x00ff'00ff)) << 8);
		return value;
	}
	else if constexpr (sizeof (Int) == 8)
	{
		value = ((value & uint64_t(0xffff'ffff'0000'0000)) >> 32) | ((value & uint64_t(0x0000'0000'ffff'ffff)) << 32);
		value = ((value & uint64_t(0xffff'0000'ffff'0000)) >> 16) | ((value & uint64_t(0x0000'ffff'0000'ffff)) << 16);
		value = ((value & uint64_t(0xff00'ff00'ff00'ff00)) >> 8) | ((value & uint64_t(0x00ff'00ff'00ff'00ff)) << 8);
		return value;
	}
	else
	{
		static_assert(bz::meta::always_false<Int>);
	}
}

template<typename T>
static T load(uint8_t const *mem, endianness_kind endianness)
{
	if constexpr (bz::meta::is_same<T, bool>)
	{
		bz_assert(*mem <= 1);
		return *mem != 0;
	}
	else
	{
		if (is_native(endianness))
		{
			T result{};
			std::memcpy(&result, mem, sizeof (T));
			return result;
		}
		else
		{
			uint_t<sizeof (T)> int_result = 0;
			std::memcpy(&int_result, mem, sizeof (T));
			return bit_cast<T>(byteswap(int_result));
		}
	}
}

template<typename T, typename U>
static void load_array(uint8_t const *mem, U *dest, size_t size, endianness_kind endianness)
{
	if (is_native(endianness))
	{
		if constexpr (sizeof (T) == sizeof (U))
		{
			std::memcpy(dest, mem, size * sizeof (T));
		}
		else
		{
			for (auto const i : bz::iota(0, size))
			{
				T elem{};
				std::memcpy(&elem, mem + i * sizeof (T), sizeof (T));
				*(dest + i) = elem;
			}
		}
	}
	else
	{
		for (auto const i : bz::iota(0, size))
		{
			uint_t<sizeof (T)> int_result = 0;
			std::memcpy(&int_result, mem + i * sizeof (T), sizeof (T));
			*(dest + i) = bit_cast<T>(byteswap(int_result));
		}
	}
}

static ast::constant_value constant_value_from_object(
	type const *object_type,
	uint8_t const *mem,
	ast::typespec_view ts,
	endianness_kind endianness
)
{
	ts = ast::remove_const_or_consteval(ts);
	if (object_type->is_builtin())
	{
		if (ts.is<ast::ts_base_type>())
		{
			auto const kind = ts.get<ast::ts_base_type>().info->kind;
			switch (kind)
			{
			case ast::type_info::int8_:
				return ast::constant_value(static_cast<int64_t>(load<int8_t>(mem, endianness)));
			case ast::type_info::int16_:
				return ast::constant_value(static_cast<int64_t>(load<int16_t>(mem, endianness)));
			case ast::type_info::int32_:
				return ast::constant_value(static_cast<int64_t>(load<int32_t>(mem, endianness)));
			case ast::type_info::int64_:
				return ast::constant_value(static_cast<int64_t>(load<int64_t>(mem, endianness)));
			case ast::type_info::uint8_:
				return ast::constant_value(static_cast<uint64_t>(load<uint8_t>(mem, endianness)));
			case ast::type_info::uint16_:
				return ast::constant_value(static_cast<uint64_t>(load<uint16_t>(mem, endianness)));
			case ast::type_info::uint32_:
				return ast::constant_value(static_cast<uint64_t>(load<uint32_t>(mem, endianness)));
			case ast::type_info::uint64_:
				return ast::constant_value(static_cast<uint64_t>(load<uint64_t>(mem, endianness)));
			case ast::type_info::float32_:
				return ast::constant_value(load<float32_t>(mem, endianness));
			case ast::type_info::float64_:
				return ast::constant_value(load<float64_t>(mem, endianness));
			case ast::type_info::char_:
				return ast::constant_value(load<bz::u8char>(mem, endianness));
			case ast::type_info::bool_:
				return ast::constant_value(load<bool>(mem, endianness));
			default:
				bz_unreachable;
			}
		}
		else
		{
			bz_assert(ts.is<ast::ts_enum>());
			auto const decl = ts.get<ast::ts_enum>().decl;

			bz_assert(object_type->is_integer_type());
			switch (object_type->get_builtin_kind())
			{
			case builtin_type_kind::i8:
				return ast::constant_value::get_enum(decl, static_cast<uint64_t>(load<uint8_t>(mem, endianness)));
			case builtin_type_kind::i16:
				return ast::constant_value::get_enum(decl, static_cast<uint64_t>(load<uint16_t>(mem, endianness)));
			case builtin_type_kind::i32:
				return ast::constant_value::get_enum(decl, static_cast<uint64_t>(load<uint32_t>(mem, endianness)));
			case builtin_type_kind::i64:
				return ast::constant_value::get_enum(decl, static_cast<uint64_t>(load<uint64_t>(mem, endianness)));
			default:
				bz_unreachable;
			}
		}
	}
	else if (object_type->is_pointer())
	{
		return ast::constant_value();
	}
	else if (object_type->is_aggregate())
	{
		if (ts.is<ast::ts_tuple>())
		{
			auto const aggregate_types = object_type->get_aggregate_types();
			auto const aggregate_offsets = object_type->get_aggregate_offsets();
			auto const tuple_types = ts.get<ast::ts_tuple>().types.as_array_view();
			bz_assert(aggregate_types.size() == tuple_types.size());

			auto result = ast::constant_value();
			auto &result_vec = result.emplace<ast::constant_value::tuple>();
			result_vec.reserve(tuple_types.size());
			for (auto const i : bz::iota(0, tuple_types.size()))
			{
				result_vec.push_back(constant_value_from_object(
					aggregate_types[i],
					mem + aggregate_offsets[i],
					tuple_types[i],
					endianness
				));
				if (result_vec.back().is_null())
				{
					result.clear();
					break;
				}
			}

			return result;
		}
		else if (ts.is<ast::ts_optional>())
		{
			auto const aggregate_types = object_type->get_aggregate_types();
			auto const aggregate_offsets = object_type->get_aggregate_offsets();
			bz_assert(aggregate_types.size() == 2);

			auto const has_value = load<bool>(mem + aggregate_offsets[1], endianness);
			if (has_value)
			{
				return constant_value_from_object(aggregate_types[0], mem, ts.get<ast::ts_optional>(), endianness);
			}
			else
			{
				return ast::constant_value::get_null();
			}
		}
		else if (ts.is<ast::ts_base_type>())
		{
			auto const info = ts.get<ast::ts_base_type>().info;
			if (info->kind != ast::type_info::aggregate)
			{
				// str or __null_t
				if (info->kind == ast::type_info::null_t_)
				{
					return ast::constant_value::get_null();
				}
				else
				{
					// TODO: implement str returning
					return ast::constant_value();
				}
			}

			auto const aggregate_types = object_type->get_aggregate_types();
			auto const aggregate_offsets = object_type->get_aggregate_offsets();
			auto const members = info->member_variables.as_array_view();
			bz_assert(aggregate_types.size() == members.size());

			auto result = ast::constant_value();
			auto &result_vec = result.emplace<ast::constant_value::aggregate>();
			result_vec.reserve(members.size());
			for (auto const i : bz::iota(0, members.size()))
			{
				result_vec.push_back(constant_value_from_object(
					aggregate_types[i],
					mem + aggregate_offsets[i],
					members[i]->get_type(),
					endianness
				));
				if (result_vec.back().is_null())
				{
					result.clear();
					break;
				}
			}

			return result;
		}
		else
		{
			// array slice
			return ast::constant_value();
		}
	}
	else if (object_type->is_array())
	{
		bz_assert(ts.is<ast::ts_array>());
		auto const info = resolve::get_flattened_array_type_and_size(ts);
		if (resolve::is_special_array_type(ts))
		{
			bz_assert(info.elem_type.is<ast::ts_base_type>());
			bz_assert([&]() {
				type const *elem_type = object_type;
				while (elem_type->is_array())
				{
					elem_type = elem_type->get_array_element_type();
				}

				return elem_type->size * info.size == object_type->size;
			}());
			auto const kind = info.elem_type.get<ast::ts_base_type>().info->kind;

			auto result = ast::constant_value();
			switch (kind)
			{
			case ast::type_info::int8_:
			{
				auto &result_array = result.emplace<ast::constant_value::sint_array>(info.size);
				load_array<int8_t>(mem, result_array.data(), info.size, endianness);
				break;
			}
			case ast::type_info::int16_:
			{
				auto &result_array = result.emplace<ast::constant_value::sint_array>(info.size);
				load_array<int16_t>(mem, result_array.data(), info.size, endianness);
				break;
			}
			case ast::type_info::int32_:
			{
				auto &result_array = result.emplace<ast::constant_value::sint_array>(info.size);
				load_array<int32_t>(mem, result_array.data(), info.size, endianness);
				break;
			}
			case ast::type_info::int64_:
			{
				auto &result_array = result.emplace<ast::constant_value::sint_array>(info.size);
				load_array<int64_t>(mem, result_array.data(), info.size, endianness);
				break;
			}
			case ast::type_info::uint8_:
			{
				auto &result_array = result.emplace<ast::constant_value::uint_array>(info.size);
				load_array<uint8_t>(mem, result_array.data(), info.size, endianness);
				break;
			}
			case ast::type_info::uint16_:
			{
				auto &result_array = result.emplace<ast::constant_value::uint_array>(info.size);
				load_array<uint16_t>(mem, result_array.data(), info.size, endianness);
				break;
			}
			case ast::type_info::uint32_:
			{
				auto &result_array = result.emplace<ast::constant_value::uint_array>(info.size);
				load_array<uint32_t>(mem, result_array.data(), info.size, endianness);
				break;
			}
			case ast::type_info::uint64_:
			{
				auto &result_array = result.emplace<ast::constant_value::uint_array>(info.size);
				load_array<uint64_t>(mem, result_array.data(), info.size, endianness);
				break;
			}
			case ast::type_info::float32_:
			{
				auto &result_array = result.emplace<ast::constant_value::float32_array>(info.size);
				load_array<float32_t>(mem, result_array.data(), info.size, endianness);
				break;
			}
			case ast::type_info::float64_:
			{
				auto &result_array = result.emplace<ast::constant_value::float64_array>(info.size);
				load_array<float64_t>(mem, result_array.data(), info.size, endianness);
				break;
			}
			default:
				bz_unreachable;
			}

			return result;
		}
		else
		{
			auto const elem_type = [&]() {
				auto result = object_type->get_array_element_type();
				while (result->is_array())
				{
					result = result->get_array_element_type();
				}
				return result;
			}();
			bz_assert(elem_type->size * info.size == object_type->size);

			auto result = ast::constant_value();
			auto &result_array = result.emplace<ast::constant_value::array>();
			result_array.reserve(info.size);

			auto mem_it = mem;
			auto const mem_end = mem + object_type->size;
			for (; mem_it != mem_end; mem_it += elem_type->size)
			{
				result_array.push_back(constant_value_from_object(elem_type, mem, info.elem_type, endianness));
				if (result_array.back().is_null())
				{
					result.clear();
					break;
				}
			}

			return result;
		}
	}
	else
	{
		bz_unreachable;
	}
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

		return constant_value_from_object(
			result_object->object_type,
			result_object->memory.data(),
			expr.get_expr_type(),
			this->codegen_ctx->machine_parameters.endianness
		);
	}
}

} // namespace comptime
