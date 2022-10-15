#ifndef RESOLVE_TYPE_MATCH_GENERIC_H
#define RESOLVE_TYPE_MATCH_GENERIC_H

#include "ast/expression.h"
#include "ctx/parse_context.h"
#include "match_common.h"
#include "match_to_type.h"

namespace resolve
{

inline reference_match_kind get_reference_match_kind_from_expr_kind(ast::expression_type_kind kind)
{
	return ast::is_rvalue(kind) ? reference_match_kind::rvalue_copy : reference_match_kind::lvalue_copy;
}

enum class type_match_function_kind
{
	can_match,
	match_level,
	matched_type,
	match_expression,
};


template<type_match_function_kind kind>
struct match_function_result
{
	using type = void;
};

template<>
struct match_function_result<type_match_function_kind::can_match>
{
	using type = bool;
};

template<>
struct match_function_result<type_match_function_kind::match_level>
{
	using type = match_level_t;
};

template<>
struct match_function_result<type_match_function_kind::matched_type>
{
	using type = ast::typespec;
};

template<>
struct match_function_result<type_match_function_kind::match_expression>
{
	using type = bool;
};

template<type_match_function_kind kind>
using match_function_result_t = typename match_function_result<kind>::type;


template<type_match_function_kind kind>
struct match_context_t;

template<>
struct match_context_t<type_match_function_kind::can_match>
{
	using expr_ref_type = ast::expression const &;

	static inline constexpr bool report_errors = false;

	ast::expression const &expr;
	ast::typespec_view dest;
	ctx::parse_context &context;
};

template<>
struct match_context_t<type_match_function_kind::match_level>
{
	using expr_ref_type = ast::expression const &;

	static inline constexpr bool report_errors = false;

	ast::expression const &expr;
	ast::typespec_view dest;
	ctx::parse_context &context;
};

template<>
struct match_context_t<type_match_function_kind::matched_type>
{
	using expr_ref_type = ast::expression const &;

	static inline constexpr bool report_errors = false;

	ast::expression const &expr;
	ast::typespec_view dest;
	ctx::parse_context &context;
};

template<>
struct match_context_t<type_match_function_kind::match_expression>
{
	using expr_ref_type = ast::expression &;

	static inline constexpr bool report_errors = true;

	ast::expression &expr;
	ast::typespec &dest_container;
	ast::typespec_view dest;
	ctx::parse_context &context;
};


template<type_match_function_kind kind>
struct strict_match_context_t;

template<>
struct strict_match_context_t<type_match_function_kind::can_match>
{
	ast::typespec_view source;
	ast::typespec_view dest;
	ctx::parse_context &context;
};

template<>
struct strict_match_context_t<type_match_function_kind::match_level>
{
	ast::typespec_view source;
	ast::typespec_view dest;
	reference_match_kind reference_match;
	type_match_kind base_type_match;
	ctx::parse_context &context;
};

template<>
struct strict_match_context_t<type_match_function_kind::matched_type>
{
	ast::typespec_view source;
	ast::typespec_view dest;
	ast::typespec_view original_dest;
	ctx::parse_context &context;
};

template<>
struct strict_match_context_t<type_match_function_kind::match_expression>
{
	ast::expression &expr;
	ast::typespec &original_dest_container;
	ast::typespec &dest_container;
	ast::typespec_view source;
	ast::typespec_view dest;
	ctx::parse_context &context;
};

template<type_match_function_kind kind>
match_function_result_t<kind> generic_type_match(match_context_t<kind> const &match_context);

} // namespace resolve

#endif // RESOLVE_TYPE_MATCH_GENERIC_H
