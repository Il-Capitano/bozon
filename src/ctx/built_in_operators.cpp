#include "built_in_operators.h"
#include "global_context.h"
#include "parse_context.h"
#include "safe_operations.h"

namespace ctx
{

static auto get_base_kinds(
	ast::typespec const &lhs_t,
	ast::typespec const &rhs_t
) -> std::pair<uint32_t, uint32_t>
{
	bz_assert(lhs_t.is<ast::ts_base_type>());
	bz_assert(rhs_t.is<ast::ts_base_type>());
	return {
		lhs_t.get<ast::ts_base_type_ptr>()->info->kind,
		rhs_t.get<ast::ts_base_type_ptr>()->info->kind
	};
};

template<size_t kind>
static auto get_constant_expression_values(
	ast::expression const &lhs,
	ast::expression const &rhs
)
{
	static_assert(kind != ast::constant_value::aggregate);
	bz_assert(lhs.is<ast::constant_expression>());
	bz_assert(rhs.is<ast::constant_expression>());
	auto &const_lhs = lhs.get<ast::constant_expression>();
	auto &const_rhs = rhs.get<ast::constant_expression>();
	bz_assert(const_lhs.value.kind() == kind);
	bz_assert(const_rhs.value.kind() == kind);

	if constexpr (kind == ast::constant_value::string)
	{
		return std::make_pair(
			const_lhs.value.get<ast::constant_value::string>().as_string_view(),
			const_rhs.value.get<ast::constant_value::string>().as_string_view()
		);
	}
	else
	{
		return std::make_pair(const_lhs.value.get<kind>(), const_rhs.value.get<kind>());
	}
}


ast::expression make_non_overloadable_operation(
	lex::token_pos op,
	ast::expression lhs,
	ast::expression rhs,
	[[maybe_unused]] parse_context &context
)
{
	bz_assert(op->kind == lex::token::comma);
	// TODO add warning if lhs has no side effects
	lex::src_tokens const src_tokens = { lhs.get_tokens_begin(), op, rhs.get_tokens_end() };
	auto const [type, type_kind] = rhs.get_expr_type_and_kind();
	auto result_type = type;
	if (lhs.is<ast::constant_expression>() && rhs.is<ast::constant_expression>())
	{
		auto value = rhs.get<ast::constant_expression>().value;
		return ast::make_constant_expression(
			src_tokens,
			type_kind,
			std::move(result_type),
			std::move(value),
			ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
		);
	}
	else
	{
		return ast::make_dynamic_expression(
			src_tokens,
			type_kind,
			std::move(result_type),
			ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
		);
	}
}

ast::expression make_non_overloadable_operation(
	lex::token_pos op,
	ast::expression expr,
	parse_context &context
)
{
	switch (op->kind)
	{
	case lex::token::address_of:
	{
		auto [type, type_kind] = expr.get_expr_type_and_kind();
		lex::src_tokens const src_tokens = { op, op, expr.get_tokens_end() };
		auto result_type = ast::make_ts_pointer({}, type);
		if (
			type_kind == ast::expression_type_kind::lvalue
			|| type_kind == ast::expression_type_kind::lvalue_reference
		)
		{
			return ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::rvalue,
				std::move(result_type),
				ast::make_expr_unary_op(op, std::move(expr))
			);
		}
		else
		{
			context.report_error(src_tokens, "cannot take address of an rvalue");
			return ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::rvalue,
				std::move(result_type),
				ast::make_expr_unary_op(op, std::move(expr))
			);
		}
	}
	case lex::token::kw_sizeof:
		bz_assert(false);
		return ast::expression();

	default:
		bz_assert(false);
		return ast::expression();
	}
}

#define undeclared_unary_message(op) "undeclared unary operator " op " with type '{}'"

ast::type_info *get_narrowest_type_info(uint64_t val, parse_context &context)
{
	if (val <= std::numeric_limits<uint8_t>::max())
	{
		return context.get_base_type_info(ast::type_info::uint8_);
	}
	else if (val <= std::numeric_limits<uint16_t>::max())
	{
		return context.get_base_type_info(ast::type_info::uint16_);
	}
	else if (val <= std::numeric_limits<uint32_t>::max())
	{
		return context.get_base_type_info(ast::type_info::uint32_);
	}
	else
	{
		return context.get_base_type_info(ast::type_info::uint64_);
	}
}

ast::type_info *get_narrowest_type_info(int64_t val, parse_context &context)
{
	if (
		val >= std::numeric_limits<int8_t>::min()
		&& val <= std::numeric_limits<int8_t>::max()
	)
	{
		return context.get_base_type_info(ast::type_info::int8_);
	}
	else if (
		val >= std::numeric_limits<int16_t>::min()
		&& val <= std::numeric_limits<int16_t>::max()
	)
	{
		return context.get_base_type_info(ast::type_info::int16_);
	}
	else if (
		val >= std::numeric_limits<int32_t>::min()
		&& val <= std::numeric_limits<int32_t>::max()
	)
	{
		return context.get_base_type_info(ast::type_info::int32_);
	}
	else
	{
		return context.get_base_type_info(ast::type_info::int64_);
	}
}

// it's the same as a no-op
// +sintN -> sintN
// +uintN -> uintN
// +floatN -> floatN
static ast::expression get_built_in_unary_plus(
	lex::token_pos op,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(op->kind == lex::token::plus);
	bz_assert(!expr.is<ast::tuple_expression>());
	bz_assert(expr.not_null());
	auto const [type, _] = expr.get_expr_type_and_kind();
	auto &expr_t = ast::remove_const(type);
	lex::src_tokens const src_tokens = { op, op, expr.get_tokens_end() };

	if (!expr_t.is<ast::ts_base_type>())
	{
		context.report_error(src_tokens, bz::format(undeclared_unary_message("+"), type));
		return ast::expression(src_tokens);
	}
	auto const kind = expr_t.get<ast::ts_base_type_ptr>()->info->kind;

	if (!is_arithmetic_kind(kind))
	{
		context.report_error(src_tokens, bz::format(undeclared_unary_message("+"), type));
		return ast::expression(src_tokens);
	}

	auto result_type = expr_t;
	if (expr.is<ast::constant_expression>())
	{
		auto const value = expr.get<ast::constant_expression>().value;
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			std::move(result_type),
			value,
			ast::make_expr_unary_op(op, std::move(expr))
		);
	}
	else
	{
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			std::move(result_type),
			ast::make_expr_unary_op(op, std::move(expr))
		);
	}
}

// -sintN -> sintN
// -floatN -> floatN
static ast::expression get_built_in_unary_minus(
	lex::token_pos op,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(op->kind == lex::token::minus);
	bz_assert(!expr.is<ast::tuple_expression>());
	bz_assert(expr.not_null());
	auto const [type, _] = expr.get_expr_type_and_kind();
	auto &expr_t = remove_const(type);
	lex::src_tokens const src_tokens = { op, op, expr.get_tokens_end() };

	if (!expr_t.is<ast::ts_base_type>())
	{
		context.report_error(
			src_tokens,
			bz::format(undeclared_unary_message("-"), type)
		);
		return ast::expression(src_tokens);
	}
	auto const type_info = expr_t.get<ast::ts_base_type_ptr>()->info;
	auto const kind = type_info->kind;
	if (is_signed_integer_kind(kind) || is_floating_point_kind(kind))
	{
		ast::typespec result_type{};
		if (expr.is<ast::constant_expression>())
		{
			auto &expr_value = expr.get<ast::constant_expression>().value;
			ast::constant_value value{};
			static_assert(-std::numeric_limits<int64_t>::max() == std::numeric_limits<int64_t>::min() + 1);

			switch (expr_value.kind())
			{
			case ast::constant_value::sint:
			{
				auto const val = expr_value.get<ast::constant_value::sint>();
				switch (kind)
				{
				case ast::type_info::int8_:
					value = static_cast<int64_t>(static_cast<int8_t>(-val));
					if (val == std::numeric_limits<int8_t>::min())
					{
						context.report_warning(
							src_tokens,
							bz::format("overflow in constant expression with type 'int8' results in {}", -val)
						);
					}
					break;
				case ast::type_info::int16_:
					value = static_cast<int64_t>(static_cast<int16_t>(-val));
					if (val == std::numeric_limits<int16_t>::min())
					{
						context.report_warning(
							src_tokens,
							bz::format("overflow in constant expression with type 'int16' results in {}", -val)
						);
					}
					break;
				case ast::type_info::int32_:
					value = static_cast<int64_t>(static_cast<int32_t>(-val));
					if (val == std::numeric_limits<int32_t>::min())
					{
						context.report_warning(
							src_tokens,
							bz::format("overflow in constant expression with type 'int32' results in {}", -val)
						);
					}
					break;
				case ast::type_info::int64_:
					value = static_cast<int64_t>(static_cast<int64_t>(-val));
					if (val == std::numeric_limits<int64_t>::min())
					{
						context.report_warning(
							src_tokens,
							bz::format("overflow in constant expression with type 'int64' results in {}", -val)
						);
					}
					break;

				default:
					bz_assert(false);
					break;
				}

				result_type = expr_t;
				break;
			}
			case ast::constant_value::float32:
				bz_assert(kind == ast::type_info::float32_);
				result_type = expr_t;
				value = -expr_value.get<ast::constant_value::float32>();
				break;
			case ast::constant_value::float64:
				bz_assert(kind == ast::type_info::float64_);
				result_type = expr_t;
				value = -expr_value.get<ast::constant_value::float64>();
				break;

			default:
				bz_assert(false);
				break;
			}

			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::rvalue,
				std::move(result_type),
				value,
				ast::make_expr_unary_op(op, std::move(expr))
			);
		}
		else
		{
			result_type = expr_t;
			return ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::rvalue,
				std::move(result_type),
				ast::make_expr_unary_op(op, std::move(expr))
			);
		}
	}
	else
	{
		context.report_error(
			src_tokens,
			bz::format(undeclared_unary_message("-"), type)
		);
		return ast::expression(src_tokens);
	}
}

// *ptr -> &typeof *ptr
static ast::expression get_built_in_unary_dereference(
	lex::token_pos op,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(op->kind == lex::token::dereference);
	bz_assert(!expr.is<ast::tuple_expression>());
	bz_assert(expr.not_null());
	auto const [type, _] = expr.get_expr_type_and_kind();
	auto &expr_t = remove_const(type);
	lex::src_tokens const src_tokens = { op, op, expr.get_tokens_end() };

	if (!expr_t.is<ast::ts_pointer>())
	{
		context.report_error(
			src_tokens,
			bz::format(undeclared_unary_message("*"), type)
		);
		return ast::expression(src_tokens);
	}

	if (expr.is<ast::constant_expression>())
	{
		auto &const_expr = expr.get<ast::constant_expression>();
		bz_assert(const_expr.value.kind() == ast::constant_value::null);
		context.report_warning(
			src_tokens,
			"operator * dereferences a null pointer"
		);
	}

	auto result_type = expr_t.get<ast::ts_pointer_ptr>()->base;
	return ast::make_dynamic_expression(
		src_tokens,
		ast::expression_type_kind::lvalue_reference,
		std::move(result_type),
		ast::make_expr_unary_op(op, std::move(expr))
	);
}

