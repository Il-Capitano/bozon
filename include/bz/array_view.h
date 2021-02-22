#ifndef _bz_array_view_h__
#define _bz_array_view_h__

#include "core.h"
#include "iterator.h"
#include "meta.h"
#include "ranges.h"

bz_begin_namespace

template<typename T>
class array_view : public collection_base<array_view<T>>
{
private:
	using self_t = array_view<T>;

public:
	using value_type             = T;
	using size_type              = size_t;
	using iterator               = random_access_iterator<T>;
	using const_iterator         = random_access_iterator<const T>;
	using reverse_iterator       = ::bz::reverse_iterator<iterator>;
	using const_reverse_iterator = ::bz::reverse_iterator<const_iterator>;

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

	constexpr value_type &front(void) const noexcept
	{ bz_assert(this->size() != 0); return *this->_data_begin; }

	constexpr value_type &back(void) const noexcept
	{ bz_assert(this->size() != 0); return *(this->_data_end - 1); }

	constexpr bool empty(void) const noexcept
	{ return this->_data_begin == this->_data_end; }


	constexpr iterator begin(void) const noexcept
	{ return iterator(this->_data_begin); }

	constexpr iterator end(void) const noexcept
	{ return iterator(this->_data_end); }

	constexpr const_iterator cbegin(void) const noexcept
	{ return const_iterator(this->_data_begin); }

	constexpr const_iterator cend(void) const noexcept
	{ return const_iterator(this->_data_end); }

	constexpr reverse_iterator rbegin(void) const noexcept
	{ return reverse_iterator(this->_data_end - 1); }

	constexpr reverse_iterator rend(void) const noexcept
	{ return reverse_iterator(this->_data_begin - 1); }

	constexpr const_reverse_iterator crbegin(void) const noexcept
	{ return const_reverse_iterator(this->_data_end - 1); }

	constexpr const_reverse_iterator crend(void) const noexcept
	{ return const_reverse_iterator(this->_data_begin - 1); }

	constexpr operator array_view<value_type const> (void) const noexcept
	{ return array_view<value_type const>{ this->_data_begin, this->_data_end }; }
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

template<typename T, typename U>
constexpr bool operator == (array_view<T> lhs, array_view<U> rhs)
{ return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin()); }

template<typename T, typename U>
constexpr bool operator != (array_view<T> lhs, array_view<U> rhs)
{ return !(lhs == rhs); }

bz_end_namespace

#endif // _bz_array_view_h__
