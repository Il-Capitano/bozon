#ifndef CTX_ERROR_H
#define CTX_ERROR_H

#include "core.h"
#include "lex/token.h"
#include "warnings.h"
#include "context_forward.h"

namespace ctx
{

using char_pos  = bz::u8string_view::const_iterator;

struct suggestion_range
{
	char_pos      erase_begin    = char_pos();
	char_pos      erase_end      = char_pos();
	char_pos      suggestion_pos = char_pos();
	bz::u8string  suggestion_str = {};
};

struct source_highlight
{
	uint32_t file_id;
	uint32_t line;

	char_pos src_begin;
	char_pos src_pivot;
	char_pos src_end;

	suggestion_range first_suggestion;
	suggestion_range second_suggestion;

	bz::u8string message;
};

struct error
{
	// equals warning_kind::_last if it's an error
	warning_kind kind;
	source_highlight src_highlight;

	bz::vector<source_highlight> notes;
	bz::vector<source_highlight> suggestions;

	bool is_error(void) const noexcept
	{ return this->kind == warning_kind::_last; }

	bool is_warning(void) const noexcept
	{ return this->kind != warning_kind::_last; }
};

void print_error_or_warning(error const &err, global_context &context);

} // namespace ctx

#endif // CTX_ERROR_H
