#include "builtin_operators.h"
#include "global_context.h"
#include "parse_context.h"
#include "token_info.h"
#include "resolve/consteval.h"
#include "resolve/expression_resolver.h"
#include "resolve/match_expression.h"
#include "overflow_operations.h"

namespace ctx
{

static auto get_base_kinds(
	ast::typespec_view lhs_t,
	ast::typespec_view rhs_t
) -> std::pair<uint32_t, uint32_t>
{
	bz_assert(lhs_t.is<ast::ts_base_type>());
	bz_assert(rhs_t.is<ast::ts_base_type>());
	return {
		lhs_t.get<ast::ts_base_type>().info->kind,
		rhs_t.get<ast::ts_base_type>().info->kind
	};
};

template<size_t kind>
static auto get_constant_expression_values(
	ast::expression const &lhs,
	ast::expression const &rhs
)
{
	static_assert(kind != ast::constant_value::aggregate);
	bz_assert(lhs.is_constant());
	bz_assert(rhs.is_constant());
	auto const &lhs_value = lhs.get_constant_value();
	auto const &rhs_value = rhs.get_constant_value();
	bz_assert(lhs_value.kind() == kind);
	bz_assert(rhs_value.kind() == kind);

	if constexpr (kind == ast::constant_value::string)
	{
		return std::make_pair(
			lhs_value.get<ast::constant_value::string>().as_string_view(),
			rhs_value.get<ast::constant_value::string>().as_string_view()
		);
	}
	else
	{
		return std::make_pair(lhs_value.get<kind>(), rhs_value.get<kind>());
	}
}

#define undeclared_unary_message(op) "no match for unary operator " op " with type '{}'"

// &val -> *typeof val
static ast::expression get_builtin_unary_address_of(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(op_kind == lex::token::address_of);
	bz_assert(expr.not_error());

	auto [type, type_kind] = expr.get_expr_type_and_kind();
	ast::typespec result_type = type;
	result_type.add_layer<ast::ts_pointer>();
	if (
		type_kind == ast::expression_type_kind::lvalue
		|| type_kind == ast::expression_type_kind::lvalue_reference
	)
	{
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			std::move(result_type),
			ast::make_expr_unary_op(op_kind, std::move(expr)),
			ast::destruct_operation()
		);
	}

	context.report_error(src_tokens, "cannot take address of an rvalue");
	return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
}

static ast::expression get_builtin_unary_dot_dot_dot(
	lex::src_tokens const &,
	uint32_t,
	ast::expression,
	parse_context &
)
{
	// this is handled in resolve_expression
	bz_unreachable;
}

