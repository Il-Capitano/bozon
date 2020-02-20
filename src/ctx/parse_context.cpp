#include "parse_context.h"

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
	switch (decl.kind())
	{
	case ast::declaration::index<ast::decl_variable>:
		this->add_global_variable(*decl.get<ast::decl_variable_ptr>(), errors);
		break;
	case ast::declaration::index<ast::decl_function>:
		this->add_global_function(*decl.get<ast::decl_function_ptr>(), errors);
		break;
	case ast::declaration::index<ast::decl_operator>:
		this->add_global_operator(*decl.get<ast::decl_operator_ptr>(), errors);
		break;
	case ast::declaration::index<ast::decl_struct>:
		this->add_global_struct(*decl.get<ast::decl_struct_ptr>(), errors);
		break;
	default:
		assert(false);
		break;
	}
}

void parse_context::add_global_variable(
	ast::decl_variable &var_decl,
	bz::vector<error> &errors
)
{
	auto const it = std::find_if(
		this->global_variables.begin(),
		this->global_variables.end(),
		[&](auto const &var) {
			return var->identifier->value == var_decl.identifier->value;
		}
	);

	if (it != this->global_variables.end())
	{
		errors.emplace_back(bad_token(
			var_decl.identifier,
			bz::format("variable '{}' has already been declared", (*it)->identifier->value),
			{ make_note((*it)->identifier, "previous declaration:") }
		));
	}
	else
	{
		this->global_variables.emplace_back(&var_decl);
	}
}

void parse_context::add_global_function(
	ast::decl_function &func_decl,
	bz::vector<error> &
)
{
	auto set = std::find_if(
		this->global_functions.begin(),
		this->global_functions.end(),
		[&](auto &set) {
			return set.id == func_decl.identifier->value;
		}
	);

	if (set == this->global_functions.end())
	{
		this->global_functions.push_back({
			func_decl.identifier->value,
			{ &func_decl }
		});
	}
	else
	{
		// TODO: we need to check, if there are conflicts
		set->functions.push_back(&func_decl);
	}
}

void parse_context::add_global_operator(
	ast::decl_operator &op_decl,
	bz::vector<error> &
)
{
	auto set = std::find_if(
		this->global_operators.begin(),
		this->global_operators.end(),
		[&](auto &set) {
			return set.op == op_decl.op->kind;
		}
	);

	if (set == this->global_operators.end())
	{
		this->global_operators.push_back({
			op_decl.op->kind,
			{ &op_decl }
		});
	}
	else
	{
		// TODO: we need to check, if there are conflicts
		set->operators.push_back(&op_decl);
	}
}

void parse_context::add_global_struct(
	ast::decl_struct &,
	bz::vector<error> &
)
{
}


void parse_context::add_local_variable(ast::decl_variable &var_decl)
{
	assert(this->scope_variables.size() != 0);
	this->scope_variables.back().push_back(&var_decl);
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
				return var->identifier->value == id->value;
			}
		);
		if (var != scope->rend())
		{
			return (*var)->var_type;
		}
	}

	auto const var = std::find_if(
		this->global_variables.begin(),
		this->global_variables.end(),
		[&](auto const &var) {
			return var->identifier->value == id->value;
		}
	);

	if (var != this->global_variables.end())
	{
		return (*var)->var_type;
	}
	else
	{
		errors.emplace_back(lex::bad_token(id, "undeclared identifier"));
		return ast::typespec();
	}
}

static bool are_directly_matchable_types(
	ast::expression::expr_type_t const &from,
	ast::typespec const &to
)
{
	if (
		to.kind() == ast::typespec::index<ast::ts_reference>
		&& from.type_kind != ast::expression::expr_type_kind::lvalue
		&& from.type_kind != ast::expression::expr_type_kind::lvalue_reference
	)
	{
		return false;
	}

	ast::typespec const *to_it = [&]() -> ast::typespec const * {
		if (to.kind() == ast::typespec::index<ast::ts_reference>)
		{
			return &to.get<ast::ts_reference_ptr>()->base;
		}
		else if (to.kind() == ast::typespec::index<ast::ts_constant>)
		{
			return &to.get<ast::ts_constant_ptr>()->base;
		}
		else
		{
			return &to;
		}
	}();

	auto from_it = &from.expr_type;

	auto const advance = [](ast::typespec const *&it)
	{
		switch (it->kind())
		{
		case ast::typespec::index<ast::ts_pointer>:
			it = &it->get<ast::ts_pointer_ptr>()->base;
			break;
		case ast::typespec::index<ast::ts_constant>:
			it = &it->get<ast::ts_constant_ptr>()->base;
			break;
		default:
			assert(false);
			break;
		}
	};

	while (true)
	{
		if (
			to_it->kind() == ast::typespec::index<ast::ts_base_type>
			&& from_it->kind() == ast::typespec::index<ast::ts_base_type>
		)
		{
			return to_it->get<ast::ts_base_type_ptr>()->info
				== from_it->get<ast::ts_base_type_ptr>()->info;
		}
		else if (to_it->kind() == from_it->kind())
		{
			advance(to_it);
			advance(from_it);
		}
		else if (to_it->kind() == ast::typespec::index<ast::ts_constant>)
		{
			advance(to_it);
		}
		else
		{
			return false;
		}
	}

	return false;
}

