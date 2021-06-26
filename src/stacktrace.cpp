#include "stacktrace.h"

#ifdef __linux__

#include <iostream>
#define BOOST_STACKTRACE_USE_ADDR2LINE
#include <boost/stacktrace.hpp>

void print_stacktrace()
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

void print_stacktrace()
{
	int count = 0;
	dwstOfLocation(&stderr_print, &count);
}

#endif // __linux__
