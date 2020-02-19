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
	{ lex::token::scope,              precedence{  1, true  } },

	{ lex::token::paren_open,         precedence{  2, true  } },
	{ lex::token::square_open,        precedence{  2, true  } },
	{ lex::token::dot,                precedence{  2, true  } },
	{ lex::token::arrow,              precedence{  2, true  } },

	{ lex::token::dot_dot,            precedence{  4, true  } },

	{ lex::token::multiply,           precedence{  5, true  } },
	{ lex::token::divide,             precedence{  5, true  } },
	{ lex::token::modulo,             precedence{  5, true  } },

	{ lex::token::plus,               precedence{  6, true  } },
	{ lex::token::minus,              precedence{  6, true  } },

	{ lex::token::bit_left_shift,     precedence{  7, true  } },
	{ lex::token::bit_right_shift,    precedence{  7, true  } },

	{ lex::token::bit_and,            precedence{  8, true  } },
	{ lex::token::bit_xor,            precedence{  9, true  } },
	{ lex::token::bit_or,             precedence{ 10, true  } },

	{ lex::token::less_than,          precedence{ 11, true  } },
	{ lex::token::less_than_eq,       precedence{ 11, true  } },
	{ lex::token::greater_than,       precedence{ 11, true  } },
	{ lex::token::greater_than_eq,    precedence{ 11, true  } },

	{ lex::token::equals,             precedence{ 12, true  } },
	{ lex::token::not_equals,         precedence{ 12, true  } },

	{ lex::token::bool_and,           precedence{ 13, true  } },
	{ lex::token::bool_xor,           precedence{ 14, true  } },
	{ lex::token::bool_or,            precedence{ 15, true  } },

	// ternary ?
	{ lex::token::assign,             precedence{ 16, false } },
	{ lex::token::plus_eq,            precedence{ 16, false } },
	{ lex::token::minus_eq,           precedence{ 16, false } },
	{ lex::token::multiply_eq,        precedence{ 16, false } },
	{ lex::token::divide_eq,          precedence{ 16, false } },
	{ lex::token::modulo_eq,          precedence{ 16, false } },
	{ lex::token::dot_dot_eq,         precedence{ 16, false } },
	{ lex::token::bit_left_shift_eq,  precedence{ 16, false } },
	{ lex::token::bit_right_shift_eq, precedence{ 16, false } },
	{ lex::token::bit_and_eq,         precedence{ 16, false } },
	{ lex::token::bit_xor_eq,         precedence{ 16, false } },
	{ lex::token::bit_or_eq,          precedence{ 16, false } },

	{ lex::token::comma,              precedence{ 18, true  } },
};

constexpr precedence no_comma{ 17, true };

static std::map<uint32_t, precedence> unary_op_precendences =
{
	{ lex::token::plus,               precedence{  3, false } },
	{ lex::token::minus,              precedence{  3, false } },
	{ lex::token::plus_plus,          precedence{  3, false } },
	{ lex::token::minus_minus,        precedence{  3, false } },
	{ lex::token::bit_not,            precedence{  3, false } },
	{ lex::token::bool_not,           precedence{  3, false } },
	{ lex::token::address_of,         precedence{  3, false } },
	{ lex::token::dereference,        precedence{  3, false } },
	{ lex::token::kw_sizeof,          precedence{  3, false } },
	{ lex::token::kw_typeof,          precedence{  3, false } },
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
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context,
	bz::vector<ctx::error> &errors,
	precedence prec
);

static bz::vector<ast::expression> parse_expression_comma_list(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context,
	bz::vector<ctx::error> &errors
);

static void resolve_literal(
	ast::expression &expr,
	ctx::parse_context &context,
	bz::vector<ctx::error> &
)
{
	assert(expr.kind() == ast::expression::index<ast::expr_literal>);
	auto &literal = *expr.get<ast::expr_literal_ptr>();

	expr.expr_type.type_kind = ast::expression::rvalue;
	switch (literal.value.index())
	{
	case ast::expr_literal::integer_number:
		expr.expr_type.expr_type = ast::make_ts_base_type(
			context.get_type_info("int32")
		);
		break;
	case ast::expr_literal::floating_point_number:
		expr.expr_type.expr_type = ast::make_ts_base_type(
			context.get_type_info("float64")
		);
		break;
	case ast::expr_literal::string:
		expr.expr_type.expr_type = ast::make_ts_base_type(
			context.get_type_info("str")
		);
		break;
	case ast::expr_literal::character:
		expr.expr_type.expr_type = ast::make_ts_base_type(
			context.get_type_info("char")
		);
		break;
	case ast::expr_literal::bool_true:
	case ast::expr_literal::bool_false:
		expr.expr_type.expr_type = ast::make_ts_base_type(
			context.get_type_info("bool")
		);
		break;
	case ast::expr_literal::null:
		expr.expr_type.expr_type = ast::make_ts_base_type(
			context.get_type_info("null_t")
		);
		break;
	default:
		assert(false);
		break;
	}
}

