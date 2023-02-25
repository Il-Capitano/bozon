#ifndef COMPTIME_MEMORY_VALUE_CONVERSION_H
#define COMPTIME_MEMORY_VALUE_CONVERSION_H

#include "types.h"
#include "memory.h"
#include "codegen_context_forward.h"
#include "executor_context_forward.h"
#include "ast/constant_value.h"

namespace comptime::memory
{

bz::fixed_vector<uint8_t> object_from_constant_value(
	lex::src_tokens const &src_tokens,
	ast::constant_value const &value,
	type const *object_type,
	codegen_context &context
);

struct constant_value_from_object_result_t
{
	ast::constant_value value;
	bz::vector<error_reason_t> reasons;
};

constant_value_from_object_result_t constant_value_from_object(
	type const *object_type,
	uint8_t const *mem,
	ast::typespec_view ts,
	endianness_kind endianness,
	executor_context const &manager
);

} // namespace comptime::memory

#endif // COMPTIME_MEMORY_VALUE_CONVERSION_H
