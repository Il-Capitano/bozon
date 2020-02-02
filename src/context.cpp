#include "context.h"
#include "ast/statement.h"

parse_context context;

bool is_built_in_convertible(ast::type_ptr from, ast::type_ptr to);


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

		for (auto &fn : fn_call_set->set)
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

ast::typespec get_built_in_operator_type(ast::expr_unary_op const &unary_op)
{
	auto decayed_type = ast::decay_typespec(unary_op.expr.expr_type);
	assert(ast::is_built_in_type(decayed_type));

	switch (unary_op.op->kind)
	{
	case token::dereference:        // '*'
		if (decayed_type.kind() != ast::typespec::index<ast::ts_pointer>)
		{
			break;
		}
		return ast::make_ts_reference(decayed_type.get<ast::ts_pointer_ptr>()->base);

	case token::bool_not:           // '!'
		if (
			decayed_type.kind() != ast::typespec::index<ast::ts_base_type>
			|| decayed_type.get<ast::ts_base_type_ptr>()->base_type != ast::bool_
		)
		{
			break;
		}
		return ast::make_ts_base_type(ast::bool_);

	case token::plus:               // '+'
	case token::minus:              // '-'
	case token::bit_not:            // '~'
	case token::plus_plus:          // '++'
	case token::minus_minus:        // '--'
	default:
		break;
	}

	return ast::typespec();
}

ast::typespec parse_context::get_operator_type(ast::expr_unary_op const &unary_op)
{
	auto op_kind = unary_op.op->kind;

	// non-overloadable unary operators (&, sizeof, typeof)
	if (op_kind == token::address_of)
	{
		if (!unary_op.expr.is_lvalue)
		{
			bad_tokens(unary_op, "Error: Cannot take address of non-lvalue");
		}
		return ast::make_ts_pointer(unary_op.expr.expr_type);
	}
	else if (op_kind == token::kw_sizeof)
	{
		assert(false);
	}
	else if (op_kind == token::kw_typeof)
	{
		assert(false);
	}

	if (
		auto decayed_type = ast::decay_typespec(unary_op.expr.expr_type);
		ast::is_built_in_type(decayed_type)
	)
	{
		auto op_type = get_built_in_operator_type(unary_op);
		if (op_type.kind() != ast::typespec::null)
		{
			return op_type;
		}
	}

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
			unary_op,
			"Error: Undeclared unary operator '{}' with type '{}'",
			unary_op.op->value, unary_op.expr.expr_type
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
		unary_op,
		"Error: Undeclared unary operator '{}' with type '{}'",
		unary_op.op->value, unary_op.expr.expr_type
	);
}

static int get_arithmetic_rank(ast::typespec const &ts)
{
	assert(ts.kind() == ast::typespec::index<ast::ts_base_type>);
	assert(
		ts.get<ast::ts_base_type_ptr>()->base_type->kind()
		== ast::type::index_of<ast::built_in_type>
	);

	switch (ts.get<ast::ts_base_type_ptr>()->base_type->get<ast::built_in_type>().kind)
	{
	case ast::built_in_type::int8_:
		return 1;
	case ast::built_in_type::uint8_:
		return 2;
	case ast::built_in_type::int16_:
		return 3;
	case ast::built_in_type::uint16_:
		return 4;
	case ast::built_in_type::int32_:
		return 5;
	case ast::built_in_type::uint32_:
		return 6;
	case ast::built_in_type::int64_:
		return 7;
	case ast::built_in_type::uint64_:
		return 8;
	case ast::built_in_type::float32_:
		return 9;
	case ast::built_in_type::float64_:
		return 10;
	default:
		assert(false);
		return -1;
	}
}

