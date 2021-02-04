#ifndef AST_STATEMENT_H
#define AST_STATEMENT_H

#include "core.h"

#include "node.h"
#include "expression.h"
#include "typespec.h"
#include "constant_value.h"
#include "abi/calling_conventions.h"

namespace ast
{

struct attribute
{
	lex::token_pos              name;
	lex::token_range            arg_tokens;
	bz::vector<ast::expression> args;

	declare_default_5(attribute)
};

enum class resolve_state : int8_t
{
	error = -1,
	none,
	resolving_parameters,
	parameters,
	resolving_symbol,
	symbol,
	resolving_all,
	all,
};

struct stmt_while;
struct stmt_for;
struct stmt_return;
struct stmt_no_op;
struct stmt_static_assert;
struct stmt_expression;

struct decl_variable;
struct decl_function;
struct decl_operator;
struct decl_function_alias;
struct decl_type_alias;
struct decl_struct;
struct decl_import;


using statement_types = bz::meta::type_pack<
	stmt_while,
	stmt_for,
	stmt_return,
	stmt_no_op,
	stmt_static_assert,
	stmt_expression,
	decl_variable,
	decl_function,
	decl_operator,
	decl_function_alias,
	decl_type_alias,
	decl_struct,
	decl_import
>;

using top_level_statement_types = bz::meta::type_pack<
	stmt_static_assert,
	decl_variable,
	decl_function,
	decl_operator,
	decl_function_alias,
	decl_type_alias,
	decl_struct,
	decl_import
>;

using declaration_types = bz::meta::type_pack<
	decl_variable,
	decl_function,
	decl_operator,
	decl_function_alias,
	decl_type_alias,
	decl_struct,
	decl_import
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
using statement_node_view_t = node_view_from_node<statement_node_t>;

struct statement : statement_node_t
{
	using base_t = statement_node_t;

	using base_t::node;
	using base_t::get;
	using base_t::kind;
	using base_t::emplace;

	declare_default_5(statement)

	bz::vector<attribute> _attributes;

	void set_attributes_without_resolve(bz::vector<attribute> attributes)
	{
		this->_attributes = std::move(attributes);
	}

	auto &get_attributes(void) noexcept
	{ return this->_attributes; }

	auto const &get_attributes(void) const noexcept
	{ return this->_attributes; }
};

struct statement_view : statement_node_view_t
{
	using base_t = statement_node_view_t;

	using base_t::node_view;
	using base_t::get;
	using base_t::kind;
	using base_t::emplace;

	declare_default_5(statement_view)

	bz::array_view<attribute> _attributes;

	statement_view(statement &stmt)
		: base_t(static_cast<statement_node_t &>(stmt)), _attributes(stmt._attributes)
	{}

	auto get_attributes(void) const noexcept
	{ return this->_attributes; }
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
	expression expr;

	declare_default_5(stmt_return)

	stmt_return(expression _expr)
		: expr(std::move(_expr))
	{}
};

struct stmt_no_op
{
	declare_default_5(stmt_no_op)
};

struct stmt_expression
{
	expression expr;

	declare_default_5(stmt_expression)

	stmt_expression(expression _expr)
		: expr(std::move(_expr))
	{}
};

struct stmt_static_assert
{
	lex::token_pos   static_assert_pos;
	lex::token_range arg_tokens;
	expression condition;
	expression message;

	declare_default_5(stmt_static_assert)

	stmt_static_assert(lex::token_pos _static_assert_pos, lex::token_range _arg_tokens)
		: static_assert_pos(_static_assert_pos),
		  arg_tokens(_arg_tokens),
		  condition(),
		  message()
	{}
};


struct decl_variable
{
	lex::src_tokens  src_tokens;
	lex::token_pos   identifier;
	lex::token_range prototype_range;
	typespec         var_type;
	expression       init_expr; // is null if there's no initializer
	resolve_state    state;
	bool             is_used;
	bool             is_export;

	declare_default_5(decl_variable)

