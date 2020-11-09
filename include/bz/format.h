#ifndef _bz_format_h__
#define _bz_format_h__

#include "core.h"
// #include "string.h"
#include "u8string.h"
#include "u8string_view.h"
#include "vector.h"

#include <utility>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <ostream>
#include <algorithm>
#include <charconv>
#ifdef _WIN32
#include <windows.h>
#undef min
#undef max
#endif // windows

bz_begin_namespace

template<typename ...Ts>
u8string format(u8string_view fmt, Ts const &...ts);

#ifdef _WIN32
namespace internal
{

inline void set_console_attribute(HANDLE h, uint32_t n)
{
	static WORD default_text_attribute = [=] {
		CONSOLE_SCREEN_BUFFER_INFO info = {};
		GetConsoleScreenBufferInfo(h, &info);
		return info.wAttributes;
	}();

	CONSOLE_SCREEN_BUFFER_INFO info = {};
	GetConsoleScreenBufferInfo(h, &info);

	constexpr WORD foreground_bits = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
	constexpr WORD background_bits = BACKGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE;
	constexpr WORD other_bits = ~(foreground_bits | background_bits);

	WORD const current_foreground = info.wAttributes & foreground_bits;
	WORD const current_background = info.wAttributes & background_bits;
	WORD const current_other = info.wAttributes & other_bits;

	WORD new_foreground = current_foreground;
	WORD new_background = current_background;
	WORD new_other = current_other;

	switch (n)
	{
	case 0:
		new_foreground = default_text_attribute & foreground_bits;
		new_background = default_text_attribute & background_bits;
		new_other = default_text_attribute & other_bits;
		break;

	// foreground
	case 30: new_foreground = 0; break;
	case 31: new_foreground = FOREGROUND_RED; break;
	case 32: new_foreground = FOREGROUND_GREEN; break;
	case 33: new_foreground = FOREGROUND_RED | FOREGROUND_GREEN; break;
	case 34: new_foreground = FOREGROUND_BLUE; break;
	case 35: new_foreground = FOREGROUND_RED | FOREGROUND_BLUE; break;
	case 36: new_foreground = FOREGROUND_GREEN | FOREGROUND_BLUE; break;
	case 37: new_foreground = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; break;

	// background
	case 40: new_background = 0; break;
	case 41: new_background = BACKGROUND_RED; break;
	case 42: new_background = BACKGROUND_GREEN; break;
	case 43: new_background = BACKGROUND_RED | BACKGROUND_GREEN; break;
	case 44: new_background = BACKGROUND_BLUE; break;
	case 45: new_background = BACKGROUND_RED | BACKGROUND_BLUE; break;
	case 46: new_background = BACKGROUND_GREEN | BACKGROUND_BLUE; break;
	case 47: new_background = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE; break;

	// bright foreground
	case 90: new_foreground = FOREGROUND_INTENSITY; break;
	case 91: new_foreground = FOREGROUND_INTENSITY | FOREGROUND_RED; break;
	case 92: new_foreground = FOREGROUND_INTENSITY | FOREGROUND_GREEN; break;
	case 93: new_foreground = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN; break;
	case 94: new_foreground = FOREGROUND_INTENSITY | FOREGROUND_BLUE; break;
	case 95: new_foreground = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_BLUE; break;
	case 96: new_foreground = FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE; break;
	case 97: new_foreground = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; break;

	// bright background
	case 100: new_background = BACKGROUND_INTENSITY; break;
	case 101: new_background = BACKGROUND_INTENSITY | BACKGROUND_RED; break;
	case 102: new_background = BACKGROUND_INTENSITY | BACKGROUND_GREEN; break;
	case 103: new_background = BACKGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_GREEN; break;
	case 104: new_background = BACKGROUND_INTENSITY | BACKGROUND_BLUE; break;
	case 105: new_background = BACKGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_BLUE; break;
	case 106: new_background = BACKGROUND_INTENSITY | BACKGROUND_GREEN | BACKGROUND_BLUE; break;
	case 107: new_background = BACKGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE; break;

	default: break;
	}

	SetConsoleTextAttribute(h, new_foreground | new_background | new_other);
}

inline void set_console_attributes(u8string_view str)
{
	auto const h = GetStdHandle(STD_OUTPUT_HANDLE);
	int32_t n = 0;
	for (auto it = str.begin(); it != str.end(); ++it)
	{
		switch (*it)
		{
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			n *= 10;
			n += *it - '0';
			break;

		case ';':
			set_console_attribute(h, n);
			n = 0;
			break;

		default:
			break;
		}
	}
	set_console_attribute(h, n);
}

inline void fprint_string_view(u8string_view str, std::FILE *file)
{
	std::fwrite(str.data(), sizeof (uint8_t), str.size(), file);
}

inline void fprint(u8string_view str, std::FILE *file)
{
	auto begin = str.begin();
	auto const end = str.end();
	while (begin != end)
	{
		auto const coloring_char = std::find(begin, end, '\033');
		if (coloring_char == end)
		{
			// we are done
			fprint_string_view(u8string_view(begin, end), file);
			break;
		}
		auto it = coloring_char;
		++it;
		if (it == end)
		{
			// we are done
			fprint_string_view(u8string_view(begin, end), file);
			break;
		}

		if (*it != '[')
		{
			// invalid coloring sequence
			// we go to the next iteration
			fprint_string_view(u8string_view(begin, it), file);
			begin = it;
			continue;
		}

		++it; // '['
		auto const closing_m = std::find(it, end, 'm');
		if (closing_m == end)
		{
			// we are done
			fprint_string_view(u8string_view(begin, end), file);
			break;
		}

		fprint_string_view(u8string_view(begin, coloring_char), file);
		internal::set_console_attributes(u8string_view(it, closing_m));
		begin = closing_m;
		++begin;
	}
}

} // namespace internal

