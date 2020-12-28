#ifndef _bz_ranges_h__
#define _bz_ranges_h__

#include "bz/meta.h"
#include "core.h"
#include <algorithm>
#include <utility>

bz_begin_namespace

template<typename Range>
struct universal_end_sentinel {};


namespace internal
{

template<typename Range>
struct range_base_filter
{
	template<typename Func>
	constexpr auto filter(Func &&func) const noexcept;
};

template<typename Collection>
struct collection_base_filter
{
	template<typename Func>
	constexpr auto filter(Func &&func) const noexcept;
};

template<typename Range>
struct range_base_transform
{
	template<typename Func>
	constexpr auto transform(Func &&func) const noexcept;
};

template<typename Collection>
struct collection_base_transform
{
	template<typename Func>
	constexpr auto transform(Func &&func) const noexcept;
};

template<typename Range>
struct range_base_member
{
	template<auto MemberPtr>
	constexpr auto member(void) const noexcept;
};

template<typename Collection>
struct collection_base_member
{
	template<auto MemberPtr>
	constexpr auto member(void) const noexcept;
	template<auto MemberPtr>
	constexpr auto member(void) noexcept;
};

template<typename Range>
struct range_base_collect
{
	template<template<typename ...Ts> typename Vec>
	constexpr auto collect(void) const;

	auto collect(void) const;
};

template<typename Range>
struct range_base_count
{
	constexpr std::size_t count(void) const noexcept;
};

template<typename Range>
struct range_base_is_any
{
	template<typename Func>
	constexpr bool is_any(Func &&func) const noexcept;
};

template<typename Collection>
struct collection_base_is_any
{
	template<typename Func>
	constexpr bool is_any(Func &&func) const noexcept;
};

template<typename Range>
struct range_base_is_all
{
	template<typename Func>
	constexpr bool is_all(Func &&func) const noexcept;
};

template<typename Collection>
struct collection_base_is_all
{
	template<typename Func>
	constexpr bool is_all(Func &&func) const noexcept;
};

template<typename Range>
struct range_base_contains
{
	template<typename Val>
	constexpr bool contains(Val &&val) const noexcept;
};

template<typename Collection>
struct collection_base_contains
{
	template<typename Val>
	constexpr bool contains(Val &&val) const noexcept;
};

template<typename Range>
struct range_base_for_each
{
	template<typename Func>
	constexpr void for_each(Func &&func) const noexcept;
};

template<typename Collection>
struct collection_base_for_each
{
	template<typename Func>
	constexpr void for_each(Func &&func) const noexcept;
};

template<typename Range>
struct range_base_sum
{
	constexpr auto sum(void) const noexcept;
};

template<typename Collection>
struct collection_base_sum
{
	constexpr auto sum(void) const noexcept;
};

template<typename Range>
struct range_base_max
{
	template<typename T>
	constexpr auto max(T &&val) const noexcept;
};

template<typename Collection>
struct collection_base_max
{
	template<typename T>
	constexpr auto max(T &&val) const noexcept;
};

template<typename Range>
struct range_base_min
{
	template<typename T>
	constexpr auto min(T &&val) const noexcept;
};

template<typename Collection>
struct collection_base_min
{
	template<typename T>
	constexpr auto min(T &&val) const noexcept;
};

template<typename Collection>
struct collection_base_sort
{
	void sort(void) noexcept;
	template<typename Cmp>
	void sort(Cmp &&cmp) noexcept;
};

template<typename Collection>
struct collection_base_append
{
	template<typename Range>
	void append(Range &&range);
	template<typename Range>
	void append_move(Range &&range);
};

} // namespace internal


template<typename Range>
struct range_base :
	internal::range_base_filter   <Range>,
	internal::range_base_transform<Range>,
	internal::range_base_member   <Range>,
	internal::range_base_collect  <Range>,
	internal::range_base_count    <Range>,
	internal::range_base_is_any   <Range>,
	internal::range_base_is_all   <Range>,
	internal::range_base_contains <Range>,
	internal::range_base_for_each <Range>,
	internal::range_base_sum      <Range>,
	internal::range_base_max      <Range>,
	internal::range_base_min      <Range>
{};

