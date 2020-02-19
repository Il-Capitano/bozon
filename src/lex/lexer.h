#ifndef LEXER_H
#define LEXER_H

#include "core.h"

#include "ctx/error.h"

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


bz::string get_token_value(uint32_t kind);
bool is_operator(uint32_t kind);
bool is_overloadable_operator(uint32_t kind);
bool is_binary_operator(uint32_t kind);
bool is_unary_operator(uint32_t kind);


struct token_range
{
	token_pos begin;
	token_pos end;
};

bz::vector<token> get_tokens(
	bz::string_view file,
	bz::string_view file_name,
	bz::vector<ctx::error> &errors
);

[[nodiscard]] inline ctx::error bad_token(
	token_pos it,
	bz::string message,
	bz::vector<ctx::note> notes = {}
)
{
	return ctx::error{
		it->src_pos.file_name, it->src_pos.line, it->src_pos.column,
		it->src_pos.begin, it->src_pos.begin, it->src_pos.end,
		std::move(message),
		std::move(notes)
	};
}

[[nodiscard]] inline ctx::error bad_token(token_pos it)
{
	return bad_token(it, bz::format("unexpected token '{}'", it->value));
}

[[nodiscard]] inline ctx::error bad_tokens(
	token_pos begin, token_pos pivot, token_pos end,
	bz::string message,
	bz::vector<ctx::note> notes = {}
)
{
	return ctx::error{
		pivot->src_pos.file_name, pivot->src_pos.line, pivot->src_pos.column,
		begin->src_pos.begin, pivot->src_pos.begin, (end - 1)->src_pos.end,
		std::move(message),
		std::move(notes)
	};
}

template<typename T, typename ...Ts>
[[nodiscard]] inline ctx::error bad_tokens(
	T const &tokens,
	bz::string message,
	bz::vector<ctx::note> notes = {}
)
{
	return bad_tokens(
		tokens.get_tokens_begin(),
		tokens.get_tokens_pivot(),
		tokens.get_tokens_end(),
		std::move(message),
		std::move(notes)
	);
}

[[nodiscard]] inline ctx::note make_note(
	token_pos it,
	bz::string message
)
{
	return ctx::note{
		it->src_pos.file_name, it->src_pos.line, it->src_pos.column,
		it->src_pos.begin, it->src_pos.begin, it->src_pos.end,
		std::move(message)
	};
}

[[nodiscard]] inline ctx::note make_note(
	token_pos begin, token_pos pivot, token_pos end,
	bz::string message
)
{
	return ctx::note{
		pivot->src_pos.file_name, pivot->src_pos.line, pivot->src_pos.column,
		begin->src_pos.begin, pivot->src_pos.begin, end->src_pos.end,
		std::move(message)
	};
}

token_pos assert_token(
	token_pos &stream,
	uint32_t kind,
	bz::vector<ctx::error> &errors
);

token_pos assert_token(
	token_pos &stream,
	uint32_t kind1, uint32_t kind2,
	bz::vector<ctx::error> &errors
);

} // namespace lex

#endif // LEXER_H
