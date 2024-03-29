#ifndef AST_ALLOCATOR_H
#define AST_ALLOCATOR_H

#include "core.h"

namespace ast
{

struct arena_allocator
{
	static void *sized_allocate(size_t size);

#ifndef BOZON_NO_ARENA
	static void sized_free(void *p, size_t size)
	{}

	static void unsized_free(void *p)
	{}
#else
	static void sized_free(void *p, size_t size);
	static void unsized_free(void *p);
#endif

	template<typename T, typename ...Ts>
	static T *allocate_and_construct(Ts &&...ts)
	{
		auto const p = sized_allocate(sizeof (T));
		return new(p) T(std::forward<Ts>(ts)...);
	}

	template<typename T>
	static void destruct_and_deallocate(T *p)
	{
		p->~T();
		sized_free(p, sizeof (T));
	}

	template<typename T>
	struct unique_ptr_deleter
	{
		void operator ()(T *p) const
		{ destruct_and_deallocate(p); }
	};

	static constexpr size_t min_alignment = __STDCPP_DEFAULT_NEW_ALIGNMENT__;

#ifdef BOZON_PROFILE_ALLOCATIONS

	static std::size_t get_allocation_count(void);
	static std::size_t get_deallocation_count(void);
	static std::size_t get_total_allocation_size(void);

#endif // BOZON_PROFILE_ALLOCATIONS
};

template<typename T>
using ast_unique_ptr = std::unique_ptr<T, arena_allocator::unique_ptr_deleter<T>>;

template<typename T, typename ...Ts>
ast_unique_ptr<T> make_ast_unique(Ts &&...ts)
{
	return ast_unique_ptr<T>(arena_allocator::allocate_and_construct<T>(std::forward<Ts>(ts)...));
}

template<typename T>
struct vector_arena_allocator
{
	using value_type = T;
	using size_type = size_t;
	using difference_type = std::ptrdiff_t;
	using propagate_on_container_move_assignment = std::true_type;

	T *allocate(size_type n)
	{ return reinterpret_cast<T *>(arena_allocator::sized_allocate(n * sizeof (T))); }

	void deallocate(T *p, size_type n)
	{ arena_allocator::sized_free(p, n * sizeof (T)); }
};

template<typename T>
using arena_vector = bz::vector<T, vector_arena_allocator<T>>;

} // namespace ast

#endif // AST_ALLOCATOR_H