	decl_variable(
		lex::src_tokens  _src_tokens,
		lex::token_pos   _id,
		lex::token_range _prototype_range,
		typespec         _var_type,
		expression       _init_expr
	)
		: src_tokens     (_src_tokens),
		  identifier     (_id),
		  prototype_range(_prototype_range),
		  var_type       (std::move(_var_type)),
		  init_expr      (std::move(_init_expr)),
		  state          (resolve_state::none),
		  is_used        (false),
		  is_export      (false)
	{}

	decl_variable(
		lex::src_tokens  _src_tokens,
		lex::token_pos   _id,
		lex::token_range _prototype_range,
		typespec         _var_type
	)
		: src_tokens     (_src_tokens),
		  identifier     (_id),
		  prototype_range(_prototype_range),
		  var_type       (std::move(_var_type)),
		  init_expr      (),
		  state          (resolve_state::none),
		  is_used        (false),
		  is_export      (false)
	{}

	decl_variable(
		lex::src_tokens  _src_tokens,
		lex::token_pos   _id,
		lex::token_range _prototype_range,
		expression       _init_expr
	)
		: src_tokens     (_src_tokens),
		  identifier     (_id),
		  prototype_range(_prototype_range),
		  var_type       (make_auto_typespec(nullptr)),
		  init_expr      (std::move(_init_expr)),
		  state          (resolve_state::none),
		  is_used        (false),
		  is_export      (false)
	{}

	lex::token_pos get_tokens_begin(void) const;
	lex::token_pos get_tokens_pivot(void) const;
	lex::token_pos get_tokens_end(void) const;
};

struct function_body
{
	using body_t = bz::variant<lex::token_range, bz::vector<statement>>;

	enum : uint32_t
	{
		module_export          = bit_at<0>,
		main                   = bit_at<1>,
		external_linkage       = bit_at<2>,
		intrinsic              = bit_at<3>,
		generic                = bit_at<4>,
		generic_specialization = bit_at<5>,
		reversed_args          = bit_at<6>,
	};

	enum : uint32_t
	{
		_builtin_first,

		builtin_str_eq = _builtin_first,
		builtin_str_neq,
		builtin_str_length,

		builtin_str_begin_ptr,
		builtin_str_end_ptr,
		builtin_str_size,
		builtin_str_from_ptrs,

		builtin_slice_begin_ptr,
		builtin_slice_begin_const_ptr,
		builtin_slice_end_ptr,
		builtin_slice_end_const_ptr,
		builtin_slice_size,
		builtin_slice_from_ptrs,
		builtin_slice_from_const_ptrs,

		print_stdout,
		println_stdout,
		print_stderr,
		println_stderr,

		// llvm intrinsics (https://releases.llvm.org/10.0.0/docs/LangRef.html#standard-c-library-intrinsics)
		// and other C standard library functions

		memcpy,
		memmove,
		memset,

		// C standard library math functions

		exp_f32,   exp_f64,
		exp2_f32,  exp2_f64,
		expm1_f32, expm1_f64,
		log_f32,   log_f64,
		log10_f32, log10_f64,
		log2_f32,  log2_f64,
		log1p_f32, log1p_f64,

		sqrt_f32,  sqrt_f64,
		pow_f32,   pow_f64,
		cbrt_f32,  cbrt_f64,
		hypot_f32, hypot_f64,

		sin_f32,   sin_f64,
		cos_f32,   cos_f64,
		tan_f32,   tan_f64,
		asin_f32,  asin_f64,
		acos_f32,  acos_f64,
		atan_f32,  atan_f64,
		atan2_f32, atan2_f64,

		sinh_f32,  sinh_f64,
		cosh_f32,  cosh_f64,
		tanh_f32,  tanh_f64,
		asinh_f32, asinh_f64,
		acosh_f32, acosh_f64,
		atanh_f32, atanh_f64,

		erf_f32,    erf_f64,
		erfc_f32,   erfc_f64,
		tgamma_f32, tgamma_f64,
		lgamma_f32, lgamma_f64,

		_builtin_last,
	};

