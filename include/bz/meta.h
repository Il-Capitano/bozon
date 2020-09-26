#ifndef _bz_meta_h__
#define _bz_meta_h__

#include "core.h"

#include <type_traits>
#include <limits>

bz_begin_namespace

namespace meta
{

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


template<typename T, typename U, typename ...Ts>
constexpr bool is_same = is_same<T, U> && (is_same<T, Ts> && ...);

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
	auto res = N;

	((res < Ns
	? (void)(res = Ns)
	: (void)0 ), ...);

	return res;
}();

template<size_t N, size_t ...Ns>
constexpr size_t max_index = max<size_t, N, Ns...>;


template<typename T, T N, T ...Ns>
constexpr T min = []
{
	auto res = N;

	((Ns < res
	? (void)(res = Ns)
	: (void)0 ), ...);

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


namespace internal
{

template<size_t, typename ...>
struct nth_type_impl;

template<size_t N, typename T, typename ...Ts>
struct nth_type_impl<N, T, Ts...>
{ using type = typename nth_type_impl<N - 1, Ts...>::type; };

template<typename T, typename ...Ts>
struct nth_type_impl<0, T, Ts...>
{ using type = T; };

} // namespace internal


template<size_t N, typename ...Ts>
using nth_type = typename internal::nth_type_impl<N, Ts...>::type;


template<typename T, typename ...Ts>
constexpr size_t type_index = std::numeric_limits<size_t>::max();

template<typename T, typename ...Ts>
constexpr size_t type_index<T, T, Ts...> = 0;

template<typename T, typename U, typename ...Ts>
constexpr size_t type_index<T, U, Ts...> = type_index<T, Ts...> + 1;


template<typename T, typename ...Ts>
constexpr bool is_in_types = (false || ... || is_same<T, Ts> );

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

} // namespace internal

template<template<typename ...> typename T, typename Pack>
using apply_type_pack = typename internal::apply_type_pack_impl<T, Pack>::type;

} // namespace meta

bz_end_namespace

#endif // _bz_meta_h__
