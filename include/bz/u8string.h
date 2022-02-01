#ifndef _bz_u8string_h_
#define _bz_u8string_h_

#include "core.h"
#include "u8string_view.h"
#include "ranges.h"
#include <tuple>
#include <cstring>
#include <algorithm>

bz_begin_namespace

class u8string : public collection_base<u8string>
{
public:
private:
	uint8_t *_data_begin;
	uint8_t *_data_end;
	uint8_t *_alloc_end;

	bool _is_short_string(void) const noexcept
	{ return (reinterpret_cast<uintptr_t>(this->_data_begin) & 0x1) || this->_data_begin == nullptr; }

	size_t _short_string_size(void) const noexcept
	{ return reinterpret_cast<uintptr_t>(this->_data_begin) >> 1; }

	void _set_short_string_size(size_t new_size) noexcept
	{ this->_data_begin = reinterpret_cast<uint8_t *>((new_size << 1) | 1); }

	static constexpr size_t _short_string_capacity(void) noexcept
	{ return (2 * sizeof(uint8_t *)) / sizeof(uint8_t); }

	uint8_t *_short_string_begin(void) noexcept
	{ return reinterpret_cast<uint8_t *>(&this->_data_end); }

	uint8_t const *_short_string_begin(void) const noexcept
	{ return reinterpret_cast<uint8_t const *>(&this->_data_end); }

	uint8_t *_short_string_end(void) noexcept
	{ return this->_short_string_begin() + this->_short_string_size(); }

	uint8_t const *_short_string_end(void) const noexcept
	{ return this->_short_string_begin() + this->_short_string_size(); }

	uint8_t *_begin_ptr(void) noexcept
	{ return this->_is_short_string() ? this->_short_string_begin() : this->_data_begin; }

	uint8_t const *_begin_ptr(void) const noexcept
	{ return this->_is_short_string() ? this->_short_string_begin() : this->_data_begin; }

	uint8_t *_end_ptr(void) noexcept
	{ return this->_is_short_string() ? this->_short_string_end() : this->_data_end; }

	uint8_t const *_end_ptr(void) const noexcept
	{ return this->_is_short_string() ? this->_short_string_end() : this->_data_end; }

	std::pair<uint8_t *, uint8_t *> _begin_end_pair(void) noexcept
	{
		using pair_t = std::pair<uint8_t *, uint8_t *>;
		return this->_is_short_string()
			? pair_t{ this->_short_string_begin(), this->_short_string_end() }
			: pair_t{ this->_data_begin, this->_data_end };
	}

	std::pair<uint8_t const *, uint8_t const *> _begin_end_pair(void) const noexcept
	{
		using pair_t = std::pair<uint8_t const *, uint8_t const *>;
		return this->_is_short_string()
			? pair_t{ this->_short_string_begin(), this->_short_string_end() }
			: pair_t{ this->_data_begin, this->_data_end };
	}

	void _power_of_two_reserve(size_t const new_cap)
	{
		// new_size must be bigger than _short_string_capacity() here
		auto const new_alloc = new uint8_t[new_cap];
		size_t size = 0;
		if (this->_is_short_string())
		{
			size = this->_short_string_size();
			std::memcpy(
				new_alloc,
				this->_short_string_begin(),
				size
			);
		}
		else
		{
			size = static_cast<size_t>(this->_data_end - this->_data_begin);
			std::memcpy(
				new_alloc,
				this->_data_begin,
				size
			);
			delete[] this->_data_begin;
		}
		this->_data_begin = new_alloc;
		this->_data_end = new_alloc + size;
		this->_alloc_end = new_alloc + new_cap;
	}

	void _no_null_clear(void) noexcept
	{
		if (this->_is_short_string())
		{
			return;
		}
		else
		{
			delete[] this->_data_begin;
		}
	}

	void _set_to_null(void) noexcept
	{
		this->_data_begin = nullptr;
		this->_data_end   = nullptr;
		this->_alloc_end  = nullptr;
	}

public:
	u8string(void)
		: _data_begin(nullptr), _data_end(nullptr), _alloc_end(nullptr)
	{}

	~u8string(void) noexcept
	{
		this->_no_null_clear();
	}

