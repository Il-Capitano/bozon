#include "first_pass_parse_context.h"
#include "global_context.h"

namespace ctx
{

void first_pass_parse_context::report_error(lex::token_pos it) const
{
	this->global_ctx.report_error(ctx::make_error(it));
}

void first_pass_parse_context::report_error(
	lex::token_pos it,
	bz::u8string message, bz::vector<ctx::note> notes
) const
{
	this->global_ctx.report_error(ctx::make_error(
		it, std::move(message), std::move(notes)
	));
}

void first_pass_parse_context::report_error(
	lex::token_pos begin, lex::token_pos pivot, lex::token_pos end,
	bz::u8string message, bz::vector<ctx::note> notes
) const
{
	this->global_ctx.report_error(ctx::make_error(
		begin, pivot, end, std::move(message), std::move(notes)
	));
}

[[nodiscard]] ctx::note first_pass_parse_context::make_note(
	lex::token_pos it, bz::u8string message
)
{
	return ctx::note{
		it->src_pos.file_name, it->src_pos.line,
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
		tokens.pivot->src_pos.file_name, tokens.pivot->src_pos.line,
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
		it->src_pos.file_name, it->src_pos.line,
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
		tokens.pivot->src_pos.file_name, tokens.pivot->src_pos.line,
		tokens.begin->src_pos.begin, tokens.pivot->src_pos.begin, (tokens.end - 1)->src_pos.end,
		{ ctx::char_pos(), ctx::char_pos(), suggestion_pos, std::move(suggestion_str) },
		{},
		std::move(message)
	};
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
