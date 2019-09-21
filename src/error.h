#include <bz/string.h>
#include <bz/format.h>

template<typename ...Args>
void fatal_error(bz::string_view fmt, Args const &...args)
{
	bz::printf(std::cerr, fmt, args...);
	exit(1);
}
