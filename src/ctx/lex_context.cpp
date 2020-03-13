#include "lex_context.h"
#include "global_context.h"

namespace ctx
{

// void lex_context::report_error(error &&err)
// {
// 	this->global_ctx.report_error(std::move(err));
// }

void lex_context::bad_char(
	file_iterator const &stream,
	bz::string message,
	bz::vector<ctx::note> notes, bz::vector<ctx::suggestion> suggestions
) const
{
	this->global_ctx.report_error(ctx::error{
		stream.file, stream.line,
		stream.it, stream.it, stream.it + 1,
		std::move(message),
		std::move(notes), std::move(suggestions)
	});
}

void lex_context::bad_chars(
	bz::string_view file, size_t line,
	ctx::char_pos begin, ctx::char_pos pivot, ctx::char_pos end,
	bz::string message,
	bz::vector<ctx::note> notes, bz::vector<ctx::suggestion> suggestions
) const
{
	this->global_ctx.report_error(ctx::error{
		file, line,
		begin, pivot, end,
		std::move(message),
		std::move(notes), std::move(suggestions)
	});
}

void lex_context::bad_eof(
	file_iterator const &stream,
	bz::string message,
	bz::vector<ctx::note> notes, bz::vector<ctx::suggestion> suggestions
) const
{
	this->global_ctx.report_error(ctx::error{
		stream.file, stream.line,
		stream.it, stream.it, stream.it,
		std::move(message),
		std::move(notes), std::move(suggestions)
	});
}

} // namespace ctx
