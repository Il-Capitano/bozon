#ifndef AST_TYPESPEC_H
#define AST_TYPESPEC_H

#include "core.h"
#include "lex/token.h"

#include "node.h"

namespace ast
{

struct type_info;

#define declare_node_type(x) struct x; using x##_ptr = std::unique_ptr<x>

declare_node_type(ts_unresolved);
declare_node_type(ts_base_type);
declare_node_type(ts_void);
declare_node_type(ts_constant);
declare_node_type(ts_pointer);
declare_node_type(ts_reference);
declare_node_type(ts_function);
declare_node_type(ts_tuple);

#undef declare_node_type

struct typespec : node<
	ts_unresolved,
	ts_base_type,
	ts_void,
	ts_constant,
	ts_pointer,
	ts_reference,
	ts_function,
	ts_tuple
>
{
	using base_t = node<
		ts_unresolved,
		ts_base_type,
		ts_void,
		ts_constant,
		ts_pointer,
		ts_reference,
		ts_function,
		ts_tuple
	>;

	using base_t::node;
	using base_t::get;
	using base_t::kind;

	lex::src_tokens src_tokens;

	template<typename ...Ts>
	typespec(lex::src_tokens _src_tokens, Ts &&...ts)
		: base_t(std::forward<Ts>(ts)...),
		  src_tokens(_src_tokens)
	{}

	typespec()
		: base_t(),
		  src_tokens{}
	{}

	lex::token_pos get_tokens_begin(void) const { return this->src_tokens.begin; }
	lex::token_pos get_tokens_pivot(void) const { return this->src_tokens.pivot; }
	lex::token_pos get_tokens_end(void)   const { return this->src_tokens.end;   }
};


struct ts_unresolved
{
	lex::token_range tokens;

	ts_unresolved(lex::token_range _tokens)
		: tokens(_tokens)
	{}

	ts_unresolved(ts_unresolved const &) = default;
	ts_unresolved(ts_unresolved &&) noexcept = default;

	lex::token_pos get_tokens_pivot(void) const
	{ return this->tokens.begin; }
};

struct ts_base_type
{
	lex::token_pos   src_pos;
	type_info *info;

	ts_base_type(lex::token_pos _src_pos, type_info *_info)
		: src_pos(_src_pos), info(_info)
	{}

	ts_base_type(type_info *_info)
		: src_pos(nullptr), info(_info)
	{}

	lex::token_pos get_tokens_pivot(void) const
	{ return this->src_pos; }
};

struct ts_void
{
	lex::token_pos src_pos;

	ts_void(lex::token_pos _src_pos)
		: src_pos(_src_pos)
	{}

	lex::token_pos get_tokens_pivot(void) const
	{ return this->src_pos; }
};

struct ts_constant
{
	lex::token_pos const_pos;
	typespec       base;

	ts_constant(lex::token_pos _const_pos, typespec _base)
		: const_pos(_const_pos), base(std::move(_base))
	{}

	ts_constant(typespec _base)
		: const_pos(nullptr), base(std::move(_base))
	{}

	ts_constant(ts_constant const &) = default;
	ts_constant(ts_constant &&) noexcept = default;

	lex::token_pos get_tokens_pivot(void) const
	{ return this->const_pos; }
};

struct ts_pointer
{
	lex::token_pos pointer_pos;
	typespec       base;

	ts_pointer(lex::token_pos _pointer_pos, typespec _base)
		: pointer_pos(_pointer_pos), base(std::move(_base))
	{}

	ts_pointer(typespec _base)
		: pointer_pos(nullptr), base(std::move(_base))
	{}

	ts_pointer(ts_pointer const &) = default;
	ts_pointer(ts_pointer &&) noexcept = default;

	lex::token_pos get_tokens_pivot(void) const
	{ return this->pointer_pos; }
};

struct ts_reference
{
	lex::token_pos reference_pos;
	typespec       base;

	ts_reference(lex::token_pos _reference_pos, typespec _base)
		: reference_pos(_reference_pos), base(std::move(_base))
	{}

	ts_reference(typespec _base)
		: reference_pos(nullptr), base(std::move(_base))
	{}

	ts_reference(ts_reference const &) = default;
	ts_reference(ts_reference &&) noexcept = default;

	lex::token_pos get_tokens_pivot(void) const
	{ return this->reference_pos; }
};

struct ts_function
{
	lex::token_pos       function_pos;
	typespec             return_type;
	bz::vector<typespec> argument_types;

	ts_function(
		lex::token_pos       _function_pos,
		typespec             _ret_type,
		bz::vector<typespec> _arg_types
	)
		: function_pos  (_function_pos),
		  return_type   (std::move(_ret_type)),
		  argument_types(std::move(_arg_types))
	{}

