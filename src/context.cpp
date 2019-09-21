#include "context.h"

parse_context context;

void parse_context::add_variable(
	intern_string id,
	ast_typespec_ptr type
)
{
	this->variables.back().push_back({ id, type });
}

void parse_context::add_function(
	intern_string id,
	ast_function_type_ptr type
)
{
	auto it = std::find_if(
		this->functions.begin(), this->functions.end(), [&](auto const &set)
		{
			return set.id == id;
		}
	);

	if (it == this->functions.end())
	{
		this->functions.push_back({ id, { type } });
	}
	else
	{
		auto set_it = std::find_if(it->set.begin(), it->set.end(), [&](auto const &t)
		{
			if (t->argument_types.size() != type->argument_types.size())
			{
				return false;
			}

			for (size_t i = 0; i < type->argument_types.size(); ++i)
			{
				if (!t->argument_types[i]->equals(type->argument_types[i]))
				{
					return false;
				}
			}

			return true;
		});

		if (set_it != it->set.end())
		{
			fatal_error("Error: redefinition of function '{}'\n", id);
		}

		it->set.emplace_back(type);
	}
}

void parse_context::add_operator(
	uint32_t op,
	ast_function_type_ptr type
)
{
	auto it = std::find_if(
		this->operators.begin(), this->operators.end(), [&](auto const &set)
		{
			return set.op == op;
		}
	);

	if (it == this->operators.end())
	{
		this->operators.push_back({ op, { type } });
	}
	else
	{
		auto set_it = std::find_if(it->set.begin(), it->set.end(), [&](auto const &t)
		{
			if (t->argument_types.size() != type->argument_types.size())
			{
				return false;
			}

			for (size_t i = 0; i < type->argument_types.size(); ++i)
			{
				if (!t->argument_types[i]->equals(type->argument_types[i]))
				{
					return false;
				}
			}

			return true;
		});

		if (set_it != it->set.end())
		{
			fatal_error("Error: redefinition of operator '{}'\n", get_token_value(op));
		}

		it->set.emplace_back(type);
	}
}


ast_typespec_ptr parse_context::get_variable_type(intern_string id)
{
	for (auto &scope : this->variables)
	{
		auto it = std::find_if(scope.rbegin(), scope.rend(), [&](auto const &var)
		{
			return var.id == id;
		});

		if (it != scope.rend())
		{
			return it->type;
		}
	}

	return nullptr;
}

ast_typespec_ptr parse_context::get_function_type(
	intern_string id,
	bz::vector<ast_typespec_ptr> const &args
)
{
	auto set_it = std::find_if(
		this->functions.begin(), this->functions.end(), [&](auto const &set)
		{
			return set.id == id;
		}
	);

	if (set_it == this->functions.end())
	{
		return nullptr;
	}

	auto &set = set_it->set;

	for (auto &fn : set)
	{
		if (fn->argument_types.size() != args.size())
		{
			continue;
		}

		size_t i;
		for (i = 0; i < args.size(); ++i)
		{
			if (!fn->argument_types[i]->equals(args[i]))
			{
				break;
			}
		}

		if (i == args.size())
		{
			return fn->return_type;
		}
	}

	return nullptr;
}

ast_typespec_ptr parse_context::get_operator_type(
	uint32_t op,
	bz::vector<ast_typespec_ptr> const &args
)
{
	auto set_it = std::find_if(
		this->operators.begin(), this->operators.end(), [&](auto const &set)
		{
			return set.op == op;
		}
	);

	if (set_it == this->operators.end())
	{
		return nullptr;
	}

	auto &set = set_it->set;

	for (auto &op : set)
	{
		if (op->argument_types.size() != args.size())
		{
			continue;
		}

		size_t i;
		for (i = 0; i < args.size(); ++i)
		{
			if (!op->argument_types[i]->equals(args[i]))
			{
				break;
			}
		}

		if (i == args.size())
		{
			return op->return_type;
		}
	}

	return nullptr;
}
