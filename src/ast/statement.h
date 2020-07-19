#ifndef AST_STATEMENT_H
#define AST_STATEMENT_H

#include "core.h"

#include "node.h"
#include "expression.h"
#include "typespec.h"
#include "constant_value.h"

#include <llvm/IR/Function.h>

namespace ast
{

#define declare_node_type(x) struct x; using x##_ptr = std::unique_ptr<x>

declare_node_type(stmt_if);
declare_node_type(stmt_while);
declare_node_type(stmt_for);
declare_node_type(stmt_return);
declare_node_type(stmt_no_op);
declare_node_type(stmt_compound);
declare_node_type(stmt_static_assert);
declare_node_type(stmt_expression);

declare_node_type(decl_variable);
declare_node_type(decl_function);
declare_node_type(decl_operator);
declare_node_type(decl_struct);

#undef declare_node_type

using statement_types = bz::meta::type_pack<
	stmt_if,
	stmt_while,
	stmt_for,
	stmt_return,
	stmt_no_op,
	stmt_compound,
	stmt_static_assert,
	stmt_expression,
	decl_variable,
	decl_function,
	decl_operator,
	decl_struct
>;

using top_level_statement_types = bz::meta::type_pack<
	stmt_static_assert,
	decl_variable,
	decl_function,
	decl_operator,
	decl_struct
>;

using declaration_types = bz::meta::type_pack<
	decl_variable,
	decl_function,
	decl_operator,
	decl_struct
>;

template<typename T>
constexpr bool is_top_level_statement_type = []<typename ...Ts, typename ...Us>(
	bz::meta::type_pack<Ts...>,
	bz::meta::type_pack<Us...>
) {
	static_assert(bz::meta::is_in_types<T, Us...>);
	return bz::meta::is_in_types<T, Ts...>;
}(top_level_statement_types{}, statement_types{});

template<typename T>
constexpr bool is_declaration_type = []<typename ...Ts, typename ...Us>(
	bz::meta::type_pack<Ts...>,
	bz::meta::type_pack<Us...>
) {
	static_assert(bz::meta::is_in_types<T, Us...>);
	return bz::meta::is_in_types<T, Ts...>;
}(declaration_types{}, statement_types{});

using statement_node_t = typename internal::node_from_type_pack<statement_types>::type;

struct statement : statement_node_t
{
	using base_t = statement_node_t;

	using base_t::node;
	using base_t::get;
	using base_t::kind;
	using base_t::emplace;
};


struct stmt_if
{
	lex::token_range        tokens;
	expression              condition;
	statement               then_block;
	bz::optional<statement> else_block;

	stmt_if(
		lex::token_range _tokens,
		expression       _condition,
		statement        _then_block
	)
		: tokens    (_tokens),
		  condition (std::move(_condition)),
		  then_block(std::move(_then_block))
	{}

	stmt_if(
		lex::token_range _tokens,
		expression       _condition,
		statement        _then_block,
		statement        _else_block
	)
		: tokens    (_tokens),
		  condition (std::move(_condition)),
		  then_block(std::move(_then_block)),
		  else_block(std::move(_else_block))
	{}
};

struct stmt_while
{
	lex::token_range tokens;
	expression       condition;
	statement        while_block;

	stmt_while(
		lex::token_range _tokens,
		expression       _condition,
		statement        _while_block
	)
		: tokens     (_tokens),
		  condition  (std::move(_condition)),
		  while_block(std::move(_while_block))
	{}
};

struct stmt_for
{
	statement  init;
	expression condition;
	expression iteration;
	statement  for_block;

	stmt_for(
		statement  _init,
		expression _condition,
		expression _iteration,
		statement  _for_block
	)
		: init     (std::move(_init)),
		  condition(std::move(_condition)),
		  iteration(std::move(_iteration)),
		  for_block(std::move(_for_block))
	{}
};

struct stmt_return
{
	lex::token_range tokens;
	expression       expr;

	stmt_return(lex::token_range _tokens, expression _expr)
		: tokens(_tokens), expr(std::move(_expr))
	{}
};

struct stmt_no_op
{
	lex::token_range tokens;

	stmt_no_op(lex::token_range _tokens)
		: tokens(_tokens)
	{}
};

struct stmt_compound
{
	lex::token_range      tokens;
	bz::vector<statement> statements;

	stmt_compound(lex::token_range _tokens)
		: tokens(_tokens)
	{}

	stmt_compound(lex::token_range _tokens, bz::vector<statement> _stms)
		: tokens(_tokens), statements(std::move(_stms))
	{}
};

struct stmt_expression
{
	lex::token_range tokens;
	expression       expr;