template<typename ...Ts>
void print(u8string_view fmt, Ts const &...ts)
{
	if constexpr (sizeof... (Ts) == 0)
	{
		internal::fprint(fmt, stdout);
	}
	else
	{
		auto const str = format(fmt, ts...);
		internal::fprint(str, stdout);
	}
}

template<typename ...Ts>
void log(u8string_view fmt, Ts const &...ts)
{
	if constexpr (sizeof... (Ts) == 0)
	{
		internal::fprint(fmt, stderr);
	}
	else
	{
		auto const str = format(fmt, ts...);
		internal::fprint(str, stderr);
	}
}

#else

template<typename ...Ts>
void print(u8string_view fmt, Ts const &...ts)
{
	if constexpr (sizeof... (Ts) == 0)
	{
		fwrite(fmt.data(), sizeof (char), fmt.size(), stdout);
	}
	else
	{
		auto const str = format(fmt, ts...);
		fwrite(str.data(), sizeof (char), str.size(), stdout);
	}
}

template<typename ...Ts>
void log(u8string_view fmt, Ts const &...ts)
{
	if constexpr (sizeof... (Ts) == 0)
	{
		fwrite(fmt.data(), sizeof (char), fmt.size(), stderr);
	}
	else
	{
		auto const str = format(fmt, ts...);
		fwrite(str.data(), sizeof (char), str.size(), stderr);
	}
}

#endif // windows

template<typename ...Ts>
void print(std::ostream &os, u8string_view fmt, Ts const &...ts)
{
	if constexpr (sizeof... (Ts) == 0)
	{
		os << fmt;
	}
	else
	{
		os << format(fmt, ts...);
	}
}


namespace internal
{

struct format_arg
{
	const void *arg;
	u8string (*format)(const void *, u8string_view);
};

using format_args = vector<format_arg>;


// max uint32_t value is 4'294'967'295
// result is between 1 and 10
constexpr uint32_t lg_uint(uint32_t val)
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
constexpr uint64_t lg_uint(uint64_t val)
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
constexpr Uint log_uint(Uint val)
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
		} components;
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
		} components;
	};
};


// this class is used in the float formatting function
// it is a basic class, that is only used once
// it behaves like a vector, but it has a max capacity of N,
// so the data can be stored on the stack to help performance
template<typename T, size_t N>
class stack_vector
{
	static_assert(meta::is_trivial_v<T>);
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

		while (it != this->_data)
		{
			--trace, --it;
			*trace = std::move(*it);
		}
		*it = std::move(val);
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
	{ return (T *)this->_data - 1; }
};


struct format_spec
{
	u8char fill;
	u8char align;
	u8char sign;
	u8char type;
	size_t width;
	size_t precision;
	bool zero_pad;

	static constexpr size_t precision_none = std::numeric_limits<size_t>::max();

	static format_spec get_default()
	{
		return { '\0', '\0', '\0', '\0', 0, precision_none, false };
	}
};

