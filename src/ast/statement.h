#ifndef AST_STATEMENT_H
#define AST_STATEMENT_H

#include "core.h"

#include "statement_forward.h"
#include "lex/token.h"
#include "node.h"
#include "expression.h"
#include "typespec.h"
#include "constant_value.h"
#include "identifier.h"
#include "scope.h"
#include "abi/calling_conventions.h"

namespace ast
{

struct attribute
{
	lex::token_pos   name;
	lex::token_range arg_tokens;
	arena_vector<expression> args;
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


using statement_node_t = bz::meta::apply_type_pack<node, statement_types>;
using statement_node_view_t = node_view_from_node<statement_node_t>;

struct statement : statement_node_t
{
	using base_t = statement_node_t;

	using base_t::node;
	using base_t::get;
	using base_t::kind;
	using base_t::emplace;
};

struct statement_view : statement_node_view_t
{
	using base_t = statement_node_view_t;

	using base_t::node_view;
	using base_t::get;
	using base_t::kind;
	using base_t::emplace;

	statement_view(statement &stmt)
		: base_t(static_cast<statement_node_t &>(stmt))
	{}
};


struct stmt_while
{
	expression condition;
	expression while_block;
	scope_t    loop_scope;

	stmt_while(
		expression _condition,
		expression _while_block
	)
		: condition  (std::move(_condition)),
		  while_block(std::move(_while_block))
	{}
};

struct stmt_for
{
	statement  init;
	expression condition;
	expression iteration;
	expression for_block;
	scope_t    init_scope;
	scope_t    loop_scope;

	stmt_for(
		statement  _init,
		expression _condition,
		expression _iteration,
		expression _for_block
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
	expression for_block;
	scope_t    init_scope;
	scope_t    loop_scope;

	stmt_foreach(
		statement  _range_var_decl,
		statement  _iter_deref_var_decl,
		expression _for_block
	)
		: range_var_decl     (std::move(_range_var_decl)),
		  iter_deref_var_decl(std::move(_iter_deref_var_decl)),
		  for_block(std::move(_for_block))
	{}
};

struct stmt_return
{
	lex::token_pos return_pos;
	expression expr;

	stmt_return(lex::token_pos _return_pos)
		: return_pos(_return_pos), expr()
	{}

	stmt_return(lex::token_pos _return_pos, expression _expr)
		: return_pos(_return_pos), expr(std::move(_expr))
	{}
};

struct stmt_defer
{
	lex::token_pos defer_pos;
	destruct_operation deferred_expr;

	stmt_defer(lex::token_pos _defer_pos, expression _expr)
		: defer_pos(_defer_pos), deferred_expr(defer_expression(std::move(_expr)))
	{}
};

struct stmt_no_op
{
};

struct stmt_expression
{
	expression expr;

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
	enclosing_scope_t enclosing_scope;

	stmt_static_assert(lex::token_pos _static_assert_pos, lex::token_range _arg_tokens, enclosing_scope_t _enclosing_scope)
		: static_assert_pos(_static_assert_pos),
		  arg_tokens(_arg_tokens),
		  condition(),
		  message(),
		  enclosing_scope(_enclosing_scope)
	{}
};


struct var_id_and_type
{
	identifier id;
	expression var_type;

	var_id_and_type(void)
		: id(), var_type(type_as_expression(typespec()))
	{}

	var_id_and_type(identifier _id, expression _var_type)
		: id(std::move(_id)), var_type(std::move(_var_type))
	{}

	var_id_and_type(identifier _id)
		: id(std::move(_id)), var_type(type_as_expression(typespec()))
	{}
};

struct decl_variable
{
	enum : uint16_t
	{
		maybe_unused     = bit_at< 0>,
		used             = bit_at< 1>,
		module_export    = bit_at< 2>,
		external_linkage = bit_at< 3>,
		extern_          = bit_at< 4>,
		no_runtime_emit  = bit_at< 5>,
		member           = bit_at< 6>,
		global           = bit_at< 7>,
		variadic         = bit_at< 8>,
		tuple_outer_ref  = bit_at< 9>,
		moved            = bit_at<10>,
		ever_moved       = bit_at<11>,
	};

	lex::src_tokens src_tokens;
	lex::token_range prototype_range;
	var_id_and_type id_and_type;
	bz::u8string symbol_name;
	arena_vector<decl_variable> tuple_decls;

	expression init_expr; // is null if there's no initializer
	destruct_operation destruction;
	ast_unique_ptr<decl_variable> original_tuple_variadic_decl; // non-null only if tuple_decls has an empty variadic declaration at the end
	lex::src_tokens move_position = {};

	arena_vector<attribute> attributes;
	enclosing_scope_t enclosing_scope;
	uint16_t       flags = 0;
	resolve_state  state = resolve_state::none;

