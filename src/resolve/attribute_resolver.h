#ifndef RESOLVE_ATTRIBUTE_RESOLVER_H
#define RESOLVE_ATTRIBUTE_RESOLVER_H

#include "core.h"
#include "ast/typespec.h"
#include "ast/statement.h"
#include "ctx/parse_context.h"

namespace resolve
{

using apply_to_function_func_t = bool (*)(
	ast::decl_function &func_decl,
	ast::attribute &attribute,
	ctx::parse_context &context
);

using apply_to_operator_func_t = bool (*)(
	ast::decl_operator &op_decl,
	ast::attribute &attribute,
	ctx::parse_context &context
);

using apply_to_function_body_func_t = bool (*)(
	ast::function_body &func_body,
	ast::attribute &attribute,
	ctx::parse_context &context
);

using apply_to_variable_func_t = bool (*)(
	ast::decl_variable &var_decl,
	ast::attribute &attribute,
	ctx::parse_context &context
);

using apply_to_type_alias_func_t = bool (*)(
	ast::decl_type_alias &alias_decl,
	ast::attribute &attribute,
	ctx::parse_context &context
);

using apply_to_type_info_func_t = bool (*)(
	ast::type_info &info,
	ast::attribute &attribute,
	ctx::parse_context &context
);

struct apply_funcs_t
{
	apply_to_function_func_t apply_to_func;
	apply_to_operator_func_t apply_to_op;
	apply_to_function_body_func_t apply_to_func_body;
	apply_to_variable_func_t apply_to_var;
	apply_to_type_alias_func_t apply_to_type_alias;
	apply_to_type_info_func_t apply_to_type_info;

	bool operator () (ast::decl_function &func_decl, ast::attribute &attribute, ctx::parse_context &context) const;
	bool operator () (ast::decl_operator &op_decl, ast::attribute &attribute, ctx::parse_context &context) const;
	bool operator () (ast::decl_variable &var_decl, ast::attribute &attribute, ctx::parse_context &context) const;
	bool operator () (ast::decl_type_alias &alias_decl, ast::attribute &attribute, ctx::parse_context &context) const;
	bool operator () (ast::type_info &info, ast::attribute &attribute, ctx::parse_context &context) const;
};

struct attribute_info_t
{
	bz::u8string_view name;
	bz::vector<ast::typespec> arg_types;
	apply_funcs_t apply_funcs;
};

bz::vector<attribute_info_t> make_attribute_infos(bz::array_view<ast::type_info * const> builtin_type_infos);

void resolve_attributes(
	ast::decl_function &func_decl,
	ctx::parse_context &context
);
void resolve_attributes(
	ast::decl_operator &op_decl,
	ctx::parse_context &context
);
void resolve_attributes(
	ast::decl_variable &var_decl,
	ctx::parse_context &context
);
void resolve_attributes(
	ast::decl_type_alias &alias_decl,
	ctx::parse_context &context
);
void resolve_attributes(
	ast::type_info &info,
	ctx::parse_context &context
);

} // namespace resolve

#endif // RESOLVE_ATTRIBUTE_RESOLVER_H
