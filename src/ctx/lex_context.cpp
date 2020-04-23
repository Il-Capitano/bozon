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
	bz::u8string message,
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
	bz::u8string_view file, size_t line,
	ctx::char_pos begin, ctx::char_pos pivot, ctx::char_pos end,
	bz::u8string message,
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
	bz::u8string message,
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

[[nodiscard]] ctx::suggestion lex_context::make_suggestion(
	file_iterator const &pos,
	bz::u8string suggestion_str,
	bz::u8string message
)
{
	return ctx::suggestion{
		pos.file, pos.line,
		pos.it, ctx::char_pos(), ctx::char_pos(),
		std::move(suggestion_str),
		std::move(message)
	};
}

[[nodiscard]] ctx::suggestion lex_context::make_suggestion(
	bz::u8string_view file, size_t line, ctx::char_pos pos,
	bz::u8string suggestion_str,
	bz::u8string message
)
{
	return ctx::suggestion{
		file, line,
		pos, ctx::char_pos(), ctx::char_pos(),
		std::move(suggestion_str),
		std::move(message)
	};
}

[[nodiscard]] ctx::suggestion lex_context::make_suggestion(
	bz::u8string_view file, size_t line, ctx::char_pos pos,
	ctx::char_pos erase_begin, ctx::char_pos erase_end,
	bz::u8string suggestion_str,
	bz::u8string message
)
{
	return ctx::suggestion{
		file, line,
		pos, erase_begin, erase_end,
		std::move(suggestion_str),
		std::move(message)
	};
}

} // namespace ctx