inline format_spec get_default_format_spec(u8string_view spec)
{
	auto fmt_spec = format_spec::get_default();
	auto it = spec.begin();
	auto const end = spec.end();

	auto is_valid_fill = [](u8char c)
	{
		return c != '\0';
	};

	auto is_align_spec = [](u8char c)
	{
		return c == '<'
			|| c == '>'
			|| c == '^';
	};

	auto is_sign = [](u8char c)
	{
		return c == '+'
			|| c == '-'
			|| c == ' ';
	};

	auto get_num = [&]() -> size_t
	{
		size_t res = 0;

		while (it != end && *it >= '0' && *it <= '9')
		{
			res *= 10;
			res += *it - '0';
			++it;
		}

		return res;
	};

	if (it == end)
	{
		return fmt_spec;
	}

	{
		auto const fill = it;
		++it;
		// align spec with a fill char
		if (it != end && is_valid_fill(*fill) && is_align_spec(*it))
		{
			fmt_spec.fill = *fill;
			fmt_spec.align = *it;
			++it;
		}
		else
		{
			it = fill;
			// align spec without a fill char
			if (is_align_spec(*it))
			{
				fmt_spec.align = *it;
				++it;
			}
		}
	}

	if (it != end && is_sign(*it))
	{
		fmt_spec.sign = *it;
		++it;
	}

	if (it != end && *it == '0')
	{
		fmt_spec.zero_pad = true;
		++it;
	}

	fmt_spec.width = get_num();

	// has precision modifier
	if (it != end && *it == '.')
	{
		++it;
		bz_assert(it != end && *it >= '0' && *it <= '9');
		fmt_spec.precision = get_num();
	}

	if (it != end)
	{
		fmt_spec.type = *it;
		++it;
	}

	bz_assert(it == end);
	return fmt_spec;
}


inline u8string format_str(u8string_view str, format_spec spec)
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

	auto const str_length = str.length();
	auto const length = std::max(str_length, spec.width);
	auto res = u8string();
	auto const fill_char_width = [fill = spec.fill]() -> size_t {
		if (fill < (1u << 7))
		{
			return 1;
		}
		else if (fill < (1u << 11))
		{
			return 2;
		}
		else if (fill < (1u << 16))
		{
			return 3;
		}
		else
		{
			return 4;
		}
	}();
	res.reserve(str.size() + (length - str_length) * fill_char_width);

	switch (spec.align)
	{
	case '>':
		for (size_t i = 0; i < length - str_length; ++i)
		{
			res += spec.fill;
		}
		res += str;
		break;

	case '^':
	{
		// center alignment has left bias
		auto const left_fill = (length - str_length) / 2;
		auto const right_fill = (length - str_length) - left_fill;
		for (size_t i = 0; i < left_fill; ++i)
		{
			res += spec.fill;
		}
		res += str;
		for (size_t i = 0; i < right_fill; ++i)
		{
			res += spec.fill;
		}
		break;
	}

	case '<':
		res += str;
		for (size_t i = 0; i < length - str_length; ++i)
		{
			res += spec.fill;
		}
		break;

	default:
		bz_unreachable;
		break;
	}

	return res;
}

inline u8string format_char(u8char c, format_spec spec)
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
	bz_assert(spec.type == 'c');

	size_t const len = 1;
	auto const length = std::max(len, spec.width);
	auto res = u8string();
	auto const fill_char_width = [fill = spec.fill]() -> size_t {
		if (fill <= internal::max_one_byte_char)
		{
			return 1;
		}
		if (fill <= internal::max_two_byte_char)
		{
			return 2;
		}
		if (fill <= internal::max_three_byte_char)
		{
			return 3;
		}
		else
		{
			return 4;
		}
	}();
	auto const char_width = [c]() -> size_t {
		if (c <= internal::max_one_byte_char)
		{
			return 1;
		}
		if (c <= internal::max_two_byte_char)
		{
			return 2;
		}
		if (c <= internal::max_three_byte_char)
		{
			return 3;
		}
		else
		{
			return 4;
		}
	}();
	res.reserve(char_width + (length - len) * fill_char_width);

	switch (spec.align)
	{
	case '>':
		for (size_t i = 0; i < length - len; ++i)
		{
			res += spec.fill;
		}
		res += c;
		break;

	case '^':
	{
		// center alignment has left bias
		auto const left_fill = (length - len) / 2;
		auto const right_fill = (length - len) - left_fill;
		for (size_t i = 0; i < left_fill; ++i)
		{
			res += spec.fill;
		}
		res += c;
		for (size_t i = 0; i < right_fill; ++i)
		{
			res += spec.fill;
		}
		break;
	}

	case '<':
		res += c;
		for (size_t i = 0; i < length - len; ++i)
		{
			res += spec.fill;
		}
		break;

	default:
		bz_unreachable;
		break;
	}

	return res;
}