	bz::vector<decl_variable> params;
	typespec                  return_type;
	body_t                    body;
	bz::u8string              function_name;
	bz::u8string              symbol_name;
	lex::src_tokens           src_tokens;
	resolve_state             state = resolve_state::none;
	abi::calling_convention   cc = abi::calling_convention::bozon;
	uint32_t                  flags = 0;
	uint32_t                  intrinsic_kind = 0;
	bz::vector<std::unique_ptr<function_body>>              generic_specializations;
	bz::vector<std::pair<lex::src_tokens, function_body *>> generic_required_from;

//	declare_default_5(function_body)
	function_body(void)             = default;
	function_body(function_body &&) = default;

	function_body(function_body const &other)
		: params        (other.params),
		  return_type   (other.return_type),
		  body          (other.body),
		  function_name (other.function_name),
		  symbol_name   (other.symbol_name),
		  src_tokens    (other.src_tokens),
		  state         (other.state),
		  cc            (other.cc),
		  flags         (other.flags),
		  intrinsic_kind(other.intrinsic_kind),
		  generic_specializations(),
		  generic_required_from(other.generic_required_from)
	{}

	bz::vector<statement> &get_statements(void) noexcept
	{
		bz_assert(this->body.is<bz::vector<ast::statement>>());
		return this->body.get<bz::vector<ast::statement>>();
	}

	bz::vector<statement> const &get_statements(void) const noexcept
	{
		bz_assert(this->body.is<bz::vector<ast::statement>>());
		return this->body.get<bz::vector<ast::statement>>();
	}

	bz::u8string get_signature(void) const;
	bz::u8string get_symbol_name(void) const;

	std::unique_ptr<function_body> get_copy_for_generic_specialization(bz::vector<std::pair<lex::src_tokens, function_body *>> required_from);
	function_body *add_specialized_body(std::unique_ptr<function_body> body);

	void resolve_symbol_name(void)
	{
		if (this->symbol_name == "")
		{
			this->symbol_name = this->get_symbol_name();
		}
	}

	bool is_external_linkage(void) const noexcept
	{
		return (this->flags & external_linkage) != 0;
	}

	bool is_main(void) const noexcept
	{
		return(this->flags & main) != 0;
	}

	bool is_export(void) const noexcept
	{
		return (this->flags & module_export) != 0;
	}

	bool is_intrinsic(void) const noexcept
	{
		return (this->flags & intrinsic) != 0;
	}

	bool is_generic(void) const noexcept
	{
		return (this->flags & generic) != 0;
	}

	bool is_generic_specialization(void) const noexcept
	{
		return (this->flags & generic_specialization) != 0;
	}

	bool is_reversed_args(void) const noexcept
	{
		return (this->flags & reversed_args) != 0;
	}

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
		lex::token_pos _id,
		function_body  _body
	)
		: identifier(_id),
		  body      (std::move(_body))
	{}
};

struct decl_operator
{
	lex::token_pos op;
	function_body  body;

	declare_default_5(decl_operator)

	decl_operator(
		lex::token_pos _op,
		function_body  _body
	)
		: op  (_op),
		  body(std::move(_body))
	{}
};

struct decl_function_alias
{
	lex::src_tokens             src_tokens;
	lex::token_pos              identifier;
	expression                  alias_expr;
	bz::vector<function_body *> aliased_bodies;
	resolve_state               state;
	bool                        is_export;

	declare_default_5(decl_function_alias)

	decl_function_alias(
		lex::src_tokens _src_tokens,
		lex::token_pos  _id,
		expression      _alias_expr
	)
		: src_tokens(_src_tokens),
		  identifier(_id),
		  alias_expr(std::move(_alias_expr)),
		  aliased_bodies{},
		  state(resolve_state::none),
		  is_export(false)
	{}
};

struct decl_type_alias
{
	lex::src_tokens src_tokens;
	lex::token_pos  identifier;
	expression      alias_expr;
	resolve_state   state;
	bool            is_export;

	decl_type_alias(
		lex::src_tokens _src_tokens,
		lex::token_pos  _id,
		expression      _alias_expr
	)
		: src_tokens(_src_tokens),
		  identifier(_id),
		  alias_expr(std::move(_alias_expr)),
		  state     (resolve_state::none),
		  is_export (false)
	{}

