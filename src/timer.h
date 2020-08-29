#ifndef TIMER_H
#define TIMER_H

#include <chrono>

#ifdef _WIN32
#include <windows.h>

struct timer
{
	using rep        = int64_t;
	using period     = std::nano;
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
		return time_point(duration(t.QuadPart * (1'000'000'000 / frequency)));
	}
};

#else // windows ^^^ // linux vvv

using timer = std::chrono::high_resolution_clock;

#endif // linux

#endif // TIMER_H
