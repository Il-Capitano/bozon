#include "parser.h"

// ================================================================
// ---------------------- expression parsing ----------------------
// ================================================================

struct precedence
{
	int value;
	bool is_left_associative;

	constexpr precedence(void)
		: value(-1), is_left_associative(true)
	{}

	constexpr precedence(int val, bool assoc)
		: value(val), is_left_associative(assoc)
	{}
};

static bool operator < (precedence lhs, precedence rhs)
{
	if (lhs.value == -1)
	{
		return false;
	}
	else if (rhs.value == -1)
	{
		return true;
	}
	else
	{
		return rhs.is_left_associative ? lhs.value < rhs.value : lhs.value <= rhs.value;
	}
}

static bool operator <= (precedence lhs, precedence rhs)
{
	if (lhs.value == -1)
	{
		return false;
	}
	else if (rhs.value == -1)
	{
		return true;
	}
	else
	{
		return lhs.value <= rhs.value;
	}
}

static std::map<uint32_t, precedence> binary_op_precendences =
{
	{ token::scope,              precedence{  1, true  } },

	{ token::paren_open,         precedence{  2, true  } },
	{ token::square_open,        precedence{  2, true  } },
	{ token::dot,                precedence{  2, true  } },
	{ token::arrow,              precedence{  2, true  } },

	{ token::dot_dot,            precedence{  4, true  } },

	{ token::multiply,           precedence{  5, true  } },
	{ token::divide,             precedence{  5, true  } },
	{ token::modulo,             precedence{  5, true  } },

	{ token::plus,               precedence{  6, true  } },
	{ token::minus,              precedence{  6, true  } },

	{ token::bit_left_shift,     precedence{  7, true  } },
	{ token::bit_right_shift,    precedence{  7, true  } },

	{ token::bit_and,            precedence{  8, true  } },
	{ token::bit_xor,            precedence{  9, true  } },
	{ token::bit_or,             precedence{ 10, true  } },

	{ token::less_than,          precedence{ 11, true  } },
	{ token::less_than_eq,       precedence{ 11, true  } },
	{ token::greater_than,       precedence{ 11, true  } },
	{ token::greater_than_eq,    precedence{ 11, true  } },

	{ token::equals,             precedence{ 12, true  } },
	{ token::not_equals,         precedence{ 12, true  } },

	{ token::bool_and,           precedence{ 13, true  } },
	{ token::bool_xor,           precedence{ 14, true  } },
	{ token::bool_or,            precedence{ 15, true  } },

	// ternary ?
	{ token::assign,             precedence{ 16, false } },
	{ token::plus_eq,            precedence{ 16, false } },
	{ token::minus_eq,           precedence{ 16, false } },
	{ token::multiply_eq,        precedence{ 16, false } },
	{ token::divide_eq,          precedence{ 16, false } },
	{ token::modulo_eq,          precedence{ 16, false } },
	{ token::dot_dot_eq,         precedence{ 16, false } },
	{ token::bit_left_shift_eq,  precedence{ 16, false } },
	{ token::bit_right_shift_eq, precedence{ 16, false } },
	{ token::bit_and_eq,         precedence{ 16, false } },
	{ token::bit_xor_eq,         precedence{ 16, false } },
	{ token::bit_or_eq,          precedence{ 16, false } },

	{ token::comma,              precedence{ 18, true  } },
};

constexpr precedence no_comma{ 17, true };

static std::map<uint32_t, precedence> unary_op_precendences =
{
	{ token::plus,               precedence{  3, false } },
	{ token::minus,              precedence{  3, false } },
	{ token::plus_plus,          precedence{  3, false } },
	{ token::minus_minus,        precedence{  3, false } },
	{ token::bit_not,            precedence{  3, false } },
	{ token::bool_not,           precedence{  3, false } },
	{ token::address_of,         precedence{  3, false } },
	{ token::dereference,        precedence{  3, false } },
	{ token::kw_sizeof,          precedence{  3, false } },
	{ token::kw_typeof,          precedence{  3, false } },
	// new, delete
};


static precedence get_binary_precedence(uint32_t kind)
{
	auto it = binary_op_precendences.find(kind);
	if (it == binary_op_precendences.end())
	{
		return precedence{};
	}
	else
	{
		return it->second;
	}
}

static precedence get_unary_precedence(uint32_t kind)
{
	auto it = unary_op_precendences.find(kind);
	if (it == unary_op_precendences.end())
	{
		return precedence{};
	}
	else
	{
		return it->second;
	}
}

