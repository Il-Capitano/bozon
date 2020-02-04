#include "type.h"
#include "../context.h"

namespace ast
{

aggregate_type::aggregate_type(bz::vector<variable> _members)
	: members(std::move(_members))
{
	size_t type_size = 0;
	size_t type_alignment = 0;

	if (this->members.size() == 0)
	{
		this->size = 0;
		this->alignment = 0;
		return;
	}

	for (auto &v : this->members)
	{
		auto var_size  = size_of(v.var_type);
		auto var_align = align_of(v.var_type);
		if (auto padding = type_size % var_align; padding != 0)
		{
			type_size += var_align - padding;
		}
		type_size += var_size;

		if (var_align > type_alignment)
		{
			type_alignment = var_align;
		}
	}

	if (auto padding = type_size % type_alignment; padding != 0)
	{
		type_size += type_alignment - padding;
	}

	this->size = type_size;
	this->alignment = type_alignment;
}


size_t size_of(typespec const &t)
{
	switch (t.kind())
	{
	case typespec::index<ts_base_type>:
		return size_of(t.get<ts_base_type_ptr>()->base_type);

	case typespec::index<ts_constant>:
		return size_of(t.get<ts_constant_ptr>()->base);

	case typespec::index<ts_pointer>:
		return 8;

	case typespec::index<ts_reference>:
		return size_of(t.get<ts_reference_ptr>()->base);

	case typespec::index<ts_function>:
		return 8;

	case typespec::index<ts_tuple>:
	{
		size_t size  = 0;
		size_t align = 0;
		auto &tuple = t.get<ts_tuple_ptr>();

		if (tuple->types.size() == 0)
		{
			return 0;
		}

		for (auto &ts : tuple->types)
		{
			auto ts_size  = size_of(ts);
			auto ts_align = align_of(ts);

			if (ts_align == 0)
			{
				continue;
			}

			if (auto padding = size % ts_align; padding != 0)
			{
				size += ts_align - padding;
			}
			size += ts_size;

			if (ts_align > align)
			{
				align = ts_align;
			}
		}

		if (align == 0)
		{
			return size;
		}

		if (auto padding = size % align; padding != 0)
		{
			size += align - padding;
		}

		return size;
	}
	default:
		assert(false);
		return 0;
	}
}

size_t align_of(typespec const &t)
{
	switch (t.kind())
	{
	case typespec::index<ts_base_type>:
		return align_of(t.get<ts_base_type_ptr>()->base_type);

	case typespec::index<ts_constant>:
		return align_of(t.get<ts_constant_ptr>()->base);

	case typespec::index<ts_pointer>:
		return 8;

	case typespec::index<ts_reference>:
		return align_of(t.get<ts_reference_ptr>()->base);

	case typespec::index<ts_function>:
		return 8;

	case typespec::index<ts_tuple>:
	{
		size_t align = 0;
		auto &tuple = t.get<ts_tuple_ptr>();

		if (tuple->types.size() == 0)
		{
			return 0;
		}

		for (auto &ts : tuple->types)
		{
			auto ts_align = align_of(ts);
			if (ts_align > align)
			{
				align = ts_align;
			}
		}

		return align;
	}
	default:
		assert(false);
		return 0;
	}
}

size_t size_of(type_ptr const &t)
{
	switch (t->kind())
	{
	case type::index_of<built_in_type>:
		return t->get<built_in_type>().size;
	case type::index_of<aggregate_type>:
		return t->get<aggregate_type>().size;
	default:
		assert(false);
		return 0;
	}
}

size_t align_of(type_ptr const &t)
{
	switch (t->kind())
	{
	case type::index_of<built_in_type>:
		return t->get<built_in_type>().alignment;
	case type::index_of<aggregate_type>:
		return t->get<aggregate_type>().alignment;
	default:
		assert(false);
		return 0;
	}
}

bool operator == (typespec const &lhs, typespec const &rhs)
{
	if (lhs.kind() != rhs.kind())
	{
		return false;
	}

	switch (lhs.kind())
	{
	case typespec::index<ts_base_type>:
		return lhs.get<ts_base_type_ptr>()->base_type->name
			== rhs.get<ts_base_type_ptr>()->base_type->name;

	case typespec::index<ts_constant>:
		return lhs.get<ts_constant_ptr>()->base == rhs.get<ts_constant_ptr>()->base;

	case typespec::index<ts_pointer>:
		return lhs.get<ts_pointer_ptr>()->base == rhs.get<ts_pointer_ptr>()->base;

	case typespec::index<ts_reference>:
		return lhs.get<ts_reference_ptr>()->base == rhs.get<ts_reference_ptr>()->base;

	case typespec::index<ts_function>:
	{
		auto &lhs_fn = lhs.get<ts_function_ptr>();
		auto &rhs_fn = rhs.get<ts_function_ptr>();

		if (
			lhs_fn->argument_types.size() != rhs_fn->argument_types.size()
			|| lhs_fn->return_type != rhs_fn->return_type
		)
		{
			return false;
		}

		for (size_t i = 0; i < lhs_fn->argument_types.size(); ++i)
		{
			if (lhs_fn->argument_types[i] != rhs_fn->argument_types[i])
			{
				return false;
			}
		}
		return true;
	}

	case typespec::index<ts_tuple>:
	{
		auto &lhs_tp = lhs.get<ts_tuple_ptr>();
		auto &rhs_tp = rhs.get<ts_tuple_ptr>();

		if (lhs_tp->types.size() != rhs_tp->types.size())
		{
			return false;
		}

		for (size_t i = 0; i < lhs_tp->types.size(); ++i)
		{
			if (lhs_tp->types[i] != rhs_tp->types[i])
			{
				return false;
			}
		}
		return true;
	}

	default:
		assert(false);
		return false;
	}
}

typespec decay_typespec(typespec const &ts)
{
	switch (ts.kind())
	{
	case typespec::index<ts_unresolved>:
		assert(false);
		return typespec();
	case typespec::index<ts_base_type>:
		return ts;
	case typespec::index<ts_constant>:
		return decay_typespec(ts.get<ts_constant_ptr>()->base);
	case typespec::index<ts_pointer>:
		return ts;
	case typespec::index<ts_reference>:
		return decay_typespec(ts.get<ts_reference_ptr>()->base);
	case typespec::index<ts_function>:
		return ts;
	case typespec::index<ts_tuple>:
	{
		auto &tuple = ts.get<ts_tuple_ptr>();
		bz::vector<typespec> decayed_types = {};
		for (auto &t : tuple->types)
		{
			decayed_types.push_back(decay_typespec(t));
		}
		return make_ts_tuple(std::move(decayed_types));
	}
	default:
		assert(false);
		return typespec();
	}
}

typespec add_lvalue_reference(typespec ts)
{
	if (ts.kind() == typespec::index<ts_reference>)
	{
		return ts;
	}
	else
	{
		return make_ts_reference(ts);
	}
}

typespec add_const(typespec ts)
{
	if (ts.kind() == typespec::index<ts_constant>)
	{
		return ts;
	}
	else
	{
		return make_ts_constant(ts);
	}
}

typespec remove_lvalue_reference(typespec ts)
{
	if (ts.kind() == typespec::index<ts_reference>)
	{
		return ts.get<ts_reference_ptr>()->base;
	}
	else
	{
		return ts;
	}
}

typespec remove_const(typespec ts)
{
	if (ts.kind() == typespec::index<ts_constant>)
	{
		return ts.get<ts_constant_ptr>()->base;
	}
	else
	{
		return ts;
	}
}

bool is_const(typespec const &ts)
{ return ts.kind() == typespec::index<ts_pointer>; }

bool is_reference(typespec const &ts)
{ return ts.kind() == typespec::index<ts_reference>; }

bool is_built_in_type(typespec const &ts)
{
	if (ts.kind() == typespec::index<ts_pointer>)
	{
		return true;
	}
	else if (ts.kind() == typespec::index<ts_base_type>)
	{
		auto &base_type = ts.get<ts_base_type_ptr>()->base_type;
		if (base_type->kind() == type::index_of<built_in_type>)
		{
			return true;
		}
	}

	return false;
}

bool is_integral_type(ast::typespec const &ts)
{
	if (ts.kind() != typespec::index<ts_base_type>)
	{
		return false;
	}
	auto &base_type = ts.get<ts_base_type_ptr>()->base_type;
	if (base_type->kind() != type::index_of<built_in_type>)
	{
		return false;
	}
	switch (base_type->get<built_in_type>().kind)
	{
	case built_in_type::int8_:
	case built_in_type::int16_:
	case built_in_type::int32_:
	case built_in_type::int64_:
	case built_in_type::uint8_:
	case built_in_type::uint16_:
	case built_in_type::uint32_:
	case built_in_type::uint64_:
		return true;
	default:
		return false;
	}
}

bool is_arithmetic_type(ast::typespec const &ts)
{
	if (ts.kind() != typespec::index<ts_base_type>)
	{
		return false;
	}
	auto &base_type = ts.get<ts_base_type_ptr>()->base_type;
	if (base_type->kind() != type::index_of<built_in_type>)
	{
		return false;
	}
	switch (base_type->get<built_in_type>().kind)
	{
	case built_in_type::int8_:
	case built_in_type::int16_:
	case built_in_type::int32_:
	case built_in_type::int64_:
	case built_in_type::uint8_:
	case built_in_type::uint16_:
	case built_in_type::uint32_:
	case built_in_type::uint64_:
	case built_in_type::float32_:
	case built_in_type::float64_:
		return true;
	default:
		return false;
	}
}

} // namespace ast
