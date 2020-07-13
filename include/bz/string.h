#ifndef _bz_string_h__
#define _bz_string_h__

#include "core.h"

#include "meta.h"
#include "iterator.h"
#include "allocator.h"
#include "string_view.h"

bz_begin_namespace

template<typename Char, typename = allocator<Char>>
class basic_string;

using string = basic_string<char>;

template<typename Char, typename Alloc>
class basic_string
{
	static_assert(
		meta::is_trivial_v<Char>,
		"basic_string value_type must be trivial"
	);
private:
	using self_t = basic_string<Char, Alloc>;

	static constexpr bool nothrow_alloc   = meta::is_fn_noexcept_v<decltype(&Alloc::allocate)>;
	static constexpr bool nothrow_dealloc = meta::is_fn_noexcept_v<decltype(&Alloc::deallocate)>;

public:
	using value_type     = Char;
	using allocator_type = Alloc;
	using size_type      = typename allocator_type::size_type;

private:
	value_type *_data_begin;
	value_type *_data_end;
	value_type *_alloc_end;

	allocator_type _allocator;

	static constexpr size_type short_string_cap
		= (2 * sizeof(value_type *)) / sizeof(value_type);

private:
	auto short_string_begin(void) noexcept
	{
		static_assert(offsetof(self_t, _alloc_end) - offsetof(self_t, _data_end) == sizeof(value_type *));
		static_assert(alignof(value_type *) % alignof(value_type) == 0);
		return reinterpret_cast<value_type *>(&this->_data_end);
	}

	auto short_string_begin(void) const noexcept
	{
		static_assert(offsetof(self_t, _alloc_end) - offsetof(self_t, _data_end) == sizeof(value_type *));
		static_assert(alignof(value_type *) % alignof(value_type) == 0);
		return reinterpret_cast<value_type const *>(&this->_data_end);
	}

	auto short_string_end(void) noexcept
	{
		auto begin = this->short_string_begin();
		auto end   = this->short_string_begin() + short_string_cap;

		for (; begin != end; ++begin)
		{
			if (*begin == value_type())
			{
				return begin;
			}
		}
		return end;
	}

	auto short_string_end(void) const noexcept
	{
		auto begin = this->short_string_begin();
		auto end   = this->short_string_begin() + short_string_cap;

		for (; begin != end; ++begin)
		{
			if (*begin == value_type())
			{
				return begin;
			}
		}
		return end;
	}

	auto begin_ptr(void) noexcept
	{
		return this->_data_begin == nullptr
			? this->short_string_begin()
			: this->_data_begin;
	}

	auto begin_ptr(void) const noexcept
	{
		return this->_data_begin == nullptr
			? this->short_string_begin()
			: this->_data_begin;
	}

	auto end_ptr(void) noexcept
	{
		return this->_data_begin == nullptr
			? this->short_string_end()
			: this->_data_end;
	}

	auto end_ptr(void) const noexcept
	{
		return this->_data_begin == nullptr
			? this->short_string_end()
			: this->_data_end;
	}

	auto alloc_new(size_type n) noexcept(nothrow_alloc)
	{ return this->_allocator.allocate(n); }

	void set_to_null(void) noexcept
	{
		this->_data_begin = nullptr;
		this->_data_end   = nullptr;
		this->_alloc_end  = nullptr;
	}

	void no_null_clear(void) noexcept(nothrow_dealloc)
	{
		if (this->_data_begin != nullptr)
		{
			this->_allocator.deallocate(this->_data_begin, this->capacity());
		}
	}

