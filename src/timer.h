#ifndef TIMER_H
#define TIMER_H

#include <chrono>
#include <windows.h>

struct timer
{
	using rep        = double;
	using period     = std::ratio<1>;
	using duration   = std::chrono::duration<rep, period>;
	using time_point = std::chrono::time_point<timer>;
	static constexpr bool is_steady = false;

	static inline long long frequency = []()
	{
		LARGE_INTEGER freq;
		QueryPerformanceFrequency(&freq);
		return freq.QuadPart;
	}();

	static time_point now(void)
	{
		LARGE_INTEGER t;
		QueryPerformanceCounter(&t);
		return time_point(duration(static_cast<double>(t.QuadPart) / frequency));
	}
};

#endif // TIMER_H