// ~uintN -> uintN
// ~bool -> bool   (it's the same as !bool)
static ast::expression get_built_in_unary_bit_not(
	lex::token_pos op,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(op->kind == lex::token::bit_not);
	bz_assert(!expr.is<ast::tuple_expression>());
	bz_assert(expr.not_null());
	auto const [type, _] = expr.get_expr_type_and_kind();
	auto &expr_t = remove_const(type);
	lex::src_tokens const src_tokens = { op, op, expr.get_tokens_end() };

	if (!expr_t.is<ast::ts_base_type>())
	{
		context.report_error(
			src_tokens,
			bz::format(undeclared_unary_message("~"), type)
		);
		return ast::expression(src_tokens);
	}

	auto const kind = expr_t.get<ast::ts_base_type_ptr>()->info->kind;
	if (!is_unsigned_integer_kind(kind) && kind != ast::type_info::bool_)
	{
		context.report_error(
			src_tokens,
			bz::format(undeclared_unary_message("~"), type)
		);
		return ast::expression(src_tokens);
	}

	auto result_type = expr_t;
	if (expr.is<ast::constant_expression>())
	{
		auto &const_expr = expr.get<ast::constant_expression>();
		ast::constant_value value{};
		if (const_expr.value.kind() == ast::constant_value::uint)
		{
			auto const val = const_expr.value.get<ast::constant_value::uint>();
			switch (kind)
			{
			case ast::type_info::uint8_:
				value = static_cast<uint64_t>(static_cast<uint8_t>(~val));
				break;
			case ast::type_info::uint16_:
				value = static_cast<uint64_t>(static_cast<uint16_t>(~val));
				break;
			case ast::type_info::uint32_:
				value = static_cast<uint64_t>(static_cast<uint32_t>(~val));
				break;
			case ast::type_info::uint64_:
				value = static_cast<uint64_t>(static_cast<uint64_t>(~val));
				break;

			default:
				bz_assert(false);
				break;
			}
		}
		else
		{
			bz_assert(kind == ast::type_info::bool_);
			auto const val = const_expr.value.get<ast::constant_value::boolean>();
			value = !val;
		}

		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			std::move(result_type),
			value,
			ast::make_expr_unary_op(op, std::move(expr))
		);
	}
	else
	{
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			std::move(result_type),
			ast::make_expr_unary_op(op, std::move(expr))
		);
	}
}

// !bool -> bool
static ast::expression get_built_in_unary_bool_not(
	lex::token_pos op,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(op->kind == lex::token::bool_not);
	bz_assert(!expr.is<ast::tuple_expression>());
	bz_assert(expr.not_null());
	auto const [type, _] = expr.get_expr_type_and_kind();
	auto &expr_t = remove_const(type);
	lex::src_tokens const src_tokens = { op, op, expr.get_tokens_end() };

	if (!expr_t.is<ast::ts_base_type>())
	{
		context.report_error(
			src_tokens,
			bz::format(undeclared_unary_message("!"), type)
		);
		return ast::expression(src_tokens);
	}

	auto const kind = expr_t.get<ast::ts_base_type_ptr>()->info->kind;
	if (kind != ast::type_info::bool_)
	{
		context.report_error(
			src_tokens,
			bz::format(undeclared_unary_message("!"), type)
		);
		return ast::expression(src_tokens);
	}

	auto result_type = expr_t;
	if (expr.is<ast::constant_expression>())
	{
		auto &const_expr = expr.get<ast::constant_expression>();
		ast::constant_value value{};
		bz_assert(const_expr.value.kind() == ast::constant_value::boolean);
		auto const val = const_expr.value.get<ast::constant_value::boolean>();
		value = !val;

		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			std::move(result_type),
			value,
			ast::make_expr_unary_op(op, std::move(expr))
		);
	}
	else
	{
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			std::move(result_type),
			ast::make_expr_unary_op(op, std::move(expr))
		);
	}
}

// ++--sintN -> &sintN
// ++--uintN -> &uintN
// ++--char -> &char
// ++--ptr -> &ptr
static ast::expression get_built_in_unary_plus_plus_minus_minus(
	lex::token_pos op,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(op->kind == lex::token::plus_plus || op->kind == lex::token::minus_minus);
	bz_assert(!expr.is<ast::tuple_expression>());
	bz_assert(expr.not_null());
	auto const [type, type_kind] = expr.get_expr_type_and_kind();
	lex::src_tokens const src_tokens = { op, op, expr.get_tokens_end() };

	if (
		type_kind != ast::expression_type_kind::lvalue
		&& type_kind != ast::expression_type_kind::lvalue_reference
	)
	{
		context.report_error(
			src_tokens,
			bz::format(
				"cannot {} an rvalue",
				op->kind == lex::token::plus_plus ? "increment" : "decrement"
			)
		);
		return ast::expression(src_tokens);
	}

	if (type.is<ast::ts_constant>())
	{
		context.report_error(
			src_tokens,
			bz::format(
				"cannot {} a constant value",
				op->kind == lex::token::plus_plus ? "increment" : "decrement"
			)
		);
		return ast::expression(src_tokens);
	}
	else if (type.is<ast::ts_base_type>())
	{
		auto kind = type.get<ast::ts_base_type_ptr>()->info->kind;
		if (
			is_integer_kind(kind)
			|| kind == ast::type_info::char_
		)
		{
			auto result_type = type;
			return ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::rvalue,
				std::move(result_type),
				ast::make_expr_unary_op(op, std::move(expr))
			);
		}
	}
	else if (type.is<ast::ts_pointer>())
	{
		auto result_type = type;
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			std::move(result_type),
			ast::make_expr_unary_op(op, std::move(expr))
		);
	}

	context.report_error(
		src_tokens,
		bz::format(
			undeclared_unary_message("{}"),
			op->kind == lex::token::plus_plus ? "++" : "--", type
		)
	);
	return ast::expression(src_tokens);
}


ast::expression make_built_in_operation(
	lex::token_pos op,
	ast::expression expr,
	parse_context &context
)
{
	switch (op->kind)
	{
	case lex::token::plus:
		return get_built_in_unary_plus(op, expr, context);
	case lex::token::minus:
		return get_built_in_unary_minus(op, expr, context);
	case lex::token::dereference:
		return get_built_in_unary_dereference(op, expr, context);
	case lex::token::bit_not:
		return get_built_in_unary_bit_not(op, expr, context);
	case lex::token::bool_not:
		return get_built_in_unary_bool_not(op, expr, context);
	case lex::token::plus_plus:
	case lex::token::minus_minus:
		return get_built_in_unary_plus_plus_minus_minus(op, expr, context);

	default:
		bz_assert(false);
		return ast::expression();
	}
}

#undef undeclared_unary_message
#define undeclared_binary_message(op) "undeclared binary operator " op " with types '{}' and '{}'"

// sintN = sintM -> &sintN   where M <= N
// uintN = uintM -> &sintN   where M <= N
// floatN = floatN -> &floatN
// char = char -> &char
// str = str -> &str
// bool = bool -> &bool
// ptr = ptr -> &ptr
static ast::expression get_built_in_binary_assign(
	lex::token_pos op,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
)
{
	bz_assert(op->kind == lex::token::assign);
	bz_assert(!lhs.is<ast::tuple_expression>());
	bz_assert(!rhs.is<ast::tuple_expression>());
	bz_assert(lhs.not_null());
	bz_assert(rhs.not_null());
	auto const [lhs_type, lhs_type_kind] = lhs.get_expr_type_and_kind();
	auto const [rhs_type, rhs_type_kind] = rhs.get_expr_type_and_kind();
	auto &lhs_t = lhs_type;
	auto &rhs_t = ast::remove_const(rhs_type);
	lex::src_tokens const src_tokens = { lhs.get_tokens_begin(), op, rhs.get_tokens_end() };

	if (
		lhs_type_kind != ast::expression_type_kind::lvalue
		&& lhs_type_kind != ast::expression_type_kind::lvalue_reference
	)
	{
		context.report_error(src_tokens, "cannot assign to an rvalue");
		return ast::expression(src_tokens);
	}
	else if (lhs_t.is<ast::ts_constant>())
	{
		context.report_error(src_tokens, "cannot assign to a constant");
		return ast::expression(src_tokens);
	}

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto const [lhs_kind, rhs_kind] = get_base_kinds(lhs_t, rhs_t);
		auto result_type = lhs_t;
		auto const result_type_kind = lhs_type_kind;

		if (lhs_kind == rhs_kind)
		{
			return ast::make_dynamic_expression(
				src_tokens,
				result_type_kind,
				std::move(result_type),
				ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
			);
		}
		else if (
			is_signed_integer_kind(lhs_kind)
			&& is_signed_integer_kind(rhs_kind)
			&& lhs_kind >= rhs_kind
		)
		{
			rhs = make_built_in_cast(nullptr, std::move(rhs), lhs_t, context);
			return ast::make_dynamic_expression(
				src_tokens,
				result_type_kind,
				std::move(result_type),
				ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
			);
		}
		else if (
			is_unsigned_integer_kind(lhs_kind)
			&& is_unsigned_integer_kind(rhs_kind)
			&& lhs_kind >= rhs_kind
		)
		{
			rhs = make_built_in_cast(nullptr, std::move(rhs), lhs_t, context);
			return ast::make_dynamic_expression(
				src_tokens,
				result_type_kind,
				std::move(result_type),
				ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
			);
		}
		// floating point are covered earlier with lhs_kind == rhs_kind
		// we don't allow implicit conversion between float32 and float64
	}
	else if (lhs_t.is<ast::ts_pointer>() && rhs_t.is<ast::ts_pointer>())
	{
		auto result_type = lhs_t;
		auto const result_type_kind = lhs_type_kind;
		// TODO: use is_convertible here
		if (lhs_t.get<ast::ts_pointer_ptr>()->base == rhs_t.get<ast::ts_pointer_ptr>()->base)
		{
			return ast::make_dynamic_expression(
				src_tokens,
				result_type_kind,
				std::move(result_type),
				ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
			);
		}
	}
	// pointer = null
	else if (
		lhs_t.is<ast::ts_pointer>()
		&& rhs.is<ast::constant_expression>()
		&& rhs.get<ast::constant_expression>().value.kind() == ast::constant_value::null
	)
	{
		rhs = make_built_in_cast(nullptr, std::move(rhs), lhs_t, context);
		auto result_type = lhs_t;
		auto const result_type_kind = lhs_type_kind;
		return ast::make_dynamic_expression(
			src_tokens,
			result_type_kind,
			std::move(result_type),
			ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
		);
	}

	context.report_error(
		src_tokens,
		bz::format(undeclared_binary_message("="), lhs_type, rhs_type)
	);
	return ast::expression(src_tokens);
}

