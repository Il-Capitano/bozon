#ifndef COMPTIME_CODEGEN_H
#define COMPTIME_CODEGEN_H

#include "instructions.h"
#include "codegen_context.h"
#include "ast/statement.h"

namespace comptime
{

void generate_code(ast::function_body &body, codegen_context &context);

} // namespace comptime

#endif // COMPTIME_CODEGEN_H
