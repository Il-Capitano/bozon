#ifndef _bz_variant_h__
#define _bz_variant_h__

#include "core.h"

#include "meta.h"

bz_begin_namespace

template<typename ...>
class variant;


template<typename ...Ts>
class variant
{
	static_assert(sizeof... (Ts) > 0);
	// don't allow void as a type
	static_assert(((!meta::is_void<Ts>) && ...));
	// don't allow references as types
	static_assert(((!meta::is_reference<Ts>) && ...));
	// don't allow const types
	static_assert(((!meta::is_const<Ts>) && ...));

private:
	using self_t = variant<Ts...>;

	template<typename T>
	struct value_type_impl
	{
		static constexpr size_t index = []() -> size_t
		{
			using val_t = meta::remove_cv_reference<T>;
			if constexpr (meta::is_in_types<val_t, Ts...>)
			{
				return meta::type_index<val_t, Ts...>;
			}
			else
			{
				size_t index = 0;
				size_t candidate_count = 0;
				size_t it = 0;

				((meta::is_convertible_v<T, Ts>
				? (void)(index = it, ++it, ++candidate_count)
				: (void)(++it) ), ...);

				if (candidate_count == 1)
				{
					return index;
				}
				else
				{
					return size_t(-1);
				}
			}
		}();
		static_assert(index < sizeof... (Ts), "cannot decide which value type should be used");

		using type = meta::nth_type<index, Ts...>;
	};

public:
	template<size_t N>
	using value_type = meta::nth_type<N, Ts...>;
	template<typename T>
	using value_type_from = typename value_type_impl<T>::type;

	template<typename T>
	static constexpr size_t index_of = meta::type_index<T, Ts...>;

private:
	static constexpr auto data_align = meta::lcm_index<alignof (Ts)...>;
	static constexpr auto data_size  = meta::max_index<sizeof (Ts)...>;

	static constexpr auto _index_type_dummy = []()
	{
		constexpr auto elem_count = sizeof... (Ts);
		static_assert(alignof(std::max_align_t) == 16);
		static_assert(elem_count < std::numeric_limits<uint64_t>::max());
		static_assert(
			data_align == 1
			|| data_align == 2
			|| data_align == 4
			|| data_align == 8
			|| data_align == 16
		);
		if constexpr (data_align <= 1 && elem_count < std::numeric_limits<uint8_t>::max())
		{
			return uint8_t();
		}
		else if constexpr (data_align <= 2 && elem_count < std::numeric_limits<uint16_t>::max())
		{
			return uint16_t();
		}
		else if constexpr (data_align <= 4 && elem_count < std::numeric_limits<uint32_t>::max())
		{
			return uint32_t();
		}
		else if constexpr (data_align <= 16 && elem_count < std::numeric_limits<uint64_t>::max())
		{
			return uint64_t();
		}
	}();
	using index_t = meta::remove_cv_reference<decltype(_index_type_dummy)>;

	alignas(data_align) uint8_t _data[data_size];
	index_t _index;


	template<size_t N>
	value_type<N> &no_check_get(void) noexcept
	{ return *reinterpret_cast<value_type<N> *>(this->_data); }

	template<size_t N>
	value_type<N> const &no_check_get(void) const noexcept
	{ return *reinterpret_cast<const value_type<N> *>(this->_data); }


	template<size_t N, typename ...Args>
	void no_clear_emplace(Args &&...args) noexcept(
		meta::is_nothrow_constructible_v<value_type<N>, Args...>
	)
	{
		new(this->_data) value_type<N>(std::forward<Args>(args)...);
		this->_index = N;
	}


	void no_null_clear(void) noexcept(
		(meta::is_nothrow_destructible_v<Ts> && ...)
	)
	{
		[&]<size_t ...Ns>(std::index_sequence<Ns...>)
		{
			((this->_index == Ns
			? (void)(this->no_check_get<Ns>().~Ts())
			: (void)0 ), ...);
		}(std::make_index_sequence<sizeof... (Ts)>());
	}

	template<size_t N>
	struct _index_indicator
	{};

	template<size_t N, typename ...Args>
	variant(_index_indicator<N>, Args &&...args) noexcept(
		meta::is_nothrow_constructible_v<value_type<N>, Args...>
	)
		: variant()
	{ this->no_clear_emplace<N>(std::forward<Args>(args)...); }


protected:
	static constexpr size_t null = std::numeric_limits<size_t>::max();

public:
	template<size_t N, typename ...Args>
	static self_t make(Args &&...args) noexcept(
		meta::is_nothrow_constructible_v<value_type<N>, Args...>
	)
	{ return self_t(_index_indicator<N>(), std::forward<Args>(args)...); }

