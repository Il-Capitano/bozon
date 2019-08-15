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

	~variant(void)
	{
		if (this->_data == nullptr)
		{
			return;
		}

		((this->_type_id == id_of<Ts>()
		? (void)(delete static_cast<Ts *>(this->_data))
		: (void)0 ), ...);
	}

	void clear(void)
	{
		if (this->_data == nullptr)
		{
			return;
		}

		((this->_type_id == id_of<Ts>()
		? (void)((delete static_cast<Ts *>(this->_data)), this->_data = nullptr, this->_type_id = -1)
		: (void)(0)
		), ...);
	}


	variant(self_t const &other)
		: _data(nullptr), _type_id(other._type_id)
	{
		if (other._data == nullptr)
		{
			return;
		}

		((this->_type_id == id_of<Ts>()
		? (void)(this->_data = new Ts(*static_cast<Ts *>(other._data)))
		: (void)0), ...);
	}

	variant(self_t &&other)
		: _data(other._data), _type_id(other._type_id)
	{
		other._data = nullptr;
		other._type_id = -1;
	}

	self_t &operator = (self_t const &other)
	{
		if (this == &other)
		{
			return *this;
		}

		this->clear();
		this->_type_id = other._type_id;

		((this->_type_id == id_of<Ts>()
		? (void)(this->_data = new Ts(*static_cast<Ts *>(other._data)))
		: (void)0), ...);

		return *this;
	}

	self_t &operator = (self_t &&other)
	{
		if (this == &other)
		{
			return *this;
		}

		this->clear();

		this->_type_id = other._type_id;
		this->_data = other._data;

		other._data = nullptr;
		other._type_id = -1;

		return *this;
	}


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