// &(typename) -> (&typename)
static ast::expression get_type_op_unary_reference(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(op_kind == lex::token::ampersand);
	bz_assert(expr.not_error());
	bz_assert(expr.is_typename());

	ast::typespec result_type = expr.get_typename();
	result_type.src_tokens = src_tokens;
	if (result_type.is<ast::ts_consteval>())
	{
		context.report_error(src_tokens, "reference to consteval type is not allowed");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_lvalue_reference>())
	{
		// nothing
	}
	else if (result_type.is<ast::ts_move_reference>())
	{
		context.report_error(src_tokens, "reference to move reference type is not allowed");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_auto_reference>())
	{
		context.report_error(src_tokens, "reference to auto reference type is not allowed");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_auto_reference_const>())
	{
		context.report_error(src_tokens, "reference to auto reference-const type is not allowed");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_variadic>())
	{
		context.report_error(src_tokens, "reference to variadic type is not allowed");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else
	{
		result_type.add_layer<ast::ts_lvalue_reference>();
	}

	return ast::make_constant_expression(
		src_tokens,
		ast::expression_type_kind::type_name,
		ast::make_typename_typespec(nullptr),
		ast::constant_value(std::move(result_type)),
		ast::make_expr_unary_op(op_kind, std::move(expr))
	);
}

// #(typename) -> (#typename)
static ast::expression get_type_op_unary_auto_ref(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(op_kind == lex::token::auto_ref);
	bz_assert(expr.not_error());
	bz_assert(expr.is_typename());

	ast::typespec result_type = expr.get_typename();
	result_type.src_tokens = src_tokens;
	if (result_type.is<ast::ts_consteval>())
	{
		context.report_error(src_tokens, "auto reference to consteval type is not allowed");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_lvalue_reference>())
	{
		context.report_error(src_tokens, "auto reference to reference type is not allowed");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_move_reference>())
	{
		context.report_error(src_tokens, "auto reference to move reference type is not allowed");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_auto_reference>())
	{
		// nothing
	}
	else if (result_type.is<ast::ts_auto_reference_const>())
	{
		context.report_error(src_tokens, "auto reference to auto reference-const type is not allowed");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_variadic>())
	{
		context.report_error(src_tokens, "auto reference to variadic type is not allowed");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else
	{
		result_type.add_layer<ast::ts_auto_reference>();
	}

	return ast::make_constant_expression(
		src_tokens,
		ast::expression_type_kind::type_name,
		ast::make_typename_typespec(nullptr),
		ast::constant_value(std::move(result_type)),
		ast::make_expr_unary_op(op_kind, std::move(expr))
	);
}

// ##(typename) -> (##typename)
static ast::expression get_type_op_unary_auto_ref_const(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(op_kind == lex::token::auto_ref_const);
	bz_assert(expr.not_error());
	bz_assert(expr.is_typename());

	ast::typespec result_type = expr.get_typename();
	result_type.src_tokens = src_tokens;
	if (result_type.is<ast::ts_consteval>())
	{
		context.report_error(src_tokens, "auto reference-const to consteval type is not allowed");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_const>())
	{
		bz_assert(src_tokens.pivot != nullptr);
		context.report_error(src_tokens, "auto reference-const to const type is not allowed", {}, {
			context.make_suggestion_before(
				src_tokens.pivot,
				src_tokens.pivot->src_pos.begin, src_tokens.pivot->src_pos.end, "#",
				"use auto reference instead"
			)
		});
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_lvalue_reference>())
	{
		context.report_error(src_tokens, "auto reference-const to reference type is not allowed");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_move_reference>())
	{
		context.report_error(src_tokens, "auto reference-const to move reference type is not allowed");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_auto_reference>())
	{
		context.report_error(src_tokens, "auto reference-const to auto reference type is not allowed");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_auto_reference_const>())
	{
		// nothing
	}
	else if (result_type.is<ast::ts_variadic>())
	{
		context.report_error(src_tokens, "auto reference-const to variadic type is not allowed");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else
	{
		result_type.add_layer<ast::ts_auto_reference_const>();
	}

	return ast::make_constant_expression(
		src_tokens,
		ast::expression_type_kind::type_name,
		ast::make_typename_typespec(nullptr),
		ast::constant_value(std::move(result_type)),
		ast::make_expr_unary_op(op_kind, std::move(expr))
	);
}

// *(typename) -> (*typename)
static ast::expression get_type_op_unary_pointer(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(op_kind == lex::token::star);
	bz_assert(expr.not_error());
	bz_assert(expr.is_typename());

	ast::typespec result_type = expr.get_typename();
	result_type.src_tokens = src_tokens;
	if (result_type.is<ast::ts_lvalue_reference>())
	{
		context.report_error(src_tokens, "pointer to reference is not allowed");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_move_reference>())
	{
		context.report_error(src_tokens, "pointer to move reference is not allowed");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_auto_reference>())
	{
		context.report_error(src_tokens, "pointer to auto reference is not allowed");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_auto_reference_const>())
	{
		context.report_error(src_tokens, "pointer to auto reference-const is not allowed");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_consteval>())
	{
		context.report_error(src_tokens, "pointer to consteval is not allowed");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_variadic>())
	{
		context.report_error(src_tokens, "pointer to variadic type is not allowed");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}

	result_type.add_layer<ast::ts_pointer>();

	return ast::make_constant_expression(
		src_tokens,
		ast::expression_type_kind::type_name,
		ast::make_typename_typespec(nullptr),
		ast::constant_value(std::move(result_type)),
		ast::make_expr_unary_op(op_kind, std::move(expr))
	);
}

// ...(typename) -> (...typename)
static ast::expression get_type_op_unary_dot_dot_dot(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(op_kind == lex::token::dot_dot_dot);
	bz_assert(expr.not_error());
	bz_assert(expr.is_typename());

	ast::typespec result_type = expr.get_typename();
	result_type.src_tokens = src_tokens;
	if (result_type.is<ast::ts_variadic>())
	{
		context.report_error(src_tokens, bz::format("type '{}' is already variadic", result_type));
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}

	result_type.add_layer<ast::ts_variadic>();

	return ast::make_constant_expression(
		src_tokens,
		ast::expression_type_kind::type_name,
		ast::make_typename_typespec(nullptr),
		ast::constant_value(std::move(result_type)),
		ast::make_expr_unary_op(op_kind, std::move(expr))
	);
}

// const (typename) -> (const typename)
static ast::expression get_type_op_unary_const(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(op_kind == lex::token::kw_const);
	bz_assert(expr.not_error());
	bz_assert(expr.is_typename());

	ast::typespec result_type = expr.get_typename();
	result_type.src_tokens = src_tokens;
	if (result_type.is<ast::ts_lvalue_reference>())
	{
		context.report_error(src_tokens, "a reference type cannot be 'const'");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_move_reference>())
	{
		context.report_error(src_tokens, "a move reference type cannot be 'const'");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_auto_reference>())
	{
		context.report_error(src_tokens, "an auto reference type cannot be 'const'");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_auto_reference_const>())
	{
		context.report_error(src_tokens, "an auto reference-const type cannot be 'const'");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_const>())
	{
		// nothing
	}
	else if (result_type.is<ast::ts_consteval>())
	{
		// nothing
	}
	else if (result_type.is<ast::ts_variadic>())
	{
		context.report_error(src_tokens, "a variadic type cannot be 'const'");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else
	{
		result_type.add_layer<ast::ts_const>();
	}

	return ast::make_constant_expression(
		src_tokens,
		ast::expression_type_kind::type_name,
		ast::make_typename_typespec(nullptr),
		ast::constant_value(std::move(result_type)),
		ast::make_expr_unary_op(op_kind, std::move(expr))
	);
}

// consteval (typename) -> (consteval typename)
static ast::expression get_type_op_unary_consteval(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(op_kind == lex::token::kw_consteval);
	bz_assert(expr.not_error());
	bz_assert(expr.is_typename());

	ast::typespec result_type = expr.get_typename();
	result_type.src_tokens = src_tokens;
	if (result_type.is<ast::ts_lvalue_reference>())
	{
		context.report_error(src_tokens, "a reference type cannot be 'consteval'");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_move_reference>())
	{
		context.report_error(src_tokens, "a move reference type cannot be 'consteval'");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_auto_reference>())
	{
		context.report_error(src_tokens, "an auto reference type cannot be 'consteval'");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_auto_reference_const>())
	{
		context.report_error(src_tokens, "an auto reference-const type cannot be 'consteval'");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_const>())
	{
		// nothing
	}
	else if (result_type.is<ast::ts_consteval>())
	{
		// nothing
	}
	else if (result_type.is<ast::ts_variadic>())
	{
		context.report_error(src_tokens, "a variadic type cannot be 'consteval'");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else
	{
		result_type.add_layer<ast::ts_consteval>();
	}

	return ast::make_constant_expression(
		src_tokens,
		ast::expression_type_kind::type_name,
		ast::make_typename_typespec(nullptr),
		ast::constant_value(std::move(result_type)),
		ast::make_expr_unary_op(op_kind, std::move(expr))
	);
}

// move (typename) -> (move typename)
static ast::expression get_type_op_unary_move(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(op_kind == lex::token::kw_move);
	bz_assert(expr.not_error());
	bz_assert(expr.is_typename());

	ast::typespec result_type = expr.get_typename();
	result_type.src_tokens = src_tokens;
	if (result_type.is<ast::ts_consteval>())
	{
		context.report_error(src_tokens, "move reference to consteval type is not allowed");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_lvalue_reference>())
	{
		context.report_error(src_tokens, "move reference to reference type is not allowed");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_move_reference>())
	{
		// nothing
	}
	else if (result_type.is<ast::ts_auto_reference>())
	{
		context.report_error(src_tokens, "move reference to auto reference type is not allowed");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_auto_reference_const>())
	{
		context.report_error(src_tokens, "move reference to auto reference-const type is not allowed");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_variadic>())
	{
		context.report_error(src_tokens, "move reference to variadic type is not allowed");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else
	{
		result_type.add_layer<ast::ts_move_reference>();
	}

	return ast::make_constant_expression(
		src_tokens,
		ast::expression_type_kind::type_name,
		ast::make_typename_typespec(nullptr),
		ast::constant_value(std::move(result_type)),
		ast::make_expr_unary_op(op_kind, std::move(expr))
	);
}

static ast::expression get_builtin_unary_sizeof(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(op_kind == lex::token::kw_sizeof);
	bz_assert(expr.not_error());

	if (expr.is_typename())
	{
		auto const type = expr.get_typename().as_typespec_view();
		if (!ast::is_complete(type))
		{
			context.report_error(src_tokens, bz::format("cannot take 'sizeof' of an incomplete type '{}'", type));
			return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
		}
		else if (!context.is_instantiable(type))
		{
			context.report_error(src_tokens, bz::format("cannot take 'sizeof' of a non-instantiable type '{}'", type));
			return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
		}
		auto const size = context.get_sizeof(type);
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			ast::make_base_type_typespec({}, context.get_usize_type_info()),
			ast::constant_value(size),
			ast::make_expr_unary_op(op_kind, std::move(expr))
		);
	}
	else
	{
		auto const type = expr.get_expr_type();
		if (!ast::is_complete(type))
		{
			// this is in case type is empty; I don't see how we could get here otherwise
			context.report_error(src_tokens, bz::format("cannot take 'sizeof' of an exprssion with incomplete type '{}'", type));
			return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
		}
		else if (!context.is_instantiable(type))
		{
			context.report_error(src_tokens, bz::format("cannot take 'sizeof' of an expression with non-instantiable type '{}'", type));
			return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
		}
		auto const size = context.get_sizeof(type);
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			ast::make_base_type_typespec({}, context.get_usize_type_info()),
			ast::constant_value(size),
			ast::make_expr_unary_op(op_kind, std::move(expr))
		);
	}
}

// typeof (val) -> (typeof val)
static ast::expression get_builtin_unary_typeof(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(op_kind == lex::token::kw_typeof);
	bz_assert(expr.not_error());
	auto const [type, kind] = expr.get_expr_type_and_kind();
	if (expr.is_overloaded_function())
	{
		context.report_error(src_tokens, "type of an overloaded function is ambiguous");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (expr.is_typename())
	{
		context.report_error(src_tokens, "cannot take 'typeof' of a type");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}

	ast::typespec result_type = type;
	result_type.src_tokens = src_tokens;
	bz_assert(type.has_value());
	if (kind == ast::expression_type_kind::lvalue_reference)
	{
		result_type.add_layer<ast::ts_lvalue_reference>();
	}
	return ast::make_constant_expression(
		src_tokens,
		ast::expression_type_kind::type_name,
		ast::make_typename_typespec(nullptr),
		ast::constant_value(std::move(result_type)),
		ast::make_expr_unary_op(op_kind, std::move(expr))
	);
}

// move (val) -> (typeof val without lvalue ref)
static ast::expression get_builtin_unary_move(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(op_kind == lex::token::kw_move);
	bz_assert(expr.not_error());
	auto const [type, kind] = expr.get_expr_type_and_kind();

	if (kind == ast::expression_type_kind::lvalue)
	{
		bz_assert(expr.get_expr().is<ast::expr_identifier>());
		auto const decl = expr.get_expr().get<ast::expr_identifier>().decl;
		context.register_move(src_tokens, decl);
		ast::typespec result_type = ast::remove_const_or_consteval(type);
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::moved_lvalue,
			result_type,
			ast::make_expr_unary_op(op_kind, std::move(expr)),
			ast::destruct_operation()
		);
	}
	else if (kind == ast::expression_type_kind::lvalue_reference)
	{
		context.report_error(src_tokens, "operator move cannot be applied to an lvalue reference");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else
	{
		context.report_error(src_tokens, "invalid use of operator move");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
}

// __move__ (val) -> (typeof val without lvalue ref)
static ast::expression get_builtin_unary_unsafe_move(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(op_kind == lex::token::kw_unsafe_move);
	bz_assert(expr.not_error());
	auto const [type, kind] = expr.get_expr_type_and_kind();

	if (
		kind == ast::expression_type_kind::lvalue
		|| (kind == ast::expression_type_kind::lvalue_reference && !type.is<ast::ts_const>())
	)
	{
		ast::typespec result_type = type;
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::moved_lvalue,
			result_type,
			ast::make_expr_unary_op(op_kind, std::move(expr)),
			ast::destruct_operation()
		);
	}
	else if (kind == ast::expression_type_kind::lvalue_reference)
	{
		context.report_error(src_tokens, "operator __move__ cannot be applied to a const lvalue reference");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else
	{
		context.report_error(src_tokens, "invalid use of operator __move__");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
}

// __forward (val) -> (rvalue of typeof val)
static ast::expression get_builtin_unary_forward(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression expr,
	parse_context &
)
{
	bz_assert(op_kind == lex::token::kw_forward);
	bz_assert(expr.not_error());
	auto const [type, kind] = expr.get_expr_type_and_kind();

	if (kind == ast::expression_type_kind::lvalue)
	{
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::moved_lvalue,
			type,
			ast::make_expr_unary_op(lex::token::kw_move, std::move(expr)),
			ast::destruct_operation()
		);
	}
	else
	{
		return expr;
	}
}

// consteval (val) -> (force evaluate val at compile time)
static ast::expression get_builtin_unary_consteval(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(op_kind == lex::token::kw_consteval);
	bz_assert(expr.not_error());

	auto auto_type = ast::make_auto_typespec(nullptr);
	resolve::match_expression_to_type(expr, auto_type, context);
	resolve::consteval_try(expr, context);
	if (expr.has_consteval_succeeded())
	{
		ast::typespec type = expr.get_expr_type();
		auto const type_kind = expr.get_expr_type_and_kind().second;
		auto value = expr.get_constant_value();
		return ast::make_constant_expression(
			src_tokens,
			type_kind, std::move(type),
			std::move(value),
			ast::make_expr_unary_op(op_kind, std::move(expr))
		);
	}
	else
	{
		context.report_error(expr.src_tokens, "failed to evaluate expression at compile time", resolve::get_consteval_fail_notes(expr));
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
}

#undef undeclared_unary_message


#define undeclared_binary_message(op) "no match for binary operator " op " with types '{}' and '{}'"

// uintN <<>> uintM -> uintN
// uintN <<>> intM -> uintN
// bool &&^^|| bool -> bool
static ast::expression get_builtin_binary_bool_and_xor_or(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
)
{
	bz_assert(
		op_kind == lex::token::bool_and
		|| op_kind == lex::token::bool_xor
		|| op_kind == lex::token::bool_or
	);
	bz_assert(lhs.not_error());
	bz_assert(rhs.not_error());
	auto const [lhs_type, lhs_type_kind] = lhs.get_expr_type_and_kind();
	auto const [rhs_type, rhs_type_kind] = rhs.get_expr_type_and_kind();
	auto const lhs_t = ast::remove_const_or_consteval(lhs_type);
	auto const rhs_t = ast::remove_const_or_consteval(rhs_type);

	auto const op_str = [op_kind]() -> bz::u8string_view {
		switch (op_kind)
		{
		case lex::token::bool_and:
			return "&&";
		case lex::token::bool_xor:
			return "^^";
		case lex::token::bool_or:
			return "||";
		default:
			bz_unreachable;
		}
	}();

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto const [lhs_kind, rhs_kind] = get_base_kinds(lhs_t, rhs_t);
		if (
			lhs_kind == ast::type_info::bool_
			&& lhs_kind == ast::type_info::bool_
		)
		{
			auto result_type = lhs_t;
			return ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::rvalue, std::move(result_type),
				ast::make_expr_binary_op(op_kind, std::move(lhs), std::move(rhs)),
				ast::destruct_operation()
			);
		}
	}

	context.report_error(
		src_tokens,
		bz::format(undeclared_binary_message("{}"), op_str, lhs_type, rhs_type)
	);
	return ast::make_error_expression(src_tokens, ast::make_expr_binary_op(op_kind, std::move(lhs), std::move(rhs)));
}

// T, U -> U
static ast::expression get_builtin_binary_comma(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression lhs,
	ast::expression rhs,
	[[maybe_unused]] parse_context &context
)
{
	// TODO add warning if lhs has no side effects
	auto const [type, type_kind] = rhs.get_expr_type_and_kind();
	auto result_type = type;

	context.add_self_destruction(lhs);

	return ast::make_dynamic_expression(
		src_tokens,
		type_kind,
		std::move(result_type),
		ast::make_expr_binary_op(op_kind, std::move(lhs), std::move(rhs)),
		ast::destruct_operation()
	);
}


ast::expression make_builtin_cast(
	lex::src_tokens const &src_tokens,
	ast::expression expr,
	ast::typespec dest_type,
	parse_context &context
)
{
	bz_assert(expr.not_error());
	auto const [expr_type, expr_type_kind] = expr.get_expr_type_and_kind();
	auto const expr_t = ast::remove_const_or_consteval(expr_type);
	auto const dest_t = ast::remove_const_or_consteval(dest_type);
	bz_assert(ast::is_complete(dest_t));

	// case from null to a pointer type
	if (
		dest_t.is<ast::ts_pointer>()
		&& ((
			expr.is_constant()
			&& expr.get_constant_value().kind() == ast::constant_value::null
		)
		|| (
			expr.is_dynamic()
			&& [&]() {
				auto const type = ast::remove_const_or_consteval(expr.get_dynamic().type);
				return type.is<ast::ts_base_type>()
					&& type.get<ast::ts_base_type>().info->kind == ast::type_info::null_t_;
			}()
		))
	)
	{
		ast::typespec dest_t_copy = dest_t;
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			std::move(dest_t_copy),
			ast::constant_value(ast::internal::null_t{}),
			ast::make_expr_cast(std::move(expr), std::move(dest_type))
		);
	}
	else if (dest_t.is<ast::ts_pointer>() && expr_t.is<ast::ts_pointer>())
	{
		auto inner_dest_t = dest_t.get<ast::ts_pointer>();
		auto inner_expr_t = expr_t.get<ast::ts_pointer>();
		if (!inner_dest_t.is<ast::ts_const>() && inner_expr_t.is<ast::ts_const>())
		{
			context.report_error(
				src_tokens,
				bz::format("invalid conversion from '{}' to '{}'", expr_type, dest_type)
			);
			return ast::make_error_expression(src_tokens, ast::make_expr_cast(std::move(expr), std::move(dest_type)));
		}
		inner_dest_t = ast::remove_const_or_consteval(inner_dest_t);
		inner_expr_t = ast::remove_const_or_consteval(inner_expr_t);
		while (inner_dest_t.is_safe_blind_get() && inner_expr_t.is_safe_blind_get() && inner_dest_t.kind() == inner_expr_t.kind())
		{
			inner_dest_t = inner_dest_t.blind_get();
			inner_expr_t = inner_expr_t.blind_get();
		}
		if (
			inner_dest_t.is<ast::ts_void>()
			|| (
				inner_dest_t.is<ast::ts_base_type>()
				&& inner_expr_t.is<ast::ts_base_type>()
				&& inner_dest_t.get<ast::ts_base_type>().info->kind == inner_expr_t.get<ast::ts_base_type>().info->kind
			)
		)
		{
			ast::typespec dest_t_copy = dest_t;
			return ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::rvalue,
				std::move(dest_t_copy),
				ast::make_expr_cast(std::move(expr), std::move(dest_type)),
				ast::destruct_operation()
			);
		}
		else
		{
			context.report_error(
				src_tokens,
				bz::format("invalid conversion from '{}' to '{}'", expr_type, dest_type)
			);
			return ast::make_error_expression(src_tokens, ast::make_expr_cast(std::move(expr), std::move(dest_type)));
		}
	}
	else if (dest_t.is<ast::ts_array_slice>() && expr_t.is<ast::ts_array>())
	{
		ast::typespec dest_t_copy = dest_t;
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			std::move(dest_t_copy),
			ast::make_expr_cast(std::move(expr), std::move(dest_type)),
			ast::destruct_operation()
		);
	}
	else if (!dest_t.is<ast::ts_base_type>())
	{
		context.report_error(
			src_tokens,
			bz::format("invalid conversion from '{}' to '{}'", expr_type, dest_type)
		);
		return ast::make_error_expression(src_tokens, ast::make_expr_cast(std::move(expr), std::move(dest_type)));
	}
	else if (expr_t.is<ast::ts_base_type>())
	{
		auto const [expr_kind, dest_kind] = get_base_kinds(expr_t, dest_t);
		if (ast::is_arithmetic_kind(expr_kind) && ast::is_arithmetic_kind(dest_kind))
		{
			ast::typespec dest_t_copy = dest_t;
			return ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::rvalue,
				std::move(dest_t_copy),
				ast::make_expr_cast(std::move(expr), std::move(dest_type)),
				ast::destruct_operation()
			);
		}
		else if (expr_kind == ast::type_info::char_ && ast::is_integer_kind(dest_kind))
		{
			ast::typespec dest_t_copy = dest_t;
			return ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::rvalue,
				std::move(dest_t_copy),
				ast::make_expr_cast(std::move(expr), std::move(dest_type)),
				ast::destruct_operation()
			);
		}
		else if (ast::is_integer_kind(expr_kind) && dest_kind == ast::type_info::char_)
		{
			ast::typespec dest_t_copy = dest_t;
			return ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::rvalue,
				std::move(dest_t_copy),
				ast::make_expr_cast(std::move(expr), std::move(dest_type)),
				ast::destruct_operation()
			);
		}
		else if (expr_kind == ast::type_info::bool_ && ast::is_integer_kind(dest_kind))
		{
			ast::typespec dest_t_copy = dest_t;
			return ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::rvalue,
				std::move(dest_t_copy),
				ast::make_expr_cast(std::move(expr), std::move(dest_type)),
				ast::destruct_operation()
			);
		}

		context.report_error(
			src_tokens,
			bz::format("invalid conversion from '{}' to '{}'", expr_type, dest_type)
		);
		return ast::make_error_expression(src_tokens, ast::make_expr_cast(std::move(expr), std::move(dest_type)));
	}
	else
	{
		context.report_error(
			src_tokens,
			bz::format("invalid conversion from '{}' to '{}'", expr_type, dest_type)
		);
		return ast::make_error_expression(src_tokens, ast::make_expr_cast(std::move(expr), std::move(dest_type)));
	}
}