ast::typespec get_built_in_op_plus(ast::expr_binary_op const &binary_op)
{
	assert(binary_op.op->kind == token::plus);
	auto lhs_decayed_type = ast::decay_typespec(binary_op.lhs.expr_type);
	auto rhs_decayed_type = ast::decay_typespec(binary_op.rhs.expr_type);
	assert(ast::is_built_in_type(lhs_decayed_type));
	assert(ast::is_built_in_type(rhs_decayed_type));

	auto is_built_in_integer_kind = [](uint32_t kind)
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

	// arithmetic types
	auto is_lhs_arithmetic = ast::is_arithmetic_type(lhs_decayed_type);
	auto is_rhs_arithmetic = ast::is_arithmetic_type(rhs_decayed_type);

	if (is_lhs_arithmetic && is_rhs_arithmetic)
	{
		auto lhs_rank = get_arithmetic_rank(lhs_decayed_type);
		auto rhs_rank = get_arithmetic_rank(rhs_decayed_type);
		if (lhs_rank > rhs_rank)
		{
			return lhs_decayed_type;
		}
		else
		{
			return rhs_decayed_type;
		}
	}

	// pointer arithmetic
	auto is_lhs_ptr = lhs_decayed_type.kind() == ast::typespec::index<ast::ts_pointer>;
	auto is_rhs_ptr = rhs_decayed_type.kind() == ast::typespec::index<ast::ts_pointer>;

	if (is_lhs_ptr && is_rhs_ptr)
	{
		return ast::typespec();
	}
	else if (is_lhs_ptr)
	{
		if (is_integral_type(rhs_decayed_type))
		{
			return lhs_decayed_type;
		}
		else
		{
			return ast::typespec();
		}
	}
	else if (is_rhs_ptr)
	{
		if (is_integral_type(lhs_decayed_type))
		{
			return rhs_decayed_type;
		}
		else
		{
			return ast::typespec();
		}
	}

	assert(!is_lhs_ptr && !is_rhs_ptr);
	assert(lhs_decayed_type.kind() == ast::typespec::index<ast::ts_base_type>);
	assert(rhs_decayed_type.kind() == ast::typespec::index<ast::ts_base_type>);

	// char arithmetic
	auto &lhs_built_in_type =
		lhs_decayed_type.get<ast::ts_base_type_ptr>()->base_type->get<ast::built_in_type>();
	auto &rhs_built_in_type =
		rhs_decayed_type.get<ast::ts_base_type_ptr>()->base_type->get<ast::built_in_type>();

	if (
		// char + int
		(
			lhs_built_in_type.kind == ast::built_in_type::char_
			&& is_built_in_integer_kind(rhs_built_in_type.kind)
		)
		// int + char
		|| (
			is_built_in_integer_kind(lhs_built_in_type.kind)
			&& rhs_built_in_type.kind == ast::built_in_type::char_
		)
	)
	{
		return ast::make_ts_base_type(ast::char_);
	}

	return ast::typespec();
}

ast::typespec get_built_in_op_minus(ast::expr_binary_op const &binary_op)
{
	assert(binary_op.op->kind == token::minus);
	auto lhs_decayed_type = ast::decay_typespec(binary_op.lhs.expr_type);
	auto rhs_decayed_type = ast::decay_typespec(binary_op.rhs.expr_type);
	assert(ast::is_built_in_type(lhs_decayed_type));
	assert(ast::is_built_in_type(rhs_decayed_type));

	auto is_built_in_integer_kind = [](uint32_t kind)
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

	// arithmetic types
	auto is_lhs_arithmetic = ast::is_arithmetic_type(lhs_decayed_type);
	auto is_rhs_arithmetic = ast::is_arithmetic_type(rhs_decayed_type);

	if (is_lhs_arithmetic && is_rhs_arithmetic)
	{
		auto lhs_rank = get_arithmetic_rank(lhs_decayed_type);
		auto rhs_rank = get_arithmetic_rank(rhs_decayed_type);
		if (lhs_rank > rhs_rank)
		{
			return lhs_decayed_type;
		}
		else
		{
			return rhs_decayed_type;
		}
	}

	// pointer arithmetic
	auto is_lhs_ptr = lhs_decayed_type.kind() == ast::typespec::index<ast::ts_pointer>;
	auto is_rhs_ptr = rhs_decayed_type.kind() == ast::typespec::index<ast::ts_pointer>;

	if (is_lhs_ptr && is_rhs_ptr)
	{
		auto &lhs_ptr = lhs_decayed_type.get<ast::ts_pointer_ptr>()->base;
		auto &rhs_ptr = rhs_decayed_type.get<ast::ts_pointer_ptr>()->base;
		if (ast::decay_typespec(lhs_ptr) == ast::decay_typespec(rhs_ptr))
		{
			return ast::make_ts_base_type(ast::int64_);
		}
		else
		{
			return ast::typespec();
		}
	}
	else if (is_lhs_ptr)
	{
		if (is_integral_type(rhs_decayed_type))
		{
			return lhs_decayed_type;
		}
		else
		{
			return ast::typespec();
		}
	}
	else if (is_rhs_ptr)
	{
		return ast::typespec();
	}

	assert(!is_lhs_ptr && !is_rhs_ptr);
	assert(lhs_decayed_type.kind() == ast::typespec::index<ast::ts_base_type>);
	assert(rhs_decayed_type.kind() == ast::typespec::index<ast::ts_base_type>);

	// char arithmetic
	auto &lhs_built_in_type =
		lhs_decayed_type.get<ast::ts_base_type_ptr>()->base_type->get<ast::built_in_type>();
	auto &rhs_built_in_type =
		rhs_decayed_type.get<ast::ts_base_type_ptr>()->base_type->get<ast::built_in_type>();

	if (
		lhs_built_in_type.kind == ast::built_in_type::char_
		&& is_built_in_integer_kind(rhs_built_in_type.kind)
	)
	{
		return ast::make_ts_base_type(ast::char_);
	}

	return ast::typespec();
}

