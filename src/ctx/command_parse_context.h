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

	size_t get_arg_position(bz::array_view<bz::u8string_view const>::const_iterator it)
	{
		return it - this->args.begin() + 1;
	}
};

} // namespace ctx

#endif // CTX_COMMAND_PARSE_CONTEXT_H
