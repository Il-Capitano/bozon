#include "token.h"

namespace lex
{

bz::string get_token_value(uint32_t kind)
{
	switch (kind)
	{
	case token::identifier:
		return "identifier";
	case token::integer_literal:
		return "integer literal";
	case token::floating_point_literal:
		return "floating-point literal";
	case token::hex_literal:
		return "hexadecimal literal";
	case token::oct_literal:
		return "octal literal";
	case token::bin_literal:
		return "binary literal";
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
	case token::square_open:        // '[]'        binary
	case token::paren_open:         // function call
		return true;

	default:
		return false;
	}
}

bool is_overloadable_unary_operator(uint32_t kind)
{
	switch (kind)
	{
	case token::plus:               // '+'
	case token::minus:              // '-'
	case token::dereference:        // '*'
	case token::bit_not:            // '~'
	case token::bool_not:           // '!'
	case token::plus_plus:          // '++'
	case token::minus_minus:        // '--'
		return true;

	default:
		return false;
	}
}

bool is_overloadable_binary_operator(uint32_t kind)
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
	case token::arrow:              // '->' ???
	case token::square_open:        // '[]'
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
	case token::dereference:        // '*'
	case token::ampersand:          // '&'
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

} // namespace lex
