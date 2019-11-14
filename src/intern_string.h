#ifndef INTERN_STRING_H
#define INTERN_STRING_H

#include <iostream>
#include <memory>
#include <cstring>
#include <bz/string.h>
#include <bz/format.h>

class intern_string
{
	// static members
private:
	static bz::vector<std::unique_ptr<const char[]>> _buffer_data;

	static char const *add_string(bz::string_view str);
	static char const *add_string(const char *begin, const char *end);

	static char const *get_string(bz::string_view str);
	static char const *get_string(const char *begin, const char *end);


private:
	const char *_data;

public:
	intern_string()
		: _data(nullptr)
	{}

	intern_string(bz::string_view str)
		: _data(get_string(str))
	{}

	template<size_t N>
	intern_string(const char str[N])
		: intern_string(static_cast<bz::string_view>(str))
	{}

	intern_string(char c)
	{
		const char tmp[2] = { c, '\0' };
		this->_data = get_string(tmp);
	}

	intern_string(const char *begin, const char *end)
		: _data(get_string(begin, end))
	{}

	intern_string(intern_string const &) = default;
	intern_string(intern_string &&)      = default;

	intern_string &operator = (intern_string const &) = default;
	intern_string &operator = (intern_string &&)      = default;


	char operator [] (size_t n) const
	{ return this->_data[n]; }

	explicit operator const char * () const
	{ return this->_data; }

	operator bz::string () const
	{ return this->_data; }

	operator bz::string_view () const
	{ return this->_data; }


	const char *get(void) const
	{ return this->_data; }

	const char *data(void) const
	{ return this->_data; }

	size_t length(void) const
	{ return strlen(this->_data); }


	friend bool operator == (intern_string lhs, intern_string rhs)
	{ return lhs._data == rhs._data; }

	friend bool operator == (intern_string lhs, bz::string_view rhs)
	{
		if (lhs.length() != rhs.length())
		{
			return false;
		}

		auto lhs_it  = lhs._data;
		auto rhs_it  = rhs.begin();
		auto rhs_end = rhs.end();

		for (; rhs_it != rhs_end; ++lhs_it, ++rhs_it)
		{
			if (*lhs_it != *rhs_it)
			{
				return false;
			}
		}

		return true;
	}

	friend bool operator == (bz::string_view lhs, intern_string rhs)
	{ return rhs == lhs; }

	friend bool operator != (intern_string lhs, intern_string rhs)
	{ return lhs._data != rhs._data; }

	friend bool operator != (intern_string lhs, bz::string_view rhs)
	{ return lhs != rhs; }

	friend bool operator != (bz::string_view lhs, intern_string rhs)
	{ return lhs != rhs; }

	friend std::ostream &operator << (std::ostream &os, intern_string str)
	{
		if (str._data)
		{
			os << str._data;
		}
		return os;
	}
};

inline intern_string operator ""_is (const char *str, size_t)
{
	return intern_string(str);
}

template<>
struct bz::formatter<intern_string>
{
	static bz::string format(intern_string str, const char *spec, const char *spec_end)
	{
		return bz::formatter<const char *>::format(str.data(), spec, spec_end);
	}
};


void intern_string_test();



#endif // INTERN_STRING_H