// sintN + sintM -> sint<max(N, M)>
// uintN + uintM -> uint<max(N, M)>
// floatN + floatN -> floatN
// char + int -> char
// int + char -> char
// ptr + int -> ptr
// int + ptr -> ptr
static ast::expression get_built_in_binary_plus(
	lex::token_pos op,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
)
{
	bz_assert(op->kind == lex::token::plus);
	bz_assert(!lhs.is<ast::tuple_expression>());
	bz_assert(!rhs.is<ast::tuple_expression>());
	bz_assert(lhs.not_null());
	bz_assert(rhs.not_null());
	auto const [lhs_type, lhs_type_kind] = lhs.get_expr_type_and_kind();
	auto const [rhs_type, rhs_type_kind] = rhs.get_expr_type_and_kind();
	auto &lhs_t = ast::remove_const(lhs_type);
	auto &rhs_t = ast::remove_const(rhs_type);
	lex::src_tokens const src_tokens = { lhs.get_tokens_begin(), op, rhs.get_tokens_end() };

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto [lhs_kind, rhs_kind] = get_base_kinds(lhs_t, rhs_t);
		if (
			is_signed_integer_kind(lhs_kind)
			&& is_signed_integer_kind(rhs_kind)
		)
		{
			auto common_kind = lhs_kind;
			if (lhs_kind > rhs_kind)
			{
				rhs = make_built_in_cast(
					nullptr,
					std::move(rhs),
					ast::make_ts_base_type({}, context.get_base_type_info(lhs_kind)),
					context
				);
				bz_assert(rhs.not_null());
			}
			else if (lhs_kind < rhs_kind)
			{
				common_kind = rhs_kind;
				lhs = make_built_in_cast(
					nullptr,
					std::move(lhs),
					ast::make_ts_base_type({}, context.get_base_type_info(rhs_kind)),
					context
				);
				bz_assert(lhs.not_null());
			}

			if (lhs.is<ast::constant_expression>() && rhs.is<ast::constant_expression>())
			{
				auto const [lhs_val, rhs_val] =
					get_constant_expression_values<ast::constant_value::sint>(lhs, rhs);
				auto const result = safe_add(
					lhs_val, rhs_val, common_kind,
					src_tokens, context
				);
				return ast::make_constant_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(common_kind)),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(common_kind)),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
		}
		else if (
			is_unsigned_integer_kind(lhs_kind)
			&& is_unsigned_integer_kind(rhs_kind)
		)
		{
			auto common_kind = lhs_kind;
			if (lhs_kind > rhs_kind)
			{
				rhs = make_built_in_cast(
					nullptr,
					std::move(rhs),
					ast::make_ts_base_type({}, context.get_base_type_info(lhs_kind)),
					context
				);
				bz_assert(rhs.not_null());
			}
			else if (lhs_kind < rhs_kind)
			{
				common_kind = rhs_kind;
				lhs = make_built_in_cast(
					nullptr,
					std::move(lhs),
					ast::make_ts_base_type({}, context.get_base_type_info(rhs_kind)),
					context
				);
				bz_assert(lhs.not_null());
			}

			if (lhs.is<ast::constant_expression>() && rhs.is<ast::constant_expression>())
			{
				auto const [lhs_val, rhs_val] =
					get_constant_expression_values<ast::constant_value::uint>(lhs, rhs);
				auto const result = safe_add(
					lhs_val, rhs_val, common_kind,
					src_tokens, context
				);
				return ast::make_constant_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(common_kind)),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(common_kind)),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
		}
		else if (
			is_floating_point_kind(lhs_kind)
			// && is_floating_point_kind(rhs_kind)
			&& lhs_kind == rhs_kind
		)
		{
			auto result_type = lhs_t;
			if (lhs.is<ast::constant_expression>() && rhs.is<ast::constant_expression>())
			{
				auto &const_lhs = lhs.get<ast::constant_expression>();
				auto &const_rhs = rhs.get<ast::constant_expression>();
				bz_assert(const_lhs.value.kind() == const_rhs.value.kind());
				ast::constant_value value{};
				switch (const_lhs.value.kind())
				{
				case ast::constant_value::float32:
					value = const_lhs.value.get<ast::constant_value::float32>()
						+ const_rhs.value.get<ast::constant_value::float32>();
					break;
				case ast::constant_value::float64:
					value = const_lhs.value.get<ast::constant_value::float64>()
						+ const_rhs.value.get<ast::constant_value::float64>();
					break;
				default:
					bz_assert(false);
					break;
				}
				return ast::make_constant_expression(
					src_tokens,
					ast::expression_type_kind::rvalue, std::move(result_type),
					value,
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue, std::move(result_type),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
		}
		else if (
			lhs_kind == ast::type_info::char_
			&& is_integer_kind(rhs_kind)
		)
		{
			auto result_type = lhs_t;
			if (lhs.is<ast::constant_expression>() && rhs.is<ast::constant_expression>())
			{
				auto &const_lhs = lhs.get<ast::constant_expression>();
				auto &const_rhs = rhs.get<ast::constant_expression>();
				bz_assert(const_lhs.value.kind() == ast::constant_value::u8char);
				auto const lhs_val = const_lhs.value.get<ast::constant_value::u8char>();
				bz::u8char result = '\0';
				switch (const_rhs.value.kind())
				{
				case ast::constant_value::sint:
					result = safe_add(
						lhs_val, const_rhs.value.get<ast::constant_value::sint>(), rhs_kind,
						src_tokens, context
					);
					break;
				case ast::constant_value::uint:
					result = safe_add(
						lhs_val, const_rhs.value.get<ast::constant_value::uint>(), rhs_kind,
						src_tokens, context
					);
					break;
				default:
					bz_assert(false);
					break;
				}
				return ast::make_constant_expression(
					src_tokens,
					ast::expression_type_kind::rvalue, std::move(result_type),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue, std::move(result_type),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
		}
		else if (
			is_integer_kind(lhs_kind)
			&& rhs_kind == ast::type_info::char_
		)
		{
			auto result_type = rhs_t;
			if (lhs.is<ast::constant_expression>() && rhs.is<ast::constant_expression>())
			{
				auto &const_lhs = lhs.get<ast::constant_expression>();
				auto &const_rhs = rhs.get<ast::constant_expression>();
				bz_assert(const_rhs.value.kind() == ast::constant_value::u8char);
				auto const rhs_val = const_rhs.value.get<ast::constant_value::u8char>();
				bz::u8char result = '\0';
				switch (const_lhs.value.kind())
				{
				case ast::constant_value::sint:
					result = safe_add(
						rhs_val, const_lhs.value.get<ast::constant_value::sint>(), lhs_kind,
						src_tokens, context, true
					);
					break;
				case ast::constant_value::uint:
					result = safe_add(
						rhs_val, const_lhs.value.get<ast::constant_value::uint>(), lhs_kind,
						src_tokens, context, true
					);
					break;
				default:
					bz_assert(false);
					break;
				}
				return ast::make_constant_expression(
					src_tokens,
					ast::expression_type_kind::rvalue, std::move(result_type),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue, std::move(result_type),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
		}
	}
	else if (
		lhs_t.is<ast::ts_pointer>()
		&& rhs_t.is<ast::ts_base_type>()
		&& is_integer_kind(rhs_t.get<ast::ts_base_type_ptr>()->info->kind)
	)
	{
		auto result_type = lhs_t;
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::rvalue, std::move(result_type),
			ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
		);
	}
	else if (
		rhs_t.is<ast::ts_pointer>()
		&& lhs_t.is<ast::ts_base_type>()
		&& is_integer_kind(lhs_t.get<ast::ts_base_type_ptr>()->info->kind)
	)
	{
		auto result_type = rhs_t;
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::rvalue, std::move(result_type),
			ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
		);
	}

	context.report_error(
		src_tokens,
		bz::format(undeclared_binary_message("+"), lhs_type, rhs_type)
	);
	return ast::expression(src_tokens);
}

// sint - sint -> sint
// uint - uint -> sint
// float - float -> float
// char - int -> char
// char - char -> int32
// ptr - int -> ptr
// ptr - ptr -> int64
static ast::expression get_built_in_binary_minus(
	lex::token_pos op,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
)
{
	bz_assert(op->kind == lex::token::minus);
	bz_assert(!lhs.is<ast::tuple_expression>());
	bz_assert(!rhs.is<ast::tuple_expression>());
	bz_assert(lhs.not_null());
	bz_assert(rhs.not_null());
	auto const [lhs_type, lhs_type_kind] = lhs.get_expr_type_and_kind();
	auto const [rhs_type, rhs_type_kind] = rhs.get_expr_type_and_kind();
	auto &lhs_t = ast::remove_const(lhs_type);
	auto &rhs_t = ast::remove_const(rhs_type);
	lex::src_tokens const src_tokens = { lhs.get_tokens_begin(), op, rhs.get_tokens_end() };

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto const [lhs_kind, rhs_kind] = get_base_kinds(lhs_t, rhs_t);
		if (
			is_signed_integer_kind(lhs_kind)
			&& is_signed_integer_kind(rhs_kind)
		)
		{
			auto common_kind = lhs_kind;
			if (lhs_kind > rhs_kind)
			{
				rhs = make_built_in_cast(
					nullptr,
					std::move(rhs),
					ast::make_ts_base_type({}, context.get_base_type_info(lhs_kind)),
					context
				);
				bz_assert(rhs.not_null());
			}
			else if (lhs_kind < rhs_kind)
			{
				common_kind = rhs_kind;
				lhs = make_built_in_cast(
					nullptr,
					std::move(lhs),
					ast::make_ts_base_type({}, context.get_base_type_info(rhs_kind)),
					context
				);
				bz_assert(lhs.not_null());
			}

			if (lhs.is<ast::constant_expression>() && rhs.is<ast::constant_expression>())
			{
				auto const [lhs_val, rhs_val] =
					get_constant_expression_values<ast::constant_value::sint>(lhs, rhs);
				auto const result = safe_subtract(
					lhs_val, rhs_val, common_kind,
					src_tokens, context
				);
				return ast::make_constant_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(common_kind)),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(common_kind)),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
		}
		else if (
			is_unsigned_integer_kind(lhs_kind)
			&& is_unsigned_integer_kind(rhs_kind)
		)
		{
			auto common_kind = lhs_kind;
			if (lhs_kind > rhs_kind)
			{
				rhs = make_built_in_cast(
					nullptr,
					std::move(rhs),
					ast::make_ts_base_type({}, context.get_base_type_info(lhs_kind)),
					context
				);
				bz_assert(rhs.not_null());
			}
			else if (lhs_kind < rhs_kind)
			{
				common_kind = rhs_kind;
				lhs = make_built_in_cast(
					nullptr,
					std::move(lhs),
					ast::make_ts_base_type({}, context.get_base_type_info(rhs_kind)),
					context
				);
				bz_assert(lhs.not_null());
			}

			if (lhs.is<ast::constant_expression>() && rhs.is<ast::constant_expression>())
			{
				auto const [lhs_val, rhs_val] =
					get_constant_expression_values<ast::constant_value::uint>(lhs, rhs);
				auto const result = safe_subtract(
					lhs_val, rhs_val, common_kind,
					src_tokens, context
				);
				return ast::make_constant_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(common_kind)),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(common_kind)),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
		}
		else if (
			is_floating_point_kind(lhs_kind)
			// && is_floating_point_kind(rhs_kind)
			&& lhs_kind == rhs_kind
		)
		{
			auto result_type = lhs_t;
			if (lhs.is<ast::constant_expression>() && rhs.is<ast::constant_expression>())
			{
				auto &const_lhs = lhs.get<ast::constant_expression>();
				auto &const_rhs = rhs.get<ast::constant_expression>();
				bz_assert(const_lhs.value.kind() == const_rhs.value.kind());
				ast::constant_value value{};
				switch (const_lhs.value.kind())
				{
				case ast::constant_value::float32:
					value = const_lhs.value.get<ast::constant_value::float32>()
						- const_rhs.value.get<ast::constant_value::float32>();
					break;
				case ast::constant_value::float64:
					value = const_lhs.value.get<ast::constant_value::float64>()
						- const_rhs.value.get<ast::constant_value::float64>();
					break;
				default:
					bz_assert(false);
					break;
				}
				return ast::make_constant_expression(
					src_tokens,
					ast::expression_type_kind::rvalue, std::move(result_type),
					value,
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue, std::move(result_type),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
		}
		else if (
			lhs_kind == ast::type_info::char_
			&& is_integer_kind(rhs_kind)
		)
		{
			auto result_type = lhs_t;
			if (lhs.is<ast::constant_expression>() && rhs.is<ast::constant_expression>())
			{
				auto &const_lhs = lhs.get<ast::constant_expression>();
				auto &const_rhs = rhs.get<ast::constant_expression>();
				bz_assert(const_lhs.value.kind() == ast::constant_value::u8char);
				auto const lhs_val = const_lhs.value.get<ast::constant_value::u8char>();
				bz::u8char result;
				switch (const_rhs.value.kind())
				{
				case ast::constant_value::sint:
					result = safe_subtract(
						lhs_val, const_rhs.value.get<ast::constant_value::sint>(), rhs_kind,
						src_tokens, context
					);
					break;
				case ast::constant_value::uint:
					result = safe_subtract(
						lhs_val, const_rhs.value.get<ast::constant_value::uint>(), rhs_kind,
						src_tokens, context
					);
					break;
				default:
					bz_assert(false);
					break;
				}
				return ast::make_constant_expression(
					src_tokens,
					ast::expression_type_kind::rvalue, std::move(result_type),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue, std::move(result_type),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
		}
		else if (
			lhs_kind == ast::type_info::char_
			&& rhs_kind == ast::type_info::char_
		)
		{
			if (lhs.is<ast::constant_expression>() && rhs.is<ast::constant_expression>())
			{
				auto const [lhs_val, rhs_val] =
					get_constant_expression_values<ast::constant_value::u8char>(lhs, rhs);
				int64_t const result = safe_subtract(
					lhs_val, rhs_val,
					src_tokens, context
				);
				return ast::make_constant_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(ast::type_info::int32_)),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(ast::type_info::int32_)),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
		}
	}
	else if (
		lhs_t.is<ast::ts_pointer>()
		&& rhs_t.is<ast::ts_base_type>()
		&& is_integer_kind(rhs_t.get<ast::ts_base_type_ptr>()->info->kind)
	)
	{
		auto result_type = lhs_t;
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::rvalue, std::move(result_type),
			ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
		);
	}
	else if (lhs_t.is<ast::ts_pointer>() && rhs_t.is<ast::ts_pointer>())
	{
		// TODO: use some kind of are_matchable_types here
		if (lhs_t.get<ast::ts_pointer_ptr>()->base == rhs_t.get<ast::ts_pointer_ptr>()->base)
		{
			return ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::rvalue,
				ast::make_ts_base_type({}, context.get_base_type_info(ast::type_info::int64_)),
				ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
			);
		}
	}

	context.report_error(
		src_tokens,
		bz::format(undeclared_binary_message("-"), lhs_type, rhs_type)
	);
	return ast::expression(src_tokens);
}

