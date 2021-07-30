#ifndef AST_STATEMENT_H
#define AST_STATEMENT_H

#include "core.h"

#include "node.h"
#include "expression.h"
#include "typespec.h"
#include "constant_value.h"
#include "identifier.h"
#include "abi/calling_conventions.h"

namespace ast
{

struct attribute
{
	lex::token_pos         name;
	lex::token_range       arg_tokens;
	bz::vector<expression> args;

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
struct stmt_foreach;
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
	stmt_foreach,
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

struct stmt_foreach
{
	statement  range_var_decl;
	statement  iter_var_decl;
	statement  end_var_decl;
	statement  iter_deref_var_decl;
	expression condition;
	expression iteration;
	statement  for_block;

	declare_default_5(stmt_foreach)

	stmt_foreach(
		statement  _range_var_decl,
		statement  _iter_var_decl,
		statement  _end_var_decl,
		statement  _iter_deref_var_decl,
		expression _condition,
		expression _iteration,
		statement  _for_block
	)
		: range_var_decl     (std::move(_range_var_decl)),
		  iter_var_decl      (std::move(_iter_var_decl)),
		  end_var_decl       (std::move(_end_var_decl)),
		  iter_deref_var_decl(std::move(_iter_deref_var_decl)),
		  condition(std::move(_condition)),
		  iteration(std::move(_iteration)),
		  for_block(std::move(_for_block))
	{}
};

struct stmt_return
{
	lex::token_pos return_pos;
	expression expr;

	declare_default_5(stmt_return)

	stmt_return(lex::token_pos _return_pos)
		: return_pos(_return_pos), expr()
	{}

	stmt_return(lex::token_pos _return_pos, expression _expr)
		: return_pos(_return_pos), expr(std::move(_expr))
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


struct var_id_and_type
{
	identifier id;
	typespec var_type;

	var_id_and_type(void)
		: id{}, var_type{}
	{}

	var_id_and_type(identifier _id, typespec _var_type)
		: id(std::move(_id)), var_type(std::move(_var_type))
	{}

	var_id_and_type(identifier _id)
		: id(std::move(_id)), var_type{}
	{}
};

struct decl_variable
{
	enum : uint16_t
	{
		maybe_unused     = bit_at<0>,
		used             = bit_at<1>,
		module_export    = bit_at<2>,
		external_linkage = bit_at<3>,
		no_runtime_emit  = bit_at<4>,
		member           = bit_at<5>,
		global           = bit_at<6>,
		variadic         = bit_at<7>,
		tuple_outer_ref  = bit_at<8>,
	};

	lex::src_tokens src_tokens;
	lex::token_range prototype_range;
	var_id_and_type id_and_type;
	arena_vector<decl_variable> tuple_decls;

	expression    init_expr; // is null if there's no initializer
	resolve_state state;
	ast_unique_ptr<decl_variable> original_tuple_variadic_decl; // non-null only if tuple_decls has an empty variadic declaration at the end
	uint16_t       flags;

	decl_variable(void) = default;
	decl_variable(decl_variable const &other)
		: src_tokens(other.src_tokens), prototype_range(other.prototype_range),
		  id_and_type(other.id_and_type), tuple_decls(other.tuple_decls),
		  init_expr(other.init_expr), state(other.state),
		  original_tuple_variadic_decl(nullptr), flags(other.flags)
	{
		if (other.original_tuple_variadic_decl != nullptr)
		{
			this->original_tuple_variadic_decl = make_ast_unique<decl_variable>(*other.original_tuple_variadic_decl);
		}
	}
	decl_variable(decl_variable &&) = default;
	decl_variable &operator = (decl_variable const &other)
	{
		if (this == &other)
		{
			return *this;
		}
		this->src_tokens = other.src_tokens;
		this->prototype_range = other.prototype_range;
		this->id_and_type = other.id_and_type;
		this->tuple_decls = other.tuple_decls;
		this->init_expr = other.init_expr;
		this->state = other.state;
		this->flags = other.flags;
		if (other.original_tuple_variadic_decl != nullptr)
		{
			this->original_tuple_variadic_decl = make_ast_unique<decl_variable>(*other.original_tuple_variadic_decl);
		}
		return *this;
	}
	decl_variable &operator = (decl_variable &&) = default;
	~decl_variable(void) noexcept = default;

