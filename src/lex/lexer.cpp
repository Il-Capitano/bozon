#include "lexer.h"
#include "colors.h"

namespace lex
{

static token get_next_token(
	file_iterator &stream,
	ctx::char_pos const end,
	ctx::lex_context &context
);

bz::vector<token> get_tokens(
	bz::string_view file,
	bz::string_view file_name,
	ctx::lex_context &context
)
{
	bz::vector<token> tokens = {};
	file_iterator stream = { file.begin(), file_name };
	auto const end = file.end();

	do
	{
		tokens.push_back(get_next_token(stream, end, context));
	} while (tokens.back().kind != token::eof);

	return tokens;
}



static constexpr bool is_num_char(char c)
{
	return c >= '0' && c <= '9';
}

static constexpr bool is_hex_char(char c)
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static constexpr bool is_oct_char(char c)
{
	return c >= '0' && c <= '7';
}

static constexpr bool is_bin_char(char c)
{
	return c == '0' || c == '1';
}

static constexpr bool is_alpha_char(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static constexpr bool is_alphanum_char(char c)
{
	return is_num_char(c) || is_alpha_char(c);
}

static constexpr bool is_identifier_char(char c)
{
	return is_alphanum_char(c) || c == '_';
}

static constexpr bool is_whitespace_char(char c)
{
	return c == ' '
		|| c == '\t'
		|| c == '\n'
		|| c == '\r';
}


static void skip_comments_and_whitespace(file_iterator &stream, ctx::char_pos const end)
{
	while (stream.it != end && is_whitespace_char(*stream.it))
	{
		++stream;
	}

	if (stream.it == end || stream.it + 1 == end || *stream.it != '/')
	{
		return;
	}

	// line comment
	if (*(stream.it + 1) == '/')
	{
		++stream; ++stream; // '//'

		while (stream.it != end && *stream.it != '\n')
		{
			++stream;
		}


		skip_comments_and_whitespace(stream, end);
		return;
	}

	// block comment
	if (*(stream.it + 1) == '*')
	{
		++stream; ++stream; // '/*'
		int comment_depth = 1;

		while (stream.it != end && comment_depth != 0)
		{
			if (stream.it + 1 != end)
			{
				if (*stream.it == '/' && *(stream.it + 1) == '*')
				{
					++stream; ++stream; // '/*'
					++comment_depth;
					continue;
				}
				else if (*stream.it == '*' && *(stream.it + 1) == '/')
				{
					++stream; ++stream; // '*/'
					--comment_depth;
					continue;
				}
			}
			++stream;
		}

		skip_comments_and_whitespace(stream, end);
		return;
	}
}

static token get_identifier_or_keyword_token(
	file_iterator &stream,
	ctx::char_pos const end,
	ctx::lex_context &
)
{
	bz_assert(stream.it != end);
	bz_assert(
		is_alpha_char(*stream.it)
		|| *stream.it == '_'
	);

	auto const begin_it = stream.it;
	auto const line     = stream.line;

	do
	{
		++stream;
	} while(stream.it != end && is_identifier_char(*stream.it));

	auto const end_it = stream.it;

	auto const id_value = bz::string_view(begin_it, end_it);

	auto it = std::find_if(
		keywords.begin(),
		keywords.end(),
		[id_value](auto const &kw)
		{
			return kw.first == id_value;
		}
	);

	// identifier
	if (it == keywords.end())
	{
		return token(
			token::identifier,
			id_value,
			stream.file, begin_it, end_it, line
		);
	}
	// keyword
	else
	{
		return token(
			it->second,
			id_value,
			stream.file, begin_it, end_it, line
		);
	}
}

static token get_character_token(
	file_iterator &stream,
	ctx::char_pos const end,
	ctx::lex_context &context
)
{
	bz_assert(stream.it != end);
	bz_assert(*stream.it == '\'');
	auto const begin_it = stream.it;
	auto const line     = stream.line;
	++stream;
	auto const char_begin = stream.it;

	if (stream.it == end)
	{
		context.bad_eof(
			stream,
			"expected a character and closing ' before end-of-file",
			{ ctx::note{
				stream.file, line,
				begin_it, begin_it, begin_it + 1,
				"to match this:"
			} }
		);

		return token(
			token::character_literal,
			bz::string_view(char_begin, char_begin),
			stream.file, begin_it, char_begin, line
		);
	}

	switch (*stream.it)
	{
	case '\\':
		++stream;
		if (stream.it == end)
		{
			break;
		}
		switch (*stream.it)
		{
		// TODO: decide on what is allowed here
		case '\\':
		case '\'':
		case '\"':
		case 'n':
		case 't':
			++stream;
			break;
		default:
			context.bad_chars(
				stream.file, stream.line,
				stream.it - 1, stream.it - 1, stream.it + 1,
				*stream.it >= ' '
				? bz::format("invalid escape sequence '\\{}'", *stream.it)
				: bz::format(
					"invalid escape sequence '\\{}\\x{:02x}{}'",
					colors::bright_black,
					static_cast<uint32_t>(*stream.it),
					colors::clear
				)
			);
			++stream;
			break;
		}
		break;

	case '\'':
		context.bad_char(stream, "expected a character before closing '");
		break;

	default:
		++stream;
		break;
	}

	auto const char_end = stream.it;
	if (stream.it == end)
	{
		context.bad_eof(
			stream,
			"expected closing ' before end-of-file",
			{ ctx::note{
				stream.file, line,
				begin_it, begin_it, begin_it + 1,
				"to match this:"
			} }
		);
	}
	else if (*stream.it != '\'')
	{
		context.bad_char(
			stream, "expected closing '",
			{ ctx::note{
				stream.file, line,
				begin_it, begin_it, begin_it + 1,
				"to match this:"
			} },
			{ ctx::suggestion{
				stream.file, stream.line,
				stream.it, "'",
				"put ' here:"
			} }
		);
	}
	else
	{
		++stream;
	}

	auto const postfix_begin = stream.it;
	if (stream.it != end && (is_alpha_char(*stream.it) || *stream.it == '_')) do
	{
		++stream;
	} while (stream.it != end && is_identifier_char(*stream.it));
	auto const postfix_end = stream.it;

	auto const end_it = stream.it;

	return token(
		token::character_literal,
		bz::string_view(char_begin, char_end),
		bz::string_view(postfix_begin, postfix_end),
		stream.file, begin_it, end_it, line
	);
}

static token get_string_token(
	file_iterator &stream,
	ctx::char_pos const end,
	ctx::lex_context &context
)
{
	bz_assert(stream.it != end);
	bz_assert(*stream.it == '\"');
	auto const begin_it = stream.it;
	auto const line     = stream.line;
	++stream;
	auto const str_begin = stream.it;

	while (stream.it != end && *stream.it != '\"')
	{
		if (*stream.it == '\\')
		{
			++stream;
			if (stream.it == end)
			{
				break;
			}
			switch (*stream.it)
			{
			// TODO: decide on what is allowed here
			case '\\':
			case '\'':
			case '\"':
			case 'n':
			case 't':
				++stream;
				break;
			default:
				context.bad_chars(
					stream.file, stream.line,
					stream.it - 1, stream.it - 1, stream.it + 1,
					*stream.it >= ' '
					? bz::format("invalid escape sequence '\\{}'", *stream.it)
					: bz::format(
						"invalid escape sequence '\\{}\\x{:02x}{}'",
						colors::bright_black,
						static_cast<uint32_t>(*stream.it),
						colors::clear
					)
				);
				++stream;
				break;
			}
		}
		else
		{
			++stream;
		}
	}

	auto const str_end = stream.it;
	if (stream.it == end)
	{
		context.bad_eof(
			stream,
			"expected closing \" before end-of-file",
			{ ctx::note{
				stream.file, line,
				begin_it, begin_it, begin_it + 1,
				"to match this:"
			} }
		);
	}
	else
	{
		bz_assert(*stream.it == '\"');
		++stream;
	}

	auto const postfix_begin = stream.it;
	if (stream.it != end && (is_alpha_char(*stream.it) || *stream.it == '_')) do
	{
		++stream;
	} while (stream.it != end && is_identifier_char(*stream.it));
	auto const postfix_end = stream.it;

	auto const end_it = stream.it;

	return token(
		token::string_literal,
		bz::string_view(str_begin, str_end),
		bz::string_view(postfix_begin, postfix_end),
		stream.file, begin_it, end_it, line
	);
}

static token get_hex_number_token(
	file_iterator &stream,
	ctx::char_pos const end
)
{
	bz_assert(stream.it != end);
	bz_assert((stream.it + 1) != end);
	bz_assert(*stream.it == '0');
	bz_assert(*(stream.it + 1) == 'x' || *(stream.it + 1) == 'X');

	auto const begin_it = stream.it;
	auto const num_begin = stream.it;
	auto const line = stream.line;

	++stream; ++stream; // '0x' or '0X'

	while (stream.it != end && (is_hex_char(*stream.it) || *stream.it == '\''))
	{
		++stream;
	}

	auto const num_end = stream.it;

	auto const postfix_begin = stream.it;
	if (stream.it != end && (is_alpha_char(*stream.it) || *stream.it == '_')) do
	{
		++stream;
	} while (stream.it != end && is_identifier_char(*stream.it));
	auto const postfix_end = stream.it;

	auto const end_it = stream.it;

	return token(
		token::hex_literal,
		bz::string_view(num_begin, num_end),
		bz::string_view(postfix_begin, postfix_end),
		stream.file, begin_it, end_it, line
	);
}

static token get_oct_number_token(
	file_iterator &stream,
	ctx::char_pos const end
)
{
	bz_assert(stream.it != end);
	bz_assert((stream.it + 1) != end);
	bz_assert(*stream.it == '0');
	bz_assert(*(stream.it + 1) == 'o' || *(stream.it + 1) == 'O');

	auto const begin_it = stream.it;
	auto const num_begin = stream.it;
	auto const line = stream.line;

	++stream; ++stream; // '0o' or '0O'

	while (stream.it != end && (is_oct_char(*stream.it) || *stream.it == '\''))
	{
		++stream;
	}

	auto const num_end = stream.it;

	auto const postfix_begin = stream.it;
	if (stream.it != end && (is_alpha_char(*stream.it) || *stream.it == '_')) do
	{
		++stream;
	} while (stream.it != end && is_identifier_char(*stream.it));
	auto const postfix_end = stream.it;

	auto const end_it = stream.it;

	return token(
		token::oct_literal,
		bz::string_view(num_begin, num_end),
		bz::string_view(postfix_begin, postfix_end),
		stream.file, begin_it, end_it, line
	);
}

static token get_bin_number_token(
	file_iterator &stream,
	ctx::char_pos const end
)
{
	bz_assert(stream.it != end);
	bz_assert((stream.it + 1) != end);
	bz_assert(*stream.it == '0');
	bz_assert(*(stream.it + 1) == 'b' || *(stream.it + 1) == 'B');

	auto const begin_it = stream.it;
	auto const num_begin = stream.it;
	auto const line = stream.line;

	++stream; ++stream; // '0b' or '0B'

	while (stream.it != end && (is_bin_char(*stream.it) || *stream.it == '\''))
	{
		++stream;
	}

	auto const num_end = stream.it;

	auto const postfix_begin = stream.it;
	if (stream.it != end && (is_alpha_char(*stream.it) || *stream.it == '_')) do
	{
		++stream;
	} while (stream.it != end && is_identifier_char(*stream.it));
	auto const postfix_end = stream.it;

	auto const end_it = stream.it;

	return token(
		token::bin_literal,
		bz::string_view(num_begin, num_end),
		bz::string_view(postfix_begin, postfix_end),
		stream.file, begin_it, end_it, line
	);
}

static token get_number_token(
	file_iterator &stream,
	ctx::char_pos const end,
	ctx::lex_context &
)
{
	bz_assert(stream.it != end);
	bz_assert(is_num_char(*stream.it));

	if (
		*stream.it == '0'
		&& (stream.it + 1) != end
	)
	{
		switch (*(stream.it + 1))
		{
		case 'x':
		case 'X':
			return get_hex_number_token(stream, end);
		case 'o':
		case 'O':
			return get_oct_number_token(stream, end);
		case 'b':
		case 'B':
			return get_bin_number_token(stream, end);
		default:
			// continue getting a regular decimal number
			break;
		}
	}


	auto const begin_it = stream.it;
	auto const num_begin = stream.it;
	auto const line = stream.line;

	do
	{
		++stream;
	} while (stream.it != end && (is_num_char(*stream.it) || *stream.it == '\''));

	uint32_t token_kind = 0;

	if (
		stream.it == end
		|| *stream.it != '.'
		// the next char after the '.' has to be a number or '\'' to count towards the token
		|| (stream.it + 1) == end
		|| !(is_num_char(*(stream.it + 1)) || *(stream.it + 1) == '\'')
	)
	{
		token_kind = token::integer_literal;
	}
	else
	{
		do
		{
			++stream;
		} while (stream.it != end && (is_num_char(*stream.it) || *stream.it == '\''));
		token_kind = token::floating_point_literal;
	}

	auto const num_end = stream.it;

	auto const postfix_begin = stream.it;
	if (stream.it != end && (is_alpha_char(*stream.it) || *stream.it == '_')) do
	{
		++stream;
	} while (stream.it != end && is_identifier_char(*stream.it));
	auto const postfix_end = stream.it;

	auto const end_it = stream.it;

	return token(
		token_kind,
		bz::string_view(num_begin, num_end),
		bz::string_view(postfix_begin, postfix_end),
		stream.file, begin_it, end_it, line
	);

	// TODO: allow exponential notations (1e10)
}

static token get_single_char_token(
	file_iterator &stream,
	ctx::char_pos const end,
	ctx::lex_context &
)
{
	bz_assert(stream.it != end);
	auto const begin_it = stream.it;
	auto const line     = stream.line;
	++stream;
	auto const end_it = stream.it;

	return token(
		static_cast<uint32_t>(*begin_it),
		bz::string_view(begin_it, end_it),
		stream.file, begin_it, end_it, line
	);
}

static bool is_str(bz::string_view str, file_iterator &stream, ctx::char_pos const end)
{
	auto str_it = str.begin();
	auto const str_end = str.end();
	auto it = stream.it;

	while (it != end && str_it != str_end && *it == *str_it)
	{
		++it, ++str_it;
	}

	return str_it == str_end;
}

static token get_next_token(
	file_iterator &stream,
	ctx::char_pos const end,
	ctx::lex_context &context
)
{
	skip_comments_and_whitespace(stream, end);

	if (stream.it == end)
	{
		return token(
			token::eof,
			bz::string_view(end, end),
			stream.file, end, end, stream.line
		);
	}

	switch (*stream.it)
	{
	// identifier or keyword
	case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h':
	case 'i': case 'j': case 'k': case 'l': case 'm': case 'n': case 'o': case 'p':
	case 'q': case 'r': case 's': case 't': case 'u': case 'v': case 'w': case 'x':
	case 'y': case 'z':
	case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H':
	case 'I': case 'J': case 'K': case 'L': case 'M': case 'N': case 'O': case 'P':
	case 'Q': case 'R': case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
	case 'Y': case 'Z':
	case '_':
		return get_identifier_or_keyword_token(stream, end, context);

	// character
	case '\'':
		return get_character_token(stream, end, context);
	// string
	case '\"':
		return get_string_token(stream, end, context);
	// number
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		return get_number_token(stream, end, context);

	default:
		break;
	}

	for (auto &t : multi_char_tokens)
	{
		if (is_str(t.first, stream, end))
		{
			auto const begin_it = stream.it;
			auto const line     = stream.line;

			for (size_t i = 0; i < t.first.length(); ++i)
			{
				++stream;
			}

			auto const end_it = stream.it;
			return token(
				t.second,
				bz::string_view(begin_it, end_it),
				stream.file, begin_it, end_it, line
			);
		}
	}

	return get_single_char_token(stream, end, context);
}

} // namespace lex
