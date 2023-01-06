#ifndef _bz_fixed_vector_h__
#define _bz_fixed_vector_h__

#include "core.h"

#include "allocator.h"
#include "meta.h"
#include "iterator.h"
#include "array_view.h"
#include "ranges.h"

bz_begin_namespace

template<typename T, typename = allocator<T>>
class fixed_vector;

template<typename T, typename Alloc>
class fixed_vector : public collection_base<fixed_vector<T, Alloc>>
{
	static_assert(
		!meta::is_reference<T>,
		"value_type of fixed_vector can't be a reference"
	);
	static_assert(
		!meta::is_void<T>,
		"value_type of fixed_vector can't be void"
	);
private:
	using self_t = fixed_vector<T, Alloc>;

#if __cpp_exceptions

	static constexpr bool nothrow_alloc   = false;
	static constexpr bool nothrow_dealloc = true;

	static constexpr bool nothrow_move_value        = meta::is_nothrow_move_constructible_v<T>;
	static constexpr bool nothrow_move_assign_value = meta::is_nothrow_move_assignable_v<T>;
	static constexpr bool nothrow_copy_value        = meta::is_nothrow_copy_constructible_v<T>;
	static constexpr bool nothrow_destruct_value    = meta::is_nothrow_destructible_v<T>;

	template<typename ...Args>
	static constexpr bool nothrow_construct_value = meta::is_nothrow_constructible_v<T, Args...>;

#else

	static constexpr bool nothrow_alloc   = true;
	static constexpr bool nothrow_dealloc = true;

	static constexpr bool nothrow_move_value        = true;
	static constexpr bool nothrow_move_assign_value = true;
	static constexpr bool nothrow_copy_value        = true;
	static constexpr bool nothrow_destruct_value    = true;

	template<typename ...Args>
	static constexpr bool nothrow_construct_value = true;

#endif // __cpp_exceptions

	static constexpr bool trivial_alloc = std::is_trivial_v<Alloc> || std::is_empty_v<Alloc>;

public:
	using value_type = T;
	using allocator_type = Alloc;
	using size_type = typename allocator_type::size_type;

	using       iterator = ::bz::random_access_iterator<      value_type>;
	using const_iterator = ::bz::random_access_iterator<const value_type>;

	using       reverse_iterator = ::bz::reverse_iterator<      iterator>;
	using const_reverse_iterator = ::bz::reverse_iterator<const_iterator>;

private:
	value_type *_data_begin;
	value_type *_data_end;

	[[no_unique_address]] allocator_type _allocator;

private:
	auto alloc_new(size_type n) noexcept(
		nothrow_alloc
	)
	{ return this->_allocator.allocate(n); }

	void set_to_null(void) noexcept
	{
		this->_data_begin = nullptr;
		this->_data_end   = nullptr;
	}

	void no_null_clear(void) noexcept(
		nothrow_dealloc
		&& nothrow_destruct_value
	)
	{
		if (this->_data_begin != nullptr)
		{
			auto it = this->_data_end;
			auto const begin = this->_data_begin;
			while (it != begin)
			{
				--it;
				it->~value_type();
			}
			this->_allocator.deallocate(this->_data_begin, this->size());
		}
	}

	template<typename ...Args>
	void try_emplace(value_type *p, Args &&...args) noexcept(
		nothrow_construct_value<Args...>
	)
	{
		if constexpr (nothrow_construct_value<Args...>)
		{
			// if it's not constructible with the provided args, we try to use
			// value_type{ args... } syntax instead
			if constexpr (meta::is_constructible_v<value_type, Args...>)
			{
				new (p) value_type(std::forward<Args>(args)...);
			}
			else
			{
				new (p) value_type{ std::forward<Args>(args)... };
			}
		}
		else
		{
			bz_try
			{
				// if it's not constructible with the provided args, we try to use
				// value_type{ args... } syntax instead
				if constexpr (meta::is_constructible_v<value_type, Args...>)
				{
					new (p) value_type(std::forward<Args>(args)...);
				}
				else
				{
					new (p) value_type{ std::forward<Args>(args)... };
				}
			}
			bz_catch_all
			{
				p->~value_type();
				bz_rethrow;
			}
		}
	}

