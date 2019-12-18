#ifndef LEXER_H
#define LEXER_H

#include "core.h"

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
		number_literal,
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

		kw_auto,             // auto
		kw_let,              // let
		kw_const,            // const

		kw_true,
		kw_false,
		kw_null,

		_last
	};

	uint32_t kind;
	bz::string_view value;
	struct
	{
		bz::string_view file_name;
		bz::string_view::const_iterator begin;
		bz::string_view::const_iterator end;
		size_t line;
		size_t column;
	} src_pos;

	token(
		uint32_t _kind,
		bz::string_view _value,
		bz::string_view _file_name,
		bz::string_view::const_iterator _begin,
		bz::string_view::const_iterator _end,
		size_t _line,
		size_t _column
	)
		: kind(_kind),
		  value(_value),
		  src_pos{ _file_name, _begin, _end, _line, _column }
	{}
};


class src_file
{
private:
	bz::string        _file_name;
	bz::string        _file;
	bz::vector<token> _tokens;

public:
	using char_pos  = bz::string_view::const_iterator;
	using token_pos = bz::vector<token>::const_iterator;

	src_file(bz::string file_name);

	bz::string_view file(void) const
	{ return this->_file; }

	auto tokens_begin(void) const
	{ return this->_tokens.begin(); }

	auto tokens_end(void) const
	{ return this->_tokens.end(); }
};



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

constexpr bool is_identifier_token(token const &t)
{
	return t.kind == token::identifier;
}

constexpr bool is_literal_token(token const &t)
{
	return (
		t.kind == token::number_literal
		|| t.kind == token::string_literal
		|| t.kind == token::character_literal
	);
}

bz::string get_token_value(uint32_t kind);
bool is_operator(uint32_t kind);
bool is_overloadable_operator(uint32_t kind);
bool is_binary_operator(uint32_t kind);
bool is_unary_operator(uint32_t kind);



struct token_range
{
	src_file::token_pos begin;
	src_file::token_pos end;
};


bz::string get_highlighted_tokens(
	src_file::token_pos token_begin,
	src_file::token_pos token_pivot,
	src_file::token_pos token_end
);

inline bz::string get_highlighted_tokens(src_file::token_pos t)
{
	return get_highlighted_tokens(t, t, t + 1);
}

[[noreturn]] inline void bad_token(src_file::token_pos stream, bz::string_view message)
{
	fatal_error(
		"In file {}:{}:{}: {}\n{}",
		stream->src_pos.file_name,
		stream->src_pos.line,
		stream->src_pos.column,
		message,
		get_highlighted_tokens(stream)
	);
}

[[noreturn]] inline void bad_token(src_file::token_pos stream)
{
	bad_token(stream, bz::format("Error: Unexpected token '{}'", stream->value));
}

[[noreturn]] inline void bad_tokens(
	src_file::token_pos begin,
	src_file::token_pos pivot,
	src_file::token_pos end,
	bz::string_view message
)
{
	fatal_error(
		"In file {}:{}:{}: {}\n{}",
		pivot->src_pos.file_name,
		pivot->src_pos.line,
		pivot->src_pos.column,
		message,
		get_highlighted_tokens(begin, pivot, end)
	);
}

inline src_file::token_pos assert_token(src_file::token_pos &stream, uint32_t kind)
{
	if (stream->kind != kind)
	{
		bad_token(stream, bz::format("Error: Expected '{}'", get_token_value(kind)));
	}
	auto t = stream;
	++stream;
	return t;
}

inline src_file::token_pos assert_token(src_file::token_pos &stream, uint32_t kind1, uint32_t kind2)
{
	if (stream->kind != kind1 && stream->kind != kind2)
	{
		bad_token(
			stream,
			bz::format(
				"Error: Expected '{}' or '{}'",
				get_token_value(kind1),
				get_token_value(kind2)
			)
		);
	}
	auto t = stream;
	++stream;
	return t;
}

inline void assert_token(src_file::token_pos stream, uint32_t kind, bool)
{
	if (stream->kind != kind)
	{
		bad_token(stream, bz::format("Error: Expected '{}'", get_token_value(kind)));
	}
}

#endif // LEXER_H
