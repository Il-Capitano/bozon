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
	bz::vector<uint64_t> index_values;
	index_values.reserve(subscript_expr.indicies.size());

	auto const &base_type = subscript_expr.base.get_expr_type_and_kind().first;

	for (size_t i = 0; i < subscript_expr.indicies.size(); ++i)
	{
		auto const &index = subscript_expr.indicies[i];

		if (index.consteval_state == ast::expression::consteval_failed)
		{
			is_consteval = false;
			continue;
		}

		bz_assert(index.is<ast::constant_expression>());
		auto const &index_const_value = index.get<ast::constant_expression>().value;
		uint64_t index_value;
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
				continue;
			}

			index_value = static_cast<uint64_t>(signed_index_value);
		}

		if (base_type.is<ast::ts_array>())
		{
			auto const &array_sizes = base_type.get<ast::ts_array>().sizes;
			bz_assert(i < array_sizes.size());
			if (index_value >= array_sizes[i])
			{
				is_consteval = false;
				if (index.paren_level < 2)
				{
					context.report_parenthesis_suppressed_warning(
						2 - index.paren_level, ctx::warning_kind::out_of_bounds_index,
						index.src_tokens,
						bz::format("index {} is out bounds for an array of size {}", index_value, array_sizes[i])
					);
				}
				continue;
			}
		}
		// tuple types shouldn't be handled, as index value checking
		// should already happen in built_in_operators
		index_values.push_back(index_value);
	}

	if (!is_consteval || subscript_expr.base.consteval_state == ast::expression::consteval_failed)
	{
		return {};
	}

	bz_assert(subscript_expr.base.is<ast::constant_expression>());
	auto const &base_value = subscript_expr.base.get<ast::constant_expression>().value;
	if (base_type.is<ast::ts_array>())
	{
		ast::constant_value const *value_ptr = &base_value;
		for (auto const index : index_values)
		{
			bz_assert(value_ptr->is<ast::constant_value::array>());
			auto const &array_value = value_ptr->get<ast::constant_value::array>();
			bz_assert(index < array_value.size());
			value_ptr = &array_value[index];
		}
		return *value_ptr;
	}
	else
	{
		// bz_assert(base_type.is<ast::ts_tuple>());
		// ^^^  this is not valid because base_type could also be empty if it's a tuple expression
		// e.g. [1, 2, 3][0]
		bz_assert(base_value.is<ast::constant_value::tuple>());
		auto const &tuple_value = base_value.get<ast::constant_value::tuple>();
		bz_assert(index_values.size() == 1);
		auto const index_value = index_values.front();
		bz_assert(index_value < tuple_value.size());
		return tuple_value[index_value];
	}
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
				is_consteval = is_consteval && elem.consteval_state == ast::expression::consteval_succeeded;
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
			if (unary_op.expr.consteval_state != ast::expression::consteval_succeeded)
			{
				return {};
			}

			return evaluate_unary_op(expr, unary_op.op->kind, unary_op.expr, context);
		},
		[&expr, &context](ast::expr_binary_op &binary_op) -> ast::constant_value {
			consteval_try(binary_op.lhs, context);
			consteval_try(binary_op.rhs, context);
			if (
				binary_op.lhs.consteval_state != ast::expression::consteval_succeeded
				|| binary_op.rhs.consteval_state != ast::expression::consteval_succeeded
			)
			{
				return {};
			}

			return evaluate_binary_op(expr, binary_op.op->kind, binary_op.lhs, binary_op.rhs, context);
		},
		[&context](ast::expr_subscript &subscript_expr) -> ast::constant_value {
			consteval_try(subscript_expr.base, context);
			for (auto &index : subscript_expr.indicies)
			{
				consteval_try(index, context);
			}
			return evaluate_subscript(subscript_expr, context);
		},
		[](ast::expr_function_call &) -> ast::constant_value {
			return {};
		},
		[&context](ast::expr_cast &cast_expr) -> ast::constant_value {
			consteval_try(cast_expr.expr, context);
			return {};
		},
		[](ast::expr_compound &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_if &) -> ast::constant_value {
			return {};
		},
	});
}

void consteval_guaranteed(ast::expression &expr, ctx::parse_context &context)
{
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
	bz_assert(expr.consteval_state == ast::expression::consteval_failed);
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
				if (elem.consteval_state == ast::expression::consteval_failed)
				{
					any_failed = true;
					get_consteval_fail_notes_helper(elem, notes);
				}
			}
			bz_assert(any_failed);
		},
		[&expr, &notes](ast::expr_unary_op const &unary_op) {
			if (unary_op.expr.consteval_state == ast::expression::consteval_succeeded)
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
				binary_op.lhs.consteval_state == ast::expression::consteval_succeeded
				&& binary_op.rhs.consteval_state == ast::expression::consteval_succeeded
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
				if (binary_op.lhs.consteval_state == ast::expression::consteval_failed)
				{
					get_consteval_fail_notes_helper(binary_op.lhs, notes);
				}
				if (binary_op.rhs.consteval_state == ast::expression::consteval_failed)
				{
					get_consteval_fail_notes_helper(binary_op.rhs, notes);
				}
			}
		},
		[&expr, &notes](ast::expr_subscript const &subscript_expr) {
			bool any_failed = false;
			if (subscript_expr.base.consteval_state == ast::expression::consteval_failed)
			{
				any_failed = true;
				get_consteval_fail_notes_helper(subscript_expr.base, notes);
			}
			for (auto const &index : subscript_expr.indicies)
			{
				if (index.consteval_state == ast::expression::consteval_failed)
				{
					any_failed = true;
					get_consteval_fail_notes_helper(index, notes);
				}
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
				if (param.consteval_state == ast::expression::consteval_failed)
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
			if (cast_expr.expr.consteval_state == ast::expression::consteval_succeeded)
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
		[&expr, &notes](ast::expr_compound const &) {
			notes.emplace_back(ctx::parse_context::make_note(
				expr.src_tokens, "subexpression is not a constant expression"
			));
		},
		[&expr, &notes](ast::expr_if const &) {
			notes.emplace_back(ctx::parse_context::make_note(
				expr.src_tokens, "subexpression is not a constant expression"
			));
		},
	});
}

bz::vector<ctx::note> get_consteval_fail_notes(ast::expression const &expr)
{
	bz::vector<ctx::note> result = {};
	if (expr.consteval_state != ast::expression::consteval_failed)
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
