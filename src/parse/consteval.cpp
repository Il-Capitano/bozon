#include "consteval.h"
#include "safe_operations.h"

namespace parse
{

static ast::constant_value evaluate_unary_plus(
	[[maybe_unused]] ast::expression const &original_expr,
	ast::expression const &expr,
	[[maybe_unused]] ctx::parse_context &context
)
{
	bz_assert(expr.is<ast::constant_expression>());
	auto const &const_expr = expr.get<ast::constant_expression>();
	auto const &value = const_expr.value;
	// this is a no-op, it doesn't change the value
	return value;
}

static ast::constant_value evaluate_unary_minus(
	ast::expression const &original_expr,
	ast::expression const &expr,
	ctx::parse_context &context
)
{
	bz_assert(expr.is<ast::constant_expression>());
	auto const &const_expr = expr.get<ast::constant_expression>();
	auto const &value = const_expr.value;

	switch (value.kind())
	{
	case ast::constant_value::sint:
	{
		bz_assert(ast::remove_const_or_consteval(const_expr.type).is<ast::ts_base_type>());
		auto const type = ast::remove_const_or_consteval(const_expr.type).get<ast::ts_base_type>().info->kind;
		auto const int_val = value.get<ast::constant_value::sint>();
		return ast::constant_value(safe_unary_minus(
			original_expr.src_tokens, original_expr.paren_level,
			int_val, type, context
		));
	}
	case ast::constant_value::float32:
	{
		// there's no possible overflow with floating point numbers
		auto const float_val = value.get<ast::constant_value::float32>();
		return ast::constant_value(-float_val);
	}
	case ast::constant_value::float64:
	{
		// there's no possible overflow with floating point numbers
		auto const float_val = value.get<ast::constant_value::float64>();
		return ast::constant_value(-float_val);
	}
	default:
		bz_unreachable;
	}
}

static ast::constant_value evaluate_unary_bit_not(
	[[maybe_unused]] ast::expression const &original_expr,
	ast::expression const &expr,
	[[maybe_unused]] ctx::parse_context &context
)
{
	bz_assert(expr.is<ast::constant_expression>());
	auto const &const_expr = expr.get<ast::constant_expression>();
	auto const &value = const_expr.value;

	bz_assert(value.is<ast::constant_value::uint>() || value.is<ast::constant_value::boolean>());
	if (value.is<ast::constant_value::uint>())
	{
		bz_assert(ast::remove_const_or_consteval(const_expr.type).is<ast::ts_base_type>());
		auto const type = ast::remove_const_or_consteval(const_expr.type).get<ast::ts_base_type>().info->kind;
		auto const int_val = value.get<ast::constant_value::uint>();

		uint64_t const result =
			type == ast::type_info::uint8_  ? static_cast<uint8_t> (~int_val) :
			type == ast::type_info::uint16_ ? static_cast<uint16_t>(~int_val) :
			type == ast::type_info::uint32_ ? static_cast<uint32_t>(~int_val) :
			static_cast<uint64_t>(~int_val);

		return ast::constant_value(result);
	}
	else
	{
		auto const bool_val = value.get<ast::constant_value::boolean>();
		return ast::constant_value(!bool_val);
	}
}

static ast::constant_value evaluate_unary_bool_not(
	[[maybe_unused]] ast::expression const &original_expr,
	ast::expression const &expr,
	[[maybe_unused]] ctx::parse_context &context
)
{
	bz_assert(expr.is<ast::constant_expression>());
	auto const &const_expr = expr.get<ast::constant_expression>();
	auto const &value = const_expr.value;

	bz_assert(value.is<ast::constant_value::boolean>());
	auto const bool_val = value.get<ast::constant_value::boolean>();
	return ast::constant_value(!bool_val);
}

static ast::constant_value evaluate_unary_op(
	ast::expression const &original_expr,
	uint32_t op, ast::expression const &expr,
	ctx::parse_context &context
)
{
	switch (op)
	{
	case lex::token::plus:
		return evaluate_unary_plus(original_expr, expr, context);
	case lex::token::minus:
		return evaluate_unary_minus(original_expr, expr, context);
	case lex::token::bit_not:
		return evaluate_unary_bit_not(original_expr, expr, context);
	case lex::token::bool_not:
		return evaluate_unary_bool_not(original_expr, expr, context);
	default:
		return {};
	}
}


static ast::constant_value evaluate_binary_plus(
	ast::expression const &original_expr,
	ast::expression const &lhs, ast::expression const &rhs,
	ctx::parse_context &context
)
{
	bz_assert(lhs.is<ast::constant_expression>());
	auto const &lhs_const_expr = lhs.get<ast::constant_expression>();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is<ast::constant_expression>());
	auto const &rhs_const_expr = rhs.get<ast::constant_expression>();
	auto const &rhs_value = rhs_const_expr.value;

	if (lhs_value.kind() == rhs_value.kind())
	{
		bz_assert(ast::remove_const_or_consteval(lhs_const_expr.type).is<ast::ts_base_type>());
		auto const type = ast::remove_const_or_consteval(lhs_const_expr.type).get<ast::ts_base_type>().info->kind;
		switch (lhs_value.kind())
		{
		case ast::constant_value::sint:
		{
			auto const lhs_int_val = lhs_value.get<ast::constant_value::sint>();
			auto const rhs_int_val = rhs_value.get<ast::constant_value::sint>();
			return ast::constant_value(safe_binary_plus(
				original_expr.src_tokens, original_expr.paren_level,
				lhs_int_val, rhs_int_val, type, context
			));
		}
		case ast::constant_value::uint:
		{
			auto const lhs_int_val = lhs_value.get<ast::constant_value::uint>();
			auto const rhs_int_val = rhs_value.get<ast::constant_value::uint>();
			return ast::constant_value(safe_binary_plus(
				original_expr.src_tokens, original_expr.paren_level,
				lhs_int_val, rhs_int_val, type, context
			));
		}
		case ast::constant_value::float32:
		{
			auto const lhs_float_val = lhs_value.get<ast::constant_value::float32>();
			auto const rhs_float_val = rhs_value.get<ast::constant_value::float32>();
			return ast::constant_value(safe_binary_plus(
				original_expr.src_tokens, original_expr.paren_level,
				lhs_float_val, rhs_float_val, context
			));
		}
		case ast::constant_value::float64:
		{
			auto const lhs_float_val = lhs_value.get<ast::constant_value::float64>();
			auto const rhs_float_val = rhs_value.get<ast::constant_value::float64>();
			return ast::constant_value(safe_binary_plus(
				original_expr.src_tokens, original_expr.paren_level,
				lhs_float_val, rhs_float_val, context
			));
		}
		default:
			bz_unreachable;
		}
	}
	else if (lhs_value.is<ast::constant_value::u8char>())
	{
		bz_assert(rhs_value.is<ast::constant_value::sint>() || rhs_value.is<ast::constant_value::uint>());

		auto const result = rhs_value.is<ast::constant_value::sint>()
			? safe_binary_plus(
				original_expr.src_tokens, original_expr.paren_level,
				lhs_value.get<ast::constant_value::u8char>(), rhs_value.get<ast::constant_value::sint>(),
				context
			)
			: safe_binary_plus(
				original_expr.src_tokens, original_expr.paren_level,
				lhs_value.get<ast::constant_value::u8char>(), rhs_value.get<ast::constant_value::uint>(),
				context
			);
		if (result.has_value())
		{
			return ast::constant_value(result.get());
		}
		else
		{
			return {};
		}
	}
	else
	{
		bz_assert(rhs_value.is<ast::constant_value::u8char>());
		bz_assert(lhs_value.is<ast::constant_value::sint>() || lhs_value.is<ast::constant_value::uint>());

		auto const result = lhs_value.is<ast::constant_value::sint>()
			? safe_binary_plus(
				original_expr.src_tokens, original_expr.paren_level,
				lhs_value.get<ast::constant_value::sint>(), rhs_value.get<ast::constant_value::u8char>(),
				context
			)
			: safe_binary_plus(
				original_expr.src_tokens, original_expr.paren_level,
				lhs_value.get<ast::constant_value::uint>(), rhs_value.get<ast::constant_value::u8char>(),
				context
			);
		if (result.has_value())
		{
			return ast::constant_value(result.get());
		}
		else
		{
			return {};
		}
	}
}

static ast::constant_value evaluate_binary_minus(
	ast::expression const &original_expr,
	ast::expression const &lhs, ast::expression const &rhs,
	ctx::parse_context &context
)
{
	bz_assert(lhs.is<ast::constant_expression>());
	auto const &lhs_const_expr = lhs.get<ast::constant_expression>();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is<ast::constant_expression>());
	auto const &rhs_const_expr = rhs.get<ast::constant_expression>();
	auto const &rhs_value = rhs_const_expr.value;

	if (lhs_value.kind() == rhs_value.kind())
	{
		bz_assert(ast::remove_const_or_consteval(lhs_const_expr.type).is<ast::ts_base_type>());
		auto const type = ast::remove_const_or_consteval(lhs_const_expr.type).get<ast::ts_base_type>().info->kind;
		switch (lhs_value.kind())
		{
		case ast::constant_value::sint:
		{
			auto const lhs_int_val = lhs_value.get<ast::constant_value::sint>();
			auto const rhs_int_val = rhs_value.get<ast::constant_value::sint>();
			return ast::constant_value(safe_binary_minus(
				original_expr.src_tokens, original_expr.paren_level,
				lhs_int_val, rhs_int_val, type, context
			));
		}
		case ast::constant_value::uint:
		{
			auto const lhs_int_val = lhs_value.get<ast::constant_value::uint>();
			auto const rhs_int_val = rhs_value.get<ast::constant_value::uint>();
			return ast::constant_value(safe_binary_minus(
				original_expr.src_tokens, original_expr.paren_level,
				lhs_int_val, rhs_int_val, type, context
			));
		}
		case ast::constant_value::float32:
		{
			auto const lhs_float_val = lhs_value.get<ast::constant_value::float32>();
			auto const rhs_float_val = rhs_value.get<ast::constant_value::float32>();
			return ast::constant_value(safe_binary_minus(
				original_expr.src_tokens, original_expr.paren_level,
				lhs_float_val, rhs_float_val, context
			));
		}
		case ast::constant_value::float64:
		{
			auto const lhs_float_val = lhs_value.get<ast::constant_value::float64>();
			auto const rhs_float_val = rhs_value.get<ast::constant_value::float64>();
			return ast::constant_value(safe_binary_minus(
				original_expr.src_tokens, original_expr.paren_level,
				lhs_float_val, rhs_float_val, context
			));
		}
		case ast::constant_value::u8char:
			return ast::constant_value(
				static_cast<int64_t>(lhs_value.get<ast::constant_value::u8char>())
				- static_cast<int64_t>(rhs_value.get<ast::constant_value::u8char>())
			);
		default:
			bz_unreachable;
		}
	}
	else
	{
		bz_assert(lhs_value.is<ast::constant_value::u8char>());
		bz_assert(rhs_value.is<ast::constant_value::sint>() || rhs_value.is<ast::constant_value::uint>());

		auto const result = rhs_value.is<ast::constant_value::sint>()
			? safe_binary_minus(
				original_expr.src_tokens, original_expr.paren_level,
				lhs_value.get<ast::constant_value::u8char>(), rhs_value.get<ast::constant_value::sint>(),
				context
			)
			: safe_binary_minus(
				original_expr.src_tokens, original_expr.paren_level,
				lhs_value.get<ast::constant_value::u8char>(), rhs_value.get<ast::constant_value::uint>(),
				context
			);
		if (result.has_value())
		{
			return ast::constant_value(result.get());
		}
		else
		{
			return {};
		}
	}
}

