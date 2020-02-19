#include "statement.h"

namespace ast
{

statement::statement(declaration decl)
	: base_t()
{
	switch (decl.kind())
	{
	case declaration::index<decl_variable>:
		this->assign(std::move(decl.get<decl_variable_ptr>()));
		break;
	case declaration::index<decl_function>:
		this->assign(std::move(decl.get<decl_function_ptr>()));
		break;
	case declaration::index<decl_operator>:
		this->assign(std::move(decl.get<decl_operator_ptr>()));
		break;
	case declaration::index<decl_struct>:
		this->assign(std::move(decl.get<decl_struct_ptr>()));
		break;
	default:
		break;
	}
}

lex::token_pos decl_variable::get_tokens_begin(void) const
{ return this->tokens.begin; }

lex::token_pos decl_variable::get_tokens_pivot(void) const
{ return this->identifier; }

lex::token_pos decl_variable::get_tokens_end(void) const
{ return this->tokens.end; }

} // namespace ast
