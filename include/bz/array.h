#ifndef _bz_array_h__
#define _bz_array_h__

#include "core.h"

#include "meta.h"
#include "iterator.h"
#include "array_view.h"
#include "ranges.h"

bz_begin_namespace

template<typename T, std::size_t N>
struct array
{
	static_assert(N != 0, "size of array must be bigger than 0");
	static_assert(
		!meta::is_reference<T>,
		"value_type of array can't be a reference"
	);
	static_assert(
		!meta::is_void<T>,
		"value_type of array can't be void"
	);

	using value_type = T;
	using size_type = std::size_t;

	using       iterator = ::bz::random_access_iterator<      value_type>;
	using const_iterator = ::bz::random_access_iterator<const value_type>;

	using       reverse_iterator = ::bz::reverse_iterator<      iterator>;
	using const_reverse_iterator = ::bz::reverse_iterator<const_iterator>;

	T _arr[N];


	// ==== size queries ====
	static constexpr auto size(void) noexcept
	{ return N; }


	// ==== member access ====
	constexpr auto &operator [] (size_type n) noexcept
	{ return this->_arr[n]; }

	constexpr auto const &operator [] (size_type n) const noexcept
	{ return this->_arr[n]; }

	constexpr auto &front(void) noexcept
	{ return this->_arr[0]; }

	constexpr auto const &front(void) const noexcept
	{ return this->_arr[0]; }

	constexpr auto &back(void) noexcept
	{ return this->_arr[N - 1]; }

	constexpr auto const &back(void) const noexcept
	{ return this->_arr[N - 1]; }

	constexpr auto *data(void) noexcept
	{ return this->_arr; }

	constexpr auto const *data(void) const noexcept
	{ return this->_arr; }

	constexpr operator array_view<value_type> (void) noexcept
	{ return array_view<value_type>(this->_arr, this->_arr + N); }

	constexpr operator array_view<value_type const> (void) const noexcept
	{ return array_view<value_type const>(this->_arr, this->_arr + N); }


	// ==== iteration ====
	constexpr iterator begin(void) noexcept
	{ return this->_arr; }

	constexpr iterator end(void) noexcept
	{ return this->_arr + N; }

	constexpr const_iterator begin(void) const noexcept
	{ return this->_arr; }

	constexpr const_iterator end(void) const noexcept
	{ return this->_arr + N; }

	constexpr const_iterator cbegin(void) const noexcept
	{ return this->_arr; }

	constexpr const_iterator cend(void) const noexcept
	{ return this->_arr + N; }


	constexpr reverse_iterator rbegin(void) noexcept
	{ return this->_arr + N - 1; }

	constexpr reverse_iterator rend(void) noexcept
	{ return this->_arr - 1; }

	constexpr const_reverse_iterator rbegin(void) const noexcept
	{ return this->_arr + N - 1; }

	constexpr const_reverse_iterator rend(void) const noexcept
	{ return this->_arr - 1; }

	constexpr const_reverse_iterator crbegin(void) const noexcept
	{ return this->_arr + N - 1; }

	constexpr const_reverse_iterator crend(void) const noexcept
	{ return this->_arr - 1; }


	// the methods inside collection_base must be implemented seperately here
	// because if we derive from it array is no longer a POD and
	// aggregate initialization doesn't work
	template<typename Func>
	constexpr auto filter(Func &&func) const noexcept
	{ return static_cast<array_view<value_type const>>(*this).filter(std::forward<Func>(func)); }

	template<typename Func>
	constexpr auto transform(Func &&func) const noexcept
	{ return static_cast<array_view<value_type const>>(*this).transform(std::forward<Func>(func)); }

	template<auto MemberPtr>
	constexpr auto member(void) const noexcept
	{ return static_cast<array_view<value_type const>>(*this).template member<MemberPtr>(); }

	template<typename Func>
	constexpr bool is_any(Func &&func) const noexcept
	{ return static_cast<array_view<value_type const>>(*this).is_any(std::forward<Func>(func)); }

	template<typename Func>
	constexpr bool is_all(Func &&func) const noexcept
	{ return static_cast<array_view<value_type const>>(*this).is_all(std::forward<Func>(func)); }

	template<typename Val>
	constexpr bool contains(Val &&val) const noexcept
	{ return static_cast<array_view<value_type const>>(*this).contains(std::forward<Val>(val)); }

	template<typename Func>
	constexpr void for_each(Func &&func) const noexcept
	{ static_cast<array_view<value_type const>>(*this).for_each(std::forward<Func>(func)); }

	constexpr auto sum(void) const noexcept
	{ return static_cast<array_view<value_type const>>(*this).sum(); }

	template<typename Val>
	constexpr auto max(Val &&val) const noexcept
	{ return static_cast<array_view<value_type const>>(*this).max(std::forward<Val>(val)); }

	template<typename Val>
	constexpr auto min(Val &&val) const noexcept
	{ return static_cast<array_view<value_type const>>(*this).min(std::forward<Val>(val)); }

	constexpr auto sort(void) noexcept
	{ return static_cast<array_view<value_type>>(*this).sort(); }

	template<typename Cmp>
	constexpr auto sort(Cmp &&cmp) noexcept
	{ return static_cast<array_view<value_type>>(*this).sort(std::forward<Cmp>(cmp)); }
};

template<typename T, typename ...U>
array(T, U ...) -> array<T, 1 + sizeof... (U)>;

bz_end_namespace

#endif // _bz_array_h__
