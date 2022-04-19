#include "safe_operations.h"

namespace parse
{

int64_t safe_unary_minus(
	lex::src_tokens const &src_tokens, int paren_level,
	int64_t value, uint32_t type_kind,
	ctx::parse_context &context
)
{
	using T = std::pair<bz::u8string_view, int64_t>;
	auto const [type_name, min_value] =
		type_kind == ast::type_info::int8_  ? T{ "int8",  std::numeric_limits<int8_t> ::min() } :
		type_kind == ast::type_info::int16_ ? T{ "int16", std::numeric_limits<int16_t>::min() } :
		type_kind == ast::type_info::int32_ ? T{ "int32", std::numeric_limits<int32_t>::min() } :
		T{ "int64", std::numeric_limits<int64_t>::min() };
	if (value == min_value)
	{
		if (paren_level < 2)
		{
			context.report_parenthesis_suppressed_warning(
				2 - paren_level, ctx::warning_kind::int_overflow,
				src_tokens,
				bz::format("overflow in constant expression '-({})' with type '{}' results in {}", value, type_name, value)
			);
		}
		return value;
	}
	else
	{
		return -value;
	}
}


int64_t safe_binary_plus(
	lex::src_tokens const &src_tokens, int paren_level,
	int64_t lhs, int64_t rhs, uint32_t type_kind,
	ctx::parse_context &context
)
{
	using T = std::tuple<bz::u8string_view, int64_t, int64_t>;
	auto const [type_name, min_value, max_value] =
		type_kind == ast::type_info::int8_  ? T{ "int8",  std::numeric_limits<int8_t> ::min(), std::numeric_limits<int8_t> ::max() } :
		type_kind == ast::type_info::int16_ ? T{ "int16", std::numeric_limits<int16_t>::min(), std::numeric_limits<int16_t>::max() } :
		type_kind == ast::type_info::int32_ ? T{ "int32", std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max() } :
		T{ "int64", std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max() };

	int64_t const result =
		type_kind == ast::type_info::int8_  ? static_cast<int8_t> (static_cast<uint64_t>(lhs) + static_cast<uint64_t>(rhs)) :
		type_kind == ast::type_info::int16_ ? static_cast<int16_t>(static_cast<uint64_t>(lhs) + static_cast<uint64_t>(rhs)) :
		type_kind == ast::type_info::int32_ ? static_cast<int32_t>(static_cast<uint64_t>(lhs) + static_cast<uint64_t>(rhs)) :
		static_cast<int64_t>(static_cast<uint64_t>(lhs) + static_cast<uint64_t>(rhs));

	if (
		paren_level < 2
		&& ((lhs > 0 && rhs > 0 && lhs > max_value - rhs)
		|| (lhs < 0 && rhs < 0 && lhs < min_value - rhs))
	)
	{
		context.report_parenthesis_suppressed_warning(
			2 - paren_level, ctx::warning_kind::int_overflow,
			src_tokens,
			bz::format("overflow in constant expression '{} + {}' with type '{}' results in {}", lhs, rhs, type_name, result)
		);
	}
	return result;
}

uint64_t safe_binary_plus(
	lex::src_tokens const &src_tokens, int paren_level,
	uint64_t lhs, uint64_t rhs, uint32_t type_kind,
	ctx::parse_context &context
)
{
	using T = std::tuple<bz::u8string_view, uint64_t>;
	auto const [type_name, max_value] =
		type_kind == ast::type_info::uint8_  ? T{ "uint8",  std::numeric_limits<uint8_t> ::max() } :
		type_kind == ast::type_info::uint16_ ? T{ "uint16", std::numeric_limits<uint16_t>::max() } :
		type_kind == ast::type_info::uint32_ ? T{ "uint32", std::numeric_limits<uint32_t>::max() } :
		T{ "uint64", std::numeric_limits<uint64_t>::max() };

	int64_t const result =
		type_kind == ast::type_info::uint8_  ? static_cast<uint8_t> (lhs + rhs) :
		type_kind == ast::type_info::uint16_ ? static_cast<uint16_t>(lhs + rhs) :
		type_kind == ast::type_info::uint32_ ? static_cast<uint32_t>(lhs + rhs) :
		static_cast<uint64_t>(lhs + rhs);

	if (paren_level < 2 && lhs > max_value - rhs)
	{
		context.report_parenthesis_suppressed_warning(
			2 - paren_level, ctx::warning_kind::int_overflow,
			src_tokens,
			bz::format("overflow in constant expression '{} + {}' with type '{}' results in {}", lhs, rhs, type_name, result)
		);
	}
	return result;
}

float32_t safe_binary_plus(
	lex::src_tokens const &src_tokens, int paren_level,
	float32_t lhs, float32_t rhs,
	ctx::parse_context &context
)
{
	auto const result = lhs + rhs;
	if (
		paren_level < 2
		&& ((!std::isnan(lhs) && !std::isnan(rhs) && std::isnan(result))
		|| (std::isfinite(lhs) && std::isfinite(rhs) && !std::isfinite(result)))
	)
	{
		context.report_parenthesis_suppressed_warning(
			2 - paren_level, ctx::warning_kind::float_overflow,
			src_tokens,
			bz::format("result of floating point arithmetic in constant expression '{} + {}' with type 'float32' is {}", lhs, rhs, result)
		);
	}
	return result;
}

float64_t safe_binary_plus(
	lex::src_tokens const &src_tokens, int paren_level,
	float64_t lhs, float64_t rhs,
	ctx::parse_context &context
)
{
	auto const result = lhs + rhs;
	if (
		paren_level < 2
		&& ((!std::isnan(lhs) && !std::isnan(rhs) && std::isnan(result))
		|| (std::isfinite(lhs) && std::isfinite(rhs) && !std::isfinite(result)))
	)
	{
		context.report_parenthesis_suppressed_warning(
			2 - paren_level, ctx::warning_kind::float_overflow,
			src_tokens,
			bz::format("result of floating point arithmetic in constant expression '{} + {}' with type 'float64' is {}", lhs, rhs, result)
		);
	}
	return result;
}

bz::optional<bz::u8char> safe_binary_plus(
	lex::src_tokens const &src_tokens, int paren_level,
	bz::u8char lhs, int64_t rhs,
	ctx::parse_context &context
)
{
	static_assert(bz::meta::is_same<bz::u8char, uint32_t>);
	auto const result = lhs + static_cast<uint32_t>(rhs);
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
	else
	{
		return result;
	}
}

bz::optional<bz::u8char> safe_binary_plus(
	lex::src_tokens const &src_tokens, int paren_level,
	bz::u8char lhs, uint64_t rhs,
	ctx::parse_context &context
)
{
	static_assert(bz::meta::is_same<bz::u8char, uint32_t>);
	auto const result = lhs + static_cast<uint32_t>(rhs);
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
	else
	{
		return result;
	}
}

bz::optional<bz::u8char> safe_binary_plus(
	lex::src_tokens const &src_tokens, int paren_level,
	int64_t lhs, bz::u8char rhs,
	ctx::parse_context &context
)
{
	static_assert(bz::meta::is_same<bz::u8char, uint32_t>);
	auto const result = static_cast<uint32_t>(lhs) + rhs;
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
	else
	{
		return result;
	}
}

bz::optional<bz::u8char> safe_binary_plus(
	lex::src_tokens const &src_tokens, int paren_level,
	uint64_t lhs, bz::u8char rhs,
	ctx::parse_context &context
)
{
	static_assert(bz::meta::is_same<bz::u8char, uint32_t>);
	auto const result = static_cast<uint32_t>(lhs) + rhs;
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
	else
	{
		return result;
	}
}


int64_t safe_binary_minus(
	lex::src_tokens const &src_tokens, int paren_level,
	int64_t lhs, int64_t rhs, uint32_t type_kind,
	ctx::parse_context &context
)
{
	using T = std::tuple<bz::u8string_view, int64_t, int64_t>;
	auto const [type_name, min_value, max_value] =
		type_kind == ast::type_info::int8_  ? T{ "int8",  std::numeric_limits<int8_t> ::min(), std::numeric_limits<int8_t> ::max() } :
		type_kind == ast::type_info::int16_ ? T{ "int16", std::numeric_limits<int16_t>::min(), std::numeric_limits<int16_t>::max() } :
		type_kind == ast::type_info::int32_ ? T{ "int32", std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max() } :
		T{ "int64", std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max() };

	int64_t const result =
		type_kind == ast::type_info::int8_  ? static_cast<int8_t> (static_cast<uint64_t>(lhs) - static_cast<uint64_t>(rhs)) :
		type_kind == ast::type_info::int16_ ? static_cast<int16_t>(static_cast<uint64_t>(lhs) - static_cast<uint64_t>(rhs)) :
		type_kind == ast::type_info::int32_ ? static_cast<int32_t>(static_cast<uint64_t>(lhs) - static_cast<uint64_t>(rhs)) :
		static_cast<int64_t>(static_cast<uint64_t>(lhs) - static_cast<uint64_t>(rhs));

	if (
		paren_level < 2
		&& ((lhs < 0 && rhs > 0 && lhs < min_value + rhs)
		|| (lhs >= 0 && rhs < 0 && lhs > max_value + rhs))
	)
	{
		context.report_parenthesis_suppressed_warning(
			2 - paren_level, ctx::warning_kind::int_overflow,
			src_tokens,
			bz::format("overflow in constant expression '{} - {}' with type '{}' results in {}", lhs, rhs, type_name, result)
		);
	}
	return result;
}

uint64_t safe_binary_minus(
	lex::src_tokens const &src_tokens, int paren_level,
	uint64_t lhs, uint64_t rhs, uint32_t type_kind,
	ctx::parse_context &context
)
{
	bz::u8string_view const type_name =
		type_kind == ast::type_info::uint8_  ? "uint8"  :
		type_kind == ast::type_info::uint16_ ? "uint16" :
		type_kind == ast::type_info::uint32_ ? "uint32" :
		"uint64";

	uint64_t const result =
		type_kind == ast::type_info::uint8_  ? static_cast<uint8_t> (lhs - rhs) :
		type_kind == ast::type_info::uint16_ ? static_cast<uint16_t>(lhs - rhs) :
		type_kind == ast::type_info::uint32_ ? static_cast<uint32_t>(lhs - rhs) :
		static_cast<uint64_t>(lhs - rhs);

	if (paren_level < 2 && lhs < rhs)
	{
		context.report_parenthesis_suppressed_warning(
			2 - paren_level, ctx::warning_kind::int_overflow,
			src_tokens,
			bz::format("overflow in constant expression '{} - {}' with type '{}' results in {}", lhs, rhs, type_name, result)
		);
	}
	return result;
}

float32_t safe_binary_minus(
	lex::src_tokens const &src_tokens, int paren_level,
	float32_t lhs, float32_t rhs,
	ctx::parse_context &context
)
{
	auto const result = lhs - rhs;
	if (
		paren_level < 2
		&& ((!std::isnan(lhs) && !std::isnan(rhs) && std::isnan(result))
		|| (std::isfinite(lhs) && std::isfinite(rhs) && !std::isfinite(result)))
	)
	{
		context.report_parenthesis_suppressed_warning(
			2 - paren_level, ctx::warning_kind::float_overflow,
			src_tokens,
			bz::format("result of floating point arithmetic in constant expression '{} - {}' with type 'float32' is {}", lhs, rhs, result)
		);
	}
	return result;
}

float64_t safe_binary_minus(
	lex::src_tokens const &src_tokens, int paren_level,
	float64_t lhs, float64_t rhs,
	ctx::parse_context &context
)
{
	auto const result = lhs - rhs;
	if (
		paren_level < 2
		&& ((!std::isnan(lhs) && !std::isnan(rhs) && std::isnan(result))
		|| (std::isfinite(lhs) && std::isfinite(rhs) && !std::isfinite(result)))
	)
	{
		context.report_parenthesis_suppressed_warning(
			2 - paren_level, ctx::warning_kind::float_overflow,
			src_tokens,
			bz::format("result of floating point arithmetic in constant expression '{} - {}' with type 'float64' is {}", lhs, rhs, result)
		);
	}
	return result;
}

bz::optional<bz::u8char> safe_binary_minus(
	lex::src_tokens const &src_tokens, int paren_level,
	bz::u8char lhs, int64_t rhs,
	ctx::parse_context &context
)
{
	static_assert(bz::meta::is_same<bz::u8char, uint32_t>);
	auto const result = lhs - static_cast<uint32_t>(rhs);
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
	else
	{
		return result;
	}
}

bz::optional<bz::u8char> safe_binary_minus(
	lex::src_tokens const &src_tokens, int paren_level,
	bz::u8char lhs, uint64_t rhs,
	ctx::parse_context &context
)
{
	static_assert(bz::meta::is_same<bz::u8char, uint32_t>);
	auto const result = lhs - static_cast<uint32_t>(rhs);
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
	else
	{
		return result;
	}
}


int64_t safe_binary_multiply(
	lex::src_tokens const &src_tokens, int paren_level,
	int64_t lhs, int64_t rhs, uint32_t type_kind,
	ctx::parse_context &context
)
{
	using T = std::tuple<bz::u8string_view, int64_t, int64_t>;
	auto const [type_name, min_value, max_value] =
		type_kind == ast::type_info::int8_  ? T{ "int8",  std::numeric_limits<int8_t> ::min(), std::numeric_limits<int8_t> ::max() } :
		type_kind == ast::type_info::int16_ ? T{ "int16", std::numeric_limits<int16_t>::min(), std::numeric_limits<int16_t>::max() } :
		type_kind == ast::type_info::int32_ ? T{ "int32", std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max() } :
		T{ "int64", std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max() };

	int64_t const result =
		type_kind == ast::type_info::int8_  ? static_cast<int8_t> (static_cast<uint64_t>(lhs) * static_cast<uint64_t>(rhs)) :
		type_kind == ast::type_info::int16_ ? static_cast<int16_t>(static_cast<uint64_t>(lhs) * static_cast<uint64_t>(rhs)) :
		type_kind == ast::type_info::int32_ ? static_cast<int32_t>(static_cast<uint64_t>(lhs) * static_cast<uint64_t>(rhs)) :
		static_cast<int64_t>(static_cast<uint64_t>(lhs) * static_cast<uint64_t>(rhs));

	if (
		paren_level < 2
		&& ((lhs < 0 && rhs < 0 && lhs < max_value / rhs)
		|| (lhs > 0 && rhs > 0 && lhs > max_value / rhs)
		|| (lhs < 0 && rhs > 0 && lhs < min_value / rhs)
		|| (lhs > 0 && rhs < 0 && lhs > min_value / rhs))
	)
	{
		context.report_parenthesis_suppressed_warning(
			2 - paren_level, ctx::warning_kind::int_overflow,
			src_tokens,
			bz::format("overflow in constant expression '{} * {}' with type '{}' results in {}", lhs, rhs, type_name, result)
		);
	}
	return result;
}

uint64_t safe_binary_multiply(
	lex::src_tokens const &src_tokens, int paren_level,
	uint64_t lhs, uint64_t rhs, uint32_t type_kind,
	ctx::parse_context &context
)
{
	using T = std::tuple<bz::u8string_view, uint64_t>;
	auto const [type_name, max_value] =
		type_kind == ast::type_info::uint8_  ? T{ "uint8",  std::numeric_limits<uint8_t> ::max() } :
		type_kind == ast::type_info::uint16_ ? T{ "uint16", std::numeric_limits<uint16_t>::max() } :
		type_kind == ast::type_info::uint32_ ? T{ "uint32", std::numeric_limits<uint32_t>::max() } :
		T{ "uint64", std::numeric_limits<uint64_t>::max() };

	uint64_t const result =
		type_kind == ast::type_info::uint8_  ? static_cast<uint8_t> (lhs * rhs) :
		type_kind == ast::type_info::uint16_ ? static_cast<uint16_t>(lhs * rhs) :
		type_kind == ast::type_info::uint32_ ? static_cast<uint32_t>(lhs * rhs) :
		static_cast<uint64_t>(lhs * rhs);

	if (paren_level < 2 && (lhs > 0 && rhs > 0 && lhs > max_value / rhs))
	{
		context.report_parenthesis_suppressed_warning(
			2 - paren_level, ctx::warning_kind::int_overflow,
			src_tokens,
			bz::format("overflow in constant expression '{} * {}' with type '{}' results in {}", lhs, rhs, type_name, result)
		);
	}
	return result;
}

float32_t safe_binary_multiply(
	lex::src_tokens const &src_tokens, int paren_level,
	float32_t lhs, float32_t rhs,
	ctx::parse_context &context
)
{
	auto const result = lhs * rhs;
	if (
		paren_level < 2
		&& ((!std::isnan(lhs) && !std::isnan(rhs) && std::isnan(result))
		|| (std::isfinite(lhs) && std::isfinite(rhs) && !std::isfinite(result)))
	)
	{
		context.report_parenthesis_suppressed_warning(
			2 - paren_level, ctx::warning_kind::float_overflow,
			src_tokens,
			bz::format("result of floating point arithmetic in constant expression '{} * {}' with type 'float32' is {}", lhs, rhs, result)
		);
	}
	return result;
}

float64_t safe_binary_multiply(
	lex::src_tokens const &src_tokens, int paren_level,
	float64_t lhs, float64_t rhs,
	ctx::parse_context &context
)
{
	auto const result = lhs * rhs;
	if (
		paren_level < 2
		&& ((!std::isnan(lhs) && !std::isnan(rhs) && std::isnan(result))
		|| (std::isfinite(lhs) && std::isfinite(rhs) && !std::isfinite(result)))
	)
	{
		context.report_parenthesis_suppressed_warning(
			2 - paren_level, ctx::warning_kind::float_overflow,
			src_tokens,
			bz::format("result of floating point arithmetic in constant expression '{} * {}' with type 'float64' is {}", lhs, rhs, result)
		);
	}
	return result;
}


bz::optional<int64_t> safe_binary_divide(
	lex::src_tokens const &src_tokens, int paren_level,
	int64_t lhs, int64_t rhs, uint32_t type_kind,
	ctx::parse_context &context
)
{
	using T = std::tuple<bz::u8string_view, int64_t>;
	auto const [type_name, min_value] =
		type_kind == ast::type_info::int8_  ? T{ "int8",  std::numeric_limits<int8_t> ::min() } :
		type_kind == ast::type_info::int16_ ? T{ "int16", std::numeric_limits<int16_t>::min() } :
		type_kind == ast::type_info::int32_ ? T{ "int32", std::numeric_limits<int32_t>::min() } :
		T{ "int64", std::numeric_limits<int64_t>::min() };

	if (rhs == 0)
	{
		if (paren_level < 2)
		{
			context.report_parenthesis_suppressed_warning(
				2 - paren_level, ctx::warning_kind::int_divide_by_zero,
				src_tokens,
				bz::format("dividing by zero in expression '{} / {}' with type '{}'", lhs, rhs, type_name)
			);
		}
		return {};
	}
	else if (lhs == min_value && rhs == -1)
	{
		if (paren_level < 2)
		{
			context.report_parenthesis_suppressed_warning(
				2 - paren_level, ctx::warning_kind::int_overflow,
				src_tokens,
				bz::format("overflow in constant expression '{} / {}' with type '{}' results in {}", lhs, rhs, type_name, min_value)
			);
		}
		return min_value;
	}
	else
	{
		return lhs / rhs;
	}
}

bz::optional<uint64_t> safe_binary_divide(
	lex::src_tokens const &src_tokens, int paren_level,
	uint64_t lhs, uint64_t rhs, uint32_t type_kind,
	ctx::parse_context &context
)
{
	bz::u8string_view const type_name =
		type_kind == ast::type_info::uint8_  ? "uint8"  :
		type_kind == ast::type_info::uint16_ ? "uint16" :
		type_kind == ast::type_info::uint32_ ? "uint32" :
		"uint64";

	if (rhs == 0)
	{
		if (paren_level < 2)
		{
			context.report_parenthesis_suppressed_warning(
				2 - paren_level, ctx::warning_kind::int_divide_by_zero,
				src_tokens,
				bz::format("dividing by zero in expression '{} / {}' with type '{}'", lhs, rhs, type_name)
			);
		}
		return {};
	}
	else
	{
		return lhs / rhs;
	}
}

float32_t safe_binary_divide(
	lex::src_tokens const &src_tokens, int paren_level,
	float32_t lhs, float32_t rhs,
	ctx::parse_context &context
)
{
	auto const result = lhs / rhs;
	if (paren_level < 2 && rhs == 0)
	{
		context.report_parenthesis_suppressed_warning(
			2 - paren_level, ctx::warning_kind::float_divide_by_zero,
			src_tokens,
			bz::format("dividing by zero in expression '{} / {}' with type 'float32' results in {}", lhs, rhs, result)
		);
	}
	else if (
		paren_level < 2
		&& ((!std::isnan(lhs) && !std::isnan(rhs) && std::isnan(result))
		|| (std::isfinite(lhs) && std::isfinite(rhs) && !std::isfinite(result)))
	)
	{
		context.report_parenthesis_suppressed_warning(
			2 - paren_level, ctx::warning_kind::float_overflow,
			src_tokens,
			bz::format("result of floating point arithmetic in constant expression '{} / {}' with type 'float32' is {}", lhs, rhs, result)
		);
	}
	return result;
}

float64_t safe_binary_divide(
	lex::src_tokens const &src_tokens, int paren_level,
	float64_t lhs, float64_t rhs,
	ctx::parse_context &context
)
{
	auto const result = lhs / rhs;
	if (paren_level < 2 && rhs == 0)
	{
		context.report_parenthesis_suppressed_warning(
			2 - paren_level, ctx::warning_kind::float_divide_by_zero,
			src_tokens,
			bz::format("dividing by zero in expression '{} / {}' with type 'float64' results in {}", lhs, rhs, result)
		);
	}
	else if (
		paren_level < 2
		&& ((!std::isnan(lhs) && !std::isnan(rhs) && std::isnan(result))
		|| (std::isfinite(lhs) && std::isfinite(rhs) && !std::isfinite(result)))
	)
	{
		context.report_parenthesis_suppressed_warning(
			2 - paren_level, ctx::warning_kind::float_overflow,
			src_tokens,
			bz::format("result of floating point arithmetic in constant expression '{} / {}' with type 'float64' is {}", lhs, rhs, result)
		);
	}
	return result;
}


bz::optional<int64_t> safe_binary_modulo(
	lex::src_tokens const &src_tokens, int paren_level,
	int64_t lhs, int64_t rhs, uint32_t type_kind,
	ctx::parse_context &context
)
{
	bz::u8string_view const type_name =
		type_kind == ast::type_info::int8_  ? "int8"  :
		type_kind == ast::type_info::int16_ ? "int16" :
		type_kind == ast::type_info::int32_ ? "int32" :
		"int64";

	if (rhs == 0)
	{
		if (paren_level < 2)
		{
			context.report_parenthesis_suppressed_warning(
				2 - paren_level, ctx::warning_kind::int_divide_by_zero,
				src_tokens,
				bz::format("modulo by zero in expression '{} % {}' with type '{}'", lhs, rhs, type_name)
			);
		}
		return {};
	}
	else
	{
		return lhs % rhs;
	}
}

bz::optional<uint64_t> safe_binary_modulo(
	lex::src_tokens const &src_tokens, int paren_level,
	uint64_t lhs, uint64_t rhs, uint32_t type_kind,
	ctx::parse_context &context
)
{
	bz::u8string_view const type_name =
		type_kind == ast::type_info::uint8_  ? "uint8"  :
		type_kind == ast::type_info::uint16_ ? "uint16" :
		type_kind == ast::type_info::uint32_ ? "uint32" :
		"uint64";

	if (rhs == 0)
	{
		if (paren_level < 2)
		{
			context.report_parenthesis_suppressed_warning(
				2 - paren_level, ctx::warning_kind::int_divide_by_zero,
				src_tokens,
				bz::format("modulo by zero in expression '{} % {}' with type '{}'", lhs, rhs, type_name)
			);
		}
		return {};
	}
	else
	{
		return lhs % rhs;
	}
}


bool safe_binary_equals(
	lex::src_tokens const &src_tokens, int paren_level,
	float32_t lhs, float32_t rhs,
	ctx::parse_context &context
)
{
	if (paren_level < 2)
	{
		if (std::isnan(lhs) && std::isnan(rhs))
		{
			context.report_parenthesis_suppressed_warning(
				2 - paren_level, ctx::warning_kind::nan_compare,
				src_tokens,
				bz::format("comparing nans in expression '{} == {}' with type 'float32' evaluates to false", lhs, rhs)
			);
		}
		else if (std::isnan(lhs) || std::isnan(rhs))
		{
			context.report_parenthesis_suppressed_warning(
				2 - paren_level, ctx::warning_kind::nan_compare,
				src_tokens,
				bz::format("comparing against nan in expression '{} == {}' with type 'float32' evaluates to false", lhs, rhs)
			);
		}
	}
	return lhs == rhs;
}

bool safe_binary_equals(
	lex::src_tokens const &src_tokens, int paren_level,
	float64_t lhs, float64_t rhs,
	ctx::parse_context &context
)
{
	if (paren_level < 2)
	{
		if (std::isnan(lhs) && std::isnan(rhs))
		{
			context.report_parenthesis_suppressed_warning(
				2 - paren_level, ctx::warning_kind::nan_compare,
				src_tokens,
				bz::format("comparing nans in expression '{} == {}' with type 'float64' evaluates to false", lhs, rhs)
			);
		}
		else if (std::isnan(lhs) || std::isnan(rhs))
		{
			context.report_parenthesis_suppressed_warning(
				2 - paren_level, ctx::warning_kind::nan_compare,
				src_tokens,
				bz::format("comparing against nan in expression '{} == {}' with type 'float64' evaluates to false", lhs, rhs)
			);
		}
	}
	return lhs == rhs;
}


bz::optional<uint64_t> safe_binary_bit_left_shift(
	lex::src_tokens const &src_tokens, int paren_level,
	uint64_t lhs, uint64_t rhs, uint32_t lhs_type_kind,
	ctx::parse_context &context
)
{
	using T = std::tuple<bz::u8string_view, uint64_t>;
	auto const [type_name, lhs_width] =
		lhs_type_kind == ast::type_info::uint8_  ? T{ "uint8",  8  } :
		lhs_type_kind == ast::type_info::uint16_ ? T{ "uint16", 16 } :
		lhs_type_kind == ast::type_info::uint32_ ? T{ "uint32", 32 } :
		T{ "uint64", 64 };

	if (rhs >= lhs_width)
	{
		if (paren_level < 2)
		{
			context.report_parenthesis_suppressed_warning(
				2 - paren_level, ctx::warning_kind::int_overflow,
				src_tokens,
				bz::format("shift amount of {} is too big for type '{}', it must be less than {}", rhs, type_name, lhs_width)
			);
		}
		return {};
	}
	else switch (lhs_type_kind)
	{
	case ast::type_info::uint8_:
		return static_cast<uint8_t>(lhs << rhs);
	case ast::type_info::uint16_:
		return static_cast<uint16_t>(lhs << rhs);
	case ast::type_info::uint32_:
		return static_cast<uint32_t>(lhs << rhs);
	case ast::type_info::uint64_:
		return static_cast<uint64_t>(lhs << rhs);
	default:
		bz_unreachable;
	}
}

bz::optional<uint64_t> safe_binary_bit_right_shift(
	lex::src_tokens const &src_tokens, int paren_level,
	uint64_t lhs, uint64_t rhs, uint32_t lhs_type_kind,
	ctx::parse_context &context
)
{
	using T = std::tuple<bz::u8string_view, uint64_t>;
	auto const [type_name, lhs_width] =
		lhs_type_kind == ast::type_info::uint8_  ? T{ "uint8",  8  } :
		lhs_type_kind == ast::type_info::uint16_ ? T{ "uint16", 16 } :
		lhs_type_kind == ast::type_info::uint32_ ? T{ "uint32", 32 } :
		T{ "uint64", 64 };

	if (rhs >= lhs_width)
	{
		if (paren_level < 2)
		{
			context.report_parenthesis_suppressed_warning(
				2 - paren_level, ctx::warning_kind::int_overflow,
				src_tokens,
				bz::format("shift amount of {} is too big for type '{}', it must be less than {}", rhs, type_name, lhs_width)
			);
		}
		return {};
	}
	else switch (lhs_type_kind)
	{
	case ast::type_info::uint8_:
		return static_cast<uint8_t>(lhs >> rhs);
	case ast::type_info::uint16_:
		return static_cast<uint16_t>(lhs >> rhs);
	case ast::type_info::uint32_:
		return static_cast<uint32_t>(lhs >> rhs);
	case ast::type_info::uint64_:
		return static_cast<uint64_t>(lhs >> rhs);
	default:
		bz_unreachable;
	}
}


bz::optional<uint64_t> safe_binary_bit_left_shift(
	lex::src_tokens const &src_tokens, int paren_level,
	uint64_t lhs, int64_t rhs, uint32_t lhs_type_kind,
	ctx::parse_context &context
)
{
	using T = std::tuple<bz::u8string_view, int64_t>;
	auto const [type_name, lhs_width] =
		lhs_type_kind == ast::type_info::uint8_  ? T{ "uint8",  8  } :
		lhs_type_kind == ast::type_info::uint16_ ? T{ "uint16", 16 } :
		lhs_type_kind == ast::type_info::uint32_ ? T{ "uint32", 32 } :
		T{ "uint64", 64 };

	if (rhs >= lhs_width)
	{
		if (paren_level < 2)
		{
			context.report_parenthesis_suppressed_warning(
				2 - paren_level, ctx::warning_kind::int_overflow,
				src_tokens,
				bz::format("shift amount of {} is too big for type '{}', it must be less than {}", rhs, type_name, lhs_width)
			);
		}
		return {};
	}
	else if (rhs < 0)
	{
		if (paren_level < 2)
		{
			context.report_parenthesis_suppressed_warning(
				2 - paren_level, ctx::warning_kind::int_overflow,
				src_tokens,
				bz::format("negative shift amount of {} for type '{}'", rhs, type_name)
			);
		}
		return {};
	}
	else switch (lhs_type_kind)
	{
	case ast::type_info::uint8_:
		return static_cast<uint8_t>(lhs << rhs);
	case ast::type_info::uint16_:
		return static_cast<uint16_t>(lhs << rhs);
	case ast::type_info::uint32_:
		return static_cast<uint32_t>(lhs << rhs);
	case ast::type_info::uint64_:
		return static_cast<uint64_t>(lhs << rhs);
	default:
		bz_unreachable;
	}
}

bz::optional<uint64_t> safe_binary_bit_right_shift(
	lex::src_tokens const &src_tokens, int paren_level,
	uint64_t lhs, int64_t rhs, uint32_t lhs_type_kind,
	ctx::parse_context &context
)
{
	using T = std::tuple<bz::u8string_view, int64_t>;
	auto const [type_name, lhs_width] =
		lhs_type_kind == ast::type_info::uint8_  ? T{ "uint8",  8  } :
		lhs_type_kind == ast::type_info::uint16_ ? T{ "uint16", 16 } :
		lhs_type_kind == ast::type_info::uint32_ ? T{ "uint32", 32 } :
		T{ "uint64", 64 };

	if (rhs >= lhs_width)
	{
		if (paren_level < 2)
		{
			context.report_parenthesis_suppressed_warning(
				2 - paren_level, ctx::warning_kind::int_overflow,
				src_tokens,
				bz::format("shift amount of {} is too big for type '{}', it must be less than {}", rhs, type_name, lhs_width)
			);
		}
		return {};
	}
	else if (rhs < 0)
	{
		if (paren_level < 2)
		{
			context.report_parenthesis_suppressed_warning(
				2 - paren_level, ctx::warning_kind::int_overflow,
				src_tokens,
				bz::format("negative shift amount of {} for type '{}'", rhs, type_name)
			);
		}
		return {};
	}
	else switch (lhs_type_kind)
	{
	case ast::type_info::uint8_:
		return static_cast<uint8_t>(lhs >> rhs);
	case ast::type_info::uint16_:
		return static_cast<uint16_t>(lhs >> rhs);
	case ast::type_info::uint32_:
		return static_cast<uint32_t>(lhs >> rhs);
	case ast::type_info::uint64_:
		return static_cast<uint64_t>(lhs >> rhs);
	default:
		bz_unreachable;
	}
}

} // namespace parse