ast::typespec get_built_in_op_mul_div(ast::expr_binary_op const &binary_op)
{
	assert(
		binary_op.op->kind == token::multiply
		|| binary_op.op->kind == token::divide
	);
	auto lhs_decayed_type = ast::decay_typespec(binary_op.lhs.expr_type);
	auto rhs_decayed_type = ast::decay_typespec(binary_op.rhs.expr_type);
	assert(ast::is_built_in_type(lhs_decayed_type));
	assert(ast::is_built_in_type(rhs_decayed_type));

	// arithmetic types
	auto is_lhs_arithmetic = ast::is_arithmetic_type(lhs_decayed_type);
	auto is_rhs_arithmetic = ast::is_arithmetic_type(rhs_decayed_type);

	if (is_lhs_arithmetic && is_rhs_arithmetic)
	{
		auto lhs_rank = get_arithmetic_rank(lhs_decayed_type);
		auto rhs_rank = get_arithmetic_rank(rhs_decayed_type);

		if (lhs_rank > rhs_rank)
		{
			return lhs_decayed_type;
		}
		else
		{
			return rhs_decayed_type;
		}
	}
	else
	{
		return ast::typespec();
	}
}

ast::typespec get_built_in_modulo(ast::expr_binary_op const &binary_op)
{
	assert(binary_op.op->kind == token::modulo);
	auto lhs_decayed_type = ast::decay_typespec(binary_op.lhs.expr_type);
	auto rhs_decayed_type = ast::decay_typespec(binary_op.rhs.expr_type);
	assert(ast::is_built_in_type(lhs_decayed_type));
	assert(ast::is_built_in_type(rhs_decayed_type));

	auto is_lhs_integral = ast::is_integral_type(lhs_decayed_type);
	auto is_rhs_integral = ast::is_integral_type(rhs_decayed_type);

	if (is_lhs_integral && is_rhs_integral)
	{
		return lhs_decayed_type;
	}
	else
	{
		return ast::typespec();
	}
}

ast::typespec get_built_in_equality(ast::expr_binary_op const &binary_op)
{
	assert(
		binary_op.op->kind == token::equals
		|| binary_op.op->kind == token::not_equals
	);
	auto lhs_decayed_type = ast::decay_typespec(binary_op.lhs.expr_type);
	auto rhs_decayed_type = ast::decay_typespec(binary_op.rhs.expr_type);
	assert(ast::is_built_in_type(lhs_decayed_type));
	assert(ast::is_built_in_type(rhs_decayed_type));

	// arithmetic types
	auto is_lhs_arithmetic = ast::is_arithmetic_type(lhs_decayed_type);
	auto is_rhs_arithmetic = ast::is_arithmetic_type(rhs_decayed_type);

	if (is_lhs_arithmetic && is_rhs_arithmetic)
	{
		return ast::make_ts_base_type(ast::bool_);
	}

	// pointers
	auto is_lhs_ptr = lhs_decayed_type.kind() == ast::typespec::index<ast::ts_pointer>;
	auto is_rhs_ptr = rhs_decayed_type.kind() == ast::typespec::index<ast::ts_pointer>;

	if (is_lhs_ptr && is_rhs_ptr)
	{
		auto lhs_base = lhs_decayed_type;
		auto rhs_base = rhs_decayed_type;

		while (
			lhs_base.kind() == ast::typespec::index<ast::ts_pointer>
			&& rhs_base.kind() == ast::typespec::index<ast::ts_pointer>
		)
		{
			lhs_base = ast::decay_typespec(lhs_base.get<ast::ts_pointer_ptr>()->base);
			rhs_base = ast::decay_typespec(rhs_base.get<ast::ts_pointer_ptr>()->base);
		}

		if (lhs_base == rhs_base)
		{
			return ast::make_ts_base_type(ast::bool_);
		}
		else
		{
			return ast::typespec();
		}
	}
	else if (is_lhs_ptr)
	{
		if (
			rhs_decayed_type.get<ast::ts_base_type_ptr>()
				->base_type->get<ast::built_in_type>().kind == ast::built_in_type::null_t_
		)
		{
			return ast::make_ts_base_type(ast::bool_);
		}
		else
		{
			return ast::typespec();
		}
	}
	else if (is_rhs_ptr)
	{
		if (
			lhs_decayed_type.get<ast::ts_base_type_ptr>()
				->base_type->get<ast::built_in_type>().kind == ast::built_in_type::null_t_
		)
		{
			return ast::make_ts_base_type(ast::bool_);
		}
		else
		{
			return ast::typespec();
		}
	}

	if (lhs_decayed_type == rhs_decayed_type)
	{
		return ast::make_ts_base_type(ast::bool_);
	}
	else
	{
		return ast::typespec();
	}
}

