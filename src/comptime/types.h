#ifndef COMPTIME_TYPES_H
#define COMPTIME_TYPES_H

#include "core.h"
#include "ast/type_prototype.h"
#include "lex/token.h"

namespace comptime
{

using type = ast::type_prototype;
using builtin_type_kind = ast::builtin_type_kind;
using type_set_t = ast::type_prototype_set_t;

struct alloca
{
	type const *object_type;
	bool is_always_initialized;
	lex::src_tokens src_tokens;
};

} // namespace comptime

#endif // COMPTIME_TYPES_H