	template<bool assign_alloc = true>
	void no_clear_assign(self_t const &other) noexcept(
		nothrow_alloc
		&& (
			!assign_alloc
			|| meta::is_nothrow_copy_assignable_v<allocator_type>
		)
	)
	{
		if constexpr (assign_alloc)
		{
			this->_allocator  = other._allocator;
		}

		if (other._data_begin == nullptr)
		{
			this->_data_begin = nullptr;
			this->_data_end   = other._data_end;
			this->_alloc_end  = other._alloc_end;
		}
		else
		{
			auto other_cap = other.capacity();
			this->_data_begin = this->alloc_new(other_cap);
			this->_data_end   = this->_data_begin;
			this->_alloc_end  = this->_data_begin + other_cap;

			auto other_it  = other._data_begin;
			auto other_end = other._data_end;

			for (; other_it != other_end; ++this->_data_end, ++other_it)
			{
				*this->_data_end = *other_it;
			}
		}
	}

	template<bool assign_alloc = true>
	void no_clear_assign(self_t &&other) noexcept(
		!assign_alloc
		|| meta::is_nothrow_move_assignable_v<allocator_type>
	)
	{
		if constexpr (assign_alloc)
		{
			this->_allocator  = std::move(other._allocator);
		}
		this->_data_begin = other._data_begin;
		this->_data_end   = other._data_end;
		this->_alloc_end  = other._alloc_end;
		other.set_to_null();
	}

	void copy_to_new_alloc(value_type *new_data, size_type new_cap) noexcept(nothrow_dealloc)
	{
		auto old_it  = this->begin_ptr();
		auto old_end = this->end_ptr();
		auto new_it  = new_data;

		for (; old_it != old_end; ++new_it, ++old_it)
		{
			*new_it = *old_it;
		}

		this->no_null_clear();
		this->_data_begin = new_data;
		this->_data_end   = new_it;
		this->_alloc_end  = new_data + new_cap;
	}

	void erase_ptr(value_type const *p) noexcept
	{
		auto begin = this->begin_ptr();
		auto end   = this->end_ptr();

		if (p < begin || p >= end)
		{
			return;
		}

		// to remove const without const_cast
		auto it = (p - begin) + begin;
		auto trace = it;
		++it;

		for (; it != end; ++trace, ++it)
		{
			*trace = *it;
		}

		if (this->_data_begin == nullptr)
		{
			*trace = value_type();
		}
		else
		{
			--this->_data_end;
		}
	}

public:
	void clear(void) noexcept(nothrow_dealloc)
	{
		this->no_null_clear();
		this->set_to_null();
	}

	void assign(self_t const &other) noexcept(
		nothrow_alloc
		&& nothrow_dealloc
		&& meta::is_nothrow_copy_assignable_v<allocator_type>
	)
	{
		if (this == &other)
		{
			return;
		}
		this->clear();
		this->no_clear_assign(other);
	}

	void assign(self_t &&other) noexcept(
		nothrow_dealloc
		&& meta::is_nothrow_copy_assignable_v<allocator_type>
	)
	{
		if (this == &other)
		{
			return;
		}
		this->clear();
		this->no_clear_assign(std::move(other));
	}

public:
	basic_string(void) noexcept(
		meta::is_nothrow_default_constructible_v<allocator_type>
	)
		: _data_begin(nullptr),
		  _data_end  (nullptr),
		  _alloc_end (nullptr),
		  _allocator ()
	{}

	basic_string(self_t const &other) noexcept(
		meta::is_nothrow_copy_constructible_v<allocator_type>
	)
		: _allocator(other._allocator)
	{ this->no_clear_assign<false>(other); }

	basic_string(self_t &&other) noexcept(
		meta::is_nothrow_move_constructible_v<allocator_type>
	)
		: _data_begin(other._data_begin),
		  _data_end  (other._data_end),
		  _alloc_end (other._alloc_end),
		  _allocator (std::move(other._allocator))
	{ other.set_to_null(); }

	~basic_string(void) noexcept(nothrow_dealloc)
	{ this->no_null_clear(); }

	self_t &operator = (self_t const &other) noexcept(
		nothrow_alloc
		&& nothrow_dealloc
	)
	{
		this->assign(other);
		return *this;
	}

	self_t &operator = (self_t &&other) noexcept(nothrow_dealloc)
	{
		this->assign(std::move(other));
		return *this;
	}

