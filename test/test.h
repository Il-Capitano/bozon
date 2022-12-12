#ifndef TEST_H
#define TEST_H

#include <stdexcept>
#include <string>
#include <sstream>
#include <iostream>
#include <bz/iterator.h>
#include <bz/format.h>

#include "colors.h"
#include "ast/constant_value.h"
#include "ast/statement.h"

struct test_result
{
	size_t test_count;
	size_t passed_count;
};

template<typename T>
inline std::ostream &operator << (std::ostream &os, bz::random_access_iterator<T> it)
{
	os << static_cast<void const *>(it.data());
	return os;
}

inline std::ostream &operator << (std::ostream &os, bz::u8iterator it)
{
	os << static_cast<void const *>(it.data());
	return os;
}

inline std::ostream &operator << (std::ostream &os, ast::constant_value const &value)
{
	switch (value.kind())
	{
	case ast::constant_value::sint:
		os << "sint: " << value.get_sint();
		break;
	case ast::constant_value::uint:
		os << "uint: " << value.get_uint();
		break;
	case ast::constant_value::float32:
		os << "float32: " << value.get_float32();
		break;
	case ast::constant_value::float64:
		os << "float64: " << value.get_float64();
		break;
	case ast::constant_value::u8char:
		os << "u8char: " << value.get_u8char();
		break;
	case ast::constant_value::string:
		os << "string: " << value.get_string();
		break;
	case ast::constant_value::boolean:
		os << "boolean: " << value.get_boolean();
		break;
	case ast::constant_value::null:
		os << "null: []";
		break;
	case ast::constant_value::void_:
		os << "void: []";
		break;
	case ast::constant_value::array:
		os << "array: [...]";
		break;
	case ast::constant_value::tuple:
		os << "tuple: [...]";
		break;
	case ast::constant_value::function:
		os << "function: " << value.get_function()->body.get_signature();
		break;
	case ast::constant_value::type:
		os << "type: " << bz::format("{}", value.get_type());
		break;
	case ast::constant_value::aggregate:
		os << "aggreate: [...]";
		break;
	default:
		bz_unreachable;
	}

	return os;
}

template<typename ...Ts>
inline bz::u8string build_str(Ts &&...ts)
{
	std::ostringstream ss;
	((ss << std::forward<Ts>(ts)), ...);
	return ss.str().c_str();
}


#define assert_true(x)                                                                               \
do {                                                                                                 \
    if (!(x))                                                                                        \
    {                                                                                                \
        return build_str("assert_true failed at " __FILE__ ":", __LINE__, "\nexpression: " #x "\n"); \
    }                                                                                                \
} while (false)

#define assert_false(x)                                                                               \
do {                                                                                                  \
    if (!!(x))                                                                                        \
    {                                                                                                 \
        return build_str("assert_false failed at " __FILE__ ":", __LINE__, "\nexpression: " #x "\n"); \
    }                                                                                                 \
} while (false)

#define assert_eq(x, y)                                         \
do {                                                            \
    if (!((x) == (y)))                                          \
    {                                                           \
        return build_str(                                       \
            "assert_eq failed at " __FILE__ ":", __LINE__, "\n" \
            "lhs: " #x " == ", (x), "\n"                        \
            "rhs: " #y " == ", (y), "\n"                        \
        );                                                      \
    }                                                           \
} while (false)

#define assert_neq(x, y)                                         \
do {                                                             \
    if (!((x) != (y)))                                           \
    {                                                            \
        return build_str(                                        \
            "assert_neq failed at " __FILE__ ":", __LINE__, "\n" \
            "lhs: " #x " == ", (x), "\n"                         \
            "rhs: " #y " == ", (y), "\n"                         \
        );                                                       \
    }                                                            \
} while (false)

#define test_fn(fn, ...)                                            \
do {                                                                \
    bz::print("    {:.<60}", #fn);                                  \
    ++test_count;                                                   \
    auto const result = fn(__VA_ARGS__);                            \
    if (result.has_value())                                         \
    {                                                               \
        bz::print("{}FAIL{}\n", colors::bright_red, colors::clear); \
        bz::print("{}", *result);                                   \
    }                                                               \
    else                                                            \
    {                                                               \
        bz::print("{}OK{}\n", colors::bright_green, colors::clear); \
        ++passed_count;                                             \
    }                                                               \
} while (false)


#define test_begin()                    \
size_t test_count = 0;                  \
size_t passed_count = 0;                \
bz::print("Running {}\n", __FUNCTION__)

#define test_end()                                                          \
bz::print(                                                                  \
    "{}{}/{}{} ({}{:.2f}%{}) tests passed\n",                               \
    passed_count == test_count ? colors::bright_green : colors::bright_red, \
    passed_count, test_count,                                               \
    colors::clear,                                                          \
    passed_count == test_count ? colors::bright_green : colors::bright_red, \
    100 * static_cast<double>(passed_count) / test_count,                   \
    colors::clear                                                           \
);                                                                          \
return test_result{ test_count, passed_count }

#endif // TEST_H
