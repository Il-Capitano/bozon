#ifndef AST_IDENTIFIER_H
#define AST_IDENTIFIER_H

#include "core.h"
#include "lex/token.h"
#include "lex/lexer.h"
#include "allocator.h"

namespace ast
{

struct identifier
{
	lex::token_range tokens = {};
	arena_vector<bz::u8string_view> values{};
	bool is_qualified = false;

	bz::u8string format_as_unqualified(void) const
	{
		bz::u8string result;
		bool first = true;
		for (auto const value : this->values)
		{
			if (first)
			{
				first = false;
			}
			else
			{
				constexpr auto scope = lex::get_token_value(lex::token::scope);
				result += scope;
			}
			result += value;
		}
		return result;
	}

	bz::u8string format_for_symbol(void) const
	{
		bz::u8string result;
		bool first = true;
		for (auto const value : this->values)
		{
			if (first)
			{
				first = false;
			}
			else
			{
				result += '.';
			}
			result += value;
		}
		return result;
	}

	bz::u8string format_for_symbol(int unique_id) const
	{
		bz::u8string result;
		bool first = true;
		for (auto const value : this->values)
		{
			if (first)
			{
				first = false;
			}
			else
			{
				result += '.';
			}
			result += value;
		}
		result += bz::format(".{}", unique_id);
		return result;
	}

	bz::u8string as_string(void) const
	{
		bz::u8string result;
		if (this->is_qualified)
		{
			result += "::";
		}
		bool first = true;
		for (auto const value : this->values)
		{
			if (first)
			{
				first = false;
			}
			else
			{
				constexpr auto scope = lex::get_token_value(lex::token::scope);
				result += scope;
			}
			result += value;
		}
		return result;
	}

	bool empty(void) const noexcept
	{ return this->values.empty(); }

	bool not_empty(void) const noexcept
	{ return this->values.not_empty(); }
};

inline identifier make_identifier(lex::token_pos id)
{
	return {
		{ id, id + 1 },
		{ id->value },
		false
	};
}

inline identifier make_identifier(bz::u8string_view id)
{
	return {
		{},
		{ id },
		false
	};
}

inline identifier make_identifier(lex::token_range tokens)
{
	bz_assert(tokens.begin != nullptr);
	bz_assert(tokens.begin != tokens.end);
	identifier result = {
		tokens,
		{},
		tokens.begin->kind == lex::token::scope
	};
	result.values.reserve((tokens.end - tokens.begin + 1) / 2);
	for (auto it = tokens.begin; it != tokens.end; ++it)
	{
		if (it->kind == lex::token::identifier)
		{
			result.values.push_back(it->value);
		}
	}
	return result;
}

inline bool operator == (identifier const &lhs, identifier const &rhs) noexcept
{
	return lhs.is_qualified == rhs.is_qualified
		&& lhs.values.size() == rhs.values.size()
		&& std::equal(lhs.values.begin(), lhs.values.end(), rhs.values.begin());
}

inline bool operator != (identifier const &lhs, identifier const &rhs) noexcept
{
	return !(lhs == rhs);
}

} // namespace ast

#endif // AST_IDENTIFIER_H