constexpr uint8_t digits_x[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
constexpr uint8_t digits_X[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

template<size_t base, typename Uint, typename = std::enable_if_t<
	std::is_same_v<Uint, uint32_t>
	|| std::is_same_v<Uint, uint64_t>
>>
u8string uint_to_string_base(Uint val, format_spec spec)
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


	auto const length = std::max(len, spec.width);
	auto res = u8string();
	auto const fill_char_width = [&]() -> size_t {
		if (spec.zero_pad)
		{
			return 1;
		}
		if (spec.fill < (1u << 7))
		{
			return 1;
		}
		else if (spec.fill < (1u << 11))
		{
			return 2;
		}
		else if (spec.fill < (1u << 16))
		{
			return 3;
		}
		else
		{
			return 4;
		}
	}();
	auto const fill_char = spec.zero_pad ? '0' : spec.fill;
	res.reserve(len + (length - len) * fill_char_width);

	auto const digits = spec.type == 'X' ? digits_X : digits_x;

	auto const put_num = [&]()
	{
		if constexpr (base == 10)
		{
			std::array<char, lg_uint(std::numeric_limits<Uint>::max())> buffer;
			auto const to_chars_res = std::to_chars(buffer.data(), buffer.data() + buffer.size(), val);
			bz_assert(to_chars_res.ec == std::errc{});
			bz_assert(to_chars_res.ptr == buffer.data() + len - (spec.sign == '-' ? 0 : 1));
			res += u8string_view(buffer.data(), to_chars_res.ptr);
		}
		else
		{
			std::array<uint8_t, log_uint<base>(std::numeric_limits<Uint>::max())> buffer;
			auto out = buffer.rbegin();
			auto copy = val;
			do
			{
				*out = digits[copy % base];
				++out;
				copy /= base;
			} while (copy != 0);
			auto const num_len = len - (spec.sign == '-' ? 0 : 1);
			bz_assert(out - buffer.rbegin() == static_cast<int64_t>(num_len));
			res += u8string_view(buffer.data() + buffer.size() - num_len, buffer.data() + buffer.size());
		}
	};

	switch (spec.align)
	{
	case '<':
		bz_assert(!spec.zero_pad);
		if (spec.sign != '-')
		{
			res += spec.sign;
		}
		put_num();
		for (size_t i = 0; i < (length - len); ++i)
		{
			res += fill_char;
		}
		break;

	case '^':
		bz_assert(!spec.zero_pad);
		// center alignment has a right bias
		for (size_t i = 0; i < (length - len + 1) / 2; ++i)
		{
			res += fill_char;
		}
		if (spec.sign != '-')
		{
			res += spec.sign;
		}
		put_num();
		for (size_t i = 0; i < (length - len) / 2; ++i)
		{
			res += fill_char;
		}
		break;

	case '>':
		if (spec.sign != '-' && spec.zero_pad)
		{
			res += spec.sign;
			for (size_t i = 0; i < length - len; ++i)
			{
				res += fill_char;
			}
		}
		else
		{
			for (size_t i = 0; i < length - len; ++i)
			{
				res += fill_char;
			}
			if (spec.sign != '-')
			{
				res += spec.sign;
			}
		}
		put_num();
		break;

	default:
		bz_unreachable;
		break;
	}

	return res;
}

// converts an unsigned integer to a string
template<typename Uint, typename = std::enable_if_t<
	std::is_same_v<Uint, uint32_t>
	|| std::is_same_v<Uint, uint64_t>
>>
u8string uint_to_string(Uint val, format_spec spec)
{
	if constexpr (std::is_same_v<Uint, uint32_t>)
	{
		if (spec.type == 'c')
		{
			return format_char(val, spec);
		}
	}
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
		bz_unreachable;
		return "";
	}
}

// converts an signed integer to a string
template<typename Int, typename = std::enable_if_t<
	std::is_same_v<Int, int32_t>
	|| std::is_same_v<Int, int64_t>
>>
u8string int_to_string(Int val, format_spec spec)
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
		// only allow decimal printing for signed numbers
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

	bool const is_negative = val < 0;
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

	bool const put_sign = is_negative || spec.sign != '-';

	size_t const len = lg_uint(abs_val) + (put_sign ? 1 : 0);

	auto const length = std::max(len, spec.width);
	auto res = u8string();
	auto const fill_char_width = [&]() -> size_t {
		if (spec.zero_pad)
		{
			return 1;
		}
		if (spec.fill < (1u << 7))
		{
			return 1;
		}
		else if (spec.fill < (1u << 11))
		{
			return 2;
		}
		else if (spec.fill < (1u << 16))
		{
			return 3;
		}
		else
		{
			return 4;
		}
	}();
	auto const fill_char = spec.zero_pad ? '0' : spec.fill;
	res.reserve(len + (length - len) * fill_char_width);

	auto put_num = [&]()
	{
		std::array<char, lg_uint(std::numeric_limits<Uint>::max())> buffer;
		auto const to_chars_res = std::to_chars(buffer.data(), buffer.data() + buffer.size(), abs_val);
		bz_assert(to_chars_res.ec == std::errc{});
		bz_assert(to_chars_res.ptr == buffer.data() + len - (put_sign ? 1 : 0));
		res += u8string_view(buffer.data(), to_chars_res.ptr);
	};

	switch (spec.align)
	{
	case '<':
		bz_assert(!spec.zero_pad);
		if (put_sign)
		{
			res += spec.sign;
		}
		put_num();
		for (size_t i = 0; i < length - len; ++i)
		{
			res += fill_char;
		}
		break;

	case '^':
		bz_assert(!spec.zero_pad);
		// center alignment has right bias
		for (size_t i = 0; i < (length - len + 1) / 2; ++i)
		{
			res += fill_char;
		}
		if (put_sign)
		{
			res += spec.sign;
		}
		put_num();
		for (size_t i = 0; i < (length - len) / 2; ++i)
		{
			res += fill_char;
		}
		break;

	case '>':
		if (spec.zero_pad)
		{
			if (put_sign)
			{
				res += spec.sign;
			}
			for (size_t i = 0; i < length - len; ++i)
			{
				res += fill_char;
			}
		}
		else
		{
			for (size_t i = 0; i < length - len; ++i)
			{
				res += fill_char;
			}
			if (put_sign)
			{
				res += spec.sign;
			}
		}
		put_num();
		break;

	default:
		bz_unreachable;
		break;
	}

	return res;
}

