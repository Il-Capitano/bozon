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

pointer_arithmetic_result_t global_object::do_pointer_arithmetic(ptr_t address, int64_t amount, type const *pointer_type) const
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

bitset::bitset(size_t size, bool value)
	: bits(size % 8 == 0 ? (size / 8) : (size / 8 + 1), value ? 0xff : 0x00)
{}

void bitset::set_range(size_t begin, size_t end, bool value)
{
	if (begin >= end)
	{
		return;
	}

	if (begin % 8 == 0 && end % 8 == 0)
	{
		auto const begin_ptr = this->bits.data() + begin / 8;
		auto const size = (end - begin) / 8;
		if (value)
		{
			std::memset(begin_ptr, 0xff, size);
		}
		else
		{
			std::memset(begin_ptr, 0x00, size);
		}
		return;
	}

	auto const begin_byte = this->bits.begin() + begin / 8;
	auto const end_byte = this->bits.begin() + (end - 1) / 8;

	if (begin_byte == end_byte)
	{
		auto const begin_bit_index = 8 - begin % 8;
		auto const end_bit_index = 8 - ((end - 1) % 8 + 1);
		auto const begin_bits = static_cast<uint8_t>((1 << begin_bit_index) - 1);
		auto const end_bits = static_cast<uint8_t>(~((1 << end_bit_index) - 1));
		auto const bits = static_cast<uint8_t>(begin_bits & end_bits);
		if (value)
		{
			*begin_byte |= bits;
		}
		else
		{
			*begin_byte &= ~bits;
		}
	}
	else
	{
		auto const begin_bit_index = 8 - begin % 8;
		auto const end_bit_index = 8 - ((end - 1) % 8 + 1);
		auto const begin_bits = static_cast<uint8_t>((1 << begin_bit_index) - 1);
		auto const end_bits = static_cast<uint8_t>(~((1 << end_bit_index) - 1));
		if (value)
		{
			*begin_byte |= begin_bits;
			*end_byte |= end_bits;
			std::memset(&*(begin_byte + 1), 0xff, end_byte - (begin_byte + 1));
		}
		else
		{
			*begin_byte &= ~begin_bits;
			*end_byte &= ~end_bits;
			std::memset(&*(begin_byte + 1), 0x00, end_byte - (begin_byte + 1));
		}
	}
}

bool bitset::is_all(size_t begin, size_t end) const
{
	if (begin >= end)
	{
		return true;
	}

	if (begin % 8 == 0 && end % 8 == 0)
	{
		return this->bits.slice(begin / 8, end / 8).is_all([](auto const byte) { return byte == 0xff; });
	}

	auto const begin_byte = this->bits.begin() + begin / 8;
	auto const end_byte = this->bits.begin() + (end - 1) / 8;

	if (begin_byte == end_byte)
	{
		auto const begin_bit_index = 8 - begin % 8;
		auto const end_bit_index = 8 - ((end - 1) % 8 + 1);
		auto const begin_bits = static_cast<uint8_t>((1 << begin_bit_index) - 1);
		auto const end_bits = static_cast<uint8_t>(~((1 << end_bit_index) - 1));
		auto const bits = static_cast<uint8_t>(begin_bits & end_bits);
		return (*begin_byte & bits) == bits;
	}
	else
	{
		auto const begin_bit_index = 8 - begin % 8;
		auto const end_bit_index = 8 - ((end - 1) % 8 + 1);
		auto const begin_bits = static_cast<uint8_t>((1 << begin_bit_index) - 1);
		auto const end_bits = static_cast<uint8_t>(~((1 << end_bit_index) - 1));
		if ((*begin_byte & begin_bits) != begin_bits || (*end_byte & end_bits) != end_bits)
		{
			return false;
		}

		return bz::basic_range(begin_byte + 1, end_byte).is_all([](auto const byte) { return byte == 0xff; });
	}
}

bool bitset::is_none(size_t begin, size_t end) const
{
	if (begin >= end)
	{
		return true;
	}

	if (begin % 8 == 0 && end % 8 == 0)
	{
		return this->bits.slice(begin / 8, end / 8).is_all([](auto const byte) { return byte == 0x00; });
	}

	auto const begin_byte = this->bits.begin() + begin / 8;
	auto const end_byte = this->bits.begin() + (end - 1) / 8;

	if (begin_byte == end_byte)
	{
		auto const begin_bit_index = 8 - begin % 8;
		auto const end_bit_index = 8 - ((end - 1) % 8 + 1);
		auto const begin_bits = static_cast<uint8_t>((1 << begin_bit_index) - 1);
		auto const end_bits = static_cast<uint8_t>(~((1 << end_bit_index) - 1));
		auto const bits = static_cast<uint8_t>(begin_bits & end_bits);
		return (*begin_byte & ~bits) == 0;
	}
	else
	{
		auto const begin_bit_index = 8 - begin % 8;
		auto const end_bit_index = 8 - ((end - 1) % 8 + 1);
		auto const begin_bits = static_cast<uint8_t>((1 << begin_bit_index) - 1);
		auto const end_bits = static_cast<uint8_t>(~((1 << end_bit_index) - 1));
		if ((*begin_byte & ~begin_bits) != 0 || (*end_byte & ~end_bits) != 0)
		{
			return false;
		}

		return bz::basic_range(begin_byte + 1, end_byte).is_all([](auto const byte) { return byte == 0x00; });
	}
}

