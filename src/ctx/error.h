#ifndef CTX_ERROR_H
#define CTX_ERROR_H

#include "core.h"

namespace ctx
{

using char_pos  = bz::string_view::const_iterator;

struct note
{
	bz::string file;
	size_t line;
	size_t column;

	char_pos src_begin;
	char_pos src_pivot;
	char_pos src_end;

	bz::string message;
};

struct error
{
	bz::string file;
	size_t line;
	size_t column;

	char_pos src_begin;
	char_pos src_pivot;
	char_pos src_end;

	bz::string message;

	bz::vector<note> notes;
};

} // namespace ctx

#endif // CTX_ERROR_H
