#include "safe_operations.h"
#include "overflow_operations.h"

namespace resolve
{

enum class op
{
	add,
	sub,
	mul,
	div,
};

template<op op_kind, typename Result>
struct overflow_operations_adapter_t
{
	static inline constexpr auto signed_func = []() {
		if constexpr (op_kind == op::add)
		{
			return static_cast<operation_result_t<Result> (*)(int64_t, int64_t)>(&add_overflow<Result>);
		}
		else if constexpr (op_kind == op::sub)
		{
			return static_cast<operation_result_t<Result> (*)(int64_t, int64_t)>(&sub_overflow<Result>);
		}
		else if constexpr (op_kind == op::mul)
		{
			return static_cast<operation_result_t<Result> (*)(int64_t, int64_t)>(&mul_overflow<Result>);
		}
		else if constexpr (op_kind == op::div)
		{
			return static_cast<operation_result_t<Result> (*)(int64_t, int64_t)>(&div_overflow<Result>);
		}
		else
		{
			return 0;
		}
	}();

	static inline constexpr auto unsigned_func = []() {
		if constexpr (op_kind == op::add)
		{
			return static_cast<operation_result_t<Result> (*)(uint64_t, uint64_t)>(&add_overflow<Result>);
		}
		else if constexpr (op_kind == op::sub)
		{
			return static_cast<operation_result_t<Result> (*)(uint64_t, uint64_t)>(&sub_overflow<Result>);
		}
		else if constexpr (op_kind == op::mul)
		{
			return static_cast<operation_result_t<Result> (*)(uint64_t, uint64_t)>(&mul_overflow<Result>);
		}
		else if constexpr (op_kind == op::div)
		{
			return static_cast<operation_result_t<Result> (*)(uint64_t, uint64_t)>(&div_overflow<Result>);
		}
		else
		{
			return 0;
		}
	}();
};

template<typename Result>
struct overflow_operation_result_t
{
	bz::u8string_view type_name;
	Result result;
	bool overflowed;
};

template<op op_kind>
static overflow_operation_result_t<int64_t> get_overflow_operation_result(int64_t lhs, int64_t rhs, uint32_t type_kind)
{
	switch (type_kind)
	{
	case ast::type_info::i8_:
	{
		auto const [result, overflowed] = overflow_operations_adapter_t<op_kind, int8_t>::signed_func(lhs, rhs);
		return { "i8", result, overflowed };
	}
	case ast::type_info::i16_:
	{
		auto const [result, overflowed] = overflow_operations_adapter_t<op_kind, int16_t>::signed_func(lhs, rhs);
		return { "i16", result, overflowed };
	}
	case ast::type_info::i32_:
	{
		auto const [result, overflowed] = overflow_operations_adapter_t<op_kind, int32_t>::signed_func(lhs, rhs);
		return { "i32", result, overflowed };
	}
	case ast::type_info::i64_:
	{
		auto const [result, overflowed] = overflow_operations_adapter_t<op_kind, int64_t>::signed_func(lhs, rhs);
		return { "i64", result, overflowed };
	}
	}
	bz_unreachable;
	return { "", 0, false };
}

template<op op_kind>
static overflow_operation_result_t<uint64_t> get_overflow_operation_result(uint64_t lhs, uint64_t rhs, uint32_t type_kind)
{
	switch (type_kind)
	{
	case ast::type_info::u8_:
	{
		auto const [result, overflowed] = overflow_operations_adapter_t<op_kind, uint8_t>::unsigned_func(lhs, rhs);
		return { "u8", result, overflowed };
	}
	case ast::type_info::u16_:
	{
		auto const [result, overflowed] = overflow_operations_adapter_t<op_kind, uint16_t>::unsigned_func(lhs, rhs);
		return { "u16", result, overflowed };
	}
	case ast::type_info::u32_:
	{
		auto const [result, overflowed] = overflow_operations_adapter_t<op_kind, uint32_t>::unsigned_func(lhs, rhs);
		return { "u32", result, overflowed };
	}
	case ast::type_info::u64_:
	{
		auto const [result, overflowed] = overflow_operations_adapter_t<op_kind, uint64_t>::unsigned_func(lhs, rhs);
		return { "u64", result, overflowed };
	}
	}
	bz_unreachable;
	return { "", 0, false };
}

int64_t safe_unary_minus(
	lex::src_tokens const &src_tokens, int paren_level,
	int64_t value, uint32_t type_kind,
	ctx::parse_context &context
)
{
	using T = std::pair<bz::u8string_view, int64_t>;
	auto const [type_name, min_value] =
		type_kind == ast::type_info::i8_  ? T{ "i8",  std::numeric_limits<int8_t> ::min() } :
		type_kind == ast::type_info::i16_ ? T{ "i16", std::numeric_limits<int16_t>::min() } :
		type_kind == ast::type_info::i32_ ? T{ "i32", std::numeric_limits<int32_t>::min() } :
		T{ "i64", std::numeric_limits<int64_t>::min() };
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
	auto const [type_name, result, overflowed] = get_overflow_operation_result<op::add>(lhs, rhs, type_kind);

	if (paren_level < 2 && overflowed)
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
	auto const [type_name, result, overflowed] = get_overflow_operation_result<op::add>(lhs, rhs, type_kind);

	if (paren_level < 2 && overflowed)
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
	auto const [type_name, result, overflowed] = get_overflow_operation_result<op::sub>(lhs, rhs, type_kind);

	if (paren_level < 2 && overflowed)
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
	auto const [type_name, result, overflowed] = get_overflow_operation_result<op::sub>(lhs, rhs, type_kind);

	if (paren_level < 2 && overflowed)
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
	auto const [type_name, result, overflowed] = get_overflow_operation_result<op::mul>(lhs, rhs, type_kind);

	if (paren_level < 2 && overflowed)
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
	auto const [type_name, result, overflowed] = get_overflow_operation_result<op::mul>(lhs, rhs, type_kind);

	if (paren_level < 2 && overflowed)
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
	auto const [type_name, result, overflowed] = get_overflow_operation_result<op::div>(lhs, rhs, type_kind);

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
	else if (paren_level < 2 && overflowed)
	{
		context.report_parenthesis_suppressed_warning(
			2 - paren_level, ctx::warning_kind::int_overflow,
			src_tokens,
			bz::format("overflow in constant expression '{} / {}' with type '{}' results in {}", lhs, rhs, type_name, result)
		);
	}
	return result;
}

bz::optional<uint64_t> safe_binary_divide(
	lex::src_tokens const &src_tokens, int paren_level,
	uint64_t lhs, uint64_t rhs, uint32_t type_kind,
	ctx::parse_context &context
)
{
	auto const [type_name, result, overflowed] = get_overflow_operation_result<op::div>(lhs, rhs, type_kind);

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
	bz_assert(!overflowed);
	return result;
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
		type_kind == ast::type_info::i8_  ? "i8"  :
		type_kind == ast::type_info::i16_ ? "i16" :
		type_kind == ast::type_info::i32_ ? "i32" :
		"i64";

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
		type_kind == ast::type_info::u8_  ? "u8"  :
		type_kind == ast::type_info::u16_ ? "u16" :
		type_kind == ast::type_info::u32_ ? "u32" :
		"u64";

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
	using T = std::pair<bz::u8string_view, uint64_t>;
	auto const [type_name, lhs_width] =
		lhs_type_kind == ast::type_info::u8_  ? T{ "u8",  8  } :
		lhs_type_kind == ast::type_info::u16_ ? T{ "u16", 16 } :
		lhs_type_kind == ast::type_info::u32_ ? T{ "u32", 32 } :
		T{ "u64", 64 };

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
	case ast::type_info::u8_:
		return static_cast<uint8_t>(lhs << rhs);
	case ast::type_info::u16_:
		return static_cast<uint16_t>(lhs << rhs);
	case ast::type_info::u32_:
		return static_cast<uint32_t>(lhs << rhs);
	case ast::type_info::u64_:
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
	using T = std::pair<bz::u8string_view, uint64_t>;
	auto const [type_name, lhs_width] =
		lhs_type_kind == ast::type_info::u8_  ? T{ "u8",  8  } :
		lhs_type_kind == ast::type_info::u16_ ? T{ "u16", 16 } :
		lhs_type_kind == ast::type_info::u32_ ? T{ "u32", 32 } :
		T{ "u64", 64 };

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
	case ast::type_info::u8_:
		return static_cast<uint8_t>(lhs >> rhs);
	case ast::type_info::u16_:
		return static_cast<uint16_t>(lhs >> rhs);
	case ast::type_info::u32_:
		return static_cast<uint32_t>(lhs >> rhs);
	case ast::type_info::u64_:
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
	using T = std::pair<bz::u8string_view, int64_t>;
	auto const [type_name, lhs_width] =
		lhs_type_kind == ast::type_info::u8_  ? T{ "u8",  8  } :
		lhs_type_kind == ast::type_info::u16_ ? T{ "u16", 16 } :
		lhs_type_kind == ast::type_info::u32_ ? T{ "u32", 32 } :
		T{ "u64", 64 };

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
	case ast::type_info::u8_:
		return static_cast<uint8_t>(lhs << rhs);
	case ast::type_info::u16_:
		return static_cast<uint16_t>(lhs << rhs);
	case ast::type_info::u32_:
		return static_cast<uint32_t>(lhs << rhs);
	case ast::type_info::u64_:
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
	using T = std::pair<bz::u8string_view, int64_t>;
	auto const [type_name, lhs_width] =
		lhs_type_kind == ast::type_info::u8_  ? T{ "u8",  8  } :
		lhs_type_kind == ast::type_info::u16_ ? T{ "u16", 16 } :
		lhs_type_kind == ast::type_info::u32_ ? T{ "u32", 32 } :
		T{ "u64", 64 };

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
	case ast::type_info::u8_:
		return static_cast<uint8_t>(lhs >> rhs);
	case ast::type_info::u16_:
		return static_cast<uint16_t>(lhs >> rhs);
	case ast::type_info::u32_:
		return static_cast<uint32_t>(lhs >> rhs);
	case ast::type_info::u64_:
		return static_cast<uint64_t>(lhs >> rhs);
	default:
		bz_unreachable;
	}
}

} // namespace resolve