	decl_variable(void) = default;
	decl_variable(decl_variable const &other)
		: src_tokens(other.src_tokens),
		  prototype_range(other.prototype_range),
		  id_and_type(other.id_and_type),
		  symbol_name(other.symbol_name),
		  tuple_decls(other.tuple_decls),
		  init_expr(other.init_expr),
		  destruction(other.destruction),
		  original_tuple_variadic_decl(nullptr),
		  move_position(other.move_position),
		  attributes(other.attributes),
		  enclosing_scope(other.enclosing_scope),
		  flags(other.flags), state(other.state)
	{
		bz_assert(other.destruction.is_null());
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
		bz_assert(other.destruction.is_null());
		this->src_tokens = other.src_tokens;
		this->prototype_range = other.prototype_range;
		this->id_and_type = other.id_and_type;
		this->symbol_name = other.symbol_name;
		this->tuple_decls = other.tuple_decls;
		this->init_expr = other.init_expr;
		this->destruction = other.destruction;
		if (other.original_tuple_variadic_decl != nullptr)
		{
			this->original_tuple_variadic_decl = make_ast_unique<decl_variable>(*other.original_tuple_variadic_decl);
		}
		this->move_position = other.move_position;
		this->attributes = other.attributes;
		this->enclosing_scope = other.enclosing_scope;
		this->flags = other.flags;
		this->state = other.state;
		return *this;
	}
	decl_variable &operator = (decl_variable &&) = default;
	~decl_variable(void) noexcept = default;

	decl_variable(
		lex::src_tokens const &_src_tokens,
		lex::token_range       _prototype_range,
		var_id_and_type        _id_and_type,
		expression             _init_expr,
		enclosing_scope_t      _enclosing_scope
	)
		: src_tokens (_src_tokens),
		  prototype_range(_prototype_range),
		  id_and_type(std::move(_id_and_type)),
		  tuple_decls{},
		  init_expr  (std::move(_init_expr)),
		  enclosing_scope(_enclosing_scope)
	{}

	decl_variable(
		lex::src_tokens const &_src_tokens,
		lex::token_range       _prototype_range,
		var_id_and_type        _id_and_type,
		enclosing_scope_t      _enclosing_scope
	)
		: src_tokens (_src_tokens),
		  prototype_range(_prototype_range),
		  id_and_type(std::move(_id_and_type)),
		  tuple_decls{},
		  init_expr  (),
		  enclosing_scope(_enclosing_scope)
	{}

	decl_variable(
		lex::src_tokens const &_src_tokens,
		lex::token_range       _prototype_range,
		arena_vector<decl_variable> _tuple_decls,
		expression             _init_expr,
		enclosing_scope_t      _enclosing_scope
	)
		: src_tokens (_src_tokens),
		  prototype_range(_prototype_range),
		  id_and_type(),
		  tuple_decls(std::move(_tuple_decls)),
		  init_expr  (std::move(_init_expr)),
		  enclosing_scope(_enclosing_scope)
	{}

	decl_variable(
		lex::src_tokens const &_src_tokens,
		lex::token_range       _prototype_range,
		arena_vector<decl_variable> _tuple_decls,
		enclosing_scope_t      _enclosing_scope
	)
		: src_tokens (_src_tokens),
		  prototype_range(_prototype_range),
		  id_and_type(),
		  tuple_decls(std::move(_tuple_decls)),
		  init_expr  (),
		  enclosing_scope(_enclosing_scope)
	{}

	bool is_maybe_unused(void) const noexcept
	{ return (this->flags & maybe_unused) != 0; }

	bool is_used(void) const noexcept
	{ return (this->flags & used) != 0; }

	bool is_module_export(void) const noexcept
	{ return (this->flags & module_export) != 0; }

	bool is_external_linkage(void) const noexcept
	{ return (this->flags & external_linkage) != 0; }

	bool is_extern(void) const noexcept
	{ return (this->flags & extern_) != 0; }

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

	bool is_moved(void) const noexcept
	{ return (this->flags & moved) != 0; }

	bool is_ever_moved(void) const noexcept
	{ return (this->flags & moved) != 0; }

	typespec &get_type(void)
	{
		bz_assert(this->id_and_type.var_type.is_typename());
		return this->id_and_type.var_type.get_typename();
	}

	typespec const &get_type(void) const
	{
		bz_assert(this->id_and_type.var_type.is_typename());
		return this->id_and_type.var_type.get_typename();
	}