// sintN +-= sintM    N >= M
// uintN +-= uintM    N >= M
// floatN +-= floatN
// char +-= int
// ptr +-= int
static ast::expression get_built_in_binary_plus_minus_eq(
	lex::token_pos op,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
)
{
	bz_assert(op->kind == lex::token::plus_eq || op->kind == lex::token::minus_eq);
	bz_assert(!lhs.is<ast::tuple_expression>());
	bz_assert(!rhs.is<ast::tuple_expression>());
	bz_assert(lhs.not_null());
	bz_assert(rhs.not_null());
	auto const [lhs_type, lhs_type_kind] = lhs.get_expr_type_and_kind();
	auto const [rhs_type, rhs_type_kind] = rhs.get_expr_type_and_kind();
	auto &lhs_t = lhs_type;
	auto &rhs_t = ast::remove_const(rhs_type);
	lex::src_tokens const src_tokens = { lhs.get_tokens_begin(), op, rhs.get_tokens_end() };

	if (
		lhs_type_kind != ast::expression_type_kind::lvalue
		&& lhs_type_kind != ast::expression_type_kind::lvalue_reference
	)
	{
		context.report_error(src_tokens, "cannot assign to an rvalue");
		return ast::expression(src_tokens);
	}
	else if (lhs_t.is<ast::ts_constant>())
	{
		context.report_error(src_tokens, "cannot assign to a constant");
		return ast::expression(src_tokens);
	}

	auto result_type = lhs_t;
	auto const result_type_kind = lhs_type_kind;

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto [lhs_kind, rhs_kind] = get_base_kinds(lhs_t, rhs_t);
		if (
			is_signed_integer_kind(lhs_kind)
			&& is_signed_integer_kind(rhs_kind)
			&& lhs_kind >= rhs_kind
		)
		{
			if (lhs_kind != rhs_kind)
			{
				rhs = make_built_in_cast(nullptr, std::move(rhs), lhs_t, context);
			}
			return ast::make_dynamic_expression(
				src_tokens,
				result_type_kind, std::move(result_type),
				ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
			);
		}
		else if (
			is_unsigned_integer_kind(lhs_kind)
			&& is_unsigned_integer_kind(rhs_kind)
			&& lhs_kind >= rhs_kind
		)
		{
			if (lhs_kind != rhs_kind)
			{
				rhs = make_built_in_cast(nullptr, std::move(rhs), lhs_t, context);
			}
			return ast::make_dynamic_expression(
				src_tokens,
				result_type_kind, std::move(result_type),
				ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
			);
		}
		else if (
			is_floating_point_kind(lhs_kind)
			// && is_floating_point_kind(rhs_kind)
			&& lhs_kind == rhs_kind
		)
		{
			if (lhs_kind != rhs_kind)
			{
				rhs = make_built_in_cast(nullptr, std::move(rhs), lhs_t, context);
			}
			return ast::make_dynamic_expression(
				src_tokens,
				result_type_kind, std::move(result_type),
				ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
			);
		}
		else if (
			lhs_kind == ast::type_info::char_
			&& is_integer_kind(rhs_kind)
		)
		{
			return ast::make_dynamic_expression(
				src_tokens,
				result_type_kind, std::move(result_type),
				ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
			);
		}
	}
	else if (
		lhs_t.is<ast::ts_pointer>()
		&& rhs_t.is<ast::ts_base_type>()
		&& is_integer_kind(rhs_t.get<ast::ts_base_type_ptr>()->info->kind)
	)
	{
		return ast::make_dynamic_expression(
			src_tokens,
			result_type_kind, std::move(result_type),
			ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
		);
	}

	context.report_error(
		src_tokens,
		bz::format(
			undeclared_binary_message("{}"),
			op->kind == lex::token::plus_eq ? "+=" : "-=", lhs_type, rhs_type
		)
	);
	return ast::expression(src_tokens);
}

// sint */ sint -> sint
// uint */ uint -> uint
// float */ float -> float
static ast::expression get_built_in_binary_multiply_divide(
	lex::token_pos op,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
)
{
	bz_assert(op->kind == lex::token::multiply || op->kind == lex::token::divide);
	bz_assert(!lhs.is<ast::tuple_expression>());
	bz_assert(!rhs.is<ast::tuple_expression>());
	bz_assert(lhs.not_null());
	bz_assert(rhs.not_null());
	auto const [lhs_type, lhs_type_kind] = lhs.get_expr_type_and_kind();
	auto const [rhs_type, rhs_type_kind] = rhs.get_expr_type_and_kind();
	auto &lhs_t = ast::remove_const(lhs_type);
	auto &rhs_t = ast::remove_const(rhs_type);
	lex::src_tokens const src_tokens = { lhs.get_tokens_begin(), op, rhs.get_tokens_end() };

	auto const is_multiply = op->kind == lex::token::multiply;

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto const [lhs_kind, rhs_kind] = get_base_kinds(lhs_t, rhs_t);
		if (
			is_signed_integer_kind(lhs_kind)
			&& is_signed_integer_kind(rhs_kind)
		)
		{
			auto common_kind = lhs_kind;
			if (lhs_kind > rhs_kind)
			{
				rhs = make_built_in_cast(
					nullptr,
					std::move(rhs),
					ast::make_ts_base_type({}, context.get_base_type_info(lhs_kind)),
					context
				);
			}
			else if (lhs_kind < rhs_kind)
			{
				common_kind = rhs_kind;
				lhs = make_built_in_cast(
					nullptr,
					std::move(lhs),
					ast::make_ts_base_type({}, context.get_base_type_info(rhs_kind)),
					context
				);
			}

			if (lhs.is<ast::constant_expression>() && rhs.is<ast::constant_expression>())
			{
				auto const [lhs_val, rhs_val] =
					get_constant_expression_values<ast::constant_value::sint>(lhs, rhs);
				auto const result = is_multiply
					? safe_multiply(
						lhs_val, rhs_val, common_kind,
						src_tokens, context
					)
					: safe_divide(
						lhs_val, rhs_val, common_kind,
						src_tokens, context
					);

				return ast::make_constant_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(common_kind)),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				if (!is_multiply && rhs.is<ast::constant_expression>())
				{
					auto &const_rhs = rhs.get<ast::constant_expression>();
					auto const rhs_val = const_rhs.value.get<ast::constant_value::sint>();
					if (rhs_val == 0)
					{
						context.report_warning(src_tokens, "dividing by zero in integer arithmetic");
					}
				}
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(common_kind)),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
		}
		else if (
			is_unsigned_integer_kind(lhs_kind)
			&& is_unsigned_integer_kind(rhs_kind)
		)
		{
			auto common_kind = lhs_kind;
			if (lhs_kind > rhs_kind)
			{
				rhs = make_built_in_cast(
					nullptr,
					std::move(rhs),
					ast::make_ts_base_type({}, context.get_base_type_info(lhs_kind)),
					context
				);
			}
			else if (lhs_kind < rhs_kind)
			{
				common_kind = rhs_kind;
				lhs = make_built_in_cast(
					nullptr,
					std::move(lhs),
					ast::make_ts_base_type({}, context.get_base_type_info(rhs_kind)),
					context
				);
			}

			if (lhs.is<ast::constant_expression>() && rhs.is<ast::constant_expression>())
			{
				auto const [lhs_val, rhs_val] =
					get_constant_expression_values<ast::constant_value::uint>(lhs, rhs);
				auto const result = is_multiply
					? safe_multiply(
						lhs_val, rhs_val, common_kind,
						src_tokens, context
					)
					: safe_divide(
						lhs_val, rhs_val, common_kind,
						src_tokens, context
					);

				return ast::make_constant_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(common_kind)),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				if (!is_multiply && rhs.is<ast::constant_expression>())
				{
					auto &const_rhs = rhs.get<ast::constant_expression>();
					auto const rhs_val = const_rhs.value.get<ast::constant_value::uint>();
					if (rhs_val == 0)
					{
						context.report_warning(src_tokens, "dividing by zero in integer arithmetic");
					}
				}
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(common_kind)),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
		}
		else if (
			is_floating_point_kind(lhs_kind)
			// && is_floating_point_kind(rhs_kind)
			&& lhs_kind == rhs_kind
		)
		{
			auto result_type = lhs_t;
			if (lhs.is<ast::constant_expression>() && rhs.is<ast::constant_expression>())
			{
				ast::constant_value value{};
				switch (lhs_kind)
				{
				case ast::type_info::float32_:
				{
					auto const [lhs_val, rhs_val] =
						get_constant_expression_values<ast::constant_value::float32>(lhs, rhs);
					if (is_multiply)
					{
						value = lhs_val * rhs_val;
					}
					else
					{
						value = lhs_val / rhs_val;
					}
					break;
				}
				case ast::type_info::float64_:
				{
					auto const [lhs_val, rhs_val] =
						get_constant_expression_values<ast::constant_value::float64>(lhs, rhs);
					if (is_multiply)
					{
						value = lhs_val * rhs_val;
					}
					else
					{
						value = lhs_val / rhs_val;
					}
					break;
				}
				default:
					bz_assert(false);
					break;
				}
				return ast::make_constant_expression(
					src_tokens,
					ast::expression_type_kind::rvalue, std::move(result_type),
					value,
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue, std::move(result_type),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
		}
	}

	context.report_error(
		src_tokens,
		bz::format(undeclared_binary_message("{}"), is_multiply ? "*" : "/", lhs_type, rhs_type)
	);
	return ast::expression(src_tokens);
}

// sintN */= sintM    N >= M
// uintN */= uintM    N >= M
// floatN */= floatN
static ast::expression get_built_in_binary_multiply_divide_eq(
	lex::token_pos op,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
)
{
	bz_assert(op->kind == lex::token::multiply_eq || op->kind == lex::token::divide_eq);
	bz_assert(!lhs.is<ast::tuple_expression>());
	bz_assert(!rhs.is<ast::tuple_expression>());
	bz_assert(lhs.not_null());
	bz_assert(rhs.not_null());
	auto const [lhs_type, lhs_type_kind] = lhs.get_expr_type_and_kind();
	auto const [rhs_type, rhs_type_kind] = rhs.get_expr_type_and_kind();
	auto &lhs_t = lhs_type;
	auto &rhs_t = ast::remove_const(rhs_type);
	lex::src_tokens const src_tokens = { lhs.get_tokens_begin(), op, rhs.get_tokens_end() };

	if (
		lhs_type_kind != ast::expression_type_kind::lvalue
		&& lhs_type_kind != ast::expression_type_kind::lvalue_reference
	)
	{
		context.report_error(src_tokens, "cannot assign to an rvalue");
		return ast::expression(src_tokens);
	}
	else if (lhs_t.is<ast::ts_constant>())
	{
		context.report_error(src_tokens, "cannot assign to a constant");
		return ast::expression(src_tokens);
	}

	auto result_type = lhs_t;
	auto const result_type_kind = lhs_type_kind;

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto [lhs_kind, rhs_kind] = get_base_kinds(lhs_t, rhs_t);
		if (
			is_signed_integer_kind(lhs_kind)
			&& is_signed_integer_kind(rhs_kind)
			&& lhs_kind >= rhs_kind
		)
		{
			if (lhs_kind != rhs_kind)
			{
				rhs = make_built_in_cast(nullptr, std::move(rhs), lhs_t, context);
			}
			// warn if there's a division by zero
			if (
				op->kind == lex::token::divide_eq
				&& rhs.is<ast::constant_expression>()
				&& rhs.get<ast::constant_expression>().value.get<ast::constant_value::sint>() == 0
			)
			{
				context.report_warning(src_tokens, "dividing by zero in integer arithmetic");
			}
			return ast::make_dynamic_expression(
				src_tokens,
				result_type_kind, std::move(result_type),
				ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
			);
		}
		else if (
			is_unsigned_integer_kind(lhs_kind)
			&& is_unsigned_integer_kind(rhs_kind)
			&& lhs_kind >= rhs_kind
		)
		{
			if (lhs_kind != rhs_kind)
			{
				rhs = make_built_in_cast(nullptr, std::move(rhs), lhs_t, context);
			}
			// warn if there's a division by zero
			if (
				op->kind == lex::token::divide_eq
				&& rhs.is<ast::constant_expression>()
				&& rhs.get<ast::constant_expression>().value.get<ast::constant_value::uint>() == 0
			)
			{
				context.report_warning(src_tokens, "dividing by zero in integer arithmetic");
			}
			return ast::make_dynamic_expression(
				src_tokens,
				result_type_kind, std::move(result_type),
				ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
			);
		}
		else if (
			is_floating_point_kind(lhs_kind)
			&& is_floating_point_kind(rhs_kind)
			&& lhs_kind >= rhs_kind
		)
		{
			return ast::make_dynamic_expression(
				src_tokens,
				result_type_kind, std::move(result_type),
				ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
			);
		}
	}

	context.report_error(
		src_tokens,
		bz::format(
			undeclared_binary_message("{}"),
			op->kind == lex::token::multiply_eq ? "*=" : "/=", lhs_type, rhs_type
		)
	);
	return ast::expression(src_tokens);
}

