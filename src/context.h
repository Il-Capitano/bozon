#ifndef CONTEXT_H
#define CONTEXT_H

#include "core.h"

#include "ast/type.h"

namespace ast
{

struct decl_operator;
using decl_operator_ptr = std::unique_ptr<decl_operator>;
struct decl_function;
using decl_function_ptr = std::unique_ptr<decl_function>;

} // namespace ast

struct function_overload_set
{
	intern_string id;
	bz::vector<ast::ts_function> set;
};

struct operator_overload_set
{
	uint32_t op;
	bz::vector<ast::ts_function> set;
};

struct parse_context
{
	bz::vector<bz::vector<ast::variable>> variables;
	bz::vector<function_overload_set>    functions;
	bz::vector<operator_overload_set>    operators;

//	bz::vector<bz::vector<>> types;

	parse_context(void)
		: variables{{}},
		  functions{},
		  operators{}
	{}

	void operator ++ (void)
	{
		this->variables.push_back({});
	}

	void operator -- (void)
	{
		this->variables.pop_back();
	}

	bool add_variable(intern_string id, ast::typespec_ptr type);
	void add_function(ast::decl_function_ptr &func_decl);
	void add_operator(ast::decl_operator_ptr &op_decl);

	bool is_variable(intern_string id);
	bool is_function(intern_string id);

	ast::typespec_ptr get_identifier_type(src_tokens::pos t);
	ast::typespec_ptr get_function_type(intern_string   id, bz::vector<ast::typespec_ptr> const &args);
	ast::typespec_ptr get_operator_type(uint32_t        op, bz::vector<ast::typespec_ptr> const &args);
};

extern parse_context context;

#endif // CONTEXT_H
