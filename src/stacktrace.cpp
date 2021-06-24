#include <bz/format.h>
#include <iostream>
#ifdef __linux__
#define BOOST_STACKTRACE_USE_ADDR2LINE
#else
// #define BOOST_STACKTRACE_USE_BACKTRACE
#endif // __linux__
#include <boost/stacktrace.hpp>

void print_stacktrace()
{
	std::cerr << boost::stacktrace::stacktrace() << std::endl;
}
