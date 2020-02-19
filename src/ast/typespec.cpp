#include "typespec.h"

namespace ast
{

bool operator == (typespec const &lhs, typespec const &rhs)
{
	if (lhs.kind() != rhs.kind())
	{
		return false;
	}

	switch (lhs.kind())
	{
	case typespec::index<ts_base_type>:
		return lhs.get<ts_base_type_ptr>()->info
			== rhs.get<ts_base_type_ptr>()->info;

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

} // namespace ast