	template<typename T, typename ...Args>
	static self_t make(Args &&...args) noexcept(
		meta::is_nothrow_constructible_v<T, Args...>
	)
	{ return make<index_of<T>>(std::forward<Args>(args)...); }

	void clear() noexcept(
		(meta::is_nothrow_destructible_v<Ts> && ...)
	)
	{
		if (this->_index == null)
		{
			return;
		}
		this->no_null_clear();
		this->_index = null;
	}

	void assign(self_t const &other) noexcept(
		(meta::is_nothrow_copy_constructible_v<Ts> && ...)
	)
	{
		if (this == &other)
		{
			return;
		}

		if (other._index == null)
		{
			this->clear();
			return;
		}

		[&other, this]<size_t ...Ns>(std::index_sequence<Ns...>)
		{
			((other._index == Ns
			? (void)[&other, this]()
			{
				if (this->_index == Ns)
				{
					this->no_check_get<Ns>() = other.template no_check_get<Ns>();
				}
				else
				{
					this->clear();
					this->no_clear_emplace<Ns>(other.template no_check_get<Ns>());
				}
			}()
			: (void)0 ), ...);
		}(std::make_index_sequence<sizeof... (Ts)>());
	}

	void assign(self_t &&other) noexcept(
		(meta::is_nothrow_destructible_v<Ts> && ...)
		&& (meta::is_nothrow_move_constructible_v<Ts> && ...)
		&& (meta::is_nothrow_move_assignable_v<Ts> && ...)
	)
	{
		if (this == &other)
		{
			return;
		}

		if (other._index == null)
		{
			this->clear();
			return;
		}

		[&other, this]<size_t ...Ns>(std::index_sequence<Ns...>)
		{
			((other._index == Ns
			? (void)[&other, this]()
			{
				if (this->_index == Ns)
				{
					this->no_check_get<Ns>()
						= std::move(other.template no_check_get<Ns>());
				}
				else
				{
					this->clear();
					this->no_clear_emplace<Ns>(
						std::move(other.template no_check_get<Ns>())
					);
				}
			}()
			: (void)0 ), ...);
		}(std::make_index_sequence<sizeof... (Ts)>());
	}

	template<size_t N, typename ...Args>
	void emplace(Args &&...args) noexcept(
		(meta::is_nothrow_destructible_v<Ts> && ...)
		&& meta::is_nothrow_constructible_v<value_type<N>, Args...>
	)
	{
		this->clear();
		this->no_clear_emplace<N>(std::forward<Args>(args)...);
	}

	template<typename T, typename ...Args>
	void emplace(Args &&...args) noexcept(
		(meta::is_nothrow_destructible_v<Ts> && ...)
		&& meta::is_nothrow_constructible_v<T, Args...>
	)
	{ this->emplace<index_of<T>>(std::forward<Args>(args)...); }

	variant(void) noexcept
		: _index(null)
	{}

	variant(self_t const &other) noexcept(
		(meta::is_nothrow_copy_constructible_v<Ts> && ...)
	)
		: variant()
	{
		[&other, this]<size_t ...Ns>(std::index_sequence<Ns...>)
		{
			((other._index == Ns
			? (void)(this->no_clear_emplace<Ns>(other.template no_check_get<Ns>()))
			: (void)0 ), ...);
		}(std::make_index_sequence<sizeof...(Ts)>());
	}

	variant(self_t &&other) noexcept(
		(meta::is_nothrow_move_constructible_v<Ts> && ...)
	)
		: variant()
	{
		[&other, this]<size_t ...Ns>(std::index_sequence<Ns...>)
		{
			((other._index == Ns
			? (void)(this->no_clear_emplace<Ns>(std::move(other.template no_check_get<Ns>())))
			: (void)0 ), ...);
		}(std::make_index_sequence<sizeof...(Ts)>());
	}

	~variant(void) noexcept(
		(meta::is_nothrow_destructible_v<Ts> && ...)
	)
	{ this->no_null_clear(); }

	self_t &operator = (self_t const &rhs) noexcept(
		(meta::is_nothrow_destructible_v<Ts> && ...)
		&& (meta::is_nothrow_copy_assignable_v<Ts> && ...)
	)
	{
		this->assign(rhs);
		return *this;
	}

	self_t &operator = (self_t &&rhs) noexcept(
		(meta::is_nothrow_destructible_v<Ts> && ...)
		&& (meta::is_nothrow_move_assignable_v<Ts> && ...)
	)
	{
		this->assign(std::move(rhs));
		return *this;
	}


