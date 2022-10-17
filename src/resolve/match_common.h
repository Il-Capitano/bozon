#ifndef RESOLVE_MATCH_COMMON_H
#define RESOLVE_MATCH_COMMON_H

#include "core.h"
#include "ast/typespec.h"
#include "ast/expression.h"
#include "ctx/parse_context.h"

namespace resolve
{

void expand_variadic_tuple_type(ast::arena_vector<ast::typespec> &tuple_types, size_t new_size);
bool is_implicitly_convertible(
	ast::typespec_view dest,
	ast::expression const &expr,
	ctx::parse_context &context
);
bool is_implicitly_convertible(
	ast::typespec_view dest,
	ast::typespec_view expr_type,
	ast::expression_type_kind expr_type_kind,
	ctx::parse_context &context
);

bool is_integer_implicitly_convertible(uint8_t dest_kind, ast::literal_kind kind, uint64_t value);
bool is_integer_implicitly_convertible(uint8_t dest_kind, ast::literal_kind kind, int64_t value);

} // namespace resolve

#endif // RESOLVE_MATCH_COMMON_H
