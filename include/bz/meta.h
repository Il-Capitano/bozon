#ifndef _bz_meta_h__
#define _bz_meta_h__

#include "core.h"

#include <type_traits>
#include <limits>

bz_begin_namespace

namespace meta
{

namespace internal
{

constexpr bool is_all_func(std::initializer_list<bool> values)
{
	for (auto const value : values)
	{
		if (!value)
		{
			return false;
		}
	}
	return true;
}

constexpr bool is_any_func(std::initializer_list<bool> values)
{
	for (auto const value : values)
	{
		if (value)
		{
			return true;
		}
	}
	return false;
}

} // namespace internal

template<bool ...Vals>
constexpr bool is_all = internal::is_all_func({ Vals... });

template<bool ...Vals>
constexpr bool is_any = internal::is_any_func({ Vals... });

template<typename T, T _value>
struct integral_constant
{ static constexpr T value = _value; };

template<size_t N>
using index_constant = integral_constant<size_t, N>;

template<typename T>
constexpr bool always_false = false;

template<typename ...Ts>
struct type_pack
{
	static constexpr size_t size(void) noexcept
	{ return sizeof... (Ts); }
};

template<typename T>
struct type_identity
{
	using type = T;
};


template<typename T, typename U, typename ...Ts>
constexpr bool is_same = internal::is_all_func({ is_same<T, U>, is_same<T, Ts>... });

template<typename T, typename U>
constexpr bool is_same<T, U> = false;

template<typename T>
constexpr bool is_same<T, T> = true;


template<typename T>
constexpr bool is_const = is_same<T, T const>;


template<typename>
constexpr bool is_lvalue_reference = false;

template<typename T>
constexpr bool is_lvalue_reference<T &> = true;


template<typename>
constexpr bool is_rvalue_reference = false;

template<typename T>
constexpr bool is_rvalue_reference<T &&> = true;


template<typename T>
constexpr bool is_reference =
	is_lvalue_reference<T>
	|| is_rvalue_reference<T>;


template<typename T>
constexpr bool is_void = is_same<T, void>;

template<typename T, template<typename ...> typename U>
constexpr bool is_specialization_of = false;

template<typename ...Ts, template<typename ...> typename U>
constexpr bool is_specialization_of<U<Ts...>, U> = true;


namespace internal
{

template<bool, typename>
struct enable_if_impl;

template<typename T>
struct enable_if_impl<true, T>
{ using type = T; };

} // namespace internal


template<bool cond, typename T = void>
using enable_if = typename internal::enable_if_impl<cond, T>::type;


namespace internal
{

template<bool, typename, typename>
struct conditional_impl;

template<typename TrueT, typename FalseT>
struct conditional_impl<true, TrueT, FalseT>
{ using type = TrueT; };

template<typename TrueT, typename FalseT>
struct conditional_impl<false, TrueT, FalseT>
{ using type = FalseT; };

} // namespace internal


template<bool cond, typename TrueT, typename FalseT>
using conditional = typename internal::conditional_impl<cond, TrueT, FalseT>::type;


template<typename T, typename ...Ts>
constexpr bool is_constructible_v = std::is_constructible_v<T, Ts...>;

template<typename T>
constexpr bool is_copy_constructible_v = std::is_copy_constructible_v<T>;

template<typename T>
constexpr bool is_move_constructible_v = std::is_move_constructible_v<T>;

template<typename T>
constexpr bool is_nothrow_destructible_v = std::is_nothrow_destructible_v<T>;

template<typename T>
constexpr bool is_nothrow_default_constructible_v = std::is_nothrow_default_constructible_v<T>;

template<typename T>
constexpr bool is_nothrow_copy_constructible_v = std::is_nothrow_copy_constructible_v<T>;

template<typename T>
constexpr bool is_nothrow_move_constructible_v = std::is_nothrow_move_constructible_v<T>;

template<typename T>
constexpr bool is_nothrow_copy_assignable_v = std::is_nothrow_copy_assignable_v<T>;

template<typename T>
constexpr bool is_nothrow_move_assignable_v = std::is_nothrow_move_assignable_v<T>;

template<typename T, typename ...Ts>
constexpr bool is_nothrow_constructible_v = std::is_nothrow_constructible_v<T, Ts...>;

template<typename T, typename ...Ts>
constexpr bool is_nothrow_assignable_v = std::is_nothrow_assignable_v<T, Ts...>;

template<typename Fn, typename ...Args>
constexpr bool is_invocable_v = std::is_invocable_v<Fn, Args...>;

template<typename Fn, typename ...Args>
constexpr bool is_nothrow_invocable_v = std::is_nothrow_invocable_v<Fn, Args...>;


template<typename T>
constexpr bool is_trivial_v = std::is_trivial_v<T>;

template<typename From, typename To>
constexpr bool is_convertible_v = std::is_convertible_v<From, To>;

template<typename T>
constexpr bool is_trivially_destructible_v = std::is_trivially_destructible_v<T>;

template<typename T, T N, T ...Ns>
constexpr T max = []
{
	T const values[] = { Ns... };
	auto res = N;

	for (auto const value : values)
	{
		if (value > res)
		{
			res = value;
		}
	}

	return res;
}();

template<size_t N, size_t ...Ns>
constexpr size_t max_index = max<size_t, N, Ns...>;


template<typename T, T N, T ...Ns>
constexpr T min = []
{
	T const values[] = { Ns... };
	auto res = N;

	for (auto const value : values)
	{
		if (value < res)
		{
			res = value;
		}
	}

	return res;
}();

template<size_t N, size_t ...Ns>
constexpr size_t min_index = min<size_t, N, Ns...>;

// greatest common divisor
template<typename T, T N, T ...Ns>
constexpr T gcd = gcd<T, N, gcd<T, Ns...>>;

template<typename T, T N, T M>
constexpr T gcd<T, N, M> = []
{
	auto a = N;
	auto b = M;

	while (a % b != 0)
	{
		auto tmp = b;
		b = a % b;
		a = tmp;
	}
	return b;
}();

template<typename T, T N>
constexpr T gcd<T, N> = N;

template<size_t N, size_t ...Ns>
constexpr size_t gcd_index = gcd<size_t, N, Ns...>;

// lowest common multiple
template<typename T, T N, T ...Ns>
constexpr T lcm = lcm<T, N, lcm<T, Ns...>>;

template<typename T, T N, T M>
constexpr T lcm<T, N, M> = (N * M) / gcd<T, N, M>;

template<typename T, T N>
constexpr T lcm<T, N> = N;

template<size_t N, size_t ...Ns>
constexpr size_t lcm_index = lcm<size_t, N, Ns...>;

#if defined(__has_builtin)
#if __has_builtin(__type_pack_element)
#define BZ_USE_TYPE_PACK_ELEMENT
#endif // __type_pack_element
#endif // __has_builtin


#ifdef BZ_USE_TYPE_PACK_ELEMENT

template<size_t N, typename ...Ts>
using nth_type = __type_pack_element<N, Ts...>;

#undef BZ_USE_TYPE_PACK_ELEMENT

#else // BZ_USE_TYPE_PACK_ELEMENT

namespace internal
{

template<size_t, typename ...>
struct nth_type_impl;

template<
	size_t N,
	typename T0,
	typename T1,
	typename T2,
	typename T3,
	typename T4,
	typename T5,
	typename T6,
	typename T7,
	typename T8,
	typename T9,
	typename ...Ts
>
requires (N >= 10)
struct nth_type_impl<
	N,
	T0,
	T1,
	T2,
	T3,
	T4,
	T5,
	T6,
	T7,
	T8,
	T9,
	Ts...
>
{ using type = typename nth_type_impl<N - 10, Ts...>::type; };

template<
	typename T0,
	typename ...Ts
>
struct nth_type_impl<
	0,
	T0,
	Ts...
>
{ using type = T0; };

template<
	typename T0,
	typename T1,
	typename ...Ts
>
struct nth_type_impl<
	1,
	T0,
	T1,
	Ts...
>
{ using type = T1; };

template<
	typename T0,
	typename T1,
	typename T2,
	typename ...Ts
>
struct nth_type_impl<
	2,
	T0,
	T1,
	T2,
	Ts...
>
{ using type = T2; };

template<
	typename T0,
	typename T1,
	typename T2,
	typename T3,
	typename ...Ts
>
struct nth_type_impl<
	3,
	T0,
	T1,
	T2,
	T3,
	Ts...
>
{ using type = T3; };

template<
	typename T0,
	typename T1,
	typename T2,
	typename T3,
	typename T4,
	typename ...Ts
>
struct nth_type_impl<
	4,
	T0,
	T1,
	T2,
	T3,
	T4,
	Ts...
>
{ using type = T4; };

template<
	typename T0,
	typename T1,
	typename T2,
	typename T3,
	typename T4,
	typename T5,
	typename ...Ts
>
struct nth_type_impl<
	5,
	T0,
	T1,
	T2,
	T3,
	T4,
	T5,
	Ts...
>
{ using type = T5; };

template<
	typename T0,
	typename T1,
	typename T2,
	typename T3,
	typename T4,
	typename T5,
	typename T6,
	typename ...Ts
>
struct nth_type_impl<
	6,
	T0,
	T1,
	T2,
	T3,
	T4,
	T5,
	T6,
	Ts...
>
{ using type = T6; };

template<
	typename T0,
	typename T1,
	typename T2,
	typename T3,
	typename T4,
	typename T5,
	typename T6,
	typename T7,
	typename ...Ts
>
struct nth_type_impl<
	7,
	T0,
	T1,
	T2,
	T3,
	T4,
	T5,
	T6,
	T7,
	Ts...
>
{ using type = T7; };

template<
	typename T0,
	typename T1,
	typename T2,
	typename T3,
	typename T4,
	typename T5,
	typename T6,
	typename T7,
	typename T8,
	typename ...Ts
>
struct nth_type_impl<
	8,
	T0,
	T1,
	T2,
	T3,
	T4,
	T5,
	T6,
	T7,
	T8,
	Ts...
>
{ using type = T8; };

template<
	typename T0,
	typename T1,
	typename T2,
	typename T3,
	typename T4,
	typename T5,
	typename T6,
	typename T7,
	typename T8,
	typename T9,
	typename ...Ts
>
struct nth_type_impl<
	9,
	T0,
	T1,
	T2,
	T3,
	T4,
	T5,
	T6,
	T7,
	T8,
	T9,
	Ts...
>
{ using type = T9; };

} // namespace internal


template<size_t N, typename ...Ts>
using nth_type = typename internal::nth_type_impl<N, Ts...>::type;

#endif // BZ_USE_TYPE_PACK_ELEMENT


template<typename T, typename ...Ts>
constexpr size_t type_index = []() {
	if constexpr (sizeof... (Ts) == 0)
	{
		return std::numeric_limits<size_t>::max();
	}
	else
	{
		bool value = false;
		bool const is_same_results[] = { (value |= is_same<T, Ts>)... };

		// find the index of the first true value with binary search
		bool const *first = is_same_results;
		bool const *last = is_same_results + (sizeof is_same_results / sizeof is_same_results[0]) - 1;

		if (*first == true)
		{
			return size_t(0);
		}
		if (*last == false)
		{
			return std::numeric_limits<size_t>::max();
		}

		while (first + 1 != last)
		{
			auto const midpoint = first + (last - first) / 2;
			if (*midpoint == true)
			{
				last = midpoint;
			}
			else
			{
				first = midpoint;
			}
		}
		return static_cast<size_t>(last - is_same_results);
	}
}();


template<typename T, typename ...Ts>
constexpr bool is_in_types = internal::is_any_func({ false, is_same<T, Ts>... });

template<typename T, typename TypePack>
constexpr bool is_in_type_pack = []() {
	static_assert(always_false<T>, "bz::meta::is_in_type_pack expects a type_pack as a second argument");
	return false;
}();

template<typename T, typename ...Ts>
constexpr bool is_in_type_pack<T, type_pack<Ts...>> = is_in_types<T, Ts...>;


template<typename T, T ...>
struct number_sequence
{};

namespace internal
{

template<typename T, T N, size_t current_index = N, T ...Ns>
struct make_number_sequence_impl
{
	using type =
		typename make_number_sequence_impl<
			T, N - 1, current_index - 1, N - 1, Ns...
		>::type;
};

template<typename T, T N, T ...Ns>
struct make_number_sequence_impl<T, N, 0, Ns...>
{ using type = number_sequence<T, Ns...>; };

} // namespace internal

template<typename T, T N>
using make_number_sequence = typename internal::make_number_sequence_impl<T, N>::type;


template<size_t ...Ns>
using index_sequence = number_sequence<size_t, Ns...>;

template<size_t N>
using make_index_sequence = make_number_sequence<size_t, N>;


namespace internal
{

template<typename T>
constexpr auto make_type_pack_impl = []<size_t ...Ns>(index_sequence<Ns...>)
{
	return type_pack<decltype((void)Ns, std::declval<T>())...>();
};

} // namespace internal


template<size_t N, typename T>
using make_type_pack = decltype(
	internal::make_type_pack_impl<T>(make_index_sequence<N>())
);


namespace internal
{

template<typename T>
struct remove_pointer_impl
{ using type = T; };

template<typename T>
struct remove_pointer_impl<T *>
{ using type = T; };

} // namespace internal

template<typename T>
using remove_pointer = typename internal::remove_pointer_impl<T>::type;


namespace internal
{

template<typename T>
struct remove_reference_impl
{ using type = T; };

template<typename T>
struct remove_reference_impl<T &>
{ using type = T; };

template<typename T>
struct remove_reference_impl<T &&>
{ using type = T; };

} // namespace internal

template<typename T>
using remove_reference = typename internal::remove_reference_impl<T>::type;


namespace internal
{

template<typename T>
struct remove_cv_impl
{ using type = T; };

template<typename T>
struct remove_cv_impl<const T>
{ using type = T; };

template<typename T>
struct remove_cv_impl<volatile T>
{ using type = T; };

template<typename T>
struct remove_cv_impl<const volatile T>
{ using type = T; };

} // namespace internal

template<typename T>
using remove_cv = typename internal::remove_cv_impl<T>::type;


template<typename T>
using remove_cv_reference = remove_cv<remove_reference<T>>;


namespace internal
{

template<typename T, typename U>
struct copy_reference_impl
{ using type = U; };

template<typename T, typename U>
struct copy_reference_impl<T &, U>
{ using type = U &; };

template<typename T, typename U>
struct copy_reference_impl<T &&, U>
{ using type = U &&; };

} // namespace internal

template<typename T, typename U>
using copy_reference = typename internal::copy_reference_impl<T, U>::type;


namespace internal
{

template<typename T, typename U>
struct copy_cv
{ using type = U; };

template<typename T, typename U>
struct copy_cv<const T, U>
{ using type = const U; };

template<typename T, typename U>
struct copy_cv<volatile T, U>
{ using type = volatile U; };

template<typename T, typename U>
struct copy_cv<const volatile T, U>
{ using type = const volatile U; };

} // namespace internal

template<typename T, typename U>
using copy_cv = typename internal::copy_cv<T, U>::type;


template<typename T, typename U>
using copy_cv_reference = copy_reference<
	T, copy_cv<remove_reference<T>, U>
>;

namespace internal
{

template<typename>
struct fn_return_type_impl;

template<typename Ret, typename ...Params>
struct fn_return_type_impl<Ret(Params...)>
{ using type = Ret; };

template<typename Ret, typename ...Params>
struct fn_return_type_impl<Ret(Params...) noexcept>
{ using type = Ret; };

template<typename Ret, typename ...Params>
struct fn_return_type_impl<Ret (*)(Params...)>
{ using type = Ret; };

template<typename Ret, typename ...Params>
struct fn_return_type_impl<Ret (*)(Params...) noexcept>
{ using type = Ret; };

#define _fn_ret_type(fn_suffix)                                \
template<typename Ret, typename Base, typename ...Params>      \
struct fn_return_type_impl<Ret (Base::*)(Params...) fn_suffix> \
{ using type = Ret; };

_fn_ret_type()
_fn_ret_type(&)
_fn_ret_type(&&)
_fn_ret_type(const )
_fn_ret_type(const &)
_fn_ret_type(const &&)
_fn_ret_type(noexcept)
_fn_ret_type(& noexcept)
_fn_ret_type(&& noexcept)
_fn_ret_type(const noexcept)
_fn_ret_type(const & noexcept)
_fn_ret_type(const && noexcept)

#undef _fn_ret_type

} // namespace internal

template<typename Fn>
using fn_return_type = typename internal::fn_return_type_impl<Fn>::type;


namespace internal
{

template<typename>
struct is_fn_noexcept_impl;

template<typename Ret, typename ...Params>
struct is_fn_noexcept_impl<Ret(Params...)>
{ static constexpr bool value = false; };

template<typename Ret, typename ...Params>
struct is_fn_noexcept_impl<Ret(Params...) noexcept>
{ static constexpr bool value = true; };

template<typename Ret, typename ...Params>
struct is_fn_noexcept_impl<Ret (*)(Params...)>
{ static constexpr bool value = false; };

template<typename Ret, typename ...Params>
struct is_fn_noexcept_impl<Ret (*)(Params...) noexcept>
{ static constexpr bool value = true; };

#define _is_fn_noexcept(b, fn_suffix)                          \
template<typename Ret, typename Base, typename ...Params>      \
struct is_fn_noexcept_impl<Ret (Base::*)(Params...) fn_suffix> \
{ static constexpr bool value = b; };

_is_fn_noexcept(false, )
_is_fn_noexcept(false, &)
_is_fn_noexcept(false, &&)
_is_fn_noexcept(false, const)
_is_fn_noexcept(false, const &)
_is_fn_noexcept(false, const &&)

_is_fn_noexcept(true, noexcept)
_is_fn_noexcept(true, & noexcept)
_is_fn_noexcept(true, && noexcept)
_is_fn_noexcept(true, const noexcept)
_is_fn_noexcept(true, const & noexcept)
_is_fn_noexcept(true, const && noexcept)

#undef _is_fn_noexcept

} // namespace internal

template<typename Fn>
constexpr bool is_fn_noexcept = internal::is_fn_noexcept_impl<Fn>::value;


namespace internal
{

template<typename>
struct fn_param_types_impl;

template<typename Ret, typename ...Params>
struct fn_param_types_impl<Ret(Params...)>
{ using type = type_pack<Params...>; };

template<typename Ret, typename ...Params>
struct fn_param_types_impl<Ret(Params...) noexcept>
{ using type = type_pack<Params...>; };

template<typename Ret, typename ...Params>
struct fn_param_types_impl<Ret (*)(Params...)>
{ using type = type_pack<Params...>; };

template<typename Ret, typename ...Params>
struct fn_param_types_impl<Ret (*)(Params...) noexcept>
{ using type = type_pack<Params...>; };

} // namespace internal

template<typename Fn>
using fn_param_types = typename internal::fn_param_types_impl<Fn>::type;


namespace internal
{

template<template<typename ...> typename T, typename TypePack>
struct apply_type_pack_impl;

template<template<typename ...> typename T, typename ...Ts>
struct apply_type_pack_impl<T, type_pack<Ts...>>
{
	using type = T<Ts...>;
};

template<template<typename> typename T, typename TypePack>
struct transform_type_pack_impl;

template<template<typename> typename T, typename ...Ts>
struct transform_type_pack_impl<T, type_pack<Ts...>>
{
	using type = type_pack<T<Ts>...>;
};

} // namespace internal

template<template<typename ...> typename T, typename Pack>
using apply_type_pack = typename internal::apply_type_pack_impl<T, Pack>::type;

template<template<typename> typename T, typename Pack>
using transform_type_pack = typename internal::transform_type_pack_impl<T, Pack>::type;


namespace internal
{

template<typename T>
struct is_member_variable_type_impl
{
	static constexpr bool value = false;
};

template<typename T, typename Base>
struct is_member_variable_type_impl<T (Base::*)>
{
	static constexpr bool value = true;
};

} // namespace internal

template<typename T>
constexpr bool is_member_variable_type = internal::is_member_variable_type_impl<T>::value;

template<auto Value>
constexpr bool is_member_variable = is_member_variable_type<decltype(Value)>;

} // namespace meta

bz_end_namespace

#endif // _bz_meta_h__
