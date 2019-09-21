#ifndef MY_ASSERT_H
#define MY_ASSERT_H

#include <iostream>
#include <string>
#include <bz/core.h>

#ifdef assert
#undef assert
#endif

#define assert(...) bz_assert(__VA_ARGS__)

#endif // MY_ASSERT_H
