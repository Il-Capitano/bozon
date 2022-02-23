#ifndef RESOLVE_MATCH_TO_TYPE_H
#define RESOLVE_MATCH_TO_TYPE_H

#include "core.h"
#include "ast/typespec.h"
#include "ast/expression.h"
#include "ctx/parse_context.h"

namespace resolve
{

struct match_level_t : public bz::variant<int, bz::vector<match_level_t>>
{
	using base_t = bz::variant<int, bz::vector<match_level_t>>;
	using base_t::variant;
	~match_level_t(void) noexcept = default;
};

// returns -1 if lhs < rhs, 0 if lhs == rhs or they are ambiguous and 1 if lhs > rhs
int match_level_compare(match_level_t const &lhs, match_level_t const &rhs);
bool operator < (match_level_t const &lhs, match_level_t const &rhs);
match_level_t operator + (match_level_t lhs, int rhs);

match_level_t get_type_match_level(ast::typespec_view dest, ast::expression &expr, ctx::parse_context &context);
match_level_t get_function_call_match_level(
	ast::statement_view func_stmt,
	ast::function_body &func_body,
	bz::array_view<ast::expression> params,
	ctx::parse_context &context,
	lex::src_tokens src_tokens
);
match_level_t get_function_call_match_level(
	ast::statement_view func_stmt,
	ast::function_body &func_body,
	ast::expression &expr,
	ctx::parse_context &context,
	lex::src_tokens src_tokens
);
match_level_t get_function_call_match_level(
	ast::statement_view func_stmt,
	ast::function_body &func_body,
	ast::expression &lhs,
	ast::expression &rhs,
	ctx::parse_context &context,
	lex::src_tokens src_tokens
);

} // namespace resolve

#endif // RESOLVE_MATCH_TO_TYPE_H
