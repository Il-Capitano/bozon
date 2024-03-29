#include "memory.h"
#include "ast/statement.h"
#include "overflow_operations.h"
#include "global_data.h"

namespace comptime::memory
{

static constexpr uint8_t heap_object_align = 16;

static void add_allocation_info(bz::vector<error_reason_t> &reasons, call_stack_info_t const &alloc_info)
{
	bz_assert(alloc_info.call_stack.not_empty());
	if (alloc_info.call_stack.back().body != nullptr)
	{
		reasons.push_back({
			alloc_info.call_stack.back().src_tokens,
			bz::format("allocation was made in call to '{}'", alloc_info.call_stack.back().body->get_signature())
		});
	}
	else
	{
		reasons.push_back({
			alloc_info.call_stack.back().src_tokens,
			"allocation was made while evaluating expression at compile time"
		});
	}

	for (auto const &call : alloc_info.call_stack.slice(0, alloc_info.call_stack.size() - 1).reversed())
	{
		if (call.body != nullptr)
		{
			reasons.push_back({ call.src_tokens, bz::format("in call to '{}'", call.body->get_signature()) });
		}
		else
		{
			reasons.push_back({ call.src_tokens, "while evaluating expression at compile time" });
		}
	}
}

static void add_free_info(bz::vector<error_reason_t> &reasons, call_stack_info_t const &free_info)
{
	reasons.push_back({ free_info.src_tokens, "allocation was freed here" });
	for (auto const &call : free_info.call_stack.reversed())
	{
		if (call.body != nullptr)
		{
			reasons.push_back({ call.src_tokens, bz::format("in call to '{}'", call.body->get_signature()) });
		}
		else
		{
			reasons.push_back({ call.src_tokens, "while evaluating expression at compile time" });
		}
	}
}

static void fill_padding_array(bz::array_view<uint8_t> data, type const *object_type, size_t size);

static void fill_padding_single(bz::array_view<uint8_t> data, type const *object_type)
{
	bz_assert(data.size() == object_type->size);
	if (!object_type->has_padding())
	{
		return;
	}
	else if (object_type->is_aggregate())
	{
		auto const types = object_type->get_aggregate_types();
		auto const offsets = object_type->get_aggregate_offsets();

		if (types.empty())
		{
			bz_assert(data.size() == 1);
			data[0] |= memory_properties::is_padding;
			return;
		}

		auto const elem_count = types.size();
		for (auto const i : bz::iota(0, elem_count))
		{
			auto const elem_type = types[i];
			auto const offset = offsets[i];
			fill_padding_single(data.slice(offset, offset + elem_type->size), elem_type);
			auto const next_offset = i + 1 == elem_count ? object_type->size : offsets[i + 1];
			if (offset + elem_type->size != next_offset)
			{
				for (auto &value : data.slice(offset + elem_type->size, next_offset))
				{
					value |= memory_properties::is_padding;
				}
			}
		}
	}
	else if (object_type->is_array())
	{
		fill_padding_array(data, object_type->get_array_element_type(), object_type->get_array_size());
	}
}

static void fill_padding_array(bz::array_view<uint8_t> data, type const *object_type, size_t size)
{
	bz_assert(object_type->has_padding());
	auto const object_size = object_type->size;
	fill_padding_single(data.slice(0, object_size), object_type);

	for (auto const i : bz::iota(object_size, size * object_size))
	{
		data[i] |= (data[i - object_size] & memory_properties::is_padding);
	}
}

memory_properties::memory_properties(type const *object_type, size_t size, uint8_t bits)
	: data(size, bits)
{
	if (object_type->has_padding())
	{
		if (size == object_type->size)
		{
			fill_padding_single(data, object_type);
		}
		else
		{
			bz_assert(size % object_type->size == 0);
			fill_padding_array(data, object_type, size / object_type->size);
		}
	}
}

bool memory_properties::is_all(size_t begin, size_t end, uint8_t bits, uint8_t exception_bits) const
{
	for (auto const value : this->data.slice(begin, end))
	{
		if ((value & (bits | exception_bits)) == 0)
		{
			return false;
		}
	}
	return true;
}

bool memory_properties::is_none(size_t begin, size_t end, uint8_t bits, uint8_t exception_bits) const
{
	for (auto const value : this->data.slice(begin, end))
	{
		if ((value & (bits | exception_bits)) == bits)
		{
			return false;
		}
	}
	return true;
}

void memory_properties::set_range(size_t begin, size_t end, uint8_t bits)
{
	for (auto &value : this->data.slice(begin, end))
	{
		value |= bits;
	}
}

void memory_properties::erase_range(size_t begin, size_t end, uint8_t bits)
{
	for (auto &value : this->data.slice(begin, end))
	{
		value &= ~bits;
	}
}

void memory_properties::clear(void)
{
	this->data.clear();
}

stack_object::stack_object(lex::src_tokens const &_object_src_tokens, ptr_t _address, type const *_object_type, bool is_always_initialized)
	: address(_address),
	  object_type(_object_type),
	  memory(_object_type->size, 0),
	  properties(_object_type, _object_type->size, is_always_initialized ? memory_properties::is_alive : 0),
	  object_src_tokens(_object_src_tokens)
{}

size_t stack_object::object_size(void) const
{
	return this->memory.size();
}

void stack_object::start_lifetime(ptr_t begin, ptr_t end)
{
	bz_assert(this->properties.is_none(
		begin - this->address,
		end - this->address,
		memory_properties::is_alive,
		memory_properties::is_padding
	));
	this->properties.set_range(begin - this->address, end - this->address, memory_properties::is_alive);
}

void stack_object::end_lifetime(ptr_t begin, ptr_t end)
{
	this->properties.erase_range(begin - this->address, end - this->address, memory_properties::is_alive);
}

bool stack_object::is_alive(ptr_t begin, ptr_t end) const
{
	return this->properties.is_all(
		begin - this->address,
		end - this->address,
		memory_properties::is_alive,
		memory_properties::is_padding
	);
}

uint8_t *stack_object::get_memory(ptr_t address)
{
	bz_assert(address >= this->address && address <= this->address + this->object_size());
	bz_assert(this->memory.not_empty());
	return this->memory.data() + (address - this->address);
}

uint8_t const *stack_object::get_memory(ptr_t address) const
{
	bz_assert(address >= this->address && address <= this->address + this->object_size());
	bz_assert(this->memory.not_empty());
	return this->memory.data() + (address - this->address);
}

bool stack_object::check_dereference(ptr_t address, type const *subobject_type) const
{
	if (address < this->address || address >= this->address + this->object_size() || this->memory.empty())
	{
		bz_unreachable;
	}
	if (!this->is_alive(address, address + subobject_type->size))
	{
		return false;
	}

	bz_assert(contained_in_object(this->object_type, address - this->address, subobject_type));
	return true;
}

bz::u8string stack_object::get_dereference_error_reason(ptr_t address, type const *object_type) const
{
	bz_assert(!this->is_alive(address, address + object_type->size));
	return "lifetime of the stack object has already ended";
}

bool stack_object::check_slice_construction(ptr_t begin, ptr_t end, bool end_is_one_past_the_end, type const *elem_type) const
{
	if (begin == end)
	{
		return true;
	}

	if (
		this->memory.empty()
		|| begin < this->address || begin >= this->address + this->object_size()
		|| end <= this->address || end > this->address + this->object_size()
	)
	{
		return false;
	}
	if (!this->is_alive(begin, end))
	{
		return false;
	}

	auto const total_size = end - begin;
	bz_assert(total_size % elem_type->size == 0);
	auto const offset = begin - this->address;

	return slice_contained_in_object(this->object_type, offset, elem_type, total_size, end_is_one_past_the_end);
}

bz::vector<bz::u8string> stack_object::get_slice_construction_error_reason(
	ptr_t begin,
	ptr_t end,
	bool end_is_one_past_the_end,
	type const *elem_type
) const
{
	auto const total_size = end - begin;
	bz_assert(total_size % elem_type->size == 0);
	auto const begin_offset = begin - this->address;
	auto const end_offset = end - this->address;

	if (slice_contained_in_object(
		this->object_type,
		begin_offset % this->object_size(),
		elem_type,
		total_size,
		end_is_one_past_the_end
	))
	{
		// must be uninitialized
		bz_assert(!this->is_alive(begin, end));
		bz::vector<bz::u8string> result;
		result.push_back("memory range contains objects outside their lifetime in this stack object");
		return result;
	}
	else
	{
		bz::vector<bz::u8string> result;
		result.reserve(3);
		result.push_back("begin and end addresses point to different subobjects in this stack object");
		bz_assert(begin != end);
		result.push_back(bz::format("begin address points to a subobject at offset {}", begin_offset));
		if (end_is_one_past_the_end)
		{
			result.push_back(bz::format("end address is a one-past-the-end pointer with offset {}", end_offset));
		}
		else
		{
			result.push_back(bz::format("end address points to a subobject at offset {}", end_offset));
		}
		return result;
	}
}

pointer_arithmetic_result_t stack_object::do_pointer_arithmetic(
	ptr_t address,
	bool is_one_past_the_end,
	int64_t amount,
	type const *pointer_type
) const
{
	auto const result_address = address + static_cast<uint64_t>(amount * static_cast<int64_t>(pointer_type->size));
	if (result_address < this->address || result_address > this->address + this->object_size())
	{
		return {
			.address = 0,
			.is_one_past_the_end = false,
		};
	}
	else if (!this->is_alive(std::min(address, result_address), std::max(address, result_address)))
	{
		return {
			.address = 0,
			.is_one_past_the_end = false,
		};
	}
	else if (address == result_address)
	{
		return {
			.address = address,
			.is_one_past_the_end = is_one_past_the_end,
		};
	}

	auto const check_result = check_pointer_arithmetic(
		this->object_type,
		address - this->address,
		result_address - this->address,
		is_one_past_the_end,
		pointer_type
	);
	switch (check_result)
	{
	case pointer_arithmetic_check_result::fail:
		return {
			.address = 0,
			.is_one_past_the_end = false,
		};
	case pointer_arithmetic_check_result::good:
		return {
			.address = result_address,
			.is_one_past_the_end = false,
		};
	case pointer_arithmetic_check_result::one_past_the_end:
		return {
			.address = result_address,
			.is_one_past_the_end = true,
		};
	}
}

bz::vector<bz::u8string> stack_object::get_pointer_arithmetic_error_reason(
	ptr_t address,
	bool is_one_past_the_end,
	int64_t offset,
	type const *object_type
) const
{
	auto const result_address = address + static_cast<uint64_t>(offset * static_cast<int64_t>(object_type->size));
	if (
		result_address >= this->address
		&& result_address <= this->address + this->object_size()
		&& !this->is_alive(std::min(address, result_address), std::max(address, result_address))
	)
	{
		bz::vector<bz::u8string> result;
		result.push_back("address points to this stack object, which is outside its lifetime");
		return result;
	}
	else if (object_type == this->object_type)
	{
		bz::vector<bz::u8string> result;
		if (is_one_past_the_end)
		{
			result.push_back("address is a one-past-the-end pointer to this stack object");
			if (global_data::do_verbose)
			{
				result.push_back("the only valid offsets are -1 and 0");
			}
		}
		else
		{
			result.push_back("address points to this stack object");
			if (global_data::do_verbose)
			{
				result.push_back("the only valid offsets are 0 and 1");
			}
		}
		return result;
	}

	auto const [index, array_size] = get_subobject_info(
		this->object_type,
		address - this->address,
		is_one_past_the_end,
		object_type
	);

	bz::vector<bz::u8string> result;
	if (array_size != 0)
	{
		if (is_one_past_the_end)
		{
			result.push_back(bz::format(
				"address is a one-past-the-end pointer to after the last element in an array of size {} in this stack object",
				array_size
			));
			if (global_data::do_verbose)
			{
				result.push_back(bz::format("the only valid offsets are -{} to 0", array_size));
			}
		}
		else
		{
			result.push_back(bz::format(
				"address points to an element at index {} in an array of size {} in this stack object",
				index, array_size
			));
			if (global_data::do_verbose)
			{
				result.push_back(bz::format("the only valid offsets are {} to {}", -static_cast<int64_t>(index), array_size - index));
			}
		}
	}
	else
	{
		if (is_one_past_the_end)
		{
			result.push_back("address is a one-past-the-end pointer to a subobject that is not in an array in this stack object");
			if (global_data::do_verbose)
			{
				result.push_back("the only valid offsets are -1 and 0");
			}
		}
		else
		{
			result.push_back("address points to a subobject that is not in an array in this stack object");
			if (global_data::do_verbose)
			{
				result.push_back("the only valid offsets are 0 and 1");
			}
		}
	}
	return result;
}

pointer_arithmetic_result_t stack_object::do_pointer_arithmetic_unchecked(
	ptr_t address,
	bool is_one_past_the_end,
	int64_t amount,
	type const *pointer_type
) const
{
	auto const result_address = address + static_cast<uint64_t>(amount * static_cast<int64_t>(pointer_type->size));
	bz_assert(result_address >= this->address && result_address <= this->address + this->object_size());
	if (address == result_address)
	{
		return {
			.address = address,
			.is_one_past_the_end = is_one_past_the_end,
		};
	}

	auto const check_result = check_pointer_arithmetic(
		this->object_type,
		address - this->address,
		result_address - this->address,
		is_one_past_the_end,
		pointer_type
	);
	bz_assert(check_result != pointer_arithmetic_check_result::fail);
	return {
		.address = result_address,
		.is_one_past_the_end = check_result == pointer_arithmetic_check_result::one_past_the_end,
	};
}

bz::optional<int64_t> stack_object::do_pointer_difference(
	ptr_t lhs,
	ptr_t rhs,
	bool lhs_is_one_past_the_end,
	bool rhs_is_one_past_the_end,
	type const *object_type
) const
{
	if (lhs == rhs)
	{
		return 0;
	}
	else if (lhs < rhs)
	{
		auto const slice_check = this->check_slice_construction(lhs, rhs, rhs_is_one_past_the_end, object_type);
		if (slice_check)
		{
			return -static_cast<int64_t>((rhs - lhs) / object_type->size);
		}
		else
		{
			return {};
		}
	}
	else
	{
		auto const slice_check = this->check_slice_construction(rhs, lhs, lhs_is_one_past_the_end, object_type);
		if (slice_check)
		{
			return static_cast<int64_t>((lhs - rhs) / object_type->size);
		}
		else
		{
			return {};
		}
	}
}

bz::vector<bz::u8string> stack_object::get_pointer_difference_error_reason(
	ptr_t lhs,
	ptr_t rhs,
	bool lhs_is_one_past_the_end,
	bool rhs_is_one_past_the_end,
	type const *
) const
{
	if (!this->is_alive(std::min(lhs, rhs), std::max(lhs, rhs)))
	{
		bz::vector<bz::u8string> result;
		result.push_back("lhs and rhs addresses point to this stack object, which is outside its lifetime");
		return result;
	}

	auto const lhs_offset = lhs - this->address;
	auto const rhs_offset = rhs - this->address;

	bz::vector<bz::u8string> result;
	result.reserve(3);
	result.push_back("lhs and rhs addresses point to different subobjects in this stack object");

	if (lhs_is_one_past_the_end)
	{
		result.push_back(bz::format("lhs address is a one-past-the-end pointer with offset {}", lhs_offset));
	}
	else
	{
		result.push_back(bz::format("lhs address points to a subobject at offset {}", lhs_offset));
	}

	if (rhs_is_one_past_the_end)
	{
		result.push_back(bz::format("rhs address is a one-past-the-end pointer with offset {}", rhs_offset));
	}
	else
	{
		result.push_back(bz::format("rhs address points to a subobject at offset {}", rhs_offset));
	}

	return result;
}

copy_values_memory_t stack_object::get_dest_memory(ptr_t address, size_t count, type const *elem_type)
{
	auto const end_address = address + count * elem_type->size;
	if (end_address > this->address + this->object_size() || !this->is_alive(address, end_address))
	{
		return {};
	}

	auto const begin_offset = address - this->address;
	auto const end_offset = end_address - this->address;
	auto const check_result = check_pointer_arithmetic(
		this->object_type,
		begin_offset,
		end_offset,
		false,
		elem_type
	);

	if (check_result == pointer_arithmetic_check_result::fail)
	{
		return {};
	}
	else
	{
		return { this->memory.slice(begin_offset, end_offset) };
	}
}

bz::vector<bz::u8string> stack_object::get_get_dest_memory_error_reasons(ptr_t address, size_t count, type const *elem_type)
{
	bz::vector<bz::u8string> result;
	auto const end_address = address + count * elem_type->size;
	if (end_address > this->address + this->object_size())
	{
		result.push_back(bz::format(
			"destination address points to an invalid memory range in this stack object with offset {} and element count {}",
			address - this->address, count
		));
	}
	else if (!this->is_alive(address, end_address))
	{
		bz::vector<bz::u8string> result;
		result.push_back("destination address points to this stack object, which is outside its lifetime");
	}
	else
	{
		auto const begin_offset = address - this->address;
		auto const end_offset = end_address - this->address;
		auto const check_result = check_pointer_arithmetic(
			this->object_type,
			begin_offset,
			end_offset,
			false,
			elem_type
		);

		if (check_result == pointer_arithmetic_check_result::fail)
		{
			result.push_back(bz::format(
				"destination address points to an invalid memory range in this stack object with offset {} and element count {}",
				address - this->address, count
			));
		}
	}
	return result;
}

copy_values_memory_t stack_object::get_copy_source_memory(ptr_t address, size_t count, type const *elem_type)
{
	auto const end_address = address + count * elem_type->size;
	if (end_address > this->address + this->object_size() || !this->is_alive(address, end_address))
	{
		return {};
	}

	auto const begin_offset = address - this->address;
	auto const end_offset = end_address - this->address;
	auto const check_result = check_pointer_arithmetic(
		this->object_type,
		begin_offset,
		end_offset,
		false,
		elem_type
	);

	if (check_result == pointer_arithmetic_check_result::fail)
	{
		return {};
	}
	else
	{
		return { this->memory.slice(begin_offset, end_offset) };
	}
}

bz::vector<bz::u8string> stack_object::get_get_copy_source_memory_error_reasons(ptr_t address, size_t count, type const *elem_type)
{
	bz::vector<bz::u8string> result;

	auto const end_address = address + count * elem_type->size;
	if (end_address <= this->address + this->object_size() && !this->is_alive(address, end_address))
	{
		result.push_back("source address points to this stack object, which is outside its lifetime");
		return result;
	}

	auto const begin_offset = address - this->address;
	auto const end_offset = end_address - this->address;
	auto const check_result = check_pointer_arithmetic(
		this->object_type,
		begin_offset,
		end_offset,
		false,
		elem_type
	);

	if (check_result == pointer_arithmetic_check_result::fail)
	{
		result.push_back(bz::format(
			"source address points to an invalid memory range in this stack object with offset {} and element count {}",
			begin_offset, count
		));
	}

	return result;
}

copy_overlapping_values_data_t stack_object::get_copy_overlapping_memory(
	ptr_t dest,
	ptr_t source,
	size_t count,
	type const *elem_type
)
{
	auto const dest_end = dest + count * elem_type->size;
	auto const source_end = source + count * elem_type->size;
	if (dest_end > this->address + this->object_size() || source_end > this->address + this->object_size())
	{
		return {};
	}

	// we need to check the range [source, dest) if it's alive
	if (!this->is_alive(source, source_end))
	{
		return {};
	}

	auto const dest_offset = dest - this->address;
	auto const dest_end_offset = dest_end - this->address;
	auto const source_offset = source - this->address;
	auto const source_end_offset = source_end - this->address;

	auto const dest_check_result = check_pointer_arithmetic(
		this->object_type,
		dest_offset,
		dest_end_offset,
		false,
		elem_type
	);

	if (dest_check_result == pointer_arithmetic_check_result::fail)
	{
		return {};
	}

	auto const source_check_result = check_pointer_arithmetic(
		this->object_type,
		source_offset,
		source_end_offset,
		false,
		elem_type
	);

	if (source_check_result == pointer_arithmetic_check_result::fail)
	{
		return {};
	}

	return {
		.dest = {
			this->memory.slice(dest_offset, dest_end_offset),
			this->properties.data.slice(dest_offset, dest_end_offset),
		},
		.source = {
			this->memory.slice(source_offset, source_end_offset),
			{},
		},
	};
}

bz::vector<bz::u8string> stack_object::get_get_copy_overlapping_memory_error_reasons(
	ptr_t dest,
	ptr_t source,
	size_t count,
	type const *elem_type
)
{
	bz::vector<bz::u8string> result;

	auto const dest_end = dest + count * elem_type->size;
	auto const source_end = source + count * elem_type->size;
	if (dest_end > this->address + this->object_size())
	{
		result.push_back(bz::format(
			"destination address points to an invalid memory range in this stack object with offset {} and element count {}",
			(dest - this->address), count
		));
	}
	if (source_end > this->address + this->object_size())
	{
		result.push_back(bz::format(
			"source address points to an invalid memory range in this stack object with offset {} and element count {}",
			(source - this->address), count
		));
	}

	if (result.not_empty())
	{
		return result;
	}

	if (dest > source)
	{
		// we need to check the range [source, dest) if it's alive, and the whole dest range
		if (!this->is_alive(source, dest))
		{
			result.push_back("source address points to this stack object, which is outside its lifetime");
		}
	}
	else
	{
		// we need to check the whole source range if it's alive, and [dest, source)
		if (!this->is_alive(source, source_end))
		{
			result.push_back("source address points to this stack object, which is outside its lifetime");
		}
	}

	auto const dest_offset = dest - this->address;
	auto const dest_end_offset = dest_end - this->address;
	auto const source_offset = source - this->address;
	auto const source_end_offset = source_end - this->address;

	auto const dest_check_result = check_pointer_arithmetic(
		this->object_type,
		dest_offset,
		dest_end_offset,
		false,
		elem_type
	);

	if (dest_check_result == pointer_arithmetic_check_result::fail)
	{
		result.push_back(bz::format(
			"destination address points to an invalid memory range in this stack object with offset {} and element count {}",
			(dest - this->address), count
		));
	}

	auto const source_check_result = check_pointer_arithmetic(
		this->object_type,
		source_offset,
		source_end_offset,
		false,
		elem_type
	);

	if (source_check_result == pointer_arithmetic_check_result::fail)
	{
		result.push_back(bz::format(
			"source address points to an invalid memory range in this stack object with offset {} and element count {}",
			(source - this->address), count
		));
	}

	return result;
}

heap_object::heap_object(ptr_t _address, type const *_elem_type, size_t _count)
	: address(_address),
	  elem_type(_elem_type),
	  count(_count),
	  memory(),
	  properties()
{
	auto const size = this->elem_type->size * this->count;
	this->memory = bz::fixed_vector<uint8_t>(size, 0);
	this->properties = memory_properties(this->elem_type, size, 0);
}

size_t heap_object::object_size(void) const
{
	return this->memory.size();
}

size_t heap_object::elem_size(void) const
{
	return this->elem_type->size;
}

void heap_object::start_lifetime(ptr_t begin, ptr_t end)
{
	bz_assert(this->is_none_alive(begin, end));
	this->properties.set_range(begin - this->address, end - this->address, memory_properties::is_alive);
}

void heap_object::end_lifetime(ptr_t begin, ptr_t end)
{
	this->properties.erase_range(begin - this->address, end - this->address, memory_properties::is_alive);
}

bool heap_object::is_alive(ptr_t begin, ptr_t end) const
{
	return this->properties.is_all(
		begin - this->address,
		end - this->address,
		memory_properties::is_alive,
		memory_properties::is_padding
	);
}

bool heap_object::is_none_alive(ptr_t begin, ptr_t end) const
{
	return this->properties.is_none(
		begin - this->address,
		end - this->address,
		memory_properties::is_alive,
		memory_properties::is_padding
	);
}

uint8_t *heap_object::get_memory(ptr_t address)
{
	bz_assert(address >= this->address && address <= this->address + this->object_size());
	bz_assert(this->memory.not_empty());
	return this->memory.data() + (address - this->address);
}

uint8_t const *heap_object::get_memory(ptr_t address) const
{
	bz_assert(address >= this->address && address <= this->address + this->object_size());
	bz_assert(this->memory.not_empty());
	return this->memory.data() + (address - this->address);
}

bool heap_object::check_dereference(ptr_t address, type const *subobject_type) const
{
	if (address < this->address || address >= this->address + this->object_size() || this->memory.empty())
	{
		bz_unreachable;
	}
	if (!this->is_alive(address, address + subobject_type->size))
	{
		return false;
	}

	bz_assert(contained_in_object(this->elem_type, (address - this->address) % this->elem_size(), subobject_type));
	return true;
}

bz::u8string heap_object::get_dereference_error_reason(ptr_t address, type const *object_type) const
{
	bz_assert(!this->is_alive(address, address + object_type->size));
	return "address points to an object outside its lifetime in this allocation";
}

bool heap_object::check_inplace_construct(ptr_t address, type const *object_type) const
{
	if (object_type != this->elem_type)
	{
		return false;
	}
	else
	{
		return this->is_none_alive(address, address + this->elem_size());
	}
}

bz::u8string heap_object::get_inplace_construct_error_reason(ptr_t, type const *object_type) const
{
	if (object_type != this->elem_type)
	{
		return "address points to a subobject of one of the elements in this allocation";
	}
	else
	{
		return "address points to an element in this allocation, that has already been constructed";
	}
}

bool heap_object::check_destruct_value(ptr_t address, type const *object_type) const
{
	if (object_type != this->elem_type)
	{
		return false;
	}
	else
	{
		return this->is_alive(address, address + this->elem_size());
	}
}

bz::u8string heap_object::get_destruct_value_error_reason(ptr_t, type const *object_type) const
{
	if (object_type != this->elem_type)
	{
		return "address points to a subobject of one of the elements in this allocation";
	}
	else
	{
		return "address points to an element outside its lifetime in this allocation";
	}
}

bool heap_object::check_slice_construction(ptr_t begin, ptr_t end, bool end_is_one_past_the_end, type const *elem_type) const
{
	if (begin == end)
	{
		return true;
	}

	if (!this->is_alive(begin, end))
	{
		return false;
	}

	auto const total_size = end - begin;
	bz_assert(total_size % elem_type->size == 0);
	auto const offset = begin - this->address;

	if (elem_type == this->elem_type)
	{
		bz_assert(offset % this->elem_size() == 0);
		bz_assert(end_is_one_past_the_end || offset + total_size < this->object_size());
		return true;
	}
	else
	{
		return slice_contained_in_object(this->elem_type, offset % this->elem_size(), elem_type, total_size, end_is_one_past_the_end);
	}
}

bz::vector<bz::u8string> heap_object::get_slice_construction_error_reason(
	ptr_t begin,
	ptr_t end,
	bool end_is_one_past_the_end,
	type const *elem_type
) const
{
	auto const total_size = end - begin;
	bz_assert(total_size % elem_type->size == 0);
	auto const begin_offset = begin - this->address;
	auto const end_offset = end - this->address;
	auto const begin_object_index = begin_offset / this->elem_size();
	auto const end_object_index = end_offset / this->elem_size() - (end_is_one_past_the_end && (end_offset % this->elem_size() == 0) ? 1 : 0);

	if (elem_type != this->elem_type && begin_object_index != end_object_index)
	{
		bz::vector<bz::u8string> result;
		result.push_back(bz::format(
			"begin and end addresses point to different elements in this allocations at indices {} and {}",
			begin_object_index, end_object_index
		));
		return result;
	}
	else if (
		elem_type == this->elem_type
		|| slice_contained_in_object(this->elem_type, begin_offset % this->elem_size(), elem_type, total_size, end_is_one_past_the_end)
	)
	{
		// must be uninitialized
		bz_assert(!this->is_alive(begin, end));
		bz::vector<bz::u8string> result;
		result.push_back("memory range contains objects outside their lifetime in this allocation");
		return result;
	}
	else
	{
		bz::vector<bz::u8string> result;
		result.reserve(3);
		result.push_back("begin and end addresses point to different subobjects of the same element in this allocation");
		bz_assert(begin != end);
		result.push_back(bz::format("begin address points to a subobject at offset {}", begin_offset % this->elem_size()));
		if (end_is_one_past_the_end)
		{
			auto const elem_offset = end_offset % this->elem_size();
			auto const offset = elem_offset == 0 ? this->elem_size() : elem_offset;
			result.push_back(bz::format("end address is a one-past-the-end pointer with offset {}", offset));
		}
		else
		{
			result.push_back(bz::format("end address points to a subobject at offset {}", end_offset % this->elem_size()));
		}
		return result;
	}
}

pointer_arithmetic_result_t heap_object::do_pointer_arithmetic(
	ptr_t address,
	bool is_one_past_the_end,
	int64_t amount,
	type const *pointer_type
) const
{
	auto const result_address = address + static_cast<uint64_t>(amount * static_cast<int64_t>(pointer_type->size));
	if (result_address < this->address || result_address > this->address + this->object_size())
	{
		return {
			.address = 0,
			.is_one_past_the_end = false,
		};
	}
	else if (pointer_type == this->elem_type)
	{
		return {
			.address = result_address,
			.is_one_past_the_end = result_address == this->address + this->object_size(),
		};
	}
	else if (!this->is_alive(std::min(address, result_address), std::max(address, result_address)))
	{
		return {
			.address = 0,
			.is_one_past_the_end = false,
		};
	}
	else if (address == result_address)
	{
		return {
			.address = address,
			.is_one_past_the_end = is_one_past_the_end,
		};
	}

	auto const offset = address - this->address;
	auto const result_offset = result_address - this->address;

	auto const elem_size = this->elem_size();
	auto const offset_in_elem = offset % elem_size;
	auto const real_offset_in_elem = offset_in_elem == 0 && is_one_past_the_end ? elem_size : offset_in_elem;
	auto const elem_offset = offset - real_offset_in_elem;
	if (result_offset < elem_offset || result_offset > elem_offset + elem_size)
	{
		return {
			.address = 0,
			.is_one_past_the_end = false,
		};
	}

	auto const check_result = check_pointer_arithmetic(
		this->elem_type,
		offset - elem_offset,
		result_offset - elem_offset,
		is_one_past_the_end,
		pointer_type
	);
	switch (check_result)
	{
	case pointer_arithmetic_check_result::fail:
		return {
			.address = 0,
			.is_one_past_the_end = false,
		};
	case pointer_arithmetic_check_result::good:
		return {
			.address = result_address,
			.is_one_past_the_end = false,
		};
	case pointer_arithmetic_check_result::one_past_the_end:
		return {
			.address = result_address,
			.is_one_past_the_end = true,
		};
	}
}

bz::vector<bz::u8string> heap_object::get_pointer_arithmetic_error_reason(
	ptr_t address,
	bool is_one_past_the_end,
	int64_t amount,
	type const *object_type
) const
{
	auto const result_address = address + static_cast<uint64_t>(amount * static_cast<int64_t>(object_type->size));
	if (object_type == this->elem_type)
	{
		bz::vector<bz::u8string> result;
		if (is_one_past_the_end)
		{
			result.push_back(bz::format(
				"address is a one-past-the-end pointer to after the last element in this allocation of size {}",
				this->count
			));
			if (global_data::do_verbose)
			{
				result.push_back(bz::format("the only valid offsets are -{} to 0", this->count));
			}
		}
		else
		{
			auto const index = (address - this->address) / this->elem_size();
			result.push_back(bz::format(
				"address points to the element at index {} in this allocation of size {}",
				index, this->count
			));
			if (global_data::do_verbose)
			{
				result.push_back(bz::format("the only valid offsets are {} to {}", -static_cast<int64_t>(index), this->count - index));
			}
		}
		return result;
	}
	else if (
		result_address >= this->address
		&& result_address <= this->address + this->object_size()
		&& !this->is_alive(std::min(address, result_address), std::max(address, result_address))
	)
	{
		bz::vector<bz::u8string> result;
		result.push_back("address points to an element outside its lifetime in this allocation");
		return result;
	}
	else
	{
		auto const offset = address - this->address;

		auto const elem_size = this->elem_size();
		auto const offset_in_elem = offset % elem_size;
		auto const real_offset_in_elem = offset_in_elem == 0 && is_one_past_the_end ? elem_size : offset_in_elem;
		auto const elem_offset = offset - real_offset_in_elem;

		auto const [index, array_size] = get_subobject_info(
			this->elem_type,
			offset - elem_offset,
			is_one_past_the_end,
			object_type
		);

		bz::vector<bz::u8string> result;
		if (array_size != 0)
		{
			if (is_one_past_the_end)
			{
				result.push_back(bz::format(
					"address is a one-past-the-end pointer to after the last element in an array of size {} in an element of this allocation",
					array_size
				));
				if (global_data::do_verbose)
				{
					result.push_back(bz::format("the only valid offsets are -{} to 0", array_size));
				}
			}
			else
			{
				result.push_back(bz::format(
					"address points to an element at index {} in an array of size {} in an element of this allocation",
					index, array_size
				));
				if (global_data::do_verbose)
				{
					result.push_back(bz::format("the only valid offsets are {} to {}", -static_cast<int64_t>(index), array_size - index));
				}
			}
		}
		else
		{
			if (is_one_past_the_end)
			{
				result.push_back("address is a one-past-the-end pointer to a subobject that is not in an array in an element of this allocation");
				if (global_data::do_verbose)
				{
					result.push_back("the only valid offsets are -1 and 0");
				}
			}
			else
			{
				result.push_back("address points to a subobject that is not in an array in an element of this allocation");
				if (global_data::do_verbose)
				{
					result.push_back("the only valid offsets are 0 and 1");
				}
			}
		}
		return result;
	}
}

pointer_arithmetic_result_t heap_object::do_pointer_arithmetic_unchecked(
	ptr_t address,
	bool is_one_past_the_end,
	int64_t amount,
	type const *pointer_type
) const
{
	auto const result_address = address + static_cast<uint64_t>(amount * static_cast<int64_t>(pointer_type->size));
	bz_assert(result_address >= this->address && result_address <= this->address + this->object_size());
	if (pointer_type == this->elem_type)
	{
		return {
			.address = result_address,
			.is_one_past_the_end = result_address == this->address + this->object_size(),
		};
	}
	else if (address == result_address)
	{
		return {
			.address = address,
			.is_one_past_the_end = is_one_past_the_end,
		};
	}

	auto const offset = address - this->address;
	auto const result_offset = result_address - this->address;

	auto const elem_size = this->elem_size();
	auto const offset_in_elem = offset % elem_size;
	auto const real_offset_in_elem = offset_in_elem == 0 && is_one_past_the_end ? elem_size : offset_in_elem;
	auto const elem_offset = offset - real_offset_in_elem;
	bz_assert(result_offset >= elem_offset && result_offset <= elem_offset + elem_size);

	auto const check_result = check_pointer_arithmetic(
		this->elem_type,
		offset - elem_offset,
		result_offset - elem_offset,
		is_one_past_the_end,
		pointer_type
	);
	bz_assert(check_result != pointer_arithmetic_check_result::fail);
	return {
		.address = result_address,
		.is_one_past_the_end = check_result == pointer_arithmetic_check_result::one_past_the_end,
	};
}

bz::optional<int64_t> heap_object::do_pointer_difference(
	ptr_t lhs,
	ptr_t rhs,
	bool lhs_is_one_past_the_end,
	bool rhs_is_one_past_the_end,
	type const *object_type
) const
{
	if (lhs == rhs)
	{
		return 0;
	}
	else if (object_type == this->elem_type)
	{
		if (lhs < rhs)
		{
			return -static_cast<int64_t>((rhs - lhs) / object_type->size);
		}
		else
		{
			return static_cast<int64_t>((lhs - rhs) / object_type->size);
		}
	}
	else if (lhs < rhs)
	{
		auto const slice_check = this->check_slice_construction(lhs, rhs, rhs_is_one_past_the_end, object_type);
		if (slice_check)
		{
			return -static_cast<int64_t>((rhs - lhs) / object_type->size);
		}
		else
		{
			return {};
		}
	}
	else
	{
		auto const slice_check = this->check_slice_construction(rhs, lhs, lhs_is_one_past_the_end, object_type);
		if (slice_check)
		{
			return static_cast<int64_t>((lhs - rhs) / object_type->size);
		}
		else
		{
			return {};
		}
	}
}

bz::vector<bz::u8string> heap_object::get_pointer_difference_error_reason(
	ptr_t lhs,
	ptr_t rhs,
	bool lhs_is_one_past_the_end,
	bool rhs_is_one_past_the_end,
	type const *object_type
) const
{
	bz_assert(object_type != this->elem_type);

	auto const lhs_offset = lhs - this->address;
	auto const rhs_offset = rhs - this->address;
	auto const lhs_index = lhs_offset / this->elem_size() - (lhs_is_one_past_the_end && lhs_offset % this->elem_size() == 0 ? 1 : 0);
	auto const rhs_index = rhs_offset / this->elem_size() - (rhs_is_one_past_the_end && rhs_offset % this->elem_size() == 0 ? 1 : 0);

	bz::vector<bz::u8string> result;
	if (lhs_index != rhs_index)
	{
		result.push_back(bz::format(
			"lhs and rhs addresses point to different elements in this allocations at indices {} and {}",
			lhs_index, rhs_index
		));
	}
	else if (!this->is_alive(std::min(lhs, rhs), std::max(lhs, rhs)))
	{
		result.push_back("lhs and rhs addresses point to an element outside its lifetime in this allocation");
	}
	else
	{
		result.reserve(3);
		result.push_back("lhs and rhs addresses point to different subobjects of the same element in this allocation");
		bz_assert(lhs != rhs);

		if (lhs_is_one_past_the_end)
		{
			auto const elem_offset = lhs_offset % this->elem_size();
			auto const offset = elem_offset == 0 ? this->elem_size() : elem_offset;
			result.push_back(bz::format("lhs address is a one-past-the-end pointer with offset {}", offset));
		}
		else
		{
			result.push_back(bz::format("lhs address points to a subobject at offset {}", lhs_offset % this->elem_size()));
		}

		if (rhs_is_one_past_the_end)
		{
			auto const elem_offset = rhs_offset % this->elem_size();
			auto const offset = elem_offset == 0 ? this->elem_size() : elem_offset;
			result.push_back(bz::format("rhs address is a one-past-the-end pointer with offset {}", offset));
		}
		else
		{
			result.push_back(bz::format("rhs address points to a subobject at offset {}", rhs_offset % this->elem_size()));
		}
	}

	return result;
}

copy_values_memory_and_properties_t heap_object::get_dest_memory(ptr_t address, size_t count, type const *elem_type, bool is_trivial)
{
	auto const end_address = address + count * elem_type->size;
	if (end_address > this->address + this->object_size())
	{
		return {};
	}
	else if (elem_type == this->elem_type)
	{
		auto const begin_offset = address - this->address;
		auto const end_offset = end_address - this->address;
		if (is_trivial || this->is_none_alive(address, end_address))
		{
			return { this->memory.slice(begin_offset, end_offset), this->properties.data.slice(begin_offset, end_offset) };
		}
		else
		{
			return {};
		}
	}
	else
	{
		bz_assert(is_trivial);

		if (!this->is_alive(address, end_address))
		{
			return {};
		}

		auto const elem_begin_offset = (address - this->address) % this->elem_size();
		auto const elem_end_offset = elem_begin_offset + (end_address - address);
		auto const check_result = check_pointer_arithmetic(
			this->elem_type,
			elem_begin_offset,
			elem_end_offset,
			false,
			elem_type
		);

		if (check_result == pointer_arithmetic_check_result::fail)
		{
			return {};
		}
		else
		{
			auto const begin_offset = address - this->address;
			auto const end_offset = end_address - this->address;
			return { this->memory.slice(begin_offset, end_offset), {} };
		}
	}
}

bz::vector<bz::u8string> heap_object::get_get_dest_memory_error_reasons(ptr_t address, size_t count, type const *elem_type, bool is_trivial)
{
	bz::vector<bz::u8string> result;
	auto const end_address = address + count * elem_type->size;
	if (end_address > this->address + this->object_size())
	{
		if (elem_type == this->elem_type)
		{
			result.push_back(bz::format(
				"destination address points to an invalid memory range in this allocation starting at index {} and with an element count of {}",
				(address - this->address) / elem_type->size, count
			));
		}
		else
		{
			result.push_back(bz::format(
				"destination address points to an invalid memory range in an element from this allocation with offset {} and element count {}",
				(address - this->address) % elem_type->size, count
			));
		}
	}
	else if (elem_type == this->elem_type)
	{
		if (!is_trivial && !this->is_none_alive(address, end_address))
		{
			result.push_back(bz::format(
				"destination address points to a memory range containing initialized elements in this allocation starting at index {} and with an element count of {}",
				(address - this->address) / elem_type->size, count
			));
		}
	}
	else if (!this->is_alive(address, end_address))
	{
		result.push_back("destination address points to an object outside its lifetime");
	}
	else
	{
		auto const elem_begin_offset = (address - this->address) % this->elem_size();
		auto const elem_end_offset = elem_begin_offset + (end_address - address);
		auto const check_result = check_pointer_arithmetic(
			this->elem_type,
			elem_begin_offset,
			elem_end_offset,
			false,
			elem_type
		);

		if (check_result == pointer_arithmetic_check_result::fail)
		{
			result.push_back(bz::format(
				"destination address points to an invalid memory range in an element from this allocation with offset {} and element count {}",
				(address - this->address) % elem_type->size, count
			));
		}
	}
	return result;
}

copy_values_memory_t heap_object::get_copy_source_memory(ptr_t address, size_t count, type const *elem_type)
{
	auto const end_address = address + count * elem_type->size;
	if (end_address > this->address + this->object_size() || !this->is_alive(address, end_address))
	{
		return {};
	}
	else if (elem_type == this->elem_type)
	{
		auto const begin_offset = address - this->address;
		auto const end_offset = end_address - this->address;
		return { this->memory.slice(begin_offset, end_offset) };
	}
	else
	{
		auto const elem_begin_offset = (address - this->address) % this->elem_size();
		auto const elem_end_offset = elem_begin_offset + (end_address - address);
		auto const check_result = check_pointer_arithmetic(
			this->elem_type,
			elem_begin_offset,
			elem_end_offset,
			false,
			elem_type
		);

		if (check_result == pointer_arithmetic_check_result::fail)
		{
			return {};
		}
		else
		{
			auto const begin_offset = address - this->address;
			auto const end_offset = end_address - this->address;
			return { this->memory.slice(begin_offset, end_offset) };
		}
	}
}

bz::vector<bz::u8string> heap_object::get_get_copy_source_memory_error_reasons(ptr_t address, size_t count, type const *elem_type)
{
	bz::vector<bz::u8string> result;

	auto const end_address = address + count * elem_type->size;
	if (end_address <= this->address + this->object_size() && !this->is_alive(address, end_address))
	{
		result.push_back("source address points to objects in this allocation, which are outside their lifetime");
	}
	else if (end_address > this->address + this->object_size())
	{
		if (elem_type == this->elem_type)
		{
			result.push_back(bz::format(
				"source address points to an invalid memory range in this allocation starting at index {} and with an element count of {}",
				(address - this->address) / elem_type->size, count
			));
		}
		else
		{
			result.push_back(bz::format(
				"source address points to an invalid memory range in an element from this allocation with offset {} and element count {}",
				(address - this->address) % elem_type->size, count
			));
		}
	}
	else if (elem_type != this->elem_type)
	{
		auto const elem_begin_offset = (address - this->address) % this->elem_size();
		auto const elem_end_offset = elem_begin_offset + (end_address - address);
		auto const check_result = check_pointer_arithmetic(
			this->elem_type,
			elem_begin_offset,
			elem_end_offset,
			false,
			elem_type
		);

		if (check_result == pointer_arithmetic_check_result::fail)
		{
			result.push_back(bz::format(
				"source address points to an invalid memory range in an element from this allocation with offset {} and element count {}",
				(address - this->address) % elem_type->size, count
			));
		}
	}

	return result;
}

copy_overlapping_values_data_t heap_object::get_copy_overlapping_memory(
	ptr_t dest,
	ptr_t source,
	size_t count,
	type const *elem_type
)
{
	auto const dest_end = dest + count * this->elem_size();
	auto const source_end = source + count * this->elem_size();
	if (dest_end > this->address + this->object_size() || source_end > this->address + this->object_size())
	{
		return {};
	}

	// we need to check the range [source, dest) if it's alive
	if (!this->is_alive(source, source_end))
	{
		return {};
	}

	auto const dest_offset = dest - this->address;
	auto const dest_end_offset = dest_end - this->address;
	auto const source_offset = source - this->address;
	auto const source_end_offset = source_end - this->address;

	if (elem_type != this->elem_type)
	{
		auto const dest_elem_offset = dest_offset % this->elem_size();
		auto const dest_end_elem_offset = dest_elem_offset + (dest_end_offset - dest_offset);
		auto const source_elem_offset = source_offset % this->elem_size();
		auto const source_end_elem_offset = source_elem_offset + (source_end_offset - source_offset);

		auto const dest_check_result = check_pointer_arithmetic(
			this->elem_type,
			dest_elem_offset,
			dest_end_elem_offset,
			false,
			elem_type
		);

		if (dest_check_result == pointer_arithmetic_check_result::fail)
		{
			return {};
		}

		auto const source_check_result = check_pointer_arithmetic(
			this->elem_type,
			source_elem_offset,
			source_end_elem_offset,
			false,
			elem_type
		);

		if (source_check_result == pointer_arithmetic_check_result::fail)
		{
			return {};
		}
	}

	return {
		.dest = {
			this->memory.slice(dest_offset, dest_end_offset),
			this->properties.data.slice(dest_offset, dest_end_offset),
		},
		.source = {
			this->memory.slice(source_offset, source_end_offset),
			{},
		},
	};
}

bz::vector<bz::u8string> heap_object::get_get_copy_overlapping_memory_error_reasons(
	ptr_t dest,
	ptr_t source,
	size_t count,
	type const *elem_type
)
{
	bz::vector<bz::u8string> result;

	auto const dest_end = dest + count * this->elem_size();
	auto const source_end = source + count * this->elem_size();
	if (dest_end > this->address + this->object_size())
	{
		if (elem_type == this->elem_type)
		{
			result.push_back(bz::format(
				"destination address points to an invalid memory range in this allocation starting at index {} and with an element count of {}",
				(dest - this->address) / this->elem_type->size, count
			));
		}
		else
		{
			result.push_back(bz::format(
				"destination address points to an invalid memory range in an element of this allocation with offset {} and element count {}",
				(dest - this->address) % this->elem_type->size, count
			));
		}
	}
	if (source_end > this->address + this->object_size())
	{
		if (elem_type == this->elem_type)
		{
			result.push_back(bz::format(
				"source address points to an invalid memory range in this allocation starting at index {} and with an element count of {}",
				(source - this->address) / this->elem_type->size, count
			));
		}
		else
		{
			result.push_back(bz::format(
				"source address points to an invalid memory range in an element of this allocation with offset {} and element count {}",
				(source - this->address) % this->elem_type->size, count
			));
		}
	}

	if (result.not_empty())
	{
		return result;
	}

	if (!this->is_alive(source, source_end))
	{
		result.push_back("source address points to objects in this allocation, which are outside their lifetime");
	}

	if (elem_type != this->elem_type)
	{
		auto const dest_elem_offset = (dest - this->address) % this->elem_size();
		auto const dest_end_elem_offset = dest_elem_offset + (dest_end - dest);
		auto const source_elem_offset = (source - this->address) % this->elem_size();
		auto const source_end_elem_offset = source_elem_offset + (source_end - source);

		auto const dest_check_result = check_pointer_arithmetic(
			this->elem_type,
			dest_elem_offset,
			dest_end_elem_offset,
			false,
			elem_type
		);

		if (dest_check_result == pointer_arithmetic_check_result::fail)
		{
			result.push_back(bz::format(
				"destination address points to an invalid memory range in an element of this allocation with offset {} and element count {}",
				dest_elem_offset, count
			));
		}

		auto const source_check_result = check_pointer_arithmetic(
			this->elem_type,
			source_elem_offset,
			source_end_elem_offset,
			false,
			elem_type
		);

		if (source_check_result == pointer_arithmetic_check_result::fail)
		{
			result.push_back(bz::format(
				"source address points to an invalid memory range in an element of this allocation with offset {} and element count {}",
				source_elem_offset, count
			));
		}
	}

	return result;
}

copy_values_memory_and_properties_t heap_object::get_relocate_source_memory(ptr_t address, size_t count, type const *elem_type)
{
	auto const end_address = address + count * elem_type->size;
	if (end_address > this->address + this->object_size() || !this->is_alive(address, end_address))
	{
		return {};
	}
	else if (elem_type == this->elem_type)
	{
		auto const begin_offset = address - this->address;
		auto const end_offset = end_address - this->address;
		return { this->memory.slice(begin_offset, end_offset), this->properties.data.slice(begin_offset, end_offset) };
	}
	else
	{
		return {};
	}
}

bz::vector<bz::u8string> heap_object::get_get_relocate_source_memory_error_reasons(ptr_t address, size_t count, type const *elem_type)
{
	bz::vector<bz::u8string> result;

	auto const end_address = address + count * elem_type->size;
	if (end_address <= this->address + this->object_size() && !this->is_alive(address, end_address))
	{
		result.push_back("source address points to objects in this allocation, which are outside their lifetime");
	}
	else if (end_address > this->address + this->object_size())
	{
		if (elem_type == this->elem_type)
		{
			result.push_back(bz::format(
				"source address points to an invalid memory range in this allocation starting at index {} and with an element count of {}",
				(address - this->address) / elem_type->size, count
			));
		}
		else
		{
			result.push_back(bz::format(
				"source address points to an invalid memory range in an element from this allocation with offset {} and element count {}",
				(address - this->address) % elem_type->size, count
			));
		}
	}

	return result;
}

copy_overlapping_values_data_t heap_object::get_relocate_overlapping_memory(
	ptr_t dest,
	ptr_t source,
	size_t count,
	bool is_trivial
)
{
	auto const dest_end = dest + count * this->elem_size();
	auto const source_end = source + count * this->elem_size();
	if (dest_end > this->address + this->object_size() || source_end > this->address + this->object_size())
	{
		return {};
	}

	if (dest > source)
	{
		// we need to check the range [source, source_end) if it's alive, and [source_end, dest_end) if it's not alive
		if (!this->is_alive(source, source_end))
		{
			return {};
		}
		else if (!is_trivial && !this->is_none_alive(source_end, dest_end))
		{
			return {};
		}
	}
	else
	{
		// we need to check the range [source, source_end) if it's alive, and [dest, source) if it's not alive
		if (!this->is_alive(source, source_end))
		{
			return {};
		}
		else if (!is_trivial && !this->is_none_alive(dest, source))
		{
			return {};
		}
	}

	auto const dest_offset = dest - this->address;
	auto const dest_end_offset = dest_end - this->address;
	auto const source_offset = source - this->address;
	auto const source_end_offset = source_end - this->address;

	return {
		.dest = {
			this->memory.slice(dest_offset, dest_end_offset),
			this->properties.data.slice(dest_offset, dest_end_offset),
		},
		.source = {
			this->memory.slice(source_offset, source_end_offset),
			this->properties.data.slice(source_offset, source_end_offset),
		},
	};
}

bz::vector<bz::u8string> heap_object::get_get_relocate_overlapping_memory_error_reasons(
	ptr_t dest,
	ptr_t source,
	size_t count,
	bool is_trivial
)
{
	bz::vector<bz::u8string> result;

	auto const dest_end = dest + count * this->elem_size();
	auto const source_end = source + count * this->elem_size();
	if (dest_end > this->address + this->object_size())
	{
		result.push_back(bz::format(
			"destination address points to an invalid memory range in this allocation starting at index {} and with an element count of {}",
			(dest - this->address) / this->elem_type->size, count
		));
	}
	if (source_end > this->address + this->object_size())
	{
		result.push_back(bz::format(
			"source address points to an invalid memory range in this allocation starting at index {} and with an element count of {}",
			(source - this->address) / this->elem_type->size, count
		));
	}

	if (result.not_empty())
	{
		return result;
	}

	if (dest > source)
	{
		if (!this->is_alive(source, source_end))
		{
			result.push_back("source address points to objects in this allocation, which are outside their lifetime");
		}
		else if (!is_trivial && !this->is_none_alive(source_end, dest_end))
		{
			result.push_back("destination address must point to uninitialized elements of this allocation if the value type is not trivially destructible");
		}
	}
	else
	{
		if (!this->is_alive(source, source_end))
		{
			result.push_back("source address points to objects in this allocation, which are outside their lifetime");
		}
		else if (!is_trivial && !this->is_none_alive(dest, source))
		{
			result.push_back("destination address must point to uninitialized elements of this allocation if the value type is not trivially destructible");
		}
	}

	return result;
}

stack_manager::stack_manager(ptr_t stack_begin)
	: head(stack_begin),
	  stack_frames(),
	  stack_frame_id(0)
{}

void stack_manager::push_stack_frame(bz::array_view<alloca const> allocas)
{
	auto const begin_address = this->head;

	auto &new_frame = this->stack_frames.emplace_back();
	new_frame.begin_address = begin_address;
	new_frame.id = this->stack_frame_id;
	this->stack_frame_id += 1;

	auto object_address = begin_address;
	new_frame.objects = bz::fixed_vector<stack_object>(
		allocas.transform([&object_address, begin_address](alloca const &a) {
			auto const &[object_type, is_always_initialized, src_tokens] = a;
			object_address = object_address == begin_address
				? begin_address
				: object_address + (object_type->align - object_address % object_type->align);
			bz_assert(object_address % object_type->align == 0);
			auto result = stack_object(src_tokens, object_address, object_type, is_always_initialized);
			object_address += object_type->size;
			return result;
		})
	);
	new_frame.total_size = object_address - begin_address;

	this->head = object_address + (max_object_align - object_address % max_object_align);
}

void stack_manager::pop_stack_frame(void)
{
	bz_assert(this->stack_frames.not_empty());
	this->head = this->stack_frames.back().begin_address;
	this->stack_frames.pop_back();
}

stack_frame *stack_manager::get_stack_frame(ptr_t address)
{
	if (
		this->stack_frames.empty()
		|| address < this->stack_frames[0].begin_address
		|| address > this->stack_frames.back().begin_address + this->stack_frames.back().total_size
	)
	{
		return nullptr;
	}
	else if (address >= this->stack_frames.back().begin_address)
	{
		return &this->stack_frames.back();
	}

	// find the last element that has an address that is less than or equal to address.
	// we do this by finding the first element, whose address is greater than address
	// and taking the element before that
	auto const it = std::upper_bound(
		this->stack_frames.begin(), this->stack_frames.end(),
		address,
		[](ptr_t address, auto const &stack_frame) {
			return address < stack_frame.begin_address;
		}
	);
	return &*(it - 1);
}

stack_frame const *stack_manager::get_stack_frame(ptr_t address) const
{
	if (
		this->stack_frames.empty()
		|| address < this->stack_frames[0].begin_address
		|| address > this->stack_frames.back().begin_address + this->stack_frames.back().total_size
	)
	{
		return nullptr;
	}
	else if (address >= this->stack_frames.back().begin_address)
	{
		return &this->stack_frames.back();
	}

	// find the last element that has an address that is less than or equal to address.
	// we do this by finding the first element, whose address is greater than address
	// and taking the element before that
	auto const it = std::upper_bound(
		this->stack_frames.begin(), this->stack_frames.end(),
		address,
		[](ptr_t address, auto const &stack_frame) {
			return address < stack_frame.begin_address;
		}
	);
	return &*(it - 1);
}

stack_object *stack_manager::get_stack_object(ptr_t address)
{
	auto const frame = this->get_stack_frame(address);
	if (frame == nullptr)
	{
		return nullptr;
	}

	bz_assert(address >= frame->begin_address && address <= frame->begin_address + frame->total_size);
	// find the last element that has an address that is less than or equal to address.
	// we do this by finding the first element, whose address is greater than address
	// and taking the element before that
	auto const it = std::upper_bound(
		frame->objects.begin(), frame->objects.end(),
		address,
		[](ptr_t address, auto const &object) {
			return address < object.address;
		}
	);
	return &*(it - 1);
}

stack_object const *stack_manager::get_stack_object(ptr_t address) const
{
	auto const frame = this->get_stack_frame(address);
	if (frame == nullptr)
	{
		return nullptr;
	}

	bz_assert(address >= frame->begin_address && address <= frame->begin_address + frame->total_size);
	// find the last element that has an address that is less than or equal to address.
	// we do this by finding the first element, whose address is greater than address
	// and taking the element before that
	auto const it = std::upper_bound(
		frame->objects.begin(), frame->objects.end(),
		address,
		[](ptr_t address, auto const &object) {
			return address < object.address;
		}
	);
	return &*(it - 1);
}

bool stack_manager::check_dereference(ptr_t address, type const *object_type) const
{
	auto const object = this->get_stack_object(address);
	return object != nullptr && object->check_dereference(address, object_type);
}

bz::vector<error_reason_t> stack_manager::get_dereference_error_reason(ptr_t address, type const *object_type) const
{
	auto const object = this->get_stack_object(address);
	bz_assert(object != nullptr);
	bz::vector<error_reason_t> result;
	result.push_back({ object->object_src_tokens, object->get_dereference_error_reason(address, object_type) });
	return result;
}

bool stack_manager::check_slice_construction(ptr_t begin, ptr_t end, bool end_is_one_past_the_end, type const *elem_type) const
{
	if (end < begin)
	{
		return false;
	}

	auto const object = this->get_stack_object(begin);
	if (object == nullptr)
	{
		return false;
	}
	else if (end > object->address + object->object_size())
	{
		return false;
	}
	else
	{
		return object->check_slice_construction(begin, end, end_is_one_past_the_end, elem_type);
	}
}

bz::vector<error_reason_t> stack_manager::get_slice_construction_error_reason(
	ptr_t begin,
	ptr_t end,
	bool end_is_one_past_the_end,
	type const *elem_type
) const
{
	auto const begin_stack_object = this->get_stack_object(begin);
	auto const end_stack_object   = this->get_stack_object(end);
	bz_assert(begin_stack_object != nullptr);
	bz_assert(end_stack_object != nullptr);

	bz::vector<error_reason_t> result;
	if (begin_stack_object == end_stack_object)
	{
		auto messages = begin_stack_object->get_slice_construction_error_reason(begin, end, end_is_one_past_the_end, elem_type);
		result.reserve(messages.size());
		result.append_move(messages.transform([begin_stack_object](auto &message) {
			return error_reason_t{ begin_stack_object->object_src_tokens, std::move(message) };
		}));
	}
	else
	{
		result.reserve(3);
		result.push_back({ {}, "begin and end addresses point to different stack objects" });
		result.push_back({ begin_stack_object->object_src_tokens, "begin address points to this stack object" });
		result.push_back({ end_stack_object->object_src_tokens, "end address points to this stack object" });
	}

	return result;
}

bz::optional<int> stack_manager::compare_pointers(ptr_t lhs, ptr_t rhs) const
{
	auto const lhs_stack_object = this->get_stack_object(lhs);
	if (lhs_stack_object == nullptr)
	{
		return {};
	}
	else if (rhs < lhs_stack_object->address || rhs > lhs_stack_object->address + lhs_stack_object->object_size())
	{
		return {};
	}
	else
	{
		return lhs == rhs ? 0 : (lhs < rhs ? -1 : 1);
	}
}

bz::vector<error_reason_t> stack_manager::get_compare_pointers_error_reason(ptr_t lhs, ptr_t rhs) const
{
	auto const lhs_stack_object = this->get_stack_object(lhs);
	auto const rhs_stack_object = this->get_stack_object(rhs);
	bz_assert(lhs_stack_object != nullptr);
	bz_assert(rhs_stack_object != nullptr);

	bz::vector<error_reason_t> result;
	bz_assert(lhs_stack_object != rhs_stack_object);

	result.reserve(3);
	result.push_back({ {}, "lhs and rhs addresses point to different stack objects" });
	result.push_back({ lhs_stack_object->object_src_tokens, "lhs address points to this stack object" });
	result.push_back({ rhs_stack_object->object_src_tokens, "rhs address points to this stack object" });

	return result;
}

pointer_arithmetic_result_t stack_manager::do_pointer_arithmetic(
	ptr_t address,
	bool is_one_past_the_end,
	int64_t offset,
	type const *object_type
) const
{
	auto const object = this->get_stack_object(address);
	if (object == nullptr)
	{
		return { 0, false };
	}
	else
	{
		return object->do_pointer_arithmetic(address, is_one_past_the_end, offset, object_type);
	}
}

bz::vector<error_reason_t> stack_manager::get_pointer_arithmetic_error_reason(
	ptr_t address,
	bool is_one_past_the_end,
	int64_t offset,
	type const *object_type
) const
{
	auto const object = this->get_stack_object(address);
	bz_assert(object != nullptr);
	auto messages = object->get_pointer_arithmetic_error_reason(address, is_one_past_the_end, offset, object_type);

	bz::vector<error_reason_t> result;
	result.reserve(messages.size());
	result.append_move(messages.transform([object](auto &message) {
		return error_reason_t{ object->object_src_tokens, std::move(message) };
	}));
	return result;
}

pointer_arithmetic_result_t stack_manager::do_pointer_arithmetic_unchecked(
	ptr_t address,
	bool is_one_past_the_end,
	int64_t offset,
	type const *object_type
) const
{
	auto const object = this->get_stack_object(address);
	bz_assert(object != nullptr);
	return object->do_pointer_arithmetic_unchecked(address, is_one_past_the_end, offset, object_type);
}

bz::optional<int64_t> stack_manager::do_pointer_difference(
	ptr_t lhs,
	ptr_t rhs,
	bool lhs_is_one_past_the_end,
	bool rhs_is_one_past_the_end,
	type const *object_type
) const
{
	auto const object = this->get_stack_object(lhs);
	if (object == nullptr)
	{
		return {};
	}
	else if (rhs < object->address || rhs > object->address + object->object_size())
	{
		return {};
	}
	else
	{
		return object->do_pointer_difference(lhs, rhs, lhs_is_one_past_the_end, rhs_is_one_past_the_end, object_type);
	}
}

bz::vector<error_reason_t> stack_manager::get_pointer_difference_error_reason(
	ptr_t lhs,
	ptr_t rhs,
	bool lhs_is_one_past_the_end,
	bool rhs_is_one_past_the_end,
	type const *object_type
) const
{
	auto const lhs_object = this->get_stack_object(lhs);
	auto const rhs_object = this->get_stack_object(rhs);
	bz_assert(lhs_object != nullptr);
	bz_assert(rhs_object != nullptr);

	bz::vector<error_reason_t> result;
	if (lhs_object != rhs_object)
	{
		result.reserve(3);
		result.push_back({ {}, "lhs and rhs addresses point to different stack objects" });
		result.push_back({ lhs_object->object_src_tokens, "lhs address points to this stack object" });
		result.push_back({ rhs_object->object_src_tokens, "rhs address points to this stack object" });
	}
	else
	{
		auto messages = lhs_object->get_pointer_difference_error_reason(
			lhs,
			rhs,
			lhs_is_one_past_the_end,
			rhs_is_one_past_the_end,
			object_type
		);
		result.reserve(messages.size());
		result.append_move(messages.transform([lhs_object](auto &message) {
			return error_reason_t{ lhs_object->object_src_tokens, std::move(message) };
		}));
	}

	return result;
}

uint8_t *stack_manager::get_memory(ptr_t address)
{
	auto const object = this->get_stack_object(address);
	bz_assert(object != nullptr);
	return object->get_memory(address);
}

uint8_t const *stack_manager::get_memory(ptr_t address) const
{
	auto const object = this->get_stack_object(address);
	bz_assert(object != nullptr);
	return object->get_memory(address);
}

allocation::allocation(call_stack_info_t alloc_info, ptr_t address, type const *elem_type, uint64_t count)
	: object(address, elem_type, count),
	  alloc_info(std::move(alloc_info)),
	  free_info(),
	  is_freed(false)
{}

free_result allocation::free(call_stack_info_t free_info)
{
	if (this->is_freed)
	{
		return free_result::double_free;
	}

	this->object.memory.clear();
	this->object.properties.clear();
	this->free_info = std::move(free_info);
	this->is_freed = true;
	return free_result::good;
}

heap_manager::heap_manager(ptr_t heap_begin)
	: head(heap_begin),
	  allocations()
{}

allocation *heap_manager::get_allocation(ptr_t address)
{
	if (this->allocations.empty() || address < this->allocations[0].object.address)
	{
		return nullptr;
	}
	else if (address >= this->allocations.back().object.address)
	{
		return &this->allocations.back();
	}

	// find the first element that has an address that is less than or equal to address.
	// we do this by finding the first element, whose address is greater than address
	// and taking the element before that
	auto const it = std::upper_bound(
		this->allocations.begin(), this->allocations.end(),
		address,
		[](ptr_t address, auto const &allocation) {
			return address < allocation.object.address;
		}
	);
	return &*(it - 1);
}

allocation const *heap_manager::get_allocation(ptr_t address) const
{
	if (this->allocations.empty() || address < this->allocations[0].object.address)
	{
		return nullptr;
	}
	else if (address >= this->allocations.back().object.address)
	{
		return &this->allocations.back();
	}

	// find the first element that has an address that is less than or equal to address.
	// we do this by finding the first element, whose address is greater than address
	// and taking the element before that
	auto const it = std::upper_bound(
		this->allocations.begin(), this->allocations.end(),
		address,
		[](ptr_t address, auto const &allocation) {
			return address < allocation.object.address;
		}
	);
	return &*(it - 1);
}

ptr_t heap_manager::allocate(call_stack_info_t alloc_info, type const *object_type, uint64_t count)
{
	auto const address = this->head;
	auto const allocation_size = object_type->size * count;
	// we round up the allocation size to the nearest 16-byte boundary (heap_object_align == 16)
	// if it's already at such a boundary, then we simply add 16 to add some padding
	auto const rounded_allocation_size = allocation_size + (heap_object_align - allocation_size % heap_object_align);
	this->head += rounded_allocation_size;
	this->allocations.emplace_back(std::move(alloc_info), address, object_type, count);
	return address;
}

free_result heap_manager::free(call_stack_info_t free_info, ptr_t address)
{
	auto const allocation = this->get_allocation(address);
	if (allocation == nullptr || address != allocation->object.address)
	{
		return free_result::invalid_pointer;
	}
	else
	{
		return allocation->free(std::move(free_info));
	}
}

bool heap_manager::check_dereference(ptr_t address, type const *object_type) const
{
	auto const allocation = this->get_allocation(address);
	return allocation != nullptr && !allocation->is_freed &&  allocation->object.check_dereference(address, object_type);
}

bz::vector<error_reason_t> heap_manager::get_dereference_error_reason(ptr_t address, type const *object_type) const
{
	auto const allocation = this->get_allocation(address);
	bz_assert(allocation != nullptr);

	bz::vector<error_reason_t> result;
	if (allocation->is_freed)
	{
		result.reserve(2 + allocation->alloc_info.call_stack.size() + allocation->free_info.call_stack.size());
		result.push_back({ allocation->alloc_info.src_tokens, "address points to an object in this allocation, which was freed" });
		add_allocation_info(result, allocation->alloc_info);
		add_free_info(result, allocation->free_info);
	}
	else
	{
		result.reserve(1 + allocation->alloc_info.call_stack.size());
		result.push_back({ allocation->alloc_info.src_tokens, allocation->object.get_dereference_error_reason(address, object_type) });
		add_allocation_info(result, allocation->alloc_info);
	}
	return result;
}

bool heap_manager::check_inplace_construct(ptr_t address, type const *object_type) const
{
	auto const allocation = this->get_allocation(address);
	bz_assert(allocation != nullptr);
	if (allocation->is_freed)
	{
		return false;
	}
	else
	{
		return allocation->object.check_inplace_construct(address, object_type);
	}
}

bz::vector<error_reason_t> heap_manager::get_inplace_construct_error_reason(ptr_t address, type const *object_type) const
{
	auto const allocation = this->get_allocation(address);
	bz_assert(allocation != nullptr);

	bz::vector<error_reason_t> result;
	if (allocation->is_freed)
	{
		result.reserve(2 + allocation->alloc_info.call_stack.size() + allocation->free_info.call_stack.size());
		result.push_back({ allocation->alloc_info.src_tokens, "address points to an element in this allocation, which was freed" });
		add_allocation_info(result, allocation->alloc_info);
		add_free_info(result, allocation->free_info);
	}
	else
	{
		result.reserve(1 + allocation->alloc_info.call_stack.size());
		result.push_back({ allocation->alloc_info.src_tokens, allocation->object.get_inplace_construct_error_reason(address, object_type) });
		add_allocation_info(result, allocation->alloc_info);
	}
	return result;
}

bool heap_manager::check_destruct_value(ptr_t address, type const *object_type) const
{
	auto const allocation = this->get_allocation(address);
	bz_assert(allocation != nullptr);
	if (allocation->is_freed)
	{
		return false;
	}
	else
	{
		return allocation->object.check_destruct_value(address, object_type);
	}
}

bz::vector<error_reason_t> heap_manager::get_destruct_value_error_reason(ptr_t address, type const *object_type) const
{
	auto const allocation = this->get_allocation(address);
	bz_assert(allocation != nullptr);

	bz::vector<error_reason_t> result;
	if (allocation->is_freed)
	{
		result.reserve(2 + allocation->alloc_info.call_stack.size() + allocation->free_info.call_stack.size());
		result.push_back({ allocation->alloc_info.src_tokens, "address points to an element in this allocation, which was freed" });
		add_allocation_info(result, allocation->alloc_info);
		add_free_info(result, allocation->free_info);
	}
	else
	{
		result.reserve(1 + allocation->alloc_info.call_stack.size());
		result.push_back({ allocation->alloc_info.src_tokens, allocation->object.get_destruct_value_error_reason(address, object_type) });
		add_allocation_info(result, allocation->alloc_info);
	}
	return result;
}

bool heap_manager::check_slice_construction(ptr_t begin, ptr_t end, bool end_is_one_past_the_end, type const *elem_type) const
{
	bz_assert(begin <= end);
	auto const allocation = this->get_allocation(begin);
	if (allocation == nullptr)
	{
		return false;
	}
	else if (allocation->is_freed)
	{
		return false;
	}
	else if (end > allocation->object.address + allocation->object.object_size()) // end is in a different allocation
	{
		return false;
	}
	else
	{
		return allocation->object.check_slice_construction(begin, end, end_is_one_past_the_end, elem_type);
	}
}

bz::vector<error_reason_t> heap_manager::get_slice_construction_error_reason(
	ptr_t begin,
	ptr_t end,
	bool end_is_one_past_the_end,
	type const *elem_type
) const
{
	bz_assert(begin <= end);
	auto const begin_allocation = this->get_allocation(begin);
	auto const end_allocation = this->get_allocation(end);

	bz_assert(begin_allocation != nullptr);
	bz_assert(end_allocation != nullptr);

	bz::vector<error_reason_t> result;
	if (begin_allocation != end_allocation)
	{
		result.reserve(3 + begin_allocation->alloc_info.call_stack.size() + end_allocation->alloc_info.call_stack.size());
		result.push_back({ {}, "begin and end addresses point to different allocations" });
		result.push_back({ begin_allocation->alloc_info.src_tokens, "begin address points to an object in this allocation" });
		add_allocation_info(result, begin_allocation->alloc_info);
		result.push_back({ end_allocation->alloc_info.src_tokens, "end address points to an object in this allocation" });
		add_allocation_info(result, end_allocation->alloc_info);
	}
	else if (begin_allocation->is_freed)
	{
		result.reserve(1 + begin_allocation->alloc_info.call_stack.size() + begin_allocation->free_info.call_stack.size() + 1);
		result.push_back({
			begin_allocation->alloc_info.src_tokens,
			"begin and end addresses point to objects in this allocation, which was freed"
		});
		add_allocation_info(result, begin_allocation->alloc_info);
		add_free_info(result, begin_allocation->free_info);
	}
	else
	{
		auto messages = begin_allocation->object.get_slice_construction_error_reason(begin, end, end_is_one_past_the_end, elem_type);
		result.reserve(messages.size() + begin_allocation->alloc_info.call_stack.size());
		result.append_move(messages.transform([begin_allocation](auto &message) {
			return error_reason_t{ begin_allocation->alloc_info.src_tokens, std::move(message) };
		}));
		add_allocation_info(result, begin_allocation->alloc_info);
	}

	return result;
}

bz::optional<int> heap_manager::compare_pointers(ptr_t lhs, ptr_t rhs) const
{
	auto const lhs_allocation = this->get_allocation(lhs);
	if (lhs_allocation == nullptr)
	{
		return {};
	}
	else if (lhs_allocation->is_freed)
	{
		return {};
	}
	else if (rhs < lhs_allocation->object.address || rhs > lhs_allocation->object.address + lhs_allocation->object.object_size())
	{
		return {};
	}
	else
	{
		return lhs == rhs ? 0 : (lhs < rhs ? -1 : 1);
	}
}

bz::vector<error_reason_t> heap_manager::get_compare_pointers_error_reason(ptr_t lhs, ptr_t rhs) const
{
	auto const lhs_allocation = this->get_allocation(lhs);
	auto const rhs_allocation = this->get_allocation(rhs);
	bz_assert(lhs_allocation != nullptr);
	bz_assert(rhs_allocation != nullptr);

	bz::vector<error_reason_t> result;
	if (lhs_allocation != rhs_allocation)
	{
		result.reserve(3 + lhs_allocation->alloc_info.call_stack.size() + rhs_allocation->alloc_info.call_stack.size());
		result.push_back({ {}, "lhs and rhs addresses point to different allocations" });
		result.push_back({ lhs_allocation->alloc_info.src_tokens, "lhs address points to an object in this allocation" });
		add_allocation_info(result, lhs_allocation->alloc_info);
		result.push_back({ rhs_allocation->alloc_info.src_tokens, "rhs address points to an object in this allocation" });
		add_allocation_info(result, rhs_allocation->alloc_info);
	}
	else if (lhs_allocation->is_freed)
	{
		result.reserve(1 + lhs_allocation->alloc_info.call_stack.size() + lhs_allocation->free_info.call_stack.size() + 1);
		result.push_back({
			lhs_allocation->alloc_info.src_tokens,
			"lhs and rhs addresses point to objects in this allocation, which was freed"
		});
		add_allocation_info(result, lhs_allocation->alloc_info);
		add_free_info(result, lhs_allocation->free_info);
	}

	return result;
}

pointer_arithmetic_result_t heap_manager::do_pointer_arithmetic(
	ptr_t address,
	bool is_one_past_the_end,
	int64_t offset,
	type const *object_type
) const
{
	auto const allocation = this->get_allocation(address);
	if (allocation == nullptr || allocation->is_freed)
	{
		return { 0, false };
	}
	else
	{
		return allocation->object.do_pointer_arithmetic(address, is_one_past_the_end, offset, object_type);
	}
}

bz::vector<error_reason_t> heap_manager::get_pointer_arithmetic_error_reason(
	ptr_t address,
	bool is_one_past_the_end,
	int64_t offset,
	type const *object_type
) const
{
	auto const allocation = this->get_allocation(address);
	bz_assert(allocation != nullptr);

	if (allocation->is_freed)
	{
		bz::vector<error_reason_t> result;
		result.reserve(2 + allocation->alloc_info.call_stack.size() + allocation->free_info.call_stack.size());
		result.push_back({ allocation->alloc_info.src_tokens, "address points to an object in this allocation, which was freed" });
		add_allocation_info(result, allocation->alloc_info);
		add_free_info(result, allocation->free_info);
		return result;
	}
	else
	{
		auto messages = allocation->object.get_pointer_arithmetic_error_reason(address, is_one_past_the_end, offset, object_type);

		bz::vector<error_reason_t> result;
		result.reserve(messages.size() + allocation->alloc_info.call_stack.size());
		result.append_move(messages.transform([allocation](auto &message) {
			return error_reason_t{ allocation->alloc_info.src_tokens, std::move(message) };
		}));
		add_allocation_info(result, allocation->alloc_info);
		return result;
	}
}

pointer_arithmetic_result_t heap_manager::do_pointer_arithmetic_unchecked(
	ptr_t address,
	bool is_one_past_the_end,
	int64_t offset,
	type const *object_type
) const
{
	auto const allocation = this->get_allocation(address);
	bz_assert(allocation != nullptr && !allocation->is_freed);
	return allocation->object.do_pointer_arithmetic_unchecked(address, is_one_past_the_end, offset, object_type);
}

bz::optional<int64_t> heap_manager::do_pointer_difference(
	ptr_t lhs,
	ptr_t rhs,
	bool lhs_is_one_past_the_end,
	bool rhs_is_one_past_the_end,
	type const *object_type
) const
{
	auto const allocation = this->get_allocation(lhs);
	if (allocation == nullptr)
	{
		return {};
	}
	else if (allocation->is_freed)
	{
		return {};
	}
	else if (rhs < allocation->object.address || rhs > allocation->object.address + allocation->object.object_size())
	{
		return {};
	}
	else
	{
		return allocation->object.do_pointer_difference(lhs, rhs, lhs_is_one_past_the_end, rhs_is_one_past_the_end, object_type);
	}
}

bz::vector<error_reason_t> heap_manager::get_pointer_difference_error_reason(
	ptr_t lhs,
	ptr_t rhs,
	bool lhs_is_one_past_the_end,
	bool rhs_is_one_past_the_end,
	type const *object_type
) const
{
	auto const lhs_allocation = this->get_allocation(lhs);
	auto const rhs_allocation = this->get_allocation(rhs);
	bz_assert(lhs_allocation != nullptr);
	bz_assert(rhs_allocation != nullptr);

	bz::vector<error_reason_t> result;
	if (lhs_allocation != rhs_allocation)
	{
		result.reserve(3 + lhs_allocation->alloc_info.call_stack.size() + rhs_allocation->alloc_info.call_stack.size());
		result.push_back({ {}, "lhs and rhs addresses point to different allocations" });
		result.push_back({ lhs_allocation->alloc_info.src_tokens, "lhs address points to an object in this allocation" });
		add_allocation_info(result, lhs_allocation->alloc_info);
		result.push_back({ rhs_allocation->alloc_info.src_tokens, "rhs address points to an object in this allocation" });
		add_allocation_info(result, rhs_allocation->alloc_info);
	}
	else if (lhs_allocation->is_freed)
	{
		result.reserve(1 + lhs_allocation->alloc_info.call_stack.size() + lhs_allocation->free_info.call_stack.size() + 1);
		result.push_back({
			lhs_allocation->alloc_info.src_tokens,
			"lhs and rhs addresses point to objects in this allocation, which was freed"
		});
		add_allocation_info(result, lhs_allocation->alloc_info);
		add_free_info(result, lhs_allocation->free_info);
	}
	else
	{
		auto messages = lhs_allocation->object.get_pointer_difference_error_reason(
			lhs,
			rhs,
			lhs_is_one_past_the_end,
			rhs_is_one_past_the_end,
			object_type
		);
		result.reserve(messages.size() + lhs_allocation->alloc_info.call_stack.size());
		result.append_move(messages.transform([lhs_allocation](auto &message) {
			return error_reason_t{ lhs_allocation->alloc_info.src_tokens, std::move(message) };
		}));
		add_allocation_info(result, lhs_allocation->alloc_info);
	}

	return result;
}

uint8_t *heap_manager::get_memory(ptr_t address)
{
	auto const allocation = this->get_allocation(address);
	bz_assert(allocation != nullptr);
	bz_assert(!allocation->is_freed);
	return allocation->object.get_memory(address);
}

uint8_t const *heap_manager::get_memory(ptr_t address) const
{
	auto const allocation = this->get_allocation(address);
	bz_assert(allocation != nullptr);
	bz_assert(!allocation->is_freed);
	return allocation->object.get_memory(address);
}

meta_memory_manager::meta_memory_manager(ptr_t meta_begin)
	: segment_info{},
	  stack_object_pointers(),
	  one_past_the_end_pointers()
{
	auto const max_address = meta_begin <= std::numeric_limits<uint32_t>::max()
		? std::numeric_limits<uint32_t>::max()
		: std::numeric_limits<uint64_t>::max();
	auto const meta_address_space_size = max_address - meta_begin + 1;

	constexpr size_t segment_count = meta_segment_info_t::segment_count;
	constexpr size_t rounded_segment_count = std::bit_ceil(segment_count); // just to make addresses a bit nicer, when we print them
	auto const segment_size = meta_address_space_size / rounded_segment_count;

	for (size_t i = 0; i < segment_count; ++i)
	{
		this->segment_info.segment_begins[i] = meta_begin + i * segment_size;
	}
}

size_t meta_memory_manager::get_stack_object_index(ptr_t address) const
{
	return address - this->segment_info.get_segment_begin<meta_memory_segment::stack_object>();
}

size_t meta_memory_manager::get_one_past_the_end_index(ptr_t address) const
{
	return address - this->segment_info.get_segment_begin<meta_memory_segment::one_past_the_end>();
}

stack_object_pointer const &meta_memory_manager::get_stack_object_pointer(ptr_t address) const
{
	auto const index = this->get_stack_object_index(address);
	bz_assert(index < this->stack_object_pointers.size());
	return this->stack_object_pointers[index];
}

one_past_the_end_pointer const &meta_memory_manager::get_one_past_the_end_pointer(ptr_t address) const
{
	auto const index = this->get_one_past_the_end_index(address);
	bz_assert(index < this->one_past_the_end_pointers.size());
	return this->one_past_the_end_pointers[index];
}

ptr_t meta_memory_manager::make_stack_object_address(
	ptr_t address,
	uint32_t stack_frame_depth,
	uint32_t stack_frame_id,
	lex::src_tokens const &object_src_tokens
)
{
	auto const result_index = this->stack_object_pointers.size();
	this->stack_object_pointers.push_back({
		.stack_address = address,
		.stack_frame_depth = stack_frame_depth,
		.stack_frame_id = stack_frame_id,
		.object_src_tokens = object_src_tokens,
	});
	return this->segment_info.get_segment_begin<meta_memory_segment::stack_object>() + result_index;
}

ptr_t meta_memory_manager::make_inherited_stack_object_address(ptr_t address, ptr_t inherit_from)
{
	bz_assert(this->segment_info.get_segment(inherit_from) == meta_memory_segment::stack_object);
	auto const &stack_object = this->get_stack_object_pointer(inherit_from);
	return this->make_stack_object_address(
		address,
		stack_object.stack_frame_depth,
		stack_object.stack_frame_id,
		stack_object.object_src_tokens
	);
}

ptr_t meta_memory_manager::make_one_past_the_end_address(ptr_t address)
{
	auto const result_index = this->one_past_the_end_pointers.size();
	this->one_past_the_end_pointers.push_back({ address });
	return this->segment_info.get_segment_begin<meta_memory_segment::one_past_the_end>() + result_index;
}

ptr_t meta_memory_manager::get_real_address(ptr_t address) const
{
	switch (this->segment_info.get_segment(address))
	{
	case meta_memory_segment::stack_object:
		return this->get_stack_object_pointer(address).stack_address;
	case meta_memory_segment::one_past_the_end:
		return this->get_one_past_the_end_pointer(address).address;
	}
}

memory_manager::memory_manager(global_segment_info_t _segment_info, global_memory_manager *_global_memory)
	: segment_info(_segment_info),
	  global_memory(_global_memory),
	  stack(_segment_info.get_segment_begin<memory_segment::stack>()),
	  heap(_segment_info.get_segment_begin<memory_segment::heap>()),
	  meta_memory(_segment_info.get_segment_begin<memory_segment::meta>())
{}

[[nodiscard]] bool memory_manager::push_stack_frame(bz::array_view<alloca const> types)
{
	this->stack.push_stack_frame(types);
	return this->stack.head < this->segment_info.get_segment_begin<memory_segment::heap>();
}

void memory_manager::pop_stack_frame(void)
{
	this->stack.pop_stack_frame();
}

ptr_t memory_manager::make_current_frame_stack_object_address(stack_object const &object)
{
	auto const &current_frame = this->stack.stack_frames.back();
	return this->meta_memory.make_stack_object_address(
		object.address,
		static_cast<uint32_t>(this->stack.stack_frames.size() - 1),
		current_frame.id,
		object.object_src_tokens
	);
}

struct remove_meta_result_t
{
	ptr_t address;
	memory_segment segment;
	bool is_one_past_the_end;
	bool is_finished_stack_frame;
	bool is_stack_pointer;
};

static remove_meta_result_t remove_meta(ptr_t address, memory_manager const &manager)
{
	auto segment = manager.segment_info.get_segment(address);
	bool is_one_past_the_end = false;
	bool is_stack_object = false;
	while (segment == memory_segment::meta)
	{
		switch (manager.meta_memory.segment_info.get_segment(address))
		{
		case meta_memory_segment::stack_object:
		{
			auto const &stack_object = manager.meta_memory.get_stack_object_pointer(address);
			if (
				stack_object.stack_frame_depth >= manager.stack.stack_frames.size()
				|| manager.stack.stack_frames[stack_object.stack_frame_depth].id != stack_object.stack_frame_id
			)
			{
				return { address, segment, is_one_past_the_end, true, true };
			}

			is_stack_object =true;
			address = stack_object.stack_address;
			break;
		}
		case meta_memory_segment::one_past_the_end:
			is_one_past_the_end = true;
			address = manager.meta_memory.get_one_past_the_end_pointer(address).address;
			break;
		}

		segment = manager.segment_info.get_segment(address);
	}

	if (segment == memory_segment::global)
	{
		auto const global_segment = manager.global_memory->segment_info.get_segment(address);
		switch (global_segment)
		{
		case global_meta_memory_segment::one_past_the_end:
			is_one_past_the_end = true;
			address = manager.global_memory->get_one_past_the_end_pointer(address).address;
			break;
		case global_meta_memory_segment::functions:
			break;
		case global_meta_memory_segment::objects:
			break;
		}
	}

	return { address, segment, is_one_past_the_end, false, is_stack_object };
}

ptr_t memory_manager::allocate(call_stack_info_t alloc_info, type const *object_type, uint64_t count)
{
	auto const remaining_size = this->segment_info.get_segment_begin<memory_segment::meta>() - this->heap.head;
	auto const [total_size, overflowed] = mul_overflow(count, object_type->size);
	if (overflowed || total_size > remaining_size)
	{
		return 0;
	}
	return this->heap.allocate(std::move(alloc_info), object_type, count);
}

free_result memory_manager::free(call_stack_info_t free_info, ptr_t address)
{
	auto const segment = this->segment_info.get_segment(address);
	if (segment != memory_segment::heap)
	{
		return free_result::invalid_pointer;
	}
	else
	{
		return this->heap.free(std::move(free_info), address);
	}
}

bz::vector<error_reason_t> memory_manager::get_free_error_reason(ptr_t _address)
{
	auto const [address, segment, is_one_past_the_end, is_finished_stack_frame, is_stack_object] = remove_meta(_address, *this);

	if (is_finished_stack_frame)
	{
		bz::vector<error_reason_t> result;
		auto const &stack_object = this->meta_memory.get_stack_object_pointer(address);
		result.push_back({ stack_object.object_src_tokens, "address points to an object from a finished stack frame" });
		return result;
	}

	switch (segment)
	{
	case memory_segment::global:
		bz_unreachable;
	case memory_segment::stack:
	{
		bz::vector<error_reason_t> result;
		auto const stack_object = this->stack.get_stack_object(address);
		result.push_back({ stack_object->object_src_tokens, "address points to this stack object" });
		return result;
	}
	case memory_segment::heap:
	{
		auto const allocation = this->heap.get_allocation(address);
		if (allocation->is_freed && address == allocation->object.address)
		{
			bz::vector<error_reason_t> result;
			result.reserve(2 + allocation->alloc_info.call_stack.size() + allocation->free_info.call_stack.size());
			result.push_back({ allocation->alloc_info.src_tokens, "address points to this allocation" });
			add_allocation_info(result, allocation->alloc_info);
			add_free_info(result, allocation->free_info);
			return result;
		}
		else
		{
			bz::vector<error_reason_t> result;
			auto const offset = (address - allocation->object.address);
			auto const elem_size = allocation->object.elem_size();
			auto const elem_index = offset / elem_size - (is_one_past_the_end && offset % elem_size == 0 ? 1 : 0);
			result.reserve(1 + allocation->alloc_info.call_stack.size());
			if (is_one_past_the_end)
			{
				result.push_back({
					allocation->alloc_info.src_tokens,
					bz::format("address is a one-past-the-end pointer to an object in element {} of this allocation", elem_index)
				});
			}
			else
			{
				result.push_back({
					allocation->alloc_info.src_tokens,
					bz::format("address points to an object in element {} of this allocation", elem_index)
				});
			}
			add_allocation_info(result, allocation->alloc_info);
			return result;
		}
	}
	case memory_segment::meta:
		bz_unreachable;
	}
}

bool memory_manager::check_dereference(ptr_t _address, type const *object_type) const
{
	bz_assert(_address != 0);
	auto const [address, segment, is_one_past_the_end, is_finished_stack_frame, is_stack_object] = remove_meta(_address, *this);

	if (is_one_past_the_end || is_finished_stack_frame)
	{
		return false;
	}

	switch (segment)
	{
	case memory_segment::global:
		return this->global_memory->check_dereference(address, object_type);
	case memory_segment::stack:
		return this->stack.check_dereference(address, object_type);
	case memory_segment::heap:
		return this->heap.check_dereference(address, object_type);
	case memory_segment::meta:
		bz_unreachable;
	}
}

bz::vector<error_reason_t> memory_manager::get_dereference_error_reason(ptr_t _address, type const *object_type) const
{
	bz_assert(_address != 0);
	auto const [address, segment, is_one_past_the_end, is_finished_stack_frame, is_stack_object] = remove_meta(_address, *this);

	if (is_finished_stack_frame)
	{
		bz::vector<error_reason_t> result;
		auto const &stack_object = this->meta_memory.get_stack_object_pointer(address);
		result.push_back({ stack_object.object_src_tokens, "address points to an object from a finished stack frame" });
		return result;
	}
	else if (is_one_past_the_end)
	{
		bz::vector<error_reason_t> result;
		switch (segment)
		{
		case memory_segment::global:
		{
			auto const global_object = this->global_memory->get_global_object(address);
			bz_assert(global_object != nullptr);
			result.push_back({ global_object->object_src_tokens, "address is a one-past-the-end pointer into this global object" });
			break;
		}
		case memory_segment::stack:
		{
			auto const stack_object = this->stack.get_stack_object(address);
			bz_assert(stack_object != nullptr);
			result.push_back({ stack_object->object_src_tokens, "address is a one-past-the-end pointer into this stack object" });
			break;
		}
		case memory_segment::heap:
		{
			auto const allocation = this->heap.get_allocation(address);
			bz_assert(allocation != nullptr);
			result.reserve(1 + allocation->alloc_info.call_stack.size());
			result.push_back({ allocation->alloc_info.src_tokens, "address is a one-past-the-end pointer into this allocation" });
			add_allocation_info(result, allocation->alloc_info);
			break;
		}
		case memory_segment::meta:
			bz_unreachable;
		}
		return result;
	}
	else
	{
		switch (segment)
		{
		case memory_segment::global:
			return this->global_memory->get_dereference_error_reason(address, object_type);
		case memory_segment::stack:
			return this->stack.get_dereference_error_reason(address, object_type);
		case memory_segment::heap:
			return this->heap.get_dereference_error_reason(address, object_type);
		case memory_segment::meta:
			bz_unreachable;
		}
	}
}

bool memory_manager::check_inplace_construct(ptr_t _address, type const *object_type) const
{
	bz_assert(_address != 0);
	auto const [address, segment, is_one_past_the_end, is_finished_stack_frame, is_stack_object] = remove_meta(_address, *this);

	if (is_one_past_the_end || segment != memory_segment::heap)
	{
		return false;
	}
	else
	{
		return this->heap.check_inplace_construct(address, object_type);
	}
}

bz::vector<error_reason_t> memory_manager::get_inplace_construct_error_reason(ptr_t _address, type const *object_type) const
{
	bz_assert(_address != 0);
	auto const [address, segment, is_one_past_the_end, is_finished_stack_frame, is_stack_object] = remove_meta(_address, *this);

	if (is_finished_stack_frame)
	{
		bz::vector<error_reason_t> result;

		auto const &stack_object = this->meta_memory.get_stack_object_pointer(address);
		result.push_back({ stack_object.object_src_tokens, "address points to a stack object from a finished stack frame" });

		return result;
	}
	else if (segment != memory_segment::heap)
	{
		bz::vector<error_reason_t> result;
		result.reserve(2);

		result.push_back({ {}, "address points to an object that is not on the heap" });
		bz_assert(segment == memory_segment::stack);
		auto const stack_object = this->stack.get_stack_object(address);
		bz_assert(stack_object != nullptr);
		result.push_back({ stack_object->object_src_tokens, "address points to this stack object" });

		return result;
	}
	else if (is_one_past_the_end)
	{
		auto const allocation = this->heap.get_allocation(address);
		bz::vector<error_reason_t> result;
		result.reserve(1 + allocation->alloc_info.call_stack.size());
		result.push_back({ allocation->alloc_info.src_tokens, "address is a one-past-the-end pointer into this allocation" });
		add_allocation_info(result, allocation->alloc_info);
		return result;
	}
	else
	{
		return this->heap.get_inplace_construct_error_reason(address, object_type);
	}
}

bool memory_manager::check_destruct_value(ptr_t _address, type const *object_type) const
{
	bz_assert(_address != 0);
	auto const [address, segment, is_one_past_the_end, is_finished_stack_frame, is_stack_object] = remove_meta(_address, *this);

	if (is_one_past_the_end || segment != memory_segment::heap)
	{
		return false;
	}
	else
	{
		return this->heap.check_destruct_value(address, object_type);
	}
}

bz::vector<error_reason_t> memory_manager::get_destruct_value_error_reason(ptr_t _address, type const *object_type) const
{
	bz_assert(_address != 0);
	auto const [address, segment, is_one_past_the_end, is_finished_stack_frame, is_stack_object] = remove_meta(_address, *this);

	if (is_finished_stack_frame)
	{
		bz::vector<error_reason_t> result;

		auto const &stack_object = this->meta_memory.get_stack_object_pointer(address);
		result.push_back({ stack_object.object_src_tokens, "address points to a stack object from a finished stack frame" });

		return result;
	}
	else if (segment != memory_segment::heap)
	{
		bz::vector<error_reason_t> result;
		result.reserve(2);

		result.push_back({ {}, "address points to an object that is not on the heap" });
		bz_assert(segment == memory_segment::stack);
		auto const stack_object = this->stack.get_stack_object(address);
		bz_assert(stack_object != nullptr);
		result.push_back({ stack_object->object_src_tokens, "address points to this stack object" });

		return result;
	}
	else if (is_one_past_the_end)
	{
		auto const allocation = this->heap.get_allocation(address);
		bz::vector<error_reason_t> result;
		result.reserve(1 + allocation->alloc_info.call_stack.size());
		result.push_back({ allocation->alloc_info.src_tokens, "address is a one-past-the-end pointer into this allocation" });
		add_allocation_info(result, allocation->alloc_info);
		return result;
	}
	else
	{
		return this->heap.get_destruct_value_error_reason(address, object_type);
	}
}

bool memory_manager::check_slice_construction(ptr_t _begin, ptr_t _end, type const *elem_type) const
{
	bz_assert(_begin != 0 && _end != 0);
	auto const [
		begin,
		begin_segment,
		begin_is_one_past_the_end,
		begin_is_finished_stack_frame,
		begin_is_stack_object
	] = remove_meta(_begin, *this);
	auto const [
		end,
		end_segment,
		end_is_one_past_the_end,
		end_is_finished_stack_frame,
		end_is_stack_object
	] = remove_meta(_end, *this);

	if (begin_is_finished_stack_frame || end_is_finished_stack_frame)
	{
		return false;
	}
	else if (begin_segment != end_segment)
	{
		return false;
	}
	else if (begin > end)
	{
		return false;
	}
	else if (begin_is_one_past_the_end && !end_is_one_past_the_end)
	{
		return false;
	}
	else if (begin_is_one_past_the_end && end_is_one_past_the_end && begin != end)
	{
		return false;
	}
	else if (begin == end && begin_is_one_past_the_end != end_is_one_past_the_end)
	{
		return false;
	}
	else
	{
		switch (begin_segment)
		{
		case memory_segment::global:
			return this->global_memory->check_slice_construction(begin, end, end_is_one_past_the_end, elem_type);
		case memory_segment::stack:
			return this->stack.check_slice_construction(begin, end, end_is_one_past_the_end, elem_type);
		case memory_segment::heap:
			return this->heap.check_slice_construction(begin, end, end_is_one_past_the_end, elem_type);
		case memory_segment::meta:
			bz_unreachable;
		}
	}
}

bz::vector<error_reason_t> memory_manager::get_slice_construction_error_reason(ptr_t _begin, ptr_t _end, type const *elem_type) const
{
	bz_assert(_begin != 0 && _end != 0);
	auto const [
		begin,
		begin_segment,
		begin_is_one_past_the_end,
		begin_is_finished_stack_frame,
		begin_is_stack_object
	] = remove_meta(_begin, *this);
	auto const [
		end,
		end_segment,
		end_is_one_past_the_end,
		end_is_finished_stack_frame,
		end_is_stack_object
	] = remove_meta(_end, *this);

	if (begin_is_finished_stack_frame && end_is_finished_stack_frame)
	{
		auto const &begin_stack_object = this->meta_memory.get_stack_object_pointer(begin);
		auto const &end_stack_object = this->meta_memory.get_stack_object_pointer(end);
		bz::vector<error_reason_t> result;
		if (
			begin_stack_object.object_src_tokens.begin == end_stack_object.object_src_tokens.begin
			&& begin_stack_object.object_src_tokens.pivot == end_stack_object.object_src_tokens.pivot
			&& begin_stack_object.object_src_tokens.end == end_stack_object.object_src_tokens.end
		)
		{
			result.push_back({
				begin_stack_object.object_src_tokens,
				"begin and end addresses point to an object from a finished stack frame"
			});
		}
		else
		{
			result.reserve(2);
			result.push_back({
				begin_stack_object.object_src_tokens,
				"begin address points to an object from a finished stack frame"
			});
			result.push_back({
				begin_stack_object.object_src_tokens,
				"end address points to an object from a finished stack frame"
			});
		}
		return result;
	}
	else if (begin_is_finished_stack_frame)
	{
		auto const &stack_object = this->meta_memory.get_stack_object_pointer(begin);
		bz::vector<error_reason_t> result;
		result.push_back({ stack_object.object_src_tokens, "begin address points to an object from a finished stack frame" });
		return result;
	}
	else if (end_is_finished_stack_frame)
	{
		auto const &stack_object = this->meta_memory.get_stack_object_pointer(end);
		bz::vector<error_reason_t> result;
		result.push_back({ stack_object.object_src_tokens, "end address points to an object from a finished stack frame" });
		return result;
	}
	else if (begin_segment != end_segment)
	{
		bz::vector<error_reason_t> result;
		result.reserve(3);
		result.push_back({ {}, "begin and end addresses point to different memory segments" });

		switch (begin_segment)
		{
		case memory_segment::global:
		{
			auto const global_object = this->global_memory->get_global_object(begin);
			bz_assert(global_object != nullptr);
			result.push_back({ global_object->object_src_tokens, "begin address points to this global object" });
			break;
		}
		case memory_segment::stack:
		{
			auto const stack_object = this->stack.get_stack_object(begin);
			bz_assert(stack_object != nullptr);
			result.push_back({ stack_object->object_src_tokens, "begin address points to this stack object" });
			break;
		}
		case memory_segment::heap:
		{
			auto const allocation = this->heap.get_allocation(begin);
			bz_assert(allocation != nullptr);
			result.reserve(1 + allocation->alloc_info.call_stack.size());
			result.push_back({ allocation->alloc_info.src_tokens, "begin address points to an object in this allocation" });
			add_allocation_info(result, allocation->alloc_info);
			break;
		}
		case memory_segment::meta:
			bz_unreachable;
		}

		switch (end_segment)
		{
		case memory_segment::global:
		{
			auto const global_object = this->global_memory->get_global_object(end);
			bz_assert(global_object != nullptr);
			result.push_back({ global_object->object_src_tokens, "end address points to this global object" });
			break;
		}
		case memory_segment::stack:
		{
			auto const stack_object = this->stack.get_stack_object(end);
			bz_assert(stack_object != nullptr);
			result.push_back({ stack_object->object_src_tokens, "end address points to this stack object" });
			break;
		}
		case memory_segment::heap:
		{
			auto const allocation = this->heap.get_allocation(end);
			bz_assert(allocation != nullptr);
			result.reserve(1 + allocation->alloc_info.call_stack.size());
			result.push_back({ allocation->alloc_info.src_tokens, "end address points to an object in this allocation" });
			add_allocation_info(result, allocation->alloc_info);
			break;
		}
		case memory_segment::meta:
			bz_unreachable;
		}

		return result;
	}
	else if (begin > end && this->check_slice_construction(_end, _begin, elem_type))
	{
		bz::vector<error_reason_t> result;
		result.push_back({ {}, "begin address points to an object that is after the end address"} );
		return result;
	}
	else if (
		begin > end
		|| (begin_is_one_past_the_end && !end_is_one_past_the_end)
		|| (begin_is_one_past_the_end && end_is_one_past_the_end && begin != end)
		|| (begin == end && begin_is_one_past_the_end != end_is_one_past_the_end)
	)
	{
		switch (begin_segment)
		{
		case memory_segment::global:
		{
			auto const begin_global_object = this->global_memory->get_global_object(begin);
			auto const end_global_object   = this->global_memory->get_global_object(end);
			bz_assert(begin_global_object != nullptr);
			bz_assert(end_global_object != nullptr);

			bz::vector<error_reason_t> result;
			result.reserve(3);
			if (begin_global_object == end_global_object)
			{
				result.push_back({
					begin_global_object->object_src_tokens,
					"begin and end addresses point to different subobjects in this global object"
				});

				if (begin_is_one_past_the_end)
				{
					result.push_back({
						begin_global_object->object_src_tokens,
						bz::format("begin address is a one-past-the-end pointer with offset {}", begin - begin_global_object->address)
					});
				}
				else
				{
					result.push_back({
						begin_global_object->object_src_tokens,
						bz::format("begin address points to a subobject at offset {}", begin - begin_global_object->address)
					});
				}

				if (end_is_one_past_the_end)
				{
					result.push_back({
						end_global_object->object_src_tokens,
						bz::format("end address is a one-past-the-end pointer with offset {}", end - end_global_object->address)
					});
				}
				else
				{
					result.push_back({
						end_global_object->object_src_tokens,
						bz::format("end address points to a subobject at offset {}", end - end_global_object->address)
					});
				}
			}
			else
			{
				result.push_back({ {}, "begin and end addresses point to different global objects" });
				result.push_back({ begin_global_object->object_src_tokens, "begin address points to this global object" });
				result.push_back({ end_global_object->object_src_tokens, "end address points to this global object" });
			}

			return result;
		}
		case memory_segment::stack:
		{
			auto const begin_stack_object = this->stack.get_stack_object(begin);
			auto const end_stack_object   = this->stack.get_stack_object(end);
			bz_assert(begin_stack_object != nullptr);
			bz_assert(end_stack_object != nullptr);

			bz::vector<error_reason_t> result;
			result.reserve(3);
			if (begin_stack_object == end_stack_object)
			{
				result.push_back({
					begin_stack_object->object_src_tokens,
					"begin and end addresses point to different subobjects in this stack object"
				});

				if (begin_is_one_past_the_end)
				{
					result.push_back({
						begin_stack_object->object_src_tokens,
						bz::format("begin address is a one-past-the-end pointer with offset {}", begin - begin_stack_object->address)
					});
				}
				else
				{
					result.push_back({
						begin_stack_object->object_src_tokens,
						bz::format("begin address points to a subobject at offset {}", begin - begin_stack_object->address)
					});
				}

				if (end_is_one_past_the_end)
				{
					result.push_back({
						end_stack_object->object_src_tokens,
						bz::format("end address is a one-past-the-end pointer with offset {}", end - end_stack_object->address)
					});
				}
				else
				{
					result.push_back({
						end_stack_object->object_src_tokens,
						bz::format("end address points to a subobject at offset {}", end - end_stack_object->address)
					});
				}
			}
			else
			{
				result.push_back({ {}, "begin and end addresses point to different stack objects" });
				result.push_back({ begin_stack_object->object_src_tokens, "begin address points to this stack object" });
				result.push_back({ end_stack_object->object_src_tokens, "end address points to this stack object" });
			}

			return result;
		}
		case memory_segment::heap:
		{
			auto const begin_allocation = this->heap.get_allocation(begin);
			auto const end_allocation   = this->heap.get_allocation(end);
			bz_assert(begin_allocation != nullptr);
			bz_assert(end_allocation != nullptr);

			bz::vector<error_reason_t> result;
			if (begin_allocation == end_allocation)
			{
				auto const begin_offset = begin - begin_allocation->object.address;
				auto const end_offset = end - begin_allocation->object.address;
				bz_assert(elem_type != begin_allocation->object.elem_type);
				auto const elem_size = begin_allocation->object.elem_size();

				auto const begin_object_index =
					begin_offset / elem_size - static_cast<size_t>(begin_is_one_past_the_end && begin_offset % elem_size == 0);
				auto const end_object_index =
					end_offset / elem_size - static_cast<size_t>(end_is_one_past_the_end && end_offset % elem_size == 0);
				if (begin_object_index == end_object_index)
				{
					result.reserve(3 + begin_allocation->alloc_info.call_stack.size());
					result.push_back({
						begin_allocation->alloc_info.src_tokens,
						"begin and end addresses point to different subobjects of the same element in this allocation"
					});

					if (begin_is_one_past_the_end)
					{
						auto const elem_offset = begin_offset % elem_size;
						auto const offset = elem_offset == 0 ? elem_size : elem_offset;
						result.push_back({
							begin_allocation->alloc_info.src_tokens,
							bz::format("begin address is a one-past-the-end pointer with offset {}", offset)
						});
					}
					else
					{
						result.push_back({
							begin_allocation->alloc_info.src_tokens,
							bz::format("begin address points to a subobject at offset {}", begin_offset % elem_size)
						});
					}

					if (end_is_one_past_the_end)
					{
						auto const elem_offset = end_offset % elem_size;
						auto const offset = elem_offset == 0 ? elem_size : elem_offset;
						result.push_back({
							end_allocation->alloc_info.src_tokens,
							bz::format("end address is a one-past-the-end pointer with offset {}", offset)
						});
					}
					else
					{
						result.push_back({
							end_allocation->alloc_info.src_tokens,
							bz::format("end address points to a subobject at offset {}", end_offset % elem_size)
						});
					}

					add_allocation_info(result, begin_allocation->alloc_info);
				}
				else
				{
					result.reserve(1 + begin_allocation->alloc_info.call_stack.size());
					result.push_back({
						begin_allocation->alloc_info.src_tokens,
						bz::format(
							"begin and end addresses point to different elements in this allocations at indices {} and {}",
							begin_object_index, end_object_index
						)
					});
					add_allocation_info(result, begin_allocation->alloc_info);
				}
			}
			else
			{
				result.reserve(3 + begin_allocation->alloc_info.call_stack.size() + end_allocation->alloc_info.call_stack.size());
				result.push_back({ {}, "begin and end addresses point to different allocations" });
				result.push_back({ begin_allocation->alloc_info.src_tokens, "begin address points to an object in this allocation" });
				add_allocation_info(result, begin_allocation->alloc_info);
				result.push_back({ end_allocation->alloc_info.src_tokens, "end address points to an object in this allocation" });
				add_allocation_info(result, end_allocation->alloc_info);
			}

			return result;
		}
		case memory_segment::meta:
			bz_unreachable;
		}
	}
	else
	{
		switch (begin_segment)
		{
		case memory_segment::global:
			return this->global_memory->get_slice_construction_error_reason(begin, end, end_is_one_past_the_end, elem_type);
		case memory_segment::stack:
			return this->stack.get_slice_construction_error_reason(begin, end, end_is_one_past_the_end, elem_type);
		case memory_segment::heap:
			return this->heap.get_slice_construction_error_reason(begin, end, end_is_one_past_the_end, elem_type);
		case memory_segment::meta:
			bz_unreachable;
		}
	}
}

bz::optional<int> memory_manager::compare_pointers(ptr_t _lhs, ptr_t _rhs) const
{
	bz_assert(_lhs != 0 && _rhs != 0);
	auto const [
		lhs,
		lhs_segment,
		lhs_is_one_past_the_end,
		lhs_is_finished_stack_frame,
		lhs_is_stack_object
	] = remove_meta(_lhs, *this);
	auto const [
		rhs,
		rhs_segment,
		rhs_is_one_past_the_end,
		rhs_is_finished_stack_frame,
		rhs_is_stack_object
	] = remove_meta(_rhs, *this);

	if (lhs_is_finished_stack_frame || rhs_is_finished_stack_frame)
	{
		return {};
	}
	else if (lhs_segment != rhs_segment)
	{
		return {};
	}
	else
	{
		switch (lhs_segment)
		{
		case memory_segment::global:
			return this->global_memory->compare_pointers(lhs, rhs);
		case memory_segment::stack:
			return this->stack.compare_pointers(lhs, rhs);
		case memory_segment::heap:
			return this->heap.compare_pointers(lhs, rhs);
		case memory_segment::meta:
			bz_unreachable;
		}
	}
}

bz::vector<error_reason_t> memory_manager::get_compare_pointers_error_reason(ptr_t _lhs, ptr_t _rhs) const
{
	bz_assert(_lhs != 0 && _rhs != 0);
	auto const [
		lhs,
		lhs_segment,
		lhs_is_one_past_the_end,
		lhs_is_finished_stack_frame,
		lhs_is_stack_object
	] = remove_meta(_lhs, *this);
	auto const [
		rhs,
		rhs_segment,
		rhs_is_one_past_the_end,
		rhs_is_finished_stack_frame,
		rhs_is_stack_object
	] = remove_meta(_rhs, *this);

	if (lhs_is_finished_stack_frame && rhs_is_finished_stack_frame)
	{
		bz::vector<error_reason_t> result;
		auto const &lhs_stack_object = this->meta_memory.get_stack_object_pointer(lhs);
		auto const &rhs_stack_object = this->meta_memory.get_stack_object_pointer(rhs);
		if (
			lhs_stack_object.object_src_tokens.begin == rhs_stack_object.object_src_tokens.begin
			&& lhs_stack_object.object_src_tokens.pivot == rhs_stack_object.object_src_tokens.pivot
			&& lhs_stack_object.object_src_tokens.end == rhs_stack_object.object_src_tokens.end
		)
		{
			result.push_back({
				lhs_stack_object.object_src_tokens,
				"lhs and rhs addresses point to an object from a finished stack frame"
			});
		}
		else
		{
			result.reserve(2);
			result.push_back({
				lhs_stack_object.object_src_tokens,
				"lhs address points to an object from a finished stack frame"
			});
			result.push_back({
				rhs_stack_object.object_src_tokens,
				"rhs address points to an object from a finished stack frame"
			});
		}
		return result;
	}
	else if (lhs_is_finished_stack_frame)
	{
		bz::vector<error_reason_t> result;
		auto const &stack_object = this->meta_memory.get_stack_object_pointer(lhs);
		result.push_back({ stack_object.object_src_tokens, "lhs address points to an object from a finished stack frame" });
		return result;
	}
	else if (rhs_is_finished_stack_frame)
	{
		bz::vector<error_reason_t> result;
		auto const &stack_object = this->meta_memory.get_stack_object_pointer(rhs);
		result.push_back({ stack_object.object_src_tokens, "rhs address points to an object from a finished stack frame" });
		return result;
	}
	else if (lhs_segment != rhs_segment)
	{
		bz::vector<error_reason_t> result;
		result.reserve(3);
		result.push_back({ {}, "lhs and rhs addresses point to different memory segments" });

		switch (lhs_segment)
		{
		case memory_segment::global:
		{
			auto const global_object = this->global_memory->get_global_object(lhs);
			bz_assert(global_object != nullptr);
			result.push_back({ global_object->object_src_tokens, "lhs address points to this global object" });
			break;
		}
		case memory_segment::stack:
		{
			auto const stack_object = this->stack.get_stack_object(lhs);
			bz_assert(stack_object != nullptr);
			result.push_back({ stack_object->object_src_tokens, "lhs address points to this stack object" });
			break;
		}
		case memory_segment::heap:
		{
			auto const allocation = this->heap.get_allocation(lhs);
			bz_assert(allocation != nullptr);
			result.reserve(1 + allocation->alloc_info.call_stack.size());
			result.push_back({ allocation->alloc_info.src_tokens, "lhs address points to an object in this allocation" });
			add_allocation_info(result, allocation->alloc_info);
			break;
		}
		case memory_segment::meta:
			bz_unreachable;
		}

		switch (rhs_segment)
		{
		case memory_segment::global:
		{
			auto const global_object = this->global_memory->get_global_object(rhs);
			bz_assert(global_object != nullptr);
			result.push_back({ global_object->object_src_tokens, "rhs address points to this global object" });
			break;
		}
		case memory_segment::stack:
		{
			auto const stack_object = this->stack.get_stack_object(rhs);
			bz_assert(stack_object != nullptr);
			result.push_back({ stack_object->object_src_tokens, "rhs address points to this stack object" });
			break;
		}
		case memory_segment::heap:
		{
			auto const allocation = this->heap.get_allocation(rhs);
			bz_assert(allocation != nullptr);
			result.reserve(1 + allocation->alloc_info.call_stack.size());
			result.push_back({ allocation->alloc_info.src_tokens, "rhs address points to an object in this allocation" });
			add_allocation_info(result, allocation->alloc_info);
			break;
		}
		case memory_segment::meta:
			bz_unreachable;
		}

		return result;
	}
	else
	{
		switch (lhs_segment)
		{
		case memory_segment::global:
			return this->global_memory->get_compare_pointers_error_reason(lhs, rhs);
		case memory_segment::stack:
			return this->stack.get_compare_pointers_error_reason(lhs, rhs);
		case memory_segment::heap:
			return this->heap.get_compare_pointers_error_reason(lhs, rhs);
		case memory_segment::meta:
			bz_unreachable;
		}
	}
}

ptr_t memory_manager::do_pointer_arithmetic(ptr_t _address, int64_t offset, type const *object_type)
{
	bz_assert(_address != 0);
	auto const [address, segment, is_one_past_the_end, is_finished_stack_frame, is_stack_object] = remove_meta(_address, *this);

	if (is_finished_stack_frame)
	{
		return 0;
	}
	else if (is_one_past_the_end && offset > 0)
	{
		return 0;
	}
	else
	{
		auto const [result, result_is_one_past_the_end] = [
			&,
			segment = segment,
			address = address,
			is_one_past_the_end = is_one_past_the_end
		]() {
			switch (segment)
			{
			case memory_segment::global:
				return this->global_memory->do_pointer_arithmetic(address, is_one_past_the_end, offset, object_type);
			case memory_segment::stack:
				return this->stack.do_pointer_arithmetic(address, is_one_past_the_end, offset, object_type);
			case memory_segment::heap:
				return this->heap.do_pointer_arithmetic(address, is_one_past_the_end, offset, object_type);
			case memory_segment::meta:
				bz_unreachable;
			}
		}();

		if (result == 0)
		{
			return 0;
		}
		else if (offset == 0)
		{
			bz_assert(result_is_one_past_the_end == is_one_past_the_end);
			return _address;
		}
		else if (result_is_one_past_the_end)
		{
			auto const one_past_the_end_result = this->meta_memory.make_one_past_the_end_address(result);
			return is_stack_object
				? this->meta_memory.make_inherited_stack_object_address(one_past_the_end_result, _address)
				: one_past_the_end_result;
		}
		else
		{
			return is_stack_object
				? this->meta_memory.make_inherited_stack_object_address(result, _address)
				: result;
		}
	}
}

bz::vector<error_reason_t> memory_manager::get_pointer_arithmetic_error_reason(ptr_t _address, int64_t offset, type const *object_type) const
{
	bz_assert(_address != 0);
	auto const [address, segment, is_one_past_the_end, is_finished_stack_frame, is_stack_object] = remove_meta(_address, *this);

	if (is_finished_stack_frame)
	{
		bz::vector<error_reason_t> result;
		auto const &stack_object = this->meta_memory.get_stack_object_pointer(address);
		result.push_back({ stack_object.object_src_tokens, "address points to an object from a finished stack frame" });
		return result;
	}
	else
	{
		switch (segment)
		{
		case memory_segment::global:
			return this->global_memory->get_pointer_arithmetic_error_reason(address, is_one_past_the_end, offset, object_type);
		case memory_segment::stack:
			return this->stack.get_pointer_arithmetic_error_reason(address, is_one_past_the_end, offset, object_type);
		case memory_segment::heap:
			return this->heap.get_pointer_arithmetic_error_reason(address, is_one_past_the_end, offset, object_type);
		case memory_segment::meta:
			bz_unreachable;
		}
	}
}

ptr_t memory_manager::do_pointer_arithmetic_unchecked(ptr_t _address, int64_t offset, type const *object_type)
{
	bz_assert(_address != 0);
	auto const [address, segment, is_one_past_the_end, is_finished_stack_frame, is_stack_object] = remove_meta(_address, *this);

	bz_assert(!is_finished_stack_frame);
	bz_assert(!(is_one_past_the_end && offset > 0));

	auto const [result, result_is_one_past_the_end] = [
		&,
		segment = segment,
		address = address,
		is_one_past_the_end = is_one_past_the_end
	]() {
		switch (segment)
		{
		case memory_segment::global:
			return this->global_memory->do_pointer_arithmetic_unchecked(address, is_one_past_the_end, offset, object_type);
		case memory_segment::stack:
			return this->stack.do_pointer_arithmetic_unchecked(address, is_one_past_the_end, offset, object_type);
		case memory_segment::heap:
			return this->heap.do_pointer_arithmetic_unchecked(address, is_one_past_the_end, offset, object_type);
		case memory_segment::meta:
			bz_unreachable;
		}
	}();

	bz_assert(result != 0);

	if (offset == 0)
	{
		bz_assert(result_is_one_past_the_end == is_one_past_the_end);
		return _address;
	}
	else if (result_is_one_past_the_end)
	{
		auto const one_past_the_end_result = this->meta_memory.make_one_past_the_end_address(result);
		return is_stack_object
			? this->meta_memory.make_inherited_stack_object_address(one_past_the_end_result, _address)
			: one_past_the_end_result;
	}
	else
	{
		return is_stack_object
			? this->meta_memory.make_inherited_stack_object_address(result, _address)
			: result;
	}
}

ptr_t memory_manager::do_gep(ptr_t _address, type const *object_type, uint64_t index)
{
	auto const [address, segment, is_one_past_the_end, is_finished_stack_frame, is_stack_object] = remove_meta(_address, *this);
	bz_assert(!is_finished_stack_frame);
	bz_assert(!is_one_past_the_end);
	switch (segment)
	{
	case memory_segment::global:
	case memory_segment::stack:
	case memory_segment::heap:
	{
		if (object_type->is_array())
		{
			auto const size = object_type->get_array_size();
			auto const result = address + object_type->get_array_element_type()->size * index;
			if (index == size)
			{
				auto const one_past_the_end_result = this->meta_memory.make_one_past_the_end_address(result);
				return is_stack_object
					? this->meta_memory.make_inherited_stack_object_address(one_past_the_end_result, _address)
					: one_past_the_end_result;
			}
			else
			{
				return is_stack_object
					? this->meta_memory.make_inherited_stack_object_address(result, _address)
					: result;
			}
		}
		else
		{
			bz_assert(object_type->is_aggregate());
			auto const offsets = object_type->get_aggregate_offsets();
			bz_assert(index < offsets.size());
			return is_stack_object
				? this->meta_memory.make_inherited_stack_object_address(address + offsets[index], _address)
				: address + offsets[index];
		}
	}
	case memory_segment::meta:
		bz_unreachable;
	}
}

bz::optional<int64_t> memory_manager::do_pointer_difference(ptr_t _lhs, ptr_t _rhs, type const *object_type) const
{
	bz_assert(_lhs != 0 && _rhs != 0);
	auto const [
		lhs,
		lhs_segment,
		lhs_is_one_past_the_end,
		lhs_is_finished_stack_frame,
		lhs_is_stack_object
	] = remove_meta(_lhs, *this);
	auto const [
		rhs,
		rhs_segment,
		rhs_is_one_past_the_end,
		rhs_is_finished_stack_frame,
		rhs_is_stack_object
	] = remove_meta(_rhs, *this);

	if (lhs_is_finished_stack_frame || rhs_is_finished_stack_frame)
	{
		return {};
	}
	else if (lhs_segment != rhs_segment)
	{
		return {};
	}
	else if (lhs == rhs && lhs_is_one_past_the_end != rhs_is_one_past_the_end)
	{
		return {};
	}
	else if (lhs < rhs && lhs_is_one_past_the_end)
	{
		return {};
	}
	else if (lhs > rhs && rhs_is_one_past_the_end)
	{
		return {};
	}
	else
	{
		switch (lhs_segment)
		{
		case memory_segment::global:
			return this->global_memory->do_pointer_difference(lhs, rhs, lhs_is_one_past_the_end, rhs_is_one_past_the_end, object_type);
		case memory_segment::stack:
			return this->stack.do_pointer_difference(lhs, rhs, lhs_is_one_past_the_end, rhs_is_one_past_the_end, object_type);
		case memory_segment::heap:
			return this->heap.do_pointer_difference(lhs, rhs, lhs_is_one_past_the_end, rhs_is_one_past_the_end, object_type);
		case memory_segment::meta:
			bz_unreachable;
		}
	}
}

bz::vector<error_reason_t> memory_manager::get_pointer_difference_error_reason(ptr_t _lhs, ptr_t _rhs, type const *object_type) const
{
	bz_assert(_lhs != 0 && _rhs != 0);
	auto const [
		lhs,
		lhs_segment,
		lhs_is_one_past_the_end,
		lhs_is_finished_stack_frame,
		lhs_is_stack_object
	] = remove_meta(_lhs, *this);
	auto const [
		rhs,
		rhs_segment,
		rhs_is_one_past_the_end,
		rhs_is_finished_stack_frame,
		rhs_is_stack_object
	] = remove_meta(_rhs, *this);

	if (lhs_is_finished_stack_frame && rhs_is_finished_stack_frame)
	{
		bz::vector<error_reason_t> result;
		auto const &lhs_stack_object = this->meta_memory.get_stack_object_pointer(lhs);
		auto const &rhs_stack_object = this->meta_memory.get_stack_object_pointer(rhs);
		if (
			lhs_stack_object.object_src_tokens.begin == rhs_stack_object.object_src_tokens.begin
			&& lhs_stack_object.object_src_tokens.pivot == rhs_stack_object.object_src_tokens.pivot
			&& lhs_stack_object.object_src_tokens.end == rhs_stack_object.object_src_tokens.end
		)
		{
			result.push_back({
				lhs_stack_object.object_src_tokens,
				"lhs and rhs addresses point to an object from a finished stack frame"
			});
		}
		else
		{
			result.reserve(2);
			result.push_back({
				lhs_stack_object.object_src_tokens,
				"lhs address points to an object from a finished stack frame"
			});
			result.push_back({
				rhs_stack_object.object_src_tokens,
				"rhs address points to an object from a finished stack frame"
			});
		}
		return result;
	}
	else if (lhs_is_finished_stack_frame)
	{
		bz::vector<error_reason_t> result;
		auto const &stack_object = this->meta_memory.get_stack_object_pointer(lhs);
		result.push_back({ stack_object.object_src_tokens, "lhs address points to an object from a finished stack frame" });
		return result;
	}
	else if (rhs_is_finished_stack_frame)
	{
		bz::vector<error_reason_t> result;
		auto const &stack_object = this->meta_memory.get_stack_object_pointer(rhs);
		result.push_back({ stack_object.object_src_tokens, "rhs address points to an object from a finished stack frame" });
		return result;
	}
	else if (lhs_segment != rhs_segment)
	{
		bz::vector<error_reason_t> result;
		result.reserve(3);
		result.push_back({ {}, "lhs and rhs addresses point to different memory segments" });

		switch (lhs_segment)
		{
		case memory_segment::global:
		{
			auto const global_object = this->global_memory->get_global_object(lhs);
			bz_assert(global_object != nullptr);
			result.push_back({ global_object->object_src_tokens, "lhs address points to this global object" });
			break;
		}
		case memory_segment::stack:
		{
			auto const stack_object = this->stack.get_stack_object(lhs);
			bz_assert(stack_object != nullptr);
			result.push_back({ stack_object->object_src_tokens, "lhs address points to this stack object" });
			break;
		}
		case memory_segment::heap:
		{
			auto const allocation = this->heap.get_allocation(lhs);
			bz_assert(allocation != nullptr);
			result.reserve(1 + allocation->alloc_info.call_stack.size());
			result.push_back({ allocation->alloc_info.src_tokens, "lhs address points to an object in this allocation" });
			add_allocation_info(result, allocation->alloc_info);
			break;
		}
		case memory_segment::meta:
			bz_unreachable;
		}

		switch (rhs_segment)
		{
		case memory_segment::global:
		{
			auto const global_object = this->global_memory->get_global_object(rhs);
			bz_assert(global_object != nullptr);
			result.push_back({ global_object->object_src_tokens, "rhs address points to this global object" });
			break;
		}
		case memory_segment::stack:
		{
			auto const stack_object = this->stack.get_stack_object(rhs);
			bz_assert(stack_object != nullptr);
			result.push_back({ stack_object->object_src_tokens, "rhs address points to this stack object" });
			break;
		}
		case memory_segment::heap:
		{
			auto const allocation = this->heap.get_allocation(rhs);
			bz_assert(allocation != nullptr);
			result.reserve(1 + allocation->alloc_info.call_stack.size());
			result.push_back({ allocation->alloc_info.src_tokens, "rhs address points to an object in this allocation" });
			add_allocation_info(result, allocation->alloc_info);
			break;
		}
		case memory_segment::meta:
			bz_unreachable;
		}

		return result;
	}
	else
	{
		switch (lhs_segment)
		{
		case memory_segment::global:
			return this->global_memory->get_pointer_difference_error_reason(lhs, rhs, lhs_is_one_past_the_end, rhs_is_one_past_the_end, object_type);
		case memory_segment::stack:
			return this->stack.get_pointer_difference_error_reason(lhs, rhs, lhs_is_one_past_the_end, rhs_is_one_past_the_end, object_type);
		case memory_segment::heap:
			return this->heap.get_pointer_difference_error_reason(lhs, rhs, lhs_is_one_past_the_end, rhs_is_one_past_the_end, object_type);
		case memory_segment::meta:
			bz_unreachable;
		}
	}
}

int64_t memory_manager::do_pointer_difference_unchecked(ptr_t lhs, ptr_t rhs, size_t stride) const
{
	if (lhs == 0 && rhs == 0)
	{
		return 0;
	}

	lhs = remove_meta(lhs, *this).address;
	rhs = remove_meta(rhs, *this).address;

	if (lhs >= rhs)
	{
		bz_assert((lhs - rhs) % stride == 0);
		return static_cast<int64_t>((lhs - rhs) / stride);
	}
	else
	{
		bz_assert((rhs - lhs) % stride == 0);
		return -static_cast<int64_t>((rhs - lhs) / stride);
	}
}

static copy_values_memory_and_properties_t get_dest_memory(
	ptr_t dest,
	memory_segment dest_segment,
	size_t count,
	type const *elem_type,
	bool is_trivial,
	memory_manager &manager
)
{
	switch (dest_segment)
	{
	case memory_segment::global:
		bz_unreachable;
	case memory_segment::heap:
	{
		auto const allocation = manager.heap.get_allocation(dest);
		bz_assert(allocation != nullptr);
		if (!is_trivial && allocation->object.elem_type != elem_type)
		{
			return {};
		}
		else if (allocation->is_freed)
		{
			return {};
		}

		return allocation->object.get_dest_memory(dest, count, elem_type, is_trivial);
	}
	case memory_segment::stack:
	{
		bz_assert(is_trivial);
		auto const object = manager.stack.get_stack_object(dest);
		bz_assert(object != nullptr);
		return { object->get_dest_memory(dest, count, elem_type).memory, {} };
	}
	case memory_segment::meta:
		bz_unreachable;
	}
}

static copy_values_memory_t get_copy_source_memory(
	ptr_t source,
	memory_segment source_segment,
	size_t count,
	type const *elem_type,
	memory_manager &manager
)
{
	switch (source_segment)
	{
	case memory_segment::global:
	{
		auto const object = manager.global_memory->get_global_object(source);
		bz_assert(object != nullptr);
		return object->get_copy_source_memory(source, count, elem_type);
	}
	case memory_segment::stack:
	{
		auto const object = manager.stack.get_stack_object(source);
		bz_assert(object != nullptr);
		return object->get_copy_source_memory(source, count, elem_type);
	}
	case memory_segment::heap:
	{
		auto const allocation = manager.heap.get_allocation(source);
		bz_assert(allocation != nullptr);
		if (allocation->is_freed)
		{
			return {};
		}

		return allocation->object.get_copy_source_memory(source, count, elem_type);
	}
	case memory_segment::meta:
		bz_unreachable;
	}
}

static copy_overlapping_values_data_t get_copy_overlapping_memory(
	ptr_t dest,
	ptr_t source,
	memory_segment segment,
	size_t count,
	type const *elem_type,
	memory_manager &manager
)
{
	switch (segment)
	{
	case memory_segment::global:
		bz_unreachable;
	case memory_segment::stack:
	{
		auto const object = manager.stack.get_stack_object(dest);
		bz_assert(object != nullptr);

		return object->get_copy_overlapping_memory(dest, source, count, elem_type);
	}
	case memory_segment::heap:
	{
		auto const allocation = manager.heap.get_allocation(dest);
		bz_assert(allocation != nullptr);
		if (allocation->is_freed)
		{
			return {};
		}

		return allocation->object.get_copy_overlapping_memory(dest, source, count, elem_type);
	}
	case memory_segment::meta:
		bz_unreachable;
	}
}

static copy_values_memory_and_properties_t get_relocate_source_memory(
	ptr_t source,
	size_t count,
	type const *elem_type,
	memory_manager &manager
)
{
	auto const allocation = manager.heap.get_allocation(source);
	bz_assert(allocation != nullptr);
	if (allocation->is_freed)
	{
		return {};
	}

	return allocation->object.get_relocate_source_memory(source, count, elem_type);
}

static void add_invalid_pointer_error_reasons(
	bz::vector<error_reason_t> &reasons,
	bz::u8string_view address_name,
	ptr_t address,
	memory_segment segment,
	bool is_one_past_the_end,
	bool is_finished_stack_frame,
	memory_manager &manager
)
{
	if (is_finished_stack_frame)
	{
		auto const &stack_object = manager.meta_memory.get_stack_object_pointer(address);
		reasons.push_back({
			stack_object.object_src_tokens,
			bz::format("{} address points to an object from a finished stack frame", address_name)
		});
	}
	else if (is_one_past_the_end)
	{
		switch (segment)
		{
		case memory_segment::global:
		{
			auto const global_object = manager.global_memory->get_global_object(address);
			bz_assert(global_object != nullptr);
			reasons.push_back({
				global_object->object_src_tokens,
				bz::format("{} address is a one-past-the-end pointer into this global object", address_name)
			});
			break;
		}
		case memory_segment::stack:
		{
			auto const stack_object = manager.stack.get_stack_object(address);
			bz_assert(stack_object != nullptr);
			reasons.push_back({
				stack_object->object_src_tokens,
				bz::format("{} address is a one-past-the-end pointer into this stack object", address_name)
			});
			break;
		}
		case memory_segment::heap:
		{
			auto const allocation = manager.heap.get_allocation(address);
			bz_assert(allocation != nullptr);
			reasons.reserve(reasons.size() + 1 + allocation->alloc_info.call_stack.size());
			reasons.push_back({
				allocation->alloc_info.src_tokens,
				bz::format("{} address is a one-past-the-end pointer into this allocation", address_name)
			});
			add_allocation_info(reasons, allocation->alloc_info);
			break;
		}
		case memory_segment::meta:
			bz_unreachable;
		}
	}
}

static void add_get_dest_memory_error_reasons(
	bz::vector<error_reason_t> &reasons,
	ptr_t dest,
	memory_segment dest_segment,
	size_t count,
	type const *elem_type,
	bool is_trivial,
	memory_manager &manager
)
{
	switch (dest_segment)
	{
	case memory_segment::global:
		bz_unreachable;
	case memory_segment::stack:
	{
		bz_assert(is_trivial);
		auto const object = manager.stack.get_stack_object(dest);
		reasons.append_move(
			object->get_get_dest_memory_error_reasons(dest, count, elem_type)
				.transform([&](auto const &message) -> error_reason_t {
					return { object->object_src_tokens, message };
				})
		);
		break;
	}
	case memory_segment::heap:
	{
		auto const allocation = manager.heap.get_allocation(dest);
		if (!is_trivial && allocation->object.elem_type != elem_type)
		{
			reasons.reserve(reasons.size() + 1 + allocation->alloc_info.call_stack.size());
			reasons.push_back({
				allocation->alloc_info.src_tokens,
				"destination address must point to uninitialized elements of this allocation if the value type is not trivially destructible"
			});
			add_allocation_info(reasons, allocation->alloc_info);
		}
		else if (allocation->is_freed)
		{
			reasons.reserve(reasons.size() + 2 + allocation->alloc_info.call_stack.size() + allocation->free_info.call_stack.size());
			reasons.push_back({
				allocation->alloc_info.src_tokens,
				"destination address points to this allocation, which was freed"
			});
			add_allocation_info(reasons, allocation->alloc_info);
			add_free_info(reasons, allocation->free_info);
		}
		else
		{
			auto messages = allocation->object.get_get_dest_memory_error_reasons(dest, count, elem_type, is_trivial);
			if (messages.not_empty())
			{
				reasons.reserve(reasons.size() + messages.size() + allocation->alloc_info.call_stack.size());
				reasons.append_move(messages.transform([&](auto &message) -> error_reason_t {
					return { allocation->alloc_info.src_tokens, std::move(message) };
				}));
				add_allocation_info(reasons, allocation->alloc_info);
			}
		}
		break;
	}
	case memory_segment::meta:
		bz_unreachable;
	}
}

static void add_get_copy_source_memory_error_reasons(
	bz::vector<error_reason_t> &reasons,
	ptr_t source,
	memory_segment source_segment,
	size_t count,
	type const *elem_type,
	memory_manager &manager
)
{
	switch (source_segment)
	{
	case memory_segment::global:
	{
		auto const object = manager.global_memory->get_global_object(source);
		reasons.append_move(
			object->get_get_copy_source_memory_error_reasons(source, count, elem_type)
				.transform([&](auto const &message) -> error_reason_t {
					return { object->object_src_tokens, message };
				})
		);
		break;
	}
	case memory_segment::stack:
	{
		auto const object = manager.stack.get_stack_object(source);
		reasons.append_move(
			object->get_get_copy_source_memory_error_reasons(source, count, elem_type)
				.transform([&](auto const &message) -> error_reason_t {
					return { object->object_src_tokens, message };
				})
		);
		break;
	}
	case memory_segment::heap:
	{
		auto const allocation = manager.heap.get_allocation(source);
		if (allocation->is_freed)
		{
			reasons.reserve(reasons.size() + 2 + allocation->alloc_info.call_stack.size() + allocation->free_info.call_stack.size());
			reasons.push_back({
				allocation->alloc_info.src_tokens,
				"source address points to this allocation, which was freed"
			});
			add_allocation_info(reasons, allocation->alloc_info);
			add_free_info(reasons, allocation->free_info);
		}
		else
		{
			auto messages = allocation->object.get_get_copy_source_memory_error_reasons(source, count, elem_type);
			if (messages.not_empty())
			{
				reasons.reserve(reasons.size() + messages.size() + allocation->alloc_info.call_stack.size());
				reasons.append_move(messages.transform([&](auto &message) -> error_reason_t {
					return { allocation->alloc_info.src_tokens, std::move(message) };
				}));
				add_allocation_info(reasons, allocation->alloc_info);
			}
		}
		break;
	}
	case memory_segment::meta:
		bz_unreachable;
	}
}

static void add_get_copy_overlapping_memory_error_reasons(
	bz::vector<error_reason_t> &reasons,
	ptr_t dest,
	ptr_t source,
	memory_segment segment,
	size_t count,
	type const *elem_type,
	memory_manager &manager
)
{
	switch (segment)
	{
	case memory_segment::global:
		bz_unreachable;
	case memory_segment::stack:
	{
		auto const object = manager.stack.get_stack_object(dest);
		bz_assert(object != nullptr);

		auto messages = object->get_get_copy_overlapping_memory_error_reasons(dest, source, count, elem_type);
		reasons.reserve(reasons.size() + messages.size());
		reasons.append_move(messages.transform([&](auto &message) -> error_reason_t {
			return { object->object_src_tokens, std::move(message) };
		}));
		break;
	}
	case memory_segment::heap:
	{
		auto const allocation = manager.heap.get_allocation(dest);
		bz_assert(allocation != nullptr);
		if (allocation->is_freed)
		{
			reasons.reserve(reasons.size() + 2 + allocation->alloc_info.call_stack.size() + allocation->free_info.call_stack.size());
			reasons.push_back({
				allocation->alloc_info.src_tokens,
				"destination and source addresses point to this allocation, which was freed"
			});
			add_allocation_info(reasons, allocation->alloc_info);
			add_free_info(reasons, allocation->free_info);
		}
		else
		{
			auto messages = allocation->object.get_get_copy_overlapping_memory_error_reasons(dest, source, count, elem_type);
			if (messages.not_empty())
			{
				reasons.reserve(reasons.size() + messages.size() + allocation->alloc_info.call_stack.size());
				reasons.append_move(messages.transform([&](auto &message) -> error_reason_t {
					return { allocation->alloc_info.src_tokens, std::move(message) };
				}));
				add_allocation_info(reasons, allocation->alloc_info);
			}
		}
		break;
	}
	case memory_segment::meta:
		bz_unreachable;
	}
}

static void add_get_relocate_source_memory_error_reasons(
	bz::vector<error_reason_t> &reasons,
	ptr_t source,
	size_t count,
	type const *elem_type,
	memory_manager &manager
)
{
	auto const allocation = manager.heap.get_allocation(source);
	if (allocation->is_freed)
	{
		reasons.reserve(reasons.size() + 2 + allocation->alloc_info.call_stack.size() + allocation->free_info.call_stack.size());
		reasons.push_back({ allocation->alloc_info.src_tokens, "source address points to this allocation, which was freed" });
		add_allocation_info(reasons, allocation->alloc_info);
		add_free_info(reasons, allocation->free_info);
	}
	else if (elem_type != allocation->object.elem_type)
	{
		reasons.push_back({ {}, "source address must point to elements of an allocation" });
		reasons.push_back({ allocation->alloc_info.src_tokens, "source address points to a subobject in this allocation" });
		add_allocation_info(reasons, allocation->alloc_info);
		add_free_info(reasons, allocation->free_info);
	}
	else
	{
		auto messages = allocation->object.get_get_relocate_source_memory_error_reasons(source, count, elem_type);
		if (messages.not_empty())
		{
			reasons.reserve(messages.size() + allocation->alloc_info.call_stack.size());
			reasons.append_move(messages.transform([&](auto &message) -> error_reason_t {
				return { allocation->alloc_info.src_tokens, std::move(message) };
			}));
			add_allocation_info(reasons, allocation->alloc_info);
		}
	}
}

bool memory_manager::copy_values(ptr_t _dest, ptr_t _source, size_t count, type const *elem_type, bool is_trivial)
{
	bz_assert(count != 0);
	bz_assert(_dest != 0);
	auto const [
		dest,
		dest_segment,
		dest_is_one_past_the_end,
		dest_is_finished_stack_frame,
		dest_is_stack_object
	] = remove_meta(_dest, *this);
	if (dest_is_finished_stack_frame || dest_is_one_past_the_end)
	{
		return false;
	}
	bz_assert(_source != 0);
	auto const [
		source,
		source_segment,
		source_is_one_past_the_end,
		source_is_finished_stack_frame,
		source_is_stack_object
	] = remove_meta(_source, *this);
	if (source_is_finished_stack_frame || source_is_one_past_the_end)
	{
		return false;
	}

	bz_assert(dest_segment != memory_segment::global);
	if (dest == source)
	{
		return false;
	}
	// if is_trivial is false, then dest must point to uninitialized memory, which is only valid for heap allocations
	else if (!is_trivial && dest_segment != memory_segment::heap)
	{
		return false;
	}
	// check if the ranges are overlapping
	else if (
		auto const memory_size = count * elem_type->size;
		dest_segment == source_segment
		&& !(
			source >= dest + memory_size
			|| source + memory_size <= dest
		)
	)
	{
		return false;
	}

	auto const [dest_memory, dest_properties] = get_dest_memory(dest, dest_segment, count, elem_type, is_trivial, *this);
	if (dest_memory.empty())
	{
		return false;
	}
	auto const [source_memory] = get_copy_source_memory(source, source_segment, count, elem_type, *this);
	if (source_memory.empty())
	{
		return false;
	}

	bz_assert(dest_memory.size() == source_memory.size());
	std::memcpy(dest_memory.data(), source_memory.data(), dest_memory.size());

	// dest_properties may be empty here
	for (auto const i : bz::iota(0, dest_properties.size()))
	{
		dest_properties[i] |= memory_properties::is_alive;
	}

	return true;
}

bz::vector<error_reason_t> memory_manager::get_copy_values_error_reason(
	ptr_t _dest,
	ptr_t _source,
	size_t count,
	type const *elem_type,
	bool is_trivial
)
{
	bz::vector<error_reason_t> reasons;

	bz_assert(count != 0);
	bz_assert(_dest != 0);
	auto const [
		dest,
		dest_segment,
		dest_is_one_past_the_end,
		dest_is_finished_stack_frame,
		dest_is_stack_object
	] = remove_meta(_dest, *this);
	add_invalid_pointer_error_reasons(
		reasons,
		"destination",
		dest,
		dest_segment,
		dest_is_one_past_the_end,
		dest_is_finished_stack_frame,
		*this
	);
	bz_assert(_source != 0);
	auto const [
		source,
		source_segment,
		source_is_one_past_the_end,
		source_is_finished_stack_frame,
		source_is_stack_object
	] = remove_meta(_source, *this);
	add_invalid_pointer_error_reasons(
		reasons,
		"source",
		source,
		source_segment,
		source_is_one_past_the_end,
		source_is_finished_stack_frame,
		*this
	);

	if (reasons.not_empty())
	{
		return reasons;
	}

	bz_assert(dest_segment != memory_segment::global);
	if (!is_trivial && dest_segment != memory_segment::heap)
	{
		reasons.reserve(2);
		reasons.push_back({
			{},
			"destination address must point to uninitialized elements of an allocation if the value type is not trivially destructible"
		});
		bz_assert(dest_segment == memory_segment::stack);
		auto const stack_object = this->stack.get_stack_object(dest);
		reasons.push_back({ stack_object->object_src_tokens, "destination address points to this stack object" });
	}
	else if (
		auto const memory_size = count * elem_type->size;
		dest == source
		|| (
			dest_segment == source_segment
			&& !(
				source >= dest + memory_size
				|| source + memory_size <= dest
			)
		)
	)
	{
		switch (dest_segment)
		{
		case memory_segment::global:
			bz_unreachable;
		case memory_segment::stack:
		{
			auto const stack_object = this->stack.get_stack_object(dest);
			bz_assert(stack_object != nullptr);
			auto const dest_offset = dest - stack_object->address;
			auto const source_offset = source - stack_object->address;
			reasons.reserve(2);
			reasons.push_back({ {}, "destination and source ranges overlap" });
			reasons.push_back({
				stack_object->object_src_tokens,
				bz::format(
					"destination and source addresses point into this stack object with offsets of {} and {}, and a total range size of {}",
					dest_offset, source_offset, memory_size
				),
			});
			break;
		}
		case memory_segment::heap:
		{
			auto const allocation = this->heap.get_allocation(dest);
			bz_assert(allocation != nullptr);
			auto const dest_offset = dest - allocation->object.address;
			auto const source_offset = source - allocation->object.address;
			reasons.reserve(2 + allocation->alloc_info.call_stack.size());
			reasons.push_back({ {}, "destination and source ranges overlap" });
			reasons.push_back({
				allocation->alloc_info.src_tokens,
				bz::format(
					"destination and source addresses point into this allocation with offsets of {} and {}, and a total range size of {}",
					dest_offset, source_offset, memory_size
				),
			});
			add_allocation_info(reasons, allocation->alloc_info);
			break;
		}
		case memory_segment::meta:
			bz_unreachable;
		}
	}
	else
	{
		add_get_dest_memory_error_reasons(reasons, dest, dest_segment, count, elem_type, is_trivial, *this);
		add_get_copy_source_memory_error_reasons(reasons, dest, dest_segment, count, elem_type, *this);
	}

	bz_assert(reasons.not_empty());
	return reasons;
}

bool memory_manager::copy_overlapping_values(ptr_t _dest, ptr_t _source, size_t count, type const *elem_type)
{
	bz_assert(count != 0);
	bz_assert(_dest != 0);
	auto const [
		dest,
		dest_segment,
		dest_is_one_past_the_end,
		dest_is_finished_stack_frame,
		dest_is_stack_object
	] = remove_meta(_dest, *this);
	if (dest_is_finished_stack_frame || dest_is_one_past_the_end)
	{
		return false;
	}
	bz_assert(_source != 0);
	auto const [
		source,
		source_segment,
		source_is_one_past_the_end,
		source_is_finished_stack_frame,
		source_is_stack_object
	] = remove_meta(_source, *this);
	if (source_is_finished_stack_frame || source_is_one_past_the_end)
	{
		return false;
	}

	bz_assert(dest_segment != memory_segment::global);
	if (dest == source)
	{
		return false;
	}
	// check if the ranges are overlapping
	else if (
		auto const memory_size = count * elem_type->size;
		dest_segment == source_segment
		&& !(
			source >= dest + memory_size
			|| source + memory_size <= dest
		)
	)
	{
		auto const [dest_memory_data, source_memory_data] = get_copy_overlapping_memory(dest, source, dest_segment, count, elem_type, *this);
		if (dest_memory_data.memory.empty())
		{
			return false;
		}

		auto const &[dest_memory, dest_properties] = dest_memory_data;
		auto const &[source_memory, source_properties] = source_memory_data;

		bz_assert(dest_memory.size() == source_memory.size());
		bz_assert(source_properties.empty());
		bz_assert(dest_properties.empty() || dest_properties.size() == dest_memory.size());

		std::memmove(dest_memory.data(), source_memory.data(), dest_memory.size());

		// dest_properties may be empty here
		for (auto const i : bz::iota(0, dest_properties.size()))
		{
			dest_properties[i] |= memory_properties::is_alive;
		}

		return true;
	}
	else
	{
		auto const [dest_memory, dest_properties] = get_dest_memory(dest, dest_segment, count, elem_type, true, *this);
		if (dest_memory.empty())
		{
			return false;
		}
		auto const [source_memory] = get_copy_source_memory(source, source_segment, count, elem_type, *this);
		if (source_memory.empty())
		{
			return false;
		}

		bz_assert(dest_memory.size() == source_memory.size());
		std::memcpy(dest_memory.data(), source_memory.data(), dest_memory.size());

		// dest_properties may be empty here
		for (auto const i : bz::iota(0, dest_properties.size()))
		{
			dest_properties[i] |= memory_properties::is_alive;
		}

		return true;
	}
}

bz::vector<error_reason_t> memory_manager::get_copy_overlapping_values_error_reason(
	ptr_t _dest,
	ptr_t _source,
	size_t count,
	type const *elem_type
)
{
	bz::vector<error_reason_t> reasons;

	bz_assert(count != 0);
	bz_assert(_dest != 0);
	auto const [
		dest,
		dest_segment,
		dest_is_one_past_the_end,
		dest_is_finished_stack_frame,
		dest_is_stack_object
	] = remove_meta(_dest, *this);
	add_invalid_pointer_error_reasons(
		reasons,
		"destination",
		dest,
		dest_segment,
		dest_is_one_past_the_end,
		dest_is_finished_stack_frame,
		*this
	);
	bz_assert(_source != 0);
	auto const [
		source,
		source_segment,
		source_is_one_past_the_end,
		source_is_finished_stack_frame,
		source_is_stack_object
	] = remove_meta(_source, *this);
	add_invalid_pointer_error_reasons(
		reasons,
		"source",
		source,
		source_segment,
		source_is_one_past_the_end,
		source_is_finished_stack_frame,
		*this
	);

	if (reasons.not_empty())
	{
		return reasons;
	}

	bz_assert(dest_segment != memory_segment::global);
	if (dest == source)
	{
		reasons.push_back({ {}, "destination and source addresses are equal" });
	}
	else if (
		auto const memory_size = count * elem_type->size;
		dest_segment == source_segment
		&& !(
			source >= dest + memory_size
			|| source + memory_size <= dest
		)
	)
	{
		add_get_copy_overlapping_memory_error_reasons(reasons, dest, source, dest_segment, count, elem_type, *this);
	}
	else
	{
		add_get_dest_memory_error_reasons(reasons, dest, dest_segment, count, elem_type, true, *this);
		add_get_copy_source_memory_error_reasons(reasons, dest, dest_segment, count, elem_type, *this);
	}

	bz_assert(reasons.not_empty());
	return reasons;
}

bool memory_manager::relocate_values(ptr_t _dest, ptr_t _source, size_t count, type const *elem_type, bool is_trivial)
{
	bz_assert(count != 0);
	bz_assert(_dest != 0);
	auto const [
		dest,
		dest_segment,
		dest_is_one_past_the_end,
		dest_is_finished_stack_frame,
		dest_is_stack_object
	] = remove_meta(_dest, *this);
	if (dest_is_finished_stack_frame || dest_is_one_past_the_end)
	{
		return false;
	}
	bz_assert(_source != 0);
	auto const [
		source,
		source_segment,
		source_is_one_past_the_end,
		source_is_finished_stack_frame,
		source_is_stack_object
	] = remove_meta(_source, *this);
	if (source_is_finished_stack_frame || source_is_one_past_the_end)
	{
		return false;
	}

	bz_assert(dest_segment != memory_segment::global);
	if (dest == source)
	{
		return false;
	}
	// if is_trivial is false, then dest must point to uninitialized memory, which is only valid for heap allocations
	else if (!is_trivial && dest_segment != memory_segment::heap)
	{
		return false;
	}
	// since source is destructed by this operation it can only be on the heap
	else if (source_segment != memory_segment::heap)
	{
		return false;
	}
	// check if the ranges are overlapping
	else if (
		auto const memory_size = count * elem_type->size;
		dest_segment == source_segment
		&& !(
			source >= dest + memory_size
			|| source + memory_size <= dest
		)
	)
	{
		auto const allocation = this->heap.get_allocation(dest);
		if (allocation->is_freed || elem_type != allocation->object.elem_type)
		{
			return false;
		}

		auto const [dest_memory_data, source_memory_data] = allocation->object.get_relocate_overlapping_memory(
			dest,
			source,
			count,
			is_trivial
		);
		if (dest_memory_data.memory.empty())
		{
			return false;
		}

		auto const &[dest_memory, dest_properties] = dest_memory_data;
		auto const &[source_memory, source_properties] = source_memory_data;

		bz_assert(dest_memory.size() == source_memory.size());
		bz_assert(dest_properties.size() == source_properties.size());
		bz_assert(dest_memory.size() == dest_properties.size());

		std::memmove(dest_memory.data(), source_memory.data(), dest_memory.size());

		if (source < dest)
		{
			auto const non_overlap_size = dest - source;
			auto const overlap_size = dest_properties.size() - non_overlap_size;

			for (auto &source_property : source_properties.slice(0, non_overlap_size))
			{
				source_property &= ~memory_properties::is_alive;
			}
			for (auto &dest_property : dest_properties.slice(overlap_size))
			{
				dest_property |= memory_properties::is_alive;
			}
		}
		else
		{
			auto const non_overlap_size = source - dest;
			auto const overlap_size = dest_properties.size() - non_overlap_size;

			for (auto &dest_property : dest_properties.slice(0, non_overlap_size))
			{
				dest_property |= memory_properties::is_alive;
			}
			for (auto &source_property : source_properties.slice(overlap_size))
			{
				source_property &= ~memory_properties::is_alive;
			}
		}

		return true;
	}
	else
	{
		auto const [dest_memory, dest_properties] = get_dest_memory(dest, dest_segment, count, elem_type, is_trivial, *this);
		if (dest_memory.empty())
		{
			return false;
		}
		auto const [source_memory, source_properties] = get_relocate_source_memory(source, count, elem_type, *this);
		if (source_memory.empty())
		{
			return false;
		}

		bz_assert(dest_memory.size() == source_memory.size());
		std::memcpy(dest_memory.data(), source_memory.data(), dest_memory.size());

		bz_assert(source_properties.size() == source_memory.size());
		if (dest_properties.not_empty())
		{
			bz_assert(dest_properties.size() == source_properties.size());
			for (auto const i : bz::iota(0, dest_properties.size()))
			{
				dest_properties[i] |= memory_properties::is_alive;
				source_properties[i] &= ~memory_properties::is_alive;
			}
		}
		else
		{
			for (auto const i : bz::iota(0, source_properties.size()))
			{
				source_properties[i] &= ~memory_properties::is_alive;
			}
		}

		return true;
	}
}

bz::vector<error_reason_t> memory_manager::get_relocate_values_error_reason(
	ptr_t _dest,
	ptr_t _source,
	size_t count,
	type const *elem_type,
	bool is_trivial
)
{
	bz::vector<error_reason_t> reasons;

	bz_assert(count != 0);
	bz_assert(_dest != 0);
	auto const [
		dest,
		dest_segment,
		dest_is_one_past_the_end,
		dest_is_finished_stack_frame,
		dest_is_stack_object
	] = remove_meta(_dest, *this);
	add_invalid_pointer_error_reasons(
		reasons,
		"destination",
		dest,
		dest_segment,
		dest_is_one_past_the_end,
		dest_is_finished_stack_frame,
		*this
	);
	bz_assert(_source != 0);
	auto const [
		source,
		source_segment,
		source_is_one_past_the_end,
		source_is_finished_stack_frame,
		source_is_stack_object
	] = remove_meta(_source, *this);
	add_invalid_pointer_error_reasons(
		reasons,
		"source",
		source,
		source_segment,
		source_is_one_past_the_end,
		source_is_finished_stack_frame,
		*this
	);

	if (reasons.not_empty())
	{
		return reasons;
	}

	if (source_segment != memory_segment::heap)
	{
		reasons.reserve(2);
		reasons.push_back({ {}, "source address must point to elements of an allocation" });
		bz_assert(source_segment == memory_segment::stack);
		auto const stack_object = this->stack.get_stack_object(source);
		reasons.push_back({ stack_object->object_src_tokens, "source address points to this stack object" });
	}
	else if (!is_trivial && dest_segment != memory_segment::heap)
	{
		reasons.reserve(2);
		reasons.push_back({
			{},
			"destination address must point to uninitialized elements of an allocation if the value type is not trivially destructible"
		});
		bz_assert(dest_segment == memory_segment::stack);
		auto const stack_object = this->stack.get_stack_object(dest);
		reasons.push_back({ stack_object->object_src_tokens, "destination address points to this stack object" });
	}
	else if (dest == source)
	{
		reasons.push_back({ {}, "destination and source addresses are equal" });
	}
	else if (
		auto const memory_size = count * elem_type->size;
		dest_segment == source_segment
		&& !(
			source >= dest + memory_size
			|| source + memory_size <= dest
		)
	)
	{
		auto const allocation = this->heap.get_allocation(dest);
		if (allocation->is_freed)
		{
			reasons.reserve(2 + allocation->alloc_info.call_stack.size() + allocation->free_info.call_stack.size());
			reasons.push_back({
				allocation->alloc_info.src_tokens,
				"destination and source addresses point to this allocation, which was freed"
			});
			add_allocation_info(reasons, allocation->alloc_info);
			add_free_info(reasons, allocation->free_info);
		}
		else if (elem_type != allocation->object.elem_type)
		{
			reasons.reserve(2 + allocation->alloc_info.call_stack.size());
			reasons.push_back({ {}, "source address must point to elements of an allocation" });
			reasons.push_back({ allocation->alloc_info.src_tokens, "source address points to a subobject in this allocation" });
			add_allocation_info(reasons, allocation->alloc_info);
			add_free_info(reasons, allocation->free_info);
		}
		else
		{
			auto messages = allocation->object.get_get_relocate_overlapping_memory_error_reasons(dest, source, count, is_trivial);
			if (messages.not_empty())
			{
				reasons.reserve(messages.size() + allocation->alloc_info.call_stack.size());
				reasons.append_move(messages.transform([&](auto &message) -> error_reason_t {
					return { allocation->alloc_info.src_tokens, std::move(message) };
				}));
				add_allocation_info(reasons, allocation->alloc_info);
			}
		}
	}
	else
	{
		add_get_dest_memory_error_reasons(reasons, dest, dest_segment, count, elem_type, is_trivial, *this);
		add_get_relocate_source_memory_error_reasons(reasons, source, count, elem_type, *this);
	}

	bz_assert(reasons.not_empty());
	return reasons;
}

bool memory_manager::set_values_i8_native(ptr_t _dest, uint8_t value, size_t count, type const *elem_type)
{
	bz_assert(count != 0);
	bz_assert(_dest != 0);
	auto const [
		dest,
		dest_segment,
		dest_is_one_past_the_end,
		dest_is_finished_stack_frame,
		dest_is_stack_object
	] = remove_meta(_dest, *this);
	if (dest_is_finished_stack_frame || dest_is_one_past_the_end)
	{
		return false;
	}

	bz_assert(elem_type->size == 1);
	auto const [dest_memory, dest_properties] = get_dest_memory(dest, dest_segment, count, elem_type, true, *this);
	if (dest_memory.empty())
	{
		return false;
	}

	std::memset(dest_memory.data(), value, dest_memory.size());

	for (auto &property : dest_properties)
	{
		property |= memory_properties::is_alive;
	}

	return true;
}

bool memory_manager::set_values_i16_native(ptr_t _dest, uint16_t value, size_t count, type const *elem_type)
{
	bz_assert(count != 0);
	bz_assert(_dest != 0);
	auto const [
		dest,
		dest_segment,
		dest_is_one_past_the_end,
		dest_is_finished_stack_frame,
		dest_is_stack_object
	] = remove_meta(_dest, *this);
	if (dest_is_finished_stack_frame || dest_is_one_past_the_end)
	{
		return false;
	}

	bz_assert(elem_type->size == sizeof value);
	auto const [dest_memory, dest_properties] = get_dest_memory(dest, dest_segment, count, elem_type, true, *this);
	if (dest_memory.empty())
	{
		return false;
	}

	if (value == 0)
	{
		std::memset(dest_memory.data(), 0, dest_memory.size());
	}
	else
	{
		for (auto const i : bz::iota(0, count))
		{
			std::memcpy(dest_memory.data() + sizeof value * i, &value, sizeof value);
		}
	}

	for (auto &property : dest_properties)
	{
		property |= memory_properties::is_alive;
	}

	return true;
}

bool memory_manager::set_values_i32_native(ptr_t _dest, uint32_t value, size_t count, type const *elem_type)
{
	bz_assert(count != 0);
	bz_assert(_dest != 0);
	auto const [
		dest,
		dest_segment,
		dest_is_one_past_the_end,
		dest_is_finished_stack_frame,
		dest_is_stack_object
	] = remove_meta(_dest, *this);
	if (dest_is_finished_stack_frame || dest_is_one_past_the_end)
	{
		return false;
	}

	bz_assert(elem_type->size == sizeof value);
	auto const [dest_memory, dest_properties] = get_dest_memory(dest, dest_segment, count, elem_type, true, *this);
	if (dest_memory.empty())
	{
		return false;
	}

	if (value == 0)
	{
		std::memset(dest_memory.data(), 0, dest_memory.size());
	}
	else
	{
		for (auto const i : bz::iota(0, count))
		{
			std::memcpy(dest_memory.data() + sizeof value * i, &value, sizeof value);
		}
	}

	for (auto &property : dest_properties)
	{
		property |= memory_properties::is_alive;
	}

	return true;
}

bool memory_manager::set_values_i64_native(ptr_t _dest, uint64_t value, size_t count, type const *elem_type)
{
	bz_assert(count != 0);
	bz_assert(_dest != 0);
	auto const [
		dest,
		dest_segment,
		dest_is_one_past_the_end,
		dest_is_finished_stack_frame,
		dest_is_stack_object
	] = remove_meta(_dest, *this);
	if (dest_is_finished_stack_frame || dest_is_one_past_the_end)
	{
		return false;
	}

	bz_assert(elem_type->size == sizeof value);
	auto const [dest_memory, dest_properties] = get_dest_memory(dest, dest_segment, count, elem_type, true, *this);
	if (dest_memory.empty())
	{
		return false;
	}

	if (value == 0)
	{
		std::memset(dest_memory.data(), 0, dest_memory.size());
	}
	else
	{
		for (auto const i : bz::iota(0, count))
		{
			std::memcpy(dest_memory.data() + sizeof value * i, &value, sizeof value);
		}
	}

	for (auto &property : dest_properties)
	{
		property |= memory_properties::is_alive;
	}

	return true;
}

bool memory_manager::set_values_ref(ptr_t _dest, uint8_t const *value_mem, size_t count, type const *elem_type)
{
	bz_assert(count != 0);
	bz_assert(_dest != 0);
	auto const [
		dest,
		dest_segment,
		dest_is_one_past_the_end,
		dest_is_finished_stack_frame,
		dest_is_stack_object
	] = remove_meta(_dest, *this);
	if (dest_is_finished_stack_frame || dest_is_one_past_the_end)
	{
		return false;
	}

	auto const [dest_memory, dest_properties] = get_dest_memory(dest, dest_segment, count, elem_type, true, *this);
	if (dest_memory.empty())
	{
		return false;
	}

	auto const elem_size = elem_type->size;
	for (auto const i : bz::iota(0, count))
	{
		std::memcpy(dest_memory.data() + elem_size * i, value_mem, elem_size);
	}

	for (auto &property : dest_properties)
	{
		property |= memory_properties::is_alive;
	}

	return true;
}

bz::vector<error_reason_t> memory_manager::get_set_values_error_reason(ptr_t _dest, size_t count, type const *elem_type)
{
	bz::vector<error_reason_t> reasons;

	bz_assert(count != 0);
	bz_assert(_dest != 0);
	auto const [
		dest,
		dest_segment,
		dest_is_one_past_the_end,
		dest_is_finished_stack_frame,
		dest_is_stack_object
	] = remove_meta(_dest, *this);
	add_invalid_pointer_error_reasons(
		reasons,
		"destination",
		dest,
		dest_segment,
		dest_is_one_past_the_end,
		dest_is_finished_stack_frame,
		*this
	);

	if (reasons.not_empty())
	{
		return reasons;
	}

	add_get_dest_memory_error_reasons(reasons, dest, dest_segment, count, elem_type, true, *this);

	bz_assert(reasons.not_empty());
	return reasons;
}

void memory_manager::start_lifetime(ptr_t _address, size_t size)
{
	auto const [address, segment, is_one_past_the_end, is_finished_stack_frame, is_stack_object] = remove_meta(_address, *this);
	bz_assert(!is_finished_stack_frame);
	bz_assert(!is_one_past_the_end);

	switch (segment)
	{
	case memory_segment::stack:
	{
		auto const object = this->stack.get_stack_object(address);
		bz_assert(object != nullptr);
		object->start_lifetime(address, address + size);
		break;
	}
	case memory_segment::heap:
	{
		auto const allocation = this->heap.get_allocation(address);
		bz_assert(allocation != nullptr);
		bz_assert(!allocation->is_freed);
		allocation->object.start_lifetime(address, address + size);
		break;
	}
	default:
		bz_unreachable;
	}
}

void memory_manager::end_lifetime(ptr_t _address, size_t size)
{
	auto const [address, segment, is_one_past_the_end, is_finished_stack_frame, is_stack_object] = remove_meta(_address, *this);
	bz_assert(!is_finished_stack_frame);
	bz_assert(!is_one_past_the_end);

	switch (segment)
	{
	case memory_segment::stack:
	{
		auto const object = this->stack.get_stack_object(address);
		bz_assert(object != nullptr);
		object->end_lifetime(address, address + size);
		break;
	}
	case memory_segment::heap:
	{
		auto const allocation = this->heap.get_allocation(address);
		bz_assert(allocation != nullptr);
		bz_assert(!allocation->is_freed);
		allocation->object.end_lifetime(address, address + size);
		break;
	}
	default:
		bz_unreachable;
	}
}

bool memory_manager::is_global(ptr_t address) const
{
	return address != 0 && remove_meta(address, *this).segment == memory_segment::global;
}

uint8_t *memory_manager::get_memory(ptr_t _address)
{
	auto const [address, segment, is_one_past_the_end, is_finished_stack_frame, is_stack_object] = remove_meta(_address, *this);
	bz_assert(!is_finished_stack_frame);
	switch (segment)
	{
	case memory_segment::global:
		return this->global_memory->get_memory(address);
	case memory_segment::stack:
		return this->stack.get_memory(address);
	case memory_segment::heap:
		return this->heap.get_memory(address);
	case memory_segment::meta:
		bz_unreachable;
	}
}

uint8_t const *memory_manager::get_memory(ptr_t _address) const
{
	auto const [address, segment, is_one_past_the_end, is_finished_stack_frame, is_stack_object] = remove_meta(_address, *this);
	bz_assert(!is_finished_stack_frame);
	switch (segment)
	{
	case memory_segment::global:
		return this->global_memory->get_memory(address);
	case memory_segment::stack:
		return this->stack.get_memory(address);
	case memory_segment::heap:
		return this->heap.get_memory(address);
	case memory_segment::meta:
		bz_unreachable;
	}
}

} // namespace comptime::memory
