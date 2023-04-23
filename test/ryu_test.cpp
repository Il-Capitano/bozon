// Copyright 2018 Ulf Adams
//
// The contents of this file may be used under the terms of the Apache License,
// Version 2.0.
//
//    (See accompanying file LICENSE-Apache or copy at
//     http://www.apache.org/licenses/LICENSE-2.0)
//
// Alternatively, the contents of this file may be used under the terms of
// the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE-Boost or copy at
//     https://www.boost.org/LICENSE_1_0.txt)
//
// Unless required by applicable law or agreed to in writing, this software
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.

// The contents of this file are a copy and an adaptation of the tests found in
// https://github.com/ulfjack/ryu/tree/master/ryu/tests

#include "test.h"
#include <bz/format.h>

static double int64_to_double(uint64_t n)
{
	double d;
	std::memcpy(&d, &n, sizeof (double));
	return d;
}

static double ieee_parts_to_double(bool sign, uint32_t ieee_exponent, uint64_t ieee_mantissa)
{
	bz_assert(ieee_exponent <= 2047);
	bz_assert(ieee_mantissa <= ((uint64_t)1 << 53) - 1);
	return int64_to_double(((uint64_t)sign << 63) | ((uint64_t)ieee_exponent << 52) | ieee_mantissa);
}

static float int32_to_float(uint32_t n)
{
	float f;
	std::memcpy(&f, &n, sizeof (float));
	return f;
}

constexpr double f64_inf = std::numeric_limits<double>::infinity();
constexpr double f64_nan = std::numeric_limits<double>::quiet_NaN();

constexpr float f32_inf = std::numeric_limits<float>::infinity();
constexpr float f32_nan = std::numeric_limits<float>::quiet_NaN();

#define x(num, string)                  \
do {                                        \
    auto const res = bz::format("{}", num); \
    assert_eq(res, string);                 \
} while (false)

// kind of just a copy of the tests in ryu

static bz::optional<bz::u8string> f64_basic_test()
{
	x(0.0, "0.0");
	x(-0.0, "-0.0");
	x(1.0, "1.0");
	x(-1.0, "-1.0");
	x(f64_inf, "inf");
	x(-f64_inf, "-inf");
	x(f64_nan, "nan");
	x(-f64_nan, "nan");

	return {};
}

static bz::optional<bz::u8string> f64_switch_to_subnormal_test()
{
	x(2.2250738585072014e-308, "2.2250738585072014e-308");

	return {};
}

