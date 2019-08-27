#ifndef LEXER_H
#define LEXER_H

#include "core.h"

void lexer_init(void);
/*
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
*/

class src_file
{
private:
	std::vector<char> _data;
public:
	using pos = std::vector<char>::const_iterator;

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
		// plus one for the '\0' char
		this->_data.resize(size + 1);

		auto begin = std::istreambuf_iterator<char>(file);
		auto end   = std::istreambuf_iterator<char>();

		// sets the file to its beginning
		file.seekg(0, std::ios_base::beg);
		std::copy(begin, end, this->_data.begin());
		this->_data.back() = '\0';

//		for (char c : this->_data)
//		{
//			std::cout << c;
//		}
	}

	pos begin(void) const
	{
		return this->_data.begin();
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
	intern_string value;
	struct
	{
		src_file::pos begin;
		src_file::pos end;
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

inline std::ostream &operator << (std::ostream &os, token const &t)
{
	return os << "type: " << t.kind << "; value: '" << t.value << "';";
}


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

std::string get_token_value(uint32_t kind);
bool is_operator(uint32_t kind);
bool is_overloadable_operator(uint32_t kind);



class src_tokens
{
private:
	std::vector<token> _tokens;
	src_file _src_file;

public:
	using pos = std::vector<token>::const_iterator;

public:
	src_tokens(const char *file)
		: _tokens(), _src_file(file)
	{
		auto stream = this->_src_file.begin();
		auto end    = this->_src_file.end();

		token t;
		do
		{
			t = get_next_token(stream, end);
			this->_tokens.emplace_back(t);
		} while (t.kind != token::eof);
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


inline token assert_token(src_tokens::pos &stream, uint32_t kind)
{
	if (stream->kind != kind)
	{
		std::cerr << "Unexpected token: " << *stream << "\n"
			"Expected " << get_token_value(kind) << '\n';
		exit(1);
	}
	auto t = *stream;
	++stream;
	return t;
}

inline void assert_token(src_tokens::pos stream, uint32_t kind, bool)
{
	if (stream->kind != kind)
	{
		std::cerr << "Unexpected token: " << *stream << "\n"
			"Expected " << get_token_value(kind) << '\n';
		exit(1);
	}
}

inline void bad_token(src_tokens::pos stream)
{
	std::cerr << "Unexpected token: " << *stream << '\n';
	exit(1);
}

inline void bad_token(src_tokens::pos stream, const char *message)
{
	std::cerr << "Unexpected token: " << *stream << '\n'
		<< message << '\n';
	exit(1);
}

#endif // LEXER_H
