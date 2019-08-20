#include "variant.h"

// this file is to test the vairant header

using namespace meta;

static_assert(is_type_in_rest_v<int, double, float, char, int>);
static_assert(!is_type_in_rest_v<int, double, float, char, long long>);
static_assert(is_same_v<int, int, int, int>);
static_assert(!is_same_v<int, int, int, double, int>);
static_assert(is_same_v<int, int>);
static_assert(!is_same_v<int, double>);
static_assert(is_any_same_v<int, double, float, int, long long>);
static_assert(!is_any_same_v<int, double, float, long long>);
static_assert(is_same_v<nth_type_t<3, int, int, double, float, long long>, float>);
//static_assert(is_same_v<nth_type_t<2, int, int>, int>);
static_assert(index_of_type_v<int, double, int, float> == 1);
//static_assert(index_of_type_v<int, int, int> == 0);
static_assert(true);
static_assert(true);
static_assert(true);
static_assert(true);
static_assert(true);
static_assert(true);
static_assert(true);
static_assert(true);
static_assert(true);
static_assert(true);