	void clear_type(void)
	{
		if (this->id_and_type.var_type.is_typename())
		{
			this->id_and_type.var_type.get_typename().clear();
		}
		else
		{
			this->id_and_type.var_type = type_as_expression(typespec());
		}
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

struct generic_required_from_t
{
	lex::src_tokens src_tokens;
	bz::variant<function_body *, type_info *> body_or_info;
};

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
		default_constructor         = bit_at<12>,
		copy_constructor            = bit_at<13>,
		move_constructor            = bit_at<14>,
		default_default_constructor = bit_at<15>,
		default_copy_constructor    = bit_at<16>,
		default_move_constructor    = bit_at<17>,
		bitcode_emitted             = bit_at<18>,
		comptime_bitcode_emitted    = bit_at<19>,
		only_consteval              = bit_at<20>,
		builtin_operator            = bit_at<21>,
		builtin_assign              = bit_at<22>,
		defaulted                   = bit_at<23>,
		deleted                     = bit_at<24>,
		copy_assign_op              = bit_at<25>,
		move_assign_op              = bit_at<26>,
	};

	enum : uint8_t
	{
		_builtin_first,

		builtin_str_length = _builtin_first,
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
		builtin_swap,

		builtin_is_comptime,
		builtin_is_option_set_impl,
		builtin_is_option_set,
		builtin_panic,

		print_stdout,
		print_stderr,

		comptime_malloc,
		comptime_malloc_type,
		comptime_free,

		comptime_compile_error,
		comptime_compile_warning,
		comptime_compile_error_src_tokens,
		comptime_compile_warning_src_tokens,

		comptime_create_global_string,

		comptime_concatenate_strs,
		comptime_format_float32,
		comptime_format_float64,

		// type manipulation functions

		typename_as_str,

		is_const,
		is_consteval,
		is_pointer,
		is_reference,
		is_move_reference,

		remove_const,
		remove_consteval,
		remove_pointer,
		remove_reference,
		remove_move_reference,

		is_default_constructible,
		is_copy_constructible,
		is_trivially_copy_constructible,
		is_trivially_destructible,

		// llvm intrinsics (https://releases.llvm.org/10.0.0/docs/LangRef.html#standard-c-library-intrinsics)
		// and other C standard library functions

		lifetime_start,
		lifetime_end,

		memcpy,
		memmove,
		memset,

		// C standard library math functions

		abs_i8, abs_i16, abs_i32, abs_i64,
		fabs_f32, fabs_f64,

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
		_builtin_default_constructor_first = _builtin_last,

		// these functions don't have a __builtin_* variant
		i8_default_constructor = _builtin_default_constructor_first,
		i16_default_constructor,
		i32_default_constructor,
		i64_default_constructor,
		u8_default_constructor,
		u16_default_constructor,
		u32_default_constructor,
		u64_default_constructor,
		f32_default_constructor,
		f64_default_constructor,
		char_default_constructor,
		str_default_constructor,
		bool_default_constructor,
		null_t_default_constructor,

		_builtin_default_constructor_last,
		_builtin_unary_operator_first = _builtin_default_constructor_last,

		builtin_unary_plus = _builtin_unary_operator_first,
		builtin_unary_minus,
		builtin_unary_dereference,
		builtin_unary_bit_not,
		builtin_unary_bool_not,
		builtin_unary_plus_plus,
		builtin_unary_minus_minus,

		_builtin_unary_operator_last,
		_builtin_binary_operator_first = _builtin_unary_operator_last,

		builtin_binary_assign = _builtin_binary_operator_first,
		builtin_binary_plus,
		builtin_binary_plus_eq,
		builtin_binary_minus,
		builtin_binary_minus_eq,
		builtin_binary_multiply,
		builtin_binary_multiply_eq,
		builtin_binary_divide,
		builtin_binary_divide_eq,
		builtin_binary_modulo,
		builtin_binary_modulo_eq,
		builtin_binary_equals,
		builtin_binary_not_equals,
		builtin_binary_less_than,
		builtin_binary_less_than_eq,
		builtin_binary_greater_than,
		builtin_binary_greater_than_eq,
		builtin_binary_bit_and,
		builtin_binary_bit_and_eq,
		builtin_binary_bit_xor,
		builtin_binary_bit_xor_eq,
		builtin_binary_bit_or,
		builtin_binary_bit_or_eq,
		builtin_binary_bit_left_shift,
		builtin_binary_bit_left_shift_eq,
		builtin_binary_bit_right_shift,
		builtin_binary_bit_right_shift_eq,

		_builtin_binary_operator_last,
	};

	arena_vector<decl_variable> params;
	typespec                    return_type;
	body_t                      body;
	bz::variant<identifier, uint32_t> function_name_or_operator_kind;
	bz::u8string                symbol_name;
	lex::src_tokens             src_tokens;
	scope_t                     scope;
	uint32_t                    flags = 0;
	resolve_state               state = resolve_state::none;
	abi::calling_convention     cc = abi::calling_convention::c;
	uint8_t                     intrinsic_kind = 0;

	type_info *constructor_or_destructor_of;

	arena_vector<ast_unique_ptr<function_body>> generic_specializations;
	arena_vector<generic_required_from_t>       generic_required_from;
	function_body *generic_parent = nullptr;

