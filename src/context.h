#ifndef CONTEXT_H
#define CONTEXT_H

#include "core.h"

#include "ast_type.h"


struct context_function
{
	intern_string identifier;
	std::vector<ast_typespec_ptr> param_types;

	context_function(intern_string _id, std::vector<ast_typespec_ptr> _param_types)
		: identifier(_id), param_types(std::move(_param_types))
	{}
};
using context_function_ptr = std::unique_ptr<context_function>;

struct context_operator
{
	uint32_t op;
	std::vector<ast_typespec_ptr> param_types;

	context_operator(uint32_t _op, std::vector<ast_typespec_ptr> _param_types)
		: op(_op), param_types(std::move(_param_types))
	{}
};
using context_operator_ptr = std::unique_ptr<context_operator>;

struct context_variable
{
	intern_string identifier;
	ast_typespec_ptr type;

	context_variable(intern_string _id, ast_typespec_ptr _type)
		: identifier(_id), type(std::move(_type))
	{}
};
using context_variable_ptr = std::unique_ptr<context_variable>;


struct parse_context
{
	std::vector<std::vector<context_variable_ptr>> variables = {};
	std::vector<std::vector<context_function_ptr>> functions = {};
	std::vector<std::vector<context_operator_ptr>> operators = {};

	parse_context()
	{
		variables.push_back({});
		functions.push_back({});
		operators.push_back({});
	}

	void operator ++ (void)
	{
		variables.push_back({});
		functions.push_back({});
		operators.push_back({});
	}
	void operator -- (void)
	{
		variables.pop_back();
		functions.pop_back();
		operators.pop_back();
	}

	void add_variable(context_variable_ptr var );
	void add_function(context_function_ptr func);
	void add_operator(context_operator_ptr op  );

	ast_typespec_ptr get_operation_typespec(
		uint32_t op, ast_typespec_ptr const &lhs, ast_typespec_ptr const &rhs
	);

	ast_typespec_ptr get_variable_typespec(intern_string id) const;
};

extern parse_context context;


template<typename... Args>
context_function_ptr make_context_function(Args &&... args)
{
	return std::make_unique<context_function>(std::forward<Args>(args)...);
}

template<typename... Args>
context_operator_ptr make_context_operator(Args &&... args)
{
	return std::make_unique<context_operator>(std::forward<Args>(args)...);
}

template<typename... Args>
context_variable_ptr make_context_variable(Args &&... args)
{
	return std::make_unique<context_variable>(std::forward<Args>(args)...);
}


#endif // CONTEXT_H
