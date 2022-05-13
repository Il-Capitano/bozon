#include "consteval.h"
#include "safe_operations.h"
#include "global_data.h"

namespace parse
{

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

	bz_assert(lhs_const_expr.type.is<ast::ts_base_type>());
	auto const lhs_type_kind = lhs_const_expr.type.get<ast::ts_base_type>().info->kind;

	bz_assert(rhs_value.is<ast::constant_value::uint>() || rhs_value.is<ast::constant_value::sint>());
	if (rhs_value.is<ast::constant_value::uint>())
	{
		auto const rhs_int_val = rhs_value.get<ast::constant_value::uint>();

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
	else
	{
		auto const rhs_int_val = rhs_value.get<ast::constant_value::sint>();

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

	bz_assert(ast::remove_const_or_consteval(lhs_const_expr.type).is<ast::ts_base_type>());
	auto const lhs_type_kind = ast::remove_const_or_consteval(lhs_const_expr.type).get<ast::ts_base_type>().info->kind;

	bz_assert(rhs_value.is<ast::constant_value::uint>() || rhs_value.is<ast::constant_value::sint>());
	if (rhs_value.is<ast::constant_value::uint>())
	{
		auto const rhs_int_val = rhs_value.get<ast::constant_value::uint>();

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
	else
	{
		auto const rhs_int_val = rhs_value.get<ast::constant_value::sint>();

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
					if (base_type.is<ast::ts_array>())
					{
						auto const size = base_type.get<ast::ts_array>().size;
						context.report_parenthesis_suppressed_warning(
							2 - index.paren_level, ctx::warning_kind::out_of_bounds_index,
							index.src_tokens,
							bz::format("negative index {} in subscript for an array of size {}", signed_index_value, size)
						);
					}
					else
					{
						context.report_parenthesis_suppressed_warning(
							2 - index.paren_level, ctx::warning_kind::out_of_bounds_index,
							index.src_tokens,
							bz::format("negative index {} in array subscript", signed_index_value)
						);
					}
				}
			}
			else
			{
				index_value = static_cast<uint64_t>(signed_index_value);
			}
		}

		if (base_type.is<ast::ts_array>())
		{
			auto const size = base_type.get<ast::ts_array>().size;
			if (index_value >= size)
			{
				is_consteval = false;
				if (index.paren_level < 2)
				{
					context.report_parenthesis_suppressed_warning(
						2 - index.paren_level, ctx::warning_kind::out_of_bounds_index,
						index.src_tokens,
						bz::format("index {} is out-of-bounds for an array of size {}", index_value, size)
					);
				}
			}
		}
		// tuple types shouldn't be handled, as index value checking
		// should already happen in built_in_operators
	}
	else
	{
		is_consteval = false;
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
	for (auto &param : func_call.params)
	{
		if (!param.has_consteval_succeeded())
		{
			return {};
		}
	}

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

	auto const report_domain_error = [&](bz::u8string message) {
		context.report_parenthesis_suppressed_warning(
			2 - paren_level, ctx::warning_kind::math_domain_error,
			src_tokens, std::move(message)
		);
	};

	using T = std::pair<bool, bz::u8string_view>;
	auto const check_math_function = [&](
		bz::u8string_view func_name,
		auto arg,
		auto result,
		std::initializer_list<T> conditions
	) -> ast::constant_value {
		for (auto const &[condition, fmt] : conditions)
		{
			if (condition)
			{
				report_domain_error(bz::format("calling '{}' {}", func_name, bz::format(fmt, arg, result)));
				return ast::constant_value(result);
			}
		}

		return ast::constant_value(result);
	};

	switch (func_call.func_body->intrinsic_kind)
	{
#define def_case_default(func_name)                         \
case ast::function_body::func_name##_f32:                   \
{                                                           \
    auto const arg = get_float32();                         \
    return check_math_function(                             \
        #func_name, arg, std::func_name(arg),               \
        {                                                   \
            T{ std::isnan(arg), "with {} results in {}" } \
        }                                                   \
    );                                                      \
}                                                           \
case ast::function_body::func_name##_f64:                   \
{                                                           \
    auto const arg = get_float64();                         \
    return check_math_function(                             \
        #func_name, arg, std::func_name(arg),               \
        {                                                   \
            T{ std::isnan(arg), "with {} results in {}" } \
        }                                                   \
    );                                                      \
}

#define def_case_error(func_name, ...)                       \
case ast::function_body::func_name##_f32:                    \
{                                                            \
    auto const arg = get_float32();                          \
    return check_math_function(                              \
        #func_name, arg, std::func_name(arg),                \
        {                                                    \
            T{ std::isnan(arg), "with {} results in {}" }, \
            __VA_ARGS__                                      \
        }                                                    \
    );                                                       \
}                                                            \
case ast::function_body::func_name##_f64:                    \
{                                                            \
    auto const arg = get_float64();                          \
    return check_math_function(                              \
        #func_name, arg, std::func_name(arg),                \
        {                                                    \
            T{ std::isnan(arg), "with {} results in {}" }, \
            __VA_ARGS__                                      \
        }                                                    \
    );                                                       \
}

	// ==== exponential and logarithmic functions ====
	// exponential functions can take any value
	def_case_default(exp)
	def_case_default(exp2)
	def_case_default(expm1)
	// log functions can't take negative arguments, except for log1p, which can't take numbers < -1.0
	def_case_error(log,   (T{ arg == 0.0,  "with {} results in {}" }), (T{ arg < 0.0,  "with a negative value {} results in {}" }))
	def_case_error(log10, (T{ arg == 0.0,  "with {} results in {}" }), (T{ arg < 0.0,  "with a negative value {} results in {}" }))
	def_case_error(log2,  (T{ arg == 0.0,  "with {} results in {}" }), (T{ arg < 0.0,  "with a negative value {} results in {}" }))
	def_case_error(log1p, (T{ arg <= -1.0, "with {} results in {}" }))

	// ==== power functions ====
	def_case_error(sqrt, (T{ arg < 0.0, "with a negative value {} results in {}" }))
	case ast::function_body::pow_f32:
	{
		auto const base = get_float32(0);
		auto const exp = get_float32(0);
		auto const result = std::pow(base, exp);
		if (base == 0.0f && exp < 0.0f)
		{
			report_domain_error(bz::format("calling 'pow' with base {} and exponent {} results in {}", base, exp, result));
		}
		else if (std::isfinite(base) && base < 0.0f && std::isfinite(exp) && exp != std::trunc(exp))
		{
			report_domain_error(bz::format("calling 'pow' with a negative base {} and a non-integer exponent {} results in {}", base, exp, result));
		}
		else if (
			base != 1.0f
			&& exp != 0.0f
			&& (std::isnan(base) || std::isnan(exp))
		)
		{
			report_domain_error(bz::format("calling 'pow' with base {} and exponent {} results in {}", base, exp, result));
		}
		return ast::constant_value(result);
	}
	case ast::function_body::pow_f64:
	{
		auto const base = get_float64(0);
		auto const exp = get_float64(0);
		auto const result = std::pow(base, exp);
		if (base == 0.0 && exp < 0.0)
		{
			report_domain_error(bz::format("calling 'pow' with base {} and exponent {} results in {}", base, exp, result));
		}
		else if (std::isfinite(base) && base < 0.0 && std::isfinite(exp) && exp != std::trunc(exp))
		{
			report_domain_error(bz::format("calling 'pow' with negative base {} and non-integer exponent {} results in {}", base, exp, result));
		}
		else if (
			base != 1.0
			&& exp != 0.0
			&& (std::isnan(base) || std::isnan(exp))
		)
		{
			report_domain_error(bz::format("calling 'pow' with base {} and exponent {} results in {}", base, exp, result));
		}
		return ast::constant_value(result);
	}
	def_case_default(cbrt)
	case ast::function_body::hypot_f32:
	{
		auto const x = get_float32(0);
		auto const y = get_float32(1);
		auto const result = std::hypot(x, y);
		if (!std::isinf(x) && !std::isinf(y) && (std::isnan(x) || std::isnan(y)))
		{
			report_domain_error(bz::format("calling 'hypot' with {} and {} results in {}", x, y, result));
		}
		return ast::constant_value(result);
	}
	case ast::function_body::hypot_f64:
	{
		auto const x = get_float64(0);
		auto const y = get_float64(1);
		auto const result = std::hypot(x, y);
		if (!std::isinf(x) && !std::isinf(y) && (std::isnan(x) || std::isnan(y)))
		{
			report_domain_error(bz::format("calling 'hypot' with {} and {} results in {}", x, y, result));
		}
		return ast::constant_value(result);
	}

	// ==== trigonometric functions ====
	def_case_error(sin, (T{ std::isinf(arg), "with {} results in {}" }))
	def_case_error(cos, (T{ std::isinf(arg), "with {} results in {}" }))
	def_case_error(tan, (T{ std::isinf(arg), "with {} results in {}" }))
	def_case_error(asin, (T{ std::abs(arg) > 1.0, "with {} results in {}" }))
	def_case_error(acos, (T{ std::abs(arg) > 1.0, "with {} results in {}" }))
	def_case_default(atan)
	case ast::function_body::atan2_f32:
	{
		auto const y = get_float32(0);
		auto const x = get_float32(1);
		auto const result = std::atan2(y, x);
		if (std::isnan(y) || std::isnan(x))
		{
			report_domain_error(bz::format("calling 'atan2' with {} and {} results in {}", y, x, result));
		}
		return ast::constant_value(result);
	}
	case ast::function_body::atan2_f64:
	{
		auto const y = get_float64(0);
		auto const x = get_float64(1);
		auto const result = std::atan2(y, x);
		if (std::isnan(y) || std::isnan(x))
		{
			report_domain_error(bz::format("calling 'atan2' with {} and {} results in {}", y, x, result));
		}
		return ast::constant_value(result);
	}

	// ==== hyperbolic functions ====
	def_case_default(sinh)
	def_case_default(cosh)
	def_case_default(tanh)
	def_case_default(asinh)
	def_case_error(acosh, (T{ arg < 1.0, "with {} results in {}" }))
	def_case_error(atanh, (T{ std::abs(arg) >= 1.0, "with {} results in {}" }))

	// ==== error and gamma functions ====
	def_case_default(erf)
	def_case_default(erfc)
	def_case_error(
		tgamma,
		(T{ arg == 0.0, "with {} results in {}" }),
		(T{ arg == -std::numeric_limits<decltype(arg)>::infinity(), "with {} results in {}" }),
		(T{ arg < 0.0 && std::trunc(arg) == arg, "with a negative integer {} results in {}" })
	)
	def_case_error(
		lgamma,
		(T{ arg == 0.0, "with {} results in {}" }),
		(T{ arg < 0.0 && std::trunc(arg) == arg, "with a negative integer {} results in {}" })
	)

	default:
		bz_unreachable;

#undef def_case_default
#undef def_case_error
	}
}

#ifdef __clang__

static uint8_t bitreverse8(uint8_t value)
{
	return __builtin_bitreverse8(value);
}

static uint16_t bitreverse16(uint16_t value)
{
	return __builtin_bitreverse16(value);
}

static uint32_t bitreverse32(uint32_t value)
{
	return __builtin_bitreverse32(value);
}

static uint64_t bitreverse64(uint64_t value)
{
	return __builtin_bitreverse64(value);
}

#else

// implementation from https://www.geeksforgeeks.org/reverse-actual-bits-given-number/

static uint8_t bitreverse8(uint8_t value)
{
	uint8_t rev = 0;
	for (int i = 0; i < 8; ++i)
	{
		rev <<= 1;
		rev |= value & 1u;
		value >>= 1;
	}
	return rev;
}

static uint16_t bitreverse16(uint16_t value)
{
	uint16_t rev = 0;
	for (int i = 0; i < 16; ++i)
	{
		rev <<= 1;
		rev |= value & 1u;
		value >>= 1;
	}
	return rev;
}

static uint32_t bitreverse32(uint32_t value)
{
	uint8_t rev = 0;
	for (int i = 0; i < 32; ++i)
	{
		rev <<= 1;
		rev |= value & 1u;
		value >>= 1;
	}
	return rev;
}

static uint64_t bitreverse64(uint64_t value)
{
	uint8_t rev = 0;
	for (int i = 0; i < 64; ++i)
	{
		rev <<= 1;
		rev |= value & 1u;
		value >>= 1;
	}
	return rev;
}

#endif // __clang__

static ast::constant_value evaluate_bit_manipulation(
	ast::expression const &original_expr,
	ast::expr_function_call const &func_call,
	ctx::parse_context &context
)
{
	for (auto &param : func_call.params)
	{
		if (!param.has_consteval_succeeded())
		{
			return {};
		}
	}

	auto const get_u8 = [&func_call](size_t i = 0) -> uint8_t {
		bz_assert(i < func_call.params.size());
		bz_assert(func_call.params[i].is<ast::constant_expression>());
		auto const &value = func_call.params[i].get<ast::constant_expression>().value;
		bz_assert(value.is<ast::constant_value::uint>());
		return static_cast<uint8_t>(value.get<ast::constant_value::uint>());
	};

	auto const get_u16 = [&func_call](size_t i = 0) -> uint16_t {
		bz_assert(i < func_call.params.size());
		bz_assert(func_call.params[i].is<ast::constant_expression>());
		auto const &value = func_call.params[i].get<ast::constant_expression>().value;
		bz_assert(value.is<ast::constant_value::uint>());
		return static_cast<uint16_t>(value.get<ast::constant_value::uint>());
	};

	auto const get_u32 = [&func_call](size_t i = 0) -> uint32_t {
		bz_assert(i < func_call.params.size());
		bz_assert(func_call.params[i].is<ast::constant_expression>());
		auto const &value = func_call.params[i].get<ast::constant_expression>().value;
		bz_assert(value.is<ast::constant_value::uint>());
		return static_cast<uint32_t>(value.get<ast::constant_value::uint>());
	};

	auto const get_u64 = [&func_call](size_t i = 0) -> uint64_t {
		bz_assert(i < func_call.params.size());
		bz_assert(func_call.params[i].is<ast::constant_expression>());
		auto const &value = func_call.params[i].get<ast::constant_expression>().value;
		bz_assert(value.is<ast::constant_value::uint>());
		return static_cast<uint64_t>(value.get<ast::constant_value::uint>());
	};

	auto const paren_level = original_expr.paren_level;
	auto const &src_tokens = original_expr.src_tokens;

	switch (func_call.func_body->intrinsic_kind)
	{
	case ast::function_body::bitreverse_u8:
		return ast::constant_value(static_cast<uint64_t>(bitreverse8(get_u8())));
	case ast::function_body::bitreverse_u16:
		return ast::constant_value(static_cast<uint64_t>(bitreverse16(get_u16())));
	case ast::function_body::bitreverse_u32:
		return ast::constant_value(static_cast<uint64_t>(bitreverse32(get_u32())));
	case ast::function_body::bitreverse_u64:
		return ast::constant_value(static_cast<uint64_t>(bitreverse64(get_u64())));
	case ast::function_body::popcount_u8:
		return ast::constant_value(static_cast<uint64_t>(__builtin_popcount(get_u8())));
	case ast::function_body::popcount_u16:
		return ast::constant_value(static_cast<uint64_t>(__builtin_popcount(get_u16())));
	case ast::function_body::popcount_u32:
		return ast::constant_value(static_cast<uint64_t>(__builtin_popcount(get_u32())));
	case ast::function_body::popcount_u64:
		return ast::constant_value(static_cast<uint64_t>(__builtin_popcountll(get_u64())));
	case ast::function_body::byteswap_u16:
		return ast::constant_value(static_cast<uint64_t>(__builtin_bswap16(get_u16())));
	case ast::function_body::byteswap_u32:
		return ast::constant_value(static_cast<uint64_t>(__builtin_bswap32(get_u32())));
	case ast::function_body::byteswap_u64:
		return ast::constant_value(static_cast<uint64_t>(__builtin_bswap64(get_u64())));
	case ast::function_body::clz_u8:
	{
		auto const val = get_u8();
		return ast::constant_value(static_cast<uint64_t>(val == 0 ? 8 : __builtin_clzll(val) - 56));
	}
	case ast::function_body::clz_u16:
	{
		auto const val = get_u16();
		return ast::constant_value(static_cast<uint64_t>(val == 0 ? 16 : __builtin_clzll(val) - 48));
	}
	case ast::function_body::clz_u32:
	{
		auto const val = get_u32();
		return ast::constant_value(static_cast<uint64_t>(val == 0 ? 32 : __builtin_clzll(val) - 32));
	}
	case ast::function_body::clz_u64:
	{
		auto const val = get_u64();
		return ast::constant_value(static_cast<uint64_t>(val == 0 ? 64 : __builtin_clzll(val)));
	}
	case ast::function_body::ctz_u8:
	{
		auto const val = get_u8();
		return ast::constant_value(static_cast<uint64_t>(val == 0 ? 8 : __builtin_ctzll(val)));
	}
	case ast::function_body::ctz_u16:
	{
		auto const val = get_u16();
		return ast::constant_value(static_cast<uint64_t>(val == 0 ? 16 : __builtin_ctzll(val)));
	}
	case ast::function_body::ctz_u32:
	{
		auto const val = get_u32();
		return ast::constant_value(static_cast<uint64_t>(val == 0 ? 32 : __builtin_ctzll(val)));
	}
	case ast::function_body::ctz_u64:
	{
		auto const val = get_u64();
		return ast::constant_value(static_cast<uint64_t>(val == 0 ? 64 : __builtin_ctzll(val)));
	}
	case ast::function_body::fshl_u8:
	{
		auto const a = get_u8(0);
		auto const b = get_u8(1);
		auto const amount = get_u8(2);
		if (amount > 8)
		{
			context.report_parenthesis_suppressed_warning(
				2 - paren_level, ctx::warning_kind::int_overflow,
				src_tokens, bz::format("shift amount of {} in '__builtin_fshl_u8' exceeds bit count of 'uint8'", amount)
			);
			return {};
		}
		auto const result = static_cast<uint8_t>((a << amount) | (b >> (8 - amount)));
		return ast::constant_value(static_cast<uint64_t>(result));
	}
	case ast::function_body::fshl_u16:
	{
		auto const a = get_u16(0);
		auto const b = get_u16(1);
		auto const amount = get_u16(2);
		if (amount > 16)
		{
			context.report_parenthesis_suppressed_warning(
				2 - paren_level, ctx::warning_kind::int_overflow,
				src_tokens, bz::format("shift amount of {} in '__builtin_fshl_u16' exceeds bit count of 'uint16'", amount)
			);
			return {};
		}
		auto const result = static_cast<uint16_t>((a << amount) | (b >> (16 - amount)));
		return ast::constant_value(static_cast<uint64_t>(result));
	}
	case ast::function_body::fshl_u32:
	{
		auto const a = get_u32(0);
		auto const b = get_u32(1);
		auto const amount = get_u32(2);
		if (amount > 32)
		{
			context.report_parenthesis_suppressed_warning(
				2 - paren_level, ctx::warning_kind::int_overflow,
				src_tokens, bz::format("shift amount of {} in '__builtin_fshl_u32' exceeds bit count of 'uint32'", amount)
			);
			return {};
		}
		auto const result = static_cast<uint32_t>((a << amount) | (b >> (32 - amount)));
		return ast::constant_value(static_cast<uint64_t>(result));
	}
	case ast::function_body::fshl_u64:
	{
		auto const a = get_u64(0);
		auto const b = get_u64(1);
		auto const amount = get_u64(2);
		if (amount > 64)
		{
			context.report_parenthesis_suppressed_warning(
				2 - paren_level, ctx::warning_kind::int_overflow,
				src_tokens, bz::format("shift amount of {} in '__builtin_fshl_u64' exceeds bit count of 'uint64'", amount)
			);
			return {};
		}
		auto const result = static_cast<uint64_t>((a << amount) | (b >> (64 - amount)));
		return ast::constant_value(static_cast<uint64_t>(result));
	}
	case ast::function_body::fshr_u8:
	{
		auto const a = get_u8(0);
		auto const b = get_u8(1);
		auto const amount = get_u8(2);
		if (amount > 8)
		{
			context.report_parenthesis_suppressed_warning(
				2 - paren_level, ctx::warning_kind::int_overflow,
				src_tokens, bz::format("shift amount of {} in '__builtin_fshr_u8' exceeds bit count of 'uint8'", amount)
			);
			return {};
		}
		auto const result = static_cast<uint8_t>((a << (8 - amount)) | (b >> amount));
		return ast::constant_value(static_cast<uint64_t>(result));
	}
	case ast::function_body::fshr_u16:
	{
		auto const a = get_u16(0);
		auto const b = get_u16(1);
		auto const amount = get_u16(2);
		if (amount > 16)
		{
			context.report_parenthesis_suppressed_warning(
				2 - paren_level, ctx::warning_kind::int_overflow,
				src_tokens, bz::format("shift amount of {} in '__builtin_fshr_u16' exceeds bit count of 'uint16'", amount)
			);
			return {};
		}
		auto const result = static_cast<uint16_t>((a << (16 - amount)) | (b >> amount));
		return ast::constant_value(static_cast<uint64_t>(result));
	}
	case ast::function_body::fshr_u32:
	{
		auto const a = get_u32(0);
		auto const b = get_u32(1);
		auto const amount = get_u32(2);
		if (amount > 32)
		{
			context.report_parenthesis_suppressed_warning(
				2 - paren_level, ctx::warning_kind::int_overflow,
				src_tokens, bz::format("shift amount of {} in '__builtin_fshr_u32' exceeds bit count of 'uint32'", amount)
			);
			return {};
		}
		auto const result = static_cast<uint32_t>((a << (32 - amount)) | (b >> amount));
		return ast::constant_value(static_cast<uint64_t>(result));
	}
	case ast::function_body::fshr_u64:
	{
		auto const a = get_u64(0);
		auto const b = get_u64(1);
		auto const amount = get_u64(2);
		if (amount > 64)
		{
			context.report_parenthesis_suppressed_warning(
				2 - paren_level, ctx::warning_kind::int_overflow,
				src_tokens, bz::format("shift amount of {} in '__builtin_fshr_u64' exceeds bit count of 'uint64'", amount)
			);
			return {};
		}
		auto const result = static_cast<uint64_t>((a << (64 - amount)) | (b >> amount));
		return ast::constant_value(static_cast<uint64_t>(result));
	}
	default:
		bz_unreachable;
	}
}

enum class function_execution_kind
{
	guaranteed_evaluate,
	force_evaluate,
	force_evaluate_without_error,
};

template<typename ...Kinds>
static ast::constant_value is_typespec_kind_helper(ast::expr_function_call &func_call)
{
	bz_assert(func_call.params.size() == 1);
	bz_assert(func_call.params[0].is<ast::constant_expression>());
	bz_assert(func_call.params[0].get<ast::constant_expression>().value.is<ast::constant_value::type>());
	auto const type = func_call.params[0]
		.get<ast::constant_expression>().value
		.get<ast::constant_value::type>().as_typespec_view();
	return ast::constant_value((type.is<Kinds>() || ...));
}

template<typename Kind>
static ast::constant_value remove_typespec_kind_helper(ast::expr_function_call &func_call)
{
	bz_assert(func_call.params.size() == 1);
	bz_assert(func_call.params[0].is<ast::constant_expression>());
	bz_assert(func_call.params[0].get<ast::constant_expression>().value.is<ast::constant_value::type>());
	auto const type = func_call.params[0]
		.get<ast::constant_expression>().value
		.get<ast::constant_value::type>().as_typespec_view();
	if (type.is<Kind>())
	{
		return ast::constant_value(type.get<Kind>());
	}
	else
	{
		return ast::constant_value(type);
	}
}

static ast::constant_value evaluate_intrinsic_function_call(
	ast::expression const &original_expr,
	ast::expr_function_call &func_call,
	function_execution_kind exec_kind,
	ctx::parse_context &context
)
{
	bz_assert(func_call.func_body->is_intrinsic());
	bz_assert(func_call.func_body->body.is_null());
	switch (func_call.func_body->intrinsic_kind)
	{
	static_assert(ast::function_body::_builtin_last - ast::function_body::_builtin_first == 144);
	static_assert(ast::function_body::_builtin_default_constructor_last - ast::function_body::_builtin_default_constructor_first == 14);
	static_assert(ast::function_body::_builtin_unary_operator_last - ast::function_body::_builtin_unary_operator_first == 7);
	static_assert(ast::function_body::_builtin_binary_operator_last - ast::function_body::_builtin_binary_operator_first == 27);
	case ast::function_body::builtin_str_length:
	case ast::function_body::builtin_str_starts_with:
	case ast::function_body::builtin_str_ends_with:
		bz_unreachable;

	case ast::function_body::builtin_str_begin_ptr:
	case ast::function_body::builtin_str_end_ptr:
		return {};
	case ast::function_body::builtin_str_size:
	{
		bz_assert(func_call.params.size() == 1);
		if (exec_kind == function_execution_kind::force_evaluate)
		{
			consteval_try(func_call.params[0], context);
		}
		else if (exec_kind == function_execution_kind::force_evaluate_without_error)
		{
			consteval_try_without_error(func_call.params[0], context);
		}
		if (!func_call.params[0].has_consteval_succeeded())
		{
			return {};
		}
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

	case ast::function_body::builtin_optional_has_value:
	case ast::function_body::builtin_optional_get_value:
		return {};

	case ast::function_body::builtin_pointer_cast:
	{
		bz_assert(func_call.params[0].is_typename());
		if (exec_kind == function_execution_kind::force_evaluate)
		{
			consteval_try(func_call.params[1], context);
		}
		else if (exec_kind == function_execution_kind::force_evaluate_without_error)
		{
			consteval_try_without_error(func_call.params[1], context);
		}
		if (!func_call.params[1].has_consteval_succeeded())
		{
			return {};
		}
		bz_assert(func_call.params[1].is<ast::constant_expression>());
		bz_assert(func_call.params[1].get<ast::constant_expression>().value.is<ast::constant_value::null>());
		return func_call.params[1].get<ast::constant_expression>().value;
	}
	case ast::function_body::builtin_pointer_to_int:
	case ast::function_body::builtin_int_to_pointer:
		return {};

	case ast::function_body::builtin_call_destructor:
	case ast::function_body::builtin_inplace_construct:
	case ast::function_body::builtin_optional_inplace_construct:
		return {};

	case ast::function_body::builtin_is_comptime:
		if (exec_kind == function_execution_kind::force_evaluate)
		{
			if (original_expr.paren_level < 2)
			{
				context.report_parenthesis_suppressed_warning(
					2 - original_expr.paren_level,
					ctx::warning_kind::is_comptime_always_true,
					original_expr.src_tokens,
					"'__builtin_is_comptime()' was forced to evaluate to always be 'true'"
				);
			}
			return ast::constant_value(true);
		}
		else
		{
			return {};
		}

	case ast::function_body::builtin_is_option_set_impl:
		return {};
	case ast::function_body::builtin_is_option_set:
	{
		bz_assert(func_call.params.size() == 1);
		if (exec_kind == function_execution_kind::force_evaluate)
		{
			consteval_try(func_call.params[0], context);
		}
		else if (exec_kind == function_execution_kind::force_evaluate_without_error)
		{
			consteval_try_without_error(func_call.params[0], context);
		}
		if (!func_call.params[0].has_consteval_succeeded())
		{
			return {};
		}
		bz_assert(func_call.params[0].is<ast::constant_expression>());
		bz_assert(func_call.params[0].get<ast::constant_expression>().value.is<ast::constant_value::string>());
		auto const &option = func_call.params[0].get<ast::constant_expression>().value.get<ast::constant_value::string>();
		return ast::constant_value(defines.contains(option));
	}

	case ast::function_body::builtin_panic:
		return {};

	// builtins end here

	case ast::function_body::print_stdout:
	case ast::function_body::print_stderr:
		return {};

	case ast::function_body::comptime_malloc:
	case ast::function_body::comptime_malloc_type:
	case ast::function_body::comptime_free:
		return {};

	case ast::function_body::comptime_compile_error:
		if (exec_kind == function_execution_kind::force_evaluate)
		{
			bz_assert(func_call.params.size() == 1);
			consteval_try(func_call.params[0], context);
			if (func_call.params[0].has_consteval_succeeded())
			{
				auto const &message_value = func_call.params[0].get<ast::constant_expression>().value;
				bz_assert(message_value.is<ast::constant_value::string>());
				auto const message = message_value.get<ast::constant_value::string>().as_string_view();
				context.report_error(func_call.src_tokens, message);
				return ast::constant_value::get_void();
			}
		}
		return {};
	case ast::function_body::comptime_compile_warning:
		if (exec_kind == function_execution_kind::force_evaluate)
		{
			bz_assert(func_call.params.size() == 1);
			consteval_try(func_call.params[0], context);
			if (func_call.params[0].has_consteval_succeeded())
			{
				auto const &message_value = func_call.params[0].get<ast::constant_expression>().value;
				bz_assert(message_value.is<ast::constant_value::string>());
				auto const message = message_value.get<ast::constant_value::string>().as_string_view();
				context.report_warning(ctx::warning_kind::comptime_warning, func_call.src_tokens, message);
				return ast::constant_value::get_void();
			}
		}
		return {};
	case ast::function_body::comptime_compile_error_src_tokens:
	case ast::function_body::comptime_compile_warning_src_tokens:
		return {};
	case ast::function_body::comptime_create_global_string:
		if (exec_kind == function_execution_kind::force_evaluate)
		{
			bz_assert(func_call.params.size() == 1);
			consteval_try(func_call.params[0], context);
			if (func_call.params[0].has_consteval_succeeded())
			{
				auto const &str_value = func_call.params[0].get<ast::constant_expression>().value;
				bz_assert(str_value.is<ast::constant_value::string>());
				return str_value;
			}
		}
		return {};

	case ast::function_body::comptime_concatenate_strs:
	{
		bz_assert(func_call.params.is_all([](auto const &param) {
			return param.template is<ast::constant_expression>();
		}));
		bz_assert(func_call.params.is_all([](auto const &param) {
			return param
				.template get<ast::constant_expression>().value
				.template is<ast::constant_value::string>();
		}));

		auto result = func_call.params
			.transform([](auto const &param) -> auto const & {
				return param
					.template get<ast::constant_expression>().value
					.template get<ast::constant_value::string>();
			})
			.reduce(bz::u8string(), [](auto lhs, auto const &rhs) {
				lhs += rhs;
				return lhs;
			});
		return ast::constant_value(std::move(result));
	}

	case ast::function_body::comptime_format_float32:
	case ast::function_body::comptime_format_float64:
		return {};

	case ast::function_body::typename_as_str:
	{
		bz_assert(func_call.params.size() == 1);
		bz_assert(func_call.params[0].is<ast::constant_expression>());
		bz_assert(func_call.params[0].get<ast::constant_expression>().value.is<ast::constant_value::type>());
		auto const type = func_call.params[0]
			.get<ast::constant_expression>().value
			.get<ast::constant_value::type>().as_typespec_view();
		return ast::constant_value(bz::format("{}", type));
	}

	case ast::function_body::is_const:
		return is_typespec_kind_helper<ast::ts_const>(func_call);
	case ast::function_body::is_consteval:
		return is_typespec_kind_helper<ast::ts_consteval>(func_call);
	case ast::function_body::is_pointer:
		return is_typespec_kind_helper<ast::ts_pointer>(func_call);
	case ast::function_body::is_optional:
		return is_typespec_kind_helper<ast::ts_optional_pointer, ast::ts_optional>(func_call);
	case ast::function_body::is_reference:
		return is_typespec_kind_helper<ast::ts_lvalue_reference>(func_call);
	case ast::function_body::is_move_reference:
		return is_typespec_kind_helper<ast::ts_move_reference>(func_call);

	case ast::function_body::remove_const:
		return remove_typespec_kind_helper<ast::ts_const>(func_call);
	case ast::function_body::remove_consteval:
		return remove_typespec_kind_helper<ast::ts_consteval>(func_call);
	case ast::function_body::remove_pointer:
		return remove_typespec_kind_helper<ast::ts_pointer>(func_call);
	case ast::function_body::remove_optional:
	{
		bz_assert(func_call.params.size() == 1);
		bz_assert(func_call.params[0].is<ast::constant_expression>());
		bz_assert(func_call.params[0].get<ast::constant_expression>().value.is<ast::constant_value::type>());
		auto const type = func_call.params[0]
			.get<ast::constant_expression>().value
			.get<ast::constant_value::type>().as_typespec_view();
		if (type.is<ast::ts_optional_pointer>())
		{
			auto result = ast::constant_value();
			auto &result_type = result.emplace<ast::constant_value::type>(type);
			result_type.nodes[0].emplace<ast::ts_pointer>();
			return result;
		}
		else if (type.is<ast::ts_optional>())
		{
			return ast::constant_value(type.get<ast::ts_optional>());
		}
		else
		{
			return ast::constant_value(type);
		}
	}
	case ast::function_body::remove_reference:
		return remove_typespec_kind_helper<ast::ts_lvalue_reference>(func_call);
	case ast::function_body::remove_move_reference:
		return remove_typespec_kind_helper<ast::ts_move_reference>(func_call);

	case ast::function_body::is_default_constructible:
	{
		bz_assert(func_call.params.size() == 1);
		bz_assert(func_call.params[0].is<ast::constant_expression>());
		bz_assert(func_call.params[0].get<ast::constant_expression>().value.is<ast::constant_value::type>());
		auto const type = func_call.params[0]
			.get<ast::constant_expression>().value
			.get<ast::constant_value::type>().as_typespec_view();
		return ast::constant_value(ast::is_default_constructible(type));
	}
	case ast::function_body::is_copy_constructible:
	{
		bz_assert(func_call.params.size() == 1);
		bz_assert(func_call.params[0].is<ast::constant_expression>());
		bz_assert(func_call.params[0].get<ast::constant_expression>().value.is<ast::constant_value::type>());
		auto const type = func_call.params[0]
			.get<ast::constant_expression>().value
			.get<ast::constant_value::type>().as_typespec_view();
		return ast::constant_value(ast::is_copy_constructible(type));
	}
	case ast::function_body::is_trivially_copy_constructible:
	{
		bz_assert(func_call.params.size() == 1);
		bz_assert(func_call.params[0].is<ast::constant_expression>());
		bz_assert(func_call.params[0].get<ast::constant_expression>().value.is<ast::constant_value::type>());
		auto const type = func_call.params[0]
			.get<ast::constant_expression>().value
			.get<ast::constant_value::type>().as_typespec_view();
		return ast::constant_value(ast::is_trivially_copy_constructible(type));
	}
	case ast::function_body::is_trivially_destructible:
	{
		bz_assert(func_call.params.size() == 1);
		bz_assert(func_call.params[0].is<ast::constant_expression>());
		bz_assert(func_call.params[0].get<ast::constant_expression>().value.is<ast::constant_value::type>());
		auto const type = func_call.params[0]
			.get<ast::constant_expression>().value
			.get<ast::constant_value::type>().as_typespec_view();
		return ast::constant_value(ast::is_trivially_destructible(type));
	}

	case ast::function_body::lifetime_start:
	case ast::function_body::lifetime_end:
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
		if (exec_kind == function_execution_kind::force_evaluate)
		{
			for (auto &param : func_call.params)
			{
				consteval_try(param, context);
			}
		}
		else if (exec_kind == function_execution_kind::force_evaluate_without_error)
		{
			for (auto &param : func_call.params)
			{
				consteval_try_without_error(param, context);
			}
		}
		return evaluate_math_functions(original_expr, func_call, context);

	case ast::function_body::bitreverse_u8:
	case ast::function_body::bitreverse_u16:
	case ast::function_body::bitreverse_u32:
	case ast::function_body::bitreverse_u64:
	case ast::function_body::popcount_u8:
	case ast::function_body::popcount_u16:
	case ast::function_body::popcount_u32:
	case ast::function_body::popcount_u64:
	case ast::function_body::byteswap_u16:
	case ast::function_body::byteswap_u32:
	case ast::function_body::byteswap_u64:
		[[fallthrough]];
	case ast::function_body::clz_u8:
	case ast::function_body::clz_u16:
	case ast::function_body::clz_u32:
	case ast::function_body::clz_u64:
	case ast::function_body::ctz_u8:
	case ast::function_body::ctz_u16:
	case ast::function_body::ctz_u32:
	case ast::function_body::ctz_u64:
	case ast::function_body::fshl_u8:
	case ast::function_body::fshl_u16:
	case ast::function_body::fshl_u32:
	case ast::function_body::fshl_u64:
	case ast::function_body::fshr_u8:
	case ast::function_body::fshr_u16:
	case ast::function_body::fshr_u32:
	case ast::function_body::fshr_u64:
		if (exec_kind == function_execution_kind::force_evaluate)
		{
			for (auto &param : func_call.params)
			{
				consteval_try(param, context);
			}
		}
		else if (exec_kind == function_execution_kind::force_evaluate_without_error)
		{
			for (auto &param : func_call.params)
			{
				consteval_try_without_error(param, context);
			}
		}
		return evaluate_bit_manipulation(original_expr, func_call, context);

	case ast::function_body::i8_default_constructor:
	case ast::function_body::i16_default_constructor:
	case ast::function_body::i32_default_constructor:
	case ast::function_body::i64_default_constructor:
		return ast::constant_value(int64_t());
	case ast::function_body::u8_default_constructor:
	case ast::function_body::u16_default_constructor:
	case ast::function_body::u32_default_constructor:
	case ast::function_body::u64_default_constructor:
		return ast::constant_value(uint64_t());
	case ast::function_body::f32_default_constructor:
		return ast::constant_value(float32_t());
	case ast::function_body::f64_default_constructor:
		return ast::constant_value(float64_t());
	case ast::function_body::char_default_constructor:
		return ast::constant_value(bz::u8char());
	case ast::function_body::str_default_constructor:
		return ast::constant_value(bz::u8string());
	case ast::function_body::bool_default_constructor:
		return ast::constant_value(bool());
	case ast::function_body::null_t_default_constructor:
		return ast::constant_value(ast::internal::null_t());

	case ast::function_body::builtin_unary_plus:
	{
		bz_assert(func_call.params.size() == 1);
		if (exec_kind == function_execution_kind::force_evaluate)
		{
			consteval_try(func_call.params[0], context);
		}
		else if (exec_kind == function_execution_kind::force_evaluate_without_error)
		{
			consteval_try_without_error(func_call.params[0], context);
		}
		if (!func_call.params[0].has_consteval_succeeded())
		{
			return {};
		}
		bz_assert(func_call.params[0].is<ast::constant_expression>());
		auto const &const_expr = func_call.params[0].get<ast::constant_expression>();
		auto const &value = const_expr.value;
		return value;
	}
	case ast::function_body::builtin_unary_minus:
	{
		bz_assert(func_call.params.size() == 1);
		if (exec_kind == function_execution_kind::force_evaluate)
		{
			consteval_try(func_call.params[0], context);
		}
		else if (exec_kind == function_execution_kind::force_evaluate_without_error)
		{
			consteval_try_without_error(func_call.params[0], context);
		}
		if (!func_call.params[0].has_consteval_succeeded())
		{
			return {};
		}
		bz_assert(func_call.params[0].is<ast::constant_expression>());
		auto const &const_expr = func_call.params[0].get<ast::constant_expression>();
		auto const &value = const_expr.value;
		if (value.is<ast::constant_value::sint>())
		{
			bz_assert(ast::remove_const_or_consteval(const_expr.type).is<ast::ts_base_type>());
			auto const type = ast::remove_const_or_consteval(const_expr.type).get<ast::ts_base_type>().info->kind;
			auto const int_val = value.get<ast::constant_value::sint>();
			return ast::constant_value(safe_unary_minus(
				original_expr.src_tokens, original_expr.paren_level,
				int_val, type, context
			));
		}
		else if (value.is<ast::constant_value::float32>())
		{
			auto const float_val = value.get<ast::constant_value::float32>();
			return ast::constant_value(-float_val);
		}
		else
		{
			bz_assert(value.is<ast::constant_value::float64>());
			auto const float_val = value.get<ast::constant_value::float64>();
			return ast::constant_value(-float_val);
		}
	}
	case ast::function_body::builtin_unary_dereference:
		return {};
	case ast::function_body::builtin_unary_bit_not:
	{
		bz_assert(func_call.params.size() == 1);
		if (exec_kind == function_execution_kind::force_evaluate)
		{
			consteval_try(func_call.params[0], context);
		}
		else if (exec_kind == function_execution_kind::force_evaluate_without_error)
		{
			consteval_try_without_error(func_call.params[0], context);
		}
		if (!func_call.params[0].has_consteval_succeeded())
		{
			return {};
		}
		bz_assert(func_call.params[0].is<ast::constant_expression>());
		auto const &value = func_call.params[0].get<ast::constant_expression>().value;
		if (value.is<ast::constant_value::boolean>())
		{
			auto const bool_val = value.get<ast::constant_value::boolean>();
			return ast::constant_value(!bool_val);
		}
		else
		{
			auto const param_type = ast::remove_const_or_consteval(func_call.params[0].get<ast::constant_expression>().type);
			bz_assert(param_type.is<ast::ts_base_type>());
			auto const param_kind = param_type.get<ast::ts_base_type>().info->kind;
			bz_assert(value.is<ast::constant_value::uint>());
			auto const uint_val = value.get<ast::constant_value::uint>();
			switch (param_kind)
			{
			case ast::type_info::uint8_:
				return ast::constant_value(uint64_t(uint8_t(~uint_val)));
			case ast::type_info::uint16_:
				return ast::constant_value(uint64_t(uint16_t(~uint_val)));
			case ast::type_info::uint32_:
				return ast::constant_value(uint64_t(uint32_t(~uint_val)));
			case ast::type_info::uint64_:
				return ast::constant_value(~uint_val);
			default:
				bz_unreachable;
			}
		}
	}
	case ast::function_body::builtin_unary_bool_not:
	{
		bz_assert(func_call.params.size() == 1);
		if (exec_kind == function_execution_kind::force_evaluate)
		{
			consteval_try(func_call.params[0], context);
		}
		else if (exec_kind == function_execution_kind::force_evaluate_without_error)
		{
			consteval_try_without_error(func_call.params[0], context);
		}
		if (!func_call.params[0].has_consteval_succeeded())
		{
			return {};
		}
		bz_assert(func_call.params[0].is<ast::constant_expression>());
		bz_assert(func_call.params[0].get<ast::constant_expression>().value.is<ast::constant_value::boolean>());
		auto const bool_val = func_call.params[0].get<ast::constant_expression>().value.get<ast::constant_value::boolean>();
		return ast::constant_value(!bool_val);
	}
	case ast::function_body::builtin_unary_plus_plus:
	case ast::function_body::builtin_unary_minus_minus:
		return {};

	case ast::function_body::builtin_binary_assign:
	case ast::function_body::builtin_binary_plus:
	case ast::function_body::builtin_binary_plus_eq:
	case ast::function_body::builtin_binary_minus:
	case ast::function_body::builtin_binary_minus_eq:
	case ast::function_body::builtin_binary_multiply:
	case ast::function_body::builtin_binary_multiply_eq:
	case ast::function_body::builtin_binary_divide:
	case ast::function_body::builtin_binary_divide_eq:
	case ast::function_body::builtin_binary_modulo:
	case ast::function_body::builtin_binary_modulo_eq:
	case ast::function_body::builtin_binary_equals:
	case ast::function_body::builtin_binary_not_equals:
	case ast::function_body::builtin_binary_less_than:
	case ast::function_body::builtin_binary_less_than_eq:
	case ast::function_body::builtin_binary_greater_than:
	case ast::function_body::builtin_binary_greater_than_eq:
	case ast::function_body::builtin_binary_bit_and:
	case ast::function_body::builtin_binary_bit_and_eq:
	case ast::function_body::builtin_binary_bit_xor:
	case ast::function_body::builtin_binary_bit_xor_eq:
	case ast::function_body::builtin_binary_bit_or:
	case ast::function_body::builtin_binary_bit_or_eq:
	case ast::function_body::builtin_binary_bit_left_shift:
	case ast::function_body::builtin_binary_bit_left_shift_eq:
	case ast::function_body::builtin_binary_bit_right_shift:
	case ast::function_body::builtin_binary_bit_right_shift_eq:
		bz_assert(func_call.params.size() == 2);
		if (exec_kind == function_execution_kind::force_evaluate)
		{
			consteval_try(func_call.params[0], context);
			consteval_try(func_call.params[1], context);
		}
		else if (exec_kind == function_execution_kind::force_evaluate_without_error)
		{
			consteval_try_without_error(func_call.params[0], context);
			consteval_try_without_error(func_call.params[1], context);
		}
		if (!func_call.params[0].has_consteval_succeeded() || !func_call.params[1].has_consteval_succeeded())
		{
			return {};
		}
		return evaluate_binary_op(
			original_expr,
			func_call.func_body->function_name_or_operator_kind.get<uint32_t>(),
			func_call.params[0],
			func_call.params[1],
			context
		);

	default:
		bz_unreachable;
	}
}

static ast::constant_value get_default_constructed_value(
	lex::src_tokens const &src_tokens,
	ast::typespec_view type,
	function_execution_kind exec_kind,
	ctx::parse_context &context
)
{
	if (type.is_empty())
	{
		return {};
	}

	type = ast::remove_const_or_consteval(type);
	return type.nodes.front().visit(bz::overload{
		[exec_kind, &src_tokens, &context](ast::ts_base_type const &base_t) -> ast::constant_value {
			if (base_t.info->kind != ast::type_info::aggregate)
			{
				switch (base_t.info->kind)
				{
				case ast::type_info::int8_:
				case ast::type_info::int16_:
				case ast::type_info::int32_:
				case ast::type_info::int64_:
					return ast::constant_value(int64_t());
				case ast::type_info::uint8_:
				case ast::type_info::uint16_:
				case ast::type_info::uint32_:
				case ast::type_info::uint64_:
					return ast::constant_value(uint64_t());
				case ast::type_info::float32_:
					return ast::constant_value(float32_t());
				case ast::type_info::float64_:
					return ast::constant_value(float64_t());
				case ast::type_info::char_:
					return ast::constant_value(bz::u8char());
				case ast::type_info::str_:
					return ast::constant_value(bz::u8string());
				case ast::type_info::bool_:
					return ast::constant_value(bool());
				case ast::type_info::null_t_:
					return ast::constant_value(ast::internal::null_t{});

				default:
					bz_unreachable;
				}
			}
			else if (base_t.info->kind == ast::type_info::aggregate && base_t.info->default_constructor == nullptr)
			{
				ast::constant_value result;
				auto &elems = result.emplace<ast::constant_value::aggregate>();
				elems.reserve(base_t.info->member_variables.size());
				for (auto const member : base_t.info->member_variables)
				{
					elems.push_back(get_default_constructed_value(src_tokens, member->get_type(), exec_kind, context));
					if (elems.back().is_null())
					{
						result.clear();
						return result;
					}
				}
				return result;
			}
			else
			{
				auto const decl = base_t.info->default_constructor;
				bz_assert(decl != nullptr);
				if (exec_kind == function_execution_kind::force_evaluate)
				{
					return context.execute_function(src_tokens, &decl->body, {});
				}
				else if (exec_kind == function_execution_kind::force_evaluate_without_error)
				{
					return context.execute_function_without_error(src_tokens, &decl->body, {});
				}
				else
				{
					return {};
				}
			}
		},
		[exec_kind, &src_tokens, &context](ast::ts_array const &array_t) -> ast::constant_value {
			ast::constant_value result;
			auto const elem_value = get_default_constructed_value(src_tokens, array_t.elem_type, exec_kind, context);
			if (elem_value.is_null())
			{
				return result;
			}
			result.emplace<ast::constant_value::array>(array_t.size, elem_value);
			return result;
		},
		[](ast::ts_array_slice const &) -> ast::constant_value {
			return {};
		},
		[exec_kind, &src_tokens, &context](ast::ts_tuple const &tuple_t) -> ast::constant_value {
			ast::constant_value result;
			auto &elems = result.emplace<ast::constant_value::tuple>();
			elems.reserve(tuple_t.types.size());
			for (auto const &type : tuple_t.types)
			{
				elems.push_back(get_default_constructed_value(src_tokens, type, exec_kind, context));
				if (elems.back().is_null())
				{
					result.clear();
					return result;
				}
			}
			return result;
		},
		[](ast::ts_pointer const &) -> ast::constant_value {
			return ast::constant_value(ast::internal::null_t{});
		},
		[](ast::ts_lvalue_reference const &) -> ast::constant_value {
			return {};
		},
		[](auto const &) -> ast::constant_value {
			bz_unreachable;
		}
	});
}

static ast::constant_value evaluate_function_call(
	ast::expression const &original_expr,
	ast::expr_function_call &func_call,
	function_execution_kind exec_kind,
	ctx::parse_context &context
)
{
	if (func_call.func_body->is_intrinsic() && func_call.func_body->body.is_null())
	{
		return evaluate_intrinsic_function_call(original_expr, func_call, exec_kind, context);
	}
	else if (func_call.func_body->is_default_default_constructor())
	{
		return get_default_constructed_value(original_expr.src_tokens, func_call.func_body->return_type, exec_kind, context);
	}
	else if (func_call.func_body->is_default_copy_constructor())
	{
		bz_assert(func_call.params.size() == 1);
		if (exec_kind == function_execution_kind::force_evaluate)
		{
			consteval_try(func_call.params[0], context);
		}
		else if (exec_kind == function_execution_kind::force_evaluate_without_error)
		{
			consteval_try_without_error(func_call.params[0], context);
		}
		if (!func_call.params[0].has_consteval_succeeded())
		{
			return {};
		}
		bz_assert(func_call.params[0].is<ast::constant_expression>());
		return func_call.params[0].get<ast::constant_expression>().value;
	}
	else if (func_call.func_body->has_builtin_implementation())
	{
		return {};
	}
	else if (exec_kind == function_execution_kind::force_evaluate)
	{
		auto const body = func_call.func_body;
		return context.execute_function(original_expr.src_tokens, body, func_call.params);
	}
	else if (exec_kind == function_execution_kind::force_evaluate_without_error)
	{
		auto const body = func_call.func_body;
		return context.execute_function_without_error(original_expr.src_tokens, body, func_call.params);
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
		case ast::constant_value::boolean:
			return ast::constant_value(static_cast<int64_t>(value.get<ast::constant_value::boolean>()));
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
		case ast::constant_value::boolean:
			return ast::constant_value(static_cast<uint64_t>(value.get<ast::constant_value::boolean>()));
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
		[](ast::expr_integer_literal &) -> ast::constant_value {
			// these are always constant expressions
			bz_unreachable;
		},
		[](ast::expr_null_literal &) -> ast::constant_value {
			// these are always constant expressions
			bz_unreachable;
		},
		[](ast::expr_typed_literal &) -> ast::constant_value {
			// these are always constant expressions
			bz_unreachable;
		},
		[&expr, &context](ast::expr_tuple &tuple) -> ast::constant_value {
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
			auto const expr_type = ast::remove_const_or_consteval(expr.get_expr_type_and_kind().first);
			auto &elem_values = expr_type.is<ast::ts_array>()
				? (result.emplace<ast::constant_value::array>(), result.get<ast::constant_value::array>())
				: (result.emplace<ast::constant_value::tuple>(), result.get<ast::constant_value::tuple>());
			elem_values.reserve(tuple.elems.size());
			for (auto &elem : tuple.elems)
			{
				bz_assert(elem.is<ast::constant_expression>());
				elem_values.emplace_back(elem.get<ast::constant_expression>().value);
			}
			return result;
		},
		[&context](ast::expr_unary_op &unary_op) -> ast::constant_value {
			// builtin operators are handled as intrinsic functions
			consteval_guaranteed(unary_op.expr, context);
			return {};
		},
		[&expr, &context](ast::expr_binary_op &binary_op) -> ast::constant_value {
			consteval_guaranteed(binary_op.lhs, context);
			consteval_guaranteed(binary_op.rhs, context);

			// special case for bool_and and bool_or shortcircuiting
			if (binary_op.lhs.has_consteval_succeeded())
			{
				auto const op = binary_op.op;
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
				return evaluate_binary_op(expr, binary_op.op, binary_op.lhs, binary_op.rhs, context);
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
			for (auto &param : func_call.params)
			{
				consteval_guaranteed(param, context);
			}
			return evaluate_function_call(expr, func_call, function_execution_kind::guaranteed_evaluate, context);
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
		[](ast::expr_take_reference const &) -> ast::constant_value {
			return {};
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
		[&context](ast::expr_array_default_construct &array_default_construct_expr) -> ast::constant_value {
			auto const type = array_default_construct_expr.type.as_typespec_view();
			bz_assert(type.is<ast::ts_array>());
			consteval_guaranteed(array_default_construct_expr.elem_ctor_call, context);
			if (!array_default_construct_expr.elem_ctor_call.is<ast::constant_expression>())
			{
				return {};
			}

			auto const &value = array_default_construct_expr.elem_ctor_call.get<ast::constant_expression>().value;
			ast::constant_value result;
			result.emplace<ast::constant_value::array>(type.get<ast::ts_array>().size, value);
			return result;
		},
		[](ast::expr_builtin_default_construct &builtin_default_construct_expr) -> ast::constant_value {
			auto const type = builtin_default_construct_expr.type.as_typespec_view();
			if (type.is<ast::ts_pointer>())
			{
				return ast::constant_value(ast::internal::null_t{});
			}
			else
			{
				return {};
			}
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
		[](ast::expr_type_member_access &) -> ast::constant_value {
			// variable constevalness is handled in parse_context::make_member_access_expression
			return {};
		},
		[&context](ast::expr_compound &compound_expr) -> ast::constant_value {
			if (compound_expr.statements.empty() && compound_expr.final_expr.not_null())
			{
				consteval_guaranteed(compound_expr.final_expr, context);
				if (compound_expr.final_expr.has_consteval_succeeded())
				{
					return compound_expr.final_expr.get<ast::constant_expression>().value;
				}
			}
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
		[&context](ast::expr_if_consteval &if_expr) -> ast::constant_value {
			bz_assert(if_expr.condition.is<ast::constant_expression>());
			auto const &condition_value = if_expr.condition.get<ast::constant_expression>().value;
			bz_assert(condition_value.is<ast::constant_value::boolean>());
			if (condition_value.get<ast::constant_value::boolean>())
			{
				consteval_guaranteed(if_expr.then_block, context);
				if (if_expr.then_block.has_consteval_succeeded())
				{
					bz_assert(if_expr.then_block.is<ast::constant_expression>());
					return if_expr.then_block.get<ast::constant_expression>().value;
				}
				else
				{
					return {};
				}
			}
			else if (if_expr.else_block.not_null())
			{
				consteval_guaranteed(if_expr.else_block, context);
				if (if_expr.else_block.has_consteval_succeeded())
				{
					bz_assert(if_expr.else_block.is<ast::constant_expression>());
					return if_expr.else_block.get<ast::constant_expression>().value;
				}
				else
				{
					return {};
				}
			}
			else
			{
				return ast::constant_value::get_void();
			}
		},
		[&context](ast::expr_switch &switch_expr) -> ast::constant_value {
			consteval_guaranteed(switch_expr.matched_expr, context);
			for (auto &[_, case_expr] : switch_expr.cases)
			{
				consteval_guaranteed(case_expr, context);
			}
			consteval_guaranteed(switch_expr.default_case, context);

			if (!switch_expr.matched_expr.has_consteval_succeeded())
			{
				return {};
			}

			auto const &matched_value = switch_expr.matched_expr.get<ast::constant_expression>().value;
			auto const case_it = std::find_if(
				switch_expr.cases.begin(), switch_expr.cases.end(),
				[&](auto const &switch_case) {
					return switch_case.values
						.transform([](auto const &expr) -> auto const &{ return expr.template get<ast::constant_expression>().value; })
						.is_any(matched_value);
				}
			);
			if (case_it == switch_expr.cases.end())
			{
				if (switch_expr.default_case.has_consteval_succeeded())
				{
					return switch_expr.default_case.get<ast::constant_expression>().value;
				}
				else
				{
					return {};
				}
			}
			else
			{
				if (case_it->expr.has_consteval_succeeded())
				{
					return case_it->expr.get<ast::constant_expression>().value;
				}
				else
				{
					return {};
				}
			}
		},
		[](ast::expr_break &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_continue &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_unreachable &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_generic_type_instantiation &) -> ast::constant_value {
			bz_unreachable;
		},
	});
}

static ast::constant_value try_evaluate_expr(
	ast::expression &expr,
	ctx::parse_context &context
)
{
	bz_assert(!expr.has_consteval_succeeded());

	return expr.get_expr().visit(bz::overload{
		[](ast::expr_identifier &) -> ast::constant_value {
			// identifiers are only constant expressions if they are a consteval
			// variable, which is handled in parse_context::make_identifier_expr (or something similar)
			return {};
		},
		[](ast::expr_integer_literal &) -> ast::constant_value {
			// these are always constant expressions
			bz_unreachable;
		},
		[](ast::expr_null_literal &) -> ast::constant_value {
			// these are always constant expressions
			bz_unreachable;
		},
		[](ast::expr_typed_literal &) -> ast::constant_value {
			// these are always constant expressions
			bz_unreachable;
		},
		[&expr, &context](ast::expr_tuple &tuple) -> ast::constant_value {
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
			auto &elem_values = expr.get_expr_type_and_kind().first.is<ast::ts_array>()
				? (result.emplace<ast::constant_value::array>(), result.get<ast::constant_value::array>())
				: (result.emplace<ast::constant_value::tuple>(), result.get<ast::constant_value::tuple>());
			elem_values.reserve(tuple.elems.size());
			for (auto &elem : tuple.elems)
			{
				bz_assert(elem.is<ast::constant_expression>());
				elem_values.emplace_back(elem.get<ast::constant_expression>().value);
			}
			return result;
		},
		[&context](ast::expr_unary_op &unary_op) -> ast::constant_value {
			// builtin operators are handled as intrinsic functions
			consteval_try(unary_op.expr, context);
			return {};
		},
		[&expr, &context](ast::expr_binary_op &binary_op) -> ast::constant_value {
			consteval_try(binary_op.lhs, context);

			// special case for bool_and and bool_or shortcircuiting
			if (binary_op.lhs.has_consteval_succeeded())
			{
				auto const op = binary_op.op;
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

			consteval_try(binary_op.rhs, context);

			if (binary_op.lhs.has_consteval_succeeded() && binary_op.rhs.has_consteval_succeeded())
			{
				return evaluate_binary_op(expr, binary_op.op, binary_op.lhs, binary_op.rhs, context);
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
			for (auto &param : func_call.params)
			{
				consteval_guaranteed(param, context);
			}
			return evaluate_function_call(expr, func_call, function_execution_kind::force_evaluate, context);
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
		[](ast::expr_take_reference const &) -> ast::constant_value {
			return {};
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
		[&context](ast::expr_array_default_construct &array_default_construct_expr) -> ast::constant_value {
			auto const type = array_default_construct_expr.type.as_typespec_view();
			bz_assert(type.is<ast::ts_array>());
			consteval_try(array_default_construct_expr.elem_ctor_call, context);
			if (!array_default_construct_expr.elem_ctor_call.is<ast::constant_expression>())
			{
				return {};
			}

			auto const &value = array_default_construct_expr.elem_ctor_call.get<ast::constant_expression>().value;
			ast::constant_value result;
			result.emplace<ast::constant_value::array>(type.get<ast::ts_array>().size, value);
			return result;
		},
		[](ast::expr_builtin_default_construct &builtin_default_construct_expr) -> ast::constant_value {
			auto const type = builtin_default_construct_expr.type.as_typespec_view();
			if (type.is<ast::ts_pointer>())
			{
				return ast::constant_value(ast::internal::null_t{});
			}
			else
			{
				return {};
			}
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
		[](ast::expr_type_member_access &) -> ast::constant_value {
			// variable constevalness is handled in parse_context::make_member_access_expression
			return {};
		},
		[&expr, &context](ast::expr_compound &compound_expr) -> ast::constant_value {
			if (compound_expr.statements.empty() && compound_expr.final_expr.not_null())
			{
				consteval_try(compound_expr.final_expr, context);
				if (compound_expr.final_expr.has_consteval_succeeded())
				{
					return compound_expr.final_expr.get<ast::constant_expression>().value;
				}
				else
				{
					return {};
				}
			}
			else
			{
				return context.execute_compound_expression(expr.src_tokens, compound_expr);
			}
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
		[&context](ast::expr_if_consteval &if_expr) -> ast::constant_value {
			bz_assert(if_expr.condition.is<ast::constant_expression>());
			auto const &condition_value = if_expr.condition.get<ast::constant_expression>().value;
			bz_assert(condition_value.is<ast::constant_value::boolean>());
			if (condition_value.get<ast::constant_value::boolean>())
			{
				consteval_try(if_expr.then_block, context);
				if (if_expr.then_block.has_consteval_succeeded())
				{
					bz_assert(if_expr.then_block.is<ast::constant_expression>());
					return if_expr.then_block.get<ast::constant_expression>().value;
				}
				else
				{
					return {};
				}
			}
			else if (if_expr.else_block.not_null())
			{
				consteval_try(if_expr.else_block, context);
				if (if_expr.else_block.has_consteval_succeeded())
				{
					bz_assert(if_expr.else_block.is<ast::constant_expression>());
					return if_expr.else_block.get<ast::constant_expression>().value;
				}
				else
				{
					return {};
				}
			}
			else
			{
				return ast::constant_value::get_void();
			}
		},
		[&context](ast::expr_switch &switch_expr) -> ast::constant_value {
			consteval_try(switch_expr.matched_expr, context);
			for (auto &[_, case_expr] : switch_expr.cases)
			{
				consteval_try(case_expr, context);
			}
			consteval_try(switch_expr.default_case, context);

			if (!switch_expr.matched_expr.has_consteval_succeeded())
			{
				return {};
			}

			auto const &matched_value = switch_expr.matched_expr.get<ast::constant_expression>().value;
			auto const case_it = std::find_if(
				switch_expr.cases.begin(), switch_expr.cases.end(),
				[&](auto const &switch_case) {
					return switch_case.values
						.transform([](auto const &expr) -> auto const &{ return expr.template get<ast::constant_expression>().value; })
						.is_any(matched_value);
				}
			);
			if (case_it == switch_expr.cases.end())
			{
				if (switch_expr.default_case.has_consteval_succeeded())
				{
					return switch_expr.default_case.get<ast::constant_expression>().value;
				}
				else
				{
					return {};
				}
			}
			else
			{
				if (case_it->expr.has_consteval_succeeded())
				{
					return case_it->expr.get<ast::constant_expression>().value;
				}
				else
				{
					return {};
				}
			}
		},
		[](ast::expr_break &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_continue &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_unreachable &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_generic_type_instantiation &) -> ast::constant_value {
			bz_unreachable;
		},
	});
}

static ast::constant_value try_evaluate_expr_without_error(
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
		[](ast::expr_integer_literal &) -> ast::constant_value {
			// these are always constant expressions
			bz_unreachable;
		},
		[](ast::expr_null_literal &) -> ast::constant_value {
			// these are always constant expressions
			bz_unreachable;
		},
		[](ast::expr_typed_literal &) -> ast::constant_value {
			// these are always constant expressions
			bz_unreachable;
		},
		[&expr, &context](ast::expr_tuple &tuple) -> ast::constant_value {
			bool is_consteval = true;
			for (auto &elem : tuple.elems)
			{
				consteval_try_without_error(elem, context);
				is_consteval = is_consteval && elem.has_consteval_succeeded();
			}
			if (!is_consteval)
			{
				return {};
			}

			ast::constant_value result;
			auto &elem_values = expr.get_expr_type_and_kind().first.is<ast::ts_array>()
				? (result.emplace<ast::constant_value::array>(), result.get<ast::constant_value::array>())
				: (result.emplace<ast::constant_value::tuple>(), result.get<ast::constant_value::tuple>());
			elem_values.reserve(tuple.elems.size());
			for (auto &elem : tuple.elems)
			{
				bz_assert(elem.is<ast::constant_expression>());
				elem_values.emplace_back(elem.get<ast::constant_expression>().value);
			}
			return result;
		},
		[&context](ast::expr_unary_op &unary_op) -> ast::constant_value {
			// builtin operators are handled as intrinsic functions
			consteval_try_without_error(unary_op.expr, context);
			return {};
		},
		[&expr, &context](ast::expr_binary_op &binary_op) -> ast::constant_value {
			consteval_try_without_error(binary_op.lhs, context);
			consteval_try_without_error(binary_op.rhs, context);

			// special case for bool_and and bool_or shortcircuiting
			if (binary_op.lhs.has_consteval_succeeded())
			{
				auto const op = binary_op.op;
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
				return evaluate_binary_op(expr, binary_op.op, binary_op.lhs, binary_op.rhs, context);
			}
			else
			{
				return {};
			}
		},
		[&context](ast::expr_subscript &subscript_expr) -> ast::constant_value {
			consteval_try_without_error(subscript_expr.base, context);
			consteval_try_without_error(subscript_expr.index, context);

			return evaluate_subscript(subscript_expr, context);
		},
		[&expr, &context](ast::expr_function_call &func_call) -> ast::constant_value {
			for (auto &param : func_call.params)
			{
				consteval_guaranteed(param, context);
			}
			return evaluate_function_call(expr, func_call, function_execution_kind::force_evaluate_without_error, context);
		},
		[&expr, &context](ast::expr_cast &cast_expr) -> ast::constant_value {
			consteval_try_without_error(cast_expr.expr, context);
			if (cast_expr.expr.has_consteval_succeeded())
			{
				return evaluate_cast(expr, cast_expr, context);
			}
			else
			{
				return {};
			}
		},
		[](ast::expr_take_reference const &) -> ast::constant_value {
			return {};
		},
		[&context](ast::expr_struct_init &struct_init_expr) -> ast::constant_value {
			bool is_consteval = true;
			for (auto &expr : struct_init_expr.exprs)
			{
				consteval_try_without_error(expr, context);
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
		[&context](ast::expr_array_default_construct &array_default_construct_expr) -> ast::constant_value {
			auto const type = array_default_construct_expr.type.as_typespec_view();
			bz_assert(type.is<ast::ts_array>());
			consteval_try_without_error(array_default_construct_expr.elem_ctor_call, context);
			if (!array_default_construct_expr.elem_ctor_call.is<ast::constant_expression>())
			{
				return {};
			}

			auto const &value = array_default_construct_expr.elem_ctor_call.get<ast::constant_expression>().value;
			ast::constant_value result;
			result.emplace<ast::constant_value::array>(type.get<ast::ts_array>().size, value);
			return result;
		},
		[](ast::expr_builtin_default_construct &builtin_default_construct_expr) -> ast::constant_value {
			auto const type = builtin_default_construct_expr.type.as_typespec_view();
			if (type.is<ast::ts_pointer>())
			{
				return ast::constant_value(ast::internal::null_t{});
			}
			else
			{
				return {};
			}
		},
		[&context](ast::expr_member_access &member_access_expr) -> ast::constant_value {
			consteval_try_without_error(member_access_expr.base, context);
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
		[](ast::expr_type_member_access &) -> ast::constant_value {
			// variable constevalness is handled in parse_context::make_member_access_expression
			return {};
		},
		[&expr, &context](ast::expr_compound &compound_expr) -> ast::constant_value {
			if (compound_expr.statements.empty() && compound_expr.final_expr.not_null())
			{
				consteval_try_without_error(compound_expr.final_expr, context);
				if (compound_expr.final_expr.has_consteval_succeeded())
				{
					return compound_expr.final_expr.get<ast::constant_expression>().value;
				}
				else
				{
					return {};
				}
			}
			else
			{
				return context.execute_compound_expression_without_error(expr.src_tokens, compound_expr);
			}
		},
		[&context](ast::expr_if &if_expr) -> ast::constant_value {
			consteval_try_without_error(if_expr.condition, context);
			consteval_try_without_error(if_expr.then_block, context);
			consteval_try_without_error(if_expr.else_block, context);
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
		[&context](ast::expr_if_consteval &if_expr) -> ast::constant_value {
			bz_assert(if_expr.condition.is<ast::constant_expression>());
			auto const &condition_value = if_expr.condition.get<ast::constant_expression>().value;
			bz_assert(condition_value.is<ast::constant_value::boolean>());
			if (condition_value.get<ast::constant_value::boolean>())
			{
				consteval_try_without_error(if_expr.then_block, context);
				if (if_expr.then_block.has_consteval_succeeded())
				{
					bz_assert(if_expr.then_block.is<ast::constant_expression>());
					return if_expr.then_block.get<ast::constant_expression>().value;
				}
				else
				{
					return {};
				}
			}
			else if (if_expr.else_block.not_null())
			{
				consteval_try_without_error(if_expr.else_block, context);
				if (if_expr.else_block.has_consteval_succeeded())
				{
					bz_assert(if_expr.else_block.is<ast::constant_expression>());
					return if_expr.else_block.get<ast::constant_expression>().value;
				}
				else
				{
					return {};
				}
			}
			else
			{
				return ast::constant_value::get_void();
			}
		},
		[&context](ast::expr_switch &switch_expr) -> ast::constant_value {
			consteval_try_without_error(switch_expr.matched_expr, context);
			for (auto &[_, case_expr] : switch_expr.cases)
			{
				consteval_try_without_error(case_expr, context);
			}
			consteval_try_without_error(switch_expr.default_case, context);

			if (!switch_expr.matched_expr.has_consteval_succeeded())
			{
				return {};
			}

			auto const &matched_value = switch_expr.matched_expr.get<ast::constant_expression>().value;
			auto const case_it = std::find_if(
				switch_expr.cases.begin(), switch_expr.cases.end(),
				[&](auto const &switch_case) {
					return switch_case.values
						.transform([](auto const &expr) -> auto const &{ return expr.template get<ast::constant_expression>().value; })
						.is_any(matched_value);
				}
			);
			if (case_it == switch_expr.cases.end())
			{
				if (switch_expr.default_case.has_consteval_succeeded())
				{
					return switch_expr.default_case.get<ast::constant_expression>().value;
				}
				else
				{
					return {};
				}
			}
			else
			{
				if (case_it->expr.has_consteval_succeeded())
				{
					return case_it->expr.get<ast::constant_expression>().value;
				}
				else
				{
					return {};
				}
			}
		},
		[](ast::expr_break &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_continue &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_unreachable &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_generic_type_instantiation &) -> ast::constant_value {
			bz_unreachable;
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
		|| (
			expr.consteval_state != ast::expression::consteval_never_tried
			&& expr.consteval_state != ast::expression::consteval_guaranteed_failed
		)
	)
	{
		expr.consteval_state = ast::expression::consteval_failed;
		return;
	}
	else if (expr.consteval_state == ast::expression::consteval_guaranteed_failed)
	{
		return;
	}

	if (context.is_aggressive_consteval_enabled)
	{
		consteval_try_without_error(expr, context);
		return;
	}

	auto const value = guaranteed_evaluate_expr(expr, context);
	if (value.is_null())
	{
		expr.consteval_state = ast::expression::consteval_guaranteed_failed;
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
		|| (
			expr.consteval_state != ast::expression::consteval_never_tried
			&& expr.consteval_state != ast::expression::consteval_guaranteed_failed
		)
	)
	{
		expr.consteval_state = ast::expression::consteval_failed;
		return;
	}

	auto value = try_evaluate_expr(expr, context);
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
			std::move(value), std::move(inner_expr)
		);
		expr.consteval_state = ast::expression::consteval_succeeded;
		return;
	}
}

void consteval_try_without_error(ast::expression &expr, ctx::parse_context &context)
{
	if (expr.is<ast::constant_expression>())
	{
		expr.consteval_state = ast::expression::consteval_succeeded;
		return;
	}
	else if (
		!expr.is<ast::dynamic_expression>()
		|| (
			expr.consteval_state != ast::expression::consteval_never_tried
			&& expr.consteval_state != ast::expression::consteval_guaranteed_failed
		)
	)
	{
		expr.consteval_state = ast::expression::consteval_failed;
		return;
	}

	auto value = try_evaluate_expr_without_error(expr, context);
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
			std::move(value), std::move(inner_expr)
		);
		expr.consteval_state = ast::expression::consteval_succeeded;
		return;
	}
}


void consteval_try_without_error_decl(ast::statement &stmt, ctx::parse_context &context)
{
	if (stmt.is_null())
	{
		return;
	}
	stmt.visit(bz::overload{
		[&context](ast::stmt_while &while_stmt) {
			consteval_try_without_error(while_stmt.condition, context);
			consteval_try_without_error(while_stmt.while_block, context);
		},
		[&context](ast::stmt_for &for_stmt) {
			consteval_try_without_error_decl(for_stmt.init, context);
			consteval_try_without_error(for_stmt.condition, context);
			consteval_try_without_error(for_stmt.iteration, context);
			consteval_try_without_error(for_stmt.for_block, context);
		},
		[&context](ast::stmt_foreach &foreach_stmt) {
			consteval_try_without_error_decl(foreach_stmt.range_var_decl, context);
			consteval_try_without_error_decl(foreach_stmt.iter_var_decl, context);
			consteval_try_without_error_decl(foreach_stmt.end_var_decl, context);
			consteval_try_without_error(foreach_stmt.condition, context);
			consteval_try_without_error(foreach_stmt.iteration, context);
			consteval_try_without_error_decl(foreach_stmt.iter_deref_var_decl, context);
			consteval_try_without_error(foreach_stmt.for_block, context);
		},
		[&context](ast::stmt_return &return_stmt) {
			consteval_try_without_error(return_stmt.expr, context);
		},
		[](ast::stmt_no_op &) {},
		[](ast::stmt_static_assert &) {},
		[&context](ast::stmt_expression &expr_stmt) {
			consteval_try_without_error(expr_stmt.expr, context);
		},
		[&context](ast::decl_variable &var_decl) {
			consteval_try_without_error(var_decl.init_expr, context);
			// could update the variable to have a consteval type for better constant propagation
		},
		[&context](ast::decl_function &func_decl) {
			auto const evaluate_func_body = [&context](ast::function_body &body) {
				bz_assert(!body.is_generic());
				if (body.body.is<bz::vector<ast::statement>>())
				{
					for (auto &stmt : body.body.get<bz::vector<ast::statement>>())
					{
						consteval_try_without_error_decl(stmt, context);
					}
				}
			};

			if (func_decl.body.is_generic())
			{
				for (auto &specialization : func_decl.body.generic_specializations)
				{
					evaluate_func_body(*specialization);
				}
			}
			else
			{
				evaluate_func_body(func_decl.body);
			}
		},
		[&context](ast::decl_operator &op_decl) {
			auto const evaluate_func_body = [&context](ast::function_body &body) {
				bz_assert(!body.is_generic());
				if (body.body.is<bz::vector<ast::statement>>())
				{
					for (auto &stmt : body.body.get<bz::vector<ast::statement>>())
					{
						consteval_try_without_error_decl(stmt, context);
					}
				}
			};

			if (op_decl.body.is_generic())
			{
				for (auto &specialization : op_decl.body.generic_specializations)
				{
					evaluate_func_body(*specialization);
				}
			}
			else
			{
				evaluate_func_body(op_decl.body);
			}
		},
		[](ast::decl_function_alias &) {},
		[](ast::decl_type_alias &) {},
		[&context](ast::decl_struct &struct_decl) {
			if (struct_decl.info.body.is<bz::vector<ast::statement>>())
			{
				for (auto &stmt : struct_decl.info.body.get<bz::vector<ast::statement>>())
				{
					consteval_try_without_error_decl(stmt, context);
				}
			}
		},
		[](ast::decl_import &) {},
	});
}


static void get_consteval_fail_notes_helper(ast::expression const &expr, bz::vector<ctx::source_highlight> &notes)
{
	if (expr.is_error())
	{
		return;
	}
	bz_assert(expr.has_consteval_failed());
	bz_assert(expr.is<ast::dynamic_expression>());
	expr.get_expr().visit(bz::overload{
		[&expr, &notes](ast::expr_identifier const &id_expr) {
			notes.emplace_back(ctx::parse_context::make_note(
				expr.src_tokens, "subexpression is not a constant expression"
			));
			if (id_expr.decl != nullptr)
			{
				notes.emplace_back(ctx::parse_context::make_note(
					id_expr.decl->src_tokens,
					bz::format("variable '{}' was declared here", id_expr.decl->get_id().format_as_unqualified())
				));
			}
		},
		[](ast::expr_integer_literal const &) {
			// these are always constant expressions
			bz_unreachable;
		},
		[](ast::expr_null_literal const &) {
			// these are always constant expressions
			bz_unreachable;
		},
		[](ast::expr_typed_literal const &) {
			// these are always constant expressions
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
						token_info[unary_op.op].token_value,
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
						token_info[binary_op.op].token_value,
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
		[&expr, &notes](ast::expr_take_reference const &take_ref_expr) {
			if (take_ref_expr.expr.is<ast::constant_expression>())
			{
				notes.emplace_back(ctx::parse_context::make_note(
					expr.src_tokens, "unable to take reference in a constant expression"
				));
			}
			else
			{
				get_consteval_fail_notes_helper(take_ref_expr.expr, notes);
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
		[&expr, &notes](ast::expr_array_default_construct const &array_default_construct_expr) {
			auto const type = array_default_construct_expr.type.as_typespec_view();
			notes.emplace_back(ctx::parse_context::make_note(
				expr.src_tokens, bz::format("subexpression '{}()' is not a constant expression", type)
			));
		},
		[&expr, &notes](ast::expr_builtin_default_construct const &builtin_default_construct_expr) {
			auto const type = builtin_default_construct_expr.type.as_typespec_view();
			notes.emplace_back(ctx::parse_context::make_note(
				expr.src_tokens, bz::format("subexpression '{}()' is not a constant expression", type)
			));
		},
		[&notes](ast::expr_member_access const &member_access_expr) {
			bz_assert(!member_access_expr.base.has_consteval_succeeded());
			get_consteval_fail_notes_helper(member_access_expr.base, notes);
		},
		[&expr, &notes](ast::expr_type_member_access const &) {
			notes.emplace_back(ctx::parse_context::make_note(
				expr.src_tokens, "subexpression is not a constant expression"
			));
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
		[&notes](ast::expr_if_consteval const &if_expr) {
			bz_assert(if_expr.condition.is<ast::constant_expression>());
			auto const &condition_value = if_expr.condition.get<ast::constant_expression>().value;
			bz_assert(condition_value.is<ast::constant_value::boolean>());
			if (condition_value.get<ast::constant_value::boolean>())
			{
				get_consteval_fail_notes_helper(if_expr.then_block, notes);
			}
			else
			{
				bz_assert(if_expr.else_block.not_null());
				get_consteval_fail_notes_helper(if_expr.else_block, notes);
			}
		},
		[&expr, &notes](ast::expr_switch const &) {
			notes.emplace_back(ctx::parse_context::make_note(
				expr.src_tokens, "subexpression is not a constant expression"
			));
		},
		[&expr, &notes](ast::expr_break const &) {
			notes.emplace_back(ctx::parse_context::make_note(
				expr.src_tokens, "'break' is not a constant expression"
			));
		},
		[&expr, &notes](ast::expr_continue const &) {
			notes.emplace_back(ctx::parse_context::make_note(
				expr.src_tokens, "'continue' is not a constant expression"
			));
		},
		[&expr, &notes](ast::expr_unreachable const &) {
			notes.emplace_back(ctx::parse_context::make_note(
				expr.src_tokens, "'unreachable' is not a constant expression"
			));
		},
		[](ast::expr_generic_type_instantiation const &) {
			bz_unreachable;
		},
	});
}

bz::vector<ctx::source_highlight> get_consteval_fail_notes(ast::expression const &expr)
{
	bz::vector<ctx::source_highlight> result = {};
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
