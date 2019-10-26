#include "lexer.h"

static std::array<
	std::pair<intern_string, uint32_t>,
	token::kw_if - token::plus_plus
> multi_char_tokens;

static std::array<
	std::pair<intern_string, uint32_t>,
	token::_last - token::kw_if
> keywords;


void lexer_init(void)
{
	multi_char_tokens =
	{
		std::make_pair( "<<="_is, token::bit_left_shift_eq  ),
		std::make_pair( ">>="_is, token::bit_right_shift_eq ),
		std::make_pair( "..."_is, token::dot_dot_dot        ),
		std::make_pair( "..="_is, token::dot_dot_eq         ),

		std::make_pair( "++"_is, token::plus_plus           ),
		std::make_pair( "--"_is, token::minus_minus         ),
		std::make_pair( "+="_is, token::plus_eq             ),
		std::make_pair( "-="_is, token::minus_eq            ),
		std::make_pair( "*="_is, token::multiply_eq         ),
		std::make_pair( "/="_is, token::divide_eq           ),
		std::make_pair( "%="_is, token::modulo_eq           ),
		std::make_pair( "<<"_is, token::bit_left_shift      ),
		std::make_pair( ">>"_is, token::bit_right_shift     ),
		std::make_pair( "&="_is, token::bit_and_eq          ),
		std::make_pair( "|="_is, token::bit_or_eq           ),
		std::make_pair( "^="_is, token::bit_xor_eq          ),

		std::make_pair( "=="_is, token::equals              ),
		std::make_pair( "!="_is, token::not_equals          ),
		std::make_pair( "<="_is, token::less_than_eq        ),
		std::make_pair( ">="_is, token::greater_than_eq     ),
		std::make_pair( "&&"_is, token::bool_and            ),
		std::make_pair( "||"_is, token::bool_or             ),
		std::make_pair( "^^"_is, token::bool_xor            ),

		std::make_pair( "->"_is, token::arrow               ),
		std::make_pair( "::"_is, token::scope               ),
		std::make_pair( ".."_is, token::dot_dot             ),
	};

	keywords =
	{
		std::make_pair( "namespace"_is, token::kw_namespace ),

		std::make_pair( "function"_is, token::kw_function   ),
		std::make_pair( "operator"_is, token::kw_operator   ),
		std::make_pair( "typename"_is, token::kw_typename   ),

		std::make_pair( "return"_is, token::kw_return       ),
		std::make_pair( "struct"_is, token::kw_struct       ),
		std::make_pair( "sizeof"_is, token::kw_sizeof       ),
		std::make_pair( "typeof"_is, token::kw_typeof       ),

		std::make_pair( "while"_is, token::kw_while         ),
		std::make_pair( "class"_is, token::kw_class         ),
		std::make_pair( "using"_is, token::kw_using         ),
		std::make_pair( "const"_is, token::kw_const         ),
		std::make_pair( "false"_is, token::kw_false         ),

		std::make_pair( "else"_is, token::kw_else           ),
		std::make_pair( "auto"_is, token::kw_auto           ),
		std::make_pair( "true"_is, token::kw_true           ),
		std::make_pair( "null"_is, token::kw_null           ),

		std::make_pair( "for"_is, token::kw_for             ),
		std::make_pair( "let"_is, token::kw_let             ),

		std::make_pair( "if"_is, token::kw_if               ),
	};
}


bz::string get_token_value(uint32_t kind)
{
	switch (kind)
	{
	case token::identifier:
		return "identifier";
	case token::number_literal:
		return "number literal";
	case token::string_literal:
		return "string literal";
	case token::character_literal:
		return "character literal";

	default:
		break;
	}

	auto multi_char_it = std::find_if(
		multi_char_tokens.begin(),
		multi_char_tokens.end(),
		[&](auto const &p) {
			return p.second == kind;
		}
	);
	if (multi_char_it != multi_char_tokens.end())
	{
		return multi_char_it->first;
	}

	auto keyword_it = std::find_if(keywords.begin(), keywords.end(), [&](auto const &p)
	{
		return p.second == kind;
	});

	if (keyword_it != keywords.end())
	{
		return keyword_it->first;
	}

	return bz::string(1, (char)kind);
}