	template<typename U>
	void initialize_from(array_view<U> arr) noexcept(
		nothrow_alloc
		&& nothrow_construct_value<U const &>
	)
	{
		auto const size = arr.size();
		if (size == 0)
		{
			return;
		}

		auto const data_begin = this->alloc_new(size);
		auto const data_end = data_begin + size;

		auto it = data_begin;
		bz_try
		{
			auto const *other_it = arr.data();
			for (; it != data_end; ++it, ++other_it)
			{
				this->try_emplace(it, *other_it);
			}
		}
		bz_catch_all
		{
			while (it != data_begin)
			{
				--it;
				it->~value_type();
			}
			this->_allocator.deallocate(data_begin, size);
			bz_rethrow;
		}

		this->_data_begin = data_begin;
		this->_data_end = data_end;
	}

	template<typename ...Args>
	void initialize_with(size_t size, Args &&...args) noexcept(
		nothrow_alloc
		&& nothrow_construct_value<Args...>
	)
	{
		if (size == 0)
		{
			return;
		}

		auto const data_begin = this->alloc_new(size);
		auto const data_end = data_begin + size;

		auto it = data_begin;
		bz_try
		{
			for (; it != data_end; ++it)
			{
				this->try_emplace(it, args...);
			}
		}
		bz_catch_all
		{
			while (it != data_begin)
			{
				--it;
				it->~value_type();
			}
			this->_allocator.deallocate(data_begin, size);
			bz_rethrow;
		}

		this->_data_begin = data_begin;
		this->_data_end = data_end;
	}

public:
	void clear(void) noexcept(nothrow_dealloc && nothrow_destruct_value)
	{
		this->no_null_clear();
		this->set_to_null();
	}

	void assign(self_t const &other) noexcept(
		nothrow_alloc
		&& nothrow_dealloc
		&& nothrow_destruct_value
		&& nothrow_copy_value
	)
	{
		if (this == &other)
		{
			return;
		}
		this->clear();

		this->initialize_from(other.as_array_view());
	}

	void assign(self_t &&other) noexcept(
		nothrow_dealloc
		&& nothrow_destruct_value
		&& (trivial_alloc || meta::is_nothrow_move_assignable_v<allocator_type>)
	)
	{
		if (this == &other)
		{
			return;
		}
		this->clear();
		this->_data_begin = other._data_begin;
		this->_data_end = other._data_end;
		this->_allocator = std::move(other._allocator);
		other.set_to_null();
	}

public:
	fixed_vector(void) noexcept(
		trivial_alloc || meta::is_nothrow_default_constructible_v<allocator_type>
	)
		: _data_begin(nullptr),
		  _data_end  (nullptr),
		  _allocator ()
	{}

	fixed_vector(self_t const &other) noexcept(
		nothrow_alloc
		&& nothrow_copy_value
		&& (trivial_alloc || meta::is_nothrow_copy_constructible_v<allocator_type>)
	)
		: _data_begin(nullptr),
		  _data_end(nullptr),
		  _allocator(other._allocator)
	{
		this->initialize_from(other.as_array_view());
	}

	fixed_vector(self_t &&other) noexcept(
		trivial_alloc || meta::is_nothrow_move_constructible_v<allocator_type>
	)
		: _data_begin(other._data_begin),
		  _data_end  (other._data_end),
		  _allocator (std::move(other._allocator))
	{ other.set_to_null(); }

	~fixed_vector(void) noexcept(
		nothrow_dealloc
		&& nothrow_destruct_value
	)
	{ this->no_null_clear(); }

	self_t &operator = (self_t const &other) noexcept(
		nothrow_alloc
		&& nothrow_dealloc
		&& nothrow_destruct_value
		&& nothrow_copy_value
		&& meta::is_nothrow_copy_assignable_v<allocator_type>
	)
	{
		this->assign(other);
		return *this;
	}

	self_t &operator = (self_t &&other) noexcept(
		nothrow_dealloc
		&& nothrow_destruct_value
		&& meta::is_nothrow_move_assignable_v<allocator_type>
	)
	{
		this->assign(std::move(other));
		return *this;
	}

	fixed_vector(std::initializer_list<value_type> init_list) noexcept(
		nothrow_alloc
		&& meta::is_nothrow_destructible_v<std::initializer_list<value_type>>
		&& nothrow_copy_value
	)
		: self_t()
	{
		this->initialize_from(array_view<value_type const>(&*init_list.begin(), &*init_list.end()));
	}

	explicit fixed_vector(size_type size) noexcept(
		nothrow_alloc       // size change
		&& meta::is_nothrow_default_constructible_v<value_type>
	)
		: self_t()
	{
		this->initialize_with(size);
	}

	fixed_vector(size_type size, value_type const &val) noexcept(
		nothrow_alloc       // size change
		&& nothrow_copy_value
	)
		: self_t()
	{
		this->initialize_with(size, val);
	}