ast::expression make_builtin_subscript_operator(
	lex::src_tokens const &src_tokens,
	ast::expression called,
	ast::expression arg,
	parse_context &context
)
{
	auto const [called_type, called_kind] = called.get_expr_type_and_kind();
	auto const called_t = ast::remove_const_or_consteval(called_type);

	if (called_t.is<ast::ts_tuple>() || called.is_tuple())
	{
		if (!arg.is_constant())
		{
			context.report_error(arg, "tuple subscript must be a constant expression");
			return ast::make_error_expression(src_tokens, ast::make_expr_subscript(std::move(called), std::move(arg)));
		}

		auto const [arg_type, _] = arg.get_expr_type_and_kind();
		auto const arg_t = ast::remove_const_or_consteval(arg_type);
		if (!arg_t.is<ast::ts_base_type>() || !ast::is_integer_kind(arg_t.get<ast::ts_base_type>().info->kind))
		{
			context.report_error(arg, bz::format("invalid type '{}' for tuple subscript", arg_type));
			return ast::make_error_expression(src_tokens, ast::make_expr_subscript(std::move(called), std::move(arg)));
		}

		auto const tuple_elem_count = called_t.is<ast::ts_tuple>()
			? called_t.get<ast::ts_tuple>().types.size()
			: called.get_expr().get<ast::expr_tuple>().elems.size();
		auto &const_arg = arg.get_constant();
		size_t index = 0;
		if (const_arg.value.kind() == ast::constant_value::uint)
		{
			auto const value = const_arg.value.get<ast::constant_value::uint>();
			if (value >= tuple_elem_count)
			{
				context.report_error(arg, bz::format("index {} is out of range for tuple type '{}'", value, called_type));
				return ast::make_error_expression(src_tokens, ast::make_expr_subscript(std::move(called), std::move(arg)));
			}
			index = value;
		}
		else
		{
			bz_assert(const_arg.value.kind() == ast::constant_value::sint);
			auto const value = const_arg.value.get<ast::constant_value::sint>();
			if (value < 0 || static_cast<size_t>(value) >= tuple_elem_count)
			{
				context.report_error(arg, bz::format("index {} is out of range for tuple type '{}'", value, called_type));
				return ast::make_error_expression(src_tokens, ast::make_expr_subscript(std::move(called), std::move(arg)));
			}
			index = static_cast<size_t>(value);
		}

		if (called.is_tuple())
		{
			auto &tuple = called.get_tuple();
			auto &result_elem = tuple.elems[index];
			auto [result_type, result_kind] = result_elem.get_expr_type_and_kind();

			for (auto const i : bz::iota(0, tuple.elems.size()))
			{
				if (i != index)
				{
					context.add_self_destruction(tuple.elems[i]);
				}
			}

			return ast::make_dynamic_expression(
				src_tokens,
				result_kind, std::move(result_type),
				ast::make_expr_tuple_subscript(std::move(tuple), std::move(arg)),
				ast::destruct_operation()
			);
		}
		else
		{
			auto &tuple_t = called_t.get<ast::ts_tuple>();
			ast::typespec result_type = tuple_t.types[index];
			if (
				!result_type.is<ast::ts_const>()
				&& !result_type.is<ast::ts_lvalue_reference>()
				&& called_type.is<ast::ts_const>()
			)
			{
				result_type.add_layer<ast::ts_const>();
			}

			auto const result_kind = result_type.is<ast::ts_lvalue_reference>()
				? ast::expression_type_kind::lvalue_reference
				: called_kind;

			if (result_type.is<ast::ts_lvalue_reference>())
			{
				result_type.remove_layer();
			}

			return ast::make_dynamic_expression(
				src_tokens,
				result_kind, std::move(result_type),
				ast::make_expr_subscript(std::move(called), std::move(arg)),
				ast::destruct_operation()
			);
		}
	}
	else if (called_t.is<ast::ts_array_slice>())
	{
		bz_assert(called_t.is<ast::ts_array_slice>());
		auto &array_slice_t = called_t.get<ast::ts_array_slice>();

		auto const [arg_type, _] = arg.get_expr_type_and_kind();
		auto const arg_t = ast::remove_const_or_consteval(arg_type);
		if (!arg_t.is<ast::ts_base_type>() || !ast::is_integer_kind(arg_t.get<ast::ts_base_type>().info->kind))
		{
			context.report_error(arg, bz::format("invalid type '{}' for array slice subscript", arg_type));
			return ast::make_error_expression(src_tokens, ast::make_expr_subscript(std::move(called), std::move(arg)));
		}

		auto result_type = array_slice_t.elem_type;

		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::lvalue, std::move(result_type),
			ast::make_expr_subscript(std::move(called), std::move(arg)),
			ast::destruct_operation()
		);
	}
	else // if (called_t.is<ast::ts_array>())
	{
		bz_assert(called_t.is<ast::ts_array>());
		auto &array_t = called_t.get<ast::ts_array>();

		auto const [arg_type, _] = arg.get_expr_type_and_kind();
		auto const arg_t = ast::remove_const_or_consteval(arg_type);
		if (!arg_t.is<ast::ts_base_type>() || !ast::is_integer_kind(arg_t.get<ast::ts_base_type>().info->kind))
		{
			context.report_error(arg, bz::format("invalid type '{}' for array subscript", arg_type));
			return ast::make_error_expression(src_tokens, ast::make_expr_subscript(std::move(called), std::move(arg)));
		}

		auto const result_kind = called_kind;
		ast::typespec result_type = array_t.elem_type;

		if (called_type.is<ast::ts_const>() || called_type.is<ast::ts_consteval>())
		{
			result_type.add_layer<ast::ts_const>();
		}

		return ast::make_dynamic_expression(
			src_tokens,
			result_kind, std::move(result_type),
			ast::make_expr_subscript(std::move(called), std::move(arg)),
			ast::destruct_operation()
		);
	}
}

