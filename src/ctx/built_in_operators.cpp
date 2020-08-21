#include "built_in_operators.h"
#include "global_context.h"
#include "parse_context.h"
#include "safe_operations.h"
#include "token_info.h"

namespace ctx
{

static precedence get_expr_precedence(ast::expression const &expression)
{
	constexpr auto default_return_val = precedence{ 0, true };
	if (!expression.is_constant_or_dynamic())
	{
		return default_return_val;
	}

	auto &expr = expression.get_expr();
	if (
		is_unary_operator(expression.src_tokens.pivot->kind)
		|| is_binary_operator(expression.src_tokens.pivot->kind)
	)
	{
		if (
			expr.is<ast::expr_unary_op>()
			|| (
				expr.is<ast::expr_function_call>()
				&& expr.get<ast::expr_function_call_ptr>()->params.size() == 1
			)
		)
		{
			return expression.src_tokens.begin == expression.src_tokens.pivot
				? get_unary_precedence(expression.src_tokens.pivot->kind)
				: default_return_val;
		}
		else if (
			expr.is<ast::expr_binary_op>()
			|| (
				expr.is<ast::expr_function_call>()
				&& expr.get<ast::expr_function_call_ptr>()->params.size() == 2
			)
		)
		{
			auto const lhs_begin = expr.is<ast::expr_binary_op>()
				? expr.get<ast::expr_binary_op_ptr>()->lhs.src_tokens.begin
				: expr.get<ast::expr_function_call_ptr>()->params[0].src_tokens.begin;
			return lhs_begin == expression.src_tokens.begin
				? get_binary_precedence(expression.src_tokens.pivot->kind)
				: default_return_val;
		}
		else
		{
			return default_return_val;
		}
	}
	else
	{
		return default_return_val;
	}
}

[[nodiscard]] static suggestion create_explicit_cast_suggestion(
	ast::expression const &expr,
	precedence op_prec,
	bz::u8string_view type,
	parse_context &context
)
{
	constexpr auto as_prec = get_binary_precedence(lex::token::kw_as);
	auto const parens_around_cast = op_prec < as_prec;
	auto const expr_prec = get_expr_precedence(expr);
	auto const parens_around_expr = as_prec < expr_prec;
	bz::u8string_view const begin_str =
		parens_around_cast && parens_around_expr ? "((" :
		parens_around_cast || parens_around_expr ? "(" :
		"";
	auto const end_str = bz::format(
		parens_around_cast && parens_around_expr ? ") as {})" :
		parens_around_cast ? " as {})" :
		parens_around_expr ? ") as {}" :
		" as {}",
		type
	);

	if (parens_around_cast || parens_around_expr)
	{
		return context.make_suggestion_around(
			expr.src_tokens.begin, begin_str,
			expr.src_tokens.end - 1, end_str,
			bz::format("add explicit cast to '{}' here:", type)
		);
	}
	else
	{
		return context.make_suggestion_after(
			expr.src_tokens.end - 1, end_str,
			bz::format("add explicit cast to '{}' here:", type)
		);
	}
}

[[nodiscard]] static suggestion create_explicit_cast_suggestion(
	ast::expression const &expr,
	precedence op_prec,
	bz::u8string_view first_type,
	bz::u8string_view second_type,
	parse_context &context
)
{
	constexpr auto as_prec = get_binary_precedence(lex::token::kw_as);
	auto const parens_around_cast = op_prec < as_prec;
	auto const expr_prec = get_expr_precedence(expr);
	auto const parens_around_expr = as_prec < expr_prec;
	bz::u8string_view const begin_str =
		parens_around_cast && parens_around_expr ? "((" :
		parens_around_cast || parens_around_expr ? "(" :
		"";
	auto const end_str = bz::format(
		parens_around_cast && parens_around_expr ? ") as {} as {})" :
		parens_around_cast ? " as {} as {})" :
		parens_around_expr ? ") as {} as {}" :
		" as {} as {}",
		first_type, second_type
	);

	if (parens_around_cast || parens_around_expr)
	{
		return context.make_suggestion_around(
			expr.src_tokens.begin, begin_str,
			expr.src_tokens.end - 1, end_str,
			bz::format("add explicit cast to '{}' and '{}' here:", first_type, second_type)
		);
	}
	else
	{
		return context.make_suggestion_after(
			expr.src_tokens.end - 1, end_str,
			bz::format("add explicit cast to '{}' and '{}' here:", first_type, second_type)
		);
	}
}

static bz::u8string_view get_type_name_from_kind(uint32_t kind)
{
	switch (kind)
	{
	case ast::type_info::int8_: return "int8";
	case ast::type_info::int16_: return "int16";
	case ast::type_info::int32_: return "int32";
	case ast::type_info::int64_: return "int64";
	case ast::type_info::uint8_: return "uint8";
	case ast::type_info::uint16_: return "uint16";
	case ast::type_info::uint32_: return "uint32";
	case ast::type_info::uint64_: return "uint64";
	case ast::type_info::float32_: return "float32";
	case ast::type_info::float64_: return "float64";
	case ast::type_info::char_: return "char";
	case ast::type_info::str_: return "str";
	case ast::type_info::bool_: return "bool";
	default: bz_unreachable; return "";
	}
}

static uint32_t signed_to_unsigned(uint32_t kind)
{
	bz_assert(is_signed_integer_kind(kind));
	static_assert(ast::type_info::uint8_ > ast::type_info::int8_);
	return kind + (ast::type_info::uint8_ - ast::type_info::int8_);
}

static uint32_t unsigned_to_signed(uint32_t kind)
{
	bz_assert(is_unsigned_integer_kind(kind));
	static_assert(ast::type_info::uint8_ > ast::type_info::int8_);
	return kind - (ast::type_info::uint8_ - ast::type_info::int8_);
}

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

#define undeclared_unary_message(op) "no match for unary operator " op " with type '{}'"

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
	bz_assert(expr.not_null());
	auto const [type, _] = expr.get_expr_type_and_kind();
	auto const expr_t = ast::remove_const_or_consteval(type);
	lex::src_tokens const src_tokens = { op, op, expr.get_tokens_end() };

	if (!expr_t.is<ast::ts_base_type>())
	{
		context.report_error(src_tokens, bz::format(undeclared_unary_message("+"), type));
		return ast::expression(src_tokens);
	}
	auto const kind = expr_t.get<ast::ts_base_type>().info->kind;

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
	bz_assert(expr.not_null());
	auto const [type, _] = expr.get_expr_type_and_kind();
	auto const expr_t = ast::remove_const_or_consteval(type);
	lex::src_tokens const src_tokens = { op, op, expr.get_tokens_end() };

	if (!expr_t.is<ast::ts_base_type>())
	{
		context.report_error(
			src_tokens,
			bz::format(undeclared_unary_message("-"), type)
		);
		return ast::expression(src_tokens);
	}
	auto const type_info = expr_t.get<ast::ts_base_type>().info;
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
				{
					auto const result = static_cast<int64_t>(static_cast<int8_t>(-val));
					value = result;
					if (val == std::numeric_limits<int8_t>::min())
					{
						context.report_parenthesis_suppressed_warning(
							warning_kind::int_overflow,
							src_tokens,
							bz::format(
								"overflow in constant expression with type 'int8' results in {}",
								result
							)
						);
					}
					break;
				}
				case ast::type_info::int16_:
				{
					auto const result = static_cast<int64_t>(static_cast<int16_t>(-val));
					value = result;
					if (val == std::numeric_limits<int16_t>::min())
					{
						context.report_parenthesis_suppressed_warning(
							warning_kind::int_overflow,
							src_tokens,
							bz::format(
								"overflow in constant expression with type 'int16' results in {}",
								result
							)
						);
					}
					break;
				}
				case ast::type_info::int32_:
				{
					auto const result = static_cast<int64_t>(static_cast<int32_t>(-val));
					value = result;
					if (val == std::numeric_limits<int32_t>::min())
					{
						context.report_parenthesis_suppressed_warning(
							warning_kind::int_overflow,
							src_tokens,
							bz::format(
								"overflow in constant expression with type 'int32' results in {}",
								result
							)
						);
					}
					break;
				}
				case ast::type_info::int64_:
				{
					auto const result = static_cast<int64_t>(static_cast<int64_t>(-val));
					value = result;
					if (val == std::numeric_limits<int64_t>::min())
					{
						context.report_parenthesis_suppressed_warning(
							warning_kind::int_overflow,
							src_tokens,
							bz::format(
								"overflow in constant expression with type 'int64' results in {}",
								result
							)
						);
					}
					break;
				}