	decl_variable(
		lex::src_tokens  _src_tokens,
		lex::token_range _prototype_range,
		var_id_and_type  _id_and_type,
		expression       _init_expr
	)
		: src_tokens (_src_tokens),
		  prototype_range(_prototype_range),
		  id_and_type(std::move(_id_and_type)),
		  tuple_decls{},
		  init_expr  (std::move(_init_expr)),
		  state      (resolve_state::none),
		  flags      (0)
	{}

	decl_variable(
		lex::src_tokens _src_tokens,
		lex::token_range _prototype_range,
		var_id_and_type _id_and_type
	)
		: src_tokens (_src_tokens),
		  prototype_range(_prototype_range),
		  id_and_type(std::move(_id_and_type)),
		  tuple_decls{},
		  init_expr  (),
		  state      (resolve_state::none),
		  flags      (0)
	{}

	decl_variable(
		lex::src_tokens _src_tokens,
		lex::token_range _prototype_range,
		arena_vector<decl_variable> _tuple_decls,
		expression      _init_expr
	)
		: src_tokens (_src_tokens),
		  prototype_range(_prototype_range),
		  id_and_type{},
		  tuple_decls(std::move(_tuple_decls)),
		  init_expr  (std::move(_init_expr)),
		  state      (resolve_state::none),
		  flags      (0)
	{}

	decl_variable(
		lex::src_tokens _src_tokens,
		lex::token_range _prototype_range,
		arena_vector<decl_variable> _tuple_decls
	)
		: src_tokens (_src_tokens),
		  prototype_range(_prototype_range),
		  id_and_type{},
		  tuple_decls(std::move(_tuple_decls)),
		  init_expr  (),
		  state      (resolve_state::none),
		  flags      (0)
	{}

	lex::token_pos get_tokens_begin(void) const;
	lex::token_pos get_tokens_pivot(void) const;
	lex::token_pos get_tokens_end(void) const;

	bool is_maybe_unused(void) const noexcept
	{ return (this->flags & maybe_unused) != 0; }

	bool is_used(void) const noexcept
	{ return (this->flags & used) != 0; }

	bool is_module_export(void) const noexcept
	{ return (this->flags & module_export) != 0; }

	bool is_external_linkage(void) const noexcept
	{ return (this->flags & external_linkage) != 0; }

	bool is_no_runtime_emit(void) const noexcept
	{ return (this->flags & no_runtime_emit) != 0; }

	bool is_member(void) const noexcept
	{ return (this->flags & member) != 0; }

	bool is_global(void) const noexcept
	{ return (this->flags & global) != 0; }

	bool is_variadic(void) const noexcept
	{ return (this->flags & variadic) != 0; }

	bool is_tuple_outer_ref(void) const noexcept
	{ return (this->flags & tuple_outer_ref) != 0; }

	typespec &get_type(void)
	{ return this->id_and_type.var_type; }

	typespec const &get_type(void) const
	{ return this->id_and_type.var_type; }

	void clear_type(void)
	{
		this->id_and_type.var_type.clear();
		for (auto &decl : this->tuple_decls)
		{
			decl.clear_type();
		}
	}

	identifier const &get_id(void) const
	{ return this->id_and_type.id; }

	bz::u8string_view get_unqualified_id_value(void) const
	{
		auto const &id = this->get_id();
		bz_assert(!id.is_qualified && id.values.size() == 1);
		return id.values.front();
	}