static ast::expression get_type_op_binary_equals_not_equals(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
)
{
	bz_assert(op_kind == lex::token::equals || op_kind == lex::token::not_equals);
	bz_assert(lhs.not_error());
	bz_assert(rhs.not_error());
	bz_assert(lhs.is_typename());
	bz_assert(rhs.is_typename());

	auto const op_str = token_info[op_kind].token_value;

	auto &lhs_type = lhs.get_typename();
	auto &rhs_type = rhs.get_typename();

	bool good = true;
	if (!ast::is_complete(lhs_type))
	{
		context.report_error(
			lhs,
			bz::format("type argument to operator {} must be a complete type", op_str)
		);
		good = false;
	}
	if (!ast::is_complete(rhs_type))
	{
		context.report_error(
			rhs,
			bz::format("type argument to operator {} must be a complete type", op_str)
		);
		good = false;
	}
	if (!good)
	{
		return ast::make_error_expression(src_tokens, ast::make_expr_binary_op(op_kind, std::move(lhs), std::move(rhs)));
	}

	auto const are_types_equal = lhs_type == rhs_type;
	auto const result = op_kind == lex::token::equals ? are_types_equal : !are_types_equal;

	return ast::make_constant_expression(
		src_tokens,
		ast::expression_type_kind::rvalue,
		ast::typespec(src_tokens, { ast::ts_base_type{ context.get_builtin_type_info(ast::type_info::bool_) } }),
		ast::constant_value(result),
		ast::make_expr_binary_op(op_kind, std::move(lhs), std::move(rhs))
	);
}