// sint % sint
// uint % uint
static ast::expression get_built_in_binary_modulo(
	lex::token_pos op,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
)
{
	bz_assert(op->kind == lex::token::modulo);
	bz_assert(!lhs.is<ast::tuple_expression>());
	bz_assert(!rhs.is<ast::tuple_expression>());
	bz_assert(lhs.not_null());
	bz_assert(rhs.not_null());
	auto const [lhs_type, lhs_type_kind] = lhs.get_expr_type_and_kind();
	auto const [rhs_type, rhs_type_kind] = rhs.get_expr_type_and_kind();
	auto &lhs_t = ast::remove_const(lhs_type);
	auto &rhs_t = ast::remove_const(rhs_type);
	lex::src_tokens const src_tokens = { lhs.get_tokens_begin(), op, rhs.get_tokens_end() };

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto const [lhs_kind, rhs_kind] = get_base_kinds(lhs_t, rhs_t);
		if (
			is_signed_integer_kind(lhs_kind)
			&& is_signed_integer_kind(rhs_kind)
		)
		{
			auto common_kind = lhs_kind;
			if (lhs_kind > rhs_kind)
			{
				rhs = make_built_in_cast(
					nullptr,
					std::move(rhs),
					ast::make_ts_base_type({}, context.get_base_type_info(lhs_kind)),
					context
				);
			}
			else if (lhs_kind < rhs_kind)
			{
				common_kind = rhs_kind;
				lhs = make_built_in_cast(
					nullptr,
					std::move(lhs),
					ast::make_ts_base_type({}, context.get_base_type_info(rhs_kind)),
					context
				);
			}

			if (lhs.is<ast::constant_expression>() && rhs.is<ast::constant_expression>())
			{
				auto const [lhs_val, rhs_val] =
					get_constant_expression_values<ast::constant_value::sint>(lhs, rhs);
				auto const result = safe_modulo(
					lhs_val, rhs_val, common_kind,
					src_tokens, context
				);

				return ast::make_constant_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(common_kind)),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				// warn if there's a modulo by zero
				if (
					rhs.is<ast::constant_expression>()
					&& rhs.get<ast::constant_expression>().value.get<ast::constant_value::sint>() == 0
				)
				{
					context.report_warning(src_tokens, "modulo by zero in integer arithmetic");
				}
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(common_kind)),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
		}
		else if (
			is_unsigned_integer_kind(lhs_kind)
			&& is_unsigned_integer_kind(rhs_kind)
		)
		{
			auto common_kind = lhs_kind;
			if (lhs_kind > rhs_kind)
			{
				rhs = make_built_in_cast(
					nullptr,
					std::move(rhs),
					ast::make_ts_base_type({}, context.get_base_type_info(lhs_kind)),
					context
				);
			}
			else if (lhs_kind < rhs_kind)
			{
				common_kind = rhs_kind;
				lhs = make_built_in_cast(
					nullptr,
					std::move(lhs),
					ast::make_ts_base_type({}, context.get_base_type_info(rhs_kind)),
					context
				);
			}

			if (lhs.is<ast::constant_expression>() && rhs.is<ast::constant_expression>())
			{
				auto const [lhs_val, rhs_val] =
					get_constant_expression_values<ast::constant_value::uint>(lhs, rhs);
				auto const result = safe_modulo(
					lhs_val, rhs_val, common_kind,
					src_tokens, context
				);

				return ast::make_constant_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(common_kind)),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				// warn if there's a modulo by zero
				if (
					rhs.is<ast::constant_expression>()
					&& rhs.get<ast::constant_expression>().value.get<ast::constant_value::uint>() == 0
				)
				{
					context.report_warning(src_tokens, "modulo by zero in integer arithmetic");
				}
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(common_kind)),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
		}
	}

	context.report_error(
		src_tokens,
		bz::format(undeclared_binary_message("%"), lhs_type, rhs_type)
	);
	return ast::expression(src_tokens);
}

// sintN %= sintM  N >= M
// uintN %= uintM  N >= M
static ast::expression get_built_in_binary_modulo_eq(
	lex::token_pos op,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
)
{
	bz_assert(op->kind == lex::token::modulo_eq);
	bz_assert(!lhs.is<ast::tuple_expression>());
	bz_assert(!rhs.is<ast::tuple_expression>());
	bz_assert(lhs.not_null());
	bz_assert(rhs.not_null());
	auto const [lhs_type, lhs_type_kind] = lhs.get_expr_type_and_kind();
	auto const [rhs_type, rhs_type_kind] = rhs.get_expr_type_and_kind();
	auto &lhs_t = lhs_type;
	auto &rhs_t = ast::remove_const(rhs_type);

	lex::src_tokens const src_tokens = { lhs.get_tokens_begin(), op, rhs.get_tokens_end() };

	if (
		lhs_type_kind != ast::expression_type_kind::lvalue
		&& lhs_type_kind != ast::expression_type_kind::lvalue_reference
	)
	{
		context.report_error(src_tokens, "cannot assign to an rvalue");
		return ast::expression(src_tokens);
	}
	else if (lhs_t.is<ast::ts_constant>())
	{
		context.report_error(src_tokens, "cannot assign to a constant");
		return ast::expression(src_tokens);
	}

	auto result_type = lhs_t;
	auto const result_type_kind = lhs_type_kind;

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto [lhs_kind, rhs_kind] = get_base_kinds(lhs_t, rhs_t);
		if (
			is_signed_integer_kind(lhs_kind)
			&& is_signed_integer_kind(rhs_kind)
			&& lhs_kind >= rhs_kind
		)
		{
			if (lhs_kind != rhs_kind)
			{
				rhs = make_built_in_cast(nullptr, std::move(rhs), lhs_t, context);
			}
			// warn if there's a modulo by zero
			if (
				rhs.is<ast::constant_expression>()
				&& rhs.get<ast::constant_expression>().value.get<ast::constant_value::sint>() == 0
			)
			{
				context.report_warning(src_tokens, "modulo by zero in integer arithmetic");
			}
			return ast::make_dynamic_expression(
				src_tokens,
				result_type_kind, std::move(result_type),
				ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
			);
		}
		else if (
			is_unsigned_integer_kind(lhs_kind)
			&& is_unsigned_integer_kind(rhs_kind)
			&& lhs_kind >= rhs_kind
		)
		{
			if (lhs_kind != rhs_kind)
			{
				rhs = make_built_in_cast(nullptr, std::move(rhs), lhs_t, context);
			}
			// warn if there's a modulo by zero
			if (
				rhs.is<ast::constant_expression>()
				&& rhs.get<ast::constant_expression>().value.get<ast::constant_value::uint>() == 0
			)
			{
				context.report_warning(src_tokens, "modulo by zero in integer arithmetic");
			}
			return ast::make_dynamic_expression(
				src_tokens,
				result_type_kind, std::move(result_type),
				ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
			);
		}
	}

	context.report_error(
		src_tokens,
		bz::format(undeclared_binary_message("%="), lhs_type, rhs_type)
	);
	return ast::expression(src_tokens);
}

