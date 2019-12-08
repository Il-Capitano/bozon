#ifndef TYPE_H
#define TYPE_H

#include "../core.h"
#include "../lexer.h"

namespace ast
{

struct typespec;
using typespec_ptr = std::shared_ptr<typespec>;


struct ts_unresolved
{
	token_range typespec;

	ts_unresolved(token_range _typespec)
		: typespec(_typespec)
	{}
};

struct ts_name
{
	intern_string name;

	ts_name(intern_string _name)
		: name(_name)
	{}
};

struct ts_constant
{
	typespec_ptr base;

	ts_constant(typespec_ptr _base)
		: base(_base)
	{}
};

struct ts_pointer
{
	typespec_ptr base;

	ts_pointer(typespec_ptr _base)
		: base(_base)
	{}
};

struct ts_reference
{
	typespec_ptr base;

	ts_reference(typespec_ptr _base)
		: base(_base)
	{}
};

struct ts_function
{
	typespec_ptr             return_type;
	bz::vector<typespec_ptr> argument_types;

	ts_function(
		typespec_ptr             _ret_type,
		bz::vector<typespec_ptr> _arg_types
	)
		: return_type   (_ret_type),
		  argument_types(std::move(_arg_types))
	{}
};

struct ts_tuple
{
	bz::vector<typespec_ptr> types;

	ts_tuple(bz::vector<typespec_ptr> _types)
		: types(std::move(_types))
	{}
};

struct ts_none
{
	// nothing
};


struct typespec : bz::variant<
	ts_unresolved,
	ts_name,
	ts_constant,
	ts_pointer,
	ts_reference,
	ts_function,
	ts_tuple,
	ts_none
>
{
	using base_t = bz::variant<
		ts_unresolved,
		ts_name,
		ts_constant,
		ts_pointer,
		ts_reference,
		ts_function,
		ts_tuple,
		ts_none
	>;

	enum : uint32_t
	{
		unresolved = index_of<ts_unresolved>,
		name       = index_of<ts_name>,
		constant   = index_of<ts_constant>,
		pointer    = index_of<ts_pointer>,
		reference  = index_of<ts_reference>,
		function   = index_of<ts_function>,
		tuple      = index_of<ts_tuple>,
		none       = index_of<ts_none>,
	};

	using base_t::get;
	using base_t::variant;

	void resolve(void);

	uint32_t kind(void) const
	{ return base_t::index(); }

	inline bool equals(typespec_ptr rhs) const;
};

struct variable
{
	intern_string    id;
	typespec_ptr type;
};


inline bool typespec::equals(typespec_ptr rhs) const
{
	if (this->kind() != rhs->kind())
	{
		return false;
	}

	switch (this->kind())
	{
	case name:
		return this->get<name>().name == rhs->get<name>().name;
	case constant:
		return this->get<constant>().base->equals(rhs->get<constant>().base);
	case pointer:
		return this->get<pointer>().base->equals(rhs->get<pointer>().base);
	case reference:
		return this->get<reference>().base->equals(rhs->get<reference>().base);
	case function:
	{
		auto &lhs_fn_ret = this->get<function>().return_type;
		auto &rhs_fn_ret = rhs->get<function>().return_type;
		auto &lhs_fn_arg = this->get<function>().argument_types;
		auto &rhs_fn_arg = rhs->get<function>().argument_types;

		if (!lhs_fn_ret->equals(rhs_fn_ret))
		{
			return false;
		}

		if (lhs_fn_arg.size() != rhs_fn_arg.size())
		{
			return false;
		}

		for (size_t i = 0; i < lhs_fn_arg.size(); ++i)
		{
			if (!lhs_fn_arg[i]->equals(rhs_fn_arg[i]))
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

template<typename ...Args>
inline typespec_ptr make_typespec(Args &&...args)
{
	return std::make_unique<typespec>(std::forward<Args>(args)...);
}

template<typename ...Args>
typespec_ptr make_unresolved_typespec(Args &&...args)
{
	return make_typespec(ts_unresolved(std::forward<Args>(args)...));
}

template<typename ...Args>
typespec_ptr make_name_typespec(Args &&...args)
{
	return make_typespec(ts_name(std::forward<Args>(args)...));
}

template<typename ...Args>
typespec_ptr make_constant_typespec(Args &&...args)
{
	return make_typespec(ts_constant(std::forward<Args>(args)...));
}

template<typename ...Args>
typespec_ptr make_pointer_typespec(Args &&...args)
{
	return make_typespec(ts_pointer(std::forward<Args>(args)...));
}

template<typename ...Args>
typespec_ptr make_reference_typespec(Args &&...args)
{
	return make_typespec(ts_reference(std::forward<Args>(args)...));
}

template<typename ...Args>
typespec_ptr make_function_typespec(Args &&...args)
{
	return make_typespec(ts_function(std::forward<Args>(args)...));
}

template<typename ...Args>
typespec_ptr make_tuple_typespec(Args &&...args)
{
	return make_typespec(ts_tuple(std::forward<Args>(args)...));
}

template<typename ...Args>
typespec_ptr make_none_typespec(Args &&...args)
{
	return make_typespec(ts_none(std::forward<Args>(args)...));
}

} // namespace ast

#endif // TYPE_H
