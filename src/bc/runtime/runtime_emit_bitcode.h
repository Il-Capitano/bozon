#ifndef BC_RUNTIME_EMIT_BITCODE_H
#define BC_RUNTIME_EMIT_BITCODE_H

#include "ast/typespec.h"
#include "ast/expression.h"
#include "ast/statement.h"
#include "ctx/bitcode_context.h"
#include "bc/common.h"

namespace bc::runtime
{

void add_function_to_module(
	ast::function_body *func_body,
	ctx::bitcode_context &context
);
void emit_function_bitcode(
	ast::function_body const &func_body,
	ctx::bitcode_context &context
);

void emit_global_variable(ast::decl_variable const &var_decl, ctx::bitcode_context &context);
void emit_global_type_symbol(ast::decl_struct const &struct_decl, ctx::bitcode_context &context);
void emit_global_type(ast::decl_struct const &struct_decl, ctx::bitcode_context &context);
void add_builtin_functions(ctx::bitcode_context &context);
void emit_necessary_functions(ctx::bitcode_context &context);

} // namespace bc::runtime

#endif // BC_RUNTIME_EMIT_BITCODE_H
