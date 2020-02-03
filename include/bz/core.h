#ifndef _bz_core_h__
#define _bz_core_h__

#include <iostream>
#include <utility>

#define bz_begin_namespace namespace bz {
#define bz_end_namespace }

#define bz_assert(expr, ...)                                                              \
(!!(expr)                                                                                 \
? (void)0                                                                                 \
: (void)(                                                                                 \
	_bz_assert_begin(#expr, __FILE__, __LINE__), _bz_assert_message(__VA_ARGS__), exit(1) \
))

inline void _bz_assert_begin(const char *expr, const char *file, int line)
{
	std::cerr << "Assertion failed:\n"
		"	File: " << file << "\n"
		"	Line: " << line << "\n"
		"	Expression: " << expr << '\n';
}

inline void _bz_assert_message(std::string_view message)
{
	std::cerr << "	Message: " << message << '\n';
}

inline void _bz_assert_message()
{}

#endif // _bz_core_h__
