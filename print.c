#include <stdio.h>
#include <stdint.h>
#include <assert.h>

void print(uint8_t const *begin, uint8_t const *end)
{
	fwrite(begin, 1, end - begin, stdout);
}

void println(uint8_t const *begin, uint8_t const *end)
{
	fwrite(begin, 1, end - begin, stdout);
	fputc('\n', stdout);
}

void print_char(uint32_t c)
{
	uint8_t buffer[4];
	size_t buffer_size;
	if (c < (1u << 7))
	{
		buffer[0] = (uint8_t)c;
		buffer_size = 1;
	}
	else if (c < (1u << 11))
	{
		buffer[0] = 0b11000000 | (uint8_t)(c >> 6);
		buffer[1] = 0b10000000 | (uint8_t)((c >> 0) & 0b00111111);
		buffer_size = 2;
	}
	else if (c < (1u << 16))
	{
		buffer[0] = 0b11100000 | (uint8_t)(c >> 12);
		buffer[1] = 0b10000000 | (uint8_t)((c >> 6) & 0b00111111);
		buffer[2] = 0b10000000 | (uint8_t)((c >> 0) & 0b00111111);
		buffer_size = 3;
	}
	else if (c < (1u << 21))
	{
		buffer[0] = 0b11100000 | (uint8_t)(c >> 18);
		buffer[1] = 0b10000000 | (uint8_t)((c >> 12) & 0b00111111);
		buffer[2] = 0b10000000 | (uint8_t)((c >>  6) & 0b00111111);
		buffer[3] = 0b10000000 | (uint8_t)((c >>  0) & 0b00111111);
		buffer_size = 4;
	}
	else
	{
		assert(0 && "invalid character in print_char");
	}
	fwrite(buffer, 1, buffer_size, stdout);
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
	fprintf(stdout, "0x%llx\n", (uint64_t)p);
}

void print_float64(double x)
{
	fprintf(stdout, "%g\n", x);
}
