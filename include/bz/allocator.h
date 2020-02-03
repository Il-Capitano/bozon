#ifndef _bz_allocator_h__
#define _bz_allocator_h__

#include "core.h"

bz_begin_namespace

template<typename T>
struct allocator : public ::std::allocator<T>
{
	using base_t = ::std::allocator<T>;
	using base_t::allocator;
};

bz_end_namespace

#endif // _bz_allocator_h__