static ast::constant_value evaluate_binary_multiply(
	ast::expression const &original_expr,
	ast::expression const &lhs, ast::expression const &rhs,
	ctx::parse_context &context
)
{
	bz_assert(lhs.is<ast::constant_expression>());
	auto const &lhs_const_expr = lhs.get<ast::constant_expression>();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is<ast::constant_expression>());
	auto const &rhs_const_expr = rhs.get<ast::constant_expression>();
	auto const &rhs_value = rhs_const_expr.value;
	bz_assert(lhs_value.kind() == rhs_value.kind());

	bz_assert(ast::remove_const_or_consteval(lhs_const_expr.type).is<ast::ts_base_type>());
	auto const type = ast::remove_const_or_consteval(lhs_const_expr.type).get<ast::ts_base_type>().info->kind;
	switch (lhs_value.kind())
	{
	case ast::constant_value::sint:
	{
		auto const lhs_int_val = lhs_value.get<ast::constant_value::sint>();
		auto const rhs_int_val = rhs_value.get<ast::constant_value::sint>();
		return ast::constant_value(safe_binary_multiply(
			original_expr.src_tokens, original_expr.paren_level,
			lhs_int_val, rhs_int_val, type, context
		));
	}
	case ast::constant_value::uint:
	{
		auto const lhs_int_val = lhs_value.get<ast::constant_value::uint>();
		auto const rhs_int_val = rhs_value.get<ast::constant_value::uint>();
		return ast::constant_value(safe_binary_multiply(
			original_expr.src_tokens, original_expr.paren_level,
			lhs_int_val, rhs_int_val, type, context
		));
	}
	case ast::constant_value::float32:
	{
		auto const lhs_float_val = lhs_value.get<ast::constant_value::float32>();
		auto const rhs_float_val = rhs_value.get<ast::constant_value::float32>();
		return ast::constant_value(safe_binary_multiply(
			original_expr.src_tokens, original_expr.paren_level,
			lhs_float_val, rhs_float_val, context
		));
	}
	case ast::constant_value::float64:
	{
		auto const lhs_float_val = lhs_value.get<ast::constant_value::float64>();
		auto const rhs_float_val = rhs_value.get<ast::constant_value::float64>();
		return ast::constant_value(safe_binary_multiply(
			original_expr.src_tokens, original_expr.paren_level,
			lhs_float_val, rhs_float_val, context
		));
	}
	default:
		bz_unreachable;
	}
}

static ast::constant_value evaluate_binary_divide(
	ast::expression const &original_expr,
	ast::expression const &lhs, ast::expression const &rhs,
	ctx::parse_context &context
)
{
	bz_assert(lhs.is<ast::constant_expression>());
	auto const &lhs_const_expr = lhs.get<ast::constant_expression>();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is<ast::constant_expression>());
	auto const &rhs_const_expr = rhs.get<ast::constant_expression>();
	auto const &rhs_value = rhs_const_expr.value;
	bz_assert(lhs_value.kind() == rhs_value.kind());

	bz_assert(ast::remove_const_or_consteval(lhs_const_expr.type).is<ast::ts_base_type>());
	auto const type = ast::remove_const_or_consteval(lhs_const_expr.type).get<ast::ts_base_type>().info->kind;
	switch (lhs_value.kind())
	{
	case ast::constant_value::sint:
	{
		auto const lhs_int_val = lhs_value.get<ast::constant_value::sint>();
		auto const rhs_int_val = rhs_value.get<ast::constant_value::sint>();
		auto const result = safe_binary_divide(
			original_expr.src_tokens, original_expr.paren_level,
			lhs_int_val, rhs_int_val, type, context
		);
		if (result.has_value())
		{
			return ast::constant_value(result.get());
		}
		else
		{
			return {};
		}
	}
	case ast::constant_value::uint:
	{
		auto const lhs_int_val = lhs_value.get<ast::constant_value::uint>();
		auto const rhs_int_val = rhs_value.get<ast::constant_value::uint>();
		auto const result = safe_binary_divide(
			original_expr.src_tokens, original_expr.paren_level,
			lhs_int_val, rhs_int_val, type, context
		);
		if (result.has_value())
		{
			return ast::constant_value(result.get());
		}
		else
		{
			return {};
		}
	}
	case ast::constant_value::float32:
	{
		auto const lhs_float_val = lhs_value.get<ast::constant_value::float32>();
		auto const rhs_float_val = rhs_value.get<ast::constant_value::float32>();
		return ast::constant_value(safe_binary_divide(
			original_expr.src_tokens, original_expr.paren_level,
			lhs_float_val, rhs_float_val, context
		));
	}
	case ast::constant_value::float64:
	{
		auto const lhs_float_val = lhs_value.get<ast::constant_value::float64>();
		auto const rhs_float_val = rhs_value.get<ast::constant_value::float64>();
		return ast::constant_value(safe_binary_divide(
			original_expr.src_tokens, original_expr.paren_level,
			lhs_float_val, rhs_float_val, context
		));
	}
	default:
		bz_unreachable;
	}
}

static ast::constant_value evaluate_binary_modulo(
	ast::expression const &original_expr,
	ast::expression const &lhs, ast::expression const &rhs,
	ctx::parse_context &context
)
{
	bz_assert(lhs.is<ast::constant_expression>());
	auto const &lhs_const_expr = lhs.get<ast::constant_expression>();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is<ast::constant_expression>());
	auto const &rhs_const_expr = rhs.get<ast::constant_expression>();
	auto const &rhs_value = rhs_const_expr.value;
	bz_assert(lhs_value.kind() == rhs_value.kind());

	bz_assert(ast::remove_const_or_consteval(lhs_const_expr.type).is<ast::ts_base_type>());
	auto const type = ast::remove_const_or_consteval(lhs_const_expr.type).get<ast::ts_base_type>().info->kind;
	switch (lhs_value.kind())
	{
	case ast::constant_value::sint:
	{
		auto const lhs_int_val = lhs_value.get<ast::constant_value::sint>();
		auto const rhs_int_val = rhs_value.get<ast::constant_value::sint>();
		auto const result = safe_binary_modulo(
			original_expr.src_tokens, original_expr.paren_level,
			lhs_int_val, rhs_int_val, type, context
		);
		if (result.has_value())
		{
			return ast::constant_value(result.get());
		}
		else
		{
			return {};
		}
	}
	case ast::constant_value::uint:
	{
		auto const lhs_int_val = lhs_value.get<ast::constant_value::uint>();
		auto const rhs_int_val = rhs_value.get<ast::constant_value::uint>();
		auto const result = safe_binary_modulo(
			original_expr.src_tokens, original_expr.paren_level,
			lhs_int_val, rhs_int_val, type, context
		);
		if (result.has_value())
		{
			return ast::constant_value(result.get());
		}
		else
		{
			return {};
		}
	}
	default:
		bz_unreachable;
	}
}

static ast::constant_value evaluate_binary_equals(
	[[maybe_unused]] ast::expression const &original_expr,
	ast::expression const &lhs, ast::expression const &rhs,
	[[maybe_unused]] ctx::parse_context &context
)
{
	bz_assert(lhs.is<ast::constant_expression>());
	auto const &lhs_const_expr = lhs.get<ast::constant_expression>();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is<ast::constant_expression>());
	auto const &rhs_const_expr = rhs.get<ast::constant_expression>();
	auto const &rhs_value = rhs_const_expr.value;

	bz_assert(lhs_value.kind() == rhs_value.kind());

	switch (lhs_value.kind())
	{
	case ast::constant_value::sint:
	{
		auto const lhs_int_val = lhs_value.get<ast::constant_value::sint>();
		auto const rhs_int_val = rhs_value.get<ast::constant_value::sint>();
		return ast::constant_value(lhs_int_val == rhs_int_val);
	}
	case ast::constant_value::uint:
	{
		auto const lhs_int_val = lhs_value.get<ast::constant_value::uint>();
		auto const rhs_int_val = rhs_value.get<ast::constant_value::uint>();
		return ast::constant_value(lhs_int_val == rhs_int_val);
	}
	case ast::constant_value::float32:
	{
		auto const lhs_float_val = lhs_value.get<ast::constant_value::float32>();
		auto const rhs_float_val = rhs_value.get<ast::constant_value::float32>();
		return ast::constant_value(safe_binary_equals(
			original_expr.src_tokens, original_expr.paren_level,
			lhs_float_val, rhs_float_val, context
		));
	}
	case ast::constant_value::float64:
	{
		auto const lhs_float_val = lhs_value.get<ast::constant_value::float64>();
		auto const rhs_float_val = rhs_value.get<ast::constant_value::float64>();
		return ast::constant_value(safe_binary_equals(
			original_expr.src_tokens, original_expr.paren_level,
			lhs_float_val, rhs_float_val, context
		));
	}
	case ast::constant_value::u8char:
	{
		auto const lhs_char_val = lhs_value.get<ast::constant_value::u8char>();
		auto const rhs_char_val = rhs_value.get<ast::constant_value::u8char>();
		return ast::constant_value(lhs_char_val == rhs_char_val);
	}
	case ast::constant_value::boolean:
	{
		auto const lhs_bool_val = lhs_value.get<ast::constant_value::boolean>();
		auto const rhs_bool_val = rhs_value.get<ast::constant_value::boolean>();
		return ast::constant_value(lhs_bool_val == rhs_bool_val);
	}
	case ast::constant_value::string:
	{
		auto const lhs_str_val = lhs_value.get<ast::constant_value::string>().as_string_view();
		auto const rhs_str_val = rhs_value.get<ast::constant_value::string>().as_string_view();
		return ast::constant_value(lhs_str_val == rhs_str_val);
	}
	case ast::constant_value::null:
	{
		return ast::constant_value(true);
	}
	default:
		bz_unreachable;
	}
}

