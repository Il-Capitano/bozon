#ifndef CODEGEN_C_CODEGEN_H
#define CODEGEN_C_CODEGEN_H

#include "core.h"
#include "codegen_context.h"
#include "ast/statement_forward.h"

namespace codegen::c
{

type generate_struct(ast::type_info const &info, codegen_context &context);
void generate_global_variable(ast::decl_variable const &var_decl, codegen_context &context);

} // namespace codegen::c

#endif // CODEGEN_C_CODEGEN_H