	u8string(u8string const &other)
		: u8string()
	{
		auto const [other_begin, other_end] = other._begin_end_pair();
		auto const size = static_cast<size_t>(other_end - other_begin);
		if (size <= _short_string_capacity() && other._is_short_string())
		{
			this->_data_begin = other._data_begin;
			this->_data_end   = other._data_end;
			this->_alloc_end  = other._alloc_end;
		}
		else if (size <= _short_string_capacity())
		{
			this->_set_short_string_size(size);
			std::memcpy(this->_short_string_begin(), other._data_begin, size);
		}
		else
		{
			this->_power_of_two_reserve(static_cast<size_t>(other._alloc_end - other._data_begin));
			std::memcpy(this->_data_begin, other._data_begin, size);
			this->_data_end = this->_data_begin + size;
		}
	}

	u8string(u8string &&other) noexcept
		: _data_begin(other._data_begin),
		  _data_end  (other._data_end),
		  _alloc_end (other._alloc_end)
	{
		other._set_to_null();
	}

	u8string(u8string_view const other)
		: u8string()
	{
		auto const size = other.size();
		if (size == 0)
		{
			// nothing
		}
		else if (size <= _short_string_capacity())
		{
			this->_set_short_string_size(size);
			std::memcpy(this->_short_string_begin(), other.data(), size);
		}
		else
		{
			this->reserve(size);
			std::memcpy(this->_data_begin, other.data(), size);
			this->_data_end = this->_data_begin + size;
		}
	}

	u8string(char const *c_str)
		: u8string(u8string_view(c_str))
	{}

