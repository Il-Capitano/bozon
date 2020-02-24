#ifndef LEX_LEXER_H
#define LEX_LEXER_H

#include "core.h"

#include "token.h"
#include "ctx/error.h"
#include "ctx/lex_context.h"

namespace lex
{

bz::vector<token> get_tokens(
	bz::string_view file,
	bz::string_view file_name,
	bz::vector<ctx::error> &errors
);

} // namespace lex

#endif // LEX_LEXER_H
