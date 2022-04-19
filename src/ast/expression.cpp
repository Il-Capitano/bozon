#include "expression.h"
#include "statement.h"

namespace ast
{

void expression::to_error(void)
{
	if (this->is<constant_expression>())
	{
		this->emplace<error_expression>(this->get_by_move<constant_expression>().expr);
	}
	else if (this->is<dynamic_expression>())
	{
		this->emplace<error_expression>(this->get_by_move<dynamic_expression>().expr);
	}
}

bool expression::is_error(void) const
{ return this->is<error_expression>(); }

bool expression::not_error(void) const
{ return !this->is<error_expression>(); }

bool expression::is_function(void) const noexcept
{
	auto const const_expr = this->get_if<constant_expression>();
	return const_expr
		&& const_expr->kind == expression_type_kind::function_name;
}

bool expression::is_overloaded_function(void) const noexcept
{
	auto const const_expr = this->get_if<constant_expression>();
	return const_expr
		&& const_expr->kind == expression_type_kind::function_name
		&& const_expr->type.is_empty();
}

bool expression::is_typename(void) const noexcept
{
	auto const const_expr = this->get_if<constant_expression>();
	return const_expr
		&& (const_expr->kind == expression_type_kind::type_name || const_expr->value.is<constant_value::type>());
}

typespec &expression::get_typename(void) noexcept
{
	bz_assert(this->is_typename());
	return this->get<constant_expression>().value.get<constant_value::type>();
}

typespec const &expression::get_typename(void) const noexcept
{
	bz_assert(this->is_typename());
	return this->get<constant_expression>().value.get<constant_value::type>();
}

bool expression::is_tuple(void) const noexcept
{
	return (this->is<constant_expression>() && this->get<constant_expression>().kind == expression_type_kind::tuple)
		|| (this->is<dynamic_expression>() && this->get<dynamic_expression>().kind == expression_type_kind::tuple);
}

expr_tuple &expression::get_tuple(void) noexcept
{
	bz_assert(this->is_tuple());
	auto &expr = this->get_expr();
	if (expr.is<expr_compound>())
	{
		return expr.get<expr_compound>().final_expr.get_tuple();
	}
	else
	{
		bz_assert(expr.is<expr_tuple>());
		return expr.get<expr_tuple>();
	}
}

expr_tuple const &expression::get_tuple(void) const noexcept
{
	bz_assert(this->is_tuple());
	auto &expr = this->get_expr();
	if (expr.is<expr_compound>())
	{
		return expr.get<expr_compound>().final_expr.get_tuple();
	}
	else
	{
		bz_assert(expr.is<expr_tuple>());
		return expr.get<expr_tuple>();
	}
}

bool expression::is_if_expr(void) const noexcept
{
	return (this->is<constant_expression>() && this->get<constant_expression>().kind == expression_type_kind::if_expr)
		|| (this->is<dynamic_expression>() && this->get<dynamic_expression>().kind == expression_type_kind::if_expr);
}

expr_if &expression::get_if_expr(void) noexcept
{
	bz_assert(this->is_if_expr());
	auto &expr = this->get_expr();
	if (expr.is<expr_compound>())
	{
		return expr.get<expr_compound>().final_expr.get_if_expr();
	}
	else
	{
		bz_assert(expr.is<expr_if>());
		return expr.get<expr_if>();
	}
}

expr_if const &expression::get_if_expr(void) const noexcept
{
	bz_assert(this->is_if_expr());
	auto &expr = this->get_expr();
	if (expr.is<expr_compound>())
	{
		return expr.get<expr_compound>().final_expr.get_if_expr();
	}
	else
	{
		bz_assert(expr.is<expr_if>());
		return expr.get<expr_if>();
	}
}

bool expression::is_switch_expr(void) const noexcept
{
	return (this->is<constant_expression>() && this->get<constant_expression>().kind == expression_type_kind::switch_expr)
		|| (this->is<dynamic_expression>() && this->get<dynamic_expression>().kind == expression_type_kind::switch_expr);
}

expr_switch &expression::get_switch_expr(void) noexcept
{
	bz_assert(this->is_switch_expr());
	auto &expr = this->get_expr();
	if (expr.is<expr_compound>())
	{
		return expr.get<expr_compound>().final_expr.get_switch_expr();
	}
	else
	{
		bz_assert(expr.is<expr_switch>());
		return expr.get<expr_switch>();
	}
}

expr_switch const &expression::get_switch_expr(void) const noexcept
{
	bz_assert(this->is_switch_expr());
	auto &expr = this->get_expr();
	if (expr.is<expr_compound>())
	{
		return expr.get<expr_compound>().final_expr.get_switch_expr();
	}
	else
	{
		bz_assert(expr.is<expr_switch>());
		return expr.get<expr_switch>();
	}
}

bool expression::is_literal(void) const noexcept
{
	if (!this->is_constant_or_dynamic())
	{
		return false;
	}
	auto &expr = this->get_expr();
	return expr.is<expr_literal>() || (expr.is<expr_compound>() && expr.get<expr_compound>().final_expr.is_literal());
}

expr_literal &expression::get_literal(void) noexcept
{
	bz_assert(this->is_literal());
	auto &expr = this->get_expr();
	if (expr.is<expr_compound>())
	{
		return expr.get<expr_compound>().final_expr.get_literal();
	}
	else
	{
		bz_assert(expr.is<expr_literal>());
		return expr.get<expr_literal>();
	}
}

expr_literal const &expression::get_literal(void) const noexcept
{
	bz_assert(this->is_literal());
	auto &expr = this->get_expr();
	if (expr.is<expr_compound>())
	{
		return expr.get<expr_compound>().final_expr.get_literal();
	}
	else
	{
		bz_assert(expr.is<expr_literal>());
		return expr.get<expr_literal>();
	}
}

constant_value &expression::get_literal_value(void) noexcept
{
	bz_assert(this->is_literal());
	if (this->is<constant_expression>())
	{
		return this->get<constant_expression>().value;
	}
	else
	{
		bz_assert(this->get_expr().is<expr_compound>());
		return this->get_expr().get<expr_compound>().final_expr.get_literal_value();
	}
}

constant_value const &expression::get_literal_value(void) const noexcept
{
	bz_assert(this->is_literal());
	if (this->is<constant_expression>())
	{
		return this->get<constant_expression>().value;
	}
	else
	{
		bz_assert(this->get_expr().is<expr_compound>());
		return this->get_expr().get<expr_compound>().final_expr.get_literal_value();
	}
}

bool expression::is_generic_type(void) const noexcept
{
	if (!this->is_typename())
	{
		return false;
	}

	auto const type = this->get_typename().as_typespec_view();
	return type.is<ts_base_type>() && type.get<ts_base_type>().info->is_generic();
}

type_info *expression::get_generic_type(void) const noexcept
{
	bz_assert(this->is_generic_type());
	return this->get_typename().get<ts_base_type>().info;
}

void expression::set_type(ast::typespec new_type)
{
	if (this->is<ast::constant_expression>())
	{
		this->get<ast::constant_expression>().type = std::move(new_type);
	}
	else if (this->is<ast::dynamic_expression>())
	{
		this->get<ast::dynamic_expression>().type = std::move(new_type);
	}
}

void expression::set_type_kind(expression_type_kind new_kind)
{
	if (this->is<ast::constant_expression>())
	{
		this->get<ast::constant_expression>().kind = new_kind;
	}
	else if (this->is<ast::dynamic_expression>())
	{
		this->get<ast::dynamic_expression>().kind = new_kind;
	}
}

std::pair<typespec_view, expression_type_kind> expression::get_expr_type_and_kind(void) const noexcept
{
	switch (this->kind())
	{
	case ast::expression::index_of<ast::constant_expression>:
	{
		auto &const_expr = this->get<ast::constant_expression>();
		return { const_expr.type, const_expr.kind };
	}
	case ast::expression::index_of<ast::dynamic_expression>:
	{
		auto &dyn_expr = this->get<ast::dynamic_expression>();
		return { dyn_expr.type, dyn_expr.kind };
	}
	default:
		return { ast::typespec_view(), static_cast<ast::expression_type_kind>(0) };
	}
}

bool expression::is_constant_or_dynamic(void) const noexcept
{
	return this->is<constant_expression>() || this->is<dynamic_expression>();
}

bool expression::is_unresolved(void) const noexcept
{
	return this->is<unresolved_expression>();
}

bool expression::is_none(void) const noexcept
{
	return this->is_constant_or_dynamic()
		&& this->get_expr_type_and_kind().second == expression_type_kind::none;
}

bool expression::is_noreturn(void) const noexcept
{
	return this->is_constant_or_dynamic()
		&& this->get_expr_type_and_kind().second == expression_type_kind::noreturn;
}

bool expression::has_consteval_succeeded(void) const noexcept
{
	return this->consteval_state == consteval_succeeded;
}

bool expression::has_consteval_failed(void) const noexcept
{
	return this->consteval_state == consteval_failed;
}

expr_t &expression::get_expr(void)
{
	bz_assert(this->is<constant_expression>() || this->is<dynamic_expression>());
	if (this->is<constant_expression>())
	{
		return this->get<constant_expression>().expr;
	}
	else
	{
		return this->get<dynamic_expression>().expr;
	}
}

expr_t const &expression::get_expr(void) const
{
	bz_assert(this->is<constant_expression>() || this->is<dynamic_expression>());
	if (this->is<constant_expression>())
	{
		return this->get<constant_expression>().expr;
	}
	else
	{
		return this->get<dynamic_expression>().expr;
	}
}

unresolved_expr_t &expression::get_unresolved_expr(void)
{
	bz_assert(this->is_unresolved());
	return this->get<unresolved_expression>().expr;
}

unresolved_expr_t const &expression::get_unresolved_expr(void) const
{
	bz_assert(this->is_unresolved());
	return this->get<unresolved_expression>().expr;
}

bool expression::is_special_top_level(void) const noexcept
{
	if (this->is_constant_or_dynamic())
	{
		auto &expr = this->get_expr();
		return (expr.is<expr_compound>() && this->paren_level == 0)
			|| (expr.is<expr_if>()       && this->paren_level == 0)
			|| (expr.is<expr_switch>()   && this->paren_level == 0);
	}
	else if (this->is_unresolved())
	{
		auto &expr = this->get_unresolved_expr();
		return (expr.is<expr_compound>() && this->paren_level == 0)
			|| (expr.is<expr_if>()       && this->paren_level == 0)
			|| (expr.is<expr_switch>()   && this->paren_level == 0);
	}
	else
	{
		return false;
	}
}

expr_compound::expr_compound(
	arena_vector<statement> _statements,
	expression              _final_expr
)
	: statements(std::move(_statements)),
	  final_expr(std::move(_final_expr)),
	  scope     ()
{}

expr_compound::expr_compound(
	arena_vector<statement> _statements,
	expression              _final_expr,
	enclosing_scope_t       _enclosing_scope
)
	: statements(std::move(_statements)),
	  final_expr(std::move(_final_expr)),
	  scope     (make_local_scope(_enclosing_scope))
{}

} // namespace ast