template<typename Collection>
struct collection_base :
	internal::collection_base_filter   <Collection>,
	internal::collection_base_transform<Collection>,
	internal::collection_base_member   <Collection>,
	internal::collection_base_is_any   <Collection>,
	internal::collection_base_is_all   <Collection>,
	internal::collection_base_contains <Collection>,
	internal::collection_base_for_each <Collection>,
	internal::collection_base_sum      <Collection>,
	internal::collection_base_max      <Collection>,
	internal::collection_base_min      <Collection>,
	internal::collection_base_sort     <Collection>,
	internal::collection_base_append   <Collection>
{
	constexpr auto as_range(void) const noexcept;
	constexpr auto as_range(void) noexcept;
};


template<typename ItType, typename EndType>
struct basic_range : range_base<basic_range<ItType, EndType>>
{
private:
	using self_t = basic_range<ItType, EndType>;
private:
	ItType _it;
	[[no_unique_address]] EndType _end;

public:
	constexpr basic_range(ItType it, EndType end)
		: _it(std::move(it)), _end(std::move(end))
	{}

	template<typename T, std::size_t N>
	constexpr basic_range(T (&arr)[N]) noexcept
		: _it(std::begin(arr)), _end(std::end(arr))
	{}

	template<typename T, std::size_t N>
	constexpr basic_range(T const (&arr)[N]) noexcept
		: _it(std::begin(arr)), _end(std::end(arr))
	{}

	constexpr bool at_end(void) const noexcept
	{ return this->_it == this->_end; }

	constexpr self_t &operator ++ (void) noexcept
	{
		++this->_it;
		return *this;
	}

	constexpr decltype(auto) operator * (void) const noexcept
	{ return *this->_it; }

	constexpr auto const &operator -> (void) const noexcept
	{ return this->_it; }

	constexpr friend bool operator == (self_t const &lhs, [[maybe_unused]] universal_end_sentinel<self_t> rhs) noexcept
	{ return lhs.at_end(); }

	constexpr friend bool operator == ([[maybe_unused]] universal_end_sentinel<self_t> lhs, self_t const &rhs) noexcept
	{ return rhs.at_end(); }

	constexpr friend bool operator != (self_t const &lhs, [[maybe_unused]] universal_end_sentinel<self_t> rhs) noexcept
	{ return !lhs.at_end(); }

	constexpr friend bool operator != ([[maybe_unused]] universal_end_sentinel<self_t> lhs, self_t const &rhs) noexcept
	{ return !rhs.at_end(); }


	constexpr self_t begin(void) const noexcept
	{ return *this; }

	constexpr universal_end_sentinel<self_t> end(void) const noexcept
	{ return universal_end_sentinel<self_t>{}; }
};

template<typename T>
struct iota_range : range_base<iota_range<T>>
{
private:
	using self_t = iota_range<T>;
private:
	T _it;
	T _end;

public:
	constexpr iota_range(T it, T end)
		: _it(std::move(it)), _end(std::move(end))
	{}

	template<typename T1, typename T2>
	constexpr iota_range(T1 it, T2 end)
		: _it(std::move(it)), _end(std::move(end))
	{}

	constexpr bool at_end(void) const noexcept
	{ return this->_it == this->_end; }

	constexpr self_t &operator ++ (void) noexcept
	{
		++this->_it;
		return *this;
	}

	constexpr decltype(auto) operator * (void) const noexcept
	{ return this->_it; }

	constexpr auto const &operator -> (void) const noexcept
	{ return &this->_it; }

	constexpr friend bool operator == (self_t const &lhs, [[maybe_unused]] universal_end_sentinel<self_t> rhs) noexcept
	{ return lhs.at_end(); }

	constexpr friend bool operator == ([[maybe_unused]] universal_end_sentinel<self_t> lhs, self_t const &rhs) noexcept
	{ return rhs.at_end(); }

	constexpr friend bool operator != (self_t const &lhs, [[maybe_unused]] universal_end_sentinel<self_t> rhs) noexcept
	{ return !lhs.at_end(); }

	constexpr friend bool operator != ([[maybe_unused]] universal_end_sentinel<self_t> lhs, self_t const &rhs) noexcept
	{ return !rhs.at_end(); }