ast::typespec get_built_in_bit_ops(ast::expr_binary_op const &binary_op)
{
	assert(
		binary_op.op->kind == token::bit_and
		|| binary_op.op->kind == token::bit_or
		|| binary_op.op->kind == token::bit_xor
	);
	auto lhs_decayed_type = ast::decay_typespec(binary_op.lhs.expr_type);
	auto rhs_decayed_type = ast::decay_typespec(binary_op.rhs.expr_type);
	assert(ast::is_built_in_type(lhs_decayed_type));
	assert(ast::is_built_in_type(rhs_decayed_type));

	auto is_lhs_integral = ast::is_integral_type(lhs_decayed_type);
	auto is_rhs_integral = ast::is_integral_type(rhs_decayed_type);

	if (is_lhs_integral && is_rhs_integral)
	{
		auto lhs_rank = get_arithmetic_rank(lhs_decayed_type);
		auto rhs_rank = get_arithmetic_rank(rhs_decayed_type);

		if (lhs_rank > rhs_rank)
		{
			return lhs_decayed_type;
		}
		else
		{
			return rhs_decayed_type;
		}
	}
	else
	{
		return ast::typespec();
	}
}

ast::typespec get_built_in_bool_ops(ast::expr_binary_op const &binary_op)
{
	assert(
		binary_op.op->kind == token::bool_and
		|| binary_op.op->kind == token::bool_or
		|| binary_op.op->kind == token::bool_xor
	);
	auto lhs_decayed_type = ast::decay_typespec(binary_op.lhs.expr_type);
	auto rhs_decayed_type = ast::decay_typespec(binary_op.rhs.expr_type);
	assert(ast::is_built_in_type(lhs_decayed_type));
	assert(ast::is_built_in_type(rhs_decayed_type));

	auto is_lhs_bool = lhs_decayed_type.kind() == ast::typespec::index<ast::ts_base_type>
		&& lhs_decayed_type.get<ast::ts_base_type_ptr>()->base_type == ast::bool_;
	auto is_rhs_bool = rhs_decayed_type.kind() == ast::typespec::index<ast::ts_base_type>
		&& rhs_decayed_type.get<ast::ts_base_type_ptr>()->base_type == ast::bool_;

	if (is_lhs_bool && is_rhs_bool)
	{
		return ast::make_ts_base_type(ast::bool_);
	}
	else
	{
		return ast::typespec();
	}
}

ast::typespec get_built_in_bit_shift(ast::expr_binary_op const &binary_op)
{
	assert(
		binary_op.op->kind == token::bit_left_shift
		|| binary_op.op->kind == token::bit_right_shift
	);
	auto lhs_decayed_type = ast::decay_typespec(binary_op.lhs.expr_type);
	auto rhs_decayed_type = ast::decay_typespec(binary_op.rhs.expr_type);
	assert(ast::is_built_in_type(lhs_decayed_type));
	assert(ast::is_built_in_type(rhs_decayed_type));

	auto is_lhs_integral = ast::is_integral_type(lhs_decayed_type);
	auto is_rhs_integral = ast::is_integral_type(rhs_decayed_type);

	if (is_lhs_integral && is_rhs_integral)
	{
		return lhs_decayed_type;
	}
	else
	{
		return ast::typespec();
	}
}

