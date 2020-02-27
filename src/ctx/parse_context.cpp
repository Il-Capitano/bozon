#include "parse_context.h"
#include "global_context.h"

namespace ctx
{

void parse_context::report_error(lex::token_pos it) const
{
	this->global_ctx.report_error(ctx::make_error(it));
}

void parse_context::report_error(
	lex::token_pos it,
	bz::string message, bz::vector<ctx::note> notes
) const
{
	this->global_ctx.report_error(ctx::make_error(
		it, std::move(message), std::move(notes)
	));
}

void parse_context::report_error(
	lex::token_pos begin, lex::token_pos pivot, lex::token_pos end,
	bz::string message, bz::vector<ctx::note> notes
) const
{
	this->global_ctx.report_error(ctx::make_error(
		begin, pivot, end, std::move(message), std::move(notes)
	));
}

bool parse_context::has_errors(void) const
{
	return this->global_ctx.has_errors();
}

lex::token_pos parse_context::assert_token(lex::token_pos &stream, uint32_t kind) const
{
	if (stream->kind != kind)
	{
		this->global_ctx.report_error(ctx::make_error(
			stream,
			stream->kind == lex::token::eof
			? bz::format("expected {} before end-of-file", lex::get_token_name_for_message(kind))
			: bz::format("expected {}", lex::get_token_name_for_message(kind))
		));
		return stream;
	}
	else
	{
		auto t = stream;
		++stream;
		return t;
	}
}

lex::token_pos parse_context::assert_token(lex::token_pos &stream, uint32_t kind1, uint32_t kind2) const
{
	if (stream->kind != kind1 && stream->kind != kind2)
	{
		this->global_ctx.report_error(ctx::make_error(
			stream,
			stream->kind == lex::token::eof
			? bz::format(
				"expected {} or {} before end-of-file",
				lex::get_token_name_for_message(kind1), lex::get_token_name_for_message(kind2)
			)
			: bz::format(
				"expected {} or {}",
				lex::get_token_name_for_message(kind1), lex::get_token_name_for_message(kind2)
			)
		));
		return stream;
	}
	else
	{
		auto t = stream;
		++stream;
		return t;
	}
}

void parse_context::report_ambiguous_id_error(lex::token_pos id) const
{
	this->global_ctx.report_ambiguous_id_error(this->scope, id);
}


void parse_context::add_scope(void)
{
	this->scope_variables.push_back({});
}

void parse_context::remove_scope(void)
{
	this->scope_variables.pop_back();
}


void parse_context::add_global_declaration(ast::declaration &decl)
{
	auto res = this->global_ctx.add_global_declaration(this->scope, decl);
	if (res.has_error())
	{
		this->global_ctx.report_error(std::move(res.get_error()));
	}
}

void parse_context::add_global_variable(ast::decl_variable &var_decl)
{
	auto res = this->global_ctx.add_global_variable(this->scope, var_decl);
	if (res.has_error())
	{
		this->global_ctx.report_error(std::move(res.get_error()));
	}
}

void parse_context::add_global_function(ast::decl_function &func_decl)
{
	auto res = this->global_ctx.add_global_function(this->scope, func_decl);
	if (res.has_error())
	{
		this->global_ctx.report_error(std::move(res.get_error()));
	}
}

void parse_context::add_global_operator(ast::decl_operator &op_decl)
{
	auto res = this->global_ctx.add_global_operator(this->scope, op_decl);
	if (res.has_error())
	{
		this->global_ctx.report_error(std::move(res.get_error()));
	}
}

void parse_context::add_global_struct(ast::decl_struct &struct_decl)
{
	auto res = this->global_ctx.add_global_struct(this->scope, struct_decl);
	if (res.has_error())
	{
		this->global_ctx.report_error(std::move(res.get_error()));
	}
}


void parse_context::add_local_variable(ast::variable var_decl)
{
	assert(this->scope_variables.size() != 0);
	this->scope_variables.back().emplace_back(std::move(var_decl));
}


ast::expression::expr_type_t parse_context::get_identifier_type(lex::token_pos id) const
{
	// we go in reverse through the scopes and the variables
	// in case there's shadowing
	for (
		auto scope = this->scope_variables.rbegin();
		scope != this->scope_variables.rend();
		++scope
	)
	{
		auto const var = std::find_if(
			scope->rbegin(), scope->rend(),
			[&](auto const &var) {
				return var.id->value == id->value;
			}
		);
		if (var != scope->rend())
		{
			return { ast::expression::lvalue, var->var_type };
		}
	}

	auto res = this->global_ctx.get_identifier_type(this->scope, id);
	if (res.has_error())
	{
		this->global_ctx.report_error(std::move(res.get_error()));
		return { ast::expression::rvalue, ast::typespec() };
	}
	else
	{
		return std::move(res.get_result());
	}
}

ast::expression::expr_type_t parse_context::get_operation_type(ast::expr_unary_op const &unary_op) const
{
	auto res = this->global_ctx.get_operation_type(this->scope, unary_op);
	if (res.has_error())
	{
		this->global_ctx.report_error(std::move(res.get_error()));
		return { ast::expression::rvalue, ast::typespec() };
	}
	else
	{
		return std::move(res.get_result());
	}
}

ast::expression::expr_type_t parse_context::get_operation_type(ast::expr_binary_op const &binary_op) const
{
	auto res = this->global_ctx.get_operation_type(this->scope, binary_op);
	if (res.has_error())
	{
		this->global_ctx.report_error(std::move(res.get_error()));
		return { ast::expression::rvalue, ast::typespec() };
	}
	else
	{
		return std::move(res.get_result());
	}
}

ast::expression::expr_type_t parse_context::get_function_call_type(ast::expr_function_call const &func_call) const
{
	auto res = this->global_ctx.get_function_call_type(this->scope, func_call);
	if (res.has_error())
	{
		this->global_ctx.report_error(std::move(res.get_error()));
		return { ast::expression::rvalue, ast::typespec() };
	}
	else
	{
		return std::move(res.get_result());
	}
}

bool parse_context::is_convertible(ast::expression::expr_type_t const &from, ast::typespec const &to)
{
	return this->global_ctx.is_convertible(this->scope, from, to);
}

ast::type_info const *parse_context::get_type_info(bz::string_view id) const
{
	return this->global_ctx.get_type_info(this->scope, id);
}

} // namespace ctx
