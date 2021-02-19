#include "parse_common.h"
#include "expression_parser.h"

namespace parse
{

ast::expression parse_parenthesized_condition(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	auto const open_paren = context.assert_token(stream, lex::token::paren_open);
	auto const condition = parse_expression(stream, end, context, precedence{});

	if (stream != end && stream->kind == lex::token::paren_close)
	{
		++stream;
	}
	else
	{
		if (open_paren->kind == lex::token::paren_open)
		{
			context.report_paren_match_error(stream, open_paren);
		}
		get_expression_tokens<
			lex::token::curly_open,
			lex::token::kw_if,
			lex::token::paren_close
		>(stream, end, context);
	}

	return condition;
}

ast::identifier get_identifier(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	if (
		stream == end
		|| (stream->kind != lex::token::identifier && stream->kind != lex::token::scope)
	)
	{
		context.assert_token(stream, lex::token::identifier);
		return ast::identifier{};
	}

	auto const begin_token = stream;
	auto const is_qualified = stream->kind == lex::token::scope;
	auto is_last_scope = !is_qualified;
	while (stream != end)
	{
		if (is_last_scope && stream->kind == lex::token::identifier)
		{
			++stream;
		}
		else if (!is_last_scope && stream->kind == lex::token::scope)
		{
			++stream;
		}
		else
		{
			break;
		}
		is_last_scope = !is_last_scope;
	}
	auto const end_token = stream;
	if (is_last_scope)
	{
		context.assert_token(stream, lex::token::identifier);
	}
	return ast::make_identifier({ begin_token, end_token });
}

} // namespace parse
