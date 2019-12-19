#include "context.h"
#include "ast/statement.h"

parse_context context;

bool is_convertible(ast::expression const &expr, ast::typespec const &type);
bool is_built_in_convertible(ast::type_ptr from, ast::type_ptr to);

bz::vector<operator_overload_set> get_default_operators(void)
{
	const ast::type_ptr types[] = {
		[ast::built_in_type::int8_]    = ast::int8_,
		[ast::built_in_type::int16_]   = ast::int16_,
		[ast::built_in_type::int32_]   = ast::int32_,
		[ast::built_in_type::int64_]   = ast::int64_,
		[ast::built_in_type::uint8_]   = ast::uint8_,
		[ast::built_in_type::uint16_]  = ast::uint16_,
		[ast::built_in_type::uint32_]  = ast::uint32_,
		[ast::built_in_type::uint64_]  = ast::uint64_,
		[ast::built_in_type::float32_] = ast::float32_,
		[ast::built_in_type::float64_] = ast::float64_,
		[ast::built_in_type::char_]    = ast::char_,
		[ast::built_in_type::bool_]    = ast::bool_,
		[ast::built_in_type::str_]     = ast::str_,
		[ast::built_in_type::void_]    = ast::void_,
		[ast::built_in_type::null_t_]  = ast::null_t_,
	};

	const uint32_t int_types[] = {
		ast::built_in_type::int8_,
		ast::built_in_type::int16_,
		ast::built_in_type::int32_,
		ast::built_in_type::int64_,
		ast::built_in_type::uint8_,
		ast::built_in_type::uint16_,
		ast::built_in_type::uint32_,
		ast::built_in_type::uint64_,
	};

	const uint32_t floating_point_types[] = {
		ast::built_in_type::float32_,
		ast::built_in_type::float64_,
	};

	bz::vector<operator_overload_set> sets;

	// ==== operator + ====
	operator_overload_set op_plus;
	op_plus.op = token::plus;

	//unary
	for (auto t : int_types)
	{
		op_plus.set.push_back(ast::ts_function(
			ast::make_ts_base_type(types[t]),
			{ ast::make_ts_base_type(types[t]) }
		));
	}
	for (auto t : floating_point_types)
	{
		op_plus.set.push_back(ast::ts_function(
			ast::make_ts_base_type(types[t]),
			{ ast::make_ts_base_type(types[t]) }
		));
	}

	// binary
	for (auto t : int_types)
	{
		op_plus.set.push_back(ast::ts_function(
			ast::make_ts_base_type(types[t]),
			{ ast::make_ts_base_type(types[t]), ast::make_ts_base_type(types[t]) }
		));
	}
	for (auto t : floating_point_types)
	{
		op_plus.set.push_back(ast::ts_function(
			ast::make_ts_base_type(types[t]),
			{ ast::make_ts_base_type(types[t]), ast::make_ts_base_type(types[t]) }
		));
	}


	// ==== operator - ====
	operator_overload_set op_minus;
	op_minus.op = token::minus;

	//unary
	// unsigned types should return signed
	for (auto t : int_types)
	{
		op_minus.set.push_back(ast::ts_function(
			ast::make_ts_base_type(types[t]),
			{ ast::make_ts_base_type(types[t]) }
		));
	}
	for (auto t : floating_point_types)
	{
		op_minus.set.push_back(ast::ts_function(
			ast::make_ts_base_type(types[t]),
			{ ast::make_ts_base_type(types[t]) }
		));
	}

	// binary
	for (auto t : int_types)
	{
		op_minus.set.push_back(ast::ts_function(
			ast::make_ts_base_type(types[t]),
			{ ast::make_ts_base_type(types[t]), ast::make_ts_base_type(types[t]) }
		));
	}
	for (auto t : floating_point_types)
	{
		op_minus.set.push_back(ast::ts_function(
			ast::make_ts_base_type(types[t]),
			{ ast::make_ts_base_type(types[t]), ast::make_ts_base_type(types[t]) }
		));
	}


	// ==== operator * ====
	operator_overload_set op_multiply;
	op_multiply.op = token::multiply;

	// binary
	for (auto t : int_types)
	{
		op_multiply.set.push_back(ast::ts_function(
			ast::make_ts_base_type(types[t]),
			{ ast::make_ts_base_type(types[t]), ast::make_ts_base_type(types[t]) }
		));
	}
	for (auto t : floating_point_types)
	{
		op_multiply.set.push_back(ast::ts_function(
			ast::make_ts_base_type(types[t]),
			{ ast::make_ts_base_type(types[t]), ast::make_ts_base_type(types[t]) }
		));
	}


	// ==== operator / ====
	operator_overload_set op_divide;
	op_divide.op = token::divide;

	// binary
	for (auto t : int_types)
	{
		op_divide.set.push_back(ast::ts_function(
			ast::make_ts_base_type(types[t]),
			{ ast::make_ts_base_type(types[t]), ast::make_ts_base_type(types[t]) }
		));
	}
	for (auto t : floating_point_types)
	{
		op_divide.set.push_back(ast::ts_function(
			ast::make_ts_base_type(types[t]),
			{ ast::make_ts_base_type(types[t]), ast::make_ts_base_type(types[t]) }
		));
	}


	// ==== operator ||, && and ^^ ====
	operator_overload_set op_bool_or;
	op_bool_or.op = token::bool_or;
	op_bool_or.set.push_back(ast::ts_function(
		ast::make_ts_base_type(ast::bool_),
		{ ast::make_ts_base_type(ast::bool_), ast::make_ts_base_type(ast::bool_) }
	));

	operator_overload_set op_bool_and;
	op_bool_and.op = token::bool_and;
	op_bool_and.set.push_back(ast::ts_function(
		ast::make_ts_base_type(ast::bool_),
		{ ast::make_ts_base_type(ast::bool_), ast::make_ts_base_type(ast::bool_) }
	));

	operator_overload_set op_bool_xor;
	op_bool_xor.op = token::bool_xor;
	op_bool_xor.set.push_back(ast::ts_function(
		ast::make_ts_base_type(ast::bool_),
		{ ast::make_ts_base_type(ast::bool_), ast::make_ts_base_type(ast::bool_) }
	));



	sets.push_back(std::move(op_plus));
	sets.push_back(std::move(op_minus));
	sets.push_back(std::move(op_multiply));
	sets.push_back(std::move(op_divide));

	sets.push_back(std::move(op_bool_or));
	sets.push_back(std::move(op_bool_and));
	sets.push_back(std::move(op_bool_xor));

	return sets;
}

