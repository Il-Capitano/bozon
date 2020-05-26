#include "safe_operations.h"

namespace ctx
{

template<typename T, typename U>
static bool is_in_range(U val) noexcept
{
	return val >= std::numeric_limits<T>::min() && val <= std::numeric_limits<T>::max();
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
	default: bz_assert(false); return "";
	}
}

// int + int
int64_t safe_add(
	int64_t a, int64_t b, uint32_t type_kind,
	lex::src_tokens src_tokens, parse_context &context
)
{
	auto const result = a + b;
#define x(type)                                                                        \
case ast::type_info::type##_:                                                          \
    bz_assert(is_in_range<type##_t>(a));                                               \
    bz_assert(is_in_range<type##_t>(b));                                               \
    if (!is_in_range<type##_t>(result))                                                \
    {                                                                                  \
        context.report_warning(                                                        \
            src_tokens,                                                                \
            bz::format(                                                                \
                "overflow in constant expression with type '" #type "' results in {}", \
                static_cast<type##_t>(result)                                          \
            )                                                                          \
        );                                                                             \
    }                                                                                  \
    return static_cast<int64_t>(static_cast<type##_t>(result));

	switch (type_kind)
	{
	x(int8)
	x(int16)
	x(int32)
	case ast::type_info::int64_:
		if (
			(a > 0 && b > 0 && a > std::numeric_limits<int64_t>::max() - b)
			|| (a < 0 && b < 0 && a < std::numeric_limits<int64_t>::min() - b)
		)
		{
			context.report_warning(
				src_tokens,
				bz::format(
					"overflow in constant expression with type 'int64' results in {}",
					result
				)
			);
		}
		return result;
	default:
		bz_assert(false);
		return 0;
	}
#undef x
}

uint64_t safe_add(
	uint64_t a, uint64_t b, uint32_t type_kind,
	lex::src_tokens src_tokens, parse_context &context
)
{
	auto const result = a + b;
#define x(type)                                                                        \
case ast::type_info::type##_:                                                          \
    bz_assert(is_in_range<type##_t>(a));                                               \
    bz_assert(is_in_range<type##_t>(b));                                               \
    if (!is_in_range<type##_t>(result))                                                \
    {                                                                                  \
        context.report_warning(                                                        \
            src_tokens,                                                                \
            bz::format(                                                                \
                "overflow in constant expression with type '" #type "' results in {}", \
                static_cast<type##_t>(result)                                          \
            )                                                                          \
        );                                                                             \
    }                                                                                  \
    return static_cast<uint64_t>(static_cast<type##_t>(result));

	switch (type_kind)
	{
	x(uint8)
	x(uint16)
	x(uint32)
	case ast::type_info::uint64_:
		if (a > std::numeric_limits<uint64_t>::max() - b)
		{
			context.report_warning(
				src_tokens,
				bz::format(
					"overflow in constant expression with type 'uint64' results in {}",
					result
				)
			);
		}
		return result;
	default:
		bz_assert(false);
		return 0;
	}
#undef x
}

// char + int
bz::u8char safe_add(
	bz::u8char a, int64_t b, uint32_t type_kind,
	lex::src_tokens src_tokens, parse_context &context,
	bool reversed
)
{
	auto const result = a + static_cast<uint32_t>(b);
	if (
		b > static_cast<int64_t>(std::numeric_limits<bz::u8char>::max() - a)
		|| b < -static_cast<int64_t>(a)
	)
	{
		auto const result_str = bz::format("0x{:x}", result);
		context.report_warning(
			src_tokens,
			bz::format(
				reversed
				? "overflow in constant expression with types '{}' and 'char' results in {}"
				: "overflow in constant expression with types 'char' and '{}' results in {}",
				get_type_name_from_kind(type_kind), result_str
			)
		);
	}
	if (result > bz::max_unicode_value)
	{
		context.report_error(
			src_tokens,
			bz::format(
				"the result of 0x{:x} in a constant expression is not a valid character, maximum value is 0x10ffff",
				result
			)
		);
	}
	return result;
}

bz::u8char safe_add(
	bz::u8char a, uint64_t b, uint32_t type_kind,
	lex::src_tokens src_tokens, parse_context &context,
	bool reversed
)
{
	auto const result = a + static_cast<uint32_t>(b);
	if (b > static_cast<uint64_t>(std::numeric_limits<bz::u8char>::max() - a))
	{
		auto const result_str = bz::format("0x{:x}", result);
		context.report_warning(
			src_tokens,
			bz::format(
				reversed
				? "overflow in constant expression with types '{}' and 'char' results in {}"
				: "overflow in constant expression with types 'char' and '{}' results in {}",
				get_type_name_from_kind(type_kind), result_str
			)
		);
	}
	if (result > bz::max_unicode_value)
	{
		context.report_error(
			src_tokens,
			bz::format(
				"the result of 0x{:x} in a constant expression is not a valid character (maximum value is 0x10ffff)",
				result
			)
		);
	}
	return result;
}

// float32 + float32
float32_t safe_add(
	float32_t a, float32_t b,
	lex::src_tokens src_tokens, parse_context &context
)
{
	auto const result = a + b;
	if (std::isfinite(a) && std::isfinite(b) && !std::isfinite(result))
	{
		context.report_parenthesis_suppressed_warning(
			src_tokens,
			bz::format("result of floating point arithmetic in constant expression is {}", result)
		);
	}
	return result;
}

// float64 + float64
float64_t safe_add(
	float64_t a, float64_t b,
	lex::src_tokens src_tokens, parse_context &context
)
{
	auto const result = a + b;
	if (std::isfinite(a) && std::isfinite(b) && !std::isfinite(result))
	{
		context.report_parenthesis_suppressed_warning(
			src_tokens,
			bz::format("result of floating point arithmetic in constant expression is {}", result)
		);
	}
	return result;
}


// int - int
int64_t safe_subtract(
	int64_t a, int64_t b, uint32_t type_kind,
	lex::src_tokens src_tokens, parse_context &context
)
{
	auto const result = a - b;
#define x(type)                                                                        \
case ast::type_info::type##_:                                                          \
    bz_assert(is_in_range<type##_t>(a));                                               \
    bz_assert(is_in_range<type##_t>(b));                                               \
    if (!is_in_range<type##_t>(result))                                                \
    {                                                                                  \
        context.report_warning(                                                        \
            src_tokens,                                                                \
            bz::format(                                                                \
                "overflow in constant expression with type '" #type "' results in {}", \
                static_cast<type##_t>(result)                                          \
            )                                                                          \
        );                                                                             \
    }                                                                                  \
    return static_cast<int64_t>(static_cast<type##_t>(result));

	switch (type_kind)
	{
	x(int8)
	x(int16)
	x(int32)
	case ast::type_info::int64_:
		if (
			(a < 0 && b > 0 && a < std::numeric_limits<int64_t>::min() + b)
			|| (a >= 0 && b < 0 && a > std::numeric_limits<int64_t>::max() + b)
		)
		{
			context.report_warning(
				src_tokens,
				bz::format(
					"overflow in constant expression with type 'int64' results in {}",
					result
				)
			);
		}
		return result;
	default:
		bz_assert(false);
		return 0;
	}
#undef x
}

uint64_t safe_subtract(
	uint64_t a, uint64_t b, uint32_t type_kind,
	lex::src_tokens src_tokens, parse_context &context
)
{
	auto const result = a - b;
#define x(type)                                                                        \
case ast::type_info::type##_:                                                          \
    bz_assert(is_in_range<type##_t>(a));                                               \
    bz_assert(is_in_range<type##_t>(b));                                               \
    if (b > a)                                                                         \
    {                                                                                  \
        context.report_warning(                                                        \
            src_tokens,                                                                \
            bz::format(                                                                \
                "overflow in constant expression with type '" #type "' results in {}", \
                static_cast<type##_t>(result)                                          \
            )                                                                          \
        );                                                                             \
    }                                                                                  \
    return static_cast<uint64_t>(static_cast<type##_t>(result));

	switch (type_kind)
	{
	x(uint8)
	x(uint16)
	x(uint32)
	x(uint64)
	default:
		bz_assert(false);
		return 0;
	}
#undef x
}

// char - int
bz::u8char safe_subtract(
	bz::u8char a, int64_t b, uint32_t type_kind,
	lex::src_tokens src_tokens, parse_context &context
)
{
	auto const result = a - static_cast<uint32_t>(b);
	if (
		(b > 0 && b > static_cast<int64_t>(a))
		|| (b < 0 && static_cast<int64_t>(a) > static_cast<int64_t>(std::numeric_limits<bz::u8char>::max()) + b)
	)
	{
		auto const result_str = bz::format("0x{:x}", result);
		context.report_warning(
			src_tokens,
			bz::format(
				"overflow in constant expression with types 'char' and '{}' results in {}",
				get_type_name_from_kind(type_kind), result_str
			)
		);
	}
	if (result > bz::max_unicode_value)
	{
		context.report_error(
			src_tokens,
			bz::format(
				"the result of 0x{:x} in a constant expression is not a valid character (maximum value is 0x10ffff)",
				result
			)
		);
	}
	return result;
}

bz::u8char safe_subtract(
	bz::u8char a, uint64_t b, uint32_t type_kind,
	lex::src_tokens src_tokens, parse_context &context
)
{
	auto const result = a - static_cast<uint32_t>(b);
	if (b > static_cast<uint64_t>(a))
	{
		auto const result_str = bz::format("0x{:x}", result);
		context.report_warning(
			src_tokens,
			bz::format(
				"overflow in constant expression with types 'char' and '{}' results in {}",
				get_type_name_from_kind(type_kind), result_str
			)
		);
	}
	if (result > bz::max_unicode_value)
	{
		context.report_error(
			src_tokens,
			bz::format(
				"the result of 0x{:x} in a constant expression is not a valid character (maximum value is 0x10ffff)",
				result
			)
		);
	}
	return result;
}

int32_t safe_subtract(
	bz::u8char a, bz::u8char b,
	lex::src_tokens, parse_context &
)
{
	// this shouldn't cause an overflow ever, as the max value for a character is 21 bytes long
	// if there would an overflow, there would have been errors previously
	return static_cast<int32_t>(a) - static_cast<int32_t>(b);
}

// float32 - float32
float32_t safe_subtract(
	float32_t a, float32_t b,
	lex::src_tokens src_tokens, parse_context &context
)
{
	auto const result = a - b;
	if (std::isfinite(a) && std::isfinite(b) && !std::isfinite(result))
	{
		context.report_parenthesis_suppressed_warning(
			src_tokens,
			bz::format("result of floating point arithmetic in constant expression is {}", result)
		);
	}
	return result;
}

// float64 - float64
float64_t safe_subtract(
	float64_t a, float64_t b,
	lex::src_tokens src_tokens, parse_context &context
)
{
	auto const result = a - b;
	if (std::isfinite(a) && std::isfinite(b) && !std::isfinite(result))
	{
		context.report_parenthesis_suppressed_warning(
			src_tokens,
			bz::format("result of floating point arithmetic in constant expression is {}", result)
		);
	}
	return result;
}


// int * int
int64_t safe_multiply(
	int64_t a, int64_t b, uint32_t type_kind,
	lex::src_tokens src_tokens, parse_context &context
)
{
	auto const result = a * b;
#define x(type)                                                                        \
case ast::type_info::type##_:                                                          \
    bz_assert(is_in_range<type##_t>(a));                                               \
    bz_assert(is_in_range<type##_t>(b));                                               \
    if (!is_in_range<type##_t>(result))                                                \
    {                                                                                  \
        context.report_warning(                                                        \
            src_tokens,                                                                \
            bz::format(                                                                \
                "overflow in constant expression with type '" #type "' results in {}", \
                static_cast<type##_t>(result)                                          \
            )                                                                          \
        );                                                                             \
    }                                                                                  \
    return static_cast<int64_t>(static_cast<type##_t>(result));

	switch (type_kind)
	{
	x(int8)
	x(int16)
	x(int32)
	case ast::type_info::int64_:
		if (
			(a < 0 && b < 0 && a < std::numeric_limits<int64_t>::max() / b)
			|| (a > 0 && b > 0 && a > std::numeric_limits<int64_t>::max() / b)
			|| (a < 0 && b > 0 && a < std::numeric_limits<int64_t>::min() / b)
			|| (a > 0 && b < 0 && a > std::numeric_limits<int64_t>::min() / b)
		)
		{
			context.report_warning(
				src_tokens,
				bz::format("overflow in constant expression with type 'int64' results in {}", result)
			);
		}
		return result;
	default:
		bz_assert(false);
		return 0;
	}
#undef x
}

uint64_t safe_multiply(
	uint64_t a, uint64_t b, uint32_t type_kind,
	lex::src_tokens src_tokens, parse_context &context
)
{
	auto const result = a * b;
#define x(type)                                                                        \
case ast::type_info::type##_:                                                          \
    bz_assert(is_in_range<type##_t>(a));                                               \
    bz_assert(is_in_range<type##_t>(b));                                               \
    if (!is_in_range<type##_t>(result))                                                \
    {                                                                                  \
        context.report_warning(                                                        \
            src_tokens,                                                                \
            bz::format(                                                                \
                "overflow in constant expression with type '" #type "' results in {}", \
                static_cast<type##_t>(result)                                          \
            )                                                                          \
        );                                                                             \
    }                                                                                  \
    return static_cast<uint64_t>(static_cast<type##_t>(result));

	switch (type_kind)
	{
	x(uint8)
	x(uint16)
	x(uint32)
	case ast::type_info::uint64_:
		if (b != 0 && a > std::numeric_limits<int64_t>::max() / b)
		{
			context.report_warning(
				src_tokens,
				bz::format("overflow in constant expression with type 'uint64' results in {}", result)
			);
		}
		return result;
	default:
		bz_assert(false);
		return 0;
	}
#undef x
}

// float32 * float32
float32_t safe_multiply(
	float32_t a, float32_t b,
	lex::src_tokens src_tokens, parse_context &context
)
{
	auto const result = a * b;
	if (std::isfinite(a) && std::isfinite(b) && !std::isfinite(result))
	{
		context.report_parenthesis_suppressed_warning(
			src_tokens,
			bz::format("result of floating point arithmetic in constant expression is {}", result)
		);
	}
	return result;
}

// float64 * float64
float64_t safe_multiply(
	float64_t a, float64_t b,
	lex::src_tokens src_tokens, parse_context &context
)
{
	auto const result = a * b;
	if (std::isfinite(a) && std::isfinite(b) && !std::isfinite(result))
	{
		context.report_parenthesis_suppressed_warning(
			src_tokens,
			bz::format("result of floating point arithmetic in constant expression is {}", result)
		);
	}
	return result;
}


// int / int
int64_t safe_divide(
	int64_t a, int64_t b, uint32_t,
	lex::src_tokens src_tokens, parse_context &context
)
{
	if (b == 0)
	{
		context.report_error(
			src_tokens,
			"dividing by zero in constant expression"
		);
		return 0;
	}

	return a / b;
}

uint64_t safe_divide(
	uint64_t a, uint64_t b, uint32_t,
	lex::src_tokens src_tokens, parse_context &context
)
{
	if (b == 0)
	{
		context.report_error(
			src_tokens,
			"dividing by zero in constant expression"
		);
		return 0;
	}

	return a / b;
}

// float32 / float32
float32_t safe_divide(
	float32_t a, float32_t b,
	lex::src_tokens src_tokens, parse_context &context
)
{
	auto const result = a / b;
	if (std::isfinite(a) && std::isfinite(b) && !std::isfinite(result))
	{
		context.report_parenthesis_suppressed_warning(
			src_tokens,
			bz::format("result of floating point arithmetic in constant expression is {}", result)
		);
	}
	return result;
}

// float64 / float64
float64_t safe_divide(
	float64_t a, float64_t b,
	lex::src_tokens src_tokens, parse_context &context
)
{
	auto const result = a / b;
	if (std::isfinite(a) && std::isfinite(b) && !std::isfinite(result))
	{
		context.report_parenthesis_suppressed_warning(
			src_tokens,
			bz::format("result of floating point arithmetic in constant expression is {}", result)
		);
	}
	return result;
}


// int % int
int64_t safe_modulo(
	int64_t a, int64_t b, uint32_t,
	lex::src_tokens src_tokens, parse_context &context
)
{
	if (b == 0)
	{
		context.report_error(
			src_tokens,
			"modulo by zero in constant expression"
		);
		return 0;
	}

	return a % b;
}

uint64_t safe_modulo(
	uint64_t a, uint64_t b, uint32_t,
	lex::src_tokens src_tokens, parse_context &context
)
{
	if (b == 0)
	{
		context.report_error(
			src_tokens,
			"modulo by zero in constant expression"
		);
		return 0;
	}

	return a % b;
}

// uint << uint
uint64_t safe_left_shift(
	uint64_t a, uint64_t b, uint32_t lhs_type_kind,
	lex::src_tokens src_tokens, parse_context &context
)
{
	switch (lhs_type_kind)
	{
	case ast::type_info::uint8_:
		if (b >= 8)
		{
			context.report_error(
				src_tokens,
				bz::format(
					"left shift amount of {} is too large for type 'uint8' in constant expression",
					b
				)
			);
			return 0;
		}
		return static_cast<uint64_t>(static_cast<uint8_t>(a << b));
	case ast::type_info::uint16_:
		if (b >= 16)
		{
			context.report_error(
				src_tokens,
				bz::format(
					"left shift amount of {} is too large for type 'uint16' in constant expression",
					b
				)
			);
			return 0;
		}
		return static_cast<uint64_t>(static_cast<uint16_t>(a << b));
	case ast::type_info::uint32_:
		if (b >= 32)
		{
			context.report_error(
				src_tokens,
				bz::format(
					"left shift amount of {} is too large for type 'uint32' in constant expression",
					b
				)
			);
			return 0;
		}
		return static_cast<uint64_t>(static_cast<uint32_t>(a << b));
	case ast::type_info::uint64_:
		if (b >= 64)
		{
			context.report_error(
				src_tokens,
				bz::format(
					"left shift amount of {} is too large for type 'uint64' in constant expression",
					b
				)
			);
			return 0;
		}
		return a << b;
	default:
		bz_assert(false);
		return 0;
	}
}

// uint >> uint
uint64_t safe_right_shift(
	uint64_t a, uint64_t b, uint32_t lhs_type_kind,
	lex::src_tokens src_tokens, parse_context &context
)
{
	switch (lhs_type_kind)
	{
	case ast::type_info::uint8_:
		if (b >= 8)
		{
			context.report_error(
				src_tokens,
				bz::format(
					"right shift amount of {} is too large for type 'uint8' in constant expression",
					b
				)
			);
			return 0;
		}
		return static_cast<uint64_t>(static_cast<uint8_t>(a >> b));
	case ast::type_info::uint16_:
		if (b >= 16)
		{
			context.report_error(
				src_tokens,
				bz::format(
					"right shift amount of {} is too large for type 'uint16' in constant expression",
					b
				)
			);
			return 0;
		}
		return static_cast<uint64_t>(static_cast<uint16_t>(a >> b));
	case ast::type_info::uint32_:
		if (b >= 32)
		{
			context.report_error(
				src_tokens,
				bz::format(
					"right shift amount of {} is too large for type 'uint32' in constant expression",
					b
				)
			);
			return 0;
		}
		return static_cast<uint64_t>(static_cast<uint32_t>(a >> b));
	case ast::type_info::uint64_:
		if (b >= 64)
		{
			context.report_error(
				src_tokens,
				bz::format(
					"right shift amount of {} is too large for type 'uint64' in constant expression",
					b
				)
			);
			return 0;
		}
		return a >> b;
	default:
		bz_assert(false);
		return 0;
	}
}

} // namespace ctx
