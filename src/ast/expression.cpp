#include "expression.h"

namespace ast
{

lex::token_pos expression::get_tokens_begin(void) const
{ return this->tokens.begin; }

lex::token_pos expression::get_tokens_pivot(void) const
{
	if (this->kind() == index<expr_tuple>)
	{
		return this->tokens.begin;
	}
	else
	{
		return base_t::get_tokens_pivot();
	}
}


lex::token_pos expression::get_tokens_end(void) const
{ return this->tokens.end; }


lex::token_pos expr_unary_op::get_tokens_begin(void) const
{ return this->op; }

lex::token_pos expr_unary_op::get_tokens_pivot(void) const
{ return this->op; }

lex::token_pos expr_unary_op::get_tokens_end(void) const
{ return this->expr.get_tokens_end(); }


lex::token_pos expr_binary_op::get_tokens_begin(void) const
{ return this->lhs.get_tokens_begin(); }

lex::token_pos expr_binary_op::get_tokens_pivot(void) const
{ return this->op; }

lex::token_pos expr_binary_op::get_tokens_end(void) const
{
	if (this->op->kind == lex::token::square_open)
	{
		return this->rhs.get_tokens_end() + 1;
	}
	else
	{
		return this->rhs.get_tokens_end();
	}
}


lex::token_pos expr_function_call::get_tokens_begin(void) const
{ return this->called.get_tokens_begin(); }

lex::token_pos expr_function_call::get_tokens_pivot(void) const
{ return this->op; }

lex::token_pos expr_function_call::get_tokens_end(void) const
{
	if (this->params.size() == 0)
	{
		return this->called.get_tokens_end() + 2;
	}
	else
	{
		return this->params.back().get_tokens_end() + 1;
	}
}

lex::token_pos expr_cast::get_tokens_begin(void) const
{ return this->expr.get_tokens_begin(); }

lex::token_pos expr_cast::get_tokens_pivot(void) const
{ return this->as_pos; }

lex::token_pos expr_cast::get_tokens_end(void) const
{ return this->type.get_tokens_end(); }



static bz::u8char get_character(bz::u8string_view::const_iterator &it)
{
	switch (*it)
	{
	case '\\':
		++it;
		switch (*it)
		{
		case '\\':
			++it;
			return '\\';
		case '\'':
			++it;
			return '\'';
		case '\"':
			++it;
			return '\"';
		case 'n':
			++it;
			return '\n';
		case 't':
			++it;
			return '\t';
		case 'x':
		{
			++it;
			bz_assert(*it >= '0' && *it <= '7');
			uint8_t val = (*it - '0') << 4;
			++it;
			bz_assert(
				(*it >= '0' && *it <= '9')
				|| (*it >= 'a' && *it <= 'f')
				|| (*it >= 'A' && *it <= 'F')
			);
			val |= *it >= '0' && *it <= '9' ? *it - '0'
				: *it >= 'a' && *it <= 'f' ? *it - 'a' + 10
				: *it - 'A' + 10;
			++it;
			return static_cast<bz::u8char>(val);
		}
		default:
			bz_assert(false);
			return '\0';
		}

	default:
	{
		auto const res = *it;
		++it;
		return res;
	}
	}
}

expr_literal::expr_literal(lex::token_pos stream)
	: src_pos(stream)
{
	switch(stream->kind)
	{
	case lex::token::integer_literal:
	{
		uint64_t num = 0;
		for (auto const c : stream->value)
		{
			if (c == '\'')
			{
				continue;
			}
			if (num > std::numeric_limits<uint64_t>::max() / 10)
			{
				// TODO: report error
				bz_assert(false);
			}
			num = (10 * num) + (c - '0');
		}
		this->value.emplace<integer_number>(num);
		break;
	}
	case lex::token::floating_point_literal:
	{
		bz::u8string value = stream->value;
		value.erase('\'');

		auto it = value.begin();
		auto const end = value.end();

		double num = 0;
		for (; it != end && *it != '.'; ++it)
		{
			auto const c = *it;
			bz_assert(c >= '0' && c <= '9');
			num *= 10;
			num += c - '0';
		}

		bz_assert(it != end);
		bz_assert(*it == '.');
		++it;

		double level = 1;
		for (; it != end; ++it)
		{
			auto const c = *it;
			bz_assert(c >= '0' && c <= '9');
			level *= 0.1;
			num += level * (c - '0');
		}

		this->value.emplace<floating_point_number>(num);
		break;
	}
	case lex::token::hex_literal:
	{
		bz_assert(stream->value.length() >= 2);
		uint64_t num = 0;
		auto it = stream->value.begin();
		++it, ++it;
		auto const end = stream->value.end();
		for (; it != end; ++it)
		{
			auto const c = *it;
			if (c == '\'')
			{
				continue;
			}
			if (num > std::numeric_limits<uint64_t>::max() / 16)
			{
				// TODO: report error
				bz_assert(false);
			}
			num *= 16;
			if (c >= '0' && c <= '9')
			{
				num += c - '0';
			}
			else if (c >= 'a' && c <= 'f')
			{
				num += c - ('a' - 10);
			}
			else
			{
				bz_assert(c >= 'A' && c <= 'F');
				num += c - ('A' - 10);
			}
		}
		this->value.emplace<integer_number>(num);
		break;
	}
	case lex::token::oct_literal:
	{
		bz_assert(stream->value.length() >= 2);
		uint64_t num = 0;
		auto it = stream->value.begin();
		++it, ++it;
		auto const end = stream->value.end();
		for (; it != end; ++it)
		{
			auto const c = *it;
			if (c == '\'')
			{
				continue;
			}
			if (num > std::numeric_limits<uint64_t>::max() / 8)
			{
				// TODO: report error
				bz_assert(false);
			}
			num = (8 * num) + (c - '0');
		}
		this->value.emplace<integer_number>(num);
		break;
	}
	case lex::token::bin_literal:
	{
		bz_assert(stream->value.length() >= 2);
		uint64_t num = 0;
		auto it = stream->value.begin();
		++it, ++it;
		auto const end = stream->value.end();
		for (; it != end; ++it)
		{
			auto const c = *it;
			if (c == '\'')
			{
				continue;
			}
			if (num > std::numeric_limits<uint64_t>::max() / 2)
			{
				// TODO: report error
				bz_assert(false);
			}
			num = (2 * num) + (c - '0');
		}
		this->value.emplace<integer_number>(num);
		break;
	}

	case lex::token::string_literal:
	{
		auto const str = stream->value;
		bz::u8string res = "";
		res.reserve(str.length());
		auto it = str.begin();
		auto const end = str.end();
		while (it != end)
		{
			res += get_character(it);
		}
		this->value.emplace<string>(std::move(res));
		break;
	}

	case lex::token::character_literal:
	{
		auto const ch = stream->value;
		auto it = ch.begin();
		auto const end = ch.end();
		this->value.emplace<character>(get_character(it));
		bz_assert(it == end);
		break;
	}

	case lex::token::kw_true:
		this->value.emplace<bool_true>();
		break;

	case lex::token::kw_false:
		this->value.emplace<bool_false>();
		break;

	case lex::token::kw_null:
		this->value.emplace<null>();
		break;

	default:
		bz_assert(false);
		break;
	}
}

} // namespace ast