parse_context::parse_context(void)
	: variables{{}},
	  functions{},
	  operators(get_default_operators()),
	  types{
		  ast::int8_, ast::int16_, ast::int32_, ast::int64_,
		  ast::uint8_, ast::uint16_, ast::uint32_, ast::uint64_,
		  ast::float32_, ast::float64_,
		  ast::char_, ast::bool_, ast::str_, ast::void_, ast::null_t_
	  }
{}

bool parse_context::add_variable(
	src_file::token_pos id,
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

void parse_context::add_function(ast::decl_function &func_decl)
{
	bz::vector<ast::typespec> param_types;

	for (auto &param : func_decl.params)
	{
		param_types.push_back(param.var_type);
	}

	ast::ts_function func_type{
		func_decl.return_type,
		std::move(param_types)
	};
	auto id = func_decl.identifier;

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
			bad_token(id, "Error: Redefinition of function");
		}

		it->set.emplace_back(func_type);
	}
}

void parse_context::add_operator(ast::decl_operator &op_decl)
{
	bz::vector<ast::typespec> param_types;

	for (auto &param : op_decl.params)
	{
		param_types.push_back(param.var_type);
	}

	ast::ts_function op_type{
		op_decl.return_type,
		std::move(param_types)
	};
	auto op = op_decl.op;

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
			bad_token(op, "Error: Redefinition of operator");
		}

		it->set.emplace_back(op_type);
	}
}

