#include "statement.h"
#include "../context.h"

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
	else if (
		this->var_type.kind() == typespec::index<ts_constant>
		&& this->var_type.get<ts_constant_ptr>()->base.kind() == typespec::null
	)
	{
		assert(this->init_expr.has_value());
		this->var_type = add_const(decay_typespec(this->init_expr->expr_type));
	}
	else if (
		this->var_type.kind() == typespec::index<ts_reference>
		&& this->var_type.get<ts_reference_ptr>()->base.kind() == typespec::null
	)
	{
		assert(this->init_expr.has_value());
		if (!this->init_expr->is_lvalue)
		{
			bad_tokens(
				this->get_tokens_begin(),
				this->init_expr->get_tokens_pivot(),
				this->get_tokens_end(),
				"Error: Cannot bind reference to non-lvalue"
			);
		}
		this->var_type = add_lvalue_reference(this->init_expr->expr_type);
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

void decl_variable::emit_bytecode(bz::vector<bytecode::instruction> &out)
{
	using namespace bytecode;
	auto const allocation_amount = context.get_identifier_stack_allocation_amount(this->identifier);
	auto const stack_offset = context.get_identifier_stack_offset(this->identifier);
	out.push_back(sub{
		rsp,
		rsp,
		register_value{._int64 = allocation_amount},
		type_kind::int64
	});
	auto ret_pos = value_pos_t(ptr_value(stack_offset));
	if (this->init_expr.has_value())
	{
		this->init_expr->emit_bytecode(out, ret_pos);
	}
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

void declaration::resolve(void)
{
	assert(this->kind() != null);
	this->visit([](auto &decl) { decl->resolve(); });
}

void statement::resolve(void)
{
	assert(this->kind() != null);
	this->visit([](auto &stmt) { stmt->resolve(); });
}

void statement::emit_bytecode(bz::vector<bytecode::instruction> &out)
{
	switch (this->kind())
	{
	case index<stmt_if>:
	case index<stmt_while>:
	case index<stmt_for>:
	case index<stmt_return>:
	case index<stmt_no_op>:
	case index<stmt_compound>:
		assert(false);
		break;
	case index<stmt_expression>:
		this->get<stmt_expression_ptr>()->expr.emit_bytecode(out, {});
		break;
	case index<decl_variable>:
		this->get<decl_variable_ptr>()->emit_bytecode(out);
		break;
	default:
		assert(false);
		break;
	}
}

} // namespace ast
