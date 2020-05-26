#ifndef LEX_TOKEN_H
#define LEX_TOKEN_H

#include "core.h"

namespace lex
{

struct token
{
	enum : uint32_t
	{
		eof           = '\0',

		paren_open    = '(',
		paren_close   = ')',
		curly_open    = '{',
		curly_close   = '}',
		square_open   = '[',
		square_close  = ']',
		angle_open    = '<',
		angle_close   = '>',
		semi_colon    = ';',
		colon         = ':',
		comma         = ',',
		dot           = '.',
		question_mark = '?',


		// ==== operators ====
		// binary
		assign        = '=',
		plus          = '+',
		minus         = '-',
		multiply      = '*',
		divide        = '/',
		modulo        = '%',
		less_than     = '<',
		greater_than  = '>',
		bit_and       = '&',
		bit_xor       = '^',
		bit_or        = '|',

		// unary
		bit_not       = '~',
		bool_not      = '!',
		address_of    = '&',
		dereference   = '*',

		star          = '*',
		ampersand     = '&',

		identifier = 256,
		integer_literal,
		floating_point_literal,
		hex_literal,
		oct_literal,
		bin_literal,
		string_literal,
		character_literal,

		// multi-char tokens
		plus_plus,           // ++
		minus_minus,         // --
		plus_eq,             // +=
		minus_eq,            // -=
		multiply_eq,         // *=
		divide_eq,           // /=
		modulo_eq,           // %=
		bit_left_shift,      // <<
		bit_right_shift,     // >>
		bit_and_eq,          // &=
		bit_xor_eq,          // ^=
		bit_or_eq,           // |=
		bit_left_shift_eq,   // <<=
		bit_right_shift_eq,  // >>=
		equals,              // ==
		not_equals,          // !=
		less_than_eq,        // <=
		greater_than_eq,     // >=
		bool_and,            // &&
		bool_xor,            // ^^
		bool_or,             // ||
		arrow,               // ->
		fat_arrow,           // =>
		scope,               // ::
		dot_dot,             // ..
		dot_dot_eq,          // ..=
		dot_dot_dot,         // ...


		// keywords
		kw_if,               // if
		kw_else,             // else
		kw_while,            // while
		kw_for,              // for
		kw_return,           // return
		kw_function,         // function
		kw_operator,         // operator
		kw_class,            // class
		kw_struct,           // struct
		kw_typename,         // typename
		kw_namespace,        // namespace
		kw_sizeof,           // sizeof
		kw_typeof,           // typeof
		kw_using,            // using

		kw_as,               // as

		kw_auto,             // auto
		kw_let,              // let
		kw_const,            // const

		kw_true,
		kw_false,
		kw_null,

		_last
	};

	uint32_t kind;
	bz::u8string_view value;
	bz::u8string_view postfix;
	struct
	{
		bz::u8string_view file_name;
		bz::u8string_view::const_iterator begin;
		bz::u8string_view::const_iterator end;
		size_t line;
	} src_pos;

	token(
		uint32_t _kind,
		bz::u8string_view _value,
		bz::u8string_view _file_name,
		bz::u8string_view::const_iterator _begin,
		bz::u8string_view::const_iterator _end,
		size_t _line
	)
		: kind   (_kind),
		  value  (_value),
		  postfix(""),
		  src_pos{ _file_name, _begin, _end, _line }
	{}

	token(
		uint32_t _kind,
		bz::u8string_view _value,
		bz::u8string_view _postfix,
		bz::u8string_view _file_name,
		bz::u8string_view::const_iterator _begin,
		bz::u8string_view::const_iterator _end,
		size_t _line
	)
		: kind   (_kind),
		  value  (_value),
		  postfix(_postfix),
		  src_pos{ _file_name, _begin, _end, _line }
	{}
};

using token_pair = std::pair<bz::u8string_view, uint32_t>;

constexpr std::array<
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
	token_pair{ "=>", token::fat_arrow           },
	token_pair{ "::", token::scope               },
	token_pair{ "..", token::dot_dot             }
};

constexpr std::array<
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

	token_pair{ "as", token::kw_as               },
	token_pair{ "if", token::kw_if               },
};


using token_pos = bz::vector<token>::const_iterator;

constexpr bool is_keyword_token(token const &t)
{
	switch (t.kind)
	{
	case token::kw_if:
	case token::kw_else:
	case token::kw_while:
	case token::kw_for:
	case token::kw_return:
	case token::kw_function:
	case token::kw_operator:
	case token::kw_class:
	case token::kw_struct:
	case token::kw_typename:
	case token::kw_namespace:
	case token::kw_sizeof:
	case token::kw_typeof:
	case token::kw_using:
	case token::kw_as:
	case token::kw_auto:
	case token::kw_let:
	case token::kw_const:
	case token::kw_true:
	case token::kw_false:
	case token::kw_null:
		return true;

	default:
		return false;
	}
}


bz::u8string get_token_value(uint32_t kind);
bool is_operator(uint32_t kind);
bool is_overloadable_unary_operator(uint32_t kind);
bool is_overloadable_binary_operator(uint32_t kind);
bool is_overloadable_operator(uint32_t kind);
bool is_binary_operator(uint32_t kind);
bool is_unary_operator(uint32_t kind);


struct token_range
{
	token_pos begin = nullptr;
	token_pos end   = nullptr;
};

struct src_tokens
{
	token_pos begin;
	token_pos pivot;
	token_pos end;

	src_tokens(void) noexcept
		: begin(nullptr), pivot(nullptr), end(nullptr)
	{}

	src_tokens(token_pos _begin, token_pos _pivot, token_pos _end) noexcept
		: begin(_begin), pivot(_pivot), end(_end)
	{}
};

inline bz::u8string get_token_name_for_message(uint32_t kind)
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
		return bz::format("'{}'", get_token_value(kind));
	}
}

} // namespace lex

#endif // LEX_TOKEN_H
