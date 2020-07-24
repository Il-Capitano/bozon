#ifndef _bz_allocator_h__
#define _bz_allocator_h__

#include "core.h"

bz_begin_namespace

template<typename T>
using allocator = ::std::allocator<T>;

bz_end_namespace

#endif // _bz_allocator_h__
