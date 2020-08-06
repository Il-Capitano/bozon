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
	bz::vector<ast::decl_function *> func_decls;
};

struct operator_overload_set
{
	uint32_t op;
	bz::vector<ast::decl_operator *> op_decls;
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
			this->func_sets.push_back({ id, { &func_decl } });
		}
		else
		{
			set->func_decls.push_back(&func_decl);
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
			this->op_sets.push_back({ op, { &op_decl } });
		}
		else
		{
			set->op_decls.push_back(&op_decl);
		}
	}
};

} // namespace ctx

#endif // CTX_DECL_SET_H