static ast::constant_value evaluate_binary_not_equals(
	[[maybe_unused]] ast::expression const &original_expr,
	ast::expression const &lhs, ast::expression const &rhs,
	[[maybe_unused]] ctx::parse_context &context
)
{
	bz_assert(lhs.is<ast::constant_expression>());
	auto const &lhs_const_expr = lhs.get<ast::constant_expression>();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is<ast::constant_expression>());
	auto const &rhs_const_expr = rhs.get<ast::constant_expression>();
	auto const &rhs_value = rhs_const_expr.value;

	bz_assert(lhs_value.kind() == rhs_value.kind());

	switch (lhs_value.kind())
	{
	case ast::constant_value::sint:
	{
		auto const lhs_int_val = lhs_value.get<ast::constant_value::sint>();
		auto const rhs_int_val = rhs_value.get<ast::constant_value::sint>();
		return ast::constant_value(lhs_int_val != rhs_int_val);
	}
	case ast::constant_value::uint:
	{
		auto const lhs_int_val = lhs_value.get<ast::constant_value::uint>();
		auto const rhs_int_val = rhs_value.get<ast::constant_value::uint>();
		return ast::constant_value(lhs_int_val != rhs_int_val);
	}
	case ast::constant_value::float32:
	{
		auto const lhs_float_val = lhs_value.get<ast::constant_value::float32>();
		auto const rhs_float_val = rhs_value.get<ast::constant_value::float32>();
		return ast::constant_value(lhs_float_val != rhs_float_val);
	}
	case ast::constant_value::float64:
	{
		auto const lhs_float_val = lhs_value.get<ast::constant_value::float64>();
		auto const rhs_float_val = rhs_value.get<ast::constant_value::float64>();
		return ast::constant_value(lhs_float_val != rhs_float_val);
	}
	case ast::constant_value::u8char:
	{
		auto const lhs_char_val = lhs_value.get<ast::constant_value::u8char>();
		auto const rhs_char_val = rhs_value.get<ast::constant_value::u8char>();
		return ast::constant_value(lhs_char_val != rhs_char_val);
	}
	case ast::constant_value::boolean:
	{
		auto const lhs_bool_val = lhs_value.get<ast::constant_value::boolean>();
		auto const rhs_bool_val = rhs_value.get<ast::constant_value::boolean>();
		return ast::constant_value(lhs_bool_val != rhs_bool_val);
	}
	case ast::constant_value::string:
	{
		auto const lhs_str_val = lhs_value.get<ast::constant_value::string>().as_string_view();
		auto const rhs_str_val = rhs_value.get<ast::constant_value::string>().as_string_view();
		return ast::constant_value(lhs_str_val != rhs_str_val);
	}
	case ast::constant_value::null:
	{
		return ast::constant_value(false);
	}
	default:
		bz_unreachable;
	}
}

static ast::constant_value evaluate_binary_less_than(
	[[maybe_unused]] ast::expression const &original_expr,
	ast::expression const &lhs, ast::expression const &rhs,
	[[maybe_unused]] ctx::parse_context &context
)
{
	bz_assert(lhs.is<ast::constant_expression>());
	auto const &lhs_const_expr = lhs.get<ast::constant_expression>();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is<ast::constant_expression>());
	auto const &rhs_const_expr = rhs.get<ast::constant_expression>();
	auto const &rhs_value = rhs_const_expr.value;

	bz_assert(lhs_value.kind() == rhs_value.kind());

	switch (lhs_value.kind())
	{
	case ast::constant_value::sint:
	{
		auto const lhs_int_val = lhs_value.get<ast::constant_value::sint>();
		auto const rhs_int_val = rhs_value.get<ast::constant_value::sint>();
		return ast::constant_value(lhs_int_val < rhs_int_val);
	}
	case ast::constant_value::uint:
	{
		auto const lhs_int_val = lhs_value.get<ast::constant_value::uint>();
		auto const rhs_int_val = rhs_value.get<ast::constant_value::uint>();
		return ast::constant_value(lhs_int_val < rhs_int_val);
	}
	case ast::constant_value::float32:
	{
		auto const lhs_float_val = lhs_value.get<ast::constant_value::float32>();
		auto const rhs_float_val = rhs_value.get<ast::constant_value::float32>();
		return ast::constant_value(lhs_float_val < rhs_float_val);
	}
	case ast::constant_value::float64:
	{
		auto const lhs_float_val = lhs_value.get<ast::constant_value::float64>();
		auto const rhs_float_val = rhs_value.get<ast::constant_value::float64>();
		return ast::constant_value(lhs_float_val < rhs_float_val);
	}
	case ast::constant_value::u8char:
	{
		auto const lhs_char_val = lhs_value.get<ast::constant_value::u8char>();
		auto const rhs_char_val = rhs_value.get<ast::constant_value::u8char>();
		return ast::constant_value(lhs_char_val < rhs_char_val);
	}
	case ast::constant_value::null:
	{
		return ast::constant_value(false);
	}
	default:
		bz_unreachable;
	}
}

static ast::constant_value evaluate_binary_less_than_eq(
	[[maybe_unused]] ast::expression const &original_expr,
	ast::expression const &lhs, ast::expression const &rhs,
	[[maybe_unused]] ctx::parse_context &context
)
{
	bz_assert(lhs.is<ast::constant_expression>());
	auto const &lhs_const_expr = lhs.get<ast::constant_expression>();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is<ast::constant_expression>());
	auto const &rhs_const_expr = rhs.get<ast::constant_expression>();
	auto const &rhs_value = rhs_const_expr.value;

	bz_assert(lhs_value.kind() == rhs_value.kind());

	switch (lhs_value.kind())
	{
	case ast::constant_value::sint:
	{
		auto const lhs_int_val = lhs_value.get<ast::constant_value::sint>();
		auto const rhs_int_val = rhs_value.get<ast::constant_value::sint>();
		return ast::constant_value(lhs_int_val <= rhs_int_val);
	}
	case ast::constant_value::uint:
	{
		auto const lhs_int_val = lhs_value.get<ast::constant_value::uint>();
		auto const rhs_int_val = rhs_value.get<ast::constant_value::uint>();
		return ast::constant_value(lhs_int_val <= rhs_int_val);
	}
	case ast::constant_value::float32:
	{
		auto const lhs_float_val = lhs_value.get<ast::constant_value::float32>();
		auto const rhs_float_val = rhs_value.get<ast::constant_value::float32>();
		return ast::constant_value(lhs_float_val <= rhs_float_val);
	}
	case ast::constant_value::float64:
	{
		auto const lhs_float_val = lhs_value.get<ast::constant_value::float64>();
		auto const rhs_float_val = rhs_value.get<ast::constant_value::float64>();
		return ast::constant_value(lhs_float_val <= rhs_float_val);
	}
	case ast::constant_value::u8char:
	{
		auto const lhs_char_val = lhs_value.get<ast::constant_value::u8char>();
		auto const rhs_char_val = rhs_value.get<ast::constant_value::u8char>();
		return ast::constant_value(lhs_char_val <= rhs_char_val);
	}
	case ast::constant_value::null:
	{
		return ast::constant_value(true);
	}
	default:
		bz_unreachable;
	}
}

static ast::constant_value evaluate_binary_greater_than(
	[[maybe_unused]] ast::expression const &original_expr,
	ast::expression const &lhs, ast::expression const &rhs,
	[[maybe_unused]] ctx::parse_context &context
)
{
	bz_assert(lhs.is<ast::constant_expression>());
	auto const &lhs_const_expr = lhs.get<ast::constant_expression>();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is<ast::constant_expression>());
	auto const &rhs_const_expr = rhs.get<ast::constant_expression>();
	auto const &rhs_value = rhs_const_expr.value;

	bz_assert(lhs_value.kind() == rhs_value.kind());

	switch (lhs_value.kind())
	{
	case ast::constant_value::sint:
	{
		auto const lhs_int_val = lhs_value.get<ast::constant_value::sint>();
		auto const rhs_int_val = rhs_value.get<ast::constant_value::sint>();
		return ast::constant_value(lhs_int_val > rhs_int_val);
	}
	case ast::constant_value::uint:
	{
		auto const lhs_int_val = lhs_value.get<ast::constant_value::uint>();
		auto const rhs_int_val = rhs_value.get<ast::constant_value::uint>();
		return ast::constant_value(lhs_int_val > rhs_int_val);
	}
	case ast::constant_value::float32:
	{
		auto const lhs_float_val = lhs_value.get<ast::constant_value::float32>();
		auto const rhs_float_val = rhs_value.get<ast::constant_value::float32>();
		return ast::constant_value(lhs_float_val > rhs_float_val);
	}
	case ast::constant_value::float64:
	{
		auto const lhs_float_val = lhs_value.get<ast::constant_value::float64>();
		auto const rhs_float_val = rhs_value.get<ast::constant_value::float64>();
		return ast::constant_value(lhs_float_val > rhs_float_val);
	}
	case ast::constant_value::u8char:
	{
		auto const lhs_char_val = lhs_value.get<ast::constant_value::u8char>();
		auto const rhs_char_val = rhs_value.get<ast::constant_value::u8char>();
		return ast::constant_value(lhs_char_val > rhs_char_val);
	}
	case ast::constant_value::null:
	{
		return ast::constant_value(false);
	}
	default:
		bz_unreachable;
	}
}

static ast::constant_value evaluate_binary_greater_than_eq(
	[[maybe_unused]] ast::expression const &original_expr,
	ast::expression const &lhs, ast::expression const &rhs,
	[[maybe_unused]] ctx::parse_context &context
)
{
	bz_assert(lhs.is<ast::constant_expression>());
	auto const &lhs_const_expr = lhs.get<ast::constant_expression>();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is<ast::constant_expression>());
	auto const &rhs_const_expr = rhs.get<ast::constant_expression>();
	auto const &rhs_value = rhs_const_expr.value;

	bz_assert(lhs_value.kind() == rhs_value.kind());

	switch (lhs_value.kind())
	{
	case ast::constant_value::sint:
	{
		auto const lhs_int_val = lhs_value.get<ast::constant_value::sint>();
		auto const rhs_int_val = rhs_value.get<ast::constant_value::sint>();
		return ast::constant_value(lhs_int_val >= rhs_int_val);
	}
	case ast::constant_value::uint:
	{
		auto const lhs_int_val = lhs_value.get<ast::constant_value::uint>();
		auto const rhs_int_val = rhs_value.get<ast::constant_value::uint>();
		return ast::constant_value(lhs_int_val >= rhs_int_val);
	}
	case ast::constant_value::float32:
	{
		auto const lhs_float_val = lhs_value.get<ast::constant_value::float32>();
		auto const rhs_float_val = rhs_value.get<ast::constant_value::float32>();
		return ast::constant_value(lhs_float_val >= rhs_float_val);
	}
	case ast::constant_value::float64:
	{
		auto const lhs_float_val = lhs_value.get<ast::constant_value::float64>();
		auto const rhs_float_val = rhs_value.get<ast::constant_value::float64>();
		return ast::constant_value(lhs_float_val >= rhs_float_val);
	}
	case ast::constant_value::u8char:
	{
		auto const lhs_char_val = lhs_value.get<ast::constant_value::u8char>();
		auto const rhs_char_val = rhs_value.get<ast::constant_value::u8char>();
		return ast::constant_value(lhs_char_val >= rhs_char_val);
	}
	case ast::constant_value::null:
	{
		return ast::constant_value(true);
	}
	default:
		bz_unreachable;
	}
}

static ast::constant_value evaluate_binary_bit_and(
	[[maybe_unused]] ast::expression const &original_expr,
	ast::expression const &lhs, ast::expression const &rhs,
	[[maybe_unused]] ctx::parse_context &context
)
{
	bz_assert(lhs.is<ast::constant_expression>());
	auto const &lhs_const_expr = lhs.get<ast::constant_expression>();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is<ast::constant_expression>());
	auto const &rhs_const_expr = rhs.get<ast::constant_expression>();
	auto const &rhs_value = rhs_const_expr.value;

	bz_assert(lhs_value.kind() == rhs_value.kind());

	switch (lhs_value.kind())
	{
	case ast::constant_value::uint:
	{
		auto const lhs_int_val = lhs_value.get<ast::constant_value::uint>();
		auto const rhs_int_val = rhs_value.get<ast::constant_value::uint>();
		return ast::constant_value(lhs_int_val & rhs_int_val);
	}
	case ast::constant_value::boolean:
	{
		auto const lhs_bool_val = lhs_value.get<ast::constant_value::boolean>();
		auto const rhs_bool_val = rhs_value.get<ast::constant_value::boolean>();
		return ast::constant_value(lhs_bool_val && rhs_bool_val);
	}
	default:
		bz_unreachable;
	}
}

