#ifndef LEXER_H
#define LEXER_H

#include "core.h"

void lexer_init(void);

class src_file
{
private:
	bz::vector<char> _data;
public:
	using pos = bz::vector<char>::const_iterator;

public:
	src_file(const char *file_name)
		: _data()
	{
		std::ifstream file(file_name, std::ios::binary);
		assert(file.is_open());
		assert(file.good());

		// sets the file to its end so we can get its size
		file.seekg(0, std::ios_base::end);
		size_t size = file.tellg();
		// resize the vector to the length of the file
		// plus two for a '\0' at the beginning and the end
		this->_data.resize(size + 2);

		auto begin = std::istreambuf_iterator<char>(file);
		auto end   = std::istreambuf_iterator<char>();

		// sets the file to its beginning
		file.seekg(0, std::ios_base::beg);
		std::copy(begin, end, this->_data.begin() + 1);
		this->_data.front() = '\0';
		this->_data.back() = '\0';
	}

	pos begin(void) const
	{
		return this->_data.begin() + 1;
	}

	pos end(void) const
	{
		return this->_data.end();
	}
};

struct token
{
	enum : uint32_t
	{
		eof           = '\0',

		paren_open    = '(',
		paren_close   = ')',
		curly_open    = '{',
		curly_close   = '}', // '
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
	bz::string value;
	struct
	{
		src_file::pos begin;
		src_file::pos end;
//		size_t line;
//		size_t columb;
	} src_pos;


	token *operator -> ()
	{
		return this;
	}

	token const *operator -> () const
	{
		return this;
	}
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
		||
		t.kind == token::string_literal
		||
		t.kind == token::character_literal
	);
}


token get_next_token(src_file::pos &stream, src_file::pos end);

bz::string get_token_value(uint32_t kind);
bool is_operator(uint32_t kind);
bool is_overloadable_operator(uint32_t kind);
bool is_binary_operator(uint32_t kind);
bool is_unary_operator(uint32_t kind);


class src_tokens
{
private:
	bz::vector<token> _tokens;
	src_file _src_file;

public:
	using pos = bz::vector<token>::const_iterator;

public:
	src_tokens(const char *file)
		: _tokens(), _src_file(file)
	{
		auto stream = this->_src_file.begin();
		auto end    = this->_src_file.end();

		do
		{
			this->_tokens.emplace_back(get_next_token(stream, end));
		} while (this->_tokens.back().kind != token::eof);
	}

	pos begin(void) const
	{
		return this->_tokens.cbegin();
	}

	pos end(void) const
	{
		return this->_tokens.cend();
	}
};


struct token_range
{
	src_tokens::pos begin;
	src_tokens::pos end;
};


bz::string get_highlighted_tokens(
	src_tokens::pos token_begin,
	src_tokens::pos token_pivot,
	src_tokens::pos token_end
);

inline bz::string get_highlighted_tokens(src_tokens::pos t)
{
	return get_highlighted_tokens(t, t, t);
}

[[noreturn]] inline void bad_token(src_tokens::pos stream)
{
	fatal_error("{}Unexpected token: '{}'\n", get_highlighted_tokens(stream), stream->value);
}

[[noreturn]] inline void bad_token(src_tokens::pos stream, bz::string_view message)
{
	fatal_error("{}{}\n", get_highlighted_tokens(stream), message);
}

[[noreturn]] inline void bad_tokens(
	src_tokens::pos begin,
	src_tokens::pos pivot,
	src_tokens::pos end,
	bz::string_view message
)
{
	fatal_error("{}{}\n", get_highlighted_tokens(begin, pivot, end), message);
}

inline src_tokens::pos assert_token(src_tokens::pos &stream, uint32_t kind)
{
	if (stream->kind != kind)
	{
		bad_token(stream, bz::format("Expected '{}'", get_token_value(kind)));
	}
	auto t = stream;
	++stream;
	return t;
}

inline src_tokens::pos assert_token(src_tokens::pos &stream, uint32_t kind1, uint32_t kind2)
{
	if (stream->kind != kind1 && stream->kind != kind2)
	{
		bad_token(
			stream,
			bz::format(
				"Expected '{}' or '{}'",
				get_token_value(kind1),
				get_token_value(kind2)
			)
		);
	}
	auto t = stream;
	++stream;
	return t;
}

inline void assert_token(src_tokens::pos stream, uint32_t kind, bool)
{
	if (stream->kind != kind)
	{
		bad_token(stream, bz::format("Expected '{}'", get_token_value(kind)));
	}
}

#endif // LEXER_H
