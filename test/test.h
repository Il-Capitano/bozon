#ifndef TEST_H
#define TEST_H

#include <stdexcept>
#include <string>
#include <sstream>
#include <iostream>
#include <bz/format.h>

namespace colors
{
	static constexpr char const *clear = "\033[0m";

	static constexpr char const *black   = "\033[30m";
	static constexpr char const *red     = "\033[31m";
	static constexpr char const *green   = "\033[32m";
	static constexpr char const *yellow  = "\033[33m";
	static constexpr char const *blue    = "\033[34m";
	static constexpr char const *magenta = "\033[35m";
	static constexpr char const *cyan    = "\033[36m";
	static constexpr char const *white   = "\033[37m";
};

struct test_fail_exception : std::exception
{
	std::string _what;

	test_fail_exception(std::string what)
		: _what(std::move(what))
	{}

	virtual char const *what(void) const noexcept override
	{ return this->_what.c_str(); }
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
        std::cout << colors::green << "OK\n" << colors::clear; \
        ++passed_count;                                        \
    }                                                          \
    catch (test_fail_exception &e)                             \
    {                                                          \
        std::cout << colors::red << "FAIL\n" << colors::clear; \
        std::cout << e.what() << '\n';                         \
    }                                                          \
} while (false)

#define test_end() \
std::cout << passed_count << '/' << test_count << " tests passed\n";

#endif // TEST_H