bool is_operator(uint32_t kind)
{
	switch (kind)
	{
	case token::assign:             // '='         binary
	case token::plus:               // '+'   unary/binary
	case token::plus_eq:            // '+='        binary
	case token::minus:              // '-'   unary/binary
	case token::minus_eq:           // '-='        binary
	case token::multiply:           // '*'   unary/binary
	case token::multiply_eq:        // '*='        binary
	case token::divide:             // '/'         binary
	case token::divide_eq:          // '/='        binary
	case token::modulo:             // '%'         binary
	case token::modulo_eq:          // '%='        binary
	case token::comma:              // ','         binary
	case token::dot_dot:            // '..'        binary
	case token::dot_dot_eq:         // '..='       binary
	case token::equals:             // '=='        binary
	case token::not_equals:         // '!='        binary
	case token::less_than:          // '<'         binary
	case token::less_than_eq:       // '<='        binary
	case token::greater_than:       // '>'         binary
	case token::greater_than_eq:    // '>='        binary
	case token::bit_and:            // '&'   unary/binary
	case token::bit_and_eq:         // '&='        binary
	case token::bit_xor:            // '^'         binary
	case token::bit_xor_eq:         // '^='        binary
	case token::bit_or:             // '|'         binary
	case token::bit_or_eq:          // '|='        binary
	case token::bit_not:            // '~'   unary
	case token::bit_left_shift:     // '<<'        binary
	case token::bit_left_shift_eq:  // '<<='       binary
	case token::bit_right_shift:    // '>>'        binary
	case token::bit_right_shift_eq: // '>>='       binary
	case token::bool_and:           // '&&'        binary
	case token::bool_xor:           // '^^'        binary
	case token::bool_or:            // '||'        binary
	case token::bool_not:           // '!'   unary
	case token::plus_plus:          // '++'  unary
	case token::minus_minus:        // '--'  unary
	case token::dot:                // '.'         binary
	case token::arrow:              // '->'        binary
	case token::scope:              // '::'        binary
	case token::dot_dot_dot:        // '...' unary
	case token::kw_sizeof:          // 'sizeof' unary
	case token::kw_typeof:          // 'typeof' unary
	case token::question_mark:      // '?' ternary
	case token::colon:              // ':' ternary and types
		return true;

	default:
		return false;
	}
}