	u8string(size_t length, u8char c)
		: u8string()
	{
		// single byte value
		if (c < (1u << 7))
		{
			this->reserve(length);
			std::memset(this->_begin_ptr(), static_cast<int>(c), length);
			if (this->_is_short_string())
			{
				this->_set_short_string_size(length);
			}
			else
			{
				this->_data_end = this->_data_begin + length;
			}
		}
		// two byte value
		else if (c < (1u << 11))
		{
			auto const size = 2 * length;
			auto const begin = this->_begin_ptr();
			auto const end   = begin + size;
			for (auto it = begin; it != end; it += 2)
			{
				*it       = static_cast<uint8_t>((c >> 6) | 0b1100'0000);
				*(it + 1) = static_cast<uint8_t>(((c >> 0) & 0b0011'1111) | 0b1000'0000);
			}
			if (this->_is_short_string())
			{
				this->_set_short_string_size(size);
			}
			else
			{
				this->_data_end = this->_data_begin + size;
			}
		}
		// three byte value
		else if (c < (1u << 16))
		{
			auto const size = 3 * length;
			auto const begin = this->_begin_ptr();
			auto const end   = begin + size;
			for (auto it = begin; it != end; it += 3)
			{
				*it       = static_cast<uint8_t>((c >> 12) | 0b1110'0000);
				*(it + 1) = static_cast<uint8_t>(((c >> 6) & 0b0011'1111) | 0b1000'0000);
				*(it + 2) = static_cast<uint8_t>(((c >> 0) & 0b0011'1111) | 0b1000'0000);
			}
			if (this->_is_short_string())
			{
				this->_set_short_string_size(size);
			}
			else
			{
				this->_data_end = this->_data_begin + size;
			}
		}
		// four byte value
		else
		{
			auto const size = 4 * length;
			auto const begin = this->_begin_ptr();
			auto const end   = begin + size;
			for (auto it = begin; it != end; it += 4)
			{
				*it       = static_cast<uint8_t>((c >> 18) | 0b1111'0000);
				*(it + 1) = static_cast<uint8_t>(((c >> 12) & 0b0011'1111) | 0b1000'0000);
				*(it + 2) = static_cast<uint8_t>(((c >> 6)  & 0b0011'1111) | 0b1000'0000);
				*(it + 3) = static_cast<uint8_t>(((c >> 0)  & 0b0011'1111) | 0b1000'0000);
			}
			if (this->_is_short_string())
			{
				this->_set_short_string_size(size);
			}
			else
			{
				this->_data_end = this->_data_begin + size;
			}
		}
	}

	u8string(u8iterator begin, u8iterator end)
		: u8string(u8string_view(begin, end))
	{}

	u8string &operator = (u8string const &rhs)
	{
		if (&rhs == this)
		{
			return *this;
		}
		this->_no_null_clear();
		auto const [other_begin, other_end] = rhs._begin_end_pair();
		auto const size = static_cast<size_t>(other_end - other_begin);
		if (size <= _short_string_capacity() && rhs._is_short_string())
		{
			this->_data_begin = rhs._data_begin;
			this->_data_end   = rhs._data_end;
			this->_alloc_end  = rhs._alloc_end;
		}
		else if (size <= _short_string_capacity())
		{
			this->_set_short_string_size(size);
			std::memcpy(this->_short_string_begin(), rhs._data_begin, size);
		}
		else
		{
			this->_set_to_null();
			this->_power_of_two_reserve(static_cast<size_t>(rhs._alloc_end - rhs._data_begin));
			std::memcpy(this->_data_begin, rhs._data_begin, size);
			this->_data_end = this->_data_begin + size;
		}

		return *this;
	}

	u8string &operator = (u8string &&rhs) noexcept
	{
		if (&rhs == this)
		{
			return *this;
		}
		this->_no_null_clear();
		this->_data_begin = rhs._data_begin;
		this->_data_end   = rhs._data_end;
		this->_alloc_end  = rhs._alloc_end;
		rhs._set_to_null();
		return *this;
	}

	u8string &operator = (u8string_view const rhs)
	{
		auto const size = rhs.size();
		if (size == 0)
		{
			this->clear();
			return *this;
		}
		this->reserve(size);
		if (this->_is_short_string())
		{
			this->_set_short_string_size(size);
			std::memcpy(this->_short_string_begin(), rhs.data(), size);
		}
		else
		{
			std::memcpy(this->_data_begin, rhs.data(), size);
			this->_data_end = this->_data_begin + size;
		}
		return *this;
	}

	u8string &operator = (char const *c_str)
	{
		return *this = u8string_view(c_str);
	}


	operator u8string_view (void) const noexcept
	{
		return this->_is_short_string()
			? u8string_view(this->_short_string_begin(), this->_short_string_end())
			: u8string_view(this->_data_begin, this->_data_end);
	}

	u8string_view as_string_view(void) const noexcept
	{ return static_cast<u8string_view>(*this); }

	bool verify(void) const noexcept
	{ return this->as_string_view().verify(); }

	size_t size(void) const noexcept
	{ return this->as_string_view().size(); }

	size_t length(void) const noexcept
	{ return this->as_string_view().length(); }

	size_t capacity(void) const noexcept
	{
		return this->_is_short_string()
			? _short_string_capacity()
			: static_cast<size_t>(this->_alloc_end - this->_data_begin);
	}


	void reserve(size_t const new_cap_)
	{
		if (new_cap_ <= this->capacity())
		{
			return;
		}

		size_t new_cap = 1;
		while (new_cap < new_cap_)
		{
			new_cap *= 2;
		}
		this->_power_of_two_reserve(new_cap);
	}

	void resize(size_t const new_size)
	{
		auto const current_size = this->size();
		if (current_size >= new_size)
		{
			this->_data_end -= current_size - new_size;
		}
		else
		{
			this->reserve(new_size);
			auto const size_diff = new_size - current_size;
			auto const old_end = this->_end_ptr();
			if (this->_is_short_string())
			{
				this->_set_short_string_size(new_size);
			}
			else
			{
				this->_data_end += size_diff;
			}
			std::memset(old_end, 0, size_diff);
		}
	}

	u8string &operator += (u8char const c)
	{
		auto const current_size = this->size();
		// single byte value
		if (c < (1u << 7))
		{
			this->reserve(current_size + 1);
			if (this->_is_short_string())
			{
				*this->_short_string_end() = static_cast<uint8_t>(c);
				this->_set_short_string_size(current_size + 1);
			}
			else
			{
				*this->_data_end = static_cast<uint8_t>(c);
				++this->_data_end;
			}
		}
		// two byte value
		else if (c < (1u << 11))
		{
			this->reserve(current_size + 2);
			if (this->_is_short_string())
			{
				auto const end = this->_short_string_end();
				*end = static_cast<uint8_t>((c >> 6) | 0b1100'0000);
				*(end + 1) = static_cast<uint8_t>((c & 0b0011'1111) | 0b1000'0000);
				this->_set_short_string_size(current_size + 2);
			}
			else
			{
				auto const end = this->_data_end;
				*end = static_cast<uint8_t>((c >> 6) | 0b1100'0000);
				*(end + 1) = static_cast<uint8_t>((c & 0b0011'1111) | 0b1000'0000);
				this->_data_end += 2;
			}
		}
		// three byte value
		else if (c < (1u << 16))
		{
			this->reserve(current_size + 3);
			if (this->_is_short_string())
			{
				auto const end = this->_short_string_end();
				*end       = static_cast<uint8_t>((c >> 12) | 0b1110'0000);
				*(end + 1) = static_cast<uint8_t>(((c >> 6) & 0b0011'1111) | 0b1000'0000);
				*(end + 2) = static_cast<uint8_t>(((c >> 0) & 0b0011'1111) | 0b1000'0000);
				this->_set_short_string_size(current_size + 3);
			}
			else
			{
				auto const end = this->_data_end;
				*end       = static_cast<uint8_t>((c >> 12) | 0b1110'0000);
				*(end + 1) = static_cast<uint8_t>(((c >> 6) & 0b0011'1111) | 0b1000'0000);
				*(end + 2) = static_cast<uint8_t>(((c >> 0) & 0b0011'1111) | 0b1000'0000);
				this->_data_end += 3;
			}
		}
		// four byte value
		else if (c < (1u << 21))
		{
			this->reserve(current_size + 4);
			if (this->_is_short_string())
			{
				auto const end = this->_short_string_end();
				*end       = static_cast<uint8_t>((c >> 18) | 0b1111'0000);
				*(end + 1) = static_cast<uint8_t>(((c >> 12) & 0b0011'1111) | 0b1000'0000);
				*(end + 2) = static_cast<uint8_t>(((c >> 6)  & 0b0011'1111) | 0b1000'0000);
				*(end + 3) = static_cast<uint8_t>(((c >> 0)  & 0b0011'1111) | 0b1000'0000);
				this->_set_short_string_size(current_size + 4);
			}
			else
			{
				auto const end = this->_data_end;
				*end       = static_cast<uint8_t>((c >> 18) | 0b1111'0000);
				*(end + 1) = static_cast<uint8_t>(((c >> 12) & 0b0011'1111) | 0b1000'0000);
				*(end + 2) = static_cast<uint8_t>(((c >> 6)  & 0b0011'1111) | 0b1000'0000);
				*(end + 3) = static_cast<uint8_t>(((c >> 0)  & 0b0011'1111) | 0b1000'0000);
				this->_data_end += 4;
			}
		}

		return *this;
	}

	u8string &operator += (u8string_view const rhs)
	{
		auto const rhs_size = rhs.size();
		auto const new_size = this->size() + rhs_size;
		this->reserve(new_size);
		if (rhs_size != 0)
		{
			std::memcpy(this->_end_ptr(), rhs.data(), rhs_size);
		}
		if (this->_is_short_string())
		{
			this->_set_short_string_size(new_size);
		}
		else
		{
			this->_data_end += rhs_size;
		}
		return *this;
	}

	u8string &operator += (u8string const &rhs)
	{
		return *this += rhs.as_string_view();
	}

	u8string &operator += (char const *rhs)
	{
		return *this += u8string_view(rhs);
	}


	void clear(void) noexcept
	{
		if (this->_is_short_string())
		{
			this->_set_short_string_size(0);
		}
		else
		{
			this->_data_end = this->_data_begin;
		}
	}

	void erase(u8char c) noexcept
	{
		if (c <= internal::max_one_byte_char)
		{
			auto [it, end] = this->_begin_end_pair();
			auto trail = it;
			while (true)
			{
				while (it != end && *it == c)
				{
					++it;
				}
				while (it != end && *it != c)
				{
					*trail = *it;
					++it, ++trail;
				}
				if (it == end)
				{
					break;
				}
			}

			if (this->_is_short_string())
			{
				this->_set_short_string_size(static_cast<size_t>(trail - this->_short_string_begin()));
			}
			else
			{
				this->_data_end = trail;
			}
		}
		else
		{
			uint8_t encoded_char[4] = {};
			int char_size = 0;
			if (c <= internal::max_two_byte_char)
			{
				char_size = 2;
				encoded_char[0] = static_cast<uint8_t>(0b1100'0000 | (c >> 6));
				encoded_char[1] = static_cast<uint8_t>(0b1000'0000 | ((c >> 0) & 0b0011'1111));
			}
			else if (c <= internal::max_three_byte_char)
			{
				char_size = 3;
				encoded_char[0] = static_cast<uint8_t>(0b1110'0000 | (c >> 12));
				encoded_char[1] = static_cast<uint8_t>(0b1000'0000 | ((c >> 6) & 0b0011'1111));
				encoded_char[2] = static_cast<uint8_t>(0b1000'0000 | ((c >> 0) & 0b0011'1111));
			}
			else
			{
				char_size = 4;
				encoded_char[0] = static_cast<uint8_t>(0b1111'0000 | (c >> 18));
				encoded_char[1] = static_cast<uint8_t>(0b1000'0000 | ((c >> 12) & 0b0011'1111));
				encoded_char[2] = static_cast<uint8_t>(0b1000'0000 | ((c >>  6) & 0b0011'1111));
				encoded_char[3] = static_cast<uint8_t>(0b1000'0000 | ((c >>  0) & 0b0011'1111));
			}

			auto const is_char = [
				begin = encoded_char,
				end   = encoded_char + char_size
			](uint8_t const *ptr)
			{
				for (auto it = begin; it != end; ++it, ++ptr)
				{
					if (*it != *ptr)
					{
						return false;
					}
				}
				return true;
			};

			auto [it, end] = this->_begin_end_pair();
			auto trail = it;
			while (true)
			{
				while (it != end && is_char(it))
				{
					++it;
				}
				while (it != end && !is_char(it))
				{
					*trail = *it;
					++it, ++trail;
				}
				if (it == end)
				{
					break;
				}
			}

			if (this->_is_short_string())
			{
				this->_set_short_string_size(static_cast<size_t>(trail - this->_short_string_begin()));
			}
			else
			{
				this->_data_end = trail;
			}
		}
	}


	using iterator       = u8iterator;
	using const_iterator = u8iterator;

	uint8_t *data(void) noexcept
	{ return this->_begin_ptr(); }

	uint8_t const *data(void) const noexcept
	{ return this->_begin_ptr(); }

	char *data_as_char_ptr(void) noexcept
	{ return reinterpret_cast<char *>(this->data()); }

	char const *data_as_char_ptr(void) const noexcept
	{ return reinterpret_cast<char const *>(this->data()); }

	const_iterator begin(void) const noexcept
	{ return const_iterator(this->_is_short_string() ? this->_short_string_begin() : this->_data_begin); }

	const_iterator end(void) const noexcept
	{ return const_iterator(this->_is_short_string() ? this->_short_string_end() : this->_data_end); }

	const_iterator cbegin(void) const noexcept
	{ return const_iterator(this->_is_short_string() ? this->_short_string_begin() : this->_data_begin); }

	const_iterator cend(void) const noexcept
	{ return const_iterator(this->_is_short_string() ? this->_short_string_end() : this->_data_end); }

	const_iterator find(u8char c) const noexcept
	{ return this->as_string_view().find(c); }

	const_iterator find(const_iterator it, u8char c) const noexcept
	{ return this->as_string_view().find(it, c); }

	const_iterator find(u8string_view str) const noexcept
	{ return this->as_string_view().find(str); }

	const_iterator find(const_iterator it, u8string_view str) const noexcept
	{ return this->as_string_view().find(it, str); }

	const_iterator find_any(u8string_view str) const noexcept
	{ return this->as_string_view().find_any(str); }

	const_iterator find_any(const_iterator it, u8string_view str) const noexcept
	{ return this->as_string_view().find_any(it, str); }

	const_iterator rfind(u8char c) const noexcept
	{ return this->as_string_view().rfind(c); }

	const_iterator rfind_any(u8string_view str) const noexcept
	{ return this->as_string_view().rfind_any(str); }

	bool contains(u8char c) const noexcept
	{ return this->as_string_view().contains(c); }

	bool contains(u8string_view str) const noexcept
	{ return this->as_string_view().contains(str); }

	bool contains_any(u8string_view str) const noexcept
	{ return this->as_string_view().contains_any(str); }


	void reverse(void) noexcept
	{
		auto const [begin_ptr, end_ptr] = this->_begin_end_pair();
		std::reverse(begin_ptr, end_ptr);
		for (auto it = begin_ptr; it != end_ptr;)
		{
			if ((*it & 0b1000'0000) != 0)
			{
				auto code_point_end = it;
				while (code_point_end != end_ptr && (*code_point_end & 0b1000'0000) != 0)
				{
					++code_point_end;
				}
				std::reverse(it, code_point_end);
				it = code_point_end;
			}
			else
			{
				++it;
			}
		}
	}

	u8string reversed(void) const noexcept
	{
		auto copy = *this;
		copy.reverse();
		return copy;
	}

	u8string_view substring(size_t start_index, size_t end_index) const noexcept
	{ return this->as_string_view().substring(start_index, end_index); }

	u8string_view substring(size_t start_index) const noexcept
	{ return this->as_string_view().substring(start_index); }

	bool starts_with(u8string_view str) const noexcept
	{ return this->as_string_view().starts_with(str); }

	bool ends_with(u8string_view str) const noexcept
	{ return this->as_string_view().ends_with(str); }
};

bz_end_namespace

#endif // _bz_u8string_h_