struct unary_operator_parse_function_t
{
	using fn_t = ast::expression (*)(lex::src_tokens const &, uint32_t, ast::expression, parse_context &);

	uint32_t kind;
	fn_t     parse_function;
};

constexpr auto builtin_unary_operators = []() {
	using T = unary_operator_parse_function_t;
	auto result = bz::array{
		T{ lex::token::address_of,     &get_builtin_unary_address_of            }, // &
		T{ lex::token::dot_dot_dot,    &get_builtin_unary_dot_dot_dot           }, // ...

		T{ lex::token::kw_sizeof,      &get_builtin_unary_sizeof                }, // sizeof
		T{ lex::token::kw_typeof,      &get_builtin_unary_typeof                }, // typeof
		T{ lex::token::kw_move,        &get_builtin_unary_move                  }, // move
		T{ lex::token::kw_unsafe_move, &get_builtin_unary_unsafe_move           }, // __move__
		T{ lex::token::kw_forward,     &get_builtin_unary_forward               }, // __forward
		T{ lex::token::kw_consteval,   &get_builtin_unary_consteval             }, // consteval
	};

	auto const builtin_unary_count = []() {
		size_t count = 0;
		for (auto &ti : token_info)
		{
			if (ti.kind < token_info.size() && is_unary_builtin_operator(ti.kind) && !is_unary_overloadable_operator(ti.kind))
			{
				++count;
			}
		}
		return count;
	}();

	if (builtin_unary_count != result.size())
	{
		exit(1);
	}

	constexpr_bubble_sort(
		result,
		[](auto lhs, auto rhs) {
			return lhs.kind < rhs.kind;
		},
		[](auto &lhs, auto &rhs) {
			auto const tmp = lhs;
			lhs = rhs;
			rhs = tmp;
		}
	);

	return result;
}();

constexpr auto type_op_unary_operators = []() {
	using T = unary_operator_parse_function_t;
	auto result = bz::array{
		T{ lex::token::address_of,     &get_type_op_unary_reference      }, // &
		T{ lex::token::auto_ref,       &get_type_op_unary_auto_ref       }, // #
		T{ lex::token::auto_ref_const, &get_type_op_unary_auto_ref_const }, // ##
		T{ lex::token::dereference,    &get_type_op_unary_pointer        }, // *
		T{ lex::token::dot_dot_dot,    &get_type_op_unary_dot_dot_dot    }, // ...
		T{ lex::token::kw_const,       &get_type_op_unary_const          }, // const
		T{ lex::token::kw_consteval,   &get_type_op_unary_consteval      }, // consteval
		T{ lex::token::kw_move,        &get_type_op_unary_move           }, // move
	};

	auto const type_op_unary_count = []() {
		size_t count = 0;
		for (auto &ti : token_info)
		{
			if (ti.kind < token_info.size() && is_unary_type_op(ti.kind))
			{
				++count;
			}
		}
		return count;
	}();

	if (type_op_unary_count != result.size())
	{
		exit(1);
	}

	constexpr_bubble_sort(
		result,
		[](auto lhs, auto rhs) {
			return lhs.kind < rhs.kind;
		},
		[](auto &lhs, auto &rhs) {
			auto const tmp = lhs;
			lhs = rhs;
			rhs = tmp;
		}
	);

	return result;
}();

struct binary_operator_parse_function_t
{
	using fn_t = ast::expression (*)(lex::src_tokens const &, uint32_t, ast::expression, ast::expression, parse_context &);

	uint32_t kind;
	fn_t     parse_function;
};

constexpr auto builtin_binary_operators = []() {
	using T = binary_operator_parse_function_t;
	auto result = bz::array{
		T{ lex::token::bool_and,           &get_builtin_binary_bool_and_xor_or    }, // &&
		T{ lex::token::bool_xor,           &get_builtin_binary_bool_and_xor_or    }, // ^^
		T{ lex::token::bool_or,            &get_builtin_binary_bool_and_xor_or    }, // ||
		T{ lex::token::comma,              &get_builtin_binary_comma              }, // ,
	};

	auto const builtin_binary_count = []() {
		size_t count = 0;
		for (auto &ti : token_info)
		{
			if (ti.kind < token_info.size() && is_binary_builtin_operator(ti.kind) && !is_binary_overloadable_operator(ti.kind))
			{
				++count;
			}
		}
		return count;
	}();

	if (builtin_binary_count != result.size())
	{
		exit(1);
	}

	constexpr_bubble_sort(
		result,
		[](auto lhs, auto rhs) {
			return lhs.kind < rhs.kind;
		},
		[](auto &lhs, auto &rhs) {
			auto const tmp = lhs;
			lhs = rhs;
			rhs = tmp;
		}
	);

	return result;
}();

constexpr auto type_op_binary_operators = []() {
	using T = binary_operator_parse_function_t;
	auto result = bz::array{
		T{ lex::token::equals,     &get_type_op_binary_equals_not_equals }, // ==
		T{ lex::token::not_equals, &get_type_op_binary_equals_not_equals }, // !=
	};

	auto const type_op_binary_count = []() {
		size_t count = 0;
		for (auto &ti : token_info)
		{
			if (ti.kind < token_info.size() && is_binary_type_op(ti.kind))
			{
				++count;
			}
		}
		return count;
	}();

	if (type_op_binary_count != result.size())
	{
		exit(1);
	}

	constexpr_bubble_sort(
		result,
		[](auto lhs, auto rhs) {
			return lhs.kind < rhs.kind;
		},
		[](auto &lhs, auto &rhs) {
			auto const tmp = lhs;
			lhs = rhs;
			rhs = tmp;
		}
	);

	return result;
}();