void parse_context::add_type(ast::decl_struct &struct_decl)
{
	auto it = std::find_if(
		this->types.begin(),
		this->types.end(),
		[&](auto const &t)
		{
			return t->name == struct_decl.identifier->value;
		}
	);

	if (it != this->types.end())
	{
		bad_token(struct_decl.identifier, "Error: Redefinition of type");
	}

	this->types.push_back(
		ast::make_aggregate_type_ptr(
			struct_decl.identifier->value,
			struct_decl.member_variables
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


ast::type_ptr parse_context::get_type(src_file::token_pos id)
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

ast::typespec parse_context::get_identifier_type(src_file::token_pos t)
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
	src_file::token_pos op,
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
				// if (fn.argument_types[i] != fn_call.params[i].expr_type)
				if (!is_convertible(fn_call.params[i], fn.argument_types[i]))
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

		bz::vector<ast::expression const *> op_exprs = {};
		op_exprs.reserve(fn_call.params.size() + 1);
		op_exprs.push_back(&fn_call.called);
		for (auto &p : fn_call.params)
		{
			op_exprs.push_back(&p);
		}

		for (auto fn : fn_call_set->set)
		{
			if (fn.argument_types.size() != op_exprs.size())
			{
				continue;
			}

			size_t i;
			for (i = 0; i < fn.argument_types.size(); ++i)
			{
				if (!is_convertible(*op_exprs[i], fn.argument_types[i]))
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
			&& is_convertible(unary_op.expr, op.argument_types[0])
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
			&& is_convertible(binary_op.lhs, op.argument_types[0])
			&& is_convertible(binary_op.rhs, op.argument_types[1])
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

bool parse_context::is_convertible(ast::expression const &expr, ast::typespec const &type)
{
	auto expr_decay_type = decay_typespec(expr.expr_type);
	auto decay_type = decay_typespec(type);

	if (type.kind() == ast::typespec::index<ast::ts_reference>)
	{
		auto &ref_type = type.get<ast::ts_reference_ptr>()->base;
		if (ref_type.kind() == ast::typespec::index<ast::ts_constant>)
		{
			auto &const_type = ref_type.get<ast::ts_constant_ptr>()->base;
			return expr_decay_type == const_type;
		}
		else
		{
			return expr.is_lvalue && expr.expr_type == ref_type;
		}
	}
	else if (
		is_built_in_type(expr_decay_type)
		&& is_built_in_type(decay_type)
	)
	{
		return is_built_in_convertible(
			expr_decay_type.get<ast::ts_base_type_ptr>()->base_type,
			decay_type.get<ast::ts_base_type_ptr>()->base_type
		);
	}
	else
	{
		return expr_decay_type == decay_type;
	}
}

bool is_built_in_convertible(ast::type_ptr from, ast::type_ptr to)
{
	auto is_int_kind = [](auto kind)
	{
		switch (kind)
		{
		case ast::built_in_type::int8_:
		case ast::built_in_type::int16_:
		case ast::built_in_type::int32_:
		case ast::built_in_type::int64_:
		case ast::built_in_type::uint8_:
		case ast::built_in_type::uint16_:
		case ast::built_in_type::uint32_:
		case ast::built_in_type::uint64_:
			return true;
		default:
			return false;
		}
	};

	auto is_floating_point_kind = [](auto kind)
	{
		switch (kind)
		{
		case ast::built_in_type::float32_:
		case ast::built_in_type::float64_:
			return true;
		default:
			return false;
		}
	};

	auto is_bigger_int = [](auto from, auto to)
	{
		switch (from)
		{
		case ast::built_in_type::int8_:
		case ast::built_in_type::uint8_:
			switch (to)
			{
			case ast::built_in_type::int16_:
			case ast::built_in_type::int32_:
			case ast::built_in_type::int64_:
			case ast::built_in_type::uint16_:
			case ast::built_in_type::uint32_:
			case ast::built_in_type::uint64_:
				return true;
			default:
				return false;
			}
		case ast::built_in_type::int16_:
		case ast::built_in_type::uint16_:
			switch (to)
			{
			case ast::built_in_type::int32_:
			case ast::built_in_type::int64_:
			case ast::built_in_type::uint32_:
			case ast::built_in_type::uint64_:
				return true;
			default:
				return false;
			}
		case ast::built_in_type::int32_:
		case ast::built_in_type::uint32_:
			switch (to)
			{
			case ast::built_in_type::int64_:
			case ast::built_in_type::uint64_:
				return true;
			default:
				return false;
			}
		case ast::built_in_type::int64_:
		case ast::built_in_type::uint64_:
			return false;
		default:
			assert(false);
			return false;
		}
	};

	if (
		from->kind() == ast::type::index_of<ast::built_in_type>
		&& to->kind() == ast::type::index_of<ast::built_in_type>
	)
	{
		auto &from_built_in = from->get<ast::built_in_type>();
		auto &to_built_in = to->get<ast::built_in_type>();

		if (from_built_in.kind == to_built_in.kind)
		{
			return true;
		}

		if (is_floating_point_kind(from_built_in.kind))
		{
			static_assert(ast::built_in_type::float64_ > ast::built_in_type::float32_);
			return is_floating_point_kind(to_built_in.kind)
				&& to_built_in.kind >= from_built_in.kind;
		}
		else if (is_int_kind(from_built_in.kind))
		{
			if (!is_floating_point_kind(to_built_in.kind) && !is_int_kind(to_built_in.kind))
			{
				return false;
			}

			return is_floating_point_kind(to_built_in.kind)
				|| is_bigger_int(from_built_in.kind, to_built_in.kind);
		}
		else
		{
			return false;
		}
	}

	return false;
}
