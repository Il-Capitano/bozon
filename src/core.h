#ifndef CORE_H
#define CORE_H

#include <iostream>
#include <fstream>
#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <map>
#include <memory>
#include <utility>
#include <algorithm>

#include "intern_string.h"

#ifdef assert
#error "don't include <cassert>!"
#endif

#define assert(b, ...)  \
	(!!(b)              \
		? (void)0       \
		: (void)(_debug_assert_begin(__VA_ARGS__), _debug_assert_end(#b, __FILE__, __LINE__)))

inline void _debug_assert_begin(void)
{
	std::cerr << "Assertion failed:\n";
}

inline void _debug_assert_begin(const char *message)
{
	std::cerr << "Assertion failed:\n"
		"    Message: '" << message << "'\n";
}

inline void _debug_assert_begin(std::string const &message)
{
	std::cerr << "Assertion failed:\n"
		"    Message: " << message << '\n';
}

inline void _debug_assert_end(const char *expr, const char *file, int line)
{
	std::cerr <<
		"    Expression: '" << expr << "'\n"
		"    File: " << file << "\n"
		"    Line: " << line << '\n';
	exit(1);
}

#include "variant.h"

#endif // CORE_H
