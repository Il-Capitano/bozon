#include "statement.h"

namespace ast
{

lex::token_pos decl_variable::get_tokens_begin(void) const
{ return this->tokens.begin; }

lex::token_pos decl_variable::get_tokens_pivot(void) const
{ return this->identifier; }

lex::token_pos decl_variable::get_tokens_end(void) const
{ return this->tokens.end; }

} // namespace ast
