#include "memory.h"

namespace comptime::memory
{

static constexpr uint8_t max_object_align = 8;

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
	  is_initialized(true)
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
	auto const rounded_size = size % max_object_align == 0 ? size : size + (max_object_align - size % max_object_align);
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

stack_manager::stack_manager(ptr_t stack_begin)
	: head(stack_begin),
	  stack_frames(),
	  stack_frame_id(0)
{}

void stack_manager::push_stack_frame(bz::array_view<type const *const> types)
{
	auto const begin_address = this->head;

	auto &new_frame = this->stack_frames.emplace_back();
	new_frame.begin_address = begin_address;
	new_frame.id = this->stack_frame_id;
	this->stack_frame_id += 1;

	auto object_address = begin_address;
	new_frame.objects = bz::fixed_vector<stack_object>(
		types.transform([&object_address, begin_address](type const *object_type) {
			object_address = object_address == begin_address
				? begin_address
				: object_address + (object_type->align - object_address % object_type->align);
			bz_assert(object_address % object_type->align == 0);
			auto result = stack_object(object_address, object_type);
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

	// find the first element that has an address that is less than or equal to address.
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
	// find the first element that has an address that is less than or equal to address.
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

bool stack_manager::check_dereference(ptr_t address, type const *object_type)
{
	auto const object = this->get_stack_object(address);
	return object != nullptr && object->check_dereference(address, object_type);
}

bool stack_manager::check_slice_construction(ptr_t begin, ptr_t end, type const *elem_type)
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
		return object->check_slice_construction(begin, end, elem_type);
	}
}

pointer_arithmetic_result_t stack_manager::do_pointer_arithmetic(ptr_t address, int64_t offset, type const *object_type)
{
	auto const object = this->get_stack_object(address);
	if (object == nullptr)
	{
		return { 0, false };
	}
	else if (!object->is_initialized)
	{
		return { 0, false };
	}
	else
	{
		return object->do_pointer_arithmetic(address, offset, object_type);
	}
}

bz::optional<int64_t> stack_manager::do_pointer_difference(ptr_t lhs, ptr_t rhs, type const *object_type)
{
	auto const object = this->get_stack_object(lhs);
	if (object == nullptr)
	{
		return {};
	}
	else if (!object->is_initialized)
	{
		return {};
	}
	else if (rhs < object->address || rhs > object->address + object->object_size())
	{
		return {};
	}
	else
	{
		return object->do_pointer_difference(lhs, rhs, object_type);
	}
}

uint8_t *stack_manager::get_memory(ptr_t address)
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
	this->object.is_initialized.clear();
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

ptr_t heap_manager::allocate(call_stack_info_t alloc_info, type const *object_type, uint64_t count)
{
	auto const address = this->head;
	auto const allocation_size = object_type->size * count;
	// we round up the allocation size to the nearest 16-byte boundary
	// if it's already at such a boundary, then we simply add 16 to add some padding
	auto const rounded_allocation_size = allocation_size + (16 - allocation_size % 16);
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

bool heap_manager::check_dereference(ptr_t address, type const *object_type)
{
	auto const allocation = this->get_allocation(address);
	return allocation != nullptr && !allocation->is_freed &&  allocation->object.check_dereference(address, object_type);
}

bool heap_manager::check_slice_construction(ptr_t begin, ptr_t end, type const *elem_type)
{
	if (end < begin)
	{
		return false;
	}

	auto const allocation = this->get_allocation(begin);
	if (allocation == nullptr)
	{
		return false;
	}
	else if (allocation->is_freed)
	{
		return false;
	}
	else if (end > allocation->object.address + allocation->object.object_size())
	{
		return false;
	}
	else
	{
		return allocation->object.check_slice_construction(begin, end, elem_type);
	}
}

pointer_arithmetic_result_t heap_manager::do_pointer_arithmetic(ptr_t address, int64_t offset, type const *object_type)
{
	auto const allocation = this->get_allocation(address);
	if (allocation == nullptr || allocation->is_freed) // FIXME: should pointer arithmetic with a freed address be an error?
	{
		return { 0, false };
	}
	else
	{
		return allocation->object.do_pointer_arithmetic(address, offset, object_type);
	}
}

bz::optional<int64_t> heap_manager::do_pointer_difference(ptr_t lhs, ptr_t rhs, type const *object_type)
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
		return allocation->object.do_pointer_difference(lhs, rhs, object_type);
	}
}

uint8_t *heap_manager::get_memory(ptr_t address)
{
	auto const allocation = this->get_allocation(address);
	bz_assert(allocation != nullptr);
	bz_assert(!allocation->is_freed);
	return allocation->object.get_memory(address);
}

meta_memory_manager::meta_memory_manager(ptr_t meta_begin)
	: stack_object_begin_address(meta_begin),
	  one_past_the_end_begin_address(meta_begin),
	  stack_object_pointers(),
	  one_past_the_end_pointers()
{
	auto const max_address = meta_begin <= std::numeric_limits<uint32_t>::max()
		? std::numeric_limits<uint32_t>::max()
		: std::numeric_limits<uint64_t>::max();
	auto const meta_address_space_size = max_address - meta_begin + 1;
	auto const segment_size = meta_address_space_size / 2;

	this->one_past_the_end_begin_address += segment_size;
}

ptr_t meta_memory_manager::get_real_address(ptr_t address) const
{
	bz_assert(address >= this->stack_object_begin_address);
	if (address < this->one_past_the_end_begin_address)
	{
		auto const index = address - this->stack_object_begin_address;
		bz_assert(index < this->stack_object_pointers.size());
		return this->stack_object_pointers[index].stack_address;
	}
	else
	{
		auto const index = address - this->one_past_the_end_begin_address;
		bz_assert(index < this->one_past_the_end_pointers.size());
		return this->one_past_the_end_pointers[index].address;
	}
}

bool meta_memory_manager::is_valid(ptr_t address, bz::array_view<stack_frame const> current_stack_frames) const
{
	if (address < this->stack_object_begin_address)
	{
		return false;
	}

	if (address < this->one_past_the_end_begin_address)
	{
		auto const index = address - this->stack_object_begin_address;
		if (index >= this->stack_object_pointers.size())
		{
			return false;
		}

		auto const &object = this->stack_object_pointers[index];
		return object.stack_frame_depth < current_stack_frames.size()
			&& object.stack_frame_id == current_stack_frames[object.stack_frame_depth].id;
	}
	else
	{
		auto const index = address - this->one_past_the_end_begin_address;
		return index < this->one_past_the_end_pointers.size();
	}
}

ptr_t meta_memory_manager::make_one_past_the_end_address(ptr_t address)
{
	auto const result_index = this->one_past_the_end_pointers.size();
	this->one_past_the_end_pointers.push_back({ address });
	return this->one_past_the_end_begin_address + result_index;
}

memory_segment memory_segment_info_t::get_segment(ptr_t address) const
{
	if (address < this->stack_begin)
	{
		return memory_segment::invalid;
	}
	else if (address < this->heap_begin)
	{
		return memory_segment::stack;
	}
	else if (address < this->meta_begin)
	{
		return memory_segment::heap;
	}
	else
	{
		return memory_segment::meta;
	}
}

memory_manager::memory_manager(memory_segment_info_t _segment_info)
	: segment_info(_segment_info),
	  stack(_segment_info.stack_begin),
	  heap(_segment_info.heap_begin),
	  meta_memory(_segment_info.meta_begin)
{}

[[nodiscard]] bool memory_manager::push_stack_frame(bz::array_view<type const *const> types)
{
	this->stack.push_stack_frame(types);
	return this->stack.head < this->segment_info.heap_begin;
}

void memory_manager::pop_stack_frame(void)
{
	this->stack.pop_stack_frame();
}

bool memory_manager::check_dereference(ptr_t address, type const *object_type)
{
	auto const segment = this->segment_info.get_segment(address);
	switch (segment)
	{
	case memory_segment::invalid:
		return false;
	case memory_segment::stack:
		return this->stack.check_dereference(address, object_type);
	case memory_segment::heap:
		return this->heap.check_dereference(address, object_type);
	case memory_segment::meta:
		// TODO: one-past-the-end pointers shouldn't be valid here
		return this->meta_memory.is_valid(address, this->stack.stack_frames)
			&& this->check_dereference(this->meta_memory.get_real_address(address), object_type);
	}
}

bool memory_manager::check_slice_construction(ptr_t begin, ptr_t end, type const *elem_type)
{
	auto const begin_segment = this->segment_info.get_segment(begin);
	auto const end_segment = this->segment_info.get_segment(end);

	if (begin_segment == memory_segment::meta && end_segment == memory_segment::meta)
	{
		return this->meta_memory.is_valid(begin, this->stack.stack_frames)
			&& this->meta_memory.is_valid(end, this->stack.stack_frames)
			&& this->check_slice_construction(
				this->meta_memory.get_real_address(begin),
				this->meta_memory.get_real_address(end),
				elem_type
			);
	}
	else if (begin_segment == memory_segment::meta)
	{
		return this->meta_memory.is_valid(begin, this->stack.stack_frames)
			&& this->check_slice_construction(
				this->meta_memory.get_real_address(begin),
				end,
				elem_type
			);
	}
	else if (end_segment == memory_segment::meta)
	{
		return this->meta_memory.is_valid(end, this->stack.stack_frames)
			&& this->check_slice_construction(
				begin,
				this->meta_memory.get_real_address(end),
				elem_type
			);
	}
	else if (begin_segment != end_segment)
	{
		return false;
	}
	else
	{
		switch (begin_segment)
		{
		case memory_segment::invalid:
			return false;
		case memory_segment::stack:
			return this->stack.check_slice_construction(begin, end, elem_type);
		case memory_segment::heap:
			return this->heap.check_slice_construction(begin, end, elem_type);
		case memory_segment::meta:
			bz_unreachable;
		}
	}
}

bz::u8string memory_manager::get_slice_construction_error_reason(ptr_t begin, ptr_t end, type const *elem_type)
{
	bz_unreachable;
}

bz::optional<int> memory_manager::compare_pointers(ptr_t lhs, ptr_t rhs)
{
	auto const lhs_segment = this->segment_info.get_segment(lhs);
	auto const rhs_segment = this->segment_info.get_segment(lhs);

	if (lhs_segment == memory_segment::meta && rhs_segment == memory_segment::meta)
	{
		if (this->meta_memory.is_valid(lhs, this->stack.stack_frames) && this->meta_memory.is_valid(rhs, this->stack.stack_frames))
		{
			return this->compare_pointers(this->meta_memory.get_real_address(lhs), this->meta_memory.get_real_address(rhs));
		}
		else
		{
			return {};
		}
	}
	else if (lhs_segment == memory_segment::meta)
	{
		if (this->meta_memory.is_valid(lhs, this->stack.stack_frames))
		{
			return this->compare_pointers(this->meta_memory.get_real_address(lhs), rhs);
		}
		else
		{
			return {};
		}
	}
	else if (rhs_segment == memory_segment::meta)
	{
		if (this->meta_memory.is_valid(rhs, this->stack.stack_frames))
		{
			return this->compare_pointers(lhs, this->meta_memory.get_real_address(rhs));
		}
		else
		{
			return {};
		}
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
			return {};
		case memory_segment::stack:
		case memory_segment::heap:
			return lhs == rhs ? 0 : (lhs < rhs ? -1 : 1);
		case memory_segment::meta:
			bz_unreachable;
		}
	}
}

ptr_t memory_manager::do_pointer_arithmetic(ptr_t address, int64_t offset, type const *object_type)
{
	auto const segment = this->segment_info.get_segment(address);
	switch (segment)
	{
	case memory_segment::invalid:
		return 0;
	case memory_segment::stack:
	{
		auto const [result, is_one_past_the_end] = this->stack.do_pointer_arithmetic(address, offset, object_type);
		if (is_one_past_the_end)
		{
			return this->meta_memory.make_one_past_the_end_address(result);
		}
		else
		{
			return result;
		}
	}
	case memory_segment::heap:
	{
		auto const [result, is_one_past_the_end] = this->heap.do_pointer_arithmetic(address, offset, object_type);
		if (is_one_past_the_end)
		{
			return this->meta_memory.make_one_past_the_end_address(result);
		}
		else
		{
			return result;
		}
	}
	case memory_segment::meta:
		if (this->meta_memory.is_valid(address, this->stack.stack_frames))
		{
			return this->do_pointer_arithmetic(this->meta_memory.get_real_address(address), offset, object_type);
		}
		else
		{
			return 0;
		}
	}
}

ptr_t memory_manager::do_gep(ptr_t address, type const *object_type, uint64_t index)
{
	auto const segment = this->segment_info.get_segment(address);
	switch (segment)
	{
	case memory_segment::invalid:
		bz_unreachable;
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
		bz_assert(this->meta_memory.is_valid(address, this->stack.stack_frames));
		return this->do_gep(this->meta_memory.get_real_address(address), object_type, index);
	}
}

bz::optional<int64_t> memory_manager::do_pointer_difference(ptr_t lhs, ptr_t rhs, type const *object_type)
{
	auto const lhs_segment = this->segment_info.get_segment(lhs);
	auto const rhs_segment = this->segment_info.get_segment(lhs);

	if (lhs_segment == memory_segment::meta && rhs_segment == memory_segment::meta)
	{
		if (this->meta_memory.is_valid(lhs, this->stack.stack_frames) && this->meta_memory.is_valid(rhs, this->stack.stack_frames))
		{
			return this->do_pointer_difference(
				this->meta_memory.get_real_address(lhs),
				this->meta_memory.get_real_address(rhs),
				object_type
			);
		}
		else
		{
			return {};
		}
	}
	else if (lhs_segment == memory_segment::meta)
	{
		if (this->meta_memory.is_valid(lhs, this->stack.stack_frames))
		{
			return this->do_pointer_difference(this->meta_memory.get_real_address(lhs), rhs, object_type);
		}
		else
		{
			return {};
		}
	}
	else if (rhs_segment == memory_segment::meta)
	{
		if (this->meta_memory.is_valid(rhs, this->stack.stack_frames))
		{
			return this->do_pointer_difference(lhs, this->meta_memory.get_real_address(rhs), object_type);
		}
		else
		{
			return {};
		}
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
			return {};
		case memory_segment::stack:
			return this->stack.do_pointer_difference(lhs, rhs, object_type);
		case memory_segment::heap:
			return this->heap.do_pointer_difference(lhs, rhs, object_type);
		case memory_segment::meta:
			bz_unreachable;
		}
	}
}

