#include "ast_statement.h"
#include "context.h"


std::vector<ast_variable_ptr> get_function_params(std::vector<token> const &tokens)
{
	if (tokens.empty())
	{
		return {};
	}

	auto stream = tokens.begin();
	auto end    = tokens.end();

	auto get_variable = [&]()
	{
		assert(stream != end);

		intern_string id = "";
		if (stream->kind == token::identifier)
		{
			id = stream->value;
			++stream;
			if (stream == end)
			{
				std::cerr << "Expected ':' in parameter list\n";
				exit(1);
			}
		}

		assert_token(*stream, token::colon);
		++stream;
		if (stream == end)
		{
			std::cerr << "Expected type in parameter list\n";
		}

		auto type = parse_ast_typespec(stream, end);
		return make_ast_variable(id, std::move(type));
	};

	std::vector<ast_variable_ptr> params = {};
	params.emplace_back(get_variable());

	while (stream != end)
	{
		assert_token(*stream, token::comma);
		++stream;
		if (stream == end)
		{
			std::cerr << "Expected variable definition after ','\n";
			exit(1);
		}
		params.emplace_back(get_variable());
	}

	return std::move(params);
}


ast_statement::ast_statement(fp_statement_ptr const &stmt)
	: kind(stmt->kind)
{
	switch (stmt->kind)
	{
	case statement::if_statement:
		this->if_statement = make_ast_if_statement(
			parse_ast_expression(stmt->if_statement->condition),
			make_ast_statement(stmt->if_statement->if_block),
			stmt->if_statement->else_block
				? make_ast_statement(stmt->if_statement->else_block)
				: nullptr
		);
		break;

	case statement::while_statement:
		this->while_statement = make_ast_while_statement(
			parse_ast_expression(stmt->while_statement->condition),
			make_ast_statement(stmt->while_statement->while_block)
		);
		break;

	case statement::for_statement:
		std::cerr << "for statement not yet implemented\n";
		exit(1);

	case statement::return_statement:
		this->return_statement = make_ast_return_statement(
			parse_ast_expression(stmt->return_statement->expr)
		);
		break;

	case statement::no_op_statement:
		this->no_op_statement = make_ast_no_op_statement();
		break;

	case statement::variable_decl_statement:
	{
		auto stream = stmt->variable_decl_statement->type_and_init.cbegin();
		auto end    = stmt->variable_decl_statement->type_and_init.cend();

		ast_typespec_ptr var_typespec = nullptr;
		// explicit type
		if (stream->kind == token::colon)
		{
			++stream; // ':'
			var_typespec = parse_ast_typespec(stream, end);
		}

		if (stream == end)
		{
			this->variable_decl_statement = make_ast_variable_decl_statement(
				stmt->variable_decl_statement->identifier,
				std::move(var_typespec),
				nullptr
			);
			context.add_variable(
				make_context_variable(
					this->variable_decl_statement->identifier,
					this->variable_decl_statement->typespec == nullptr
						? nullptr
						: this->variable_decl_statement->typespec->clone()
				)
			);
			break;
		}
		assert_token(*stream, token::assign);
		++stream; // '='

		auto init_expr = parse_ast_expression(stream, end);
		assert(stream == end);

		this->variable_decl_statement = make_ast_variable_decl_statement(
			stmt->variable_decl_statement->identifier,
			std::move(var_typespec),
			std::move(init_expr)
		);
		context.add_variable(
			make_context_variable(
				this->variable_decl_statement->identifier,
				this->variable_decl_statement->typespec == nullptr
					? nullptr
					: this->variable_decl_statement->typespec->clone()
			)
		);
		break;
	}

	case statement::compound_statement:
	{
		std::vector<ast_statement_ptr> statements;
		for (auto &s : stmt->compound_statement->statements)
		{
			statements.emplace_back(make_ast_statement(s));
		}
		this->compound_statement = make_ast_compound_statement(
			std::move(statements)
		);
		break;
	}

	case statement::expression_statement:
		this->expression_statement = make_ast_expression_statement(
			parse_ast_expression(stmt->expression_statement->expr)
		);
		break;

	case statement::struct_definition:
		std::cerr << "struct definition not yet implemented\n";
		exit(1);

	case statement::function_definition:
	{
		std::vector<ast_variable_ptr> params = get_function_params(
			stmt->function_definition->params
		);

		std::vector<ast_statement_ptr> body_stmts;
		for (auto &s : stmt->function_definition->body->statements)
		{
			body_stmts.emplace_back(make_ast_statement(s));
		}

		this->function_definition = make_ast_function_definition(
			stmt->function_definition->identifier,
			std::move(params),
			parse_ast_typespec(stmt->function_definition->return_type),
			make_ast_compound_statement(std::move(body_stmts))
		);
		break;
	}

	case statement::operator_definition:
		std::cerr << "types need to be parsed!!\n";
		exit(1);

	default:
		std::cerr << "Error in call to ast_statement::ast_statement(fp_statement_ptr)\n";
		exit(1);
	}
}
