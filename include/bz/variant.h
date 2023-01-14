#ifndef _bz_variant_h__
#define _bz_variant_h__

#include "core.h"

#include "meta.h"

bz_begin_namespace

namespace internal
{

constexpr size_t max(std::initializer_list<size_t> values)
{
	size_t result = 0;
	for (size_t value : values)
	{
		if (value > result)
		{
			result = value;
		}
	}
	return result;
}

constexpr bool is_all(std::initializer_list<bool> values)
{
	for (bool value : values)
	{
		if (!value)
		{
			return false;
		}
	}
	return true;
}

constexpr bool is_any(std::initializer_list<bool> values)
{
	for (bool value : values)
	{
		if (value)
		{
			return true;
		}
	}
	return false;
}

#if defined(__has_builtin)
#if __has_builtin(__type_pack_element)
#define BZ_HAS_TYPE_PACK_ELEMENT
#endif // __type_pack_element
#endif // __has_builtin

#ifndef BZ_HAS_TYPE_PACK_ELEMENT

template<size_t N, typename T>
struct variant_value_type_helper_elem
{
	T operator [] (meta::index_constant<N>);
};

template<typename Seq, typename ...Ts>
struct variant_value_type_helper;

template<size_t ...Ns, typename ...Ts>
struct variant_value_type_helper<std::index_sequence<Ns...>, Ts...> : variant_value_type_helper_elem<Ns, Ts>...
{
	using variant_value_type_helper_elem<Ns, Ts>::operator []...;
};

#endif // BZ_HAS_TYPE_PACK_ELEMENT

template<typename ...Ts>
struct variant_storage_t
{
#ifdef BZ_HAS_TYPE_PACK_ELEMENT

public:
	template<size_t N>
	using value_type = __type_pack_element<N, Ts...>;

#undef BZ_HAS_TYPE_PACK_ELEMENT
#else

private:
	using value_type_helper = variant_value_type_helper<std::make_index_sequence<sizeof ...(Ts)>, Ts...>;
public:
	template<size_t N>
	using value_type = decltype(value_type_helper()[meta::index_constant<N>()]);

#endif // BZ_HAS_TYPE_PACK_ELEMENT

protected:
	static constexpr auto data_align = max({ alignof (Ts)... });
	static constexpr auto data_size  = max({ sizeof (Ts)... });

	static constexpr auto _index_type_dummy = []()
	{
		constexpr auto elem_count = sizeof... (Ts);
		static_assert(alignof(std::max_align_t) <= 16);
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

	static constexpr index_t null = std::numeric_limits<index_t>::max();

	alignas(data_align) uint8_t _data[data_size];
	index_t _index = null;


	template<size_t N>
	[[gnu::always_inline]] value_type<N> &no_check_get(void) noexcept
	{ return *reinterpret_cast<value_type<N> *>(this->_data); }

	template<size_t N>
	[[gnu::always_inline]] value_type<N> const &no_check_get(void) const noexcept
	{ return *reinterpret_cast<const value_type<N> *>(this->_data); }

	template<size_t N, typename ...Args>
	void no_clear_emplace(Args &&...args) noexcept(
		meta::is_nothrow_constructible_v<value_type<N>, Args...>
	)
	{
		if constexpr (meta::is_constructible_v<value_type<N>, Args...>)
		{
			new(this->_data) value_type<N>(std::forward<Args>(args)...);
		}
		else
		{
			new(this->_data) value_type<N>{std::forward<Args>(args)...};
		}
		this->_index = N;
	}
};

template<typename ...Ts>
struct variant_trivial_base : public variant_storage_t<Ts...>
{
private:
	using self_t = variant_trivial_base<Ts...>;
	using base_t = variant_storage_t<Ts...>;
protected:
	using base_t::null;

public:
	using base_t::value_type;
public:
	void clear() noexcept
	{
		this->_index = null;
	}

	void assign(self_t const &other) noexcept
	{
		*this = other;
	}

