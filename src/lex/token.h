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
		auto_ref      = '#',

		star          = '*',
		ampersand     = '&',

		at            = '@',

		identifier = 256,
		integer_literal,
		floating_point_literal,
		hex_literal,
		oct_literal,
		bin_literal,
		string_literal,
		raw_string_literal,
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
		auto_ref_const,      // ##


		// keywords
		kw_if,               // if
		kw_else,             // else
		kw_switch,           // switch
		kw_while,            // while
		kw_for,              // for
		kw_return,           // return
		kw_function,         // function
		kw_operator,         // operator
		kw_class,            // class
		kw_struct,           // struct
		kw_typename,         // typename
		kw_type,             // type
		kw_namespace,        // namespace
		kw_sizeof,           // sizeof
		kw_typeof,           // typeof
		kw_using,            // using
		kw_export,           // export
		kw_import,           // import
		kw_in,               // in

		kw_as,               // as

		kw_auto,             // auto
		kw_let,              // let
		kw_const,            // const
		kw_consteval,        // consteval
		kw_move,             // move
		kw_forward,          // __forward

		kw_true,             // true
		kw_false,            // false
		kw_null,             // null
		kw_unreachable,      // unreachable
		kw_break,            // break
		kw_continue,         // continue

		kw_static_assert,    // static_assert

		non_ascii_character,

		_last
	};

	uint32_t kind;
	bz::u8string_view value;
	bz::u8string_view postfix;
	struct
	{
		uint32_t file_id;
		uint32_t line;
		bz::u8string_view::const_iterator begin;
		bz::u8string_view::const_iterator end;
	} src_pos;

	token(
		uint32_t _kind,
		bz::u8string_view _value,
		uint32_t _file_id,
		uint32_t _line,
		bz::u8string_view::const_iterator _begin,
		bz::u8string_view::const_iterator _end
	)
		: kind   (_kind),
		  value  (_value),
		  postfix(""),
		  src_pos{ _file_id, _line, _begin, _end }
	{}

	token(
		uint32_t _kind,
		bz::u8string_view _value,
		bz::u8string_view _postfix,
		uint32_t _file_id,
		uint32_t _line,
		bz::u8string_view::const_iterator _begin,
		bz::u8string_view::const_iterator _end
	)
		: kind   (_kind),
		  value  (_value),
		  postfix(_postfix),
		  src_pos{ _file_id, _line, _begin, _end }
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

	static src_tokens from_single_token(token_pos it) noexcept
	{
		if (it == nullptr)
		{
			return {};
		}
		else
		{
			return { it, it, it + 1 };
		}
	}

	static src_tokens from_range(token_range range) noexcept
	{
		return { range.begin, range.begin, range.end };
	}
};

} // namespace lex

#endif // LEX_TOKEN_H