	basic_string(basic_string_view<value_type> str) noexcept(nothrow_alloc)
		: self_t()
	{
		this->reserve(static_cast<size_type>(str.length()));

		auto str_it  = str.begin();
		auto str_end = str.end();

		if (this->_data_begin == nullptr)
		{
			auto it = this->short_string_begin();
			for (; str_it != str_end; ++it, ++str_it)
			{
				*it = *str_it;
			}
		}
		else
		{
			auto it = this->_data_begin;
			for (; str_it != str_end; ++it, ++str_it)
			{
				*it = *str_it;
			}
			this->_data_end = it;
		}}

	basic_string(value_type const *str) noexcept(nothrow_alloc)
		: self_t(basic_string_view<value_type>(str))
	{}

	explicit basic_string(value_type const *begin, value_type const *end)
		: self_t(basic_string_view<value_type>(begin, end))
	{}

	explicit basic_string(value_type c) noexcept
		: self_t()
	{ *this->short_string_begin() = c; }

	explicit basic_string(size_type size, value_type val) noexcept(nothrow_alloc)
		: self_t()
	{ this->resize(size, val); }

public:
	// ==== size modifiers ====
	void resize(size_type new_size, value_type c = ' ') noexcept(
		nothrow_alloc
		&& nothrow_dealloc
	)
	{
		auto current_size = this->size();
		if (new_size == current_size)
		{
			return;
		}
		else if (new_size < current_size)
		{
			if (this->_data_begin == nullptr)
			{
				auto it  = this->short_string_begin() + new_size;
				auto end = it + current_size;
				for (; it != end; ++it)
				{
					*it = value_type();
				}
			}
			else
			{
				this->_data_end = this->_data_begin + new_size;
				this->shrink_to_fit();
			}
		}
		else
		{
			this->reserve(new_size);
			if (this->_data_begin == nullptr)
			{
				auto new_end = this->short_string_begin() + new_size;
				auto end_it  = this->short_string_end();

				for (; end_it != new_end; ++end_it)
				{
					*end_it = c;
				}
			}
			else
			{
				auto it = this->_data_end;
				for (; current_size != new_size; ++it, ++current_size)
				{
					*it = c;
				}
				this->_data_end = it;
			}
		}
	}

	void reserve(size_type reserve_size) noexcept(
		nothrow_alloc
		&& nothrow_dealloc
	)
	{
		auto current_cap = this->capacity();
		if (current_cap >= reserve_size)
		{
			return;
		}
		else
		{
			auto new_cap = current_cap == 0 ? 1 : current_cap;

			while (new_cap < reserve_size)
			{
				new_cap *= 2;
			}

			auto new_data = this->alloc_new(new_cap);
			this->copy_to_new_alloc(new_data, new_cap);
		}
	}

	void shrink_to_fit(void) noexcept(
		nothrow_alloc
		&& nothrow_dealloc
	)
	{
		if (this->_data_begin == nullptr)
		{
			return;
		}

		auto current_size = this->size();
		auto current_cap  = this->capacity();

		if (current_size <= 3 * (short_string_cap) / 4)
		{
			auto alloc_begin = this->_data_begin;

			auto it    = alloc_begin;
			auto end   = this->_data_end;

			this->set_to_null();
			auto short_it = this->short_string_begin();
			for (; it != end; ++short_it, ++it)
			{
				*short_it = *it;
			}

			this->_allocator.deallocate(alloc_begin, current_cap);
			return;
		}

		if (current_size > 3 * (current_cap / 8))
		{
			return;
		}

		auto new_cap = current_cap;
		while (current_size <= new_cap / 2)
		{
			new_cap /= 2;
		}

		auto new_data = this->alloc_new(new_cap);
		this->copy_to_new_alloc(new_data, new_cap);
	}

	auto push_back(value_type c) noexcept(
		nothrow_alloc
		&& nothrow_dealloc
	) -> value_type &
	{
		this->reserve(this->size() + 1);
		if (this->_data_begin == nullptr)
		{
			auto end = this->short_string_end();
			*end = c;
			return *end;
		}
		else
		{
			auto end = this->_data_end;
			*end = c;
			++this->_data_end;
			return *end;
		}
	}