template<size_t N>
u8string float_to_string_dec(
	typename stack_vector<uint8_t, N>::iterator dig_it,
	typename stack_vector<uint8_t, N>::iterator dig_end,
	int32_t dig_exponent,
	bool put_sign,
	format_spec spec
)
{
	bz_assert(spec.precision != format_spec::precision_none);
	size_t len = spec.precision                                         // decimal part
		+ (spec.precision == 0 ? 0 : 1)                                 // .
		+ (dig_exponent < 0 ? 1 : static_cast<size_t>(dig_exponent + 1)) // integer part
		+ (put_sign ? 1 : 0)                                            // sign
		+ (spec.type == '%' ? 1 : 0);                                   // %

	auto const length = std::max(len, spec.width);
	auto res = u8string();
	auto const fill_char_width = [&]() -> size_t {
		if (spec.zero_pad)
		{
			return 1;
		}
		if (spec.fill < (1u << 7))
		{
			return 1;
		}
		else if (spec.fill < (1u << 11))
		{
			return 2;
		}
		else if (spec.fill < (1u << 16))
		{
			return 3;
		}
		else
		{
			return 4;
		}
	}();
	auto const fill_char = spec.zero_pad ? '0' : spec.fill;
	res.reserve(len + (length - len) * fill_char_width);

	auto const put_num = [&]()
	{
		// integer part
		if (dig_exponent < 0)
		{
			res += '0';
		}
		else
		{
			for (int i = 0; i < dig_exponent + 1; ++i)
			{
				res += *dig_it + '0';
				++dig_it;
			}
		}

		// decimal part
		if (spec.precision != 0)
		{
			res += '.';
			size_t i = 0;
			// print the leading zero digits (if any)
			if (dig_exponent < 0)
			{
				auto const abs_dig_exponent = static_cast<uint32_t>(-dig_exponent);
				for (; i < abs_dig_exponent - 1; ++i)
				{
					res += '0';
				}
			}
			// print the non-zero digits
			for (; i < spec.precision && dig_it != dig_end; ++i)
			{
				res += *dig_it + '0';
				++dig_it;
			}
			// print the trailing zero digits (if any)
			for (; i < spec.precision; ++i)
			{
				res += '0';
			}
		}
	};

	switch (spec.align)
	{
	case '<':
		bz_assert(!spec.zero_pad);
		if (put_sign)
		{
			res += spec.sign;
		}
		put_num();
		if (spec.type == '%')
		{
			res += '%';
		}
		for (size_t i = 0; i < length - len; ++i)
		{
			res += fill_char;
		}
		break;

	case '^':
		bz_assert(!spec.zero_pad);
		for (size_t i = 0; i < (length - len + 1) / 2; ++i)
		{
			res += fill_char;
		}
		if (put_sign)
		{
			res += spec.sign;
		}
		put_num();
		if (spec.type == '%')
		{
			res += '%';
		}
		for (size_t i = 0; i < (length - len) / 2; ++i)
		{
			res += fill_char;
		}
		break;

	case '>':
		if (spec.zero_pad)
		{
			if (put_sign)
			{
				res += spec.sign;
			}
			for (size_t i = 0; i < length - len; ++i)
			{
				res += fill_char;
			}
		}
		else
		{
			for (size_t i = 0; i < length - len; ++i)
			{
				res += fill_char;
			}
			if (put_sign)
			{
				res += spec.sign;
			}
		}
		put_num();
		if (spec.type == '%')
		{
			res += '%';
		}
		break;

	default:
		bz_unreachable;
		break;
	}

	return res;


/*
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
*/
}

