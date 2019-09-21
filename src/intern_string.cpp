#include "intern_string.h"

#include <algorithm>
#include <cstring>
#include <cassert>


bz::vector<std::unique_ptr<const char[]>> intern_string::_buffer_data = {};

char const *intern_string::add_string(bz::string_view str)
{
	if (str.length() == 0)
	{
		return nullptr;
	}

	if (str.length() == 0)
	{
		return nullptr;
	}

	auto new_str = std::make_unique<char[]>(str.length() + 1);
	for (size_t i = 0; i < str.length(); ++i)
	{
		new_str[i] = str[i];
	}
	new_str[str.length()] = '\0';

	_buffer_data.emplace_back(std::move(new_str));
	return _buffer_data.back().get();
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

	_buffer_data.emplace_back(std::move(new_str));
	return _buffer_data.back().get();
}

char const *intern_string::get_string(bz::string_view str)
{
	if (str.length() == 0)
	{
		return nullptr;
	}

	auto it = std::find_if(
		_buffer_data.begin(),
		_buffer_data.end(),
		[&](auto const &s)
		{
			return s.get() == str;
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