	template<typename ...Args>
	auto emplace_back(Args &&...args) noexcept(
		nothrow_alloc
		&& nothrow_dealloc
	) -> value_type &
	{ return this->push_back(value_type(std::forward<Args>(args)...)); }

	void append(value_type c) noexcept(
		nothrow_alloc
		&& nothrow_dealloc
	)
	{ this->push_back(c); }

	void append(basic_string_view<value_type> str) noexcept(
		nothrow_alloc
		&& nothrow_dealloc
	)
	{
		this->reserve(this->size() + str.length());
		auto str_it  = str.begin();
		auto str_end = str.end();

		if (this->_data_begin == nullptr)
		{
			auto it = this->short_string_end();
			for (; str_it != str_end; ++it, ++str_it)
			{
				*it = *str_it;
			}
		}
		else
		{
			auto it = this->_data_end;
			for (; str_it != str_end; ++it, ++str_it)
			{
				*it = *str_it;
			}
			this->_data_end = it;
		}
	}

public:
	// ==== size and capacity ====
	auto size(void) const noexcept
	{
		if (this->_data_begin == nullptr)
		{
			return static_cast<size_type>(
				this->short_string_end() - this->short_string_begin()
			);
		}
		else
		{
			return static_cast<size_type>(this->_data_end - this->_data_begin);
		}
	}

	auto length(void) const noexcept
	{ return this->size(); }

	auto capacity(void) const noexcept
	{
		if (this->_data_begin == nullptr)
		{
			return short_string_cap;
		}
		else
		{
			return static_cast<size_type>(this->_alloc_end - this->_data_begin);
		}
	}

	auto max_size(void) const
	{ return this->_allocator.max_size(); }

public:
	// ==== member access ====
	auto operator [] (size_type n) noexcept -> value_type &
	{ return *(this->begin_ptr() + n); }

	auto operator [] (size_type n) const noexcept -> value_type const &
	{ return *(this->begin_ptr() + n); }

	auto front(void) noexcept -> value_type &
	{ return *this->begin_ptr(); }

	auto front(void) const noexcept -> value_type const &
	{ return *this->begin_ptr(); }

	auto back(void) const noexcept -> value_type const &
	{ return *(this->end_ptr() - 1); }

	auto back(void) noexcept -> value_type &
	{ return *(this->end_ptr() - 1); }

	auto data(void) noexcept
	{ return this->begin_ptr(); }

	auto data(void) const noexcept
	{ return this->begin_ptr(); }

public:
	// ==== iteration ====
	using       iterator = ::bz::random_access_iterator<      value_type>;
	using const_iterator = ::bz::random_access_iterator<const value_type>;

	using       reverse_iterator = ::bz::reverse_iterator<      iterator>;
	using const_reverse_iterator = ::bz::reverse_iterator<const_iterator>;

	auto begin(void) noexcept
	{ return iterator(this->begin_ptr()); }

	auto begin(void) const noexcept
	{ return const_iterator(this->begin_ptr()); }

	auto cbegin(void) const noexcept
	{ return const_iterator(this->begin_ptr()); }

	auto rbegin(void) noexcept
	{ return reverse_iterator(this->end_ptr() - 1); }

	auto rbegin(void) const noexcept
	{ return const_reverse_iterator(this->end_ptr() - 1); }

	auto crbegin(void) const noexcept
	{ return const_reverse_iterator(this->end_ptr() - 1); }

	auto end(void) noexcept
	{ return iterator(this->end_ptr()); }

	auto end(void) const noexcept
	{ return const_iterator(this->end_ptr()); }

	auto cend(void) const noexcept
	{ return const_iterator(this->end_ptr()); }

	auto rend(void) noexcept
	{ return reverse_iterator(this->begin_ptr() - 1); }

	auto rend(void) const noexcept
	{ return const_reverse_iterator(this->begin_ptr() - 1); }