static ast::expression parse_primary_expression(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context,
	bz::vector<ctx::error> &errors
)
{
	if (stream == end)
	{
		errors.emplace_back(bad_token(stream, "expected primary expression"));
		return ast::expression();
	}

	auto const get_paren_matched_range = [](lex::token_pos &stream, lex::token_pos end) -> lex::token_range
	{
		auto const begin = stream;
		size_t paren_level = 1;
		while (stream != end && paren_level != 0)
		{
			if (
				stream->kind == lex::token::paren_open
				|| stream->kind == lex::token::square_open
				|| stream->kind == lex::token::curly_open
			)
			{
				++paren_level;
			}
			else if (
				stream->kind == lex::token::paren_close
				|| stream->kind == lex::token::square_close
				|| stream->kind == lex::token::curly_close
			)
			{
				--paren_level;
			}
		}
		assert(stream != end);
		assert(
			(stream - 1)->kind == lex::token::paren_close
			|| (stream - 1)->kind == lex::token::square_close
			|| (stream - 1)->kind == lex::token::curly_close
		);
		assert(paren_level == 0);
		return { begin, stream - 1 };
	};

	switch (stream->kind)
	{
	case lex::token::identifier:
	{
		auto id = ast::make_expr_identifier(stream);
		id.expr_type.expr_type = context.get_identifier_type(stream);
		if (id.expr_type.expr_type.kind() == ast::typespec::null)
		{
			errors.emplace_back(bad_token(stream, "undeclared identifier"));
		}
		else if (id.expr_type.expr_type.kind() == ast::typespec::index<ast::ts_reference>)
		{
			id.expr_type.type_kind = ast::expression::lvalue_reference;
			auto ref_ptr = std::move(id.expr_type.expr_type.get<ast::ts_reference_ptr>());
			id.expr_type.expr_type = std::move(ref_ptr->base);
		}
		else
		{
			id.expr_type.type_kind = ast::expression::lvalue;
		}
		++stream;
		return id;
	}

	// literals
	case lex::token::number_literal:
	case lex::token::string_literal:
	case lex::token::character_literal:
	case lex::token::kw_true:
	case lex::token::kw_false:
	case lex::token::kw_null:
	{
		auto literal = ast::make_expr_literal(stream);
		resolve_literal(literal, context, errors);
		++stream;
		return literal;
	}

	case lex::token::paren_open:
	{
		++stream;
		auto [inner_stream, inner_end] = get_paren_matched_range(stream, end);
		return parse_expression(inner_stream, inner_end, context, errors, precedence{});
	}

	// tuple
	case lex::token::square_open:
	{
		auto const begin_token = stream;
		++stream;
		auto [inner_stream, inner_end] = get_paren_matched_range(stream, end);
		auto elems = parse_expression_comma_list(inner_stream, inner_end, context, errors);
		auto const end_token = stream;
		return make_expr_tuple(std::move(elems), lex::token_range{ begin_token, end_token });
	}

	// unary operators
	default:
		if (lex::is_unary_operator(stream->kind))
		{
			auto op = stream;
			auto prec = get_unary_precedence(op->kind);
			++stream;
			auto expr = parse_expression(stream, end, context, errors, prec);

			auto result = make_expr_unary_op(op, std::move(expr));
			result.expr_type.expr_type = context.get_operation_type(
				*result.get<ast::expr_unary_op_ptr>(), errors
			);
			if (result.expr_type.expr_type.kind() == ast::typespec::index<ast::ts_reference>)
			{
				result.expr_type.type_kind = ast::expression::lvalue_reference;
				auto ref_ptr = std::move(result.expr_type.expr_type.get<ast::ts_reference_ptr>());
				result.expr_type.expr_type = std::move(ref_ptr->base);
			}
			else
			{
				result.expr_type.type_kind = ast::expression::rvalue;
			}

			return result;
		}
		else
		{
			errors.emplace_back(bad_token(stream, "expected primary expression"));
			return ast::expression();
		}
	}
};