ast::expression make_builtin_operation(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression expr,
	parse_context &context
)
{
	return [&]<size_t ...Ns>(bz::meta::index_sequence<Ns...>) {
		ast::expression result;
		((
			op_kind == builtin_unary_operators[Ns].kind
			? (void)(result = builtin_unary_operators[Ns].parse_function(src_tokens, op_kind, std::move(expr), context))
			: (void)0
		), ...);
		return result;
	}(bz::meta::make_index_sequence<builtin_unary_operators.size()>{});
}

ast::expression make_builtin_type_operation(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression expr,
	parse_context &context
)
{
	return [&]<size_t ...Ns>(bz::meta::index_sequence<Ns...>) {
		ast::expression result;
		((
			op_kind == type_op_unary_operators[Ns].kind
			? (void)(result = type_op_unary_operators[Ns].parse_function(src_tokens, op_kind, std::move(expr), context))
			: (void)0
		), ...);
		return result;
	}(bz::meta::make_index_sequence<type_op_unary_operators.size()>{});
}

ast::expression make_builtin_operation(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
)
{
	return [&]<size_t ...Ns>(bz::meta::index_sequence<Ns...>) {
		ast::expression result;
		((
			op_kind == builtin_binary_operators[Ns].kind
			? (void)(result = builtin_binary_operators[Ns].parse_function(src_tokens, op_kind, std::move(lhs), std::move(rhs), context))
			: (void)0
		), ...);
		return result;
	}(bz::meta::make_index_sequence<builtin_binary_operators.size()>{});
}

ast::expression make_builtin_type_operation(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
)
{
	return [&]<size_t ...Ns>(bz::meta::index_sequence<Ns...>) {
		ast::expression result;
		((
			op_kind == type_op_binary_operators[Ns].kind
			? (void)(result = type_op_binary_operators[Ns].parse_function(src_tokens, op_kind, std::move(lhs), std::move(rhs), context))
			: (void)0
		), ...);
		return result;
	}(bz::meta::make_index_sequence<type_op_binary_operators.size()>{});
}


static constexpr auto int32_min = std::numeric_limits<int32_t>::min();
static constexpr auto int32_max = std::numeric_limits<int32_t>::max();
static constexpr auto int64_min = std::numeric_limits<int64_t>::min();
static constexpr auto int64_max = std::numeric_limits<int64_t>::max();
static constexpr auto uint32_max = std::numeric_limits<uint32_t>::max();
static constexpr auto uint64_max = std::numeric_limits<uint64_t>::max();

static ast::typespec get_literal_integer_type(
	lex::src_tokens const &src_tokens,
	ast::literal_kind kind,
	uint64_t value,
	parse_context &context
)
{
	auto const [default_type_info, wide_default_type_info] = [&]() -> std::pair<ast::type_info *, ast::type_info *> {
		if (kind == ast::literal_kind::integer || kind == ast::literal_kind::signed_integer)
		{
			return {
				context.get_builtin_type_info(ast::type_info::int32_),
				context.get_builtin_type_info(ast::type_info::int64_)
			};
		}
		else
		{
			return {
				context.get_builtin_type_info(ast::type_info::uint32_),
				context.get_builtin_type_info(ast::type_info::uint64_)
			};
		}
	}();
	auto const [default_max_value, wide_default_max_value] = [&]() -> std::pair<uint64_t, uint64_t> {
		if (kind == ast::literal_kind::integer || kind == ast::literal_kind::signed_integer)
		{
			return {
				static_cast<uint64_t>(int32_max),
				static_cast<uint64_t>(int64_max),
			};
		}
		else
		{
			return {
				static_cast<uint64_t>(uint32_max),
				static_cast<uint64_t>(uint64_max),
			};
		}
	}();

	if (value <= default_max_value)
	{
		return ast::make_base_type_typespec(src_tokens, default_type_info);
	}
	else if (value <= wide_default_max_value)
	{
		return ast::make_base_type_typespec(src_tokens, wide_default_type_info);
	}
	else
	{
		return ast::make_base_type_typespec(src_tokens, context.get_builtin_type_info(ast::type_info::uint64_));
	}
}

static ast::typespec get_literal_integer_type(
	lex::src_tokens const &src_tokens,
	ast::literal_kind kind,
	int64_t value,
	parse_context &context
)
{
	bz_assert(kind == ast::literal_kind::integer || kind == ast::literal_kind::signed_integer);

	if (value <= int32_max && value >= int32_min)
	{
		return ast::make_base_type_typespec(src_tokens, context.get_builtin_type_info(ast::type_info::int32_));
	}
	else
	{
		return ast::make_base_type_typespec(src_tokens, context.get_builtin_type_info(ast::type_info::int64_));
	}
}

static ast::expression make_unary_plus_literal_operation(
	lex::src_tokens const &src_tokens,
	ast::literal_kind kind,
	ast::constant_value const &value,
	parse_context &context
)
{
	// we don't really do anything with the value here
	return ast::make_constant_expression(
		src_tokens,
		ast::expression_type_kind::integer_literal,
		value.is<ast::constant_value::sint>()
			? get_literal_integer_type(src_tokens, kind, value.get<ast::constant_value::sint>(), context)
			: get_literal_integer_type(src_tokens, kind, value.get<ast::constant_value::uint>(), context),
		value,
		ast::make_expr_integer_literal(kind)
	);
}

static ast::expression make_unary_minus_literal_operation(
	lex::src_tokens const &src_tokens,
	ast::literal_kind kind,
	ast::constant_value const &value,
	parse_context &context
)
{
	bz_assert(kind == ast::literal_kind::integer || kind == ast::literal_kind::signed_integer);
	bz_assert(value.is<ast::constant_value::sint>()); // uint can't match to 'operator -'
	auto const int_value = value.get<ast::constant_value::sint>();
	if (int_value == int64_min)
	{
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::integer_literal,
			ast::make_base_type_typespec(src_tokens, context.get_builtin_type_info(ast::type_info::uint64_)),
			ast::constant_value(static_cast<uint64_t>(int64_max) + 1),
			ast::make_expr_integer_literal(kind)
		);
	}
	else
	{
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::integer_literal,
			get_literal_integer_type(src_tokens, kind, -int_value, context),
			ast::constant_value(-int_value),
			ast::make_expr_integer_literal(kind)
		);
	}
}

ast::expression make_unary_literal_operation(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression const &expr,
	parse_context &context
)
{
	auto const [kind, value] = expr.get_integer_literal_kind_and_value();
	switch (op_kind)
	{
	case lex::token::plus:
		return make_unary_plus_literal_operation(src_tokens, kind, value, context);
	case lex::token::minus:
		return make_unary_minus_literal_operation(src_tokens, kind, value, context);
	// case lex::token::bit_not:
	default:
		return ast::expression();
	}
}

