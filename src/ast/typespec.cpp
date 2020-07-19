#include "typespec.h"
#include "statement.h"

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
		return false;
	}
}

typespec decay_typespec(typespec const &ts)
{
	switch (ts.kind())
	{
	case typespec::index<ts_unresolved>:
		bz_assert(false);
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
		return make_ts_tuple(ts.src_tokens, tuple->pivot_pos, std::move(decayed_types));
	}
	default:
		bz_assert(false);
		return typespec();
	}
}

typespec const &remove_lvalue_reference(typespec const &ts)
{
	if (ts.is<ts_reference>())
	{
		return ts.get<ts_reference_ptr>()->base;
	}
	else
	{
		return ts;
	}
}

typespec const &remove_const(typespec const &ts)
{
	if (ts.is<ts_constant>())
	{
		return ts.get<ts_constant_ptr>()->base;
	}
	else
	{
		return ts;
	}
}

typespec &remove_const(typespec &ts)
{
	if (ts.is<ts_constant>())
	{
		return ts.get<ts_constant_ptr>()->base;
	}
	else
	{
		return ts;
	}
}

typespec const &remove_const_or_consteval(typespec const &ts)
{
	if (ts.is<ts_constant>())
	{
		return ts.get<ts_constant_ptr>()->base;
	}
	else if (ts.is<ts_consteval>())
	{
		return ts.get<ts_consteval_ptr>()->base;
	}
	else
	{
		return ts;
	}
}

typespec &remove_const_or_consteval(typespec &ts)
{
	if (ts.is<ts_constant>())
	{
		return ts.get<ts_constant_ptr>()->base;
	}
	else if (ts.is<ts_consteval>())
	{
		return ts.get<ts_consteval_ptr>()->base;
	}
	else
	{
		return ts;
	}
}

typespec const &remove_pointer(typespec const &ts)
{
	if (ts.is<ts_pointer>())
	{
		return ts.get<ts_pointer_ptr>()->base;
	}
	else
	{
		return ts;
	}
}

bool is_instantiable(typespec const &ts)
{
	switch (ts.kind())
	{
	case typespec::index<ts_base_type>:
		return ts.get<ts_base_type_ptr>()->info->flags & type_info::instantiable;
	case typespec::index<ts_void>:
		return false;
	case typespec::index<ts_constant>:
		return is_instantiable(ts.get<ts_constant_ptr>()->base);
	case typespec::index<ts_consteval>:
		return is_instantiable(ts.get<ts_consteval_ptr>()->base);
	case typespec::index<ts_pointer>:
		return true;
	case typespec::index<ts_reference>:
	{
		auto &base = ts.get<ts_reference_ptr>()->base;
		if (remove_const_or_consteval(base).is<ts_void>())
		{
			return false;
		}
		else
		{
			return true;
		}
	}
	case typespec::index<ts_function>:
		return true;
	case typespec::index<ts_tuple>:
	{
		auto &tup = *ts.get<ts_tuple_ptr>();
		for (auto &t : tup.types)
		{
			if (!is_instantiable(t))
			{
				return false;
			}
		}
		return true;
	}
	default:
		bz_assert(false);
		return false;
	}
}

} // namespace ast
