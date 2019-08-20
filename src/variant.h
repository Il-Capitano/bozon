#ifndef VARIANT_H
#define VARIANT_H

#include <cstdint>
#include <utility>
#include <iostream>
#include <type_traits>

#include "my_assert.h"

namespace meta
{

#define _sc static constexpr


// checks if the argument types are the same
template<typename ...>
struct is_same;

template<typename ...Ts>
constexpr bool is_same_v = is_same<Ts...>::value;

template<typename T>
struct is_same<T, T>
{
	_sc bool value = true;
};

template<typename T, typename U, typename ...Ts>
struct is_same<T, U, Ts...>
{
	_sc bool value = false;
};

template<typename T, typename ...Ts>
struct is_same<T, T, Ts...>
{
	_sc bool value = is_same_v<T, Ts...>;
};


// checks if the first type argument appears in the rest
template<typename ...>
struct is_type_in_rest;

template<typename ...Ts>
constexpr bool is_type_in_rest_v = is_type_in_rest<Ts...>::value;

template<typename T>
struct is_type_in_rest<T>
{
	_sc bool value = false;
};

template<typename T, typename ...Ts>
struct is_type_in_rest<T, Ts...>
{
	_sc bool value = (is_same_v<T, Ts> || ...);
};


// checks if any two type arguments are the same
template<typename ...>
struct is_any_same;

template<typename ...Ts>
constexpr bool is_any_same_v = is_any_same<Ts...>::value;

template<typename T>
struct is_any_same<T>
{
	_sc bool value = false;
};

template<typename T, typename ...Ts>
struct is_any_same<T, Ts...>
{
	_sc bool value = is_type_in_rest_v<T, Ts...> || is_any_same_v<Ts...>;
};


// extracts the nth type from the arguments
template<uint32_t, typename ...>
struct nth_type;

template<uint32_t N, typename ...Ts>
using nth_type_t = typename nth_type<N, Ts...>::type;

template<typename First, typename ...Ts>
struct nth_type<0, First, Ts...>
{
	using type = First;
};

template<uint32_t N, typename First, typename ...Ts>
struct nth_type<N, First, Ts...>
{
	static_assert(N <= sizeof... (Ts), "index is out of range in nth_type<>");
	using type = nth_type_t<N - 1, Ts...>;
};


// returns the index of the first type in the rest of the types
template<typename ...>
struct index_of_type;

template<typename ...Ts>
constexpr uint32_t index_of_type_v = index_of_type<Ts...>::value;

template<uint32_t, typename ...>
struct index_of_type_impl;

template<uint32_t current_index, typename T, typename ...Ts>
struct index_of_type_impl<current_index, T, T, Ts...>
{
	static_assert(
		!is_type_in_rest_v<T, Ts...>,
		"cannot deduce index of type, as there are multiple instances of it"
	);

	_sc uint32_t value = current_index;
};

template<uint32_t current_index, typename T, typename First, typename ...Ts>
struct index_of_type_impl<current_index, T, First, Ts...>
{
	_sc uint32_t value = index_of_type_impl<current_index + 1, T, Ts...>::value;
};

template<typename ...Ts>
struct index_of_type
{
	_sc uint32_t value = index_of_type_impl<0, Ts...>::value;
};


template<uint32_t ...Ns>
struct max
{
	_sc uint32_t value = []()
	{
		uint32_t rv = 0;
		((Ns > rv
		? (void)(rv = Ns)
		: (void)0 ), ...);
		return rv;
	}();
};

template<uint32_t ...Ns>
constexpr uint32_t max_v = max<Ns...>::value;


// finds the lowest common divisor of the arguments
template<uint32_t ...>
struct lowest_common_divisor;

template<uint32_t ...Ns>
constexpr uint32_t lowest_common_divisor_v = lowest_common_divisor<Ns...>::value;

template<uint32_t N>
struct lowest_common_divisor<N>
{
	_sc uint32_t value = N;
};

template<uint32_t A, uint32_t B>
struct lowest_common_divisor<A, B>
{
	_sc uint32_t value = [](uint32_t a, uint32_t b)
	{
		while (b != 0)
		{
			auto tmp = b;
			b = a % b;
			a = tmp;
		}
		return a;
	}(A, B);
};

template<uint32_t N, uint32_t ...Ns>
struct lowest_common_divisor<N, Ns...>
{
	_sc uint32_t value = lowest_common_divisor_v<N, lowest_common_divisor_v<Ns...>>;
};

#undef _sc

} // namespace meta


constexpr uint32_t _gcd(uint32_t a, uint32_t b)
{
	while (b != 0)
	{
		auto tmp = b;
		b = a % b;
		a = tmp;
	}
	return a;
}

constexpr uint32_t _lcd(uint32_t a, uint32_t b)
{
	return a * b / _gcd(a, b);
}


template<typename ...Ts>
class variant
{
public:
	using self_t = variant<Ts...>;
	template<uint32_t I>
	using value_type = meta::nth_type_t<I, Ts...>;
	static constexpr uint32_t npos = 0xff'ff'ff'ff;

private:
	alignas(meta::lowest_common_divisor_v<alignof(Ts)...>)
		char _data[meta::max_v<sizeof(Ts)...>];
	uint32_t _type_id;