	lex::token_range get_prototype_range(void) const
	{ return this->prototype_range; }
};

inline bool is_generic_parameter(decl_variable const &var_decl)
{
	return !var_decl.get_type().is_empty()
		&& (
			!is_complete(var_decl.get_type())
			|| var_decl.get_type().is_typename()
			|| var_decl.get_type().is<ast::ts_consteval>()
		);
}

struct function_body
{
	using body_t = bz::variant<lex::token_range, bz::vector<statement>>;

	enum : uint32_t
	{
		module_export               = bit_at< 0>,
		main                        = bit_at< 1>,
		external_linkage            = bit_at< 2>,
		intrinsic                   = bit_at< 3>,
		generic                     = bit_at< 4>,
		generic_specialization      = bit_at< 5>,
		default_op_assign           = bit_at< 6>,
		default_op_move_assign      = bit_at< 7>,
		no_comptime_checking        = bit_at< 8>,
		local                       = bit_at< 9>,
		destructor                  = bit_at<10>,
		constructor                 = bit_at<11>,
		default_default_constructor = bit_at<12>,
		default_copy_constructor    = bit_at<13>,
		bitcode_emitted             = bit_at<14>,
		comptime_bitcode_emitted    = bit_at<15>,
		only_consteval              = bit_at<16>,
	};

	enum : uint8_t
	{
		_builtin_first,

		builtin_str_eq = _builtin_first,
		builtin_str_neq,
		builtin_str_length,
		builtin_str_starts_with,
		builtin_str_ends_with,

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

		builtin_pointer_cast,
		builtin_pointer_to_int,
		builtin_int_to_pointer,

		builtin_call_destructor,
		builtin_inplace_construct,

		builtin_is_comptime,
		builtin_panic,

		print_stdout,
		println_stdout,
		print_stderr,
		println_stderr,

		comptime_malloc,
		comptime_malloc_type,
		comptime_free,

		comptime_compile_error,
		comptime_compile_warning,
		comptime_compile_error_src_tokens,
		comptime_compile_warning_src_tokens,

		comptime_create_global_string,

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

		// bit manipulation intrinsics

		bitreverse_u8, bitreverse_u16, bitreverse_u32, bitreverse_u64,
		popcount_u8,   popcount_u16,   popcount_u32,   popcount_u64,
		byteswap_u16,  byteswap_u32,   byteswap_u64,
		clz_u8,  clz_u16,  clz_u32,  clz_u64,
		ctz_u8,  ctz_u16,  ctz_u32,  ctz_u64,
		fshl_u8, fshl_u16, fshl_u32, fshl_u64,
		fshr_u8, fshr_u16, fshr_u32, fshr_u64,

		_builtin_last,
	};

	bz::vector<decl_variable> params;
	typespec                  return_type;
	body_t                    body;
	bz::variant<identifier, uint32_t> function_name_or_operator_kind;
	bz::u8string              symbol_name;
	lex::src_tokens           src_tokens;
	resolve_state             state = resolve_state::none;
	abi::calling_convention   cc = abi::calling_convention::c;
	uint8_t                   intrinsic_kind = 0;
	uint32_t                  flags = 0;

	type_info *constructor_or_destructor_of;

	bz::vector<std::unique_ptr<function_body>>              generic_specializations;
	bz::vector<std::pair<lex::src_tokens, function_body *>> generic_required_from;
	function_body *generic_parent = nullptr;

//	declare_default_5(function_body)
	function_body(void)             = default;
	function_body(function_body &&) = default;

	function_body(function_body const &other)
		: params        (other.params),
		  return_type   (other.return_type),
		  body          (other.body),
		  function_name_or_operator_kind(other.function_name_or_operator_kind),
		  symbol_name   (other.symbol_name),
		  src_tokens    (other.src_tokens),
		  state         (other.state),
		  cc            (other.cc),
		  intrinsic_kind(other.intrinsic_kind),
		  flags         (other.flags),
		  constructor_or_destructor_of(nullptr),
		  generic_specializations(),
		  generic_required_from(other.generic_required_from),
		  generic_parent(other.generic_parent)
	{}

