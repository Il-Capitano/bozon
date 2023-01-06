#ifndef COMPTIME_CODEGEN_H
#define COMPTIME_CODEGEN_H

#include "core.h"
#include "codegen_context.h"
#include "ast/statement.h"

namespace comptime
{

void generate_code(ast::function_body &body, codegen_context &context);

void generate_destruct_operation(codegen_context::destruct_operation_info_t const &destruct_op_info, codegen_context &context);

} // namespace comptime

#endif // COMPTIME_CODEGEN_H
