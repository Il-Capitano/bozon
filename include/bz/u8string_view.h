#ifndef _bz_u8string_view_h_
#define _bz_u8string_view_h_

#include "core.h"
#include "iterator.h"
#include "ranges.h"

bz_begin_namespace

class u8string_view;
using u8char = uint32_t;

constexpr u8char max_unicode_value = 0x10'ffff;

constexpr bool is_in_unicode_surrogate_range(u8char c)
{
	return c >= 0xd800 && c <= 0xdfff;
}

constexpr bool is_valid_unicode_value(u8char c)
{
	return c <= max_unicode_value && !is_in_unicode_surrogate_range(c);
}

namespace internal
{

constexpr u8char max_one_byte_char   = (1u <<  7) - 1;
constexpr u8char max_two_byte_char   = (1u << 11) - 1;
constexpr u8char max_three_byte_char = (1u << 16) - 1;
constexpr u8char max_four_byte_char  = (1u << 21) - 1;

} // namespace internal

struct u8iterator
{
private:
	using self_t = u8iterator;
public:
	using iterator_category = std::input_iterator_tag;
	using value_type = u8char;
	using reference = value_type;
	using pointer = void;
	using difference_type = void;
private:
	char const *_data;

public:
	constexpr u8iterator(void) noexcept
		: _data(nullptr)
	{}

	constexpr explicit u8iterator(char const *const data) noexcept
		: _data(data)
	{}

	explicit u8iterator(uint8_t const *const data) noexcept
		: _data(reinterpret_cast<char const *>(data))
	{}

	constexpr char const *data(void) const noexcept
	{ return this->_data; }

