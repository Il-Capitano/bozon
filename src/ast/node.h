#ifndef AST_NODE_H
#define AST_NODE_H

#include "core.h"
#include "lex/lexer.h"

namespace ast
{

template<typename ...Ts>
struct node : public bz::variant<std::unique_ptr<Ts>...>
{
	using base_t = bz::variant<std::unique_ptr<Ts>...>;
	using self_t = node<Ts...>;

	template<typename T>
	static constexpr auto index = base_t::template index_of<std::unique_ptr<T>>;

	auto kind(void) const noexcept
	{ return this->base_t::index(); }

	using base_t::get;
	using base_t::variant;
	using base_t::emplace;

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
					std::make_unique<Ts>(*other.template get<std::unique_ptr<Ts>>())
				);
			} ...
		};
		emplacers[other.kind()](*this, other);
	}

	node(self_t &&) noexcept = default;

	self_t &operator = (self_t const &rhs)
	{
		if (rhs.kind() == base_t::null)
		{
			return *this;
		}

		using fn_t = void (*)(self_t &, self_t const &);
		fn_t emplacers[] = {
			[](self_t &self, self_t const &other)
			{
				self.template emplace<std::unique_ptr<Ts>>(
					std::make_unique<Ts>(*other.template get<std::unique_ptr<Ts>>())
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
};

} // namespace ast

#endif // AST_NODE_H