void bitset::clear(void)
{
	this->bits.clear();
}

stack_object::stack_object(lex::src_tokens const &_object_src_tokens, ptr_t _address, type const *_object_type, bool is_always_initialized)
	: address(_address),
	  object_type(_object_type),
	  memory(_object_type->size, 0),
	  is_alive_bitset(_object_type->size, is_always_initialized),
	  object_src_tokens(_object_src_tokens)
{}

size_t stack_object::object_size(void) const
{
	return this->memory.size();
}

void stack_object::start_lifetime(ptr_t begin, ptr_t end)
{
	bz_assert(this->is_alive_bitset.is_none(begin - this->address, end - this->address));
	this->is_alive_bitset.set_range(begin - this->address, end - this->address, true);
}

void stack_object::end_lifetime(ptr_t begin, ptr_t end)
{
	bz_assert(this->is_alive_bitset.is_all(begin - this->address, end - this->address));
	this->is_alive_bitset.set_range(begin - this->address, end - this->address, false);
}

bool stack_object::is_alive(ptr_t begin, ptr_t end) const
{
	return this->is_alive_bitset.is_all(begin - this->address, end - this->address);
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
	  is_alive_bitset()
{
	auto const size = this->elem_type->size * this->count;
	this->memory = bz::fixed_vector<uint8_t>(size, 0);
	this->is_alive_bitset = bitset(size, false);
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
	this->is_alive_bitset.set_range(begin - this->address, end - this->address, true);
}

void heap_object::end_lifetime(ptr_t begin, ptr_t end)
{
	this->is_alive_bitset.set_range(begin - this->address, end - this->address, false);
}

bool heap_object::is_alive(ptr_t begin, ptr_t end) const
{
	return this->is_alive_bitset.is_all(begin - this->address, end - this->address);
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
	return "address points to a heap object outside its lifetime";
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

error_reason_t global_memory_manager::get_dereference_error_reason(ptr_t address, type const *object_type) const
{
	auto const object = this->get_global_object(address);
	bz_assert(object != nullptr);
	return { object->object_src_tokens, object->get_dereference_error_reason(address, object_type) };
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

pointer_arithmetic_result_t global_memory_manager::do_pointer_arithmetic(ptr_t address, int64_t offset, type const *object_type) const
{
	auto const object = this->get_global_object(address);
	if (object == nullptr)
	{
		return { 0, false };
	}
	else
	{
		return object->do_pointer_arithmetic(address, offset, object_type);
	}
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

error_reason_t stack_manager::get_dereference_error_reason(ptr_t address, type const *object_type) const
{
	auto const object = this->get_stack_object(address);
	bz_assert(object != nullptr);
	return { object->object_src_tokens, object->get_dereference_error_reason(address, object_type) };
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

pointer_arithmetic_result_t stack_manager::do_pointer_arithmetic(ptr_t address, int64_t offset, type const *object_type) const
{
	auto const object = this->get_stack_object(address);
	if (object == nullptr)
	{
		return { 0, false };
	}
	else
	{
		return object->do_pointer_arithmetic(address, offset, object_type);
	}
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
	this->object.is_alive_bitset.clear();
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

error_reason_t heap_manager::get_dereference_error_reason(ptr_t address, type const *object_type) const
{
	auto const allocation = this->get_allocation(address);
	bz_assert(allocation != nullptr);

	if (allocation->is_freed)
	{
		return { allocation->free_info.src_tokens, "address points into an allocation that was freed here" };
	}
	else
	{
		return { allocation->alloc_info.src_tokens, allocation->object.get_dereference_error_reason(address, object_type) };
	}
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

pointer_arithmetic_result_t heap_manager::do_pointer_arithmetic(ptr_t address, int64_t offset, type const *object_type) const
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

ptr_t memory_manager::get_non_meta_address(ptr_t address)
{
	auto segment = this->segment_info.get_segment(address);
	while (segment == memory_segment::meta)
	{
		address = this->meta_memory.get_real_address(address);
		segment = this->segment_info.get_segment(address);
	}

	return address;
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
	}

	return { address, segment, is_one_past_the_end, false };
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

error_reason_t memory_manager::get_dereference_error_reason(ptr_t address, type const *object_type) const
{
	bz_assert(address != 0);
	bool is_one_past_the_end = false;
	auto segment = this->segment_info.get_segment(address);
	while (segment == memory_segment::meta)
	{
		switch (this->meta_memory.segment_info.get_segment(address))
		{
		case meta_memory_segment::stack_object:
		{
			auto const &stack_object = this->meta_memory.get_stack_object_pointer(address);
			if (
				stack_object.stack_frame_depth >= this->stack.stack_frames.size()
				|| this->stack.stack_frames[stack_object.stack_frame_depth].id != stack_object.stack_frame_id
			)
			{
				return { stack_object.object_src_tokens, "address points to an object from a finished stack frame" };
			}

			address = stack_object.stack_address;
			break;
		}
		case meta_memory_segment::one_past_the_end:
			is_one_past_the_end = true;
			address = this->meta_memory.get_one_past_the_end_pointer(address).address;
			break;
		}

		segment = this->segment_info.get_segment(address);
	}

	if (is_one_past_the_end)
	{
		switch (segment)
		{
		case memory_segment::invalid:
			bz_unreachable;
		case memory_segment::global:
		{
			auto const global_object = this->global_memory->get_global_object(address);
			bz_assert(global_object != nullptr);
			return { global_object->object_src_tokens, "address is a one-past-the-end pointer into this global object" };
		}
		case memory_segment::stack:
		{
			auto const stack_object = this->stack.get_stack_object(address);
			bz_assert(stack_object != nullptr);
			return { stack_object->object_src_tokens, "address is a one-past-the-end pointer into this stack object" };
		}
		case memory_segment::heap:
		{
			auto const allocation = this->heap.get_allocation(address);
			bz_assert(allocation != nullptr);
			return { allocation->alloc_info.src_tokens, "address is a one-past-the-end pointer into this allocation" };
		}
		case memory_segment::meta:
			bz_unreachable;
		}
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

bz::u8string memory_manager::get_slice_construction_error_reason(ptr_t begin, ptr_t end, type const *elem_type) const
{
	bz_unreachable;
}

bz::optional<int> memory_manager::compare_pointers(ptr_t lhs, ptr_t rhs)
{
	auto const lhs_segment = this->segment_info.get_segment(lhs);
	auto const rhs_segment = this->segment_info.get_segment(rhs);

	if (lhs_segment == memory_segment::meta && rhs_segment == memory_segment::meta)
	{
		return this->compare_pointers(this->meta_memory.get_real_address(lhs), this->meta_memory.get_real_address(rhs));
	}
	else if (lhs_segment == memory_segment::meta)
	{
		return this->compare_pointers(this->meta_memory.get_real_address(lhs), rhs);
	}
	else if (rhs_segment == memory_segment::meta)
	{
		return this->compare_pointers(lhs, this->meta_memory.get_real_address(rhs));
	}
	else if (lhs_segment != rhs_segment)
	{
		return {};
	}
	else
	{
		// TODO: this should be checked more thoroughly
		switch (lhs_segment)
		{
		case memory_segment::invalid:
			return {};
		case memory_segment::global:
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
	case memory_segment::global:
	{
		auto const [result, is_one_past_the_end] = this->global_memory->do_pointer_arithmetic(address, offset, object_type);
		if (is_one_past_the_end)
		{
			return this->meta_memory.make_one_past_the_end_address(result);
		}
		else
		{
			return result;
		}
	}
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
		return this->do_pointer_arithmetic(this->meta_memory.get_real_address(address), offset, object_type);
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

	auto const lhs_segment = this->segment_info.get_segment(lhs);
	auto const rhs_segment = this->segment_info.get_segment(rhs);

	if (lhs_segment == memory_segment::meta && rhs_segment == memory_segment::meta)
	{
		return this->do_pointer_difference_unchecked(
			this->meta_memory.get_real_address(lhs),
			this->meta_memory.get_real_address(rhs),
			stride
		);
	}
	else if (lhs_segment == memory_segment::meta)
	{
		return this->do_pointer_difference_unchecked(this->meta_memory.get_real_address(lhs), rhs, stride);
	}
	else if (rhs_segment == memory_segment::meta)
	{
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

void memory_manager::start_lifetime(ptr_t address, size_t size)
{
	address = this->get_non_meta_address(address);
	bz_assert(this->segment_info.get_segment(address) == memory_segment::stack);

	auto const object = this->stack.get_stack_object(address);
	bz_assert(object != nullptr);
	bz_assert(address == object->address);
	object->start_lifetime(address, address + size);
}

void memory_manager::end_lifetime(ptr_t address, size_t size)
{
	address = this->get_non_meta_address(address);
	bz_assert(this->segment_info.get_segment(address) == memory_segment::stack);

	auto const object = this->stack.get_stack_object(address);
	bz_assert(object != nullptr);
	object->end_lifetime(address, address + size);
}

bool memory_manager::is_global(ptr_t address) const
{
	auto segment = this->segment_info.get_segment(address);
	while (segment == memory_segment::meta)
	{
		address = this->meta_memory.get_real_address(address);
		segment = this->segment_info.get_segment(address);
	}

	return segment == memory_segment::global;
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