	fixed_vector(const_iterator begin, const_iterator end) noexcept(
		nothrow_alloc
		&& nothrow_copy_value
	)
		: self_t()
	{
		this->initialize_from(array_view<value_type const>(begin.data(), end.data()));
	}

	template<typename U>
	fixed_vector(array_view<U> arr_view) noexcept(
		nothrow_alloc
		&& nothrow_construct_value<U const &>
	)
		: self_t()
	{
		this->initialize_from(arr_view);
	}

public:
	// ==== size queries ====
	auto max_size(void) noexcept(
		meta::is_nothrow_invocable_v<decltype(&allocator_type::max_size)>
	)
	{ return this->_allocator.max_size(); }

	auto size(void) const noexcept
	{ return static_cast<size_type>(this->_data_end - this->_data_begin); }

	auto empty(void) const noexcept
	{ return this->_data_begin == this->_data_end; }

	auto not_empty(void) const noexcept
	{ return !this->empty(); }

public:
	// ==== member access ====
	auto &operator [] (size_type n) noexcept
	{ return *(this->_data_begin + n); }

	auto const &operator [] (size_type n) const noexcept
	{ return *(this->_data_begin + n); }

	auto &front(void) noexcept
	{ return *this->_data_begin; }

	auto const &front(void) const noexcept
	{ return *this->_data_begin; }

	auto &back(void) noexcept
	{ return *(this->_data_end - 1); }

	auto const &back(void) const noexcept
	{ return *(this->_data_end - 1); }

	auto *data(void) noexcept
	{ return this->_data_begin; }

	auto const *data(void) const noexcept
	{ return this->_data_begin; }

	auto *data_end(void) noexcept
	{ return this->_data_end; }

	auto const *data_end(void) const noexcept
	{ return this->_data_end; }

	operator array_view<value_type> (void) noexcept
	{ return array_view<value_type>(this->_data_begin, this->_data_end); }

	operator array_view<value_type const> (void) const noexcept
	{ return array_view<value_type const>(this->_data_begin, this->_data_end); }

	array_view<value_type> as_array_view(void) noexcept
	{ return array_view<value_type>(this->_data_begin, this->_data_end); }

	array_view<value_type const> as_array_view(void) const noexcept
	{ return array_view<value_type const>(this->_data_begin, this->_data_end); }

	array_view<value_type> slice(std::size_t begin) noexcept
	{ return array_view<value_type>(std::min(this->_data_begin + begin, this->_data_end), this->_data_end); }

	array_view<value_type const> slice(std::size_t begin) const noexcept
	{ return array_view<value_type const>(std::min(this->_data_begin + begin, this->_data_end), this->_data_end); }

	array_view<value_type> slice(std::size_t begin, std::size_t end) noexcept
	{
		return array_view<value_type>(
			std::min(this->_data_begin + begin, this->_data_end),
			std::min(this->_data_begin + end, this->_data_end)
		);
	}

	array_view<value_type const> slice(std::size_t begin, std::size_t end) const noexcept
	{
		return array_view<value_type const>(
			std::min(this->_data_begin + begin, this->_data_end),
			std::min(this->_data_begin + end, this->_data_end)
		);
	}


public:
	// ==== iteration ====
	iterator begin(void) noexcept
	{ return this->_data_begin; }

	iterator end(void) noexcept
	{ return this->_data_end; }

	const_iterator begin(void) const noexcept
	{ return this->_data_begin; }

	const_iterator end(void) const noexcept
	{ return this->_data_end; }

	const_iterator cbegin(void) const noexcept
	{ return this->_data_begin; }

	const_iterator cend(void) const noexcept
	{ return this->_data_end; }


	reverse_iterator rbegin(void) noexcept
	{ return this->_data_end == nullptr ? nullptr : this->_data_end - 1; }

	reverse_iterator rend(void) noexcept
	{ return this->_data_begin == nullptr ? nullptr : this->_data_begin - 1; }

	const_reverse_iterator rbegin(void) const noexcept
	{ return this->_data_end == nullptr ? nullptr : this->_data_end - 1; }

	const_reverse_iterator rend(void) const noexcept
	{ return this->_data_begin == nullptr ? nullptr : this->_data_begin - 1; }

	const_reverse_iterator crbegin(void) const noexcept
	{ return this->_data_end - 1; }

	const_reverse_iterator crend(void) const noexcept
	{ return this->_data_begin - 1; }
};

template<typename T>
fixed_vector(array_view<T>) -> fixed_vector<T>;

template<typename T>
fixed_vector(array_view<T const>) -> fixed_vector<T>;

bz_end_namespace

#endif // _bz_fixed_vector_h__
