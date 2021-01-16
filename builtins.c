#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct
{
	uint8_t const *begin;
	uint8_t const *end;
} str;

typedef struct
{
	str *begin;
	str *end;
} str_slice;

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

void __bozon_builtin_print_stdout(str s)
{
	fwrite(s.begin, 1, (size_t)(s.end - s.begin), stdout);
}

void __bozon_builtin_println_stdout(str s)
{
	size_t len = (size_t)(s.end - s.begin);
	uint8_t *buff = malloc(len + 1);
	memcpy(buff, s.begin, len);
	buff[len] = '\n';
	fwrite(buff, 1, len + 1, stdout);
	free(buff);
}

void __bozon_builtin_print_stderr(str s)
{
	fwrite(s.begin, 1, (size_t)(s.end - s.begin), stderr);
}

void __bozon_builtin_println_stderr(str s)
{
	size_t len = (size_t)(s.end - s.begin);
	uint8_t *buff = malloc(len + 1);
	memcpy(buff, s.begin, len);
	buff[len] = '\n';
	fwrite(buff, 1, len + 1, stderr);
	free(buff);
}

int32_t __bozon_main(str_slice args);

int main(int argc, char const **argv)
{
	str buffer[8] = {};
	str *args = argc <= 8 ? buffer : malloc(argc * sizeof (str));
	if (args == NULL)
	{
		return -1;
	}
	str_slice args_slice = { args, args + argc };

	for (int i = 0; i < argc; ++i)
	{
		args[i].begin = (uint8_t const *)argv[i];
		args[i].end   = (uint8_t const *)argv[i] + strlen(argv[i]);
	}
	int32_t const res = __bozon_main(args_slice);
	if (args != buffer)
	{
		free(args);
	}
	return res;
}
