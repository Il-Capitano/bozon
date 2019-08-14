#include "lexer.h"

static std::array<
	std::pair<intern_string, uint32_t>,
	token::kw_if - token::plus_plus
> multi_char_tokens;

static std::array<
	std::pair<intern_string, uint32_t>,
	token::eof - token::kw_if
> keywords;


void lexer_init(void)
{
	multi_char_tokens =
	{
		std::make_pair( "<<="_is, token::bit_left_shift_eq  ),
		std::make_pair( ">>="_is, token::bit_right_shift_eq ),
		std::make_pair( "..."_is, token::dot_dot_dot        ),

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


std::string get_token_value(uint32_t kind)
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
		return '\'' + std::string{multi_char_it->first} + '\'';
	}

	auto keyword_it = std::find_if(keywords.begin(), keywords.end(), [&](auto const &p){
		return p.second == kind;
	});
	if (keyword_it != keywords.end())
	{
		return '\'' + std::string{keyword_it->first} + '\'';
	}

	return '\'' + std::string{(char)kind} + '\'';
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
		return true;

	default:
		return false;
	}
}


bool is_identifier(buffered_ifstream &is)
{
	return is_number_char(is.current()) ? false : is_identifier_char(is.current());
}

bool is_string_literal(buffered_ifstream &is)
{
	return is.current() == '"';
}

bool is_character_literal(buffered_ifstream &is)
{
	return is.current() == '\'';
}

bool is_number_literal(buffered_ifstream &is)
{
	if (is.current() == '.')
	{
		return is_number_char(is.next());
	}
	else
	{
		return is_number_char(is.current());
	}
}

bool is_token(buffered_ifstream &is)
{
	return !is.eof();
}



intern_string get_identifier(buffered_ifstream &is)
{
	if (!is_identifier(is))
	{
		return "";
	}

	std::string rv;
	while (is_identifier_char(is.current()))
	{
		rv += is.current();
		is.step();
	}

	return rv;
}

intern_string get_string_literal(buffered_ifstream &is)
{
	if (!is_string_literal(is))
	{
		return "";
	}

	is.step();
	std::string rv;
	bool loop = true;

	while (loop)
	{
		switch (is.current())
		{
		case '\\':
			switch(is.next())
			{
			case '\'':
				rv += '\'';
				is.step(2);
				break;

			case '"':
				rv += '"';
				is.step(2);
				break;

			case '\\':
				rv += '\\';
				is.step(2);
				break;

			case 'b':
				rv += '\b';
				is.step(2);
				break;

			case 'n':
				rv += '\n';
				is.step(2);
				break;

			case 't':
				rv += '\t';
				is.step(2);
				break;

			default:
				// TODO: error
				break;
			}
			break;

		case '"':
			loop = false;
			is.step();
			break;

		default:
			rv += is.current();
			is.step();
			break;
		}
	}

	return rv;
}

intern_string get_character_literal(buffered_ifstream &is)
{
	assert(is_character_literal(is));

	std::string rv = "";
	is.step(); // '

	switch (is.current())
	{
	case '\\':
		switch(is.next())
		{
		case '\'':
			rv += '\'';
			is.step(2);
			break;

		case '"':
			rv += '"';
			is.step(2);
			break;

		case '\\':
			rv += '\\';
			is.step(2);
			break;

		case 'b':
			rv += '\b';
			is.step(2);
			break;

		case 'n':
			rv += '\n';
			is.step(2);
			break;

		case 't':
			rv += '\t';
			is.step(2);
			break;

		default:
			// TODO: error
			break;
		}
		break;

	case '\'':
		std::cout << "Illegal character literal ''\n";
		exit(1);

	default:
		rv += is.current();
		is.step();
		break;
	}

	if (is.current() == '\'')
	{
		is.step();
		return rv;
	}
	else
	{
		std::cout << "Illegal character literal\n";
		exit(1);
	}
}

intern_string get_number_literal(buffered_ifstream &is)
{
	if (!is_number_literal(is))
	{
		return "";
	}

	std::string rv;
	bool is_decimal = false;
	while (
		is_number_char(is.current())
		||
		(
			is.current() == '\''
			&&
			is_number_char(is.previous())
			&&
			is_number_char(is.next())
		)
		||
		(
			is.current() == '.'
			&&
			!is_decimal
			&&
			is_number_char(is.next())
		)
	)
	{
		if (is.current() == '.') { is_decimal = true; }
		rv += is.current();
		is.step();
	}

	return rv;
}


void skip_comments(buffered_ifstream &is)
{
	is.step_while_whitespace();

	// line comment
	while (!is.eof() && is.is_string("//"))
	{
		is.step(2);
		while (!is.eof() && is.current() != '\n')
		{
			is.step();
		}
		is.step_while_whitespace();
	}

	// block comment
	while (!is.eof() && is.is_string("/*"))
	{
		int level = 1;
		is.step(2);

		while (!is.eof() && level > 0)
		{
			if (is.is_string("*/"))
			{
				--level;
				is.step(2);
			}
			else if (is.is_string("/*"))
			{
 				++level;
				is.step(2);
			}
			else
			{
				is.step();
			}
		}
		is.step_while_whitespace();
	}

	if (!is.eof() && is.is_string("//"))
	{
		skip_comments(is);
	}
}

token get_next_token(buffered_ifstream &is)
{
	skip_comments(is);

	for (auto const &token : multi_char_tokens)
	{
		if (is.is_string(token.first))
		{
			for (unsigned i = 0; i < token.first.length(); ++i)
			{
				is.step();
			}

			return { token.second, token.first, is.line_num() };
		}
	}

	if (is_identifier(is))
	{
		auto id = get_identifier(is);
		// check if the identifier is a keyword
		for (auto keyword : keywords)
		{
			if (id == keyword.first)
			{
				return { keyword.second, id, is.line_num() };
			}
		}
		return { token::identifier, id, is.line_num() };
	}
	else if (is_string_literal(is))
	{
		return { token::string_literal, get_string_literal(is), is.line_num() };
	}
	else if (is_character_literal(is))
	{
		return { token::character_literal, get_character_literal(is), is.line_num() };
	}
	else if (is_number_literal(is))
	{
		return { token::number_literal, get_number_literal(is), is.line_num() };
	}
	else
	{
		is.step();
		if (is.eof())
		{
			return { token::eof, "eof", is.line_num() };
		}
		else
		{
			return { static_cast<uint32_t>(is.previous()), is.previous(), is.line_num() };
		}
	}
}
