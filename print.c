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

void print_uint32(uint32_t n)
{
	fprintf(stdout, "%u\n", n);
}

void print_int32_ptr(int32_t const *p)
{
	fprintf(stdout, "0x%u\n", (uint32_t)(uint64_t)p);
}

void print_float64(double x)
{
	fprintf(stdout, "%g\n", x);
}