	template<typename T>
	T &no_check_get(void);

	template<typename T>
	T const &no_check_get(void) const;

	template<uint32_t I>
	value_type<I> &no_check_get(void);

	template<uint32_t I>
	value_type<I> const &no_check_get(void) const;

	template<typename T, typename ...Args>
	void no_check_emplace(Args &&...args);

	template<typename T>
	struct _
	{};

	template<typename T, typename ...Args>
	variant(_<T>, Args &&...args);

public:
	template<typename T>
	static constexpr uint32_t index_of(void)
	{
		static_assert(meta::is_type_in_rest_v<T, Ts...>);
		return meta::index_of_type_v<T, Ts...>;
	}

	template<typename T, typename ...Args>
	static self_t make(Args &&...args)
	{
		static_assert(meta::is_type_in_rest_v<T, Ts...>);
		return self_t(_<T>{}, std::forward<Args>(args)...);
	}

	variant(void)
		: _data(), _type_id(npos)
	{}

	~variant(void);

	void clear(void);

	uint32_t index(void) const
	{
		return this->_type_id;
	}


	variant(self_t const &other);
	variant(self_t &&     other);
	self_t &operator = (self_t const &other);
	self_t &operator = (self_t &&     other);

	template<typename T, typename = std::enable_if_t<!meta::is_same_v<T, self_t>>>
	variant(T const &val);

	template<typename T, typename = std::enable_if_t<!meta::is_same_v<T, self_t>>>
	variant(T &&val);

	template<typename T, typename ...Args>
	void emplace(Args &&...args);

	template<uint32_t I, typename ...Args>
	void emplace(Args &&...args);

	template<typename T, typename = std::enable_if_t<!meta::is_same_v<T, self_t>>>
	self_t &operator = (T const &val);

	template<typename T, typename = std::enable_if_t<!meta::is_same_v<T, self_t>>>
	self_t &operator = (T &&val);

	template<typename T>
	T *get_if(void);

	template<typename T>
	const T *get_if(void) const;

	template<uint32_t I>
	value_type<I> *get_if(void);

	template<uint32_t I>
	const value_type<I> *get_if(void) const;

	template<typename T>
	T &get(void);

	template<typename T>
	T const &get(void) const;

	template<uint32_t I>
	value_type<I> &get(void);

	template<uint32_t I>
	value_type<I> const &get(void) const;



	friend std::ostream &operator << (std::ostream &os, self_t const &v)
	{
		((v._type_id == index_of<Ts>()
		? (void)(os << *reinterpret_cast<const Ts *>(v._data))
		: (void)0), ...);
		return os;
	}
};

template<typename ...Ts>
template<typename T>
T &variant<Ts...>::no_check_get(void)
{
	return *reinterpret_cast<T *>(this->_data);
}

template<typename ...Ts>
template<typename T>
T const &variant<Ts...>::no_check_get(void) const
{
	return *reinterpret_cast<T *>(this->_data);
}

template<typename ...Ts>
template<uint32_t I>
variant<Ts...>::value_type<I> &variant<Ts...>::no_check_get(void)
{
	return *reinterpret_cast<value_type<I> *>(this->_data);
}

template<typename ...Ts>
template<uint32_t I>
variant<Ts...>::value_type<I> const &variant<Ts...>::no_check_get(void) const
{
	return *reinterpret_cast<const value_type<I> *>(this->_data);
}

template<typename ...Ts>
template<typename T, typename ...Args>
void variant<Ts...>::no_check_emplace(Args &&...args)
{
	new(this->_data) T(std::forward<Args>(args)...);
}

template<typename ...Ts>
template<typename T, typename ...Args>
variant<Ts...>::variant(self_t::_<T>, Args &&...args)
	: _type_id(index_of<T>())
{
	static_assert(meta::is_type_in_rest_v<T, Ts...>);
	this->no_check_emplace<T>(std::forward<Args>(args)...);
}

template<typename ...Ts>
variant<Ts...>::~variant(void)
{
	if (this->_type_id == npos)
	{
		return;
	}

	((this->_type_id == index_of<Ts>()
	? (void)(this->no_check_get<Ts>().~Ts())
	: (void)0 ), ...);
}

template<typename ...Ts>
void variant<Ts...>::clear(void)
{
		if (this->_type_id == npos)
	{
		return;
	}

	((this->_type_id == index_of<Ts>()
	? (void)((this->no_check_get<Ts>().~Ts()), this->_type_id = npos)
	: (void)(0)
	), ...);

	this->_type_id = self_t::npos;
}

template<typename ...Ts>
variant<Ts...>::variant(self_t const &other)
	: _type_id(other._type_id)
{
	if (other._type_id == npos)
	{
		return;
	}

	((this->_type_id == index_of<Ts>()
	? (void)(this->no_check_emplace<Ts>(other.no_check_get<Ts>()))
	: (void)0), ...);
}

