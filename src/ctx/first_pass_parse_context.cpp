#include "first_pass_parse_context.h"
#include "global_context.h"
#include "lex/lexer.h"

namespace ctx
{

void first_pass_parse_context::report_error(lex::token_pos it) const
{
	this->global_ctx.report_error(ctx::make_error(it));
}

void first_pass_parse_context::report_error(
	lex::token_pos it, bz::u8string message,
	bz::vector<note> notes, bz::vector<suggestion> suggestions
) const
{
	this->global_ctx.report_error(error{
		warning_kind::_last,
		it->src_pos.file_id, it->src_pos.line,
		it->src_pos.begin, it->src_pos.begin, it->src_pos.end,
		std::move(message),
		std::move(notes), std::move(suggestions)
	});
}

void first_pass_parse_context::report_error(
	lex::src_tokens tokens, bz::u8string message,
	bz::vector<note> notes, bz::vector<suggestion> suggestions
) const
{
	this->global_ctx.report_error(error{
		warning_kind::_last,
		tokens.pivot->src_pos.file_id, tokens.pivot->src_pos.line,
		tokens.begin->src_pos.begin, tokens.pivot->src_pos.begin, (tokens.end - 1)->src_pos.end,
		std::move(message),
		std::move(notes), std::move(suggestions)
	});
}

void first_pass_parse_context::report_paren_match_error(
	lex::token_pos it, lex::token_pos open_paren_it
) const
{
	auto const message = [&]() {
		switch (open_paren_it->kind)
		{
		case lex::token::paren_open:
			return it->kind == lex::token::eof
				? bz::u8string("expected closing ) before end-of-file")
				: bz::format("expected closing ) before '{}'", it->value);
		case lex::token::square_open:
			return it->kind == lex::token::eof
				? bz::u8string("expected closing ] before end-of-file")
				: bz::format("expected closing ] before '{}'", it->value);
		case lex::token::curly_open:
			return it->kind == lex::token::eof
				? bz::u8string("expected closing } before end-of-file")
				: bz::format("expected closing } before '{}'", it->value);
		default:
			bz_unreachable;
			return bz::u8string();
		}
	}();
	this->report_error(
		it, message,
		{ this->make_paren_match_note(it, open_paren_it) }
	);
}

void first_pass_parse_context::report_warning(
	warning_kind kind,
	lex::token_pos it, bz::u8string message,
	bz::vector<note> notes, bz::vector<suggestion> suggestions
) const
{
	this->global_ctx.report_warning(error{
		kind,
		it->src_pos.file_id, it->src_pos.line,
		it->src_pos.begin, it->src_pos.begin, it->src_pos.end,
		std::move(message),
		std::move(notes), std::move(suggestions)
	});
}

void first_pass_parse_context::report_warning(
	warning_kind kind,
	lex::src_tokens tokens, bz::u8string message,
	bz::vector<note> notes, bz::vector<suggestion> suggestions
) const
{
	this->global_ctx.report_warning(error{
		kind,
		tokens.pivot->src_pos.file_id, tokens.pivot->src_pos.line,
		tokens.begin->src_pos.begin, tokens.pivot->src_pos.begin, (tokens.end - 1)->src_pos.end,
		std::move(message),
		std::move(notes), std::move(suggestions)
	});
}

[[nodiscard]] ctx::note first_pass_parse_context::make_note(
	lex::token_pos it, bz::u8string message
)
{
	return ctx::note{
		it->src_pos.file_id, it->src_pos.line,
		it->src_pos.begin, it->src_pos.begin, it->src_pos.end,
		{}, {},
		std::move(message)
	};
}

[[nodiscard]] ctx::note first_pass_parse_context::make_note(
	lex::src_tokens tokens, bz::u8string message
)
{
	return ctx::note{
		tokens.pivot->src_pos.file_id, tokens.pivot->src_pos.line,
		tokens.begin->src_pos.begin, tokens.pivot->src_pos.begin, (tokens.end - 1)->src_pos.end,
		{}, {},
		std::move(message)
	};
}

[[nodiscard]] ctx::note first_pass_parse_context::make_note(
	lex::token_pos it, bz::u8string message,
	ctx::char_pos suggestion_pos, bz::u8string suggestion_str
)
{
	return ctx::note{
		it->src_pos.file_id, it->src_pos.line,
		it->src_pos.begin, it->src_pos.begin, it->src_pos.end,
		{ ctx::char_pos(), ctx::char_pos(), suggestion_pos, std::move(suggestion_str) },
		{},
		std::move(message)
	};
}

[[nodiscard]] ctx::note first_pass_parse_context::make_note(
	lex::src_tokens tokens, bz::u8string message,
	ctx::char_pos suggestion_pos, bz::u8string suggestion_str
)
{
	return ctx::note{
		tokens.pivot->src_pos.file_id, tokens.pivot->src_pos.line,
		tokens.begin->src_pos.begin, tokens.pivot->src_pos.begin, (tokens.end - 1)->src_pos.end,
		{ ctx::char_pos(), ctx::char_pos(), suggestion_pos, std::move(suggestion_str) },
		{},
		std::move(message)
	};
}

[[nodiscard]] ctx::note first_pass_parse_context::make_paren_match_note(
	lex::token_pos it, lex::token_pos open_paren_it
)
{
	if (open_paren_it->kind == lex::token::curly_open)
	{
		return make_note(open_paren_it, "to match this:");
	}

	bz_assert(open_paren_it->kind == lex::token::paren_open || open_paren_it->kind == lex::token::square_open);
	auto const suggestion_str = open_paren_it->kind == lex::token::paren_open ? ")" : "]";
	auto const [suggested_paren_pos, suggested_paren_line] = [&]() {
		switch (it->kind)
		{
		case lex::token::paren_close:
			if ((open_paren_it - 1)->kind == lex::token::paren_open && (open_paren_it - 1)->src_pos.end == open_paren_it->src_pos.begin)
			{
				return std::make_pair(it->src_pos.begin, it->src_pos.line);
			}
			else
			{
				return std::make_pair((it - 1)->src_pos.end, (it - 1)->src_pos.line);
			}
		case lex::token::square_close:
			if ((open_paren_it - 1)->kind == lex::token::square_open && (open_paren_it - 1)->src_pos.end == open_paren_it->src_pos.begin)
			{
				return std::make_pair(it->src_pos.begin, it->src_pos.line);
			}
			else
			{
				return std::make_pair((it - 1)->src_pos.end, (it - 1)->src_pos.line);
			}
		case lex::token::semi_colon:
			return std::make_pair(it->src_pos.begin, it->src_pos.line);
		default:
			return std::make_pair((it - 1)->src_pos.end, (it - 1)->src_pos.line);
		}
	}();
	auto const open_paren_line = open_paren_it->src_pos.line;
	bz_assert(open_paren_line <= suggested_paren_line);
	if (suggested_paren_line - open_paren_line > 1)
	{
		return make_note(open_paren_it, "to match this:");
	}
	else
	{
		return make_note(open_paren_it, "to match this:", suggested_paren_pos, suggestion_str);
	}
}


void first_pass_parse_context::check_curly_indent(lex::token_pos open, lex::token_pos close) const
{
	if (!is_warning_enabled(warning_kind::mismatched_brace_indent))
	{
		return;
	}

	bz_assert(open->kind == lex::token::curly_open);
	bz_assert(close->kind == lex::token::curly_close);
	bz_assert(open < close);

	if (open->src_pos.line == close->src_pos.line)
	{
		return;
	}

	auto const open_line_begin = [&]() {
		if (open->src_pos.line == 1)
		{
			return this->global_ctx.get_file_begin(open->src_pos.file_id);
		}
		else
		{
			auto it = open->src_pos.begin.data();
			while (*(it - 1) != '\n')
			{
				--it;
			}
			return char_pos(it);
		}
	}();

	auto open_it = open_line_begin;
	auto c = *open_it;
	while (c == '\t' /* || c == ' ' */)
	{
		++open_it;
		c = *open_it;
	}

	auto const open_indent = bz::u8string_view(open_line_begin, open_it);

	auto const close_line_begin = [&]() {
		auto it = close->src_pos.begin.data();
		while (*(it - 1) != '\n')
		{
			--it;
		}
		return char_pos(it);
	}();

	auto close_it = close_line_begin;
	c = *close_it;
	while (c == '\t' /* || c == ' ' */)
	{
		++close_it;
		c = *close_it;
	}

	auto const close_indent = bz::u8string_view(close_line_begin, close_it);

	if (open_indent != close_indent)
	{
		this->report_warning(
			warning_kind::mismatched_brace_indent,
			close, "mismatched indentation of braces",
			{ make_note(open, "opening brace is here:") }
		);
	}
}


lex::token_pos first_pass_parse_context::assert_token(lex::token_pos &stream, uint32_t kind) const
{
	if (stream->kind != kind)
	{
		auto suggestions = kind == lex::token::semi_colon
			? bz::vector<suggestion>{ make_suggestion_after(stream - 1, ";", "add ';' here:") }
			: bz::vector<suggestion>{};
		this->global_ctx.report_error(ctx::make_error(
			stream,
			stream->kind == lex::token::eof
			? bz::format("expected {} before end-of-file", lex::get_token_name_for_message(kind))
			: bz::format("expected {}", lex::get_token_name_for_message(kind)),
			{}, std::move(suggestions)
		));
		return stream;
	}
	else
	{
		auto t = stream;
		++stream;
		return t;
	}
}

lex::token_pos first_pass_parse_context::assert_token(lex::token_pos &stream, uint32_t kind1, uint32_t kind2) const
{
	if (stream->kind != kind1 && stream->kind != kind2)
	{
		this->global_ctx.report_error(ctx::make_error(
			stream,
			stream->kind == lex::token::eof
			? bz::format(
				"expected {} or {} before end-of-file",
				lex::get_token_name_for_message(kind1), lex::get_token_name_for_message(kind2)
			)
			: bz::format(
				"expected {} or {}",
				lex::get_token_name_for_message(kind1), lex::get_token_name_for_message(kind2)
			)
		));
		return stream;
	}
	else
	{
		auto t = stream;
		++stream;
		return t;
	}
}


} // namespace ctx
