#ifndef _bz_result_h_
#define _bz_result_h_

#include "core.h"

#include "meta.h"

bz_begin_namespace

template<typename Res, typename Err>
class result
{
	static_assert(!meta::is_void<Res>);
	static_assert(!meta::is_reference<Res>);
	static_assert(!meta::is_const<Res>);

	static_assert(!meta::is_reference<Err>);
	static_assert(!meta::is_const<Err>);

	static_assert(!meta::is_same<Res, Err>);

private:
	using self_t = result<Res, Err>;
public:
	using result_type = Res;
	using error_type = Err;

private:
	alignas(meta::lcm_index<alignof(result_type), alignof(error_type)>)
	uint8_t _data[meta::max_index<sizeof (result_type), sizeof (error_type)>];
	bool _has_error;

	result_type &_get_result(void) noexcept
	{
		return *reinterpret_cast<result_type *>(this->_data);
	}

	error_type &_get_error(void) noexcept
	{
		return *reinterpret_cast<error_type *>(this->_data);
	}

	result_type const &_get_result(void) const noexcept
	{
		return *reinterpret_cast<result_type const *>(this->_data);
	}

	error_type const &_get_error(void) const noexcept
	{
		return *reinterpret_cast<error_type const *>(this->_data);
	}

	void _clear(void) const noexcept(
		meta::is_nothrow_destructible_v<result_type>
		&& meta::is_nothrow_destructible_v<error_type>
	)
	{
		if (this->_has_error)
		{
			this->_get_error().~error_type();
		}
		else
		{
			this->_get_result().~result_type();
		}
	}

public:
	result(result_type const &res) noexcept(
		meta::is_nothrow_copy_constructible_v<result_type>
	)
		: _has_error(false)
	{
		new(this->_data) result_type(res);
	}
	result(result_type &&res) noexcept(
		meta::is_nothrow_move_constructible_v<result_type>
	)
		: _has_error(false)
	{
		new(this->_data) result_type(std::move(res));
	}

	result(error_type const &err) noexcept(
		meta::is_nothrow_copy_constructible_v<error_type>
	)
		: _has_error(true)
	{
		new(this->_data) error_type(err);
	}

	result(error_type &&err) noexcept(
		meta::is_nothrow_move_constructible_v<error_type>
	)
		: _has_error(true)
	{
		new(this->_data) error_type(std::move(err));
	}

	result(self_t const &other) noexcept(
		meta::is_nothrow_copy_constructible_v<result_type>
		&& meta::is_nothrow_copy_constructible_v<error_type>
	)
		: _has_error(other._has_error)
	{
		if (this->_has_error)
		{
			new(this->_data) error_type(other._get_error());
		}
		else
		{
			new(this->_data) result_type(other._get_result());
		}
	}

	result(self_t &&other) noexcept(
		meta::is_nothrow_move_constructible_v<result_type>
		&& meta::is_nothrow_move_constructible_v<error_type>
	)
		: _has_error(other._has_error)
	{
		if (this->_has_error)
		{
			new(this->_data) error_type(std::move(other._get_error()));
		}
		else
		{
			new(this->_data) result_type(std::move(other._get_result()));
		}
	}

	self_t &operator = (self_t const &lhs) noexcept(
		meta::is_nothrow_copy_assignable_v<result_type>
		&& meta::is_nothrow_copy_assignable_v<error_type>
	)
	{
		this->_clear();
		this->_has_error = lhs._has_error;
		if (this->_has_error)
		{
			new(this->_data) error_type(lhs._get_error());
		}
		else
		{
			new(this->_data) result_type(lhs._get_result());
		}
		return *this;
	}

	self_t &operator = (self_t &&lhs) noexcept(
		meta::is_nothrow_move_assignable_v<result_type>
		&& meta::is_nothrow_move_assignable_v<error_type>
	)
	{
		this->_clear();
		this->_has_error = lhs._has_error;
		if (this->_has_error)
		{
			new(this->_data) error_type(std::move(lhs._get_error()));
		}
		else
		{
			new(this->_data) result_type(std::move(lhs._get_result()));
		}
		return *this;
	}

	~result(void) noexcept(
		meta::is_nothrow_destructible_v<result_type>
		&& meta::is_nothrow_destructible_v<error_type>
	)
	{
		if (this->_has_error)
		{
			this->_get_error().~error_type();
		}
		else
		{
			this->_get_result().~result_type();
		}
	}

public:
	bool has_error(void) const noexcept
	{ return this->_has_error; }


	result_type &get_result(void) & noexcept
	{
		bz_assert(!this->_has_error);
		return this->_get_result();
	}

	result_type const &get_result(void) const & noexcept
	{
		bz_assert(!this->_has_error);
		return this->_get_result();
	}

	result_type &&get_result(void) && noexcept
	{
		bz_assert(!this->_has_error);
		return std::move(this->_get_result());
	}


	error_type &get_error(void) & noexcept
	{
		bz_assert(this->_has_error);
		return this->_get_error();
	}

	error_type const &get_error(void) const & noexcept
	{
		bz_assert(this->_has_error);
		return this->_get_error();
	}

	error_type &&get_error(void) && noexcept
	{
		bz_assert(this->_has_error);
		return std::move(this->_get_error());
	}
};

bz_end_namespace

#endif // _bz_result_h_