template<size_t N>
u8string float_to_string_exp(
	typename stack_vector<uint8_t, N>::iterator dig_it,
	typename stack_vector<uint8_t, N>::iterator dig_end,
	int32_t dig_exponent,
	bool put_sign,
	format_spec spec
)
{
	auto const print_len = dig_end - dig_it;
	bz_assert(print_len != 0);
	size_t const len = static_cast<size_t>(
		print_len + 1                             // digits + .
		+ (put_sign ? 1 : 0)                      // sign
		+ (std::abs(dig_exponent) >= 100 ? 5 : 4) // exponential notation
	);
	// the exponential notation is either e+... or e+.. (. is a digit)

	auto const length = std::max(len, spec.width);
	auto res = u8string();
	auto const fill_char_width = [&]() -> size_t {
		if (spec.zero_pad)
		{
			return 1;
		}
		if (spec.fill < (1u << 7))
		{
			return 1;
		}
		else if (spec.fill < (1u << 11))
		{
			return 2;
		}
		else if (spec.fill < (1u << 16))
		{
			return 3;
		}
		else
		{
			return 4;
		}
	}();
	auto const fill_char = spec.zero_pad ? '0' : spec.fill;
	res.reserve(len + (length - len) * fill_char_width);

	auto const put_num = [&]()
	{
		res += *dig_it + '0';
		++dig_it;
		res += '.';
		for (; dig_it != dig_end; ++dig_it)
		{
			res += *dig_it + '0';
		}
		res += 'e';
		res += dig_exponent < 0 ? '-' : '+';
		auto abs_dig_exponent = std::abs(dig_exponent);
		if (abs_dig_exponent >= 100)
		{
			auto const div_100 = abs_dig_exponent / 100;
			auto const rem_100 = abs_dig_exponent % 100;
			abs_dig_exponent = rem_100;
			res += static_cast<u8char>(div_100 + '0');
		}
		auto const div_10 = abs_dig_exponent / 10;
		auto const rem_10 = abs_dig_exponent % 10;
		res += static_cast<u8char>(div_10 + '0');
		res += static_cast<u8char>(rem_10 + '0');
	};

	switch (spec.align)
	{
	case '<':
		bz_assert(!spec.zero_pad);
		if (put_sign)
		{
			res += spec.sign;
		}
		put_num();
		if (spec.type == '%')
		{
			res += '%';
		}
		for (size_t i = 0; i < length - len; ++i)
		{
			res += fill_char;
		}
		break;

	case '^':
		bz_assert(!spec.zero_pad);
		for (size_t i = 0; i < (length - len + 1) / 2; ++i)
		{
			res += fill_char;
		}
		if (put_sign)
		{
			res += spec.sign;
		}
		put_num();
		if (spec.type == '%')
		{
			res += '%';
		}
		for (size_t i = 0; i < (length - len) / 2; ++i)
		{
			res += fill_char;
		}
		break;

	case '>':
		if (spec.zero_pad)
		{
			if (put_sign)
			{
				res += spec.sign;
			}
			for (size_t i = 0; i < length - len; ++i)
			{
				res += fill_char;
			}
		}
		else
		{
			for (size_t i = 0; i < length - len; ++i)
			{
				res += fill_char;
			}
			if (put_sign)
			{
				res += spec.sign;
			}
		}
		put_num();
		if (spec.type == '%')
		{
			res += '%';
		}
		break;

	default:
		bz_unreachable;
		break;
	}

	return res;


/*
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
*/
}

template<typename Float, typename = std::enable_if_t<
	std::is_same_v<Float, float>
	|| std::is_same_v<Float, double>