static ast::expression parse_expression(
	token_pos &stream, token_pos end,
	parse_context &context,
	bz::vector<error> &errors,
	precedence prec
);

static bz::vector<ast::expression> parse_expression_comma_list(
	token_pos &stream, token_pos end,
	parse_context &context,
	bz::vector<error> &errors
);

static ast::expression parse_primary_expression(
	token_pos &stream, token_pos end,
	parse_context &context,
	bz::vector<error> &errors
)
{
	switch (stream->kind)
	{
	case token::identifier:
	{
		auto id = ast::make_expr_identifier(stream);
		++stream;
		return id;
	}

	// literals
	case token::number_literal:
	case token::string_literal:
	case token::character_literal:
	case token::kw_true:
	case token::kw_false:
	case token::kw_null:
	{
		auto literal = ast::make_expr_literal(stream);
		++stream;
		return literal;
	}

	case token::paren_open:
	{
		++stream;
		auto paren_begin = stream;

		size_t paren_level;
		for (paren_level = 1; stream != end && paren_level != 0; ++stream)
		{
			if (stream->kind == token::paren_open)
			{
				++paren_level;
			}
			else if (stream->kind == token::paren_close)
			{
				--paren_level;
			}
		}

		if (paren_level != 0)
		{
			errors.emplace_back(bad_token(stream, "expected ')'"));
		}

		return ast::make_expr_unresolved(token_range{ paren_begin, stream });
	}

	// tuple
	case token::square_open:
	{
		auto const begin_token = stream;
		++stream;
		auto elems = parse_expression_comma_list(stream, end, context, errors);
		assert_token(stream, token::square_close, errors);
		auto const end_token = stream;
		return make_expr_tuple(std::move(elems), token_range{ begin_token, end_token });
	}

	// unary operators
	case token::plus:
	case token::minus:
	case token::plus_plus:
	case token::minus_minus:
	case token::bit_not:
	case token::bool_not:
	case token::address_of:
	case token::dereference:
	case token::kw_sizeof:
	case token::kw_typeof:
	{
		auto op = stream;
		++stream;
		auto prec = get_unary_precedence(op->kind);
		auto expr = parse_expression(stream, end, context, errors, prec);

		auto result = make_expr_unary_op(op, std::move(expr));
		return result;
	}

	default:
		errors.emplace_back(bad_token(stream, "expected primary expression"));
		return ast::expression();
	}
};

static ast::expression parse_expression_helper(
	ast::expression lhs,
	token_pos &stream, token_pos end,
	parse_context &context,
	bz::vector<error> &errors,
	precedence prec
)
{
	token_pos op = nullptr;
	precedence op_prec;

	while (
		stream != end
		&& (op_prec = get_binary_precedence((op = stream)->kind)) <= prec)
	{
		++stream;
		switch (op->kind)
		{
		case token::paren_open:
		{
			if (
				stream != end
				&& stream->kind == token::paren_close
			)
			{
				++stream;
				lhs = make_expr_function_call(
					op, std::move(lhs), bz::vector<ast::expression>{}
				);
				break;
			}

			auto params = parse_expression_comma_list(stream, end, context, errors);
			assert_token(stream, token::paren_close, errors);

			lhs = make_expr_function_call(
				op, std::move(lhs), std::move(params)
			);
			break;
		}

		case token::square_open:
		{
			auto rhs = parse_expression(stream, end, context, errors, precedence{});
			assert_token(stream, token::square_close, errors);

			lhs = make_expr_binary_op(
				op, std::move(lhs), std::move(rhs)
			);
			break;
		}

		default:
		{
			auto rhs = parse_primary_expression(stream, end, context, errors);
			precedence rhs_prec;

			while (
				stream != end
				&& (rhs_prec = get_binary_precedence(stream->kind)) < op_prec
			)
			{
				rhs = parse_expression_helper(std::move(rhs), stream, end, context, errors, rhs_prec);
			}

			lhs = make_expr_binary_op(
				op, std::move(lhs), std::move(rhs)
			);
			break;
		}
		}
	}

	return lhs;
}

static bz::vector<ast::expression> parse_expression_comma_list(
	token_pos &stream, token_pos end,
	parse_context &context,
	bz::vector<error> &errors
)
{
	bz::vector<ast::expression> exprs = {};

	exprs.emplace_back(parse_expression(stream, end, context, errors, no_comma));

	while (stream != end && stream->kind == token::comma)
	{
		++stream; // ','
		exprs.emplace_back(parse_expression(stream, end, context, errors, no_comma));
	}

	return exprs;
}