template<typename ...Ts>
variant<Ts...>::variant(self_t &&other)
	: _type_id(other._type_id)
{
	((this->_type_id == index_of<Ts>()
	? (void)(this->no_check_emplace<Ts>(std::move(other.no_check_get<Ts>())))
	: (void)0), ...);
}

template<typename ...Ts>
variant<Ts...> &variant<Ts...>::operator = (self_t const &other)
{
	if (this == &other)
	{
		return *this;
	}

	this->clear();
	this->_type_id = other._type_id;

	((this->_type_id == index_of<Ts>()
	? (void)(this->no_check_emplace<Ts>(other.no_check_get<Ts>()))
	: (void)0), ...);

	return *this;
}

template<typename ...Ts>
variant<Ts...> &variant<Ts...>::operator = (self_t &&other)
{
	if (this == &other)
	{
		return *this;
	}

	this->clear();
	this->_type_id = other._type_id;

	((this->_type_id == index_of<Ts>()
	? (void)(this->no_check_emplace<Ts>(std::move(other.no_check_get<Ts>())))
	: (void)0), ...);

	return *this;
}

template<typename ...Ts>
template<typename T, typename>
variant<Ts...>::variant(T const &val)
	: _type_id(index_of<T>())
{
	static_assert(meta::is_type_in_rest_v<T, Ts...>);
	this->no_check_emplace<T>(val);
}

template<typename ...Ts>
template<typename T, typename>
variant<Ts...>::variant(T &&val)
	: _type_id(index_of<T>())
{
	static_assert(meta::is_type_in_rest_v<T, Ts...>);
	this->no_check_emplace<T>(std::move(val));
}

template<typename ...Ts>
template<typename T, typename ...Args>
void variant<Ts...>::emplace(Args &&...args)
{
	static_assert(meta::is_type_in_rest_v<T, Ts...>);
	this->clear();
	this->no_check_emplace<T>(std::forward<Args>(args)...);
	this->_type_id = index_of<T>();
}

template<typename ...Ts>
template<uint32_t I, typename ...Args>
void variant<Ts...>::emplace(Args &&...args)
{
	this->clear();
	this->no_check_emplace<value_type<I>>(std::forward<Args>(args)...);
	this->_type_id = I;
}

template<typename ...Ts>
template<typename T, typename>
variant<Ts...> &variant<Ts...>::operator = (T const &val)
{
	static_assert(meta::is_type_in_rest_v<T, Ts...>);
	this->emplace(val);
	return *this;
}

template<typename ...Ts>
template<typename T, typename>
variant<Ts...> &variant<Ts...>::operator = (T &&val)
{
	static_assert(meta::is_type_in_rest_v<T, Ts...>);
	this->emplace(std::move(val));
	return *this;
}

template<typename ...Ts>
template<typename T>
T *variant<Ts...>::get_if(void)
{
	static_assert(meta::is_type_in_rest_v<T, Ts...>);
	if (this->_type_id != index_of<T>())
	{
		return nullptr;
	}
	return &this->no_check_get<T>();
}

template<typename ...Ts>
template<typename T>
const T *variant<Ts...>::get_if(void) const
{
	static_assert(meta::is_type_in_rest_v<T, Ts...>);
	if (this->_type_id != index_of<T>())
	{
		return nullptr;
	}
	return &this->no_check_get<T>();
}

template<typename ...Ts>
template<uint32_t I>
variant<Ts...>::value_type<I> *variant<Ts...>::get_if(void)
{
	if (this->_type_id != I)
	{
		return nullptr;
	}
	return &this->no_check_get<I>();
}

template<typename ...Ts>
template<uint32_t I>
variant<Ts...>::value_type<I> const *variant<Ts...>::get_if(void) const
{
	if (this->_type_id != I)
	{
		return nullptr;
	}
	return &this->no_check_get<I>();
}

template<typename ...Ts>
template<typename T>
T &variant<Ts...>::get(void)
{
	static_assert(meta::is_type_in_rest_v<T, Ts...>);
	if (this->_type_id != index_of<T>())
	{
		assert(false, "Bad type in variant::get");
	}
	return this->no_check_get<T>();
}

template<typename ...Ts>
template<typename T>
T const &variant<Ts...>::get(void) const
{
	static_assert(meta::is_type_in_rest_v<T, Ts...>);
	if (this->_type_id != index_of<T>())
	{
		assert(false, "Bad type in variant::get");
	}
	return this->no_check_get<T>();
}

template<typename ...Ts>
template<uint32_t I>
variant<Ts...>::value_type<I> &variant<Ts...>::get(void)
{
	if (I != this->_type_id)
	{
		assert(false, "Bad index in variant::get");
	}
	return this->no_check_get<I>();
}

template<typename ...Ts>
template<uint32_t I>
variant<Ts...>::value_type<I> const &variant<Ts...>::get(void) const
{
	if (I != this->_type_id)
	{
		assert(false, "Bad index in variant::get");
	}
	return this->no_check_get<I>();
}


#endif // VARIANT_H
