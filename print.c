#include <stdio.h>
#include <stdint.h>

void print(uint8_t const *begin, uint8_t const *end)
{
	fwrite(begin, 1, end - begin, stdout);
}

void println(uint8_t const *begin, uint8_t const *end)
{
	fwrite(begin, 1, end - begin, stdout);
	fputc('\n', stdout);
}

void print_int32(int32_t n)
{
	fprintf(stdout, "%i\n", n);
}