	stmt_expression(lex::token_range _tokens, expression _expr)
		: tokens(_tokens), expr(std::move(_expr))
	{}
};

struct stmt_static_assert
{
	lex::token_range arg_tokens;
	expression condition;
	expression message;

	stmt_static_assert(lex::token_range _arg_tokens)
		: arg_tokens(_arg_tokens),
		  condition(),
		  message()
	{}
};


struct decl_variable
{
	lex::token_range tokens;
	lex::token_pos   identifier;
	lex::token_range prototype_range;
	typespec         var_type;
	expression       init_expr; // is null if there's no initializer

	decl_variable(
		lex::token_range _tokens,
		lex::token_pos   _id,
		lex::token_range _prototype_range,
		typespec         _var_type,
		expression       _init_expr
	)
		: tokens         (_tokens),
		  identifier     (_id),
		  prototype_range(_prototype_range),
		  var_type       (std::move(_var_type)),
		  init_expr      (std::move(_init_expr))
	{}

	decl_variable(
		lex::token_range _tokens,
		lex::token_pos   _id,
		lex::token_range _prototype_range,
		typespec         _var_type
	)
		: tokens         (_tokens),
		  identifier     (_id),
		  prototype_range(_prototype_range),
		  var_type       (std::move(_var_type)),
		  init_expr      ()
	{}

	decl_variable(
		lex::token_range _tokens,
		lex::token_pos   _id,
		lex::token_range _prototype_range,
		expression       _init_expr
	)
		: tokens         (_tokens),
		  identifier     (_id),
		  prototype_range(_prototype_range),
		  var_type       (make_ts_auto({}, nullptr)),
		  init_expr      (std::move(_init_expr))
	{}

	lex::token_pos get_tokens_begin(void) const;
	lex::token_pos get_tokens_pivot(void) const;
	lex::token_pos get_tokens_end(void) const;
};

struct function_body
{
	bz::vector<decl_variable>           params;
	typespec                            return_type;
	bz::optional<bz::vector<statement>> body;
	bz::u8string                        symbol_name;
	llvm::Function                     *llvm_func;
};

struct decl_function
{
	lex::token_pos identifier;
	function_body  body;

	decl_function(
		lex::token_pos            _id,
		bz::vector<decl_variable> _params,
		typespec                  _ret_type
	)
		: identifier (_id),
		  body{
			  std::move(_params),
			  std::move(_ret_type),
			  {},
			  _id->value,
			  nullptr
		  }
	{}

	decl_function(
		lex::token_pos            _id,
		bz::vector<decl_variable> _params,
		typespec                  _ret_type,
		bz::vector<statement>     _body
	)
		: identifier (_id),
		  body{
			  std::move(_params),
			  std::move(_ret_type),
			  std::move(_body),
			  _id->value,
			  nullptr
		  }
	{}
};

struct decl_operator
{
	lex::token_pos op;
	function_body  body;

	decl_operator(
		lex::token_pos            _op,
		bz::vector<decl_variable> _params,
		typespec                  _ret_type,
		bz::vector<statement>     _body
	)
		: op  (_op),
		  body{
			  std::move(_params),
			  std::move(_ret_type),
			  std::move(_body),
			  "",
			  nullptr
		  }
	{}

	decl_operator(
		lex::token_pos            _op,
		bz::vector<decl_variable> _params,
		typespec                  _ret_type
	)
		: op  (_op),
		  body{
			  std::move(_params),
			  std::move(_ret_type),
			  {},
			  "",
			  nullptr
		  }
	{}
};

struct type_info
{
	enum : uint32_t
	{
		int8_, int16_, int32_, int64_,
		uint8_, uint16_, uint32_, uint64_,
		float32_, float64_,
		char_, str_,
		bool_, null_t_,

		aggregate,
	};

	enum : size_t
	{
		built_in     = 1ull << 0,
		resolved     = 1ull << 1,
		instantiable = 1ull << 2,
	};

	static constexpr size_t default_built_in_flags =
		built_in | resolved | instantiable;