	template<
		typename T,
		typename = meta::enable_if<
			!meta::is_same<
				meta::remove_cv_reference<T>,
				self_t
			>
		>
	>
	variant(T &&val) noexcept(
		meta::is_nothrow_constructible_v<value_type_from<T>, T>
	)
	{ this->no_clear_emplace<index_of<value_type_from<T>>>(std::forward<T>(val)); }

	template<
		typename T,
		typename = meta::enable_if<
			!meta::is_same<
				meta::remove_cv_reference<T>,
				self_t
			>
		>
	>
	self_t &operator = (T &&val) noexcept(
		(meta::is_nothrow_destructible_v<Ts> && ...)
		&& meta::is_nothrow_assignable_v<value_type_from<T>, T>
		&& meta::is_nothrow_constructible_v<value_type_from<T>, T>
	)
	{
		constexpr auto index = index_of<value_type_from<T>>;
		if (this->_index == index)
		{
			this->no_check_get<index>() = std::forward<T>(val);
		}
		else
		{
			this->clear();
			this->no_clear_emplace<index>(std::forward<T>(val));
		}
		return *this;
	}

	template<size_t N>
	value_type<N> &get(void) noexcept
	{
		bz_assert(this->_index == N);
		return this->no_check_get<N>();
	}

	template<size_t N>
	value_type<N> const &get(void) const noexcept
	{
		bz_assert(this->_index == N);
		return this->no_check_get<N>();
	}

	template<typename T>
	T &get(void) noexcept
	{ return this->get<index_of<T>>(); }

	template<typename T>
	T const &get(void) const noexcept
	{ return this->get<index_of<T>>(); }


	template<size_t N>
	value_type<N> *get_if(void) noexcept
	{
		if (this->_index == N)
		{
			return &this->no_check_get<N>();
		}
		else
		{
			return nullptr;
		}
	}

	template<size_t N>
	const value_type<N> *get_if(void) const noexcept
	{
		if (this->_index == N)
		{
			return &this->no_check_get<N>();
		}
		else
		{
			return nullptr;
		}
	}

	template<typename T>
	T *get_if(void) noexcept
	{ return this->get_if<index_of<T>>(); }

	template<typename T>
	const T *get_if(void) const noexcept
	{ return this->get_if<index_of<T>>(); }


	template<typename Visitor>
	auto visit(Visitor &&visitor) noexcept(
		(meta::is_nothrow_invocable_v<Visitor, Ts &> && ...)
	) -> decltype(
		std::forward<decltype(visitor)>(visitor)(
			std::declval<value_type<0> &>()
		)
	)
	{
		static_assert(
			meta::is_same<decltype(visitor(std::declval<Ts &>()))...>,
			"Visitor must return the same type for every element"
		);
		static_assert(
			(meta::is_invocable_v<Visitor, Ts &> && ...),
			"Visitor is not invocable for one or more variants"
		);
		bz_assert(this->_index != null, "visit called on empty variant");
		using ret_t     = decltype(visitor(std::declval<value_type<0> &>()));
		using visitor_t = decltype(visitor);
		using fn_t      = ret_t (*)(visitor_t, void *);

		constexpr fn_t calls[] = {
			[](visitor_t visitor, void *ptr) -> ret_t
			{
				return std::forward<visitor_t>(visitor)(*reinterpret_cast<Ts *>(ptr));
			}...
		};

		return calls[this->_index](
			std::forward<visitor_t>(visitor),
			&this->_data
		);
	}

	template<typename Visitor>
	auto visit(Visitor &&visitor) const noexcept(
		(meta::is_nothrow_invocable_v<Visitor, Ts const &> && ...)
	) -> decltype(
		std::forward<decltype(visitor)>(visitor)(
			std::declval<value_type<0> const &>()
		)
	)
	{
		static_assert(
			meta::is_same<decltype(visitor(std::declval<Ts const &>()))...>,
			"Visitor must return the same type for every element"
		);
		static_assert(
			(meta::is_invocable_v<Visitor, Ts const &> && ...),
			"Visitor is not invocable for one or more variants"
		);
		bz_assert(this->_index != null, "visit called on empty variant");
		using ret_t     = decltype(visitor(std::declval<value_type<0> const &>()));
		using visitor_t = decltype(visitor);
		using fn_t      = ret_t (*)(visitor_t, void const *);

		constexpr fn_t calls[] = {
			[](visitor_t visitor, void const *ptr) -> ret_t
			{
				return std::forward<visitor_t>(visitor)(*reinterpret_cast<Ts const *>(ptr));
			}...
		};

		return calls[this->_index](
			std::forward<visitor_t>(visitor),
			&this->_data
		);
	}


