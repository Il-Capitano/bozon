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
	static_assert(std::is_integral_v<Result>);
	Result result;
	auto const overflowed = __builtin_add_overflow(a, b, &result);
	return { result, overflowed };
}

template<typename Result = int64_t>
operation_result_t<Result> sub_overflow(int64_t a, int64_t b)
{
	static_assert(std::is_integral_v<Result>);
	Result result;
	auto const overflowed = __builtin_sub_overflow(a, b, &result);
	return { result, overflowed };
}

template<typename Result = int64_t>
operation_result_t<Result> mul_overflow(int64_t a, int64_t b)
{
	static_assert(std::is_integral_v<Result>);
	Result result;
	auto const overflowed = __builtin_mul_overflow(a, b, &result);
	return { result, overflowed };
}

template<typename Result = int64_t>
operation_result_t<Result> div_overflow(int64_t a, int64_t b)
{
	static_assert(std::is_integral_v<Result>);
	if (b == 0)
	{
		return { Result(0), true };
	}

	if constexpr (std::is_signed_v<Result>)
	{
		if (a == std::numeric_limits<int64_t>::min() && b == -1)
		{
			auto const result = std::numeric_limits<int64_t>::min();
			return { static_cast<Result>(result), true };
		}
		else
		{
			auto const result = a / b;
			return { static_cast<Result>(result), static_cast<Result>(result) != result };
		}
	}
	else
	{
		if (a == std::numeric_limits<int64_t>::min() && b == -1)
		{
			auto const result = static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + 1;
			return { static_cast<Result>(result), static_cast<Result>(result) != result };
		}
		else
		{
			auto const result = a / b;
			return { static_cast<Result>(result), result < 0 || static_cast<Result>(result) != static_cast<uint64_t>(result) };
		}
	}
}


template<typename Result = uint64_t>
operation_result_t<Result> add_overflow(uint64_t a, uint64_t b)
{
	static_assert(std::is_integral_v<Result>);
	Result result;
	auto const overflowed = __builtin_add_overflow(a, b, &result);
	return { result, overflowed };
}

template<typename Result = uint64_t>
operation_result_t<Result> sub_overflow(uint64_t a, uint64_t b)
{
	static_assert(std::is_integral_v<Result>);
	Result result;
	auto const overflowed = __builtin_sub_overflow(a, b, &result);
	return { result, overflowed };
}

template<typename Result = uint64_t>
operation_result_t<Result> mul_overflow(uint64_t a, uint64_t b)
{
	static_assert(std::is_integral_v<Result>);
	Result result;
	auto const overflowed = __builtin_mul_overflow(a, b, &result);
	return { result, overflowed };
}

template<typename Result = uint64_t>
operation_result_t<Result> div_overflow(uint64_t a, uint64_t b)
{
	static_assert(std::is_integral_v<Result>);
	if (b == 0)
	{
		return { Result(0), true };
	}

	auto const result = a / b;
	return { static_cast<Result>(result), result > static_cast<uint64_t>(std::numeric_limits<Result>::max()) };
}

#endif // OVERFLOW_OPERATIONS_H