	uint32_t              kind;
	size_t                flags;
	bz::u8string_view     name;
	bz::vector<statement> member_decls;
};

namespace internal
{

template<uint32_t N>
struct type_from_type_info;

template<>
struct type_from_type_info<type_info::int8_>
{ using type = int8_t; };
template<>
struct type_from_type_info<type_info::int16_>
{ using type = int16_t; };
template<>
struct type_from_type_info<type_info::int32_>
{ using type = int32_t; };
template<>
struct type_from_type_info<type_info::int64_>
{ using type = int64_t; };

template<>
struct type_from_type_info<type_info::uint8_>
{ using type = uint8_t; };
template<>
struct type_from_type_info<type_info::uint16_>
{ using type = uint16_t; };
template<>
struct type_from_type_info<type_info::uint32_>
{ using type = uint32_t; };
template<>
struct type_from_type_info<type_info::uint64_>
{ using type = uint64_t; };

template<>
struct type_from_type_info<type_info::float32_>
{ using type = float32_t; };
template<>
struct type_from_type_info<type_info::float64_>
{ using type = float64_t; };


template<typename T>
struct type_info_from_type;

template<>
struct type_info_from_type<int8_t>
{ static constexpr auto value = ast::type_info::int8_; };
template<>
struct type_info_from_type<int16_t>
{ static constexpr auto value = ast::type_info::int16_; };
template<>
struct type_info_from_type<int32_t>
{ static constexpr auto value = ast::type_info::int32_; };
template<>
struct type_info_from_type<int64_t>
{ static constexpr auto value = ast::type_info::int64_; };

template<>
struct type_info_from_type<uint8_t>
{ static constexpr auto value = ast::type_info::uint8_; };
template<>
struct type_info_from_type<uint16_t>
{ static constexpr auto value = ast::type_info::uint16_; };
template<>
struct type_info_from_type<uint32_t>
{ static constexpr auto value = ast::type_info::uint32_; };
template<>
struct type_info_from_type<uint64_t>
{ static constexpr auto value = ast::type_info::uint64_; };

template<>
struct type_info_from_type<float32_t>
{ static constexpr auto value = ast::type_info::float32_; };
template<>
struct type_info_from_type<float64_t>
{ static constexpr auto value = ast::type_info::float64_; };

} // namespace internal

template<uint32_t N>
using type_from_type_info_t = typename internal::type_from_type_info<N>::type;
template<typename T>
constexpr auto type_info_from_type_v = internal::type_info_from_type<T>::value;



struct decl_struct
{
	lex::token_pos identifier;
	type_info      info;

	decl_struct(lex::token_pos _id, bz::vector<statement> _member_decls)
		: identifier(_id),
		  info{
			  type_info::aggregate,
			  0ull,
			  _id->value,
			  std::move(_member_decls)
		  }
	{}

	lex::token_pos get_tokens_begin(void) const;
	lex::token_pos get_tokens_pivot(void) const;
	lex::token_pos get_tokens_end(void) const;
};


template<typename ...Args>
statement make_statement(Args &&...args)
{ return statement(std::forward<Args>(args)...); }


#define def_make_fn(ret_type, node_type)                                       \
template<typename ...Args>                                                     \
ret_type make_ ## node_type (Args &&...args)                                   \
{ return ret_type(std::make_unique<node_type>(std::forward<Args>(args)...)); }

def_make_fn(statement, decl_variable)
def_make_fn(statement, decl_function)
def_make_fn(statement, decl_operator)
def_make_fn(statement, decl_struct)

def_make_fn(statement, stmt_if)
def_make_fn(statement, stmt_while)
def_make_fn(statement, stmt_for)
def_make_fn(statement, stmt_return)
def_make_fn(statement, stmt_no_op)
def_make_fn(statement, stmt_compound)
def_make_fn(statement, stmt_expression)
def_make_fn(statement, stmt_static_assert)

#undef def_make_fn

} // namespace ast

template<>
struct bz::formatter<ast::typespec>
{
	static bz::u8string format(ast::typespec const &typespec, u8string_view)
	{
		switch (typespec.kind())
		{
		case ast::typespec::index<ast::ts_base_type>:
			return bz::format("{}", typespec.get<ast::ts_base_type_ptr>()->info->name);

		case ast::typespec::index<ast::ts_void>:
			return "void";

		case ast::typespec::index<ast::ts_constant>:
			return bz::format("const {}", typespec.get<ast::ts_constant_ptr>()->base);

		case ast::typespec::index<ast::ts_consteval>:
			return bz::format("consteval {}", typespec.get<ast::ts_consteval_ptr>()->base);

		case ast::typespec::index<ast::ts_pointer>:
			return bz::format("*{}", typespec.get<ast::ts_pointer_ptr>()->base);

		case ast::typespec::index<ast::ts_reference>:
			return bz::format("&{}", typespec.get<ast::ts_reference_ptr>()->base);

		case ast::typespec::index<ast::ts_function>:
		{
			auto &fn = typespec.get<ast::ts_function_ptr>();
			bz::u8string res = "function(";

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
			bz::u8string res = "[";

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

		case ast::typespec::index<ast::ts_auto>:
			return "auto";

		case ast::typespec::index<ast::ts_unresolved>:
			return "<unresolved>";

		default:
			return "<error-type>";
		}
	}
};

#endif // AST_STATEMENT_H
