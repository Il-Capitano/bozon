#ifndef TEST_H
#define TEST_H

#include <stdexcept>
#include <string>
#include <sstream>
#include <iostream>
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
	return os << static_cast<void const *>(&*it);
}

inline std::ostream &operator << (std::ostream &os, std::nullptr_t)
{
	return os << "0x0";
}

template<typename ...Ts>
inline std::string build_str(Ts &&...ts)
{
	std::ostringstream ss;
	(ss << ... << std::forward<Ts>(ts));
	return ss.str();
}

#define assert_true(x)                                                             \
do { if (!(x)) {                                                                   \
    throw test_fail_exception(build_str(                                           \
        "assert_true failed in " __FILE__ ":", __LINE__, ":\nexpression: " #x "\n" \
    ));                                                                            \
} } while (false)

#define assert_false(x)                                                             \
do { if (!!(x)) {                                                                   \
    throw test_fail_exception(build_str(                                            \
        "assert_false failed in " __FILE__ ":", __LINE__, ":\nexpression: " #x "\n" \
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

inline void print_start_test(char const *fn_name)
{
	std::string fn_name_padded = fn_name;
	fn_name_padded.resize(60, '.');
	std::cout << "    " << fn_name_padded;
}

#define test_begin()     \
size_t test_count = 0;   \
size_t passed_count = 0; \
std::cout << "Running " << __FUNCTION__ << '\n'

#define test(fn)                                               \
do {                                                           \
    print_start_test(#fn);                                     \
    try                                                        \
    {                                                          \
        ++test_count;                                          \
        fn();                                                  \
        std::cout << colors::bright_green << "OK\n" << colors::clear; \
        ++passed_count;                                        \
    }                                                          \
    catch (test_fail_exception &e)                             \
    {                                                          \
        std::cout << colors::bright_red << "FAIL\n" << colors::clear; \
        std::cout << e.what() << '\n';                         \
    }                                                          \
} while (false)

#define test_end()                                                   \
std::cout << passed_count << '/' << test_count << " tests passed\n"; \
return test_result{ test_count, passed_count }

#endif // TEST_H
