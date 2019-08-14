#ifndef LEXER_H
#define LEXER_H

#include "core.h"

void lexer_init(void);

class buffered_ifstream
{
private:
	std::ifstream _is;
	char _buff[256];
	bool _eof;
	int _line_num;

public:
	buffered_ifstream(std::string const &file)
		: _is(file), _buff(), _eof(false), _line_num(1)
	{
		for (int i = 128; i < 256 && this->_is; ++i)
		{
			this->_buff[i] = this->_is.get();
		}
	}

	operator bool(void) const
	{
		return !this->_eof;
	}

	char operator[](int index) const
	{
		return this->_buff[index + 128];
	}

	char previous_non_whitespace(void) const
	{
		int i = 127;
		while (
			this->_buff[i] == ' '
			||
			this->_buff[i] == '\t'
			||
			this->_buff[i] == '\n'
		)
		{
			--i;
		}
		return i < 0 ? '\0' : this->_buff[i];
	}

	char previous(void) const
	{
		return this->_buff[127];
	}

	char current(void) const
	{
		return this->_buff[128];
	}

	char next(void) const
	{
		return this->_buff[129];
	}

	char next_non_whitespace(void) const
	{
		int i = 129;
		while (
			this->_buff[i] == ' '
			||
			this->_buff[i] == '\t'
			||
			this->_buff[i] == '\n'
		)
		{
			++i;
		}
		return i > 255 ? '\0' : this->_buff[i];
	}

	void step(unsigned count = 1)
	{
		if (this->current() == decltype(this->_is)::traits_type::eof())
		{
			this->_eof = true;
			return;
		}
		else if (this->current() == '\n')
		{
			++this->_line_num;
		}

		for (unsigned i = 0; i < count; ++i)
		{
			for (int j = 0; j < 255; ++j)
			{
				this->_buff[j] = this->_buff[j + 1];
			}

			if (this->_is)
			{
				this->_buff[255] = this->_is.get();
			}
			else
			{
				this->_buff[255] = '\0';
			}
		}
	}

	void step_while_whitespace(void)
	{
		while (
			this->current() == ' '
			||
			this->current() == '\t'
			||
			this->current() == '\n'
		)
		{
			this->step();
		}
	}

	bool is_string(intern_string str) const
	{
		auto len = str.length();
		assert(len <= 128);
		for (unsigned i = 0; i < len; ++i)
		{
			if (this->_buff[i + 128] != str[i])
			{
				return false;
			}
		}
		return true;
	}

	bool eof(void) const
	{
		return this->_eof;
	}

	int line_num(void) const
	{
		return this->_line_num;
	}
};

struct token
{
	enum : uint32_t
	{
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

		eof,
	};

	uint32_t kind;
	intern_string value;
	int line_num;
	//int char_num;

	token *operator ->()
	{
		return this;
	}

	token const *operator -> () const
	{
		return this;
	}
};

inline std::ostream &operator << (std::ostream &os, token const &t)
{
	return os << "type: " << t.kind << "; value: '" << t.value << "'; line number: " << t.line_num;
}


constexpr bool is_keyword_token(token const &t)
{
	return t.kind > token::dot_dot_dot && t.kind < token::eof;
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



constexpr bool is_number_char(char c)
{
	return c >= '0' && c <= '9';
}

constexpr bool is_identifier_char(char c)
{
	return (
		(c >= 'a' && c <= 'z')
		||
		(c >= 'A' && c <= 'Z')
		||
		(c >= '0' && c <= '9')
		||
		c == '_'
	);
}

constexpr bool is_whitespace_char(char c)
{
	return c == ' ' || c == '\t' || c == '\n';
}

bool is_identifier(buffered_ifstream &is);
bool is_string_literal(buffered_ifstream &is);
bool is_character_literal(buffered_ifstream &is);
bool is_number_literal(buffered_ifstream &is);
bool is_token(buffered_ifstream &is);

intern_string get_identifier(buffered_ifstream &is);
intern_string get_string_literal(buffered_ifstream &is);
intern_string get_character_literal(buffered_ifstream &is);
intern_string get_number_literal(buffered_ifstream &is);

token get_next_token(buffered_ifstream &is);


std::string get_token_value(uint32_t kind);
bool is_operator(uint32_t kind);
bool is_overloadable_operator(uint32_t kind);


#endif // LEXER_H