	function_body(void)             = default;
	function_body(function_body &&) = default;

	struct generic_copy_t {};

	function_body(function_body const &other, generic_copy_t)
		: params         (),
		  return_type    (other.return_type),
		  body           (other.body),
		  function_name_or_operator_kind(other.function_name_or_operator_kind),
		  symbol_name    (other.symbol_name),
		  src_tokens     (other.src_tokens),
		  scope          (other.scope),
		  flags          ((other.flags & ~generic) | generic_specialization),
		  state          (other.state),
		  cc             (other.cc),
		  intrinsic_kind (other.intrinsic_kind),
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

	arena_vector<decl_variable> get_params_copy_for_generic_specialization(void);
	std::pair<function_body *, bz::u8string> add_specialized_body(
		arena_vector<decl_variable> params,
		arena_vector<generic_required_from_t> required_from
	);

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

	bool is_default_constructor(void) const noexcept
	{ return (this->flags & default_constructor) != 0; }

	bool is_copy_constructor(void) const noexcept
	{ return (this->flags & copy_constructor) != 0; }

	bool is_move_constructor(void) const noexcept
	{ return (this->flags & move_constructor) != 0; }

	bool is_default_default_constructor(void) const noexcept
	{ return (this->flags & default_default_constructor) != 0; }

	bool is_default_copy_constructor(void) const noexcept
	{ return (this->flags & default_copy_constructor) != 0; }

	bool is_default_move_constructor(void) const noexcept
	{ return (this->flags & default_move_constructor) != 0; }

	bool is_bitcode_emitted(void) const noexcept
	{ return (this->flags & bitcode_emitted) != 0; }

	bool is_comptime_bitcode_emitted(void) const noexcept
	{ return (this->flags & comptime_bitcode_emitted) != 0; }

	bool is_only_consteval(void) const noexcept
	{ return (this->flags & only_consteval) != 0; }

	bool is_builtin_operator(void) const noexcept
	{ return (this->flags & builtin_operator) != 0; }

	bool is_builtin_assign(void) const noexcept
	{ return (this->flags & builtin_assign) != 0; }

	bool is_defaulted(void) const noexcept
	{ return (this->flags & defaulted) != 0; }

	bool is_deleted(void) const noexcept
	{ return (this->flags & deleted) != 0; }

	bool is_copy_assign_op(void) const noexcept
	{ return (this->flags & copy_assign_op) != 0; }

	bool is_move_assign_op(void) const noexcept
	{ return (this->flags & move_assign_op) != 0; }

	bool has_builtin_implementation(void) const noexcept
	{
		return (this->is_intrinsic() && this->body.is_null())
			|| this->is_default_default_constructor()
			|| this->is_default_copy_constructor()
			|| this->is_default_move_constructor()
			|| this->is_default_op_assign()
			|| this->is_default_op_move_assign();
	}

	type_info *get_destructor_of(void) const noexcept
	{
		bz_assert(this->is_destructor());
		bz_assert(this->constructor_or_destructor_of != nullptr);
		return this->constructor_or_destructor_of;
	}

	type_info *get_constructor_of(void) const noexcept
	{
		bz_assert(this->is_constructor());
		bz_assert(this->constructor_or_destructor_of != nullptr);
		return this->constructor_or_destructor_of;
	}

	enclosing_scope_t get_enclosing_scope(void) const noexcept;

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
	identifier              id;
	function_body           body;
	arena_vector<attribute> attributes;

	decl_function(void) = default;

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
	arena_vector<attribute>       attributes;

	decl_operator(void) = default;

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
	lex::src_tokens               src_tokens;
	identifier                    id;
	expression                    alias_expr;
	arena_vector<decl_function *> aliased_decls;
	enclosing_scope_t             enclosing_scope;
	bool                          is_export = false;
	resolve_state                 state = resolve_state::none;

	decl_function_alias(
		lex::src_tokens const &_src_tokens,
		identifier             _id,
		expression             _alias_expr,
		enclosing_scope_t      _enclosing_scope
	)
		: src_tokens(_src_tokens),
		  id(std::move(_id)),
		  alias_expr(std::move(_alias_expr)),
		  aliased_decls{},
		  enclosing_scope(_enclosing_scope)
	{}
};

struct decl_type_alias
{
	enum : uint8_t
	{
		module_export    = bit_at<0>,
		global           = bit_at<1>,
	};

	lex::src_tokens   src_tokens;
	identifier        id;
	expression        alias_expr;
	enclosing_scope_t enclosing_scope;
	uint8_t           flags = 0;
	resolve_state     state = resolve_state::none;

