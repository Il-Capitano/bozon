#ifndef VARIANT_H
#define VARIANT_H


template<typename First, typename ...Ts>
struct _all_different
{
	static constexpr bool value = (!std::is_same_v<First, Ts> && ...) && _all_different<Ts...>::value;
};

template<typename First>
struct _all_different<First>
{
	static constexpr bool value = true;
};

template<typename First, typename Second>
struct _all_different<First, Second>
{
	static constexpr bool value = !std::is_same_v<First, Second>;
};

template<typename ...Ts>
constexpr bool _all_different_v = _all_different<Ts...>::value;


template<typename T, typename ...Ts>
constexpr bool _is_in_v = _all_different<Ts...>::value && !_all_different<T, Ts...>::value;



template<typename T, typename First, typename ...Ts>
constexpr int _get_id_func(void)
{
	if (std::is_same_v<T, First>)
	{
		return 0;
	}
	else
	{
		if constexpr (sizeof... (Ts) == 0)
		{
			extern char *_bad_symbol__;
			throw _bad_symbol__;
		}
		else
		{
			return _get_id_func<T, Ts...>() + 1;
		}
	}
}


template<typename T, typename First, typename ...Ts>
struct _get_id
{
	static constexpr int value = _get_id_func<T, First, Ts...>();
};


template<typename ...Ts>
class variant
{
	static_assert(_all_different_v<Ts...>);

public:
	using self_t = variant<Ts...>;

private:
	void *_data;
	int _type_id;

	variant(void *data, int type_id)
		: _data(data), _type_id(type_id)
	{}

public:
	template<typename T>
	static constexpr int id_of(void)
	{
		return _get_id<T, Ts...>::value;
	}

	template<typename T, typename ...Args>
	static self_t make(Args &&...args)
	{
		static_assert(_is_in_v<T, Ts...>);
		return variant<Ts...>(new T(std::forward<Args>(args)...), id_of<T>());
	}

	variant(void)
		: _data(nullptr), _type_id(-1)
	{}

	~variant(void)
	{
		((this->_type_id == id_of<Ts>()
		? (void)(delete static_cast<Ts *>(this->_data))
		: (void)0 ), ...);
	}


	variant(self_t const &self)
		: _data(nullptr), _type_id(self._type_id)
	{
		((this->_data == nullptr
		? (void)( this->_data = static_cast<Ts *>(this->_type_id == id_of<Ts>()
			? (new Ts(*static_cast<Ts *>(self._data)))
			: (nullptr) ))
		: (void)0 ), ...);
	}

	variant(self_t &&self)
		: _data(self._data), _type_id(self._type_id)
	{
		self._data = nullptr;
		self._type_id = -1;
	}

	operator = (self_t const &) = delete;
	operator = (self_t &&)      = delete;


	template<typename T>
	T *get_if(void)
	{
		static_assert(_is_in_v<T, Ts...>);
		if (this->_type_id == id_of<T>())
		{
			return static_cast<T *>(this->_data);
		}
		else
		{
			return nullptr;
		}
	}

	template<typename T>
	const T *get_if(void) const
	{
		static_assert(_is_in_v<T, Ts...>);
		if (this->_type_id == id_of<T>())
		{
			return static_cast<T *>(this->_data);
		}
		else
		{
			return nullptr;
		}
	}

	template<typename T>
	T *get(void)
	{
		static_assert(_is_in_v<T, Ts...>);
		if (this->_type_id == id_of<T>())
		{
			return static_cast<T *>(this->_data);
		}
		else
		{
			assert(false, "Bad type in variant::get");
			return nullptr;
		}
	}


	template<typename T>
	const T *get(void) const
	{
		static_assert(_is_in_v<T, Ts...>);
		if (this->_type_id == id_of<T>())
		{
			return static_cast<T *>(this->_data);
		}
		else
		{
			assert(false, "Bad type in variant::get");
			return nullptr;
		}
	}


	friend std::ostream &operator << (std::ostream &os, self_t const &v)
	{
		((v._type_id == id_of<Ts>()
		? (void)(os << *static_cast<Ts *>(v._data))
		: (void)0), ...);
		return os;
	}
};

#endif // VARIANT_H
