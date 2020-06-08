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
		false,
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
		false,
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
		false,
		stream.file, stream.line,
		stream.it, stream.it, stream.it,
		std::move(message),
		std::move(notes), std::move(suggestions)
	});
}


[[nodiscard]] ctx::note lex_context::make_note(
	file_iterator const &pos,
	bz::u8string message
)
{
	return ctx::note{
		pos.file, pos.line,
		pos.it, pos.it, pos.it + 1,
		{}, {},
		std::move(message)
	};
}

[[nodiscard]] ctx::note lex_context::make_note(
	bz::u8string_view file, size_t line,
	ctx::char_pos begin, ctx::char_pos pivot, ctx::char_pos end,
	bz::u8string message
)
{
	return ctx::note{
		file, line,
		begin, pivot, end,
		{}, {},
		std::move(message)
	};
}

[[nodiscard]] ctx::note lex_context::make_note(
	bz::u8string_view file, size_t line,
	ctx::char_pos pivot,
	bz::u8string message
)
{
	return ctx::note{
		file, line,
		pivot, pivot, pivot + 1,
		{}, {},
		std::move(message)
	};
}

[[nodiscard]] ctx::note lex_context::make_note(
	bz::u8string_view file, size_t line,
	ctx::char_pos pivot, bz::u8string message,
	ctx::char_pos suggesetion_pos, bz::u8string suggestion_str
)
{
	return ctx::note{
		file, line,
		pivot, pivot, pivot + 1,
		{ char_pos(), char_pos(), suggesetion_pos, suggestion_str },
		{},
		std::move(message)
	};
}


[[nodiscard]] ctx::suggestion lex_context::make_suggestion(
	file_iterator const &pos,
	bz::u8string suggestion_str,
	bz::u8string message
)
{
	return ctx::suggestion{
		pos.file, pos.line,
		{ char_pos(), char_pos(), pos.it, std::move(suggestion_str) },
		{},
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
		{ ctx::char_pos(), ctx::char_pos(), pos, std::move(suggestion_str) },
		{},
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
		{ erase_begin, erase_end, pos, std::move(suggestion_str) },
		{},
		std::move(message)
	};
}

} // namespace ctx
