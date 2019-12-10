#include "context.h"
#include "ast/statement.h"

parse_context context;

parse_context::parse_context(void)
	: variables{{}},
	  functions{},
	  operators{},
	  types{
		  ast::int8_, ast::int16_, ast::int32_, ast::int64_,
		  ast::uint8_, ast::uint16_, ast::uint32_, ast::uint64_,
		  ast::float32_, ast::float64_,
		  ast::char_, ast::bool_, ast::str_, ast::void_, ast::null_t_
	  }
{}

bool parse_context::add_variable(
	src_tokens::pos id,
	ast::typespec   type
)
{
	if (&*id == nullptr || id->value == "")
	{
		return false;
	}

	this->variables.back().push_back({ id, type });
	return true;
}

void parse_context::add_function(ast::decl_function_ptr &func_decl)
{
	bz::vector<ast::typespec> param_types;

	for (auto &param : func_decl->params)
	{
		param_types.push_back(param.type);
	}

	ast::ts_function func_type{
		func_decl->return_type,
		std::move(param_types)
	};
	auto id = func_decl->identifier;

	auto it = std::find_if(
		this->functions.begin(), this->functions.end(), [&](auto const &set)
		{
			return set.id == id->value;
		}
	);

	if (it == this->functions.end())
	{
		this->functions.push_back({ id->value, { func_type } });
	}
	else
	{
		auto set_it = std::find_if(it->set.begin(), it->set.end(), [&](auto const &t)
		{
			if (t.argument_types.size() != func_type.argument_types.size())
			{
				return false;
			}

			for (size_t i = 0; i < func_type.argument_types.size(); ++i)
			{
				if (t.argument_types[i] != func_type.argument_types[i])
				{
					return false;
				}
			}

			return true;
		});

		if (set_it != it->set.end())
		{
			bad_token(id, "Redefinition of function");
		}

		it->set.emplace_back(func_type);
	}
}

void parse_context::add_operator(ast::decl_operator_ptr &op_decl)
{
	bz::vector<ast::typespec> param_types;

	for (auto &param : op_decl->params)
	{
		param_types.push_back(param.type);
	}

	ast::ts_function op_type{
		op_decl->return_type,
		std::move(param_types)
	};
	auto op = op_decl->op;

	auto it = std::find_if(
		this->operators.begin(), this->operators.end(), [&](auto const &set)
		{
			return set.op == op->kind;
		}
	);

	if (it == this->operators.end())
	{
		this->operators.push_back({ op->kind, { op_type } });
	}
	else
	{
		auto set_it = std::find_if(it->set.begin(), it->set.end(), [&](auto const &t)
		{
			if (t.argument_types.size() != op_type.argument_types.size())
			{
				return false;
			}

			for (size_t i = 0; i < op_type.argument_types.size(); ++i)
			{
				if (t.argument_types[i] != op_type.argument_types[i])
				{
					return false;
				}
			}

			return true;
		});

		if (set_it != it->set.end())
		{
			bad_token(op, "Redefinition of operator");
		}

		it->set.emplace_back(op_type);
	}
}


bool parse_context::is_variable(bz::string id)
{
	for (auto &scope : this->variables)
	{
		auto it = std::find_if(scope.begin(), scope.end(), [&](auto const &var)
		{
			return var.id->value == id;
		});

		if (it != scope.end())
		{
			return true;
		}
	}

	return false;
}

bool parse_context::is_function(bz::string id)
{
	auto it = std::find_if(
		this->functions.begin(),
		this->functions.end(),
		[&](auto const &fn)
		{
			return fn.id == id;
		}
	);

	return it != this->functions.end();
}


ast::type_ptr parse_context::get_type(src_tokens::pos id)
{
	auto it = std::find_if(this->types.begin(), this->types.end(), [&](auto const &t)
	{
		return t->name == id->value;
	});

	if (it == this->types.end())
	{
		bad_token(id, "Error: unknown type");
	}

	return *it;
}

ast::typespec parse_context::get_identifier_type(src_tokens::pos t)
{
	assert(t->kind == token::identifier);

	auto id = t->value;
	for (
		auto scope = this->variables.rbegin();
		scope != this->variables.rend();
		++scope
	)
	{
		auto it = std::find_if(scope->rbegin(), scope->rend(), [&](auto const &var)
		{
			return var.id->value == id;
		});

		if (it != scope->rend())
		{
			return it->type;
		}
	}

	// it is not a variable, so maybe it is a function
	auto it = std::find_if(this->functions.begin(), this->functions.end(), [&](auto const &set)
	{
		return set.id == id;
	});

	if (it == this->functions.end())
	{
		bad_token(t, "Error: undeclared identifier");
	}

	if (it->set.size() != 1)
	{
		bad_token(t, "Error: identifier is ambiguous");
	}

	return ast::typespec(std::make_unique<ast::ts_function>(it->set[0]));
}

ast::typespec parse_context::get_function_type(
	bz::string id,
	bz::vector<ast::typespec> const &args
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
		fatal_error("Error: unknown function: '{}'", id);
	}

	auto &set = set_it->set;

	for (auto &fn : set)
	{
		if (fn.argument_types.size() != args.size())
		{
			continue;
		}

		size_t i;
		for (i = 0; i < args.size(); ++i)
		{
			if (fn.argument_types[i] != args[i])
			{
				break;
			}
		}

		if (i == args.size())
		{
			return fn.return_type;
		}
	}

	fatal_error("Error: unknown function: '{}'", id);
}

ast::typespec parse_context::get_operator_type(
	src_tokens::pos op,
	bz::vector<ast::typespec> const &args
)
{
	auto set_it = std::find_if(
		this->operators.begin(), this->operators.end(), [&](auto const &set)
		{
			return set.op == op->kind;
		}
	);

	if (set_it == this->operators.end())
	{
		bad_token(op, "Error: Undeclared operator");
	}

	auto &set = set_it->set;

	for (auto &op : set)
	{
		if (op.argument_types.size() != args.size())
		{
			continue;
		}

		size_t i;
		for (i = 0; i < args.size(); ++i)
		{
			if (op.argument_types[i] != args[i])
			{
				break;
			}
		}

		if (i == args.size())
		{
			return op.return_type;
		}
	}

	bad_token(op, "Error: Undeclared operator");
}
