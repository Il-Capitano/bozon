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
	src_file::token_pos id;
	typespec            var_type;

	variable(src_file::token_pos _id, typespec _var_type)
		: id      (_id),
		  var_type(std::move(_var_type))
	{}
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
	size_t   size;
	size_t   alignment;

	built_in_type(uint32_t _kind, size_t _size, size_t _alignment)
		: kind(_kind), size(_size), alignment(_alignment)
	{}
};

struct aggregate_type
{
	// TODO: for now, only PODS's are allowed, so no ctors, dtors, and so on...
	bz::vector<variable> members;
	size_t size;
	size_t alignment;

	aggregate_type(bz::vector<variable> _members);
};

struct type : bz::variant<built_in_type, aggregate_type>
{
	using base_t = bz::variant<built_in_type, aggregate_type>;
	using base_t::variant;

	bz::string name;

	template<typename T>
	type(bz::string _name, T &&_type)
		: base_t(std::forward<T>(_type)),
		  name  (std::move(_name))
	{}

	auto kind(void) const
	{ return base_t::index(); }
};
using type_ptr = std::shared_ptr<type>;


size_t size_of(type_ptr const &t);
size_t align_of(type_ptr const &t);

size_t size_of(typespec const &t);
size_t align_of(typespec const &t);


bool operator == (typespec const &lhs, typespec const &rhs);

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


template<typename ...Args>
type_ptr make_type_ptr(Args &&...args)
{ return std::make_shared<type>(std::forward<Args>(args)...); }

template<typename ...Args>
type_ptr make_built_in_type_ptr(bz::string name, Args &&...args)
{ return std::make_shared<type>(std::move(name), built_in_type(std::forward<Args>(args)...)); }

template<typename ...Args>
type_ptr make_aggregate_type_ptr(bz::string name, Args &&...args)
{ return std::make_shared<type>(std::move(name), aggregate_type(std::forward<Args>(args)...)); }


#define def_built_in_type(type_name, size, alignment)        \
inline const auto type_name##_ = make_built_in_type_ptr(     \
    #type_name, built_in_type::type_name##_, size, alignment \
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
def_built_in_type(void, 0, 0);
def_built_in_type(null_t, 0, 0);
inline const auto str_ = make_aggregate_type_ptr(
	"str", bz::vector<variable>{
		variable(nullptr, make_ts_pointer(make_ts_base_type(void_))),
		variable(nullptr, make_ts_pointer(make_ts_base_type(void_)))
	}
);

#undef def_built_in_type


typespec decay_typespec(typespec const &ts);

inline typespec add_lvalue_reference(typespec ts)
{
	if (ts.kind() == typespec::index<ts_reference>)
	{
		return ts;
	}
	else
	{
		assert(ts.kind() != typespec::null);
		return make_ts_reference(ts);
	}
}

} // namespace ast


template<>
struct bz::formatter<ast::typespec>
{
	static bz::string format(ast::typespec const &typespec, const char *, const char *)
	{
		switch (typespec.kind())
		{
		case ast::typespec::index<ast::ts_base_type>:
			return bz::format("{}", typespec.get<ast::ts_base_type_ptr>()->base_type->name);

		case ast::typespec::index<ast::ts_constant>:
			return bz::format("const {}", typespec.get<ast::ts_constant_ptr>()->base);

		case ast::typespec::index<ast::ts_pointer>:
			return bz::format("*{}", typespec.get<ast::ts_pointer_ptr>()->base);

		case ast::typespec::index<ast::ts_reference>:
			return bz::format("&{}", typespec.get<ast::ts_reference_ptr>()->base);

		case ast::typespec::index<ast::ts_function>:
		{
			auto &fn = typespec.get<ast::ts_function_ptr>();
			bz::string res = "function(";

			bool put_comma = false;
			for (auto &type : fn->argument_types)
			{
				if (put_comma)
				{
					res += bz::format(", {}", type);
				}
				else
				{
					res += bz::format("{}", type);
					put_comma = true;
				}
			}

			res += bz::format(") -> {}", fn->return_type);

			return res;
		}

		case ast::typespec::index<ast::ts_tuple>:
		{
			auto &tuple = typespec.get<ast::ts_tuple_ptr>();
			bz::string res = "[";

			bool put_comma = false;
			for (auto &type : tuple->types)
			{
				if (put_comma)
				{
					res += bz::format(", {}", type);
				}
				else
				{
					res += bz::format("{}", type);
					put_comma = true;
				}
			}
			res += "]";

			return res;
		}

		case ast::typespec::index<ast::ts_unresolved>:
			assert(false);
			return "";

		default:
			assert(false);
			return "";
		}
	}
};

#endif // TYPE_H
