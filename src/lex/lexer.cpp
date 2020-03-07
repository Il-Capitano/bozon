#include "lexer.h"

namespace lex
{

struct file_iterator
{
	ctx::char_pos it;
	bz::string_view file;
	size_t line = 1;
	size_t column = 1;

	file_iterator &operator ++ (void)
	{
		if (*it == '\n')
		{
			++this->line;
			this->column = 1;
		}
		else
		{
			++this->column;
		}
		++this->it;
		return *this;
	}
};


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

[[nodiscard]] static ctx::error bad_char(
	file_iterator const &stream,
	bz::string message, bz::vector<ctx::note> notes = {}
)
{
	return ctx::error{
		stream.file, stream.line, stream.column,
		stream.it, stream.it, stream.it + 1,
		std::move(message),
		std::move(notes), {}
	};
}

[[nodiscard]] static ctx::error bad_char(
	bz::string_view file, size_t line, size_t column,
	bz::string message, bz::vector<ctx::note> notes = {}
)
{
	return ctx::error{
		file, line, column,
		nullptr, nullptr, nullptr,
		std::move(message),
		std::move(notes), {}
	};
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
	assert(stream.it != end);
	assert(
		is_alpha_char(*stream.it)
		|| *stream.it == '_'
	);

	auto const begin_it = stream.it;
	auto const line     = stream.line;
	auto const column   = stream.column;

	do
	{
		++stream;
	} while(stream.it != end && is_identifier_char(*stream.it));

	auto const end_it = stream.it;

	auto const id_value = bz::string_view(&*begin_it, &*end_it);

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
			stream.file, begin_it, end_it, line, column
		);
	}
	// keyword
	else
	{
		return token(
			it->second,
			id_value,
			stream.file, begin_it, end_it, line, column
		);
	}
}

static token get_character_token(
	file_iterator &stream,
	ctx::char_pos const end,
	ctx::lex_context &context
)
{
	assert(stream.it != end);
	assert(*stream.it == '\'');
	auto const begin_it = stream.it;
	auto const line     = stream.line;
	auto const column   = stream.column;
	++stream;
	auto const char_begin = stream.it;

	if (stream.it == end)
	{
		context.report_error(bad_char(
			stream.file, stream.line, stream.column,
			"expected closing ' before end-of-file",
			{ ctx::note{
				stream.file, line, column,
				begin_it, begin_it, begin_it + 1,
				"to match this:"
			} }
		));

		return token(
			token::character_literal,
			bz::string_view(&*char_begin, &*char_begin),
			stream.file, begin_it, char_begin, line, column
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
			context.report_error(bad_char(
				stream, bz::format("invalid escape sequence '\\{}'", *stream.it)
			));
			++stream;
			break;
		}
		break;

	case '\'':
		context.report_error(bad_char(stream, "expected a character before closing '"));
		break;

	default:
		++stream;
		break;
	}

	auto const char_end = stream.it;
	if (stream.it == end)
	{
		context.report_error(bad_char(
			stream.file, stream.line, stream.column,
			"expected closing ' before end-of-file",
			{ ctx::note{
				stream.file, line, column,
				begin_it, begin_it, begin_it + 1,
				"to match this:"
			} }
		));
	}
	else if (*stream.it != '\'')
	{
		context.report_error(bad_char(
			stream, "expected closing '",
			{ ctx::note{
				stream.file, line, column,
				begin_it, begin_it, begin_it + 1,
				"to match this:"
			} }
		));
	}
	else
	{
		++stream;
	}
	auto const end_it = stream.it;

	return token(
		token::character_literal,
		bz::string_view(&*char_begin, &*char_end),
		stream.file, begin_it, end_it, line, column
	);
}

static token get_string_token(
	file_iterator &stream,
	ctx::char_pos const end,
	ctx::lex_context &context
)
{
	assert(stream.it != end);
	assert(*stream.it == '\"');
	auto const begin_it = stream.it;
	auto const line     = stream.line;
	auto const column   = stream.column;
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
				context.report_error(bad_char(
					stream, bz::format("invalid escape sequence '\\{}'", *stream.it)
				));
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
		context.report_error(bad_char(
			stream.file, stream.line, stream.column,
			"expected closing \" before end-of-file",
			{ ctx::note{
				stream.file, line, column,
				begin_it, begin_it, begin_it + 1,
				"to match this:"
			} }
		));
	}
	else
	{
		assert(*stream.it == '\"');
		++stream;
	}
	auto const end_it = stream.it;

	return token(
		token::string_literal,
		bz::string_view(&*str_begin, &*str_end),
		stream.file, begin_it, end_it, line, column
	);
}

static token get_number_token(
	file_iterator &stream,
	ctx::char_pos const end,
	ctx::lex_context &
)
{
	assert(stream.it != end);
	assert(is_num_char(*stream.it));
	auto const begin_it = stream.it;
	auto const line     = stream.line;
	auto const column   = stream.column;

	do
	{
		++stream;
	} while (stream.it != end && (is_num_char(*stream.it) || *stream.it == '\''));

	if (stream.it == end || *stream.it != '.')
	{
		return token(
			token::number_literal,
			bz::string_view(&*begin_it, &*stream.it),
			stream.file, begin_it, stream.it, line, column
		);
	}

	// the next char after the '.' has to be a number or '\'' to count towards the token
	if ((stream.it + 1) == end || !(is_num_char(*(stream.it + 1)) || *(stream.it + 1) == '\''))
	{
		return token(
			token::number_literal,
			bz::string_view(&*begin_it, &*stream.it),
			stream.file, begin_it, stream.it, line, column
		);
	}

	do
	{
		++stream;
	} while (stream.it != end && (is_num_char(*stream.it) || *stream.it == '\''));

	auto const end_it = stream.it;
	return token(
		token::number_literal,
		bz::string_view(&*begin_it, &*end_it),
		stream.file, begin_it, end_it, line, column
	);

	// TODO: allow hex, oct and bin numbers (0x, 0o, 0b) and exponential notations (1e10)
}

static token get_single_char_token(
	file_iterator &stream,
	ctx::char_pos const end,
	ctx::lex_context &
)
{
	assert(stream.it != end);
	auto const begin_it = stream.it;
	auto const line     = stream.line;
	auto const column   = stream.column;
	++stream;
	auto const end_it = stream.it;

	return token(
		static_cast<uint32_t>(*begin_it),
		bz::string_view(&*begin_it, &*end_it),
		stream.file, begin_it, end_it, line, column
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
			bz::string_view(&*end, &*end),
			stream.file, end, end, stream.line, stream.column
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
			auto const column   = stream.column;

			for (size_t i = 0; i < t.first.length(); ++i)
			{
				++stream;
			}

			auto const end_it = stream.it;
			return token(
				t.second,
				bz::string_view(&*begin_it, &*end_it),
				stream.file, begin_it, end_it, line, column
			);
		}
	}

	return get_single_char_token(stream, end, context);
}

} // namespace lex
