#ifndef LEX_LEXER_H
#define LEX_LEXER_H

#include "core.h"

#include "token.h"
#include "ctx/error.h"

namespace lex
{

bz::vector<token> get_tokens(
	bz::string_view file,
	bz::string_view file_name,
	bz::vector<ctx::error> &errors
);

[[nodiscard]] inline ctx::error bad_token(
	token_pos it,
	bz::string message,
	bz::vector<ctx::note> notes = {}
)
{
	return ctx::error{
		it->src_pos.file_name, it->src_pos.line, it->src_pos.column,
		it->src_pos.begin, it->src_pos.begin, it->src_pos.end,
		std::move(message),
		std::move(notes)
	};
}


} // namespace lex

#endif // LEX_LEXER_H
