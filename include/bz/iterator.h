#ifndef _bz_iterator_h__
#define _bz_iterator_h__

#include "core.h"

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


	constexpr reference operator * (void) noexcept
	{ return *this->_data; }

	constexpr const_reference operator * (void) const noexcept
	{ return *this->_data; }

	constexpr pointer operator -> (void) noexcept
	{ return this->_data; }

	constexpr const_pointer operator -> (void) const noexcept
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


	constexpr reference operator * (void) noexcept
	{ return *this->_data; }

	constexpr const_reference operator * (void) const noexcept
	{ return *this->_data; }

	constexpr pointer operator -> (void) noexcept
	{ return this->_data; }

	constexpr const_pointer operator -> (void) const noexcept
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


bz_end_namespace

#endif // _bz_iterator_h__