	constexpr self_t begin(void) const noexcept
	{ return *this; }

	constexpr universal_end_sentinel<self_t> end(void) const noexcept
	{ return universal_end_sentinel<self_t>{}; }
};

template<typename ItType, typename EndType, typename FilterFuncType>
struct filter_range : range_base<filter_range<ItType, EndType, FilterFuncType>>
{
private:
	using self_t = filter_range<ItType, EndType, FilterFuncType>;
private:
	ItType _it;
	[[no_unique_address]] EndType _end;
	[[no_unique_address]] FilterFuncType _filter_function;

public:
	constexpr filter_range(ItType it, EndType end, FilterFuncType func)
		: _it(std::move(it)), _end(std::move(end)), _filter_function(std::move(func))
	{
		while (this->_it != this->_end && !this->_filter_function(*this->_it))
		{
			++this->_it;
		}
	}

	constexpr bool at_end(void) const noexcept
	{ return this->_it == this->_end; }

	constexpr self_t &operator ++ (void) noexcept
	{
		do
		{
			++this->_it;
		} while (this->_it != this->_end && !this->_filter_function(*this->_it));
		return *this;
	}

	constexpr decltype(auto) operator * (void) const noexcept
	{ return *this->_it; }

	constexpr auto const &operator -> (void) const noexcept
	{ return this->_it; }

	constexpr friend bool operator == (self_t const &lhs, [[maybe_unused]] universal_end_sentinel<self_t> rhs) noexcept
	{ return lhs.at_end(); }

	constexpr friend bool operator == ([[maybe_unused]] universal_end_sentinel<self_t> lhs, self_t const &rhs) noexcept
	{ return rhs.at_end(); }

	constexpr friend bool operator != (self_t const &lhs, [[maybe_unused]] universal_end_sentinel<self_t> rhs) noexcept
	{ return !lhs.at_end(); }

	constexpr friend bool operator != ([[maybe_unused]] universal_end_sentinel<self_t> lhs, self_t const &rhs) noexcept
	{ return !rhs.at_end(); }


	constexpr self_t begin(void) const noexcept
	{ return *this; }

	constexpr universal_end_sentinel<self_t> end(void) const noexcept
	{ return universal_end_sentinel<self_t>{}; }
};

template<typename ItType, typename EndType, typename TransformFuncType>
struct transform_range : range_base<transform_range<ItType, EndType, TransformFuncType>>
{
private:
	using self_t = transform_range<ItType, EndType, TransformFuncType>;
private:
	ItType _it;
	[[no_unique_address]] EndType _end;
	[[no_unique_address]] TransformFuncType _transform_func;

public:
	constexpr transform_range(ItType it, EndType end, TransformFuncType func)
		: _it(std::move(it)), _end(std::move(end)), _transform_func(std::move(func))
	{}

	constexpr bool at_end(void) const noexcept
	{ return this->_it == this->_end; }

	constexpr self_t &operator ++ (void) noexcept
	{
		++this->_it;
		return *this;
	}

	constexpr decltype(auto) operator * (void) const noexcept
	{ return this->_transform_func(*this->_it); }

	constexpr auto const &operator -> (void) const noexcept
	{ return this->_it; }

	constexpr friend bool operator == (self_t const &lhs, [[maybe_unused]] universal_end_sentinel<self_t> rhs) noexcept
	{ return lhs.at_end(); }

	constexpr friend bool operator == ([[maybe_unused]] universal_end_sentinel<self_t> lhs, self_t const &rhs) noexcept
	{ return rhs.at_end(); }

	constexpr friend bool operator != (self_t const &lhs, [[maybe_unused]] universal_end_sentinel<self_t> rhs) noexcept
	{ return !lhs.at_end(); }

	constexpr friend bool operator != ([[maybe_unused]] universal_end_sentinel<self_t> lhs, self_t const &rhs) noexcept
	{ return !rhs.at_end(); }


	constexpr self_t begin(void) const noexcept
	{ return *this; }

	constexpr universal_end_sentinel<self_t> end(void) const noexcept
	{ return universal_end_sentinel<self_t>{}; }
};

