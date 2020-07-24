#ifndef CTX_ERROR_H
#define CTX_ERROR_H

#include "core.h"
#include "lex/token.h"
#include "warnings.h"

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

struct note
{
	bz::u8string file;
	size_t line;

	char_pos src_begin;
	char_pos src_pivot;
	char_pos src_end;

	suggestion_range first_suggestion;
	suggestion_range second_suggestion;

	bz::u8string message;
};

struct suggestion
{
	bz::u8string file;
	size_t line;

	suggestion_range first_suggestion;
	suggestion_range second_suggestion;

	bz::u8string message;
};

struct error
{
	// equals warning_kind::_last if it's an error
	warning_kind kind;
	bz::u8string file;
	size_t line;

	char_pos src_begin;
	char_pos src_pivot;
	char_pos src_end;

	bz::u8string message;

	bz::vector<note> notes;
	bz::vector<suggestion> suggestions;

	bool is_error(void) const noexcept
	{ return this->kind == warning_kind::_last; }

	bool is_warning(void) const noexcept
	{ return this->kind != warning_kind::_last; }
};

[[nodiscard]] inline error make_error(
	lex::token_pos it
)
{
	return {
		warning_kind::_last,
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
		warning_kind::_last,
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
		warning_kind::_last,
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

[[nodiscard]] inline error make_warning(
	warning_kind kind,
	lex::token_pos it,
	bz::u8string message,
	bz::vector<note> notes = {}, bz::vector<suggestion> suggestions = {}
)
{
	return {
		kind,
		it->src_pos.file_name, it->src_pos.line,
		it->src_pos.begin, it->src_pos.begin, it->src_pos.end,
		std::move(message), std::move(notes), std::move(suggestions)
	};
}

[[nodiscard]] inline error make_warning(
	warning_kind kind,
	lex::token_pos begin, lex::token_pos pivot, lex::token_pos end,
	bz::u8string message,
	bz::vector<note> notes = {}, bz::vector<suggestion> suggestions = {}
)
{
	bz_assert(end > begin);
	return {
		kind,
		pivot->src_pos.file_name, pivot->src_pos.line,
		begin->src_pos.begin, pivot->src_pos.begin, (end - 1)->src_pos.end,
		std::move(message), std::move(notes), std::move(suggestions)
	};
}

template<typename T>
[[nodiscard]] inline error make_warning(
	warning_kind kind,
	T const &tokens,
	bz::u8string message,
	bz::vector<note> notes = {}, bz::vector<suggestion> suggestions = {}
)
{
	return make_warning(
		kind,
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
		{}, {},
		std::move(message)
	};
}

[[nodiscard]] inline note make_note(
	lex::src_tokens tokens,
	bz::u8string message
)
{
	return {
		tokens.pivot->src_pos.file_name, tokens.pivot->src_pos.line,
		tokens.begin->src_pos.begin, tokens.pivot->src_pos.begin, (tokens.end - 1)->src_pos.end,
		{}, {},
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

[[nodiscard]] inline note make_note_with_suggestion(
	lex::src_tokens tokens,
	lex::token_pos begin, bz::u8string begin_suggestion_str,
	lex::token_pos end, bz::u8string end_suggestion_str,
	bz::u8string message
)
{
	bz_assert(begin != nullptr && end != nullptr);
	if (tokens.pivot == nullptr)
	{
		return {
			begin->src_pos.file_name, begin->src_pos.line,
			char_pos(), char_pos(), char_pos(),
			{ char_pos(), char_pos(), begin->src_pos.begin, std::move(begin_suggestion_str) },
			{ char_pos(), char_pos(), (end - 1)->src_pos.end, std::move(end_suggestion_str) },
			std::move(message)
		};
	}
	else
	{
		return {
			tokens.pivot->src_pos.file_name, tokens.pivot->src_pos.line,
			tokens.begin->src_pos.begin, tokens.pivot->src_pos.begin, (tokens.end - 1)->src_pos.end,
			{ char_pos(), char_pos(), begin->src_pos.begin, std::move(begin_suggestion_str) },
			{ char_pos(), char_pos(), (end - 1)->src_pos.end, std::move(end_suggestion_str) },
			std::move(message)
		};
	}
}

[[nodiscard]] inline suggestion make_suggestion_after(
	lex::token_pos it, bz::u8string suggestion_str,
	bz::u8string message
)
{
	bz_assert(it != nullptr);
	return {
		it->src_pos.file_name, it->src_pos.line,
		{ char_pos(), char_pos(), it->src_pos.end, std::move(suggestion_str) },
		{},
		std::move(message)
	};
}

[[nodiscard]] inline suggestion make_suggestion_before(
	lex::token_pos it, bz::u8string suggestion_str,
	bz::u8string message
)
{
	bz_assert(it != nullptr);
	return {
		it->src_pos.file_name, it->src_pos.line,
		{ char_pos(), char_pos(), it->src_pos.begin, std::move(suggestion_str) },
		{},
		std::move(message)
	};
}

[[nodiscard]] inline suggestion make_suggestion_around(
	lex::token_pos begin, bz::u8string begin_suggestion_str,
	lex::token_pos end, bz::u8string end_suggestion_str,
	bz::u8string message
)
{
	return {
		begin->src_pos.file_name, begin->src_pos.line,
		{ char_pos(), char_pos(), begin->src_pos.begin, std::move(begin_suggestion_str) },
		{ char_pos(), char_pos(), (end - 1)->src_pos.end, std::move(end_suggestion_str) },
		std::move(message)
	};
}

void print_error_or_warning(
	char_pos file_begin, char_pos file_end,
	error const &err
);

} // namespace ctx

#endif // CTX_ERROR_H