static ast::constant_value evaluate_binary_bit_xor(
	[[maybe_unused]] ast::expression const &original_expr,
	ast::expression const &lhs, ast::expression const &rhs,
	[[maybe_unused]] ctx::parse_context &context
)
{
	bz_assert(lhs.is<ast::constant_expression>());
	auto const &lhs_const_expr = lhs.get<ast::constant_expression>();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is<ast::constant_expression>());
	auto const &rhs_const_expr = rhs.get<ast::constant_expression>();
	auto const &rhs_value = rhs_const_expr.value;

	bz_assert(lhs_value.kind() == rhs_value.kind());

	switch (lhs_value.kind())
	{
	case ast::constant_value::uint:
	{
		auto const lhs_int_val = lhs_value.get<ast::constant_value::uint>();
		auto const rhs_int_val = rhs_value.get<ast::constant_value::uint>();
		return ast::constant_value(lhs_int_val ^ rhs_int_val);
	}
	case ast::constant_value::boolean:
	{
		auto const lhs_bool_val = lhs_value.get<ast::constant_value::boolean>();
		auto const rhs_bool_val = rhs_value.get<ast::constant_value::boolean>();
		return ast::constant_value(lhs_bool_val != rhs_bool_val);
	}
	default:
		bz_unreachable;
	}
}

static ast::constant_value evaluate_binary_bit_or(
	[[maybe_unused]] ast::expression const &original_expr,
	ast::expression const &lhs, ast::expression const &rhs,
	[[maybe_unused]] ctx::parse_context &context
)
{
	bz_assert(lhs.is<ast::constant_expression>());
	auto const &lhs_const_expr = lhs.get<ast::constant_expression>();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is<ast::constant_expression>());
	auto const &rhs_const_expr = rhs.get<ast::constant_expression>();
	auto const &rhs_value = rhs_const_expr.value;

	bz_assert(lhs_value.kind() == rhs_value.kind());

	switch (lhs_value.kind())
	{
	case ast::constant_value::uint:
	{
		auto const lhs_int_val = lhs_value.get<ast::constant_value::uint>();
		auto const rhs_int_val = rhs_value.get<ast::constant_value::uint>();
		return ast::constant_value(lhs_int_val | rhs_int_val);
	}
	case ast::constant_value::boolean:
	{
		auto const lhs_bool_val = lhs_value.get<ast::constant_value::boolean>();
		auto const rhs_bool_val = rhs_value.get<ast::constant_value::boolean>();
		return ast::constant_value(lhs_bool_val || rhs_bool_val);
	}
	default:
		bz_unreachable;
	}
}

static ast::constant_value evaluate_binary_bit_left_shift(
	ast::expression const &original_expr,
	ast::expression const &lhs, ast::expression const &rhs,
	ctx::parse_context &context
)
{
	bz_assert(lhs.is<ast::constant_expression>());
	auto const &lhs_const_expr = lhs.get<ast::constant_expression>();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is<ast::constant_expression>());
	auto const &rhs_const_expr = rhs.get<ast::constant_expression>();
	auto const &rhs_value = rhs_const_expr.value;

	bz_assert(lhs_value.is<ast::constant_value::uint>());
	auto const lhs_int_val = lhs_value.get<ast::constant_value::uint>();
	bz_assert(rhs_value.is<ast::constant_value::uint>());
	auto const rhs_int_val = rhs_value.get<ast::constant_value::uint>();

	bz_assert(lhs_const_expr.type.is<ast::ts_base_type>());
	auto const lhs_type_kind = lhs_const_expr.type.get<ast::ts_base_type>().info->kind;

	auto const result = safe_binary_bit_left_shift(
		original_expr.src_tokens, original_expr.paren_level,
		lhs_int_val, rhs_int_val, lhs_type_kind,
		context
	);
	if (result.has_value())
	{
		return ast::constant_value(result.get());
	}
	else
	{
		return {};
	}
}

static ast::constant_value evaluate_binary_bit_right_shift(
	ast::expression const &original_expr,
	ast::expression const &lhs, ast::expression const &rhs,
	ctx::parse_context &context
)
{
	bz_assert(lhs.is<ast::constant_expression>());
	auto const &lhs_const_expr = lhs.get<ast::constant_expression>();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is<ast::constant_expression>());
	auto const &rhs_const_expr = rhs.get<ast::constant_expression>();
	auto const &rhs_value = rhs_const_expr.value;

	bz_assert(lhs_value.is<ast::constant_value::uint>());
	auto const lhs_int_val = lhs_value.get<ast::constant_value::uint>();
	bz_assert(rhs_value.is<ast::constant_value::uint>());
	auto const rhs_int_val = rhs_value.get<ast::constant_value::uint>();

	bz_assert(lhs_const_expr.type.is<ast::ts_base_type>());
	auto const lhs_type_kind = lhs_const_expr.type.get<ast::ts_base_type>().info->kind;

	auto const result = safe_binary_bit_right_shift(
		original_expr.src_tokens, original_expr.paren_level,
		lhs_int_val, rhs_int_val, lhs_type_kind,
		context
	);
	if (result.has_value())
	{
		return ast::constant_value(result.get());
	}
	else
	{
		return {};
	}
}

static ast::constant_value evaluate_binary_bool_and(
	[[maybe_unused]] ast::expression const &original_expr,
	ast::expression const &lhs, ast::expression const &rhs,
	[[maybe_unused]] ctx::parse_context &context
)
{
	bz_assert(lhs.is<ast::constant_expression>());
	auto const &lhs_const_expr = lhs.get<ast::constant_expression>();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is<ast::constant_expression>());
	auto const &rhs_const_expr = rhs.get<ast::constant_expression>();
	auto const &rhs_value = rhs_const_expr.value;

	bz_assert(lhs_value.is<ast::constant_value::boolean>());
	auto const lhs_bool_val = lhs_value.get<ast::constant_value::boolean>();
	bz_assert(rhs_value.is<ast::constant_value::boolean>());
	auto const rhs_bool_val = rhs_value.get<ast::constant_value::boolean>();

	// short-circuiting is handled elsewhere
	bz_assert(lhs_bool_val);
	return ast::constant_value(rhs_bool_val);
}

static ast::constant_value evaluate_binary_bool_xor(
	[[maybe_unused]] ast::expression const &original_expr,
	ast::expression const &lhs, ast::expression const &rhs,
	[[maybe_unused]] ctx::parse_context &context
)
{
	bz_assert(lhs.is<ast::constant_expression>());
	auto const &lhs_const_expr = lhs.get<ast::constant_expression>();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is<ast::constant_expression>());
	auto const &rhs_const_expr = rhs.get<ast::constant_expression>();
	auto const &rhs_value = rhs_const_expr.value;

	bz_assert(lhs_value.is<ast::constant_value::boolean>());
	auto const lhs_bool_val = lhs_value.get<ast::constant_value::boolean>();
	bz_assert(rhs_value.is<ast::constant_value::boolean>());
	auto const rhs_bool_val = rhs_value.get<ast::constant_value::boolean>();

	return ast::constant_value(lhs_bool_val != rhs_bool_val);
}

static ast::constant_value evaluate_binary_bool_or(
	[[maybe_unused]] ast::expression const &original_expr,
	ast::expression const &lhs, ast::expression const &rhs,
	[[maybe_unused]] ctx::parse_context &context
)
{
	bz_assert(lhs.is<ast::constant_expression>());
	auto const &lhs_const_expr = lhs.get<ast::constant_expression>();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is<ast::constant_expression>());
	auto const &rhs_const_expr = rhs.get<ast::constant_expression>();
	auto const &rhs_value = rhs_const_expr.value;

	bz_assert(lhs_value.is<ast::constant_value::boolean>());
	auto const lhs_bool_val = lhs_value.get<ast::constant_value::boolean>();
	bz_assert(rhs_value.is<ast::constant_value::boolean>());
	auto const rhs_bool_val = rhs_value.get<ast::constant_value::boolean>();

	// short-circuiting is handled elsewhere
	bz_assert(!lhs_bool_val);
	return ast::constant_value(rhs_bool_val);
}

static ast::constant_value evaluate_binary_comma(
	[[maybe_unused]] ast::expression const &original_expr,
	[[maybe_unused]] ast::expression const &lhs, ast::expression const &rhs,
	[[maybe_unused]] ctx::parse_context &context
)
{
	bz_assert(rhs.is<ast::constant_expression>());
	auto const &rhs_const_expr = rhs.get<ast::constant_expression>();
	auto const &rhs_value = rhs_const_expr.value;

	return rhs_value;
}


static ast::constant_value evaluate_binary_op(
	ast::expression const &original_expr,
	uint32_t op, ast::expression const &lhs, ast::expression const &rhs,
	ctx::parse_context &context
)
{
	switch (op)
	{
	case lex::token::plus:
		return evaluate_binary_plus(original_expr, lhs, rhs, context);
	case lex::token::minus:
		return evaluate_binary_minus(original_expr, lhs, rhs, context);
	case lex::token::multiply:
		return evaluate_binary_multiply(original_expr, lhs, rhs, context);
	case lex::token::divide:
		return evaluate_binary_divide(original_expr, lhs, rhs, context);
	case lex::token::modulo:
		return evaluate_binary_modulo(original_expr, lhs, rhs, context);
	case lex::token::equals:
		return evaluate_binary_equals(original_expr, lhs, rhs, context);
	case lex::token::not_equals:
		return evaluate_binary_not_equals(original_expr, lhs, rhs, context);
	case lex::token::less_than:
		return evaluate_binary_less_than(original_expr, lhs, rhs, context);
	case lex::token::less_than_eq:
		return evaluate_binary_less_than_eq(original_expr, lhs, rhs, context);
	case lex::token::greater_than:
		return evaluate_binary_greater_than(original_expr, lhs, rhs, context);
	case lex::token::greater_than_eq:
		return evaluate_binary_greater_than_eq(original_expr, lhs, rhs, context);
	case lex::token::bit_and:
		return evaluate_binary_bit_and(original_expr, lhs, rhs, context);
	case lex::token::bit_xor:
		return evaluate_binary_bit_xor(original_expr, lhs, rhs, context);
	case lex::token::bit_or:
		return evaluate_binary_bit_or(original_expr, lhs, rhs, context);
	case lex::token::bit_left_shift:
		return evaluate_binary_bit_left_shift(original_expr, lhs, rhs, context);
	case lex::token::bit_right_shift:
		return evaluate_binary_bit_right_shift(original_expr, lhs, rhs, context);
	case lex::token::bool_and:
		return evaluate_binary_bool_and(original_expr, lhs, rhs, context);
	case lex::token::bool_xor:
		return evaluate_binary_bool_xor(original_expr, lhs, rhs, context);
	case lex::token::bool_or:
		return evaluate_binary_bool_or(original_expr, lhs, rhs, context);
	case lex::token::comma:
		return evaluate_binary_comma(original_expr, lhs, rhs, context);
	default:
		return {};
	}
}

