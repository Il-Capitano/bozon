#include "types.h"

namespace comptime
{

type_set_t::type_set_t(size_t pointer_size)
	: aggregate_map(),
	  array_map(),
	  aggregate_and_array_types(),
	  builtin_types{
		  type(builtin_type{ builtin_type_kind::i1 }, 1, 1),
		  type(builtin_type{ builtin_type_kind::i8 }, 1, 1),
		  type(builtin_type{ builtin_type_kind::i16 }, 2, 2),
		  type(builtin_type{ builtin_type_kind::i32 }, 4, 4),
		  type(builtin_type{ builtin_type_kind::i64 }, 8, 8),
		  type(builtin_type{ builtin_type_kind::f32 }, 4, 4),
		  type(builtin_type{ builtin_type_kind::f64 }, 8, 8),
		  type(builtin_type{ builtin_type_kind::void_ }, 0, 0),
	  },
	  pointer(pointer_type(), pointer_size, pointer_size)
{}

type const *type_set_t::get_builtin_type(builtin_type_kind kind)
{
	bz_assert(static_cast<uint8_t>(kind) < this->builtin_types.size());
	return &this->builtin_types[static_cast<uint8_t>(kind)];
}

type const *type_set_t::get_pointer_type(void)
{
	return &this->pointer;
}

static size_t round_up(size_t value, size_t align)
{
	if (align <= 1)
	{
		return value;
	}

	bz_assert(std::popcount(align) == 1);
	auto const rem = value & (align - 1);
	if (rem == 0)
	{
		return value;
	}
	else
	{
		return value - rem + align;
	}
}

struct type_size_info
{
	bz::vector<size_t> offsets;
	size_t size;
	size_t align;
};

static type_size_info get_type_size_info(bz::array_view<type const * const> elem_types)
{
	bz::vector<size_t> offsets = {};
	offsets.reserve(elem_types.size());
	size_t size = 0;
	size_t align = 0;

	for (auto const type : elem_types)
	{
		align = std::max(align, type->align);
		size = round_up(size, type->align);
		offsets.push_back(size);
		size += type->size;
	}

	size = round_up(size, align);
	return { std::move(offsets), size, align };
}

type const *type_set_t::get_aggregate_type(bz::array_view<type const * const> elem_types)
{
	auto const it = this->aggregate_map.find(elem_types);
	if (it != this->aggregate_map.end())
	{
		return it->second;
	}

	auto [offsets, size, align] = get_type_size_info(elem_types);
	auto &new_type = this->aggregate_and_array_types.emplace_back(aggregate_type{ elem_types, std::move(offsets) }, size, align);
	this->aggregate_map.insert({ elem_types, &new_type });
	return &new_type;
}

type const *type_set_t::get_array_type(type const *elem_type, size_t size)
{
	auto const array_t = array_type{ elem_type, size };
	auto const it = this->array_map.find(array_t);
	if (it != this->array_map.end())
	{
		return it->second;
	}

	auto &new_type = this->aggregate_and_array_types.emplace_back(array_t, size * elem_type->size, elem_type->align);
	this->array_map.insert({ array_t, &new_type });
	return &new_type;
}

} // namespace comptime