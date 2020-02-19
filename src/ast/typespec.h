#ifndef AST_TYPESPEC_H
#define AST_TYPESPEC_H

#include "core.h"
#include "lex/lexer.h"

#include "node.h"

namespace ast
{

struct type_info;

#define declare_node_type(x) struct x; using x##_ptr = std::unique_ptr<x>

declare_node_type(ts_unresolved);
declare_node_type(ts_base_type);
declare_node_type(ts_constant);
declare_node_type(ts_pointer);
declare_node_type(ts_reference);
declare_node_type(ts_function);
declare_node_type(ts_tuple);

#undef declare_node_type

struct typespec : node<
	ts_unresolved,
	ts_base_type,
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
		ts_constant,
		ts_pointer,
		ts_reference,
		ts_function,
		ts_tuple
	>;

	using base_t::node;
	using base_t::get;
	using base_t::kind;
};

struct type;
using type_ptr = std::shared_ptr<type>;


struct ts_unresolved
{
	lex::token_range tokens;

	ts_unresolved(lex::token_range _tokens)
		: tokens(_tokens)
	{}

	ts_unresolved(ts_unresolved const &) = default;
	ts_unresolved(ts_unresolved &&) noexcept = default;
};

struct ts_base_type
{
	type_info const *info;

	ts_base_type(type_info const *_info)
		: info(_info)
	{}
};

struct ts_constant
{
	typespec base;

	ts_constant(typespec _base)
		: base(std::move(_base))
	{}

	ts_constant(ts_constant const &) = default;
	ts_constant(ts_constant &&) noexcept = default;
};

struct ts_pointer
{
	typespec base;

	ts_pointer(typespec _base)
		: base(std::move(_base))
	{}

	ts_pointer(ts_pointer const &) = default;
	ts_pointer(ts_pointer &&) noexcept = default;
};

struct ts_reference
{
	typespec base;

	ts_reference(typespec _base)
		: base(std::move(_base))
	{}

	ts_reference(ts_reference const &) = default;
	ts_reference(ts_reference &&) noexcept = default;
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

	ts_function(ts_function const &) = default;
	ts_function(ts_function &&) noexcept = default;
};

struct ts_tuple
{
	bz::vector<typespec> types;

	ts_tuple(bz::vector<typespec> _types)
		: types(std::move(_types))
	{}

	ts_tuple(ts_tuple const &) = default;
	ts_tuple(ts_tuple &&) noexcept = default;
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


struct type_info
{
	enum class type_kind
	{
		int8_, int16_, int32_, int64_,
		uint8_, uint16_, uint32_, uint64_,
		float32_, float64_,
		char_, str_,
		bool_, null_t_,
		void_,

		aggregate,
	};

	enum : size_t
	{
		built_in = 1ull,
	};

	type_kind  kind;
	bz::string identifier;
	size_t     size;
	size_t     alignment;
	size_t     flags;
	bz::vector<variable> member_variables;
};


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


typespec decay_typespec(typespec const &ts);

typespec add_lvalue_reference(typespec ts);
typespec add_const(typespec ts);

typespec remove_lvalue_reference(typespec ts);
typespec remove_const(typespec ts);


} // namespace ast


template<>
struct bz::formatter<ast::typespec>
{
	static bz::string format(ast::typespec const &typespec, const char *, const char *)
	{
		switch (typespec.kind())
		{
		case ast::typespec::null:
			return "<error-type>";

		case ast::typespec::index<ast::ts_base_type>:
			return bz::format("{}", typespec.get<ast::ts_base_type_ptr>()->info->identifier);

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
			return "<unresolved>";

		default:
			assert(false);
			return "";
		}
	}
};

#endif // AST_TYPESPEC_H