bool is_overloadable_operator(uint32_t kind)
{
	switch (kind)
	{
	case token::assign:             // '='         binary
	case token::plus:               // '+'   unary/binary
	case token::plus_eq:            // '+='        binary
	case token::minus:              // '-'   unary/binary
	case token::minus_eq:           // '-='        binary
	case token::multiply:           // '*'   unary/binary
	case token::multiply_eq:        // '*='        binary
	case token::divide:             // '/'         binary
	case token::divide_eq:          // '/='        binary
	case token::modulo:             // '%'         binary
	case token::modulo_eq:          // '%='        binary
	case token::dot_dot:            // '..'        binary
	case token::dot_dot_eq:         // '..='       binary
	case token::equals:             // '=='        binary
	case token::not_equals:         // '!='        binary
	case token::less_than:          // '<'         binary
	case token::less_than_eq:       // '<='        binary
	case token::greater_than:       // '>'         binary
	case token::greater_than_eq:    // '>='        binary
	case token::bit_and:            // '&'   unary/binary
	case token::bit_and_eq:         // '&='        binary
	case token::bit_xor:            // '^'         binary
	case token::bit_xor_eq:         // '^='        binary
	case token::bit_or:             // '|'         binary
	case token::bit_or_eq:          // '|='        binary
	case token::bit_not:            // '~'   unary
	case token::bit_left_shift:     // '<<'        binary
	case token::bit_left_shift_eq:  // '<<='       binary
	case token::bit_right_shift:    // '>>'        binary
	case token::bit_right_shift_eq: // '>>='       binary
	case token::bool_and:           // '&&'        binary
	case token::bool_xor:           // '^^'        binary
	case token::bool_or:            // '||'        binary
	case token::bool_not:           // '!'   unary
	case token::plus_plus:          // '++'  unary
	case token::minus_minus:        // '--'  unary
	case token::arrow:              // '->'        binary
	case token::square_open:        // '[]'        binary
	case token::paren_open:         // function call
		return true;

	default:
		return false;
	}
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

static constexpr bool is_whitespace_char(char c)
{
	return c == ' '
		|| c == '\t'
		|| c == '\n'
		|| c == '\r'; // for windows line ends
}

static bool is_identifier(src_file::pos stream)
{
	return is_alpha_char(*stream) || *stream == '_';
}

static bool is_string_literal(src_file::pos stream)
{
	return *stream == '"';
}

static bool is_character_literal(src_file::pos stream)
{
	return *stream == '\'';
}

static bool is_number_literal(src_file::pos stream)
{
	if (*stream == '.')
	{
		// we can do this because the stream is null terminated
		return is_num_char(*++stream);
	}
	else
	{
		return is_num_char(*stream);
	}
}

static bool is_string(src_file::pos stream, src_file::pos end, intern_string str)
{
	int i = 0;
	while (stream != end && str[i] != '\0' && *stream == str[i])
	{
		++i;
		++stream;
	}
	if (stream == end)
	{
		return str[i] == '\0';
	}
	else
	{
		return !is_alphanum_char(*stream) && *stream != '_' && str[i] == '\0';
	}
}

static intern_string get_identifier_name(src_file::pos &stream, src_file::pos end)
{
	assert(is_identifier(stream));

	auto begin = stream;
	while (
		stream != end
		&& (
			is_alphanum_char(*stream)
			|| *stream == '_'
		)
	)
	{
		++stream;
	}

	return intern_string(&*begin, &*stream);
}

static token get_string_literal(src_file::pos &stream, src_file::pos end)
{
	assert(is_string_literal(stream));

	++stream; // '"'
	auto begin = stream;
	bool loop = true;

	while (stream != end && loop)
	{
		switch (*stream)
		{
		case '\\':
			switch(++stream, *stream)
			{
			case '\'':
			case '"':
			case '\\':
			case 'b':
			case 'n':
			case 't':
				++stream;
				break;

			default:
				assert(false);
				break;
			}
			break;

		case '"':
			loop = false;
			++stream;
			break;

		default:
			++stream;
			break;
		}
	}
	assert(stream != end);

	return { token::string_literal, intern_string(&*begin, &*stream), { begin, stream } };
}

static token get_character_literal(src_file::pos &stream, src_file::pos end)
{
	assert(is_character_literal(stream));

	auto begin = stream;
	++stream; // '\''
	assert(stream != end);

	switch (*stream)
	{
	case '\\':
		switch(++stream, assert(stream != end), *stream)
		{
		case '\'':
		case '"':
		case '\\':
		case 'b':
		case 'n':
		case 't':
			++stream;
			break;

		default:
			assert(false);
			break;
		}
		break;

	case '\'':
		assert(false);
		break;

	default:
		++stream;
		break;
	}

	assert(stream != end);
	assert(*stream == '\'');

	return { token::character_literal, intern_string(&*begin, &*stream), { begin, stream } };
}

static token get_number_literal(src_file::pos &stream, src_file::pos end)
{
	assert(is_number_literal(stream));

	auto begin = stream;
	while (
		stream != end
		&& (
			is_num_char(*stream)
			|| (*stream == '.' && is_num_char(*(stream + 1)))
			|| *stream == '\''
		)
	)
	{
		++stream;
	}

	return { token::number_literal, intern_string(&*begin, &*stream), { begin, stream } };
}

void skip_comments(src_file::pos &stream, src_file::pos &end)
{
	if (stream == end)
	{
		return;
	}

	while (stream != end && is_whitespace_char(*stream))
	{
		++stream;
	}

	switch (*stream)
	{
	case '/':
	{
		auto copy = stream;
		++copy;

		if (copy == end)
		{
			return;
		}

		// line comment
		if (*copy == '/')
		{
			// skips '//'
			++stream;
			++stream;

			while (stream != end && *stream != '\n')
			{
				++stream;
			}
			skip_comments(stream, end);
			return;
		}
		// block comment
		else if (*copy == '*')
		{
			// skips '/*'
			++stream;
			++stream;

			for (; stream != end; ++stream)
			{
				if (*stream == '*')
				{
					auto c = stream;
					++c;
					if (*c == '/')
					{
						// skips '*/'
						++stream;
						++stream;

						skip_comments(stream, end);
						return;
					}
				}
			}
		}
	}

	default:
		return;
	}
}

token get_next_token(src_file::pos &stream, src_file::pos end)
{
	skip_comments(stream, end);

	for (auto const &token : multi_char_tokens)
	{
		if (is_string(stream, end, token.first))
		{
			auto begin = stream;
			for (unsigned i = 0; i < token.first.length(); ++i)
			{
				++stream;
			}

			return {
				token.second,
				token.first,
				{ begin, stream }
			};
		}
	}

	if (is_identifier(stream))
	{
		auto begin = stream;
		auto id = get_identifier_name(stream, end);
		// check if the identifier is a keyword
		for (auto keyword : keywords)
		{
			if (id == keyword.first)
			{
				return { keyword.second, id, { begin, stream } };
			}
		}
		return { token::identifier, id, { begin, stream } };
	}
	else if (is_string_literal(stream))
	{
		return get_string_literal(stream, end);
	}
	else if (is_character_literal(stream))
	{
		return get_character_literal(stream, end);
	}
	else if (is_number_literal(stream))
	{
		return get_number_literal(stream, end);
	}
	else
	{
		assert(stream != end);

		auto prev = stream;
		++stream;
		return {
			static_cast<uint32_t>(*prev),
			intern_string(&*prev, &*stream),
			{ prev, stream }
		};
	}
}

bz::string get_highlighted_tokens(
	src_tokens::pos token_begin,
	src_tokens::pos token_pivot,
	src_tokens::pos token_end
)
{
	auto src_begin = token_begin->src_pos.begin;
	auto src_end   = token_end->src_pos.end;

	auto token_begin_pos = token_begin->src_pos.begin;
	auto token_pivot_pos = token_pivot->src_pos.begin;
	auto token_end_pos   = token_end->src_pos.end;

	assert(*src_begin != '\0');
	while (
		*(src_begin - 1) != '\n'
		&& *(src_begin - 1) != '\0'
	)
	{
		--src_begin;
	}

	while (
		*src_end != '\n'
		&& *src_end != '\0'
	)
	{
		++src_end;
	}

	bz::string res;

	res.reserve((src_end - src_begin) * 2);

	auto print_line = [&]()
	{
		auto line_begin = src_begin;

		for (; src_begin != src_end && *src_begin != '\n'; ++src_begin)
		{
			if (*src_begin != '\r')
			{
				res += *src_begin;
			}
		}
		if (src_begin != src_end)
		{
			++src_begin;
		}

		res += '\n';

		for (; line_begin < token_begin_pos; ++line_begin)
		{
			if (*line_begin == '\t')
			{
				res += '\t';
			}
			else
			{
				res += ' ';
			}
		}

		for (;
			line_begin != token_end_pos && line_begin != src_begin;
			++line_begin
		)
		{
			if (line_begin == token_pivot_pos)
			{
				res += '^';
			}
			else
			{
				res += '~';
			}
		}
		res += '\n';
	};

	while (src_begin != src_end)
	{
		print_line();
	}

	return res;
}
