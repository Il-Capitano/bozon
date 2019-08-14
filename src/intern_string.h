#ifndef INTERN_STRING_H
#define INTERN_STRING_H

#include <iostream>
#include <vector>
#include <memory>
#include <string>
#include <cstring>

class intern_string
{
	// static members
private:
	static std::vector<std::unique_ptr<const char[]>> _buffer_data;

	static char const *add_string(std::string const &str);
	static char const *add_string(const char *str);

	static char const *get_string(std::string const &str);
	static char const *get_string(const char *str);


private:
	const char *_data;

public:
	intern_string()
		: _data(nullptr)
	{}

	intern_string(std::string const &str)
		: _data(get_string(str))
	{}

	intern_string(const char *str)
		: _data(get_string(str))
	{}

	intern_string(char c)
	{
		char str[2] = { c, '\0' };
		this->_data = get_string(str);
	}

	intern_string(intern_string const &) = default;
	intern_string(intern_string &&)      = default;

	intern_string &operator = (intern_string const &) = default;
	intern_string &operator = (intern_string &&)      = default;


	char operator [] (size_t n) const
	{ return this->_data[n]; }

	explicit operator const char * ()
	{ return this->_data; }

	explicit operator std::string ()
	{ return this->_data; }


	const char *get(void) const
	{ return this->_data; }

	size_t length(void) const
	{ return strlen(this->_data); }


	friend bool operator == (intern_string lhs, intern_string rhs)
	{ return lhs._data == rhs._data; }

	friend bool operator != (intern_string lhs, intern_string rhs)
	{ return lhs._data != rhs._data; }

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


void intern_string_test();



#endif // INTERN_STRING_H