	decl_type_alias(
		lex::src_tokens const &_src_tokens,
		identifier             _id,
		expression             _alias_expr,
		enclosing_scope_t      _enclosing_scope
	)
		: src_tokens(_src_tokens),
		  id        (std::move(_id)),
		  alias_expr(std::move(_alias_expr)),
		  enclosing_scope(_enclosing_scope)
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

	bool is_module_export(void) const noexcept
	{ return (this->flags & module_export) != 0; }

	bool is_global(void) const noexcept
	{ return (this->flags & global) != 0; }
};

struct type_info
{
	using body_t = bz::variant<lex::token_range, bz::vector<statement>>;

	enum : uint32_t
	{
		generic                         = bit_at< 0>,
		generic_instantiation           = bit_at< 1>,

		default_constructible           = bit_at< 2>,
		default_zero_initialized        = bit_at< 3>,
		copy_constructible              = bit_at< 4>,
		trivially_copy_constructible    = bit_at< 5>,
		move_constructible              = bit_at< 6>,
		trivially_move_constructible    = bit_at< 7>,
		trivially_destructible          = bit_at< 8>,
		trivially_move_destructible     = bit_at< 9>,
		trivially_relocatable           = bit_at<10>,
		trivial                         = bit_at<11>,
		module_export                   = bit_at<12>,
	};

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
	uint32_t        flags;
	identifier      type_name;
	bz::u8string    symbol_name;
	body_t          body;
	scope_t         scope;

	bz::vector<ast::decl_variable *> member_variables;

	using decl_function_ptr = ast_unique_ptr<decl_function>;
	using decl_operator_ptr = ast_unique_ptr<decl_operator>;

	decl_operator_ptr default_op_assign;
	decl_operator_ptr default_op_move_assign;

	decl_function_ptr default_default_constructor;
	decl_function_ptr default_copy_constructor;
	decl_function_ptr default_move_constructor;

	decl_function *default_constructor = nullptr;
	decl_function *copy_constructor = nullptr;
	decl_function *move_constructor = nullptr;

	decl_function *destructor = nullptr;
	decl_function *move_destructor = nullptr;

	arena_vector<decl_function *> constructors{};
	arena_vector<decl_function *> destructors{};

	arena_vector<decl_variable>             generic_parameters{};
	arena_vector<ast_unique_ptr<type_info>> generic_instantiations{};
	type_info *generic_parent = nullptr;
	arena_vector<generic_required_from_t> generic_required_from;

//	function_body *move_constructor;
//	function_body_ptr move_destuctor;

	type_info(lex::src_tokens const &_src_tokens, identifier _type_name, lex::token_range range, enclosing_scope_t _enclosing_scope)
		: src_tokens(_src_tokens),
		  kind(range.begin == nullptr ? forward_declaration : aggregate),
		  state(resolve_state::none),
		  flags(0),
		  type_name(std::move(_type_name)),
		  symbol_name(),
		  body(range),
		  scope(make_global_scope(_enclosing_scope, {}))
	{}

	type_info(
		lex::src_tokens const &_src_tokens,
		identifier _type_name,
		lex::token_range range,
		arena_vector<decl_variable> _generic_parameters,
		enclosing_scope_t _enclosing_scope
	)
		: src_tokens(_src_tokens),
		  kind(range.begin == nullptr ? forward_declaration : aggregate),
		  state(resolve_state::none),
		  flags(generic),
		  type_name(std::move(_type_name)),
		  symbol_name(),
		  body(range),
		  scope(make_global_scope(_enclosing_scope, {})),
		  generic_parameters(std::move(_generic_parameters))
	{}

	struct generic_copy_t {};

	type_info(type_info const &other, generic_copy_t)
		: src_tokens(other.src_tokens),
		  kind(other.kind),
		  state(resolve_state::none),
		  flags((other.flags & ~generic) | generic_instantiation),
		  type_name(other.type_name),
		  symbol_name(),
		  body(other.body),
		  scope(other.scope),
		  generic_required_from(other.generic_required_from)
	{}

private:
	type_info(bz::u8string_view name, uint8_t kind)
		: src_tokens{},
		  kind(kind),
		  state(resolve_state::all),
		  flags(
			  default_constructible
			  | copy_constructible
			  | trivially_copy_constructible
			  | move_constructible
			  | trivially_move_constructible
			  | trivially_destructible
			  | trivially_move_destructible
			  | trivial
			  | trivially_relocatable
			  | default_zero_initialized
		  ),
		  type_name(),
		  symbol_name(bz::format("builtin.{}", name)),
		  body(bz::vector<statement>{}),
		  scope(make_global_scope({}, {})),
		  member_variables{},
		  default_op_assign(nullptr),
		  default_op_move_assign(nullptr),
		  default_default_constructor(nullptr),
		  default_copy_constructor(nullptr),
		  default_move_constructor(nullptr)
	{}
public:

	bool is_generic(void) const noexcept
	{ return (this->flags & generic) != 0; }

