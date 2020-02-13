#ifndef _bz_string_view_h__
#define _bz_string_view_h__

#include "core.h"
#include "iterator.h"

bz_begin_namespace

template<typename>
class basic_string_view;

using string_view = basic_string_view<char>;

template<typename Char>
class basic_string_view
{
	static_assert(
		meta::is_trivial_v<Char>,
		"basic_string value_type must be trivial"
	);
private:
	using self_t = basic_string_view<Char>;

public:
	using value_type = Char;
	using size_type  = std::size_t;

	// ==== iteration ====
	using const_iterator = ::bz::random_access_iterator<const value_type>;
	using       iterator = const_iterator;

	using const_reverse_iterator = ::bz::reverse_iterator<const_iterator>;
	using       reverse_iterator = const_reverse_iterator;

private:
	value_type const *_data_begin;
	value_type const *_data_end;

public:
	constexpr basic_string_view(void) noexcept
		: _data_begin(nullptr),
		  _data_end  (nullptr)
	{}

	constexpr basic_string_view  (self_t const &) noexcept = default;
	constexpr basic_string_view  (self_t &&)      noexcept = default;
	         ~basic_string_view  (void)           noexcept = default;
	constexpr self_t &operator = (self_t const &) noexcept = default;
	constexpr self_t &operator = (self_t &&)      noexcept = default;

	constexpr explicit basic_string_view(
		value_type const *begin,
		value_type const *end
	) noexcept
		: _data_begin(begin),
		  _data_end  (end)
	{}

	constexpr explicit basic_string_view(
		const_iterator begin,
		const_iterator end
	) noexcept
		: _data_begin(begin.data()),
		  _data_end  (end.data())
	{}

	constexpr basic_string_view(value_type const *str) noexcept
		: _data_begin(str), _data_end()
	{
		if (str == nullptr)
		{
			this->_data_end = nullptr;
			return;
		}

		while (*str != value_type())
		{
			++str;
		}

		this->_data_end = str;
	}

public:
	// ==== length ====
	constexpr auto size(void) const noexcept
	{ return static_cast<size_type>(this->_data_end - this->_data_begin); }

	constexpr auto length(void) const noexcept
	{ return this->size(); }

public:
	// ==== memeber access ====
	constexpr auto operator [] (size_type n) const noexcept -> value_type const &
	{ return *(this->_data_begin + n); }

	constexpr auto front(void) const noexcept -> value_type const &
	{ return *this->_data_begin; }

	constexpr auto back(void) const noexcept -> value_type const &
	{ return *(this->_data_end - 1); }

	constexpr auto data(void) const noexcept
	{ return this->_data_begin; }

public:
	auto begin(void) const noexcept
	{ return const_iterator(this->_data_begin); }

	auto end(void) const noexcept
	{ return const_iterator(this->_data_end); }

	auto cbegin(void) const noexcept
	{ return const_iterator(this->_data_begin); }

	auto cend(void) const noexcept
	{ return const_iterator(this->_data_end); }


	auto rbegin(void) const noexcept
	{ return const_reverse_iterator(this->_data_end - 1); }

	auto rend(void) const noexcept
	{ return const_reverse_iterator(this->_data_begin - 1); }

	auto crbegin(void) const noexcept
	{ return const_reverse_iterator(this->_data_end - 1); }

	auto crend(void) const noexcept
	{ return const_reverse_iterator(this->_data_begin - 1); }
};

template<typename Char>
constexpr bool operator == (
	basic_string_view<Char> lhs,
	basic_string_view<Char> rhs
) noexcept
{
	if (lhs.length() != rhs.length())
	{
		return false;
	}

	auto lhs_it  = lhs.begin();
	auto rhs_it  = rhs.begin();
	auto lhs_end = lhs.end();

	while (
		lhs_it != lhs_end
		&& *lhs_it == *rhs_it
	)
	{
		++lhs_it;
		++rhs_it;
	}

	return lhs_it == lhs_end;
}

template<typename Char>
constexpr bool operator != (
	basic_string_view<Char> lhs,
	basic_string_view<Char> rhs
) noexcept
{ return !(lhs == rhs); }

template<typename Char>
constexpr bool operator == (
	Char const             *lhs,
	basic_string_view<Char> rhs
) noexcept
{ return basic_string_view<Char>(lhs) == rhs; }

template<typename Char>
constexpr bool operator != (
	Char const             *lhs,
	basic_string_view<Char> rhs
) noexcept
{ return !(lhs == rhs); }

template<typename Char>
constexpr bool operator == (
	basic_string_view<Char> lhs,
	Char const             *rhs
) noexcept
{ return lhs == basic_string_view<Char>(rhs); }

template<typename Char>
constexpr bool operator != (
	basic_string_view<Char> lhs,
	Char const             *rhs
) noexcept
{ return !(lhs == rhs); }


template<typename Char>
::std::basic_ostream<Char> &operator << (::std::basic_ostream<Char> &os, string_view str)
{
	return os.write(str.data(), str.length());
}

namespace literals
{

constexpr auto operator ""_sv (char const *str, size_t) noexcept
{ return string_view(str); }

} // namespace literals

bz_end_namespace

#endif // _bz_string_view_h__
