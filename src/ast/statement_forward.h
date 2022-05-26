#ifndef AST_STATEMENT_FORWARD_H
#define AST_STATEMENT_FORWARD_H

#include "core.h"

namespace ast
{

struct stmt_while;
struct stmt_for;
struct stmt_foreach;
struct stmt_return;
struct stmt_no_op;
struct stmt_static_assert;
struct stmt_expression;

struct decl_variable;
struct decl_function;
struct decl_operator;
struct decl_function_alias;
struct decl_type_alias;
struct decl_struct;
struct decl_import;

struct function_body;
struct type_info;


using statement_types = bz::meta::type_pack<
	stmt_while,
	stmt_for,
	stmt_foreach,
	stmt_return,
	stmt_no_op,
	stmt_static_assert,
	stmt_expression,
	decl_variable,
	decl_function,
	decl_operator,
	decl_function_alias,
	decl_type_alias,
	decl_struct,
	decl_import
>;

using top_level_statement_types = bz::meta::type_pack<
	stmt_static_assert,
	decl_variable,
	decl_function,
	decl_operator,
	decl_function_alias,
	decl_type_alias,
	decl_struct,
	decl_import
>;

using declaration_types = bz::meta::type_pack<
	decl_variable,
	decl_function,
	decl_operator,
	decl_function_alias,
	decl_type_alias,
	decl_struct,
	decl_import
>;

template<typename T>
constexpr bool is_top_level_statement_type = []<typename ...Ts, typename ...Us>(
	bz::meta::type_pack<Ts...>,
	bz::meta::type_pack<Us...>
) {
	static_assert(bz::meta::is_in_types<T, Us...>);
	return bz::meta::is_in_types<T, Ts...>;
}(top_level_statement_types{}, statement_types{});

template<typename T>
constexpr bool is_declaration_type = []<typename ...Ts, typename ...Us>(
	bz::meta::type_pack<Ts...>,
	bz::meta::type_pack<Us...>
) {
	static_assert(bz::meta::is_in_types<T, Us...>);
	return bz::meta::is_in_types<T, Ts...>;
}(declaration_types{}, statement_types{});

} // namespace ast

#endif // AST_STATEMENT_FORWARD_H
