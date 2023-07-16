#ifndef RESOLVE_MATCH_TO_TYPE_H
#define RESOLVE_MATCH_TO_TYPE_H

#include "core.h"
#include "ast/typespec.h"
#include "ast/expression.h"
#include "ctx/parse_context.h"

namespace resolve
{

enum class reference_match_kind : uint8_t
{
	reference_exact = 0,
	reference_remove_mut,
	rvalue_copy,
	auto_reference_exact,
	auto_reference_remove_mut,
	auto_reference_const,
	lvalue_copy,
};

enum class type_match_kind : uint8_t
{
	exact_match = 0,
	implicit_literal_conversion,
	generic_match,
	direct_match, // match to 'auto' or 'typename'
	implicit_conversion,
	none,
};

struct single_match_t
{
	uint16_t             modifier_match_level; // pointer, const, etc...
	reference_match_kind reference_match;
	type_match_kind      type_match;
};

struct match_level_t : public bz::variant<single_match_t, bz::vector<match_level_t>>
{
	using base_t = bz::variant<single_match_t, bz::vector<match_level_t>>;
	using base_t::variant;
	~match_level_t(void) noexcept = default;

	bool is_single(void) const noexcept
	{
		return this->is<single_match_t>();
	}

	bool is_multi(void) const noexcept
	{
		return this->is<bz::vector<match_level_t>>();
	}

	single_match_t &get_single(void) noexcept
	{
		bz_assert(this->is_single());
		return this->get<single_match_t>();
	}

	single_match_t const &get_single(void) const noexcept
	{
		bz_assert(this->is_single());
		return this->get<single_match_t>();
	}

	bz::vector<match_level_t> &get_multi(void) noexcept
	{
		bz_assert(this->is_multi());
		return this->get<bz::vector<match_level_t>>();
	}

	bz::vector<match_level_t> const &get_multi(void) const noexcept
	{
		bz_assert(this->is_multi());
		return this->get<bz::vector<match_level_t>>();
	}

	bz::vector<match_level_t> &emplace_multi(void) noexcept
	{
		return this->emplace<bz::vector<match_level_t>>();
	}
};

// returns -2 if lhs < rhs
// returns  2 if lhs > rhs
// returns -1 if lhs < rhs only by implicit literal conversions
// returns  1 if lhs > rhs only by implicit literal conversions
// returns  0 otherwise
int match_level_compare(match_level_t const &lhs, match_level_t const &rhs);
bool operator < (match_level_t const &lhs, match_level_t const &rhs);
match_level_t &operator += (match_level_t &lhs, uint16_t rhs);
match_level_t operator + (match_level_t lhs, uint16_t rhs);

match_level_t get_type_match_level(ast::typespec_view dest, ast::expression const &expr, ctx::parse_context &context);
match_level_t get_function_call_match_level(
	ast::statement_view func_stmt,
	ast::function_body &func_body,
	bz::array_view<ast::expression const> params,
	ctx::parse_context &context,
	lex::src_tokens const &src_tokens
);
match_level_t get_function_call_match_level(
	ast::statement_view func_stmt,
	ast::function_body &func_body,
	ast::expression const &expr,
	ctx::parse_context &context,
	lex::src_tokens const &src_tokens
);
match_level_t get_function_call_match_level(
	ast::statement_view func_stmt,
	ast::function_body &func_body,
	ast::expression const &lhs,
	ast::expression const &rhs,
	ctx::parse_context &context,
	lex::src_tokens const &src_tokens
);

} // namespace resolve

#endif // RESOLVE_MATCH_TO_TYPE_H