	auto crend(void) const noexcept
	{ return const_reverse_iterator(this->begin_ptr() - 1); }

	auto find(value_type c) const noexcept
	{
		auto it  = this->begin();
		auto end = this->end();
		while (it != end && *it != c)
		{
			++it;
		}
		return it;
	}

	auto find(value_type c) noexcept
	{
		auto it  = this->begin();
		auto end = this->end();
		while (it != end && *it != c)
		{
			++it;
		}
		return it;
	}

	auto rfind(value_type c) const noexcept
	{
		auto it  = this->rbegin();
		auto end = this->rend();
		while (it != end && *it != c)
		{
			++it;
		}
		return it;
	}

	auto rfind(value_type c) noexcept
	{
		auto it  = this->rbegin();
		auto end = this->rend();
		while (it != end && *it != c)
		{
			++it;
		}
		return it;
	}

	void erase(const_iterator it) noexcept
	{ this->erase_ptr(&*it); }

	void erase(const_reverse_iterator it) noexcept
	{ this->erase_ptr(&*it); }

	void erase(value_type c) noexcept(
		nothrow_alloc
		&& nothrow_dealloc
	)
	{
		if (this->_data_begin == nullptr)
		{
			auto it  = this->short_string_begin();
			auto end = it + short_string_cap;

			while (it != end && *it != value_type())
			{
				if (*it == c)
				{
					this->erase_ptr(it);
				}
				else
				{
					++it;
				}
			}
		}
		else
		{
			auto it = this->_data_begin;
			while (it != this->_data_end)
			{
				if (*it == c)
				{
					this->erase_ptr(it);
				}
				else
				{
					++it;
				}
			}
			this->shrink_to_fit();
		}
	}

public:
	// ==== operators ====
	operator basic_string_view<value_type> (void) const noexcept
	{
		if (this->_data_begin == nullptr)
		{
			return basic_string_view<value_type>(
				this->short_string_begin(),
				this->short_string_end()
			);
		}
		else
		{
			return basic_string_view<value_type>(
				this->_data_begin,
				this->_data_end
			);
		}
	}

	self_t &operator += (value_type c) noexcept(
		nothrow_alloc
		&& nothrow_dealloc
	)
	{
		this->push_back(c);
		return *this;
	}

	self_t &operator += (self_t const &str) noexcept(
		nothrow_alloc
		&& nothrow_dealloc
	)
	{
		this->append(str);
		return *this;
	}

	self_t &operator += (basic_string_view<value_type> str) noexcept(
		nothrow_alloc
		&& nothrow_dealloc
	)
	{
		this->append(str);
		return *this;
	}

	self_t &operator += (value_type const *str) noexcept(
		nothrow_alloc
		&& nothrow_dealloc
	)
	{
		this->append(str);
		return *this;
	}
};


template<typename Char, typename Alloc>
bool operator == (
	basic_string<Char, Alloc> const &lhs,
	basic_string<Char, Alloc> const &rhs
) noexcept
{ return basic_string_view<Char>(lhs) == basic_string_view<Char>(rhs); }

template<typename Char, typename Alloc>
bool operator != (
	basic_string<Char, Alloc> const &lhs,
	basic_string<Char, Alloc> const &rhs
) noexcept
{ return !(lhs == rhs); }

#define string_cmp_op_definition(...)                                    \
                                                                         \
template<typename Char, typename Alloc>                                  \
bool operator == (                                                       \
    __VA_ARGS__                      lhs,                                \
    basic_string<Char, Alloc> const &rhs                                 \
) noexcept                                                               \
{ return basic_string_view<Char>(lhs) == basic_string_view<Char>(rhs); } \
                                                                         \
template<typename Char, typename Alloc>                                  \
bool operator != (                                                       \
    __VA_ARGS__                      lhs,                                \
    basic_string<Char, Alloc> const &rhs                                 \
) noexcept                                                               \
{ return !(lhs == rhs); }                                                \
                                                                         \
