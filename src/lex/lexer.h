#ifndef LEX_LEXER_H
#define LEX_LEXER_H

#include "core.h"

#include "token.h"
#include "token_info.h"
#include "ctx/error.h"
#include "ctx/lex_context.h"

namespace lex
{

bz::vector<token> get_tokens(
	bz::u8string_view file,
	bz::u8string_view file_name,
	ctx::lex_context &context
);

constexpr bz::u8string_view get_token_value(uint32_t kind)
{
	bz_assert(kind < token_info.size());
	auto &ti = token_info[kind];
	if (ti.token_value.size() == 0)
	{
		return ti.token_name;
	}
	else
	{
		return ti.token_value;
	}
}

inline bz::u8string get_token_name_for_message(uint32_t kind)
{
	bz_assert(kind < token_info.size());
	auto &ti = token_info[kind];
	if (ti.token_value.size() == 0)
	{
		return ti.token_name;
	}
	else
	{
		return bz::format("'{}'", ti.token_value);
	}
}

} // namespace lex

#endif // LEX_LEXER_H
