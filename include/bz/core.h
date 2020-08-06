#ifndef _bz_core_h__
#define _bz_core_h__

#include <iostream>
#include <utility>
#include <cstddef>
#include <cstring>

#define bz_begin_namespace namespace bz {
#define bz_end_namespace }
#define bz_likely
#define bz_unlikely

#define bz_assert(expr, ...)                                                                                \
(!!(expr)                                                                                                   \
? (void)0                                                                                                   \
: (void)(                                                                                                   \
    _bz_assert_begin(#expr, __FILE__, __LINE__), _bz_assert_message(__VA_ARGS__), __builtin_trap(), exit(1) \
))

inline void _bz_assert_begin(const char *expr, const char *file, int line)
{
	std::cerr << "assertion failed at " << file << ':' << line << "\n"
		"    expression: " << expr << '\n';
}

inline void _bz_assert_message(char const *message)
{
	std::cerr << "    message: " << message << '\n';
}

inline void _bz_assert_message()
{}

#endif // _bz_core_h__
