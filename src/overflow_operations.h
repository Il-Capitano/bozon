#ifndef OVERFLOW_OPERATIONS_H
#define OVERFLOW_OPERATIONS_H

#include "core.h"

template<typename Int>
struct operation_result_t
{
	Int result;
	bool overflowed;
};

template<typename Result = int64_t>
operation_result_t<Result> add_overflow(int64_t a, int64_t b)
{
	Result result;
	auto const overflowed = __builtin_add_overflow(a, b, &result);
	return { result, overflowed };
}

template<typename Result = int64_t>
operation_result_t<Result> sub_overflow(int64_t a, int64_t b)
{
	Result result;
	auto const overflowed = __builtin_sub_overflow(a, b, &result);
	return { result, overflowed };
}

template<typename Result = int64_t>
operation_result_t<Result> mul_overflow(int64_t a, int64_t b)
{
	Result result;
	auto const overflowed = __builtin_mul_overflow(a, b, &result);
	return { result, overflowed };
}

template<typename Result = uint64_t>
operation_result_t<Result> add_overflow(uint64_t a, uint64_t b)
{
	Result result;
	auto const overflowed = __builtin_add_overflow(a, b, &result);
	return { result, overflowed };
}

template<typename Result = uint64_t>
operation_result_t<Result> sub_overflow(uint64_t a, uint64_t b)
{
	Result result;
	auto const overflowed = __builtin_sub_overflow(a, b, &result);
	return { result, overflowed };
}

template<typename Result = uint64_t>
operation_result_t<Result> mul_overflow(uint64_t a, uint64_t b)
{
	Result result;
	auto const overflowed = __builtin_mul_overflow(a, b, &result);
	return { result, overflowed };
}

#endif // OVERFLOW_OPERATIONS_H
