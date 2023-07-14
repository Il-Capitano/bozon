#ifndef CTX_LEX_CONTEXT_H
#define CTX_LEX_CONTEXT_H

#include "core.h"
#include "error.h"
#include "context_forward.h"
#include "lex/file_iterator.h"
#include "lex/token.h"

namespace ctx
{

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
		bz::vector<source_highlight> notes = {}, bz::vector<source_highlight> suggestions = {}
	) const;
	void bad_char(
		file_iterator const &stream,
		char_pos end,
		bz::u8string message,
		bz::vector<source_highlight> notes = {}, bz::vector<source_highlight> suggestions = {}
	) const;
	void bad_chars(
		uint32_t file_id, uint32_t line,
		char_pos begin, char_pos pivot, char_pos end,
		bz::u8string message,
		bz::vector<source_highlight> notes = {}, bz::vector<source_highlight> suggestions = {}
	) const;
	void bad_eof(
		file_iterator const &stream,
		bz::u8string message,
		bz::vector<source_highlight> notes = {}, bz::vector<source_highlight> suggestions = {}
	) const;

	void report_warning(
		warning_kind kind,
		uint32_t file_id, uint32_t line,
		char_pos begin, char_pos pivot, char_pos end,
		bz::u8string message,
		bz::vector<source_highlight> notes = {}, bz::vector<source_highlight> suggestions = {}
	) const;
	void report_warning(
		warning_kind kind,
		uint32_t file_id, uint32_t line,
		char_pos it,
		bz::u8string message,
		bz::vector<source_highlight> notes = {}, bz::vector<source_highlight> suggestions = {}
	) const;

	[[nodiscard]] static source_highlight make_note(
		file_iterator const &pos,
		bz::u8string message
	);
	[[nodiscard]] static source_highlight make_note(
		uint32_t file_id, uint32_t line,
		bz::u8string message
	);
	[[nodiscard]] static source_highlight make_note(
		uint32_t file_id, uint32_t line,
		char_pos pivot,
		bz::u8string message
	);
	[[nodiscard]] static source_highlight make_note(
		uint32_t file_id, uint32_t line,
		char_pos begin, char_pos pivot, char_pos end,
		bz::u8string message
	);
	[[nodiscard]] static source_highlight make_note(
		uint32_t file_id, uint32_t line,
		char_pos pivot, bz::u8string message,
		char_pos suggesetion_pos, bz::u8string suggestion_str
	);

	[[nodiscard]] static source_highlight make_suggestion(
		file_iterator const &pos,
		bz::u8string suggestion_str,
		bz::u8string message
	);
	[[nodiscard]] static source_highlight make_suggestion(
		uint32_t file_id, uint32_t line, char_pos pos,
		bz::u8string suggestion_str,
		bz::u8string message
	);
	[[nodiscard]] static source_highlight make_suggestion(
		uint32_t file_id, uint32_t line, char_pos pos,
		char_pos erase_begin, char_pos erase_end,
		bz::u8string suggestion_str,
		bz::u8string message
	);
};

} // namespace ctx

#endif // CTX_LEX_CONTEXT_H