static ast::constant_value evaluate_subscript(
	ast::expr_subscript const &subscript_expr,
	ctx::parse_context &context
)
{
	bool is_consteval = true;
	auto const &base_type = ast::remove_const_or_consteval(subscript_expr.base.get_expr_type_and_kind().first);

	auto const &index = subscript_expr.index;
	uint64_t index_value = 0;

	if (index.is<ast::constant_expression>())
	{
		bz_assert(index.is<ast::constant_expression>());
		auto const &index_const_value = index.get<ast::constant_expression>().value;
		if (index_const_value.is<ast::constant_value::uint>())
		{
			index_value = index_const_value.get<ast::constant_value::uint>();
		}
		else
		{
			bz_assert(index_const_value.is<ast::constant_value::sint>());
			auto const signed_index_value = index_const_value.get<ast::constant_value::sint>();
			if (signed_index_value < 0)
			{
				is_consteval = false;
				if (index.paren_level < 2)
				{
					context.report_parenthesis_suppressed_warning(
						2 - index.paren_level, ctx::warning_kind::out_of_bounds_index,
						index.src_tokens,
						bz::format("negative index {} in subscript", signed_index_value)
					);
				}
			}
			else
			{
				index_value = static_cast<uint64_t>(signed_index_value);
			}
		}

		if (base_type.is<ast::ts_array>())
		{
			auto const &array_sizes = base_type.get<ast::ts_array>().sizes;
			bz_assert(!array_sizes.empty());
			if (index_value >= array_sizes.front())
			{
				is_consteval = false;
				if (index.paren_level < 2)
				{
					context.report_parenthesis_suppressed_warning(
						2 - index.paren_level, ctx::warning_kind::out_of_bounds_index,
						index.src_tokens,
						bz::format("index {} is out of bounds for an array of size {}", index_value, array_sizes.front())
					);
				}
			}
		}
		// tuple types shouldn't be handled, as index value checking
		// should already happen in built_in_operators
	}

	if (!is_consteval || !subscript_expr.base.has_consteval_succeeded())
	{
		return {};
	}

	bz_assert(subscript_expr.base.is<ast::constant_expression>());
	auto const &value = subscript_expr.base.get<ast::constant_expression>().value;
	if (base_type.is<ast::ts_array>())
	{
		bz_assert(value.is<ast::constant_value::array>());
		auto const &array_value = value.get<ast::constant_value::array>();
		bz_assert(index_value < array_value.size());
		return array_value[index_value];
	}
	else
	{
		// bz_assert(base_type.is<ast::ts_tuple>());
		// ^^^  this is not valid because base_type could also be empty if it's a tuple expression
		// e.g. [1, 2, 3][0]
		bz_assert(value.is<ast::constant_value::tuple>());
		auto const &tuple_value = value.get<ast::constant_value::tuple>();
		bz_assert(index_value < tuple_value.size());
		return tuple_value[index_value];
	}
}

