#include "codegen.h"
#include "instructions.h"

namespace comptime
{

struct expr_value
{
	instruction *inst;
	bool is_ref;
};


static expr_value generate_expr_code(
	ast::constant_expression const &const_expr,
	codegen_context &context,
	instruction *result_address
)
{
	bz_unreachable;
}

static expr_value generate_expr_code(
	ast::dynamic_expression const &dyn_expr,
	codegen_context &context,
	instruction *result_address
)
{
	bz_unreachable;
}

static expr_value generate_expr_code(
	ast::expression const &expr,
	codegen_context &context,
	instruction *result_address
)
{
	switch (expr.kind())
	{
		case ast::expression::index_of<ast::constant_expression>:
			return generate_expr_code(expr.get_constant(), context, result_address);
		case ast::expression::index_of<ast::dynamic_expression>:
			return generate_expr_code(expr.get_dynamic(), context, result_address);
		case ast::expression::index_of<ast::error_expression>:
			bz_unreachable;
		default:
			bz_unreachable;
	}
}

static void generate_stmt_code(ast::stmt_while const &, codegen_context &context);

static void generate_stmt_code(ast::stmt_for const &, codegen_context &context);

static void generate_stmt_code(ast::stmt_foreach const &, codegen_context &context);

static void generate_stmt_code(ast::stmt_return const &, codegen_context &context);

static void generate_stmt_code(ast::stmt_defer const &, codegen_context &context);

static void generate_stmt_code(ast::stmt_no_op const &, codegen_context &context);

static void generate_stmt_code(ast::stmt_expression const &expr_stmt, codegen_context &context)
{
	generate_expr_code(expr_stmt.expr, context, nullptr);
}

static void generate_stmt_code(ast::statement const &stmt, codegen_context &context)
{
	switch (stmt.kind())
	{
	static_assert(ast::statement::variant_count == 16);
	case ast::statement::index<ast::stmt_while>:
		generate_stmt_code(stmt.get<ast::stmt_while>(), context);
		break;
	case ast::statement::index<ast::stmt_for>:
		generate_stmt_code(stmt.get<ast::stmt_for>(), context);
		break;
	case ast::statement::index<ast::stmt_foreach>:
		generate_stmt_code(stmt.get<ast::stmt_foreach>(), context);
		break;
	case ast::statement::index<ast::stmt_return>:
		generate_stmt_code(stmt.get<ast::stmt_return>(), context);
		break;
	case ast::statement::index<ast::stmt_defer>:
		generate_stmt_code(stmt.get<ast::stmt_defer>(), context);
		break;
	case ast::statement::index<ast::stmt_no_op>:
		generate_stmt_code(stmt.get<ast::stmt_no_op>(), context);
		break;
	case ast::statement::index<ast::stmt_expression>:
		generate_stmt_code(stmt.get<ast::stmt_expression>(), context);
		break;
	case ast::statement::index<ast::stmt_static_assert>:
		// nothing
		break;

	case ast::statement::index<ast::decl_variable>:
		generate_stmt_code(stmt.get<ast::decl_variable>(), context);
		break;

	case ast::statement::index<ast::decl_function>:
	case ast::statement::index<ast::decl_operator>:
	case ast::statement::index<ast::decl_struct>:
	case ast::statement::index<ast::decl_enum>:
	case ast::statement::index<ast::decl_import>:
	case ast::statement::index<ast::decl_type_alias>:
		break;
	default:
		bz_unreachable;
	}
}

void generate_code(ast::function_body &body, codegen_context &context)
{
	bz_assert(body.state == ast::resolve_state::all);

	for (auto const &stmt : body.get_statements())
	{
		generate_stmt_code(stmt, context);
	}
}

} // namespace comptime