int64_t memory_manager::do_pointer_difference_unchecked(ptr_t lhs, ptr_t rhs, size_t stride)
{
	auto const lhs_segment = this->segment_info.get_segment(lhs);
	auto const rhs_segment = this->segment_info.get_segment(lhs);

	if (lhs_segment == memory_segment::meta && rhs_segment == memory_segment::meta)
	{
		bz_assert(this->meta_memory.is_valid(lhs, this->stack.stack_frames));
		bz_assert(this->meta_memory.is_valid(rhs, this->stack.stack_frames));
		return this->do_pointer_difference_unchecked(
			this->meta_memory.get_real_address(lhs),
			this->meta_memory.get_real_address(rhs),
			stride
		);
	}
	else if (lhs_segment == memory_segment::meta)
	{
		bz_assert(this->meta_memory.is_valid(lhs, this->stack.stack_frames));
		return this->do_pointer_difference_unchecked(this->meta_memory.get_real_address(lhs), rhs, stride);
	}
	else if (rhs_segment == memory_segment::meta)
	{
		bz_assert(this->meta_memory.is_valid(rhs, this->stack.stack_frames));
		return this->do_pointer_difference_unchecked(lhs, this->meta_memory.get_real_address(rhs), stride);
	}
	else
	{
		bz_assert(lhs_segment == rhs_segment);
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
}

uint8_t *memory_manager::get_memory(ptr_t address)
{
	auto const segment = this->segment_info.get_segment(address);
	switch (segment)
	{
	case memory_segment::invalid:
		bz::log("0x{:x}\n", address);
		bz_unreachable;
	case memory_segment::stack:
		return this->stack.get_memory(address);
	case memory_segment::heap:
		return this->heap.get_memory(address);
	case memory_segment::meta:
		bz_assert(this->meta_memory.is_valid(address, this->stack.stack_frames));
		return this->get_memory(this->meta_memory.get_real_address(address));
	}
}

} // namespace comptime::memory
