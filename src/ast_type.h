#ifndef AST_TYPE_H
#define AST_TYPE_H

#include "core.h"

#include "first_pass_parser.h"

namespace type
{

enum : uint32_t
{
	name,
	constant,
	pointer,
	reference,
};

}

struct ast_built_in_type;
using ast_built_in_type_ptr = std::unique_ptr<ast_built_in_type>;
struct ast_user_defined_type;
using ast_user_defined_type_ptr = std::unique_ptr<ast_user_defined_type>;


struct ast_typespec;
using ast_typespec_ptr = std::unique_ptr<ast_typespec>;

struct ast_typespec
{
	uint32_t kind;

	intern_string    name = {};
	ast_typespec_ptr base = nullptr;

	ast_typespec(uint32_t _kind, ast_typespec_ptr _base)
		: kind(_kind), base(std::move(_base))
	{
		assert(
			_kind == type::pointer
			|| _kind == type::reference
			|| _kind == type::constant
		);
	}

	ast_typespec(intern_string _name)
		: kind(type::name), name(_name)
	{}

	ast_typespec_ptr clone(void) const;
};


struct ast_variable
{
	intern_string  identifier;
	ast_typespec_ptr typespec;

	ast_variable(intern_string _id, ast_typespec_ptr _typespec)
		: identifier(_id), typespec(std::move(_typespec))
	{}
};

using ast_variable_ptr = std::unique_ptr<ast_variable>;


template<typename... Args>
inline ast_typespec_ptr make_ast_typespec(Args &&... args)
{
	return std::make_unique<ast_typespec>(std::forward<Args>(args)...);
}

template<typename... Args>
inline ast_variable_ptr make_ast_variable(Args &&... args)
{
	return std::make_unique<ast_variable>(std::forward<Args>(args)...);
}

ast_typespec_ptr parse_ast_typespec(std::vector<token> const &t);

ast_typespec_ptr parse_ast_typespec(
	std::vector<token>::const_iterator &stream,
	std::vector<token>::const_iterator &end
);

#endif // AST_TYPE_H