static ast::expression parse_expression_helper(
	ast::expression lhs,
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context,
	bz::vector<ctx::error> &errors,
	precedence prec
)
{
	lex::token_pos op = nullptr;
	precedence op_prec;

	auto const resolve_expr = [&context, &errors](ast::expression &expr)
	{
		assert(expr.kind() == ast::expression::index<ast::expr_binary_op>);

		auto &binary_expr = *expr.get<ast::expr_binary_op_ptr>();
		if (
			binary_expr.lhs.kind() == ast::expression::null
			|| binary_expr.lhs.expr_type.expr_type.kind() == ast::typespec::null
			|| binary_expr.rhs.kind() == ast::expression::null
			|| binary_expr.rhs.expr_type.expr_type.kind() == ast::typespec::null
		)
		{
			return;
		}

		expr.expr_type.expr_type = context.get_operation_type(
			*expr.get<ast::expr_binary_op_ptr>(), errors
		);
		if (expr.expr_type.expr_type.kind() == ast::typespec::index<ast::ts_reference>)
		{
			expr.expr_type.type_kind = ast::expression::lvalue_reference;
			auto ref_ptr = std::move(expr.expr_type.expr_type.get<ast::ts_reference_ptr>());
			expr.expr_type.expr_type = std::move(ref_ptr->base);
		}
		else
		{
			expr.expr_type.type_kind = ast::expression::rvalue;
		}
	};

	while (
		stream != end
		// not really clean... we assign to op and op_prec here
		&& (op_prec = get_binary_precedence((op = stream)->kind)) <= prec
	)
	{
		++stream;
		switch (op->kind)
		{
		case lex::token::paren_open:
		{
			if (
				stream != end
				&& stream->kind == lex::token::paren_close
			)
			{
				++stream;
				lhs = make_expr_function_call(
					op, std::move(lhs), bz::vector<ast::expression>{}
				);
				resolve_expr(lhs);
			}
			else
			{
				auto params = parse_expression_comma_list(stream, end, context, errors);
				assert_token(stream, lex::token::paren_close, errors);

				lhs = make_expr_function_call(
					op, std::move(lhs), std::move(params)
				);
				resolve_expr(lhs);
			}
			break;
		}

		case lex::token::square_open:
		{
			auto rhs = parse_expression(stream, end, context, errors, precedence{});
			assert_token(stream, lex::token::square_close, errors);

			lhs = make_expr_binary_op(
				op, std::move(lhs), std::move(rhs)
			);
			resolve_expr(lhs);
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
				resolve_expr(rhs);
			}

			lhs = make_expr_binary_op(
				op, std::move(lhs), std::move(rhs)
			);
			resolve_expr(lhs);
			break;
		}
		}
	}

	return lhs;
}

static bz::vector<ast::expression> parse_expression_comma_list(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context,
	bz::vector<ctx::error> &errors
)
{
	bz::vector<ast::expression> exprs = {};

	exprs.emplace_back(parse_expression(stream, end, context, errors, no_comma));

	while (stream != end && stream->kind == lex::token::comma)
	{
		++stream; // ','
		exprs.emplace_back(parse_expression(stream, end, context, errors, no_comma));
	}

	return exprs;
}

static ast::expression parse_expression(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context,
	bz::vector<ctx::error> &errors,
	precedence prec
)
{
	auto lhs = parse_primary_expression(stream, end, context, errors);
	if (lhs.kind() == ast::expression::null)
	{
		assert(errors.size() != 0);
		return ast::expression();
	}
	auto result = parse_expression_helper(std::move(lhs), stream, end, context, errors, prec);
	return result;
}

// ================================================================
// ------------------------ type parsing --------------------------
// ================================================================

