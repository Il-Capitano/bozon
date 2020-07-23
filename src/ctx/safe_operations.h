#ifndef CTX_SAFE_OPERATIONS_H
#define CTX_SAFE_OPERATIONS_H

#include "parse_context.h"

namespace ctx
{

template<typename T, typename U>
constexpr bool is_in_range(U val) noexcept
{
	return val >= std::numeric_limits<T>::min() && val <= std::numeric_limits<T>::max();
}

// int + int
int64_t safe_add(
	int64_t a, int64_t b, uint32_t type_kind,
	lex::src_tokens src_tokens, parse_context &context
);
uint64_t safe_add(
	uint64_t a, uint64_t b, uint32_t type_kind,
	lex::src_tokens src_tokens, parse_context &context
);
// char + int
bz::u8char safe_add(
	bz::u8char a, int64_t b, uint32_t type_kind,
	lex::src_tokens src_tokens, parse_context &context,
	bool reversed = false
);
bz::u8char safe_add(
	bz::u8char a, uint64_t b, uint32_t type_kind,
	lex::src_tokens src_tokens, parse_context &context,
	bool reversed = false
);
// float32 + float32
float32_t safe_add(
	float32_t a, float32_t b,
	lex::src_tokens src_tokens, parse_context &context
);
// float64 + float64
float64_t safe_add(
	float64_t a, float64_t b,
	lex::src_tokens src_tokens, parse_context &context
);

// int - int
int64_t safe_subtract(
	int64_t a, int64_t b, uint32_t type_kind,
	lex::src_tokens src_tokens, parse_context &context
);
uint64_t safe_subtract(
	uint64_t a, uint64_t b, uint32_t type_kind,
	lex::src_tokens src_tokens, parse_context &context
);
// char - int
bz::u8char safe_subtract(
	bz::u8char a, int64_t b, uint32_t type_kind,
	lex::src_tokens src_tokens, parse_context &context
);
bz::u8char safe_subtract(
	bz::u8char a, uint64_t b, uint32_t type_kind,
	lex::src_tokens src_tokens, parse_context &context
);
// char - char
int32_t safe_subtract(
	bz::u8char a, bz::u8char b,
	lex::src_tokens src_tokens, parse_context &context
);
// float32 - float32
float32_t safe_subtract(
	float32_t a, float32_t b,
	lex::src_tokens src_tokens, parse_context &context
);
// float64 - float64
float64_t safe_subtract(
	float64_t a, float64_t b,
	lex::src_tokens src_tokens, parse_context &context
);

// int * int
int64_t safe_multiply(
	int64_t a, int64_t b, uint32_t type_kind,
	lex::src_tokens src_tokens, parse_context &context
);
uint64_t safe_multiply(
	uint64_t a, uint64_t b, uint32_t type_kind,
	lex::src_tokens src_tokens, parse_context &context
);
// float32 * float32
float32_t safe_multiply(
	float32_t a, float32_t b,
	lex::src_tokens src_tokens, parse_context &context
);
// float64 * float64
float64_t safe_multiply(
	float64_t a, float64_t b,
	lex::src_tokens src_tokens, parse_context &context
);

// int / int
int64_t safe_divide(
	int64_t a, int64_t b, uint32_t type_kind,
	lex::src_tokens src_tokens, parse_context &context
);
uint64_t safe_divide(
	uint64_t a, uint64_t b, uint32_t type_kind,
	lex::src_tokens src_tokens, parse_context &context
);
// float32 / float32
float32_t safe_divide(
	float32_t a, float32_t b,
	lex::src_tokens src_tokens, parse_context &context
);
// float64 / float64
float64_t safe_divide(
	float64_t a, float64_t b,
	lex::src_tokens src_tokens, parse_context &context
);

// int % int
int64_t safe_modulo(
	int64_t a, int64_t b, uint32_t type_kind,
	lex::src_tokens src_tokens, parse_context &context
);
uint64_t safe_modulo(
	uint64_t a, uint64_t b, uint32_t type_kind,
	lex::src_tokens src_tokens, parse_context &context
);

// uint << uint
uint64_t safe_left_shift(
	uint64_t a, uint64_t b, uint32_t lhs_type_kind,
	lex::src_tokens src_tokens, parse_context &context
);
// uint >> uint
uint64_t safe_right_shift(
	uint64_t a, uint64_t b, uint32_t lhs_type_kind,
	lex::src_tokens src_tokens, parse_context &context
);

} // namespace ctx

#endif // CTX_SAFE_OPERATIONS_H
