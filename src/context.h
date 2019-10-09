#ifndef CONTEXT_H
#define CONTEXT_H

#include "core.h"

#include "ast_type.h"

struct function_overload_set
{
	intern_string id;
	bz::vector<ast_ts_function> set;
};

struct operator_overload_set
{
	uint32_t op;
	bz::vector<ast_ts_function> set;
};

struct parse_context
{
	bz::vector<bz::vector<ast_variable>> variables;
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

	bool add_variable(intern_string id, ast_typespec_ptr type);
	bool add_function(intern_string id, ast_ts_function  type);
	bool add_operator(uint32_t      op, ast_ts_function  type);

	bool is_variable(intern_string id);
	bool is_function(intern_string id);

	ast_typespec_ptr get_identifier_type(src_tokens::pos t);
	ast_typespec_ptr get_function_type(intern_string   id, bz::vector<ast_typespec_ptr> const &args);
	ast_typespec_ptr get_operator_type(uint32_t        op, bz::vector<ast_typespec_ptr> const &args);
};

extern parse_context context;

#endif // CONTEXT_H
