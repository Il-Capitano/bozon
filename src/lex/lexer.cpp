#include "lexer.h"
#include "escape_sequences.h"

namespace lex
{

static token get_next_token(
	file_iterator &stream,
	ctx::char_pos const end,
	ctx::lex_context &context
);

bz::vector<token> get_tokens(
	bz::u8string_view file,
	uint32_t file_id,
	ctx::lex_context &context
)
{
	bz::vector<token> tokens = {};
	// file.size() / 4  seems like a good estimate of the final token count,
	// uncomment the commented out code in this function to check whether it's really good or not
	tokens.reserve(file.size() / 4);
	// auto const capacity_before = tokens.capacity();
	file_iterator stream = { file.begin(), file_id };
	auto const end = file.end();

	do
	{
		tokens.push_back(get_next_token(stream, end, context));
	} while (tokens.back().kind != token::eof);

	// auto const capacity_after = tokens.capacity();

	// bz::log("{:3}: {:5} bytes, {:4} tokens, {:%}, ({} -> {})\n", file_id, file.size(), tokens.size(), (double)tokens.size() / (double)file.size(), capacity_before, capacity_after);

	return tokens;
}


static constexpr bool is_num_char(bz::u8char c)
{
	return c >= '0' && c <= '9';
}

static constexpr bool is_oct_char(bz::u8char c)
{
	return c >= '0' && c <= '7';
}

static constexpr bool is_bin_char(bz::u8char c)
{
	return c == '0' || c == '1';
}

static constexpr bool is_alpha_char(bz::u8char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static constexpr bool is_alphanum_char(bz::u8char c)
{
	return is_num_char(c) || is_alpha_char(c);
}

static constexpr bool is_identifier_char(bz::u8char c)
{
	return is_alphanum_char(c) || c == '_';
}

static constexpr bool is_whitespace_char(bz::u8char c)
{
	return c == ' '
		|| c == '\t'
		|| c == '\n'
		|| c == '\r';
}


static void skip_block_comment(
	file_iterator &stream, ctx::char_pos const end,
	ctx::lex_context &context
)
{
	auto const comment_begin = stream.it;
	auto const comment_begin_line = stream.line;
	bz_assert(*stream.it == '/');
	bz_assert(*(stream.it + 1) == '*');
	++stream; ++stream; // '/*'
	auto const comment_begin_end = stream.it;

	while (stream.it != end)
	{
		if (stream.it + 1 != end)
		{
			if (*stream.it == '/' && *(stream.it + 1) == '*')
			{
				skip_block_comment(stream, end, context);
				continue;
			}
			else if (*stream.it == '*' && *(stream.it + 1) == '/')
			{
				break;
			}
		}
		++stream;
	}

	if (stream.it == end)
	{
		// report warning
		context.report_warning(
			ctx::warning_kind::unclosed_comment,
			stream.file_id, comment_begin_line,
			comment_begin, comment_begin, comment_begin_end,
			"block comment is never closed"
		);
	}
	else
	{
		++stream; ++stream; // '*/'
	}
}

static void skip_comments_and_whitespace(
	file_iterator &stream, ctx::char_pos const end,
	ctx::lex_context &context
)
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


		skip_comments_and_whitespace(stream, end, context);
		return;
	}

	// block comment
	if (*(stream.it + 1) == '*')
	{
		skip_block_comment(stream, end, context);
		skip_comments_and_whitespace(stream, end, context);
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

	auto const id_value = bz::u8string_view(begin_it, end_it);

#include "keywords.inc"
}

static void match_character(
	file_iterator &stream,
	ctx::char_pos const end,
	ctx::lex_context &context
)
{
	bz_assert(stream.it != end);
	if (*stream.it == '\\')
	{
		++stream;
		verify_escape_sequence(stream, end, context);
	}
	else
	{
		++stream;
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
			{ context.make_note(stream.file_id, line, begin_it, "to match this:") }
		);

		return token(
			token::character_literal,
			bz::u8string_view(char_begin, char_begin),
			stream.file_id, line, begin_it, char_begin
		);
	}

