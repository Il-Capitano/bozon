#ifndef CTX_FIRST_PASS_PARSE_CONTEXT_H
#define CTX_FIRST_PASS_PARSE_CONTEXT_H

#include "core.h"
#include "error.h"

namespace ctx
{

struct global_context;

struct first_pass_parse_context
{
	global_context &global_ctx;

	first_pass_parse_context(global_context &_global_ctx)
		: global_ctx(_global_ctx)
	{}

	void report_error(lex::token_pos it) const;
	void report_error(
		lex::token_pos it, bz::u8string message,
		bz::vector<note> notes = {}, bz::vector<suggestion> suggestions = {}
	) const;
	void report_error(
		lex::token_pos begin, lex::token_pos pivot, lex::token_pos end,
		bz::u8string message,
		bz::vector<note> notes = {}, bz::vector<suggestion> suggestions = {}
	) const;
	void report_paren_match_error(
		lex::token_pos it, lex::token_pos open_paren_it
	) const;

	void report_warning(
		warning_kind kind,
		lex::token_pos it, bz::u8string message,
		bz::vector<note> notes = {}, bz::vector<suggestion> suggestions = {}
	) const;
	void report_warning(
		warning_kind kind,
		lex::token_pos begin, lex::token_pos pivot, lex::token_pos end,
		bz::u8string message,
		bz::vector<note> notes = {}, bz::vector<suggestion> suggestions = {}
	) const;

	[[nodiscard]] static ctx::note make_note(
		lex::token_pos it, bz::u8string message
	);
	[[nodiscard]] static ctx::note make_note(
		lex::src_tokens tokens, bz::u8string message
	);
	[[nodiscard]] static ctx::note make_note(
		lex::token_pos it, bz::u8string message,
		ctx::char_pos suggestion_pos, bz::u8string suggestion_str
	);
	[[nodiscard]] static ctx::note make_note(
		lex::src_tokens tokens, bz::u8string message,
		ctx::char_pos suggestion_pos, bz::u8string suggestion_str
	);

	[[nodiscard]] static ctx::note make_paren_match_note(
		lex::token_pos it, lex::token_pos open_paren_it
	);

	void check_curly_indent(lex::token_pos open, lex::token_pos close) const;

	lex::token_pos assert_token(lex::token_pos &stream, uint32_t kind) const;
	lex::token_pos assert_token(lex::token_pos &stream, uint32_t kind1, uint32_t kind2) const;
};

} // namespace ctx


#endif // CTX_FIRST_PASS_PARSE_CONTEXT_H
