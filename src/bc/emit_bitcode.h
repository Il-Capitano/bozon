#ifndef BC_EMIT_BITCODE_H
#define BC_EMIT_BITCODE_H

#include "ast/typespec.h"
#include "ast/expression.h"
#include "ast/statement.h"
#include "ctx/bitcode_context.h"

namespace bc
{

void add_function_to_module(
	ast::function_body &func_body,
	bz::string_view id,
	ctx::bitcode_context &context
);
void emit_function_bitcode(
	ast::function_body &func_body,
	ctx::bitcode_context &context
);

} // namespace bc

#endif // BC_EMIT_BITCODE_H
