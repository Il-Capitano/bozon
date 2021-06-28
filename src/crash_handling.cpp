#include "crash_handling.h"
#include <csignal>
#include <cstdlib>
#include <bz/format.h>

#ifdef __linux__

#include <iostream>
#define BOOST_STACKTRACE_USE_ADDR2LINE
#include <boost/stacktrace.hpp>

static void print_stacktrace(void)
{
	std::cerr << boost::stacktrace::stacktrace() << std::endl;
}

#else

#include <bz/format.h>
#include <dwarfstack.h>

static void stderr_print(
	uint64_t address,
	char const *filename,
	int line,
	[[maybe_unused]] char const *func_name,
	void *context,
	int column
)
{
	int *count = reinterpret_cast<int *>(context);

	auto const ptr = reinterpret_cast<void *>(address);
	switch (line)
	{
	case DWST_BASE_ADDR:
		bz::print(stderr, "base address: {} ({})\n", filename, ptr);
		break;

	case DWST_NOT_FOUND:
		bz::print(stderr, "    not found: {} ({})\n", filename, ptr);
		break;

	case DWST_NO_DBG_SYM:
	case DWST_NO_SRC_FILE:
		bz::print(stderr, "    #{:2}: {} ({})\n", *count, filename, ptr);
		*count += 1;
		break;

	default:
		if (column > 0)
		{
			bz::print("    #{:2}: {}:{}:{} ({})\n", *count, filename, line, column, ptr);
			*count += 1;
		}
		else
		{
			bz::print("    #{:2}: {}:{} ({})\n", *count, filename, line, ptr);
			*count += 1;
		}
		break;
	}
}

static void print_stacktrace(void)
{
	int count = 0;
	dwstOfLocation(&stderr_print, &count);
}

#endif // __linux__

static void handle_segv(int)
{
	bz::print(stderr, "Segmentation fault\n");
	print_stacktrace();
	std::_Exit(-1);
}

static void handle_int(int)
{
	bz::print(stderr, "Program interrupted\n");
	std::_Exit(-1);
}

static void handle_ill(int)
{
	bz::print(stderr, "Invalid instruction\n");
	print_stacktrace();
	std::_Exit(-1);
}

void register_crash_handlers(void)
{
	std::signal(SIGSEGV, &handle_segv);
	std::signal(SIGINT,  &handle_int);
	std::signal(SIGILL,  &handle_ill);
}
