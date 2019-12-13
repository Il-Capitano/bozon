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
		param_types.push_back(param.var_type);
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
		param_types.push_back(param.var_type);
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

void parse_context::add_type(ast::decl_struct_ptr &struct_decl)
{
	auto it = std::find_if(
		this->types.begin(),
		this->types.end(),
		[&](auto const &t)
		{
			return t->name == struct_decl->identifier->value;
		}
	);

	if (it != this->types.end())
	{
		bad_token(struct_decl->identifier, "Error: Redefinition of type");
	}

	this->types.push_back(
		ast::make_user_defined_type_ptr(
			struct_decl->identifier->value,
			struct_decl->member_variables
		)
	);
}

bool parse_context::is_variable(bz::string_view id)
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

bool parse_context::is_function(bz::string_view id)
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
			return it->var_type;
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

ast::typespec parse_context::get_function_call_type(ast::expr_function_call const &fn_call)
{
	if (
		fn_call.called.kind() == ast::expression::index<ast::expr_identifier>
		&& !is_variable(fn_call.called.get<ast::expr_identifier_ptr>()->identifier->value)
	)
	{
		// it is a function, so we should look in the function overload set
		auto fn_id = fn_call.called.get<ast::expr_identifier_ptr>()->identifier->value;
		auto set_it = std::find_if(
			this->functions.begin(),
			this->functions.end(),
			[&](auto const &set)
			{
				return set.id == fn_id;
			}
		);

		if (set_it == this->functions.end())
		{
			bad_tokens(
				fn_call.get_tokens_begin(),
				fn_call.get_tokens_pivot(),
				fn_call.get_tokens_end(),
				"Error: Undeclared function"
			);
		}

		for (auto &fn : set_it->set)
		{
			if (fn.argument_types.size() != fn_call.params.size())
			{
				continue;
			}

			size_t i;
			for (i = 0; i < fn.argument_types.size(); ++i)
			{
				if (fn.argument_types[i] != get_expression_type(fn_call.params[i]))
				{
					break;
				}
			}
			if (i == fn.argument_types.size())
			{
				return fn.return_type;
			}
		}

		bad_tokens(
			fn_call.get_tokens_begin(),
			fn_call.get_tokens_pivot(),
			fn_call.get_tokens_end(),
			"Error: Undeclared function"
		);
	}
	else
	{
		// it is a variable, so we should look for a function call operator

		auto fn_call_set = std::find_if(
			this->operators.begin(),
			this->operators.end(),
			[](auto const &set)
			{
				return set.op == token::paren_open;
			}
		);

		if (fn_call_set == this->operators.end())
		{
			bad_tokens(
				fn_call.get_tokens_begin(),
				fn_call.get_tokens_pivot(),
				fn_call.get_tokens_end(),
				"Error: Unknown function call"
			);
		}

		bz::vector<ast::typespec> op_types = {};
		op_types.reserve(fn_call.params.size() + 1);
		op_types.emplace_back(get_expression_type(fn_call.called));
		for (auto &p : fn_call.params)
		{
			op_types.emplace_back(get_expression_type(p));
		}

		for (auto fn : fn_call_set->set)
		{
			if (fn.argument_types.size() != op_types.size())
			{
				continue;
			}

			size_t i;
			for (i = 0; i < fn.argument_types.size(); ++i)
			{
				if (fn.argument_types[i] != op_types[i])
				{
					break;
				}
			}

			if (i == fn.argument_types.size())
			{
				return fn.return_type;
			}
		}

		bad_tokens(
			fn_call.get_tokens_begin(),
			fn_call.get_tokens_pivot(),
			fn_call.get_tokens_end(),
			"Error: Unknown function call"
		);
	}
}

ast::typespec parse_context::get_operator_type(ast::expr_unary_op const &unary_op)
{
	auto op_kind = unary_op.op->kind;
	auto set_it = std::find_if(
		this->operators.begin(),
		this->operators.end(),
		[op_kind](auto const &set)
		{
			return set.op == op_kind;
		}
	);

	if (set_it == this->operators.end())
	{
		bad_tokens(
			unary_op.get_tokens_begin(),
			unary_op.get_tokens_pivot(),
			unary_op.get_tokens_end(),
			"Error: Undeclared operator"
		);
	}

	for (auto &op : set_it->set)
	{
		if (
			op.argument_types.size() == 1
			&& op.argument_types[0] == get_expression_type(unary_op.expr)
		)
		{
			return op.return_type;
		}
	}

	bad_tokens(
		unary_op.get_tokens_begin(),
		unary_op.get_tokens_pivot(),
		unary_op.get_tokens_end(),
		"Error: Undeclared operator"
	);
}

ast::typespec parse_context::get_operator_type(ast::expr_binary_op const &binary_op)
{
	auto op_kind = binary_op.op->kind;
	auto set_it = std::find_if(
		this->operators.begin(),
		this->operators.end(),
		[op_kind](auto const &set)
		{
			return set.op == op_kind;
		}
	);

	if (set_it == this->operators.end())
	{
		bad_tokens(
			binary_op.get_tokens_begin(),
			binary_op.get_tokens_pivot(),
			binary_op.get_tokens_end(),
			"Error: Undeclared operator"
		);
	}

	for (auto &op : set_it->set)
	{
		if (
			op.argument_types.size() == 2
			&& op.argument_types[0] == get_expression_type(binary_op.lhs)
			&& op.argument_types[1] == get_expression_type(binary_op.rhs)
		)
		{
			return op.return_type;
		}
	}

	bad_tokens(
		binary_op.get_tokens_begin(),
		binary_op.get_tokens_pivot(),
		binary_op.get_tokens_end(),
		"Error: Undeclared operator"
	);
}

ast::typespec parse_context::get_expression_type(ast::expression const &expr)
{
	switch (expr.kind())
	{
	case ast::expression::index<ast::expr_unresolved>:
		assert(false);
		return ast::typespec();

	case ast::expression::index<ast::expr_identifier>:
		return get_identifier_type(expr.get<ast::expr_identifier_ptr>()->identifier);

	case ast::expression::index<ast::expr_literal>:
		assert(expr.get<ast::expr_literal_ptr>()->expr_type.kind() != ast::typespec::null);
		return expr.get<ast::expr_literal_ptr>()->expr_type;

	case ast::expression::index<ast::expr_unary_op>:
		return get_operator_type(*expr.get<ast::expr_unary_op_ptr>());

	case ast::expression::index<ast::expr_binary_op>:
		return get_operator_type(*expr.get<ast::expr_binary_op_ptr>());

	case ast::expression::index<ast::expr_function_call>:
		return get_function_call_type(*expr.get<ast::expr_function_call_ptr>());

	default:
		assert(false);
		return ast::typespec();
	}
}
