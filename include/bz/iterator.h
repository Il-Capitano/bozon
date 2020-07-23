#ifndef _bz_iterator_h__
#define _bz_iterator_h__

#include "core.h"
#include "meta.h"
#include <tuple>

bz_begin_namespace

template<typename T>
struct random_access_iterator
{
private:
	using self_t = random_access_iterator<T>;

public:
	using iterator_category = std::random_access_iterator_tag;
	using value_type        = T;
	using difference_type   = decltype(std::declval<T *>() - std::declval<T *>());
	using pointer           = T *;
	using const_pointer     = T const *;
	using reference         = T &;
	using const_reference   = T const &;

private:
	value_type *_data;

public:
	constexpr random_access_iterator(value_type *data) noexcept
		: _data(data)
	{}

	constexpr pointer data(void) const noexcept
	{ return this->_data; }

	constexpr operator random_access_iterator<const value_type> (void) const noexcept
	{ return { this->_data }; }

	constexpr self_t &operator ++ (void) noexcept
	{ ++this->_data; return *this; }

	constexpr self_t &operator -- (void) noexcept
	{ --this->_data; return *this; }


	constexpr reference operator * (void) const noexcept
	{ return *this->_data; }

	constexpr pointer operator -> (void) const noexcept
	{ return this->_data; }


	constexpr self_t &operator += (size_t n) noexcept
	{ this->_data += n; return *this; }

	constexpr self_t &operator -= (size_t n) noexcept
	{ this->_data -= n; return *this; }


	constexpr friend self_t operator + (self_t lhs, size_t rhs) noexcept
	{ return lhs._data + rhs; }

	constexpr friend self_t operator + (size_t lhs, self_t rhs) noexcept
	{ return lhs + rhs._data; }

	constexpr friend self_t operator - (self_t lhs, size_t rhs) noexcept
	{ return lhs._data - rhs; }


#define cmp_op_def(op)                                              \
constexpr friend bool operator op (self_t lhs, self_t rhs) noexcept \
{ return lhs._data op rhs._data; }

	cmp_op_def(==)
	cmp_op_def(!=)
	cmp_op_def(<)
	cmp_op_def(<=)
	cmp_op_def(>)
	cmp_op_def(>=)

#undef cmp_op_def


	constexpr friend difference_type operator - (self_t lhs, self_t rhs) noexcept
	{ return lhs._data - rhs._data; }
};


template<typename T>
struct forward_iterator;

template<typename T>
struct reverse_iterator;


template<typename T>
struct reverse_iterator<random_access_iterator<T>>
{
private:
	using self_t = reverse_iterator<random_access_iterator<T>>;

public:
	using iterator_category = std::random_access_iterator_tag;
	using value_type        = T;
	using difference_type   = decltype(std::declval<T *>() - std::declval<T *>());
	using pointer           = T *;
	using const_pointer     = T const *;
	using reference         = T &;
	using const_reference   = T const &;

private:
	value_type *_data;

public:
	constexpr reverse_iterator(value_type *ptr) noexcept
		: _data(ptr)
	{}

	constexpr operator reverse_iterator<
		random_access_iterator<const value_type>
	> (void) const noexcept
	{ return { this->_data }; }

	constexpr self_t &operator ++ (void) noexcept
	{ --this->_data; return *this; }

	constexpr self_t &operator -- (void) noexcept
	{ ++this->_data; return *this; }


	constexpr reference operator * (void) const noexcept
	{ return *this->_data; }

	constexpr pointer operator -> (void) const noexcept
	{ return this->_data; }


	constexpr self_t &operator += (size_t n) noexcept
	{ this->_data -= n; return *this; }

	constexpr self_t &operator -= (size_t n) noexcept
	{ this->_data += n; return *this; }


	constexpr friend self_t operator + (self_t lhs, size_t rhs) noexcept
	{ lhs += rhs; return lhs; }

	constexpr friend self_t operator + (size_t lhs, self_t rhs) noexcept
	{ rhs += lhs; return rhs; }

