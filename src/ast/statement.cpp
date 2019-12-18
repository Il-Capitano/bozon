#include "statement.h"
#include "../context.h"

namespace ast
{

src_file::token_pos decl_variable::get_tokens_begin(void) const
{ return this->tokens.begin; }

src_file::token_pos decl_variable::get_tokens_pivot(void) const
{ return this->identifier; }

src_file::token_pos decl_variable::get_tokens_end(void) const
{ return this->tokens.end; }


void stmt_if::resolve(void)
{
	this->condition.resolve();
	this->then_block.resolve();
	if (this->else_block.has_value())
	{
		this->else_block->resolve();
	}
}

void stmt_while::resolve(void)
{
	this->condition.resolve();
	this->while_block.resolve();
}

void stmt_for::resolve(void)
{
	assert(false);
}

void stmt_return::resolve(void)
{
	this->expr.resolve();
}

void stmt_compound::resolve(void)
{
	++context;
	for (auto &stmt : this->statements)
	{
		stmt.resolve();
	}
	--context;
}

void stmt_expression::resolve(void)
{
	this->expr.resolve();
}

template<>
void statement::resolve(void)
{
	assert(this->kind() != null);
	this->visit([](auto &stmt) { stmt->resolve(); });
}


void decl_variable::resolve(void)
{
	if (this->init_expr.has_value())
	{
		this->init_expr->resolve();
	}

	if (this->var_type.kind() == typespec::null)
	{
		assert(this->init_expr.has_value());
		this->var_type = decay_typespec(this->init_expr.get().expr_type);
	}
	else
	{
		this->var_type.resolve();
		if (this->init_expr.has_value())
		{
			if (!context.is_convertible(this->init_expr.get(), this->var_type))
			{
				bad_tokens(
					this->get_tokens_begin(),
					this->get_tokens_pivot(),
					this->get_tokens_end(),
					"Error: Cannot convert initializer expression from {} to {}",
					this->init_expr.get().expr_type, this->var_type
				);
			}
		}
	}

	context.add_variable(this->identifier, this->var_type);

	return;
}

void decl_function::resolve(void)
{
	++context;
	for (auto &p : this->params)
	{
		p.var_type.resolve();
		context.add_variable(p.id, p.var_type);
	}
	this->return_type.resolve();
	context.add_function(*this);
	this->body.resolve();
	--context;
	return;
}

void decl_operator::resolve(void)
{
	++context;
	for (auto &p : this->params)
	{
		p.var_type.resolve();
		context.add_variable(p.id, p.var_type);
	}
	this->return_type.resolve();
	context.add_operator(*this);
	this->body.resolve();
	--context;
	return;
}

void decl_struct::resolve(void)
{
	for (auto &var : this->member_variables)
	{
		var.var_type.resolve();
	}

	context.add_type(*this);
	return;
}

template<>
void stmt_declaration::resolve(void)
{
	assert(this->kind() != null);
	this->visit([](auto &decl) { decl->resolve(); });
}

} // namespace ast
