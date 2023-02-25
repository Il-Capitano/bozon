#ifndef RESOLVE_CONSTEVAL_H
#define RESOLVE_CONSTEVAL_H

#include "ast/expression.h"
#include "ctx/parse_context.h"

namespace resolve
{

struct flattened_array_info_t
{
	ast::typespec_view elem_type;
	size_t size;
	bool is_multi_dimensional;
};

flattened_array_info_t get_flattened_array_type_and_size(ast::typespec_view type);
bool is_special_array_type(ast::typespec_view type);

void consteval_guaranteed(ast::expression &expr, ctx::parse_context &context);
void consteval_try(ast::expression &expr, ctx::parse_context &context);
void consteval_try_without_error(ast::expression &expr, ctx::parse_context &context);

void consteval_try_without_error_decl(ast::statement &stmt, ctx::parse_context &context);

} // namespace resolve

#endif // RESOLVE_CONSTEVAL_H
