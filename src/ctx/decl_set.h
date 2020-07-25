#ifndef CTX_DECL_SET_H
#define CTX_DECL_SET_H

#include "ast/typespec.h"
#include "ast/expression.h"
#include "ast/statement.h"

namespace ctx
{

struct function_overload_set
{
	bz::u8string id;
	bz::vector<ast::decl_function *> func_decls;
};

struct operator_overload_set
{
	uint32_t op;
	bz::vector<ast::decl_operator *> op_decls;
};

struct typespec_with_name
{
	bz::u8string_view id;
	ast::typespec     ts;
};

struct decl_set
{
	bz::vector<ast::decl_variable *>  var_decls;
	bz::vector<function_overload_set> func_sets;
	bz::vector<operator_overload_set> op_sets;
	bz::vector<typespec_with_name>    types;
};

} // namespace ctx

#endif // CTX_DECL_SET_H