	constexpr self_t &operator ++ (void) noexcept
	{
		auto const c = static_cast<uint8_t>(*this->_data);
		// single byte value
		if (c <= 0b0111'1111) bz_likely
		{
			this->_data += 1;
		}
		// two byte value
		else if (c <= 0b1101'1111) bz_likely
		{
			this->_data += 2;
		}
		// three byte value
		else if (c <= 0b1110'1111) bz_likely
		{
			this->_data += 3;
		}
		// four byte value
		else // if (c <= 0b1111'0111) bz_likely
		{
			this->_data += 4;
		}
//		// this is not permitted by UTF-8; the max value for a character is 21 bits (4 bytes of data)
//		else // if (c <= 0b1111'1011) bz_likely
//		{
//			this->_data += 5;
//		}
		return *this;
	}

	constexpr value_type operator * (void) const noexcept
	{
		auto const c = static_cast<value_type>(static_cast<uint8_t>(*this->_data));
		// single byte value
		if (c <= 0b0111'1111) bz_likely
		{
			return c;
		}
		// two byte value
		else if (c <= 0b1101'1111) bz_likely
		{
			auto const lower = static_cast<value_type>(static_cast<uint8_t>(*(this->_data + 1)));
			return ((c & 0b0001'1111) << 6)
				| (lower & 0b0011'1111);
		}
		// three byte value
		else if (c <= 0b1110'1111) bz_likely
		{
			auto const lower1 = static_cast<value_type>(static_cast<uint8_t>(*(this->_data + 1)));
			auto const lower2 = static_cast<value_type>(static_cast<uint8_t>(*(this->_data + 2)));
			return ((c & 0b0000'1111) << 12)
				| ((lower1 & 0b0011'1111) << 6)
				| ((lower2 & 0b0011'1111) << 0);
		}
		// four byte value
		else // if (c <= 0b1111'0111) bz_likely
		{
			auto const lower1 = static_cast<value_type>(static_cast<uint8_t>(*(this->_data + 1)));
			auto const lower2 = static_cast<value_type>(static_cast<uint8_t>(*(this->_data + 2)));
			auto const lower3 = static_cast<value_type>(static_cast<uint8_t>(*(this->_data + 3)));
			return ((c & 0b0000'0111) << 18)
				| ((lower1 & 0b0011'1111) << 12)
				| ((lower2 & 0b0011'1111) << 6)
				| ((lower3 & 0b0011'1111) << 0);
		}
		/* this is not permitted by UTF-8; the max value for a character is 21 bits (4 bytes in UTF-8)
		else //  if (c <= 0b1111'1011) bz_likely
		{
			auto const lower1 = *(this->_data + 1);
			auto const lower2 = *(this->_data + 2);
			auto const lower3 = *(this->_data + 3);
			auto const lower4 = *(this->_data + 4);
			return ((c & 0b0000'0011) << 24)
				| ((lower1 & 0b0011'1111) << 18)
				| ((lower2 & 0b0011'1111) << 12)
				| ((lower3 & 0b0011'1111) << 6)
				| ((lower4 & 0b0011'1111) << 0);
		}
		*/
	}

#define def_cmp_operator(op)                                        \
constexpr friend bool operator op (self_t lhs, self_t rhs) noexcept \
{ return lhs._data op rhs._data; }

def_cmp_operator(==)
def_cmp_operator(!=)
def_cmp_operator(<)
def_cmp_operator(<=)
def_cmp_operator(>)
def_cmp_operator(>=)

#undef def_cmp_operator

	constexpr friend self_t operator + (self_t lhs, size_t rhs) noexcept
	{
		for (size_t i = 0; i < rhs; ++i)
		{
			++lhs;
		}
		return lhs;
	}
};

class u8string_view : collection_base<u8string_view>
{
public:
	using       iterator = u8iterator;
	using const_iterator = u8iterator;
	using char_type      = u8char;
private:
	char const *_data_begin;
	char const *_data_end;

public:
	constexpr explicit u8string_view(void) noexcept
		: _data_begin(nullptr), _data_end(nullptr)
	{}

	constexpr explicit u8string_view(char const *const data_begin, char const *const data_end) noexcept
		: _data_begin(data_begin), _data_end(data_end)
	{}

	explicit u8string_view(uint8_t const *const data_begin, uint8_t const *const data_end) noexcept
		: _data_begin(reinterpret_cast<char const *>(data_begin)),
		  _data_end  (reinterpret_cast<char const *>(data_end))
	{}

	constexpr u8string_view(iterator const begin, iterator const end)
		: _data_begin(begin.data()), _data_end(end.data())
	{}

	constexpr u8string_view(char const *c_str) noexcept
		: _data_begin(c_str),
		  _data_end  (c_str)
	{
		if (c_str == nullptr)
		{
			return;
		}
		while (*c_str != '\0')
		{
			++c_str;
		}
		this->_data_end = c_str;
	}


	constexpr char const *data(void) const noexcept
	{ return this->_data_begin; }


	constexpr size_t size(void) const noexcept
	{ return static_cast<size_t>(this->_data_end - this->_data_begin); }

	constexpr size_t length(void) const noexcept
	{
		size_t extension_byte_count = 0;
		auto it = this->_data_begin;
		auto const end = this->_data_end;
		for (; it != end; ++it)
		{
			auto const c = static_cast<uint8_t>(*it);
			if ((c & 0b1100'0000) == 0b1000'0000)
			{
				++extension_byte_count;
			}
		}
		return static_cast<size_t>(this->size() - extension_byte_count);
	}

	// returns false if the string is invalid UTF-8
	constexpr bool verify(void) const noexcept
	{
		auto it = this->_data_begin;
		auto const end = this->_data_end;
		for (; it != end; ++it)
		{
			auto const c = static_cast<uint8_t>(*it);
			// single byte value
			if (c <= 0b0111'1111) bz_likely
			{
				continue;
			}
			// two byte value
			else if (c <= 0b1101'1111) bz_likely
			{
				// verify the second byte
				++it;
				if (
					c < 0b1100'0010                        // the value needs to be at least 8 bits
					|| it == end                           // the byte must be part of the string
					|| (*it & 0b1100'0000) != 0b1000'0000  // the byte must start with 10
				) bz_unlikely
				{
					return false;
				}
			}
			// three byte value
			else if (c <= 0b1110'1111) bz_likely
			{
				// verify the second byte
				++it;
				if (
					c < 0b1110'0000                        // the value needs to be at least 12 bits (the data in the first byte may be 0)
					|| it == end                           // the byte must be part of the string
					|| (*it & 0b1100'0000) != 0b1000'0000  // the byte must start with 10
				) bz_unlikely
				{
					return false;
				}

				// verify the third byte
				++it;
				if (
					it == end                              // the byte must be part of the string
					|| (*it & 0b1100'0000) != 0b1000'0000  // the byte must start with 10
				) bz_unlikely
				{
					return false;
				}
			}
			// four byte value
			else if (c <= 0b1111'0111) bz_likely
			{
				// verify the second byte
				++it;
				if (
					c < 0b1110'0000                        // the value needs to be at least 12 bits (the data in the first byte may be 0)
					|| it == end                           // the byte must be part of the string
					|| (*it & 0b1100'0000) != 0b1000'0000  // the byte must start with 10
				) bz_unlikely
				{
					return false;
				}

				// verify the third byte
				++it;
				if (
					it == end                              // the byte must be part of the string
					|| (*it & 0b1100'0000) != 0b1000'0000  // the byte must start with 10
				) bz_unlikely
				{
					return false;
				}

				// verify the fourth byte
				++it;
				if (
					it == end                              // the byte must be part of the string
					|| (*it & 0b1100'0000) != 0b1000'0000  // the byte must start with 10
				) bz_unlikely
				{
					return false;
				}
			}
			else
			{
				return false;
			}
		}
		return true;
	}

	constexpr bool is_ascii(void) const noexcept
	{
		auto const end = this->_data_end;
		for (auto it = this->_data_begin; it != end; ++it)
		{
			if ((static_cast<uint8_t>(*it) & 0b1000'0000) != 0)
			{
				return false;
			}
		}
		return true;
	}


	constexpr const_iterator begin(void) const noexcept
	{ return const_iterator(this->_data_begin); }

	constexpr const_iterator end(void) const noexcept
	{ return const_iterator(this->_data_end); }

	constexpr const_iterator cbegin(void) const noexcept
	{ return const_iterator(this->_data_begin); }

	constexpr const_iterator cend(void) const noexcept
	{ return const_iterator(this->_data_end); }


	constexpr u8string_view substring(size_t begin_index, size_t end_index) const noexcept
	{
		auto it = this->begin();
		auto const end = this->end();
		size_t i = 0;
		for (; i != begin_index && it != end; ++i)
		{
			++it;
		}
		auto const substring_begin = it;
		if (end_index > this->size())
		{
			return u8string_view(substring_begin, end);
		}

		for (; i != end_index && it != end; ++i)
		{
			++it;
		}
		auto const substring_end = it;
		return u8string_view(substring_begin, substring_end);
	}

	constexpr u8string_view substring(size_t begin_index) const noexcept
	{
		auto it = this->begin();
		auto const end = this->end();
		for (size_t i = 0; i != begin_index && it != end; ++i)
		{
			++it;
		}

		return u8string_view(it, end);
	}

	constexpr const_iterator find(const_iterator it_, char_type c) const noexcept
	{
		auto it = it_.data();
		bz_assert(it >= this->_data_begin && it <= this->_data_end);
		auto const end = this->_data_end;
		if (c <= internal::max_one_byte_char)
		{
			while (it != end)
			{
				if (static_cast<uint8_t>(*it) == c)
				{
					return const_iterator(it);
				}
				else
				{
					++it;
				}
			}
		}
		else
		{
			char encoded_char[4] = {};
			int char_size = 0;
			if (c <= internal::max_two_byte_char)
			{
				char_size = 2;
				encoded_char[0] = static_cast<char>(0b1100'0000 | (c >> 6));
				encoded_char[1] = static_cast<char>(0b1000'0000 | ((c >> 0) & 0b0011'1111));
			}
			else if (c <= internal::max_three_byte_char)
			{
				char_size = 3;
				encoded_char[0] = static_cast<char>(0b1110'0000 | (c >> 12));
				encoded_char[1] = static_cast<char>(0b1000'0000 | ((c >> 6) & 0b0011'1111));
				encoded_char[2] = static_cast<char>(0b1000'0000 | ((c >> 0) & 0b0011'1111));
			}
			else
			{
				char_size = 4;
				encoded_char[0] = static_cast<char>(0b1111'0000 | (c >> 18));
				encoded_char[1] = static_cast<char>(0b1000'0000 | ((c >> 12) & 0b0011'1111));
				encoded_char[2] = static_cast<char>(0b1000'0000 | ((c >>  6) & 0b0011'1111));
				encoded_char[3] = static_cast<char>(0b1000'0000 | ((c >>  0) & 0b0011'1111));
			}

			auto const is_char = [
				begin = encoded_char,
				end   = encoded_char + char_size
			](char const *ptr)
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

			while (it != end)
			{
				if (is_char(it))
				{
					return const_iterator(it);
				}
				else
				{
					++it;
				}
			}
		}
		return const_iterator(end);
	}

	constexpr const_iterator find(char_type c) const noexcept
	{ return this->find(this->begin(), c); }

	constexpr const_iterator find(const_iterator it_, u8string_view str) const noexcept
	{
		auto it = it_.data();
		auto const end = this->_data_end;
		if (str.size() == 1)
		{
			return this->find(it_, static_cast<char_type>(*str.data()));
		}

		auto const is_char = [
			begin = str.data(),
			end   = str.data() + str.size()
		](char const *ptr)
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

		while (it != end)
		{
			if (is_char(it))
			{
				return const_iterator(it);
			}
			else
			{
				++it;
			}
		}

		return const_iterator(end);
	}

	constexpr const_iterator find(u8string_view str) const noexcept
	{ return this->find(this->begin(), str); }

	constexpr const_iterator find_any(const_iterator it_, u8string_view str) const noexcept
	{
		bz_assert(it_ >= this->begin() && it_ <= this->end());
		if (str.is_ascii())
		{
			auto it = it_.data();
			auto const end = this->_data_end;
			auto const str_begin = str._data_begin;
			auto const str_end   = str._data_end;

			for (; it != end; ++it)
			{
				for (auto str_it = str_begin; str_it != str_end; ++str_it)
				{
					if (*it == *str_it)
					{
						return const_iterator(it);
					}
				}
			}
			return const_iterator(end);
		}
		else
		{
			auto it = it_;
			auto const end = this->end();
			auto const str_begin = str.begin();
			auto const str_end   = str.end();

			for (; it != end; ++it)
			{
				for (auto str_it = str_begin; str_it != str_end; ++str_it)
				{
					if (*it == *str_it)
					{
						return it;
					}
				}
			}
			return end;
		}
	}

	constexpr const_iterator find_any(u8string_view str) const noexcept
	{ return this->find_any(this->begin(), str); }

	constexpr const_iterator rfind(char_type c) const noexcept
	{
		auto it = this->_data_end;
		auto const begin = this->_data_begin;
		if (c <= internal::max_one_byte_char)
		{
			while (it != begin)
			{
				--it;
				if (static_cast<uint8_t>(*it) == c)
				{
					return const_iterator(it);
				}
			}
		}
		else
		{
			char encoded_char[4] = {};
			int char_size = 0;
			if (c <= internal::max_two_byte_char)
			{
				char_size = 2;
				encoded_char[0] = static_cast<char>(0b1100'0000 | (c >> 6));
				encoded_char[1] = static_cast<char>(0b1000'0000 | ((c >> 0) & 0b0011'1111));
			}
			else if (c <= internal::max_three_byte_char)
			{
				char_size = 3;
				encoded_char[0] = static_cast<char>(0b1110'0000 | (c >> 12));
				encoded_char[1] = static_cast<char>(0b1000'0000 | ((c >> 6) & 0b0011'1111));
				encoded_char[2] = static_cast<char>(0b1000'0000 | ((c >> 0) & 0b0011'1111));
			}
			else
			{
				char_size = 4;
				encoded_char[0] = static_cast<char>(0b1111'0000 | (c >> 18));
				encoded_char[1] = static_cast<char>(0b1000'0000 | ((c >> 12) & 0b0011'1111));
				encoded_char[2] = static_cast<char>(0b1000'0000 | ((c >>  6) & 0b0011'1111));
				encoded_char[3] = static_cast<char>(0b1000'0000 | ((c >>  0) & 0b0011'1111));
			}

			auto const is_char = [
				begin = encoded_char,
				end   = encoded_char + char_size
			](char const *ptr)
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

			while (it != begin)
			{
				--it;
				if (is_char(it))
				{
					return const_iterator(it);
				}
			}
		}
		return const_iterator(static_cast<char const *>(nullptr));
	}

	constexpr const_iterator rfind_any(u8string_view str) const noexcept
	{
		if (str.is_ascii())
		{
			auto const str_begin = str._data_begin;
			auto const str_end   = str._data_end;
			auto it = this->_data_end;
			auto const begin = this->_data_begin;

			while (it != begin)
			{
				--it;
				for (auto str_it = str_begin; str_it != str_end; ++str_it)
				{
					if (*it == *str_it)
					{
						return const_iterator(it);
					}
				}
			}
		}
		else
		{
			auto const is_char = [str_begin = str._data_begin, str_end = str._data_end](char const *ptr) {
				uint8_t const first_c = static_cast<uint8_t>(*ptr);
				if ((first_c & 0b1100'0000) == 0b1000'0000)
				{
					// continuation byte
					return false;
				}
				auto const size = (first_c & 0b1000'0000) == 0b0000'0000 ? 1 :
					(first_c & 0b1110'0000) == 0b1100'0000 ? 2 :
					(first_c & 0b1111'0000) == 0b1110'0000 ? 3 :
					4;
				auto const self_begin = ptr;
				auto const self_end = ptr + size;
				for (auto str_it = str_begin; str_it != str_end; ++str_it)
				{
					if (bz::u8string_view(str_it, str_end).starts_with(bz::u8string_view(self_begin, self_end)))
					{
						return true;
					}
				}
				return false;
			};

			auto it = this->_data_end;
			auto const begin = this->_data_begin;
			while (it != begin)
			{
				--it;
				if (is_char(it))
				{
					return const_iterator(it);
				}
			}
		}
		return const_iterator(static_cast<char const *>(nullptr));
	}

	constexpr bool starts_with(u8string_view str) const noexcept
	{
		if (str.size() > this->size())
		{
			return false;
		}

		auto self_it = this->_data_begin;
		auto str_it  = str._data_begin;
		auto const str_end = str._data_end;
		for (; str_it != str_end; ++str_it, ++self_it)
		{
			if (*self_it != *str_it)
			{
				return false;
			}
		}
		return true;
	}

	constexpr bool starts_with(char_type c) const noexcept
	{
		if (this->_data_begin == this->_data_end)
		{
			return false;
		}
		auto const begin = this->begin();
		return *begin == c;
	}

	constexpr bool ends_with(u8string_view str) const noexcept
	{
		auto const str_size = str.size();
		if (str_size > this->size())
		{
			return false;
		}

		auto self_it = this->_data_end - str_size;
		auto str_it  = str._data_begin;
		auto const str_end = str._data_end;
		for (; str_it != str_end; ++str_it, ++self_it)
		{
			if (*self_it != *str_it)
			{
				return false;
			}
		}
		return true;
	}

	constexpr bool ends_with(char_type c) const noexcept
	{
		if (this->_data_begin == this->_data_end)
		{
			return false;
		}
		else if (c <= internal::max_one_byte_char)
		{
			return *(this->_data_end - 1) == (char)c;
		}

		char encoded_char[4] = {};
		std::size_t char_size = 0;
		if (c <= internal::max_two_byte_char)
		{
			char_size = 2;
			encoded_char[0] = static_cast<char>(0b1100'0000 | (c >> 6));
			encoded_char[1] = static_cast<char>(0b1000'0000 | ((c >> 0) & 0b0011'1111));
		}
		else if (c <= internal::max_three_byte_char)
		{
			char_size = 3;
			encoded_char[0] = static_cast<char>(0b1110'0000 | (c >> 12));
			encoded_char[1] = static_cast<char>(0b1000'0000 | ((c >> 6) & 0b0011'1111));
			encoded_char[2] = static_cast<char>(0b1000'0000 | ((c >> 0) & 0b0011'1111));
		}
		else
		{
			char_size = 4;
			encoded_char[0] = static_cast<char>(0b1111'0000 | (c >> 18));
			encoded_char[1] = static_cast<char>(0b1000'0000 | ((c >> 12) & 0b0011'1111));
			encoded_char[2] = static_cast<char>(0b1000'0000 | ((c >>  6) & 0b0011'1111));
			encoded_char[3] = static_cast<char>(0b1000'0000 | ((c >>  0) & 0b0011'1111));
		}
		return this->ends_with(u8string_view(encoded_char, encoded_char + char_size));
	}

	constexpr bool contains(char_type c) const noexcept
	{ return this->find(c) != this->end(); }

	constexpr bool contains(u8string_view str) const noexcept
	{ return this->find(str) != this->end(); }

	constexpr bool contains_any(u8string_view str) const noexcept
	{ return this->find_any(str) != this->end(); }

	constexpr size_t count_chars(char_type c) const noexcept
	{
		if (c <= internal::max_one_byte_char)
		{
			size_t result = 0;
			auto it = this->_data_begin;
			auto const end = this->_data_end;
			for (; it != end; ++it)
			{
				if (static_cast<char_type>(*it) == c)
				{
					++result;
				}
			}
			return result;
		}
		else
		{
			size_t result = 0;
			auto it = this->begin();
			auto const end = this->end();
			for (; it != end; ++it)
			{
				if (*it == c)
				{
					++result;
				}
			}
			return result;
		}
	}
};

constexpr bool operator == (u8string_view lhs, u8string_view rhs) noexcept
{
	if (lhs.size() != rhs.size())
	{
		return false;
	}
	auto lhs_it = lhs.data();
	auto rhs_it = rhs.data();
	/*
	// clang thinks this is not a constant expression, GCC accepts it fine, see https://godbolt.org/z/vdofz3
	if (lhs_it == rhs_it)
	{
		return true;
	}
	*/

	auto const lhs_end = lhs_it + lhs.size();
	for (; lhs_it != lhs_end; ++lhs_it, ++rhs_it)
	{
		if (*lhs_it != *rhs_it)
		{
			return false;
		}
	}
	return true;
}

constexpr bool operator == (u8string_view lhs, char const *rhs) noexcept
{ return lhs == u8string_view(rhs); }

constexpr bool operator == (char const *lhs, u8string_view rhs) noexcept
{ return u8string_view(lhs) == rhs; }

constexpr bool constexpr_equals(u8string_view lhs, u8string_view rhs) noexcept
{
	if (lhs.size() != rhs.size())
	{
		return false;
	}

	auto lhs_it = lhs.data();
	auto rhs_it = rhs.data();
	auto const lhs_end = lhs.data() + lhs.size();

	for (; lhs_it != lhs_end; ++lhs_it, ++rhs_it)
	{
		if (*lhs_it != *rhs_it)
		{
			return false;
		}
	}
	return true;
}

constexpr bool operator != (u8string_view lhs, u8string_view rhs) noexcept
{ return !(lhs == rhs); }

constexpr bool operator != (u8string_view lhs, char const *rhs) noexcept
{ return lhs != u8string_view(rhs); }

constexpr bool operator != (char const *lhs, u8string_view rhs) noexcept
{ return u8string_view(lhs) != rhs; }

constexpr bool constexpr_not_equals(u8string_view lhs, u8string_view rhs) noexcept
{ return !constexpr_equals(lhs, rhs); }

inline std::ostream &operator << (std::ostream &os, u8string_view sv)
{
	return os.write(sv.data(), static_cast<std::streamsize>(sv.size()));
}

bz_end_namespace

#endif // _bz_u8string_view_h_
