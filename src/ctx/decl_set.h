#ifndef CTX_DECL_SET_H
#define CTX_DECL_SET_H

#include "ast/typespec.h"
#include "ast/expression.h"
#include "ast/statement.h"

namespace ctx
{

struct function_overload_set
{
	bz::string id;
	bz::vector<ast::decl_function *> func_decls;
};

struct operator_overload_set
{
	uint32_t op;
	bz::vector<ast::decl_operator *> op_decls;
};

struct type_info_with_name
{
	bz::string_view id;
	ast::type_info *info;
};

struct decl_set
{
	bz::vector<ast::decl_variable *>  var_decls;
	bz::vector<function_overload_set> func_sets;
	bz::vector<operator_overload_set> op_sets;
	bz::vector<type_info_with_name>   type_infos;
};

} // namespace ctx

#endif // CTX_DECL_SET_H