static bool is_built_in_type(ast::typespec const &ts)
{
	switch (ts.kind())
	{
	case ast::typespec::index<ast::ts_constant>:
		return is_built_in_type(ts.get<ast::ts_constant_ptr>()->base);
	case ast::typespec::index<ast::ts_base_type>:
	{
		auto &base = *ts.get<ast::ts_base_type_ptr>();
		return (base.info->flags & ast::type_info::built_in) != 0;
	}
	case ast::typespec::index<ast::ts_pointer>:
	case ast::typespec::index<ast::ts_function>:
	case ast::typespec::index<ast::ts_tuple>:
		return true;
	default:
		return false;
	}
}

static ast::typespec get_built_in_operation_type(
	ast::expr_unary_op const &unary_op,
	bz::vector<error> &errors
)
{
	switch (unary_op.op->kind)
	{
	case lex::token::ampersand:
		if (
			unary_op.expr.expr_type.type_kind == ast::expression::lvalue
			|| unary_op.expr.expr_type.type_kind == ast::expression::lvalue_reference
		)
		{
			return ast::make_ts_pointer(unary_op.expr.expr_type.expr_type);
		}
		else
		{
			errors.emplace_back(lex::bad_tokens(unary_op, "cannot take address of an rvalue"));
			return ast::typespec();
		}
	default:
		assert(false);
	}
	return ast::typespec();
}

ast::typespec parse_context::get_operation_type(
	ast::expr_unary_op const &unary_op,
	bz::vector<error> &errors
) const
{
	if (is_built_in_type(unary_op.expr.expr_type.expr_type))
	{
		return get_built_in_operation_type(unary_op, errors);
	}

	auto const set = std::find_if(
		this->global_operators.begin(),
		this->global_operators.end(),
		[&](auto &set) {
			return set.op == unary_op.op->kind;
		}
	);

	if (set == this->global_operators.end())
	{
		errors.emplace_back(lex::bad_tokens(
			unary_op,
			bz::format("undeclared unary operator {}", unary_op.op->value)
		));
		return ast::typespec();
	}

	for (auto op : set->operators)
	{
		if (op->params.size() != 1)
		{
			continue;
		}

		if (are_directly_matchable_types(unary_op.expr.expr_type, op->params[0].var_type))
		{
			return op->return_type;
		}
	}

	errors.emplace_back(lex::bad_tokens(
		unary_op,
		bz::format("undeclared unary operator {}", unary_op.op->value)
	));
	return ast::typespec();
}

ast::typespec parse_context::get_operation_type(
	ast::expr_binary_op const &binary_op,
	bz::vector<error> &errors
) const
{
	auto const report_undeclared = [&]()
	{
		if (binary_op.op->kind == lex::token::square_open)
		{
			errors.emplace_back(lex::bad_tokens(binary_op, "undeclared binary operator []"));
		}
		else
		{
			errors.emplace_back(lex::bad_tokens(
				binary_op,
				bz::format("undeclared binary operator {}", binary_op.op->value)
			));
		}
	};

	auto const set = std::find_if(
		this->global_operators.begin(),
		this->global_operators.end(),
		[&](auto &set) {
			return set.op == binary_op.op->kind;
		}
	);

	if (set == this->global_operators.end())
	{
		report_undeclared();
		return ast::typespec();
	}

	for (auto op : set->operators)
	{
		if (op->params.size() != 2)
		{
			continue;
		}

		if (
			are_directly_matchable_types(binary_op.lhs.expr_type, op->params[0].var_type)
			&& are_directly_matchable_types(binary_op.rhs.expr_type, op->params[1].var_type)
		)
		{
			return op->return_type;
		}
	}

	report_undeclared();
	return ast::typespec();
}

ast::type_info const *parse_context::get_type_info(bz::string_view id) const
{
	for (auto it = this->types.begin(); it != this->types.end(); ++it)
	{
		if (it->identifier == id)
		{
			// getting the pointer...
			return it.operator->();
		}
	}

	return nullptr;
}

} // namespace ctx
