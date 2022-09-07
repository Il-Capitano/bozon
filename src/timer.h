#ifndef TIMER_H
#define TIMER_H

#include <chrono>
#include <bz/u8string.h>
#include <bz/vector.h>

#ifdef _WIN32
#include <windows.h>
#endif // _WIN32

#ifdef _WIN32
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

#else

struct timer : std::chrono::high_resolution_clock
{

#endif // _WIN32

	struct timing_section_t
	{
		bz::u8string name;
		time_point begin;
		time_point end;
	};

	bz::vector<timing_section_t> timing_sections;
	bool running = false;

	void start_section(bz::u8string name)
	{
		if (this->running)
		{
			bz_assert(this->timing_sections.not_empty());
			this->timing_sections.back().end = now();
		}

		this->running = true;
		auto &new_section = this->timing_sections.emplace_back();
		new_section.name = std::move(name);
		new_section.begin = now();
	}

	void end_section(void)
	{
		bz_assert(this->running);
		bz_assert(this->timing_sections.not_empty());
		this->timing_sections.back().end = now();
		this->running = false;
	}

	timing_section_t const &get_section(bz::u8string_view name) const
	{
		for (auto const &section : this->timing_sections)
		{
			if (name == section.name)
			{
				return section;
			}
		}
		bz_unreachable;
	}

	duration get_section_duration(bz::u8string_view name) const
	{
		auto const &section = this->get_section(name);
		return section.end - section.begin;
	}

	duration get_total_duration(void) const
	{
		auto result = duration();
		for (auto const &[name, begin, end] : this->timing_sections)
		{
			result += end - begin;
		}
		return result;
	}
};

#endif // TIMER_H
