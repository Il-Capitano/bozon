#ifndef _bz_vector_h__
#define _bz_vector_h__

#include "core.h"

#include "allocator.h"
#include "meta.h"
#include "iterator.h"
#include "array_view.h"

bz_begin_namespace

template<typename T, typename = allocator<T>>
class vector;

template<typename T, typename Alloc>
class vector
{
	static_assert(
		!meta::is_reference<T>,
		"value_type of vector can't be a reference"
	);
	static_assert(
		!meta::is_void<T>,
		"value_type of vector can't be void"
	);
private:
	using self_t = vector<T, Alloc>;

	static constexpr bool nothrow_alloc   = meta::is_fn_noexcept_v<decltype(&Alloc::allocate)>;
	static constexpr bool nothrow_dealloc = meta::is_fn_noexcept_v<decltype(&Alloc::deallocate)>;

public:
	using value_type = T;
	using allocator_type = Alloc;
	using size_type = typename allocator_type::size_type;

private:
	value_type *_data_begin;
	value_type *_data_end;
	value_type *_alloc_end;

	allocator_type _allocator;

private:
	auto alloc_new(size_type n) noexcept(
		nothrow_alloc
	)
	{ return this->_allocator.allocate(n, this->_data_begin); }

	auto alloc_new(size_type n, value_type *p) noexcept(
		nothrow_alloc
	)
	{ return this->_allocator.allocate(n, p); }

	void set_to_null(void) noexcept
	{
		this->_data_begin = nullptr;
		this->_data_end   = nullptr;
		this->_alloc_end  = nullptr;
	}

	void no_null_clear(void) noexcept(
		nothrow_dealloc
		&& meta::is_nothrow_destructible_v<value_type>
	)
	{
		if (this->_data_begin != nullptr)
		{
			while (this->_data_end != this->_data_begin)
			{
				this->_allocator.destroy(--this->_data_end);
			}
			this->_allocator.deallocate(this->_data_begin, this->capacity());
		}
	}

	template<typename ...Args>
	void try_emplace(value_type *p, Args &&...args) noexcept(
		meta::is_nothrow_constructible_v<value_type, Args...>
	)
	{
		if constexpr (meta::is_nothrow_constructible_v<value_type, Args...>)
		{
			this->_allocator.construct(p, std::forward<Args>(args)...);
		}
		else
		{
			try
			{
				this->_allocator.construct(p, std::forward<Args>(args)...);
			}
			catch (...)
			{
				this->_allocator.destroy(p);
				throw;
			}
		}
	}