// sint !== sint
// uint !== uint
// floatN !== floatN
// char !== char
// str !== str
// bool !== bool
// ptr !== ptr
static ast::expression get_built_in_binary_equals_not_equals(
	lex::token_pos op,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
)
{
	bz_assert(op->kind == lex::token::equals || op->kind == lex::token::not_equals);
	bz_assert(!lhs.is<ast::tuple_expression>());
	bz_assert(!rhs.is<ast::tuple_expression>());
	bz_assert(lhs.not_null());
	bz_assert(rhs.not_null());
	auto const [lhs_type, lhs_type_kind] = lhs.get_expr_type_and_kind();
	auto const [rhs_type, rhs_type_kind] = rhs.get_expr_type_and_kind();
	auto &lhs_t = ast::remove_const(lhs_type);
	auto &rhs_t = ast::remove_const(rhs_type);
	lex::src_tokens const src_tokens = { lhs.get_tokens_begin(), op, rhs.get_tokens_end() };

	const auto is_equals = op->kind == lex::token::equals;

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto const [lhs_kind, rhs_kind] = get_base_kinds(lhs_t, rhs_t);

		if (
			is_signed_integer_kind(lhs_kind)
			&& is_signed_integer_kind(rhs_kind)
		)
		{
			auto common_kind = lhs_kind;
			if (lhs_kind > rhs_kind)
			{
				rhs = make_built_in_cast(
					nullptr,
					std::move(rhs),
					ast::make_ts_base_type({}, context.get_base_type_info(lhs_kind)),
					context
				);
			}
			else if (lhs_kind < rhs_kind)
			{
				common_kind = rhs_kind;
				lhs = make_built_in_cast(
					nullptr,
					std::move(lhs),
					ast::make_ts_base_type({}, context.get_base_type_info(rhs_kind)),
					context
				);
			}

			if (lhs.is<ast::constant_expression>() && rhs.is<ast::constant_expression>())
			{
				auto const [lhs_val, rhs_val] =
					get_constant_expression_values<ast::constant_value::sint>(lhs, rhs);
				auto const result = is_equals ? lhs_val == rhs_val : lhs_val != rhs_val;
				return ast::make_constant_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(ast::type_info::bool_)),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(ast::type_info::bool_)),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
		}
		else if (
			is_unsigned_integer_kind(lhs_kind)
			&& is_unsigned_integer_kind(rhs_kind)
		)
		{
			auto common_kind = lhs_kind;
			if (lhs_kind > rhs_kind)
			{
				rhs = make_built_in_cast(
					nullptr,
					std::move(rhs),
					ast::make_ts_base_type({}, context.get_base_type_info(lhs_kind)),
					context
				);
				bz_assert(rhs.not_null());
			}
			else if (lhs_kind < rhs_kind)
			{
				common_kind = rhs_kind;
				lhs = make_built_in_cast(
					nullptr,
					std::move(lhs),
					ast::make_ts_base_type({}, context.get_base_type_info(rhs_kind)),
					context
				);
				bz_assert(lhs.not_null());
			}

			if (lhs.is<ast::constant_expression>() && rhs.is<ast::constant_expression>())
			{
				auto const [lhs_val, rhs_val] =
					get_constant_expression_values<ast::constant_value::uint>(lhs, rhs);
				auto const result = is_equals ? lhs_val == rhs_val : lhs_val != rhs_val;
				return ast::make_constant_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(ast::type_info::bool_)),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(ast::type_info::bool_)),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
		}
		else if (
			is_floating_point_kind(lhs_kind)
			// && is_floating_point_kind(rhs_kind)
			&& lhs_kind == rhs_kind
		)
		{
			if (lhs.is<ast::constant_expression>() && rhs.is<ast::constant_expression>())
			{
				bool result;
				if (lhs_kind == ast::type_info::float32_)
				{
					auto const [lhs_val, rhs_val] =
						get_constant_expression_values<ast::constant_value::float32>(lhs, rhs);
					result = is_equals ? lhs_val == rhs_val : lhs_val != rhs_val;
				}
				else
				{
					bz_assert(lhs_kind == ast::type_info::float64_);
					auto const [lhs_val, rhs_val] =
						get_constant_expression_values<ast::constant_value::float64>(lhs, rhs);
					result = is_equals ? lhs_val == rhs_val : lhs_val != rhs_val;
				}
				return ast::make_constant_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(ast::type_info::bool_)),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(ast::type_info::bool_)),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
		}
		else if (
			lhs_kind == ast::type_info::char_
			&& rhs_kind == ast::type_info::char_
		)
		{
			if (lhs.is<ast::constant_expression>() && rhs.is<ast::constant_expression>())
			{
				auto const [lhs_val, rhs_val] =
					get_constant_expression_values<ast::constant_value::u8char>(lhs, rhs);
				auto const result = is_equals ? lhs_val == rhs_val : lhs_val != rhs_val;
				return ast::make_constant_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(ast::type_info::bool_)),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(ast::type_info::bool_)),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
		}
		else if (
			lhs_kind == ast::type_info::str_
			&& rhs_kind == ast::type_info::str_
		)
		{
			if (lhs.is<ast::constant_expression>() && rhs.is<ast::constant_expression>())
			{
				auto const [lhs_val, rhs_val] =
					get_constant_expression_values<ast::constant_value::string>(lhs, rhs);
				auto const result = is_equals ? lhs_val == rhs_val : lhs_val != rhs_val;
				return ast::make_constant_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(ast::type_info::bool_)),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(ast::type_info::bool_)),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
		}
		else if (
			lhs_kind == ast::type_info::bool_
			&& rhs_kind == ast::type_info::bool_
		)
		{
			if (lhs.is<ast::constant_expression>() && rhs.is<ast::constant_expression>())
			{
				auto const [lhs_val, rhs_val] =
					get_constant_expression_values<ast::constant_value::boolean>(lhs, rhs);
				auto const result = is_equals ? lhs_val == rhs_val : lhs_val != rhs_val;
				return ast::make_constant_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(ast::type_info::bool_)),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(ast::type_info::bool_)),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
		}
	}
	else if (
		lhs_t.is<ast::ts_pointer>()
		&& rhs_t.is<ast::ts_pointer>()
		// TODO: use some kind of are_matchable_types here
		&& lhs_t.get<ast::ts_pointer_ptr>()->base == rhs_t.get<ast::ts_pointer_ptr>()->base
	)
	{
		if (lhs.is<ast::constant_expression>() && rhs.is<ast::constant_expression>())
		{
			bz_assert(lhs.get<ast::constant_expression>().value.kind() == ast::constant_value::null);
			bz_assert(rhs.get<ast::constant_expression>().value.kind() == ast::constant_value::null);
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::rvalue,
				ast::make_ts_base_type({}, context.get_base_type_info(ast::type_info::bool_)),
				ast::constant_value(is_equals),
				ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
			);
		}
		else
		{
			return ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::rvalue,
				ast::make_ts_base_type({}, context.get_base_type_info(ast::type_info::bool_)),
				ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
			);
		}
	}
	// ptr !== null
	else if (
		lhs_t.is<ast::ts_pointer>()
		&& rhs.is<ast::constant_expression>()
		&& rhs.get<ast::constant_expression>().value.kind() == ast::constant_value::null
	)
	{
		rhs = make_built_in_cast(nullptr, std::move(rhs), lhs_t, context);
		if (lhs.is<ast::constant_expression>())
		{
			auto &const_lhs = lhs.get<ast::constant_expression>();
			bz_assert(const_lhs.value.kind() == ast::constant_value::null);
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::rvalue,
				ast::make_ts_base_type({}, context.get_base_type_info(ast::type_info::bool_)),
				ast::constant_value(is_equals),
				ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
			);
		}
		else
		{
			return ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::rvalue,
				ast::make_ts_base_type({}, context.get_base_type_info(ast::type_info::bool_)),
				ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
			);
		}
	}
	// null !== ptr
	else if (
		rhs_t.is<ast::ts_pointer>()
		&& lhs.is<ast::constant_expression>()
		&& lhs.get<ast::constant_expression>().value.kind() == ast::constant_value::null
	)
	{
		auto &const_lhs = lhs.get<ast::constant_expression>();
		const_lhs.type = rhs_t;
		const_lhs.kind = ast::expression_type_kind::rvalue;
		if (rhs.is<ast::constant_expression>())
		{
			auto &const_rhs = rhs.get<ast::constant_expression>();
			bz_assert(const_rhs.value.kind() == ast::constant_value::null);
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::rvalue,
				ast::make_ts_base_type({}, context.get_base_type_info(ast::type_info::bool_)),
				ast::constant_value(is_equals),
				ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
			);
		}
		else
		{
			return ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::rvalue,
				ast::make_ts_base_type({}, context.get_base_type_info(ast::type_info::bool_)),
				ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
			);
		}
	}

	context.report_error(
		src_tokens,
		bz::format(
			undeclared_binary_message("{}"),
			is_equals ? "==" : "!=", lhs_type, rhs_type
		)
	);
	return ast::expression(src_tokens);
}

// sint <=> sint
// uint <=> uint
// float <=> float
// char <=> char
// ptr <=> ptr
// (no bool and str (for now))
static ast::expression get_built_in_binary_compare(
	lex::token_pos op,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
)
{
	bz_assert(
		op->kind == lex::token::less_than
		|| op->kind == lex::token::less_than_eq
		|| op->kind == lex::token::greater_than
		|| op->kind == lex::token::greater_than_eq
	);
	bz_assert(!lhs.is<ast::tuple_expression>());
	bz_assert(!rhs.is<ast::tuple_expression>());
	bz_assert(lhs.not_null());
	bz_assert(rhs.not_null());
	auto const [lhs_type, lhs_type_kind] = lhs.get_expr_type_and_kind();
	auto const [rhs_type, rhs_type_kind] = rhs.get_expr_type_and_kind();
	auto &lhs_t = ast::remove_const(lhs_type);
	auto &rhs_t = ast::remove_const(rhs_type);
	lex::src_tokens const src_tokens = { lhs.get_tokens_begin(), op, rhs.get_tokens_end() };

	auto const do_compare = [op_kind = op->kind](auto lhs, auto rhs) {
		switch (op_kind)
		{
		case lex::token::less_than:
			return lhs < rhs;
		case lex::token::less_than_eq:
			return lhs <= rhs;
		case lex::token::greater_than:
			return lhs > rhs;
		case lex::token::greater_than_eq:
			return lhs >= rhs;
		default:
			bz_assert(false);
			return false;
		}
	};

	auto const op_str = [op_kind = op->kind]() -> bz::u8string_view {
		switch (op_kind)
		{
		case lex::token::less_than:
			return "<";
		case lex::token::less_than_eq:
			return "<=";
		case lex::token::greater_than:
			return ">";
		case lex::token::greater_than_eq:
			return ">=";
		default:
			bz_assert(false);
			return "";
		}
	}();

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto const [lhs_kind, rhs_kind] = get_base_kinds(lhs_t, rhs_t);

		if (
			is_signed_integer_kind(lhs_kind)
			&& is_signed_integer_kind(rhs_kind)
		)
		{
			auto common_kind = lhs_kind;
			if (lhs_kind > rhs_kind)
			{
				rhs = make_built_in_cast(
					nullptr,
					std::move(rhs),
					ast::make_ts_base_type({}, context.get_base_type_info(lhs_kind)),
					context
				);
			}
			else if (lhs_kind < rhs_kind)
			{
				common_kind = rhs_kind;
				lhs = make_built_in_cast(
					nullptr,
					std::move(lhs),
					ast::make_ts_base_type({}, context.get_base_type_info(rhs_kind)),
					context
				);
			}

			if (lhs.is<ast::constant_expression>() && rhs.is<ast::constant_expression>())
			{
				auto const [lhs_val, rhs_val] =
					get_constant_expression_values<ast::constant_value::sint>(lhs, rhs);
				auto const result = do_compare(lhs_val, rhs_val);
				return ast::make_constant_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(ast::type_info::bool_)),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(ast::type_info::bool_)),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
		}
		else if (
			is_unsigned_integer_kind(lhs_kind)
			&& is_unsigned_integer_kind(rhs_kind)
		)
		{
			auto common_kind = lhs_kind;
			if (lhs_kind > rhs_kind)
			{
				rhs = make_built_in_cast(
					nullptr,
					std::move(rhs),
					ast::make_ts_base_type({}, context.get_base_type_info(lhs_kind)),
					context
				);
			}
			else if (lhs_kind < rhs_kind)
			{
				common_kind = rhs_kind;
				lhs = make_built_in_cast(
					nullptr,
					std::move(lhs),
					ast::make_ts_base_type({}, context.get_base_type_info(rhs_kind)),
					context
				);
			}

			if (lhs.is<ast::constant_expression>() && rhs.is<ast::constant_expression>())
			{
				auto const [lhs_val, rhs_val] =
					get_constant_expression_values<ast::constant_value::uint>(lhs, rhs);
				auto const result = do_compare(lhs_val, rhs_val);
				return ast::make_constant_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(ast::type_info::bool_)),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(ast::type_info::bool_)),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
		}
		else if (
			is_floating_point_kind(lhs_kind)
			// && is_floating_point_kind(rhs_kind)
			&& lhs_kind == rhs_kind
		)
		{
			if (lhs.is<ast::constant_expression>() && rhs.is<ast::constant_expression>())
			{
				bool result;
				if (lhs_kind == ast::type_info::float32_)
				{
					auto const [lhs_val, rhs_val] =
						get_constant_expression_values<ast::constant_value::float32>(lhs, rhs);
					result = do_compare(lhs_val, rhs_val);
				}
				else
				{
					bz_assert(lhs_kind == ast::type_info::float64_);
					auto const [lhs_val, rhs_val] =
						get_constant_expression_values<ast::constant_value::float64>(lhs, rhs);
					result = do_compare(lhs_val, rhs_val);
				}
				return ast::make_constant_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(ast::type_info::bool_)),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(ast::type_info::bool_)),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
		}
		else if (
			lhs_kind == ast::type_info::char_
			&& rhs_kind == ast::type_info::char_
		)
		{
			if (lhs.is<ast::constant_expression>() && rhs.is<ast::constant_expression>())
			{
				auto const [lhs_val, rhs_val] =
					get_constant_expression_values<ast::constant_value::u8char>(lhs, rhs);
				auto const result = do_compare(lhs_val, rhs_val);
				return ast::make_constant_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(ast::type_info::bool_)),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::make_ts_base_type({}, context.get_base_type_info(ast::type_info::bool_)),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
		}
//		else if (
//			lhs_kind == ast::type_info::str_
//			&& rhs_kind == ast::type_info::str_
//		)
//		{
//		}
	}
	else if (
		lhs_t.is<ast::ts_pointer>()
		&& rhs_t.is<ast::ts_pointer>()
		// TODO: use some kind of are_matchable_types here
		&& lhs_t.get<ast::ts_pointer_ptr>()->base == rhs_t.get<ast::ts_pointer_ptr>()->base
	)
	{
		if (lhs.is<ast::constant_expression>() && rhs.is<ast::constant_expression>())
		{
			bz_assert(lhs.get<ast::constant_expression>().value.kind() == ast::constant_value::null);
			bz_assert(rhs.get<ast::constant_expression>().value.kind() == ast::constant_value::null);
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::rvalue,
				ast::make_ts_base_type({}, context.get_base_type_info(ast::type_info::bool_)),
				ast::constant_value(do_compare(0, 0)),
				ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
			);
		}
		else
		{
			return ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::rvalue,
				ast::make_ts_base_type({}, context.get_base_type_info(ast::type_info::bool_)),
				ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
			);
		}
	}

	context.report_error(
		src_tokens,
		bz::format(undeclared_binary_message("{}"), op_str, lhs_type, rhs_type)
	);
	return ast::expression(src_tokens);
}

