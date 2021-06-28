#include "crash_handling.h"
#include <csignal>
#include <cstdlib>
#include <bz/format.h>
#include "stacktrace.h"

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