	if (*stream.it == '\'')
	{
		context.bad_char(stream, "expected a character before closing '");
	}
	else
	{
		match_character(stream, end, context);
	}

	auto const char_end = stream.it;
	if (stream.it == end)
	{
		context.bad_eof(
			stream,
			"expected closing ' before end-of-file",
			{ context.make_note(stream.file_id, line, begin_it, "to match this:") }
		);
	}
	else if (*stream.it != '\'')
	{
		context.bad_char(
			stream, "expected closing '",
			{ context.make_note(stream.file_id, line, begin_it, "to match this:", stream.it, "'") }
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
		bz::u8string_view(char_begin, char_end),
		bz::u8string_view(postfix_begin, postfix_end),
		stream.file_id, line, begin_it, end_it
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
		match_character(stream, end, context);
	}

	auto const str_end = stream.it;
	if (stream.it == end)
	{
		context.bad_eof(
			stream,
			"expected closing \" before end-of-file",
			{ context.make_note(stream.file_id, line, begin_it, "to match this:") }
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
		bz::u8string_view(str_begin, str_end),
		bz::u8string_view(postfix_begin, postfix_end),
		stream.file_id, line, begin_it, end_it
	);
}

static token get_raw_string_token(
	file_iterator &stream,
	ctx::char_pos const end,
	ctx::lex_context &context
)
{
	bz_assert(stream.it != end);
	bz_assert(*stream.it == '\"');
	bz_assert(stream.it + 1 != end);
	bz_assert(*(stream.it + 1) == '\"');
	bz_assert(stream.it + 2 != end);
	bz_assert(*(stream.it + 2) == '\"');
	auto const begin_it = stream.it;
	auto const line = stream.line;
	++stream; ++stream; ++stream;
	auto const str_begin = stream.it;

	while (stream.it != end && !(
		*stream.it == '\"'
		&& stream.it + 1 != end && *(stream.it + 1) == '\"'
		&& stream.it + 2 != end && *(stream.it + 2) == '\"'
	))
	{
		++stream;
	}

	auto const str_end = stream.it;
	if (stream.it == end)
	{
		context.bad_eof(
			stream,
			"expected closing \"\"\" before end-of-file",
			{ context.make_note(stream.file_id, line, begin_it, begin_it, str_begin, "to match this:") }
		);
	}
	else
	{
		// bz_assert(stream.it != end);
		bz_assert(*stream.it == '\"');
		bz_assert(stream.it + 1 != end);
		bz_assert(*(stream.it + 1) == '\"');
		bz_assert(stream.it + 2 != end);
		bz_assert(*(stream.it + 2) == '\"');
		++stream; ++stream; ++stream;
	}

	auto const postfix_begin = stream.it;
	if (stream.it != end && (is_alpha_char(*stream.it) || *stream.it == '_')) do
	{
		++stream;
	} while (stream.it != end && is_identifier_char(*stream.it));
	auto const postfix_end = stream.it;

	auto const end_it = stream.it;

	return token(
		token::raw_string_literal,
		bz::u8string_view(str_begin, str_end),
		bz::u8string_view(postfix_begin, postfix_end),
		stream.file_id, line, begin_it, end_it
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
		bz::u8string_view(num_begin, num_end),
		bz::u8string_view(postfix_begin, postfix_end),
		stream.file_id, line, begin_it, end_it
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
		bz::u8string_view(num_begin, num_end),
		bz::u8string_view(postfix_begin, postfix_end),
		stream.file_id, line, begin_it, end_it
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
		bz::u8string_view(num_begin, num_end),
		bz::u8string_view(postfix_begin, postfix_end),
		stream.file_id, line, begin_it, end_it
	);
}

static std::pair<bz::u8string_view, uint32_t> get_number_token_str_and_kind(
	file_iterator &stream,
	ctx::char_pos const end
)
{
	auto const num_begin = stream.it;

	do
	{
		++stream;
	} while (stream.it != end && (is_num_char(*stream.it) || *stream.it == '\''));

	uint32_t token_kind = 0;

	if (
		stream.it == end
		|| *stream.it != '.'
		// the next char after the '.' has to be a number to count towards the token
		|| (stream.it + 1) == end
		|| !is_num_char(*(stream.it + 1))
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

	auto const has_exponent = [&]() {
		auto it = stream.it;
		if (it == end || (*it != 'e' && *it != 'E'))
		{
			return false;
		}

		++it;
		if (it == end)
		{
			return false;
		}

		auto const c = *it;
		if (is_num_char(c))
		{
			return true;
		}
		if (c != '+' && c != '-')
		{
			return false;
		}

		++it;
		if (it == end)
		{
			return false;
		}
		auto const c2 = *it;
		if (is_num_char(c2))
		{
			return true;
		}
		else
		{
			return false;
		}
	};

	// exponential notation
	// e.g. 1.4e3  1.0e+234  1e-99
	if (has_exponent())
	{
		token_kind = token::floating_point_literal;
		++stream; ++stream;  // gets rid of the 'e' and +- or a number (since there has to be at least one)
		// ' is not allowed in the exponent part
		while (stream.it != end && is_num_char(*stream.it))
		{
			++stream;
		}
	}

	auto const num_end = stream.it;

	return { bz::u8string_view(num_begin, num_end), token_kind };
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
	auto const line = stream.line;

	auto const [num_str, token_kind] = get_number_token_str_and_kind(stream, end);

	auto const postfix_begin = stream.it;
	if (stream.it != end && (is_alpha_char(*stream.it) || *stream.it == '_')) do
	{
		++stream;
	} while (stream.it != end && is_identifier_char(*stream.it));
	auto const postfix_end = stream.it;

	auto const end_it = stream.it;

	return token(
		token_kind,
		num_str,
		bz::u8string_view(postfix_begin, postfix_end),
		stream.file_id, line, begin_it, end_it
	);
}

static token make_regular_token(
	uint32_t kind,
	ctx::char_pos begin_it,
	ctx::char_pos end_it,
	uint32_t file_id,
	uint32_t line,
	ctx::lex_context &
)
{
	bz_assert(*begin_it <= 127);
	return token(
		kind,
		bz::u8string_view(begin_it, end_it),
		file_id, line, begin_it, end_it
	);
}

static token get_single_char_token(
	file_iterator &stream,
	ctx::char_pos const end,
	ctx::lex_context &context
)
{
	bz_assert(stream.it != end);
	auto const begin_it = stream.it;
	auto const char_value = *begin_it;
	auto const line     = stream.line;
	++stream;
	auto const end_it = stream.it;

	// ascii
	if (char_value <= 127)
	{
		return token(
			static_cast<uint32_t>(char_value),
			bz::u8string_view(begin_it, end_it),
			stream.file_id, line, begin_it, end_it
		);
	}
	// non-ascii
	else
	{
		// U+037E: ';' greek question mark
		if (char_value == 0x037e)
		{
			context.report_warning(
				ctx::warning_kind::greek_question_mark,
				stream.file_id, line, begin_it,
				"unicode character U+037E (greek question mark) looks the same as a semicolon, but it is not"
			);
		}
		return token(
			token::non_ascii_character,
			bz::u8string_view(begin_it, end_it),
			stream.file_id, line, begin_it, end_it
		);
	}
}

static token get_next_token(
	file_iterator &stream,
	ctx::char_pos const end,
	ctx::lex_context &context
)
{
	skip_comments_and_whitespace(stream, end, context);

	if (stream.it == end)
	{
		return token(
			token::eof,
			bz::u8string_view(end, end),
			stream.file_id, stream.line, end, end
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
		if (stream.it + 1 != end && *(stream.it + 1) == '\"' && stream.it + 2 != end && *(stream.it + 2) == '\"')
		{
			return get_raw_string_token(stream, end, context);
		}
		else
		{
			return get_string_token(stream, end, context);
		}
	// number
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		return get_number_token(stream, end, context);

#include "regular_tokens.inc"

	default:
		break;
	}

	return get_single_char_token(stream, end, context);
}

} // namespace lex
