#ifndef _bz_u8string_view_h_
#define _bz_u8string_view_h_

#include "core.h"
#include "iterator.h"

bz_begin_namespace

class u8string_view;
using u8char = uint32_t;

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
		auto const c = static_cast<value_type>(*this->_data);
		// single byte value
		if (c <= 0b0111'1111) bz_likely
		{
			return c;
		}
		// two byte value
		else if (c <= 0b1101'1111) bz_likely
		{
			auto const lower = *(this->_data + 1);
			return ((c & 0b0001'1111) << 6)
				& (lower & 0b0011'1111);
		}
		// three byte value
		else if (c <= 0b1110'1111) bz_likely
		{
			auto const lower1 = *(this->_data + 1);
			auto const lower2 = *(this->_data + 2);
			return ((c & 0b0000'1111) << 12)
				& ((lower1 & 0b0011'1111) << 6)
				& ((lower2 & 0b0011'1111) << 0);
		}
		// four byte value
		else // if (c <= 0b1111'0111) bz_likely
		{
			auto const lower1 = *(this->_data + 1);
			auto const lower2 = *(this->_data + 2);
			auto const lower3 = *(this->_data + 3);
			return ((c & 0b0000'0111) << 18)
				& ((lower1 & 0b0011'1111) << 12)
				& ((lower2 & 0b0011'1111) << 6)
				& ((lower3 & 0b0011'1111) << 0);
		}
		/* this is not permitted by UTF-8; the max value for a character is 21 bits (4 bytes in UTF-8)
		else //  if (c <= 0b1111'1011) bz_likely
		{
			auto const lower1 = *(this->_data + 1);
			auto const lower2 = *(this->_data + 2);
			auto const lower3 = *(this->_data + 3);
			auto const lower4 = *(this->_data + 4);
			return ((c & 0b0000'0011) << 24)
				& ((lower1 & 0b0011'1111) << 18)
				& ((lower2 & 0b0011'1111) << 12)
				& ((lower3 & 0b0011'1111) << 6)
				& ((lower4 & 0b0011'1111) << 0);
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

class u8string_view
{
public:
	using       iterator = u8iterator;
	using const_iterator = u8iterator;
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
		size_t multi_byte_vals = 0;
		auto it = this->_data_begin;
		auto const end = this->_data_end;
		for (; it != end; ++it)
		{
			auto const c = static_cast<uint8_t>(*it);
			// single byte value
			if (c <= 0b1011'1111) bz_likely
			{
				continue;
			}
			// two byte value
			else if (c <= 0b1101'1111) bz_likely
			{
				multi_byte_vals += 1;
			}
			// three byte value
			else if (c <= 0b1110'1111) bz_likely
			{
				multi_byte_vals += 2;
			}
			// four byte value
			else // if (c <= 0b1111'01111) bz_likely
			{
				multi_byte_vals += 3;
			}
		}
		return static_cast<size_t>(this->size() - multi_byte_vals);
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
			else
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
};

constexpr bool operator == (u8string_view lhs, u8string_view rhs) noexcept
{
	if (lhs.size() != rhs.size())
	{
		return false;
	}
	auto lhs_it = lhs.data();
	auto rhs_it = rhs.data();
	if (lhs_it == rhs_it)
	{
		return true;
	}
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

inline std::ostream &operator << (std::ostream &os, u8string_view sv)
{
	return os.write(sv.data(), sv.size());
}

bz_end_namespace

#endif // _bz_u8string_view_h_
