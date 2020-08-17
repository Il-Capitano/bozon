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

#define bz_assert(expr, ...)                                                                         \
(!!(expr)                                                                                            \
? (void)0                                                                                            \
: (void)(                                                                                            \
    bz::_assert_begin(#expr, __FILE__, __LINE__), bz::_assert_message(__VA_ARGS__), __builtin_trap() \
))

#define bz_unreachable \
(bz::_unreachable_message(__FILE__, __LINE__), __builtin_trap())

bz_begin_namespace

inline void _assert_begin(const char *expr, const char *file, int line)
{
	std::cerr << "assertion failed at " << file << ':' << line << "\n"
		"    expression: " << expr << '\n';
}

inline void _assert_message(char const *message)
{
	std::cerr << "    message: " << message << '\n';
}

inline void _assert_message()
{}

inline void _unreachable_message(const char *file, int line)
{
	std::cerr << "hit unreachable code at " << file << ':' << line << "\n";
}

bz_end_namespace

#endif // _bz_core_h__
