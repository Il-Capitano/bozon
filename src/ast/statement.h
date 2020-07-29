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

struct attribute
{
	lex::token_pos              name;
	lex::token_range            arg_tokens;
	bz::vector<ast::expression> args;

	declare_default_5(attribute)
};

enum class resolve_state : uint8_t
{
	none,
	symbol,
	all,
};

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


using statement_node_t = bz::meta::apply_type_pack<node, statement_types>;

struct statement : statement_node_t
{
	using base_t = statement_node_t;

	using base_t::node;
	using base_t::get;
	using base_t::kind;
	using base_t::emplace;

	declare_default_5(statement)

	bz::vector<attribute> _attributes;
	void set_attributes(bz::vector<attribute> attributes) noexcept
	{
		this->_attributes = std::move(attributes);
	}

	auto &get_attributes(void) noexcept
	{ return this->_attributes; }

	auto const &get_attributes(void) const noexcept
	{ return this->_attributes; }
};


struct stmt_if
{
	lex::token_range        tokens;
	expression              condition;
	statement               then_block;
	bz::optional<statement> else_block;

	declare_default_5(stmt_if)

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

	declare_default_5(stmt_while)

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

	declare_default_5(stmt_for)

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

	declare_default_5(stmt_return)

	stmt_return(lex::token_range _tokens, expression _expr)
		: tokens(_tokens), expr(std::move(_expr))
	{}
};

struct stmt_no_op
{
	lex::token_range tokens;

	declare_default_5(stmt_no_op)

	stmt_no_op(lex::token_range _tokens)
		: tokens(_tokens)
	{}
};

struct stmt_compound
{
	lex::token_range      tokens;
	bz::vector<statement> statements;

	declare_default_5(stmt_compound)

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

	declare_default_5(stmt_expression)

	stmt_expression(lex::token_range _tokens, expression _expr)
		: tokens(_tokens), expr(std::move(_expr))
	{}
};

struct stmt_static_assert
{
	lex::token_range arg_tokens;
	expression condition;
	expression message;

	declare_default_5(stmt_static_assert)

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

	declare_default_5(decl_variable)

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
		  var_type       (make_auto_typespec(nullptr)),
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
	bz::u8string                        function_name;
	bz::u8string                        symbol_name;
	llvm::Function                     *llvm_func;

	declare_default_5(function_body)

	bool is_symbol_resolved(void) const noexcept
	{ return this->llvm_func != nullptr; }

	bool not_symbol_resolved(void) const noexcept
	{ return this->llvm_func == nullptr; }

	bz::u8string get_symbol_name(void) const;
	static bz::u8string decode_symbol_name(
		bz::u8string_view::const_iterator &it,
		bz::u8string_view::const_iterator end
	);
	static bz::u8string decode_symbol_name(bz::u8string_view name)
	{
		auto it = name.begin();
		auto const end = name.end();
		return decode_symbol_name(it, end);
	}
};

struct decl_function
{
	lex::token_pos identifier;
	function_body  body;

	declare_default_5(decl_function)

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
			  "",
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
			  "",
			  nullptr
		  }
	{}
};

struct decl_operator
{
	lex::token_pos op;
	function_body  body;

	declare_default_5(decl_operator)

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
			  bz::format("{}", _op->kind),
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
			  bz::format("{}", _op->kind),
			  "",
			  nullptr
		  }
	{}
};

namespace type_info_flags
{

enum : uint32_t
{
	built_in     = bit_at<0>,
	resolved     = bit_at<1>,
	instantiable = bit_at<2>,

	default_zero_initialize = bit_at<3>,
	trivially_copyable      = bit_at<4>,
	trivially_movable       = bit_at<5>,
	trivially_destructible  = bit_at<6>,
};

static constexpr size_t default_built_in_flags =
	built_in | resolved | instantiable
	| default_zero_initialize | trivially_copyable | trivially_movable;

} // namespace type_info_flags

struct type_info
{
	enum : uint8_t
	{
		int8_, int16_, int32_, int64_,
		uint8_, uint16_, uint32_, uint64_,
		float32_, float64_,
		char_, str_,
		bool_, null_t_,

		aggregate,
	};

	uint8_t       kind;
	resolve_state resolve;
	uint32_t      flags;
	bz::u8string  symbol_name;

	bz::vector<function_body *> constructors;

	function_body *default_constructor;
	function_body *copy_constructor;
	function_body *move_constructor;
	function_body *destructor;
	function_body *move_destructor;

	declare_default_5(type_info)

	static type_info make_built_in(bz::u8string_view name, uint8_t kind)
	{
		return type_info{
			kind, resolve_state::all,
			type_info_flags::default_built_in_flags,
			bz::format("built_in.{}", name),
			{}, nullptr, nullptr, nullptr, nullptr, nullptr
		};
	}

	static bz::u8string_view decode_symbol_name(bz::u8string_view symbol_name)
	{
		constexpr bz::u8string_view built_in = "built_in.";
		constexpr bz::u8string_view struct_  = "struct.";
		if (symbol_name.starts_with(built_in))
		{
			return symbol_name.substring(built_in.length());
		}
		else
		{
			bz_assert(symbol_name.starts_with(struct_));
			return symbol_name.substring(struct_.length());
		}
	}

	static bz::u8string decode_symbol_name(
		bz::u8string_view::const_iterator &it,
		bz::u8string_view::const_iterator end
	)
	{
		auto const whole_str = bz::u8string_view(it, end);
		constexpr bz::u8string_view built_in = "built_in.";
		constexpr bz::u8string_view struct_  = "struct.";
		if (whole_str.starts_with(built_in))
		{
			static_assert(built_in.length() == built_in.size());
			auto const begin = bz::u8string_view::const_iterator(it.data() + built_in.size());
			auto const end = whole_str.find(begin, '.');
			it = end;
			return bz::u8string(begin, end);
		}
		else
		{
			bz_assert(whole_str.starts_with(struct_));
			static_assert(struct_.length() == struct_.size());
			auto const begin = bz::u8string_view::const_iterator(it.data() + struct_.size());
			auto const end = whole_str.find(begin, '.');
			it = end;
			return bz::u8string(begin, end);
		}
	}
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

	declare_default_5(decl_struct)
/*
	decl_struct(lex::token_pos _id)
		: identifier(_id),
		  info{
			  type_info::aggregate,
			  resolve_state::none,
			  0ull,
			  _id->value,
		  }
	{}
*/
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

#endif // AST_STATEMENT_H