	void assign(self_t &&other) noexcept
	{
		*this = std::move(other);
	}
};

template<typename ...Ts>
struct variant_non_trivial_base : public variant_storage_t<Ts...>
{
private:
	using self_t = variant_non_trivial_base<Ts...>;
	using base_t = variant_storage_t<Ts...>;

protected:
	using base_t::null;
private:
	void no_null_clear(void) noexcept
	{
		[&]<size_t ...Ns>(std::index_sequence<Ns...>)
		{
			[[maybe_unused]] int _dummy[] = {
				((this->_index == Ns
				? (void)(this->template no_check_get<Ns>().~Ts())
				: (void)0 ), 0)...
			};
		}(std::make_index_sequence<sizeof... (Ts)>());
	}

public:
	using base_t::value_type;
public:
	variant_non_trivial_base(void) noexcept = default;

	variant_non_trivial_base(self_t const &other) noexcept(
		is_all({ meta::is_nothrow_copy_constructible_v<Ts>... })
	)
		: base_t()
	{
		[&other, this]<size_t ...Ns>(std::index_sequence<Ns...>)
		{
			[[maybe_unused]] int _dummy[] = {
				((other._index == Ns
				? (void)(this->template no_clear_emplace<Ns>(other.template no_check_get<Ns>()))
				: (void)0 ), 0)...
			};
		}(std::make_index_sequence<sizeof...(Ts)>());
	}

	variant_non_trivial_base(self_t &&other) noexcept(
		is_all({ meta::is_nothrow_move_constructible_v<Ts>... })
	)
		: base_t()
	{
		[&other, this]<size_t ...Ns>(std::index_sequence<Ns...>)
		{
			[[maybe_unused]] int _dummy[] = {
				((other._index == Ns
				? (void)(this->template no_clear_emplace<Ns>(std::move(other.template no_check_get<Ns>())))
				: (void)0 ), 0)...
			};
		}(std::make_index_sequence<sizeof...(Ts)>());
	}

	~variant_non_trivial_base(void) noexcept
	{
		this->no_null_clear();
	}

	self_t &operator = (self_t const &rhs) noexcept(
		is_all({ meta::is_nothrow_copy_constructible_v<Ts>... })
		&& is_all({ meta::is_nothrow_copy_assignable_v<Ts>... })
	)
	{
		this->assign(rhs);
		return *this;
	}

	self_t &operator = (self_t &&rhs) noexcept(
		is_all({ meta::is_nothrow_move_constructible_v<Ts>... })
		&& is_all({ meta::is_nothrow_move_assignable_v<Ts>... })
	)
	{
		this->assign(std::move(rhs));
		return *this;
	}

	void clear() noexcept
	{
		if (this->_index == null)
		{
			return;
		}
		this->no_null_clear();
		this->_index = null;
	}

	void assign(self_t const &other) noexcept(
		is_all({ meta::is_nothrow_copy_constructible_v<Ts>... })
		&& is_all({ meta::is_nothrow_copy_assignable_v<Ts>... })
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
			[[maybe_unused]] int _dummy[] = {
				((other._index == Ns
				? (void)[&other, this]()
				{
					if (this->_index == Ns)
					{
						this->template no_check_get<Ns>() = other.template no_check_get<Ns>();
					}
					else
					{
						this->clear();
						this->template no_clear_emplace<Ns>(other.template no_check_get<Ns>());
					}
				}()
				: (void)0 ), 0)...
			};
		}(std::make_index_sequence<sizeof... (Ts)>());
	}

	void assign(self_t &&other) noexcept(
		is_all({ meta::is_nothrow_move_constructible_v<Ts>... })
		&& is_all({ meta::is_nothrow_move_assignable_v<Ts>... })
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
			[[maybe_unused]] int _dummy[] = {
				((other._index == Ns
				? (void)[&other, this]()
				{
					if (this->_index == Ns)
					{
						this->template no_check_get<Ns>() = std::move(other.template no_check_get<Ns>());
					}
					else
					{
						this->clear();
						this->template no_clear_emplace<Ns>(std::move(other.template no_check_get<Ns>()));
					}
				}()
				: (void)0 ), 0)...
			};
		}(std::make_index_sequence<sizeof... (Ts)>());
	}
};

} // namespace internal


