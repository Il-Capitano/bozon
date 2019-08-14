#ifndef TOKEN_STREAM_H
#define TOKEN_STREAM_H

#include "core.h"

#include "lexer.h"

class token_stream
{
private:
	token _tokens[128];
	buffered_ifstream _is;

public:
	token_stream(std::string const &file)
		: _tokens(), _is(file)
	{
		for (int i = 0; i < 128; ++i)
		{
			this->_tokens[i] = get_next_token(this->_is);
		}
	}

	token const &current(void) const
	{
		return this->_tokens[0];
	}

	token const &next(void) const
	{
		return this->_tokens[1];
	}

	token const &operator [] (size_t i) const
	{
		return this->_tokens[i];
	}

	void step(int count = 1)
	{
		for (int i = 0; i < count; ++i)
		{
			for (int j = 0; j < 127; ++j)
			{
				this->_tokens[j] = std::move(this->_tokens[j + 1]);
			}
			this->_tokens[127] = get_next_token(this->_is);
		}
	}

	token get(void)
	{
		auto t = this->current();
		this->step();
		return t;
	}
};

inline token assert_token(token_stream &stream, uint32_t kind)
{
	if (stream.current().kind != kind)
	{
		std::cerr << "Unexpected token: " << stream.current() << '\n';
		std::cerr << "Expected " << get_token_value(kind) << '\n';
		exit(1);
	}
	return stream.get();
}

inline void assert_token(token_stream const &stream, uint32_t kind, bool)
{
	if (stream.current().kind != kind)
	{
		std::cerr << "Unexpected token: " << stream.current() << '\n';
		std::cerr << "Expected " << get_token_value(kind) << '\n';
		exit(1);
	}
}

inline void assert_token(token const &t, uint32_t kind)
{
	if (t.kind != kind)
	{
		std::cerr << "Unexpected token: " << t << '\n';
		std::cerr << "Expected " << get_token_value(kind) << '\n';
		exit(1);
	}
}

inline void bad_token(token_stream const &stream)
{
	std::cerr << "Unexpected token: " << stream.current() << '\n';
	exit(1);
}

inline void bad_token(token_stream const &stream, std::string const &message)
{
	std::cerr << stream.current() << '\n';
	std::cerr << message << '\n';
	exit(1);
}

inline void bad_token(token const &t)
{
	std::cerr << "Unexpected token: " << t << '\n';
	exit(1);
}

inline void bad_token(token const &t, std::string const &message)
{
	std::cerr << t << '\n';
	std::cerr << message << '\n';
	exit(1);
}


#endif // TOKEN_STREAM_H