	bool is_generic_instantiation(void) const noexcept
	{ return (this->flags & generic_instantiation) != 0; }

	bool is_default_constructible(void) const noexcept
	{ return (this->flags & default_constructible) != 0; }

	bool is_copy_constructible(void) const noexcept
	{ return (this->flags & copy_constructible) != 0; }

	bool is_trivially_copy_constructible(void) const noexcept
	{ return (this->flags & trivially_copy_constructible) != 0; }

	bool is_move_constructible(void) const noexcept
	{ return (this->flags & move_constructible) != 0; }

	bool is_trivially_move_constructible(void) const noexcept
	{ return (this->flags & trivially_move_constructible) != 0; }

	bool is_trivially_destructible(void) const noexcept
	{ return (this->flags & trivially_destructible) != 0; }

	bool is_trivially_move_destructible(void) const noexcept
	{ return (this->flags & trivially_move_destructible) != 0; }

	bool is_trivially_relocatable(void) const noexcept
	{ return (this->flags & trivially_relocatable) != 0; }

	bool is_trivial(void) const noexcept
	{ return (this->flags & trivial) != 0; }

	bool is_default_zero_initialized(void) const noexcept
	{ return (this->flags & default_zero_initialized) != 0; }

	bool is_module_export(void) const noexcept
	{ return (this->flags & module_export) != 0; }

	static decl_operator_ptr make_default_op_assign(lex::src_tokens const &src_tokens, type_info &info);
	static decl_operator_ptr make_default_op_move_assign(lex::src_tokens const &src_tokens, type_info &info);
	static decl_function_ptr make_default_default_constructor(lex::src_tokens const &src_tokens, type_info &info);
	static decl_function_ptr make_default_copy_constructor(lex::src_tokens const &src_tokens, type_info &info);
	static decl_function_ptr make_default_move_constructor(lex::src_tokens const &src_tokens, type_info &info);

	arena_vector<decl_variable> get_params_copy_for_generic_instantiation(void);
	type_info *add_generic_instantiation(
		arena_vector<decl_variable> generic_params,
		arena_vector<generic_required_from_t> required_from
	);

	bz::u8string get_typename_as_string(void) const;

	enclosing_scope_t get_scope(void) noexcept;
	enclosing_scope_t get_enclosing_scope(void) const noexcept;

	static type_info make_builtin(bz::u8string_view name, uint8_t kind)
	{
		return type_info(name, kind);
	}

	static bz::u8string_view decode_symbol_name(bz::u8string_view symbol_name)
	{
		constexpr bz::u8string_view builtin = "builtin.";
		constexpr bz::u8string_view struct_ = "struct.";
		constexpr bz::u8string_view non_global_struct = "non_global_struct.";
		if (symbol_name.starts_with(builtin))
		{
			return symbol_name.substring(builtin.length());
		}
		else if (symbol_name.starts_with(struct_))
		{
			return symbol_name.substring(struct_.length());
		}
		else
		{
			bz_assert(symbol_name.starts_with(non_global_struct));
			return symbol_name.substring(non_global_struct.length());
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
		constexpr bz::u8string_view non_global_struct = "non_global_struct.";
		if (whole_str.starts_with(builtin))
		{
			static_assert(builtin.length() == builtin.size());
			auto const begin = bz::u8string_view::const_iterator(it.data() + builtin.size());
			auto const dot = whole_str.find(begin, '.');
			it = dot;
			return bz::u8string(begin, dot);
		}
		else if (whole_str.starts_with(struct_))
		{
			static_assert(struct_.length() == struct_.size());
			auto const begin = bz::u8string_view::const_iterator(it.data() + struct_.size());
			auto const dot = whole_str.find(begin, '.');
			it = dot;
			return bz::u8string(begin, dot);
		}
		else
		{
			bz_assert(whole_str.starts_with(non_global_struct));
			static_assert(non_global_struct.length() == non_global_struct.size());
			auto const begin = bz::u8string_view::const_iterator(it.data() + non_global_struct.size());
			auto const dot = whole_str.find(begin, '.');
			it = dot;
			return bz::u8string(begin, dot);
		}
	}
};

constexpr bool is_integer_kind(uint8_t kind)
{
	return kind >= ast::type_info::int8_
		&& kind <= ast::type_info::uint64_;
}

constexpr bool is_unsigned_integer_kind(uint8_t kind)
{
	return kind >= ast::type_info::uint8_
		&& kind <= ast::type_info::uint64_;
}

constexpr bool is_signed_integer_kind(uint8_t kind)
{
	return kind >= ast::type_info::int8_
		&& kind <= ast::type_info::int64_;
}

constexpr bool is_floating_point_kind(uint8_t kind)
{
	return kind == ast::type_info::float32_
		|| kind == ast::type_info::float64_;
}

constexpr bool is_arithmetic_kind(uint8_t kind)
{
	return kind >= ast::type_info::int8_
		&& kind <= ast::type_info::float64_;
}

inline bz::u8string_view get_type_name_from_kind(uint8_t kind)
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
	identifier id;
	type_info  info;