ast::typespec get_built_in_assign(ast::expr_binary_op const &binary_op)
{
	assert(binary_op.op->kind == token::assign);
	auto lhs_decayed_type = ast::decay_typespec(binary_op.lhs.expr_type);
	auto rhs_decayed_type = ast::decay_typespec(binary_op.rhs.expr_type);
	assert(ast::is_built_in_type(lhs_decayed_type));
	assert(ast::is_built_in_type(rhs_decayed_type));

	if (
		!binary_op.lhs.is_lvalue
		|| binary_op.lhs.expr_type.kind() == ast::typespec::index<ast::ts_constant>
	)
	{
		return ast::typespec();
	}

	if (context.is_convertible(binary_op.rhs, lhs_decayed_type))
	{
		return ast::make_ts_reference(lhs_decayed_type);
	}
	else
	{
		return ast::typespec();
	}
}

ast::typespec get_built_in_operator_type(ast::expr_binary_op const &binary_op)
{
	switch (binary_op.op->kind)
	{
	case token::plus:               // '+'
		return get_built_in_op_plus(binary_op);
	case token::minus:              // '-'
		return get_built_in_op_minus(binary_op);

	case token::multiply:           // '*'
	case token::divide:             // '/'
		return get_built_in_op_mul_div(binary_op);

	case token::modulo:             // '%'
		return get_built_in_modulo(binary_op);

	case token::equals:             // '=='
	case token::not_equals:         // '!='
		return get_built_in_equality(binary_op);

	case token::less_than:          // '<'
	case token::less_than_eq:       // '<='
	case token::greater_than:       // '>'
	case token::greater_than_eq:    // '>='
		break;

	case token::bit_and:            // '&'
	case token::bit_xor:            // '^'
	case token::bit_or:             // '|'
		return get_built_in_bit_ops(binary_op);

	case token::bool_and:           // '&&'
	case token::bool_xor:           // '^^'
	case token::bool_or:            // '||'
		return get_built_in_bool_ops(binary_op);

	case token::bit_left_shift:     // '<<'
	case token::bit_right_shift:    // '>>'
		return get_built_in_bit_shift(binary_op);

	case token::assign:             // '='
		return get_built_in_assign(binary_op);

	case token::plus_eq:            // '+='
	case token::minus_eq:           // '-='
	case token::multiply_eq:        // '*='
	case token::divide_eq:          // '/='
	case token::modulo_eq:          // '%='
	case token::bit_and_eq:         // '&='
	case token::bit_xor_eq:         // '^='
	case token::bit_or_eq:          // '|='
	case token::bit_left_shift_eq:  // '<<='
	case token::bit_right_shift_eq: // '>>='
		break;

	case token::dot_dot:            // '..'
	case token::dot_dot_eq:         // '..='
	case token::square_open:        // '[]'
	default:
		break;
	}
	return ast::typespec();
}

