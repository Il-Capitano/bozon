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

template<typename T, typename First, typename ...Ts>
constexpr int _get_id_v = _get_id<T, First, Ts...>::value;


template<int first, int ...vals>
struct _max_of
{
	static constexpr int value = first > _max_of<vals...>::value ? first : _max_of<vals...>::value;
};

template<int first>
struct _max_of<first>
{
	static constexpr int value = first;
};

template<int first, int ...vals>
constexpr int _max_of_v = _max_of<first, vals...>::value;


template<int id, typename First, typename ...Ts>
struct _nth_type
{
	static_assert(id <= sizeof... (Ts));
	using type = typename _nth_type<id - 1, Ts...>::type;
};

template<typename First, typename ...Ts>
struct _nth_type<0, First, Ts...>
{
	using type = First;
};

template<int id, typename ...Ts>
using _nth_type_t = typename _nth_type<id, Ts...>::type;



template<typename ...Ts>
class variant
{
	static_assert(_all_different_v<Ts...>);
public:
	using self_t = variant<Ts...>;
	template<int id>
	using value_type = _nth_type_t<id, Ts...>;

private:
	alignas(_max_of_v<alignof(Ts)...>) char _data[_max_of_v<sizeof(Ts)...>];
	int _type_id;


	template<typename T>
	T &no_check_get(void)
	{
		return *reinterpret_cast<T *>(this->_data);
	}

	template<typename T>
	T const &no_check_get(void) const
	{
		return *reinterpret_cast<const T *>(this->_data);
	}

	template<typename T, typename ...Args>
	void no_check_emplace(Args &&...args)
	{
		new(this->_data) T(std::forward<Args>(args)...);
	}


	template<typename T>
	struct _
	{};

	template<typename T, typename ...Args>
	variant(_<T>, Args &&...args)
		: _data(), _type_id(id_of<T>())
	{
		static_assert(_is_in_v<T, Ts...>);
		this->no_check_emplace<T>(std::forward<Args>(args)...);
	}

public:
	template<typename T>
	static constexpr int id_of(void)
	{
		static_assert(_is_in_v<T, Ts...>);
		return _get_id_v<T, Ts...>;
	}

	template<typename T, typename ...Args>
	static self_t make(Args &&...args)
	{
		static_assert(_is_in_v<T, Ts...>);
		return self_t(_<T>{}, std::forward<Args>(args)...);
	}

	variant(void)
		: _data(), _type_id(-1)
	{}

	~variant(void)
	{
		if (this->_type_id == -1)
		{
			return;
		}

		((this->_type_id == id_of<Ts>()
		? (void)(this->no_check_get<Ts>().~Ts())
		: (void)0 ), ...);
	}

	void clear(void)
	{
		if (this->_type_id == -1)
		{
			return;
		}

		((this->_type_id == id_of<Ts>()
		? (void)((this->no_check_get<Ts>().~Ts()), this->_type_id = -1)
		: (void)(0)
		), ...);
	}


	variant(self_t const &other)
		: _data(), _type_id(other._type_id)
	{
		if (other._type_id == -1)
		{
			return;
		}

		((this->_type_id == id_of<Ts>()
		? (void)(this->no_check_emplace<Ts>(other.no_check_get<Ts>()))
		: (void)0), ...);
	}

	variant(self_t &&other)
		: _data(), _type_id(other._type_id)
	{
		((this->_type_id == id_of<Ts>()
		? (void)(this->no_check_emplace<Ts>(std::move(other.no_check_get<Ts>())))
		: (void)0), ...);

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
		? (void)(this->no_check_emplace<Ts>(other.no_check_get<Ts>()))
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

		((this->_type_id == id_of<Ts>()
		? (void)(this->no_check_emplace<Ts>(std::move(other.no_check_get<Ts>())))
		: (void)0), ...);

		other._type_id = -1;

		return *this;
	}

	template<typename T, typename = std::enable_if_t<!std::is_same_v<T, self_t>>>
	variant(T const &val)
		: _data(), _type_id(id_of<T>())
	{
		static_assert(_is_in_v<T, Ts...>);
		this->no_check_emplace<T>(val);
	}

	template<typename T, typename = std::enable_if_t<!std::is_same_v<T, self_t>>>
	variant(T &&val)
		: _data(), _type_id(id_of<T>())
	{
		static_assert(_is_in_v<T, Ts...>);
		this->no_check_emplace<T>(std::move(val));
	}

	template<typename T, typename ...Args>
	void emplace(Args &&...args)
	{
		static_assert(_is_in_v<T, Ts...>);
		this->clear();
		this->no_check_emplace<T>(std::forward<Args>(args)...);
	}

	template<typename T, std::enable_if_t<!std::is_same_v<T, self_t>>>
	self_t &operator = (T const &val)
	{
		static_assert(_is_in_v<T, Ts...>);
		this->emplace(val);
		return *this;
	}

	template<typename T, std::enable_if_t<!std::is_same_v<T, self_t>>>
	self_t &operator = (T &&val)
	{
		static_assert(_is_in_v<T, Ts...>);
		this->emplace(std::move(val));
		return *this;
	}

	template<typename T>
	T *get_if(void)
	{
		static_assert(_is_in_v<T, Ts...>);
		if (this->_type_id == id_of<T>())
		{
			return &this->no_check_get<T>();
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
			return &this->no_check_get<T>();
		}
		else
		{
			return nullptr;
		}
	}

	template<typename T>
	T &get(void)
	{
		static_assert(_is_in_v<T, Ts...>);
		if (this->_type_id != id_of<T>())
		{
			assert(false, "Bad type in variant::get");
		}
		return this->no_check_get<T>();
	}

	template<typename T>
	T const &get(void) const
	{
		static_assert(_is_in_v<T, Ts...>);
		if (this->_type_id != id_of<T>())
		{
			assert(false, "Bad type in variant::get");
		}
		return this->no_check_get<T>();
	}



	friend std::ostream &operator << (std::ostream &os, self_t const &v)
	{
		((v._type_id == id_of<Ts>()
		? (void)(os << *reinterpret_cast<const Ts *>(v._data))
		: (void)0), ...);
		return os;
	}
};

#endif // VARIANT_H
