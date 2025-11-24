#include "builtin_operators.h"
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

template<ast::constant_value_kind kind>
static auto get_constant_expression_values(
	ast::expression const &lhs,
	ast::expression const &rhs
)
{
	static_assert(kind != ast::constant_value_kind::aggregate);
	bz_assert(lhs.is_constant());
	bz_assert(rhs.is_constant());
	auto const &lhs_value = lhs.get_constant_value();
	auto const &rhs_value = rhs.get_constant_value();
	bz_assert(lhs_value.kind() == kind);
	bz_assert(rhs_value.kind() == kind);

	if constexpr (kind == ast::constant_value_kind::string)
	{
		return std::make_pair(
			lhs_value.get_string(),
			rhs_value.get_string()
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
	ast::typespec result_type = type.remove_reference();
	result_type.add_layer<ast::ts_pointer>();
	if (type_kind == ast::expression_type_kind::lvalue || type.is_reference())
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
	else if (result_type.is<ast::ts_auto_reference_mut>())
	{
		context.report_error(src_tokens, "reference to auto reference-mut type is not allowed");
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
		context.add_constant_type(std::move(result_type)),
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
	else if (result_type.is<ast::ts_auto_reference_mut>())
	{
		context.report_error(src_tokens, "auto reference to auto reference-mut type is not allowed");
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
		context.add_constant_type(std::move(result_type)),
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
		context.report_error(src_tokens, "auto reference-mut to consteval type is not allowed");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_mut>())
	{
		bz_assert(src_tokens.pivot != nullptr);
		context.report_error(src_tokens, "auto reference-mut to mut type is not allowed", {}, {
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
		context.report_error(src_tokens, "auto reference-mut to reference type is not allowed");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_move_reference>())
	{
		context.report_error(src_tokens, "auto reference-mut to move reference type is not allowed");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_auto_reference>())
	{
		context.report_error(src_tokens, "auto reference-mut to auto reference type is not allowed");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_auto_reference_mut>())
	{
		// nothing
	}
	else if (result_type.is<ast::ts_variadic>())
	{
		context.report_error(src_tokens, "auto reference-mut to variadic type is not allowed");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else
	{
		result_type.add_layer<ast::ts_auto_reference_mut>();
	}

	return ast::make_constant_expression(
		src_tokens,
		ast::expression_type_kind::type_name,
		ast::make_typename_typespec(nullptr),
		context.add_constant_type(std::move(result_type)),
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
	else if (result_type.is<ast::ts_auto_reference_mut>())
	{
		context.report_error(src_tokens, "pointer to auto reference-mut is not allowed");
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
		context.add_constant_type(std::move(result_type)),
		ast::make_expr_unary_op(op_kind, std::move(expr))
	);
}

// ?(typename) -> (?typename)
static ast::expression get_type_op_unary_question_mark(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(op_kind == lex::token::question_mark);
	bz_assert(expr.not_error());
	bz_assert(expr.is_typename());

	ast::typespec result_type = expr.get_typename();
	result_type.src_tokens = src_tokens;
	if (result_type.is<ast::ts_move_reference>())
	{
		context.report_error(src_tokens, "optional of move reference is not allowed");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_mut>())
	{
		context.report_error(src_tokens, "optional of mut is not allowed");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_consteval>())
	{
		context.report_error(src_tokens, "optional of consteval is not allowed");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_optional>())
	{
		context.report_error(src_tokens, "optional of optional is not allowed");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_variadic>())
	{
		context.report_error(src_tokens, "optional of variadic type is not allowed");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}

	result_type.add_layer<ast::ts_optional>();

	return ast::make_constant_expression(
		src_tokens,
		ast::expression_type_kind::type_name,
		ast::make_typename_typespec(nullptr),
		context.add_constant_type(std::move(result_type)),
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
		context.add_constant_type(std::move(result_type)),
		ast::make_expr_unary_op(op_kind, std::move(expr))
	);
}

// mut (typename) -> (mut typename)
static ast::expression get_type_op_unary_mut(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(op_kind == lex::token::kw_mut);
	bz_assert(expr.not_error());
	bz_assert(expr.is_typename());

	ast::typespec result_type = expr.get_typename();
	result_type.src_tokens = src_tokens;
	if (result_type.is<ast::ts_lvalue_reference>())
	{
		context.report_error(src_tokens, "a reference type cannot be 'mut'");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_move_reference>())
	{
		context.report_error(src_tokens, "a move reference type cannot be 'mut'");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_auto_reference>())
	{
		context.report_error(src_tokens, "an auto reference type cannot be 'mut'");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_auto_reference_mut>())
	{
		context.report_error(src_tokens, "an auto reference-mut type cannot be 'mut'");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_mut>())
	{
		// nothing
	}
	else if (result_type.is<ast::ts_consteval>())
	{
		context.report_error(src_tokens, "a consteval type cannot be 'mut'");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_variadic>())
	{
		context.report_error(src_tokens, "a variadic type cannot be 'mut'");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else
	{
		result_type.add_layer<ast::ts_mut>();
	}

	return ast::make_constant_expression(
		src_tokens,
		ast::expression_type_kind::type_name,
		ast::make_typename_typespec(nullptr),
		context.add_constant_type(std::move(result_type)),
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
	else if (result_type.is<ast::ts_auto_reference_mut>())
	{
		context.report_error(src_tokens, "an auto reference-mut type cannot be 'consteval'");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (result_type.is<ast::ts_mut>())
	{
		context.report_error(src_tokens, "a mut type cannot be 'consteval'");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
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
		context.add_constant_type(std::move(result_type)),
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
	else if (result_type.is<ast::ts_auto_reference_mut>())
	{
		context.report_error(src_tokens, "move reference to auto reference-mut type is not allowed");
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
		context.add_constant_type(std::move(result_type)),
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
		auto const type = expr.get_typename();
		if (!ast::is_complete(type))
		{
			context.report_error(src_tokens, bz::format("cannot take 'sizeof' of an incomplete type '{}'", type));
			return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
		}
		else if (!context.is_instantiable(src_tokens, type))
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
	else if (expr.is<ast::expanded_variadic_expression>())
	{
		uint64_t const size = expr.get<ast::expanded_variadic_expression>().exprs.size();
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
		else if (!context.is_instantiable(src_tokens, type))
		{
			context.report_error(src_tokens, bz::format("cannot take 'sizeof' of an expression with non-instantiable type '{}'", type));
			return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
		}
		uint64_t const size = context.get_sizeof(type);
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
	if (expr.is_function_name() && type.is_empty())
	{
		bz_assert(expr.get_function_name().decl->body.is_generic());
		context.report_error(
			src_tokens,
			bz::format("cannot take 'typeof' of a generic function '{}'", expr.get_function_name().decl->body.get_signature())
		);
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (
		(expr.is_function_alias_name() && type.is_empty())
		|| expr.is_function_overload_set()
	)
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
	bz_assert(type.not_empty());
	return ast::make_constant_expression(
		src_tokens,
		ast::expression_type_kind::type_name,
		ast::make_typename_typespec(nullptr),
		context.add_constant_type(std::move(result_type)),
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
		bz_assert(expr.get_expr().is<ast::expr_variable_name>());
		auto const &var_name_expr = expr.get_expr().get<ast::expr_variable_name>();
		bz_assert(var_name_expr.decl != nullptr);
		if (!var_name_expr.is_local)
		{
			context.report_error(
				src_tokens,
				bz::format("unable to move non-local variable '{}'", var_name_expr.decl->get_id().format_as_unqualified()),
				{ context.make_note(var_name_expr.decl->src_tokens, "variable was declared here") }
			);
			return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
		}
		else if (var_name_expr.loop_boundary_count != 0)
		{
			context.report_error(
				src_tokens,
				bz::format(
					"unable to move variable '{}', because it is outside a loop boundary",
					var_name_expr.decl->get_id().format_as_unqualified()
				),
				{ context.make_note(var_name_expr.decl->src_tokens, "variable was declared here") }
			);
			return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
		}
		else
		{
			context.register_move(src_tokens, var_name_expr.decl);
			ast::typespec result_type = type.remove_any_mut();
			return ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::moved_lvalue,
				result_type,
				ast::make_expr_unary_op(op_kind, std::move(expr)),
				ast::destruct_operation()
			);
		}
	}
	else if (type.is_reference())
	{
		context.report_error(src_tokens, "operator move cannot be applied to a reference");
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

	if (kind == ast::expression_type_kind::lvalue || type.is_mut_reference())
	{
		ast::typespec result_type = type.remove_mut_reference();
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::moved_lvalue,
			result_type,
			ast::make_expr_unary_op(op_kind, std::move(expr)),
			ast::destruct_operation()
		);
	}
	else if (type.is_reference())
	{
		context.report_error(src_tokens, "operator __move__ cannot be applied to an immutable reference");
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
		context.report_error(expr.src_tokens, "failed to evaluate expression at compile time");
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
}

#undef undeclared_unary_message


#define undeclared_binary_message(op) "no match for binary operator " op " with types '{}' and '{}'"

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

	{
		auto bool_type = ast::make_base_type_typespec({}, context.get_builtin_type_info(ast::type_info::bool_));
		resolve::match_expression_to_type(lhs, bool_type, context);
		resolve::match_expression_to_type(rhs, bool_type, context);
	}

	if (lhs.is_error() || rhs.is_error())
	{
		return ast::make_error_expression(src_tokens, ast::make_expr_binary_op(op_kind, std::move(lhs), std::move(rhs)));
	}

	auto const [lhs_type, lhs_type_kind] = lhs.get_expr_type_and_kind();
	auto const [rhs_type, rhs_type_kind] = rhs.get_expr_type_and_kind();
	auto const lhs_t = lhs_type.remove_any_mut().remove_any_mut();
	auto const rhs_t = rhs_type.remove_any_mut();

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
	auto const expr_t = expr_type.remove_mut_reference();
	auto const dest_t = dest_type.remove_any_mut();
	bz_assert(ast::is_complete(dest_t));

	// case from null to a optional type
	if (
		dest_t.is<ast::ts_optional>()
		&& ((
			expr.is_constant()
			&& expr.get_constant_value().is_null_constant()
		)
		|| (
			expr.is_dynamic()
			&& [&]() {
				auto const type = expr.get_dynamic().type.remove_any_mut();
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
			ast::constant_value::get_null(),
			ast::make_expr_cast(std::move(expr), std::move(dest_type))
		);
	}
	else if (
		(dest_t.is<ast::ts_pointer>() || dest_t.is_optional_pointer())
		&& (expr_t.is<ast::ts_pointer>() || expr_t.is_optional_pointer())
	)
	{
		auto inner_dest_t = dest_t.remove_optional_pointer();
		auto inner_expr_t = expr_t.remove_optional_pointer();
		if (inner_dest_t.is<ast::ts_mut>() && !inner_expr_t.is<ast::ts_mut>())
		{
			context.report_error(
				src_tokens,
				bz::format("invalid conversion from '{}' to '{}'", expr_type, dest_type)
			);
			return ast::make_error_expression(src_tokens, ast::make_expr_cast(std::move(expr), std::move(dest_type)));
		}
		inner_dest_t = inner_dest_t.remove_mut();
		inner_expr_t = inner_expr_t.remove_mut();
		while (
			inner_dest_t.is_safe_blind_get()
			&& inner_expr_t.is_safe_blind_get()
			&& inner_dest_t.modifier_kind() == inner_expr_t.modifier_kind()
		)
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
		// this is a really common case, where integer literals are converted to a specific integer type
		// this check covers a wider range, but that should be fine
		if (expr.is_constant() && ast::is_integer_kind(expr_kind) && ast::is_integer_kind(dest_kind))
		{
			auto const &expr_value = expr.get_constant_value();
			auto const dest_max_value = [dest_kind = dest_kind]() -> uint64_t {
				switch (dest_kind)
				{
				case ast::type_info::i8_:
					return static_cast<uint64_t>(std::numeric_limits<int8_t>::max());
				case ast::type_info::i16_:
					return static_cast<uint64_t>(std::numeric_limits<int16_t>::max());
				case ast::type_info::i32_:
					return static_cast<uint64_t>(std::numeric_limits<int32_t>::max());
				case ast::type_info::i64_:
					return static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
				case ast::type_info::u8_:
					return static_cast<uint64_t>(std::numeric_limits<uint8_t>::max());
				case ast::type_info::u16_:
					return static_cast<uint64_t>(std::numeric_limits<uint16_t>::max());
				case ast::type_info::u32_:
					return static_cast<uint64_t>(std::numeric_limits<uint32_t>::max());
				case ast::type_info::u64_:
					return static_cast<uint64_t>(std::numeric_limits<uint64_t>::max());
				default:
					bz_unreachable;
				}
			}();

			if (expr_value.is_sint())
			{
				auto const value = expr_value.get_sint();
				if (
					ast::is_signed_integer_kind(dest_kind)
					&& value <= static_cast<int64_t>(dest_max_value)
					&& value >= (-static_cast<int64_t>(dest_max_value) - 1)
				)
				{
					ast::typespec dest_t_copy = dest_t;
					return ast::make_constant_expression(
						src_tokens,
						ast::expression_type_kind::rvalue,
						std::move(dest_t_copy),
						ast::constant_value(value),
						ast::make_expr_cast(std::move(expr), std::move(dest_type))
					);
				}
				else if (
					ast::is_unsigned_integer_kind(dest_kind)
					&& value >= 0
					&& static_cast<uint64_t>(value) <= dest_max_value
				)
				{
					ast::typespec dest_t_copy = dest_t;
					return ast::make_constant_expression(
						src_tokens,
						ast::expression_type_kind::rvalue,
						std::move(dest_t_copy),
						ast::constant_value(static_cast<uint64_t>(value)),
						ast::make_expr_cast(std::move(expr), std::move(dest_type))
					);
				}
				else
				{
					// fall back to the general case
				}
			}
			else
			{
				auto const value = expr_value.get_uint();
				if (value <= dest_max_value)
				{
					ast::typespec dest_t_copy = dest_t;
					ast::constant_value result_value;
					if (ast::is_signed_integer_kind(dest_kind))
					{
						result_value.emplace<ast::constant_value_kind::sint>(static_cast<int64_t>(value));
					}
					else
					{
						result_value.emplace<ast::constant_value_kind::uint>(static_cast<uint64_t>(value));
					}
					return ast::make_constant_expression(
						src_tokens,
						ast::expression_type_kind::rvalue,
						std::move(dest_t_copy),
						std::move(result_value),
						ast::make_expr_cast(std::move(expr), std::move(dest_type))
					);
				}
				else
				{
					// fall back to the general case
				}
			}
		}

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
	auto const bare_called_type = called_type.remove_mut_reference();
	bz_assert(ast::is_rvalue_or_literal(arg.get_expr_type_and_kind().second));
	auto const arg_type = arg.get_expr_type();

	if (bare_called_type.is<ast::ts_tuple>() || called.is_tuple())
	{
		if (!arg.is_constant())
		{
			context.report_error(arg, "tuple subscript must be a constant expression");
			return ast::make_error_expression(src_tokens, ast::make_expr_subscript(std::move(called), std::move(arg)));
		}

		if (!arg_type.is<ast::ts_base_type>() || !ast::is_integer_kind(arg_type.get<ast::ts_base_type>().info->kind))
		{
			context.report_error(arg, bz::format("invalid type '{}' for tuple subscript", arg_type));
			return ast::make_error_expression(src_tokens, ast::make_expr_subscript(std::move(called), std::move(arg)));
		}

		auto const tuple_elem_count = bare_called_type.is<ast::ts_tuple>()
			? bare_called_type.get<ast::ts_tuple>().types.size()
			: called.get_expr().get<ast::expr_tuple>().elems.size();
		auto &const_arg = arg.get_constant();
		size_t index = 0;
		if (const_arg.value.is_uint())
		{
			auto const value = const_arg.value.get_uint();
			if (value >= tuple_elem_count)
			{
				context.report_error(arg, bz::format("index {} is out of range for tuple type '{}'", value, called_type));
				return ast::make_error_expression(src_tokens, ast::make_expr_subscript(std::move(called), std::move(arg)));
			}
			index = value;
		}
		else
		{
			bz_assert(const_arg.value.is_sint());
			auto const value = const_arg.value.get_sint();
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

			ast::typespec result_type_copy = result_type;
			return ast::make_dynamic_expression(
				src_tokens,
				result_kind, std::move(result_type_copy),
				ast::make_expr_tuple_subscript(std::move(tuple), std::move(arg)),
				ast::destruct_operation()
			);
		}
		else
		{
			auto &tuple_t = bare_called_type.get<ast::ts_tuple>();
			ast::typespec result_type = tuple_t.types[index];
			if (!result_type.is_reference())
			{
				bz_assert(!result_type.is<ast::ts_mut>());
				auto const is_lvalue = called_kind == ast::expression_type_kind::lvalue || called_type.is_reference();
				if (is_lvalue && called_type.remove_reference().is<ast::ts_mut>())
				{
					result_type.add_layer<ast::ts_mut>();
					result_type.add_layer<ast::ts_lvalue_reference>();
				}
				else if (is_lvalue)
				{
					result_type.add_layer<ast::ts_lvalue_reference>();
				}
			}

			auto const result_kind = result_type.is_reference()
				? ast::expression_type_kind::rvalue
				: called_kind;

			if (called_kind == ast::expression_type_kind::rvalue && !called_type.is_reference())
			{
				auto const elem_refs = bz::iota(0, tuple_t.types.size())
					.transform([&](size_t const i) {
						if (i == index)
						{
							auto const type_kind = result_type.is_reference()
								? ast::expression_type_kind::rvalue
								: ast::expression_type_kind::rvalue_reference;
							return ast::make_dynamic_expression(
								src_tokens,
								type_kind, result_type,
								ast::make_expr_bitcode_value_reference(),
								ast::destruct_operation()
							);
						}

						auto const elem_t = tuple_t.types[i].as_typespec_view();
						if (
							// comptime execution needs to end lifetimes for all elements, including these,
							// but dealing with reference members for this seems too problematic, so there's
							// an extra check in the comptime code generation instead, which ends lifetimes
							// when an elem_ref is missing
							elem_t.is<ast::ts_lvalue_reference>()
							|| context.is_trivially_destructible(called.src_tokens, elem_t)
						)
						{
							return ast::expression();
						}

						auto result = ast::make_dynamic_expression(
							src_tokens,
							ast::expression_type_kind::rvalue_reference, elem_t,
							ast::make_expr_bitcode_value_reference(),
							ast::destruct_operation()
						);
						context.add_self_destruction(result);
						return result;
					})
					.collect<ast::arena_vector>();

				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue_reference, std::move(result_type),
					ast::make_expr_rvalue_tuple_subscript(std::move(called), std::move(elem_refs), std::move(arg)),
					ast::destruct_operation()
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					result_kind, std::move(result_type),
					ast::make_expr_subscript(std::move(called), std::move(arg)),
					ast::destruct_operation()
				);
			}
		}
	}
	else if (bare_called_type.is<ast::ts_array_slice>())
	{
		auto &array_slice_t = bare_called_type.get<ast::ts_array_slice>();

		if (!arg_type.is<ast::ts_base_type>() || !ast::is_integer_kind(arg_type.get<ast::ts_base_type>().info->kind))
		{
			context.report_error(arg, bz::format("invalid type '{}' for array slice subscript", arg_type));
			return ast::make_error_expression(src_tokens, ast::make_expr_subscript(std::move(called), std::move(arg)));
		}

		ast::typespec result_type = array_slice_t.elem_type;
		result_type.add_layer<ast::ts_lvalue_reference>();

		if (called_kind == ast::expression_type_kind::rvalue)
		{
			// bz_assert(context.is_trivially_destructible(src_tokens, called_type));
			context.add_self_destruction(called);
		}

		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::rvalue, std::move(result_type),
			ast::make_expr_subscript(std::move(called), std::move(arg)),
			ast::destruct_operation()
		);
	}
	else // if (bare_called_type.is<ast::ts_array>())
	{
		bz_assert(bare_called_type.is<ast::ts_array>());
		auto &array_t = bare_called_type.get<ast::ts_array>();

		if (!arg_type.is<ast::ts_base_type>() || !ast::is_integer_kind(arg_type.get<ast::ts_base_type>().info->kind))
		{
			context.report_error(arg, bz::format("invalid type '{}' for array subscript", arg_type));
			return ast::make_error_expression(src_tokens, ast::make_expr_subscript(std::move(called), std::move(arg)));
		}

		if (ast::is_rvalue(called_kind) && !called_type.is_reference())
		{
			auto const elem_destruct_op = context.make_rvalue_array_destruction(src_tokens, called_type);

			ast::typespec result_type = array_t.elem_type;
			return ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::rvalue_reference, std::move(result_type),
				ast::make_expr_rvalue_array_subscript(std::move(called), std::move(elem_destruct_op), std::move(arg)),
				ast::destruct_operation()
			);
		}
		else
		{
			ast::typespec result_type = array_t.elem_type;
			bz_assert(!result_type.is_reference());
			if (called_type.is<ast::ts_mut>() || called_type.is_mut_reference())
			{
				result_type.add_layer<ast::ts_mut>();
			}
			result_type.add_layer<ast::ts_lvalue_reference>();

			return ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::rvalue, std::move(result_type),
				ast::make_expr_subscript(std::move(called), std::move(arg)),
				ast::destruct_operation()
			);
		}
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

	auto const lhs_type = lhs.get_typename();
	auto const rhs_type = rhs.get_typename();

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
		ast::make_base_type_typespec(src_tokens, context.get_builtin_type_info(ast::type_info::bool_)),
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
		T{ lex::token::star,           &get_type_op_unary_pointer        }, // *
		T{ lex::token::question_mark,  &get_type_op_unary_question_mark  }, // ?
		T{ lex::token::dot_dot_dot,    &get_type_op_unary_dot_dot_dot    }, // ...
		T{ lex::token::kw_mut,         &get_type_op_unary_mut            }, // mut
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

template<auto const &_info, typename ...Rest>
static ast::expression make_builtin_operation_generic(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	Rest &&...rest
)
{
	static constexpr auto info = _info;
#ifdef __GNUC__
	// this reliably compiles into the same code as a switch with clang, or an if-else cascade with GCC
	// see: https://godbolt.org/z/eGz1rrj49
	return [&]<size_t ...Ns>(bz::meta::index_sequence<Ns...>) {
		size_t i = 0;
		((
			op_kind == info[Ns].kind
			? (void)({ return info[i].parse_function(src_tokens, op_kind, std::forward<Rest>(rest)...); })
			: (void)(i += 1)
		), ...);
		return ast::expression();
	}(bz::meta::make_index_sequence<info.size()>{});
#else
	return [&]<size_t ...Ns>(bz::meta::index_sequence<Ns...>) {
		ast::expression result;
		bool done = false;
		((
			!done && op_kind == info[Ns].kind
			? (void)(done = true, result = info[Ns].parse_function(src_tokens, op_kind, std::forward<Rest>(rest)...))
			: (void)0
		), ...);
		return result;
	}(bz::meta::make_index_sequence<info.size()>{});
#endif // __GNUC__
}

ast::expression make_builtin_operation(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression expr,
	parse_context &context
)
{
	return make_builtin_operation_generic<builtin_unary_operators>(src_tokens, op_kind, std::move(expr), context);
}

ast::expression make_builtin_type_operation(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression expr,
	parse_context &context
)
{
	return make_builtin_operation_generic<type_op_unary_operators>(src_tokens, op_kind, std::move(expr), context);
}

ast::decl_operator *get_builtin_operator(
	uint32_t op_kind,
	ast::expression const &expr,
	parse_context &context
)
{
	auto const expr_type = expr.get_expr_type().remove_mut_reference();
	if (!expr_type.is<ast::ts_base_type>())
	{
		return nullptr;
	}

	auto const expr_type_kind = expr_type.get<ast::ts_base_type>().info->kind;
	return context.get_builtin_operator(op_kind, expr_type_kind);
}

ast::expression make_builtin_operation(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
)
{
	return make_builtin_operation_generic<builtin_binary_operators>(src_tokens, op_kind, std::move(lhs), std::move(rhs), context);
}

ast::expression make_builtin_type_operation(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
)
{
	return make_builtin_operation_generic<type_op_binary_operators>(src_tokens, op_kind, std::move(lhs), std::move(rhs), context);
}

static bool is_integer_literal_compatible(ast::expression const &literal, uint8_t type_kind)
{
	if (!literal.is_integer_literal())
	{
		return false;
	}

	auto const &[kind, value] = literal.get_integer_literal_kind_and_value();
	switch (kind)
	{
	case ast::literal_kind::integer:
		switch (type_kind)
		{
		case ast::type_info::i8_:
		{
			constexpr int8_t min = std::numeric_limits<int8_t>::min();
			constexpr int8_t max = std::numeric_limits<int8_t>::max();
			if (value.is_sint())
			{
				return value.get_sint() >= min && value.get_sint() <= max;
			}
			else
			{
				bz_assert(value.is_uint());
				return value.get_uint() <= static_cast<uint64_t>(max);
			}
		}
		case ast::type_info::i16_:
		{
			constexpr int16_t min = std::numeric_limits<int16_t>::min();
			constexpr int16_t max = std::numeric_limits<int16_t>::max();
			if (value.is_sint())
			{
				return value.get_sint() >= min && value.get_sint() <= max;
			}
			else
			{
				bz_assert(value.is_uint());
				return value.get_uint() <= static_cast<uint64_t>(max);
			}
		}
		case ast::type_info::i32_:
		{
			constexpr int32_t min = std::numeric_limits<int32_t>::min();
			constexpr int32_t max = std::numeric_limits<int32_t>::max();
			if (value.is_sint())
			{
				return value.get_sint() >= min && value.get_sint() <= max;
			}
			else
			{
				bz_assert(value.is_uint());
				return value.get_uint() <= static_cast<uint64_t>(max);
			}
		}
		case ast::type_info::i64_:
		{
			constexpr int64_t min = std::numeric_limits<int64_t>::min();
			constexpr int64_t max = std::numeric_limits<int64_t>::max();
			if (value.is_sint())
			{
				return value.get_sint() >= min && value.get_sint() <= max;
			}
			else
			{
				bz_assert(value.is_uint());
				return value.get_uint() <= static_cast<uint64_t>(max);
			}
		}

		case ast::type_info::u8_:
		{
			constexpr uint8_t max = std::numeric_limits<uint8_t>::max();
			if (value.is_sint())
			{
				return value.get_sint() >= 0 && static_cast<uint64_t>(value.get_sint()) <= max;
			}
			else
			{
				bz_assert(value.is_uint());
				return value.get_uint() <= max;
			}
		}
		case ast::type_info::u16_:
		{
			constexpr uint16_t max = std::numeric_limits<uint16_t>::max();
			if (value.is_sint())
			{
				return value.get_sint() >= 0 && static_cast<uint64_t>(value.get_sint()) <= max;
			}
			else
			{
				bz_assert(value.is_uint());
				return value.get_uint() <= max;
			}
		}
		case ast::type_info::u32_:
		{
			constexpr uint32_t max = std::numeric_limits<uint32_t>::max();
			if (value.is_sint())
			{
				return value.get_sint() >= 0 && static_cast<uint64_t>(value.get_sint()) <= max;
			}
			else
			{
				bz_assert(value.is_uint());
				return value.get_uint() <= max;
			}
		}
		case ast::type_info::u64_:
		{
			constexpr uint64_t max = std::numeric_limits<uint64_t>::max();
			if (value.is_sint())
			{
				return value.get_sint() >= 0 && static_cast<uint64_t>(value.get_sint()) <= max;
			}
			else
			{
				bz_assert(value.is_uint());
				return value.get_uint() <= max;
			}
		}

		default:
			return false;
		}

	case ast::literal_kind::signed_integer:
		switch (type_kind)
		{
		case ast::type_info::i8_:
		{
			constexpr int8_t min = std::numeric_limits<int8_t>::min();
			constexpr int8_t max = std::numeric_limits<int8_t>::max();
			bz_assert(value.is_sint());
			return value.get_sint() >= min && value.get_sint() <= max;
		}
		case ast::type_info::i16_:
		{
			constexpr int16_t min = std::numeric_limits<int16_t>::min();
			constexpr int16_t max = std::numeric_limits<int16_t>::max();
			bz_assert(value.is_sint());
			return value.get_sint() >= min && value.get_sint() <= max;
		}
		case ast::type_info::i32_:
		{
			constexpr int32_t min = std::numeric_limits<int32_t>::min();
			constexpr int32_t max = std::numeric_limits<int32_t>::max();
			bz_assert(value.is_sint());
			return value.get_sint() >= min && value.get_sint() <= max;
		}
		case ast::type_info::i64_:
		{
			constexpr int64_t min = std::numeric_limits<int64_t>::min();
			constexpr int64_t max = std::numeric_limits<int64_t>::max();
			bz_assert(value.is_sint());
			return value.get_sint() >= min && value.get_sint() <= max;
		}
		default:
			return false;
		}

	case ast::literal_kind::unsigned_integer:
		switch (type_kind)
		{
		case ast::type_info::u8_:
		{
			constexpr uint8_t max = std::numeric_limits<uint8_t>::max();
			bz_assert(value.is_uint());
			return value.get_uint() <= max;
		}
		case ast::type_info::u16_:
		{
			constexpr uint16_t max = std::numeric_limits<uint16_t>::max();
			bz_assert(value.is_uint());
			return value.get_uint() <= max;
		}
		case ast::type_info::u32_:
		{
			constexpr uint32_t max = std::numeric_limits<uint32_t>::max();
			bz_assert(value.is_uint());
			return value.get_uint() <= max;
		}
		case ast::type_info::u64_:
		{
			constexpr uint64_t max = std::numeric_limits<uint64_t>::max();
			bz_assert(value.is_uint());
			return value.get_uint() <= max;
		}
		default:
			return false;
		}
	}
}

static bool is_compatible_with_lhs(uint8_t lhs_kind, ast::expression const &rhs, uint8_t rhs_kind)
{
	if (ast::is_signed_integer_kind(lhs_kind))
	{
		return (ast::is_signed_integer_kind(rhs_kind) && rhs_kind <= lhs_kind)
			|| is_integer_literal_compatible(rhs, lhs_kind);
	}
	else if (ast::is_unsigned_integer_kind(lhs_kind))
	{
		return (ast::is_unsigned_integer_kind(rhs_kind) && rhs_kind <= lhs_kind)
			|| is_integer_literal_compatible(rhs, lhs_kind);
	}
	else
	{
		return lhs_kind == rhs_kind;
	}
}

static bz::optional<uint8_t> get_common_type_kind(ast::expression const &lhs, uint8_t lhs_kind, ast::expression const &rhs, uint8_t rhs_kind)
{
	if (lhs.is_integer_literal() && rhs.is_integer_literal())
	{
		return {};
	}
	else if (lhs.is_integer_literal())
	{
		if (ast::is_signed_integer_kind(rhs_kind))
		{
			if (rhs_kind < ast::type_info::i32_ && is_integer_literal_compatible(lhs, rhs_kind))
			{
				return rhs_kind;
			}

			for (auto kind = std::max(rhs_kind, static_cast<uint8_t>(ast::type_info::i32_)); kind <= ast::type_info::i64_; ++kind)
			{
				if (is_integer_literal_compatible(lhs, kind))
				{
					return kind;
				}
			}
		}
		else if (ast::is_unsigned_integer_kind(rhs_kind))
		{
			if (rhs_kind < ast::type_info::u32_ && is_integer_literal_compatible(lhs, rhs_kind))
			{
				return rhs_kind;
			}

			for (auto kind = std::max(rhs_kind, static_cast<uint8_t>(ast::type_info::u32_)); kind <= ast::type_info::u64_; ++kind)
			{
				if (is_integer_literal_compatible(lhs, kind))
				{
					return kind;
				}
			}
		}

		return {};
	}
	else if (rhs.is_integer_literal())
	{
		if (ast::is_signed_integer_kind(lhs_kind))
		{
			if (lhs_kind < ast::type_info::i32_ && is_integer_literal_compatible(rhs, lhs_kind))
			{
				return lhs_kind;
			}

			for (auto kind = std::max(lhs_kind, static_cast<uint8_t>(ast::type_info::i32_)); kind <= ast::type_info::i64_; ++kind)
			{
				if (is_integer_literal_compatible(rhs, kind))
				{
					return kind;
				}
			}
		}
		else if (ast::is_unsigned_integer_kind(lhs_kind))
		{
			if (lhs_kind < ast::type_info::u32_ && is_integer_literal_compatible(rhs, lhs_kind))
			{
				return lhs_kind;
			}

			for (auto kind = std::max(lhs_kind, static_cast<uint8_t>(ast::type_info::u32_)); kind <= ast::type_info::u64_; ++kind)
			{
				if (is_integer_literal_compatible(rhs, kind))
				{
					return kind;
				}
			}
		}

		return {};
	}
	else if (ast::is_signed_integer_kind(lhs_kind))
	{
		if (ast::is_signed_integer_kind(rhs_kind))
		{
			return std::max(lhs_kind, rhs_kind);
		}
		else
		{
			return {};
		}
	}
	else if (ast::is_unsigned_integer_kind(lhs_kind))
	{
		if (ast::is_unsigned_integer_kind(rhs_kind))
		{
			return std::max(lhs_kind, rhs_kind);
		}
		else
		{
			return {};
		}
	}
	else if (lhs_kind == rhs_kind)
	{
		return lhs_kind;
	}
	else
	{
		return {};
	}
}

ast::decl_operator *get_builtin_operator(
	uint32_t op_kind,
	ast::expression const &lhs,
	ast::expression const &rhs,
	parse_context &context
)
{
	auto const lhs_type = lhs.get_expr_type().remove_mut_reference();
	auto const rhs_type = rhs.get_expr_type();
	if (!lhs_type.is<ast::ts_base_type>() || !rhs_type.is<ast::ts_base_type>())
	{
		return nullptr;
	}

	auto const lhs_type_kind = lhs_type.get<ast::ts_base_type>().info->kind;
	auto const rhs_type_kind = rhs_type.get<ast::ts_base_type>().info->kind;
	switch (op_kind)
	{
	// basic
	case lex::token::assign:
	// arithmetic
	case lex::token::plus_eq:
	case lex::token::minus_eq:
	case lex::token::multiply_eq:
	case lex::token::divide_eq:
	case lex::token::modulo_eq:
	// bitwise
	case lex::token::bit_and_eq:
	case lex::token::bit_xor_eq:
	case lex::token::bit_or_eq:
		if (is_compatible_with_lhs(lhs_type_kind, rhs, rhs_type_kind))
		{
			return context.get_builtin_operator(op_kind, lhs_type_kind, lhs_type_kind);
		}
		else
		{
			return nullptr;
		}

	// basic
	case lex::token::equals:
	case lex::token::not_equals:
	case lex::token::less_than:
	case lex::token::less_than_eq:
	case lex::token::greater_than:
	case lex::token::greater_than_eq:
	// arithmetic
	case lex::token::plus:
	case lex::token::minus:
	case lex::token::multiply:
	case lex::token::divide:
	case lex::token::modulo:
	// bitwise
	case lex::token::bit_and:
	case lex::token::bit_xor:
	case lex::token::bit_or:
		if (auto const common_type_kind = get_common_type_kind(lhs, lhs_type_kind, rhs, rhs_type_kind); common_type_kind.has_value())
		{
			return context.get_builtin_operator(op_kind, common_type_kind.get(), common_type_kind.get());
		}
		else
		{
			return nullptr;
		}

	case lex::token::bit_left_shift:
	case lex::token::bit_left_shift_eq:
	case lex::token::bit_right_shift:
	case lex::token::bit_right_shift_eq:
		// works with conversions as well, so we don't have to deal with that
		return context.get_builtin_operator(op_kind, lhs_type_kind, rhs_type_kind);
	}
	return context.get_builtin_operator(op_kind, lhs_type_kind, rhs_type_kind);
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
				context.get_builtin_type_info(ast::type_info::i32_),
				context.get_builtin_type_info(ast::type_info::i64_)
			};
		}
		else
		{
			return {
				context.get_builtin_type_info(ast::type_info::u32_),
				context.get_builtin_type_info(ast::type_info::u64_)
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
		return ast::make_base_type_typespec(src_tokens, context.get_builtin_type_info(ast::type_info::u64_));
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
		return ast::make_base_type_typespec(src_tokens, context.get_builtin_type_info(ast::type_info::i32_));
	}
	else
	{
		return ast::make_base_type_typespec(src_tokens, context.get_builtin_type_info(ast::type_info::i64_));
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
		value.is_sint()
			? get_literal_integer_type(src_tokens, kind, value.get_sint(), context)
			: get_literal_integer_type(src_tokens, kind, value.get_uint(), context),
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
	bz_assert(value.is_sint()); // uint can't match to 'operator -'
	auto const int_value = value.get_sint();
	if (int_value == int64_min)
	{
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::integer_literal,
			ast::make_base_type_typespec(src_tokens, context.get_builtin_type_info(ast::type_info::u64_)),
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
	if (lhs_value.is_uint() || rhs_value.is_uint())
	{
		bz_assert(lhs_kind == ast::literal_kind::integer || lhs_kind == ast::literal_kind::unsigned_integer);
		bz_assert(rhs_kind == ast::literal_kind::integer || rhs_kind == ast::literal_kind::unsigned_integer);
		bz_assert(lhs_kind != ast::literal_kind::integer || lhs_value.is_uint() || lhs_value.get_sint() >= 0);
		bz_assert(rhs_kind != ast::literal_kind::integer || rhs_value.is_uint() || rhs_value.get_sint() >= 0);
		auto const lhs = lhs_value.is_uint()
			? lhs_value.get_uint()
			: static_cast<uint64_t>(lhs_value.get_sint());
		auto const rhs = rhs_value.is_uint()
			? rhs_value.get_uint()
			: static_cast<uint64_t>(rhs_value.get_sint());

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
		auto const lhs = lhs_value.get_sint();
		auto const rhs = rhs_value.get_sint();

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
	if (lhs_value.is_uint() || rhs_value.is_uint())
	{
		bz_assert(lhs_kind == ast::literal_kind::integer || lhs_kind == ast::literal_kind::unsigned_integer);
		bz_assert(rhs_kind == ast::literal_kind::integer || rhs_kind == ast::literal_kind::unsigned_integer);
		bz_assert(lhs_kind != ast::literal_kind::integer || lhs_value.is_uint() || lhs_value.get_sint() >= 0);
		bz_assert(rhs_kind != ast::literal_kind::integer || rhs_value.is_uint() || rhs_value.get_sint() >= 0);
		auto const lhs = lhs_value.is_uint()
			? lhs_value.get_uint()
			: static_cast<uint64_t>(lhs_value.get_sint());
		auto const rhs = rhs_value.is_uint()
			? rhs_value.get_uint()
			: static_cast<uint64_t>(rhs_value.get_sint());

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
		auto const lhs = lhs_value.get_sint();
		auto const rhs = rhs_value.get_sint();

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
	if (lhs_value.is_uint() || rhs_value.is_uint())
	{
		bz_assert(lhs_kind == ast::literal_kind::integer || lhs_kind == ast::literal_kind::unsigned_integer);
		bz_assert(rhs_kind == ast::literal_kind::integer || rhs_kind == ast::literal_kind::unsigned_integer);
		bz_assert(lhs_kind != ast::literal_kind::integer || lhs_value.is_uint() || lhs_value.get_sint() >= 0);
		bz_assert(rhs_kind != ast::literal_kind::integer || rhs_value.is_uint() || rhs_value.get_sint() >= 0);
		auto const lhs = lhs_value.is_uint()
			? lhs_value.get_uint()
			: static_cast<uint64_t>(lhs_value.get_sint());
		auto const rhs = rhs_value.is_uint()
			? rhs_value.get_uint()
			: static_cast<uint64_t>(rhs_value.get_sint());

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
		auto const lhs = lhs_value.get_sint();
		auto const rhs = rhs_value.get_sint();

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
	if (lhs_value.is_uint() || rhs_value.is_uint())
	{
		bz_assert(lhs_kind == ast::literal_kind::integer || lhs_kind == ast::literal_kind::unsigned_integer);
		bz_assert(rhs_kind == ast::literal_kind::integer || rhs_kind == ast::literal_kind::unsigned_integer);
		bz_assert(lhs_kind != ast::literal_kind::integer || lhs_value.is_uint() || lhs_value.get_sint() >= 0);
		bz_assert(rhs_kind != ast::literal_kind::integer || rhs_value.is_uint() || rhs_value.get_sint() >= 0);
		auto const lhs = lhs_value.is_uint()
			? lhs_value.get_uint()
			: static_cast<uint64_t>(lhs_value.get_sint());
		auto const rhs = rhs_value.is_uint()
			? rhs_value.get_uint()
			: static_cast<uint64_t>(rhs_value.get_sint());

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
		auto const lhs = lhs_value.get_sint();
		auto const rhs = rhs_value.get_sint();

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
	if (lhs_value.is_uint() || rhs_value.is_uint())
	{
		bz_assert(lhs_kind == ast::literal_kind::integer || lhs_kind == ast::literal_kind::unsigned_integer);
		bz_assert(rhs_kind == ast::literal_kind::integer || rhs_kind == ast::literal_kind::unsigned_integer);
		bz_assert(lhs_kind != ast::literal_kind::integer || lhs_value.is_uint() || lhs_value.get_sint() >= 0);
		bz_assert(rhs_kind != ast::literal_kind::integer || rhs_value.is_uint() || rhs_value.get_sint() >= 0);
		auto const lhs = lhs_value.is_uint()
			? lhs_value.get_uint()
			: static_cast<uint64_t>(lhs_value.get_sint());
		auto const rhs = rhs_value.is_uint()
			? rhs_value.get_uint()
			: static_cast<uint64_t>(rhs_value.get_sint());

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
		auto const lhs = lhs_value.get_sint();
		auto const rhs = rhs_value.get_sint();

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
