#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
	uint8_t const *begin;
	uint8_t const *end;
} str;

bool __bozon_builtin_str_eq(str lhs, str rhs)
{
	size_t const lhs_size = (size_t)(lhs.end - lhs.begin);
	size_t const rhs_size = (size_t)(rhs.end - rhs.begin);
	if (lhs_size != rhs_size)
	{
		return false;
	}
	return lhs.begin == rhs.begin || memcmp(lhs.begin, rhs.begin, lhs_size) == 0;
}

bool __bozon_builtin_str_neq(str lhs, str rhs)
{
	return !__bozon_builtin_str_eq(lhs, rhs);
}