>>
u8string float_to_string(Float val, format_spec spec)
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
		auto const fill_char_width = [&]() -> size_t {
			if (spec.zero_pad)
			{
				return 1;
			}
			if (spec.fill < (1u << 7))
			{
				return 1;
			}
			else if (spec.fill < (1u << 11))
			{
				return 2;
			}
			else if (spec.fill < (1u << 16))
			{
				return 3;
			}
			else
			{
				return 4;
			}
		}();
		if (std::isinf(val))
		{
			size_t const len = 3 + (put_sign ? 1 : 0);
			auto const length = std::max(len, spec.width);
			auto res = u8string();
			res.reserve(len + (length - len) * fill_char_width);

			switch (spec.align)
			{
			case '<':
				bz_assert(!spec.zero_pad);
				if (put_sign)
				{
					res += spec.sign;
				}
				res += "inf";
				for (size_t i = 0; i < length - len; ++i)
				{
					res += spec.fill;
				}
				break;

			case '^':
				bz_assert(!spec.zero_pad);
				for (size_t i = 0; i < (length - len + 1) / 2; ++i)
				{
					res += spec.fill;
				}
				if (put_sign)
				{
					res += spec.sign;
				}
				res += "inf";
				for (size_t i = 0; i < (length - len) / 2; ++i)
				{
					res += spec.fill;
				}
				break;

			case '>':
				for (size_t i = 0; i < length - len; ++i)
				{
					res += spec.fill;
				}
				if (put_sign)
				{
					res += spec.sign;
				}
				res += "inf";
				break;

			default:
				bz_unreachable;
				break;
			}

			return res;
		}
		else
		{
			size_t const len = 3;
			auto const length = std::max(len, spec.width);
			auto res = u8string();
			res.reserve(len + (length - len) * fill_char_width);

			switch (spec.align)
			{
			case '<':
				bz_assert(!spec.zero_pad);
				res += "nan";
				for (size_t i = 0; i < length - len; ++i)
				{
					res += spec.fill;
				}
				break;

			case '^':
				bz_assert(!spec.zero_pad);
				for (size_t i = 0; i < (length - len + 1) / 2; ++i)
				{
					res += spec.fill;
				}
				res += "nan";
				for (size_t i = 0; i < (length - len) / 2; ++i)
				{
					res += spec.fill;
				}
				break;

			case '>':
				for (size_t i = 0; i < length - len; ++i)
				{
					res += spec.fill;
				}
				res += "nan";
				break;

			default:
				bz_unreachable;
				break;
			}

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
			out_it += (out_digits.size() - static_cast<size_t>(out_exponent))
				- (frac_digits.size() - static_cast<size_t>(frac_exponent));

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
		auto val_exp = val_components.components.exponent - exponent_offset;

		frac_digits.push_back(1);

		// if the exponent is zero, the number uses fixed point notation
		// with a base of 2^(-exponent_offset + 1)
		if (val_components.components.exponent == 0)
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
			if ((val_components.components.mantissa & (unit << i)) != 0)
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
		print_len = static_cast<decltype(print_len)>(spec.precision);
	}
	else
	{
		int32_t len = (out_exponent + 1) + static_cast<int32_t>(spec.precision);
		print_len = len < 0 ? 0 : len;
	}

	// round the last digit of the range that we print
	{
		auto begin = out_digits.rbegin();
		auto it    = begin;
		auto end   = out_digits.rend();

		if (static_cast<int32_t>(out_digits.size()) > print_len)
		{
			it += static_cast<size_t>(static_cast<int32_t>(out_digits.size()) - print_len);
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
		int32_t len = (out_exponent + 1) + static_cast<int32_t>(spec.precision);
		print_len = len < 0 ? 0 : len;
	}

	if (static_cast<int32_t>(out_digits.size()) < print_len)
	{
		print_len = static_cast<int32_t>(out_digits.size());
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

		print_len = static_cast<int32_t>(last_non_zero - begin);
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
			spec.precision = static_cast<size_t>(print_len - out_exponent - 1);
			spec.type = 'f';
			return float_to_string_dec<N>(
				out_digits.begin(),
				out_digits.begin() + print_len,
				out_exponent,
				put_sign,
				spec
			);
		}
		// print with exponential format
		else
		{
			return float_to_string_exp<N>(
				out_digits.begin(),
				out_digits.begin() + print_len,
				out_exponent,
				put_sign,
				spec
			);
		}

	case 'f':
	case '%':
		return float_to_string_dec<N>(
			out_digits.begin(),
			out_digits.begin() + print_len,
			out_exponent,
			put_sign,
			spec
		);

	default:
		bz_unreachable;
		return "";
	}

	return "";
}

inline u8string pointer_to_string(const void *ptr, format_spec spec)
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

	auto const val = reinterpret_cast<uint64_t>(ptr);
	size_t const len = log_uint<16>(val) + 2;

	auto const length = std::max(len, spec.width);
	auto res = u8string();
	auto const fill_char_width = [&]() -> size_t {
		if (spec.zero_pad)
		{
			return 1;
		}
		if (spec.fill < (1u << 7))
		{
			return 1;
		}
		else if (spec.fill < (1u << 11))
		{
			return 2;
		}
		else if (spec.fill < (1u << 16))
		{
			return 3;
		}
		else
		{
			return 4;
		}
	}();
	auto const fill_char = spec.zero_pad ? '0' : spec.fill;
	res.reserve(len + (length - len) * fill_char_width);

	auto put_num = [&]()
	{
		res += "0x";
		auto copy = val;
		auto shift_amount = (len - 2) * 4;
		do
		{
			shift_amount -= 4;
			res += digits_x[(copy >> shift_amount) & 0xf];
			copy &= shift_amount == 0 ? 0 : (1ull << shift_amount) - 1;
		} while (shift_amount != 0);
	};

	if (spec.zero_pad)
	{
		put_num();
	}
	else
	{
		switch (spec.align)
		{
		case '<':
			put_num();
			for (size_t i = 0; i < length - len; ++i)
			{
				res += fill_char;
			}
			break;

		case '^':
			// center alignment has right bias
			for (size_t i = 0; i < (length - len + 1) / 2; ++i)
			{
				res += fill_char;
			}
			put_num();
			for (size_t i = 0; i < (length - len) / 2; ++i)
			{
				res += fill_char;
			}
			break;

		case '>':
			for (size_t i = 0; i < length - len; ++i)
			{
				res += fill_char;
			}
			put_num();
			break;

		default:
			bz_unreachable;
			break;
		}
	}

	return res;
}

} // namepsace internal




