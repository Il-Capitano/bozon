#include "consteval.h"
#include "safe_operations.h"
#include "global_data.h"
#include "resolve/match_expression.h"

namespace resolve
{

struct flattened_array_info_t
{
	ast::typespec_view elem_type;
	size_t size;
	bool is_multi_dimensional;
};

static flattened_array_info_t get_flattened_array_type_and_size(ast::typespec_view type)
{
	size_t size = type.get<ast::ts_array>().size;
	bool is_multi_dimensional = false;
	type = type.get<ast::ts_array>().elem_type;
	while (type.is<ast::ts_array>())
	{
		size *= type.get<ast::ts_array>().size;
		type = type.get<ast::ts_array>().elem_type;
		is_multi_dimensional = true;
	}
	return { type, size, is_multi_dimensional };
}

static ast::constant_value evaluate_binary_plus(
	ast::expression const &original_expr,
	ast::expression const &lhs, ast::expression const &rhs,
	ctx::parse_context &context
)
{
	bz_assert(lhs.is_constant());
	auto const &lhs_const_expr = lhs.get_constant();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is_constant());
	auto const &rhs_const_expr = rhs.get_constant();
	auto const &rhs_value = rhs_const_expr.value;

	if (lhs_value.kind() == rhs_value.kind())
	{
		bz_assert(ast::remove_const_or_consteval(lhs_const_expr.type).is<ast::ts_base_type>());
		auto const type = ast::remove_const_or_consteval(lhs_const_expr.type).get<ast::ts_base_type>().info->kind;
		switch (lhs_value.kind())
		{
		static_assert(ast::constant_value::variant_count == 21);
		case ast::constant_value::sint:
		{
			auto const lhs_int_val = lhs_value.get_sint();
			auto const rhs_int_val = rhs_value.get_sint();
			return ast::constant_value(safe_binary_plus(
				original_expr.src_tokens, original_expr.paren_level,
				lhs_int_val, rhs_int_val, type, context
			));
		}
		case ast::constant_value::uint:
		{
			auto const lhs_int_val = lhs_value.get_uint();
			auto const rhs_int_val = rhs_value.get_uint();
			return ast::constant_value(safe_binary_plus(
				original_expr.src_tokens, original_expr.paren_level,
				lhs_int_val, rhs_int_val, type, context
			));
		}
		case ast::constant_value::float32:
		{
			auto const lhs_float_val = lhs_value.get_float32();
			auto const rhs_float_val = rhs_value.get_float32();
			return ast::constant_value(safe_binary_plus(
				original_expr.src_tokens, original_expr.paren_level,
				lhs_float_val, rhs_float_val, context
			));
		}
		case ast::constant_value::float64:
		{
			auto const lhs_float_val = lhs_value.get_float64();
			auto const rhs_float_val = rhs_value.get_float64();
			return ast::constant_value(safe_binary_plus(
				original_expr.src_tokens, original_expr.paren_level,
				lhs_float_val, rhs_float_val, context
			));
		}
		default:
			bz_unreachable;
		}
	}
	else if (lhs_value.is_u8char())
	{
		bz_assert(rhs_value.is_sint() || rhs_value.is_uint());

		auto const result = rhs_value.is_sint()
			? safe_binary_plus(
				original_expr.src_tokens, original_expr.paren_level,
				lhs_value.get_u8char(), rhs_value.get_sint(),
				context
			)
			: safe_binary_plus(
				original_expr.src_tokens, original_expr.paren_level,
				lhs_value.get_u8char(), rhs_value.get_uint(),
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
		bz_assert(rhs_value.is_u8char());
		bz_assert(lhs_value.is_sint() || lhs_value.is_uint());

		auto const result = lhs_value.is_sint()
			? safe_binary_plus(
				original_expr.src_tokens, original_expr.paren_level,
				lhs_value.get_sint(), rhs_value.get_u8char(),
				context
			)
			: safe_binary_plus(
				original_expr.src_tokens, original_expr.paren_level,
				lhs_value.get_uint(), rhs_value.get_u8char(),
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
	bz_assert(lhs.is_constant());
	auto const &lhs_const_expr = lhs.get_constant();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is_constant());
	auto const &rhs_const_expr = rhs.get_constant();
	auto const &rhs_value = rhs_const_expr.value;

	if (lhs_value.kind() == rhs_value.kind())
	{
		bz_assert(ast::remove_const_or_consteval(lhs_const_expr.type).is<ast::ts_base_type>());
		auto const type = ast::remove_const_or_consteval(lhs_const_expr.type).get<ast::ts_base_type>().info->kind;
		switch (lhs_value.kind())
		{
		static_assert(ast::constant_value::variant_count == 21);
		case ast::constant_value::sint:
		{
			auto const lhs_int_val = lhs_value.get_sint();
			auto const rhs_int_val = rhs_value.get_sint();
			return ast::constant_value(safe_binary_minus(
				original_expr.src_tokens, original_expr.paren_level,
				lhs_int_val, rhs_int_val, type, context
			));
		}
		case ast::constant_value::uint:
		{
			auto const lhs_int_val = lhs_value.get_uint();
			auto const rhs_int_val = rhs_value.get_uint();
			return ast::constant_value(safe_binary_minus(
				original_expr.src_tokens, original_expr.paren_level,
				lhs_int_val, rhs_int_val, type, context
			));
		}
		case ast::constant_value::float32:
		{
			auto const lhs_float_val = lhs_value.get_float32();
			auto const rhs_float_val = rhs_value.get_float32();
			return ast::constant_value(safe_binary_minus(
				original_expr.src_tokens, original_expr.paren_level,
				lhs_float_val, rhs_float_val, context
			));
		}
		case ast::constant_value::float64:
		{
			auto const lhs_float_val = lhs_value.get_float64();
			auto const rhs_float_val = rhs_value.get_float64();
			return ast::constant_value(safe_binary_minus(
				original_expr.src_tokens, original_expr.paren_level,
				lhs_float_val, rhs_float_val, context
			));
		}
		case ast::constant_value::u8char:
			return ast::constant_value(
				static_cast<int64_t>(lhs_value.get_u8char())
				- static_cast<int64_t>(rhs_value.get_u8char())
			);
		default:
			bz_unreachable;
		}
	}
	else
	{
		bz_assert(lhs_value.is_u8char());
		bz_assert(rhs_value.is_sint() || rhs_value.is_uint());

		auto const result = rhs_value.is_sint()
			? safe_binary_minus(
				original_expr.src_tokens, original_expr.paren_level,
				lhs_value.get_u8char(), rhs_value.get_sint(),
				context
			)
			: safe_binary_minus(
				original_expr.src_tokens, original_expr.paren_level,
				lhs_value.get_u8char(), rhs_value.get_uint(),
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
	bz_assert(lhs.is_constant());
	auto const &lhs_const_expr = lhs.get_constant();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is_constant());
	auto const &rhs_const_expr = rhs.get_constant();
	auto const &rhs_value = rhs_const_expr.value;
	bz_assert(lhs_value.kind() == rhs_value.kind());

	bz_assert(ast::remove_const_or_consteval(lhs_const_expr.type).is<ast::ts_base_type>());
	auto const type = ast::remove_const_or_consteval(lhs_const_expr.type).get<ast::ts_base_type>().info->kind;
	switch (lhs_value.kind())
	{
	static_assert(ast::constant_value::variant_count == 21);
	case ast::constant_value::sint:
	{
		auto const lhs_int_val = lhs_value.get_sint();
		auto const rhs_int_val = rhs_value.get_sint();
		return ast::constant_value(safe_binary_multiply(
			original_expr.src_tokens, original_expr.paren_level,
			lhs_int_val, rhs_int_val, type, context
		));
	}
	case ast::constant_value::uint:
	{
		auto const lhs_int_val = lhs_value.get_uint();
		auto const rhs_int_val = rhs_value.get_uint();
		return ast::constant_value(safe_binary_multiply(
			original_expr.src_tokens, original_expr.paren_level,
			lhs_int_val, rhs_int_val, type, context
		));
	}
	case ast::constant_value::float32:
	{
		auto const lhs_float_val = lhs_value.get_float32();
		auto const rhs_float_val = rhs_value.get_float32();
		return ast::constant_value(safe_binary_multiply(
			original_expr.src_tokens, original_expr.paren_level,
			lhs_float_val, rhs_float_val, context
		));
	}
	case ast::constant_value::float64:
	{
		auto const lhs_float_val = lhs_value.get_float64();
		auto const rhs_float_val = rhs_value.get_float64();
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
	bz_assert(lhs.is_constant());
	auto const &lhs_const_expr = lhs.get_constant();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is_constant());
	auto const &rhs_const_expr = rhs.get_constant();
	auto const &rhs_value = rhs_const_expr.value;
	bz_assert(lhs_value.kind() == rhs_value.kind());

	bz_assert(ast::remove_const_or_consteval(lhs_const_expr.type).is<ast::ts_base_type>());
	auto const type = ast::remove_const_or_consteval(lhs_const_expr.type).get<ast::ts_base_type>().info->kind;
	switch (lhs_value.kind())
	{
	static_assert(ast::constant_value::variant_count == 21);
	case ast::constant_value::sint:
	{
		auto const lhs_int_val = lhs_value.get_sint();
		auto const rhs_int_val = rhs_value.get_sint();
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
		auto const lhs_int_val = lhs_value.get_uint();
		auto const rhs_int_val = rhs_value.get_uint();
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
		auto const lhs_float_val = lhs_value.get_float32();
		auto const rhs_float_val = rhs_value.get_float32();
		return ast::constant_value(safe_binary_divide(
			original_expr.src_tokens, original_expr.paren_level,
			lhs_float_val, rhs_float_val, context
		));
	}
	case ast::constant_value::float64:
	{
		auto const lhs_float_val = lhs_value.get_float64();
		auto const rhs_float_val = rhs_value.get_float64();
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
	bz_assert(lhs.is_constant());
	auto const &lhs_const_expr = lhs.get_constant();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is_constant());
	auto const &rhs_const_expr = rhs.get_constant();
	auto const &rhs_value = rhs_const_expr.value;
	bz_assert(lhs_value.kind() == rhs_value.kind());

	bz_assert(ast::remove_const_or_consteval(lhs_const_expr.type).is<ast::ts_base_type>());
	auto const type = ast::remove_const_or_consteval(lhs_const_expr.type).get<ast::ts_base_type>().info->kind;
	switch (lhs_value.kind())
	{
	static_assert(ast::constant_value::variant_count == 21);
	case ast::constant_value::sint:
	{
		auto const lhs_int_val = lhs_value.get_sint();
		auto const rhs_int_val = rhs_value.get_sint();
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
		auto const lhs_int_val = lhs_value.get_uint();
		auto const rhs_int_val = rhs_value.get_uint();
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
	bz_assert(lhs.is_constant());
	auto const &lhs_const_expr = lhs.get_constant();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is_constant());
	auto const &rhs_const_expr = rhs.get_constant();
	auto const &rhs_value = rhs_const_expr.value;

	bz_assert(lhs_value.kind() == rhs_value.kind());

	switch (lhs_value.kind())
	{
	static_assert(ast::constant_value::variant_count == 21);
	case ast::constant_value::sint:
	{
		auto const lhs_int_val = lhs_value.get_sint();
		auto const rhs_int_val = rhs_value.get_sint();
		return ast::constant_value(lhs_int_val == rhs_int_val);
	}
	case ast::constant_value::uint:
	{
		auto const lhs_int_val = lhs_value.get_uint();
		auto const rhs_int_val = rhs_value.get_uint();
		return ast::constant_value(lhs_int_val == rhs_int_val);
	}
	case ast::constant_value::float32:
	{
		auto const lhs_float_val = lhs_value.get_float32();
		auto const rhs_float_val = rhs_value.get_float32();
		return ast::constant_value(safe_binary_equals(
			original_expr.src_tokens, original_expr.paren_level,
			lhs_float_val, rhs_float_val, context
		));
	}
	case ast::constant_value::float64:
	{
		auto const lhs_float_val = lhs_value.get_float64();
		auto const rhs_float_val = rhs_value.get_float64();
		return ast::constant_value(safe_binary_equals(
			original_expr.src_tokens, original_expr.paren_level,
			lhs_float_val, rhs_float_val, context
		));
	}
	case ast::constant_value::u8char:
	{
		auto const lhs_char_val = lhs_value.get_u8char();
		auto const rhs_char_val = rhs_value.get_u8char();
		return ast::constant_value(lhs_char_val == rhs_char_val);
	}
	case ast::constant_value::boolean:
	{
		auto const lhs_bool_val = lhs_value.get_boolean();
		auto const rhs_bool_val = rhs_value.get_boolean();
		return ast::constant_value(lhs_bool_val == rhs_bool_val);
	}
	case ast::constant_value::string:
	{
		auto const lhs_str_val = lhs_value.get_string();
		auto const rhs_str_val = rhs_value.get_string();
		return ast::constant_value(lhs_str_val == rhs_str_val);
	}
	case ast::constant_value::null:
	{
		return ast::constant_value(true);
	}
	case ast::constant_value::enum_:
	{
		auto const lhs_enum_value = lhs_value.get_enum().value;
		auto const rhs_enum_value = rhs_value.get_enum().value;
		return ast::constant_value(lhs_enum_value == rhs_enum_value);
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
	bz_assert(lhs.is_constant());
	auto const &lhs_const_expr = lhs.get_constant();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is_constant());
	auto const &rhs_const_expr = rhs.get_constant();
	auto const &rhs_value = rhs_const_expr.value;

	bz_assert(lhs_value.kind() == rhs_value.kind());

	switch (lhs_value.kind())
	{
	static_assert(ast::constant_value::variant_count == 21);
	case ast::constant_value::sint:
	{
		auto const lhs_int_val = lhs_value.get_sint();
		auto const rhs_int_val = rhs_value.get_sint();
		return ast::constant_value(lhs_int_val != rhs_int_val);
	}
	case ast::constant_value::uint:
	{
		auto const lhs_int_val = lhs_value.get_uint();
		auto const rhs_int_val = rhs_value.get_uint();
		return ast::constant_value(lhs_int_val != rhs_int_val);
	}
	case ast::constant_value::float32:
	{
		auto const lhs_float_val = lhs_value.get_float32();
		auto const rhs_float_val = rhs_value.get_float32();
		return ast::constant_value(lhs_float_val != rhs_float_val);
	}
	case ast::constant_value::float64:
	{
		auto const lhs_float_val = lhs_value.get_float64();
		auto const rhs_float_val = rhs_value.get_float64();
		return ast::constant_value(lhs_float_val != rhs_float_val);
	}
	case ast::constant_value::u8char:
	{
		auto const lhs_char_val = lhs_value.get_u8char();
		auto const rhs_char_val = rhs_value.get_u8char();
		return ast::constant_value(lhs_char_val != rhs_char_val);
	}
	case ast::constant_value::boolean:
	{
		auto const lhs_bool_val = lhs_value.get_boolean();
		auto const rhs_bool_val = rhs_value.get_boolean();
		return ast::constant_value(lhs_bool_val != rhs_bool_val);
	}
	case ast::constant_value::string:
	{
		auto const lhs_str_val = lhs_value.get_string();
		auto const rhs_str_val = rhs_value.get_string();
		return ast::constant_value(lhs_str_val != rhs_str_val);
	}
	case ast::constant_value::null:
	{
		return ast::constant_value(false);
	}
	case ast::constant_value::enum_:
	{
		auto const lhs_enum_value = lhs_value.get_enum().value;
		auto const rhs_enum_value = rhs_value.get_enum().value;
		return ast::constant_value(lhs_enum_value != rhs_enum_value);
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
	bz_assert(lhs.is_constant());
	auto const &lhs_const_expr = lhs.get_constant();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is_constant());
	auto const &rhs_const_expr = rhs.get_constant();
	auto const &rhs_value = rhs_const_expr.value;

	bz_assert(lhs_value.kind() == rhs_value.kind());

	switch (lhs_value.kind())
	{
	static_assert(ast::constant_value::variant_count == 21);
	case ast::constant_value::sint:
	{
		auto const lhs_int_val = lhs_value.get_sint();
		auto const rhs_int_val = rhs_value.get_sint();
		return ast::constant_value(lhs_int_val < rhs_int_val);
	}
	case ast::constant_value::uint:
	{
		auto const lhs_int_val = lhs_value.get_uint();
		auto const rhs_int_val = rhs_value.get_uint();
		return ast::constant_value(lhs_int_val < rhs_int_val);
	}
	case ast::constant_value::float32:
	{
		auto const lhs_float_val = lhs_value.get_float32();
		auto const rhs_float_val = rhs_value.get_float32();
		return ast::constant_value(lhs_float_val < rhs_float_val);
	}
	case ast::constant_value::float64:
	{
		auto const lhs_float_val = lhs_value.get_float64();
		auto const rhs_float_val = rhs_value.get_float64();
		return ast::constant_value(lhs_float_val < rhs_float_val);
	}
	case ast::constant_value::u8char:
	{
		auto const lhs_char_val = lhs_value.get_u8char();
		auto const rhs_char_val = rhs_value.get_u8char();
		return ast::constant_value(lhs_char_val < rhs_char_val);
	}
	case ast::constant_value::null:
	{
		return ast::constant_value(false);
	}
	case ast::constant_value::enum_:
	{
		auto const [decl, lhs_enum_value] = lhs_value.get_enum();
		auto const rhs_enum_value = rhs_value.get_enum().value;
		auto const is_signed = ast::is_signed_integer_kind(decl->underlying_type.get<ast::ts_base_type>().info->kind);
		return is_signed
			? ast::constant_value(bit_cast<int64_t>(lhs_enum_value) < bit_cast<int64_t>(rhs_enum_value))
			: ast::constant_value(lhs_enum_value < rhs_enum_value);
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
	bz_assert(lhs.is_constant());
	auto const &lhs_const_expr = lhs.get_constant();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is_constant());
	auto const &rhs_const_expr = rhs.get_constant();
	auto const &rhs_value = rhs_const_expr.value;

	bz_assert(lhs_value.kind() == rhs_value.kind());

	switch (lhs_value.kind())
	{
	static_assert(ast::constant_value::variant_count == 21);
	case ast::constant_value::sint:
	{
		auto const lhs_int_val = lhs_value.get_sint();
		auto const rhs_int_val = rhs_value.get_sint();
		return ast::constant_value(lhs_int_val <= rhs_int_val);
	}
	case ast::constant_value::uint:
	{
		auto const lhs_int_val = lhs_value.get_uint();
		auto const rhs_int_val = rhs_value.get_uint();
		return ast::constant_value(lhs_int_val <= rhs_int_val);
	}
	case ast::constant_value::float32:
	{
		auto const lhs_float_val = lhs_value.get_float32();
		auto const rhs_float_val = rhs_value.get_float32();
		return ast::constant_value(lhs_float_val <= rhs_float_val);
	}
	case ast::constant_value::float64:
	{
		auto const lhs_float_val = lhs_value.get_float64();
		auto const rhs_float_val = rhs_value.get_float64();
		return ast::constant_value(lhs_float_val <= rhs_float_val);
	}
	case ast::constant_value::u8char:
	{
		auto const lhs_char_val = lhs_value.get_u8char();
		auto const rhs_char_val = rhs_value.get_u8char();
		return ast::constant_value(lhs_char_val <= rhs_char_val);
	}
	case ast::constant_value::null:
	{
		return ast::constant_value(true);
	}
	case ast::constant_value::enum_:
	{
		auto const [decl, lhs_enum_value] = lhs_value.get_enum();
		auto const rhs_enum_value = rhs_value.get_enum().value;
		auto const is_signed = ast::is_signed_integer_kind(decl->underlying_type.get<ast::ts_base_type>().info->kind);
		return is_signed
			? ast::constant_value(bit_cast<int64_t>(lhs_enum_value) <= bit_cast<int64_t>(rhs_enum_value))
			: ast::constant_value(lhs_enum_value <= rhs_enum_value);
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
	bz_assert(lhs.is_constant());
	auto const &lhs_const_expr = lhs.get_constant();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is_constant());
	auto const &rhs_const_expr = rhs.get_constant();
	auto const &rhs_value = rhs_const_expr.value;

	bz_assert(lhs_value.kind() == rhs_value.kind());

	switch (lhs_value.kind())
	{
	static_assert(ast::constant_value::variant_count == 21);
	case ast::constant_value::sint:
	{
		auto const lhs_int_val = lhs_value.get_sint();
		auto const rhs_int_val = rhs_value.get_sint();
		return ast::constant_value(lhs_int_val > rhs_int_val);
	}
	case ast::constant_value::uint:
	{
		auto const lhs_int_val = lhs_value.get_uint();
		auto const rhs_int_val = rhs_value.get_uint();
		return ast::constant_value(lhs_int_val > rhs_int_val);
	}
	case ast::constant_value::float32:
	{
		auto const lhs_float_val = lhs_value.get_float32();
		auto const rhs_float_val = rhs_value.get_float32();
		return ast::constant_value(lhs_float_val > rhs_float_val);
	}
	case ast::constant_value::float64:
	{
		auto const lhs_float_val = lhs_value.get_float64();
		auto const rhs_float_val = rhs_value.get_float64();
		return ast::constant_value(lhs_float_val > rhs_float_val);
	}
	case ast::constant_value::u8char:
	{
		auto const lhs_char_val = lhs_value.get_u8char();
		auto const rhs_char_val = rhs_value.get_u8char();
		return ast::constant_value(lhs_char_val > rhs_char_val);
	}
	case ast::constant_value::null:
	{
		return ast::constant_value(false);
	}
	case ast::constant_value::enum_:
	{
		auto const [decl, lhs_enum_value] = lhs_value.get_enum();
		auto const rhs_enum_value = rhs_value.get_enum().value;
		auto const is_signed = ast::is_signed_integer_kind(decl->underlying_type.get<ast::ts_base_type>().info->kind);
		return is_signed
			? ast::constant_value(bit_cast<int64_t>(lhs_enum_value) > bit_cast<int64_t>(rhs_enum_value))
			: ast::constant_value(lhs_enum_value > rhs_enum_value);
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
	bz_assert(lhs.is_constant());
	auto const &lhs_const_expr = lhs.get_constant();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is_constant());
	auto const &rhs_const_expr = rhs.get_constant();
	auto const &rhs_value = rhs_const_expr.value;

	bz_assert(lhs_value.kind() == rhs_value.kind());

	switch (lhs_value.kind())
	{
	static_assert(ast::constant_value::variant_count == 21);
	case ast::constant_value::sint:
	{
		auto const lhs_int_val = lhs_value.get_sint();
		auto const rhs_int_val = rhs_value.get_sint();
		return ast::constant_value(lhs_int_val >= rhs_int_val);
	}
	case ast::constant_value::uint:
	{
		auto const lhs_int_val = lhs_value.get_uint();
		auto const rhs_int_val = rhs_value.get_uint();
		return ast::constant_value(lhs_int_val >= rhs_int_val);
	}
	case ast::constant_value::float32:
	{
		auto const lhs_float_val = lhs_value.get_float32();
		auto const rhs_float_val = rhs_value.get_float32();
		return ast::constant_value(lhs_float_val >= rhs_float_val);
	}
	case ast::constant_value::float64:
	{
		auto const lhs_float_val = lhs_value.get_float64();
		auto const rhs_float_val = rhs_value.get_float64();
		return ast::constant_value(lhs_float_val >= rhs_float_val);
	}
	case ast::constant_value::u8char:
	{
		auto const lhs_char_val = lhs_value.get_u8char();
		auto const rhs_char_val = rhs_value.get_u8char();
		return ast::constant_value(lhs_char_val >= rhs_char_val);
	}
	case ast::constant_value::null:
	{
		return ast::constant_value(true);
	}
	case ast::constant_value::enum_:
	{
		auto const [decl, lhs_enum_value] = lhs_value.get_enum();
		auto const rhs_enum_value = rhs_value.get_enum().value;
		auto const is_signed = ast::is_signed_integer_kind(decl->underlying_type.get<ast::ts_base_type>().info->kind);
		return is_signed
			? ast::constant_value(bit_cast<int64_t>(lhs_enum_value) >= bit_cast<int64_t>(rhs_enum_value))
			: ast::constant_value(lhs_enum_value >= rhs_enum_value);
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
	bz_assert(lhs.is_constant());
	auto const &lhs_const_expr = lhs.get_constant();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is_constant());
	auto const &rhs_const_expr = rhs.get_constant();
	auto const &rhs_value = rhs_const_expr.value;

	bz_assert(lhs_value.kind() == rhs_value.kind());

	switch (lhs_value.kind())
	{
	static_assert(ast::constant_value::variant_count == 21);
	case ast::constant_value::uint:
	{
		auto const lhs_int_val = lhs_value.get_uint();
		auto const rhs_int_val = rhs_value.get_uint();
		return ast::constant_value(lhs_int_val & rhs_int_val);
	}
	case ast::constant_value::boolean:
	{
		auto const lhs_bool_val = lhs_value.get_boolean();
		auto const rhs_bool_val = rhs_value.get_boolean();
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
	bz_assert(lhs.is_constant());
	auto const &lhs_const_expr = lhs.get_constant();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is_constant());
	auto const &rhs_const_expr = rhs.get_constant();
	auto const &rhs_value = rhs_const_expr.value;

	bz_assert(lhs_value.kind() == rhs_value.kind());

	switch (lhs_value.kind())
	{
	static_assert(ast::constant_value::variant_count == 21);
	case ast::constant_value::uint:
	{
		auto const lhs_int_val = lhs_value.get_uint();
		auto const rhs_int_val = rhs_value.get_uint();
		return ast::constant_value(lhs_int_val ^ rhs_int_val);
	}
	case ast::constant_value::boolean:
	{
		auto const lhs_bool_val = lhs_value.get_boolean();
		auto const rhs_bool_val = rhs_value.get_boolean();
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
	bz_assert(lhs.is_constant());
	auto const &lhs_const_expr = lhs.get_constant();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is_constant());
	auto const &rhs_const_expr = rhs.get_constant();
	auto const &rhs_value = rhs_const_expr.value;

	bz_assert(lhs_value.kind() == rhs_value.kind());

	switch (lhs_value.kind())
	{
	static_assert(ast::constant_value::variant_count == 21);
	case ast::constant_value::uint:
	{
		auto const lhs_int_val = lhs_value.get_uint();
		auto const rhs_int_val = rhs_value.get_uint();
		return ast::constant_value(lhs_int_val | rhs_int_val);
	}
	case ast::constant_value::boolean:
	{
		auto const lhs_bool_val = lhs_value.get_boolean();
		auto const rhs_bool_val = rhs_value.get_boolean();
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
	bz_assert(lhs.is_constant());
	auto const &lhs_const_expr = lhs.get_constant();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is_constant());
	auto const &rhs_const_expr = rhs.get_constant();
	auto const &rhs_value = rhs_const_expr.value;

	bz_assert(lhs_value.is_uint());
	auto const lhs_int_val = lhs_value.get_uint();

	bz_assert(lhs_const_expr.type.is<ast::ts_base_type>());
	auto const lhs_type_kind = lhs_const_expr.type.get<ast::ts_base_type>().info->kind;

	bz_assert(rhs_value.is_uint() || rhs_value.is_sint());
	if (rhs_value.is_uint())
	{
		auto const rhs_int_val = rhs_value.get_uint();

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
		auto const rhs_int_val = rhs_value.get_sint();

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
	bz_assert(lhs.is_constant());
	auto const &lhs_const_expr = lhs.get_constant();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is_constant());
	auto const &rhs_const_expr = rhs.get_constant();
	auto const &rhs_value = rhs_const_expr.value;

	bz_assert(lhs_value.is_uint());
	auto const lhs_int_val = lhs_value.get_uint();

	bz_assert(ast::remove_const_or_consteval(lhs_const_expr.type).is<ast::ts_base_type>());
	auto const lhs_type_kind = ast::remove_const_or_consteval(lhs_const_expr.type).get<ast::ts_base_type>().info->kind;

	bz_assert(rhs_value.is_uint() || rhs_value.is_sint());
	if (rhs_value.is_uint())
	{
		auto const rhs_int_val = rhs_value.get_uint();

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
		auto const rhs_int_val = rhs_value.get_sint();

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
	bz_assert(lhs.is_constant());
	auto const &lhs_const_expr = lhs.get_constant();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is_constant());
	auto const &rhs_const_expr = rhs.get_constant();
	auto const &rhs_value = rhs_const_expr.value;

	bz_assert(lhs_value.is_boolean());
	auto const lhs_bool_val = lhs_value.get_boolean();
	bz_assert(rhs_value.is_boolean());
	auto const rhs_bool_val = rhs_value.get_boolean();

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
	bz_assert(lhs.is_constant());
	auto const &lhs_const_expr = lhs.get_constant();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is_constant());
	auto const &rhs_const_expr = rhs.get_constant();
	auto const &rhs_value = rhs_const_expr.value;

	bz_assert(lhs_value.is_boolean());
	auto const lhs_bool_val = lhs_value.get_boolean();
	bz_assert(rhs_value.is_boolean());
	auto const rhs_bool_val = rhs_value.get_boolean();

	return ast::constant_value(lhs_bool_val != rhs_bool_val);
}

static ast::constant_value evaluate_binary_bool_or(
	[[maybe_unused]] ast::expression const &original_expr,
	ast::expression const &lhs, ast::expression const &rhs,
	[[maybe_unused]] ctx::parse_context &context
)
{
	bz_assert(lhs.is_constant());
	auto const &lhs_const_expr = lhs.get_constant();
	auto const &lhs_value = lhs_const_expr.value;
	bz_assert(rhs.is_constant());
	auto const &rhs_const_expr = rhs.get_constant();
	auto const &rhs_value = rhs_const_expr.value;

	bz_assert(lhs_value.is_boolean());
	auto const lhs_bool_val = lhs_value.get_boolean();
	bz_assert(rhs_value.is_boolean());
	auto const rhs_bool_val = rhs_value.get_boolean();

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
	bz_assert(rhs.is_constant());
	return rhs.get_constant_value();
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

static ast::constant_value evaluate_tuple_subscript(ast::expr_tuple_subscript const &tuple_subscript_expr)
{
	bz_assert(tuple_subscript_expr.index.is<ast::constant_expression>());
	auto const is_consteval = tuple_subscript_expr.base.elems
		.is_all([](auto const &elem) {
			return elem.template is<ast::constant_expression>();
		});
	if (!is_consteval)
	{
		return {};
	}

	auto const &index_value = tuple_subscript_expr.index.get_constant_value();
	bz_assert(
		index_value.is_uint()
		|| (index_value.is_sint() && index_value.get_sint() >= 0)
	);
	auto const index_int_value = index_value.is_uint()
		? index_value.get_uint()
		: static_cast<uint64_t>(index_value.get_sint());
	return tuple_subscript_expr.base.elems[index_int_value].get_constant_value();
}

static ast::constant_value evaluate_subscript(
	ast::expression const &base,
	ast::expression const &index,
	ctx::parse_context &context
)
{
	bool is_consteval = true;
	auto const &base_type = ast::remove_const_or_consteval(base.get_expr_type());

	uint64_t index_value = 0;

	if (index.is_constant())
	{
		bz_assert(index.is_constant());
		auto const &index_const_value = index.get_constant_value();
		if (index_const_value.is_uint())
		{
			index_value = index_const_value.get_uint();
		}
		else
		{
			bz_assert(index_const_value.is_sint());
			auto const signed_index_value = index_const_value.get_sint();
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

	if (!is_consteval || !base.has_consteval_succeeded())
	{
		return {};
	}

	bz_assert(base.is_constant());
	auto const &value = base.get_constant_value();
	if (base_type.is<ast::ts_array>())
	{
		auto const elem_type = base_type.get<ast::ts_array>().elem_type.as_typespec_view();

		if (elem_type.is<ast::ts_array>())
		{
			auto const [inner_elem_type, inner_size, is_multi_dimensional] = get_flattened_array_type_and_size(elem_type);
			auto const begin_index = index_value * inner_size;
			auto const end_index = begin_index + inner_size;
			switch (value.index())
			{
			static_assert(ast::constant_value::variant_count == 21);
			case ast::constant_value::array:
			{
				auto const &array_value = value.get_array();
				bz_assert(end_index <= array_value.size());
				auto result = ast::constant_value();
				result.emplace<ast::constant_value::array>(array_value.slice(begin_index, end_index));
				return result;
			}
			case ast::constant_value::sint_array:
			{
				auto const &array_value = value.get_sint_array();
				bz_assert(end_index <= array_value.size());
				auto result = ast::constant_value();
				result.emplace<ast::constant_value::sint_array>(array_value.slice(begin_index, end_index));
				return result;
			}
			case ast::constant_value::uint_array:
			{
				auto const &array_value = value.get_uint_array();
				bz_assert(end_index <= array_value.size());
				auto result = ast::constant_value();
				result.emplace<ast::constant_value::uint_array>(array_value.slice(begin_index, end_index));
				return result;
			}
			case ast::constant_value::float32_array:
			{
				auto const &array_value = value.get_float32_array();
				bz_assert(end_index <= array_value.size());
				auto result = ast::constant_value();
				result.emplace<ast::constant_value::float32_array>(array_value.slice(begin_index, end_index));
				return result;
			}
			case ast::constant_value::float64_array:
			{
				auto const &array_value = value.get_float64_array();
				bz_assert(end_index <= array_value.size());
				auto result = ast::constant_value();
				result.emplace<ast::constant_value::float64_array>(array_value.slice(begin_index, end_index));
				return result;
			}
			default:
				bz_unreachable;
			}
		}
		else
		{
			switch (value.index())
			{
			static_assert(ast::constant_value::variant_count == 21);
			case ast::constant_value::array:
			{
				auto const &array_value = value.get_array();
				bz_assert(index_value < array_value.size());
				return array_value[index_value];
			}
			case ast::constant_value::sint_array:
			{
				auto const &array_value = value.get_sint_array();
				bz_assert(index_value < array_value.size());
				return ast::constant_value(array_value[index_value]);
			}
			case ast::constant_value::uint_array:
			{
				auto const &array_value = value.get_uint_array();
				bz_assert(index_value < array_value.size());
				return ast::constant_value(array_value[index_value]);
			}
			case ast::constant_value::float32_array:
			{
				auto const &array_value = value.get_float32_array();
				bz_assert(index_value < array_value.size());
				return ast::constant_value(array_value[index_value]);
			}
			case ast::constant_value::float64_array:
			{
				auto const &array_value = value.get_float64_array();
				bz_assert(index_value < array_value.size());
				return ast::constant_value(array_value[index_value]);
			}
			default:
				bz_unreachable;
			}
		}
	}
	else
	{
		bz_assert(base_type.is<ast::ts_tuple>());
		bz_assert(value.is_tuple());
		auto const &tuple_value = value.get_tuple();
		bz_assert(index_value < tuple_value.size());
		return tuple_value[index_value];
	}
}

enum class function_execution_kind
{
	guaranteed_evaluate,
	force_evaluate,
	force_evaluate_without_error,
};

template<typename Kind>
static ast::constant_value is_typespec_kind_helper(ast::expr_function_call &func_call)
{
	bz_assert(func_call.params.size() == 1);
	bz_assert(func_call.params[0].is_constant());
	bz_assert(func_call.params[0].get_constant_value().is_type());
	auto const type = func_call.params[0]
		.get_constant_value()
		.get_type();
	return ast::constant_value(type.is<Kind>());
}

template<typename Kind>
static ast::constant_value remove_typespec_kind_helper(ast::expr_function_call &func_call)
{
	bz_assert(func_call.params.size() == 1);
	bz_assert(func_call.params[0].is_constant());
	bz_assert(func_call.params[0].get_constant_value().is_type());
	auto const type = func_call.params[0]
		.get_constant_value()
		.get_type();
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
	static_assert(ast::function_body::_builtin_last - ast::function_body::_builtin_first == 192);
	static_assert(ast::function_body::_builtin_default_constructor_last - ast::function_body::_builtin_default_constructor_first == 14);
	static_assert(ast::function_body::_builtin_unary_operator_last - ast::function_body::_builtin_unary_operator_first == 7);
	static_assert(ast::function_body::_builtin_binary_operator_last - ast::function_body::_builtin_binary_operator_first == 27);

	case ast::function_body::builtin_array_size:
	{
		bz_assert(func_call.params.size() == 1);
		auto const type = ast::remove_const_or_consteval(func_call.params[0].get_expr_type());
		bz_assert(type.is<ast::ts_array>());
		bz_assert(type.get<ast::ts_array>().size != 0);
		return ast::constant_value(type.get<ast::ts_array>().size);
	}
	case ast::function_body::builtin_enum_value:
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
		bz_assert(func_call.params[0].is_constant());
		auto const &value = func_call.params[0].get_constant_value();
		bz_assert(value.is_enum());
		auto const [decl, enum_value] = value.get_enum();
		bz_assert(decl->underlying_type.is<ast::ts_base_type>());
		auto const is_signed = ast::is_signed_integer_kind(decl->underlying_type.get<ast::ts_base_type>().info->kind);
		if (is_signed)
		{
			return ast::constant_value(bit_cast<int64_t>(enum_value));
		}
		else
		{
			return ast::constant_value(enum_value);
		}
	}
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
	case ast::function_body::comptime_concatenate_strs:
	{
		bz_assert(func_call.params.is_all([](auto const &param) {
			return param.is_constant();
		}));
		bz_assert(func_call.params.is_all([](auto const &param) {
			return param
				.get_constant_value()
				.is_string();
		}));

		auto result = func_call.params
			.transform([](auto const &param) {
				return param
					.get_constant_value()
					.get_string();
			})
			.reduce(bz::u8string(), [](auto lhs, auto const &rhs) {
				lhs += rhs;
				return lhs;
			});
		return ast::constant_value(std::move(result));
	}

	case ast::function_body::typename_as_str:
	{
		bz_assert(func_call.params.size() == 1);
		bz_assert(func_call.params[0].is_constant());
		bz_assert(func_call.params[0].get_constant_value().is_type());
		auto const type = func_call.params[0]
			.get_constant_value()
			.get_type();
		return ast::constant_value(bz::format("{}", type));
	}

	case ast::function_body::is_const:
		return is_typespec_kind_helper<ast::ts_const>(func_call);
	case ast::function_body::is_consteval:
		return is_typespec_kind_helper<ast::ts_consteval>(func_call);
	case ast::function_body::is_pointer:
		return is_typespec_kind_helper<ast::ts_pointer>(func_call);
	case ast::function_body::is_optional:
		return is_typespec_kind_helper<ast::ts_optional>(func_call);
	case ast::function_body::is_reference:
		return is_typespec_kind_helper<ast::ts_lvalue_reference>(func_call);
	case ast::function_body::is_move_reference:
		return is_typespec_kind_helper<ast::ts_move_reference>(func_call);
	case ast::function_body::is_slice:
		return is_typespec_kind_helper<ast::ts_array_slice>(func_call);
	case ast::function_body::is_array:
		return is_typespec_kind_helper<ast::ts_array>(func_call);
	case ast::function_body::is_tuple:
		return is_typespec_kind_helper<ast::ts_tuple>(func_call);
	case ast::function_body::is_enum:
		return is_typespec_kind_helper<ast::ts_enum>(func_call);

	case ast::function_body::remove_const:
		return remove_typespec_kind_helper<ast::ts_const>(func_call);
	case ast::function_body::remove_consteval:
		return remove_typespec_kind_helper<ast::ts_consteval>(func_call);
	case ast::function_body::remove_pointer:
		return remove_typespec_kind_helper<ast::ts_pointer>(func_call);
	case ast::function_body::remove_optional:
		return remove_typespec_kind_helper<ast::ts_optional>(func_call);
	case ast::function_body::remove_reference:
		return remove_typespec_kind_helper<ast::ts_lvalue_reference>(func_call);
	case ast::function_body::remove_move_reference:
		return remove_typespec_kind_helper<ast::ts_move_reference>(func_call);
	case ast::function_body::slice_value_type:
	{
		bz_assert(func_call.params.size() == 1);
		bz_assert(func_call.params[0].is_constant());
		bz_assert(func_call.params[0].get_constant_value().is_type());
		auto const type = func_call.params[0]
			.get_constant_value()
			.get_type();
		if (!type.is<ast::ts_array_slice>())
		{
			context.report_error(
				original_expr.src_tokens,
				bz::format("'__builtin_slice_value_type' called on non-slice type '{}'", type)
			);
			return ast::constant_value(type);
		}
		else
		{
			return ast::constant_value(type.get<ast::ts_array_slice>().elem_type);
		}
	}
	case ast::function_body::array_value_type:
	{
		bz_assert(func_call.params.size() == 1);
		bz_assert(func_call.params[0].is_constant());
		bz_assert(func_call.params[0].get_constant_value().is_type());
		auto const type = func_call.params[0]
			.get_constant_value()
			.get_type();
		if (!type.is<ast::ts_array>())
		{
			context.report_error(
				original_expr.src_tokens,
				bz::format("'__builtin_array_value_type' called on non-array type '{}'", type)
			);
			return ast::constant_value(type);
		}
		else
		{
			return ast::constant_value(type.get<ast::ts_array>().elem_type);
		}
	}
	case ast::function_body::tuple_value_type:
	{
		bz_assert(func_call.params.size() == 2);
		bz_assert(func_call.params[0].is_constant());
		bz_assert(func_call.params[0].get_constant_value().is_type());
		bz_assert(func_call.params[1].is_constant());
		bz_assert(func_call.params[1].get_constant_value().is_uint());
		auto const type = func_call.params[0]
			.get_constant_value()
			.get_type();
		auto const index = func_call.params[1]
			.get_constant_value()
			.get_uint();
		if (!type.is<ast::ts_tuple>())
		{
			context.report_error(
				original_expr.src_tokens,
				bz::format("'__builtin_tuple_value_type' called on non-tuple type '{}'", type)
			);
			return ast::constant_value(type);
		}
		else if (index >= type.get<ast::ts_tuple>().types.size())
		{
			context.report_error(
				original_expr.src_tokens,
				bz::format("index {} is out of range in '__builtin_tuple_value_type' with tuple type '{}'", index, type)
			);
			return ast::constant_value(type.get<ast::ts_tuple>().types.back());
		}
		else
		{
			return ast::constant_value(type.get<ast::ts_tuple>().types[index]);
		}
	}
	case ast::function_body::concat_tuple_types:
	{
		bz_assert(func_call.params.size() == 2);
		bz_assert(func_call.params[0].is_constant());
		bz_assert(func_call.params[0].get_constant_value().is_type());
		bz_assert(func_call.params[1].is_constant());
		bz_assert(func_call.params[1].get_constant_value().is_type());
		auto const lhs_type = func_call.params[0]
			.get_constant_value()
			.get_type();
		auto const rhs_type = func_call.params[1]
			.get_constant_value()
			.get_type();
		if (!lhs_type.is<ast::ts_tuple>() || !rhs_type.is<ast::ts_tuple>())
		{
			if (!lhs_type.is<ast::ts_tuple>())
			{
				context.report_error(
					original_expr.src_tokens,
					bz::format("'__builtin_concat_tuple_types' called with non-tuple type '{}' as lhs", lhs_type)
				);
			}
			if (!rhs_type.is<ast::ts_tuple>())
			{
				context.report_error(
					original_expr.src_tokens,
					bz::format("'__builtin_concat_tuple_types' called with non-tuple type '{}' as rhs", rhs_type)
				);
			}
			return ast::constant_value(lhs_type);
		}
		else
		{
			auto result = ast::constant_value();
			auto &result_type = result.emplace<ast::constant_value::type>();
			result_type = ast::make_tuple_typespec(original_expr.src_tokens, {});
			auto const &lhs_tuple_types = lhs_type.get<ast::ts_tuple>().types;
			auto const &rhs_tuple_types = rhs_type.get<ast::ts_tuple>().types;

			auto &result_tuple_types = result_type.terminator->get<ast::ts_tuple>().types;
			result_tuple_types.reserve(lhs_tuple_types.size() + rhs_tuple_types.size());
			result_tuple_types.append(lhs_tuple_types);
			result_tuple_types.append(rhs_tuple_types);

			return result;
		}
	}
	case ast::function_body::enum_underlying_type:
	{
		bz_assert(func_call.params.size() == 1);
		bz_assert(func_call.params[0].is_constant());
		bz_assert(func_call.params[0].get_constant_value().is_type());
		auto const type = func_call.params[0]
			.get_constant_value()
			.get_type();
		if (!type.is<ast::ts_enum>())
		{
			context.report_error(
				original_expr.src_tokens,
				bz::format("'__builtin_enum_underlying_type' called on non-enum type '{}'", type)
			);
			return ast::constant_value(type);
		}
		else
		{
			context.resolve_type(original_expr.src_tokens, type.get<ast::ts_enum>().decl);
			return ast::constant_value(type.get<ast::ts_enum>().decl->underlying_type);
		}
	}

	case ast::function_body::is_default_constructible:
	{
		bz_assert(func_call.params.size() == 1);
		bz_assert(func_call.params[0].is_constant());
		bz_assert(func_call.params[0].get_constant_value().is_type());
		auto const type = func_call.params[0]
			.get_constant_value()
			.get_type();
		return ast::constant_value(context.is_default_constructible(original_expr.src_tokens, type));
	}
	case ast::function_body::is_copy_constructible:
	{
		bz_assert(func_call.params.size() == 1);
		bz_assert(func_call.params[0].is_constant());
		bz_assert(func_call.params[0].get_constant_value().is_type());
		auto const type = func_call.params[0]
			.get_constant_value()
			.get_type();
		return ast::constant_value(context.is_copy_constructible(original_expr.src_tokens, type));
	}
	case ast::function_body::is_trivially_copy_constructible:
	{
		bz_assert(func_call.params.size() == 1);
		bz_assert(func_call.params[0].is_constant());
		bz_assert(func_call.params[0].get_constant_value().is_type());
		auto const type = func_call.params[0]
			.get_constant_value()
			.get_type();
		return ast::constant_value(context.is_trivially_copy_constructible(original_expr.src_tokens, type));
	}
	case ast::function_body::is_move_constructible:
	{
		bz_assert(func_call.params.size() == 1);
		bz_assert(func_call.params[0].is_constant());
		bz_assert(func_call.params[0].get_constant_value().is_type());
		auto const type = func_call.params[0]
			.get_constant_value()
			.get_type();
		return ast::constant_value(context.is_move_constructible(original_expr.src_tokens, type));
	}
	case ast::function_body::is_trivially_move_constructible:
	{
		bz_assert(func_call.params.size() == 1);
		bz_assert(func_call.params[0].is_constant());
		bz_assert(func_call.params[0].get_constant_value().is_type());
		auto const type = func_call.params[0]
			.get_constant_value()
			.get_type();
		return ast::constant_value(context.is_trivially_move_constructible(original_expr.src_tokens, type));
	}
	case ast::function_body::is_trivially_destructible:
	{
		bz_assert(func_call.params.size() == 1);
		bz_assert(func_call.params[0].is_constant());
		bz_assert(func_call.params[0].get_constant_value().is_type());
		auto const type = func_call.params[0]
			.get_constant_value()
			.get_type();
		return ast::constant_value(context.is_trivially_destructible(original_expr.src_tokens, type));
	}
	case ast::function_body::is_trivially_move_destructible:
	{
		bz_assert(func_call.params.size() == 1);
		bz_assert(func_call.params[0].is_constant());
		bz_assert(func_call.params[0].get_constant_value().is_type());
		auto const type = func_call.params[0]
			.get_constant_value()
			.get_type();
		return ast::constant_value(context.is_trivially_move_destructible(original_expr.src_tokens, type));
	}
	case ast::function_body::is_trivially_relocatable:
	{
		bz_assert(func_call.params.size() == 1);
		bz_assert(func_call.params[0].is_constant());
		bz_assert(func_call.params[0].get_constant_value().is_type());
		auto const type = func_call.params[0]
			.get_constant_value()
			.get_type();
		return ast::constant_value(context.is_trivially_relocatable(original_expr.src_tokens, type));
	}
	case ast::function_body::is_trivial:
	{
		bz_assert(func_call.params.size() == 1);
		bz_assert(func_call.params[0].is_constant());
		bz_assert(func_call.params[0].get_constant_value().is_type());
		auto const type = func_call.params[0]
			.get_constant_value()
			.get_type();
		return ast::constant_value(context.is_trivial(original_expr.src_tokens, type));
	}

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
		return ast::constant_value::get_null();

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
		bz_assert(func_call.params[0].is_constant());
		auto const &const_expr = func_call.params[0].get_constant();
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
		bz_assert(func_call.params[0].is_constant());
		auto const &const_expr = func_call.params[0].get_constant();
		auto const &value = const_expr.value;
		if (value.is_sint())
		{
			bz_assert(ast::remove_const_or_consteval(const_expr.type).is<ast::ts_base_type>());
			auto const type = ast::remove_const_or_consteval(const_expr.type).get<ast::ts_base_type>().info->kind;
			auto const int_val = value.get_sint();
			return ast::constant_value(safe_unary_minus(
				original_expr.src_tokens, original_expr.paren_level,
				int_val, type, context
			));
		}
		else if (value.is_float32())
		{
			auto const float_val = value.get_float32();
			return ast::constant_value(-float_val);
		}
		else
		{
			bz_assert(value.is_float64());
			auto const float_val = value.get_float64();
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
		bz_assert(func_call.params[0].is_constant());
		auto const &value = func_call.params[0].get_constant_value();
		if (value.is_boolean())
		{
			auto const bool_val = value.get_boolean();
			return ast::constant_value(!bool_val);
		}
		else
		{
			auto const param_type = ast::remove_const_or_consteval(func_call.params[0].get_constant().type);
			bz_assert(param_type.is<ast::ts_base_type>());
			auto const param_kind = param_type.get<ast::ts_base_type>().info->kind;
			bz_assert(value.is_uint());
			auto const uint_val = value.get_uint();
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
		bz_assert(func_call.params[0].is_constant());
		bz_assert(func_call.params[0].get_constant_value().is_boolean());
		auto const bool_val = func_call.params[0].get_constant_value().get_boolean();
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
		return {};
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
	if (type.modifiers.not_empty())
	{
		return type.modifiers[0].visit(bz::overload{
			[](ast::ts_optional const &) -> ast::constant_value {
				return ast::constant_value::get_null();
			},
			[](auto const &) -> ast::constant_value {
				bz_unreachable;
			}
		});
	}
	else if (type.is<ast::ts_array>())
	{
		auto const [elem_type, size, is_multi_dimensional] = get_flattened_array_type_and_size(type);
		ast::constant_value result;
		auto const elem_builtin_kind = elem_type.is<ast::ts_base_type>()
			? elem_type.get<ast::ts_base_type>().info->kind
			: ast::type_info::aggregate;
		switch (elem_builtin_kind)
		{
		case ast::type_info::int8_:
		case ast::type_info::int16_:
		case ast::type_info::int32_:
		case ast::type_info::int64_:
			result.emplace<ast::constant_value::sint_array>(size, 0);
			break;
		case ast::type_info::uint8_:
		case ast::type_info::uint16_:
		case ast::type_info::uint32_:
		case ast::type_info::uint64_:
			result.emplace<ast::constant_value::uint_array>(size, 0);
			break;
		case ast::type_info::float32_:
			result.emplace<ast::constant_value::float32_array>(size, 0.0f);
			break;
		case ast::type_info::float64_:
			result.emplace<ast::constant_value::float64_array>(size, 0.0);
			break;
		default:
		{
			auto const elem_value = get_default_constructed_value(src_tokens, elem_type, exec_kind, context);
			if (elem_value.not_null())
			{
				result.emplace<ast::constant_value::array>(size, elem_value);
			}
			break;
		}
		}
		return result;
	}
	else
	{
		return type.terminator->visit(bz::overload{
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
						return ast::constant_value::get_null();

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
						bz_unreachable;
					}
					else if (exec_kind == function_execution_kind::force_evaluate_without_error)
					{
						bz_unreachable;
					}
					else
					{
						return {};
					}
				}
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
			[](auto const &) -> ast::constant_value {
				bz_unreachable;
			}
		});
	}
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
		auto maybe_result = evaluate_intrinsic_function_call(original_expr, func_call, exec_kind, context);
		if (maybe_result.not_null())
		{
			return maybe_result;
		}
	}

	if (func_call.func_body->is_default_default_constructor())
	{
		return get_default_constructed_value(original_expr.src_tokens, func_call.func_body->return_type, exec_kind, context);
	}
	else if (exec_kind == function_execution_kind::force_evaluate)
	{
		return context.execute_function(original_expr.src_tokens, func_call);
	}
	else if (exec_kind == function_execution_kind::force_evaluate_without_error)
	{
		return context.execute_function_without_error(original_expr.src_tokens, func_call);
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
	bz_assert(subscript_expr.expr.is_constant());
	auto const dest_type = ast::remove_const_or_consteval(subscript_expr.type);
	if (!dest_type.is<ast::ts_base_type>())
	{
		return {};
	}

	auto const dest_kind = dest_type.get<ast::ts_base_type>().info->kind;
	auto const paren_level = original_expr.paren_level;
	auto const &value = subscript_expr.expr.get_constant_value();
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
		static_assert(ast::constant_value::variant_count == 21);
		case ast::constant_value::sint:
		{
			using T = std::tuple<bz::u8string_view, int64_t, int64_t, int64_t>;
			auto const int_val = value.get_sint();
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
			auto const int_val = value.get_uint();
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
			auto const float_val = value.get_float32();
			auto const result =
				dest_kind == ast::type_info::int8_  ? static_cast<int8_t> (float_val) :
				dest_kind == ast::type_info::int16_ ? static_cast<int16_t>(float_val) :
				dest_kind == ast::type_info::int32_ ? static_cast<int32_t>(float_val) :
				static_cast<int64_t>(float_val);
			return ast::constant_value(result);
		}
		case ast::constant_value::float64:
		{
			auto const float_val = value.get_float64();
			auto const result =
				dest_kind == ast::type_info::int8_  ? static_cast<int8_t> (float_val) :
				dest_kind == ast::type_info::int16_ ? static_cast<int16_t>(float_val) :
				dest_kind == ast::type_info::int32_ ? static_cast<int32_t>(float_val) :
				static_cast<int64_t>(float_val);
			return ast::constant_value(result);
		}
		case ast::constant_value::u8char:
			// no overflow possible in constant expressions
			return ast::constant_value(static_cast<int64_t>(value.get_u8char()));
		case ast::constant_value::boolean:
			return ast::constant_value(static_cast<int64_t>(value.get_boolean()));
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
		static_assert(ast::constant_value::variant_count == 21);
		case ast::constant_value::sint:
		{
			using T = std::tuple<bz::u8string_view, uint64_t, uint64_t>;
			auto const int_val = value.get_sint();
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
			auto const int_val = value.get_uint();
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
			auto const float_val = value.get_float32();
			auto const result =
				dest_kind == ast::type_info::uint8_  ? static_cast<uint8_t> (float_val) :
				dest_kind == ast::type_info::uint16_ ? static_cast<uint16_t>(float_val) :
				dest_kind == ast::type_info::uint32_ ? static_cast<uint32_t>(float_val) :
				static_cast<uint64_t>(float_val);
			return ast::constant_value(result);
		}
		case ast::constant_value::float64:
		{
			auto const float_val = value.get_float64();
			auto const result =
				dest_kind == ast::type_info::uint8_  ? static_cast<uint8_t> (float_val) :
				dest_kind == ast::type_info::uint16_ ? static_cast<uint16_t>(float_val) :
				dest_kind == ast::type_info::uint32_ ? static_cast<uint32_t>(float_val) :
				static_cast<uint64_t>(float_val);
			return ast::constant_value(result);
		}
		case ast::constant_value::u8char:
			// no overflow possible in constant expressions
			return ast::constant_value(static_cast<uint64_t>(value.get_u8char()));
		case ast::constant_value::boolean:
			return ast::constant_value(static_cast<uint64_t>(value.get_boolean()));
		default:
			bz_unreachable;
		}
	}

	case ast::type_info::float32_:
		switch (value.kind())
		{
		static_assert(ast::constant_value::variant_count == 21);
		case ast::constant_value::sint:
			return ast::constant_value(static_cast<float32_t>(value.get_sint()));
		case ast::constant_value::uint:
			return ast::constant_value(static_cast<float32_t>(value.get_uint()));
		case ast::constant_value::float32:
			return ast::constant_value(static_cast<float32_t>(value.get_float32()));
		case ast::constant_value::float64:
			return ast::constant_value(static_cast<float32_t>(value.get_float64()));
		default:
			bz_unreachable;
		}
	case ast::type_info::float64_:
		switch (value.kind())
		{
		static_assert(ast::constant_value::variant_count == 21);
		case ast::constant_value::sint:
			return ast::constant_value(static_cast<float64_t>(value.get_sint()));
		case ast::constant_value::uint:
			return ast::constant_value(static_cast<float64_t>(value.get_uint()));
		case ast::constant_value::float32:
			return ast::constant_value(static_cast<float64_t>(value.get_float32()));
		case ast::constant_value::float64:
			return ast::constant_value(static_cast<float64_t>(value.get_float64()));
		default:
			bz_unreachable;
		}
	case ast::type_info::char_:
		switch (value.kind())
		{
		static_assert(ast::constant_value::variant_count == 21);
		case ast::constant_value::sint:
		{
			auto const result = static_cast<bz::u8char>(value.get_sint());
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
			auto const result = static_cast<bz::u8char>(value.get_uint());
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

static bool is_special_array_type(ast::typespec_view type)
{
	if (!type.is<ast::ts_array>())
	{
		return false;
	}

	auto elem_type = type.get<ast::ts_array>().elem_type.as_typespec_view();
	while (elem_type.is<ast::ts_array>())
	{
		elem_type = elem_type.get<ast::ts_array>().elem_type;
	}

	if (!elem_type.is<ast::ts_base_type>())
	{
		return false;
	}

	auto const type_kind = elem_type.get<ast::ts_base_type>().info->kind;

	switch (type_kind)
	{
	case ast::type_info::int8_:
	case ast::type_info::int16_:
	case ast::type_info::int32_:
	case ast::type_info::int64_:
	case ast::type_info::uint8_:
	case ast::type_info::uint16_:
	case ast::type_info::uint32_:
	case ast::type_info::uint64_:
	case ast::type_info::float32_:
	case ast::type_info::float64_:
		return true;
	default:
		return false;
	}
}

static ast::constant_value get_special_array_value(ast::typespec_view array_type, bz::array_view<ast::expression const> exprs)
{
	auto const [elem_type, size, is_multi_dimensional] = get_flattened_array_type_and_size(array_type);
	bz_assert(elem_type.is<ast::ts_base_type>());
	auto const type_kind = elem_type.get<ast::ts_base_type>().info->kind;

	switch (type_kind)
	{
	case ast::type_info::int8_:
	case ast::type_info::int16_:
	case ast::type_info::int32_:
	case ast::type_info::int64_:
	{
		auto result = ast::constant_value();
		auto &sint_array = result.emplace<ast::constant_value::sint_array>();
		sint_array.reserve(size);
		if (is_multi_dimensional)
		{
			for (auto const &expr : exprs)
			{
				sint_array.append(expr.get_constant_value().get_sint_array());
			}
		}
		else
		{
			for (auto const &expr : exprs)
			{
				sint_array.push_back(expr.get_constant_value().get_sint());
			}
		}
		return result;
	}
	case ast::type_info::uint8_:
	case ast::type_info::uint16_:
	case ast::type_info::uint32_:
	case ast::type_info::uint64_:
	{
		auto result = ast::constant_value();
		auto &uint_array = result.emplace<ast::constant_value::uint_array>();
		uint_array.reserve(size);
		if (is_multi_dimensional)
		{
			for (auto const &expr : exprs)
			{
				uint_array.append(expr.get_constant_value().get_uint_array());
			}
		}
		else
		{
			for (auto const &expr : exprs)
			{
				uint_array.push_back(expr.get_constant_value().get_uint());
			}
		}
		return result;
	}
	case ast::type_info::float32_:
	{
		auto result = ast::constant_value();
		auto &float32_array = result.emplace<ast::constant_value::float32_array>();
		float32_array.reserve(size);
		if (is_multi_dimensional)
		{
			for (auto const &expr : exprs)
			{
				float32_array.append(expr.get_constant_value().get_float32_array());
			}
		}
		else
		{
			for (auto const &expr : exprs)
			{
				float32_array.push_back(expr.get_constant_value().get_float32());
			}
		}
		return result;
	}
	case ast::type_info::float64_:
	{
		auto result = ast::constant_value();
		auto &float64_array = result.emplace<ast::constant_value::float64_array>();
		float64_array.reserve(size);
		if (is_multi_dimensional)
		{
			for (auto const &expr : exprs)
			{
				float64_array.append(expr.get_constant_value().get_float64_array());
			}
		}
		else
		{
			for (auto const &expr : exprs)
			{
				float64_array.push_back(expr.get_constant_value().get_float64());
			}
		}
		return result;
	}
	default:
		bz_unreachable;
	}
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
		[](ast::expr_enum_literal &) -> ast::constant_value {
			// these are always constant expressions
			bz_unreachable;
		},
		[](ast::expr_typed_literal &) -> ast::constant_value {
			// these are always constant expressions
			bz_unreachable;
		},
		[](ast::expr_placeholder_literal &) -> ast::constant_value {
			return {};
		},
		[&context](ast::expr_tuple &tuple) -> ast::constant_value {
			for (auto &elem : tuple.elems)
			{
				consteval_guaranteed(elem, context);
			}
			return {};
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
					bz_assert(binary_op.lhs.is_constant());
					auto const &lhs_value = binary_op.lhs.get_constant_value();
					bz_assert(lhs_value.is_boolean());
					auto const lhs_bool_val = lhs_value.get_boolean();
					if (!lhs_bool_val)
					{
						return ast::constant_value(false);
					}
				}
				else if (op == lex::token::bool_or)
				{
					bz_assert(binary_op.lhs.is_constant());
					auto const &lhs_value = binary_op.lhs.get_constant_value();
					bz_assert(lhs_value.is_boolean());
					auto const lhs_bool_val = lhs_value.get_boolean();
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
		[&context](ast::expr_tuple_subscript &tuple_subscript_expr) -> ast::constant_value {
			for (auto &elem : tuple_subscript_expr.base.elems)
			{
				consteval_guaranteed(elem, context);
			}

			return evaluate_tuple_subscript(tuple_subscript_expr);
		},
		[&context](ast::expr_rvalue_tuple_subscript &rvalue_tuple_subscript_expr) -> ast::constant_value {
			consteval_guaranteed(rvalue_tuple_subscript_expr.base, context);

			if (rvalue_tuple_subscript_expr.base.is_constant())
			{
				bz_assert(rvalue_tuple_subscript_expr.index.is_constant());
				auto const &index_value = rvalue_tuple_subscript_expr.index.get_constant_value();
				bz_assert(index_value.is_uint() || index_value.is_sint());
				auto const index = index_value.is_uint()
					? index_value.get_uint()
					: static_cast<uint64_t>(index_value.get_sint());
				return rvalue_tuple_subscript_expr.base.get_constant_value().get_aggregate()[index];
			}
			else
			{
				return {};
			}
		},
		[&context](ast::expr_subscript &subscript_expr) -> ast::constant_value {
			consteval_guaranteed(subscript_expr.base, context);
			consteval_guaranteed(subscript_expr.index, context);

			// don't evaluate the subscript, because that may cause it to convert from an
			// lvalue reference to an rvalue
			return {};
		},
		[&context](ast::expr_rvalue_array_subscript &rvalue_array_subscript_expr) -> ast::constant_value {
			consteval_guaranteed(rvalue_array_subscript_expr.base, context);
			consteval_guaranteed(rvalue_array_subscript_expr.index, context);

			return evaluate_subscript(rvalue_array_subscript_expr.base, rvalue_array_subscript_expr.index, context);
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
		[&context](ast::expr_optional_cast &optional_cast_expr) -> ast::constant_value {
			consteval_guaranteed(optional_cast_expr.expr, context);
			if (optional_cast_expr.expr.has_consteval_succeeded())
			{
				return optional_cast_expr.expr.get_constant_value();
			}
			else
			{
				return {};
			}
		},
		[](ast::expr_take_reference const &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_take_move_reference const &) -> ast::constant_value {
			return {};
		},
		[&context](ast::expr_aggregate_init &aggregate_init_expr) -> ast::constant_value {
			bool is_consteval = true;
			for (auto &expr : aggregate_init_expr.exprs)
			{
				consteval_guaranteed(expr, context);
				is_consteval = is_consteval && expr.has_consteval_succeeded();
			}
			if (!is_consteval)
			{
				return {};
			}

			if (is_special_array_type(aggregate_init_expr.type))
			{
				return get_special_array_value(aggregate_init_expr.type, aggregate_init_expr.exprs);
			}
			else if (aggregate_init_expr.type.is<ast::ts_array>())
			{
				auto const [elem_type, size, is_multi_dimensional] = get_flattened_array_type_and_size(aggregate_init_expr.type);
				auto result = ast::constant_value();
				auto &array = result.emplace<ast::constant_value::array>();
				array.reserve(size);
				if (is_multi_dimensional)
				{
					for (auto const &expr : aggregate_init_expr.exprs)
					{
						array.append(expr.get_constant_value().get_array());
					}
				}
				else
				{
					for (auto const &expr : aggregate_init_expr.exprs)
					{
						array.emplace_back(expr.get_constant_value());
					}
				}

				return result;
			}
			else
			{
				auto result = ast::constant_value();
				auto &aggregate = result.emplace<ast::constant_value::aggregate>();
				aggregate.reserve(aggregate_init_expr.exprs.size());
				for (auto const &expr : aggregate_init_expr.exprs)
				{
					aggregate.emplace_back(expr.get_constant_value());
				}
				return result;
			}
		},
		[&context](ast::expr_aggregate_default_construct &aggregate_default_construct_expr) -> ast::constant_value {
			bool is_consteval = true;
			for (auto &expr : aggregate_default_construct_expr.default_construct_exprs)
			{
				consteval_guaranteed(expr, context);
				is_consteval = is_consteval && expr.has_consteval_succeeded();
			}
			if (!is_consteval)
			{
				return {};
			}

			ast::constant_value result{};
			auto &aggregate = aggregate_default_construct_expr.type.is<ast::ts_tuple>()
				? result.emplace<ast::constant_value::tuple>()
				: result.emplace<ast::constant_value::aggregate>();
			aggregate.reserve(aggregate_default_construct_expr.default_construct_exprs.size());
			for (auto const &expr : aggregate_default_construct_expr.default_construct_exprs)
			{
				aggregate.emplace_back(expr.get_constant_value());
			}
			return result;
		},
		[&expr, &context](ast::expr_aggregate_copy_construct &aggregate_copy_construct_expr) -> ast::constant_value {
			consteval_guaranteed(aggregate_copy_construct_expr.copied_value, context);
			if (!aggregate_copy_construct_expr.copied_value.has_consteval_succeeded())
			{
				return {};
			}

			if (context.is_trivially_copy_constructible(expr.src_tokens, aggregate_copy_construct_expr.copied_value.get_expr_type()))
			{
				return aggregate_copy_construct_expr.copied_value.get_constant_value();
			}
			else
			{
				return {};
			}
		},
		[&context](ast::expr_aggregate_move_construct &aggregate_move_construct_expr) -> ast::constant_value {
			consteval_guaranteed(aggregate_move_construct_expr.moved_value, context);
			return {};
		},
		[&expr, &context](ast::expr_array_default_construct &array_default_construct_expr) -> ast::constant_value {
			auto const type = array_default_construct_expr.type.as_typespec_view();
			bz_assert(type.is<ast::ts_array>());
			consteval_guaranteed(array_default_construct_expr.default_construct_expr, context);
			if (!array_default_construct_expr.default_construct_expr.is_constant())
			{
				return {};
			}

			if (is_special_array_type(type))
			{
				return get_default_constructed_value(expr.src_tokens, type, function_execution_kind::force_evaluate, context);
			}
			else
			{
				auto const &value = array_default_construct_expr.default_construct_expr.get_constant_value();
				auto const [elem_type, size, is_multi_dimensional] = get_flattened_array_type_and_size(type);
				auto result = ast::constant_value();
				if (is_multi_dimensional)
				{
					bz_assert(value.is_array() && value.get_array().not_empty());
					result.emplace<ast::constant_value::array>(size, value.get_array()[0]);
				}
				else
				{
					result.emplace<ast::constant_value::array>(size, value);
				}
				return result;
			}
		},
		[&expr, &context](ast::expr_array_copy_construct &array_copy_construct_expr) -> ast::constant_value {
			consteval_guaranteed(array_copy_construct_expr.copied_value, context);
			if (!array_copy_construct_expr.copied_value.has_consteval_succeeded())
			{
				return {};
			}

			if (context.is_trivially_copy_constructible(expr.src_tokens, array_copy_construct_expr.copied_value.get_expr_type()))
			{
				return array_copy_construct_expr.copied_value.get_constant_value();
			}
			else
			{
				return {};
			}
		},
		[&context](ast::expr_array_move_construct &array_move_construct_expr) -> ast::constant_value {
			consteval_guaranteed(array_move_construct_expr.moved_value, context);
			return {};
		},
		[&expr, &context](ast::expr_optional_default_construct &optional_default_construct_expr) -> ast::constant_value {
			if (context.is_trivially_destructible(expr.src_tokens, optional_default_construct_expr.type))
			{
				return ast::constant_value::get_null();
			}
			else
			{
				return {};
			}
		},
		[&expr, &context](ast::expr_optional_copy_construct &optional_copy_construct_expr) -> ast::constant_value {
			consteval_guaranteed(optional_copy_construct_expr.copied_value, context);
			if (!optional_copy_construct_expr.copied_value.has_consteval_succeeded())
			{
				return {};
			}

			if (context.is_trivially_copy_constructible(expr.src_tokens, optional_copy_construct_expr.copied_value.get_expr_type()))
			{
				return optional_copy_construct_expr.copied_value.get_constant_value();
			}
			else
			{
				return {};
			}
		},
		[&expr, &context](ast::expr_optional_move_construct &optional_move_construct_expr) -> ast::constant_value {
			consteval_guaranteed(optional_move_construct_expr.moved_value, context);
			if (!optional_move_construct_expr.moved_value.has_consteval_succeeded())
			{
				return {};
			}

			if (context.is_trivially_copy_constructible(expr.src_tokens, optional_move_construct_expr.moved_value.get_expr_type()))
			{
				return optional_move_construct_expr.moved_value.get_constant_value();
			}
			else
			{
				return {};
			}
		},
		[](ast::expr_builtin_default_construct &builtin_default_construct_expr) -> ast::constant_value {
			bz_assert(builtin_default_construct_expr.type.is<ast::ts_array_slice>());
			return {};
		},
		[&expr, &context](ast::expr_builtin_copy_construct &builtin_copy_construct_expr) -> ast::constant_value {
			consteval_guaranteed(builtin_copy_construct_expr.copied_value, context);
			if (!builtin_copy_construct_expr.copied_value.has_consteval_succeeded())
			{
				return {};
			}

			bz_assert(context.is_trivially_copy_constructible(expr.src_tokens, builtin_copy_construct_expr.copied_value.get_expr_type()));
			return builtin_copy_construct_expr.copied_value.get_constant_value();
		},
		[&context](ast::expr_trivial_relocate &trivial_relocate_expr) -> ast::constant_value {
			consteval_guaranteed(trivial_relocate_expr.value, context);
			return {};
		},
		[](ast::expr_aggregate_destruct &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_array_destruct &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_optional_destruct &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_base_type_destruct &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_destruct_value &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_aggregate_assign &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_array_assign &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_optional_assign &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_optional_null_assign &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_optional_value_assign &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_base_type_assign &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_trivial_assign &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_aggregate_swap &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_array_swap &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_optional_swap &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_base_type_swap &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_trivial_swap &) -> ast::constant_value {
			return {};
		},
		[&context](ast::expr_member_access &member_access_expr) -> ast::constant_value {
			consteval_guaranteed(member_access_expr.base, context);
			if (member_access_expr.base.has_consteval_succeeded())
			{
				bz_assert(member_access_expr.base.get_constant_value().is_aggregate());
				return member_access_expr.base
					.get_constant_value()
					.get_aggregate()[member_access_expr.index];
			}
			else
			{
				return {};
			}
		},
		[&context, &expr](ast::expr_optional_extract_value &optional_extract_value) -> ast::constant_value {
			consteval_guaranteed(optional_extract_value.optional_value, context);
			if (optional_extract_value.optional_value.has_consteval_succeeded())
			{
				auto const &value = optional_extract_value.optional_value.get_constant_value();
				if (value.is_null_constant())
				{
					context.report_warning(
						ctx::warning_kind::get_value_null,
						expr.src_tokens,
						"getting value of a null optional"
					);
					return {};
				}
				else
				{
					return value;
				}
			}
			else
			{
				return {};
			}
		},
		[&context](ast::expr_rvalue_member_access &rvalue_member_access_expr) -> ast::constant_value {
			consteval_guaranteed(rvalue_member_access_expr.base, context);
			if (rvalue_member_access_expr.base.has_consteval_succeeded())
			{
				bz_assert(rvalue_member_access_expr.base.get_constant_value().is_aggregate());
				return rvalue_member_access_expr.base
					.get_constant_value()
					.get_aggregate()[rvalue_member_access_expr.index];
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
					return compound_expr.final_expr.get_constant_value();
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
				bz_assert(if_expr.condition.is_constant());
				bz_assert(if_expr.condition.get_constant_value().is_boolean());
				auto const condition_value = if_expr.condition
					.get_constant_value()
					.get_boolean();
				if (condition_value)
				{
					if (if_expr.then_block.has_consteval_succeeded())
					{
						return if_expr.then_block.get_constant_value();
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
						return if_expr.else_block.get_constant_value();
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
			bz_assert(if_expr.condition.is_constant());
			auto const &condition_value = if_expr.condition.get_constant_value();
			bz_assert(condition_value.is_boolean());
			if (condition_value.get_boolean())
			{
				consteval_guaranteed(if_expr.then_block, context);
				if (if_expr.then_block.has_consteval_succeeded())
				{
					bz_assert(if_expr.then_block.is_constant());
					return if_expr.then_block.get_constant_value();
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
					bz_assert(if_expr.else_block.is_constant());
					return if_expr.else_block.get_constant_value();
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

			auto const &matched_value = switch_expr.matched_expr.get_constant_value();
			auto const case_it = std::find_if(
				switch_expr.cases.begin(), switch_expr.cases.end(),
				[&](auto const &switch_case) {
					return switch_case.values
						.transform([](auto const &expr) -> auto const &{ return expr.get_constant_value(); })
						.is_any(matched_value);
				}
			);
			if (case_it == switch_expr.cases.end())
			{
				if (switch_expr.default_case.has_consteval_succeeded())
				{
					return switch_expr.default_case.get_constant_value();
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
					return case_it->expr.get_constant_value();
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
		[](ast::expr_bitcode_value_reference &) -> ast::constant_value {
			return {};
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
		[](ast::expr_enum_literal &) -> ast::constant_value {
			// these are always constant expressions
			bz_unreachable;
		},
		[](ast::expr_typed_literal &) -> ast::constant_value {
			// these are always constant expressions
			bz_unreachable;
		},
		[](ast::expr_placeholder_literal &) -> ast::constant_value {
			return {};
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

			auto const expr_type = expr.get_expr_type();
			if (is_special_array_type(expr_type))
			{
				return get_special_array_value(expr_type, tuple.elems);
			}
			else if (expr_type.is<ast::ts_array>())
			{
				auto const [elem_type, size, is_multi_dimensional] = get_flattened_array_type_and_size(expr_type);
				auto result = ast::constant_value();
				auto &array = result.emplace<ast::constant_value::array>();
				array.reserve(size);
				if (is_multi_dimensional)
				{
					for (auto &elem : tuple.elems)
					{
						bz_assert(elem.get_constant_value().is_array());
						array.append(elem.get_constant_value().get_array());
					}
				}
				else
				{
					for (auto &elem : tuple.elems)
					{
						array.emplace_back(elem.get_constant_value());
					}
				}
				return result;
			}
			else
			{
				auto result = ast::constant_value();
				auto &elem_values = result.emplace<ast::constant_value::tuple>();
				elem_values.reserve(tuple.elems.size());
				for (auto &elem : tuple.elems)
				{
					bz_assert(elem.is_constant());
					elem_values.emplace_back(elem.get_constant_value());
				}
				return result;
			}
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
					bz_assert(binary_op.lhs.is_constant());
					auto const &lhs_value = binary_op.lhs.get_constant_value();
					bz_assert(lhs_value.is_boolean());
					auto const lhs_bool_val = lhs_value.get_boolean();
					if (!lhs_bool_val)
					{
						return ast::constant_value(false);
					}
				}
				else if (op == lex::token::bool_or)
				{
					bz_assert(binary_op.lhs.is_constant());
					auto const &lhs_value = binary_op.lhs.get_constant_value();
					bz_assert(lhs_value.is_boolean());
					auto const lhs_bool_val = lhs_value.get_boolean();
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
		[&context](ast::expr_tuple_subscript &tuple_subscript_expr) -> ast::constant_value {
			for (auto &elem : tuple_subscript_expr.base.elems)
			{
				consteval_try(elem, context);
			}

			return evaluate_tuple_subscript(tuple_subscript_expr);
		},
		[&context](ast::expr_rvalue_tuple_subscript &rvalue_tuple_subscript_expr) -> ast::constant_value {
			consteval_try(rvalue_tuple_subscript_expr.base, context);

			if (rvalue_tuple_subscript_expr.base.is_constant())
			{
				bz_assert(rvalue_tuple_subscript_expr.index.is_constant());
				auto const &index_value = rvalue_tuple_subscript_expr.index.get_constant_value();
				bz_assert(index_value.is_uint() || index_value.is_sint());
				auto const index = index_value.is_uint()
					? index_value.get_uint()
					: static_cast<uint64_t>(index_value.get_sint());
				return rvalue_tuple_subscript_expr.base.get_constant_value().get_aggregate()[index];
			}
			else
			{
				return {};
			}
		},
		[&context](ast::expr_subscript &subscript_expr) -> ast::constant_value {
			consteval_try(subscript_expr.base, context);
			consteval_try(subscript_expr.index, context);

			return evaluate_subscript(subscript_expr.base, subscript_expr.index, context);
		},
		[&context](ast::expr_rvalue_array_subscript &rvalue_array_subscript_expr) -> ast::constant_value {
			consteval_try(rvalue_array_subscript_expr.base, context);
			consteval_try(rvalue_array_subscript_expr.index, context);

			return evaluate_subscript(rvalue_array_subscript_expr.base, rvalue_array_subscript_expr.index, context);
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
		[&context](ast::expr_optional_cast &optional_cast_expr) -> ast::constant_value {
			consteval_try(optional_cast_expr.expr, context);
			if (optional_cast_expr.expr.has_consteval_succeeded())
			{
				return optional_cast_expr.expr.get_constant_value();
			}
			else
			{
				return {};
			}
		},
		[](ast::expr_take_reference const &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_take_move_reference const &) -> ast::constant_value {
			return {};
		},
		[&context](ast::expr_aggregate_init &aggregate_init_expr) -> ast::constant_value {
			bool is_consteval = true;
			for (auto &expr : aggregate_init_expr.exprs)
			{
				consteval_try(expr, context);
				is_consteval = is_consteval && expr.has_consteval_succeeded();
			}
			if (!is_consteval)
			{
				return {};
			}

			if (is_special_array_type(aggregate_init_expr.type))
			{
				return get_special_array_value(aggregate_init_expr.type, aggregate_init_expr.exprs);
			}
			else if (aggregate_init_expr.type.is<ast::ts_array>())
			{
				auto const [elem_type, size, is_multi_dimensional] = get_flattened_array_type_and_size(aggregate_init_expr.type);
				auto result = ast::constant_value();
				auto &array = result.emplace<ast::constant_value::array>();
				array.reserve(size);
				if (is_multi_dimensional)
				{
					for (auto const &expr : aggregate_init_expr.exprs)
					{
						array.append(expr.get_constant_value().get_array());
					}
				}
				else
				{
					for (auto const &expr : aggregate_init_expr.exprs)
					{
						array.emplace_back(expr.get_constant_value());
					}
				}

				return result;
			}
			else
			{
				auto result = ast::constant_value();
				auto &aggregate = result.emplace<ast::constant_value::aggregate>();
				aggregate.reserve(aggregate_init_expr.exprs.size());
				for (auto const &expr : aggregate_init_expr.exprs)
				{
					aggregate.emplace_back(expr.get_constant_value());
				}
				return result;
			}
		},
		[&context](ast::expr_aggregate_default_construct &aggregate_default_construct_expr) -> ast::constant_value {
			bool is_consteval = true;
			for (auto &expr : aggregate_default_construct_expr.default_construct_exprs)
			{
				consteval_try(expr, context);
				is_consteval = is_consteval && expr.has_consteval_succeeded();
			}
			if (!is_consteval)
			{
				return {};
			}

			ast::constant_value result{};
			auto &aggregate = aggregate_default_construct_expr.type.is<ast::ts_tuple>()
				? result.emplace<ast::constant_value::tuple>()
				: result.emplace<ast::constant_value::aggregate>();
			aggregate.reserve(aggregate_default_construct_expr.default_construct_exprs.size());
			for (auto const &expr : aggregate_default_construct_expr.default_construct_exprs)
			{
				aggregate.emplace_back(expr.get_constant_value());
			}
			return result;
		},
		[&expr, &context](ast::expr_aggregate_copy_construct &aggregate_copy_construct_expr) -> ast::constant_value {
			consteval_try(aggregate_copy_construct_expr.copied_value, context);
			if (!aggregate_copy_construct_expr.copied_value.has_consteval_succeeded())
			{
				return {};
			}

			if (context.is_trivially_copy_constructible(expr.src_tokens, aggregate_copy_construct_expr.copied_value.get_expr_type()))
			{
				return aggregate_copy_construct_expr.copied_value.get_constant_value();
			}
			else
			{
				return {};
			}
		},
		[&context](ast::expr_aggregate_move_construct &aggregate_move_construct_expr) -> ast::constant_value {
			consteval_try(aggregate_move_construct_expr.moved_value, context);
			return {};
		},
		[&expr, &context](ast::expr_array_default_construct &array_default_construct_expr) -> ast::constant_value {
			auto const type = array_default_construct_expr.type.as_typespec_view();
			bz_assert(type.is<ast::ts_array>());
			consteval_try(array_default_construct_expr.default_construct_expr, context);
			if (!array_default_construct_expr.default_construct_expr.is<ast::constant_expression>())
			{
				return {};
			}

			if (is_special_array_type(type))
			{
				return get_default_constructed_value(expr.src_tokens, type, function_execution_kind::force_evaluate, context);
			}
			else
			{
				auto const &value = array_default_construct_expr.default_construct_expr.get_constant_value();
				auto const [elem_type, size, is_multi_dimensional] = get_flattened_array_type_and_size(type);
				auto result = ast::constant_value();
				if (is_multi_dimensional)
				{
					bz_assert(value.is_array() && value.get_array().not_empty());
					result.emplace<ast::constant_value::array>(size, value.get_array()[0]);
				}
				else
				{
					result.emplace<ast::constant_value::array>(size, value);
				}
				return result;
			}
		},
		[&expr, &context](ast::expr_array_copy_construct &array_copy_construct_expr) -> ast::constant_value {
			consteval_try(array_copy_construct_expr.copied_value, context);
			if (!array_copy_construct_expr.copied_value.has_consteval_succeeded())
			{
				return {};
			}

			if (context.is_trivially_copy_constructible(expr.src_tokens, array_copy_construct_expr.copied_value.get_expr_type()))
			{
				return array_copy_construct_expr.copied_value.get_constant_value();
			}
			else
			{
				return {};
			}
		},
		[&context](ast::expr_array_move_construct &array_move_construct_expr) -> ast::constant_value {
			consteval_try(array_move_construct_expr.moved_value, context);
			return {};
		},
		[&expr, &context](ast::expr_optional_default_construct &optional_default_construct_expr) -> ast::constant_value {
			if (context.is_trivially_destructible(expr.src_tokens, optional_default_construct_expr.type))
			{
				return ast::constant_value::get_null();
			}
			else
			{
				return {};
			}
		},
		[&expr, &context](ast::expr_optional_copy_construct &optional_copy_construct_expr) -> ast::constant_value {
			consteval_try(optional_copy_construct_expr.copied_value, context);
			if (!optional_copy_construct_expr.copied_value.has_consteval_succeeded())
			{
				return {};
			}

			if (context.is_trivially_copy_constructible(expr.src_tokens, optional_copy_construct_expr.copied_value.get_expr_type()))
			{
				return optional_copy_construct_expr.copied_value.get_constant_value();
			}
			else
			{
				return {};
			}
		},
		[&expr, &context](ast::expr_optional_move_construct &optional_move_construct_expr) -> ast::constant_value {
			consteval_try(optional_move_construct_expr.moved_value, context);
			if (!optional_move_construct_expr.moved_value.has_consteval_succeeded())
			{
				return {};
			}

			if (context.is_trivially_copy_constructible(expr.src_tokens, optional_move_construct_expr.moved_value.get_expr_type()))
			{
				return optional_move_construct_expr.moved_value.get_constant_value();
			}
			else
			{
				return {};
			}
		},
		[](ast::expr_builtin_default_construct &builtin_default_construct_expr) -> ast::constant_value {
			bz_assert(builtin_default_construct_expr.type.is<ast::ts_array_slice>());
			return {};
		},
		[&expr, &context](ast::expr_builtin_copy_construct &builtin_copy_construct_expr) -> ast::constant_value {
			consteval_try(builtin_copy_construct_expr.copied_value, context);
			if (!builtin_copy_construct_expr.copied_value.has_consteval_succeeded())
			{
				return {};
			}

			bz_assert(context.is_trivially_copy_constructible(expr.src_tokens, builtin_copy_construct_expr.copied_value.get_expr_type()));
			return builtin_copy_construct_expr.copied_value.get_constant_value();
		},
		[&context](ast::expr_trivial_relocate &trivial_relocate_expr) -> ast::constant_value {
			consteval_try(trivial_relocate_expr.value, context);
			return {};
		},
		[](ast::expr_aggregate_destruct &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_array_destruct &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_optional_destruct &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_base_type_destruct &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_destruct_value &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_aggregate_assign &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_array_assign &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_optional_assign &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_optional_null_assign &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_optional_value_assign &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_base_type_assign &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_trivial_assign &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_aggregate_swap &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_array_swap &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_optional_swap &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_base_type_swap &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_trivial_swap &) -> ast::constant_value {
			return {};
		},
		[&context](ast::expr_member_access &member_access_expr) -> ast::constant_value {
			consteval_try(member_access_expr.base, context);
			if (member_access_expr.base.has_consteval_succeeded())
			{
				return member_access_expr.base
					.get_constant_value()
					.get_aggregate()[member_access_expr.index];
			}
			else
			{
				return {};
			}
		},
		[&context, &expr](ast::expr_optional_extract_value &optional_extract_value) -> ast::constant_value {
			consteval_try(optional_extract_value.optional_value, context);
			if (optional_extract_value.optional_value.has_consteval_succeeded())
			{
				auto const &value = optional_extract_value.optional_value.get_constant_value();
				if (value.is_null_constant())
				{
					context.report_warning(
						ctx::warning_kind::get_value_null,
						expr.src_tokens,
						"getting value of a null optional"
					);
					return {};
				}
				else
				{
					return value;
				}
			}
			else
			{
				return {};
			}
		},
		[&context](ast::expr_rvalue_member_access &rvalue_member_access_expr) -> ast::constant_value {
			consteval_try(rvalue_member_access_expr.base, context);
			if (rvalue_member_access_expr.base.has_consteval_succeeded())
			{
				bz_assert(rvalue_member_access_expr.base.get_constant_value().is_aggregate());
				return rvalue_member_access_expr.base
					.get_constant_value()
					.get_aggregate()[rvalue_member_access_expr.index];
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
					return compound_expr.final_expr.get_constant_value();
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
				bz_assert(if_expr.condition.is_constant());
				bz_assert(if_expr.condition.get_constant_value().is_boolean());
				auto const condition_value = if_expr.condition
					.get_constant_value()
					.get_boolean();
				if (condition_value)
				{
					if (if_expr.then_block.has_consteval_succeeded())
					{
						return if_expr.then_block.get_constant_value();
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
						return if_expr.else_block.get_constant_value();
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
			bz_assert(if_expr.condition.is_constant());
			auto const &condition_value = if_expr.condition.get_constant_value();
			bz_assert(condition_value.is_boolean());
			if (condition_value.get_boolean())
			{
				consteval_try(if_expr.then_block, context);
				if (if_expr.then_block.has_consteval_succeeded())
				{
					bz_assert(if_expr.then_block.is_constant());
					return if_expr.then_block.get_constant_value();
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
					bz_assert(if_expr.else_block.is_constant());
					return if_expr.else_block.get_constant_value();
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

			auto const &matched_value = switch_expr.matched_expr.get_constant_value();
			auto const case_it = std::find_if(
				switch_expr.cases.begin(), switch_expr.cases.end(),
				[&](auto const &switch_case) {
					return switch_case.values
						.transform([](auto const &expr) -> auto const &{ return expr.get_constant_value(); })
						.is_any(matched_value);
				}
			);
			if (case_it == switch_expr.cases.end())
			{
				if (switch_expr.default_case.has_consteval_succeeded())
				{
					return switch_expr.default_case.get_constant_value();
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
					return case_it->expr.get_constant_value();
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
		[](ast::expr_bitcode_value_reference &) -> ast::constant_value {
			return {};
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
		[](ast::expr_enum_literal &) -> ast::constant_value {
			// these are always constant expressions
			bz_unreachable;
		},
		[](ast::expr_typed_literal &) -> ast::constant_value {
			// these are always constant expressions
			bz_unreachable;
		},
		[](ast::expr_placeholder_literal &) -> ast::constant_value {
			return {};
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

			auto const expr_type = expr.get_expr_type();
			if (is_special_array_type(expr_type))
			{
				return get_special_array_value(expr_type, tuple.elems);
			}
			else if (expr_type.is<ast::ts_array>())
			{
				auto const [elem_type, size, is_multi_dimensional] = get_flattened_array_type_and_size(expr_type);
				auto result = ast::constant_value();
				auto &array = result.emplace<ast::constant_value::array>();
				array.reserve(size);
				if (is_multi_dimensional)
				{
					for (auto &elem : tuple.elems)
					{
						bz_assert(elem.get_constant_value().is_array());
						array.append(elem.get_constant_value().get_array());
					}
				}
				else
				{
					for (auto &elem : tuple.elems)
					{
						array.emplace_back(elem.get_constant_value());
					}
				}
				return result;
			}
			else
			{
				auto result = ast::constant_value();
				auto &elem_values = result.emplace<ast::constant_value::tuple>();
				elem_values.reserve(tuple.elems.size());
				for (auto &elem : tuple.elems)
				{
					bz_assert(elem.is_constant());
					elem_values.emplace_back(elem.get_constant_value());
				}
				return result;
			}
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
					bz_assert(binary_op.lhs.is_constant());
					auto const &lhs_value = binary_op.lhs.get_constant_value();
					bz_assert(lhs_value.is_boolean());
					auto const lhs_bool_val = lhs_value.get_boolean();
					if (!lhs_bool_val)
					{
						return ast::constant_value(false);
					}
				}
				else if (op == lex::token::bool_or)
				{
					bz_assert(binary_op.lhs.is_constant());
					auto const &lhs_value = binary_op.lhs.get_constant_value();
					bz_assert(lhs_value.is_boolean());
					auto const lhs_bool_val = lhs_value.get_boolean();
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
		[&context](ast::expr_tuple_subscript &tuple_subscript_expr) -> ast::constant_value {
			for (auto &elem : tuple_subscript_expr.base.elems)
			{
				consteval_try_without_error(elem, context);
			}

			return evaluate_tuple_subscript(tuple_subscript_expr);
		},
		[&context](ast::expr_rvalue_tuple_subscript &rvalue_tuple_subscript_expr) -> ast::constant_value {
			consteval_try_without_error(rvalue_tuple_subscript_expr.base, context);

			if (rvalue_tuple_subscript_expr.base.is_constant())
			{
				bz_assert(rvalue_tuple_subscript_expr.index.is_constant());
				auto const &index_value = rvalue_tuple_subscript_expr.index.get_constant_value();
				bz_assert(index_value.is_uint() || index_value.is_sint());
				auto const index = index_value.is_uint()
					? index_value.get_uint()
					: static_cast<uint64_t>(index_value.get_sint());
				return rvalue_tuple_subscript_expr.base.get_constant_value().get_aggregate()[index];
			}
			else
			{
				return {};
			}
		},
		[&context](ast::expr_subscript &subscript_expr) -> ast::constant_value {
			consteval_try_without_error(subscript_expr.base, context);
			consteval_try_without_error(subscript_expr.index, context);

			return evaluate_subscript(subscript_expr.base, subscript_expr.index, context);
		},
		[&context](ast::expr_rvalue_array_subscript &rvalue_array_subscript_expr) -> ast::constant_value {
			consteval_try_without_error(rvalue_array_subscript_expr.base, context);
			consteval_try_without_error(rvalue_array_subscript_expr.index, context);

			return evaluate_subscript(rvalue_array_subscript_expr.base, rvalue_array_subscript_expr.index, context);
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
		[&context](ast::expr_optional_cast &optional_cast_expr) -> ast::constant_value {
			consteval_try_without_error(optional_cast_expr.expr, context);
			if (optional_cast_expr.expr.has_consteval_succeeded())
			{
				return optional_cast_expr.expr.get_constant_value();
			}
			else
			{
				return {};
			}
		},
		[](ast::expr_take_reference const &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_take_move_reference const &) -> ast::constant_value {
			return {};
		},
		[&context](ast::expr_aggregate_init &aggregate_init_expr) -> ast::constant_value {
			bool is_consteval = true;
			for (auto &expr : aggregate_init_expr.exprs)
			{
				consteval_try_without_error(expr, context);
				is_consteval = is_consteval && expr.has_consteval_succeeded();
			}
			if (!is_consteval)
			{
				return {};
			}

			if (is_special_array_type(aggregate_init_expr.type))
			{
				return get_special_array_value(aggregate_init_expr.type, aggregate_init_expr.exprs);
			}
			else if (aggregate_init_expr.type.is<ast::ts_array>())
			{
				auto const [elem_type, size, is_multi_dimensional] = get_flattened_array_type_and_size(aggregate_init_expr.type);
				auto result = ast::constant_value();
				auto &array = result.emplace<ast::constant_value::array>();
				array.reserve(size);
				if (is_multi_dimensional)
				{
					for (auto const &expr : aggregate_init_expr.exprs)
					{
						array.append(expr.get_constant_value().get_array());
					}
				}
				else
				{
					for (auto const &expr : aggregate_init_expr.exprs)
					{
						array.emplace_back(expr.get_constant_value());
					}
				}

				return result;
			}
			else
			{
				auto result = ast::constant_value();
				auto &aggregate = result.emplace<ast::constant_value::aggregate>();
				aggregate.reserve(aggregate_init_expr.exprs.size());
				for (auto const &expr : aggregate_init_expr.exprs)
				{
					aggregate.emplace_back(expr.get_constant_value());
				}
				return result;
			}
		},
		[&context](ast::expr_aggregate_default_construct &aggregate_default_construct_expr) -> ast::constant_value {
			bool is_consteval = true;
			for (auto &expr : aggregate_default_construct_expr.default_construct_exprs)
			{
				consteval_try_without_error(expr, context);
				is_consteval = is_consteval && expr.has_consteval_succeeded();
			}
			if (!is_consteval)
			{
				return {};
			}

			ast::constant_value result{};
			auto &aggregate = aggregate_default_construct_expr.type.is<ast::ts_tuple>()
				? result.emplace<ast::constant_value::tuple>()
				: result.emplace<ast::constant_value::aggregate>();
			aggregate.reserve(aggregate_default_construct_expr.default_construct_exprs.size());
			for (auto const &expr : aggregate_default_construct_expr.default_construct_exprs)
			{
				aggregate.emplace_back(expr.get_constant_value());
			}
			return result;
		},
		[&expr, &context](ast::expr_aggregate_copy_construct &aggregate_copy_construct_expr) -> ast::constant_value {
			consteval_try_without_error(aggregate_copy_construct_expr.copied_value, context);
			if (!aggregate_copy_construct_expr.copied_value.has_consteval_succeeded())
			{
				return {};
			}

			if (context.is_trivially_copy_constructible(expr.src_tokens, aggregate_copy_construct_expr.copied_value.get_expr_type()))
			{
				return aggregate_copy_construct_expr.copied_value.get_constant_value();
			}
			else
			{
				return {};
			}
		},
		[&context](ast::expr_aggregate_move_construct &aggregate_move_construct_expr) -> ast::constant_value {
			consteval_try_without_error(aggregate_move_construct_expr.moved_value, context);
			return {};
		},
		[&expr, &context](ast::expr_array_default_construct &array_default_construct_expr) -> ast::constant_value {
			auto const type = array_default_construct_expr.type.as_typespec_view();
			bz_assert(type.is<ast::ts_array>());
			consteval_try_without_error(array_default_construct_expr.default_construct_expr, context);
			if (!array_default_construct_expr.default_construct_expr.is<ast::constant_expression>())
			{
				return {};
			}

			if (is_special_array_type(type))
			{
				return get_default_constructed_value(expr.src_tokens, type, function_execution_kind::force_evaluate, context);
			}
			else
			{
				auto const &value = array_default_construct_expr.default_construct_expr.get_constant_value();
				auto const [elem_type, size, is_multi_dimensional] = get_flattened_array_type_and_size(type);
				auto result = ast::constant_value();
				if (is_multi_dimensional)
				{
					bz_assert(value.is_array() && value.get_array().not_empty());
					result.emplace<ast::constant_value::array>(size, value.get_array()[0]);
				}
				else
				{
					result.emplace<ast::constant_value::array>(size, value);
				}
				return result;
			}
		},
		[&expr, &context](ast::expr_array_copy_construct &array_copy_construct_expr) -> ast::constant_value {
			consteval_try_without_error(array_copy_construct_expr.copied_value, context);
			if (!array_copy_construct_expr.copied_value.has_consteval_succeeded())
			{
				return {};
			}

			if (context.is_trivially_copy_constructible(expr.src_tokens, array_copy_construct_expr.copied_value.get_expr_type()))
			{
				return array_copy_construct_expr.copied_value.get_constant_value();
			}
			else
			{
				return {};
			}
		},
		[&context](ast::expr_array_move_construct &array_move_construct_expr) -> ast::constant_value {
			consteval_try_without_error(array_move_construct_expr.moved_value, context);
			return {};
		},
		[&expr, &context](ast::expr_optional_default_construct &optional_default_construct_expr) -> ast::constant_value {
			if (context.is_trivially_destructible(expr.src_tokens, optional_default_construct_expr.type))
			{
				return ast::constant_value::get_null();
			}
			else
			{
				return {};
			}
		},
		[&expr, &context](ast::expr_optional_copy_construct &optional_copy_construct_expr) -> ast::constant_value {
			consteval_try_without_error(optional_copy_construct_expr.copied_value, context);
			if (!optional_copy_construct_expr.copied_value.has_consteval_succeeded())
			{
				return {};
			}

			if (context.is_trivially_copy_constructible(expr.src_tokens, optional_copy_construct_expr.copied_value.get_expr_type()))
			{
				return optional_copy_construct_expr.copied_value.get_constant_value();
			}
			else
			{
				return {};
			}
		},
		[&expr, &context](ast::expr_optional_move_construct &optional_move_construct_expr) -> ast::constant_value {
			consteval_try_without_error(optional_move_construct_expr.moved_value, context);
			if (!optional_move_construct_expr.moved_value.has_consteval_succeeded())
			{
				return {};
			}

			if (context.is_trivially_copy_constructible(expr.src_tokens, optional_move_construct_expr.moved_value.get_expr_type()))
			{
				return optional_move_construct_expr.moved_value.get_constant_value();
			}
			else
			{
				return {};
			}
		},
		[](ast::expr_builtin_default_construct &builtin_default_construct_expr) -> ast::constant_value {
			bz_assert(builtin_default_construct_expr.type.is<ast::ts_array_slice>());
			return {};
		},
		[&expr, &context](ast::expr_builtin_copy_construct &builtin_copy_construct_expr) -> ast::constant_value {
			consteval_try_without_error(builtin_copy_construct_expr.copied_value, context);
			if (!builtin_copy_construct_expr.copied_value.has_consteval_succeeded())
			{
				return {};
			}

			bz_assert(context.is_trivially_copy_constructible(expr.src_tokens, builtin_copy_construct_expr.copied_value.get_expr_type()));
			return builtin_copy_construct_expr.copied_value.get_constant_value();
		},
		[&context](ast::expr_trivial_relocate &trivial_relocate_expr) -> ast::constant_value {
			consteval_try_without_error(trivial_relocate_expr.value, context);
			return {};
		},
		[](ast::expr_aggregate_destruct &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_array_destruct &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_optional_destruct &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_base_type_destruct &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_destruct_value &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_aggregate_assign &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_array_assign &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_optional_assign &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_optional_null_assign &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_optional_value_assign &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_base_type_assign &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_trivial_assign &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_aggregate_swap &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_array_swap &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_optional_swap &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_base_type_swap &) -> ast::constant_value {
			return {};
		},
		[](ast::expr_trivial_swap &) -> ast::constant_value {
			return {};
		},
		[&context](ast::expr_member_access &member_access_expr) -> ast::constant_value {
			consteval_try_without_error(member_access_expr.base, context);
			if (member_access_expr.base.has_consteval_succeeded())
			{
				return member_access_expr.base
					.get_constant_value()
					.get_aggregate()[member_access_expr.index];
			}
			else
			{
				return {};
			}
		},
		[&context, &expr](ast::expr_optional_extract_value &optional_extract_value) -> ast::constant_value {
			consteval_try_without_error(optional_extract_value.optional_value, context);
			if (optional_extract_value.optional_value.has_consteval_succeeded())
			{
				auto const &value = optional_extract_value.optional_value.get_constant_value();
				if (value.is_null_constant())
				{
					context.report_warning(
						ctx::warning_kind::get_value_null,
						expr.src_tokens,
						"getting value of a null optional"
					);
					return {};
				}
				else
				{
					return value;
				}
			}
			else
			{
				return {};
			}
		},
		[&context](ast::expr_rvalue_member_access &rvalue_member_access_expr) -> ast::constant_value {
			consteval_try_without_error(rvalue_member_access_expr.base, context);
			if (rvalue_member_access_expr.base.has_consteval_succeeded())
			{
				bz_assert(rvalue_member_access_expr.base.get_constant_value().is_aggregate());
				return rvalue_member_access_expr.base
					.get_constant_value()
					.get_aggregate()[rvalue_member_access_expr.index];
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
					return compound_expr.final_expr.get_constant_value();
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
				bz_assert(if_expr.condition.is_constant());
				bz_assert(if_expr.condition.get_constant_value().is_boolean());
				auto const condition_value = if_expr.condition
					.get_constant_value()
					.get_boolean();
				if (condition_value)
				{
					if (if_expr.then_block.has_consteval_succeeded())
					{
						return if_expr.then_block.get_constant_value();
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
						return if_expr.else_block.get_constant_value();
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
			bz_assert(if_expr.condition.is_constant());
			auto const &condition_value = if_expr.condition.get_constant_value();
			bz_assert(condition_value.is_boolean());
			if (condition_value.get_boolean())
			{
				consteval_try_without_error(if_expr.then_block, context);
				if (if_expr.then_block.has_consteval_succeeded())
				{
					bz_assert(if_expr.then_block.is_constant());
					return if_expr.then_block.get_constant_value();
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
					bz_assert(if_expr.else_block.is_constant());
					return if_expr.else_block.get_constant_value();
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

			auto const &matched_value = switch_expr.matched_expr.get_constant_value();
			auto const case_it = std::find_if(
				switch_expr.cases.begin(), switch_expr.cases.end(),
				[&](auto const &switch_case) {
					return switch_case.values
						.transform([](auto const &expr) -> auto const &{ return expr.get_constant_value(); })
						.is_any(matched_value);
				}
			);
			if (case_it == switch_expr.cases.end())
			{
				if (switch_expr.default_case.has_consteval_succeeded())
				{
					return switch_expr.default_case.get_constant_value();
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
					return case_it->expr.get_constant_value();
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
		[](ast::expr_bitcode_value_reference &) -> ast::constant_value {
			return {};
		},
	});
}

void consteval_guaranteed(ast::expression &expr, ctx::parse_context &context)
{
	if (expr.is_constant())
	{
		expr.consteval_state = ast::expression::consteval_succeeded;
		return;
	}
	else if (
		!expr.is_dynamic()
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
		auto &dyn_expr  = expr.get_dynamic();
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
	if (expr.is_constant())
	{
		expr.consteval_state = ast::expression::consteval_succeeded;
		return;
	}
	else if (
		!expr.is_dynamic()
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
		auto &dyn_expr  = expr.get_dynamic();
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
	if (expr.is_constant())
	{
		expr.consteval_state = ast::expression::consteval_succeeded;
		return;
	}
	else if (
		!expr.is_dynamic()
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
		auto &dyn_expr  = expr.get_dynamic();
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
		[&context](ast::stmt_defer &defer_stmt) {
			bz_assert(defer_stmt.deferred_expr.is<ast::defer_expression>());
			consteval_try_without_error(*defer_stmt.deferred_expr.get<ast::defer_expression>().expr, context);
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
		[](ast::decl_enum &) {},
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
	bz_assert(expr.is_dynamic());
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
		[](ast::expr_enum_literal const &) {
			// these are always constant expressions
			bz_unreachable;
		},
		[](ast::expr_typed_literal const &) {
			// these are always constant expressions
			bz_unreachable;
		},
		[](ast::expr_placeholder_literal const &) {
			// nothing
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
						ast::get_value_string(unary_op.expr.get_constant_value())
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
						ast::get_value_string(binary_op.lhs.get_constant_value()),
						token_info[binary_op.op].token_value,
						ast::get_value_string(binary_op.rhs.get_constant_value())
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
		[&notes](ast::expr_tuple_subscript const &tuple_subscript_expr) {
			for (auto const &elem : tuple_subscript_expr.base.elems)
			{
				if (elem.has_consteval_failed())
				{
					get_consteval_fail_notes_helper(elem, notes);
				}
			}
		},
		[&notes](ast::expr_rvalue_tuple_subscript const &rvalue_tuple_subscript_expr) {
			get_consteval_fail_notes_helper(rvalue_tuple_subscript_expr.base, notes);
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
		[&expr, &notes](ast::expr_rvalue_array_subscript const &rvalue_array_subscript_expr) {
			bool any_failed = false;
			if (rvalue_array_subscript_expr.base.has_consteval_failed())
			{
				any_failed = true;
				get_consteval_fail_notes_helper(rvalue_array_subscript_expr.base, notes);
			}
			if (rvalue_array_subscript_expr.index.has_consteval_failed())
			{
				any_failed = true;
				get_consteval_fail_notes_helper(rvalue_array_subscript_expr.index, notes);
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
						ast::get_value_string(cast_expr.expr.get_constant_value()),
						cast_expr.type
					)
				));
			}
			else
			{
				get_consteval_fail_notes_helper(cast_expr.expr, notes);
			}
		},
		[&notes](ast::expr_optional_cast const &optional_cast_expr) {
			get_consteval_fail_notes_helper(optional_cast_expr.expr, notes);
		},
		[&expr, &notes](ast::expr_take_reference const &take_ref_expr) {
			if (take_ref_expr.expr.is_constant())
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
		[&expr, &notes](ast::expr_take_move_reference const &) {
			notes.emplace_back(ctx::parse_context::make_note(
				expr.src_tokens, "subexpression is not a constant expression"
			));
		},
		[&notes](ast::expr_aggregate_init const &aggregate_init_expr) {
			for (auto const &expr : aggregate_init_expr.exprs)
			{
				if (expr.has_consteval_failed())
				{
					get_consteval_fail_notes_helper(expr, notes);
				}
			}
		},
		[&notes](ast::expr_aggregate_default_construct const &aggregate_default_construct_expr) {
			for (auto const &expr : aggregate_default_construct_expr.default_construct_exprs)
			{
				if (expr.has_consteval_failed())
				{
					auto const type = expr.get_expr_type();
					notes.emplace_back(ctx::parse_context::make_note(
						expr.src_tokens, bz::format("default construction of a value of type '{}' is not a constant expression", type)
					));
				}
			}
		},
		[&expr, &notes](ast::expr_aggregate_copy_construct const &aggregate_copy_construct_expr) {
			if (aggregate_copy_construct_expr.copied_value.has_consteval_failed())
			{
				get_consteval_fail_notes_helper(aggregate_copy_construct_expr.copied_value, notes);
			}
			else
			{
				auto const type = ast::remove_const_or_consteval(aggregate_copy_construct_expr.copied_value.get_expr_type());
				notes.emplace_back(ctx::parse_context::make_note(
					expr.src_tokens, bz::format("copy construction of a value of type '{}' is not a constant expression", type)
				));
			}
		},
		[&expr, &notes](ast::expr_aggregate_move_construct const &aggregate_move_construct_expr) {
			if (aggregate_move_construct_expr.moved_value.has_consteval_failed())
			{
				get_consteval_fail_notes_helper(aggregate_move_construct_expr.moved_value, notes);
			}
			else
			{
				auto const type = ast::remove_const_or_consteval(aggregate_move_construct_expr.moved_value.get_expr_type());
				notes.emplace_back(ctx::parse_context::make_note(
					expr.src_tokens, bz::format("move construction of a value of type '{}' is not a constant expression", type)
				));
			}
		},
		[&expr, &notes](ast::expr_array_default_construct const &array_default_construct_expr) {
			auto const type = array_default_construct_expr.type.as_typespec_view();
			notes.emplace_back(ctx::parse_context::make_note(
				expr.src_tokens, bz::format("subexpression '{}()' is not a constant expression", type)
			));
		},
		[&expr, &notes](ast::expr_array_copy_construct const &array_copy_construct_expr) {
			if (array_copy_construct_expr.copied_value.has_consteval_failed())
			{
				get_consteval_fail_notes_helper(array_copy_construct_expr.copied_value, notes);
			}
			else
			{
				auto const type = ast::remove_const_or_consteval(array_copy_construct_expr.copied_value.get_expr_type());
				notes.emplace_back(ctx::parse_context::make_note(
					expr.src_tokens, bz::format("copy construction of a value of type '{}' is not a constant expression", type)
				));
			}
		},
		[&expr, &notes](ast::expr_array_move_construct const &array_move_construct_expr) {
			if (array_move_construct_expr.moved_value.has_consteval_failed())
			{
				get_consteval_fail_notes_helper(array_move_construct_expr.moved_value, notes);
			}
			else
			{
				auto const type = ast::remove_const_or_consteval(array_move_construct_expr.moved_value.get_expr_type());
				notes.emplace_back(ctx::parse_context::make_note(
					expr.src_tokens, bz::format("move construction of a value of type '{}' is not a constant expression", type)
				));
			}
		},
		[&expr, &notes](ast::expr_optional_default_construct const &optional_default_construct_expr) {
			auto const type = optional_default_construct_expr.type.as_typespec_view();
			notes.emplace_back(ctx::parse_context::make_note(
				expr.src_tokens, bz::format("subexpression '({})()' is not a constant expression", type)
			));
		},
		[&expr, &notes](ast::expr_optional_copy_construct const &optional_copy_construct_expr) {
			if (optional_copy_construct_expr.copied_value.has_consteval_failed())
			{
				get_consteval_fail_notes_helper(optional_copy_construct_expr.copied_value, notes);
			}
			else
			{
				auto const type = ast::remove_const_or_consteval(optional_copy_construct_expr.copied_value.get_expr_type());
				notes.emplace_back(ctx::parse_context::make_note(
					expr.src_tokens, bz::format("copy construction of a value of type '{}' is not a constant expression", type)
				));
			}
		},
		[&expr, &notes](ast::expr_optional_move_construct const &optional_move_construct_expr) {
			if (optional_move_construct_expr.moved_value.has_consteval_failed())
			{
				get_consteval_fail_notes_helper(optional_move_construct_expr.moved_value, notes);
			}
			else
			{
				auto const type = ast::remove_const_or_consteval(optional_move_construct_expr.moved_value.get_expr_type());
				notes.emplace_back(ctx::parse_context::make_note(
					expr.src_tokens, bz::format("move construction of a value of type '{}' is not a constant expression", type)
				));
			}
		},
		[&expr, &notes](ast::expr_builtin_default_construct const &builtin_default_construct_expr) {
			auto const type = builtin_default_construct_expr.type.as_typespec_view();
			notes.emplace_back(ctx::parse_context::make_note(
				expr.src_tokens, bz::format("subexpression '{}()' is not a constant expression", type)
			));
		},
		[&expr, &notes](ast::expr_builtin_copy_construct const &builtin_copy_construct_expr) {
			if (builtin_copy_construct_expr.copied_value.has_consteval_failed())
			{
				get_consteval_fail_notes_helper(builtin_copy_construct_expr.copied_value, notes);
			}
			else
			{
				auto const type = ast::remove_const_or_consteval(builtin_copy_construct_expr.copied_value.get_expr_type());
				notes.emplace_back(ctx::parse_context::make_note(
					expr.src_tokens, bz::format("copy construction of a value of type '{}' is not a constant expression", type)
				));
			}
		},
		[&expr, &notes](ast::expr_trivial_relocate const &trivial_relocate_expr) {
			if (trivial_relocate_expr.value.has_consteval_failed())
			{
				get_consteval_fail_notes_helper(trivial_relocate_expr.value, notes);
			}
			else
			{
				auto const type = ast::remove_const_or_consteval(trivial_relocate_expr.value.get_expr_type());
				notes.emplace_back(ctx::parse_context::make_note(
					expr.src_tokens, bz::format("move construction of a value of type '{}' is not a constant expression", type)
				));
			}
		},
		[&expr, &notes](ast::expr_aggregate_destruct const &) {
			notes.push_back(ctx::parse_context::make_note(
				expr.src_tokens, "value destruction is not a constant expression"
			));
		},
		[&expr, &notes](ast::expr_array_destruct const &) {
			notes.push_back(ctx::parse_context::make_note(
				expr.src_tokens, "value destruction is not a constant expression"
			));
		},
		[&expr, &notes](ast::expr_optional_destruct const &) {
			notes.push_back(ctx::parse_context::make_note(
				expr.src_tokens, "value destruction is not a constant expression"
			));
		},
		[&expr, &notes](ast::expr_base_type_destruct const &) {
			notes.push_back(ctx::parse_context::make_note(
				expr.src_tokens, "value destruction is not a constant expression"
			));
		},
		[&expr, &notes](ast::expr_destruct_value const &) {
			notes.push_back(ctx::parse_context::make_note(
				expr.src_tokens, "value destruction is not a constant expression"
			));
		},
		[&expr, &notes](ast::expr_aggregate_assign const &) {
			notes.push_back(ctx::parse_context::make_note(
				expr.src_tokens, "assignment is not a constant expression"
			));
		},
		[&expr, &notes](ast::expr_array_assign const &) {
			notes.push_back(ctx::parse_context::make_note(
				expr.src_tokens, "assignment is not a constant expression"
			));
		},
		[&expr, &notes](ast::expr_optional_assign const &) {
			notes.push_back(ctx::parse_context::make_note(
				expr.src_tokens, "assignment is not a constant expression"
			));
		},
		[&expr, &notes](ast::expr_optional_null_assign const &) {
			notes.push_back(ctx::parse_context::make_note(
				expr.src_tokens, "assignment is not a constant expression"
			));
		},
		[&expr, &notes](ast::expr_optional_value_assign const &) {
			notes.push_back(ctx::parse_context::make_note(
				expr.src_tokens, "assignment is not a constant expression"
			));
		},
		[&expr, &notes](ast::expr_base_type_assign const &) {
			notes.push_back(ctx::parse_context::make_note(
				expr.src_tokens, "assignment is not a constant expression"
			));
		},
		[&expr, &notes](ast::expr_trivial_assign const &) {
			notes.push_back(ctx::parse_context::make_note(
				expr.src_tokens, "assignment is not a constant expression"
			));
		},
		[&expr, &notes](ast::expr_aggregate_swap const &) {
			notes.push_back(ctx::parse_context::make_note(
				expr.src_tokens, "swapping is not a constant expression"
			));
		},
		[&expr, &notes](ast::expr_array_swap const &) {
			notes.push_back(ctx::parse_context::make_note(
				expr.src_tokens, "swapping is not a constant expression"
			));
		},
		[&expr, &notes](ast::expr_optional_swap const &) {
			notes.push_back(ctx::parse_context::make_note(
				expr.src_tokens, "swapping is not a constant expression"
			));
		},
		[&expr, &notes](ast::expr_base_type_swap const &) {
			notes.push_back(ctx::parse_context::make_note(
				expr.src_tokens, "swapping is not a constant expression"
			));
		},
		[&expr, &notes](ast::expr_trivial_swap const &) {
			notes.push_back(ctx::parse_context::make_note(
				expr.src_tokens, "swapping is not a constant expression"
			));
		},
		[&notes](ast::expr_member_access const &member_access_expr) {
			bz_assert(!member_access_expr.base.has_consteval_succeeded());
			get_consteval_fail_notes_helper(member_access_expr.base, notes);
		},
		[&expr, &notes](ast::expr_optional_extract_value const &optional_extract_value) {
			if (optional_extract_value.optional_value.has_consteval_failed())
			{
				get_consteval_fail_notes_helper(optional_extract_value.optional_value, notes);
			}
			else if (optional_extract_value.optional_value.get_constant_value().is_null_constant())
			{
				notes.push_back(ctx::parse_context::make_note(
					expr.src_tokens, "getting value of a null optional is not a constant expression"
				));
			}
			else
			{
				notes.push_back(ctx::parse_context::make_note(
					expr.src_tokens,
					bz::format(
						"getting value of an optional of type '{}' is not a constant expression",
						optional_extract_value.optional_value.get_expr_type()
					)
				));
			}
		},
		[&notes](ast::expr_rvalue_member_access const &rvalue_member_access_expr) {
			bz_assert(!rvalue_member_access_expr.base.has_consteval_succeeded());
			get_consteval_fail_notes_helper(rvalue_member_access_expr.base, notes);
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
				bz_assert(if_expr.condition.is_constant());
				bz_assert(if_expr.condition.get_constant_value().is_boolean());
				auto const condition_value = if_expr.condition
					.get_constant_value()
					.get_boolean();
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
			bz_assert(if_expr.condition.is_constant());
			auto const &condition_value = if_expr.condition.get_constant_value();
			bz_assert(condition_value.is_boolean());
			if (condition_value.get_boolean())
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
		[&expr, &notes](ast::expr_bitcode_value_reference const &) {
			notes.emplace_back(ctx::parse_context::make_note(
				expr.src_tokens, "subexpression is not a constant expression"
			));
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

} // namespace resolve
