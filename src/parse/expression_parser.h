#ifndef PARSER_EXPRESSION_PARSER_H
#define PARSER_EXPRESSION_PARSER_H

#include "core.h"
#include "ast/expression.h"
#include "ctx/parse_context.h"
#include "token_info.h"

namespace parse
{

ast::expression parse_expression(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context,
	precedence prec
);

bz::vector<ast::expression> parse_expression_comma_list(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

} // namespace parse

#endif // PARSER_EXPRESSION_PARSER_H