				default:
					bz_unreachable;
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
				bz_unreachable;
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
		if (is_unsigned_integer_kind(kind))
		{
			auto const type_name = get_type_name_from_kind(unsigned_to_signed(kind));
			bz_assert(src_tokens.pivot != nullptr);
			context.report_error(
				src_tokens,
				bz::format(undeclared_unary_message("-"), type),
				{ context.make_note(
					src_tokens.pivot->src_pos.file_id, src_tokens.pivot->src_pos.line,
					"unary operator - is not allowed for unsigned integers"
				) },
				{ create_explicit_cast_suggestion(
					expr, get_unary_precedence(lex::token::minus),
					type_name, context
				) }
			);
		}
		else
		{
			context.report_error(
				src_tokens,
				bz::format(undeclared_unary_message("-"), type)
			);
		}
		return ast::expression(src_tokens);
	}
}

// &val -> *typeof val
static ast::expression get_built_in_unary_address_of(
	lex::token_pos op,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(op->kind == lex::token::address_of);
	bz_assert(expr.not_null());
	lex::src_tokens const src_tokens = { op, op, expr.get_tokens_end() };

	auto [type, type_kind] = expr.get_expr_type_and_kind();
	auto result_type = type;
	result_type.add_layer<ast::ts_pointer>(nullptr);
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

// *ptr -> &typeof *ptr
static ast::expression get_built_in_unary_dereference(
	lex::token_pos op,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(op->kind == lex::token::dereference);
	bz_assert(expr.not_null());
	lex::src_tokens const src_tokens = { op, op, expr.get_tokens_end() };

	auto const [type, _] = expr.get_expr_type_and_kind();
	auto const expr_t = ast::remove_const_or_consteval(type);

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
		context.report_parenthesis_suppressed_warning(
			warning_kind::null_pointer_dereference,
			src_tokens,
			"operator * dereferences a null pointer"
		);
	}

	auto result_type = expr_t.get<ast::ts_pointer>();
	return ast::make_dynamic_expression(
		src_tokens,
		ast::expression_type_kind::lvalue,
		result_type,
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
	bz_assert(expr.not_null());
	auto const [type, _] = expr.get_expr_type_and_kind();
	auto const expr_t = ast::remove_const_or_consteval(type);
	lex::src_tokens const src_tokens = { op, op, expr.get_tokens_end() };

	if (!expr_t.is<ast::ts_base_type>())
	{
		context.report_error(
			src_tokens,
			bz::format(undeclared_unary_message("~"), type)
		);
		return ast::expression(src_tokens);
	}

	auto const kind = expr_t.get<ast::ts_base_type>().info->kind;
	if (!is_unsigned_integer_kind(kind) && kind != ast::type_info::bool_)
	{
		if (is_signed_integer_kind(kind))
		{
			auto const signed_type_name = get_type_name_from_kind(kind);
			auto const unsigned_type_name = get_type_name_from_kind(signed_to_unsigned(kind));
			bz_assert(src_tokens.pivot != nullptr);
			context.report_error(
				src_tokens,
				bz::format(undeclared_unary_message("~"), type),
				{ context.make_note(
					src_tokens.pivot->src_pos.file_id, src_tokens.pivot->src_pos.line,
					"bit manipulation of signed integers is not allowed"
				) },
				{ create_explicit_cast_suggestion(
					expr, get_unary_precedence(lex::token::bit_not),
					unsigned_type_name, signed_type_name,
					context
				) }
			);
		}
		else
		{
			context.report_error(
				src_tokens,
				bz::format(undeclared_unary_message("~"), type)
			);
		}
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
				bz_unreachable;
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
	bz_assert(expr.not_null());
	auto const [type, _] = expr.get_expr_type_and_kind();
	auto const expr_t = ast::remove_const_or_consteval(type);
	lex::src_tokens const src_tokens = { op, op, expr.get_tokens_end() };

	if (!expr_t.is<ast::ts_base_type>())
	{
		context.report_error(
			src_tokens,
			bz::format(undeclared_unary_message("!"), type)
		);
		return ast::expression(src_tokens);
	}

	auto const kind = expr_t.get<ast::ts_base_type>().info->kind;
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

	if (type.is<ast::ts_const>() || type.is<ast::ts_consteval>())
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
		auto kind = type.get<ast::ts_base_type>().info->kind;
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

// &(typename) -> (&typename)
static ast::expression get_type_op_unary_reference(
	lex::token_pos op,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(op->kind == lex::token::ampersand);
	bz_assert(expr.not_null());
	bz_assert(expr.is_typename());
	auto const src_tokens = expr.get_tokens_begin() == nullptr
		? lex::src_tokens{ op, op, op + 1 }
		: lex::src_tokens{ op, op, expr.get_tokens_end() };

	auto result_type = expr.get_typename();
	if (result_type.is<ast::ts_consteval>())
	{
		context.report_error(src_tokens, "reference to consteval type is not allowed");
		return ast::expression(src_tokens);
	}
	else if (result_type.is<ast::ts_lvalue_reference>())
	{
		result_type.nodes.front().get<ast::ts_lvalue_reference>().reference_pos = op;
	}
	else
	{
		result_type.add_layer<ast::ts_lvalue_reference>(op);
	}

	return ast::make_constant_expression(
		src_tokens,
		ast::expression_type_kind::type_name,
		ast::make_typename_typespec(nullptr),
		std::move(result_type),
		ast::make_expr_unary_op(op, std::move(expr))
	);
}

// *(typename) -> (*typename)
static ast::expression get_type_op_unary_pointer(
	lex::token_pos op,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(op->kind == lex::token::star);
	bz_assert(expr.not_null());
	bz_assert(expr.is_typename());
	auto const src_tokens = expr.get_tokens_begin() == nullptr
		? lex::src_tokens{ op, op, op + 1 }
		: lex::src_tokens{ op, op, expr.get_tokens_end() };

	auto result_type = expr.get_typename();
	if (result_type.is<ast::ts_lvalue_reference>())
	{
		context.report_error(src_tokens, "pointer to reference is not allowed");
		return ast::expression(src_tokens);
	}
	else if (result_type.is<ast::ts_consteval>())
	{
		context.report_error(src_tokens, "pointer to consteval is not allowed");
		return ast::expression(src_tokens);
	}

	result_type.add_layer<ast::ts_pointer>(op);

	return ast::make_constant_expression(
		src_tokens,
		ast::expression_type_kind::type_name,
		ast::make_typename_typespec(nullptr),
		std::move(result_type),
		ast::make_expr_unary_op(op, std::move(expr))
	);
}

// const (typename) -> (const typename)
static ast::expression get_type_op_unary_const(
	lex::token_pos op,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(op->kind == lex::token::kw_const);
	bz_assert(expr.not_null());
	bz_assert(expr.is_typename());
	auto const src_tokens = expr.get_tokens_begin() == nullptr
		? lex::src_tokens{ op, op, op + 1 }
		: lex::src_tokens{ op, op, expr.get_tokens_end() };

	auto result_type = expr.get_typename();
	if (result_type.is<ast::ts_lvalue_reference>())
	{
		context.report_error(src_tokens, "a reference type cannot be 'const'");
		return ast::expression(src_tokens);
	}

	if (result_type.is<ast::ts_const>())
	{
		result_type.nodes.front().get<ast::ts_const>().const_pos = op;
	}
	else if (result_type.is<ast::ts_consteval>())
	{
		result_type.nodes.front().get<ast::ts_consteval>().consteval_pos = op;
	}
	else
	{
		result_type.add_layer<ast::ts_const>(op);
	}

	return ast::make_constant_expression(
		src_tokens,
		ast::expression_type_kind::type_name,
		ast::make_typename_typespec(nullptr),
		std::move(result_type),
		ast::make_expr_unary_op(op, std::move(expr))
	);
}

// consteval (typename) -> (consteval typename)
static ast::expression get_type_op_unary_consteval(
	lex::token_pos op,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(op->kind == lex::token::kw_consteval);
	bz_assert(expr.not_null());
	bz_assert(expr.is_typename());
	auto const src_tokens = expr.get_tokens_begin() == nullptr
		? lex::src_tokens{ op, op, op + 1 }
		: lex::src_tokens{ op, op, expr.get_tokens_end() };

	auto const_expr_type = expr.get<ast::constant_expression>().value.get<ast::constant_value::type>();
	if (const_expr_type.is<ast::ts_lvalue_reference>())
	{
		context.report_error(src_tokens, "a reference type cannot be 'consteval'");
		return ast::expression(src_tokens);
	}

	if (const_expr_type.is<ast::ts_const>())
	{
		const_expr_type.nodes.front() = ast::ts_consteval{ op };
	}
	else if (const_expr_type.is<ast::ts_consteval>())
	{
		const_expr_type.nodes.front().get<ast::ts_consteval>().consteval_pos = op;
	}
	else
	{
		const_expr_type.add_layer<ast::ts_consteval>(op);
	}

	return ast::make_constant_expression(
		src_tokens,
		ast::expression_type_kind::type_name,
		ast::make_typename_typespec(nullptr),
		std::move(const_expr_type),
		ast::make_expr_unary_op(op, std::move(expr))
	);
}

static ast::expression get_built_in_unary_sizeof(
	lex::token_pos sizeof_pos,
	ast::expression expr,
	parse_context &context
)
{
	auto const src_tokens = lex::src_tokens{ sizeof_pos, sizeof_pos, expr.get_tokens_end() };
	context.report_error(src_tokens, "operator sizeof is not yet implemented");
	return ast::expression(src_tokens);
}

// typeof (val) -> (typeof val)
static ast::expression get_built_in_unary_typeof(
	lex::token_pos op,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(op->kind == lex::token::kw_typeof);
	bz_assert(expr.not_null());
	lex::src_tokens const src_tokens = { op, op, expr.get_tokens_end() };
	auto const [type, kind] = expr.get_expr_type_and_kind();
	if (expr.is_overloaded_function())
	{
		context.report_error(src_tokens, "type of an overloaded function is ambiguous");
		return ast::expression(src_tokens);
	}
	else if (expr.is_typename())
	{
		context.report_error(src_tokens, "cannot take 'typeof' of a type");
		return ast::expression(src_tokens);
	}

	auto res_type = type;
	bz_assert(type.has_value());
	if (kind == ast::expression_type_kind::lvalue_reference)
	{
		res_type.add_layer<ast::ts_lvalue_reference>(nullptr);
	}
	return ast::make_constant_expression(
		src_tokens,
		ast::expression_type_kind::type_name,
		ast::make_typename_typespec(nullptr),
		std::move(res_type),
		ast::make_expr_unary_op(op, std::move(expr))
	);
}

#undef undeclared_unary_message


#define undeclared_binary_message(op) "no match for binary operator " op " with types '{}' and '{}'"

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
	bz_assert(lhs.not_null());
	bz_assert(rhs.not_null());
	auto const [lhs_type, lhs_type_kind] = lhs.get_expr_type_and_kind();
	auto const [rhs_type, rhs_type_kind] = rhs.get_expr_type_and_kind();
	auto &lhs_t = lhs_type;
	auto const rhs_t = ast::remove_const_or_consteval(rhs_type);
	lex::src_tokens const src_tokens = { lhs.get_tokens_begin(), op, rhs.get_tokens_end() };

	if (
		lhs_type_kind != ast::expression_type_kind::lvalue
		&& lhs_type_kind != ast::expression_type_kind::lvalue_reference
	)
	{
		context.report_error(src_tokens, "cannot assign to an rvalue");
		return ast::expression(src_tokens);
	}
	else if (lhs_t.is<ast::ts_const>() || lhs_t.is<ast::ts_consteval>())
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
			rhs = make_built_in_cast(src_tokens, nullptr, std::move(rhs), lhs_t, context);
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
			rhs = make_built_in_cast(src_tokens, nullptr, std::move(rhs), lhs_t, context);
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
		if (lhs_t.get<ast::ts_pointer>() == rhs_t.get<ast::ts_pointer>())
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
		rhs = make_built_in_cast(src_tokens, nullptr, std::move(rhs), lhs_t, context);
		auto result_type = lhs_t;
		auto const result_type_kind = lhs_type_kind;
		return ast::make_dynamic_expression(
			src_tokens,
			result_type_kind,
			std::move(result_type),
			ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
		);
	}

	bz::vector<note> notes = {};

	if (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_base_type>())
	{
		auto const [lhs_kind, rhs_kind] = get_base_kinds(lhs_t, rhs_t);

		if (
			(is_signed_integer_kind(lhs_kind) && is_unsigned_integer_kind(rhs_kind))
			|| (is_unsigned_integer_kind(lhs_kind) && is_signed_integer_kind(rhs_kind))
		)
		{
			notes.push_back(context.make_note(
				src_tokens.pivot->src_pos.file_id, src_tokens.pivot->src_pos.line,
				"implicit conversion between signed and unsigned integer types is not allowed"
			));
		}
		else if (
			(is_floating_point_kind(lhs_kind) && is_integer_kind(rhs_kind))
			|| (is_integer_kind(lhs_kind) && is_floating_point_kind(rhs_kind))
		)
		{
			notes.push_back(context.make_note(
				src_tokens.pivot->src_pos.file_id, src_tokens.pivot->src_pos.line,
				"implicit conversion between floating-point and integer types is not allowed"
			));
		}
	}

	context.report_error(
		src_tokens,
		bz::format(undeclared_binary_message("="), lhs_type, rhs_type),
		std::move(notes)
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
	bz_assert(lhs.not_null());
	bz_assert(rhs.not_null());
	auto const [lhs_type, lhs_type_kind] = lhs.get_expr_type_and_kind();
	auto const [rhs_type, rhs_type_kind] = rhs.get_expr_type_and_kind();
	auto const lhs_t = ast::remove_const_or_consteval(lhs_type);
	auto const rhs_t = ast::remove_const_or_consteval(rhs_type);
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
					src_tokens,
					nullptr,
					std::move(rhs),
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(lhs_kind) } }),
					context
				);
				bz_assert(rhs.not_null());
			}
			else if (lhs_kind < rhs_kind)
			{
				common_kind = rhs_kind;
				lhs = make_built_in_cast(
					src_tokens,
					nullptr,
					std::move(lhs),
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(rhs_kind) } }),
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
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(common_kind) } }),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(common_kind) } }),
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
					src_tokens,
					nullptr,
					std::move(rhs),
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(lhs_kind) } }),
					context
				);
				bz_assert(rhs.not_null());
			}
			else if (lhs_kind < rhs_kind)
			{
				common_kind = rhs_kind;
				lhs = make_built_in_cast(
					src_tokens,
					nullptr,
					std::move(lhs),
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(rhs_kind) } }),
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
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(common_kind) } }),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(common_kind) } }),
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
				if (lhs_kind == ast::type_info::float32_)
				{
					auto const [lhs_val, rhs_val] =
						get_constant_expression_values<ast::constant_value::float32>(lhs, rhs);
					value = safe_add(lhs_val, rhs_val, src_tokens, context);
				}
				else
				{
					auto const [lhs_val, rhs_val] =
						get_constant_expression_values<ast::constant_value::float64>(lhs, rhs);
					value = safe_add(lhs_val, rhs_val, src_tokens, context);
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
					bz_unreachable;
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
					bz_unreachable;
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
		&& is_integer_kind(rhs_t.get<ast::ts_base_type>().info->kind)
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
		&& is_integer_kind(lhs_t.get<ast::ts_base_type>().info->kind)
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
	bz_assert(lhs.not_null());
	bz_assert(rhs.not_null());
	auto const [lhs_type, lhs_type_kind] = lhs.get_expr_type_and_kind();
	auto const [rhs_type, rhs_type_kind] = rhs.get_expr_type_and_kind();
	auto const lhs_t = ast::remove_const_or_consteval(lhs_type);
	auto const rhs_t = ast::remove_const_or_consteval(rhs_type);
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
					src_tokens,
					nullptr,
					std::move(rhs),
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(lhs_kind) } }),
					context
				);
				bz_assert(rhs.not_null());
			}
			else if (lhs_kind < rhs_kind)
			{
				common_kind = rhs_kind;
				lhs = make_built_in_cast(
					src_tokens,
					nullptr,
					std::move(lhs),
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(rhs_kind) } }),
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
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(common_kind) } }),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(common_kind) } }),
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
					src_tokens,
					nullptr,
					std::move(rhs),
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(lhs_kind) } }),
					context
				);
				bz_assert(rhs.not_null());
			}
			else if (lhs_kind < rhs_kind)
			{
				common_kind = rhs_kind;
				lhs = make_built_in_cast(
					src_tokens,
					nullptr,
					std::move(lhs),
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(rhs_kind) } }),
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
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(common_kind) } }),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(common_kind) } }),
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
				if (lhs_kind == ast::type_info::float32_)
				{
					auto const [lhs_val, rhs_val] =
						get_constant_expression_values<ast::constant_value::float32>(lhs, rhs);
					value = safe_subtract(lhs_val, rhs_val, src_tokens, context);
				}
				else
				{
					auto const [lhs_val, rhs_val] =
						get_constant_expression_values<ast::constant_value::float64>(lhs, rhs);
					value = safe_subtract(lhs_val, rhs_val, src_tokens, context);
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
					bz_unreachable;
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
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(ast::type_info::int32_) } }),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(ast::type_info::int32_) } }),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
		}
	}
	else if (
		lhs_t.is<ast::ts_pointer>()
		&& rhs_t.is<ast::ts_base_type>()
		&& is_integer_kind(rhs_t.get<ast::ts_base_type>().info->kind)
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
		if (lhs_t.get<ast::ts_pointer>() == rhs_t.get<ast::ts_pointer>())
		{
			return ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::rvalue,
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(ast::type_info::int64_) } }),
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
	bz_assert(lhs.not_null());
	bz_assert(rhs.not_null());
	auto const [lhs_type, lhs_type_kind] = lhs.get_expr_type_and_kind();
	auto const [rhs_type, rhs_type_kind] = rhs.get_expr_type_and_kind();
	auto &lhs_t = lhs_type;
	auto const rhs_t = ast::remove_const_or_consteval(rhs_type);
	lex::src_tokens const src_tokens = { lhs.get_tokens_begin(), op, rhs.get_tokens_end() };

	if (
		lhs_type_kind != ast::expression_type_kind::lvalue
		&& lhs_type_kind != ast::expression_type_kind::lvalue_reference
	)
	{
		context.report_error(src_tokens, "cannot assign to an rvalue");
		return ast::expression(src_tokens);
	}
	else if (lhs_t.is<ast::ts_const>() || lhs_t.is<ast::ts_consteval>())
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
				rhs = make_built_in_cast(src_tokens, nullptr, std::move(rhs), lhs_t, context);
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
				rhs = make_built_in_cast(src_tokens, nullptr, std::move(rhs), lhs_t, context);
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
				rhs = make_built_in_cast(src_tokens, nullptr, std::move(rhs), lhs_t, context);
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
		&& is_integer_kind(rhs_t.get<ast::ts_base_type>().info->kind)
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
	bz_assert(lhs.not_null());
	bz_assert(rhs.not_null());
	auto const [lhs_type, lhs_type_kind] = lhs.get_expr_type_and_kind();
	auto const [rhs_type, rhs_type_kind] = rhs.get_expr_type_and_kind();
	auto const lhs_t = ast::remove_const_or_consteval(lhs_type);
	auto const rhs_t = ast::remove_const_or_consteval(rhs_type);
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
					src_tokens,
					nullptr,
					std::move(rhs),
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(lhs_kind) } }),
					context
				);
			}
			else if (lhs_kind < rhs_kind)
			{
				common_kind = rhs_kind;
				lhs = make_built_in_cast(
					src_tokens,
					nullptr,
					std::move(lhs),
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(rhs_kind) } }),
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
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(common_kind) } }),
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
						context.report_parenthesis_suppressed_warning(
							warning_kind::int_divide_by_zero,
							src_tokens, "dividing by zero in integer arithmetic"
						);
					}
				}
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(common_kind) } }),
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
					src_tokens,
					nullptr,
					std::move(rhs),
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(lhs_kind) } }),
					context
				);
			}
			else if (lhs_kind < rhs_kind)
			{
				common_kind = rhs_kind;
				lhs = make_built_in_cast(
					src_tokens,
					nullptr,
					std::move(lhs),
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(rhs_kind) } }),
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
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(common_kind) } }),
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
						context.report_parenthesis_suppressed_warning(
							warning_kind::int_divide_by_zero,
							src_tokens, "dividing by zero in integer arithmetic"
						);
					}
				}
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(common_kind) } }),
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
				if (lhs_kind == ast::type_info::float32_)
				{
					auto const [lhs_val, rhs_val] =
						get_constant_expression_values<ast::constant_value::float32>(lhs, rhs);
					value = is_multiply
						? safe_multiply(lhs_val, rhs_val, src_tokens, context)
						: safe_divide(lhs_val, rhs_val, src_tokens, context);
				}
				else
				{
					auto const [lhs_val, rhs_val] =
						get_constant_expression_values<ast::constant_value::float64>(lhs, rhs);
					value = is_multiply
						? safe_multiply(lhs_val, rhs_val, src_tokens, context)
						: safe_divide(lhs_val, rhs_val, src_tokens, context);
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
				if (!is_multiply && rhs.is<ast::constant_expression>())
				{
					auto &const_rhs = rhs.get<ast::constant_expression>();
					auto const is_zero = rhs_kind == ast::type_info::float32_
						? const_rhs.value.get<ast::constant_value::float32>() == 0.0f
						: const_rhs.value.get<ast::constant_value::float64>() == 0.0;
					if (is_zero)
					{
						context.report_parenthesis_suppressed_warning(
							warning_kind::float_divide_by_zero,
							src_tokens, "dividing by zero in floating point arithmetic"
						);
					}
				}
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
	bz_assert(lhs.not_null());
	bz_assert(rhs.not_null());
	auto const [lhs_type, lhs_type_kind] = lhs.get_expr_type_and_kind();
	auto const [rhs_type, rhs_type_kind] = rhs.get_expr_type_and_kind();
	auto &lhs_t = lhs_type;
	auto const rhs_t = ast::remove_const_or_consteval(rhs_type);
	lex::src_tokens const src_tokens = { lhs.get_tokens_begin(), op, rhs.get_tokens_end() };

	if (
		lhs_type_kind != ast::expression_type_kind::lvalue
		&& lhs_type_kind != ast::expression_type_kind::lvalue_reference
	)
	{
		context.report_error(src_tokens, "cannot assign to an rvalue");
		return ast::expression(src_tokens);
	}
	else if (lhs_t.is<ast::ts_const>() || lhs_t.is<ast::ts_consteval>())
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
				rhs = make_built_in_cast(src_tokens, nullptr, std::move(rhs), lhs_t, context);
			}
			// warn if there's a division by zero
			if (
				op->kind == lex::token::divide_eq
				&& rhs.is<ast::constant_expression>()
				&& rhs.get<ast::constant_expression>().value.get<ast::constant_value::sint>() == 0
			)
			{
				context.report_parenthesis_suppressed_warning(
					warning_kind::int_divide_by_zero,
					src_tokens, "dividing by zero in integer arithmetic"
				);
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
				rhs = make_built_in_cast(src_tokens, nullptr, std::move(rhs), lhs_t, context);
			}
			// warn if there's a division by zero
			if (
				op->kind == lex::token::divide_eq
				&& rhs.is<ast::constant_expression>()
				&& rhs.get<ast::constant_expression>().value.get<ast::constant_value::uint>() == 0
			)
			{
				context.report_parenthesis_suppressed_warning(
					warning_kind::int_divide_by_zero,
					src_tokens, "dividing by zero in integer arithmetic"
				);
			}
			return ast::make_dynamic_expression(
				src_tokens,
				result_type_kind, std::move(result_type),
				ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
			);
		}
		else if (
			is_floating_point_kind(lhs_kind)
//			&& is_floating_point_kind(rhs_kind)
			&& lhs_kind == rhs_kind
		)
		{
			// warn if there's a division by zero
			if (op->kind == lex::token::divide_eq && rhs.is<ast::constant_expression>())
			{
				auto &const_rhs = rhs.get<ast::constant_expression>();
				auto const is_zero = rhs_kind == ast::type_info::float32_
					? const_rhs.value.get<ast::constant_value::float32>() == 0.0f
					: const_rhs.value.get<ast::constant_value::float64>() == 0.0;
				if (is_zero)
				{
					context.report_parenthesis_suppressed_warning(
						warning_kind::float_divide_by_zero,
						src_tokens, "dividing by zero in floating point arithmetic"
					);
				}
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
	bz_assert(lhs.not_null());
	bz_assert(rhs.not_null());
	auto const [lhs_type, lhs_type_kind] = lhs.get_expr_type_and_kind();
	auto const [rhs_type, rhs_type_kind] = rhs.get_expr_type_and_kind();
	auto const lhs_t = ast::remove_const_or_consteval(lhs_type);
	auto const rhs_t = ast::remove_const_or_consteval(rhs_type);
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
					src_tokens,
					nullptr,
					std::move(rhs),
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(lhs_kind) } }),
					context
				);
			}
			else if (lhs_kind < rhs_kind)
			{
				common_kind = rhs_kind;
				lhs = make_built_in_cast(
					src_tokens,
					nullptr,
					std::move(lhs),
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(rhs_kind) } }),
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
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(common_kind) } }),
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
					context.report_parenthesis_suppressed_warning(
						warning_kind::int_divide_by_zero,
						src_tokens, "modulo by zero in integer arithmetic"
					);
				}
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(common_kind) } }),
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
					src_tokens,
					nullptr,
					std::move(rhs),
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(lhs_kind) } }),
					context
				);
			}
			else if (lhs_kind < rhs_kind)
			{
				common_kind = rhs_kind;
				lhs = make_built_in_cast(
					src_tokens,
					nullptr,
					std::move(lhs),
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(rhs_kind) } }),
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
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(common_kind) } }),
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
					context.report_parenthesis_suppressed_warning(
						warning_kind::int_divide_by_zero,
						src_tokens, "modulo by zero in integer arithmetic"
					);
				}
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(common_kind) } }),
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
	bz_assert(lhs.not_null());
	bz_assert(rhs.not_null());
	auto const [lhs_type, lhs_type_kind] = lhs.get_expr_type_and_kind();
	auto const [rhs_type, rhs_type_kind] = rhs.get_expr_type_and_kind();
	auto &lhs_t = lhs_type;
	auto const rhs_t = ast::remove_const_or_consteval(rhs_type);

	lex::src_tokens const src_tokens = { lhs.get_tokens_begin(), op, rhs.get_tokens_end() };

	if (
		lhs_type_kind != ast::expression_type_kind::lvalue
		&& lhs_type_kind != ast::expression_type_kind::lvalue_reference
	)
	{
		context.report_error(src_tokens, "cannot assign to an rvalue");
		return ast::expression(src_tokens);
	}
	else if (lhs_t.is<ast::ts_const>() || lhs_t.is<ast::ts_consteval>())
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
				rhs = make_built_in_cast(src_tokens, nullptr, std::move(rhs), lhs_t, context);
			}
			// warn if there's a modulo by zero
			if (
				rhs.is<ast::constant_expression>()
				&& rhs.get<ast::constant_expression>().value.get<ast::constant_value::sint>() == 0
			)
			{
				context.report_parenthesis_suppressed_warning(
					warning_kind::int_divide_by_zero,
					src_tokens, "modulo by zero in integer arithmetic"
				);
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
				rhs = make_built_in_cast(src_tokens, nullptr, std::move(rhs), lhs_t, context);
			}
			// warn if there's a modulo by zero
			if (
				rhs.is<ast::constant_expression>()
				&& rhs.get<ast::constant_expression>().value.get<ast::constant_value::uint>() == 0
			)
			{
				context.report_parenthesis_suppressed_warning(
					warning_kind::int_divide_by_zero,
					src_tokens, "modulo by zero in integer arithmetic"
				);
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
	bz_assert(lhs.not_null());
	bz_assert(rhs.not_null());
	auto const [lhs_type, lhs_type_kind] = lhs.get_expr_type_and_kind();
	auto const [rhs_type, rhs_type_kind] = rhs.get_expr_type_and_kind();
	auto const lhs_t = ast::remove_const_or_consteval(lhs_type);
	auto const rhs_t = ast::remove_const_or_consteval(rhs_type);
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
			if (lhs_kind > rhs_kind)
			{
				rhs = make_built_in_cast(
					src_tokens,
					nullptr,
					std::move(rhs),
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(lhs_kind) } }),
					context
				);
			}
			else if (lhs_kind < rhs_kind)
			{
				lhs = make_built_in_cast(
					src_tokens,
					nullptr,
					std::move(lhs),
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(lhs_kind) } }),
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
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(ast::type_info::bool_) } }),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(ast::type_info::bool_) } }),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
		}
		else if (
			is_unsigned_integer_kind(lhs_kind)
			&& is_unsigned_integer_kind(rhs_kind)
		)
		{
			if (lhs_kind > rhs_kind)
			{
				rhs = make_built_in_cast(
					src_tokens,
					nullptr,
					std::move(rhs),
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(lhs_kind) } }),
					context
				);
				bz_assert(rhs.not_null());
			}
			else if (lhs_kind < rhs_kind)
			{
				lhs = make_built_in_cast(
					src_tokens,
					nullptr,
					std::move(lhs),
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(rhs_kind) } }),
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
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(ast::type_info::bool_) } }),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(ast::type_info::bool_) } }),
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
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(ast::type_info::bool_) } }),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(ast::type_info::bool_) } }),
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
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(ast::type_info::bool_) } }),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(ast::type_info::bool_) } }),
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
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(ast::type_info::bool_) } }),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(ast::type_info::bool_) } }),
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
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(ast::type_info::bool_) } }),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(ast::type_info::bool_) } }),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
		}
	}
	else if (
		lhs_t.is<ast::ts_pointer>()
		&& rhs_t.is<ast::ts_pointer>()
		// TODO: use some kind of are_matchable_types here
		&& lhs_t.get<ast::ts_pointer>() == rhs_t.get<ast::ts_pointer>()
	)
	{
		if (lhs.is<ast::constant_expression>() && rhs.is<ast::constant_expression>())
		{
			bz_assert(lhs.get<ast::constant_expression>().value.kind() == ast::constant_value::null);
			bz_assert(rhs.get<ast::constant_expression>().value.kind() == ast::constant_value::null);
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::rvalue,
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(ast::type_info::bool_) } }),
				ast::constant_value(is_equals),
				ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
			);
		}
		else
		{
			return ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::rvalue,
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(ast::type_info::bool_) } }),
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
		rhs = make_built_in_cast(src_tokens, nullptr, std::move(rhs), lhs_t, context);
		if (lhs.is<ast::constant_expression>())
		{
			auto &const_lhs = lhs.get<ast::constant_expression>();
			bz_assert(const_lhs.value.kind() == ast::constant_value::null);
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::rvalue,
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(ast::type_info::bool_) } }),
				ast::constant_value(is_equals),
				ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
			);
		}
		else
		{
			return ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::rvalue,
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(ast::type_info::bool_) } }),
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
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(ast::type_info::bool_) } }),
				ast::constant_value(is_equals),
				ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
			);
		}
		else
		{
			return ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::rvalue,
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(ast::type_info::bool_) } }),
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
	bz_assert(lhs.not_null());
	bz_assert(rhs.not_null());
	auto const [lhs_type, lhs_type_kind] = lhs.get_expr_type_and_kind();
	auto const [rhs_type, rhs_type_kind] = rhs.get_expr_type_and_kind();
	auto const lhs_t = ast::remove_const_or_consteval(lhs_type);
	auto const rhs_t = ast::remove_const_or_consteval(rhs_type);
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
			bz_unreachable;
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
			bz_unreachable;
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
			if (lhs_kind > rhs_kind)
			{
				rhs = make_built_in_cast(
					src_tokens,
					nullptr,
					std::move(rhs),
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(lhs_kind) } }),
					context
				);
			}
			else if (lhs_kind < rhs_kind)
			{
				lhs = make_built_in_cast(
					src_tokens,
					nullptr,
					std::move(lhs),
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(rhs_kind) } }),
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
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(ast::type_info::bool_) } }),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(ast::type_info::bool_) } }),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
		}
		else if (
			is_unsigned_integer_kind(lhs_kind)
			&& is_unsigned_integer_kind(rhs_kind)
		)
		{
			if (lhs_kind > rhs_kind)
			{
				rhs = make_built_in_cast(
					src_tokens,
					nullptr,
					std::move(rhs),
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(lhs_kind) } }),
					context
				);
			}
			else if (lhs_kind < rhs_kind)
			{
				lhs = make_built_in_cast(
					src_tokens,
					nullptr,
					std::move(lhs),
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(rhs_kind) } }),
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
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(ast::type_info::bool_) } }),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(ast::type_info::bool_) } }),
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
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(ast::type_info::bool_) } }),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(ast::type_info::bool_) } }),
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
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(ast::type_info::bool_) } }),
					ast::constant_value(result),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue,
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(ast::type_info::bool_) } }),
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
		&& lhs_t.get<ast::ts_pointer>() == rhs_t.get<ast::ts_pointer>()
	)
	{
		if (lhs.is<ast::constant_expression>() && rhs.is<ast::constant_expression>())
		{
			bz_assert(lhs.get<ast::constant_expression>().value.kind() == ast::constant_value::null);
			bz_assert(rhs.get<ast::constant_expression>().value.kind() == ast::constant_value::null);
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::rvalue,
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(ast::type_info::bool_) } }),
				ast::constant_value(do_compare(0, 0)),
				ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
			);
		}
		else
		{
			return ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::rvalue,
					ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(ast::type_info::bool_) } }),
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
	bz_assert(lhs.not_null());
	bz_assert(rhs.not_null());
	auto const [lhs_type, lhs_type_kind] = lhs.get_expr_type_and_kind();
	auto const [rhs_type, rhs_type_kind] = rhs.get_expr_type_and_kind();
	auto const lhs_t = ast::remove_const_or_consteval(lhs_type);
	auto const rhs_t = ast::remove_const_or_consteval(rhs_type);
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
			bz_unreachable;
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
					bz_unreachable;
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
					bz_unreachable;
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
	bz_assert(lhs.not_null());
	bz_assert(rhs.not_null());
	auto const [lhs_type, lhs_type_kind] = lhs.get_expr_type_and_kind();
	auto const [rhs_type, rhs_type_kind] = rhs.get_expr_type_and_kind();
	auto &lhs_t = lhs_type;
	auto const rhs_t = ast::remove_const_or_consteval(rhs_type);
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
			bz_unreachable;
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
	else if (lhs_t.is<ast::ts_const>() || lhs_t.is<ast::ts_consteval>())
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
	bz_assert(lhs.not_null());
	bz_assert(rhs.not_null());
	auto const [lhs_type, lhs_type_kind] = lhs.get_expr_type_and_kind();
	auto const [rhs_type, rhs_type_kind] = rhs.get_expr_type_and_kind();
	auto const lhs_t = ast::remove_const_or_consteval(lhs_type);
	auto const rhs_t = ast::remove_const_or_consteval(rhs_type);
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
				rhs = make_built_in_cast(src_tokens, nullptr, std::move(rhs), lhs_t, context);
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
						bz_unreachable;
						return { 0, "" };
					}
				}();
				if (rhs_val >= lhs_bit_width)
				{
					context.report_parenthesis_suppressed_warning(
						warning_kind::int_overflow,
						src_tokens,
						bz::format(
							"{} shift amount of {} is too large for type '{}'",
							op->kind == lex::token::bit_left_shift_eq ? "left" : "right",
							rhs_val, lhs_type_str
						)
					);
				}
				rhs = make_built_in_cast(src_tokens, nullptr, std::move(rhs), lhs_t, context);
				return ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue, std::move(result_type),
					ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
				);
			}
			else
			{
				rhs = make_built_in_cast(src_tokens, nullptr, std::move(rhs), lhs_t, context);
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
	bz_assert(lhs.not_null());
	bz_assert(rhs.not_null());
	auto const [lhs_type, lhs_type_kind] = lhs.get_expr_type_and_kind();
	auto const [rhs_type, rhs_type_kind] = rhs.get_expr_type_and_kind();
	auto &lhs_t = lhs_type;
	auto const rhs_t = ast::remove_const_or_consteval(rhs_type);
	lex::src_tokens const src_tokens = { lhs.get_tokens_begin(), op, rhs.get_tokens_end() };

	if (
		lhs_type_kind != ast::expression_type_kind::lvalue
		&& lhs_type_kind != ast::expression_type_kind::lvalue_reference
	)
	{
		context.report_error(src_tokens, "cannot assign to an rvalue");
		return ast::expression(src_tokens);
	}
	else if (lhs_t.is<ast::ts_const>() || lhs_t.is<ast::ts_consteval>())
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
						bz_unreachable;
						return { 0, "" };
					}
				}();
				if (rhs_val >= lhs_bit_width)
				{
					context.report_parenthesis_suppressed_warning(
						warning_kind::int_overflow,
						src_tokens,
						bz::format(
							"{} shift amount of {} is too large for type '{}'",
							op->kind == lex::token::bit_left_shift_eq ? "left" : "right",
							rhs_val, lhs_type_str
						)
					);
				}
			}
			rhs = make_built_in_cast(src_tokens, nullptr, std::move(rhs), lhs_t, context);
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
	bz_assert(lhs.not_null());
	bz_assert(rhs.not_null());
	auto const [lhs_type, lhs_type_kind] = lhs.get_expr_type_and_kind();
	auto const [rhs_type, rhs_type_kind] = rhs.get_expr_type_and_kind();
	auto const lhs_t = ast::remove_const_or_consteval(lhs_type);
	auto const rhs_t = ast::remove_const_or_consteval(rhs_type);
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
			bz_unreachable;
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
					bz_unreachable;
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

