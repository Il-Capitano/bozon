#ifndef COMPTIME_CODEGEN_H
#define COMPTIME_CODEGEN_H

#include "core.h"
#include "codegen_context_forward.h"
#include "instructions_forward.h"
#include "types.h"
#include "ast/statement.h"

namespace comptime
{

type const *get_type(ast::typespec_view type, codegen_context &context);

void generate_code(function &func, codegen_context &context);
std::unique_ptr<function> generate_from_symbol(ast::function_body &body, codegen_context &context);

function generate_code_for_expression(ast::expression const &expr, codegen_context &context);

void generate_destruct_operation(destruct_operation_info_t const &destruct_op_info, codegen_context &context);
void generate_consteval_variable(ast::decl_variable const &var_decl, codegen_context &context);

} // namespace comptime

#endif // COMPTIME_CODEGEN_H