static ast::expression make_binary_plus_literal_operation(
	lex::src_tokens const &src_tokens,
	ast::literal_kind lhs_kind,
	ast::literal_kind rhs_kind,
	ast::constant_value const &lhs_value,
	ast::constant_value const &rhs_value,
	parse_context &context
)
{
	if (lhs_value.is<ast::constant_value::uint>() || rhs_value.is<ast::constant_value::uint>())
	{
		bz_assert(lhs_kind == ast::literal_kind::integer || lhs_kind == ast::literal_kind::unsigned_integer);
		bz_assert(rhs_kind == ast::literal_kind::integer || rhs_kind == ast::literal_kind::unsigned_integer);
		bz_assert(lhs_kind != ast::literal_kind::integer || lhs_value.is<ast::constant_value::uint>() || lhs_value.get<ast::constant_value::sint>() >= 0);
		bz_assert(rhs_kind != ast::literal_kind::integer || rhs_value.is<ast::constant_value::uint>() || rhs_value.get<ast::constant_value::sint>() >= 0);
		auto const lhs = lhs_value.is<ast::constant_value::uint>()
			? lhs_value.get<ast::constant_value::uint>()
			: static_cast<uint64_t>(lhs_value.get<ast::constant_value::sint>());
		auto const rhs = rhs_value.is<ast::constant_value::uint>()
			? rhs_value.get<ast::constant_value::uint>()
			: static_cast<uint64_t>(rhs_value.get<ast::constant_value::sint>());

		auto const is_unsigned = lhs_kind == ast::literal_kind::unsigned_integer || rhs_kind == ast::literal_kind::unsigned_integer;
		auto const kind = is_unsigned ? ast::literal_kind::unsigned_integer : ast::literal_kind::integer;
		auto const [result_value, overflowed] = add_overflow(lhs, rhs);
		if (!overflowed)
		{
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::integer_literal,
				get_literal_integer_type(src_tokens, kind, result_value, context),
				ast::constant_value(result_value),
				ast::make_expr_integer_literal(kind)
			);
		}

		return ast::expression();
	}
	else
	{
		auto const lhs = lhs_value.get<ast::constant_value::sint>();
		auto const rhs = rhs_value.get<ast::constant_value::sint>();

		auto const is_signed = lhs_kind == ast::literal_kind::signed_integer || rhs_kind == ast::literal_kind::signed_integer;
		auto const kind = is_signed ? ast::literal_kind::signed_integer : ast::literal_kind::integer;
		auto const [signed_result_value, signed_overflowed] = add_overflow(lhs, rhs);
		if (!signed_overflowed)
		{
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::integer_literal,
				get_literal_integer_type(src_tokens, kind, signed_result_value, context),
				ast::constant_value(signed_result_value),
				ast::make_expr_integer_literal(kind)
			);
		}
		else if (is_signed)
		{
			// explicitly signed integer overflow isn't handled
			return ast::expression();
		}

		auto const [unsigned_result_value, unsigned_overflowed] = add_overflow<uint64_t>(lhs, rhs);
		if (!unsigned_overflowed)
		{
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::integer_literal,
				get_literal_integer_type(src_tokens, kind, unsigned_result_value, context),
				ast::constant_value(unsigned_result_value),
				ast::make_expr_integer_literal(kind)
			);
		}

		return ast::expression();
	}
}

static ast::expression make_binary_minus_literal_operation(
	lex::src_tokens const &src_tokens,
	ast::literal_kind lhs_kind,
	ast::literal_kind rhs_kind,
	ast::constant_value const &lhs_value,
	ast::constant_value const &rhs_value,
	parse_context &context
)
{
	if (lhs_value.is<ast::constant_value::uint>() || rhs_value.is<ast::constant_value::uint>())
	{
		bz_assert(lhs_kind == ast::literal_kind::integer || lhs_kind == ast::literal_kind::unsigned_integer);
		bz_assert(rhs_kind == ast::literal_kind::integer || rhs_kind == ast::literal_kind::unsigned_integer);
		bz_assert(lhs_kind != ast::literal_kind::integer || lhs_value.is<ast::constant_value::uint>() || lhs_value.get<ast::constant_value::sint>() >= 0);
		bz_assert(rhs_kind != ast::literal_kind::integer || rhs_value.is<ast::constant_value::uint>() || rhs_value.get<ast::constant_value::sint>() >= 0);
		auto const lhs = lhs_value.is<ast::constant_value::uint>()
			? lhs_value.get<ast::constant_value::uint>()
			: static_cast<uint64_t>(lhs_value.get<ast::constant_value::sint>());
		auto const rhs = rhs_value.is<ast::constant_value::uint>()
			? rhs_value.get<ast::constant_value::uint>()
			: static_cast<uint64_t>(rhs_value.get<ast::constant_value::sint>());

		auto const is_unsigned = lhs_kind == ast::literal_kind::unsigned_integer || rhs_kind == ast::literal_kind::unsigned_integer;
		auto const kind = is_unsigned ? ast::literal_kind::unsigned_integer : ast::literal_kind::integer;
		auto const [unsigned_result_value, unsigned_overflowed] = sub_overflow(lhs, rhs);
		if (!unsigned_overflowed)
		{
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::integer_literal,
				get_literal_integer_type(src_tokens, kind, unsigned_result_value, context),
				ast::constant_value(unsigned_result_value),
				ast::make_expr_integer_literal(kind)
			);
		}
		else if (is_unsigned)
		{
			return ast::expression();
		}

		auto const [signed_result_value, signed_overflowed] = sub_overflow<int64_t>(lhs, rhs);
		if (!signed_overflowed)
		{
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::integer_literal,
				get_literal_integer_type(src_tokens, kind, signed_result_value, context),
				ast::constant_value(signed_result_value),
				ast::make_expr_integer_literal(kind)
			);
		}

		return ast::expression();
	}
	else
	{
		auto const lhs = lhs_value.get<ast::constant_value::sint>();
		auto const rhs = rhs_value.get<ast::constant_value::sint>();

		auto const is_signed = lhs_kind == ast::literal_kind::signed_integer || rhs_kind == ast::literal_kind::signed_integer;
		auto const kind = is_signed ? ast::literal_kind::signed_integer : ast::literal_kind::integer;
		auto const [signed_result_value, signed_overflowed] = sub_overflow(lhs, rhs);
		if (!signed_overflowed)
		{
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::integer_literal,
				get_literal_integer_type(src_tokens, kind, signed_result_value, context),
				ast::constant_value(signed_result_value),
				ast::make_expr_integer_literal(kind)
			);
		}
		else if (is_signed)
		{
			return ast::expression();
		}

		auto const [unsigned_result_value, unsigned_overflowed] = sub_overflow<uint64_t>(lhs, rhs);
		if (!unsigned_overflowed)
		{
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::integer_literal,
				get_literal_integer_type(src_tokens, kind, unsigned_result_value, context),
				ast::constant_value(unsigned_result_value),
				ast::make_expr_integer_literal(kind)
			);
		}

		return ast::expression();
	}
}

static ast::expression make_binary_multiply_literal_operation(
	lex::src_tokens const &src_tokens,
	ast::literal_kind lhs_kind,
	ast::literal_kind rhs_kind,
	ast::constant_value const &lhs_value,
	ast::constant_value const &rhs_value,
	parse_context &context
)
{
	if (lhs_value.is<ast::constant_value::uint>() || rhs_value.is<ast::constant_value::uint>())
	{
		bz_assert(lhs_kind == ast::literal_kind::integer || lhs_kind == ast::literal_kind::unsigned_integer);
		bz_assert(rhs_kind == ast::literal_kind::integer || rhs_kind == ast::literal_kind::unsigned_integer);
		bz_assert(lhs_kind != ast::literal_kind::integer || lhs_value.is<ast::constant_value::uint>() || lhs_value.get<ast::constant_value::sint>() >= 0);
		bz_assert(rhs_kind != ast::literal_kind::integer || rhs_value.is<ast::constant_value::uint>() || rhs_value.get<ast::constant_value::sint>() >= 0);
		auto const lhs = lhs_value.is<ast::constant_value::uint>()
			? lhs_value.get<ast::constant_value::uint>()
			: static_cast<uint64_t>(lhs_value.get<ast::constant_value::sint>());
		auto const rhs = rhs_value.is<ast::constant_value::uint>()
			? rhs_value.get<ast::constant_value::uint>()
			: static_cast<uint64_t>(rhs_value.get<ast::constant_value::sint>());

		auto const is_unsigned = lhs_kind == ast::literal_kind::unsigned_integer || rhs_kind == ast::literal_kind::unsigned_integer;
		auto const kind = is_unsigned ? ast::literal_kind::unsigned_integer : ast::literal_kind::integer;
		auto const [result_value, overflowed] = mul_overflow(lhs, rhs);
		if (!overflowed)
		{
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::integer_literal,
				get_literal_integer_type(src_tokens, kind, result_value, context),
				ast::constant_value(result_value),
				ast::make_expr_integer_literal(kind)
			);
		}

		return ast::expression();
	}
	else
	{
		auto const lhs = lhs_value.get<ast::constant_value::sint>();
		auto const rhs = rhs_value.get<ast::constant_value::sint>();

		auto const is_signed = lhs_kind == ast::literal_kind::signed_integer || rhs_kind == ast::literal_kind::signed_integer;
		auto const kind = is_signed ? ast::literal_kind::signed_integer : ast::literal_kind::integer;
		auto const [signed_result_value, signed_overflowed] = mul_overflow(lhs, rhs);
		if (!signed_overflowed)
		{
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::integer_literal,
				get_literal_integer_type(src_tokens, kind, signed_result_value, context),
				ast::constant_value(signed_result_value),
				ast::make_expr_integer_literal(kind)
			);
		}
		else if (is_signed)
		{
			return ast::expression();
		}

		auto const [unsigned_result_value, unsigned_overflowed] = mul_overflow<uint64_t>(lhs, rhs);
		if (!unsigned_overflowed)
		{
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::integer_literal,
				get_literal_integer_type(src_tokens, kind, unsigned_result_value, context),
				ast::constant_value(unsigned_result_value),
				ast::make_expr_integer_literal(kind)
			);
		}

		return ast::expression();
	}
}

