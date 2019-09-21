#include "ast_statement.h"
#include "context.h"


bz::vector<ast_variable> get_function_params(token_range tokens)
{
	auto it  = tokens.begin;
	auto end = tokens.end;

	if (it == end)
	{
		return {};
	}

	auto get_variable = [&]() -> ast_variable
	{
		if (it == end)
		{
			bad_token(end, "Expected variable definition");
		}

		intern_string id;
		if (it->kind == token::identifier)
		{
			id = it->value;
			++it;
			if (it == end)
			{
				fatal_error("Expected ':' in parameter list\n");
			}
		}

		assert_token(it, token::colon);
		if (it == end)
		{
			fatal_error("Expected type in parameter list\n");
		}

		auto type = parse_ast_typespec(it, end);
		return { id, type };
	};

	bz::vector<ast_variable> params = {};
	params.emplace_back(get_variable());

	while (it != end)
	{
		assert_token(it, token::comma);
		params.emplace_back(get_variable());
	}

	return params;
}


ast_statement::ast_statement(fp_statement_ptr const &stmt)
	: base_t()
{
	switch (stmt->kind())
	{
	case fp_statement::if_statement:
	{
		auto &if_stmt = stmt->get<fp_statement::if_statement>();
		this->emplace<if_statement>(
			make_ast_if_statement(
				parse_ast_expression(if_stmt->condition),
				make_ast_statement(if_stmt->then_block),
				if_stmt->else_block
					? make_ast_statement(if_stmt->else_block)
					: nullptr
			)
		);
		break;
	}

	case fp_statement::while_statement:
	{
		auto &while_stmt = stmt->get<fp_statement::while_statement>();
		this->emplace<while_statement>(
			make_ast_while_statement(
				parse_ast_expression(while_stmt->condition),
				make_ast_statement(while_stmt->while_block)
			)
		);
		break;
	}

	case fp_statement::for_statement:
		assert(false);
		return;

	case fp_statement::return_statement:
		this->emplace<return_statement>(
			make_ast_return_statement(
				parse_ast_expression(stmt->get<fp_statement::return_statement>()->expr)
			)
		);
		break;

	case fp_statement::no_op_statement:
		this->emplace<no_op_statement>(make_ast_no_op_statement());
		break;

	case fp_statement::compound_statement:
	{
		bz::vector<ast_statement_ptr> statements;
		auto &comp_stmt = stmt->get<fp_statement::compound_statement>();
		for (auto &s : comp_stmt->statements)
		{
			statements.emplace_back(make_ast_statement(s));
		}
		this->emplace<compound_statement>(
			make_ast_compound_statement(
				std::move(statements)
			)
		);
		break;
	}

	case fp_statement::expression_statement:
		this->emplace<expression_statement>(
			make_ast_expression_statement(
				parse_ast_expression(stmt->get<fp_statement::expression_statement>()->expr)
			)
		);
		break;

	case fp_statement::declaration_statement:
		this->emplace<declaration_statement>(
			make_ast_declaration_statement(
				stmt->get<fp_statement::declaration_statement>()
			)
		);
		break;

	default:
		assert(false);
		break;
	}
}

ast_declaration_statement::ast_declaration_statement(fp_declaration_statement_ptr const &decl)
	: base_t()
{
	switch (decl->kind())
	{
	case fp_declaration_statement::variable_decl:
	{
		auto &var_decl = decl->get<fp_declaration_statement::variable_decl>();
		auto id = var_decl->identifier;

		auto stream = var_decl->type_and_init.begin;
		auto end    = var_decl->type_and_init.end;

		if (stream == end)
		{
			bad_token(stream, "Invalid variable declaration");
		}

		ast_typespec_ptr typespec = nullptr;
		if (stream->kind == token::colon)
		{
			++stream; // ':'
			assert(stream != end);
			typespec = parse_ast_typespec(stream, end);

			context.add_variable(id, typespec);

			if (stream == end)
			{
				this->emplace<variable_decl>(
					make_ast_variable_decl(
						id, std::move(typespec), ast_expression_ptr(nullptr)
					)
				);
				return;
			}
		}

		assert_token(stream, token::assign);

		auto init_expr = parse_ast_expression(stream, end);
		if (stream != end)
		{
			bad_token(stream, "Expected ';'");
		}

		if (!typespec)
		{
			typespec = init_expr->typespec;
			context.add_variable(id, typespec);
		}

		this->emplace<variable_decl>(
			make_ast_variable_decl(
				id, std::move(typespec), std::move(init_expr)
			)
		);
		break;
	}

	case fp_declaration_statement::function_decl:
	{
		auto &func_decl = decl->get<fp_declaration_statement::function_decl>();

		auto id = func_decl->identifier;
		auto ret_type = parse_ast_typespec(func_decl->return_type);
		auto params = get_function_params(func_decl->params);

		bz::vector<ast_typespec_ptr> param_types;
		bz::vector<intern_string>    param_ids;

		param_types.reserve(params.size());
		param_ids.reserve(params.size());

		for (auto &p : params)
		{
			param_types.emplace_back(p.type);
			param_ids.emplace_back(p.id);
		}

		auto fn_type = make_ast_function_type(ret_type, param_types);
		context.add_function(id, fn_type);

		++context;
		for (auto &p : params)
		{
			context.add_variable(p.id, p.type);
		}

		bz::vector<ast_statement_ptr> body;
		body.reserve(func_decl->body->statements.size());

		for (auto &stmt : func_decl->body->statements)
		{
			body.emplace_back(make_ast_statement(stmt));
		}

		this->emplace<function_decl>(
			make_ast_function_decl(
				id,
				fn_type,
				make_ast_compound_statement(std::move(body))
			)
		);
		break;
	}

	case fp_declaration_statement::operator_decl:
	{
		auto &op_decl = decl->get<fp_declaration_statement::operator_decl>();

		auto op = op_decl->op;
		auto ret_type = parse_ast_typespec(op_decl->return_type);
		auto params = get_function_params(op_decl->params);

		bz::vector<ast_typespec_ptr> param_types;
		bz::vector<intern_string>    param_ids;

		param_types.reserve(params.size());
		param_ids.reserve(params.size());

		for (auto &p : params)
		{
			param_types.emplace_back(p.type);
			param_ids.emplace_back(p.id);
		}

		auto fn_type = make_ast_function_type(ret_type, param_types);
		context.add_operator(op, fn_type);

		++context;
		for (auto &p : params)
		{
			context.add_variable(p.id, p.type);
		}

		bz::vector<ast_statement_ptr> body;
		body.reserve(op_decl->body->statements.size());

		for (auto &stmt : op_decl->body->statements)
		{
			body.emplace_back(make_ast_statement(stmt));
		}

		this->emplace<operator_decl>(
			make_ast_operator_decl(
				op,
				fn_type,
				make_ast_compound_statement(std::move(body))
			)
		);
		break;
	}

	case fp_declaration_statement::struct_decl:
		break;

	default:
		assert(false);
		break;
	}
}
