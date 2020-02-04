#include "lexer.h"

using token_pair = std::pair<bz::string_view, uint32_t>;

static constexpr std::array<
	token_pair,
	token::kw_if - token::plus_plus
> multi_char_tokens = {
	token_pair{ "<<=", token::bit_left_shift_eq  },
	token_pair{ ">>=", token::bit_right_shift_eq },
	token_pair{ "...", token::dot_dot_dot        },
	token_pair{ "..=", token::dot_dot_eq         },

	token_pair{ "++", token::plus_plus           },
	token_pair{ "--", token::minus_minus         },
	token_pair{ "+=", token::plus_eq             },
	token_pair{ "-=", token::minus_eq            },
	token_pair{ "*=", token::multiply_eq         },
	token_pair{ "/=", token::divide_eq           },
	token_pair{ "%=", token::modulo_eq           },
	token_pair{ "<<", token::bit_left_shift      },
	token_pair{ ">>", token::bit_right_shift     },
	token_pair{ "&=", token::bit_and_eq          },
	token_pair{ "|=", token::bit_or_eq           },
	token_pair{ "^=", token::bit_xor_eq          },

	token_pair{ "==", token::equals              },
	token_pair{ "!=", token::not_equals          },
	token_pair{ "<=", token::less_than_eq        },
	token_pair{ ">=", token::greater_than_eq     },
	token_pair{ "&&", token::bool_and            },
	token_pair{ "||", token::bool_or             },
	token_pair{ "^^", token::bool_xor            },

	token_pair{ "->", token::arrow               },
	token_pair{ "::", token::scope               },
	token_pair{ "..", token::dot_dot             }
};

static constexpr std::array<
	token_pair,
	token::_last - token::kw_if
> keywords = {
	token_pair{ "namespace", token::kw_namespace },

	token_pair{ "function", token::kw_function   },
	token_pair{ "operator", token::kw_operator   },
	token_pair{ "typename", token::kw_typename   },

	token_pair{ "return", token::kw_return       },
	token_pair{ "struct", token::kw_struct       },
	token_pair{ "sizeof", token::kw_sizeof       },
	token_pair{ "typeof", token::kw_typeof       },

	token_pair{ "while", token::kw_while         },
	token_pair{ "class", token::kw_class         },
	token_pair{ "using", token::kw_using         },
	token_pair{ "const", token::kw_const         },
	token_pair{ "false", token::kw_false         },

	token_pair{ "else", token::kw_else           },
	token_pair{ "auto", token::kw_auto           },
	token_pair{ "true", token::kw_true           },
	token_pair{ "null", token::kw_null           },

	token_pair{ "for", token::kw_for             },
	token_pair{ "let", token::kw_let             },

	token_pair{ "if", token::kw_if               },
};


struct file_iterator
{
	char_pos it;
	bz::string_view file;
	size_t line;
	size_t column;

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
	char_pos const end,
	bz::vector<error> &errors
);

