#include "expression.h"
#include "statement.h"

namespace ast
{

destruct_variable::destruct_variable(expression _destruct_call)
	: destruct_call(make_ast_unique<expression>(std::move(_destruct_call)))
{}

destruct_variable::destruct_variable(destruct_variable const &other)
	: destruct_call(make_ast_unique<expression>(*other.destruct_call))
{}

destruct_variable &destruct_variable::operator = (destruct_variable const &other)
{
	if (this == &other)
	{
		return *this;
	}

	if (this->destruct_call == nullptr)
	{
		this->destruct_call = make_ast_unique<expression>(*other.destruct_call);
	}
	else
	{
		*this->destruct_call = *other.destruct_call;
	}
	return *this;
}

destruct_self::destruct_self(expression _destruct_call)
	: destruct_call(make_ast_unique<expression>(std::move(_destruct_call)))
{}

destruct_self::destruct_self(destruct_self const &other)
	: destruct_call(make_ast_unique<expression>(*other.destruct_call))
{}

destruct_self &destruct_self::operator = (destruct_self const &other)
{
	if (this == &other)
	{
		return *this;
	}

	if (this->destruct_call == nullptr)
	{
		this->destruct_call = make_ast_unique<expression>(*other.destruct_call);
	}
	else
	{
		*this->destruct_call = *other.destruct_call;
	}
	return *this;
}

defer_expression::defer_expression(expression _expr)
	: expr(make_ast_unique<expression>(std::move(_expr)))
{}

defer_expression::defer_expression(defer_expression const &other)
	: expr(make_ast_unique<expression>(*other.expr))
{}

defer_expression &defer_expression::operator = (defer_expression const &other)
{
	if (this == &other)
	{
		return *this;
	}

	if (this->expr == nullptr)
	{
		this->expr = make_ast_unique<expression>(*other.expr);
	}
	else
	{
		*this->expr = *other.expr;
	}
	return *this;
}

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
		&& (const_expr->kind == expression_type_kind::type_name || const_expr->value.is_type());
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

template<typename expr_t, bool get_inner = true>
static bz::meta::conditional<get_inner, expr_t, expression> &get_expr_kind(expression &expr_)
{
	auto &expr = expr_.get_expr();
	if (expr.is<expr_t>())
	{
		if constexpr (get_inner)
		{
			return expr.get<expr_t>();
		}
		else
		{
			return expr_;
		}
	}
	else if (expr.is<expr_compound>())
	{
		return get_expr_kind<expr_t, get_inner>(expr.get<expr_compound>().final_expr);
	}
	else if (expr.is<expr_binary_op>())
	{
		bz_assert(expr.get<expr_binary_op>().op == lex::token::comma);
		return get_expr_kind<expr_t, get_inner>(expr.get<expr_binary_op>().rhs);
	}
	else if (expr.is<expr_subscript>())
	{
		auto &subscript = expr.get<expr_subscript>();
		bz_assert(subscript.base.is_tuple());
		bz_assert(subscript.index.is<constant_expression>());
		auto const &index_value = subscript.index.get<constant_expression>().value;
		bz_assert(index_value.is_sint() || index_value.is_uint());
		auto const index = index_value.is_sint()
			? static_cast<uint64_t>(index_value.get_sint())
			: index_value.get_uint();
		bz_assert(index < subscript.base.get_tuple().elems.size());
		return get_expr_kind<expr_t, get_inner>(subscript.base.get_tuple().elems[index]);
	}
	else
	{
		bz_unreachable;
	}
}

template<typename expr_t, bool get_inner = true>
static bz::meta::conditional<get_inner, expr_t, expression> const &get_expr_kind(expression const &expr_)
{
	auto &expr = expr_.get_expr();
	if (expr.is<expr_t>())
	{
		if constexpr (get_inner)
		{
			return expr.get<expr_t>();
		}
		else
		{
			return expr_;
		}
	}
	else if (expr.is<expr_compound>())
	{
		return get_expr_kind<expr_t, get_inner>(expr.get<expr_compound>().final_expr);
	}
	else if (expr.is<expr_binary_op>())
	{
		bz_assert(expr.get<expr_binary_op>().op == lex::token::comma);
		return get_expr_kind<expr_t, get_inner>(expr.get<expr_binary_op>().rhs);
	}
	else if (expr.is<expr_subscript>())
	{
		auto &subscript = expr.get<expr_subscript>();
		bz_assert(subscript.base.is_tuple());
		bz_assert(subscript.index.is<constant_expression>());
		auto const &index_value = subscript.index.get<constant_expression>().value;
		bz_assert(index_value.is_sint() || index_value.is_uint());
		auto const index = index_value.is_sint()
			? static_cast<uint64_t>(index_value.get_sint())
			: index_value.get_uint();
		bz_assert(index < subscript.base.get_tuple().elems.size());
		return get_expr_kind<expr_t, get_inner>(subscript.base.get_tuple().elems[index]);
	}
	else
	{
		bz_unreachable;
	}
}

static bool is_expr_kind_helper(expression const &expr, expression_type_kind kind)
{
	return (expr.is<constant_expression>() && expr.get<constant_expression>().kind == kind)
		|| (expr.is<dynamic_expression>() && expr.get<dynamic_expression>().kind == kind);
}

bool expression::is_tuple(void) const noexcept
{
	return is_expr_kind_helper(*this, expression_type_kind::tuple);
}

expr_tuple &expression::get_tuple(void) noexcept
{
	bz_assert(this->is_tuple());
	return get_expr_kind<expr_tuple>(*this);
}

expr_tuple const &expression::get_tuple(void) const noexcept
{
	bz_assert(this->is_tuple());
	return get_expr_kind<expr_tuple>(*this);
}

bool expression::is_if_expr(void) const noexcept
{
	return is_expr_kind_helper(*this, expression_type_kind::if_expr);
}

expr_if &expression::get_if_expr(void) noexcept
{
	bz_assert(this->is_if_expr());
	return get_expr_kind<expr_if>(*this);
}

expr_if const &expression::get_if_expr(void) const noexcept
{
	bz_assert(this->is_if_expr());
	return get_expr_kind<expr_if>(*this);
}

bool expression::is_switch_expr(void) const noexcept
{
	return is_expr_kind_helper(*this, expression_type_kind::switch_expr);
}

expr_switch &expression::get_switch_expr(void) noexcept
{
	bz_assert(this->is_switch_expr());
	return get_expr_kind<expr_switch>(*this);
}

expr_switch const &expression::get_switch_expr(void) const noexcept
{
	bz_assert(this->is_switch_expr());
	return get_expr_kind<expr_switch>(*this);
}

bool expression::is_integer_literal(void) const noexcept
{
	return is_expr_kind_helper(*this, expression_type_kind::integer_literal);
}

constant_value &expression::get_integer_literal_value(void) noexcept
{
	bz_assert(this->is_integer_literal());
	auto &literal_expr = get_expr_kind<expr_integer_literal, false>(*this);
	bz_assert(literal_expr.is<constant_expression>());
	return literal_expr.get<constant_expression>().value;
}

constant_value const &expression::get_integer_literal_value(void) const noexcept
{
	bz_assert(this->is_integer_literal());
	auto const &literal_expr = get_expr_kind<expr_integer_literal, false>(*this);
	bz_assert(literal_expr.is<constant_expression>());
	return literal_expr.get<constant_expression>().value;
}

std::pair<literal_kind, constant_value const &> expression::get_integer_literal_kind_and_value(void) const noexcept
{
	bz_assert(this->is_integer_literal());
	auto const &literal_expr = get_expr_kind<expr_integer_literal, false>(*this);
	bz_assert(literal_expr.is<constant_expression>());
	return {
		literal_expr.get<constant_expression>().expr.get<expr_integer_literal>().kind,
		literal_expr.get<constant_expression>().value
	};
}

bool expression::is_null_literal(void) const noexcept
{
	return is_expr_kind_helper(*this, expression_type_kind::null_literal);
}

bool expression::is_placeholder_literal(void) const noexcept
{
	return is_expr_kind_helper(*this, expression_type_kind::placeholder_literal);
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

void expression::set_type(typespec new_type)
{
	if (this->is<constant_expression>())
	{
		this->get<constant_expression>().type = std::move(new_type);
	}
	else if (this->is<dynamic_expression>())
	{
		this->get<dynamic_expression>().type = std::move(new_type);
	}
}

void expression::set_type_kind(expression_type_kind new_kind)
{
	if (this->is<constant_expression>())
	{
		this->get<constant_expression>().kind = new_kind;
	}
	else if (this->is<dynamic_expression>())
	{
		this->get<dynamic_expression>().kind = new_kind;
	}
}

std::pair<typespec_view, expression_type_kind> expression::get_expr_type_and_kind(void) const noexcept
{
	switch (this->kind())
	{
	case expression::index_of<constant_expression>:
	{
		auto &const_expr = this->get<constant_expression>();
		return { const_expr.type, const_expr.kind };
	}
	case expression::index_of<dynamic_expression>:
	{
		auto &dyn_expr = this->get<dynamic_expression>();
		return { dyn_expr.type, dyn_expr.kind };
	}
	default:
		return { typespec_view(), static_cast<expression_type_kind>(0) };
	}
}

typespec_view expression::get_expr_type(void) const noexcept
{
	switch (this->kind())
	{
	case expression::index_of<constant_expression>:
	{
		auto &const_expr = this->get<constant_expression>();
		return const_expr.type;
	}
	case expression::index_of<dynamic_expression>:
	{
		auto &dyn_expr = this->get<dynamic_expression>();
		return dyn_expr.type;
	}
	default:
		return typespec_view();
	}
}

bool expression::is_constant(void) const noexcept
{
	return this->is<constant_expression>();
}

constant_expression &expression::get_constant(void) noexcept
{
	bz_assert(this->is_constant());
	return this->get<constant_expression>();
}

constant_expression const &expression::get_constant(void) const noexcept
{
	bz_assert(this->is_constant());
	return this->get<constant_expression>();
}

constant_value &expression::get_constant_value(void) noexcept
{
	bz_assert(this->is_constant());
	return this->get<constant_expression>().value;
}

constant_value const &expression::get_constant_value(void) const noexcept
{
	bz_assert(this->is_constant());
	return this->get<constant_expression>().value;
}

bool expression::is_dynamic(void) const noexcept
{
	return this->is<dynamic_expression>();
}

dynamic_expression &expression::get_dynamic(void) noexcept
{
	bz_assert(this->is_dynamic());
	return this->get<dynamic_expression>();
}

dynamic_expression const &expression::get_dynamic(void) const noexcept
{
	bz_assert(this->is_dynamic());
	return this->get<dynamic_expression>();
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
	  scope     (make_local_scope(_enclosing_scope, false))
{}

} // namespace ast