ast::typespec parse_typespec(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context,
	bz::vector<ctx::error> &errors
)
{
	if (stream == end)
	{
		errors.emplace_back(bad_token(stream));
		return ast::typespec();
	}

	switch (stream->kind)
	{
	case lex::token::identifier:
	{
		auto const id = stream;
		++stream;
		auto const info = context.get_type_info(id->value);
		if (info)
		{
			return ast::make_ts_base_type(info);
		}
		else
		{
			errors.emplace_back(bad_token(id, "undeclared typename"));
			return ast::typespec();
		}
	}

	case lex::token::kw_const:
		++stream; // 'const'
		return ast::make_ts_constant(parse_typespec(stream, end, context, errors));

	case lex::token::star:
		++stream; // '*'
		return ast::make_ts_pointer(parse_typespec(stream, end, context, errors));

	case lex::token::ampersand:
		++stream; // '&'
		return ast::make_ts_reference(parse_typespec(stream, end, context, errors));

	case lex::token::kw_function:
	{
		++stream; // 'function'
		assert_token(stream, lex::token::paren_open, errors);

		bz::vector<ast::typespec> param_types = {};
		if (stream->kind != lex::token::paren_close) while (stream != end)
		{
			param_types.push_back(parse_typespec(stream, end, context, errors));
			if (stream->kind != lex::token::paren_close)
			{
				assert_token(stream, lex::token::comma, errors);
			}
			else
			{
				break;
			}
		}
		assert(stream != end);
		assert_token(stream, lex::token::paren_close, errors);
		assert_token(stream, lex::token::arrow, errors);

		auto ret_type = parse_typespec(stream, end, context, errors);

		return make_ts_function(std::move(ret_type), std::move(param_types));
	}

	case lex::token::square_open:
	{
		++stream; // '['

		bz::vector<ast::typespec> types = {};
		if (stream->kind != lex::token::square_close) while (stream != end)
		{
			types.push_back(parse_typespec(stream, end, context, errors));
			if (stream->kind != lex::token::square_close)
			{
				assert_token(stream, lex::token::comma, errors);
			}
			else
			{
				break;
			}
		}
		assert(stream != end);
		assert_token(stream, lex::token::square_close, errors);

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
	ctx::parse_context &context,
	bz::vector<ctx::error> &errors
)
{
	switch (ts.kind())
	{
	case ast::typespec::index<ast::ts_unresolved>:
	{
		auto &unresolved_ts = ts.get<ast::ts_unresolved_ptr>();
		auto stream = unresolved_ts->tokens.begin;
		auto const end = unresolved_ts->tokens.end;
		ts = parse_typespec(stream, end, context, errors);
		break;
	}
	default:
		break;
	}
}

static void resolve(
	ast::decl_variable &var_decl,
	ctx::parse_context &context,
	bz::vector<ctx::error> &errors
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
	ctx::parse_context &context,
	bz::vector<ctx::error> &errors
)
{
}

static void resolve(
	ast::decl_operator &decl,
	ctx::parse_context &context,
	bz::vector<ctx::error> &errors
)
{
}

static void resolve(
	ast::decl_struct &decl,
	ctx::parse_context &context,
	bz::vector<ctx::error> &errors
)
{
}

void resolve(
	ast::declaration &decl,
	ctx::parse_context &context,
	bz::vector<ctx::error> &errors
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
	ctx::parse_context &context,
	bz::vector<ctx::error> &errors
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
	ast::expr_literal const &literal,
	ctx::parse_context &context,
	bz::vector<ctx::error> &
)
{
	resolve_ret_val ret_val = {
		ast::expression::rvalue,
		ast::typespec()
	};

	switch (literal.value.index())
	{
	case ast::expr_literal::integer_number:
		ret_val.expr_type = ast::make_ts_base_type(
			context.get_type_info("int32")
		);
		break;
	case ast::expr_literal::floating_point_number:
		ret_val.expr_type = ast::make_ts_base_type(
			context.get_type_info("float64")
		);
		break;
	case ast::expr_literal::string:
		ret_val.expr_type = ast::make_ts_base_type(
			context.get_type_info("str")
		);
		break;
	case ast::expr_literal::character:
		ret_val.expr_type = ast::make_ts_base_type(
			context.get_type_info("char")
		);
		break;
	case ast::expr_literal::bool_true:
	case ast::expr_literal::bool_false:
		ret_val.expr_type = ast::make_ts_base_type(
			context.get_type_info("bool")
		);
		break;
	case ast::expr_literal::null:
		ret_val.expr_type = ast::make_ts_base_type(
			context.get_type_info("null_t")
		);
		break;
	default:
		assert(false);
		break;
	}
	return ret_val;
}

static resolve_ret_val resolve(
	ast::expr_tuple &tuple,
	ctx::parse_context &context,
	bz::vector<ctx::error> &errors
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
	ast::expr_function_call &fn_call,
	ctx::parse_context &context,
	bz::vector<ctx::error> &errors
)
{
	errors.emplace_back(lex::bad_tokens(fn_call, "not yet implemented"));
	return {
		ast::expression::rvalue,
		ast::typespec()
	};
}


void resolve(ast::expression &expr, ctx::parse_context &context, bz::vector<ctx::error> &errors)
{
	if (expr.kind() == ast::expression::index<ast::expr_unresolved>)
	{
		auto unresolved_expr = *expr.get<ast::expr_unresolved_ptr>();
		auto stream = unresolved_expr.expr.begin;
		auto const end = unresolved_expr.expr.end;
		auto new_expr = parse_expression(stream, end, context, errors, precedence{});
		if (stream != end)
		{
			errors.emplace_back(bad_tokens(
				stream, stream, end,
				"expected ';'"
			));
		}
		expr = std::move(new_expr);
	}
	return;

	switch (expr.kind())
	{
	case ast::expression::index<ast::expr_literal>:
	{
		auto &literal = *expr.get<ast::expr_literal_ptr>();
		expr.expr_type = resolve(literal, context, errors);
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
