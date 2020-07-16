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

		kw_true,             // true
		kw_false,            // false
		kw_null,             // null

		kw_static_assert,    // static_assert

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



using token_pos = bz::vector<token>::const_iterator;

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

} // namespace lex

#endif // LEX_TOKEN_H