	bz::vector<statement> &get_statements(void) noexcept
	{
		bz_assert(this->body.is<bz::vector<statement>>());
		return this->body.get<bz::vector<statement>>();
	}

	bz::vector<statement> const &get_statements(void) const noexcept
	{
		bz_assert(this->body.is<bz::vector<statement>>());
		return this->body.get<bz::vector<statement>>();
	}

	bz::u8string get_signature(void) const;
	bz::u8string get_symbol_name(void) const;
	bz::u8string get_candidate_message(void) const;

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
	{ return (this->flags & external_linkage) != 0; }

	bool is_main(void) const noexcept
	{ return(this->flags & main) != 0; }

	bool is_export(void) const noexcept
	{ return (this->flags & module_export) != 0; }

	bool is_intrinsic(void) const noexcept
	{ return (this->flags & intrinsic) != 0; }

	bool is_generic(void) const noexcept
	{ return (this->flags & generic) != 0; }

	bool is_generic_specialization(void) const noexcept
	{ return (this->flags & generic_specialization) != 0; }

	bool is_default_op_assign(void) const noexcept
	{ return (this->flags & default_op_assign) != 0; }

	bool is_default_op_move_assign(void) const noexcept
	{ return (this->flags & default_op_move_assign) != 0; }

	bool is_no_comptime_checking(void) const noexcept
	{ return (this->flags & no_comptime_checking) != 0; }

	bool is_local(void) const noexcept
	{ return (this->flags & local) != 0; }

	bool is_destructor(void) const noexcept
	{ return (this->flags & destructor) != 0; }

	bool is_constructor(void) const noexcept
	{ return (this->flags & constructor) != 0; }

	bool is_default_default_constructor(void) const noexcept
	{ return (this->flags & default_default_constructor) != 0; }

	bool is_default_copy_constructor(void) const noexcept
	{ return (this->flags & default_copy_constructor) != 0; }

	bool is_bitcode_emitted(void) const noexcept
	{ return (this->flags & bitcode_emitted) != 0; }

	bool is_comptime_bitcode_emitted(void) const noexcept
	{ return (this->flags & comptime_bitcode_emitted) != 0; }

	bool is_only_consteval(void) const noexcept
	{ return (this->flags & only_consteval) != 0; }

	type_info *get_destructor_of(void) const noexcept
	{
		bz_assert(this->is_destructor());
		return this->constructor_or_destructor_of;
	}

	type_info *get_constructor_of(void) const noexcept
	{
		bz_assert(this->is_constructor());
		return this->constructor_or_destructor_of;
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
	identifier    id;
	function_body body;

	declare_default_5(decl_function)

	decl_function(
		identifier    _id,
		function_body _body
	)
		: id  (std::move(_id)),
		  body(std::move(_body))
	{}
};

struct decl_operator
{
	bz::vector<bz::u8string_view> scope;
	lex::token_pos                op;
	function_body                 body;

	declare_default_5(decl_operator)

	decl_operator(
		bz::vector<bz::u8string_view> _scope,
		lex::token_pos                _op,
		function_body                 _body
	)
		: scope(std::move(_scope)),
		  op   (_op),
		  body (std::move(_body))
	{}
};

struct decl_function_alias
{
	lex::src_tokens             src_tokens;
	identifier                  id;
	expression                  alias_expr;
	bz::vector<function_body *> aliased_bodies;
	resolve_state               state;
	bool                        is_export;

	declare_default_5(decl_function_alias)

	decl_function_alias(
		lex::src_tokens _src_tokens,
		identifier      _id,
		expression      _alias_expr
	)
		: src_tokens(_src_tokens),
		  id(std::move(_id)),
		  alias_expr(std::move(_alias_expr)),
		  aliased_bodies{},
		  state(resolve_state::none),
		  is_export(false)
	{}
};

struct decl_type_alias
{
	lex::src_tokens src_tokens;
	identifier      id;
	expression      alias_expr;
	resolve_state   state;
	bool            is_export;

