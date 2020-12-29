#ifndef AST_NODE_H
#define AST_NODE_H

#include "core.h"
#include "lex/token.h"

namespace ast
{

template<typename ...Ts>
struct node : public bz::variant<std::unique_ptr<Ts>...>
{
	using base_t = bz::variant<std::unique_ptr<Ts>...>;
	using self_t = node<Ts...>;

	template<typename T>
	static constexpr size_t index = base_t::template index_of<std::unique_ptr<T>>;

	auto kind(void) const noexcept
	{ return this->base_t::index(); }

	template<typename T>
	bool is(void) const noexcept
	{ return index<T> == this->kind(); }

	template<typename T>
	T &get(void) noexcept
	{ return *this->base_t::template get<std::unique_ptr<T>>(); }

	template<typename T>
	T &get(void) const noexcept
	{ return *this->base_t::template get<std::unique_ptr<T>>(); }

	node(void) = default;

	template<typename T>
	node(T &&val)
		: base_t(std::forward<T>(val))
	{}


	node(self_t const &other)
		: base_t()
	{
		if (other.kind() == base_t::null)
		{
			return;
		}
		using fn_t = void (*)(self_t &, self_t const &);
		fn_t emplacers[] = {
			[](self_t &self, self_t const &other)
			{
				self.template emplace<std::unique_ptr<Ts>>(
					std::make_unique<Ts>(other.template get<Ts>())
				);
			} ...
		};
		emplacers[other.kind()](*this, other);
	}

	node(self_t &&) noexcept = default;

	self_t &operator = (self_t const &rhs)
	{
		if (this == &rhs || rhs.kind() == base_t::null)
		{
			return *this;
		}

		using fn_t = void (*)(self_t &, self_t const &);
		fn_t emplacers[] = {
			[](self_t &self, self_t const &other)
			{
				self.template emplace<std::unique_ptr<Ts>>(
					std::make_unique<Ts>(other.template get<Ts>())
				);
			} ...
		};
		emplacers[rhs.kind()](*this, rhs);

		return *this;
	}

	self_t &operator = (self_t &&) = default;

	template<int = 0>
	auto get_tokens_begin(void) const
	{
		return this->visit(
			[](auto const &elem) -> lex::token_pos
			{
				return elem->get_tokens_begin();
			}
		);
	}

	template<int = 0>
	auto get_tokens_pivot(void) const
	{
		return this->visit(
			[](auto const &elem) -> lex::token_pos
			{
				return elem->get_tokens_pivot();
			}
		);
	}

	template<int = 0>
	auto get_tokens_end(void) const
	{
		return this->visit(
			[](auto const &elem) -> lex::token_pos
			{
				return elem->get_tokens_end();
			}
		);
	}

	template<typename Fn>
	decltype(auto) visit(Fn &&fn)
	{
		static_assert(
			(bz::meta::is_invocable_v<Fn, Ts &> && ...),
			"Visitor is not invocable for one or more types"
		);
		return this->base_t::visit([&fn](auto &node) -> decltype(auto) {
			return std::forward<Fn>(fn)(*node.get());
		});
	}

	template<typename Fn>
	decltype(auto) visit(Fn &&fn) const
	{
		static_assert(
			(bz::meta::is_invocable_v<Fn, Ts const &> && ...),
			"Visitor is not invocable for one or more types"
		);
		return this->base_t::visit([&fn](auto &node) -> decltype(auto) {
			return std::forward<Fn>(fn)(*node.get());
		});
	}
};

template<typename ...Ts>
struct node_view : public bz::variant<Ts *...>
{
	using base_t = bz::variant<Ts *...>;
	using self_t = node_view<Ts...>;
	using node_t = node<Ts...>;

	template<typename T>
	static constexpr size_t index = base_t::template index_of<T *>;

	auto kind(void) const noexcept
	{ return this->base_t::index(); }

	template<typename T>
	bool is(void) const noexcept
	{ return index<T> == this->kind(); }

	template<typename T>
	T &get(void) noexcept
	{ return *this->base_t::template get<T *>(); }

	template<typename T>
	T &get(void) const noexcept
	{ return *this->base_t::template get<T *>(); }

	node_view(void) = default;

	node_view(self_t const &) noexcept = default;
	node_view(self_t &&) noexcept = default;

	self_t &operator = (self_t const &rhs) noexcept = default;
	self_t &operator = (self_t &&) = default;

	node_view(node_t &node)
	{
		if (node.is_null())
		{
			return;
		}
		using fn_t = void (*)(self_t &, node_t &);
		fn_t emplacers[] = {
			[](self_t &self, node_t &other)
			{
				self.template emplace<Ts *>(&other.template get<Ts>());
			} ...
		};
		emplacers[node.kind()](*this, node);
	}

	template<typename T>
	node_view(T *val)
		: base_t(val)
	{}


	template<int = 0>
	auto get_tokens_begin(void) const
	{
		return this->visit(
			[](auto const &elem) -> lex::token_pos
			{
				return elem->get_tokens_begin();
			}
		);
	}

	template<int = 0>
	auto get_tokens_pivot(void) const
	{
		return this->visit(
			[](auto const &elem) -> lex::token_pos
			{
				return elem->get_tokens_pivot();
			}
		);
	}

	template<int = 0>
	auto get_tokens_end(void) const
	{
		return this->visit(
			[](auto const &elem) -> lex::token_pos
			{
				return elem->get_tokens_end();
			}
		);
	}

	template<typename Fn>
	decltype(auto) visit(Fn &&fn)
	{
		static_assert(
			(bz::meta::is_invocable_v<Fn, Ts &> && ...),
			"Visitor is not invocable for one or more types"
		);
		return this->base_t::visit([&fn](auto &node) -> decltype(auto) {
			return std::forward<Fn>(fn)(*node);
		});
	}

	template<typename Fn>
	decltype(auto) visit(Fn &&fn) const
	{
		static_assert(
			(bz::meta::is_invocable_v<Fn, Ts const &> && ...),
			"Visitor is not invocable for one or more types"
		);
		return this->base_t::visit([&fn](auto &node) -> decltype(auto) {
			return std::forward<Fn>(fn)(*node);
		});
	}
};

namespace internal
{

template<typename>
struct node_from_type_pack;

template<typename ...Ts>
struct node_from_type_pack<bz::meta::type_pack<Ts...>>
{
	using type = node<Ts...>;
};

template<typename>
struct node_view_from_node_impl;

template<typename ...Ts>
struct node_view_from_node_impl<node<Ts...>>
{
	using type = node_view<Ts...>;
};

} // namespace internal

template<typename Node>
using node_view_from_node = typename internal::node_view_from_node_impl<Node>::type;

} // namespace ast

#endif // AST_NODE_H
