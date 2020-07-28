#ifndef LEX_FILE_ITERATOR_H
#define LEX_FILE_ITERATOR_H

#include "core.h"
#include "ctx/error.h"

struct file_iterator
{
	ctx::char_pos it;
	uint32_t file_id;
	uint32_t line = 1;

	file_iterator &operator ++ (void)
	{
		if (*it == '\n')
		{
			bz_assert(this->line != std::numeric_limits<uint32_t>::max());
			++this->line;
		}
		++this->it;
		return *this;
	}
};

#endif // LEX_FILE_ITERATOR_H
