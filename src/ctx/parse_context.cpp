#include "parse_context.h"
#include "global_context.h"

namespace ctx
{

void parse_context::add_scope(void)
{
	this->scope_variables.push_back({});
}

void parse_context::remove_scope(void)
{
	this->scope_variables.pop_back();
}


void parse_context::add_global_declaration(
	ast::declaration &decl,
	bz::vector<error> &errors
)
{
	auto res = this->global_ctx->add_global_declaration(this->scope, decl);
	if (res.has_error())
	{
		errors.emplace_back(std::move(res.get_error()));
	}
}

void parse_context::add_global_variable(
	ast::decl_variable &var_decl,
	bz::vector<error> &errors
)
{
	auto res = this->global_ctx->add_global_variable(this->scope, var_decl);
	if (res.has_error())
	{
		errors.emplace_back(std::move(res.get_error()));
	}
}

void parse_context::add_global_function(
	ast::decl_function &func_decl,
	bz::vector<error> &errors
)
{
	auto res = this->global_ctx->add_global_function(this->scope, func_decl);
	if (res.has_error())
	{
		errors.emplace_back(std::move(res.get_error()));
	}
}

void parse_context::add_global_operator(
	ast::decl_operator &op_decl,
	bz::vector<error> &errors
)
{
	auto res = this->global_ctx->add_global_operator(this->scope, op_decl);
	if (res.has_error())
	{
		errors.emplace_back(std::move(res.get_error()));
	}
}

void parse_context::add_global_struct(
	ast::decl_struct &struct_decl,
	bz::vector<error> &errors
)
{
	auto res = this->global_ctx->add_global_struct(this->scope, struct_decl);
	if (res.has_error())
	{
		errors.emplace_back(std::move(res.get_error()));
	}
}


void parse_context::add_local_variable(ast::variable var_decl)
{
	assert(this->scope_variables.size() != 0);
	this->scope_variables.back().emplace_back(std::move(var_decl));
}


ast::typespec parse_context::get_identifier_type(
	lex::token_pos id,
	bz::vector<error> &errors
) const
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
			return var->var_type;
		}
	}

	auto res = this->global_ctx->get_identifier_type(this->scope, id);
	if (res.has_error())
	{
		errors.emplace_back(std::move(res.get_error()));
		return ast::typespec();
	}
	else
	{
		return std::move(res.get_result());
	}
}

ast::typespec parse_context::get_operation_type(
	ast::expr_unary_op const &unary_op,
	bz::vector<error> &errors
) const
{
	auto res = this->global_ctx->get_operation_type(this->scope, unary_op);
	if (res.has_error())
	{
		errors.emplace_back(std::move(res.get_error()));
		return ast::typespec();
	}
	else
	{
		return std::move(res.get_result());
	}
}

ast::typespec parse_context::get_operation_type(
	ast::expr_binary_op const &binary_op,
	bz::vector<error> &errors
) const
{
	auto res = this->global_ctx->get_operation_type(this->scope, binary_op);
	if (res.has_error())
	{
		errors.emplace_back(std::move(res.get_error()));
		return ast::typespec();
	}
	else
	{
		return std::move(res.get_result());
	}
}

ast::typespec parse_context::get_function_call_type(
	ast::expr_function_call const &func_call,
	bz::vector<error> &errors
) const
{
	auto res = this->global_ctx->get_function_call_type(this->scope, func_call);
	if (res.has_error())
	{
		errors.emplace_back(std::move(res.get_error()));
		return ast::typespec();
	}
	else
	{
		return std::move(res.get_result());
	}
}

ast::type_info const *parse_context::get_type_info(bz::string_view id) const
{
	return this->global_ctx->get_type_info(this->scope, id);
}

} // namespace ctx
