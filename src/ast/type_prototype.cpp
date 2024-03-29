#include "type_prototype.h"

namespace ast
{

bool is_integer_kind(builtin_type_kind kind)
{
	switch (kind)
	{
	case builtin_type_kind::i1:
	case builtin_type_kind::i8:
	case builtin_type_kind::i16:
	case builtin_type_kind::i32:
	case builtin_type_kind::i64:
		return true;
	default:
		return false;
	}
}

bool is_floating_point_kind(builtin_type_kind kind)
{
	switch (kind)
	{
	case builtin_type_kind::f32:
	case builtin_type_kind::f64:
		return true;
	default:
		return false;
	}
}

bz::u8string type_prototype::to_string(void) const
{
	if (this->is_builtin())
	{
		switch (this->get_builtin_kind())
		{
		case builtin_type_kind::i1:
			return "i1";
		case builtin_type_kind::i8:
			return "i8";
		case builtin_type_kind::i16:
			return "i16";
		case builtin_type_kind::i32:
			return "i32";
		case builtin_type_kind::i64:
			return "i64";
		case builtin_type_kind::f32:
			return "f32";
		case builtin_type_kind::f64:
			return "f64";
		case builtin_type_kind::void_:
			return "void";
		}
	}
	else if (this->is_pointer())
	{
		return "ptr";
	}
	else if (this->is_aggregate())
	{
		auto const types = this->get_aggregate_types();
		if (types.empty())
		{
			return "[]";
		}

		bz::u8string result = "[";
		result += types[0]->to_string();
		for (auto const type : types.slice(1))
		{
			result += ", ";
			result += type->to_string();
		}
		result += "]";
		return result;
	}
	else if (this->is_array())
	{
		return bz::format("[{}: {}]", this->get_array_size(), this->get_array_element_type()->to_string());
	}
	else
	{
		bz_unreachable;
	}
}

type_prototype_set_t::type_prototype_set_t(size_t pointer_size)
	: aggregate_map(),
	  array_map(),
	  aggregate_and_array_types(),
	  builtin_types{
		  type_prototype(builtin_type{ builtin_type_kind::i1 }, 1, 1),
		  type_prototype(builtin_type{ builtin_type_kind::i8 }, 1, 1),
		  type_prototype(builtin_type{ builtin_type_kind::i16 }, 2, 2),
		  type_prototype(builtin_type{ builtin_type_kind::i32 }, 4, 4),
		  type_prototype(builtin_type{ builtin_type_kind::i64 }, 8, 8),
		  type_prototype(builtin_type{ builtin_type_kind::f32 }, 4, 4),
		  type_prototype(builtin_type{ builtin_type_kind::f64 }, 8, 8),
		  type_prototype(builtin_type{ builtin_type_kind::void_ }, 0, 0),
	  },
	  pointer(pointer_type(), pointer_size, pointer_size)
{}

type_prototype const *type_prototype_set_t::get_builtin_type(builtin_type_kind kind)
{
	bz_assert(static_cast<uint8_t>(kind) < this->builtin_types.size());
	return &this->builtin_types[static_cast<uint8_t>(kind)];
}

type_prototype const *type_prototype_set_t::get_pointer_type(void)
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
		return value + align - rem;
	}
}

struct type_size_info
{
	bz::vector<size_t> offsets;
	size_t size;
	size_t align;
	bool has_padding;
};

static type_size_info get_type_size_info(bz::array_view<type_prototype const * const> elem_types)
{
	bz::vector<size_t> offsets = {};
	offsets.reserve(elem_types.size());
	size_t size = 0;
	size_t align = 0;
	bool has_padding = false;

	for (auto const type : elem_types)
	{
		has_padding |= type->has_padding();
		align = std::max(align, type->align);
		auto const new_size = round_up(size, type->align);
		has_padding |= (new_size != size);
		size = new_size;
		offsets.push_back(size);
		size += type->size;
	}

	auto const new_size = round_up(size, align);
	has_padding |= (new_size != size);
	size = new_size;
	if (size == 0)
	{
		size = 1;
		align = 1;
	}
	return { std::move(offsets), size, align, has_padding };
}

type_prototype const *type_prototype_set_t::get_aggregate_type(bz::array_view<type_prototype const * const> elem_types)
{
	auto const it = this->aggregate_map.find(elem_types);
	if (it != this->aggregate_map.end())
	{
		return it->second;
	}

	auto [offsets, size, align, has_padding] = get_type_size_info(elem_types);
	auto &new_type = this->aggregate_and_array_types.emplace_back(aggregate_type{ elem_types, std::move(offsets), has_padding }, size, align);
	this->aggregate_map.insert({ new_type.get_aggregate_types(), &new_type });
	return &new_type;
}

type_prototype const *type_prototype_set_t::get_array_type(type_prototype const *elem_type, size_t size)
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

} // namespace ast