// T, U -> U
static ast::expression get_built_in_binary_comma(
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

	if (
		lhs.is<ast::constant_expression>()
		&& !lhs.get_expr_type_and_kind().first.is<ast::ts_void>()
	)
	{
		context.report_warning(
			warning_kind::unused_value,
			lhs, "value of left-hand-side of comma operator is never used"
		);
	}

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


ast::expression make_built_in_cast(
	lex::src_tokens src_tokens,
	lex::token_pos as_pos,
	ast::expression expr,
	ast::typespec dest_type,
	parse_context &context
)
{
	bz_assert(expr.not_null());
	auto const [expr_type, expr_type_kind] = expr.get_expr_type_and_kind();
	auto const expr_t = ast::remove_const_or_consteval(expr_type);
	auto const dest_t = ast::remove_const_or_consteval(dest_type);
	bz_assert(ast::is_complete(dest_t));

	if (
		dest_t.is<ast::ts_pointer>()
		&& ((
			expr.is<ast::constant_expression>()
			&& expr.get<ast::constant_expression>().value.kind() == ast::constant_value::null
		)
		|| (
			expr.is<ast::dynamic_expression>()
			&& [&]() {
				auto const type = ast::remove_const_or_consteval(expr.get<ast::dynamic_expression>().type);
				return type.is<ast::ts_base_type>()
					&& type.get<ast::ts_base_type>().info->kind == ast::type_info::null_t_;
			}()
		))
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

	auto const do_arithmetic_cast = [dest_kind = dest_kind, &context, &src_tokens](
		auto value
	) -> ast::constant_value {
		switch (dest_kind)
		{
#define CASE(ret_t, dest_t)                                                                         \
case ast::type_info::dest_t##_:                                                                     \
    if constexpr (                                                                                  \
        is_signed_integer_kind(ast::type_info_from_type_v<decltype(value)>)                         \
        && is_signed_integer_kind(ast::type_info::dest_t##_)                                        \
    )                                                                                               \
    {                                                                                               \
        auto const result = static_cast<ret_t>(static_cast<dest_t##_t>(value));                     \
        if (!is_in_range<dest_t##_t>(value))                                                        \
        {                                                                                           \
            context.report_parenthesis_suppressed_warning(                                          \
                warning_kind::int_overflow,                                                         \
                src_tokens, bz::format("truncation in cast to '" #dest_t "' results in {}", result) \
            );                                                                                      \
        }                                                                                           \
        return result;                                                                              \
    }                                                                                               \
    else if constexpr (                                                                             \
        is_unsigned_integer_kind(ast::type_info_from_type_v<decltype(value)>)                       \
        && is_unsigned_integer_kind(ast::type_info::dest_t##_)                                      \
    )                                                                                               \
    {                                                                                               \
        auto const result = static_cast<ret_t>(static_cast<dest_t##_t>(value));                     \
        if (!is_in_range<dest_t##_t>(value))                                                        \
        {                                                                                           \
            context.report_parenthesis_suppressed_warning(                                          \
                warning_kind::int_overflow,                                                         \
                src_tokens, bz::format("truncation in cast to '" #dest_t "' results in {}", result) \
            );                                                                                      \
        }                                                                                           \
        return result;                                                                              \
    }                                                                                               \
    else if constexpr (                                                                             \
        is_signed_integer_kind(ast::type_info_from_type_v<decltype(value)>)                         \
        && is_unsigned_integer_kind(ast::type_info::dest_t##_)                                      \
    )                                                                                               \
    {                                                                                               \
        auto const result = static_cast<ret_t>(static_cast<dest_t##_t>(value));                     \
        if (value < 0)                                                                              \
        {                                                                                           \
            context.report_parenthesis_suppressed_warning(                                          \
                warning_kind::int_overflow,                                                         \
                src_tokens, bz::format("truncation in cast to '" #dest_t "' results in {}", result) \
            );                                                                                      \
        }                                                                                           \
        return result;                                                                              \
    }                                                                                               \
    else if constexpr (                                                                             \
        is_unsigned_integer_kind(ast::type_info_from_type_v<decltype(value)>)                       \
        && is_signed_integer_kind(ast::type_info::dest_t##_)                                        \
    )                                                                                               \
    {                                                                                               \
        auto const result = static_cast<ret_t>(static_cast<dest_t##_t>(value));                     \
        if (value > static_cast<uint64_t>(std::numeric_limits<dest_t##_t>::max()))                  \
        {                                                                                           \
            context.report_parenthesis_suppressed_warning(                                          \
                warning_kind::int_overflow,                                                         \
                src_tokens, bz::format("truncation in cast to '" #dest_t "' results in {}", result) \
            );                                                                                      \
        }                                                                                           \
        return result;                                                                              \
    }                                                                                               \
    else                                                                                            \
    {                                                                                               \
        /* floating point numbers */                                                                \
        return static_cast<ret_t>(static_cast<dest_t##_t>(value));                                  \
    }

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
			bz_unreachable;
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
				bz_unreachable;
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
	else if (
		expr_kind == ast::type_info::char_
		&& (dest_kind == ast::type_info::uint32_ || dest_kind == ast::type_info::int32_)
	)
	{
		if (expr.is<ast::constant_expression>())
		{
			auto &const_expr = expr.get<ast::constant_expression>();
			bz_assert(const_expr.value.kind() == ast::constant_value::u8char);
			auto const value = const_expr.value.get<ast::constant_value::u8char>();
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::rvalue, dest_t,
				dest_kind == ast::type_info::uint32_
					? ast::constant_value(static_cast<uint64_t>(value))
					: ast::constant_value(static_cast<int64_t>(value)),
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
	else if (
		(expr_kind == ast::type_info::uint32_ || expr_kind == ast::type_info::int32_)
		&& dest_kind == ast::type_info::char_
	)
	{
		if (expr.is<ast::constant_expression>())
		{
			auto &const_expr = expr.get<ast::constant_expression>();
			bz_assert(
				const_expr.value.kind() == ast::constant_value::uint
				|| const_expr.value.kind() == ast::constant_value::sint
			);
			auto const value = expr_kind == ast::type_info::uint32_
				? static_cast<uint32_t>(const_expr.value.get<ast::constant_value::uint>())
				: static_cast<uint32_t>(const_expr.value.get<ast::constant_value::sint>());
			if (!bz::is_valid_unicode_value(value))
			{
				context.report_error(
					src_tokens,
					bz::format(
						"the result of U+{:04X} in a constant expression is not a valid unicode codepoint",
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

ast::expression make_built_in_subscript_operator(
	lex::src_tokens src_tokens,
	ast::expression called,
	bz::vector<ast::expression> args,
	parse_context &context
)
{
	bz_assert(args.size() > 0);
	auto const [array_type, array_kind] = called.get_expr_type_and_kind();
	bz_assert(ast::remove_const_or_consteval(array_type).is<ast::ts_array>());
	auto &array_t = ast::remove_const_or_consteval(array_type).get<ast::ts_array>();

	if (args.size() > array_t.sizes.size())
	{
		context.report_error(
			src_tokens,
			bz::format("too many indicies for a {} dimensional array",
			array_t.sizes.size())
		);
		return ast::expression(src_tokens);
	}

	bool good = true;
	bool is_consteval = true;
	for (auto const &[arg, size] : bz::zip(args, array_t.sizes))
	{
		auto const [arg_type, _] = arg.get_expr_type_and_kind();
		auto const arg_t = ast::remove_const_or_consteval(arg_type);
		if (!arg_t.is<ast::ts_base_type>() || !is_integer_kind(arg_t.get<ast::ts_base_type>().info->kind))
		{
			good = false;
			context.report_error(arg, bz::format("invalid type '{}' for array subscript", arg_type));
		}

		if (!arg.is<ast::constant_expression>())
		{
			is_consteval = false;
		}
		else
		{
			auto &const_arg = arg.get<ast::constant_expression>();
			if (const_arg.value.kind() == ast::constant_value::uint)
			{
				auto const value = const_arg.value.get<ast::constant_value::uint>();
				if (value >= size)
				{
					is_consteval = false;
					context.report_error(
						arg,
						bz::format(
							"index {} is out of range in array subscript, the size is {}",
							value, size
						)
					);
				}
			}
			else if (const_arg.value.kind() == ast::constant_value::sint)
			{
				auto const value = const_arg.value.get<ast::constant_value::sint>();
				if (static_cast<uint64_t>(value) >= size || value < 0)
				{
					is_consteval = false;
					context.report_error(
						arg,
						bz::format(
							"index {} is out of range in array subscript, the size is {}",
							value, size
						)
					);
				}
			}
		}
	}

	if (!good)
	{
		return ast::expression(src_tokens);
	}

	auto const result_kind =
		array_kind == ast::expression_type_kind::lvalue
		|| array_kind == ast::expression_type_kind::lvalue_reference
			? ast::expression_type_kind::lvalue
			: ast::expression_type_kind::rvalue;

	auto result_type = [&]() {
		auto &elem_type = array_t.elem_type;

		if (args.size() == array_t.sizes.size())
		{
			return elem_type;
		}

		bz::vector<uint64_t> sizes = {};

		size_t dim = 0;
		for (auto const size : bz::reversed(array_t.sizes))
		{
			sizes.push_back(size);
			++dim;
			if (dim == args.size())
			{
				break;
			}
		}
		return ast::make_array_typespec({}, std::move(sizes), elem_type);
	}();

	if (array_type.is<ast::ts_const>() || array_type.is<ast::ts_consteval>())
	{
		result_type.add_layer<ast::ts_const>(nullptr);
	}

	if (is_consteval && called.is<ast::constant_expression>())
	{
		context.report_error(src_tokens, "array type in constant expression is not yet implemented");
		return ast::expression(src_tokens);
	}
	else
	{
		return ast::make_dynamic_expression(
			src_tokens,
			result_kind, std::move(result_type),
			ast::make_expr_subscript(std::move(called), std::move(args))
		);
	}
}

static ast::expression get_type_op_binary_equals_not_equals(
	lex::token_pos op,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
)
{
	bz_assert(op->kind == lex::token::equals || op->kind == lex::token::not_equals);
	bz_assert(lhs.not_null());
	bz_assert(rhs.not_null());
	bz_assert(lhs.is_typename());
	bz_assert(rhs.is_typename());
	lex::src_tokens const src_tokens = { lhs.get_tokens_begin(), op, rhs.get_tokens_end() };

	auto const op_str = op->value;

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
		return ast::expression(src_tokens);
	}

	auto const are_types_equal = lhs_type == rhs_type;
	auto const result = op->kind == lex::token::equals ? are_types_equal : !are_types_equal;

	return ast::make_constant_expression(
		src_tokens,
		ast::expression_type_kind::rvalue,
		ast::typespec({ ast::ts_base_type{ {}, context.get_base_type_info(ast::type_info::bool_) } }),
		ast::constant_value(result),
		ast::make_expr_binary_op(op, std::move(lhs), std::move(rhs))
	);
}

struct unary_operator_parse_function_t
{
	using fn_t = ast::expression (*)(lex::token_pos, ast::expression, parse_context &);

	uint32_t kind;
	fn_t     parse_function;
};

constexpr auto built_in_unary_operators = []() {
	using T = unary_operator_parse_function_t;
	auto result = std::array{
		T{ lex::token::plus,        &get_built_in_unary_plus                  }, // +
		T{ lex::token::minus,       &get_built_in_unary_minus                 }, // -
		T{ lex::token::address_of,  &get_built_in_unary_address_of            }, // &
		T{ lex::token::dereference, &get_built_in_unary_dereference           }, // *
		T{ lex::token::bit_not,     &get_built_in_unary_bit_not               }, // ~
		T{ lex::token::bool_not,    &get_built_in_unary_bool_not              }, // !
		T{ lex::token::plus_plus,   &get_built_in_unary_plus_plus_minus_minus }, // ++
		T{ lex::token::minus_minus, &get_built_in_unary_plus_plus_minus_minus }, // --

		T{ lex::token::kw_sizeof,   &get_built_in_unary_sizeof                }, // sizeof
		T{ lex::token::kw_typeof,   &get_built_in_unary_typeof                }, // typeof
	};

	auto const built_in_unary_count = []() {
		size_t count = 0;
		for (auto &ti : token_info)
		{
			if (ti.kind < token_info.size() && is_unary_built_in_operator(ti.kind))
			{
				++count;
			}
		}
		return count;
	}();

	if (built_in_unary_count != result.size())
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
	auto result = std::array{
		T{ lex::token::address_of,   &get_type_op_unary_reference }, // &
		T{ lex::token::dereference,  &get_type_op_unary_pointer   }, // *
		T{ lex::token::kw_const,     &get_type_op_unary_const     }, // const
		T{ lex::token::kw_consteval, &get_type_op_unary_consteval }, // consteval
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
	using fn_t = ast::expression (*)(lex::token_pos, ast::expression, ast::expression, parse_context &);

	uint32_t kind;
	fn_t     parse_function;
};

constexpr auto built_in_binary_operators = []() {
	using T = binary_operator_parse_function_t;
	auto result = std::array{
		T{ lex::token::assign,             &get_built_in_binary_assign             }, // =
		T{ lex::token::plus,               &get_built_in_binary_plus               }, // +
		T{ lex::token::plus_eq,            &get_built_in_binary_plus_minus_eq      }, // +=
		T{ lex::token::minus,              &get_built_in_binary_minus              }, // -
		T{ lex::token::minus_eq,           &get_built_in_binary_plus_minus_eq      }, // -=
		T{ lex::token::multiply,           &get_built_in_binary_multiply_divide    }, // *
		T{ lex::token::multiply_eq,        &get_built_in_binary_multiply_divide_eq }, // *=
		T{ lex::token::divide,             &get_built_in_binary_multiply_divide    }, // /
		T{ lex::token::divide_eq,          &get_built_in_binary_multiply_divide_eq }, // /=
		T{ lex::token::modulo,             &get_built_in_binary_modulo             }, // %
		T{ lex::token::modulo_eq,          &get_built_in_binary_modulo_eq          }, // %=
		T{ lex::token::equals,             &get_built_in_binary_equals_not_equals  }, // ==
		T{ lex::token::not_equals,         &get_built_in_binary_equals_not_equals  }, // !=
		T{ lex::token::less_than,          &get_built_in_binary_compare            }, // <
		T{ lex::token::less_than_eq,       &get_built_in_binary_compare            }, // <=
		T{ lex::token::greater_than,       &get_built_in_binary_compare            }, // >
		T{ lex::token::greater_than_eq,    &get_built_in_binary_compare            }, // >=
		T{ lex::token::bit_and,            &get_built_in_binary_bit_and_xor_or     }, // &
		T{ lex::token::bit_and_eq,         &get_built_in_binary_bit_and_xor_or_eq  }, // &=
		T{ lex::token::bit_xor,            &get_built_in_binary_bit_and_xor_or     }, // ^
		T{ lex::token::bit_xor_eq,         &get_built_in_binary_bit_and_xor_or_eq  }, // ^=
		T{ lex::token::bit_or,             &get_built_in_binary_bit_and_xor_or     }, // |
		T{ lex::token::bit_or_eq,          &get_built_in_binary_bit_and_xor_or_eq  }, // |=
		T{ lex::token::bit_left_shift,     &get_built_in_binary_bit_shift          }, // <<
		T{ lex::token::bit_left_shift_eq,  &get_built_in_binary_bit_shift_eq       }, // <<=
		T{ lex::token::bit_right_shift,    &get_built_in_binary_bit_shift          }, // >>
		T{ lex::token::bit_right_shift_eq, &get_built_in_binary_bit_shift_eq       }, // >>=
		T{ lex::token::bool_and,           &get_built_in_binary_bool_and_xor_or    }, // &&
		T{ lex::token::bool_xor,           &get_built_in_binary_bool_and_xor_or    }, // ^^
		T{ lex::token::bool_or,            &get_built_in_binary_bool_and_xor_or    }, // ||

		T{ lex::token::comma,              &get_built_in_binary_comma              }, // ,
	};

	auto const built_in_binary_count = []() {
		size_t count = 0;
		for (auto &ti : token_info)
		{
			if (ti.kind < token_info.size() && is_binary_built_in_operator(ti.kind))
			{
				++count;
			}
		}
		return count;
	}();

	if (built_in_binary_count != result.size())
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
	auto result = std::array{
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

ast::expression make_built_in_operation(
	lex::token_pos op,
	ast::expression expr,
	parse_context &context
)
{
	auto const op_kind = op->kind;
	return [&]<size_t ...Ns>(bz::meta::index_sequence<Ns...>) {
		ast::expression result;
		((
			op_kind == built_in_unary_operators[Ns].kind
			? (void)(result = built_in_unary_operators[Ns].parse_function(op, std::move(expr), context))
			: (void)0
		), ...);
		return result;
	}(bz::meta::make_index_sequence<built_in_unary_operators.size()>{});
}

ast::expression make_built_in_type_operation(
	lex::token_pos op,
	ast::expression expr,
	parse_context &context
)
{
	auto const op_kind = op->kind;
	return [&]<size_t ...Ns>(bz::meta::index_sequence<Ns...>) {
		ast::expression result;
		((
			op_kind == type_op_unary_operators[Ns].kind
			? (void)(result = type_op_unary_operators[Ns].parse_function(op, std::move(expr), context))
			: (void)0
		), ...);
		return result;
	}(bz::meta::make_index_sequence<type_op_unary_operators.size()>{});
}

ast::expression make_built_in_operation(
	lex::token_pos op,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
)
{
	auto const op_kind = op->kind;
	return [&]<size_t ...Ns>(bz::meta::index_sequence<Ns...>) {
		ast::expression result;
		((
			op_kind == built_in_binary_operators[Ns].kind
			? (void)(result = built_in_binary_operators[Ns].parse_function(op, std::move(lhs), std::move(rhs), context))
			: (void)0
		), ...);
		return result;
	}(bz::meta::make_index_sequence<built_in_binary_operators.size()>{});
}

ast::expression make_built_in_type_operation(
	lex::token_pos op,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
)
{
	auto const op_kind = op->kind;
	return [&]<size_t ...Ns>(bz::meta::index_sequence<Ns...>) {
		ast::expression result;
		((
			op_kind == type_op_binary_operators[Ns].kind
			? (void)(result = type_op_binary_operators[Ns].parse_function(op, std::move(lhs), std::move(rhs), context))
			: (void)0
		), ...);
		return result;
	}(bz::meta::make_index_sequence<type_op_binary_operators.size()>{});
}

} // namespace ctx
