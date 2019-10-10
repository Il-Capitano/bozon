#ifndef AST_TYPE_H
#define AST_TYPE_H

#include "core.h"

#include "lexer.h"


struct ast_typespec;
using ast_typespec_ptr = std::shared_ptr<ast_typespec>;


struct ast_ts_unresolved
{
	token_range typespec;

	ast_ts_unresolved(token_range _typespec)
		: typespec(_typespec)
	{}
};

struct ast_ts_name
{
	intern_string name;

	ast_ts_name(intern_string _name)
		: name(_name)
	{}
};

struct ast_ts_constant
{
	ast_typespec_ptr base;

	ast_ts_constant(ast_typespec_ptr _base)
		: base(_base)
	{}
};

struct ast_ts_pointer
{
	ast_typespec_ptr base;

	ast_ts_pointer(ast_typespec_ptr _base)
		: base(_base)
	{}
};

struct ast_ts_reference
{
	ast_typespec_ptr base;

	ast_ts_reference(ast_typespec_ptr _base)
		: base(_base)
	{}
};

struct ast_ts_function
{
	ast_typespec_ptr             return_type;
	bz::vector<ast_typespec_ptr> argument_types;

	ast_ts_function(
		ast_typespec_ptr             _ret_type,
		bz::vector<ast_typespec_ptr> _arg_types
	)
		: return_type   (_ret_type),
		  argument_types(std::move(_arg_types))
	{}
};

struct ast_ts_tuple
{
	bz::vector<ast_typespec_ptr> types;

	ast_ts_tuple(bz::vector<ast_typespec_ptr> _types)
		: types(std::move(_types))
	{}
};

struct ast_ts_none
{
	// nothing
};


struct ast_typespec : bz::variant<
	ast_ts_unresolved,
	ast_ts_name,
	ast_ts_constant,
	ast_ts_pointer,
	ast_ts_reference,
	ast_ts_function,
	ast_ts_tuple,
	ast_ts_none
>
{
	using base_t = bz::variant<
		ast_ts_unresolved,
		ast_ts_name,
		ast_ts_constant,
		ast_ts_pointer,
		ast_ts_reference,
		ast_ts_function,
		ast_ts_tuple,
		ast_ts_none
	>;

	enum : uint32_t
	{
		unresolved = index_of<ast_ts_unresolved>,
		name       = index_of<ast_ts_name>,
		constant   = index_of<ast_ts_constant>,
		pointer    = index_of<ast_ts_pointer>,
		reference  = index_of<ast_ts_reference>,
		function   = index_of<ast_ts_function>,
		tuple      = index_of<ast_ts_tuple>,
		none       = index_of<ast_ts_none>,
	};

	using base_t::get;
	using base_t::variant;

	void resolve(void);

	uint32_t kind(void) const
	{ return base_t::index(); }

	inline bool equals(ast_typespec_ptr rhs) const;
};

struct ast_variable
{
	intern_string    id;
	ast_typespec_ptr type;
};


inline bool ast_typespec::equals(ast_typespec_ptr rhs) const
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
		return this->equals(rhs->get<constant>().base);
	case pointer:
		return this->equals(rhs->get<pointer>().base);
	case reference:
		return this->equals(rhs->get<reference>().base);
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
inline ast_typespec_ptr make_ast_typespec(Args &&...args)
{
	return std::make_unique<ast_typespec>(std::forward<Args>(args)...);
}

template<typename ...Args>
ast_typespec_ptr make_ast_unresolved_typespec(Args &&...args)
{
	return make_ast_typespec(ast_ts_unresolved(std::forward<Args>(args)...));
}

template<typename ...Args>
ast_typespec_ptr make_ast_name_typespec(Args &&...args)
{
	return make_ast_typespec(ast_ts_name(std::forward<Args>(args)...));
}

template<typename ...Args>
ast_typespec_ptr make_ast_constant_typespec(Args &&...args)
{
	return make_ast_typespec(ast_ts_constant(std::forward<Args>(args)...));
}

template<typename ...Args>
ast_typespec_ptr make_ast_pointer_typespec(Args &&...args)
{
	return make_ast_typespec(ast_ts_pointer(std::forward<Args>(args)...));
}

template<typename ...Args>
ast_typespec_ptr make_ast_reference_typespec(Args &&...args)
{
	return make_ast_typespec(ast_ts_reference(std::forward<Args>(args)...));
}

template<typename ...Args>
ast_typespec_ptr make_ast_function_typespec(Args &&...args)
{
	return make_ast_typespec(ast_ts_function(std::forward<Args>(args)...));
}

template<typename ...Args>
ast_typespec_ptr make_ast_tuple_typespec(Args &&...args)
{
	return make_ast_typespec(ast_ts_tuple(std::forward<Args>(args)...));
}

template<typename ...Args>
ast_typespec_ptr make_ast_none_typespec(Args &&...args)
{
	return make_ast_typespec(ast_ts_none(std::forward<Args>(args)...));
}





#endif // AST_TYPE_H
