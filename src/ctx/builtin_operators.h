#ifndef CTX_BUILT_IN_OPERATORS_H
#define CTX_BUILT_IN_OPERATORS_H

#include "ast/expression.h"
#include "parse_context.h"

namespace ctx
{

ast::expression make_builtin_operation(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression expr,
	parse_context &context
);

ast::expression make_builtin_type_operation(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression expr,
	parse_context &context
);

ast::decl_operator *get_builtin_operator(
	uint32_t op_kind,
	ast::expression const &expr,
	parse_context &context
);

ast::expression make_builtin_operation(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
);

ast::expression make_builtin_type_operation(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
);

ast::decl_operator *get_builtin_operator(
	uint32_t op_kind,
	ast::expression const &lhs,
	ast::expression const &rhs,
	parse_context &context
);

ast::expression make_builtin_cast(
	lex::src_tokens const &src_tokens,
	ast::expression expr,
	ast::typespec dest_type,
	parse_context &context
);

ast::expression make_builtin_subscript_operator(
	lex::src_tokens const &src_tokens,
	ast::expression called,
	ast::expression arg,
	parse_context &context
);

ast::expression make_unary_literal_operation(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression const &expr,
	parse_context &context
);

ast::expression make_binary_literal_operation(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression const &lhs,
	ast::expression const &rhs,
	parse_context &context
);

} // namespace ctx

#endif // CTX_BUILT_IN_OPERATORS_H
