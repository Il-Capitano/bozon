#include "ast_statement.h"
#include "context.h"


void ast_statement::resolve(void)
{
	switch (this->kind())
	{
	case if_statement:
	{
		auto &if_stmt = this->get<if_statement>();
		if_stmt->condition->resolve();
		if_stmt->then_block->resolve();
		if_stmt->else_block->resolve();
		return;
	}

	case while_statement:
	{
		auto &while_stmt = this->get<while_statement>();
		while_stmt->condition->resolve();
		while_stmt->while_block->resolve();
		return;
	}

	case for_statement:
		assert(false);
		return;

	case return_statement:
	{
		auto &ret_stmt = this->get<return_statement>();
		ret_stmt->expr->resolve();
		return;
	}

	case no_op_statement:
		return;

	case compound_statement:
	{
		++context;
		auto &comp_stmt = this->get<compound_statement>();
		for (auto &s : comp_stmt->statements)
		{
			s->resolve();
		}
		--context;
		return;
	}

	case expression_statement:
	{
		auto &expr_stmt = this->get<expression_statement>();
		expr_stmt->expr->resolve();
		return;
	}

	case declaration_statement:
	{
		auto &decl_stmt = this->get<declaration_statement>();
		decl_stmt->resolve();
		return;
	}

	default:
		assert(false);
		return;
	}
}

void ast_declaration_statement::resolve(void)
{
	switch (this->kind())
	{
	case variable_decl:
	{
		auto &var_decl = this->get<variable_decl>();

		if (var_decl->typespec)
		{
			var_decl->typespec->resolve();
		}
		if (var_decl->init_expr)
		{
			var_decl->init_expr->resolve();
		}

		if (!var_decl->typespec || var_decl->typespec->kind() == ast_typespec::none)
		{
			assert(var_decl->init_expr);
			var_decl->typespec = var_decl->init_expr->typespec;
		}

		context.add_variable(var_decl->identifier->value, var_decl->typespec);

		return;
	}

	case function_decl:
	{
		auto &fn_decl = this->get<function_decl>();

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
			s->resolve();
		}
		--context;

		return;
	}

	case operator_decl:
	{
		auto &op_decl = this->get<operator_decl>();

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
			s->resolve();
		}
		--context;

		return;
	}

	case struct_decl:
		assert(false);
		return;

	default:
		assert(false);
		return;
	}
}
