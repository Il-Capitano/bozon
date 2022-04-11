#ifndef _bz_format_h__
#define _bz_format_h__

#include "core.h"
// #include "string.h"
#include "u8string.h"
#include "u8string_view.h"
#include "vector.h"
#include "optional.h"
#include "array.h"

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

inline void set_console_attributes(HANDLE h, u8string_view str)
{
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
	if (file != stdout && file != stderr)
	{
		fprint_string_view(str, file);
	}
	else
	{
		auto const h = file == stdout ? GetStdHandle(STD_OUTPUT_HANDLE) : GetStdHandle(STD_ERROR_HANDLE);
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
			internal::set_console_attributes(h, u8string_view(it, closing_m));
			begin = closing_m;
			++begin;
		}
	}
}

} // namespace internal

template<typename ...Ts>
void print(std::FILE *file, u8string_view fmt, Ts const &...ts)
{
	if constexpr (sizeof... (Ts) == 0)
	{
		internal::fprint(fmt, file);
	}
	else
	{
		auto const str = format(fmt, ts...);
		internal::fprint(str, file);
	}
}

#else

template<typename ...Ts>
void print(std::FILE *file, u8string_view fmt, Ts const &...ts)
{
	if constexpr (sizeof... (Ts) == 0)
	{
		fwrite(fmt.data(), sizeof (char), fmt.size(), file);
	}
	else
	{
		auto const str = format(fmt, ts...);
		fwrite(str.data(), sizeof (char), str.size(), file);
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

template<typename ...Ts>
void print(u8string_view fmt, Ts const &...ts)
{
	print(stdout, fmt, ts...);
}

template<typename ...Ts>
void log(u8string_view fmt, Ts const &...ts)
{
	print(stderr, fmt, ts...);
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

extern "C" char *d2s_short(double x, char *buffer);
extern "C" char *d2exp_short(double x, uint32_t precision, char *buffer, bool capital_e, bool strip_trailing_zeros);
extern "C" char *d2fixed_short(double x, uint32_t precision, char *buffer, bool strip_trailing_zeros);
extern "C" char *f2s_short(float x, char *buffer);

// generated by scripts/gen_float_limit_table.py
// bounds[precision] returns the number x for which if num < x, precision number of digits
// are needed before the decimal point to represent the number
constexpr array<double, 309> upper_bounds = {
	1.0, 10.0, 100.0, 1000.0, 10000.0,
	100000.0, 1000000.0, 10000000.0, 100000000.0, 1000000000.0,
	10000000000.0, 100000000000.0, 1000000000000.0, 10000000000000.0, 100000000000000.0,
	1000000000000000.0, 1e+16, 1e+17, 1e+18, 1e+19,
	1e+20, 1e+21, 1e+22, 1.0000000000000001e+23, 1.0000000000000001e+24,
	1e+25, 1e+26, 1e+27, 1.0000000000000002e+28, 1.0000000000000001e+29,
	1e+30, 1.0000000000000001e+31, 1e+32, 1.0000000000000001e+33, 1.0000000000000001e+34,
	1.0000000000000002e+35, 1e+36, 1.0000000000000001e+37, 1.0000000000000002e+38, 1.0000000000000001e+39,
	1e+40, 1e+41, 1e+42, 1e+43, 1e+44,
	1.0000000000000001e+45, 1.0000000000000001e+46, 1e+47, 1e+48, 1.0000000000000001e+49,
	1e+50, 1.0000000000000002e+51, 1.0000000000000001e+52, 1.0000000000000002e+53, 1e+54,
	1e+55, 1e+56, 1e+57, 1.0000000000000001e+58, 1.0000000000000001e+59,
	1.0000000000000001e+60, 1.0000000000000001e+61, 1e+62, 1e+63, 1e+64,
	1.0000000000000001e+65, 1.0000000000000001e+66, 1.0000000000000001e+67, 1.0000000000000001e+68, 1e+69,
	1e+70, 1e+71, 1.0000000000000001e+72, 1.0000000000000001e+73, 1.0000000000000001e+74,
	1.0000000000000001e+75, 1e+76, 1.0000000000000001e+77, 1e+78, 1.0000000000000001e+79,
	1e+80, 1.0000000000000001e+81, 1.0000000000000001e+82, 1e+83, 1e+84,
	1e+85, 1e+86, 1.0000000000000002e+87, 1.0000000000000001e+88, 1.0000000000000001e+89,
	1.0000000000000001e+90, 1e+91, 1e+92, 1e+93, 1e+94,
	1e+95, 1e+96, 1e+97, 1.0000000000000001e+98, 1.0000000000000001e+99,
	1e+100, 1.0000000000000001e+101, 1.0000000000000001e+102, 1e+103, 1e+104,
	1.0000000000000001e+105, 1e+106, 1.0000000000000001e+107, 1e+108, 1.0000000000000002e+109,
	1e+110, 1.0000000000000001e+111, 1.0000000000000001e+112, 1e+113, 1e+114,
	1e+115, 1e+116, 1e+117, 1.0000000000000001e+118, 1.0000000000000001e+119,
	1.0000000000000001e+120, 1e+121, 1e+122, 1.0000000000000001e+123, 1.0000000000000001e+124,
	1.0000000000000001e+125, 1.0000000000000001e+126, 1.0000000000000001e+127, 1e+128, 1.0000000000000002e+129,
	1e+130, 1.0000000000000001e+131, 1.0000000000000001e+132, 1e+133, 1.0000000000000001e+134,
	1.0000000000000001e+135, 1e+136, 1e+137, 1e+138, 1e+139,
	1e+140, 1e+141, 1e+142, 1e+143, 1e+144,
	1.0000000000000001e+145, 1.0000000000000002e+146, 1.0000000000000002e+147, 1e+148, 1e+149,
	1.0000000000000002e+150, 1e+151, 1e+152, 1.0000000000000002e+153, 1e+154,
	1e+155, 1.0000000000000002e+156, 1.0000000000000001e+157, 1.0000000000000001e+158, 1.0000000000000001e+159,
	1e+160, 1e+161, 1.0000000000000001e+162, 1.0000000000000001e+163, 1e+164,
	1.0000000000000001e+165, 1.0000000000000001e+166, 1e+167, 1.0000000000000001e+168, 1.0000000000000001e+169,
	1e+170, 1.0000000000000002e+171, 1e+172, 1e+173, 1e+174,
	1.0000000000000001e+175, 1e+176, 1e+177, 1e+178, 1.0000000000000001e+179,
	1e+180, 1.0000000000000001e+181, 1e+182, 1.0000000000000001e+183, 1e+184,
	1.0000000000000001e+185, 1.0000000000000001e+186, 1.0000000000000001e+187, 1e+188, 1e+189,
	1e+190, 1e+191, 1e+192, 1e+193, 1.0000000000000001e+194,
	1.0000000000000001e+195, 1.0000000000000002e+196, 1.0000000000000001e+197, 1e+198, 1e+199,
	1.0000000000000001e+200, 1e+201, 1.0000000000000001e+202, 1.0000000000000002e+203, 1.0000000000000001e+204,
	1e+205, 1e+206, 1e+207, 1.0000000000000001e+208, 1e+209,
	1.0000000000000001e+210, 1.0000000000000001e+211, 1.0000000000000001e+212, 1.0000000000000001e+213, 1.0000000000000001e+214,
	1.0000000000000001e+215, 1e+216, 1.0000000000000001e+217, 1e+218, 1.0000000000000001e+219,
	1.0000000000000001e+220, 1e+221, 1e+222, 1e+223, 1.0000000000000002e+224,
	1.0000000000000001e+225, 1.0000000000000001e+226, 1e+227, 1.0000000000000001e+228, 1.0000000000000001e+229,
	1e+230, 1e+231, 1e+232, 1.0000000000000002e+233, 1e+234,
	1e+235, 1e+236, 1.0000000000000001e+237, 1e+238, 1.0000000000000001e+239,
	1e+240, 1e+241, 1e+242, 1e+243, 1e+244,
	1e+245, 1e+246, 1.0000000000000001e+247, 1e+248, 1.0000000000000001e+249,
	1.0000000000000001e+250, 1e+251, 1e+252, 1.0000000000000001e+253, 1.0000000000000001e+254,
	1.0000000000000002e+255, 1e+256, 1e+257, 1e+258, 1.0000000000000001e+259,
	1e+260, 1.0000000000000001e+261, 1e+262, 1e+263, 1e+264,
	1e+265, 1e+266, 1.0000000000000001e+267, 1.0000000000000002e+268, 1e+269,
	1e+270, 1.0000000000000001e+271, 1e+272, 1.0000000000000001e+273, 1.0000000000000001e+274,
	1.0000000000000001e+275, 1e+276, 1e+277, 1.0000000000000001e+278, 1e+279,
	1e+280, 1e+281, 1e+282, 1.0000000000000002e+283, 1e+284,
	1.0000000000000001e+285, 1e+286, 1e+287, 1e+288, 1e+289,
	1e+290, 1.0000000000000001e+291, 1e+292, 1.0000000000000001e+293, 1e+294,
	1.0000000000000001e+295, 1.0000000000000002e+296, 1e+297, 1.0000000000000001e+298, 1e+299,
	1e+300, 1e+301, 1e+302, 1e+303, 1.0000000000000001e+304,
	1.0000000000000001e+305, 1e+306, 1.0000000000000001e+307, 1e+308,
};

constexpr array<double, 17> lower_bounds = {
	0.0, // this is unused
	9.499999999999999e-05, 9.949999999999999e-05, 9.994999999999999e-05, 9.9995e-05, 9.99995e-05,
	9.999994999999999e-05, 9.9999995e-05, 9.99999995e-05, 9.999999994999999e-05, 9.999999999499999e-05,
	9.99999999995e-05, 9.999999999995e-05, 9.999999999999499e-05, 9.999999999999949e-05, 9.999999999999994e-05,
	9.999999999999999e-05,
};

constexpr array<array<double, 4>, 17> trailing_zeros_upper_bounds = {{
	{ 0.0, 0.0, 0.0, 0.0 }, // this is unused
	{ 0.09499999999999999, 0.0095, 0.00095, 9.499999999999999e-05 },
	{ 0.09949999999999999, 0.009949999999999999, 0.0009949999999999998, 9.949999999999999e-05 },
	{ 0.09995, 0.009994999999999999, 0.0009995, 9.994999999999999e-05 },
	{ 0.09999499999999999, 0.0099995, 0.0009999499999999999, 9.9995e-05 },
	{ 0.09999949999999999, 0.009999949999999999, 0.0009999949999999998, 9.99995e-05 },
	{ 0.09999994999999999, 0.009999995, 0.0009999994999999999, 9.999994999999999e-05 },
	{ 0.099999995, 0.009999999499999999, 0.0009999999499999998, 9.9999995e-05 },
	{ 0.09999999949999999, 0.00999999995, 0.0009999999949999998, 9.99999995e-05 },
	{ 0.09999999994999999, 0.009999999995, 0.0009999999994999998, 9.999999994999999e-05 },
	{ 0.09999999999499999, 0.009999999999499999, 0.0009999999999499999, 9.999999999499999e-05 },
	{ 0.09999999999949999, 0.009999999999949999, 0.000999999999995, 9.99999999995e-05 },
	{ 0.09999999999994999, 0.009999999999994999, 0.0009999999999995, 9.999999999995e-05 },
	{ 0.099999999999995, 0.009999999999999499, 0.00099999999999995, 9.999999999999499e-05 },
	{ 0.09999999999999949, 0.00999999999999995, 0.0009999999999999948, 9.999999999999949e-05 },
	{ 0.09999999999999994, 0.009999999999999993, 0.0009999999999999994, 9.999999999999994e-05 },
	{ 0.09999999999999999, 0.009999999999999998, 0.0009999999999999998, 9.999999999999999e-05 },
}};

inline u8string format_number(u8string_view number_str, format_spec spec, bool put_sign)
{
	if (spec.align == '\0')
	{
		spec.align = '>';
	}
	if (spec.fill == '\0')
	{
		spec.fill = ' ';
	}

	switch (spec.align)
	{
	case '<':
	{
		u8string result = "";
		auto const len = number_str.length() + static_cast<std::size_t>(put_sign);
		result.reserve(std::max(len, spec.width));
		if (put_sign)
		{
			result += spec.sign;
		}
		result += number_str;
		if (len < spec.width)
		{
			for ([[maybe_unused]] auto const _ : iota(len, spec.width))
			{
				result += spec.fill;
			}
		}
		return result;
	}
	case '^':
	{
		u8string result = "";
		auto const len = number_str.length() + static_cast<std::size_t>(put_sign);
		result.reserve(std::max(len, spec.width));
		if (len < spec.width)
		{
			auto const diff = spec.width - len;
			// if the spacing is uneven, prefer right alignment
			if (spec.zero_pad)
			{
				if (put_sign)
				{
					result += spec.sign;
				}
				for ([[maybe_unused]] auto const _ : iota(0, (diff + 1) / 2))
				{
					result += '0';
				}
				result += number_str;
			}
			else
			{
				for ([[maybe_unused]] auto const _ : iota(0, (diff + 1) / 2))
				{
					result += spec.fill;
				}
				if (put_sign)
				{
					result += spec.sign;
				}
				result += number_str;
			}
			for ([[maybe_unused]] auto const _ : iota(0, diff / 2))
			{
				result += spec.fill;
			}
		}
		else
		{
			if (put_sign)
			{
				result += spec.sign;
			}
			result += number_str;
		}
	}
	case '>':
	{
		u8string result = "";
		auto const len = number_str.length() + static_cast<std::size_t>(put_sign);
		result.reserve(std::max(len, spec.width));
		if (len < spec.width)
		{
			if (spec.zero_pad)
			{
				if (put_sign)
				{
					result += spec.sign;
				}
				for ([[maybe_unused]] auto const _ : iota(len, spec.width))
				{
					result += '0';
				}
				result += number_str;
			}
			else
			{
				for ([[maybe_unused]] auto const _ : iota(len, spec.width))
				{
					result += spec.fill;
				}
				if (put_sign)
				{
					result += spec.sign;
				}
				result += number_str;
			}
		}
		else
		{
			if (put_sign)
			{
				result += spec.sign;
			}
			result += number_str;
		}
		return result;
	}
	default:
		bz_unreachable;
	}
}

inline u8string format_float64_fixed(double x, format_spec spec)
{
	// most results should fit in this buffer
	array<char, 32> static_buffer = {};
	auto const finite = std::isfinite(x);
	spec.zero_pad &= finite;

	auto const abs_x = std::abs(x);
	auto const result_len = [&]() -> std::size_t {
		if (!finite)
		{
			return 4;
		}
		else
		{
			std::size_t const leading_digit_count = std::upper_bound(upper_bounds.begin() + 1, upper_bounds.end(), abs_x) - upper_bounds.begin();
			return spec.precision == 0 ? leading_digit_count + 1 : leading_digit_count + 1 + 1 + spec.precision;
		}
	}();
	auto const result_buffer = result_len <= static_buffer.size() ? static_buffer.data() : new char[result_len]{};

	auto const end = d2fixed_short(x, spec.precision, result_buffer, spec.type != 'f' && spec.type != 'F');
	bz_assert(end <= result_buffer + result_len);
	auto const result_str = *result_buffer == '-' ? u8string_view(result_buffer + 1, end) : u8string_view(result_buffer, end);
	auto const put_sign = *result_buffer == '-' ? (spec.sign = '-', true) : (!std::isnan(x) && (spec.sign == '+' || spec.sign == ' '));

	auto result = format_number(result_str, spec, put_sign);

	if (result_buffer != static_buffer.data())
	{
		delete[] result_buffer;
	}

	return result;
}

inline u8string format_float64_exponential(double x, format_spec spec)
{
	// most results should fit in this buffer
	array<char, 32> static_buffer = {};
	auto const finite = std::isfinite(x);
	spec.zero_pad &= finite;
	// sign + "X." + precision + e+XXX
	std::size_t const result_len = !finite
		// sign + "inf" or "nan"
		? 4
		// sign + leading digit + dot + precision + "e+000"
		: 1 + 1 + 1 + spec.precision + 5;
	auto const result_buffer = result_len <= static_buffer.size() ? static_buffer.data() : new char[result_len]{};

	auto const end = d2exp_short(
		x,
		spec.precision,
		result_buffer, spec.type == 'E' || spec.type == 'G',
		spec.type != 'e' && spec.type != 'E'
	);
	assert(end <= result_buffer + result_len);
	auto const result_str = *result_buffer == '-' ? u8string_view(result_buffer + 1, end) : u8string_view(result_buffer, end);
	auto const put_sign = *result_buffer == '-' ? (spec.sign = '-', true) : (!std::isnan(x) && (spec.sign == '+' || spec.sign == ' '));

	auto result = format_number(result_str, spec, put_sign);

	if (result_buffer != static_buffer.data())
	{
		delete[] result_buffer;
	}

	return result;
}

inline u8string format_float64_generic(double x, format_spec spec)
{
	assert(spec.precision != format_spec::precision_none);
	if (spec.precision == 0)
	{
		spec.precision = 1;
	}

	auto const abs_x = std::abs(x);
	auto const upper_bound = upper_bounds[spec.precision];
	auto const lower_bound = spec.precision >= lower_bounds.size() ? lower_bounds.back() : lower_bounds[spec.precision];
	if (abs_x == 0.0)
	{
		// special case for 0.0
		spec.precision = 0;
		return format_float64_fixed(x, spec);
	}
	else if (abs_x <= lower_bound || abs_x >= upper_bound || !std::isfinite(x))
	{
		// the leading digit isn't counted in format_float64_exponential
		spec.precision -= 1;
		return format_float64_exponential(x, spec);
	}
	else if (abs_x >= 1.0)
	{
		std::size_t const leading_digit_count = std::upper_bound(upper_bounds.begin() + 1, upper_bounds.end(), abs_x) - upper_bounds.begin();
		assert(leading_digit_count <= spec.precision);
		spec.precision -= leading_digit_count;
		return format_float64_fixed(x, spec);
	}
	else
	{
		auto const &bounds = spec.precision >= trailing_zeros_upper_bounds.size()
			? trailing_zeros_upper_bounds.back()
			: trailing_zeros_upper_bounds[spec.precision];
		std::size_t const trailing_zeros_count = std::upper_bound(bounds.begin() + 1, bounds.end(), abs_x) - bounds.begin();
		spec.precision += trailing_zeros_count;
		return format_float64_fixed(x, spec);
	}
}

inline u8string format_float64_percentage(double x, format_spec spec)
{
	if (!std::isfinite(x))
	{
		return format_float64_generic(x, spec);
	}

	// most results should fit in this buffer
	array<char, 32> static_buffer = {};

	auto const abs_x = std::abs(x);
	std::size_t const leading_digit_count = std::upper_bound(upper_bounds.begin() + 1, upper_bounds.end(), abs_x) - upper_bounds.begin();
	// sign + leading digits + dot + precision + '%'
	std::size_t const result_len = 1 + leading_digit_count + 1 + spec.precision + 2 + 1;
	auto const result_buffer = result_len <= static_buffer.size() ? static_buffer.data() : new char[result_len]{};

	// we need 2 more digits to get the specified precision in percentage form
	auto end = d2fixed_short(x, spec.precision + 2, result_buffer, false);

	// the beginning of the number could change, e.g. 0.0012 would be transformed to 0.12 and not 000.12
	auto const begin = [&]() {
		auto const dot_it = end - spec.precision + 2 - 1;
		assert(*dot_it == '.');
		auto const tenth_it = dot_it + 1;
		auto const hundredth_it = dot_it + 2;

		*dot_it = *tenth_it;
		*tenth_it = *hundredth_it;
		*hundredth_it = '.';

		if (spec.precision == 0)
		{
			// remove trailing dot
			--end;
		}
		if (leading_digit_count == 1 && *(dot_it - 1) == '0')
		{
			return *dot_it == '0' ? dot_it + 1 : dot_it;
		}
		else
		{
			return *result_buffer == '-' ? result_buffer + 1 : result_buffer;
		}
	}();

	*end = '%';
	++end;
	assert(end <= result_buffer + result_len);
	auto const result_str = u8string_view(begin, end);
	auto const put_sign = *result_buffer == '-' ? (spec.sign = '-', true) : (!std::isnan(x) && (spec.sign == '+' || spec.sign == ' '));

	auto result = format_number(result_str, spec, put_sign);

	if (result_buffer != static_buffer.data())
	{
		delete[] result_buffer;
	}

	return result;
}

inline u8string format_float64_shortest(double x, format_spec spec)
{
	spec.zero_pad &= std::isfinite(x);
	array<char, 24> result_buffer = {};
	auto const end = d2s_short(x, result_buffer.data());
	auto const result_str = result_buffer[0] == '-' ? u8string_view(result_buffer.data() + 1, end) : u8string_view(result_buffer.data(), end);
	auto const put_sign = result_buffer[0] == '-' ? (spec.sign = '-', true) : (!std::isnan(x) && (spec.sign == '+' || spec.sign == ' '));
	return format_number(result_str, spec, put_sign);
}

inline u8string format_float32_shortest(float x, format_spec spec)
{
	spec.zero_pad &= std::isfinite(x);
	array<char, 15> result_buffer = {};
	auto const end = f2s_short(x, result_buffer.data());
	auto const result_str = result_buffer[0] == '-' ? u8string_view(result_buffer.data() + 1, end) : u8string_view(result_buffer.data(), end);
	auto const put_sign = result_buffer[0] == '-' ? (spec.sign = '-', true) : (!std::isnan(x) && (spec.sign == '+' || spec.sign == ' '));
	return format_number(result_str, spec, put_sign);
}

inline u8string float_to_string(double x, format_spec spec)
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

	switch (spec.type)
	{
	case 'f': case 'F':
		if (spec.precision == format_spec::precision_none)
		{
			spec.precision = 6;
		}
		return format_float64_fixed(x, spec);
	case 'e': case 'E':
		if (spec.precision == format_spec::precision_none)
		{
			spec.precision = 6;
		}
		return format_float64_exponential(x, spec);
	case 'g': case 'G':
		if (spec.precision == format_spec::precision_none)
		{
			spec.precision = 6;
		}
		return format_float64_generic(x, spec);
	case '%':
		if (spec.precision == format_spec::precision_none)
		{
			spec.precision = 2;
		}
		return format_float64_percentage(x, spec);
	default:
		if (spec.precision == format_spec::precision_none)
		{
			return format_float64_shortest(x, spec);
		}
		else
		{
			return format_float64_generic(x, spec);
		}
	}
}

inline u8string float_to_string(float x, format_spec spec)
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

	switch (spec.type)
	{
	case 'f': case 'F':
	case 'e': case 'E':
	case 'g': case 'G':
	case '%':
		return float_to_string(static_cast<double>(x), spec);
	default:
		if (spec.precision == format_spec::precision_none)
		{
			return format_float32_shortest(x, spec);
		}
		else
		{
			return float_to_string(static_cast<double>(x), spec);
		}
	}
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
struct formatter<char const *> : formatter<u8string_view>
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
	static u8string format(char val, [[maybe_unused]] u8string_view spec)
	{
		bz_assert(spec.size() == 0);
		return u8string(1, static_cast<u8char>(val));
	}
};

template<>
struct formatter<bool>
{
	static u8string format(bool val, [[maybe_unused]] u8string_view spec)
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

enum class float_parse_result : uint32_t
{
	SUCCESS         = 0u,
	INPUT_TOO_SHORT = 1u,
	INPUT_TOO_LONG  = 2u,
	MALFORMED_INPUT = 3u,
};

extern "C" [[nodiscard]] float_parse_result s2d_str(char const *begin, char const *end, double *result);
extern "C" [[nodiscard]] float_parse_result s2f_str(char const *begin, char const *end, float *result);

inline bz::optional<double> parse_double(u8string_view str) noexcept
{
	double result = 0.0;
	auto const parse_res = s2d_str(str.data(), str.data() + str.size(), &result);
	if (parse_res != float_parse_result::SUCCESS)
	{
		return {};
	}
	else
	{
		return result;
	}
}

inline bz::optional<float> parse_float(u8string_view str) noexcept
{
	float result = 0.0f;
	auto const parse_res = s2f_str(str.data(), str.data() + str.size(), &result);
	if (parse_res != float_parse_result::SUCCESS)
	{
		return {};
	}
	else
	{
		return result;
	}
}

bz_end_namespace

#endif // _bz_format_h__
