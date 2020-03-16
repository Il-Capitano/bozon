#ifndef BC_EMIT_BITCODE_H
#define BC_EMIT_BITCODE_H

#include "ast/typespec.h"
#include "ast/expression.h"
#include "ast/statement.h"
#include "ctx/bitcode_context.h"

namespace bc
{

llvm::Function *get_function_decl_bitcode(ast::decl_function &func, ctx::bitcode_context &context);
void emit_function_bitcode(ast::decl_function &func, ctx::bitcode_context &context);

} // namespace bc

#endif // BC_EMIT_BITCODE_H
