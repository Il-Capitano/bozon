#ifndef RESOLVE_SAFE_OPERATIONS_H
#define RESOLVE_SAFE_OPERATIONS_H

#include "ctx/parse_context.h"

namespace resolve
{

int64_t safe_unary_minus(
	lex::src_tokens const &src_tokens, int paren_level,
	int64_t value, uint32_t type_kind,
	ctx::parse_context &context
);


int64_t safe_binary_plus(
	lex::src_tokens const &src_tokens, int paren_level,
	int64_t lhs, int64_t rhs, uint32_t type_kind,
	ctx::parse_context &context
);

uint64_t safe_binary_plus(
	lex::src_tokens const &src_tokens, int paren_level,
	uint64_t lhs, uint64_t rhs, uint32_t type_kind,
	ctx::parse_context &context
);

float32_t safe_binary_plus(
	lex::src_tokens const &src_tokens, int paren_level,
	float32_t lhs, float32_t rhs,
	ctx::parse_context &context
);

float64_t safe_binary_plus(
	lex::src_tokens const &src_tokens, int paren_level,
	float64_t lhs, float64_t rhs,
	ctx::parse_context &context
);

bz::optional<bz::u8char> safe_binary_plus(
	lex::src_tokens const &src_tokens, int paren_level,
	bz::u8char lhs, int64_t rhs,
	ctx::parse_context &context
);

bz::optional<bz::u8char> safe_binary_plus(
	lex::src_tokens const &src_tokens, int paren_level,
	bz::u8char lhs, uint64_t rhs,
	ctx::parse_context &context
);

bz::optional<bz::u8char> safe_binary_plus(
	lex::src_tokens const &src_tokens, int paren_level,
	int64_t lhs, bz::u8char rhs,
	ctx::parse_context &context
);

bz::optional<bz::u8char> safe_binary_plus(
	lex::src_tokens const &src_tokens, int paren_level,
	uint64_t lhs, bz::u8char rhs,
	ctx::parse_context &context
);


int64_t safe_binary_minus(
	lex::src_tokens const &src_tokens, int paren_level,
	int64_t lhs, int64_t rhs, uint32_t type_kind,
	ctx::parse_context &context
);

uint64_t safe_binary_minus(
	lex::src_tokens const &src_tokens, int paren_level,
	uint64_t lhs, uint64_t rhs, uint32_t type_kind,
	ctx::parse_context &context
);

float32_t safe_binary_minus(
	lex::src_tokens const &src_tokens, int paren_level,
	float32_t lhs, float32_t rhs,
	ctx::parse_context &context
);

float64_t safe_binary_minus(
	lex::src_tokens const &src_tokens, int paren_level,
	float64_t lhs, float64_t rhs,
	ctx::parse_context &context
);

bz::optional<bz::u8char> safe_binary_minus(
	lex::src_tokens const &src_tokens, int paren_level,
	bz::u8char lhs, int64_t rhs,
	ctx::parse_context &context
);

bz::optional<bz::u8char> safe_binary_minus(
	lex::src_tokens const &src_tokens, int paren_level,
	bz::u8char lhs, uint64_t rhs,
	ctx::parse_context &context
);


int64_t safe_binary_multiply(
	lex::src_tokens const &src_tokens, int paren_level,
	int64_t lhs, int64_t rhs, uint32_t type_kind,
	ctx::parse_context &context
);

uint64_t safe_binary_multiply(
	lex::src_tokens const &src_tokens, int paren_level,
	uint64_t lhs, uint64_t rhs, uint32_t type_kind,
	ctx::parse_context &context
);

float32_t safe_binary_multiply(
	lex::src_tokens const &src_tokens, int paren_level,
	float32_t lhs, float32_t rhs,
	ctx::parse_context &context
);

float64_t safe_binary_multiply(
	lex::src_tokens const &src_tokens, int paren_level,
	float64_t lhs, float64_t rhs,
	ctx::parse_context &context
);


bz::optional<int64_t> safe_binary_divide(
	lex::src_tokens const &src_tokens, int paren_level,
	int64_t lhs, int64_t rhs, uint32_t type_kind,
	ctx::parse_context &context
);

bz::optional<uint64_t> safe_binary_divide(
	lex::src_tokens const &src_tokens, int paren_level,
	uint64_t lhs, uint64_t rhs, uint32_t type_kind,
	ctx::parse_context &context
);

float32_t safe_binary_divide(
	lex::src_tokens const &src_tokens, int paren_level,
	float32_t lhs, float32_t rhs,
	ctx::parse_context &context
);

float64_t safe_binary_divide(
	lex::src_tokens const &src_tokens, int paren_level,
	float64_t lhs, float64_t rhs,
	ctx::parse_context &context
);


bz::optional<int64_t> safe_binary_modulo(
	lex::src_tokens const &src_tokens, int paren_level,
	int64_t lhs, int64_t rhs, uint32_t type_kind,
	ctx::parse_context &context
);

bz::optional<uint64_t> safe_binary_modulo(
	lex::src_tokens const &src_tokens, int paren_level,
	uint64_t lhs, uint64_t rhs, uint32_t type_kind,
	ctx::parse_context &context
);


bool safe_binary_equals(
	lex::src_tokens const &src_tokens, int paren_level,
	float32_t lhs, float32_t rhs,
	ctx::parse_context &context
);

bool safe_binary_equals(
	lex::src_tokens const &src_tokens, int paren_level,
	float64_t lhs, float64_t rhs,
	ctx::parse_context &context
);


bz::optional<uint64_t> safe_binary_bit_left_shift(
	lex::src_tokens const &src_tokens,
	uint64_t lhs, uint64_t rhs, uint32_t lhs_type_kind,
	ctx::parse_context &context
);

bz::optional<uint64_t> safe_binary_bit_right_shift(
	lex::src_tokens const &src_tokens,
	uint64_t lhs, uint64_t rhs, uint32_t lhs_type_kind,
	ctx::parse_context &context
);


bz::optional<uint64_t> safe_binary_bit_left_shift(
	lex::src_tokens const &src_tokens,
	uint64_t lhs, int64_t rhs, uint32_t lhs_type_kind,
	ctx::parse_context &context
);

bz::optional<uint64_t> safe_binary_bit_right_shift(
	lex::src_tokens const &src_tokens,
	uint64_t lhs, int64_t rhs, uint32_t lhs_type_kind,
	ctx::parse_context &context
);

} // namespace resolve

#endif // RESOLVE_SAFE_OPERATIONS_H