	decl_struct(decl_struct const &) = delete;
	decl_struct(decl_struct &&)      = default;

	decl_struct(
		lex::src_tokens const &_src_tokens,
		identifier _id,
		lex::token_range _range,
		enclosing_scope_t _enclosing_scope
	)
		: id  (std::move(_id)),
		  info(_src_tokens, this->id, _range, _enclosing_scope)
	{}

	decl_struct(
		lex::src_tokens const &_src_tokens,
		identifier _id,
		lex::token_range _range,
		arena_vector<decl_variable> _generic_parameters,
		enclosing_scope_t _enclosing_scope
	)
		: id  (std::move(_id)),
		  info(_src_tokens, this->id, _range, std::move(_generic_parameters), _enclosing_scope)
	{}

	decl_struct(
		lex::src_tokens const &_src_tokens,
		identifier _id,
		enclosing_scope_t _enclosing_scope
	)
		: id  (std::move(_id)),
		  info(_src_tokens, this->id, {}, _enclosing_scope)
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
def_make_fn(statement, stmt_defer)
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

struct builtin_operator
{
	uint32_t op;
	bz::vector<decl_operator> decls;
};

bz::vector<type_info>              make_builtin_type_infos(void);
bz::vector<type_and_name_pair>     make_builtin_types    (bz::array_view<type_info> builtin_type_infos, size_t pointer_size);
bz::vector<universal_function_set> make_builtin_universal_functions(void);

scope_t make_builtin_global_scope(
	bz::array_view<decl_function *> builtin_functions,
	bz::array_view<decl_operator *> builtin_operators
);

struct intrinsic_info_t
{
	uint32_t kind;
	bz::u8string_view func_name;
};

constexpr auto intrinsic_info = []() {
	static_assert(function_body::_builtin_last - function_body::_builtin_first == 146);
	constexpr size_t size = function_body::_builtin_last - function_body::_builtin_first;
	return bz::array<intrinsic_info_t, size>{{
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
		{ function_body::builtin_swap,              "__builtin_swap"              },

		{ function_body::builtin_is_comptime,        "__builtin_is_comptime"        },
		{ function_body::builtin_is_option_set_impl, "__builtin_is_option_set_impl" },
		{ function_body::builtin_is_option_set,      "__builtin_is_option_set"      },
		{ function_body::builtin_panic,              "__builtin_panic"              },

		{ function_body::print_stdout,   "__builtin_print_stdout"   },
		{ function_body::print_stderr,   "__builtin_print_stderr"   },

		{ function_body::comptime_malloc,      "__builtin_comptime_malloc"      },
		{ function_body::comptime_malloc_type, "__builtin_comptime_malloc_type" },
		{ function_body::comptime_free,        "__builtin_comptime_free"        },

		{ function_body::comptime_compile_error,              "__builtin_comptime_compile_error"              },
		{ function_body::comptime_compile_warning,            "__builtin_comptime_compile_warning"            },
		{ function_body::comptime_compile_error_src_tokens,   "__builtin_comptime_compile_error_src_tokens"   },
		{ function_body::comptime_compile_warning_src_tokens, "__builtin_comptime_compile_warning_src_tokens" },

		{ function_body::comptime_create_global_string, "__builtin_comptime_create_global_string" },

		{ function_body::comptime_concatenate_strs, "__builtin_comptime_concatenate_strs" },
		{ function_body::comptime_format_float32,   "__builtin_comptime_format_float32"   },
		{ function_body::comptime_format_float64,   "__builtin_comptime_format_float64"   },

		{ function_body::typename_as_str, "__builtin_typename_as_str" },

		{ function_body::is_const,          "__builtin_is_const"          },
		{ function_body::is_consteval,      "__builtin_is_consteval"      },
		{ function_body::is_pointer,        "__builtin_is_pointer"        },
		{ function_body::is_reference,      "__builtin_is_reference"      },
		{ function_body::is_move_reference, "__builtin_is_move_reference" },

		{ function_body::remove_const,          "__builtin_remove_const"          },
		{ function_body::remove_consteval,      "__builtin_remove_consteval"      },
		{ function_body::remove_pointer,        "__builtin_remove_pointer"        },
		{ function_body::remove_reference,      "__builtin_remove_reference"      },
		{ function_body::remove_move_reference, "__builtin_remove_move_reference" },

		{ function_body::is_default_constructible,        "__builtin_is_default_constructible"        },
		{ function_body::is_copy_constructible,           "__builtin_is_copy_constructible"           },
		{ function_body::is_trivially_copy_constructible, "__builtin_is_trivially_copy_constructible" },
		{ function_body::is_trivially_destructible,       "__builtin_is_trivially_destructible"       },

		// llvm intrinsics (https://releases.llvm.org/10.0.0/docs/LangRef.html#standard-c-library-intrinsics)
		// and other C standard library functions

		{ function_body::lifetime_start, "__builtin_lifetime_start" },
		{ function_body::lifetime_end,   "__builtin_lifetime_end"   },

		{ function_body::memcpy,  "__builtin_memcpy"  },
		{ function_body::memmove, "__builtin_memmove" },
		{ function_body::memset,  "__builtin_memset"  },

		// C standard library math functions

		{ function_body::abs_i8,   "__builtin_abs_i8"   },
		{ function_body::abs_i16,  "__builtin_abs_i16"  },
		{ function_body::abs_i32,  "__builtin_abs_i32"  },
		{ function_body::abs_i64,  "__builtin_abs_i64"  },
		{ function_body::fabs_f32, "__builtin_fabs_f32" },
		{ function_body::fabs_f64, "__builtin_fabs_f64" },

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

struct builtin_operator_info_t
{
	uint32_t kind;
	uint32_t op;
};

constexpr auto builtin_unary_operator_info = []() {
	static_assert(function_body::_builtin_unary_operator_last - function_body::_builtin_unary_operator_first == 7);
	constexpr size_t size = function_body::_builtin_unary_operator_last - function_body::_builtin_unary_operator_first;
	return bz::array<builtin_operator_info_t, size>{{
		{ function_body::builtin_unary_plus,        lex::token::plus        },
		{ function_body::builtin_unary_minus,       lex::token::minus       },
		{ function_body::builtin_unary_dereference, lex::token::dereference },
		{ function_body::builtin_unary_bit_not,     lex::token::bit_not     },
		{ function_body::builtin_unary_bool_not,    lex::token::bool_not    },
		{ function_body::builtin_unary_plus_plus,   lex::token::plus_plus   },
		{ function_body::builtin_unary_minus_minus, lex::token::minus_minus },
	}};
}();

constexpr auto builtin_binary_operator_info = []() {
	static_assert(function_body::_builtin_binary_operator_last - function_body::_builtin_binary_operator_first == 27);
	constexpr size_t size = function_body::_builtin_binary_operator_last - function_body::_builtin_binary_operator_first;
	return bz::array<builtin_operator_info_t, size>{{
		{ function_body::builtin_binary_assign,             lex::token::assign             },
		{ function_body::builtin_binary_plus,               lex::token::plus               },
		{ function_body::builtin_binary_plus_eq,            lex::token::plus_eq            },
		{ function_body::builtin_binary_minus,              lex::token::minus              },
		{ function_body::builtin_binary_minus_eq,           lex::token::minus_eq           },
		{ function_body::builtin_binary_multiply,           lex::token::multiply           },
		{ function_body::builtin_binary_multiply_eq,        lex::token::multiply_eq        },
		{ function_body::builtin_binary_divide,             lex::token::divide             },
		{ function_body::builtin_binary_divide_eq,          lex::token::divide_eq          },
		{ function_body::builtin_binary_modulo,             lex::token::modulo             },
		{ function_body::builtin_binary_modulo_eq,          lex::token::modulo_eq          },
		{ function_body::builtin_binary_equals,             lex::token::equals             },
		{ function_body::builtin_binary_not_equals,         lex::token::not_equals         },
		{ function_body::builtin_binary_less_than,          lex::token::less_than          },
		{ function_body::builtin_binary_less_than_eq,       lex::token::less_than_eq       },
		{ function_body::builtin_binary_greater_than,       lex::token::greater_than       },
		{ function_body::builtin_binary_greater_than_eq,    lex::token::greater_than_eq    },
		{ function_body::builtin_binary_bit_and,            lex::token::bit_and            },
		{ function_body::builtin_binary_bit_and_eq,         lex::token::bit_and_eq         },
		{ function_body::builtin_binary_bit_xor,            lex::token::bit_xor            },
		{ function_body::builtin_binary_bit_xor_eq,         lex::token::bit_xor_eq         },
		{ function_body::builtin_binary_bit_or,             lex::token::bit_or             },
		{ function_body::builtin_binary_bit_or_eq,          lex::token::bit_or_eq          },
		{ function_body::builtin_binary_bit_left_shift,     lex::token::bit_left_shift     },
		{ function_body::builtin_binary_bit_left_shift_eq,  lex::token::bit_left_shift_eq  },
		{ function_body::builtin_binary_bit_right_shift,    lex::token::bit_right_shift    },
		{ function_body::builtin_binary_bit_right_shift_eq, lex::token::bit_right_shift_eq },
	}};
}();


} // namespace ast

#endif // AST_STATEMENT_H
