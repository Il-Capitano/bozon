#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <inttypes.h>

void print_float32(float x)
{
	fprintf(stdout, "%g", x);
}

void println_float32(float x)
{
	fprintf(stdout, "%g\n", x);
}

void print_float64(double x)
{
	fprintf(stdout, "%g", x);
}

void println_float64(double x)
{
	fprintf(stdout, "%g\n", x);
}