	decl_type_alias(
		lex::src_tokens _src_tokens,
		identifier      _id,
		expression      _alias_expr
	)
		: src_tokens(_src_tokens),
		  id        (std::move(_id)),
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

struct type_info
{
	using body_t = bz::variant<lex::token_range, bz::vector<statement>>;

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

	lex::src_tokens src_tokens;
	uint8_t         kind;
	resolve_state   state;
	bool            is_export;
	identifier      type_name;
	uint32_t        file_id;
	bz::u8string    symbol_name;
	body_t          body;

	bz::vector<ast::decl_variable *> member_variables;

	using function_body_ptr = ast_unique_ptr<function_body>;
	function_body_ptr default_op_assign;
	function_body_ptr default_op_move_assign;

	function_body_ptr default_default_constructor;
	function_body_ptr default_copy_constructor;

	function_body *op_assign = nullptr;
	function_body *op_move_assign = nullptr;

	function_body *default_constructor = nullptr;
	function_body *copy_constructor = nullptr;

	function_body *destructor = nullptr;
	bz::vector<function_body *> constructors{};

//	function_body *move_constructor;
//	function_body_ptr move_destuctor;

	type_info(lex::src_tokens _src_tokens, identifier _type_name, lex::token_range range)
		: src_tokens(_src_tokens),
		  kind(range.begin == nullptr ? forward_declaration : aggregate),
		  state(resolve_state::none),
		  is_export(false),
		  type_name(std::move(_type_name)),
		  file_id(_src_tokens.pivot == nullptr ? 0 : _src_tokens.pivot->src_pos.file_id),
		  symbol_name(),
		  body(range),
		  member_variables{},
		  default_op_assign(make_default_op_assign(src_tokens, *this)),
		  default_op_move_assign(make_default_op_move_assign(src_tokens, *this)),
		  default_default_constructor(make_default_default_constructor(src_tokens, *this)),
		  default_copy_constructor(make_default_copy_constructor(src_tokens, *this))
//		  move_constructor(nullptr),
//		  move_destuctor(nullptr)
	{}

private:
	type_info(bz::u8string_view name, uint8_t kind)
		: src_tokens{},
		  kind(kind),
		  state(resolve_state::all),
		  is_export(false),
		  type_name(),
		  file_id(0),
		  symbol_name(bz::format("builtin.{}", name)),
		  body(bz::vector<statement>{}),
		  member_variables{},
		  default_op_assign(nullptr),
		  default_op_move_assign(nullptr),
		  default_default_constructor(nullptr),
		  default_copy_constructor(nullptr)
//		  move_constructor(nullptr),
//		  move_destuctor(nullptr)
	{}
public:

	static function_body_ptr make_default_op_assign(lex::src_tokens src_tokens, type_info &info);
	static function_body_ptr make_default_op_move_assign(lex::src_tokens src_tokens, type_info &info);
	static function_body_ptr make_default_default_constructor(lex::src_tokens src_tokens, type_info &info);
	static function_body_ptr make_default_copy_constructor(lex::src_tokens src_tokens, type_info &info);

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
	case type_info::int8_:    return "int8";
	case type_info::int16_:   return "int16";
	case type_info::int32_:   return "int32";
	case type_info::int64_:   return "int64";
	case type_info::uint8_:   return "uint8";
	case type_info::uint16_:  return "uint16";
	case type_info::uint32_:  return "uint32";
	case type_info::uint64_:  return "uint64";
	case type_info::float32_: return "float32";
	case type_info::float64_: return "float64";
	case type_info::char_:    return "char";
	case type_info::str_:     return "str";
	case type_info::bool_:    return "bool";
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
{ static constexpr auto value = type_info::int8_; };
template<>
struct type_info_from_type<int16_t>
{ static constexpr auto value = type_info::int16_; };
template<>
struct type_info_from_type<int32_t>
{ static constexpr auto value = type_info::int32_; };
template<>
struct type_info_from_type<int64_t>
{ static constexpr auto value = type_info::int64_; };

template<>
struct type_info_from_type<uint8_t>
{ static constexpr auto value = type_info::uint8_; };
template<>
struct type_info_from_type<uint16_t>
{ static constexpr auto value = type_info::uint16_; };
template<>
struct type_info_from_type<uint32_t>
{ static constexpr auto value = type_info::uint32_; };
template<>
struct type_info_from_type<uint64_t>
{ static constexpr auto value = type_info::uint64_; };

template<>
struct type_info_from_type<float32_t>
{ static constexpr auto value = type_info::float32_; };
template<>
struct type_info_from_type<float64_t>
{ static constexpr auto value = type_info::float64_; };

} // namespace internal

template<uint32_t N>
using type_from_type_info_t = typename internal::type_from_type_info<N>::type;
template<typename T>
constexpr auto type_info_from_type_v = internal::type_info_from_type<T>::value;



struct decl_struct
{
	using info_t = bz::variant<lex::token_range, type_info>;