// uintN &^| uintN -> uintN
// bool &^| bool -> bool      this has no short-circuiting
static ast::expression get_built_in_binary_bit_and_xor_or(
	lex::token_pos op,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
)
{
	bz_assert(
		op->kind == lex::token::bit_and
		|| op->kind == lex::token::bit_xor
		|| op->kind == lex::token::bit_or
	);
	bz_assert(!lhs.is<ast::tuple_expression>());
	bz_assert(!rhs.is<ast::tuple_expression>());
	bz_assert(lhs.not_null());
	bz_assert(rhs.not_null());
	auto const [lhs_type, lhs_type_kind] = lhs.get_expr_type_and_kind();
	auto const [rhs_type, rhs_type_kind] = rhs.get_expr_type_and_kind();
	auto &lhs_t = ast::remove_const(lhs_type);
	auto &rhs_t = ast::remove_const(rhs_type);
	lex::src_tokens const src_tokens = { lhs.get_tokens_begin(), op, rhs.get_tokens_end() };

	auto const op_str = [op_kind = op->kind]() -> bz::u8char {
		switch (op_kind)
		{
		case lex::token::bit_and:
			return '&';
		case lex::token::bit_xor:
			return '^';
		case lex::token::bit_or:
			return '|';
		default:
			bz_assert(false);
			return '\0';
		}
	}();

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto const [lhs_kind, rhs_kind] = get_base_kinds(lhs_t, rhs_t);
		if (
			is_unsigned_integer_kind(lhs_kind)
			&& lhs_kind == rhs_kind
		)
		{
			auto result_type = lhs_t;
			if (lhs.is<ast::constant_expression>() && rhs.is<ast::constant_expression>())
			{
				auto const [lhs_val, rhs_val] =
					get_constant_expression_values<ast::constant_value::uint>(lhs, rhs);
				uint64_t result = 0;
				switch (op->kind)
				{
				case lex::token::bit_and:
					result = lhs_val & rhs_val;
					break;
				case lex::token::bit_xor:
					result = lhs_val ^ rhs_val;
					break;
				case lex::token::bit_or:
					result = lhs_val | rhs_val;
					break;
				default:
					bz_assert(false);
					break;
				}
				return ast::make_constant_expression(
					src_tokens,
					ast::expression_type_kind::rvalue, std::move(result_type),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue, std::move(result_type),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
		}
		else if (
			lhs_kind == ast::type_info::bool_
			&& rhs_kind == ast::type_info::bool_
		)
		{
			auto result_type = lhs_t;
			if (lhs.is<ast::constant_expression>() && rhs.is<ast::constant_expression>())
			{
				auto const [lhs_val, rhs_val] =
					get_constant_expression_values<ast::constant_value::boolean>(lhs, rhs);
				bool result = false;
				switch (op->kind)
				{
				case lex::token::bit_and:
					result = lhs_val && rhs_val;
					break;
				case lex::token::bit_xor:
					result = lhs_val != rhs_val;
					break;
				case lex::token::bit_or:
					result = lhs_val || rhs_val;
					break;
				default:
					bz_assert(false);
					break;
				}
				return ast::make_constant_expression(
					src_tokens,
					ast::expression_type_kind::rvalue, std::move(result_type),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue, std::move(result_type),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
		}
	}

	context.report_error(
		src_tokens,
		bz::format(undeclared_binary_message("{}"), op_str, lhs_type, rhs_type)
	);
	return ast::expression(src_tokens);
}

// uintN &^|= uintN
// bool &^|= bool
static ast::expression get_built_in_binary_bit_and_xor_or_eq(
	lex::token_pos op,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
)
{
	bz_assert(
		op->kind == lex::token::bit_and_eq
		|| op->kind == lex::token::bit_xor_eq
		|| op->kind == lex::token::bit_or_eq
	);
	bz_assert(!lhs.is<ast::tuple_expression>());
	bz_assert(!rhs.is<ast::tuple_expression>());
	bz_assert(lhs.not_null());
	bz_assert(rhs.not_null());
	auto const [lhs_type, lhs_type_kind] = lhs.get_expr_type_and_kind();
	auto const [rhs_type, rhs_type_kind] = rhs.get_expr_type_and_kind();
	auto &lhs_t = lhs_type;
	auto &rhs_t = ast::remove_const(rhs_type);
	lex::src_tokens const src_tokens = { lhs.get_tokens_begin(), op, rhs.get_tokens_end() };

	auto const op_str = [op_kind = op->kind]() -> bz::u8string_view {
		switch (op_kind)
		{
		case lex::token::bit_and_eq:
			return "&=";
		case lex::token::bit_xor_eq:
			return "^=";
		case lex::token::bit_or_eq:
			return "|=";
		default:
			bz_assert(false);
			return "";
		}
	}();

	if (
		lhs_type_kind != ast::expression_type_kind::lvalue
		&& lhs_type_kind != ast::expression_type_kind::lvalue_reference
	)
	{
		context.report_error(src_tokens, "cannot assign to an rvalue");
		return ast::expression(src_tokens);
	}
	else if (lhs_t.is<ast::ts_constant>())
	{
		context.report_error(src_tokens, "cannot assign to a constant");
		return ast::expression(src_tokens);
	}

	auto result_type = lhs_t;
	auto const result_type_kind = lhs_type_kind;

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto const [lhs_kind, rhs_kind] = get_base_kinds(lhs_t, rhs_t);
		if (
			(is_unsigned_integer_kind(lhs_kind) && lhs_kind == rhs_kind)
			|| (lhs_kind == ast::type_info::bool_ && rhs_kind == ast::type_info::bool_)
		)
		{
			return ast::make_dynamic_expression(
				src_tokens,
				result_type_kind, std::move(result_type),
				ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
			);
		}
	}

	context.report_error(
		src_tokens,
		bz::format(undeclared_binary_message("{}"), op_str, lhs_type, rhs_type)
	);
	return ast::expression(src_tokens);
}

// uintN <<>> uintM -> uintN
static ast::expression get_built_in_binary_bit_shift(
	lex::token_pos op,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
)
{
	bz_assert(op->kind == lex::token::bit_left_shift || op->kind == lex::token::bit_right_shift);
	bz_assert(!lhs.is<ast::tuple_expression>());
	bz_assert(!rhs.is<ast::tuple_expression>());
	bz_assert(lhs.not_null());
	bz_assert(rhs.not_null());
	auto const [lhs_type, lhs_type_kind] = lhs.get_expr_type_and_kind();
	auto const [rhs_type, rhs_type_kind] = rhs.get_expr_type_and_kind();
	auto &lhs_t = ast::remove_const(lhs_type);
	auto &rhs_t = ast::remove_const(rhs_type);
	lex::src_tokens const src_tokens = { lhs.get_tokens_begin(), op, rhs.get_tokens_end() };

	auto const is_left_shift = op->kind == lex::token::bit_left_shift;

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto const [lhs_kind, rhs_kind] = get_base_kinds(lhs_t, rhs_t);
		if (
			is_unsigned_integer_kind(lhs_kind)
			&& is_unsigned_integer_kind(rhs_kind)
		)
		{
			auto result_type = lhs_t;
			if (lhs.is<ast::constant_expression>() && rhs.is<ast::constant_expression>())
			{
				auto const [lhs_val, rhs_val] =
					get_constant_expression_values<ast::constant_value::uint>(lhs, rhs);
				auto const result = is_left_shift
					? safe_left_shift(
						lhs_val, rhs_val, lhs_kind,
						src_tokens, context
					)
					: safe_right_shift(
						lhs_val, rhs_val, lhs_kind,
						src_tokens, context
					);
				rhs = make_built_in_cast(nullptr, std::move(rhs), lhs_t, context);
				return ast::make_constant_expression(
					src_tokens,
					ast::expression_type_kind::rvalue, std::move(result_type),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else if (rhs.is<ast::constant_expression>())
			{
				auto &const_rhs = rhs.get<ast::constant_expression>();
				bz_assert(const_rhs.value.kind() == ast::constant_value::uint);
				auto const rhs_val = const_rhs.value.get<ast::constant_value::uint>();
				auto const [lhs_bit_width, lhs_type_str] = [lhs_kind = lhs_kind]()
					-> std::pair<uint32_t, bz::u8string_view>
				{
					switch (lhs_kind)
					{
					case ast::type_info::uint8_:
						return { 8, "uint8" };
					case ast::type_info::uint16_:
						return { 16, "uint16" };
					case ast::type_info::uint32_:
						return { 32, "uint32" };
					case ast::type_info::uint64_:
						return { 64, "uint64" };
					default:
						bz_assert(false);
						return { 0, "" };
					}
				}();
				if (rhs_val >= lhs_bit_width)
				{
					context.report_warning(
						src_tokens,
						bz::format(
							"{} shift value of {} is too large for type '{}'; will result in undefined behaviour",
							op->kind == lex::token::bit_left_shift_eq ? "left" : "right",
							rhs_val, lhs_type_str
						)
					);
				}
				rhs = make_built_in_cast(nullptr, std::move(rhs), lhs_t, context);
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue, std::move(result_type),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				rhs = make_built_in_cast(nullptr, std::move(rhs), lhs_t, context);
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue, std::move(result_type),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
		}
	}

	context.report_error(
		src_tokens,
		bz::format(
			undeclared_binary_message("{}"),
			is_left_shift ? "<<" : ">>", lhs_type, rhs_type
		)
	);
	return ast::expression(src_tokens);
}

// uint <<>>= uint
static ast::expression get_built_in_binary_bit_shift_eq(
	lex::token_pos op,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
)
{
	bz_assert(op->kind == lex::token::bit_left_shift_eq || op->kind == lex::token::bit_right_shift_eq);
	bz_assert(!lhs.is<ast::tuple_expression>());
	bz_assert(!rhs.is<ast::tuple_expression>());
	bz_assert(lhs.not_null());
	bz_assert(rhs.not_null());
	auto const [lhs_type, lhs_type_kind] = lhs.get_expr_type_and_kind();
	auto const [rhs_type, rhs_type_kind] = rhs.get_expr_type_and_kind();
	auto &lhs_t = lhs_type;
	auto &rhs_t = ast::remove_const(rhs_type);
	lex::src_tokens const src_tokens = { lhs.get_tokens_begin(), op, rhs.get_tokens_end() };

	if (
		lhs_type_kind != ast::expression_type_kind::lvalue
		&& lhs_type_kind != ast::expression_type_kind::lvalue_reference
	)
	{
		context.report_error(src_tokens, "cannot assign to an rvalue");
		return ast::expression(src_tokens);
	}
	else if (lhs_t.is<ast::ts_constant>())
	{
		context.report_error(src_tokens, "cannot assign to a constant");
		return ast::expression(src_tokens);
	}

	auto result_type = lhs_t;
	auto const result_type_kind = lhs_type_kind;

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto const [lhs_kind, rhs_kind] = get_base_kinds(lhs_t, rhs_t);
		if (
			is_unsigned_integer_kind(lhs_kind)
			&& is_unsigned_integer_kind(rhs_kind)
		)
		{
			if (rhs.is<ast::constant_expression>())
			{
				auto &const_rhs = rhs.get<ast::constant_expression>();
				bz_assert(const_rhs.value.kind() == ast::constant_value::uint);
				auto const rhs_val = const_rhs.value.get<ast::constant_value::uint>();
				auto const [lhs_bit_width, lhs_type_str] = [lhs_kind = lhs_kind]()
					-> std::pair<uint32_t, bz::u8string_view>
				{
					switch (lhs_kind)
					{
					case ast::type_info::uint8_:
						return { 8, "uint8" };
					case ast::type_info::uint16_:
						return { 16, "uint16" };
					case ast::type_info::uint32_:
						return { 32, "uint32" };
					case ast::type_info::uint64_:
						return { 64, "uint64" };
					default:
						bz_assert(false);
						return { 0, "" };
					}
				}();
				if (rhs_val >= lhs_bit_width)
				{
					context.report_warning(
						src_tokens,
						bz::format(
							"{} shift value of {} is too large for type '{}'; will result in undefined behaviour",
							op->kind == lex::token::bit_left_shift_eq ? "left" : "right",
							rhs_val, lhs_type_str
						)
					);
				}
			}
			rhs = make_built_in_cast(nullptr, std::move(rhs), lhs_t, context);
			return ast::make_dynamic_expression(
				src_tokens,
				result_type_kind, std::move(result_type),
				ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
			);
		}
	}

	context.report_error(
		src_tokens,
		bz::format(
			undeclared_binary_message("{}"),
			op->kind == lex::token::bit_left_shift_eq ? "<<=" : ">>=", lhs_type, rhs_type
		)
	);
	return ast::expression(src_tokens);
}

// bool &&^^|| bool -> bool
static ast::expression get_built_in_binary_bool_and_xor_or(
	lex::token_pos op,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
)
{
	bz_assert(
		op->kind == lex::token::bool_and
		|| op->kind == lex::token::bool_xor
		|| op->kind == lex::token::bool_or
	);
	bz_assert(!lhs.is<ast::tuple_expression>());
	bz_assert(!rhs.is<ast::tuple_expression>());
	bz_assert(lhs.not_null());
	bz_assert(rhs.not_null());
	auto const [lhs_type, lhs_type_kind] = lhs.get_expr_type_and_kind();
	auto const [rhs_type, rhs_type_kind] = rhs.get_expr_type_and_kind();
	auto &lhs_t = ast::remove_const(lhs_type);
	auto &rhs_t = ast::remove_const(rhs_type);
	lex::src_tokens const src_tokens = { lhs.get_tokens_begin(), op, rhs.get_tokens_end() };

	auto const op_str = [op_kind = op->kind]() -> bz::u8string_view {
		switch (op_kind)
		{
		case lex::token::bool_and:
			return "&&";
		case lex::token::bool_xor:
			return "^^";
		case lex::token::bool_or:
			return "||";
		default:
			bz_assert(false);
			return "";
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
			if (lhs.is<ast::constant_expression>() && rhs.is<ast::constant_expression>())
			{
				auto const [lhs_val, rhs_val] =
					get_constant_expression_values<ast::constant_value::boolean>(lhs, rhs);
				bool result = false;
				switch (op->kind)
				{
				case lex::token::bool_and:
					result = lhs_val && rhs_val;
					break;
				case lex::token::bool_xor:
					result = lhs_val != rhs_val;
					break;
				case lex::token::bool_or:
					result = lhs_val || rhs_val;
					break;
				default:
					bz_assert(false);
					break;
				}
				return ast::make_constant_expression(
					src_tokens,
					ast::expression_type_kind::rvalue, std::move(result_type),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else if (lhs.is<ast::constant_expression>() && op->kind != lex::token::bool_xor)
			{
				auto &const_lhs = lhs.get<ast::constant_expression>();
				bz_assert(const_lhs.value.kind() == ast::constant_value::boolean);
				auto const lhs_val = const_lhs.value.get<ast::constant_value::boolean>();
				// if lhs is true and the operator is ||,
				// rhs won't be evaluated because of short-circuiting
				// and the result is true
				if (lhs_val && op->kind == lex::token::bool_or)
				{
					return ast::make_constant_expression(
						src_tokens,
						ast::expression_type_kind::rvalue, std::move(result_type),
						ast::constant_value(true),
						ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
					);
				}
				// if lhs is false and the operator is &&,
				// rhs won't be evaluated because of short-circuiting
				// and the result is false
				else if (!lhs_val && op->kind == lex::token::bool_and)
				{
					return ast::make_constant_expression(
						src_tokens,
						ast::expression_type_kind::rvalue, std::move(result_type),
						ast::constant_value(false),
						ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
					);
				}
				else
				{
					return ast::make_dynamic_expression(
						src_tokens,
						ast::expression_type_kind::rvalue, std::move(result_type),
						ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
					);
				}
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue, std::move(result_type),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
		}
	}

	context.report_error(
		src_tokens,
		bz::format(undeclared_binary_message("{}"), op_str, lhs_type, rhs_type)
	);
	return ast::expression(src_tokens);
}



ast::expression make_built_in_operation(
	lex::token_pos op,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
)
{
	switch (op->kind)
	{
	case lex::token::assign:             // '='
		return get_built_in_binary_assign(op, std::move(lhs), std::move(rhs), context);
	case lex::token::plus:               // '+'
		return get_built_in_binary_plus(op, std::move(lhs), std::move(rhs), context);
	case lex::token::minus:              // '-'
		return get_built_in_binary_minus(op, std::move(lhs), std::move(rhs), context);
	case lex::token::plus_eq:            // '+='
	case lex::token::minus_eq:           // '-='
		return get_built_in_binary_plus_minus_eq(op, std::move(lhs), std::move(rhs), context);
	case lex::token::multiply:           // '*'
	case lex::token::divide:             // '/'
		return get_built_in_binary_multiply_divide(op, std::move(lhs), std::move(rhs), context);
	case lex::token::multiply_eq:        // '*='
	case lex::token::divide_eq:          // '/='
		return get_built_in_binary_multiply_divide_eq(op, std::move(lhs), std::move(rhs), context);
	case lex::token::modulo:             // '%'
		return get_built_in_binary_modulo(op, std::move(lhs), std::move(rhs), context);
	case lex::token::modulo_eq:          // '%='
		return get_built_in_binary_modulo_eq(op, std::move(lhs), std::move(rhs), context);
	case lex::token::equals:             // '=='
	case lex::token::not_equals:         // '!='
		return get_built_in_binary_equals_not_equals(op, std::move(lhs), std::move(rhs), context);
	case lex::token::less_than:          // '<'
	case lex::token::less_than_eq:       // '<='
	case lex::token::greater_than:       // '>'
	case lex::token::greater_than_eq:    // '>='
		return get_built_in_binary_compare(op, std::move(lhs), std::move(rhs), context);
	case lex::token::bit_and:            // '&'
	case lex::token::bit_xor:            // '^'
	case lex::token::bit_or:             // '|'
		return get_built_in_binary_bit_and_xor_or(op, std::move(lhs), std::move(rhs), context);
	case lex::token::bit_and_eq:         // '&='
	case lex::token::bit_xor_eq:         // '^='
	case lex::token::bit_or_eq:          // '|='
		return get_built_in_binary_bit_and_xor_or_eq(op, std::move(lhs), std::move(rhs), context);
	case lex::token::bit_left_shift:     // '<<'
	case lex::token::bit_right_shift:    // '>>'
		return get_built_in_binary_bit_shift(op, std::move(lhs), std::move(rhs), context);
	case lex::token::bit_left_shift_eq:  // '<<='
	case lex::token::bit_right_shift_eq: // '>>='
		return get_built_in_binary_bit_shift_eq(op, std::move(lhs), std::move(rhs), context);
	case lex::token::bool_and:           // '&&'
	case lex::token::bool_xor:           // '^^'
	case lex::token::bool_or:            // '||'
		return get_built_in_binary_bool_and_xor_or(op, std::move(lhs), std::move(rhs), context);

	case lex::token::square_open:        // '[]'
		bz_assert(false);
		return ast::expression();

	// these have no built-in operations
	case lex::token::dot_dot:            // '..'
	case lex::token::dot_dot_eq:         // '..='
	{
		bz::u8string_view const op_str = op->kind == lex::token::dot_dot ? ".." : "..=";
		auto &lhs_type = lhs.get_expr_type_and_kind().first;
		auto &rhs_type = rhs.get_expr_type_and_kind().first;
		lex::src_tokens const src_tokens = { lhs.get_tokens_begin(), op, rhs.get_tokens_end() };
		context.report_error(
			src_tokens,
			bz::format(undeclared_binary_message("{}"), op_str, lhs_type, rhs_type)
		);
		return ast::expression(src_tokens);
	}

	default:
		bz_assert(false);
		return ast::expression();
	}
}

ast::expression make_built_in_cast(
	lex::token_pos as_pos,
	ast::expression expr,
	ast::typespec dest_type,
	parse_context &context
)
{
	bz_assert(!expr.is<ast::tuple_expression>());
	bz_assert(expr.not_null());
	auto const [expr_type, expr_type_kind] = expr.get_expr_type_and_kind();
	auto &expr_t = ast::remove_const(expr_type);
	auto &dest_t = ast::remove_const(dest_type);

	auto const src_tokens = as_pos == nullptr
		? expr.src_tokens
		: lex::src_tokens{ expr.get_tokens_begin(), as_pos, dest_type.get_tokens_end() };

	if (
		dest_t.is<ast::ts_pointer>()
		&& expr.is<ast::constant_expression>()
		&& expr.get<ast::constant_expression>().value.kind() == ast::constant_value::null
	)
	{
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			dest_t,
			ast::constant_value(ast::internal::null_t{}),
			ast::make_expr_cast(as_pos, std::move(expr), dest_type)
		);
	}
	else if (!dest_t.is<ast::ts_base_type>())
	{
		context.report_error(
			src_tokens,
			bz::format("invalid conversion from '{}' to '{}'", expr_type, dest_type)
		);
		return ast::expression(src_tokens);
	}

	bz_assert(expr_t.is<ast::ts_base_type>());
	bz_assert(dest_t.is<ast::ts_base_type>());

	auto const [expr_kind, dest_kind] = get_base_kinds(expr_t, dest_t);

	auto const do_arithmetic_cast = [dest_kind = dest_kind](auto value) -> ast::constant_value {
		switch (dest_kind)
		{
#define CASE(ret_t, dest_t)                                   \
case ast::type_info::dest_t##_:                               \
    return static_cast<ret_t>(static_cast<dest_t##_t>(value))

		CASE(int64_t, int8);
		CASE(int64_t, int16);
		CASE(int64_t, int32);
		CASE(int64_t, int64);
		CASE(uint64_t, uint8);
		CASE(uint64_t, uint16);
		CASE(uint64_t, uint32);
		CASE(uint64_t, uint64);
		CASE(float32_t, float32);
		CASE(float64_t, float64);
		default:
			bz_assert(false);
			return ast::constant_value();
		}
#undef CASE
	};

	if (is_arithmetic_kind(expr_kind) && is_arithmetic_kind(dest_kind))
	{
		if (expr.is<ast::constant_expression>())
		{
			auto &const_expr = expr.get<ast::constant_expression>();
			ast::constant_value value{};
			switch (const_expr.value.kind())
			{
			case ast::constant_value::sint:
				value = do_arithmetic_cast(const_expr.value.get<ast::constant_value::sint>());
				break;
			case ast::constant_value::uint:
				value = do_arithmetic_cast(const_expr.value.get<ast::constant_value::uint>());
				break;
			case ast::constant_value::float32:
				value = do_arithmetic_cast(const_expr.value.get<ast::constant_value::float32>());
				break;
			case ast::constant_value::float64:
				value = do_arithmetic_cast(const_expr.value.get<ast::constant_value::float64>());
				break;
			default:
				bz_assert(false);
				break;
			}
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::rvalue, dest_t,
				value,
				ast::make_expr_cast(as_pos, std::move(expr), dest_type)
			);
		}
		else
		{
			return ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::rvalue, dest_t,
				ast::make_expr_cast(as_pos, std::move(expr), dest_type)
			);
		}
	}
	else if (expr_kind == ast::type_info::char_ && dest_kind == ast::type_info::uint32_)
	{
		if (expr.is<ast::constant_expression>())
		{
			auto &const_expr = expr.get<ast::constant_expression>();
			bz_assert(const_expr.value.kind() == ast::constant_value::u8char);
			uint64_t const value = const_expr.value.get<ast::constant_value::u8char>();
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::rvalue, dest_t,
				ast::constant_value(value),
				ast::make_expr_cast(as_pos, std::move(expr), dest_type)
			);
		}
		else
		{
			return ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::rvalue, dest_t,
				ast::make_expr_cast(as_pos, std::move(expr), dest_type)
			);
		}
	}
	else if (expr_kind == ast::type_info::uint32_ && dest_kind == ast::type_info::char_)
	{
		if (expr.is<ast::constant_expression>())
		{
			auto &const_expr = expr.get<ast::constant_expression>();
			bz_assert(const_expr.value.kind() == ast::constant_value::uint);
			auto const value = const_expr.value.get<ast::constant_value::uint>();
			if (value > bz::max_unicode_value)
			{
				context.report_error(
					src_tokens,
					bz::format(
						"the result of 0x{:x} in a constant expression is not a valid character (maximum value is 0x10ffff)",
						value
					)
				);
			}
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::rvalue, dest_t,
				ast::constant_value(static_cast<bz::u8char>(value)),
				ast::make_expr_cast(as_pos, std::move(expr), dest_type)
			);
		}
		else
		{
			return ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::rvalue, dest_t,
				ast::make_expr_cast(as_pos, std::move(expr), dest_type)
			);
		}
	}

	context.report_error(
		src_tokens,
		bz::format("invalid conversion from '{}' to '{}'", expr_type, dest_type)
	);
	return ast::expression(src_tokens);
}

} // namespace ctx
