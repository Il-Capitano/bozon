#include "expression.h"

namespace ast
{

lex::token_pos expr_cast::get_tokens_begin(void) const
{ return this->expr.get_tokens_begin(); }

lex::token_pos expr_cast::get_tokens_pivot(void) const
{ return this->as_pos; }

lex::token_pos expr_cast::get_tokens_end(void) const
{ return this->type.get_tokens_end(); }

} // namespace ast
