#include "codegen.h"
#include "instructions.h"

namespace comptime
{

static void generate_stmt_code(ast::statement const &stmt, codegen_context &context);

static expr_value generate_expr_code(
	ast::expression const &expr,
	codegen_context &context,
	instruction_ref result_address
);


static expr_value generate_expr_code(
	ast::constant_expression const &const_expr,
	codegen_context &context,
	instruction_ref result_address
)
{
	bz_unreachable;
}

static expr_value generate_expr_code(
	ast::dynamic_expression const &dyn_expr,
	codegen_context &context,
	instruction_ref result_address
)
{
	bz_unreachable;
}

static expr_value generate_expr_code(
	ast::expression const &expr,
	codegen_context &context,
	instruction_ref result_address
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

static void generate_stmt_code(ast::stmt_while const &while_stmt, codegen_context &context)
{
	auto const cond_check_bb = context.add_basic_block();
	auto const end_bb = context.add_basic_block();

	auto const prev_loop_info = context.push_loop(end_bb, cond_check_bb);

	context.create_jump(cond_check_bb);
	context.set_current_basic_block(cond_check_bb);
	auto const cond_prev_info = context.push_expression_scope();
	auto const condition = generate_expr_code(while_stmt.condition, context, {}).get_value(context);
	context.pop_expression_scope(cond_prev_info);
	auto const cond_check_bb_end = context.get_current_basic_block();

	auto const while_bb = context.add_basic_block();
	context.set_current_basic_block(while_bb);

	auto const while_prev_info = context.push_expression_scope();
	generate_expr_code(while_stmt.while_block, context, {});
	context.pop_expression_scope(while_prev_info);
	context.create_jump(cond_check_bb);

	context.set_current_basic_block(cond_check_bb_end);
	context.create_conditional_jump(condition, while_bb, end_bb);
	context.set_current_basic_block(end_bb);

	context.pop_loop(prev_loop_info);
}

static void generate_stmt_code(ast::stmt_for const &for_stmt, codegen_context &context)
{
	auto const init_prev_info = context.push_expression_scope();
	if (for_stmt.init.not_null())
	{
		generate_stmt_code(for_stmt.init, context);
	}

	auto const begin_bb = context.get_current_basic_block();

	auto const iteration_bb = context.add_basic_block();
	auto const end_bb = context.add_basic_block();
	auto const prev_loop_info = context.push_loop(end_bb, iteration_bb);

	context.set_current_basic_block(iteration_bb);
	if (for_stmt.iteration.not_null())
	{
		auto const prev_info = context.push_expression_scope();
		generate_expr_code(for_stmt.iteration, context, {});
		context.pop_expression_scope(prev_info);
	}

	auto const cond_check_bb = context.add_basic_block();
	context.create_jump(cond_check_bb);

	context.set_current_basic_block(begin_bb);
	context.create_jump(cond_check_bb);

	context.set_current_basic_block(cond_check_bb);

	instruction_ref condition = {};
	if (for_stmt.condition.not_null())
	{
		auto const prev_info = context.push_expression_scope();
		condition = generate_expr_code(for_stmt.condition, context, {}).get_value(context);
		context.pop_expression_scope(prev_info);
	}

	auto const cond_check_bb_end = context.get_current_basic_block();

	auto const for_bb = context.add_basic_block();
	context.set_current_basic_block(for_bb);

	auto const for_prev_info = context.push_expression_scope();
	generate_expr_code(for_stmt.for_block, context, {});
	context.pop_expression_scope(for_prev_info);

	context.create_jump(iteration_bb);

	context.set_current_basic_block(cond_check_bb_end);
	if (for_stmt.condition.not_null())
	{
		context.create_conditional_jump(condition, for_bb, end_bb);
	}
	else
	{
		context.create_jump(for_bb);
	}
	context.set_current_basic_block(end_bb);

	context.pop_expression_scope(init_prev_info);

	context.pop_loop(prev_loop_info);
}

static void generate_stmt_code(ast::stmt_foreach const &, codegen_context &context);

static void generate_stmt_code(ast::stmt_return const &, codegen_context &context);

static void generate_stmt_code(ast::stmt_defer const &, codegen_context &context);

static void generate_stmt_code(ast::stmt_no_op const &, codegen_context &context);

static void generate_stmt_code(ast::stmt_expression const &expr_stmt, codegen_context &context)
{
	auto const prev_info = context.push_expression_scope();
	generate_expr_code(expr_stmt.expr, context, {});
	context.pop_expression_scope(prev_info);
}

static void generate_stmt_code(ast::decl_variable const &, codegen_context &context);

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