	template<bool assign_alloc = true>
	void no_clear_assign(self_t const &other) noexcept(
		nothrow_alloc
		&& (
			!assign_alloc
			|| meta::is_nothrow_copy_constructible_v<value_type>
		)
	)
	{
		auto other_cap = other.capacity();

		if constexpr (assign_alloc)
		{
			this->_allocator = other._allocator;
		}
		if (other_cap == 0)
		{
			this->set_to_null();
			return;
		}
		else
		{
			this->_data_begin = this->alloc_new(other_cap, nullptr);
			this->_data_end   = this->_data_begin;
			this->_alloc_end  = this->_data_begin + other_cap;
		}

		for (auto it = other._data_begin; it != other._data_end; ++it, ++this->_data_end)
		{
			this->try_emplace(this->_data_end, *it);
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

	void copy_to_new_alloc(value_type *new_data, size_type new_cap) noexcept(
		nothrow_dealloc  // no_null_clear
		&& meta::is_nothrow_destructible_v<value_type> // no_null_clear
		&& (  // data copying
			meta::is_nothrow_move_constructible_v<value_type>
			|| meta::is_nothrow_copy_constructible_v<value_type>
		)
	)
	{
		auto old_it  = this->_data_begin;
		auto old_end = this->_data_end;
		auto new_it  = new_data;

		if constexpr (
			meta::is_nothrow_move_constructible_v<value_type>
			|| meta::is_nothrow_copy_constructible_v<value_type>
		)
		{
			for (; old_it != old_end; ++new_it, ++old_it)
			{
				if constexpr (meta::is_nothrow_move_constructible_v<value_type>)
				{
					this->try_emplace(new_it, std::move(*old_it));
				}
				else
				{
					this->try_emplace(new_it, *old_it);
				}
			}

			this->no_null_clear();
			this->_data_begin = new_data;
			this->_data_end   = new_it;
			this->_alloc_end  = new_data + new_cap;
		}
		else
		{
			try
			{
				for (; old_it != old_end; ++new_it, ++old_it)
				{
					if constexpr (!meta::is_copy_constructible_v<value_type>)
					{
						this->try_emplace(new_it, std::move(*old_it));
					}
					else
					{
						this->try_emplace(new_it, *old_it);
					}
				}
				this->no_null_clear();
				this->_data_begin = new_data;
				this->_data_end   = new_it;
				this->_alloc_end  = new_data + new_cap;
			}
			catch (...)
			{
				this->no_null_clear();
				this->_data_begin = new_data;
				this->_data_end   = new_it;
				this->_alloc_end  = new_data + new_cap;
				throw;
			}
		}
	}

public:
	void clear(void) noexcept(
		nothrow_dealloc
		&& meta::is_nothrow_destructible_v<value_type>
	)
	{
		this->no_null_clear();
		this->set_to_null();
	}

	void assign(self_t const &other) noexcept(
		nothrow_alloc
		&& nothrow_dealloc
		&& meta::is_nothrow_destructible_v<value_type>
		&& meta::is_nothrow_copy_constructible_v<value_type>
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
		&& meta::is_nothrow_destructible_v<value_type>
		&& meta::is_nothrow_move_assignable_v<allocator_type>
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
	vector(void) noexcept(
		meta::is_nothrow_default_constructible_v<allocator_type>
	)
		: _data_begin(nullptr),
		  _data_end  (nullptr),
		  _alloc_end (nullptr),
		  _allocator ()
	{}

	vector(self_t const &other) noexcept(
		meta::is_nothrow_copy_constructible_v<allocator_type>
	)
		: _allocator(other._allocator)
	{ this->no_clear_assign<false>(other); }

	vector(self_t &&other) noexcept(
		meta::is_nothrow_move_constructible_v<allocator_type>
	)
		: _data_begin(other._data_begin),
		  _data_end  (other._data_end),
		  _alloc_end (other._alloc_end),
		  _allocator (std::move(other._allocator))
	{ other.set_to_null(); }

	~vector(void) noexcept(
		nothrow_dealloc
		&& meta::is_nothrow_destructible_v<value_type>
	)
	{ this->no_null_clear(); }

	self_t &operator = (self_t const &other) noexcept(
		nothrow_alloc
		&& nothrow_dealloc
		&& meta::is_nothrow_destructible_v<value_type>
		&& meta::is_nothrow_copy_constructible_v<value_type>
	)
	{
		this->assign(other);
		return *this;
	}

	self_t &operator = (self_t &&other) noexcept(
		nothrow_dealloc
		&& meta::is_nothrow_destructible_v<value_type>
		&& meta::is_nothrow_move_assignable_v<allocator_type>
	)
	{
		this->assign(std::move(other));
		return *this;
	}

	vector(std::initializer_list<value_type> init_list) noexcept(
		nothrow_alloc
		&& meta::is_nothrow_destructible_v<std::initializer_list<value_type>>
		&& (
			meta::is_nothrow_move_constructible_v<value_type>
			|| meta::is_nothrow_copy_constructible_v<value_type>
		)
	)
		: self_t()
	{
		this->reserve(init_list.size());
		auto list_it  = init_list.begin();
		auto list_end = init_list.end();
		for (; list_it != list_end; ++this->_data_end, ++list_it)
		{
			if constexpr (meta::is_nothrow_move_constructible_v<value_type>)
			{
				this->try_emplace(this->_data_end, std::move(*list_it));
			}
			else
			{
				this->try_emplace(this->_data_end, *list_it);
			}
		}
	}

	vector(size_type size) noexcept(
		nothrow_alloc       // size change
		&& meta::is_nothrow_default_constructible_v<value_type>  // new_size > current_size
	)
		: self_t()
	{
		this->resize(size);
	}

	vector(size_type size, value_type const &val) noexcept(
		nothrow_alloc       // size change
		&& meta::is_nothrow_copy_constructible_v<value_type>     // new_size > current_size
	)
		: self_t()
	{
		this->resize(size, val);
	}

public:
	// ==== size modifiers ====
	void resize(size_type new_size) noexcept(
		nothrow_alloc       // size change
		&& nothrow_dealloc  // size change
		&& meta::is_nothrow_destructible_v<value_type>           // new_size < current_size
		&& meta::is_nothrow_copy_constructible_v<value_type>     // shrink_to_fit
		&& meta::is_nothrow_default_constructible_v<value_type>  // new_size > current_size
	)
	{
		auto current_size = this->size();
		if (new_size == current_size)
		{
			return;
		}
		else if (new_size < current_size)
		{
			while (current_size != new_size)
			{
				--current_size;
				--this->_data_end;
				this->_allocator.destroy(this->_data_end);
			}
			this->shrink_to_fit();
		}
		else
		{
			this->reserve(new_size);
			for (; current_size != new_size; ++this->_data_end, ++current_size)
			{
				this->try_emplace(this->_data_end);
			}
		}
	}

	void resize(size_type new_size, value_type const &val) noexcept(
		nothrow_alloc       // size change
		&& nothrow_dealloc  // size change
		&& meta::is_nothrow_destructible_v<value_type>           // new_size < current_size
		&& meta::is_nothrow_copy_constructible_v<value_type>     // new_size > current_size, shrink_to_fit
	)
	{
		auto current_size = this->size();
		if (new_size == current_size)
		{
			return;
		}
		else if (new_size < current_size)
		{
			while (current_size != new_size)
			{
				--current_size;
				--this->_data_end;
				this->_allocator.destroy(this->_data_end);
			}
			this->shrink_to_fit();
		}
		else
		{
			this->reserve(new_size);
			for (; current_size != new_size; ++this->_data_end, ++current_size)
			{
				this->try_emplace(this->_data_end, val);
			}
		}
	}

	void reserve(size_type reserve_size) noexcept(
		nothrow_alloc  // alloc_new
		&& nothrow_dealloc  // rest is for data copying
		&& meta::is_nothrow_destructible_v<value_type>
		&& (
			meta::is_nothrow_move_constructible_v<value_type>
			|| meta::is_nothrow_copy_constructible_v<value_type>
		)
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
		nothrow_alloc  // alloc_new
		&& nothrow_dealloc  // rest is for data copying
		&& meta::is_nothrow_destructible_v<value_type>
		&& (
			meta::is_nothrow_move_constructible_v<value_type>
			|| meta::is_nothrow_copy_constructible_v<value_type>
		)
	)
	{
		if (this->_data_begin == nullptr)
		{
			return;
		}

		auto current_size = this->size();
		auto current_cap  = this->capacity();

		if (current_size == 0)
		{
			this->clear();
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


	// ==== size queries ====
	auto max_size(void) noexcept(
		meta::is_nothrow_invocable_v<allocator_type::max_size>
	)
	{ return this->_allocator.max_size(); }

	auto size(void) const noexcept
	{ return static_cast<size_type>(this->_data_end - this->_data_begin); }

	auto capacity(void) const noexcept
	{ return static_cast<size_type>(this->_alloc_end - this->_data_begin); }

	auto empty(void) const noexcept
	{ return this->_data_begin == this->_data_end; }


public:
	auto push_back(value_type const &val) noexcept(
		nothrow_alloc  // reserve
		&& nothrow_dealloc
		&& meta::is_nothrow_destructible_v<value_type>
		&& (
			meta::is_nothrow_move_constructible_v<value_type>
			|| meta::is_nothrow_copy_constructible_v<value_type>
		)
		&& meta::is_nothrow_copy_constructible_v<value_type>  // val construction
	) -> value_type &
	{
		if (this->_data_end == this->_alloc_end)
		{
			this->reserve(this->capacity() + 1);
		}
		this->try_emplace(this->_data_end, val);
		auto end = this->_data_end;
		++this->_data_end;
		return *end;
	}

	auto push_back(value_type &&val) noexcept(
		nothrow_alloc  // reserve
		&& nothrow_dealloc
		&& meta::is_nothrow_destructible_v<value_type>
		&& (
			meta::is_nothrow_move_constructible_v<value_type>
			|| meta::is_nothrow_copy_constructible_v<value_type>
		)
	) -> value_type &
	{
		if (this->_data_end == this->_alloc_end)
		{
			this->reserve(this->capacity() + 1);
		}
		if constexpr (meta::is_nothrow_move_constructible_v<value_type>)
		{
			this->try_emplace(this->_data_end, std::move(val));
		}
		else
		{
			this->try_emplace(this->_data_end, val);
		}
		auto end = this->_data_end;
		++this->_data_end;
		return *end;
	}

	template<typename ...Args>
	auto emplace_back(Args &&...args) noexcept(
		nothrow_alloc  // reserve
		&& nothrow_dealloc
		&& meta::is_nothrow_destructible_v<value_type>
		&& (
			meta::is_nothrow_move_constructible_v<value_type>
			|| meta::is_nothrow_copy_constructible_v<value_type>
		)
		&& meta::is_nothrow_constructible_v<value_type, Args...>
	) -> value_type &
	{
		if (this->_data_end == this->_alloc_end)
		{
			this->reserve(this->capacity() + 1);
		}
		this->try_emplace(this->_data_end, std::forward<Args>(args)...);
		auto end = this->_data_end;
		++this->_data_end;
		return *end;
	}

	void pop_back(void) noexcept(
		meta::is_nothrow_destructible_v<value_type>
	)
	{
		if (this->_data_end == this->_data_begin)
		{
			return;
		}
		this->_allocator.destroy(--this->_data_end);
	}


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

	operator array_view<value_type> (void) noexcept
	{ return array_view<value_type>(this->_data_begin, this->_data_end); }

	operator array_view<value_type const> (void) const noexcept
	{ return array_view<value_type const>(this->_data_begin, this->_data_end); }


public:
	// ==== iteration ====
	using       iterator = ::bz::random_access_iterator<      value_type>;
	using const_iterator = ::bz::random_access_iterator<const value_type>;

	using       reverse_iterator = ::bz::reverse_iterator<      iterator>;
	using const_reverse_iterator = ::bz::reverse_iterator<const_iterator>;


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
	{ return this->_data_end - 1; }

	reverse_iterator rend(void) noexcept
	{ return this->_data_begin - 1; }

	const_reverse_iterator rbegin(void) const noexcept
	{ return this->_data_end - 1; }

	const_reverse_iterator rend(void) const noexcept
	{ return this->_data_begin - 1; }

	const_reverse_iterator crbegin(void) const noexcept
	{ return this->_data_end - 1; }

	const_reverse_iterator crend(void) const noexcept
	{ return this->_data_begin - 1; }
};

bz_end_namespace

#endif // _bz_vector_h__