static ast::constant_value evaluate_math_functions(
	ast::expression const &original_expr,
	ast::expr_function_call const &func_call,
	ctx::parse_context &context
)
{

	auto const get_float32 = [&func_call](size_t i = 0) {
		bz_assert(i < func_call.params.size());
		bz_assert(func_call.params[i].is<ast::constant_expression>());
		auto const &value = func_call.params[i].get<ast::constant_expression>().value;
		bz_assert(value.is<ast::constant_value::float32>());
		return value.get<ast::constant_value::float32>();
	};

	auto const get_float64 = [&func_call](size_t i = 0) {
		bz_assert(i < func_call.params.size());
		bz_assert(func_call.params[i].is<ast::constant_expression>());
		auto const &value = func_call.params[i].get<ast::constant_expression>().value;
		bz_assert(value.is<ast::constant_value::float64>());
		return value.get<ast::constant_value::float64>();
	};

	auto const paren_level = original_expr.paren_level;
	auto const &src_tokens = original_expr.src_tokens;

	switch (func_call.func_body->intrinsic_kind)
	{
#define def_case_default(func_name)                            \
case ast::function_body::func_name##_f32:                      \
    return ast::constant_value(std::func_name(get_float32())); \
case ast::function_body::func_name##_f64:                      \
    return ast::constant_value(std::func_name(get_float64()));

#define def_case_error(func_name, condition, message)                            \
case ast::function_body::func_name##_f32:                                        \
{                                                                                \
    auto const arg = get_float32();                                              \
    if (condition)                                                               \
    {                                                                            \
        if (paren_level < 2)                                                     \
        {                                                                        \
            context.report_parenthesis_suppressed_warning(                       \
                2 - paren_level, ctx::warning_kind::bad_float_math,              \
                src_tokens, bz::format("calling '" #func_name "' " message, arg) \
            );                                                                   \
        }                                                                        \
        return {};                                                               \
    }                                                                            \
    else                                                                         \
    {                                                                            \
        return ast::constant_value(std::func_name(arg));                         \
    }                                                                            \
}                                                                                \
case ast::function_body::func_name##_f64:                                        \
{                                                                                \
    auto const arg = get_float64();                                              \
    if (condition)                                                               \
    {                                                                            \
        if (paren_level < 2)                                                     \
        {                                                                        \
            context.report_parenthesis_suppressed_warning(                       \
                2 - paren_level, ctx::warning_kind::bad_float_math,              \
                src_tokens, bz::format("calling '" #func_name "' " message, arg) \
            );                                                                   \
        }                                                                        \
        return {};                                                               \
    }                                                                            \
    else                                                                         \
    {                                                                            \
        return ast::constant_value(std::func_name(arg));                         \
    }                                                                            \
}

	// ==== exponential and logarithmic functions ====
	// exponential functions can take any value
	def_case_default(exp)
	def_case_default(exp2)
	def_case_default(expm1)
	// log functions can't take negative arguments, except for log1p, which can't take numbers < -1.0
	def_case_error(log,   arg < 0,  "with a negative value, {}")
	def_case_error(log10, arg < 0,  "with a negative value, {}")
	def_case_error(log2,  arg < 0,  "with a negative value, {}")
	def_case_error(log1p, arg < -1, "with a value less than -1, {}")

	// ==== power functions ====
	case ast::function_body::pow_f32:
		return ast::constant_value(std::pow(get_float32(0), get_float32(1)));
	case ast::function_body::pow_f64:
		return ast::constant_value(std::pow(get_float64(0), get_float64(1)));
	def_case_error(sqrt, arg < 0, "with a negative value, {}")
	def_case_default(cbrt)
	case ast::function_body::hypot_f32:
		return ast::constant_value(std::hypot(get_float32(0), get_float32(1)));
	case ast::function_body::hypot_f64:
		return ast::constant_value(std::hypot(get_float64(0), get_float64(1)));

	// ==== trigonometric functions ====
	def_case_default(sin)
	def_case_default(cos)
	def_case_default(tan)
	def_case_error(asin, arg < -1 || arg > 1, "with a value not in the range [-1, 1], {}")
	def_case_error(acos, arg < -1 || arg > 1, "with a value not in the range [-1, 1], {}")
	def_case_default(atan)
	case ast::function_body::atan2_f32:
		return ast::constant_value(std::atan2(get_float32(0), get_float32(1)));
	case ast::function_body::atan2_f64:
		return ast::constant_value(std::atan2(get_float64(0), get_float64(1)));

	// ==== hyperbolic functions ====
	def_case_default(sinh)
	def_case_default(cosh)
	def_case_default(tanh)
	def_case_default(asinh)
	def_case_error(acosh, arg < 1, "with a value less than 1, {}")
	def_case_error(atanh, arg < -1 || arg > 1, "with a value not in the range [-1, 1], {}")

	// ==== error and gamma functions ====
	def_case_default(erf)
	def_case_default(erfc)
	def_case_error(tgamma, arg < 0, "with a negative value, {}")
	def_case_error(lgamma, arg < 0, "with a negative value, {}")

	default:
		bz_unreachable;

#undef def_case_default
#undef def_case_error
	}
}

static ast::constant_value evaluate_function_call(
	ast::expression const &original_expr,
	ast::expr_function_call const &func_call,
	bool force_evaluate,
	ctx::parse_context &context
)
{
	if (func_call.func_body->is_intrinsic())
	{
		switch (func_call.func_body->intrinsic_kind)
		{
		static_assert(ast::function_body::_builtin_last - ast::function_body::_builtin_first == 80);
		case ast::function_body::builtin_str_eq:
		{
			bz_assert(func_call.params.size() == 2);
			bz_assert(func_call.params[0].is<ast::constant_expression>());
			auto const &lhs_value = func_call.params[0].get<ast::constant_expression>().value;
			bz_assert(func_call.params[1].is<ast::constant_expression>());
			auto const &rhs_value = func_call.params[1].get<ast::constant_expression>().value;
			bz_assert(lhs_value.is<ast::constant_value::string>());
			bz_assert(rhs_value.is<ast::constant_value::string>());
			return ast::constant_value(
				lhs_value.get<ast::constant_value::string>()
				== rhs_value.get<ast::constant_value::string>()
			);
		}
		case ast::function_body::builtin_str_neq:
		{
			bz_assert(func_call.params.size() == 2);
			bz_assert(func_call.params[0].is<ast::constant_expression>());
			auto const &lhs_value = func_call.params[0].get<ast::constant_expression>().value;
			bz_assert(func_call.params[1].is<ast::constant_expression>());
			auto const &rhs_value = func_call.params[1].get<ast::constant_expression>().value;
			bz_assert(lhs_value.is<ast::constant_value::string>());
			bz_assert(rhs_value.is<ast::constant_value::string>());
			return ast::constant_value(
				lhs_value.get<ast::constant_value::string>()
				!= rhs_value.get<ast::constant_value::string>()
			);
		}
		case ast::function_body::builtin_str_length:
		{
			bz_assert(func_call.params.size() == 1);
			bz_assert(func_call.params[0].is<ast::constant_expression>());
			auto const &str_value = func_call.params[0].get<ast::constant_expression>().value;
			bz_assert(str_value.is<ast::constant_value::string>());
			return ast::constant_value(str_value.get<ast::constant_value::string>().length());
		}

		case ast::function_body::builtin_str_begin_ptr:
		case ast::function_body::builtin_str_end_ptr:
			return {};
		case ast::function_body::builtin_str_size:
		{
			bz_assert(func_call.params.size() == 1);
			bz_assert(func_call.params[0].is<ast::constant_expression>());
			auto const &str_value = func_call.params[0].get<ast::constant_expression>().value;
			bz_assert(str_value.is<ast::constant_value::string>());
			return ast::constant_value(str_value.get<ast::constant_value::string>().size());
		}
		case ast::function_body::builtin_str_from_ptrs:
			return {};

		case ast::function_body::builtin_slice_begin_ptr:
		case ast::function_body::builtin_slice_begin_const_ptr:
		case ast::function_body::builtin_slice_end_ptr:
		case ast::function_body::builtin_slice_end_const_ptr:
		case ast::function_body::builtin_slice_size:
		case ast::function_body::builtin_slice_from_ptrs:
		case ast::function_body::builtin_slice_from_const_ptrs:
			return {};

		case ast::function_body::builtin_pointer_cast:
		{
			bz_assert(func_call.params[0].is_typename());
			bz_assert(func_call.params[1].is<ast::constant_expression>());
			bz_assert(func_call.params[1].get<ast::constant_expression>().value.is<ast::constant_value::null>());
			return func_call.params[1].get<ast::constant_expression>().value;
		}
		case ast::function_body::builtin_pointer_to_int:
		case ast::function_body::builtin_int_to_pointer:
			return {};

		// builtins end here

		case ast::function_body::print_stdout:
		case ast::function_body::println_stdout:
		case ast::function_body::print_stderr:
		case ast::function_body::println_stderr:
			return {};

		case ast::function_body::memcpy:
		case ast::function_body::memmove:
		case ast::function_body::memset:
			return {};

		case ast::function_body::exp_f32:   case ast::function_body::exp_f64:
		case ast::function_body::exp2_f32:  case ast::function_body::exp2_f64:
		case ast::function_body::expm1_f32: case ast::function_body::expm1_f64:
		case ast::function_body::log_f32:   case ast::function_body::log_f64:
		case ast::function_body::log10_f32: case ast::function_body::log10_f64:
		case ast::function_body::log2_f32:  case ast::function_body::log2_f64:
		case ast::function_body::log1p_f32: case ast::function_body::log1p_f64:
			[[fallthrough]];
		case ast::function_body::pow_f32:   case ast::function_body::pow_f64:
		case ast::function_body::sqrt_f32:  case ast::function_body::sqrt_f64:
		case ast::function_body::cbrt_f32:  case ast::function_body::cbrt_f64:
		case ast::function_body::hypot_f32: case ast::function_body::hypot_f64:
		case ast::function_body::sin_f32:   case ast::function_body::sin_f64:
		case ast::function_body::cos_f32:   case ast::function_body::cos_f64:
		case ast::function_body::tan_f32:   case ast::function_body::tan_f64:
		case ast::function_body::asin_f32:  case ast::function_body::asin_f64:
		case ast::function_body::acos_f32:  case ast::function_body::acos_f64:
		case ast::function_body::atan_f32:  case ast::function_body::atan_f64:
		case ast::function_body::atan2_f32: case ast::function_body::atan2_f64:
			[[fallthrough]];
		case ast::function_body::sinh_f32:  case ast::function_body::sinh_f64:
		case ast::function_body::cosh_f32:  case ast::function_body::cosh_f64:
		case ast::function_body::tanh_f32:  case ast::function_body::tanh_f64:
		case ast::function_body::asinh_f32: case ast::function_body::asinh_f64:
		case ast::function_body::acosh_f32: case ast::function_body::acosh_f64:
		case ast::function_body::atanh_f32: case ast::function_body::atanh_f64:
			[[fallthrough]];
		case ast::function_body::erf_f32:    case ast::function_body::erf_f64:
		case ast::function_body::erfc_f32:   case ast::function_body::erfc_f64:
		case ast::function_body::tgamma_f32: case ast::function_body::tgamma_f64:
		case ast::function_body::lgamma_f32: case ast::function_body::lgamma_f64:
			return evaluate_math_functions(original_expr, func_call, context);

		default:
			bz_unreachable;
		}
	}
	else if (force_evaluate)
	{
		auto const body = func_call.func_body;
		auto const params = bz::zip(func_call.params, body->params)
			.filter([](auto const &pair) { return !ast::is_generic_parameter(pair.second); })
			.transform([](auto const &pair) { return pair.first.template get<ast::constant_expression>().value; })
			.collect();
		return context.execute_function(original_expr.src_tokens, body, params);
	}
	else
	{
		return {};
	}
}

static ast::constant_value evaluate_cast(
	ast::expression const &original_expr,
	ast::expr_cast const &subscript_expr,
	ctx::parse_context &context
)
{
	bz_assert(subscript_expr.expr.is<ast::constant_expression>());
	auto const dest_type = ast::remove_const_or_consteval(subscript_expr.type);
	if (!dest_type.is<ast::ts_base_type>())
	{
		return {};
	}

	auto const dest_kind = dest_type.get<ast::ts_base_type>().info->kind;
	auto const paren_level = original_expr.paren_level;
	auto const &value = subscript_expr.expr.get<ast::constant_expression>().value;
	auto const &src_tokens = original_expr.src_tokens;

	switch (dest_kind)
	{
	case ast::type_info::int8_:
	case ast::type_info::int16_:
	case ast::type_info::int32_:
	case ast::type_info::int64_:
	{
		switch (value.kind())
		{
		case ast::constant_value::sint:
		{
			using T = std::tuple<bz::u8string_view, int64_t, int64_t, int64_t>;
			auto const int_val = value.get<ast::constant_value::sint>();
			auto const [type_name, min_val, max_val, result] =
				dest_kind == ast::type_info::int8_  ? T{ "int8",  std::numeric_limits<int8_t> ::min(), std::numeric_limits<int8_t> ::max(), static_cast<int8_t> (int_val) } :
				dest_kind == ast::type_info::int16_ ? T{ "int16", std::numeric_limits<int16_t>::min(), std::numeric_limits<int16_t>::max(), static_cast<int16_t>(int_val) } :
				dest_kind == ast::type_info::int32_ ? T{ "int32", std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max(), static_cast<int32_t>(int_val) } :
				T{ "int64", std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max(), static_cast<int64_t>(int_val) };
			if (paren_level < 2 && (int_val < min_val || int_val > max_val))
			{
				context.report_parenthesis_suppressed_warning(
					2 - paren_level, ctx::warning_kind::int_overflow,
					src_tokens,
					bz::format("overflow in constant expression '{} as {}' results in {}", int_val, type_name, result)
				);
			}
			return ast::constant_value(result);
		}
		case ast::constant_value::uint:
		{
			using T = std::tuple<bz::u8string_view, int64_t, int64_t>;
			auto const int_val = value.get<ast::constant_value::uint>();
			auto const [type_name, max_val, result] =
				dest_kind == ast::type_info::int8_  ? T{ "int8",  std::numeric_limits<int8_t> ::max(), static_cast<int8_t> (int_val) } :
				dest_kind == ast::type_info::int16_ ? T{ "int16", std::numeric_limits<int16_t>::max(), static_cast<int16_t>(int_val) } :
				dest_kind == ast::type_info::int32_ ? T{ "int32", std::numeric_limits<int32_t>::max(), static_cast<int32_t>(int_val) } :
				T{ "int64", std::numeric_limits<int64_t>::max(), static_cast<int64_t>(int_val) };
			if (paren_level < 2 && int_val > static_cast<uint64_t>(max_val))
			{
				context.report_parenthesis_suppressed_warning(
					2 - paren_level, ctx::warning_kind::int_overflow,
					src_tokens,
					bz::format("overflow in constant expression '{} as {}' results in {}", int_val, type_name, result)
				);
			}
			return ast::constant_value(result);
		}
		case ast::constant_value::float32:
		{
			auto const float_val = value.get<ast::constant_value::float32>();
			auto const result =
				dest_kind == ast::type_info::int8_  ? static_cast<int8_t> (float_val) :
				dest_kind == ast::type_info::int16_ ? static_cast<int16_t>(float_val) :
				dest_kind == ast::type_info::int32_ ? static_cast<int32_t>(float_val) :
				static_cast<int64_t>(float_val);
			return ast::constant_value(result);
		}
		case ast::constant_value::float64:
		{
			auto const float_val = value.get<ast::constant_value::float64>();
			auto const result =
				dest_kind == ast::type_info::int8_  ? static_cast<int8_t> (float_val) :
				dest_kind == ast::type_info::int16_ ? static_cast<int16_t>(float_val) :
				dest_kind == ast::type_info::int32_ ? static_cast<int32_t>(float_val) :
				static_cast<int64_t>(float_val);
			return ast::constant_value(result);
		}
		case ast::constant_value::u8char:
			// no overflow possible in constant expressions
			return ast::constant_value(static_cast<int64_t>(value.get<ast::constant_value::u8char>()));
		default:
			bz_unreachable;
		}
	}

	case ast::type_info::uint8_:
	case ast::type_info::uint16_:
	case ast::type_info::uint32_:
	case ast::type_info::uint64_:
	{
		switch (value.kind())
		{
		case ast::constant_value::sint:
		{
			using T = std::tuple<bz::u8string_view, uint64_t, uint64_t>;
			auto const int_val = value.get<ast::constant_value::sint>();
			auto const [type_name, max_val, result] =
				dest_kind == ast::type_info::uint8_  ? T{ "uint8",  std::numeric_limits<uint8_t> ::max(), static_cast<uint8_t> (int_val) } :
				dest_kind == ast::type_info::uint16_ ? T{ "uint16", std::numeric_limits<uint16_t>::max(), static_cast<uint16_t>(int_val) } :
				dest_kind == ast::type_info::uint32_ ? T{ "uint32", std::numeric_limits<uint32_t>::max(), static_cast<uint32_t>(int_val) } :
				T{ "uint64", std::numeric_limits<uint64_t>::max(), static_cast<uint64_t>(int_val) };
			if (paren_level < 2 && (int_val < 0 || static_cast<uint64_t>(int_val) > max_val))
			{
				context.report_parenthesis_suppressed_warning(
					2 - paren_level, ctx::warning_kind::int_overflow,
					src_tokens,
					bz::format("overflow in constant expression '{} as {}' results in {}", int_val, type_name, result)
				);
			}
			return ast::constant_value(result);
		}
		case ast::constant_value::uint:
		{
			using T = std::tuple<bz::u8string_view, uint64_t, uint64_t>;
			auto const int_val = value.get<ast::constant_value::uint>();
			auto const [type_name, max_val, result] =
				dest_kind == ast::type_info::uint8_  ? T{ "uint8",  std::numeric_limits<uint8_t> ::max(), static_cast<uint8_t> (int_val) } :
				dest_kind == ast::type_info::uint16_ ? T{ "uint16", std::numeric_limits<uint16_t>::max(), static_cast<uint16_t>(int_val) } :
				dest_kind == ast::type_info::uint32_ ? T{ "uint32", std::numeric_limits<uint32_t>::max(), static_cast<uint32_t>(int_val) } :
				T{ "uint64", std::numeric_limits<uint64_t>::max(), static_cast<uint64_t>(int_val) };
			if (paren_level < 2 && int_val > max_val)
			{
				context.report_parenthesis_suppressed_warning(
					2 - paren_level, ctx::warning_kind::int_overflow,
					src_tokens,
					bz::format("overflow in constant expression '{} as {}' results in {}", int_val, type_name, result)
				);
			}
			return ast::constant_value(result);
		}
		case ast::constant_value::float32:
		{
			auto const float_val = value.get<ast::constant_value::float32>();
			auto const result =
				dest_kind == ast::type_info::uint8_  ? static_cast<uint8_t> (float_val) :
				dest_kind == ast::type_info::uint16_ ? static_cast<uint16_t>(float_val) :
				dest_kind == ast::type_info::uint32_ ? static_cast<uint32_t>(float_val) :
				static_cast<uint64_t>(float_val);
			return ast::constant_value(result);
		}
		case ast::constant_value::float64:
		{
			auto const float_val = value.get<ast::constant_value::float64>();
			auto const result =
				dest_kind == ast::type_info::uint8_  ? static_cast<uint8_t> (float_val) :
				dest_kind == ast::type_info::uint16_ ? static_cast<uint16_t>(float_val) :
				dest_kind == ast::type_info::uint32_ ? static_cast<uint32_t>(float_val) :
				static_cast<uint64_t>(float_val);
			return ast::constant_value(result);
		}
		case ast::constant_value::u8char:
			// no overflow possible in constant expressions
			return ast::constant_value(static_cast<uint64_t>(value.get<ast::constant_value::u8char>()));
		default:
			bz_unreachable;
		}
	}

	case ast::type_info::float32_:
		switch (value.kind())
		{
		case ast::constant_value::sint:
			return ast::constant_value(static_cast<float32_t>(value.get<ast::constant_value::sint>()));
		case ast::constant_value::uint:
			return ast::constant_value(static_cast<float32_t>(value.get<ast::constant_value::uint>()));
		case ast::constant_value::float32:
			return ast::constant_value(static_cast<float32_t>(value.get<ast::constant_value::float32>()));
		case ast::constant_value::float64:
			return ast::constant_value(static_cast<float32_t>(value.get<ast::constant_value::float64>()));
		default:
			bz_unreachable;
		}
	case ast::type_info::float64_:
		switch (value.kind())
		{
		case ast::constant_value::sint:
			return ast::constant_value(static_cast<float64_t>(value.get<ast::constant_value::sint>()));
		case ast::constant_value::uint:
			return ast::constant_value(static_cast<float64_t>(value.get<ast::constant_value::uint>()));
		case ast::constant_value::float32:
			return ast::constant_value(static_cast<float64_t>(value.get<ast::constant_value::float32>()));
		case ast::constant_value::float64:
			return ast::constant_value(static_cast<float64_t>(value.get<ast::constant_value::float64>()));
		default:
			bz_unreachable;
		}
	case ast::type_info::char_:
		switch (value.kind())
		{
		case ast::constant_value::sint:
		{
			auto const result = static_cast<bz::u8char>(value.get<ast::constant_value::sint>());
			if (!bz::is_valid_unicode_value(result))
			{
				if (paren_level < 2)
				{
					context.report_parenthesis_suppressed_warning(
						2 - paren_level, ctx::warning_kind::invalid_unicode,
						src_tokens,
						bz::format("the result of U+{:04X} is not a valid unicode codepoint", result)
					);
				}
				return {};
			}
			return ast::constant_value(result);
		}
		case ast::constant_value::uint:
		{
			auto const result = static_cast<bz::u8char>(value.get<ast::constant_value::uint>());
			if (!bz::is_valid_unicode_value(result))
			{
				if (paren_level < 2)
				{
					context.report_parenthesis_suppressed_warning(
						2 - paren_level, ctx::warning_kind::invalid_unicode,
						src_tokens,
						bz::format("the result of U+{:04X} is not a valid unicode codepoint", result)
					);
				}
				return {};
			}
			return ast::constant_value(result);
		}
		default:
			bz_unreachable;
		}
	case ast::type_info::str_:
	case ast::type_info::bool_:
	case ast::type_info::null_t_:
	default:
		bz_unreachable;
	}
	return {};
}

static ast::constant_value guaranteed_evaluate_expr(
	ast::expression &expr,
	ctx::parse_context &context
)
{
	return expr.get_expr().visit(bz::overload{
		[](ast::expr_identifier &) -> ast::constant_value {
			// identifiers are only constant expressions if they are a consteval
			// variable, which is handled in parse_context::make_identifier_expr (or something similar)
			return {};
		},
		[](ast::expr_literal &) -> ast::constant_value {
			// literals are always constant expressions
			bz_unreachable;
		},
		[&context](ast::expr_tuple &tuple) -> ast::constant_value {
			bool is_consteval = true;
			for (auto &elem : tuple.elems)
			{
				consteval_guaranteed(elem, context);
				is_consteval = is_consteval && elem.has_consteval_succeeded();
			}
			if (!is_consteval)
			{
				return {};
			}

			ast::constant_value result;
			result.emplace<ast::constant_value::tuple>();
			auto &elem_values = result.get<ast::constant_value::tuple>();
			elem_values.reserve(tuple.elems.size());
			for (auto &elem : tuple.elems)
			{
				bz_assert(elem.is<ast::constant_expression>());
				elem_values.emplace_back(elem.get<ast::constant_expression>().value);
			}
			return result;
		},
		[&expr, &context](ast::expr_unary_op &unary_op) -> ast::constant_value {
			consteval_guaranteed(unary_op.expr, context);
			if (!unary_op.expr.has_consteval_succeeded())
			{
				return {};
			}

			return evaluate_unary_op(expr, unary_op.op->kind, unary_op.expr, context);
		},
		[&expr, &context](ast::expr_binary_op &binary_op) -> ast::constant_value {
			consteval_guaranteed(binary_op.lhs, context);
			consteval_guaranteed(binary_op.rhs, context);

			// special case for bool_and and bool_or shortcircuiting
			if (binary_op.lhs.has_consteval_succeeded())
			{
				auto const op = binary_op.op->kind;
				if (op == lex::token::bool_and)
				{
					bz_assert(binary_op.lhs.is<ast::constant_expression>());
					auto const &lhs_value = binary_op.lhs.get<ast::constant_expression>().value;
					bz_assert(lhs_value.is<ast::constant_value::boolean>());
					auto const lhs_bool_val = lhs_value.get<ast::constant_value::boolean>();
					if (!lhs_bool_val)
					{
						return ast::constant_value(false);
					}
				}
				else if (op == lex::token::bool_or)
				{
					bz_assert(binary_op.lhs.is<ast::constant_expression>());
					auto const &lhs_value = binary_op.lhs.get<ast::constant_expression>().value;
					bz_assert(lhs_value.is<ast::constant_value::boolean>());
					auto const lhs_bool_val = lhs_value.get<ast::constant_value::boolean>();
					if (lhs_bool_val)
					{
						return ast::constant_value(true);
					}
				}
			}

			if (binary_op.lhs.has_consteval_succeeded() && binary_op.rhs.has_consteval_succeeded())
			{
				return evaluate_binary_op(expr, binary_op.op->kind, binary_op.lhs, binary_op.rhs, context);
			}
			else
			{
				return {};
			}
		},
		[&context](ast::expr_subscript &subscript_expr) -> ast::constant_value {
			consteval_guaranteed(subscript_expr.base, context);
			consteval_guaranteed(subscript_expr.index, context);

			return evaluate_subscript(subscript_expr, context);
		},
		[&expr, &context](ast::expr_function_call &func_call) -> ast::constant_value {
			bool is_consteval = true;
			for (auto &param : func_call.params)
			{
				consteval_guaranteed(param, context);
				if (!param.has_consteval_succeeded())
				{
					is_consteval = false;
				}
			}
			if (!is_consteval)
			{
				return {};
			}
			else
			{
				return evaluate_function_call(expr, func_call, false, context);
			}
		},
		[&expr, &context](ast::expr_cast &cast_expr) -> ast::constant_value {
			consteval_guaranteed(cast_expr.expr, context);
			if (cast_expr.expr.has_consteval_succeeded())
			{
				return evaluate_cast(expr, cast_expr, context);
			}
			else
			{
				return {};
			}
		},
		[&context](ast::expr_struct_init &struct_init_expr) -> ast::constant_value {
			bool is_consteval = true;
			for (auto &expr : struct_init_expr.exprs)
			{
				consteval_guaranteed(expr, context);
				is_consteval = is_consteval && expr.has_consteval_succeeded();
			}
			if (!is_consteval)
			{
				return {};
			}

			ast::constant_value result{};
			result.emplace<ast::constant_value::aggregate>();
			auto &aggregate = result.get<ast::constant_value::aggregate>();
			for (auto const &expr : struct_init_expr.exprs)
			{
				aggregate.emplace_back(expr.get<ast::constant_expression>().value);
			}
			return result;
		},
		[&context](ast::expr_member_access &member_access_expr) -> ast::constant_value {
			consteval_guaranteed(member_access_expr.base, context);
			if (member_access_expr.base.has_consteval_succeeded())
			{
				bz_assert(member_access_expr.base.get<ast::constant_expression>().value.is<ast::constant_value::aggregate>());
				return member_access_expr.base
					.get<ast::constant_expression>().value
					.get<ast::constant_value::aggregate>()[member_access_expr.index];
			}
			else
			{
				return {};
			}
		},
		[](ast::expr_compound &) -> ast::constant_value {
			return {};
		},
		[&context](ast::expr_if &if_expr) -> ast::constant_value {
			consteval_guaranteed(if_expr.condition, context);
			consteval_guaranteed(if_expr.then_block, context);
			consteval_guaranteed(if_expr.else_block, context);
			if (if_expr.condition.has_consteval_succeeded())
			{
				bz_assert(if_expr.condition.is<ast::constant_expression>());
				bz_assert(if_expr.condition.get<ast::constant_expression>().value.is<ast::constant_value::boolean>());
				auto const condition_value = if_expr.condition
					.get<ast::constant_expression>().value
					.get<ast::constant_value::boolean>();
				if (condition_value)
				{
					if (if_expr.then_block.has_consteval_succeeded())
					{
						return if_expr.then_block.get<ast::constant_expression>().value;
					}
					else
					{
						return {};
					}
				}
				else
				{
					if (if_expr.else_block.has_consteval_succeeded())
					{
						return if_expr.else_block.get<ast::constant_expression>().value;
					}
					else
					{
						return {};
					}
				}
			}
			else
			{
				return {};
			}
		},
	});
}

static ast::constant_value try_evaluate_expr(
	ast::expression &expr,
	ctx::parse_context &context
)
{
	return expr.get_expr().visit(bz::overload{
		[](ast::expr_identifier &) -> ast::constant_value {
			// identifiers are only constant expressions if they are a consteval
			// variable, which is handled in parse_context::make_identifier_expr (or something similar)
			return {};
		},
		[](ast::expr_literal &) -> ast::constant_value {
			// literals are always constant expressions
			bz_unreachable;
		},
		[&context](ast::expr_tuple &tuple) -> ast::constant_value {
			bool is_consteval = true;
			for (auto &elem : tuple.elems)
			{
				consteval_try(elem, context);
				is_consteval = is_consteval && elem.has_consteval_succeeded();
			}
			if (!is_consteval)
			{
				return {};
			}

			ast::constant_value result;
			result.emplace<ast::constant_value::tuple>();
			auto &elem_values = result.get<ast::constant_value::tuple>();
			elem_values.reserve(tuple.elems.size());
			for (auto &elem : tuple.elems)
			{
				bz_assert(elem.is<ast::constant_expression>());
				elem_values.emplace_back(elem.get<ast::constant_expression>().value);
			}
			return result;
		},
		[&expr, &context](ast::expr_unary_op &unary_op) -> ast::constant_value {
			consteval_try(unary_op.expr, context);
			if (unary_op.expr.has_consteval_succeeded())
			{
				return evaluate_unary_op(expr, unary_op.op->kind, unary_op.expr, context);
			}
			else
			{
				return {};
			}
		},
		[&expr, &context](ast::expr_binary_op &binary_op) -> ast::constant_value {
			consteval_try(binary_op.lhs, context);
			consteval_try(binary_op.rhs, context);

			// special case for bool_and and bool_or shortcircuiting
			if (binary_op.lhs.has_consteval_succeeded())
			{
				auto const op = binary_op.op->kind;
				if (op == lex::token::bool_and)
				{
					bz_assert(binary_op.lhs.is<ast::constant_expression>());
					auto const &lhs_value = binary_op.lhs.get<ast::constant_expression>().value;
					bz_assert(lhs_value.is<ast::constant_value::boolean>());
					auto const lhs_bool_val = lhs_value.get<ast::constant_value::boolean>();
					if (!lhs_bool_val)
					{
						return ast::constant_value(false);
					}
				}
				else if (op == lex::token::bool_or)
				{
					bz_assert(binary_op.lhs.is<ast::constant_expression>());
					auto const &lhs_value = binary_op.lhs.get<ast::constant_expression>().value;
					bz_assert(lhs_value.is<ast::constant_value::boolean>());
					auto const lhs_bool_val = lhs_value.get<ast::constant_value::boolean>();
					if (lhs_bool_val)
					{
						return ast::constant_value(true);
					}
				}
			}

			if (binary_op.lhs.has_consteval_succeeded() && binary_op.rhs.has_consteval_succeeded())
			{
				return evaluate_binary_op(expr, binary_op.op->kind, binary_op.lhs, binary_op.rhs, context);
			}
			else
			{
				return {};
			}
		},
		[&context](ast::expr_subscript &subscript_expr) -> ast::constant_value {
			consteval_try(subscript_expr.base, context);
			consteval_try(subscript_expr.index, context);

			return evaluate_subscript(subscript_expr, context);
		},
		[&expr, &context](ast::expr_function_call &func_call) -> ast::constant_value {
			bool is_consteval = true;
			for (auto &param : func_call.params)
			{
				consteval_try(param, context);
				if (param.has_consteval_failed())
				{
					is_consteval = false;
				}
			}
			if (!is_consteval)
			{
				return {};
			}
			else
			{
				return evaluate_function_call(expr, func_call, true, context);
			}
		},
		[&expr, &context](ast::expr_cast &cast_expr) -> ast::constant_value {
			consteval_try(cast_expr.expr, context);
			if (cast_expr.expr.has_consteval_succeeded())
			{
				return evaluate_cast(expr, cast_expr, context);
			}
			else
			{
				return {};
			}
		},
		[&context](ast::expr_struct_init &struct_init_expr) -> ast::constant_value {
			bool is_consteval = true;
			for (auto &expr : struct_init_expr.exprs)
			{
				consteval_try(expr, context);
				is_consteval = is_consteval && expr.has_consteval_succeeded();
			}
			if (!is_consteval)
			{
				return {};
			}

			ast::constant_value result{};
			result.emplace<ast::constant_value::aggregate>();
			auto &aggregate = result.get<ast::constant_value::aggregate>();
			for (auto const &expr : struct_init_expr.exprs)
			{
				aggregate.emplace_back(expr.get<ast::constant_expression>().value);
			}
			return result;
		},
		[&context](ast::expr_member_access &member_access_expr) -> ast::constant_value {
			consteval_try(member_access_expr.base, context);
			if (member_access_expr.base.has_consteval_succeeded())
			{
				return member_access_expr.base
					.get<ast::constant_expression>().value
					.get<ast::constant_value::aggregate>()[member_access_expr.index];
			}
			else
			{
				return {};
			}
		},
		[](ast::expr_compound &) -> ast::constant_value {
			return {};
		},
		[&context](ast::expr_if &if_expr) -> ast::constant_value {
			consteval_try(if_expr.condition, context);
			consteval_try(if_expr.then_block, context);
			consteval_try(if_expr.else_block, context);
			if (if_expr.condition.has_consteval_succeeded())
			{
				bz_assert(if_expr.condition.is<ast::constant_expression>());
				bz_assert(if_expr.condition.get<ast::constant_expression>().value.is<ast::constant_value::boolean>());
				auto const condition_value = if_expr.condition
					.get<ast::constant_expression>().value
					.get<ast::constant_value::boolean>();
				if (condition_value)
				{
					if (if_expr.then_block.has_consteval_succeeded())
					{
						return if_expr.then_block.get<ast::constant_expression>().value;
					}
					else
					{
						return {};
					}
				}
				else
				{
					if (if_expr.else_block.has_consteval_succeeded())
					{
						return if_expr.else_block.get<ast::constant_expression>().value;
					}
					else
					{
						return {};
					}
				}
			}
			else
			{
				return {};
			}
		},
	});
}

void consteval_guaranteed(ast::expression &expr, ctx::parse_context &context)
{
	if (expr.is<ast::constant_expression>())
	{
		expr.consteval_state = ast::expression::consteval_succeeded;
		return;
	}
	else if (
		!expr.is<ast::dynamic_expression>()
		|| expr.consteval_state != ast::expression::consteval_never_tried
	)
	{
		expr.consteval_state = ast::expression::consteval_failed;
		return;
	}

	auto const value = guaranteed_evaluate_expr(expr, context);
	if (value.is_null())
	{
		return;
	}
	else
	{
		auto &dyn_expr  = expr.get<ast::dynamic_expression>();
		auto type       = std::move(dyn_expr.type);
		auto inner_expr = std::move(dyn_expr.expr);
		expr.emplace<ast::constant_expression>(
			dyn_expr.kind, std::move(type),
			value, std::move(inner_expr)
		);
		expr.consteval_state = ast::expression::consteval_succeeded;
		return;
	}
}

void consteval_try(ast::expression &expr, ctx::parse_context &context)
{
	if (expr.is<ast::constant_expression>())
	{
		expr.consteval_state = ast::expression::consteval_succeeded;
		return;
	}
	else if (
		!expr.is<ast::dynamic_expression>()
		|| expr.consteval_state != ast::expression::consteval_never_tried
	)
	{
		expr.consteval_state = ast::expression::consteval_failed;
		return;
	}

	auto const value = try_evaluate_expr(expr, context);
	if (value.is_null())
	{
		expr.consteval_state = ast::expression::consteval_failed;
		return;
	}
	else
	{
		auto &dyn_expr  = expr.get<ast::dynamic_expression>();
		auto type       = std::move(dyn_expr.type);
		auto inner_expr = std::move(dyn_expr.expr);
		expr.emplace<ast::constant_expression>(
			dyn_expr.kind, std::move(type),
			value, std::move(inner_expr)
		);
		expr.consteval_state = ast::expression::consteval_succeeded;
		return;
	}
}


static void get_consteval_fail_notes_helper(ast::expression const &expr, bz::vector<ctx::note> &notes)
{
	if (expr.is_null())
	{
		return;
	}
	bz_assert(expr.has_consteval_failed());
	bz_assert(expr.is<ast::dynamic_expression>());
	expr.get_expr().visit(bz::overload{
		[&expr, &notes](ast::expr_identifier const &) {
			notes.emplace_back(ctx::parse_context::make_note(
				expr.src_tokens, "subexpression is not a constant expression"
			));
		},
		[](ast::expr_literal const &) {
			// literals are always constant expressions
			bz_unreachable;
		},
		[&notes](ast::expr_tuple const &tuple) {
			bool any_failed = false;
			for (auto const &elem : tuple.elems)
			{
				if (elem.has_consteval_failed())
				{
					any_failed = true;
					get_consteval_fail_notes_helper(elem, notes);
				}
			}
			bz_assert(any_failed);
		},
		[&expr, &notes](ast::expr_unary_op const &unary_op) {
			if (unary_op.expr.has_consteval_succeeded())
			{
				notes.emplace_back(ctx::parse_context::make_note(
					expr.src_tokens,
					bz::format(
						"subexpression '{}{}' is not a constant expression",
						unary_op.op->value,
						ast::get_value_string(unary_op.expr.get<ast::constant_expression>().value)
					)
				));
			}
			else
			{
				get_consteval_fail_notes_helper(unary_op.expr, notes);
			}
		},
		[&expr, &notes](ast::expr_binary_op const &binary_op) {
			if (
				binary_op.lhs.has_consteval_succeeded()
				&& binary_op.rhs.has_consteval_succeeded()
			)
			{
				notes.emplace_back(ctx::parse_context::make_note(
					expr.src_tokens,
					bz::format(
						"subexpression '{} {} {}' is not a constant expression",
						ast::get_value_string(binary_op.lhs.get<ast::constant_expression>().value),
						binary_op.op->value,
						ast::get_value_string(binary_op.rhs.get<ast::constant_expression>().value)
					)
				));
			}
			else
			{
				if (binary_op.lhs.has_consteval_failed())
				{
					get_consteval_fail_notes_helper(binary_op.lhs, notes);
				}
				if (binary_op.rhs.has_consteval_failed())
				{
					get_consteval_fail_notes_helper(binary_op.rhs, notes);
				}
			}
		},
		[&expr, &notes](ast::expr_subscript const &subscript_expr) {
			bool any_failed = false;
			if (subscript_expr.base.has_consteval_failed())
			{
				any_failed = true;
				get_consteval_fail_notes_helper(subscript_expr.base, notes);
			}
			if (subscript_expr.index.has_consteval_failed())
			{
				any_failed = true;
				get_consteval_fail_notes_helper(subscript_expr.index, notes);
			}
			if (!any_failed)
			{
				notes.emplace_back(ctx::parse_context::make_note(
					expr.src_tokens, "subexpression is not a constant expression"
				));
			}
		},
		[&expr, &notes](ast::expr_function_call const &func_call) {
			bool any_failed = false;
			for (auto const &param : func_call.params)
			{
				if (param.has_consteval_failed())
				{
					any_failed = true;
					get_consteval_fail_notes_helper(param, notes);
				}
			}
			if (!any_failed)
			{
				notes.emplace_back(ctx::parse_context::make_note(
					expr.src_tokens, "subexpression is not a constant expression"
				));
			}
		},
		[&expr, &notes](ast::expr_cast const &cast_expr) {
			if (cast_expr.expr.has_consteval_succeeded())
			{
				notes.emplace_back(ctx::parse_context::make_note(
					expr.src_tokens,
					bz::format(
						"subexpression '{} as {}' is not a constant expression",
						ast::get_value_string(cast_expr.expr.get<ast::constant_expression>().value),
						cast_expr.type
					)
				));
			}
			else
			{
				get_consteval_fail_notes_helper(cast_expr.expr, notes);
			}
		},
		[&notes](ast::expr_struct_init const &struct_init_expr) {
			bool any_failed = false;
			for (auto const &expr : struct_init_expr.exprs)
			{
				if (expr.has_consteval_failed())
				{
					any_failed = true;
					get_consteval_fail_notes_helper(expr, notes);
				}
			}
			bz_assert(any_failed);
		},
		[&notes](ast::expr_member_access const &member_access_expr) {
			bz_assert(!member_access_expr.base.has_consteval_succeeded());
			get_consteval_fail_notes_helper(member_access_expr.base, notes);
		},
		[&expr, &notes](ast::expr_compound const &) {
			notes.emplace_back(ctx::parse_context::make_note(
				expr.src_tokens, "subexpression is not a constant expression"
			));
		},
		[&notes](ast::expr_if const &if_expr) {
			if (!if_expr.condition.has_consteval_succeeded())
			{
				get_consteval_fail_notes_helper(if_expr.condition, notes);
			}
			else
			{
				bz_assert(if_expr.condition.is<ast::constant_expression>());
				bz_assert(if_expr.condition.get<ast::constant_expression>().value.is<ast::constant_value::boolean>());
				auto const condition_value = if_expr.condition
					.get<ast::constant_expression>().value
					.get<ast::constant_value::boolean>();
				if (condition_value)
				{
					bz_assert(!if_expr.then_block.has_consteval_succeeded());
					get_consteval_fail_notes_helper(if_expr.then_block, notes);
				}
				else
				{
					bz_assert(!if_expr.else_block.has_consteval_succeeded());
					get_consteval_fail_notes_helper(if_expr.else_block, notes);
				}
			}
		},
	});
}

bz::vector<ctx::note> get_consteval_fail_notes(ast::expression const &expr)
{
	bz::vector<ctx::note> result = {};
	if (!expr.has_consteval_failed())
	{
		return result;
	}
	else
	{
		get_consteval_fail_notes_helper(expr, result);
		return result;
	}
}

} // namespace parse