template<typename Char, typename Alloc>                                  \
bool operator == (                                                       \
    basic_string<Char, Alloc> const &lhs,                                \
    __VA_ARGS__                      rhs                                 \
) noexcept                                                               \
{ return basic_string_view<Char>(lhs) == basic_string_view<Char>(rhs); } \
                                                                         \
template<typename Char, typename Alloc>                                  \
bool operator != (                                                       \
    basic_string<Char, Alloc> const &lhs,                                \
    __VA_ARGS__                      rhs                                 \
) noexcept                                                               \
{ return !(lhs == rhs); }                                                \

string_cmp_op_definition(basic_string_view<Char>)
string_cmp_op_definition(Char const *)

#undef string_cmp_op_definition


template<typename Char, typename Alloc>
auto operator + (
	basic_string<Char, Alloc> const &lhs,
	basic_string<Char, Alloc> const &rhs
) noexcept(
	noexcept(
		basic_string<Char, Alloc>(lhs) += rhs
	)
) -> basic_string<Char, Alloc>
{
	basic_string<Char, Alloc> cpy = lhs;
	cpy += rhs;
	return cpy;
}

template<typename Char, typename Alloc>
auto operator + (
	basic_string<Char, Alloc> const &lhs,
	basic_string_view<Char>          rhs
) noexcept(
	noexcept(
		basic_string<Char, Alloc>(lhs) += rhs
	)
) -> basic_string<Char, Alloc>
{
	basic_string<Char, Alloc> cpy = lhs;
	cpy += rhs;
	return cpy;
}

template<typename Char, typename Alloc>
auto operator + (
	basic_string_view<Char>          lhs,
	basic_string<Char, Alloc> const &rhs
) noexcept(
	noexcept(
		basic_string<Char, Alloc>(lhs) += rhs
	)
) -> basic_string<Char, Alloc>
{
	basic_string<Char, Alloc> cpy = lhs;
	cpy += rhs;
	return cpy;
}

template<typename Char, typename Alloc>
auto operator + (
	basic_string<Char, Alloc> const &lhs,
	Char const                      *rhs
) noexcept(
	noexcept(
		basic_string<Char, Alloc>(lhs) += rhs
	)
) -> basic_string<Char, Alloc>
{
	basic_string<Char, Alloc> cpy = lhs;
	cpy += rhs;
	return cpy;
}

template<typename Char, typename Alloc>
auto operator + (
	Char const                      *lhs,
	basic_string<Char, Alloc> const &rhs
) noexcept(
	noexcept(
		basic_string<Char, Alloc>(lhs) += rhs
	)
) -> basic_string<Char, Alloc>
{
	basic_string<Char, Alloc> cpy = lhs;
	cpy += rhs;
	return cpy;
}

template<typename Char, typename Alloc>
auto operator + (
	basic_string<Char, Alloc> const &lhs,
	Char                             rhs
) noexcept(
	noexcept(
		basic_string<Char, Alloc>(lhs) += rhs
	)
) -> basic_string<Char, Alloc>
{
	basic_string<Char, Alloc> cpy = lhs;
	cpy += rhs;
	return cpy;
}

template<typename Char, typename Alloc>
auto operator + (
	Char                             lhs,
	basic_string<Char, Alloc> const &rhs
) noexcept(
	noexcept(
		basic_string<Char, Alloc>(1, lhs) += rhs
	)
) -> basic_string<Char, Alloc>
{
	basic_string<Char, Alloc> cpy(1, lhs);
	cpy += rhs;
	return cpy;
}


template<typename Char, typename Alloc>
auto operator << (
	::std::basic_ostream<Char> &os,
	basic_string<Char, Alloc> const &str
) -> ::std::basic_ostream<Char> &
{
	return os.write(str.data(), str.length());
}

namespace literals
{

inline auto operator ""_s (char const *str, size_t) noexcept(noexcept(string(str)))
{ return string(str); }

} // namespace literals

bz_end_namespace

#endif // _bz_string_h__
