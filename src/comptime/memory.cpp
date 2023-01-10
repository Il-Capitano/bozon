#include "memory.h"

namespace comptime::memory
{

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

static bool slice_contained_in_object(type const *object_type, size_t offset, type const *elem_type, size_t total_size)
{
	bz_assert(total_size / elem_type->size > 1);
	static_assert(type::variant_count == 4);
	if (offset + total_size > object_type->size)
	{
		return false;
	}
	else if (object_type->is_builtin() || object_type->is_pointer())
	{
		// builtin and pointer types cannot contain more than one consecutive elements
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
		return slice_contained_in_object(members[member_index], offset - offsets[member_index], elem_type, total_size);
	}
	else if (object_type->is_array())
	{
		auto const array_elem_type = object_type->get_array_element_type();
		auto const offset_in_elem = offset % array_elem_type->size;
		if (array_elem_type == elem_type)
		{
			// the slice must be able to fit into this array because of the check `offset + total_size > object_type->size`
			// at the beginning
			return offset_in_elem == 0;
		}
		else
		{
			bz_assert(offset / array_elem_type->size < object_type->get_array_size());
			return slice_contained_in_object(array_elem_type, offset_in_elem, elem_type, total_size);
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
		auto const member_index = std::upper_bound(offsets.begin() + 1, offsets.end(), offset) - offsets.begin() - 1;
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
			auto const elem_offset = offset - offset % array_elem_type->size;
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

stack_object::stack_object(ptr_t _address, type const *_object_type)
	: address(_address),
	  object_type(_object_type),
	  memory(),
	  is_initialized(false)
{
	auto const size = this->object_type->size;
	this->memory = bz::fixed_vector<uint8_t>(size, 0);
}

size_t stack_object::object_size(void) const
{
	return this->memory.size();
}

void stack_object::initialize(void)
{
	this->is_initialized = true;
}

void stack_object::deinitialize(void)
{
	this->is_initialized = false;
}

uint8_t *stack_object::get_memory(ptr_t address)
{
	bz_assert(address >= this->address && address < this->address + this->object_size());
	bz_assert(this->memory.not_empty());
	return this->memory.data() + (address - this->address);
}

bool stack_object::check_dereference(ptr_t address, type const *subobject_type) const
{
	if (!this->is_initialized)
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

bool stack_object::check_slice_construction(ptr_t begin, ptr_t end, type const *elem_type) const
{
	if (begin == end)
	{
		return true;
	}

	if (!this->is_initialized)
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

	if (total_size == elem_type->size) // slice of size 1
	{
		return contained_in_object(this->object_type, offset, elem_type);
	}
	else
	{
		return slice_contained_in_object(this->object_type, offset, elem_type, total_size);
	}
}

pointer_arithmetic_result_t stack_object::do_pointer_arithmetic(ptr_t address, int64_t amount, type const *pointer_type) const
{
	auto const result_address = address + static_cast<uint64_t>(amount * static_cast<int64_t>(pointer_type->size));
	if (result_address < this->address || result_address > this->address + this->object_size())
	{
		return {
			.address = 0,
			.is_one_past_the_end = false,
		};
	}

	auto const check_result = check_pointer_arithmetic(
		this->object_type,
		address - this->address,
		result_address - this->address,
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

bz::optional<int64_t> stack_object::do_pointer_difference(ptr_t lhs, ptr_t rhs, type const *object_type)
{
	if (lhs <= rhs)
	{
		auto const slice_check = this->check_slice_construction(lhs, rhs, object_type);
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
		auto const slice_check = this->check_slice_construction(rhs, lhs, object_type);
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
	  is_initialized()
{
	auto const size = this->elem_type->size * this->count;
	auto const rounded_size = size % 8 == 0 ? size : size + (8 - size % 8);
	this->memory = bz::fixed_vector<uint8_t>(size, 0);
	this->is_initialized = bz::fixed_vector<uint8_t>(rounded_size / 8, 0xff);
}

size_t heap_object::object_size(void) const
{
	return this->memory.size();
}

size_t heap_object::elem_size(void) const
{
	return this->elem_type->size;
}

void heap_object::initialize_region(ptr_t begin, ptr_t end)
{
	if (begin >= end)
	{
		return;
	}

	bz_assert(begin >= this->address && begin < this->address + this->object_size());
	bz_assert(end > this->address && end <= this->address + this->object_size());

	auto begin_offset = begin - this->address;
	auto end_offset = end - this->address;

	if (begin_offset / 8 == end_offset / 8)
	{
		auto const begin_rem = begin_offset % 8;
		auto const end_rem = end_offset % 8;

		uint8_t const begin_bits = begin_rem == 0 ? uint8_t(-1) : uint8_t((1u << begin_rem) - 1);
		uint8_t const end_bits = uint8_t(-1) << (8 - end_rem);
		uint8_t const needed_bits = begin_bits & end_bits;
		this->is_initialized[begin_offset / 8] &= ~needed_bits;
	}
	else
	{
		if (begin_offset % 8 != 0)
		{
			auto const rem = begin_offset % 8;
			uint8_t const needed_bits = (uint8_t(1) << rem) - 1;
			this->is_initialized[begin_offset / 8] &= ~needed_bits;
			begin_offset += 8 - rem;
		}

		if (end_offset % 8 != 0)
		{
			auto const rem = end_offset % 8;
			uint8_t const needed_bits = uint8_t(-1) << (8 - rem);
			this->is_initialized[end_offset / 8] &= ~needed_bits;
			end_offset -= rem;
		}

		auto const slice = this->is_initialized.slice(begin_offset / 8, end_offset / 8);
		std::memset(slice.data(), 0, slice.size());
	}
}

bool heap_object::is_region_initialized(ptr_t begin, ptr_t end) const
{
	if (begin == end)
	{
		return true;
	}
	else if (begin > end)
	{
		return false;
	}

	bz_assert(begin >= this->address && begin < this->address + this->object_size());
	bz_assert(end > this->address && end <= this->address + this->object_size());

	auto begin_offset = begin - this->address;
	auto end_offset = end - this->address;

	if (begin_offset / 8 == end_offset / 8)
	{
		auto const begin_rem = begin_offset % 8;
		auto const end_rem = end_offset % 8;

		uint8_t const begin_bits = begin_rem == 0 ? uint8_t(-1) : uint8_t((1u << begin_rem) - 1);
		uint8_t const end_bits = uint8_t(-1) << (8 - end_rem);
		uint8_t const needed_bits = begin_bits & end_bits;
		return (this->is_initialized[begin_offset / 8] & needed_bits) == 0;
	}
	else
	{
		if (begin_offset % 8 != 0)
		{
			auto const rem = begin_offset % 8;
			uint8_t const needed_bits = (uint8_t(1) << rem) - 1;
			if ((this->is_initialized[begin_offset / 8] & needed_bits) != 0)
			{
				return false;
			}
			begin_offset += 8 - rem;
		}

		if (end_offset % 8 != 0)
		{
			auto const rem = end_offset % 8;
			uint8_t const needed_bits = uint8_t(-1) << (8 - rem);
			if ((this->is_initialized[end_offset / 8] & needed_bits) != 0)
			{
				return false;
			}
			end_offset -= rem;
		}

		auto const slice = this->is_initialized.slice(begin_offset / 8, end_offset / 8);
		return slice.is_all([](auto const val) { return val == 0; });
	}
}

uint8_t *heap_object::get_memory(ptr_t address)
{
	bz_assert(address >= this->address && address < this->address + this->object_size());
	bz_assert(this->memory.not_empty());
	return this->memory.data() + (address - this->address);
}

bool heap_object::check_dereference(ptr_t address, type const *subobject_type) const
{
	if (!this->is_region_initialized(address, address + subobject_type->size))
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

bool heap_object::check_slice_construction(ptr_t begin, ptr_t end, type const *elem_type) const
{
	if (begin == end)
	{
		return true;
	}

	if (!this->is_region_initialized(begin, end))
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

	if (elem_type == this->elem_type)
	{
		return offset % this->elem_size() == 0;
	}
	else if (total_size == elem_type->size) // slice of size 1
	{
		return contained_in_object(this->elem_type, offset % this->elem_size(), elem_type);
	}
	else
	{
		return slice_contained_in_object(this->elem_type, offset % this->elem_size(), elem_type, total_size);
	}
}

pointer_arithmetic_result_t heap_object::do_pointer_arithmetic(ptr_t address, int64_t amount, type const *pointer_type) const
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

	auto const offset = address - this->address;
	auto const result_offset = result_address - this->address;

	auto const elem_offset = offset - offset % this->elem_size();
	if (result_offset < elem_offset)
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

bz::optional<int64_t> heap_object::do_pointer_difference(ptr_t lhs, ptr_t rhs, type const *object_type)
{
	if (lhs <= rhs)
	{
		auto const slice_check = this->check_slice_construction(lhs, rhs, object_type);
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
		auto const slice_check = this->check_slice_construction(rhs, lhs, object_type);
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

bool allocation::free(lex::src_tokens const &free_src_tokens)
{
	if (this->is_freed)
	{
		return false;
	}

	this->object.memory.clear();
	this->object.is_initialized.clear();
	this->free_src_tokens = free_src_tokens;
	this->is_freed = true;
	return true;
}

} // namespace comptime::memory
