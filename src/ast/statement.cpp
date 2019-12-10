#include "statement.h"
#include "../context.h"

namespace ast
{

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
	this->visit([](auto &stmt) { stmt->resolve(); });
}


void decl_variable::resolve(void)
{

}

template<>
void stmt_declaration::resolve(void)
{
	switch (this->kind())
	{
	case index<decl_variable>:
	{
		auto &var_decl = this->get<decl_variable_ptr>();

		if (var_decl->init_expr.has_value())
		{
			var_decl->init_expr->resolve();
		}

		if (var_decl->var_type.kind() == typespec::null)
		{
			assert(var_decl->init_expr.has_value());
			var_decl->var_type = get_typespec(var_decl->init_expr.get());
		}
		else
		{
			var_decl->var_type.resolve();
		}

		context.add_variable(var_decl->identifier, var_decl->var_type);

		return;
	}

	case index<decl_function>:
	{
		auto &fn_decl = this->get<decl_function_ptr>();
		++context;
		for (auto &p : fn_decl->params)
		{
			p.type.resolve();
			context.add_variable(p.id, p.type);
		}
		fn_decl->return_type.resolve();
		context.add_function(fn_decl);
		fn_decl->body->resolve();
		--context;
		return;
	}

	case index<decl_operator>:
	{
		auto &op_decl = this->get<decl_operator_ptr>();
		++context;
		for (auto &p : op_decl->params)
		{
			p.type.resolve();
			context.add_variable(p.id, p.type);
		}
		op_decl->return_type.resolve();
		context.add_operator(op_decl);
		op_decl->body->resolve();
		--context;
		return;
	}

	case index<decl_struct>:
		assert(false);
		return;

	default:
		assert(false);
		return;
	}
}

} // namespace ast
