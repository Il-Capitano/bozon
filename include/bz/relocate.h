#ifndef _bz_relocate_h__
#define _bz_relocate_h__

#include "core.h"

bz_begin_namespace

namespace internal
{

template<typename T>
concept relocatable_via_function = requires {
	{ &T::relocate } -> std::same_as<void(*)(T *, T *)>;
};

} // namespace internal

template<typename T>
[[gnu::always_inline]] inline void relocate(T *dest, T *source)
{
	if constexpr (internal::relocatable_via_function<T>)
	{
		T::relocate(dest, source);
	}
	else
	{
		new(dest) T(std::move(*source));
		source->~T();
	}
}

bz_end_namespace

#endif // _bz_relocate_h__