static bz::optional<bz::u8string> f64_min_and_max_test()
{
	x(int64_to_double(0x7fef'ffff'ffff'ffff), "1.7976931348623157e+308"); // max
	x(std::numeric_limits<double>::max(),     "1.7976931348623157e+308"); // max
	x(int64_to_double(0x0000'0000'0000'0001),    "5e-324"); // min
	x(std::numeric_limits<double>::denorm_min(), "5e-324"); // min

	return {};
}

static bz::optional<bz::u8string> f64_lots_of_trailing_zeros_test()
{
	x(2.98023223876953125e-8, "2.9802322387695312e-08");

	return {};
}

static bz::optional<bz::u8string> f64_regression_test()
{
	x(-2.109808898695963e16, "-2.109808898695963e+16");
	x(4.940656e-318, "4.940656e-318");
	x(1.18575755e-316, "1.18575755e-316");
	x(2.989102097996e-312, "2.989102097996e-312");
	x(9.0608011534336e15, "9.0608011534336e+15");
	x(4.708356024711512e18, "4.708356024711512e+18");
	x(9.409340012568248e18, "9.409340012568248e+18");
	x(1.2345678, "1.2345678");

	return {};
}

static bz::optional<bz::u8string> f64_looks_like_pow5_test()
{
	x(int64_to_double(0x4830F0CF064DD592), "5.764607523034235e+39");
	x(int64_to_double(0x4840F0CF064DD592), "1.152921504606847e+40");
	x(int64_to_double(0x4850F0CF064DD592), "2.305843009213694e+40");

	return {};
}

static bz::optional<bz::u8string> f64_output_length_test()
{
	x(1, "1"); // already tested in Basic
	x(1.2, "1.2");
	x(1.23, "1.23");
	x(1.234, "1.234");
	x(1.2345, "1.2345");
	x(1.23456, "1.23456");
	x(1.234567, "1.234567");
	x(1.2345678, "1.2345678"); // already tested in Regression
	x(1.23456789, "1.23456789");
	x(1.234567895, "1.234567895"); // 1.234567890 would be trimmed
	x(1.2345678901, "1.2345678901");
	x(1.23456789012, "1.23456789012");
	x(1.234567890123, "1.234567890123");
	x(1.2345678901234, "1.2345678901234");
	x(1.23456789012345, "1.23456789012345");
	x(1.234567890123456, "1.234567890123456");
	x(1.2345678901234567, "1.2345678901234567");

	// Test 32-bit chunking
	x(4.294967294, "4.294967294"); // 2^32 - 2
	x(4.294967295, "4.294967295"); // 2^32 - 1
	x(4.294967296, "4.294967296"); // 2^32
	x(4.294967297, "4.294967297"); // 2^32 + 1
	x(4.294967298, "4.294967298"); // 2^32 + 2

	return {};
}

static bz::optional<bz::u8string> f64_min_max_shift_test()
{
	const uint64_t maxMantissa = ((uint64_t)1 << 53) - 1;

	// 32-bit opt-size=0:  49 <= dist <= 50
	// 32-bit opt-size=1:  30 <= dist <= 50
	// 64-bit opt-size=0:  50 <= dist <= 50
	// 64-bit opt-size=1:  30 <= dist <= 50
	x(ieee_parts_to_double(false, 4, 0), "1.7800590868057611e-307");
	// 32-bit opt-size=0:  49 <= dist <= 49
	// 32-bit opt-size=1:  28 <= dist <= 49
	// 64-bit opt-size=0:  50 <= dist <= 50
	// 64-bit opt-size=1:  28 <= dist <= 50
	x(ieee_parts_to_double(false, 6, maxMantissa), "2.8480945388892175e-306");
	// 32-bit opt-size=0:  52 <= dist <= 53
	// 32-bit opt-size=1:   2 <= dist <= 53
	// 64-bit opt-size=0:  53 <= dist <= 53
	// 64-bit opt-size=1:   2 <= dist <= 53
	x(ieee_parts_to_double(false, 41, 0), "2.446494580089078e-296");
	// 32-bit opt-size=0:  52 <= dist <= 52
	// 32-bit opt-size=1:   2 <= dist <= 52
	// 64-bit opt-size=0:  53 <= dist <= 53
	// 64-bit opt-size=1:   2 <= dist <= 53
	x(ieee_parts_to_double(false, 40, maxMantissa), "4.8929891601781557e-296");

	// 32-bit opt-size=0:  57 <= dist <= 58
	// 32-bit opt-size=1:  57 <= dist <= 58
	// 64-bit opt-size=0:  58 <= dist <= 58
	// 64-bit opt-size=1:  58 <= dist <= 58
	x(ieee_parts_to_double(false, 1077, 0), "1.8014398509481984e+16");
	// 32-bit opt-size=0:  57 <= dist <= 57
	// 32-bit opt-size=1:  57 <= dist <= 57
	// 64-bit opt-size=0:  58 <= dist <= 58
	// 64-bit opt-size=1:  58 <= dist <= 58
	x(ieee_parts_to_double(false, 1076, maxMantissa), "3.6028797018963964e+16");
	// 32-bit opt-size=0:  51 <= dist <= 52
	// 32-bit opt-size=1:  51 <= dist <= 59
	// 64-bit opt-size=0:  52 <= dist <= 52
	// 64-bit opt-size=1:  52 <= dist <= 59
	x(ieee_parts_to_double(false, 307, 0), "2.900835519859558e-216");
	// 32-bit opt-size=0:  51 <= dist <= 51
	// 32-bit opt-size=1:  51 <= dist <= 59
	// 64-bit opt-size=0:  52 <= dist <= 52
	// 64-bit opt-size=1:  52 <= dist <= 59
	x(ieee_parts_to_double(false, 306, maxMantissa), "5.801671039719115e-216");

	// https://github.com/ulfjack/ryu/commit/19e44d16d80236f5de25800f56d82606d1be00b9#commitcomment-30146483
	// 32-bit opt-size=0:  49 <= dist <= 49
	// 32-bit opt-size=1:  44 <= dist <= 49
	// 64-bit opt-size=0:  50 <= dist <= 50
	// 64-bit opt-size=1:  44 <= dist <= 50
	x(ieee_parts_to_double(false, 934, 0x000FA7161A4D6E0Cu), "3.196104012172126e-27");

	return {};
}

static bz::optional<bz::u8string> f64_small_integers_test()
{
	x(9007199254740991.0, "9.007199254740991e+15"); // 2^53-1
	x(9007199254740992.0, "9.007199254740992e+15"); // 2^53

	x(1.0e+0, "1.0");
	x(1.2e+1, "12.0");
	x(1.23e+2, "123.0");
	x(1.234e+3, "1234.0");
	x(1.2345e+4, "12345.0");
	x(1.23456e+5, "123456.0");
	x(1.234567e+6, "1234567.0");
	x(1.2345678e+7, "12345678.0");
	x(1.23456789e+8, "123456789.0");
	x(1.23456789e+9, "1234567890.0");
	x(1.234567895e+9, "1234567895.0");
	x(1.2345678901e+10, "12345678901.0");
	x(1.23456789012e+11, "123456789012.0");
	x(1.234567890123e+12, "1234567890123.0");
	x(1.2345678901234e+13, "12345678901234.0");
	x(1.23456789012345e+14, "123456789012345.0");
	x(1.234567890123456e+15, "1.234567890123456e+15");

	// 10^i
	x(1.0e+0, "1.0");
	x(1.0e+1, "10.0");
	x(1.0e+2, "100.0");
	x(1.0e+3, "1000.0");
	x(1.0e+4, "10000.0");
	x(1.0e+5, "100000.0");
	x(1.0e+6, "1000000.0");
	x(1.0e+7, "10000000.0");
	x(1.0e+8, "100000000.0");
	x(1.0e+9, "1000000000.0");
	x(1.0e+10, "10000000000.0");
	x(1.0e+11, "100000000000.0");
	x(1.0e+12, "1000000000000.0");
	x(1.0e+13, "10000000000000.0");
	x(1.0e+14, "100000000000000.0");
	x(1.0e+15, "1e+15");

	// 10^15 + 10^i
	x(1.0e+15 + 1.0e+0, "1.000000000000001e+15");
	x(1.0e+15 + 1.0e+1, "1.00000000000001e+15");
	x(1.0e+15 + 1.0e+2, "1.0000000000001e+15");
	x(1.0e+15 + 1.0e+3, "1.000000000001e+15");
	x(1.0e+15 + 1.0e+4, "1.00000000001e+15");
	x(1.0e+15 + 1.0e+5, "1.0000000001e+15");
	x(1.0e+15 + 1.0e+6, "1.000000001e+15");
	x(1.0e+15 + 1.0e+7, "1.00000001e+15");
	x(1.0e+15 + 1.0e+8, "1.0000001e+15");
	x(1.0e+15 + 1.0e+9, "1.000001e+15");
	x(1.0e+15 + 1.0e+10, "1.00001e+15");
	x(1.0e+15 + 1.0e+11, "1.0001e+15");
	x(1.0e+15 + 1.0e+12, "1.001e+15");
	x(1.0e+15 + 1.0e+13, "1.01e+15");
	x(1.0e+15 + 1.0e+14, "1.1e+15");

	// Largest power of 2 <= 10^(i+1)
	x(8.0, "8.0");
	x(64.0, "64.0");
	x(512.0, "512.0");
	x(8192.0, "8192.0");
	x(65536.0, "65536.0");
	x(524288.0, "524288.0");
	x(8388608.0, "8388608.0");
	x(67108864.0, "67108864.0");
	x(536870912.0, "536870912.0");
	x(8589934592.0, "8589934592.0");
	x(68719476736.0, "68719476736.0");
	x(549755813888.0, "549755813888.0");
	x(8796093022208.0, "8796093022208.0");
	x(70368744177664.0, "70368744177664.0");
	x(562949953421312.0, "562949953421312.0");
	x(9007199254740992.0, "9.007199254740992e+15");

	// 1000 * (Largest power of 2 <= 10^(i+1))
	x(8.0e+3, "8000.0");
	x(64.0e+3, "64000.0");
	x(512.0e+3, "512000.0");
	x(8192.0e+3, "8192000.0");
	x(65536.0e+3, "65536000.0");
	x(524288.0e+3, "524288000.0");
	x(8388608.0e+3, "8388608000.0");
	x(67108864.0e+3, "67108864000.0");
	x(536870912.0e+3, "536870912000.0");
	x(8589934592.0e+3, "8589934592000.0");
	x(68719476736.0e+3, "68719476736000.0");
	x(549755813888.0e+3, "549755813888000.0");
	x(8796093022208.0e+3, "8.796093022208e+15");

	return {};
}

static bz::optional<bz::u8string> f32_basic_test()
{
	x(0.0f, "0.0");
	x(-0.0f, "-0.0");
	x(1.0f, "1.0");
	x(-1.0f, "-1.0");
	x(f32_inf, "inf");
	x(-f32_inf, "-inf");
	x(f32_nan, "nan");
	x(-f32_nan, "nan");

	return {};
}

static bz::optional<bz::u8string> f32_switch_to_subnormal_test()
{
	x(1.1754944e-38f, "1.1754944e-38");

	return {};
}

static bz::optional<bz::u8string> f32_min_and_max_test()
{
	x(int32_to_float(0x7f7f'ffff), "3.4028235e+38");
	x(int32_to_float(0x0000'0001), "1e-45");

	return {};
}

static bz::optional<bz::u8string> f32_boundary_round_even_test()
{
	x(3.355445e7f, "3.355445e+07");
	x(8.999999e9f, "9e+09");
	x(3.4366717e10f, "3.436672e+10");

	return {};
}

static bz::optional<bz::u8string> f32_exact_value_round_even_test()
{
	x(3.0540412e5f, "305404.12");
	x(8.0990312e3f, "8099.0312");

	return {};
}

static bz::optional<bz::u8string> f32_lots_of_trailing_zeros_test()
{
	// Pattern for the first test: 00111001100000000000000000000000
	x(2.4414062e-4f, "0.00024414062");
	x(2.4414062e-3f, "0.0024414062");
	x(4.3945312e-3f, "0.0043945312");
	x(6.3476562e-3f, "0.0063476562");

	return {};
}

static bz::optional<bz::u8string> f32_regression_test()
{
	x(4.7223665e21f, "4.7223665e+21");
	x(8388608.0f, "8388608.0");
	x(1.6777216e7f, "1.6777216e+07");
	x(3.3554436e7f, "3.3554436e+07");
	x(6.7131496e7f, "6.7131496e+07");
	x(1.9310392e-38f, "1.9310392e-38");
	x(-2.47e-43f, "-2.47e-43");
	x(1.993244e-38f, "1.993244e-38");
	x(4103.9003f, "4103.9004");
	x(5.3399997e9f, "5.3399997e+09");
	x(6.0898e-39f, "6.0898e-39");
	x(0.0010310042f, "0.0010310042");
	x(2.8823261e17f, "2.882326e+17");
	x(7.038531e-26f, "7.038531e-26");
	x(9.2234038e17f, "9.223404e+17");
	x(6.7108872e7f, "6.710887e+07");
	x(1.0e-44f, "1e-44");
	x(2.816025e14f, "2.816025e+14");
	x(9.223372e18f, "9.223372e+18");
	x(1.5846085e29f, "1.5846086e+29");
	x(1.1811161e19f, "1.1811161e+19");
	x(5.368709e18f, "5.368709e+18");
	x(4.6143165e18f, "4.6143166e+18");
	x(0.007812537f, "0.007812537");
	x(1.4e-45f, "1e-45");
	x(1.18697724e20f, "1.18697725e+20");
	x(1.00014165e-36f, "1.00014165e-36");
	x(200.0f, "200.0");
	x(3.3554432e7f, "3.3554432e+07");

	return {};
}

static bz::optional<bz::u8string> f32_looks_like_pow5_test()
{
	x(int32_to_float(0x5D15'02F9), "6.7108864e+17");
	x(int32_to_float(0x5D95'02F9), "1.3421773e+18");
	x(int32_to_float(0x5E15'02F9), "2.6843546e+18");

	return {};
}

static bz::optional<bz::u8string> f32_output_length_test()
{
	x(1.0f, "1.0"); // already tested in Basic
	x(1.2f, "1.2");
	x(1.23f, "1.23");
	x(1.234f, "1.234");
	x(1.2345f, "1.2345");
	x(1.23456f, "1.23456");
	x(1.234567f, "1.234567");
	x(1.2345678f, "1.2345678");
	x(1.23456735e-36f, "1.23456735e-36");

	return {};
}

#undef x

test_result ryu_test()
{
	test_begin();

	test_fn(f64_basic_test);
	test_fn(f64_switch_to_subnormal_test);
	test_fn(f64_min_and_max_test);
	test_fn(f64_lots_of_trailing_zeros_test);
	test_fn(f64_regression_test);
	test_fn(f64_looks_like_pow5_test);
	test_fn(f64_output_length_test);
	test_fn(f64_min_max_shift_test);
	test_fn(f64_small_integers_test);

	test_fn(f32_basic_test);
	test_fn(f32_switch_to_subnormal_test);
	test_fn(f32_min_and_max_test);
	test_fn(f32_boundary_round_even_test);
	test_fn(f32_exact_value_round_even_test);
	test_fn(f32_lots_of_trailing_zeros_test);
	test_fn(f32_regression_test);
	test_fn(f32_looks_like_pow5_test);
	test_fn(f32_output_length_test);

	test_end();
}
