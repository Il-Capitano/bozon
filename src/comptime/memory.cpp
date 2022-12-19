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

bool stack_object::check_memory_access(ptr_t address, type const *subobject_type) const
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
	auto const size = total_size / elem_type->size;
	auto const offset = begin - this->address;

	if (size == 1)
	{
		return contained_in_object(this->object_type, offset, elem_type);
	}
	else
	{
		return slice_contained_in_object(this->object_type, offset, elem_type, total_size);
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

bool heap_object::check_memory_access(ptr_t address, type const *subobject_type) const
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
	auto const size = total_size / elem_type->size;
	auto const offset = begin - this->address;

	if (elem_type == this->elem_type)
	{
		return offset % this->elem_size() == 0;
	}
	else if (size == 1)
	{
		return contained_in_object(this->elem_type, offset % this->elem_size(), elem_type);
	}
	else
	{
		return slice_contained_in_object(this->elem_type, offset % this->elem_size(), elem_type, total_size);
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

static size_t round_up(size_t size, size_t align)
{
	if (size % align == 0)
	{
		return size;
	}
	else
	{
		return size + (align - size % align);
	}
}

} // namespace comptime::memory
