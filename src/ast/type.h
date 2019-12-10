#ifndef TYPE_H
#define TYPE_H

#include "../core.h"
#include "../lexer.h"

#include "node.h"

namespace ast
{

#define declare_node_type(x) struct x; using x##_ptr = std::unique_ptr<x>

declare_node_type(ts_unresolved);
declare_node_type(ts_base_type);
declare_node_type(ts_constant);
declare_node_type(ts_pointer);
declare_node_type(ts_reference);
declare_node_type(ts_function);
declare_node_type(ts_tuple);

#undef declare_node_type

using typespec = node<
	ts_unresolved,
	ts_base_type,
	ts_constant,
	ts_pointer,
	ts_reference,
	ts_function,
	ts_tuple
>;

template<>
void typespec::resolve(void);

struct type;
using type_ptr = std::shared_ptr<type>;


struct ts_unresolved
{
	token_range tokens;

	ts_unresolved(token_range _tokens)
		: tokens(_tokens)
	{}
};

struct ts_base_type
{
	type_ptr    base_type;

	ts_base_type(type_ptr _base_type)
		: base_type(_base_type)
	{}
};

struct ts_constant
{
	typespec base;

	ts_constant(typespec _base)
		: base(std::move(_base))
	{}
};

struct ts_pointer
{
	typespec base;

	ts_pointer(typespec _base)
		: base(std::move(_base))
	{}
};

struct ts_reference
{
	typespec base;

	ts_reference(typespec _base)
		: base(std::move(_base))
	{}
};

struct ts_function
{
	typespec             return_type;
	bz::vector<typespec> argument_types;

	ts_function(
		typespec             _ret_type,
		bz::vector<typespec> _arg_types
	)
		: return_type   (std::move(_ret_type)),
		  argument_types(std::move(_arg_types))
	{}
};

struct ts_tuple
{
	bz::vector<typespec> types;

	ts_tuple(bz::vector<typespec> _types)
		: types(std::move(_types))
	{}
};


struct variable
{
	src_tokens::pos id;
	typespec        type;
};


struct built_in_type
{
	enum : uint32_t
	{
		int8_, int16_, int32_, int64_, // int128_,
		uint8_, uint16_, uint32_, uint64_, // uint128_,
		float32_, float64_,
		char_, bool_, str_, void_,
		null_t_,
	};

	uint32_t kind;
	size_t size;
	size_t alignment;
};


struct user_defined_type
{
	// TODO: for now, only PODS's are allowed, so no ctors, dtors, and so on...
	bz::vector<variable> members;
};


struct type : bz::variant<built_in_type, user_defined_type>
{
	using base_t = bz::variant<built_in_type, user_defined_type>;
	using base_t::variant;

	bz::string name;

	template<typename T>
	type(T &&_type, bz::string _name)
		: base_t(std::forward<T>(_type)), name(_name)
	{}

	auto kind(void) const
	{ return base_t::index(); }
};
using type_ptr = std::shared_ptr<type>;

#define def_built_in_type(type_name, size, alignment)              \
inline const auto type_name##_ = std::make_shared<type>(           \
    built_in_type{ built_in_type::type_name##_, size, alignment }, \
    #type_name                                                     \
)

def_built_in_type(int8, 1, 1);
def_built_in_type(int16, 2, 2);
def_built_in_type(int32, 4, 4);
def_built_in_type(int64, 8, 8);
// def_built_in_type(int128, 16, 16);
def_built_in_type(uint8, 1, 1);
def_built_in_type(uint16, 2, 2);
def_built_in_type(uint32, 4, 4);
def_built_in_type(uint64, 8, 8);
// def_built_in_type(uint128, 16, 16);
def_built_in_type(float32, 4, 4);
def_built_in_type(float64, 8, 8);
def_built_in_type(char, 4, 4);
def_built_in_type(bool, 1, 1);
def_built_in_type(str, 16, 8);
def_built_in_type(void, 0, 0);
def_built_in_type(null_t, 0, 0);

#undef def_built_in_type



inline bool operator == (typespec const &lhs, typespec const &rhs)
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

inline bool operator != (typespec const &lhs, typespec const &rhs)
{ return !(lhs == rhs); }


template<typename ...Args>
typespec make_ts_unresolved(Args &&...args)
{ return typespec(std::make_unique<ts_unresolved>(std::forward<Args>(args)...)); }

template<typename ...Args>
typespec make_ts_base_type(Args &&...args)
{ return typespec(std::make_unique<ts_base_type>(std::forward<Args>(args)...)); }

template<typename ...Args>
typespec make_ts_constant(Args &&...args)
{ return typespec(std::make_unique<ts_constant>(std::forward<Args>(args)...)); }

template<typename ...Args>
typespec make_ts_pointer(Args &&...args)
{ return typespec(std::make_unique<ts_pointer>(std::forward<Args>(args)...)); }

template<typename ...Args>
typespec make_ts_reference(Args &&...args)
{ return typespec(std::make_unique<ts_reference>(std::forward<Args>(args)...)); }

template<typename ...Args>
typespec make_ts_function(Args &&...args)
{ return typespec(std::make_unique<ts_function>(std::forward<Args>(args)...)); }

template<typename ...Args>
typespec make_ts_tuple(Args &&...args)
{ return typespec(std::make_unique<ts_tuple>(std::forward<Args>(args)...)); }



} // namespace ast

#endif // TYPE_H