static ast::expression make_binary_divide_literal_operation(
	lex::src_tokens const &src_tokens,
	ast::literal_kind lhs_kind,
	ast::literal_kind rhs_kind,
	ast::constant_value const &lhs_value,
	ast::constant_value const &rhs_value,
	parse_context &context
)
{
	if (lhs_value.is<ast::constant_value::uint>() || rhs_value.is<ast::constant_value::uint>())
	{
		bz_assert(lhs_kind == ast::literal_kind::integer || lhs_kind == ast::literal_kind::unsigned_integer);
		bz_assert(rhs_kind == ast::literal_kind::integer || rhs_kind == ast::literal_kind::unsigned_integer);
		bz_assert(lhs_kind != ast::literal_kind::integer || lhs_value.is<ast::constant_value::uint>() || lhs_value.get<ast::constant_value::sint>() >= 0);
		bz_assert(rhs_kind != ast::literal_kind::integer || rhs_value.is<ast::constant_value::uint>() || rhs_value.get<ast::constant_value::sint>() >= 0);
		auto const lhs = lhs_value.is<ast::constant_value::uint>()
			? lhs_value.get<ast::constant_value::uint>()
			: static_cast<uint64_t>(lhs_value.get<ast::constant_value::sint>());
		auto const rhs = rhs_value.is<ast::constant_value::uint>()
			? rhs_value.get<ast::constant_value::uint>()
			: static_cast<uint64_t>(rhs_value.get<ast::constant_value::sint>());

		auto const is_unsigned = lhs_kind == ast::literal_kind::unsigned_integer || rhs_kind == ast::literal_kind::unsigned_integer;
		auto const kind = is_unsigned ? ast::literal_kind::unsigned_integer : ast::literal_kind::integer;
		auto const [result_value, overflowed] = div_overflow(lhs, rhs);
		if (!overflowed)
		{
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::integer_literal,
				get_literal_integer_type(src_tokens, kind, result_value, context),
				ast::constant_value(result_value),
				ast::make_expr_integer_literal(kind)
			);
		}

		return ast::expression();
	}
	else
	{
		auto const lhs = lhs_value.get<ast::constant_value::sint>();
		auto const rhs = rhs_value.get<ast::constant_value::sint>();

		auto const is_signed = lhs_kind == ast::literal_kind::signed_integer || rhs_kind == ast::literal_kind::signed_integer;
		auto const kind = is_signed ? ast::literal_kind::signed_integer : ast::literal_kind::integer;
		auto const [signed_result_value, signed_overflowed] = div_overflow(lhs, rhs);
		if (!signed_overflowed)
		{
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::integer_literal,
				get_literal_integer_type(src_tokens, kind, signed_result_value, context),
				ast::constant_value(signed_result_value),
				ast::make_expr_integer_literal(kind)
			);
		}
		else if (is_signed)
		{
			return ast::expression();
		}

		auto const [unsigned_result_value, unsigned_overflowed] = div_overflow<uint64_t>(lhs, rhs);
		if (!unsigned_overflowed)
		{
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::integer_literal,
				get_literal_integer_type(src_tokens, kind, unsigned_result_value, context),
				ast::constant_value(unsigned_result_value),
				ast::make_expr_integer_literal(kind)
			);
		}

		return ast::expression();
	}
}

static ast::expression make_binary_modulo_literal_operation(
	lex::src_tokens const &src_tokens,
	ast::literal_kind lhs_kind,
	ast::literal_kind rhs_kind,
	ast::constant_value const &lhs_value,
	ast::constant_value const &rhs_value,
	parse_context &context
)
{
	if (lhs_value.is<ast::constant_value::uint>() || rhs_value.is<ast::constant_value::uint>())
	{
		bz_assert(lhs_kind == ast::literal_kind::integer || lhs_kind == ast::literal_kind::unsigned_integer);
		bz_assert(rhs_kind == ast::literal_kind::integer || rhs_kind == ast::literal_kind::unsigned_integer);
		bz_assert(lhs_kind != ast::literal_kind::integer || lhs_value.is<ast::constant_value::uint>() || lhs_value.get<ast::constant_value::sint>() >= 0);
		bz_assert(rhs_kind != ast::literal_kind::integer || rhs_value.is<ast::constant_value::uint>() || rhs_value.get<ast::constant_value::sint>() >= 0);
		auto const lhs = lhs_value.is<ast::constant_value::uint>()
			? lhs_value.get<ast::constant_value::uint>()
			: static_cast<uint64_t>(lhs_value.get<ast::constant_value::sint>());
		auto const rhs = rhs_value.is<ast::constant_value::uint>()
			? rhs_value.get<ast::constant_value::uint>()
			: static_cast<uint64_t>(rhs_value.get<ast::constant_value::sint>());

		auto const is_unsigned = lhs_kind == ast::literal_kind::unsigned_integer || rhs_kind == ast::literal_kind::unsigned_integer;
		auto const kind = is_unsigned ? ast::literal_kind::unsigned_integer : ast::literal_kind::integer;
		if (rhs == 0)
		{
			// modulo by zero
			return ast::expression();
		}
		else
		{
			auto const result_value = lhs % rhs;
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::integer_literal,
				get_literal_integer_type(src_tokens, kind, result_value, context),
				ast::constant_value(result_value),
				ast::make_expr_integer_literal(kind)
			);
		}
	}
	else
	{
		auto const lhs = lhs_value.get<ast::constant_value::sint>();
		auto const rhs = rhs_value.get<ast::constant_value::sint>();

		auto const is_signed = lhs_kind == ast::literal_kind::signed_integer || rhs_kind == ast::literal_kind::signed_integer;
		auto const kind = is_signed ? ast::literal_kind::signed_integer : ast::literal_kind::integer;
		if (rhs == 0)
		{
			// modulo by zero
			return ast::expression();
		}
		else
		{
			auto const result_value = lhs % rhs;
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::integer_literal,
				get_literal_integer_type(src_tokens, kind, result_value, context),
				ast::constant_value(result_value),
				ast::make_expr_integer_literal(kind)
			);
		}
	}
}

ast::expression make_binary_literal_operation(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression const &lhs,
	ast::expression const &rhs,
	parse_context &context
)
{
	auto const [lhs_kind, lhs_value] = lhs.get_integer_literal_kind_and_value();
	auto const [rhs_kind, rhs_value] = rhs.get_integer_literal_kind_and_value();
	switch (op_kind)
	{
	case lex::token::plus:
		return make_binary_plus_literal_operation(src_tokens, lhs_kind, rhs_kind, lhs_value, rhs_value, context);
	case lex::token::minus:
		return make_binary_minus_literal_operation(src_tokens, lhs_kind, rhs_kind, lhs_value, rhs_value, context);
	case lex::token::multiply:
		return make_binary_multiply_literal_operation(src_tokens, lhs_kind, rhs_kind, lhs_value, rhs_value, context);
	case lex::token::divide:
		return make_binary_divide_literal_operation(src_tokens, lhs_kind, rhs_kind, lhs_value, rhs_value, context);
	case lex::token::modulo:
		return make_binary_modulo_literal_operation(src_tokens, lhs_kind, rhs_kind, lhs_value, rhs_value, context);
	default:
		return ast::expression();
	}
}

} // namespace ctx
