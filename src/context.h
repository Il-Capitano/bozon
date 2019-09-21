#ifndef CONTEXT_H
#define CONTEXT_H

#include "core.h"

#include "ast_type.h"

struct function_overload_set
{
	intern_string id;
	bz::vector<ast_function_type_ptr> set;
};

struct operator_overload_set
{
	uint32_t op;
	bz::vector<ast_function_type_ptr> set;
};

struct ast_variable
{
	intern_string id;
	ast_typespec_ptr type;
};

struct parse_context
{
	bz::vector<bz::vector<ast_variable>> variables;
	bz::vector<function_overload_set>    functions;
	bz::vector<operator_overload_set>    operators;

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

	void add_variable(intern_string id, ast_typespec_ptr      type);
	void add_function(intern_string id, ast_function_type_ptr type);
	void add_operator(uint32_t      op, ast_function_type_ptr type);

	ast_typespec_ptr get_variable_type(intern_string id);
	ast_typespec_ptr get_function_type(intern_string id, bz::vector<ast_typespec_ptr> const &args);
	ast_typespec_ptr get_operator_type(uint32_t      op, bz::vector<ast_typespec_ptr> const &args);
};

extern parse_context context;

#endif // CONTEXT_H
