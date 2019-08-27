#include "intern_string.h"

#include <algorithm>
#include <cstring>
#include <cassert>


std::vector<std::unique_ptr<const char[]>> intern_string::_buffer_data = {};

char const *intern_string::add_string(std::string const &str)
{
	auto len = str.length();
	if (len == 0)
	{
		return nullptr;
	}

	auto new_str = std::make_unique<char[]>(len + 1);
	for (size_t i = 0; i < len; ++i)
	{
		new_str[i] = str[i];
	}
	new_str[len] = '\0';

	return _buffer_data.emplace_back(std::move(new_str)).get();
}

char const *intern_string::add_string(const char *str)
{
	if (str == nullptr)
	{
		return nullptr;
	}

	auto len = strlen(str);
	if (len == 0)
	{
		return nullptr;
	}

	auto new_str = std::make_unique<char[]>(len + 1);
	for (size_t i = 0; i < len; ++i)
	{
		new_str[i] = str[i];
	}
	new_str[len] = '\0';

	return _buffer_data.emplace_back(std::move(new_str)).get();
}

char const *intern_string::add_string(const char *begin, const char *end)
{
	if (
		begin == nullptr
		|| end == nullptr
		|| begin >= end)
	{
		return nullptr;
	}

	size_t len = end - begin;
	auto new_str = std::make_unique<char[]>(len + 1);
	for (size_t i = 0; i < len; ++i)
	{
		new_str[i] = begin[i];
	}
	new_str[len] = '\0';

	return _buffer_data.emplace_back(std::move(new_str)).get();
}

char const *intern_string::get_string(std::string const &str)
{
	auto it = std::find_if(
		_buffer_data.begin(),
		_buffer_data.end(),
		[&](auto const &s)
		{
			return strcmp(s.get(), str.c_str()) == 0;
		}
	);

	if (it == _buffer_data.end())
	{
		return add_string(str);
	}

	return it->get();
}

char const *intern_string::get_string(const char *str)
{
	if (str == nullptr)
	{
		return nullptr;
	}

	auto it = std::find_if(
		_buffer_data.begin(),
		_buffer_data.end(),
		[&](auto const &s)
		{
			return strcmp(s.get(), str) == 0;
		}
	);

	if (it == _buffer_data.end())
	{
		return add_string(str);
	}

	return it->get();
}

char const *intern_string::get_string(const char *begin, const char *end)
{
	auto it = std::find_if(
		_buffer_data.begin(),
		_buffer_data.end(),
		[&](auto const &s)
		{
			auto len = strlen(s.get());
			if (len != static_cast<size_t>(end - begin))
			{
				return false;
			}

			for (size_t i = 0; i < len; ++i)
			{
				if (s[i] != begin[i])
				{
					return false;
				}
			}
			return true;
		}
	);

	if (it == _buffer_data.end())
	{
		return add_string(begin, end);
	}

	return it->get();
}



void intern_string_test()
{
	using namespace std::string_literals;
	assert("foo"_is == "foo"s);
	intern_string foo = "foo";
	intern_string bar = "bar";
	assert(foo != bar);
	intern_string foo2 = "foo";
	assert(foo == foo2);
	intern_string foo_string = "foo"s;
	assert(foo == foo_string);
	auto bar_copy = bar;
	assert(bar_copy == bar);

	assert("foo"_is == "foo");

	intern_string empty = "";
	assert(empty == nullptr);
}
