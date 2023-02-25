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

#ifndef NDEBUG
#define bz_assert(expr)                                                           \
(!!(expr)                                                                         \
? (void)0                                                                         \
: (void)(                                                                         \
    ::bz::_handle_assert_fail(#expr, __FILE__, __LINE__), __builtin_unreachable() \
))
#else
#define bz_assert(expr, ...)
#endif // NDEBUG

#ifndef NDEBUG
#define bz_unreachable \
(::bz::_handle_unreachable(__FILE__, __LINE__), __builtin_unreachable())
#else
#define bz_unreachable \
(__builtin_unreachable())
#endif // NDEBUG

bz_begin_namespace

using assert_fail_handler_t = void (*)(char const *expr, char const *file, int line);
using unreachable_handler_t = void (*)(char const *file, int line);

inline void _default_handle_assert_fail(char const *expr, char const *file, int line)
{
	std::cerr << "assertion failed at " << file << ':' << line << "\n"
		"    expression: " << expr << '\n';
}

inline void _default_handle_unreachable(char const *file, int line)
{
	std::cerr << "hit unreachable code at " << file << ':' << line << '\n';
}

inline assert_fail_handler_t _assert_fail_handler = nullptr;
inline unreachable_handler_t _unreachable_handler = nullptr;

[[noreturn]] inline void _handle_assert_fail(char const *expr, char const *file, int line)
{
	if (_assert_fail_handler != nullptr)
	{
		_assert_fail_handler(expr, file, line);
	}
	else
	{
		_default_handle_assert_fail(expr, file, line);
	}
	std::abort();
}

[[noreturn]] inline void _handle_unreachable(char const *file, int line)
{
	if (_unreachable_handler != nullptr)
	{
		_unreachable_handler(file, line);
	}
	else
	{
		_default_handle_unreachable(file, line);
	}
	std::abort();
}

inline void register_assert_fail_handler(assert_fail_handler_t handler)
{
	_assert_fail_handler = handler;
}

inline void register_unreachable_handler(unreachable_handler_t handler)
{
	_unreachable_handler = handler;
}

#if __cpp_exceptions

#define bz_try         try
#define bz_catch(x)    catch (x)
#define bz_catch_all   catch (...)
#define bz_throw(x)    throw x
#define bz_rethrow     throw
#define bz_noexcept(...) noexcept(__VA_ARGS__)

#else

#define bz_try         if constexpr (true)
#define bz_catch(x)    if constexpr (false)
#define bz_catch_all   if constexpr (false)
#define bz_throw(x)    bz_unreachable
#define bz_rethrow     bz_unreachable
#define bz_noexcept(...) noexcept(true)

#endif // __cpp_exceptions

bz_end_namespace

#endif // _bz_core_h__
