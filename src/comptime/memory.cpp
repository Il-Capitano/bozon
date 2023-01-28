#include "memory.h"
#include "ast/statement.h"
#include "resolve/consteval.h"

namespace comptime::memory
{

static constexpr uint8_t max_object_align = 8;
static constexpr uint8_t heap_object_align = 16;

static bool contained_in_object(type const *object_type, size_t offset, type const *subobject_type)
{
	static_assert(type::variant_count == 4);
	if (offset == 0 && subobject_type == object_type)
	{
		return true;
	}
	else if (object_type->is_builtin() || object_type->is_pointer())
	{
		// return offset == 0 && object_type == subobject_type;
		return false;
	}
	else if (object_type->is_aggregate())
	{
		auto const members = object_type->get_aggregate_types();
		auto const offsets = object_type->get_aggregate_offsets();
		// get the index of the largest offset that is <= to offset
		// upper_bound returns the first offset that is strictly greater than offset,
		// so we need the previous element
		auto const member_index = std::upper_bound(offsets.begin() + 1, offsets.end(), offset) - offsets.begin() - 1;
		return contained_in_object(members[member_index], offset - offsets[member_index], subobject_type);
	}
	else if (object_type->is_array())
	{
		auto const array_elem_type = object_type->get_array_element_type();
		auto const offset_in_elem = offset % array_elem_type->size;
		bz_assert(offset / array_elem_type->size < object_type->get_array_size());
		return contained_in_object(array_elem_type, offset_in_elem, subobject_type);
	}
	else
	{
		return false;
	}
}

static bool slice_contained_in_object(type const *object_type, size_t offset, type const *elem_type, size_t total_size, bool end_is_one_past_the_end)
{
	bz_assert(total_size != 0);
	static_assert(type::variant_count == 4);
	if (offset + total_size > object_type->size)
	{
		return false;
	}
	else if (object_type == elem_type)
	{
		bz_assert(offset == 0);
		return end_is_one_past_the_end && total_size == object_type->size;
	}
	else if (object_type->is_aggregate())
	{
		auto const members = object_type->get_aggregate_types();
		auto const offsets = object_type->get_aggregate_offsets();
		// get the index of the largest offset that is <= to offset
		// upper_bound returns the first offset that is strictly greater than offset,
		// so we need the previous element
		auto const member_index = std::upper_bound(offsets.begin() + 1, offsets.end(), offset) - offsets.begin() - 1;
		return slice_contained_in_object(members[member_index], offset - offsets[member_index], elem_type, total_size, end_is_one_past_the_end);
	}
	else if (object_type->is_array())
	{
		auto const array_elem_type = object_type->get_array_element_type();
		auto const offset_in_elem = offset % array_elem_type->size;
		if (array_elem_type == elem_type)
		{
			// the slice must be able to fit into this array because of the check `offset + total_size > object_type->size`
			// at the beginning
			return offset_in_elem == 0 && (end_is_one_past_the_end || offset + total_size < object_type->size);
		}
		else
		{
			bz_assert(offset / array_elem_type->size < object_type->get_array_size());
			return slice_contained_in_object(array_elem_type, offset_in_elem, elem_type, total_size, end_is_one_past_the_end);
		}
	}
	else
	{
		return false;
	}
}

enum class pointer_arithmetic_check_result
{
	fail,
	good,
	one_past_the_end,
};

static pointer_arithmetic_check_result check_pointer_arithmetic(
	type const *object_type,
	size_t offset,
	size_t result_offset,
	bool is_one_past_the_end,
	type const *pointer_type
)
{
	static_assert(type::variant_count == 4);
	if (result_offset > object_type->size)
	{
		return pointer_arithmetic_check_result::fail;
	}
	else if (object_type == pointer_type)
	{
		if (result_offset == 0)
		{
			return pointer_arithmetic_check_result::good;
		}
		else if (result_offset == object_type->size)
		{
			return pointer_arithmetic_check_result::one_past_the_end;
		}
		else
		{
			return pointer_arithmetic_check_result::fail;
		}
	}
	else if (object_type->is_builtin() || object_type->is_pointer())
	{
		bz_unreachable;
	}
	else if (object_type->is_aggregate())
	{
		auto const members = object_type->get_aggregate_types();
		auto const offsets = object_type->get_aggregate_offsets();
		// get the index of the largest offset that is <= to offset
		// upper_bound returns the first offset that is strictly greater than offset,
		// so we need the previous element
		auto const it = is_one_past_the_end
			? std::lower_bound(offsets.begin() + 1, offsets.end(), offset)
			: std::upper_bound(offsets.begin() + 1, offsets.end(), offset);
		auto const member_index = (it - 1) - offsets.begin();
		if (result_offset < offsets[member_index])
		{
			return pointer_arithmetic_check_result::fail;
		}
		else
		{
			return check_pointer_arithmetic(
				members[member_index],
				offset - offsets[member_index],
				result_offset - offsets[member_index],
				is_one_past_the_end,
				pointer_type
			);
		}
	}
	else if (object_type->is_array())
	{
		auto const array_elem_type = object_type->get_array_element_type();
		if (array_elem_type == pointer_type)
		{
			// the result_offset must be valid, because of the check `result_offset > object_type->size` at the beginning
			if (result_offset == object_type->size)
			{
				return pointer_arithmetic_check_result::one_past_the_end;
			}
			else
			{
				return pointer_arithmetic_check_result::good;
			}
		}
		else
		{
			auto const offset_in_elem = offset % array_elem_type->size;
			auto const real_offset_in_elem = is_one_past_the_end && offset_in_elem == array_elem_type->size
				? array_elem_type->size
				: offset_in_elem;
			auto const elem_offset = offset - real_offset_in_elem;
			if (result_offset < elem_offset)
			{
				return pointer_arithmetic_check_result::fail;
			}
			else
			{
				return check_pointer_arithmetic(
					array_elem_type,
					offset - elem_offset,
					result_offset - elem_offset,
					is_one_past_the_end,
					pointer_type
				);
			}
		}
	}
	else
	{
		// we should never get to here...
		bz_assert(false); // there's no point in using bz_unreachable here I think, so this branch is only checked in debug mode
		return pointer_arithmetic_check_result::fail;
	}
}

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

global_object::global_object(lex::src_tokens const &_object_src_tokens, ptr_t _address, type const *_object_type, bz::fixed_vector<uint8_t> data)
	: address(_address),
	  object_type(_object_type),
	  memory(std::move(data)),
	  object_src_tokens(_object_src_tokens)
{}

size_t global_object::object_size(void) const
{
	return this->memory.size();
}

uint8_t *global_object::get_memory(ptr_t address)
{
	bz_assert(address >= this->address && address <= this->address + this->object_size());
	bz_assert(this->memory.not_empty());
	return this->memory.data() + (address - this->address);
}

uint8_t const *global_object::get_memory(ptr_t address) const
{
	bz_assert(address >= this->address && address <= this->address + this->object_size());
	bz_assert(this->memory.not_empty());
	return this->memory.data() + (address - this->address);
}

bool global_object::check_dereference(ptr_t address, type const *subobject_type) const
{
	if (address < this->address || address >= this->address + this->object_size() || this->memory.empty())
	{
		return false;
	}

	auto const offset = address - this->address;
	return contained_in_object(this->object_type, offset, subobject_type);
}

bz::u8string global_object::get_dereference_error_reason(ptr_t, type const *) const
{
	// this shouldn't ever happen, because the only kind of invalid memory access
	// into global objects is a one-past-the-end pointer dereference, which is handled
	// much earlier as a meta pointer
	bz_unreachable;
	return {};
}

bool global_object::check_slice_construction(ptr_t begin, ptr_t end, bool end_is_one_past_the_end, type const *elem_type) const
{
	if (begin == end)
	{
		return true;
	}

	auto const total_size = end - begin;
	bz_assert(total_size % elem_type->size == 0);
	auto const offset = begin - this->address;

	return slice_contained_in_object(this->object_type, offset, elem_type, total_size, end_is_one_past_the_end);
}

bz::vector<bz::u8string> global_object::get_slice_construction_error_reason(
	ptr_t begin,
	ptr_t end,
	bool end_is_one_past_the_end,
	type const *
) const
{
	auto const begin_offset = begin - this->address;
	auto const end_offset = end - this->address;

	bz::vector<bz::u8string> result;
	result.reserve(3);
	result.push_back("begin and end addresses point to different subobjects in this global object");
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

pointer_arithmetic_result_t global_object::do_pointer_arithmetic(
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

pointer_arithmetic_result_t global_object::do_pointer_arithmetic_unchecked(
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

bz::optional<int64_t> global_object::do_pointer_difference(
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
			return static_cast<int64_t>((lhs - rhs) / object_type->size);
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
			return -static_cast<int64_t>((rhs - lhs) / object_type->size);
		}
		else
		{
			return {};
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
		for (auto const i : bz::iota(0, types.size() - 1))
		{
			auto const elem_type = types[i];
			fill_padding_single(data.slice(0, elem_type->size), elem_type);
			auto const offset = offsets[i];
			auto const next_offset = offsets[i + 1];
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

bool memory_properties::is_all(size_t begin, size_t end, uint8_t bits) const
{
	for (auto const value : this->data.slice(begin, end))
	{
		if ((value & bits) == 0)
		{
			return false;
		}
	}
	return true;
}

bool memory_properties::is_none(size_t begin, size_t end, uint8_t bits) const
{
	for (auto const value : this->data.slice(begin, end))
	{
		if ((value & bits) != 0)
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
	bz_assert(this->properties.is_none(begin - this->address, end - this->address, memory_properties::is_alive));
	this->properties.set_range(begin - this->address, end - this->address, memory_properties::is_alive);
}

void stack_object::end_lifetime(ptr_t begin, ptr_t end)
{
	bz_assert(this->is_alive(begin, end));
	this->properties.erase_range(begin - this->address, end - this->address, memory_properties::is_alive);
}

bool stack_object::is_alive(ptr_t begin, ptr_t end) const
{
	return this->properties.is_all(
		begin - this->address,
		end - this->address,
		memory_properties::is_alive | memory_properties::is_padding
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
	if (!this->is_alive(address, address + subobject_type->size))
	{
		return false;
	}
	if (address < this->address || address >= this->address + this->object_size() || this->memory.empty())
	{
		return false;
	}

	auto const offset = address - this->address;
	return contained_in_object(this->object_type, offset, subobject_type);
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

	if (!this->is_alive(begin, end))
	{
		return false;
	}
	if (
		this->memory.empty()
		|| begin < this->address || begin >= this->address + this->object_size()
		|| end <= this->address || end > this->address + this->object_size()
	)
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
			return static_cast<int64_t>((lhs - rhs) / object_type->size);
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
			return -static_cast<int64_t>((rhs - lhs) / object_type->size);
		}
		else
		{
			return {};
		}
	}
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
	bz_assert(this->properties.is_none(begin - this->address, end - this->address, memory_properties::is_alive));
	this->properties.set_range(begin - this->address, end - this->address, memory_properties::is_alive);
}

void heap_object::end_lifetime(ptr_t begin, ptr_t end)
{
	bz_assert(this->is_alive(begin, end));
	this->properties.erase_range(begin - this->address, end - this->address, memory_properties::is_alive);
}

bool heap_object::is_alive(ptr_t begin, ptr_t end) const
{
	return this->properties.is_all(
		begin - this->address,
		end - this->address,
		memory_properties::is_alive | memory_properties::is_padding
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
	if (!this->is_alive(address, address + subobject_type->size))
	{
		return false;
	}
	if (address < this->address || address >= this->address + this->object_size() || this->memory.empty())
	{
		return false;
	}

	auto const offset = address - this->address;
	return contained_in_object(this->elem_type, offset % this->elem_size(), subobject_type);
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
		auto const begin = address - this->address;
		return this->properties.is_none(begin, begin + this->elem_size(), memory_properties::is_alive);
	}
}

bz::u8string heap_object::get_inplace_construct_error_reason(ptr_t, type const *object_type) const
{
	if (object_type != this->elem_type)
	{
		return "address points to a subobject of one of the elemnts of this allocation";
	}
	else
	{
		return "address points to an element in this allocation, that has already been constructed";
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
	auto const end_object_index = end_offset / this->elem_size();

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
	else if (lhs < rhs)
	{
		auto const slice_check = this->check_slice_construction(lhs, rhs, rhs_is_one_past_the_end, object_type);
		if (slice_check)
		{
			return static_cast<int64_t>((lhs - rhs) / object_type->size);
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
			return -static_cast<int64_t>((rhs - lhs) / object_type->size);
		}
		else
		{
			return {};
		}
	}
}

global_memory_manager::global_memory_manager(ptr_t global_memory_begin)
	: head(global_memory_begin),
	  objects()
{}

uint32_t global_memory_manager::add_object(lex::src_tokens const &object_src_tokens, type const *object_type, bz::fixed_vector<uint8_t> data)
{
	auto const result = static_cast<uint32_t>(this->objects.size());
	this->objects.emplace_back(object_src_tokens, this->head, object_type, std::move(data));
	this->head += object_type->size;
	this->head += (max_object_align - this->head % max_object_align);
	return result;
}

void global_memory_manager::add_one_past_the_end_pointer_info(one_past_the_end_pointer_info_t info)
{
	this->one_past_the_end_pointer_infos.push_back(info);
}

global_object *global_memory_manager::get_global_object(ptr_t address)
{
	if (
		this->objects.empty()
		|| address < this->objects[0].address
		|| address > this->objects.back().address + this->objects.back().object_size()
	)
	{
		return nullptr;
	}

	// find the last element that has an address that is less than or equal to address.
	// we do this by finding the first element, whose address is greater than address
	// and taking the element before that
	auto const it = std::upper_bound(
		this->objects.begin(), this->objects.end(),
		address,
		[](ptr_t address, auto const &object) {
			return address < object.address;
		}
	);
	return &*(it - 1);
}

global_object const *global_memory_manager::get_global_object(ptr_t address) const
{
	if (
		this->objects.empty()
		|| address < this->objects[0].address
		|| address > this->objects.back().address + this->objects.back().object_size()
	)
	{
		return nullptr;
	}

	// find the last element that has an address that is less than or equal to address.
	// we do this by finding the first element, whose address is greater than address
	// and taking the element before that
	auto const it = std::upper_bound(
		this->objects.begin(), this->objects.end(),
		address,
		[](ptr_t address, auto const &object) {
			return address < object.address;
		}
	);
	return &*(it - 1);
}

bool global_memory_manager::check_dereference(ptr_t address, type const *object_type) const
{
	auto const object = this->get_global_object(address);
	return object != nullptr && object->check_dereference(address, object_type);
}

bz::vector<error_reason_t> global_memory_manager::get_dereference_error_reason(ptr_t address, type const *object_type) const
{
	auto const object = this->get_global_object(address);
	bz_assert(object != nullptr);
	bz::vector<error_reason_t> result;
	result.push_back({ object->object_src_tokens, object->get_dereference_error_reason(address, object_type) });
	return result;
}

bool global_memory_manager::check_slice_construction(ptr_t begin, ptr_t end, bool end_is_one_past_the_end, type const *elem_type) const
{
	bz_assert(begin <= end);
	auto const object = this->get_global_object(begin);
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

bz::vector<error_reason_t> global_memory_manager::get_slice_construction_error_reason(
	ptr_t begin,
	ptr_t end,
	bool end_is_one_past_the_end,
	type const *elem_type
) const
{
	bz_assert(begin <= end);
	auto const begin_global_object = this->get_global_object(begin);
	auto const end_global_object   = this->get_global_object(end);
	bz_assert(begin_global_object != nullptr);
	bz_assert(end_global_object != nullptr);

	bz::vector<error_reason_t> result;
	if (begin_global_object == end_global_object)
	{
		auto messages = begin_global_object->get_slice_construction_error_reason(begin, end, end_is_one_past_the_end, elem_type);
		result.reserve(messages.size());
		result.append(messages.transform([begin_global_object](auto const &message) {
			return error_reason_t{ begin_global_object->object_src_tokens, message };
		}));
	}
	else
	{
		result.reserve(3);
		result.push_back({ {}, "begin and end addresses point to different global objects" });
		result.push_back({ begin_global_object->object_src_tokens, "begin address points to this global object" });
		result.push_back({ end_global_object->object_src_tokens, "end address points to this global object" });
	}

	return result;
}

bz::optional<int> global_memory_manager::compare_pointers(ptr_t lhs, ptr_t rhs) const
{
	auto const lhs_global_object = this->get_global_object(lhs);
	if (lhs_global_object == nullptr)
	{
		return {};
	}
	else if (rhs < lhs_global_object->address || rhs > lhs_global_object->address + lhs_global_object->object_size())
	{
		return {};
	}
	else
	{
		return lhs == rhs ? 0 : (lhs < rhs ? -1 : 1);
	}
}

pointer_arithmetic_result_t global_memory_manager::do_pointer_arithmetic(
	ptr_t address,
	bool is_one_past_the_end,
	int64_t offset,
	type const *object_type
) const
{
	auto const object = this->get_global_object(address);
	if (object == nullptr)
	{
		return { 0, false };
	}
	else
	{
		return object->do_pointer_arithmetic(address, is_one_past_the_end, offset, object_type);
	}
}

pointer_arithmetic_result_t global_memory_manager::do_pointer_arithmetic_unchecked(
	ptr_t address,
	bool is_one_past_the_end,
	int64_t offset,
	type const *object_type
) const
{
	auto const object = this->get_global_object(address);
	bz_assert(object != nullptr);
	return object->do_pointer_arithmetic_unchecked(address, is_one_past_the_end, offset, object_type);
}

bz::optional<int64_t> global_memory_manager::do_pointer_difference(
	ptr_t lhs,
	ptr_t rhs,
	bool lhs_is_one_past_the_end,
	bool rhs_is_one_past_the_end,
	type const *object_type
) const
{
	auto const object = this->get_global_object(lhs);
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

uint8_t *global_memory_manager::get_memory(ptr_t address)
{
	auto const object = this->get_global_object(address);
	bz_assert(object != nullptr);
	return object->get_memory(address);
}

uint8_t const *global_memory_manager::get_memory(ptr_t address) const
{
	auto const object = this->get_global_object(address);
	bz_assert(object != nullptr);
	return object->get_memory(address);
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
		result.append(messages.transform([begin_stack_object](auto const &message) {
			return error_reason_t{ begin_stack_object->object_src_tokens, message };
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
	else if (!lhs_stack_object->is_alive(std::min(lhs, rhs), std::max(lhs, rhs)))
	{
		return {};
	}
	else
	{
		return lhs == rhs ? 0 : (lhs < rhs ? -1 : 1);
	}
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
	if (
		this->allocations.empty()
		|| address < this->allocations[0].object.address
		|| address > this->allocations.back().object.address + this->allocations.back().object.object_size()
	)
	{
		return nullptr;
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
	if (
		this->allocations.empty()
		|| address < this->allocations[0].object.address
		|| address > this->allocations.back().object.address + this->allocations.back().object.object_size()
	)
	{
		return nullptr;
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
	if (allocation == nullptr)
	{
		return free_result::unknown_address;
	}
	else if (address != allocation->object.address)
	{
		return free_result::address_inside_object;
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
	if (begin_allocation == end_allocation)
	{
		if (begin_allocation->is_freed)
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
			result.append(messages.transform([begin_allocation](auto const &message) {
				return error_reason_t{ begin_allocation->alloc_info.src_tokens, message };
			}));
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
	: segment_info{ meta_begin, meta_begin },
	  stack_object_pointers(),
	  one_past_the_end_pointers()
{
	auto const max_address = meta_begin <= std::numeric_limits<uint32_t>::max()
		? std::numeric_limits<uint32_t>::max()
		: std::numeric_limits<uint64_t>::max();
	auto const meta_address_space_size = max_address - meta_begin + 1;
	auto const segment_size = meta_address_space_size / meta_memory_manager::segment_info_t::N;

	static_assert(meta_memory_manager::segment_info_t::N == 2);
	this->segment_info.segment_begins[1] += segment_size;
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

memory_manager::memory_manager(
	segment_info_t _segment_info,
	global_memory_manager *_global_memory,
	bool is_64_bit,
	bool is_native_endianness
)
	: segment_info(_segment_info),
	  global_memory(_global_memory),
	  stack(_segment_info.get_segment_begin<memory_segment::stack>()),
	  heap(_segment_info.get_segment_begin<memory_segment::heap>()),
	  meta_memory(_segment_info.get_segment_begin<memory_segment::meta>())
{
	for (auto const &info : this->global_memory->one_past_the_end_pointer_infos)
	{
		auto const ptr = this->meta_memory.make_one_past_the_end_address(info.address);
		auto &memory = this->global_memory->objects[info.object_index].memory;
		if (is_64_bit)
		{
			auto const ptr64 = is_native_endianness
				? static_cast<uint64_t>(ptr)
				: byteswap(static_cast<uint64_t>(ptr));
			std::memcpy(memory.data() + info.offset, &ptr64, sizeof ptr64);
		}
		else
		{
			auto const ptr32 = is_native_endianness
				? static_cast<uint32_t>(ptr)
				: byteswap(static_cast<uint32_t>(ptr));
			std::memcpy(memory.data() + info.offset, &ptr32, sizeof ptr32);
		}
	}
}

[[nodiscard]] bool memory_manager::push_stack_frame(bz::array_view<alloca const> types)
{
	this->stack.push_stack_frame(types);
	return this->stack.head < this->segment_info.get_segment_begin<memory_segment::heap>();
}

void memory_manager::pop_stack_frame(void)
{
	this->stack.pop_stack_frame();
}

struct remove_meta_result_t
{
	ptr_t address;
	memory_segment segment;
	bool is_one_past_the_end;
	bool is_finished_stack_frame;
};

static remove_meta_result_t remove_meta(ptr_t address, memory_manager const &manager)
{
	auto segment = manager.segment_info.get_segment(address);
	bool is_one_past_the_end = false;
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
				return { address, segment, is_one_past_the_end, true };
			}

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

	return { address, segment, is_one_past_the_end, false };
}

static bz::u8string_view get_segment_name(memory_segment segment)
{
	switch (segment)
	{
	case memory_segment::invalid:
		return "invalid";
	case memory_segment::global:
		return "global";
	case memory_segment::stack:
		return "stack";
	case memory_segment::heap:
		return "heap";
	case memory_segment::meta:
		return "meta";
	}
}

bool memory_manager::check_dereference(ptr_t _address, type const *object_type) const
{
	bz_assert(_address != 0);
	auto const[address, segment, is_one_past_the_end, is_finished_stack_frame] = remove_meta(_address, *this);

	if (is_one_past_the_end || is_finished_stack_frame)
	{
		return false;
	}

	switch (segment)
	{
	case memory_segment::invalid:
		return false;
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
	auto const[address, segment, is_one_past_the_end, is_finished_stack_frame] = remove_meta(_address, *this);

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
		case memory_segment::invalid:
			bz_unreachable;
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
			result.push_back({ allocation->alloc_info.src_tokens, "address is a one-past-the-end pointer into this allocation" });
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
		case memory_segment::invalid:
			bz_unreachable;
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
	auto const[address, segment, is_one_past_the_end, is_finished_stack_frame] = remove_meta(_address, *this);

	if (is_finished_stack_frame || is_one_past_the_end || segment != memory_segment::heap)
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
	auto const[address, segment, is_one_past_the_end, is_finished_stack_frame] = remove_meta(_address, *this);

	if (segment != memory_segment::heap)
	{
		bz::vector<error_reason_t> result;
		result.push_back({ {}, "address points to an object, that is not on the heap" });
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

bool memory_manager::check_slice_construction(ptr_t _begin, ptr_t _end, type const *elem_type) const
{
	bz_assert(_begin != 0 && _end != 0);
	auto const[begin, begin_segment, begin_is_one_past_the_end, begin_is_finished_stack_frame] = remove_meta(_begin, *this);
	auto const[end, end_segment, end_is_one_past_the_end, end_is_finished_stack_frame] = remove_meta(_end, *this);

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
		case memory_segment::invalid:
			bz_unreachable;
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
	auto const[begin, begin_segment, begin_is_one_past_the_end, begin_is_finished_stack_frame] = remove_meta(_begin, *this);
	auto const[end, end_segment, end_is_one_past_the_end, end_is_finished_stack_frame] = remove_meta(_end, *this);

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
		result.push_back({
			{},
			bz::format(
				"begin and end addresses point to different memory segments",
				get_segment_name(begin_segment), get_segment_name(end_segment)
			)
		});

		switch (begin_segment)
		{
		case memory_segment::invalid:
			bz_unreachable;
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
		case memory_segment::invalid:
			bz_unreachable;
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
		case memory_segment::invalid:
			bz_unreachable;
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
		case memory_segment::invalid:
			bz_unreachable;
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
	auto const[lhs, lhs_segment, lhs_is_one_past_the_end, lhs_is_finished_stack_frame] = remove_meta(_lhs, *this);
	auto const[rhs, rhs_segment, rhs_is_one_past_the_end, rhs_is_finished_stack_frame] = remove_meta(_rhs, *this);

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
		case memory_segment::invalid:
			bz_unreachable;
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

ptr_t memory_manager::do_pointer_arithmetic(ptr_t _address, int64_t offset, type const *object_type)
{
	bz_assert(_address != 0);
	auto const [address, segment, is_one_past_the_end, is_finished_stack_frame] = remove_meta(_address, *this);

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
			case memory_segment::invalid:
				bz_unreachable;
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
		else if (offset == 0 && result_is_one_past_the_end)
		{
			bz_assert(is_one_past_the_end);
			return _address;
		}
		else if (result_is_one_past_the_end)
		{
			return this->meta_memory.make_one_past_the_end_address(result);
		}
		else
		{
			return result;
		}
	}
}

ptr_t memory_manager::do_pointer_arithmetic_unchecked(ptr_t _address, int64_t offset, type const *object_type)
{
	bz_assert(_address != 0);
	auto const [address, segment, is_one_past_the_end, is_finished_stack_frame] = remove_meta(_address, *this);

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
		case memory_segment::invalid:
			bz_unreachable;
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

	if (offset == 0 && result_is_one_past_the_end)
	{
		bz_assert(is_one_past_the_end);
		return _address;
	}
	else if (result_is_one_past_the_end)
	{
		return this->meta_memory.make_one_past_the_end_address(result);
	}
	else
	{
		return result;
	}
}

ptr_t memory_manager::do_gep(ptr_t address, type const *object_type, uint64_t index)
{
	auto const segment = this->segment_info.get_segment(address);
	switch (segment)
	{
	case memory_segment::invalid:
		bz_unreachable;
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
				return this->meta_memory.make_one_past_the_end_address(result);
			}
			else
			{
				return result;
			}
		}
		else
		{
			bz_assert(object_type->is_aggregate());
			auto const offsets = object_type->get_aggregate_offsets();
			bz_assert(index < offsets.size());
			return address + offsets[index];
		}
	}
	case memory_segment::meta:
		return this->do_gep(this->meta_memory.get_real_address(address), object_type, index);
	}
}

bz::optional<int64_t> memory_manager::do_pointer_difference(ptr_t _lhs, ptr_t _rhs, type const *object_type)
{
	bz_assert(_lhs != 0 && _rhs != 0);
	auto const[lhs, lhs_segment, lhs_is_one_past_the_end, lhs_is_finished_stack_frame] = remove_meta(_lhs, *this);
	auto const[rhs, rhs_segment, rhs_is_one_past_the_end, rhs_is_finished_stack_frame] = remove_meta(_rhs, *this);

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
		case memory_segment::invalid:
			bz_unreachable;
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

int64_t memory_manager::do_pointer_difference_unchecked(ptr_t lhs, ptr_t rhs, size_t stride)
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

void memory_manager::start_lifetime(ptr_t _address, size_t size)
{
	auto const [address, segment, is_one_past_the_end, is_finished_stack_frame] = remove_meta(_address, *this);
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
		bz_assert(allocation->object.elem_size() == size);
		allocation->object.start_lifetime(address, address + size);
		break;
	}
	default:
		bz_unreachable;
	}
}

void memory_manager::end_lifetime(ptr_t address, size_t size)
{
	address = remove_meta(address, *this).address;
	bz_assert(this->segment_info.get_segment(address) == memory_segment::stack);

	auto const object = this->stack.get_stack_object(address);
	bz_assert(object != nullptr);
	object->end_lifetime(address, address + size);
}

bool memory_manager::is_global(ptr_t address) const
{
	return address != 0 && remove_meta(address, *this).segment == memory_segment::global;
}

uint8_t *memory_manager::get_memory(ptr_t address)
{
	auto const segment = this->segment_info.get_segment(address);
	switch (segment)
	{
	case memory_segment::invalid:
		bz_unreachable;
	case memory_segment::global:
		return this->global_memory->get_memory(address);
	case memory_segment::stack:
		return this->stack.get_memory(address);
	case memory_segment::heap:
		return this->heap.get_memory(address);
	case memory_segment::meta:
		return this->get_memory(this->meta_memory.get_real_address(address));
	}
}

uint8_t const *memory_manager::get_memory(ptr_t address) const
{
	auto const segment = this->segment_info.get_segment(address);
	switch (segment)
	{
	case memory_segment::invalid:
		bz_unreachable;
	case memory_segment::global:
		return this->global_memory->get_memory(address);
	case memory_segment::stack:
		return this->stack.get_memory(address);
	case memory_segment::heap:
		return this->heap.get_memory(address);
	case memory_segment::meta:
		return this->get_memory(this->meta_memory.get_real_address(address));
	}
}

static type const *get_multi_dimensional_array_elem_type(type const *arr_type)
{
	while (arr_type->is_array())
	{
		arr_type = arr_type->get_array_element_type();
	}
	return arr_type;
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

template<typename T>
static void store(auto value, uint8_t *mem, endianness_kind endianness)
{
	if constexpr (bz::meta::is_same<T, bool>)
	{
		*mem = value ? 1 : 0;
	}
	else
	{
		if (is_native(endianness))
		{
			auto const actual_value = static_cast<T>(value);
			std::memcpy(mem, &actual_value, sizeof (T));
		}
		else
		{
			auto const int_value = byteswap(bit_cast<uint_t<sizeof  (T)>>(static_cast<T>(value)));
			std::memcpy(mem, &int_value, sizeof (T));
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

template<typename T, typename U>
static void store_array(bz::array_view<U const> values, uint8_t *mem, endianness_kind endianness)
{
	if (is_native(endianness))
	{
		if constexpr (sizeof (T) == sizeof (U))
		{
			std::memcpy(mem, values.data(), values.size() * sizeof (T));
		}
		else
		{
			for (auto const i : bz::iota(0, values.size()))
			{
				auto const elem = static_cast<T>(values[i]);
				std::memcpy(mem + i * sizeof (T), &elem, sizeof (T));
			}
		}
	}
	else
	{
		for (auto const i : bz::iota(0, values.size()))
		{
			auto const int_value = byteswap(bit_cast<uint_t<sizeof (T)>>(static_cast<T>(values[i])));
			std::memcpy(mem + i * sizeof (T), &int_value, sizeof (T));
		}
	}
}

ast::constant_value constant_value_from_object(
	type const *object_type,
	uint8_t const *mem,
	ast::typespec_view ts,
	endianness_kind endianness,
	memory_manager const &manager
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
					endianness,
					manager
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
				return constant_value_from_object(aggregate_types[0], mem, ts.get<ast::ts_optional>(), endianness, manager);
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
					bz_assert(info->kind == ast::type_info::str_);
					bz_assert(object_type->is_aggregate());
					auto const pointer_size = object_type->size / 2;
					bz_assert(pointer_size == object_type->get_aggregate_types()[0]->size);
					auto const begin_ptr = pointer_size == 8
						? load<uint64_t>(mem, endianness)
						: load<uint32_t>(mem, endianness);
					auto const end_ptr = pointer_size == 8
						? load<uint64_t>(mem + pointer_size, endianness)
						: load<uint32_t>(mem + pointer_size, endianness);

					if (manager.is_global(begin_ptr))
					{
						return ast::constant_value(bz::u8string_view(
							manager.get_memory(begin_ptr),
							manager.get_memory(end_ptr)
						));
					}
					else
					{
						return ast::constant_value();
					}
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
					endianness,
					manager
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
			bz_assert(get_multi_dimensional_array_elem_type(object_type)->size * info.size == object_type->size);
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
			auto const elem_type = get_multi_dimensional_array_elem_type(object_type);
			bz_assert(elem_type->size * info.size == object_type->size);

			auto result = ast::constant_value();
			auto &result_array = result.emplace<ast::constant_value::array>();
			result_array.reserve(info.size);

			auto mem_it = mem;
			auto const mem_end = mem + object_type->size;
			for (; mem_it != mem_end; mem_it += elem_type->size)
			{
				result_array.push_back(constant_value_from_object(elem_type, mem, info.elem_type, endianness, manager));
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

static void object_from_constant_value(
	lex::src_tokens const &src_tokens,
	ast::constant_value const &value,
	type const *object_type,
	uint8_t *mem,
	endianness_kind endianness,
	size_t current_offset,
	bz::vector<global_memory_manager::one_past_the_end_pointer_info_t> &one_past_the_end_infos,
	global_memory_manager &manager,
	type_set_t &type_set
)
{
	switch (value.kind())
	{
	case ast::constant_value::sint:
		bz_assert(object_type->is_integer_type());
		switch (object_type->get_builtin_kind())
		{
		case builtin_type_kind::i8:
			store<int8_t>(value.get_sint(), mem, endianness);
			break;
		case builtin_type_kind::i16:
			store<int16_t>(value.get_sint(), mem, endianness);
			break;
		case builtin_type_kind::i32:
			store<int32_t>(value.get_sint(), mem, endianness);
			break;
		case builtin_type_kind::i64:
			store<int64_t>(value.get_sint(), mem, endianness);
			break;
		default:
			bz_unreachable;
		}
		break;
	case ast::constant_value::uint:
		bz_assert(object_type->is_integer_type());
		switch (object_type->get_builtin_kind())
		{
		case builtin_type_kind::i8:
			store<uint8_t>(value.get_uint(), mem, endianness);
			break;
		case builtin_type_kind::i16:
			store<uint16_t>(value.get_uint(), mem, endianness);
			break;
		case builtin_type_kind::i32:
			store<uint32_t>(value.get_uint(), mem, endianness);
			break;
		case builtin_type_kind::i64:
			store<uint64_t>(value.get_uint(), mem, endianness);
			break;
		default:
			bz_unreachable;
		}
		break;
	case ast::constant_value::float32:
		bz_assert(object_type->is_floating_point_type() && object_type->get_builtin_kind() == builtin_type_kind::f32);
		store<float32_t>(value.get_float32(), mem, endianness);
		break;
	case ast::constant_value::float64:
		bz_assert(object_type->is_floating_point_type() && object_type->get_builtin_kind() == builtin_type_kind::f64);
		store<float64_t>(value.get_float64(), mem, endianness);
		break;
	case ast::constant_value::u8char:
		bz_assert(object_type->is_integer_type() && object_type->get_builtin_kind() == builtin_type_kind::i32);
		static_assert(sizeof (bz::u8char) == 4);
		store<bz::u8char>(value.get_u8char(), mem, endianness);
		break;
	case ast::constant_value::string:
	{
		auto const str = value.get_string();
		if (str.size() == 0)
		{
			std::memset(mem, 0, object_type->size);
			break;
		}

		auto const char_array_type = type_set.get_array_type(type_set.get_builtin_type(builtin_type_kind::i8), str.size());
		auto const char_array_index = manager.add_object(
			src_tokens,
			char_array_type,
			bz::fixed_vector<uint8_t>(bz::array_view(str.data(), str.size()))
		);
		auto const begin_ptr = manager.objects[char_array_index].address;
		auto const end_ptr = manager.objects[char_array_index].address + str.size();
		bz_assert(object_type->is_aggregate() && object_type->get_aggregate_types().size() == 2);
		auto const pointer_size = object_type->get_aggregate_types()[0]->size;
		bz_assert(object_type->size == 2 * pointer_size);
		if (pointer_size == 8)
		{
			auto const begin_ptr64 = is_native(endianness)
				? static_cast<uint64_t>(begin_ptr)
				: memory::byteswap(static_cast<uint64_t>(begin_ptr));
			std::memcpy(mem, &begin_ptr64, sizeof begin_ptr64);
		}
		else
		{
			auto const begin_ptr32 = is_native(endianness)
				? static_cast<uint32_t>(begin_ptr)
				: memory::byteswap(static_cast<uint32_t>(begin_ptr));
			std::memcpy(mem, &begin_ptr32, sizeof begin_ptr32);
		}
		one_past_the_end_infos.push_back({
			.object_index = 0,
			.offset = static_cast<uint32_t>(current_offset + pointer_size),
			.address = end_ptr,
		});
		break;
	}
	case ast::constant_value::boolean:
		bz_assert(object_type->is_integer_type() && object_type->get_builtin_kind() == builtin_type_kind::i1);
		store<bool>(value.get_boolean(), mem, endianness);
		break;
	case ast::constant_value::null:
		// pointers are set to null, optionals are set to not having a value
		std::memset(mem, 0, object_type->size);
		break;
	case ast::constant_value::void_:
		bz_unreachable;
	case ast::constant_value::enum_:
		bz_assert(object_type->is_integer_type());
		switch (object_type->get_builtin_kind())
		{
		case builtin_type_kind::i8:
			store<uint8_t>(value.get_enum().value, mem, endianness);
			break;
		case builtin_type_kind::i16:
			store<uint16_t>(value.get_enum().value, mem, endianness);
			break;
		case builtin_type_kind::i32:
			store<uint32_t>(value.get_enum().value, mem, endianness);
			break;
		case builtin_type_kind::i64:
			store<uint64_t>(value.get_enum().value, mem, endianness);
			break;
		default:
			bz_unreachable;
		}
		break;
	case ast::constant_value::array:
	{
		bz_assert(object_type->is_array());
		auto const elem_type = get_multi_dimensional_array_elem_type(object_type);
		auto const array = value.get_array();
		bz_assert(array.size() == object_type->size / elem_type->size);
		auto const elem_size = elem_type->size;
		for (auto const &elem : array)
		{
			object_from_constant_value(src_tokens, elem, elem_type, mem, endianness, current_offset, one_past_the_end_infos, manager, type_set);
			mem += elem_size;
			current_offset += elem_size;
		}
		break;
	}
	case ast::constant_value::sint_array:
	{
		bz_assert(object_type->is_array());
		auto const elem_type = get_multi_dimensional_array_elem_type(object_type);
		auto const array = value.get_sint_array();
		bz_assert(array.size() == object_type->size / elem_type->size);
		bz_assert(elem_type->is_integer_type());
		switch (elem_type->get_builtin_kind())
		{
		case builtin_type_kind::i8:
			store_array<int8_t>(array, mem, endianness);
			break;
		case builtin_type_kind::i16:
			store_array<int16_t>(array, mem, endianness);
			break;
		case builtin_type_kind::i32:
			store_array<int32_t>(array, mem, endianness);
			break;
		case builtin_type_kind::i64:
			store_array<int64_t>(array, mem, endianness);
			break;
		default:
			bz_unreachable;
		}
		break;
	}
	case ast::constant_value::uint_array:
	{
		bz_assert(object_type->is_array());
		auto const elem_type = get_multi_dimensional_array_elem_type(object_type);
		auto const array = value.get_uint_array();
		bz_assert(array.size() == object_type->size / elem_type->size);
		bz_assert(elem_type->is_integer_type());
		switch (elem_type->get_builtin_kind())
		{
		case builtin_type_kind::i8:
			store_array<uint8_t>(array, mem, endianness);
			break;
		case builtin_type_kind::i16:
			store_array<uint16_t>(array, mem, endianness);
			break;
		case builtin_type_kind::i32:
			store_array<uint32_t>(array, mem, endianness);
			break;
		case builtin_type_kind::i64:
			store_array<uint64_t>(array, mem, endianness);
			break;
		default:
			bz_unreachable;
		}
		break;
	}
	case ast::constant_value::float32_array:
	{
		bz_assert(object_type->is_array());
		auto const elem_type = get_multi_dimensional_array_elem_type(object_type);
		auto const array = value.get_float32_array();
		bz_assert(array.size() == object_type->size / elem_type->size);
		bz_assert(elem_type->is_floating_point_type() && elem_type->get_builtin_kind() == builtin_type_kind::f32);
		store_array<float32_t>(array, mem, endianness);
		break;
	}
	case ast::constant_value::float64_array:
	{
		bz_assert(object_type->is_array());
		auto const elem_type = get_multi_dimensional_array_elem_type(object_type);
		auto const array = value.get_float64_array();
		bz_assert(array.size() == object_type->size / elem_type->size);
		bz_assert(elem_type->is_floating_point_type() && elem_type->get_builtin_kind() == builtin_type_kind::f64);
		store_array<float64_t>(array, mem, endianness);
		break;
	}
	case ast::constant_value::tuple:
	{
		auto const values = value.get_tuple();
		bz_assert(object_type->is_aggregate());
		auto const types = object_type->get_aggregate_types();
		auto const offsets = object_type->get_aggregate_offsets();
		bz_assert(types.size() == values.size());

		for (auto const i : bz::iota(0, values.size()))
		{
			object_from_constant_value(
				src_tokens,
				values[i],
				types[i],
				mem + offsets[i],
				endianness,
				current_offset + offsets[i],
				one_past_the_end_infos,
				manager,
				type_set
			);
		}
		break;
	}
	case ast::constant_value::function:
		bz_unreachable; // TODO
	case ast::constant_value::type:
		bz_unreachable;
	case ast::constant_value::aggregate:
	{
		auto const values = value.get_aggregate();
		bz_assert(object_type->is_aggregate());
		auto const types = object_type->get_aggregate_types();
		auto const offsets = object_type->get_aggregate_offsets();
		bz_assert(types.size() == values.size());

		for (auto const i : bz::iota(0, values.size()))
		{
			object_from_constant_value(
				src_tokens,
				values[i],
				types[i],
				mem + offsets[i],
				endianness,
				current_offset + offsets[i],
				one_past_the_end_infos,
				manager,
				type_set
			);
		}
		break;
	}
	default:
		bz_unreachable;
	}
}

object_from_constant_value_result_t object_from_constant_value(
	lex::src_tokens const &src_tokens,
	ast::constant_value const &value,
	type const *object_type,
	endianness_kind endianness,
	global_memory_manager &manager,
	type_set_t &type_set
)
{
	auto result = object_from_constant_value_result_t{
		.data = bz::fixed_vector<uint8_t>(object_type->size),
		.one_past_the_end_infos = {},
	};
	object_from_constant_value(
		src_tokens,
		value,
		object_type,
		result.data.data(),
		endianness,
		0,
		result.one_past_the_end_infos,
		manager,
		type_set
	);
	return result;
}

} // namespace comptime::memory