template<typename FirstItType, typename FirstEndType, typename SecondItType, typename SecondEndType>
struct zip_range : range_base<zip_range<FirstItType, FirstEndType, SecondItType, SecondEndType>>
{
private:
	using self_t = zip_range<FirstItType, FirstEndType, SecondItType, SecondEndType>;
private:
	FirstItType _first_it;
	[[no_unique_address]] FirstEndType _first_end;
	SecondItType _second_it;
	[[no_unique_address]] SecondEndType _second_end;

public:
	zip_range(FirstItType first_it, FirstEndType first_end, SecondItType second_it, SecondEndType second_end) noexcept
		: _first_it(std::move(first_it)), _first_end(std::move(first_end)),
		  _second_it(std::move(second_it)), _second_end(std::move(second_end))
	{}

	constexpr bool at_end(void) const noexcept
	{ return this->_first_it == this->_first_end || this->_second_it == this->_second_end; }

	constexpr self_t &operator ++ (void) noexcept
	{
		++this->_first_it;
		++this->_second_it;
		return *this;
	}

	constexpr auto operator * (void) const noexcept
	{
		return std::pair<
			decltype(*this->_first_it),
			decltype(*this->_second_it)
		>{ *this->_first_it, *this->_second_it };
	}

	constexpr friend bool operator == (self_t const &lhs, [[maybe_unused]] universal_end_sentinel<self_t> rhs) noexcept
	{ return lhs.at_end(); }

	constexpr friend bool operator == ([[maybe_unused]] universal_end_sentinel<self_t> lhs, self_t const &rhs) noexcept
	{ return rhs.at_end(); }

	constexpr friend bool operator != (self_t const &lhs, [[maybe_unused]] universal_end_sentinel<self_t> rhs) noexcept
	{ return !lhs.at_end(); }

	constexpr friend bool operator != ([[maybe_unused]] universal_end_sentinel<self_t> lhs, self_t const &rhs) noexcept
	{ return !rhs.at_end(); }

	constexpr self_t begin(void) const noexcept
	{ return *this; }

	constexpr universal_end_sentinel<self_t> end(void) const noexcept
	{ return universal_end_sentinel<self_t>{}; }
};


template<typename ItType, typename EndType>
basic_range(ItType it, EndType end) -> basic_range<ItType, EndType>;

template<typename T, std::size_t N>
basic_range(T (&arr)[N]) -> basic_range<decltype(std::begin(arr)), decltype(std::end(arr))>;

template<typename T, std::size_t N>
basic_range(T const (&arr)[N]) -> basic_range<decltype(std::begin(arr)), decltype(std::end(arr))>;

template<typename T>
iota_range(T it, T end) -> iota_range<T>;

template<typename T1, typename T2>
iota_range(T1 it, T2 end) -> iota_range<std::common_type_t<T1, T2>>;

template<typename ItType, typename EndType, typename FilterFuncType>
filter_range(ItType it, EndType end, FilterFuncType func) -> filter_range<ItType, EndType, FilterFuncType>;

template<typename ItType, typename EndType, typename TransformFuncType>
transform_range(ItType it, EndType end, TransformFuncType func) -> transform_range<ItType, EndType, TransformFuncType>;

template<typename FirstItType, typename FirstEndType, typename SecondItType, typename SecondEndType>
zip_range(FirstItType first_it, FirstEndType first_end, SecondItType second_it, SecondEndType second_end) -> zip_range<FirstItType, FirstEndType, SecondItType, SecondEndType>;


template<typename Collection>
constexpr auto collection_base<Collection>::as_range(void) const noexcept
{
	auto const self = static_cast<Collection const *>(this);
	return basic_range{ self->begin(), self->end() };
}

template<typename Collection>
constexpr auto collection_base<Collection>::as_range(void) noexcept
{
	auto const self = static_cast<Collection *>(this);
	return basic_range{ self->begin(), self->end() };
}

template<typename Range>
constexpr auto to_range(Range &&range) noexcept
{
	return basic_range{ std::begin(range), std::end(range) };
}

template<typename T1, typename T2>
constexpr auto iota(T1 begin, T2 end) noexcept
{
	return iota_range{ std::move(begin), std::move(end) };
}

