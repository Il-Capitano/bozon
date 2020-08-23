#ifndef _bz_optional_h__
#define _bz_optional_h__

#include "core.h"
#include "meta.h"

bz_begin_namespace

namespace internal
{

struct optional_trivial
{
};

template<typename Optional, typename Element>
struct optional_destructor_base
{
	~optional_destructor_base(void) noexcept
	{
		auto &This = *static_cast<Optional *>(this);
		if (This._has_value)
		{
			This.destruct();
		}
	}
};

} // namespace internal

template<typename T>
class optional :
	public meta::conditional<
		meta::is_nothrow_destructible_v<T>,
		internal::optional_trivial,
		internal::optional_destructor_base<optional<T>, T>
	>
{
	static_assert(!meta::is_void<T>,      "bz::optional element type must not be void");
	static_assert(!meta::is_reference<T>, "bz::optional element type must not be a reference");
private:
	using self_t = optional<T>;
public:
	using element_type = T;

private:
	alignas(alignof(T)) uint8_t _data[sizeof(T)];
	bool _has_value;

private:
	auto &no_check_get(void) noexcept
	{ return *reinterpret_cast<element_type *>(this->_data); }

	auto &no_check_get(void) const noexcept
	{ return *reinterpret_cast<element_type const *>(this->_data); }

	template<typename ...Args>
	void try_emplace(Args &&...args) noexcept(
		meta::is_nothrow_constructible_v<element_type, Args...>
	)
	{
		if constexpr (meta::is_nothrow_constructible_v<element_type, Args...>)
		{
			new(this->_data) element_type(std::forward<Args>(args)...);
			this->_has_value = true;
		}
		else
		{
			try
			{
				new(this->_data) element_type(std::forward<Args>(args)...);
				this->_has_value = true;
			}
			catch (...)
			{
				this->no_check_get().~element_type();
				this->_has_value = false;
				throw;
			}
		}
	}

	void destruct(void) noexcept(
		meta::is_nothrow_destructible_v<element_type>
	)
	{
		this->no_check_get().~element_type();
	}

public:
	void clear(void) noexcept(
		meta::is_nothrow_destructible_v<element_type>
	)
	{
		if (this->_has_value)
		{
			this->destruct();
			this->_has_value = false;
		}
	}

	template<typename ...Args>
	void emplace(Args &&...args) noexcept(
		meta::is_nothrow_constructible_v<element_type>
		&& meta::is_nothrow_destructible_v<element_type>
	)
	{
		if (this->_has_value)
		{
			this->destruct();
		}
		this->try_emplace(std::forward<Args>(args)...);
	}

public:
	optional(void) noexcept
		: _has_value(false)
	{}

	optional(self_t const &other) noexcept(
		meta::is_nothrow_copy_constructible_v<element_type>
	)
	{
		if (other._has_value)
		{
			this->try_emplace(other.no_check_get());
		}
		else
		{
			this->_has_value = false;
		}
	}

	optional(self_t &&other) noexcept(
		meta::is_nothrow_move_constructible_v<element_type>
	)
	{
		if (other._has_value)
		{
			this->try_emplace(std::move(other.no_check_get()));
		}
		else
		{
			this->_has_value = false;
		}
	}

	self_t &operator = (self_t const &other) noexcept(
		meta::is_nothrow_copy_constructible_v<element_type>
		&& meta::is_nothrow_copy_assignable_v<element_type>
		&& meta::is_nothrow_destructible_v<element_type>
	)
	{
		if (other._has_value)
		{
			if (this->_has_value)
			{
				this->no_check_get() = other.no_check_get();
			}
			else
			{
				this->try_emplace(other.no_check_get());
			}
		}
		else
		{
			if (this->_has_value)
			{
				this->destruct();
			}
		}

		return *this;
	}

	self_t &operator = (self_t &&other) noexcept(
		meta::is_nothrow_move_constructible_v<element_type>
		&& meta::is_nothrow_move_assignable_v<element_type>
		&& meta::is_nothrow_destructible_v<element_type>
	)
	{
		if (other._has_value)
		{
			if (this->_has_value)
			{
				this->no_check_get() = std::move(other.no_check_get());
			}
			else
			{
				this->try_emplace(std::move(other.no_check_get()));
			}
		}
		else
		{
			if (this->_has_value)
			{
				this->destruct();
			}
		}

		return *this;
	}

	template<
		typename ...Args,
		typename = meta::enable_if<
			sizeof...(Args) != 1
			|| (!meta::is_same<
				meta::remove_cv_reference<Args>,
				self_t
			> && ...)
		>
	>
	optional(Args &&...args) noexcept(
		meta::is_nothrow_constructible_v<element_type, Args...>
	)
	{
		this->try_emplace(std::forward<Args>(args)...);
	}

public:
	bool has_value(void) const noexcept
	{ return this->_has_value; }

	auto &get(void) noexcept
	{
		bz_assert(this->_has_value, "bz::optional::get() called, but there's no value to return");
		return this->no_check_get();
	}

	auto &get(void) const noexcept
	{
		bz_assert(this->_has_value, "bz::optional::get() called, but there's no value to return");
		return this->no_check_get();
	}

	auto &operator * (void) noexcept
	{ return this->get(); }

	auto &operator * (void) const noexcept
	{ return this->get(); }

	auto operator -> (void) noexcept
	{ return &this->get(); }

	auto operator -> (void) const noexcept
	{ return &this->get(); }
};

bz_end_namespace

#endif // _bz_optional_h__
