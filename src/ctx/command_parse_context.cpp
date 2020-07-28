#include "command_parse_context.h"
#include "global_context.h"

namespace ctx
{

void command_parse_context::report_error(iter_t it, bz::u8string message)
{
	this->global_ctx.report_error(error{
		warning_kind::_last,
		global_context::command_line_file_id, this->get_arg_position(it),
		char_pos(), char_pos(), char_pos(),
		std::move(message),
		{}, {}
	});
}

} // namespace ctx