template<typename Range, typename Func>
constexpr auto filter(Range &&range, Func &&func) noexcept
{
	return filter_range{
		std::begin(range), std::end(range),
		std::forward<Func>(func)
	};
}

template<typename Range, typename Func>
constexpr auto transform(Range &&range, Func &&func) noexcept
{
	return transform_range{
		std::begin(range), std::end(range),
		std::forward<Func>(func)
	};
}

template<typename FirstRange, typename SecondRange>
constexpr auto zip(FirstRange &&first_range, SecondRange &&second_range) noexcept
{
	return zip_range{
		std::begin(first_range),  std::end(first_range),
		std::begin(second_range), std::end(second_range)
	};
}


namespace internal
{

template<typename Range>
template<typename Func>
constexpr auto range_base_filter<Range>::filter(Func &&func) const noexcept
{ return ::bz::filter(*static_cast<Range const *>(this), std::forward<Func>(func)); }

template<typename Collection>
template<typename Func>
constexpr auto collection_base_filter<Collection>::filter(Func &&func) const noexcept
{ return static_cast<Collection const *>(this)->as_range().filter(std::forward<Func>(func)); }

template<typename Range>
template<typename Func>
constexpr auto range_base_transform<Range>::transform(Func &&func) const noexcept
{ return ::bz::transform(*static_cast<Range const *>(this), std::forward<Func>(func)); }

template<typename Collection>
template<typename Func>
constexpr auto collection_base_transform<Collection>::transform(Func &&func) const noexcept
{ return static_cast<Collection const *>(this)->as_range().transform(std::forward<Func>(func)); }

template<typename Range>
template<auto MemberPtr>
constexpr auto range_base_member<Range>::member(void) const noexcept
{
	static_assert(meta::is_member_variable<MemberPtr>, "template parameter must be a struct member pointer (e.g. &foo::i)");
	return ::bz::transform(
		*static_cast<Range const *>(this),
		[](auto &&value) -> auto && { return value.*MemberPtr; }
	);
}

template<typename Collection>
template<auto MemberPtr>
constexpr auto collection_base_member<Collection>::member(void) const noexcept
{
	static_assert(meta::is_member_variable<MemberPtr>, "template parameter must be a struct member pointer (e.g. &foo::i)");
	return static_cast<Collection const *>(this)->as_range().template member<MemberPtr>();
}

template<typename Collection>
template<auto MemberPtr>
constexpr auto collection_base_member<Collection>::member(void) noexcept
{
	static_assert(meta::is_member_variable<MemberPtr>, "template parameter must be a struct member pointer (e.g. &foo::i)");
	return static_cast<Collection const *>(this)->as_range().template member<MemberPtr>();
}

template<typename Range>
template<template<typename ...Ts> typename Vec>
constexpr auto range_base_collect<Range>::collect(void) const
{
	auto const self = static_cast<Range const *>(this);
	Vec<std::decay_t<decltype(self->operator*())>> result;
	for (auto &&it : *self)
	{
		result.emplace_back(std::forward<decltype(it)>(it));
	}
	return result;
}

template<typename Range>
constexpr std::size_t range_base_count<Range>::count(void) const noexcept
{
	auto const self = static_cast<Range const *>(this);
	std::size_t result = 0;
	while (!self->at_end())
	{
		++(*self);
		++result;
	}
	return result;
}

template<typename Range>
template<typename Func>
constexpr bool range_base_is_any<Range>::is_any(Func &&func) const noexcept
{
	auto const self = static_cast<Range const *>(this);
	for (auto &&it : *self)
	{
		if (func(std::forward<decltype(it)>(it)))
		{
			return true;
		}
	}
	return false;
}

template<typename Collection>
template<typename Func>
constexpr bool collection_base_is_any<Collection>::is_any(Func &&func) const noexcept
{ return static_cast<Collection const *>(this)->as_range().is_any(std::forward<Func>(func)); }

template<typename Range>
template<typename Func>
constexpr bool range_base_is_all<Range>::is_all(Func &&func) const noexcept
{
	auto const self = static_cast<Range const *>(this);
	for (auto &&it : *self)
	{
		if (!func(std::forward<decltype(it)>(it)))
		{
			return false;
		}
	}
	return true;
}

template<typename Collection>
template<typename Func>
constexpr bool collection_base_is_all<Collection>::is_all(Func &&func) const noexcept
{ return static_cast<Collection const *>(this)->as_range().is_all(std::forward<Func>(func)); }

template<typename Range>
template<typename Val>
constexpr bool range_base_contains<Range>::contains(Val &&val) const noexcept
{
	auto const self = static_cast<Range const *>(this);
	for (auto &&it : *self)
	{
		if (std::forward<decltype(it)>(it) == val)
		{
			return true;
		}
	}
	return false;
}

template<typename Collection>
template<typename Val>
constexpr bool collection_base_contains<Collection>::contains(Val &&val) const noexcept
{ return static_cast<Collection const *>(this)->as_range().contains(std::forward<Val>(val)); }

template<typename Range>
template<typename Func>
constexpr void range_base_for_each<Range>::for_each(Func &&func) const noexcept
{
	auto const self = static_cast<Range const *>(this);
	for (auto &&it : *self)
	{
		func(std::forward<decltype(it)>(it));
	}
}

template<typename Collection>
template<typename Func>
constexpr void collection_base_for_each<Collection>::for_each(Func &&func) const noexcept
{ return static_cast<Collection const *>(this)->as_range().for_each(std::forward<Func>(func)); }

template<typename Range>
constexpr auto range_base_sum<Range>::sum(void) const noexcept
{
	auto const self = static_cast<Range const *>(this);
	std::decay_t<decltype(self->operator*())> result{};
	for (auto &&it : *self)
	{
		result += std::forward<decltype(it)>(it);
	}
	return result;
}

template<typename Collection>
constexpr auto collection_base_sum<Collection>::sum(void) const noexcept
{ return static_cast<Collection const *>(this)->as_range().sum(); }

template<typename Range>
template<typename T>
constexpr auto range_base_max<Range>::max(T &&val) const noexcept
{
	auto const self = static_cast<Range const *>(this);
	std::decay_t<decltype(self->operator*())> result(std::forward<T>(val));
	for (auto &&it : *self)
	{
		if (it > result)
		{
			result = std::forward<decltype(it)>(it);
		}
	}
	return result;
}

template<typename Collection>
template<typename T>
constexpr auto collection_base_max<Collection>::max(T &&val) const noexcept
{ return static_cast<Collection const *>(this)->as_range().max(std::forward<T>(val)); }

template<typename Range>
template<typename T>
constexpr auto range_base_min<Range>::min(T &&val) const noexcept
{
	auto const self = static_cast<Range const *>(this);
	std::decay_t<decltype(self->operator*())> result{std::forward<T>(val)};
	for (auto &&it : *self)
	{
		if (it < result)
		{
			result = std::forward<decltype(it)>(it);
		}
	}
	return result;
}

template<typename Collection>
template<typename T>
constexpr auto collection_base_min<Collection>::min(T &&val) const noexcept
{ return static_cast<Collection const *>(this)->as_range().min(std::forward<T>(val)); }

template<typename Collection>
void collection_base_sort<Collection>::sort(void) noexcept
{
	auto const self = static_cast<Collection *>(this);
	std::sort(self->begin(), self->end());
}

template<typename Collection>
template<typename Cmp>
void collection_base_sort<Collection>::sort(Cmp &&cmp) noexcept
{
	auto const self = static_cast<Collection *>(this);
	std::sort(self->begin(), self->end(), std::forward<Cmp>(cmp));
}

template<typename Collection>
template<typename Range>
void collection_base_append<Collection>::append(Range &&range)
{
	auto const self = static_cast<Collection *>(this);
	for (auto &&it : std::forward<Range>(range))
	{
		self->push_back(std::forward<decltype(it)>(it));
	}
}

template<typename Collection>
template<typename Range>
void collection_base_append<Collection>::append_move(Range &&range)
{
	auto const self = static_cast<Collection *>(this);
	for (auto &&it : std::forward<Range>(range))
	{
		self->push_back(std::move(it));
	}
}

} // namespace internal

bz_end_namespace

#endif // _bz_ranges_h__
