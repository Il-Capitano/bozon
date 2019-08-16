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
{
	switch (stmt->kind)
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
		std::vector<ast_statement_ptr> statements;
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
	: base_t(),
	  kind()
{
	switch (decl->kind)
	{
	case fp_declaration_statement::variable_decl:
	{
		auto &var_decl = decl->get<fp_declaration_statement::variable_decl>();
		auto id = var_decl->identifier;

		auto stream = var_decl->type_and_init.cbegin();
		auto end    = var_decl->type_and_init.cend();

		if (stream == end)
		{
			std::cerr << "Error: cannot have an empty variable declaration\n";
			exit(1);
		}

		ast_typespec_ptr typespec = nullptr;
		if (stream->kind == token::colon)
		{
			++stream; // ':'
			assert(stream != end);
			typespec = parse_ast_typespec(stream, end);
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

		assert_token(*stream, token::assign);
		++stream; // '='

		auto init_expr = parse_ast_expression(stream, end);
		if (stream != end)
		{
			bad_token(*stream, "Expected ';'");
		}

		this->emplace<variable_decl>(
			make_ast_variable_decl(
				id, std::move(typespec), std::move(init_expr)
			)
		);
		break;
	}

	case fp_declaration_statement::function_decl:
		break;

	case fp_declaration_statement::operator_decl:
		break;

	case fp_declaration_statement::struct_decl:
		break;

	default:
		assert(false);
		break;
	}
}
