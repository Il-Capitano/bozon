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
	bz::vector<std::pair<ast::statement *, ast::decl_function *>> func_decls;
};

struct operator_overload_set
{
	uint32_t op;
	// will always be ast::decl_operator
	bz::vector<std::pair<ast::statement *, ast::decl_operator *>> op_decls;
};

struct typespec_with_name
{
	bz::u8string_view id;
	ast::typespec     ts;
};

struct decl_set
{
	bz::vector<ast::decl_variable *>  var_decls;
	bz::vector<function_overload_set> func_sets;
	bz::vector<operator_overload_set> op_sets;
	bz::vector<typespec_with_name>    types;

	void add_function(ast::statement &stmt)
	{
		bz_assert(stmt.is<ast::decl_function>());
		auto &func_decl = *stmt.get<ast::decl_function_ptr>();
		auto const id = func_decl.identifier->value;
		auto const set = std::find_if(
			this->func_sets.begin(), this->func_sets.end(),
			[id](auto const &set) {
				return id == set.id;
			}
		);
		if (set == this->func_sets.end())
		{
			this->func_sets.push_back({ id, {{ &stmt, &func_decl }} });
		}
		else
		{
			set->func_decls.push_back({ &stmt, &func_decl });
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
			this->func_sets.push_back({ id, {{ nullptr, &func_decl }} });
		}
		else
		{
			set->func_decls.push_back({ nullptr, &func_decl });
		}
	}

	void add_operator(ast::statement &stmt)
	{
		bz_assert(stmt.is<ast::decl_operator>());
		auto &op_decl = *stmt.get<ast::decl_operator_ptr>();
		auto const op = op_decl.op->kind;
		auto const set = std::find_if(
			this->op_sets.begin(), this->op_sets.end(),
			[op](auto const &set) {
				return op == set.op;
			}
		);
		if (set == this->op_sets.end())
		{
			this->op_sets.push_back({ op, {{ &stmt, &op_decl }} });
		}
		else
		{
			set->op_decls.push_back({ &stmt, &op_decl });
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
			this->op_sets.push_back({ op, {{ nullptr, &op_decl }} });
		}
		else
		{
			set->op_decls.push_back({ nullptr, &op_decl });
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
};

} // namespace ctx

#endif // CTX_DECL_SET_H