ast::typespec parse_context::get_operator_type(ast::expr_binary_op const &binary_op)
{
	if (binary_op.op->kind == token::comma)
	{
		if (binary_op.rhs.is_lvalue)
		{
			return ast::make_ts_reference(binary_op.rhs.expr_type);
		}
		else
		{
			return binary_op.rhs.expr_type;
		}
	}

	auto lhs_decayed_type = ast::decay_typespec(binary_op.lhs.expr_type);
	auto rhs_decayed_type = ast::decay_typespec(binary_op.rhs.expr_type);
	if (ast::is_built_in_type(lhs_decayed_type) && ast::is_built_in_type(rhs_decayed_type))
	{
		auto op_type = get_built_in_operator_type(binary_op);
		if (op_type.kind() != ast::typespec::null)
		{
			return op_type;
		}
	}

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
			binary_op,
			"Error: Undeclared operator with types '{}' and '{}'",
			binary_op.lhs.expr_type, binary_op.rhs.expr_type
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
		binary_op,
		"Error: Undeclared operator with types '{}' and '{}'",
		binary_op.lhs.expr_type, binary_op.rhs.expr_type
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

static size_t get_align(ast::typespec const &ts)
{
	switch (ts.kind())
	{
	case ast::typespec::index<ast::ts_base_type>:
		return ast::align_of(ts.get<ast::ts_base_type_ptr>()->base_type);
	case ast::typespec::index<ast::ts_constant>:
		return get_align(ts.get<ast::ts_constant_ptr>()->base);
	case ast::typespec::index<ast::ts_pointer>:
		return 8;
	case ast::typespec::index<ast::ts_reference>:
		return 8;
	case ast::typespec::index<ast::ts_function>:
		return 8;
	case ast::typespec::index<ast::ts_tuple>:
	{
		size_t align = 0;
		auto &tuple = ts.get<ast::ts_tuple_ptr>();

		for (auto &member : tuple->types)
		{
			if (auto member_align = get_align(member); member_align > align)
			{
				align = member_align;
			}
		}

		return align;
	}
	default:
		assert(false);
		break;
	}
}

static size_t get_size(ast::typespec const &ts)
{
	switch (ts.kind())
	{
	case ast::typespec::index<ast::ts_base_type>:
		return ast::size_of(ts.get<ast::ts_base_type_ptr>()->base_type);
	case ast::typespec::index<ast::ts_constant>:
		return get_size(ts.get<ast::ts_constant_ptr>()->base);
	case ast::typespec::index<ast::ts_pointer>:
		return 8;
	case ast::typespec::index<ast::ts_reference>:
		return 8;
	case ast::typespec::index<ast::ts_function>:
		return 8;
	case ast::typespec::index<ast::ts_tuple>:
	{
		size_t size  = 0;
		size_t align = 0;
		auto &tuple = ts.get<ast::ts_tuple_ptr>();

		if (tuple->types.size() == 0)
		{
			return 0;
		}

		for (auto &memeber : tuple->types)
		{
			auto ts_size  = get_size(memeber);
			auto ts_align = get_align(memeber);

			if (ts_align == 0)
			{
				continue;
			}

			if (auto padding = size % ts_align; padding != 0)
			{
				size += ts_align - padding;
			}
			size += ts_size;

			if (ts_align > align)
			{
				align = ts_align;
			}
		}

		if (align == 0)
		{
			return size;
		}

		if (auto padding = size % align; padding != 0)
		{
			size += align - padding;
		}

		return size;
	}
	default:
		assert(false);
		break;
	}
}

int64_t parse_context::get_identifier_stack_offset(src_file::token_pos id) const
{
	int64_t offset = 0;
	ast::variable const *var = nullptr;
	bool loop = true;

	for (
		auto scope = this->variables.rbegin();
		loop && scope != this->variables.rend();
		++scope
	)
	{
		for (auto v = scope->rbegin(); loop && v != scope->rend(); ++v)
		{
			if (v->id == id)
			{
				loop = false;
				var = &*v;
			}
		}
	}
	assert(var);

	for (auto scope = this->variables.begin(); scope != this->variables.end(); ++scope)
	{
		for (auto v = scope->begin(); v != scope->end(); ++v)
		{
			auto size = get_size(v->var_type);
			auto align = get_align(v->var_type);
			if (auto padding = offset % align; padding != 0)
			{
				offset += align - padding;
			}
			offset += size;

			if (var == &*v)
			{
				return -offset;
			}
		}
	}

	assert(false);
	return 0;
}

int64_t parse_context::get_identifier_stack_allocation_amount(src_file::token_pos id) const
{
	int64_t offset = 0;
	int64_t prev_offset = 0;
	ast::variable const *var = nullptr;
	bool loop = true;

	for (
		auto scope = this->variables.rbegin();
		loop && scope != this->variables.rend();
		++scope
	)
	{
		for (auto v = scope->rbegin(); loop && v != scope->rend(); ++v)
		{
			if (v->id == id)
			{
				loop = false;
				var = &*v;
			}
		}
	}
	assert(var);

	for (auto scope = this->variables.begin(); scope != this->variables.end(); ++scope)
	{
		for (auto v = scope->begin(); v != scope->end(); ++v)
		{
			prev_offset = offset;
			auto size = get_size(v->var_type);
			auto align = get_align(v->var_type);
			if (auto padding = offset % align; padding != 0)
			{
				offset += align - padding;
			}
			offset += size;

			if (var == &*v)
			{
				return offset - prev_offset;
			}
		}
	}

	assert(false);
	return 0;
}
