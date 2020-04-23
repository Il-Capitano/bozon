#ifndef CTX_ERROR_H
#define CTX_ERROR_H

#include "core.h"
#include "lex/token.h"

namespace ctx
{

using char_pos  = bz::u8string_view::const_iterator;

struct note
{
	bz::u8string file;
	size_t line;

	char_pos src_begin;
	char_pos src_pivot;
	char_pos src_end;

	bz::u8string message;
};

struct suggestion
{
	bz::u8string file;
	size_t line;

	char_pos place;
	char_pos erase_begin;
	char_pos erase_end;
	bz::u8string suggestion_str;

	bz::u8string message;
};

struct error
{
	bz::u8string file;
	size_t line;

	char_pos src_begin;
	char_pos src_pivot;
	char_pos src_end;

	bz::u8string message;

	bz::vector<note> notes;
	bz::vector<suggestion> suggestions;
};

[[nodiscard]] inline error make_error(
	lex::token_pos it
)
{
	return {
		it->src_pos.file_name, it->src_pos.line,
		it->src_pos.begin, it->src_pos.begin, it->src_pos.end,
		bz::format("unexpected token '{}'", it->value),
		{}, {}
	};
}

[[nodiscard]] inline error make_error(
	lex::token_pos it,
	bz::u8string message,
	bz::vector<note> notes = {}, bz::vector<suggestion> suggestions = {}
)
{
	return {
		it->src_pos.file_name, it->src_pos.line,
		it->src_pos.begin, it->src_pos.begin, it->src_pos.end,
		std::move(message), std::move(notes), std::move(suggestions)
	};
}

[[nodiscard]] inline error make_error(
	lex::token_pos begin, lex::token_pos pivot, lex::token_pos end,
	bz::u8string message,
	bz::vector<note> notes = {}, bz::vector<suggestion> suggestions = {}
)
{
	bz_assert(end > begin);
	return {
		pivot->src_pos.file_name, pivot->src_pos.line,
		begin->src_pos.begin, pivot->src_pos.begin, (end - 1)->src_pos.end,
		std::move(message), std::move(notes), std::move(suggestions)
	};
}

template<typename T>
[[nodiscard]] inline error make_error(
	T const &tokens,
	bz::u8string message,
	bz::vector<note> notes = {}, bz::vector<suggestion> suggestions = {}
)
{
	return make_error(
		tokens.get_tokens_begin(), tokens.get_tokens_pivot(), tokens.get_tokens_end(),
		std::move(message), std::move(notes), std::move(suggestions)
	);
}

[[nodiscard]] inline note make_note(
	lex::token_pos it,
	bz::u8string message
)
{
	return {
		it->src_pos.file_name, it->src_pos.line,
		it->src_pos.begin, it->src_pos.begin, it->src_pos.end,
		std::move(message)
	};
}

[[nodiscard]] inline note make_note(
	lex::token_pos begin, lex::token_pos pivot, lex::token_pos end,
	bz::u8string message
)
{
	return {
		pivot->src_pos.file_name, pivot->src_pos.line,
		begin->src_pos.begin, pivot->src_pos.begin, end->src_pos.end,
		std::move(message)
	};
}

template<typename T>
[[nodiscard]] inline note make_note(
	T const &tokens,
	bz::u8string message
)
{
	return make_note(
		tokens.get_tokens_begin(), tokens.get_tokens_pivot(), tokens.get_tokens_end(),
		std::move(message)
	);
}

[[nodiscard]] inline suggestion make_suggestion_after(
	lex::token_pos it, bz::u8string suggestion_str,
	bz::u8string message
)
{
	return {
		it->src_pos.file_name, it->src_pos.line,
		it->src_pos.end, char_pos(), char_pos(),
		std::move(suggestion_str),
		std::move(message)
	};
}

} // namespace ctx

#endif // CTX_ERROR_H