	constexpr friend self_t operator - (self_t lhs, size_t rhs) noexcept
	{ lhs -= rhs; return lhs; }


#define cmp_op_def(op)                                              \
constexpr friend bool operator op (self_t lhs, self_t rhs) noexcept \
{ return rhs._data op lhs._data; }

	cmp_op_def(==)
	cmp_op_def(!=)
	cmp_op_def(<)
	cmp_op_def(<=)
	cmp_op_def(>)
	cmp_op_def(>=)

#undef cmp_op_def


	constexpr friend difference_type operator - (self_t lhs, self_t rhs) noexcept
	{ return rhs._data - lhs._data; }
};


template<typename T, size_t N>
constexpr random_access_iterator<T> begin(T (&arr)[N]) noexcept
{ return &arr[0]; }

template<typename T, size_t N>
constexpr random_access_iterator<const T> begin(const T (&arr)[N]) noexcept
{ return &arr[0]; }

template<typename T, size_t N>
constexpr random_access_iterator<const T> cbegin(T (&arr)[N]) noexcept
{ return &arr[0]; }

template<typename T, size_t N>
constexpr random_access_iterator<const T> cbegin(const T (&arr)[N]) noexcept
{ return &arr[0]; }

template<typename T, size_t N>
constexpr random_access_iterator<T> end(T (&arr)[N]) noexcept
{ return &arr[0] + N; }

template<typename T, size_t N>
constexpr random_access_iterator<const T> end(const T (&arr)[N]) noexcept
{ return &arr[0] + N; }

template<typename T, size_t N>
constexpr random_access_iterator<const T> cend(T (&arr)[N]) noexcept
{ return &arr[0] + N; }

template<typename T, size_t N>
constexpr random_access_iterator<const T> cend(const T (&arr)[N]) noexcept
{ return &arr[0] + N; }


template<typename T, size_t N>
constexpr reverse_iterator<random_access_iterator<T>> rbegin(T (&arr)[N]) noexcept
{ return &arr[N - 1]; }

template<typename T, size_t N>
constexpr reverse_iterator<random_access_iterator<const T>> rbegin(const T (&arr)[N]) noexcept
{ return &arr[N - 1]; }

template<typename T, size_t N>
constexpr reverse_iterator<random_access_iterator<const T>> crbegin(T (&arr)[N]) noexcept
{ return &arr[N - 1]; }

template<typename T, size_t N>
constexpr reverse_iterator<random_access_iterator<const T>> crbegin(const T (&arr)[N]) noexcept
{ return &arr[N - 1]; }

template<typename T, size_t N>
constexpr reverse_iterator<random_access_iterator<T>> rend(T (&arr)[N]) noexcept
{ return &arr[N - 1] - N; }

template<typename T, size_t N>
constexpr reverse_iterator<random_access_iterator<const T>> rend(const T (&arr)[N]) noexcept
{ return &arr[N - 1] - N; }

template<typename T, size_t N>
constexpr reverse_iterator<random_access_iterator<const T>> crend(T (&arr)[N]) noexcept
{ return &arr[N - 1] - N; }

template<typename T, size_t N>
constexpr reverse_iterator<random_access_iterator<const T>> crend(const T (&arr)[N]) noexcept
{ return &arr[N - 1] - N; }


namespace internal
{

template<typename ...Iter>
struct tuple_iterator
{
	using self_t = tuple_iterator<Iter...>;
	using tuple_t = std::tuple<Iter...>;
	static constexpr size_t iter_count = sizeof... (Iter);

	template<typename It>
	using value_type_of = decltype(*std::declval<It>());
	using deref_t = std::tuple<value_type_of<Iter> &...>;

	tuple_t _data;

	constexpr self_t &operator ++ (void) noexcept((noexcept(++std::declval<Iter &>()) && ...))
	{
		[this]<size_t ...Ns>(meta::index_sequence<Ns...>) {
			((++std::get<Ns>(this->_data)), ...);
		}(meta::make_index_sequence<iter_count>{});
		return *this;
	}

