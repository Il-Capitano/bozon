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
