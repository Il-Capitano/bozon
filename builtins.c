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


int32_t __bozon_main(str_slice args);

int main(int argc, char const *const *argv)
{
	str buffer[8] = {};
	str *args = argc <= 8 ? buffer : malloc(argc * sizeof *args);
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
