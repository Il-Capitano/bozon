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
	bz::string id;
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
	// TODO: we need to deal with scoped declarations, if they are even allowed
	// should be at least for types
	bz::vector<function_overload_set>     functions;
	bz::vector<operator_overload_set>     operators;
	bz::vector<ast::type_ptr>             types;

	parse_context(void);

	void operator ++ (void)
	{
		this->variables.push_back({});
	}

	void operator -- (void)
	{
		this->variables.pop_back();
	}

	bool add_variable(src_tokens::pos id, ast::typespec type);
	void add_function(ast::decl_function_ptr &func_decl);
	void add_operator(ast::decl_operator_ptr &op_decl);

	bool is_variable(bz::string id);
	bool is_function(bz::string id);

	ast::type_ptr get_type(src_tokens::pos id);
	ast::typespec get_identifier_type(src_tokens::pos t);
	ast::typespec get_function_type(bz::string   id, bz::vector<ast::typespec> const &args);
	ast::typespec get_operator_type(src_tokens::pos op, bz::vector<ast::typespec> const &args);
};

extern parse_context context;

#endif // CONTEXT_H
