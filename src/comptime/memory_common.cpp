#include "memory_common.h"

namespace comptime::memory
{

bool contained_in_object(type const *object_type, size_t offset, type const *subobject_type)
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

bool slice_contained_in_object(type const *object_type, size_t offset, type const *elem_type, size_t total_size, bool end_is_one_past_the_end)
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

pointer_arithmetic_check_result check_pointer_arithmetic(
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

type const *get_multi_dimensional_array_elem_type(type const *arr_type)
{
	while (arr_type->is_array())
	{
		arr_type = arr_type->get_array_element_type();
	}
	return arr_type;
}

bool is_native(endianness_kind endianness)
{
	return (endianness == endianness_kind::little) == (std::endian::native == std::endian::little);
}

} // namespace comptime::memory