template<typename ...Ts>
class variant :
	public meta::conditional<
		internal::is_all({ meta::is_trivial_v<Ts>... }),
		internal::variant_trivial_base<Ts...>,
		internal::variant_non_trivial_base<Ts...>
	>
{
	static_assert(sizeof... (Ts) > 0);
	// don't allow void as a type
	static_assert(internal::is_all({ !meta::is_void<Ts>... }));
	// don't allow references as types
	static_assert(internal::is_all({ !meta::is_reference<Ts>... }));
	// don't allow const types
	static_assert(internal::is_all({ !meta::is_const<Ts>... }));

private:
	using self_t = variant<Ts...>;
	using base_t = meta::conditional<
		internal::is_all({ meta::is_trivial_v<Ts>... }),
		internal::variant_trivial_base<Ts...>,
		internal::variant_non_trivial_base<Ts...>
	>;

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

				[[maybe_unused]] int _dummy[] = {
					((meta::is_convertible_v<T, Ts>
					? (void)(index = it, ++it, ++candidate_count)
					: (void)(++it) ), 0)...
				};

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

		using type = typename base_t::template value_type<index>;
	};

public:
	static constexpr size_t variant_count = sizeof... (Ts);

	template<size_t N>
	using value_type = typename base_t::template value_type<N>;
	template<typename T>
	using value_type_from = typename value_type_impl<T>::type;

	template<typename T>
	static constexpr size_t index_of = meta::type_index<T, Ts...>;

