#ifndef COMPTIME_GLOBAL_CODEGEN_CONTEXT_H
#define COMPTIME_GLOBAL_CODEGEN_CONTEXT_H

#include "core.h"
#include "types.h"
#include "lex/token.h"

namespace comptime
{

struct error_info_t
{
	lex::src_tokens src_tokens;
	bz::u8string message;
};

struct global_codegen_context
{
	type_set_t type_set;
	type const *pointer_pair_t;
	type const *null_t;

	bz::vector<error_info_t> errors;
	bz::vector<lex::src_tokens> src_tokens;

	global_codegen_context(size_t pointer_size);

	type const *get_builtin_type(builtin_type_kind kind);
	type const *get_pointer_type(void);
	type const *get_aggregate_type(bz::array_view<type const * const> elem_types);
	type const *get_array_type(type const *elem_type, size_t size);
	type const *get_str_t(void);
	type const *get_null_t(void);
	type const *get_slice_t(void);
	type const *get_optional_type(type const *value_type);
};

} // namespace comptime

#endif // COMPTIME_GLOBAL_CODEGEN_CONTEXT_H