	ts_function(
		typespec             _ret_type,
		bz::vector<typespec> _arg_types
	)
		: function_pos  (nullptr),
		  return_type   (std::move(_ret_type)),
		  argument_types(std::move(_arg_types))
	{}

	ts_function(ts_function const &) = default;
	ts_function(ts_function &&) noexcept = default;

	lex::token_pos get_tokens_pivot(void) const
	{ return this->function_pos; }
};

struct ts_tuple
{
	lex::token_pos       pivot_pos;
	bz::vector<typespec> types;

	ts_tuple(lex::token_pos _pivot_pos, bz::vector<typespec> _types)
		: pivot_pos(_pivot_pos), types(std::move(_types))
	{}

	ts_tuple(ts_tuple const &) = default;
	ts_tuple(ts_tuple &&) noexcept = default;

	lex::token_pos get_tokens_pivot(void) const
	{ return this->pivot_pos; }
};

struct variable
{
	lex::token_pos id;
	typespec       var_type;

	variable(lex::token_pos _id, typespec _var_type)
		: id      (_id),
		  var_type(std::move(_var_type))
	{}
};


bool operator == (typespec const &lhs, typespec const &rhs);

inline bool operator != (typespec const &lhs, typespec const &rhs)
{ return !(lhs == rhs); }



template<typename ...Args>
typespec make_ts_unresolved(lex::src_tokens src_tokens, Args &&...args)
{ return typespec(src_tokens, std::make_unique<ts_unresolved>(std::forward<Args>(args)...)); }

template<typename ...Args>
typespec make_ts_base_type(lex::src_tokens src_tokens, Args &&...args)
{ return typespec(src_tokens, std::make_unique<ts_base_type>(std::forward<Args>(args)...)); }

template<typename ...Args>
typespec make_ts_void(lex::src_tokens src_tokens, Args &&...args)
{ return typespec(src_tokens, std::make_unique<ts_void>(std::forward<Args>(args)...)); }

template<typename ...Args>
typespec make_ts_constant(lex::src_tokens src_tokens, Args &&...args)
{ return typespec(src_tokens, std::make_unique<ts_constant>(std::forward<Args>(args)...)); }

template<typename ...Args>
typespec make_ts_pointer(lex::src_tokens src_tokens, Args &&...args)
{ return typespec(src_tokens, std::make_unique<ts_pointer>(std::forward<Args>(args)...)); }

template<typename ...Args>
typespec make_ts_reference(lex::src_tokens src_tokens, Args &&...args)
{ return typespec(src_tokens, std::make_unique<ts_reference>(std::forward<Args>(args)...)); }

template<typename ...Args>
typespec make_ts_function(lex::src_tokens src_tokens, Args &&...args)
{ return typespec(src_tokens, std::make_unique<ts_function>(std::forward<Args>(args)...)); }

template<typename ...Args>
typespec make_ts_tuple(lex::src_tokens src_tokens, Args &&...args)
{ return typespec(src_tokens, std::make_unique<ts_tuple>(std::forward<Args>(args)...)); }


typespec decay_typespec(typespec const &ts);

typespec add_lvalue_reference(typespec ts);
typespec add_const(typespec ts);

typespec const &remove_lvalue_reference(typespec const &ts);
typespec const &remove_const(typespec const &ts);
typespec &remove_const(typespec &ts);
typespec const &remove_pointer(typespec const &ts);
inline bool is_complete(typespec const &ts)
{
	switch (ts.kind())
	{
	case typespec::index<ts_base_type>:
	case typespec::index<ts_void>:
		return true;
	case typespec::index<ts_constant>:
		return is_complete(ts.get<ts_constant_ptr>()->base);
	case typespec::index<ts_pointer>:
		return is_complete(ts.get<ts_pointer_ptr>()->base);
	case typespec::index<ts_reference>:
		return is_complete(ts.get<ts_reference_ptr>()->base);
	case typespec::index<ts_function>:
	{
		auto &fn_t = *ts.get<ts_function_ptr>();
		for (auto &arg_t : fn_t.argument_types)
		{
			if (!is_complete(arg_t))
			{
				return false;
			}
		}
		return is_complete(fn_t.return_type);
	}
	case typespec::index<ts_tuple>:
	{
		auto &tup = *ts.get<ts_tuple_ptr>();
		for (auto &t : tup.types)
		{
			if (!is_complete(t))
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

bool is_instantiable(typespec const &ts);

} // namespace ast

#endif // AST_TYPESPEC_H