private:
	template<size_t N>
	struct _index_indicator
	{};

	template<size_t N, typename ...Args>
	variant(_index_indicator<N>, Args &&...args) noexcept(
		meta::is_nothrow_constructible_v<value_type<N>, Args...>
	)
		: variant()
	{ this->template no_clear_emplace<N>(std::forward<Args>(args)...); }


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

	template<size_t N, typename ...Args>
	[[gnu::always_inline]] auto &emplace(Args &&...args) noexcept(
		internal::is_all({ meta::is_nothrow_destructible_v<Ts>... })
		&& meta::is_nothrow_constructible_v<value_type<N>, Args...>
	)
	{
		this->clear();
		this->template no_clear_emplace<N>(std::forward<Args>(args)...);
		return this->template no_check_get<N>();
	}

	template<typename T, typename ...Args>
	[[gnu::always_inline]] auto &emplace(Args &&...args) noexcept(
		internal::is_all({ meta::is_nothrow_destructible_v<Ts>... })
		&& meta::is_nothrow_constructible_v<T, Args...>
	)
	{ return this->emplace<index_of<T>>(std::forward<Args>(args)...); }

	variant(void) noexcept = default;
	using base_t::base_t;

	variant(self_t const &other) = default;
	variant(self_t &&other) = default;
	self_t &operator = (self_t const &other) = default;
	self_t &operator = (self_t &&other) = default;

	template<
		typename T,
		typename = meta::enable_if<
			!meta::is_same<
				meta::remove_cv_reference<T>,
				self_t
			>
			&& internal::is_any({ meta::is_constructible_v<Ts, T>... })
		>
	>
	variant(T &&val) noexcept(
		meta::is_nothrow_constructible_v<value_type_from<T>, T>
	)
	{ this->template no_clear_emplace<index_of<value_type_from<T>>>(std::forward<T>(val)); }

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
		internal::is_all({ meta::is_nothrow_destructible_v<Ts>... })
		&& meta::is_nothrow_assignable_v<value_type_from<T>, T>
		&& meta::is_nothrow_constructible_v<value_type_from<T>, T>
	)
	{
		constexpr auto index = index_of<value_type_from<T>>;
		if (this->_index == index)
		{
			this->template no_check_get<index>() = std::forward<T>(val);
		}
		else
		{
			this->clear();
			this->template no_clear_emplace<index>(std::forward<T>(val));
		}
		return *this;
	}

	template<size_t N>
	[[gnu::always_inline]] value_type<N> &get(void) noexcept
	{
		bz_assert(this->_index == N);
		return this->template no_check_get<N>();
	}

	template<size_t N>
	[[gnu::always_inline]] value_type<N> const &get(void) const noexcept
	{
		bz_assert(this->_index == N);
		return this->template no_check_get<N>();
	}

	template<typename T>
	[[gnu::always_inline]] T &get(void) noexcept
	{
		static_assert(meta::is_in_types<T, Ts...>);
		return this->get<index_of<T>>();
	}

	template<typename T>
	[[gnu::always_inline]] T const &get(void) const noexcept
	{
		static_assert(meta::is_in_types<T, Ts...>);
		return this->get<index_of<T>>();
	}


	template<size_t N>
	[[gnu::always_inline]] value_type<N> get_by_move(void) noexcept
	{
		bz_assert(this->_index == N);
		using T = value_type<N>;

		T &value = this->template no_check_get<N>();
		T result = std::move(value);
		value.~T();
		this->_index = base_t::null;

		return result;
	}

	template<typename T>
	[[gnu::always_inline]] T get_by_move(void) noexcept
	{
		static_assert(meta::is_in_types<T, Ts...>);
		return this->get_by_move<index_of<T>>();
	}


	template<size_t N>
	[[gnu::always_inline]] value_type<N> *get_if(void) noexcept
	{
		if (this->_index == N)
		{
			return &this->template no_check_get<N>();
		}
		else
		{
			return nullptr;
		}
	}

	template<size_t N>
	[[gnu::always_inline]] value_type<N> const *get_if(void) const noexcept
	{
		if (this->_index == N)
		{
			return &this->template no_check_get<N>();
		}
		else
		{
			return nullptr;
		}
	}

	template<typename T>
	[[gnu::always_inline]] T *get_if(void) noexcept
	{ return this->get_if<index_of<T>>(); }

	template<typename T>
	[[gnu::always_inline]] T const *get_if(void) const noexcept
	{ return this->get_if<index_of<T>>(); }


	template<typename Visitor>
	auto visit(Visitor &&visitor) noexcept(
		internal::is_all({ meta::is_nothrow_invocable_v<Visitor, Ts &>... })
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
			internal::is_all({ meta::is_invocable_v<Visitor, Ts &> ... }),
			"Visitor is not invocable for one or more variants"
		);
		bz_assert(this->_index != base_t::null && "visit called on empty variant");
		using ret_t     = decltype(visitor(std::declval<value_type<0> &>()));
		using visitor_t = decltype(visitor);
		using fn_t      = ret_t (*)(visitor_t, void *);

		static constexpr fn_t calls[] = {
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
		internal::is_all({ meta::is_nothrow_invocable_v<Visitor, Ts const &>... })
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
			internal::is_all({ meta::is_invocable_v<Visitor, Ts const &> ... }),
			"Visitor is not invocable for one or more variants"
		);
		bz_assert(this->_index != base_t::null && "visit called on empty variant");
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
	{ return this->_index == base_t::null; }

	bool not_null(void) const noexcept
	{ return this->_index != base_t::null; }

	template<typename T>
	bool is(void) const noexcept
	{ return this->_index == index_of<T>; }

	template<size_t N>
	bool is(void) const noexcept
	{ return this->_index == N; }

	template<typename ...Us>
	bool is_any(void) const noexcept
	{ return (this->is<Us>() || ...); }

	template<size_t ...Ns>
	bool is_any(void) const noexcept
	{ return (this->is<Ns>() || ...); }
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
		else if (lhs.is_null())
		{
			return true;
		}
		else
		{
			bool result;

			[[maybe_unused]] int _dummy[] = {
				((lhs.index() == Ns
				? (void)(result = (lhs.template get<Ns>() == rhs.template get<Ns>()))
				: (void)0 ), 0)...
			};

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
		else if (lhs.is_null())
		{
			return false;
		}
		else
		{
			bool result;

			[[maybe_unused]] int _dummy[] = {
				((lhs.index() == Ns
				? (void)(result = (lhs.template get<Ns>() != rhs.template get<Ns>()))
				: (void)0 ), 0)...
			};

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
	decltype(&variant<Ts...>::swap), variant<Ts...>, variant<Ts...>
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
