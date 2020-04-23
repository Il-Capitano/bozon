#ifndef LEX_FILE_ITERATOR_H
#define LEX_FILE_ITERATOR_H

#include "core.h"
#include "ctx/error.h"

struct file_iterator
{
	ctx::char_pos it;
	bz::u8string_view file;
	size_t line = 1;

	file_iterator &operator ++ (void)
	{
		if (*it == '\n')
		{
			++this->line;
		}
		++this->it;
		return *this;
	}
};

#endif // LEX_FILE_ITERATOR_H