	identifier id;
	type_info  info;

//	declare_default_5(decl_struct)
	decl_struct(decl_struct const &) = delete;
	decl_struct(decl_struct &&)      = default;

	decl_struct(lex::src_tokens _src_tokens, identifier _id, lex::token_range _range)
		: id  (std::move(_id)),
		  info(_src_tokens, this->id, _range)
	{}

	decl_struct(lex::src_tokens _src_tokens, identifier _id)
		: id  (std::move(_id)),
		  info(_src_tokens, this->id, {})
	{}

	lex::token_pos get_tokens_begin(void) const;
	lex::token_pos get_tokens_pivot(void) const;
	lex::token_pos get_tokens_end(void) const;
};

struct decl_import
{
	identifier id;

	decl_import(identifier _id)
		: id(std::move(_id))
	{}

	declare_default_5(decl_import)
};


template<typename ...Args>
statement make_statement(Args &&...args)
{ return statement(std::forward<Args>(args)...); }


#define def_make_fn(ret_type, node_type)                                       \
template<typename ...Args>                                                     \
ret_type make_ ## node_type (Args &&...args)                                   \
{ return ret_type(make_ast_unique<node_type>(std::forward<Args>(args)...)); }

def_make_fn(statement, decl_variable)
def_make_fn(statement, decl_function)
def_make_fn(statement, decl_operator)
def_make_fn(statement, decl_function_alias)
def_make_fn(statement, decl_type_alias)
def_make_fn(statement, decl_struct)
def_make_fn(statement, decl_import)

def_make_fn(statement, stmt_while)
def_make_fn(statement, stmt_for)
def_make_fn(statement, stmt_foreach)
def_make_fn(statement, stmt_return)
def_make_fn(statement, stmt_no_op)
def_make_fn(statement, stmt_expression)
def_make_fn(statement, stmt_static_assert)

#undef def_make_fn

struct type_and_name_pair
{
	typespec type;
	bz::u8string_view name;
};

struct universal_function_set
{
	bz::u8string_view id;
	bz::vector<uint32_t> func_kinds;
};

bz::vector<type_info>              make_builtin_type_infos(void);
bz::vector<type_and_name_pair>     make_builtin_types    (bz::array_view<type_info> builtin_type_infos, size_t pointer_size);
bz::vector<function_body>          make_builtin_functions(bz::array_view<type_info> builtin_type_infos, size_t pointer_size);
bz::vector<universal_function_set> make_builtin_universal_functions(void);

struct intrinsic_info_t
{
	uint32_t kind;
	bz::u8string_view func_name;
};

constexpr auto intrinsic_info = []() {
	static_assert(function_body::_builtin_last - function_body::_builtin_first == 121);
	constexpr size_t size = function_body::_builtin_last - function_body::_builtin_first;
	return bz::array<intrinsic_info_t, size>{{
		{ function_body::builtin_str_eq,          "__builtin_str_eq"          },
		{ function_body::builtin_str_neq,         "__builtin_str_neq"         },
		{ function_body::builtin_str_length,      "__builtin_str_length"      },
		{ function_body::builtin_str_starts_with, "__builtin_str_starts_with" },
		{ function_body::builtin_str_ends_with,   "__builtin_str_ends_with"   },

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

		{ function_body::builtin_pointer_cast,   "__builtin_pointer_cast"   },
		{ function_body::builtin_pointer_to_int, "__builtin_pointer_to_int" },
		{ function_body::builtin_int_to_pointer, "__builtin_int_to_pointer" },

		{ function_body::builtin_call_destructor,   "__builtin_call_destructor"   },
		{ function_body::builtin_inplace_construct, "__builtin_inplace_construct" },

		{ function_body::builtin_is_comptime, "__builtin_is_comptime" },
		{ function_body::builtin_panic,       "__builtin_panic"       },

		{ function_body::print_stdout,   "__builtin_print_stdout"   },
		{ function_body::println_stdout, "__builtin_println_stdout" },
		{ function_body::print_stderr,   "__builtin_print_stderr"   },
		{ function_body::println_stderr, "__builtin_println_stderr" },

		{ function_body::comptime_malloc,      "__builtin_comptime_malloc"      },
		{ function_body::comptime_malloc_type, "__builtin_comptime_malloc_type" },
		{ function_body::comptime_free,        "__builtin_comptime_free"        },

		{ function_body::comptime_compile_error,              "__builtin_comptime_compile_error"              },
		{ function_body::comptime_compile_warning,            "__builtin_comptime_compile_warning"            },
		{ function_body::comptime_compile_error_src_tokens,   "__builtin_comptime_compile_error_src_tokens"   },
		{ function_body::comptime_compile_warning_src_tokens, "__builtin_comptime_compile_warning_src_tokens" },

		{ function_body::comptime_create_global_string, "__builtin_comptime_create_global_string" },

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

		{ function_body::bitreverse_u8,  "__builtin_bitreverse_u8"  },
		{ function_body::bitreverse_u16, "__builtin_bitreverse_u16" },
		{ function_body::bitreverse_u32, "__builtin_bitreverse_u32" },
		{ function_body::bitreverse_u64, "__builtin_bitreverse_u64" },
		{ function_body::popcount_u8,    "__builtin_popcount_u8"    },
		{ function_body::popcount_u16,   "__builtin_popcount_u16"   },
		{ function_body::popcount_u32,   "__builtin_popcount_u32"   },
		{ function_body::popcount_u64,   "__builtin_popcount_u64"   },
		{ function_body::byteswap_u16,   "__builtin_byteswap_u16"   },
		{ function_body::byteswap_u32,   "__builtin_byteswap_u32"   },
		{ function_body::byteswap_u64,   "__builtin_byteswap_u64"   },

		{ function_body::clz_u8,   "__builtin_clz_u8"   },
		{ function_body::clz_u16,  "__builtin_clz_u16"  },
		{ function_body::clz_u32,  "__builtin_clz_u32"  },
		{ function_body::clz_u64,  "__builtin_clz_u64"  },
		{ function_body::ctz_u8,   "__builtin_ctz_u8"   },
		{ function_body::ctz_u16,  "__builtin_ctz_u16"  },
		{ function_body::ctz_u32,  "__builtin_ctz_u32"  },
		{ function_body::ctz_u64,  "__builtin_ctz_u64"  },
		{ function_body::fshl_u8,  "__builtin_fshl_u8"  },
		{ function_body::fshl_u16, "__builtin_fshl_u16" },
		{ function_body::fshl_u32, "__builtin_fshl_u32" },
		{ function_body::fshl_u64, "__builtin_fshl_u64" },
		{ function_body::fshr_u8,  "__builtin_fshr_u8"  },
		{ function_body::fshr_u16, "__builtin_fshr_u16" },
		{ function_body::fshr_u32, "__builtin_fshr_u32" },
		{ function_body::fshr_u64, "__builtin_fshr_u64" },
	}};
}();


} // namespace ast

#endif // AST_STATEMENT_H