static ast::expression parse_expression(
	token_pos &stream, token_pos end,
	parse_context &context,
	bz::vector<error> &errors,
	precedence prec
)
{
	auto lhs = parse_primary_expression(stream, end, context, errors);
	auto result = parse_expression_helper(std::move(lhs), stream, end, context, errors, prec);
	return result;
}

// ================================================================
// ------------------------ type parsing --------------------------
// ================================================================

ast::typespec parse_typespec(
	token_pos &stream, token_pos end,
	bz::vector<error> &errors
)
{
	if (stream == end)
	{
		errors.emplace_back(bad_token(stream));
		return ast::typespec();
	}

	switch (stream->kind)
	{
	case token::identifier:
	{
		auto id = stream;
		++stream;
		return ast::typespec();
//		return ast::make_ts_base_type(context.get_type(id));
	}

	case token::kw_const:
		++stream; // 'const'
		return ast::make_ts_constant(parse_typespec(stream, end, errors));

	case token::star:
		++stream; // '*'
		return ast::make_ts_pointer(parse_typespec(stream, end, errors));

	case token::ampersand:
		++stream; // '&'
		return ast::make_ts_reference(parse_typespec(stream, end, errors));

	case token::kw_function:
	{
		++stream; // 'function'
		assert_token(stream, token::paren_open, errors);

		bz::vector<ast::typespec> param_types = {};
		if (stream->kind != token::paren_close) while (stream != end)
		{
			param_types.push_back(parse_typespec(stream, end, errors));
			if (stream->kind != token::paren_close)
			{
				assert_token(stream, token::comma, errors);
			}
			else
			{
				break;
			}
		}
		assert(stream != end);
		assert_token(stream, token::paren_close, errors);
		assert_token(stream, token::arrow, errors);

		auto ret_type = parse_typespec(stream, end, errors);

		return make_ts_function(std::move(ret_type), std::move(param_types));
	}

	case token::square_open:
	{
		++stream; // '['

		bz::vector<ast::typespec> types = {};
		if (stream->kind != token::square_close) while (stream != end)
		{
			types.push_back(parse_typespec(stream, end, errors));
			if (stream->kind != token::square_close)
			{
				assert_token(stream, token::comma, errors);
			}
			else
			{
				break;
			}
		}
		assert(stream != end);
		assert_token(stream, token::square_close, errors);

		return make_ts_tuple(std::move(types));
	}

	default:
		assert(false);
		return ast::typespec();
	}
}

// ================================================================
// -------------------------- resolving ---------------------------
// ================================================================