template<typename T>
struct formatter;

namespace internal
{

template<typename T>
struct has_formatter_impl
{
	using yes = int;
	using no  = unsigned;
	static_assert(!meta::is_same<yes, no>);

	template<typename U>
	static decltype(
		formatter<U>::format(std::declval<T>(), std::declval<u8string_view>()),
		yes{}
	) test(U const &u);

	static no test(...);

	static constexpr bool value = meta::is_same<decltype(test(std::declval<T const &>())), yes>;
};

} // namespace internal

template<typename T>
constexpr bool has_formatter = internal::has_formatter_impl<T>::value;

template<typename T>
struct formatter<const T> : formatter<T>
{};

template<>
struct formatter<u8string_view>
{
	static u8string format(u8string_view str, u8string_view spec)
	{
		return internal::format_str(str, internal::get_default_format_spec(spec));
	}
};

template<>
struct formatter<u8string> : formatter<u8string_view>
{};

template<>
struct formatter<const char *> : formatter<u8string_view>
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
	static u8string format(int32_t val, u8string_view spec)
	{
		return internal::int_to_string(val, internal::get_default_format_spec(spec));
	}
};

template<>
struct formatter<uint32_t>
{
	static u8string format(uint32_t val, u8string_view spec)
	{
		return internal::uint_to_string(val, internal::get_default_format_spec(spec));
	}
};

template<>
struct formatter<int64_t>
{
	static u8string format(int64_t val, u8string_view spec)
	{
		return internal::int_to_string(val, internal::get_default_format_spec(spec));
	}
};

template<>
struct formatter<uint64_t>
{
	static u8string format(uint64_t val, u8string_view spec)
	{
		return internal::uint_to_string(val, internal::get_default_format_spec(spec));
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
	static u8string format(char val, u8string_view spec)
	{
		bz_assert(spec.size() == 0);
		return u8string(1, static_cast<u8char>(val));
	}
};

template<>
struct formatter<bool>
{
	static u8string format(bool val, u8string_view spec)
	{
		bz_assert(spec.size() == 0);

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
	static u8string format(float val, u8string_view spec)
	{
		return internal::float_to_string(val, internal::get_default_format_spec(spec));
	}
};

template<>
struct formatter<double>
{
	static u8string format(double val, u8string_view spec)
	{
		return internal::float_to_string(val, internal::get_default_format_spec(spec));
	}
};

template<>
struct formatter<const void *>
{
	static u8string format(const void *ptr, u8string_view spec)
	{
		return internal::pointer_to_string(ptr, internal::get_default_format_spec(spec));
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

template<typename ...Args>
format_args make_format_args(Args const &...args)
{
	return {
		format_arg{
			&args,
			[](const void *arg, u8string_view spec) -> u8string
			{
				return formatter<Args>::format(
					*static_cast<const Args *>(arg),
					spec
				);
			}
		}...
	};
}

inline u8string format_impl(
	size_t &current_arg,
	u8string_view::iterator fmt,
	u8string_view::iterator fmt_end,
	format_args const &args
)
{
	u8string res = "";
	auto begin = fmt;

	while (fmt != fmt_end)
	{
		switch (*fmt)
		{
		case '{':
		{
			res += u8string_view(begin, fmt);
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
			res += to_print.format(to_print.arg, u8string_view(spec, spec_end));

			begin = fmt;
			break;
		}

		default:
			++fmt;
			break;
		}
	}
	res += u8string_view(begin, fmt);

	return res;
}

} // namespace internal

template<typename ...Ts>
u8string format(u8string_view fmt, Ts const &...ts)
{
	static_assert((has_formatter<Ts> && ...), "one or more arguments don't have a bz::formatter specialization");
	if constexpr ((has_formatter<Ts> && ...))
	{
		size_t current_arg = 0;
		return internal::format_impl(
			current_arg,
			fmt.begin(),
			fmt.end(),
			internal::make_format_args(ts...)
		);
	}
	else
	{
		return "";
	}
}

bz_end_namespace

#endif // _bz_format_h__