	size_t index(void) const noexcept
	{ return this->_index; }

	bool is_null(void) const noexcept
	{ return this->_index == null; }

	bool not_null(void) const noexcept
	{ return this->_index != null; }

	template<typename T>
	bool is(void) const noexcept
	{ return this->_index == index_of<T>; }
};

template<typename ...Ts>
bool operator == (variant<Ts...> const &lhs, variant<Ts...> const &rhs)
{
	return [&]<size_t ...Ns>(std::index_sequence<Ns...>)
	{
		if (lhs.index() != rhs.index())
		{
			return false;
		}
		else if (lhs.index() == lhs.null)
		{
			return true;
		}
		else
		{
			bool result;

			((lhs.index() == Ns
			? (void)(result = (lhs.template get<Ns>() == rhs.template get<Ns>()))
			: (void)0 ), ...);

			return result;
		}
	}(std::make_index_sequence<sizeof... (Ts)>());
}

template<typename ...Ts, typename Rhs, meta::enable_if<meta::is_in_types<Rhs, Ts...>, int> = 0>
bool operator == (variant<Ts...> const &lhs, Rhs const &rhs)
{
	return lhs.template is<Rhs>() && lhs.template get<Rhs>() == rhs;
}

template<typename Lhs, typename ...Ts, meta::enable_if<meta::is_in_types<Lhs, Ts...>, int> = 0>
bool operator == (Lhs const &lhs, variant<Ts...> const &rhs)
{
	return rhs.template is<Lhs>() && lhs == rhs.template get<Lhs>();
}

template<typename ...Ts>
bool operator != (variant<Ts...> const &lhs, variant<Ts...> const &rhs)
{
	return [&]<size_t ...Ns>(std::index_sequence<Ns...>)
	{
		if (lhs.index() != rhs.index())
		{
			return true;
		}
		else if (lhs.index() == lhs.null)
		{
			return false;
		}
		else
		{
			bool result;

			((lhs.index() == Ns
			? (void)(result = (lhs.template get<Ns>() != rhs.template get<Ns>()))
			: (void)0 ), ...);

			return result;
		}
	}(std::make_index_sequence<sizeof... (Ts)>());
}

template<typename ...Ts, typename Rhs, meta::enable_if<meta::is_in_types<Rhs, Ts...>, int> = 0>
bool operator != (variant<Ts...> const &lhs, Rhs const &rhs)
{
	return !(lhs == rhs);
}

template<typename Lhs, typename ...Ts, meta::enable_if<meta::is_in_types<Lhs, Ts...>, int> = 0>
bool operator != (Lhs const &lhs, variant<Ts...> const &rhs)
{
	return !(lhs == rhs);
}


template<size_t N, typename ...Ts>
auto get(variant<Ts...> &v) noexcept -> decltype(v.template get<N>())
{ return v.template get<N>(); }

template<size_t N, typename ...Ts>
auto get(variant<Ts...> const &v) noexcept -> decltype(v.template get<N>())
{ return v.template get<N>(); }

template<typename T, typename ...Ts>
auto get(variant<Ts...> &v) noexcept -> decltype(v.template get<T>())
{ return v.template get<T>(); }

template<typename T, typename ...Ts>
auto get(variant<Ts...> const &v) noexcept -> decltype(v.template get<T>())
{ return v.template get<T>(); }


template<size_t N, typename ...Ts>
auto get_if(variant<Ts...> &v) noexcept -> decltype(v.template get_if<N>())
{ return v.template get_if<N>(); }

template<size_t N, typename ...Ts>
auto get_if(variant<Ts...> const &v) noexcept -> decltype(v.template get_if<N>())
{ return v.template get_if<N>(); }

template<typename T, typename ...Ts>
auto get_if(variant<Ts...> &v) noexcept -> decltype(v.template get_if<T>())
{ return v.template get_if<T>(); }

template<typename T, typename ...Ts>
auto get_if(variant<Ts...> const &v) noexcept -> decltype(v.template get_if<T>())
{ return v.template get_if<T>(); }


template<typename ...Ts>
void swap(variant<Ts...> &v1, variant<Ts...> &v2) noexcept(std::is_nothrow_invocable_v<
	variant<Ts...>::swap, variant<Ts...>, variant<Ts...>
>)
{ v1.swap(v2); }


template<typename ...Ts>
struct overload : Ts...
{
	using Ts::operator()...;
};

template<typename ...Ts>
overload(Ts...) -> overload<Ts...>;

bz_end_namespace

#endif // _bz_variant_h__
