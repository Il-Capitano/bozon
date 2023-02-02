#ifndef _bz_vector_h__
#define _bz_vector_h__

#include "core.h"

#include "allocator.h"
#include "meta.h"
#include "iterator.h"
#include "array_view.h"
#include "ranges.h"
#include "fixed_vector.h"

bz_begin_namespace

template<typename T, typename = allocator<T>>
class vector;

template<typename T, typename Alloc>
class vector : public collection_base<vector<T, Alloc>>
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
	value_type *_alloc_end;

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
		this->_alloc_end  = nullptr;
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
			this->_allocator.deallocate(this->_data_begin, this->capacity());
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

	template<bool assign_alloc = true>
	void no_clear_assign(self_t const &other) noexcept(
		nothrow_alloc
		&& (!assign_alloc || trivial_alloc || meta::is_nothrow_copy_assignable_v<allocator_type>)
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
			this->_data_begin = this->alloc_new(other_cap);
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
		|| (trivial_alloc || meta::is_nothrow_move_assignable_v<allocator_type>)
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
		&& nothrow_destruct_value  // no_null_clear
		&& (  // data copying
			nothrow_move_value || nothrow_copy_value
		)
	)
	{
		auto old_it  = this->_data_begin;
		auto old_end = this->_data_end;
		auto new_it  = new_data;

		if constexpr (nothrow_move_value)
		{
			// if it's nothrow movable immediately destruct the moved-from object
			// this may help the compiler in optimizing trivially relocatable type moving
			for (; old_it != old_end; ++new_it, ++old_it)
			{
				new(new_it) value_type(std::move(*old_it));
				old_it->~value_type();
			}

			this->_allocator.deallocate(this->_data_begin, this->capacity());
			this->_data_begin = new_data;
			this->_data_end   = new_it;
			this->_alloc_end  = new_data + new_cap;
		}
		else if constexpr (nothrow_move_value || nothrow_copy_value)
		{
			for (; old_it != old_end; ++new_it, ++old_it)
			{
				if constexpr (nothrow_move_value)
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
			bz_try
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
			bz_catch_all
			{
				this->no_null_clear();
				this->_data_begin = new_data;
				this->_data_end   = new_it;
				this->_alloc_end  = new_data + new_cap;
				bz_rethrow;
			}
		}
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
		this->no_clear_assign(other);
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
		this->no_clear_assign(std::move(other));
	}

public:
	vector(void) noexcept(
		trivial_alloc || meta::is_nothrow_default_constructible_v<allocator_type>
	)
		: _data_begin(nullptr),
		  _data_end  (nullptr),
		  _alloc_end (nullptr),
		  _allocator ()
	{}

	vector(self_t const &other) noexcept(
		trivial_alloc || meta::is_nothrow_copy_constructible_v<allocator_type>
	)
		: _allocator(other._allocator)
	{ this->no_clear_assign<false>(other); }

	vector(self_t &&other) noexcept(
		trivial_alloc || meta::is_nothrow_move_constructible_v<allocator_type>
	)
		: _data_begin(other._data_begin),
		  _data_end  (other._data_end),
		  _alloc_end (other._alloc_end),
		  _allocator (std::move(other._allocator))
	{ other.set_to_null(); }

	~vector(void) noexcept(
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

	vector(std::initializer_list<value_type> init_list) noexcept(
		nothrow_alloc
		&& meta::is_nothrow_destructible_v<std::initializer_list<value_type>>
		&& nothrow_copy_value
	)
		: self_t()
	{
		this->reserve(init_list.size());
		auto list_it  = init_list.begin();
		auto list_end = init_list.end();
		for (; list_it != list_end; ++this->_data_end, ++list_it)
		{
			this->try_emplace(this->_data_end, *list_it);
		}
	}

	explicit vector(size_type size) noexcept(
		nothrow_alloc       // size change
		&& meta::is_nothrow_default_constructible_v<value_type>
	)
		: self_t()
	{
		this->resize(size);
	}

	vector(size_type size, value_type const &val) noexcept(
		nothrow_alloc       // size change
		&& nothrow_copy_value
	)
		: self_t()
	{
		this->resize(size, val);
	}

	vector(const_iterator begin, const_iterator end) noexcept(
		nothrow_alloc
		&& nothrow_copy_value
	)
		: self_t()
	{
		this->reserve(end - begin);
		auto it = begin;
		auto insert_it = this->_data_begin;
		for (; it != end; ++it, ++insert_it)
		{
			new (insert_it) value_type(*it);
		}
		this->_data_end = insert_it;
	}

	template<typename U>
	vector(array_view<U> arr_view) noexcept(
		nothrow_alloc
		&& nothrow_construct_value<U const &>
	)
		: self_t()
	{
		this->reserve(arr_view.size());
		auto it = arr_view.begin();
		auto const end = arr_view.end();
		auto insert_it = this->_data_begin;
		for (; it != end; ++it, ++insert_it)
		{
			new (insert_it) value_type(*it);
		}
		this->_data_end = insert_it;
	}

public:
	// ==== size modifiers ====
	void resize(size_type new_size) noexcept(
		nothrow_alloc       // size change
		&& nothrow_dealloc  // size change
		&& nothrow_destruct_value                                // new_size < current_size
		&& (nothrow_move_value || nothrow_copy_value)            // shrink_to_fit
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
				this->_data_end->~value_type();
			}
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

	template<typename U>
	void resize(size_type new_size, U const &val) noexcept(
		nothrow_alloc       // size change
		&& nothrow_dealloc  // size change
		&& nothrow_destruct_value             // new_size < current_size
		&& nothrow_construct_value<U const &> // new_size > current_size, shrink_to_fit
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
				this->_data_end->~value_type();
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
		&& nothrow_destruct_value
		&& (nothrow_move_value || nothrow_copy_value)
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
		&& nothrow_destruct_value
		&& (nothrow_move_value || nothrow_copy_value)
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
		meta::is_nothrow_invocable_v<decltype(&allocator_type::max_size)>
	)
	{ return this->_allocator.max_size(); }

	auto size(void) const noexcept
	{ return static_cast<size_type>(this->_data_end - this->_data_begin); }

	auto capacity(void) const noexcept
	{ return static_cast<size_type>(this->_alloc_end - this->_data_begin); }

	auto empty(void) const noexcept
	{ return this->_data_begin == this->_data_end; }

	auto not_empty(void) const noexcept
	{ return !this->empty(); }


public:
	auto push_back(value_type const &val) noexcept(
		nothrow_alloc  // reserve
		&& nothrow_dealloc
		&& nothrow_destruct_value
		&& (nothrow_move_value || nothrow_copy_value)
		&& nothrow_copy_value  // val construction
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
		&& nothrow_destruct_value
		&& (nothrow_move_value || nothrow_copy_value)
		&& nothrow_move_value  // val construction
	) -> value_type &
	{
		if (this->_data_end == this->_alloc_end)
		{
			this->reserve(this->capacity() + 1);
		}
		if constexpr (nothrow_move_value)
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

	auto push_front(value_type const &val) noexcept(
		nothrow_alloc  // reserve
		&& nothrow_dealloc
		&& nothrow_destruct_value
		&& (nothrow_move_value || nothrow_copy_value)
		&& nothrow_copy_value  // val construction
	) -> value_type &
	{
		if (this->_data_end == this->_alloc_end)
		{
			this->reserve(this->capacity() + 1);
		}
		// shift every element by one
		if constexpr (meta::is_nothrow_move_constructible_v<value_type>)
		{
			auto move_from_it = this->_data_end;
			auto end_it = move_from_it;
			auto const begin = this->_data_begin;
			for (; move_from_it != begin; --end_it)
			{
				--move_from_it;
				this->try_emplace(end_it, std::move(*move_from_it));
				move_from_it->~value_type();
			}
			this->try_emplace(begin, val);
			++this->_data_end;
			return *begin;
		}
		else
		{
			auto copy_from_it = this->_data_end;
			auto end_it = copy_from_it;
			auto const begin = this->_data_begin;
			if (copy_from_it != begin)
			{
				--copy_from_it;
				this->try_emplace(end_it, *copy_from_it);
				--end_it;
				for (; copy_from_it != begin; --end_it)
				{
					--copy_from_it;
					*end_it = *copy_from_it;
				}
			}
			*begin = val;
			++this->_data_end;
			return *begin;
		}
	}

	auto push_front(value_type &&val) noexcept(
		nothrow_alloc  // reserve
		&& nothrow_dealloc
		&& nothrow_destruct_value
		&& (nothrow_move_value || nothrow_copy_value)
		&& nothrow_move_value  // val construction
	) -> value_type &
	{
		if (this->_data_end == this->_alloc_end)
		{
			this->reserve(this->capacity() + 1);
		}
		// shift every element by one
		if constexpr (nothrow_move_value)
		{
			auto move_from_it = this->_data_end;
			auto end_it = move_from_it;
			auto const begin = this->_data_begin;
			for (; move_from_it != begin; --end_it)
			{
				--move_from_it;
				this->try_emplace(end_it, std::move(*move_from_it));
				move_from_it->~value_type();
			}
			this->try_emplace(begin, std::move(val));
			++this->_data_end;
			return *begin;
		}
		else
		{
			auto copy_from_it = this->_data_end;
			auto end_it = copy_from_it;
			auto const begin = this->_data_begin;
			if (copy_from_it != begin)
			{
				--copy_from_it;
				this->try_emplace(end_it, *copy_from_it);
				--end_it;
				for (; copy_from_it != begin; --end_it)
				{
					--copy_from_it;
					*end_it = *copy_from_it;
				}
			}
			*begin = std::move(val);
			++this->_data_end;
			return *begin;
		}
	}

	template<typename ...Args>
	auto emplace_back(Args &&...args) noexcept(
		nothrow_alloc  // reserve
		&& nothrow_dealloc
		&& nothrow_destruct_value
		&& (nothrow_move_value || nothrow_copy_value)
		&& nothrow_construct_value<Args...>
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

	template<typename ...Args>
	auto emplace_front(Args &&...args) noexcept(
		nothrow_alloc  // reserve
		&& nothrow_dealloc
		&& nothrow_destruct_value
		&& (nothrow_move_value || nothrow_copy_value)
		&& nothrow_construct_value<Args...>  // val construction
	) -> value_type &
	{
		if (this->_data_end == this->_alloc_end)
		{
			this->reserve(this->capacity() + 1);
		}
		// shift every element by one
		if constexpr (nothrow_move_value)
		{
			auto move_from_it = this->_data_end;
			auto end_it = move_from_it;
			auto const begin = this->_data_begin;
			for (; move_from_it != begin; --end_it)
			{
				--move_from_it;
				this->try_emplace(end_it, std::move(*move_from_it));
				move_from_it->~value_type();
			}
			this->try_emplace(begin, std::forward<Args>(args)...);
			++this->_data_end;
			return *begin;
		}
		else
		{
			auto copy_from_it = this->_data_end;
			auto end_it = copy_from_it;
			auto const begin = this->_data_begin;
			if (copy_from_it != begin)
			{
				--copy_from_it;
				this->try_emplace(end_it, *copy_from_it);
				--end_it;
				for (; copy_from_it != begin; --end_it)
				{
					--copy_from_it;
					*end_it = *copy_from_it;
				}
			}
			begin->~value_type();
			this->try_emplace(begin, std::forward<Args>(args)...);
			++this->_data_end;
			return *begin;
		}
	}

	void pop_back(void) noexcept(nothrow_destruct_value)
	{
		if (this->_data_end == this->_data_begin)
		{
			return;
		}
		--this->_data_end;
		this->_data_end->~value_type();
	}

	void pop_front(void) noexcept(
		nothrow_destruct_value
		&& (nothrow_move_value || nothrow_copy_value)
	)
	{
		if (this->_data_end == this->_data_begin)
		{
			return;
		}

		auto it = this->_data_begin;
		auto trace = it;
		auto const end = this->_data_end;
		++it;
		for (; it != end; ++it, ++trace)
		{
			trace->~value_type();
			if constexpr (nothrow_move_value)
			{
				this->try_emplace(trace, std::move(*it));
			}
			else
			{
				this->try_emplace(trace, *it);
			}
		}
		trace->~value_type();
		--this->_data_end;
	}

	fixed_vector<value_type, allocator_type> release_as_fixed_vector(void) noexcept(
		nothrow_alloc
		&& nothrow_dealloc
		&& nothrow_destruct_value
		&& (nothrow_move_value || nothrow_copy_value)
	)
	{
		fixed_vector<value_type, allocator_type> result;

		auto const size = this->size();
		if (size == 0)
		{
			return result;
		}

		auto const data_begin = result.alloc_new(size);
		auto const data_end = data_begin + size;

		auto it = data_begin;
		bz_try
		{
			auto *self_it = this->data();
			for (; it != data_end; ++it, ++self_it)
			{
				if constexpr (nothrow_move_value)
				{
					result.try_emplace(it, std::move(*self_it));
				}
				else
				{
					result.try_emplace(it, *self_it);
				}
				self_it->~value_type();
			}
		}
		bz_catch_all
		{
			while (it != data_begin)
			{
				--it;
				it->~value_type();
			}
			result._allocator.deallocate(data_begin, size);
			bz_rethrow;
		}

		result._data_begin = data_begin;
		result._data_end = data_end;

		this->_allocator.deallocate(this->_data_begin, this->capacity());
		this->set_to_null();

		return result;
	}

private:
	void erase(value_type const *it_) noexcept(
		nothrow_destruct_value
		&& (nothrow_move_value || nothrow_copy_value)
	)
	{
		auto it = const_cast<value_type *>(it_);
		auto trace = it;
		auto const end = this->_data_end;
		++it;
		for (; it != end; ++it, ++trace)
		{
			trace->~value_type();
			if constexpr (nothrow_move_value)
			{
				this->try_emplace(trace, std::move(*it));
			}
			else
			{
				this->try_emplace(trace, *it);
			}
		}
		trace->~value_type();
		--this->_data_end;
	}

public:
	const_iterator erase(const_iterator it) noexcept(
		nothrow_destruct_value
		&& (nothrow_move_value || nothrow_copy_value)
	)
	{
		this->erase(it.data());
		return it;
	}

	iterator erase(iterator it) noexcept(
		nothrow_destruct_value
		&& (nothrow_move_value || nothrow_copy_value)
	)
	{
		this->erase(it.data());
		return it;
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

namespace internal
{

template<typename Range>
auto range_base_collect<Range>::collect(void) const
{ return this->template collect<vector>(); }

template<typename Range>
auto range_base_collect<Range>::collect(std::size_t reserve) const
{ return this->template collect<vector>(reserve); }

} // namespace internal

template<typename T, typename Alloc1, typename Alloc2>
bool operator == (vector<T, Alloc1> const &lhs, vector<T, Alloc2> const &rhs)
{ return lhs.as_array_view() == rhs.as_array_view(); }

template<typename T, typename Alloc1, typename Alloc2>
bool operator != (vector<T, Alloc1> const &lhs, vector<T, Alloc2> const &rhs)
{ return lhs.as_array_view() != rhs.as_array_view(); }

bz_end_namespace

#endif // _bz_vector_h__
