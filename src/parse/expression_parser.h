#ifndef PARSER_EXPRESSION_PARSER_H
#define PARSER_EXPRESSION_PARSER_H

#include "core.h"
#include "ast/expression.h"
#include "ctx/parse_context.h"

namespace parse
{

ast::expression parse_expression_without_semi_colon(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context,
	precedence prec
);
void consume_semi_colon_at_end_of_expression(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context,
	ast::expression const &expression
);

ast::expression parse_top_level_expression(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

ast::expression parse_expression(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context,
	precedence prec
);

ast::arena_vector<ast::expression> parse_expression_comma_list(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

} // namespace parse

#endif // PARSER_EXPRESSION_PARSER_H