static ast::typespec add_prototype_to_type(
	ast::typespec const &prototype,
	ast::typespec const &type
)
{
	ast::typespec const *proto_it = &prototype;
	ast::typespec const *type_it = &type;

	auto const advance = [](ast::typespec const *&it)
	{
		switch (it->kind())
		{
		case ast::typespec::index<ast::ts_reference>:
			it = &it->get<ast::ts_reference_ptr>()->base;
			break;
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

	ast::typespec ret_type;
	ast::typespec *ret_type_it = &ret_type;

	while (proto_it->kind() != ast::typespec::null)
	{
#define x(type)                                                                \
if (                                                                           \
    proto_it->kind() == ast::typespec::index<type>                             \
    || type_it->kind() == ast::typespec::index<type>                           \
)                                                                              \
{                                                                              \
    ret_type_it->emplace<type##_ptr>(std::make_unique<type>(ast::typespec())); \
    ret_type_it = &ret_type_it->get<type##_ptr>()->base;                       \
    if (proto_it->kind() == ast::typespec::index<type>) advance(proto_it);     \
    if (type_it->kind() == ast::typespec::index<type>) advance(type_it);       \
}

	x(ast::ts_reference)
	else x(ast::ts_constant)
	else x(ast::ts_pointer)
	else
	{
		assert(false);
	}

#undef x
	}

	*ret_type_it = *type_it;

	return ret_type;
}

void resolve(
	ast::typespec &ts,
	parse_context &context,
	bz::vector<error> &errors
)
{
	switch (ts.kind())
	{
	case ast::typespec::index<ast::ts_unresolved>:
	{
		auto &unresolved_ts = ts.get<ast::ts_unresolved_ptr>();
		auto stream = unresolved_ts->tokens.begin;
		auto const end = unresolved_ts->tokens.end;
		ts = parse_typespec(stream, end, errors);
		break;
	}
	default:
		break;
	}
}

static void resolve(
	ast::decl_variable &var_decl,
	parse_context &context,
	bz::vector<error> &errors
)
{
	if (var_decl.var_type.kind() != ast::typespec::null)
	{
		resolve(var_decl.var_type, context, errors);
	}
	var_decl.var_type = add_prototype_to_type(var_decl.prototype, var_decl.var_type);
	if (var_decl.init_expr.has_value())
	{
		resolve(*var_decl.init_expr, context, errors);
	}

	// TODO: check if expression is convertible to type
}

static void resolve(
	ast::decl_function &decl,
	parse_context &context,
	bz::vector<error> &errors
)
{
}

static void resolve(
	ast::decl_operator &decl,
	parse_context &context,
	bz::vector<error> &errors
)
{
}

static void resolve(
	ast::decl_struct &decl,
	parse_context &context,
	bz::vector<error> &errors
)
{
}

void resolve(
	ast::declaration &decl,
	parse_context &context,
	bz::vector<error> &errors
)
{
	switch (decl.kind())
	{
	case ast::declaration::index<ast::decl_variable>:
		resolve(*decl.get<ast::decl_variable_ptr>(), context, errors);
		break;
	case ast::declaration::index<ast::decl_function>:
	case ast::declaration::index<ast::decl_operator>:
	case ast::declaration::index<ast::decl_struct>:
	default:
		break;
	}
}

void resolve(
	ast::statement &stmt,
	parse_context &context,
	bz::vector<error> &errors
)
{
	switch (stmt.kind())
	{
	case ast::statement::index<ast::stmt_if>:
	{
		auto &if_stmt = *stmt.get<ast::stmt_if_ptr>();
		resolve(if_stmt.condition, context, errors);
		resolve(if_stmt.then_block, context, errors);
		if (if_stmt.else_block.has_value())
		{
			resolve(*if_stmt.else_block, context, errors);
		}
		break;
	}
	case ast::statement::index<ast::stmt_while>:
	{
		auto &while_stmt = *stmt.get<ast::stmt_while_ptr>();
		resolve(while_stmt.condition, context, errors);
		resolve(while_stmt.while_block, context, errors);
		break;
	}
	case ast::statement::index<ast::stmt_for>:
		assert(false);
		break;
	case ast::statement::index<ast::stmt_return>:
		resolve(stmt.get<ast::stmt_return_ptr>()->expr, context, errors);
		break;
	case ast::statement::index<ast::stmt_no_op>:
		break;
	case ast::statement::index<ast::stmt_compound>:
	{
		auto &comp_stmt = *stmt.get<ast::stmt_compound_ptr>();
		context.add_scope();
		for (auto &s : comp_stmt.statements)
		{
			resolve(s, context, errors);
		}
		context.remove_scope();
		break;
	}
	case ast::statement::index<ast::stmt_expression>:
		resolve(stmt.get<ast::stmt_expression_ptr>()->expr, context, errors);
		break;
	case ast::statement::index<ast::decl_variable>:
	case ast::statement::index<ast::decl_function>:
	case ast::statement::index<ast::decl_operator>:
	case ast::statement::index<ast::decl_struct>:
	default:
		assert(false);
		break;
	}
}


using resolve_ret_val = ast::expression::expr_type_t;

static resolve_ret_val resolve(
	ast::expr_identifier const &id,
	parse_context &context,
	bz::vector<error> &errors
)
{
	resolve_ret_val ret_val = {
		ast::expression::lvalue,
		context.get_identifier_type(id.identifier)
	};
	if (ret_val.expr_type.kind() == ast::typespec::null)
	{
		errors.emplace_back(bad_token(id.identifier, "undeclared identifier"));
	}
	else if (ret_val.expr_type.kind() == ast::typespec::index<ast::ts_reference>)
	{
		ret_val.type_kind = ast::expression::lvalue_reference;
		auto ref_ptr = std::move(ret_val.expr_type.get<ast::ts_reference_ptr>());
		ret_val.expr_type = std::move(ref_ptr->base);
	}

	return ret_val;
}

static resolve_ret_val resolve(
	ast::expr_literal const &literal,
	parse_context &,
	bz::vector<error> &
)
{
	resolve_ret_val ret_val = {
		ast::expression::rvalue,
		ast::typespec()
	};

	switch (literal.value.index())
	{
	case ast::expr_literal::integer_number:
		ret_val.expr_type = ast::make_ts_base_type(ast::int32_);
		break;
	case ast::expr_literal::floating_point_number:
		ret_val.expr_type = ast::make_ts_base_type(ast::float64_);
		break;
	case ast::expr_literal::string:
		ret_val.expr_type = ast::make_ts_base_type(ast::str_);
		break;
	case ast::expr_literal::character:
		ret_val.expr_type = ast::make_ts_base_type(ast::char_);
		break;
	case ast::expr_literal::bool_true:
	case ast::expr_literal::bool_false:
		ret_val.expr_type = ast::make_ts_base_type(ast::bool_);
		break;
	case ast::expr_literal::null:
		ret_val.expr_type = ast::make_ts_base_type(ast::null_t_);
		break;
	default:
		assert(false);
		break;
	}
	return ret_val;
}

static resolve_ret_val resolve(
	ast::expr_tuple &tuple,
	parse_context &context,
	bz::vector<error> &errors
)
{
	resolve_ret_val ret_val = {
		ast::expression::rvalue,
		ast::make_ts_tuple(bz::vector<ast::typespec>{})
	};

	auto &tuple_t = *ret_val.expr_type.get<ast::ts_tuple_ptr>();

	for (auto &elem : tuple.elems)
	{
		resolve(elem, context, errors);
		tuple_t.types.emplace_back(elem.expr_type.expr_type);
	}

	return ret_val;
}

static resolve_ret_val resolve(
	ast::expr_unary_op &unary_op,
	parse_context &context,
	bz::vector<error> &errors
)
{
	resolve(unary_op.expr, context, errors);
	resolve_ret_val ret_val = {
		ast::expression::rvalue,
		context.get_operation_type(unary_op)
	};
	if (ret_val.expr_type.kind() == ast::typespec::null)
	{
		errors.emplace_back(bad_tokens(unary_op, "undeclared operation"));
	}
	else if (ret_val.expr_type.kind() == ast::typespec::index<ast::ts_reference>)
	{
		ret_val.type_kind = ast::expression::lvalue_reference;
		auto ref_ptr = std::move(ret_val.expr_type.get<ast::ts_reference_ptr>());
		ret_val.expr_type = std::move(ref_ptr->base);
	}

	return ret_val;
}

static resolve_ret_val resolve(
	ast::expr_binary_op &binary_op,
	parse_context &context,
	bz::vector<error> &errors
)
{
	resolve(binary_op.lhs, context, errors);
	resolve(binary_op.rhs, context, errors);
	resolve_ret_val ret_val = {
		ast::expression::rvalue,
		context.get_operation_type(binary_op)
	};
	if (ret_val.expr_type.kind() == ast::typespec::null)
	{
		errors.emplace_back(bad_tokens(binary_op, "undeclared operation"));
	}
	else if (ret_val.expr_type.kind() == ast::typespec::index<ast::ts_reference>)
	{
		ret_val.type_kind = ast::expression::lvalue_reference;
		auto ref_ptr = std::move(ret_val.expr_type.get<ast::ts_reference_ptr>());
		ret_val.expr_type = std::move(ref_ptr->base);
	}

	return ret_val;
}

static resolve_ret_val resolve(
	ast::expr_function_call &fn_call,
	parse_context &context,
	bz::vector<error> &errors
)
{
	errors.emplace_back(bad_tokens(fn_call, "not yet implemented"));
	return {
		ast::expression::rvalue,
		ast::typespec()
	};
}


void resolve(ast::expression &expr, parse_context &context, bz::vector<error> &errors)
{
	switch (expr.kind())
	{
	case ast::expression::index<ast::expr_unresolved>:
		break;
	case ast::expression::index<ast::expr_identifier>:
	{
		auto &id = *expr.get<ast::expr_identifier_ptr>();
		expr.expr_type = resolve(id, context, errors);
		break;
	}
	case ast::expression::index<ast::expr_literal>:
	{
		auto &literal = *expr.get<ast::expr_literal_ptr>();
		expr.expr_type = resolve(literal, context, errors);
		break;
	}
	case ast::expression::index<ast::expr_tuple>:
	{
		auto &tuple = *expr.get<ast::expr_tuple_ptr>();
		expr.expr_type = resolve(tuple, context, errors);
		break;
	}
	case ast::expression::index<ast::expr_unary_op>:
	{
		auto &unary_op = *expr.get<ast::expr_unary_op_ptr>();
		expr.expr_type = resolve(unary_op, context, errors);
		break;
	}
	case ast::expression::index<ast::expr_binary_op>:
	{
		auto &binary_op = *expr.get<ast::expr_binary_op_ptr>();
		expr.expr_type = resolve(binary_op, context, errors);
		break;
	}
	case ast::expression::index<ast::expr_function_call>:
	{
		auto &fn_call = *expr.get<ast::expr_function_call_ptr>();
		expr.expr_type = resolve(fn_call, context, errors);
		break;
	}
	default:
		assert(false);
		break;
	}
}
