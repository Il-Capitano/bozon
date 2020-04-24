#ifndef CTX_LEX_CONTEXT_H
#define CTX_LEX_CONTEXT_H

#include "error.h"
#include "lex/file_iterator.h"
#include "lex/token.h"

namespace ctx
{

struct global_context;

struct lex_context
{
	global_context &global_ctx;

	lex_context(global_context &_global_ctx)
		: global_ctx(_global_ctx)
	{}

//	void report_error(error &&err);
	void bad_char(
		file_iterator const &stream,
		bz::u8string message,
		bz::vector<ctx::note> notes = {}, bz::vector<ctx::suggestion> suggestions = {}
	) const;
	void bad_chars(
		bz::u8string_view file, size_t line,
		ctx::char_pos begin, ctx::char_pos pivot, ctx::char_pos end,
		bz::u8string message,
		bz::vector<ctx::note> notes = {}, bz::vector<ctx::suggestion> suggestions = {}
	) const;
	void bad_eof(
		file_iterator const &stream,
		bz::u8string message,
		bz::vector<ctx::note> notes = {}, bz::vector<ctx::suggestion> suggestions = {}
	) const;

	[[nodiscard]] static ctx::note make_note(
		file_iterator const &pos,
		bz::u8string message
	);
	[[nodiscard]] static ctx::note make_note(
		bz::u8string_view file, size_t line,
		ctx::char_pos begin, ctx::char_pos pivot, ctx::char_pos end,
		bz::u8string message
	);
	[[nodiscard]] static ctx::note make_note(
		bz::u8string_view file, size_t line,
		ctx::char_pos pivot,
		bz::u8string message
	);

	[[nodiscard]] static ctx::suggestion make_suggestion(
		file_iterator const &pos,
		bz::u8string suggestion_str,
		bz::u8string message
	);
	[[nodiscard]] static ctx::suggestion make_suggestion(
		bz::u8string_view file, size_t line, ctx::char_pos pos,
		bz::u8string suggestion_str,
		bz::u8string message
	);
	[[nodiscard]] static ctx::suggestion make_suggestion(
		bz::u8string_view file, size_t line, ctx::char_pos pos,
		ctx::char_pos erase_begin, ctx::char_pos erase_end,
		bz::u8string suggestion_str,
		bz::u8string message
	);
};

} // namespace ctx

#endif // CTX_LEX_CONTEXT_H
