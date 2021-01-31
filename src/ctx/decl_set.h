#ifndef CTX_DECL_SET_H
#define CTX_DECL_SET_H

#include "ast/typespec.h"
#include "ast/expression.h"
#include "ast/statement.h"

namespace ctx
{

struct function_overload_set
{
	bz::u8string id;
	bz::vector<ast::statement_view> func_decls;
	bz::vector<ast::statement_view> alias_decls;
};

struct operator_overload_set
{
	uint32_t op;
	// will always be ast::decl_operator
	bz::vector<ast::statement_view> op_decls;
};

struct decl_set
{
	bz::vector<ast::decl_variable *>   var_decls;
	bz::vector<function_overload_set>  func_sets;
	bz::vector<operator_overload_set>  op_sets;
	bz::vector<ast::decl_type_alias *> types;

	void add_function(ast::statement &stmt)
	{
		bz_assert(stmt.is<ast::decl_function>());
		auto &func_decl = stmt.get<ast::decl_function>();
		auto const id = func_decl.identifier->value;
		auto const set = std::find_if(
			this->func_sets.begin(), this->func_sets.end(),
			[id](auto const &set) {
				return id == set.id;
			}
		);
		if (set == this->func_sets.end())
		{
			this->func_sets.push_back({ id, { ast::statement_view(stmt) }, {} });
		}
		else
		{
			set->func_decls.emplace_back(stmt);
		}
	}

	void add_function(ast::decl_function &func_decl)
	{
		auto const id = func_decl.identifier->value;
		auto const set = std::find_if(
			this->func_sets.begin(), this->func_sets.end(),
			[id](auto const &set) {
				return id == set.id;
			}
		);
		if (set == this->func_sets.end())
		{
			this->func_sets.push_back({ id, { ast::statement_view(&func_decl) }, {} });
		}
		else
		{
			set->func_decls.emplace_back(&func_decl);
		}
	}

	void add_operator(ast::statement &stmt)
	{
		bz_assert(stmt.is<ast::decl_operator>());
		auto &op_decl = stmt.get<ast::decl_operator>();
		auto const op = op_decl.op->kind;
		auto const set = std::find_if(
			this->op_sets.begin(), this->op_sets.end(),
			[op](auto const &set) {
				return op == set.op;
			}
		);
		if (set == this->op_sets.end())
		{
			this->op_sets.push_back({ op, { ast::statement_view(stmt) } });
		}
		else
		{
			set->op_decls.emplace_back(stmt);
		}
	}

	void add_operator(ast::decl_operator &op_decl)
	{
		auto const op = op_decl.op->kind;
		auto const set = std::find_if(
			this->op_sets.begin(), this->op_sets.end(),
			[op](auto const &set) {
				return op == set.op;
			}
		);
		if (set == this->op_sets.end())
		{
			this->op_sets.push_back({ op, { ast::statement_view(&op_decl) } });
		}
		else
		{
			set->op_decls.emplace_back(&op_decl);
		}
	}

	void add_function_alias(ast::statement &stmt)
	{
		bz_assert(stmt.is<ast::decl_function_alias>());
		auto &alias_decl = stmt.get<ast::decl_function_alias>();
		auto const id = alias_decl.identifier->value;
		auto const set = std::find_if(
			this->func_sets.begin(), this->func_sets.end(),
			[id](auto const &set) {
				return id == set.id;
			}
		);
		if (set == this->func_sets.end())
		{
			this->func_sets.push_back({ id, {}, { ast::statement_view(stmt) } });
		}
		else
		{
			set->alias_decls.emplace_back(stmt);
		}
	}

	void add_function_set(function_overload_set const &func_set)
	{
		auto const id = func_set.id;
		auto const set = std::find_if(
			this->func_sets.begin(), this->func_sets.end(),
			[id](auto const &set) {
				return id == set.id;
			}
		);
		if (set == this->func_sets.end())
		{
			this->func_sets.push_back(func_set);
		}
		else
		{
			for (auto const &decl : func_set.func_decls)
			{
				set->func_decls.push_back(decl);
			}
		}
	}

	void add_operator_set(operator_overload_set const &op_set)
	{
		auto const op = op_set.op;
		auto const set = std::find_if(
			this->op_sets.begin(), this->op_sets.end(),
			[op](auto const &set) {
				return op == set.op;
			}
		);
		if (set == this->op_sets.end())
		{
			this->op_sets.push_back(op_set);
		}
		else
		{
			for (auto const &decl : op_set.op_decls)
			{
				set->op_decls.push_back(decl);
			}
		}
	}

	void add_type_alias(ast::decl_type_alias &alias_decl)
	{
		this->types.push_back(&alias_decl);
	}

	void add_variable(ast::decl_variable &var_decl)
	{
		this->var_decls.push_back(&var_decl);
	}
};

} // namespace ctx

#endif // CTX_DECL_SET_H
