#include "global_memory.h"
#include "global_data.h"

namespace comptime::memory
{

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
	bz_assert(contained_in_object(this->object_type, offset, subobject_type));
	return true;
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

bz::vector<bz::u8string> global_object::get_pointer_arithmetic_error_reason(
	ptr_t address,
	bool is_one_past_the_end,
	[[maybe_unused]] int64_t offset,
	type const *object_type
) const
{
	if (object_type == this->object_type)
	{
		bz::vector<bz::u8string> result;
		if (is_one_past_the_end)
		{
			result.push_back("address is a one-past-the-end pointer to this global object");
			if (global_data::do_verbose)
			{
				result.push_back("the only valid offsets are -1 and 0");
			}
		}
		else
		{
			result.push_back("address points to this global object");
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
				"address is a one-past-the-end pointer to after the last element in an array of size {} in this global object",
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
				"address points to an element at index {} in an array of size {} in this global object",
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
			result.push_back("address is a one-past-the-end pointer to a subobject that is not in an array in this global object");
			if (global_data::do_verbose)
			{
				result.push_back("the only valid offsets are -1 and 0");
			}
		}
		else
		{
			result.push_back("address points to a subobject that is not in an array in this global object");
			if (global_data::do_verbose)
			{
				result.push_back("the only valid offsets are 0 and 1");
			}
		}
	}
	return result;
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

bz::vector<bz::u8string> global_object::get_pointer_difference_error_reason(
	ptr_t lhs,
	ptr_t rhs,
	bool lhs_is_one_past_the_end,
	bool rhs_is_one_past_the_end,
	type const *
) const
{
	auto const lhs_offset = lhs - this->address;
	auto const rhs_offset = rhs - this->address;

	bz::vector<bz::u8string> result;
	result.reserve(3);
	result.push_back("lhs and rhs addresses point to different subobjects in this global object");

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

copy_values_memory_t global_object::get_copy_source_memory(ptr_t address, size_t count, type const *elem_type)
{
	auto const begin_offset = address - this->address;
	auto const end_offset = begin_offset + count * elem_type->size;
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

bz::vector<bz::u8string> global_object::get_get_copy_source_memory_error_reasons(ptr_t address, size_t count, type const *elem_type)
{
	bz::vector<bz::u8string> result;

	auto const begin_offset = address - this->address;
	auto const end_offset = begin_offset + count * elem_type->size;
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
			"source address points to an invalid memory range in this global object with offset {} and element count {}",
			begin_offset, count
		));
	}

	return result;
}

global_memory_manager::global_memory_manager(ptr_t global_memory_begin)
	: segment_info{},
	  head(0),
	  one_past_the_end_pointers(),
	  objects()
{
	constexpr size_t segment_count = global_meta_segment_info_t::segment_count;
	constexpr size_t segment_size = 1u << 16;
	for (size_t i = 0; i < segment_count; ++i)
	{
		this->segment_info.segment_begins[i] = global_memory_begin + i * segment_size;
	}
	this->head = this->segment_info.get_segment_begin<global_meta_memory_segment::objects>();
}

ptr_t global_memory_manager::make_global_one_past_the_end_address(ptr_t address)
{
	auto const result_index = this->one_past_the_end_pointers.size();
	this->one_past_the_end_pointers.push_back({ address });
	return this->segment_info.get_segment_begin<global_meta_memory_segment::one_past_the_end>() + result_index;
}

global_memory_manager::one_past_the_end_pointer const &global_memory_manager::get_one_past_the_end_pointer(ptr_t address) const
{
	auto const index = address - this->segment_info.get_segment_begin<global_meta_memory_segment::one_past_the_end>();
	bz_assert(index < this->one_past_the_end_pointers.size());
	return this->one_past_the_end_pointers[index];
}

ptr_t global_memory_manager::make_unique_function_pointer(function *func)
{
	auto const result_index = this->function_pointers.size();
	bz_assert(this->function_pointers.is_all([func](auto const fp) { return fp.func != func; }));
	this->function_pointers.push_back({ func });
	return this->segment_info.get_segment_begin<global_meta_memory_segment::functions>() + result_index;
}

global_memory_manager::function_pointer const &global_memory_manager::get_function_pointer(ptr_t address) const
{
	auto const index = address - this->segment_info.get_segment_begin<global_meta_memory_segment::functions>();
	bz_assert(index < this->function_pointers.size());
	return this->function_pointers[index];
}

uint32_t global_memory_manager::add_object(lex::src_tokens const &object_src_tokens, type const *object_type, bz::fixed_vector<uint8_t> data)
{
	auto const result = static_cast<uint32_t>(this->objects.size());
	this->objects.emplace_back(object_src_tokens, this->head, object_type, std::move(data));
	this->head += object_type->size;
	this->head += (max_object_align - this->head % max_object_align);
	return result;
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
		result.append_move(messages.transform([begin_global_object](auto &message) {
			return error_reason_t{ begin_global_object->object_src_tokens, std::move(message) };
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
	auto const lhs_segment = this->segment_info.get_segment(lhs);
	bz_assert(this->segment_info.get_segment(rhs) == lhs_segment);

	switch (lhs_segment)
	{
	case global_meta_memory_segment::one_past_the_end:
		bz_unreachable;
	case global_meta_memory_segment::functions:
		if (lhs == rhs)
		{
			return 0;
		}
		else
		{
			return {};
		}
	case global_meta_memory_segment::objects:
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
	}
}

bz::vector<error_reason_t> global_memory_manager::get_compare_pointers_error_reason(ptr_t lhs, ptr_t rhs) const
{
	auto const lhs_segment = this->segment_info.get_segment(lhs);
	auto const rhs_segment = this->segment_info.get_segment(rhs);

	bz_assert(lhs_segment == rhs_segment && lhs_segment == global_meta_memory_segment::objects);
	auto const lhs_global_object = this->get_global_object(lhs);
	auto const rhs_global_object = this->get_global_object(rhs);
	bz_assert(lhs_global_object != nullptr);
	bz_assert(rhs_global_object != nullptr);
	bz_assert(lhs_global_object != rhs_global_object);

	bz::vector<error_reason_t> result;
	result.reserve(3);
	result.push_back({ {}, "lhs and rhs addresses point to different global objects" });
	result.push_back({ lhs_global_object->object_src_tokens, "lhs address points to this global object" });
	result.push_back({ rhs_global_object->object_src_tokens, "rhs address points to this global object" });
	return result;
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

bz::vector<error_reason_t> global_memory_manager::get_pointer_arithmetic_error_reason(
	ptr_t address,
	bool is_one_past_the_end,
	int64_t offset,
	type const *object_type
) const
{
	auto const object = this->get_global_object(address);
	bz_assert(object != nullptr);
	auto messages = object->get_pointer_arithmetic_error_reason(address, is_one_past_the_end, offset, object_type);

	bz::vector<error_reason_t> result;
	result.reserve(messages.size());
	result.append_move(messages.transform([object](auto &message) {
		return error_reason_t{ object->object_src_tokens, std::move(message) };
	}));
	return result;
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

bz::vector<error_reason_t> global_memory_manager::get_pointer_difference_error_reason(
	ptr_t lhs,
	ptr_t rhs,
	bool lhs_is_one_past_the_end,
	bool rhs_is_one_past_the_end,
	type const *object_type
) const
{
	auto const lhs_object = this->get_global_object(lhs);
	auto const rhs_object = this->get_global_object(rhs);
	bz_assert(lhs_object != nullptr);
	bz_assert(rhs_object != nullptr);

	bz::vector<error_reason_t> result;
	if (lhs_object != rhs_object)
	{
		result.reserve(3);
		result.push_back({ {}, "lhs and rhs addresses point to different global objects" });
		result.push_back({ lhs_object->object_src_tokens, "lhs address points to this global object" });
		result.push_back({ rhs_object->object_src_tokens, "rhs address points to this global object" });
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

} // namespace comptime::memory
