#include "crash_handling.h"
#include <csignal>
#include <cstdlib>
#include <bz/format.h>
#include "colors.h"

#ifdef __linux__

#include <bz/format.h>
#include <backtrace.h>

static void error_callback([[maybe_unused]] void *data, char const *msg, int errnum)
{
	bz::print(stderr, "error while printing stack trace: {} (error {})\n", msg, errnum);
}

static int full_callback(void *data, uintptr_t pc, char const *filename, int line, char const *func_name)
{
	auto &count = *reinterpret_cast<int *>(data);

	auto const ptr = reinterpret_cast<void *>(pc);
	if (line == 0)
	{
		// bz::print(stderr, "    not found: {} ({})\n", filename, ptr);
	}
	else
	{
		bz::print(stderr, "    #{:2}: {}:{} ({})\n", count, filename, line, ptr);
		count += 1;
	}
	return 0;
}

static void print_stacktrace(void)
{
	static auto const backtrace = backtrace_create_state(nullptr, false, &error_callback, nullptr);

	int count = 0;
	backtrace_full(backtrace, 2, &full_callback, &error_callback, &count);
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
	auto &count = *reinterpret_cast<int *>(context);
	if (count < 0)
	{
		count += 1;
		return;
	}

	auto const ptr = reinterpret_cast<void *>(address);
	switch (line)
	{
	case DWST_BASE_ADDR:
		break;

	case DWST_NOT_FOUND:
		// bz::print(stderr, "    not found: {} ({})\n", filename, ptr);
		break;

	case DWST_NO_DBG_SYM:
	case DWST_NO_SRC_FILE:
		// bz::print(stderr, "    #{:2}: {} ({})\n", count, filename, ptr);
		// count += 1;
		break;

	default:
		if (column > 0)
		{
			bz::print(stderr, "    #{:2}: {}:{}:{} ({})\n", count, filename, line, column, ptr);
			count += 1;
		}
		else
		{
			bz::print(stderr, "    #{:2}: {}:{} ({})\n", count, filename, line, ptr);
			count += 1;
		}
		break;
	}
}

static void print_stacktrace(void)
{
	int count = -3;
	dwstOfLocation(&stderr_print, &count);
}

#endif // __linux__

static void print_internal_compiler_error_message(bz::u8string_view msg)
{
	bz::print(
		stderr, "{}bozon:{} {}internal compiler error:{} {}\n",
		colors::bright_white, colors::clear, colors::bright_red, colors::clear,
		msg
	);
}

static void handle_segv(int)
{
	print_internal_compiler_error_message("segmentation fault");
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
	print_internal_compiler_error_message("invalid instruction");
	print_stacktrace();
	std::_Exit(-1);
}

void register_crash_handlers(void)
{
	std::signal(SIGSEGV, &handle_segv);
	std::signal(SIGINT,  &handle_int);
	std::signal(SIGILL,  &handle_ill);
}