	typespec_view get_type(void) const
	{
		if (this->alias_expr.is_typename())
		{
			return this->alias_expr.get_typename();
		}
		else
		{
			return {};
		}
	}
};

struct member_variable 
{
	lex::src_tokens   src_tokens;
	bz::u8string_view identifier;
	typespec          type;
};

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
		forward_declaration,
	};

	lex::src_tokens  src_tokens;
	uint8_t          kind;
	resolve_state    state;
	bool             is_export;
	bz::u8string     type_name;
	bz::u8string     symbol_name;
	lex::token_range body_tokens;

	bz::vector<member_variable> member_variables;

	using function_body_ptr = std::unique_ptr<function_body>;
//	bz::vector<function_body_ptr> constructors;
//	function_body *default_constructor;
//	function_body *copy_constructor;
//	function_body *move_constructor;
//	function_body_ptr destructor;
//	function_body_ptr move_destuctor;

	type_info(lex::src_tokens _src_tokens, bz::u8string _type_name, lex::token_range range)
		: src_tokens(_src_tokens),
		  kind(range.begin == range.end ? forward_declaration : aggregate),
		  state(resolve_state::none),
		  is_export(false),
		  type_name(std::move(_type_name)),
		  symbol_name(),
		  body_tokens(range),
		  member_variables{}
//		  constructors{},
//		  default_constructor(nullptr),
//		  copy_constructor(nullptr),
//		  move_constructor(nullptr),
//		  destructor(nullptr),
//		  move_destuctor(nullptr)
	{}

private:
	type_info(bz::u8string_view name, uint8_t kind)
		: src_tokens{},
		  kind(kind),
		  state(resolve_state::all),
		  is_export(false),
		  symbol_name(bz::format("builtin.{}", name)),
		  body_tokens{},
		  member_variables{}
//		  constructors{},
//		  default_constructor(nullptr),
//		  copy_constructor(nullptr),
//		  move_constructor(nullptr),
//		  destructor(nullptr),
//		  move_destuctor(nullptr)
	{}
public:

	static type_info make_builtin(bz::u8string_view name, uint8_t kind)
	{
		return type_info(name, kind);
	}

	static bz::u8string_view decode_symbol_name(bz::u8string_view symbol_name)
	{
		constexpr bz::u8string_view builtin = "builtin.";
		constexpr bz::u8string_view struct_ = "struct.";
		if (symbol_name.starts_with(builtin))
		{
			return symbol_name.substring(builtin.length());
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
		constexpr bz::u8string_view builtin = "builtin.";
		constexpr bz::u8string_view struct_ = "struct.";
		if (whole_str.starts_with(builtin))
		{
			static_assert(builtin.length() == builtin.size());
			auto const begin = bz::u8string_view::const_iterator(it.data() + builtin.size());
			auto const dot = whole_str.find(begin, '.');
			it = dot;
			return bz::u8string(begin, dot);
		}
		else
		{
			bz_assert(whole_str.starts_with(struct_));
			static_assert(struct_.length() == struct_.size());
			auto const begin = bz::u8string_view::const_iterator(it.data() + struct_.size());
			auto const dot = whole_str.find(begin, '.');
			it = dot;
			return bz::u8string(begin, dot);
		}
	}
};

inline bz::u8string_view get_type_name_from_kind(uint32_t kind)
{
	switch (kind)
	{
	case ast::type_info::int8_:    return "int8";
	case ast::type_info::int16_:   return "int16";
	case ast::type_info::int32_:   return "int32";
	case ast::type_info::int64_:   return "int64";
	case ast::type_info::uint8_:   return "uint8";
	case ast::type_info::uint16_:  return "uint16";
	case ast::type_info::uint32_:  return "uint32";
	case ast::type_info::uint64_:  return "uint64";
	case ast::type_info::float32_: return "float32";
	case ast::type_info::float64_: return "float64";
	case ast::type_info::char_:    return "char";
	case ast::type_info::str_:     return "str";
	case ast::type_info::bool_:    return "bool";
	default: bz_unreachable;
	}
}

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
	using info_t = bz::variant<lex::token_range, type_info>;

	lex::token_pos  identifier;
	type_info       info;