bz::vector<token> get_tokens(
	bz::string_view file,
	bz::string_view file_name,
	bz::vector<error> &errors
)
{
	assert(file.front() == '\n');
	assert(file.back() == '\n');
	bz::vector<token> tokens = {};
	file_iterator stream = { file.begin() + 1, file_name, 1, 1 };
	auto const end = file.end();

	do
	{
		tokens.push_back(get_next_token(stream, end, errors));
	} while(tokens.back().kind != token::eof);

	return tokens;
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

bool is_binary_operator(uint32_t kind)
{
	switch (kind)
	{
	case token::assign:             // '='
	case token::plus:               // '+'
	case token::plus_eq:            // '+='
	case token::minus:              // '-'
	case token::minus_eq:           // '-='
	case token::multiply:           // '*'
	case token::multiply_eq:        // '*='
	case token::divide:             // '/'
	case token::divide_eq:          // '/='
	case token::modulo:             // '%'
	case token::modulo_eq:          // '%='
	case token::comma:              // ','
	case token::dot_dot:            // '..'
	case token::dot_dot_eq:         // '..='
	case token::equals:             // '=='
	case token::not_equals:         // '!='
	case token::less_than:          // '<'
	case token::less_than_eq:       // '<='
	case token::greater_than:       // '>'
	case token::greater_than_eq:    // '>='
	case token::bit_and:            // '&'
	case token::bit_and_eq:         // '&='
	case token::bit_xor:            // '^'
	case token::bit_xor_eq:         // '^='
	case token::bit_or:             // '|'
	case token::bit_or_eq:          // '|='
	case token::bit_left_shift:     // '<<'
	case token::bit_left_shift_eq:  // '<<='
	case token::bit_right_shift:    // '>>'
	case token::bit_right_shift_eq: // '>>='
	case token::bool_and:           // '&&'
	case token::bool_xor:           // '^^'
	case token::bool_or:            // '||'
	case token::dot:                // '.'
	case token::arrow:              // '->'
	case token::scope:              // '::'
	case token::square_open:        // '[]'
		return true;

	default:
		return false;
	}
}

bool is_unary_operator(uint32_t kind)
{
	switch (kind)
	{
	case token::plus:               // '+'
	case token::minus:              // '-'
	case token::multiply:           // '*'
	case token::bit_and:            // '&'
	case token::bit_not:            // '~'
	case token::bool_not:           // '!'
	case token::plus_plus:          // '++'
	case token::minus_minus:        // '--'
	case token::kw_sizeof:          // 'sizeof'
	case token::kw_typeof:          // 'typeof'
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

static constexpr bool is_identifier_char(char c)
{
	return is_alphanum_char(c) || c == '_';
}

static constexpr bool is_whitespace_char(char c)
{
	return c == ' '
		|| c == '\t'
		|| c == '\n'
		|| c == '\r'; // for windows line ends
}

bz::string get_highlighted_chars(
	char_pos char_begin,
	char_pos char_pivot,
	char_pos char_end
);

[[nodiscard]] static error bad_char(
	file_iterator const &stream,
	bz::string message, bz::vector<note> notes = {}
)
{
	return error{
		stream.file, stream.line, stream.column,
		stream.it, stream.it, stream.it + 1,
		std::move(message),
		std::move(notes)
	};
}

[[nodiscard]] static error bad_char(
	bz::string_view file, size_t line, size_t column,
	bz::string message, bz::vector<note> notes = {}
)
{
	return error{
		file, line, column,
		nullptr, nullptr, nullptr,
		std::move(message),
		std::move(notes)
	};
}


void skip_comments_and_whitespace(file_iterator &stream, char_pos const end)
{
	while (stream.it != end && is_whitespace_char(*stream.it))
	{
		++stream;
	}

	if (stream.it == end || *stream.it != '/')
	{
		return;
	}

	// line comment
	if (stream.it + 1 != end && *(stream.it + 1) == '/')
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
	if (stream.it + 1 != end && *(stream.it + 1) == '*')
	{
		++stream; ++stream; // '/*'
		int comment_depth = 1;

		for (; stream.it != end && comment_depth != 0; ++stream)
		{
			switch (*stream.it)
			{
			case '/':
				if (stream.it + 1 != end && *(stream.it + 1) == '*')
				{
					++stream; ++stream; // '/*'
					++comment_depth;
				}
				break;

			case '*':
				if (stream.it + 1 != end && *(stream.it + 1) == '/')
				{
					++stream; ++stream; // '*/'
					--comment_depth;
				}
				break;

			default:
				break;
			}
		}

		skip_comments_and_whitespace(stream, end);
		return;
	}
}

token get_identifier_or_keyword_token(
	file_iterator &stream,
	char_pos const end,
	bz::vector<error> &
)
{
	assert(
		(*stream.it >= 'a' && *stream.it <= 'z')
		|| (*stream.it >= 'A' && *stream.it <= 'Z')
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

token get_character_token(
	file_iterator &stream,
	char_pos const,
	bz::vector<error> &errors
)
{
	assert(*stream.it == '\'');
	auto const begin_it = stream.it;
	auto const line     = stream.line;
	auto const column   = stream.column;
	++stream;
	auto const char_begin = stream.it;

	switch (*stream.it)
	{
	case '\\':
		++stream;
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
			errors.emplace_back(bad_char(
				stream, bz::format("invalid escape sequence '\\{}'", *stream.it)
			));
			break;
		}
		break;

	default:
		++stream;
		break;
	}

	auto const char_end = stream.it;
	if (*stream.it != '\'')
	{
		errors.emplace_back(bad_char(
			stream, "expected closing '",
			{ note{
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

token get_string_token(
	file_iterator &stream,
	char_pos const end,
	bz::vector<error> &errors
)
{
	assert(*stream.it == '\"');
	auto const begin_it = stream.it;
	auto const line     = stream.line;
	auto const column   = stream.column;
	++stream;
	auto const str_begin = stream.it;

	while (stream.it != end && *stream.it != '\"')
	{
		switch (*stream.it)
		{
		case '\\':
			++stream;
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
				errors.emplace_back(bad_char(
					stream, bz::format("invalid escape sequence '\\{}'", *stream.it)
				));
				break;
			}
			break;

		default:
			++stream;
			break;
		}
	}

	auto const str_end = stream.it;
	if (stream.it == end)
	{
		errors.emplace_back(bad_char(
			stream.file, stream.line, stream.column,
			"expected closing \" before end-of-file",
			{ note{
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
		token::string_literal,
		bz::string_view(&*str_begin, &*str_end),
		stream.file, begin_it, end_it, line, column
	);
}

token get_number_token(
	file_iterator &stream,
	char_pos const end,
	bz::vector<error> &
)
{
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

token get_single_char_token(
	file_iterator &stream,
	char_pos const,
	bz::vector<error> &
)
{
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

bool is_str(bz::string_view str, file_iterator &stream, char_pos const end)
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
	char_pos const end,
	bz::vector<error> &errors
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
		return get_identifier_or_keyword_token(stream, end, errors);

	// character
	case '\'':
		return get_character_token(stream, end, errors);
	// string
	case '\"':
		return get_string_token(stream, end, errors);
	// number
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		return get_number_token(stream, end, errors);

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

	return get_single_char_token(stream, end, errors);
}

bz::string get_highlighted_chars(
	char_pos char_begin,
	char_pos char_pivot,
	char_pos char_end
)
{
	assert(char_begin < char_end);
	assert(char_begin <= char_pivot);
	assert(char_pivot < char_end);

	auto line_begin = char_begin;

	while (*(line_begin - 1) != '\n')
	{
		--line_begin;
	}

	auto line_end = char_end;

	while (*(line_end - 1) != '\n')
	{
		++line_end;
	}

	bz::string file_line = "";
	bz::string highlight_line = "";

	bz::string result = "";

	auto it = line_begin;

	for (; it != line_end; ++it)
	{
		file_line = "";
		highlight_line = "";

		while (true)
		{
			if (*it == '\t')
			{
				if (it == char_pivot)
				{
					file_line += ' ';
					highlight_line += '^';
					while (file_line.size() % 4 != 0)
					{
						file_line += ' ';
						highlight_line += '~';
					}
				}
				else
				{
					char highlight_char = it >= char_begin && it < char_end ? '~' : ' ';
					do
					{
						file_line += ' ';
						highlight_line += highlight_char;
					} while (file_line.size() % 4 != 0);
				}
			}
			else
			{
				file_line += *it;
				if (it == char_pivot)
				{
					highlight_line += '^';
				}
				else if (it >= char_begin && it < char_end)
				{
					highlight_line += '~';
				}
				else
				{
					highlight_line += ' ';
				}
			}

			if (*it == '\n')
			{
				break;
			}
			++it;
		}

		result += file_line;
		result += highlight_line;
		result += '\n';
	}

	return result;
}

bz::string get_highlighted_tokens(
	token_pos token_begin,
	token_pos token_pivot,
	token_pos token_end
)
{
	if (token_pivot->kind == token::eof)
	{
		return "\n";
	}

	return get_highlighted_chars(
		token_begin->src_pos.begin,
		token_pivot->src_pos.begin,
		(token_end - 1)->src_pos.end
	);
}
