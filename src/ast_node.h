#ifndef AST_NODE_H
#define AST_NODE_H

#include "core.h"
#include "lexer.h"

template<typename ...Ts>
struct ast_node : public bz::variant<std::unique_ptr<Ts>...>
{
	using base_t = bz::variant<std::unique_ptr<Ts>...>;

	template<typename T>
	static constexpr auto index = base_t::template index_of<std::unique_ptr<T>>;

	uint32_t kind(void) const noexcept
	{ return this->base_t::index(); }

	using base_t::get;
	using base_t::variant;

	void resolve(void);

	template<int = 0>
	auto get_tokens_begin(void) const
	{
		return this->visit(
			[](auto const &elem) -> src_tokens::pos
			{
				return elem->get_tokens_begin();
			}
		);
	}

	template<int = 0>
	auto get_tokens_pivot(void) const
	{
		return this->visit(
			[](auto const &elem) -> src_tokens::pos
			{
				return elem->get_tokens_pivot();
			}
		);
	}

	template<int = 0>
	auto get_tokens_end(void) const
	{
		return this->visit(
			[](auto const &elem) -> src_tokens::pos
			{
				return elem->get_tokens_end();
			}
		);
	}
};

#endif // AST_NODE_H
