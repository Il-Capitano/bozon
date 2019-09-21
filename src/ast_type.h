#ifndef AST_TYPE_H
#define AST_TYPE_H

#include "core.h"

#include "first_pass_parser.h"

namespace type
{

}

struct ast_function_type;
using ast_function_type_ptr = std::shared_ptr<ast_function_type>;


struct ast_typespec;
using ast_typespec_ptr = std::shared_ptr<ast_typespec>;

struct ast_typespec : bz::variant<
	intern_string,
	ast_typespec_ptr,
	ast_typespec_ptr,
	ast_typespec_ptr,
	ast_function_type_ptr
>
{
	using base_t = bz::variant<
		intern_string,
		ast_typespec_ptr,
		ast_typespec_ptr,
		ast_typespec_ptr,
		ast_function_type_ptr
	>;

	enum : uint32_t
	{
		name      = 0,
		constant  = 1,
		pointer   = 2,
		reference = 3,
		function  = 4,
	};

	using base_t::get;

	ast_typespec(base_t &&b)
		: base_t(std::move(b))
	{}

	inline bool equals(ast_typespec_ptr rhs) const;

	uint32_t kind(void) const
	{ return this->index(); }
};


struct ast_function_type
{
	ast_typespec_ptr             return_type;
	bz::vector<ast_typespec_ptr> argument_types;

	ast_function_type(
		ast_typespec_ptr             _ret_type,
		bz::vector<ast_typespec_ptr> _arg_types
	)
		: return_type   (std::move(_ret_type)),
		  argument_types(std::move(_arg_types))
	{}
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
		return this->get<name>() == rhs->get<name>();
	case constant:
		return this->equals(rhs->get<constant>());
	case pointer:
		return this->equals(rhs->get<pointer>());
	case reference:
		return this->equals(rhs->get<reference>());
	case function:
	{
		auto &lhs_fn_ret = this->get<function>()->return_type;
		auto &rhs_fn_ret = rhs->get<function>()->return_type;
		auto &lhs_fn_arg = this->get<function>()->argument_types;
		auto &rhs_fn_arg = rhs->get<function>()->argument_types;

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

template<size_t N, typename... Args>
inline ast_typespec_ptr make_ast_typespec(Args &&... args)
{
	return std::make_unique<ast_typespec>(
		ast_typespec::make<N>(std::forward<Args>(args)...)
	);
}

template<typename ...Args>
inline ast_function_type_ptr make_ast_function_type(Args &&...args)
{
	return std::make_shared<ast_function_type>(std::forward<Args>(args)...);
}

ast_typespec_ptr parse_ast_typespec(token_range type);

ast_typespec_ptr parse_ast_typespec(
	src_tokens::pos &stream,
	src_tokens::pos  end
);

#endif // AST_TYPE_H
