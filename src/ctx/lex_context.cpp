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
	bz::vector<source_highlight> notes, bz::vector<source_highlight> suggestions
) const
{
	this->global_ctx.report_error(error{
		warning_kind::_last,
		{
			stream.file_id, stream.line,
			stream.it, stream.it, stream.it + 1,
			suggestion_range{}, suggestion_range{},
			std::move(message),
		},
		std::move(notes), std::move(suggestions)
	});
}

void lex_context::bad_char(
	file_iterator const &stream,
	char_pos end,
	bz::u8string message,
	bz::vector<source_highlight> notes, bz::vector<source_highlight> suggestions
) const
{
	this->global_ctx.report_error(ctx::error{
		warning_kind::_last,
		{
			stream.file_id, stream.line,
			stream.it, stream.it, stream.it == end ? stream.it : stream.it + 1,
			suggestion_range{}, suggestion_range{},
			std::move(message),
		},
		std::move(notes), std::move(suggestions)
	});
}

void lex_context::bad_chars(
	uint32_t file_id, uint32_t line,
	ctx::char_pos begin, ctx::char_pos pivot, ctx::char_pos end,
	bz::u8string message,
	bz::vector<source_highlight> notes, bz::vector<source_highlight> suggestions
) const
{
	this->global_ctx.report_error(ctx::error{
		warning_kind::_last,
		{
			file_id, line,
			begin, pivot, end,
			suggestion_range{}, suggestion_range{},
			std::move(message),
		},
		std::move(notes), std::move(suggestions)
	});
}

void lex_context::bad_eof(
	file_iterator const &stream,
	bz::u8string message,
	bz::vector<source_highlight> notes, bz::vector<source_highlight> suggestions
) const
{
	this->global_ctx.report_error(ctx::error{
		warning_kind::_last,
		{
			stream.file_id, stream.line,
			stream.it, stream.it, stream.it,
			suggestion_range{}, suggestion_range{},
			std::move(message),
		},
		std::move(notes), std::move(suggestions)
	});
}

void lex_context::report_warning(
	warning_kind kind,
	uint32_t file_id, uint32_t line,
	ctx::char_pos begin, ctx::char_pos pivot, ctx::char_pos end,
	bz::u8string message,
	bz::vector<source_highlight> notes, bz::vector<source_highlight> suggestions
) const
{
	this->global_ctx.report_warning(ctx::error{
		kind,
		{
			file_id, line,
			begin, pivot, end,
			suggestion_range{}, suggestion_range{},
			std::move(message),
		},
		std::move(notes), std::move(suggestions)
	});
}

void lex_context::report_warning(
	warning_kind kind,
	uint32_t file_id, uint32_t line,
	ctx::char_pos it,
	bz::u8string message,
	bz::vector<source_highlight> notes, bz::vector<source_highlight> suggestions
) const
{
	this->global_ctx.report_warning(ctx::error{
		kind,
		{
			file_id, line,
			it, it, it + 1,
			suggestion_range{}, suggestion_range{},
			std::move(message),
		},
		std::move(notes), std::move(suggestions)
	});
}


[[nodiscard]] source_highlight lex_context::make_note(
	file_iterator const &pos,
	bz::u8string message
)
{
	return source_highlight{
		pos.file_id, pos.line,
		pos.it, pos.it, pos.it + 1,
		{}, {},
		std::move(message)
	};
}

[[nodiscard]] source_highlight lex_context::make_note(
	uint32_t file_id, uint32_t line,
	bz::u8string message
)
{
	return source_highlight{
		file_id, line,
		char_pos(), char_pos(), char_pos(),
		{}, {},
		std::move(message)
	};
}

[[nodiscard]] source_highlight lex_context::make_note(
	uint32_t file_id, uint32_t line,
	ctx::char_pos pivot,
	bz::u8string message
)
{
	return source_highlight{
		file_id, line,
		pivot, pivot, pivot + 1,
		{}, {},
		std::move(message)
	};
}

[[nodiscard]] source_highlight lex_context::make_note(
	uint32_t file_id, uint32_t line,
	ctx::char_pos begin, ctx::char_pos pivot, ctx::char_pos end,
	bz::u8string message
)
{
	return source_highlight{
		file_id, line,
		begin, pivot, end,
		{}, {},
		std::move(message)
	};
}

[[nodiscard]] source_highlight lex_context::make_note(
	uint32_t file_id, uint32_t line,
	ctx::char_pos pivot, bz::u8string message,
	ctx::char_pos suggesetion_pos, bz::u8string suggestion_str
)
{
	return source_highlight{
		file_id, line,
		pivot, pivot, pivot + 1,
		{ char_pos(), char_pos(), suggesetion_pos, suggestion_str },
		{},
		std::move(message)
	};
}


[[nodiscard]] source_highlight lex_context::make_suggestion(
	file_iterator const &pos,
	bz::u8string suggestion_str,
	bz::u8string message
)
{
	return source_highlight{
		pos.file_id, pos.line,
		char_pos(), char_pos(), char_pos(),
		suggestion_range{ char_pos(), char_pos(), pos.it, std::move(suggestion_str) },
		suggestion_range{},
		std::move(message)
	};
}

[[nodiscard]] source_highlight lex_context::make_suggestion(
	uint32_t file_id, uint32_t line, ctx::char_pos pos,
	bz::u8string suggestion_str,
	bz::u8string message
)
{
	return source_highlight{
		file_id, line,
		char_pos(), char_pos(), char_pos(),
		suggestion_range{ ctx::char_pos(), ctx::char_pos(), pos, std::move(suggestion_str) },
		suggestion_range{},
		std::move(message)
	};
}

[[nodiscard]] source_highlight lex_context::make_suggestion(
	uint32_t file_id, uint32_t line, ctx::char_pos pos,
	ctx::char_pos erase_begin, ctx::char_pos erase_end,
	bz::u8string suggestion_str,
	bz::u8string message
)
{
	return source_highlight{
		file_id, line,
		char_pos(), char_pos(), char_pos(),
		suggestion_range{ erase_begin, erase_end, pos, std::move(suggestion_str) },
		suggestion_range{},
		std::move(message)
	};
}

} // namespace ctx
