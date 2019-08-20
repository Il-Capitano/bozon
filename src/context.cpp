#include "context.h"

parse_context context{};


bool are_types_equal(
	ast_typespec_ptr const &lhs,
	ast_typespec_ptr const &rhs
)
{
	if (lhs->kind != rhs->kind)
	{
		return false;
	}

	switch (lhs->kind)
	{
	case type::constant:
	case type::pointer:
	case type::reference:
		return are_types_equal(lhs->base, rhs->base);

	case type::name:
		return lhs->name == rhs->name;

	default:
		assert(false);
		return false;
	}
}

bool compare_params(
	std::vector<ast_typespec_ptr> const &lhs,
	std::vector<ast_typespec_ptr> const &rhs
)
{
	if (lhs.size() != rhs.size())
	{
		return false;
	}

	for (size_t i = 0; i < lhs.size(); ++i)
	{
		if (!are_types_equal(lhs[i], rhs[i]))
		{
			return false;
		}
	}
	return true;
}

void parse_context::add_variable(context_variable_ptr var)
{
	auto it = std::find_if(
		this->variables.back().begin(),
		this->variables.back().end(),
		[&](auto const &old_var)
		{
			return var->identifier == old_var->identifier;
		}
	);

	if (it == this->variables.back().end())
	{
		this->variables.back().emplace_back(std::move(var));
	}
	else
	{
		std::cerr << "Variable '" << var->identifier
			<< "' has already been defined in the current scope\n";
		exit(1);
	}
}

void parse_context::add_function(context_function_ptr func)
{
	auto it = find_if(
		this->functions.back().begin(),
		this->functions.back().end(),
		[&](auto const &old_fn)
		{
			return func->identifier == old_fn->identifier
				&& compare_params(func->param_types, old_fn->param_types);
		}
	);

	if (it == this->functions.back().begin())
	{
		this->functions.back().emplace_back(std::move(func));
	}
	else
	{
		std::cerr << "Function '" << func->identifier
			<< "' has already been defined\n";
		exit(1);
	}
}

void parse_context::add_operator(context_operator_ptr op)
{
	auto it = std::find_if(
		this->operators.back().begin(),
		this->operators.back().end(),
		[&](auto const &old_op)
		{
			return op->op == old_op->op
				&& compare_params(op->param_types, old_op->param_types);
		}
	);

	if (it == this->operators.back().end())
	{
		this->operators.back().emplace_back(std::move(op));
	}
	else
	{
		std::cerr << "Operator " << get_token_value(op->op)
			<< " defined with the same parameters\n";
		exit(1);
	}
}

ast_typespec_ptr parse_context::get_variable_typespec(intern_string id) const
{
	for (
		auto scope = this->variables.rbegin();
		scope != this->variables.rend();
		++scope
	)
	{
		auto it = std::find_if(
			scope->begin(),
			scope->end(),
			[&](auto const &var)
			{
				return id == var->identifier;
			}
		);
		if (it != scope->end())
		{
			return (*it)->type->clone();
		}
	}

	return nullptr;
}

ast_typespec_ptr parse_context::get_binary_operation_typespec(
	uint32_t op, ast_typespec_ptr const &lhs, ast_typespec_ptr const &rhs
)
{
	switch (op)
	{
	case token::comma:
		return rhs->clone();

	case token::assign:
	case token::plus_eq:
	case token::minus_eq:
	case token::multiply_eq:
	case token::divide_eq:
	case token::modulo_eq:
	case token::bit_left_shift_eq:
	case token::bit_right_shift_eq:
	case token::bit_and_eq:
	case token::bit_xor_eq:
	case token::bit_or_eq:
		return lhs->clone();

	case token::bool_and:
	case token::bool_xor:
	case token::bool_or:
		if (lhs->name != "bool" || rhs->name != "bool")
		{
			std::cerr << get_token_value(op) << " operation between non-bool types\n";
			exit(1);
		}
		return make_ast_typespec("bool");

	case token::plus:
	case token::minus:
	case token::multiply:
	case token::divide:
	case token::modulo:
		assert(are_types_equal(lhs, rhs));
		return lhs->clone();

	default:
		assert(false);
		return nullptr;
	}
}

ast_typespec_ptr parse_context::get_unary_operation_typespec(
	uint32_t op, ast_typespec_ptr const &arg
)
{
	switch (op)
	{
		case token::ampersand:
			return make_ast_typespec(type::pointer, arg->clone());

		case token::star:
		// ...
		default:
			assert(false);
			return nullptr;
	}
}

