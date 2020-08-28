#ifndef CTX_COMMAND_PARSE_CONTEXT_H
#define CTX_COMMAND_PARSE_CONTEXT_H

#include "core.h"
#include "warnings.h"

namespace ctx
{

struct global_context;

struct command_parse_context
{
	using iter_t = bz::array_view<bz::u8string_view const>::const_iterator;
	bz::array_view<bz::u8string_view const> args;
	global_context &global_ctx;

	command_parse_context(bz::array_view<bz::u8string_view const> _args, global_context &_global_ctx)
		: args(_args), global_ctx(_global_ctx)
	{}

	void report_error(iter_t it, bz::u8string message);
	void report_warning(warning_kind kind, iter_t it, bz::u8string message);

	uint32_t get_arg_position(bz::array_view<bz::u8string_view const>::const_iterator it)
	{
		auto const result = it - this->args.begin();
		bz_assert(result < std::numeric_limits<uint32_t>::max());
		return static_cast<uint32_t>(result);
	}

	bz::u8string_view get_arg_value(uint32_t index)
	{
		bz_assert(index > 0 && index - 1 < this->args.size());
		return this->args[index - 1];
	}
};

} // namespace ctx

#endif // CTX_COMMAND_PARSE_CONTEXT_H
