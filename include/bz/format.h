#ifndef _bz_format_h__
#define _bz_format_h__

#include "core.h"
#include "string.h"
#include "vector.h"

#include <utility>
#include <cassert>
#include <cmath>
#include <iostream>

bz_begin_namespace

template<typename ...Ts>
string format(string_view fmt, Ts const &...ts);

template<typename ...Ts>
void printf(string_view fmt, Ts const &...ts)
{
	std::cout << format(fmt, ts...);
}

template<typename ...Ts>
void printf(std::ostream &os, string_view fmt, Ts const &...ts)
{
	os << format(fmt, ts...);
}

inline void print(string_view str)
{
	std::cout << str;
}

inline void print(std::ostream &os, string_view str)
{
	os << str;
}


namespace internal
{

struct format_arg
{
	const void *arg;
	string (*format)(const void *, const char *, const char *);
};

using format_args = vector<format_arg>;


// max uint32_t value is 4'294'967'295
// result is between 1 and 10
inline uint32_t lg_uint(uint32_t val)
{
	// check for small numbers first
	// 1 - 2
	if (val < 100ul)
	{
		return val < 10ul ? 1 : 2;
	}

	// 3 - 6
	if (val < 1'000'000ul)
	{
		// 3 - 4
		if (val < 10'000ul)
		{
			return val < 1'000ul ? 3 : 4;
		}

		// 5 - 6
		return val < 100'000ul ? 5 : 6;
	}

	// 7 - 8
	if (val < 100'000'000ul)
	{
		return val < 10'000'000ul ? 7 : 8;
	}

	// 9 - 10
	return val < 1'000'000'000ul ? 9 : 10;
}

// max uint64_t value is 18'446'744'073'709'551'615
// result is between 1 and 20
inline uint64_t lg_uint(uint64_t val)
{
	// check for small numbers first
	// 1 - 4
	if (val < 10'000ull)
	{
		// 1 - 2
		if (val < 100ull)
		{
			return val < 10ull ? 1 : 2;
		}

		// 3 - 4
		return val < 1'000ull ? 3 : 4;
	}

	// 5 - 12
	if (val < 1'000'000'000'000ull)
	{
		// 5 - 8
		if (val < 100'000'000ull)
		{
			// 5 - 6
			if (val < 1'000'000ull)
			{
				return val < 100'000ull ? 5 : 6;
			}

			// 7 - 8
			return val < 10'000'000ull ? 7 : 8;
		}

		// 9 - 10
		if (val < 10'000'000'000)
		{
			return val < 1'000'000'000ull ? 9 : 10;
		}

		// 11 - 12
		return val < 100'000'000'000ull ? 11 : 12;
	}

	// 13 - 16
	if (val < 10'000'000'000'000'000ull)
	{
		// 13 - 14
		if (val < 100'000'000'000'000ull)
		{
			return val < 10'000'000'000'000ull ? 13 : 14;
		}

		// 15 - 16
		return val < 1'000'000'000'000'000ull ? 15 : 16;
	}

	// 17 - 18
	if (val < 1'000'000'000'000'000'000ull)
	{
		return val < 100'000'000'000'000'000ull ? 17 : 18;
	}

	// 19 - 20
	return val < 10'000'000'000'000'000'000ull ? 19 : 20;
}

template<size_t base, typename Uint, typename = std::enable_if_t<
	std::is_same_v<Uint, uint32_t>
	|| std::is_same_v<Uint, uint64_t>
>>
Uint log_uint(Uint val)
{
	if constexpr (base == 10)
	{
		return lg_uint(val);
	}
	else
	{
		if (val == 0)
		{
			return 1;
		}

		Uint i = 0;
		while (val != 0)
		{
			val /= base;
			++i;
		}

		return i;
	}
}


template<typename>
struct float_components;

template<typename Float>
using float_components_t = typename float_components<Float>::type;

template<>
struct float_components<double>
{
	static_assert(std::numeric_limits<double>::is_iec559);
	using type = union
	{
		double float_val;
		struct
		{
			uint64_t mantissa: 52;
			uint64_t exponent: 11;
			uint64_t sign    : 1;
		};
	};
};

template<>
struct float_components<float>
{
	static_assert(std::numeric_limits<float>::is_iec559);
	using type = union
	{
		float float_val;
		struct
		{
			uint32_t mantissa: 23;
			uint32_t exponent: 8;
			uint32_t sign    : 1;
		};
	};
};


// this class is used in the float formatting function
// it is a basic class, that is only used once
// it behaves like a vector, but it has a max capacity of N,
// so the data can be stored on the stack to help performance
template<typename T, size_t N>
class stack_vector
{
private:
	T  _data[N];
	T *_data_end;

public:
	stack_vector(void)
		: _data{}, _data_end(_data)
	{}

	size_t size(void) const
	{ return static_cast<size_t>(this->_data_end - this->_data); }

	T &push_back(T val)
	{
		bz_assert(this->_data_end < &this->_data[0] + N);
		*this->_data_end = std::move(val);
		++this->_data_end;
		return *(this->_data_end - 1);
	}

	T &push_front(T val)
	{
		bz_assert(this->_data_end < &this->_data[0] + N);
		++this->_data_end;

		auto it = this->_data_end;
		auto trace = it;
		--it;

		while (--trace, --it, it != this->_data - 1)
		{
			*trace = std::move(*it);
		}
		*trace = std::move(val);
		return this->_data[0];
	}

	void pop_back(void)
	{
		if (this->_data != this->_data_end)
		{
			--this->_data_end;
		}
	}

	void pop_front(void)
	{
		if (this->_data == this->_data_end)
		{
			return;
		}
		auto it    = this->_data;
		auto trace = it;
		++it;

		for (; it != this->_data_end; ++trace, ++it)
		{
			*trace = std::move(*it);
		}

		--this->_data_end;
	}

	using iterator         = random_access_iterator<T>;
	using reverse_iterator = reverse_iterator<iterator>;

	iterator begin(void)
	{ return this->_data; }

	iterator end(void)
	{ return this->_data_end; }

	reverse_iterator rbegin(void)
	{ return this->_data_end - 1; }

	reverse_iterator rend(void)
	{ return this->_data - 1; }
};


struct format_spec
{
	char fill;
	char align;
	char sign;
	bool zero_pad;
	size_t width;
	size_t precision;
	char type;

	static constexpr size_t precision_none = std::numeric_limits<size_t>::max();

	static format_spec get_default()
	{
		return { '\0', '\0', '\0', false, 0, precision_none, '\0' };
	}
};

inline format_spec get_default_format_spec(const char *spec, const char *spec_end)
{
	auto fmt_spec = format_spec::get_default();

	auto is_valid_fill = [](char c)
	{
		return c != '\0';
	};

	auto is_align_spec = [](char c)
	{
		return c == '<'
			|| c == '>'
			|| c == '^';
	};

	auto is_sign = [](char c)
	{
		return c == '+'
			|| c == '-'
			|| c == ' ';
	};

	auto get_num = [&]() -> size_t
	{
		size_t res = 0;

		while (spec != spec_end && *spec >= '0' && *spec <= '9')
		{
			res *= 10;
			res += *spec - '0';
			++spec;
		}

		return res;
	};

	if (spec == spec_end)
	{
		return fmt_spec;
	}

	// align spec with a fill char
	if ((spec + 1) != spec_end && is_valid_fill(*spec) && is_align_spec(*(spec + 1)))
	{
		fmt_spec.fill = *spec;
		++spec;
		fmt_spec.align = *spec;
		++spec;
	}
	// align spec without a fill char
	else if (is_align_spec(*spec))
	{
		fmt_spec.align = *spec;
		++spec;
	}

	if (spec != spec_end && is_sign(*spec))
	{
		fmt_spec.sign = *spec;
		++spec;
	}

	if (spec != spec_end && *spec == '0')
	{
		fmt_spec.zero_pad = true;
		++spec;
	}

	fmt_spec.width = get_num();

	// has precision modifier
	if (spec != spec_end && *spec == '.')
	{
		++spec;
		bz_assert(spec != spec_end && *spec >= '0' && *spec <= '9');
		fmt_spec.precision = get_num();
	}

	if (spec != spec_end)
	{
		fmt_spec.type = *spec;
		++spec;
	}

	bz_assert(spec == spec_end);
	return fmt_spec;
}


inline string format_str(string_view str, format_spec spec)
{
	if (spec.fill == '\0')
	{
		spec.fill = ' ';
	}
	if (spec.align == '\0')
	{
		spec.align = '<';
	}
	bz_assert(spec.sign == '\0');
	bz_assert(spec.zero_pad == false);
	bz_assert(spec.precision == format_spec::precision_none);
	bz_assert(spec.type == '\0' || spec.type == 's');

	auto res = string(std::max(str.length(), spec.width), spec.fill);

	auto out = res.begin();
	auto end = res.end();

	if (spec.align == '>')
	{
		out += res.length() - str.length();
	}
	else if (spec.align == '^')
	{
		// center alignment has left bias
		out += (res.length() - str.length()) / 2;
	}

	for (size_t i = 0; i < str.length(); ++i, ++out)
	{
		bz_assert(out != end);
		*out = str[i];
	}

	return res;
}

constexpr char digits_x[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
constexpr char digits_X[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

template<size_t base, typename Uint, typename = std::enable_if_t<
	std::is_same_v<Uint, uint32_t>
	|| std::is_same_v<Uint, uint64_t>
>>
string uint_to_string_base(Uint val, format_spec spec)
{
	// uints are always positive, so a - sign can't add any chars
	size_t len = spec.sign == '-' ? 0 : 1;

	if constexpr (base == 10)
	{
		len += lg_uint(val);
	}
	else
	{
		bz_assert(spec.sign == '-');
		len += log_uint<base>(val);
	}

	auto res = string(std::max(len, spec.width), spec.fill);

	auto out = res.rbegin();
	auto end = res.rend();

	auto put_num = [&]()
	{
		auto copy = val;
		do
		{
			bz_assert(out != end);
			if (spec.type == 'X')
			{
				*out = digits_X[copy % base];
			}
			else
			{
				*out = digits_x[copy % base];
			}
			++out;
			copy /= base;
		} while (copy != 0);
	};

	switch (spec.align)
	{
	case '<':
	case '^':
		bz_assert(!spec.zero_pad);

		if (spec.align == '<')
		{
			out += res.length() - len;
		}
		else
		{
			// center alignment has a right bias
			out += (res.length() - len) / 2;
		}

		put_num();

		if (spec.sign != '-')
		{
			bz_assert(out != end);
			*out = spec.sign;
			++out;
		}

		if (spec.align == '<')
		{
			bz_assert(out == end);
		}
		return res;

	case '>':
		put_num();

		if (spec.sign == '-')
		{
			if (spec.zero_pad)
			{
				while (out != end)
				{
					*out = '0';
					++out;
				}
			}
		}
		else
		{
			if (spec.zero_pad)
			{
				bz_assert(out != end);
				while (out + 1 != end)
				{
					*out = '0';
					++out;
				}
			}
			bz_assert(out != end);
			*out = spec.sign;
			++out;
		}

		return res;

	default:
		bz_assert(false);
		return "";
	}
}

// converts an unsigned integer to a string
template<typename Uint, typename = std::enable_if_t<
	std::is_same_v<Uint, uint32_t>
	|| std::is_same_v<Uint, uint64_t>
>>
string uint_to_string(Uint val, format_spec spec)
{
	// set the defaults for spec
	if (spec.align == '\0')
	{
		spec.align = '>';
		spec.fill = ' ';
	}
	else if (spec.fill == '\0')
	{
		spec.fill = ' ';
	}
	if (spec.sign == '\0')
	{
		spec.sign = '-';
	}
	bz_assert(spec.precision == format_spec::precision_none);
	if (spec.type == '\0')
	{
		spec.type = 'd';
	}
	else
	{
		bz_assert(
			spec.type == 'd'
			|| spec.type == 'b'
			|| spec.type == 'B'
			|| spec.type == 'o'
			|| spec.type == 'x'
			|| spec.type == 'X'
		);
	}

	// if the value to print is zero don't print a plus sign,
	// instead leave a space
	// could change later, if we decide it's not a good thing
	if (val == 0 && spec.sign == '+')
	{
		spec.sign = ' ';
	}

	switch (spec.type)
	{
	case 'd':
		return uint_to_string_base<10>(val, spec);

	case 'b':
	case 'B':
		return uint_to_string_base<2>(val, spec);

	case 'o':
		return uint_to_string_base<8>(val, spec);

	case 'x':
	case 'X':
		return uint_to_string_base<16>(val, spec);

	default:
		bz_assert(false);
		return "";
	}
}

// converts an signed integer to a string
template<typename Int, typename = std::enable_if_t<
	std::is_same_v<Int, int32_t>
	|| std::is_same_v<Int, int64_t>
>>
string int_to_string(Int val, format_spec spec)
{
	using Uint = std::conditional_t<std::is_same_v<Int, int32_t>, uint32_t, uint64_t>;

	// set the defaults for spec
	if (spec.align == '\0')
	{
		spec.align = '>';
		spec.fill = ' ';
	}
	else if (spec.fill == '\0')
	{
		spec.fill = ' ';
	}
	if (spec.sign == '\0')
	{
		spec.sign = '-';
	}
	bz_assert(spec.precision == format_spec::precision_none);
	if (spec.type == '\0')
	{
		spec.type = 'd';
	}
	else
	{
		// only allow decimal printing for unsigned numbers
		// could change later
		bz_assert(spec.type == 'd');
	}

	// if the value to print is zero don't print a plus sign,
	// instead leave a space
	// could change later, if we decide it's not a good thing
	if (val == 0 && spec.sign == '+')
	{
		spec.sign = ' ';
	}

	bool is_negative = val < 0;
	Uint abs_val;

	if (is_negative)
	{
		// if we were to do -int_min, we would still have int_min,
		// so we need to check it
		if (val == std::numeric_limits<Int>::min())
		{
			abs_val = static_cast<Uint>(std::numeric_limits<Int>::max()) + 1;
		}
		else
		{
			abs_val = static_cast<Uint>(-val);
		}
	}
	else
	{
		abs_val = static_cast<Uint>(val);
	}

	// if the number is negative set the sign to '-', so we can use spec.sign
	// when putting in the sign instead of checking is_negative
	if (is_negative)
	{
		spec.sign = '-';
	}

	bool put_sign = is_negative || spec.sign != '-';

	size_t len = lg_uint(abs_val) + (put_sign ? 1 : 0);

	auto res = string(std::max(len, spec.width), spec.fill);

	// we output the number from right to left
	auto out = res.rbegin();
	auto end = res.rend();

	auto put_num = [&]()
	{
		auto copy = abs_val;
		do
		{
			bz_assert(out != end);
			*out = copy % 10 + '0';
			++out;
			copy /= 10;
		} while (copy != 0);
	};

	switch (spec.align)
	{
	case '<':
	case '^':
	{
		bz_assert(!spec.zero_pad);

		if (spec.align == '<')
		{
			out += res.length() - len;
		}
		else
		{
			out += (res.length() - len) / 2;
		}

		put_num();

		if (put_sign)
		{
			bz_assert(out != end);
			*out = spec.sign;
			++out;
		}

		return res;
	}

	case '>':
	{
		put_num();

		if (spec.zero_pad)
		{
			if (put_sign)
			{
				bz_assert(out != end);
				while (out + 1 != end)
				{
					*out = '0';
					++out;
				}
			}
			else
			{
				while (out != end)
				{
					*out = '0';
					++out;
				}
				bz_assert(out == end);
			}
		}
		if (put_sign)
		{
			bz_assert(out != end);
			*out = spec.sign;
			++out;
		}

		return res;
	}

	default:
		bz_assert(false);
		return "";
	}
}

template<size_t N>
string float_to_string_dec(
	typename stack_vector<uint8_t, N>::reverse_iterator dig_it,
	typename stack_vector<uint8_t, N>::reverse_iterator dig_end,
	int32_t dig_exponent,
	bool put_sign,
	format_spec spec
)
{
	bz_assert(spec.precision != format_spec::precision_none);
	size_t len = spec.precision                     // decimal part
		+ (spec.precision == 0 ? 0 : 1)             // .
		+ (dig_exponent < 0 ? 1 : dig_exponent + 1) // integer part
		+ (put_sign ? 1 : 0)                        // sign
		+ (spec.type == '%' ? 1 : 0);               // %

	auto print_len = dig_end - dig_it;

	auto res = string(std::max(len, spec.width), spec.fill);

	auto it  = res.rbegin();
	auto end = res.rend();

	auto put_digit = [&]()
	{
		bz_assert(it != end);
		bz_assert(dig_it != dig_end);
		*it = *dig_it + '0';
		++dig_it;
		++it;
	};

	auto put_char = [&](char c)
	{
		bz_assert(it != end);
		*it = c;
		++it;
	};

	if (spec.align == '<')
	{
		it += res.length() - len;
	}
	else if (spec.align == '^')
	{
		// center alignment has right bias
		it += (res.length() - len) / 2;
	}

	if (spec.type == '%')
	{
		put_char('%');
	}

	// print the decimal part if we need it
	if (spec.precision != 0)
	{
		auto prec = static_cast<int64_t>(spec.precision);
		int64_t i = 0;
		// trailing zeroes if the precision is higher than the number can hold
		for (; i < (prec - print_len) + dig_exponent + 1; ++i)
		{
			put_char('0');
		}
		// the stored digits
		for (; i < prec && dig_it != dig_end; ++i)
		{
			put_digit();
		}
		// if val < 1, print the zeroes until the decimal point
		if (i != prec)
		{
			for (; i < prec; ++i)
			{
				put_char('0');
			}
		}
		put_char('.');
	}

	// print the whole part of the number
	if (dig_it == dig_end)
	{
		put_char('0');
	}
	else
	{
		while (dig_it != dig_end)
		{
			put_digit();
		}
	}

	if (spec.zero_pad)
	{
		if (put_sign)
		{
			while (it + 1 != end)
			{
				put_char('0');
			}
		}
		else
		{
			while (it != end)
			{
				put_char('0');
			}
		}
	}

	if (put_sign)
	{
		put_char(spec.sign);
	}

	return res;
}

template<size_t N>
string float_to_string_exp(
	typename stack_vector<uint8_t, N>::reverse_iterator dig_it,
	typename stack_vector<uint8_t, N>::reverse_iterator dig_end,
	int32_t dig_exponent,
	bool put_sign,
	format_spec spec
)
{
	auto print_len = dig_end - dig_it;
	bz_assert(print_len != 0);
	size_t len = print_len + 1                     // digits + .
		+ (put_sign ? 1 : 0)                       // sign
		+ (std::abs(dig_exponent) >= 100 ? 5 : 4); // exponential notation
	// the exponential notation is either e+... or e+.. (. is a digit)

	auto res = string(std::max(len, spec.width), spec.fill);

	auto it  = res.rbegin();
	auto end = res.rend();

	auto put_digit = [&]()
	{
		bz_assert(it != end);
		bz_assert(dig_it != dig_end);
		*it = *dig_it + '0';
		++dig_it;
		++it;
	};

	auto put_char = [&](char c)
	{
		bz_assert(it != end);
		*it = c;
		++it;
	};

	if (spec.align == '<')
	{
		it += res.length() - len;
	}
	else if (spec.align == '^')
	{
		// center alignment has right bias
		it += (res.length() - len) / 2;
	}

	// print the exponential part
	{
		auto num = std::abs(dig_exponent);

		auto mod = num % 10;
		put_char('0' + mod);
		num -= mod;

		if (num == 0)
		{
			put_char('0');
		}
		else
		{
			mod = (num % 100) / 10;
			put_char('0' + mod);

			if (num >= 300)
			{
				put_char('3');
			}
			else if (num >= 200)
			{
				put_char('2');
			}
			else if (num >= 100)
			{
				put_char('1');
			}
		}
		put_char(dig_exponent < 0 ? '-' : '+');
		put_char('e');
	}

	if (print_len > 1)
	{
		while (dig_it + 1 != dig_end)
		{
			put_digit();
		}

		put_char('.');
	}

	// last digit (iteger part)
	put_digit();

	if (spec.zero_pad)
	{
		if (put_sign)
		{
			while (it + 1 != end)
			{
				put_char('0');
			}
		}
		else
		{
			while (it != end)
			{
				put_char('0');
			}
		}
	}

	if (put_sign)
	{
		put_char(spec.sign);
	}

	return res;
}

template<typename Float, typename = std::enable_if_t<
	std::is_same_v<Float, float>
	|| std::is_same_v<Float, double>
>>
string float_to_string(Float val, format_spec spec)
{
	if (spec.align == '\0')
	{
		spec.align = '>';
	}
	else if (spec.align == '^' || spec.align == '<')
	{
		bz_assert(!spec.zero_pad);
	}
	if (spec.fill == '\0')
	{
		spec.fill = ' ';
	}
	if (spec.sign == '\0')
	{
		spec.sign = '-';
	}
	if (spec.type == '\0' || spec.type == 'g')
	{
		spec.type = 'g';
		if (spec.precision == format_spec::precision_none)
		{
			spec.precision = 6;
		}
	}
	else
	{
		bz_assert(
			spec.type == 'g'
			|| spec.type == 'f'
			|| spec.type == '%'
		);
	}
	if (spec.precision == format_spec::precision_none)
	{
		spec.precision = spec.type == '%' ? 2 : 6;
	}


	bool is_negative = std::signbit(val);
	bool put_sign = is_negative || spec.sign != '-';

	if (is_negative)
	{
		spec.sign = '-';
	}

	if (!std::isfinite(val))
	{
		if (std::isinf(val))
		{
			size_t len = 3 + (put_sign ? 1 : 0);
			auto res = string(std::max(len, spec.width), spec.fill);

			auto out = res.rbegin();

			if (spec.align == '<')
			{
				out += res.length() - len;
			}
			else if (spec.align == '^')
			{
				out += (res.length() - len) / 2;
			}

			*out = 'f';
			++out;
			*out = 'n';
			++out;
			*out = 'i';
			++out;

			if (put_sign)
			{
				*out = spec.sign;
			}

			return res;
		}
		else
		{
			size_t len = 3;
			auto res = string(std::max(len, spec.width), spec.fill);

			auto out = res.rbegin();

			if (spec.align == '<')
			{
				out += res.length() - len;
			}
			else if (spec.align == '^')
			{
				out += (res.length() - len) / 2;
			}

			*out = 'n';
			++out;
			*out = 'a';
			++out;
			*out = 'n';

			return res;
		}
	}


	// 767 is the maximum length that a double needs to pring its digits
	// 112 is the maximum length for a float
	constexpr size_t N = std::is_same_v<Float, double> ? 767 : 112;

	// the exponent part of a floating point number has an offset, so to get the
	// actual exponent we have to calculate exp - offset
	// this offset is 1023 for doubles and 127 for floats
	constexpr int32_t exponent_offset = std::is_same_v<Float, double> ? 1023 : 127;

	// the concept here is that we get the digits of the number by using
	// an array of size N to represent the digits of the number

	stack_vector<uint8_t, N> out_digits;
	stack_vector<uint8_t, N> frac_digits;

	int32_t out_exponent  = 0;
	int32_t frac_exponent = 0;

	{
		auto half_frac = [&]()
		{
			frac_digits.push_back(0);

			auto it    = frac_digits.rbegin();
			auto trace = it;
			++it;
			auto end = frac_digits.rend();

			for (; it != end; ++trace, ++it)
			{
				if (*it % 2 == 1)
				{
					*trace += 5;
				}

				*it /= 2;
			}

			if (*trace == 0)
			{
				frac_digits.pop_front();
				--frac_exponent;
			}
		};

		auto double_frac = [&]()
		{
			bool carry = false;
			for (auto it = frac_digits.rbegin(); it != frac_digits.rend(); ++it)
			{
				*it = (*it * 2) + (carry ? 1 : 0);
				carry = false;
				if (*it >= 10)
				{
					*it -= 10;
					carry = true;
				}
			}
			if (carry)
			{
				frac_digits.push_front(1);
				++frac_exponent;
			}
		};

		auto add_frac_to_out = [&]()
		{
			if (out_digits.size() == 0)
			{
				out_exponent = frac_exponent;
			}

			// pad the front of out_digits with zeroes
			for (; out_exponent < frac_exponent; ++out_exponent)
			{
				out_digits.push_front(0);
			}

			// pad the back of out_digits with zeroes
			while (
				static_cast<int32_t>(out_digits.size()) - out_exponent
				< static_cast<int32_t>(frac_digits.size()) - frac_exponent
			)
			{
				out_digits.push_back(0);
			}

			auto out_it  = out_digits.rbegin();
			auto out_end = out_digits.rend();

			auto frac_it  = frac_digits.rbegin();
			auto frac_end = frac_digits.rend();

			// if the last digit of frac_digits has a higher
			// decimal place than the last digit of out_digits,
			// we need to add the difference to out_it
			out_it += (out_digits.size() - out_exponent) - (frac_digits.size() - frac_exponent);

			bool carry = false;
			for (; frac_it != frac_end; ++out_it, ++frac_it)
			{
				bz_assert(out_it != out_end);
				*out_it += *frac_it + (carry ? 1 : 0);
				carry = false;
				if (*out_it >= 10)
				{
					*out_it -= 10;
					carry = true;
				}
			}

			if (carry)
			{
				for (; out_it != out_end; ++out_it)
				{
					if (*out_it == 9)
					{
						*out_it = 0;
					}
					else
					{
						*out_it += 1;
						break;
					}
				}
				if (out_it == out_end)
				{
					out_digits.push_front(1);
					++out_exponent;
				}
			}
		};

		float_components_t<Float> val_components;
		val_components.float_val = val;
		auto val_exp = val_components.exponent - exponent_offset;

		frac_digits.push_back(1);

		// if the exponent is zero, the number uses fixed point notation
		// with a base of 2^(-exponent_offset + 1)
		if (val_components.exponent == 0)
		{
			for (int i = 0; i < exponent_offset - 1; ++i)
			{
				half_frac();
			}
		}
		else if (val_exp < 0)
		{
			for (int i = 0; i > val_exp; --i)
			{
				half_frac();
			}
			add_frac_to_out();
		}
		else
		{
			for (int i = 0; i < val_exp; ++i)
			{
				double_frac();
			}
			add_frac_to_out();
		}

		constexpr std::conditional_t<
			std::is_same_v<Float, double>, uint64_t, uint32_t
		> unit = 1;

		auto stop_condition = [&]()
		{
			if (spec.type == 'g')
			{
				return out_exponent - frac_exponent > static_cast<int32_t>(spec.precision) + 1;
			}
			else if (spec.type == 'f')
			{
				return -frac_exponent > static_cast<int32_t>(spec.precision) + 2;
			}
			else
			{
				return -frac_exponent > static_cast<int32_t>(spec.precision) + 4;
			}
		};

		for (
			int i = (std::is_same_v<Float, double> ? 51 : 22);
			i >= 0 && !stop_condition();
			--i
		)
		{
			half_frac();
			if ((val_components.mantissa & (unit << i)) != 0)
			{
				add_frac_to_out();
			}
		}
	}

	if (spec.type == '%')
	{
		out_exponent += 2;
	}

	// print_len is the max number of digits we will print
	int32_t print_len = 0;

	if (spec.type == 'g')
	{
		print_len = spec.precision;
	}
	else
	{
		int32_t len = (out_exponent + 1) + spec.precision;
		print_len = len < 0 ? 0 : len;
	}

	// round the last digit of the range that we print
	{
		auto begin = out_digits.rbegin();
		auto it    = begin;
		auto end   = out_digits.rend();

		if (static_cast<int32_t>(out_digits.size()) > print_len)
		{
			it += out_digits.size() - print_len;
		}

		bool carry = (it != begin && *(it - 1) >= 5);
		for (; carry && it != end; ++it)
		{
			if (*it == 9)
			{
				*it = 0;
			}
			else
			{
				*it += 1;
				carry = false;
			}
		}

		if (carry)
		{
			out_digits.push_front(1);
			++out_exponent;
		}
	}

	if (spec.type != 'g')
	{
		int32_t len = (out_exponent + 1) + spec.precision;
		print_len = len < 0 ? 0 : len;
	}

	if (static_cast<int32_t>(out_digits.size()) < print_len)
	{
		print_len = out_digits.size();
	}

	// we don't want to print trailing zeros if we type spec is 'g'
	// so we count them and subtract it from print_len
	if (spec.type == 'g')
	{
		auto begin = out_digits.begin();
		auto it = begin;
		auto last_non_zero = it;
		auto end = it + print_len;

		for (; it != end; ++it)
		{
			if (*it != 0)
			{
				last_non_zero = it + 1;
			}
		}

		auto actual_len = last_non_zero - begin;
		print_len = actual_len;
	}

	// we need to print trailing zeroes until the decimal point though
	if (print_len <= out_exponent)
	{
		if (spec.type == 'g')
		{
			if (out_exponent < static_cast<int32_t>(spec.precision))
			{
				print_len = out_exponent + 1;
			}
		}
		else
		{
			print_len = out_exponent + 1;
		}
	}



	switch (spec.type)
	{
	case 'g':
		// print with regular 'f' format
		if (out_exponent < static_cast<int32_t>(spec.precision) && out_exponent >= -4)
		{
			spec.precision = print_len - out_exponent - 1;
			spec.type = 'f';
			return float_to_string_dec<N>(
				out_digits.rend() - print_len,
				out_digits.rend(),
				out_exponent,
				put_sign,
				spec
			);
		}
		// print with exponential format
		else
		{
			return float_to_string_exp<N>(
				out_digits.rend() - print_len,
				out_digits.rend(),
				out_exponent,
				put_sign,
				spec
			);
		}

	case 'f':
	case '%':
		return float_to_string_dec<N>(
			out_digits.rend() - print_len,
			out_digits.rend(),
			out_exponent,
			put_sign,
			spec
		);

	default:
		bz_assert(false);
		return "";
	}

	return "";
}

inline string pointer_to_string(const void *ptr, format_spec spec)
{
	if (spec.align == '\0')
	{
		spec.align = '>';
	}
	else if (spec.align == '^' || spec.align == '<')
	{
		bz_assert(!spec.zero_pad);
	}
	if (spec.fill == '\0')
	{
		spec.fill = ' ';
	}
	bz_assert(spec.sign == '\0');
	bz_assert(spec.precision == format_spec::precision_none);
	bz_assert(spec.type == '\0' || spec.type == 'p');

	auto val = reinterpret_cast<uint64_t>(ptr);
	size_t len = log_uint<16>(val) + 2;

	auto res = string(std::max(len, spec.width), spec.fill);

	auto out = res.rbegin();
	auto end = res.rend();

	auto put_num = [&]()
	{
		auto copy = val;
		do
		{
			bz_assert(out != end);
			*out = digits_x[copy % 16];
			++out;
			copy /= 16;
		} while (copy != 0);
	};

	if (spec.align == '<')
	{
		out += res.length() - len;
	}
	else if (spec.align == '^')
	{
		// center alignment has right bias
		out += (res.length() - len) / 2;
	}

	put_num();

	if (spec.zero_pad)
	{
		bz_assert(end - out >= 2);
		while (end - out > 2)
		{
			*out = '0';
			++out;
		}
	}

	bz_assert(out != end);
	*out = 'x';
	++out;
	bz_assert(out != end);
	*out = '0';
	++out;

	return res;
}

} // namepsace internal




template<typename T>
struct formatter;

template<typename T>
struct formatter<const T> : formatter<T>
{};

template<>
struct formatter<string_view>
{
	static string format(string_view str, const char *spec, const char *spec_end)
	{
		return internal::format_str(str, internal::get_default_format_spec(spec, spec_end));
	}
};

template<>
struct formatter<string> : formatter<string_view>
{};

template<>
struct formatter<const char *> : formatter<string_view>
{};

template<>
struct formatter<char *> : formatter<const char *>
{};

template<size_t N>
struct formatter<char[N]> : formatter<const char *>
{};

template<size_t N>
struct formatter<const char[N]> : formatter<const char *>
{};


template<>
struct formatter<int32_t>
{
	static string format(int32_t val, const char *spec, const char *spec_end)
	{
		return internal::int_to_string(val, internal::get_default_format_spec(spec, spec_end));
	}
};

template<>
struct formatter<uint32_t>
{
	static string format(uint32_t val, const char *spec, const char *spec_end)
	{
		return internal::uint_to_string(val, internal::get_default_format_spec(spec, spec_end));
	}
};

template<>
struct formatter<int64_t>
{
	static string format(int64_t val, const char *spec, const char *spec_end)
	{
		return internal::int_to_string(val, internal::get_default_format_spec(spec, spec_end));
	}
};

template<>
struct formatter<uint64_t>
{
	static string format(uint64_t val, const char *spec, const char *spec_end)
	{
		return internal::uint_to_string(val, internal::get_default_format_spec(spec, spec_end));
	}
};

#define formatter_int_spec(type, base)                                           \
                                                                                 \
template<>                                                                       \
struct formatter<type> : formatter<base>                                         \
{};

formatter_int_spec(int8_t,   int32_t)
formatter_int_spec(int16_t,  int32_t)
formatter_int_spec(uint8_t,  uint32_t)
formatter_int_spec(uint16_t, uint32_t)

#undef formatter_int_spec

template<>
struct formatter<char>
{
	static string format(char val, const char *spec, const char *spec_end)
	{
		bz_assert(spec == spec_end);

		return string(1, val);
	}
};

template<>
struct formatter<bool>
{
	static string format(bool val, const char *spec, const char *spec_end)
	{
		bz_assert(spec == spec_end);

		if (val)
		{
			return "true";
		}
		else
		{
			return "false";
		}
	}
};

template<>
struct formatter<float>
{
	static string format(float val, const char *spec, const char *spec_end)
	{
		return internal::float_to_string(val, internal::get_default_format_spec(spec, spec_end));
	}
};

template<>
struct formatter<double>
{
	static string format(double val, const char *spec, const char *spec_end)
	{
		return internal::float_to_string(val, internal::get_default_format_spec(spec, spec_end));
	}
};

template<>
struct formatter<const void *>
{
	static string format(const void *ptr, const char *spec, const char *spec_end)
	{
		return internal::pointer_to_string(ptr, internal::get_default_format_spec(spec, spec_end));
	}
};

template<typename T>
struct formatter<T *> : formatter<const void *>
{};

template<>
struct formatter<std::nullptr_t> : formatter<const void *>
{};


namespace internal
{

inline void append_char_range(string &str, string_view::iterator begin, string_view::iterator end)
{
	if (begin == end)
	{
		return;
	}

	str.reserve(str.length() + static_cast<size_t>(end - begin));
	while (begin != end)
	{
		str += *begin;
		++begin;
	}
}


template<typename ...Args>
format_args make_format_args(Args const &...args)
{
	return {
		format_arg{
			&args,
			[](const void *arg, const char *spec, const char *spec_end) -> string
			{
				return formatter<Args>::format(
					*static_cast<const Args *>(arg),
					spec,
					spec_end
				);
			}
		}...
	};
}

inline string format_impl(
	size_t &current_arg,
	string_view::iterator fmt,
	string_view::iterator fmt_end,
	format_args const &args
)
{
	string res = "";
	auto begin = fmt;

	while (fmt != fmt_end)
	{
		switch (*fmt)
		{
		case '{':
		{
			append_char_range(res, begin, fmt);
			++fmt; // '{'

			auto spec_str_begin = fmt;
			int lvl = 1;
			while (fmt != fmt_end && lvl > 0)
			{
				if (*fmt == '{')
				{
					++lvl;
				}
				else if (*fmt == '}' && --lvl == 0)
				{
					break;
				}
				++fmt;
			}

			bz_assert(fmt != fmt_end);

			auto spec_str_end = fmt;
			++fmt; // '}'

			const auto fmt_spec_str = format_impl(
				current_arg, spec_str_begin, spec_str_end, args
			);
			auto spec = fmt_spec_str.begin();
			auto spec_end = fmt_spec_str.end();

			auto arg_num = current_arg;
			++current_arg;
			if (spec != spec_end && *spec >= '0' && *spec <= '9')
			{
				size_t n = 0;
				do
				{
					n *= 10;
					n += *spec - '0';
					++spec;
				} while (spec != spec_end && *spec >= '0' && *spec <= '9');

				arg_num = n;
				// the next default argument is 1 + previous
				current_arg = arg_num + 1;
			}

			if (spec != spec_end)
			{
				bz_assert(*spec == ':');
				++spec;
			}

			bz_assert(arg_num < args.size());

			auto &to_print = args[arg_num];
			res += to_print.format(to_print.arg, &*spec, &*spec_end);

			begin = fmt;
			break;
		}

		default:
			++fmt;
			break;
		}
	}
	append_char_range(res, begin, fmt);

	return std::move(res);
}

} // namespace internal

template<typename ...Ts>
string format(string_view fmt, Ts const &...ts)
{
	size_t current_arg = 0;
	return internal::format_impl(
		current_arg,
		fmt.begin(),
		fmt.end(),
		internal::make_format_args(ts...)
	);
}

bz_end_namespace

#endif // _bz_format_h__