	constexpr deref_t operator * (void) noexcept((noexcept(*std::declval<Iter>()) && ...))
	{
		return [this]<size_t ...Ns>(meta::index_sequence<Ns...>) {
			return deref_t{ (*std::get<Ns>(this->_data))... };
		}(meta::make_index_sequence<iter_count>{});
	}
};

template<typename ...Iter>
constexpr bool operator == (tuple_iterator<Iter...> const &lhs, tuple_iterator<Iter...> const &rhs) noexcept((noexcept(std::declval<Iter>() == std::declval<Iter>()) && ...))
{
	return [&lhs, &rhs]<size_t ...Ns>(meta::index_sequence<Ns...>) {
		return ((std::get<Ns>(lhs._data) == std::get<Ns>(rhs._data)) && ...);
	}(meta::make_index_sequence<sizeof... (Iter)>{});
}

template<typename ...Iter>
constexpr bool operator != (tuple_iterator<Iter...> const &lhs, tuple_iterator<Iter...> const &rhs) noexcept((noexcept(std::declval<Iter>() != std::declval<Iter>()) && ...))
{
	// && has to used for iteration, so it stops when any one of the iterators
	// reaches the end
	return [&lhs, &rhs]<size_t ...Ns>(meta::index_sequence<Ns...>) {
		return ((std::get<Ns>(lhs._data) != std::get<Ns>(rhs._data)) && ...);
	}(meta::make_index_sequence<sizeof... (Iter)>{});
}

template<typename ...Iter>
struct zipped_iterators
{
	using tuple_t = std::tuple<Iter...>;
	using iterator = tuple_iterator<Iter...>;

	tuple_t _begin;
	tuple_t _end;

	constexpr zipped_iterators(tuple_t begin, tuple_t end) noexcept(meta::is_nothrow_copy_constructible_v<tuple_t>)
		: _begin(std::move(begin)), _end(std::move(end))
	{}

	constexpr iterator begin(void) const noexcept(meta::is_nothrow_copy_constructible_v<tuple_t>)
	{ return iterator{this->_begin}; }

	constexpr iterator end(void) const noexcept(meta::is_nothrow_copy_constructible_v<tuple_t>)
	{ return iterator{this->_end}; }
};

} // namespace internal

template<typename T, typename U>
constexpr auto zip(T const &t, U const &u) noexcept(
	noexcept(t.begin()) && noexcept(t.end())
	&& noexcept(u.begin()) && noexcept(u.end())
	&& meta::is_nothrow_constructible_v<
		internal::zipped_iterators<
			meta::remove_cv_reference<decltype(t.begin())>,
			meta::remove_cv_reference<decltype(u.begin())>
		>,
		decltype(std::make_tuple(t.begin(), u.begin())),
		decltype(std::make_tuple(t.end(), u.end()))
	>
) -> internal::zipped_iterators<
	meta::remove_cv_reference<decltype(t.begin())>,
	meta::remove_cv_reference<decltype(u.begin())>
>
{
	using ret_t = internal::zipped_iterators<
		meta::remove_cv_reference<decltype(t.begin())>,
		meta::remove_cv_reference<decltype(u.begin())>
	>;
	return ret_t(std::make_tuple(t.begin(), u.begin()), std::make_tuple(t.end(), u.end()));
}


namespace internal
{

template<typename It>
struct reverse_iteration_range
{
	It _begin;
	It _end;

	constexpr It begin(void) const
	{ return this->_begin; }

	constexpr It end(void) const
	{ return this->_end; }
};

} // namespace internal

template<typename Range>
constexpr auto reversed(Range &&range)
{
	using ret_t = internal::reverse_iteration_range<decltype(std::forward<Range>(range).rbegin())>;
	return ret_t{
		std::forward<Range>(range).rbegin(),
		std::forward<Range>(range).rend(),
	};
}

bz_end_namespace

#endif // _bz_iterator_h__
