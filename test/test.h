#ifndef TEST_H
#define TEST_H

#include <stdexcept>
#include <string>
#include <sstream>
#include <iostream>
#include <bz/iterator.h>
#include <bz/format.h>

#include "colors.h"

struct test_fail_exception : std::exception
{
	std::string _what;

	test_fail_exception(std::string what)
		: _what(std::move(what))
	{}

	virtual char const *what(void) const noexcept override
	{ return this->_what.c_str(); }
};

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

template<typename ...Ts>
inline std::string build_str(Ts &&...ts)
{
	std::ostringstream ss;
	((ss << std::forward<Ts>(ts)), ...);
	return ss.str();
}

#if __cpp_exceptions

#define assert_true(x)                                                             \
do { if (!(x)) {                                                                   \
    throw test_fail_exception(build_str(                                           \
        "assert_true failed at " __FILE__ ":", __LINE__, ":\nexpression: " #x "\n" \
    ));                                                                            \
} } while (false)

#define assert_false(x)                                                             \
do { if (!!(x)) {                                                                   \
    throw test_fail_exception(build_str(                                            \
        "assert_false failed at " __FILE__ ":", __LINE__, ":\nexpression: " #x "\n" \
    ));                                                                             \
} } while (false)

#define assert_eq(x, y)                                      \
do { if (!((x) == (y))) {                                    \
    throw test_fail_exception(build_str(                     \
        "assert_eq failed in " __FILE__ ":", __LINE__, ":\n" \
        "lhs: " #x " == ", (x), "\n"                         \
        "rhs: " #y " == ", (y), "\n"                         \
    ));                                                      \
} } while (false)

#define assert_neq(x, y)                                      \
do { if (!((x) != (y))) {                                     \
    throw test_fail_exception(build_str(                      \
        "assert_neq failed in " __FILE__ ":", __LINE__, ":\n" \
        "lhs: " #x " == ", (x), "\n"                          \
        "rhs: " #y " == ", (y), "\n"                          \
    ));                                                       \
} } while (false)

#define test_fn(fn)                                                 \
do {                                                                \
    bz::print("    {:.<60}", #fn);                                  \
    try                                                             \
    {                                                               \
        ++test_count;                                               \
        fn();                                                       \
        bz::print("{}OK{}\n", colors::bright_green, colors::clear); \
        ++passed_count;                                             \
    }                                                               \
    catch (test_fail_exception &e)                                  \
    {                                                               \
        bz::print("{}FAIL{}\n", colors::bright_red, colors::clear); \
        bz::print(e.what());                                        \
    }                                                               \
} while (false)

#else

#define assert_true(x)                                                               \
do { if (!(x)) {                                                                     \
    std::cerr << (build_str(                                                         \
        "\nassert_true failed at " __FILE__ ":", __LINE__, ":\nexpression: " #x "\n" \
    ));                                                                              \
    bz_unreachable;                                                                  \
} } while (false)

#define assert_false(x)                                                               \
do { if (!!(x)) {                                                                     \
    std::cerr << (build_str(                                                          \
        "\nassert_false failed at " __FILE__ ":", __LINE__, ":\nexpression: " #x "\n" \
    ));                                                                               \
    bz_unreachable;                                                                   \
} } while (false)

#define assert_eq(x, y)                                        \
do { if (!((x) == (y))) {                                      \
    std::cerr << (build_str(                                   \
        "\nassert_eq failed in " __FILE__ ":", __LINE__, ":\n" \
        "lhs: " #x " == ", (x), "\n"                           \
        "rhs: " #y " == ", (y), "\n"                           \
    ));                                                        \
    bz_unreachable;                                            \
} } while (false)

#define assert_neq(x, y)                                        \
do { if (!((x) != (y))) {                                       \
    std::cerr << (build_str(                                    \
        "\nassert_neq failed in " __FILE__ ":", __LINE__, ":\n" \
        "lhs: " #x " == ", (x), "\n"                            \
        "rhs: " #y " == ", (y), "\n"                            \
    ));                                                         \
    bz_unreachable;                                             \
} } while (false)

#define test_fn(fn)                                             \
do {                                                            \
    bz::print("    {:.<60}", #fn);                              \
	++test_count;                                               \
	fn();                                                       \
	bz::print("{}OK{}\n", colors::bright_green, colors::clear); \
	++passed_count;                                             \
} while (false)

#endif // exceptions


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