//	declare_default_5(decl_struct)
	decl_struct(decl_struct const &) = delete;
	decl_struct(decl_struct &&)      = default;

	decl_struct(lex::src_tokens _src_tokens, lex::token_pos _id, lex::token_range _range)
		: identifier(_id),
		  info      (_src_tokens, _id->value, _range)
	{}

	decl_struct(lex::src_tokens _src_tokens, lex::token_pos _id)
		: identifier(_id),
		  info      (_src_tokens, _id->value, {})
	{}

	lex::token_pos get_tokens_begin(void) const;
	lex::token_pos get_tokens_pivot(void) const;
	lex::token_pos get_tokens_end(void) const;
};

struct decl_import
{
	lex::token_pos identifier;

	decl_import(lex::token_pos _id)
		: identifier(_id)
	{}

	declare_default_5(decl_import)
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
def_make_fn(statement, decl_function_alias)
def_make_fn(statement, decl_type_alias)
def_make_fn(statement, decl_struct)
def_make_fn(statement, decl_import)

def_make_fn(statement, stmt_while)
def_make_fn(statement, stmt_for)
def_make_fn(statement, stmt_return)
def_make_fn(statement, stmt_no_op)
def_make_fn(statement, stmt_expression)
def_make_fn(statement, stmt_static_assert)

#undef def_make_fn

type_info *get_builtin_type_info(uint32_t kind);
typespec_view get_builtin_type(bz::u8string_view name);
function_body *get_builtin_function(uint32_t kind);

struct intrinsic_info_t
{
	uint32_t kind;
	bz::u8string_view func_name;
};

constexpr auto intrinsic_info = []() {
	static_assert(function_body::_builtin_last - function_body::_builtin_first == 77);
	constexpr size_t size = function_body::_builtin_last - function_body::_builtin_first;
	return bz::array<intrinsic_info_t, size>{{
		{ function_body::builtin_str_eq,     "__builtin_str_eq"     },
		{ function_body::builtin_str_neq,    "__builtin_str_neq"    },
		{ function_body::builtin_str_length, "__builtin_str_length" },

		{ function_body::builtin_str_begin_ptr, "__builtin_str_begin_ptr" },
		{ function_body::builtin_str_end_ptr,   "__builtin_str_end_ptr"   },
		{ function_body::builtin_str_size,      "__builtin_str_size"      },
		{ function_body::builtin_str_from_ptrs, "__builtin_str_from_ptrs" },

		{ function_body::builtin_slice_begin_ptr,       "__builtin_slice_begin_ptr"       },
		{ function_body::builtin_slice_begin_const_ptr, "__builtin_slice_begin_const_ptr" },
		{ function_body::builtin_slice_end_ptr,         "__builtin_slice_end_ptr"         },
		{ function_body::builtin_slice_end_const_ptr,   "__builtin_slice_end_const_ptr"   },
		{ function_body::builtin_slice_size,            "__builtin_slice_size"            },
		{ function_body::builtin_slice_from_ptrs,       "__builtin_slice_from_ptrs"       },
		{ function_body::builtin_slice_from_const_ptrs, "__builtin_slice_from_const_ptrs" },

		{ function_body::print_stdout,   "__builtin_print_stdout"   },
		{ function_body::println_stdout, "__builtin_println_stdout" },
		{ function_body::print_stderr,   "__builtin_print_stderr"   },
		{ function_body::println_stderr, "__builtin_println_stderr" },

		// llvm intrinsics (https://releases.llvm.org/10.0.0/docs/LangRef.html#standard-c-library-intrinsics)
		// and other C standard library functions

		{ function_body::memcpy,  "__builtin_memcpy"  },
		{ function_body::memmove, "__builtin_memmove" },
		{ function_body::memset,  "__builtin_memset"  },

		// C standard library math functions

		{ function_body::exp_f32,   "__builtin_exp_f32"   },
		{ function_body::exp_f64,   "__builtin_exp_f64"   },
		{ function_body::exp2_f32,  "__builtin_exp2_f32"  },
		{ function_body::exp2_f64,  "__builtin_exp2_f64"  },
		{ function_body::expm1_f32, "__builtin_expm1_f32" },
		{ function_body::expm1_f64, "__builtin_expm1_f64" },
		{ function_body::log_f32,   "__builtin_log_f32"   },
		{ function_body::log_f64,   "__builtin_log_f64"   },
		{ function_body::log10_f32, "__builtin_log10_f32" },
		{ function_body::log10_f64, "__builtin_log10_f64" },
		{ function_body::log2_f32,  "__builtin_log2_f32"  },
		{ function_body::log2_f64,  "__builtin_log2_f64"  },
		{ function_body::log1p_f32, "__builtin_log1p_f32" },
		{ function_body::log1p_f64, "__builtin_log1p_f64" },

		{ function_body::sqrt_f32,  "__builtin_sqrt_f32"  },
		{ function_body::sqrt_f64,  "__builtin_sqrt_f64"  },
		{ function_body::pow_f32,   "__builtin_pow_f32"   },
		{ function_body::pow_f64,   "__builtin_pow_f64"   },
		{ function_body::cbrt_f32,  "__builtin_cbrt_f32"  },
		{ function_body::cbrt_f64,  "__builtin_cbrt_f64"  },
		{ function_body::hypot_f32, "__builtin_hypot_f32" },
		{ function_body::hypot_f64, "__builtin_hypot_f64" },

		{ function_body::sin_f32,   "__builtin_sin_f32"   },
		{ function_body::sin_f64,   "__builtin_sin_f64"   },
		{ function_body::cos_f32,   "__builtin_cos_f32"   },
		{ function_body::cos_f64,   "__builtin_cos_f64"   },
		{ function_body::tan_f32,   "__builtin_tan_f32"   },
		{ function_body::tan_f64,   "__builtin_tan_f64"   },
		{ function_body::asin_f32,  "__builtin_asin_f32"  },
		{ function_body::asin_f64,  "__builtin_asin_f64"  },
		{ function_body::acos_f32,  "__builtin_acos_f32"  },
		{ function_body::acos_f64,  "__builtin_acos_f64"  },
		{ function_body::atan_f32,  "__builtin_atan_f32"  },
		{ function_body::atan_f64,  "__builtin_atan_f64"  },
		{ function_body::atan2_f32, "__builtin_atan2_f32" },
		{ function_body::atan2_f64, "__builtin_atan2_f64" },

		{ function_body::sinh_f32,  "__builtin_sinh_f32"  },
		{ function_body::sinh_f64,  "__builtin_sinh_f64"  },
		{ function_body::cosh_f32,  "__builtin_cosh_f32"  },
		{ function_body::cosh_f64,  "__builtin_cosh_f64"  },
		{ function_body::tanh_f32,  "__builtin_tanh_f32"  },
		{ function_body::tanh_f64,  "__builtin_tanh_f64"  },
		{ function_body::asinh_f32, "__builtin_asinh_f32" },
		{ function_body::asinh_f64, "__builtin_asinh_f64" },
		{ function_body::acosh_f32, "__builtin_acosh_f32" },
		{ function_body::acosh_f64, "__builtin_acosh_f64" },
		{ function_body::atanh_f32, "__builtin_atanh_f32" },
		{ function_body::atanh_f64, "__builtin_atanh_f64" },

		{ function_body::erf_f32,    "__builtin_erf_f32"    },
		{ function_body::erf_f64,    "__builtin_erf_f64"    },
		{ function_body::erfc_f32,   "__builtin_erfc_f32"   },
		{ function_body::erfc_f64,   "__builtin_erfc_f64"   },
		{ function_body::tgamma_f32, "__builtin_tgamma_f32" },
		{ function_body::tgamma_f64, "__builtin_tgamma_f64" },
		{ function_body::lgamma_f32, "__builtin_lgamma_f32" },
		{ function_body::lgamma_f64, "__builtin_lgamma_f64" },
	}};
}();


} // namespace ast

#endif // AST_STATEMENT_H
