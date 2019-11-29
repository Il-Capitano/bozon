#include "ast_statement.h"
#include "context.h"

void ast_stmt_if::resolve(void)
{
	this->condition.resolve();
	this->then_block.resolve();
	if (this->else_block.has_value())
	{
		this->else_block->resolve();
	}
}

void ast_stmt_while::resolve(void)
{
	this->condition.resolve();
	this->while_block.resolve();
}

void ast_stmt_for::resolve(void)
{
	assert(false);
}

void ast_stmt_return::resolve(void)
{
	this->expr.resolve();
}

void ast_stmt_compound::resolve(void)
{
	++context;
	for (auto &stmt : this->statements)
	{
		stmt.resolve();
	}
	--context;
}

void ast_stmt_expression::resolve(void)
{
	this->expr.resolve();
}

template<>
void ast_statement::resolve(void)
{
	this->visit([](auto &stmt) { stmt->resolve(); });
}

template<>
void ast_stmt_declaration::resolve(void)
{
	switch (this->kind())
	{
	case index<ast_decl_variable>:
	{
		auto &var_decl = this->get<ast_decl_variable_ptr>();

		if (var_decl->typespec)
		{
			var_decl->typespec->resolve();
		}
		if (var_decl->init_expr.has_value())
		{
			var_decl->init_expr->resolve();
		}

		if (!var_decl->typespec || var_decl->typespec->kind() == ast_typespec::none)
		{
			assert(var_decl->init_expr.has_value());
			var_decl->typespec = get_typespec(var_decl->init_expr.get());
		}

		context.add_variable(var_decl->identifier->value, var_decl->typespec);

		return;
	}

	case index<ast_decl_function>:
	{
		auto &fn_decl = this->get<ast_decl_function_ptr>();

		++context;
		bz::vector<ast_typespec_ptr> param_types = {};
		param_types.reserve(fn_decl->params.size());
		for (auto &p : fn_decl->params)
		{
			p.type->resolve();
			param_types.emplace_back(p.type);
			context.add_variable(p.id, p.type);
		}
		if (!context.add_function(
			fn_decl->identifier->value,
			ast_ts_function(fn_decl->return_type, std::move(param_types))
		))
		{
			bad_token(fn_decl->identifier, "Error: function redefinition");
		}

		fn_decl->return_type->resolve();

		for (auto &s : fn_decl->body->statements)
		{
			s.resolve();
		}
		--context;

		return;
	}

	case index<ast_decl_operator>:
	{
		auto &op_decl = this->get<ast_decl_operator_ptr>();

		++context;
		bz::vector<ast_typespec_ptr> param_types = {};
		param_types.reserve(op_decl->params.size());
		for (auto &p : op_decl->params)
		{
			p.type->resolve();
			param_types.emplace_back(p.type);
			context.add_variable(p.id, p.type);
		}
		if (!context.add_operator(
			op_decl->op->kind,
			ast_ts_function(op_decl->return_type, std::move(param_types))
		))
		{
			bad_token(op_decl->op, "Error: operator redefinition");
		}

		op_decl->return_type->resolve();

		for (auto &s : op_decl->body->statements)
		{
			s.resolve();
		}
		--context;

		return;
	}

	case index<ast_decl_struct>:
		assert(false);
		return;

	default:
		assert(false);
		return;
	}
}
