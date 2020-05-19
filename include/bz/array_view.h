#ifndef _bz_array_view_h__
#define _bz_array_view_h__

#include "core.h"
#include "iterator.h"
#include "meta.h"

bz_begin_namespace

template<typename T>
class array_view
{
private:
	using self_t = array_view<T>;

public:
	using value_type = T;
	using size_type = size_t;
	using iterator = random_access_iterator<T>;
	using const_iterator = random_access_iterator<const T>;

private:
	value_type *_data_begin;
	value_type *_data_end;

public:
	constexpr array_view(void) noexcept
		: _data_begin(nullptr), _data_end(nullptr)
	{}

	constexpr array_view(self_t const &) noexcept = default;
	constexpr array_view(self_t &&) noexcept      = default;
	constexpr self_t &operator = (self_t const &) noexcept = default;
	constexpr self_t &operator = (self_t &&) noexcept      = default;

	constexpr array_view(value_type *data_begin, size_type size) noexcept
		: _data_begin(data_begin), _data_end(data_begin + size)
	{}

	constexpr array_view(value_type *data_begin, value_type *data_end) noexcept
		: _data_begin(data_begin), _data_end(data_end)
	{}

	constexpr array_view(iterator begin, size_type size) noexcept
		: _data_begin(begin.data()), _data_end(begin.data() + size)
	{}

	constexpr array_view(iterator begin, iterator end) noexcept
		: _data_begin(begin.data()), _data_end(end.data())
	{}

	template<size_t N>
	constexpr array_view(T (&c_array)[N]) noexcept
		: _data_begin(c_array), _data_end(c_array + N)
	{}


	value_type &operator [] (size_type index) const noexcept
	{ return this->_data_begin[index]; }


	constexpr size_type size(void) const noexcept
	{ return static_cast<size_type>(this->_data_end - this->_data_begin); }

	constexpr value_type *data(void) const noexcept
	{ return this->_data_begin; }


	constexpr iterator begin(void) const noexcept
	{ return iterator(this->_data_begin); }

	constexpr iterator end(void) const noexcept
	{ return iterator(this->_data_end); }

	constexpr const_iterator cbegin(void) const noexcept
	{ return const_iterator(this->_data_begin); }

	constexpr const_iterator cend(void) const noexcept
	{ return const_iterator(this->_data_end); }
};

template<typename T>
array_view(T *, T *) -> array_view<T>;

template<typename T>
array_view(T const *, T const *) -> array_view<T const>;

template<typename T>
array_view(T *, size_t) -> array_view<T>;

template<typename T>
array_view(T const *, size_t) -> array_view<T const>;

template<typename T>
array_view(random_access_iterator<T>, random_access_iterator<T>) -> array_view<T>;

template<typename T>
array_view(random_access_iterator<T const>, random_access_iterator<T const>) -> array_view<T const>;

template<typename T>
array_view(random_access_iterator<T>, size_t) -> array_view<T>;

template<typename T>
array_view(random_access_iterator<T const>, size_t) -> array_view<T const>;

bz_end_namespace

#endif // _bz_array_view_h__
