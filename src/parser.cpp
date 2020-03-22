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

lex::token_range get_paren_matched_range(lex::token_pos &stream, lex::token_pos end)
{
	auto const begin = stream;
	size_t paren_level = 1;
	for (; stream != end && paren_level != 0; ++stream)
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
	assert(paren_level == 0);
	assert(
		(stream - 1)->kind == lex::token::paren_close
		|| (stream - 1)->kind == lex::token::square_close
		|| (stream - 1)->kind == lex::token::curly_close
	);
	return { begin, stream - 1 };
};

static ast::expression parse_expression(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context,
	precedence prec
);

static bz::vector<ast::expression> parse_expression_comma_list(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

static void resolve_literal(
	ast::expression &expr,
	ctx::parse_context &context
)
{
	assert(expr.is<ast::expr_literal>());
	auto &literal = *expr.get<ast::expr_literal_ptr>();

	expr.expr_type.type_kind = ast::expression::rvalue;
	switch (literal.value.index())
	{
	case ast::expr_literal::integer_number:
	{
#define set_type_from_postfix(pf, type)                                              \
if (postfix == pf)                                                                   \
{                                                                                    \
    if (val > static_cast<uint64_t>(std::numeric_limits<type##_t>::max()))           \
    {                                                                                \
        context.report_error(literal, "value is too big to fit into a '" #type "'"); \
    }                                                                                \
    literal.type_kind = ast::type_info::type_kind::type##_;                          \
    expr.expr_type.expr_type = ast::make_ts_base_type(                               \
        context.get_type_info(#type)                                                 \
    );                                                                               \
}
		auto const val = literal.value.get<ast::expr_literal::integer_number>();
		auto const postfix = literal.src_pos->postfix;

		set_type_from_postfix("", int32)
		else set_type_from_postfix("i8",  int8)
		else set_type_from_postfix("i16", int16)
		else set_type_from_postfix("i32", int32)
		else set_type_from_postfix("i64", int64)
		else set_type_from_postfix("u8",  uint8)
		else set_type_from_postfix("u16", uint16)
		else set_type_from_postfix("u32", uint32)
		else set_type_from_postfix("u64", uint64)

		break;
#undef set_type_from_postfix
	}
	case ast::expr_literal::floating_point_number:
		literal.type_kind = ast::type_info::type_kind::float64_;
		expr.expr_type.expr_type = ast::make_ts_base_type(
			context.get_type_info("float64")
		);
		break;
	case ast::expr_literal::string:
		literal.type_kind = ast::type_info::type_kind::str_;
		expr.expr_type.expr_type = ast::make_ts_base_type(
			context.get_type_info("str")
		);
		break;
	case ast::expr_literal::character:
		literal.type_kind = ast::type_info::type_kind::char_;
		expr.expr_type.expr_type = ast::make_ts_base_type(
			context.get_type_info("char")
		);
		break;
	case ast::expr_literal::bool_true:
	case ast::expr_literal::bool_false:
		literal.type_kind = ast::type_info::type_kind::bool_;
		expr.expr_type.expr_type = ast::make_ts_base_type(
			context.get_type_info("bool")
		);
		break;
	case ast::expr_literal::null:
		literal.type_kind = ast::type_info::type_kind::null_t_;
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
	ctx::parse_context &context
)
{
	if (stream == end)
	{
		context.report_error(stream, "expected primary expression");
		return ast::expression();
	}

	switch (stream->kind)
	{
	case lex::token::identifier:
	{
		auto const decl = context.get_identifier_decl(stream);
		auto id = ast::make_expr_identifier({ stream, stream + 1 }, stream, decl);
		id.expr_type = context.get_identifier_type(stream);
		++stream;
		return id;
	}

	// literals
	case lex::token::integer_literal:
	case lex::token::floating_point_literal:
	case lex::token::hex_literal:
	case lex::token::oct_literal:
	case lex::token::bin_literal:
	case lex::token::string_literal:
	case lex::token::character_literal:
	case lex::token::kw_true:
	case lex::token::kw_false:
	case lex::token::kw_null:
	{
		auto literal = ast::make_expr_literal({ stream, stream + 1 }, stream);
		++stream;
		resolve_literal(literal, context);
		return literal;
	}

	case lex::token::paren_open:
	{
		auto const paren_begin = stream;
		++stream;
		auto [inner_stream, inner_end] = get_paren_matched_range(stream, end);
		auto expr = parse_expression(inner_stream, inner_end, context, precedence{});
		expr.tokens = { paren_begin, stream };
		return expr;
	}

	// tuple
	case lex::token::square_open:
	{
		auto const begin_token = stream;
		++stream;
		auto [inner_stream, inner_end] = get_paren_matched_range(stream, end);
		auto elems = parse_expression_comma_list(inner_stream, inner_end, context);
		auto const end_token = stream;
		auto expr = make_expr_tuple(
			{ begin_token, end_token },
			std::move(elems)
		);

		expr.expr_type.type_kind = ast::expression::rvalue;
		expr.expr_type.expr_type = ast::make_ts_tuple(bz::vector<ast::typespec>{});

		auto &tuple = *expr.get<ast::expr_tuple_ptr>();
		auto &tuple_t = *expr.expr_type.expr_type.get<ast::ts_tuple_ptr>();

		for (auto &e : tuple.elems)
		{
			tuple_t.types.emplace_back(e.expr_type.expr_type);
		}

		return expr;
	}

	// unary operators
	default:
		if (lex::is_unary_operator(stream->kind))
		{
			auto op = stream;
			auto prec = get_unary_precedence(op->kind);
			++stream;
			auto expr = parse_expression(stream, end, context, prec);

			auto result = make_expr_unary_op({ op, stream }, op, std::move(expr));
			auto &unary_op = *result.get<ast::expr_unary_op_ptr>();
			std::tie(unary_op.op_body, result.expr_type) = context.get_operation_body_and_type(unary_op);
			return result;
		}
		else
		{
			context.report_error(stream, "expected primary expression");
			return ast::expression();
		}
	}
};

static ast::expression parse_expression_helper(
	ast::expression lhs,
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context,
	precedence prec
)
{
	lex::token_pos op = nullptr;
	precedence op_prec;

	auto const resolve_expr = [&context](ast::expression &expr)
	{
		if (expr.is<ast::expr_binary_op>())
		{
			auto &binary_op = *expr.get<ast::expr_binary_op_ptr>();
			if (
				binary_op.lhs.kind() == ast::expression::null
				|| binary_op.lhs.expr_type.expr_type.kind() == ast::typespec::null
				|| binary_op.rhs.kind() == ast::expression::null
				|| binary_op.rhs.expr_type.expr_type.kind() == ast::typespec::null
			)
			{
				return;
			}

			std::tie(binary_op.op_body, expr.expr_type)
				= context.get_operation_body_and_type(binary_op);
		}
		else if (expr.is<ast::expr_function_call>())
		{
			auto &func_call = *expr.get<ast::expr_function_call_ptr>();
			std::tie(func_call.func_body, expr.expr_type)
				= context.get_function_call_body_and_type(func_call);
		}
		else
		{
			assert(false);
		}
	};

	while (
		stream != end
		// not really clean... we assign to both op and op_prec here
		&& (op_prec = get_binary_precedence((op = stream)->kind)) <= prec
	)
	{
		++stream;
		switch (op->kind)
		{
		case lex::token::paren_open:
		{
			if (stream->kind == lex::token::paren_close)
			{
				++stream;
				lhs = make_expr_function_call(
					{ lhs.get_tokens_begin(), stream },
					op, std::move(lhs), bz::vector<ast::expression>{}
				);
				resolve_expr(lhs);
			}
			else
			{
				auto [inner_stream, inner_end] = get_paren_matched_range(stream, end);
				auto params = parse_expression_comma_list(inner_stream, inner_end, context);
				if (inner_stream != inner_end)
				{
					context.report_error(inner_stream, "expected ',' or closing )");
				}

				lhs = make_expr_function_call(
					{ lhs.get_tokens_begin(), stream },
					op, std::move(lhs), std::move(params)
				);
				resolve_expr(lhs);
			}
			break;
		}

		case lex::token::square_open:
		{
			auto [inner_stream, inner_end] = get_paren_matched_range(stream, end);
			auto rhs = parse_expression(inner_stream, inner_end, context, precedence{});
			if (inner_stream != inner_end)
			{
				context.report_error(inner_stream, "expected closing ]");
			}

			lhs = make_expr_binary_op(
				{ lhs.get_tokens_begin(), stream },
				op, std::move(lhs), std::move(rhs)
			);
			resolve_expr(lhs);
			break;
		}

		default:
		{
			// overloaded function
			if (
				lhs.expr_type.type_kind == ast::expression::function_name
				&& lhs.expr_type.expr_type.kind() == ast::typespec::null
			)
			{
				assert(lhs.is<ast::expr_identifier>());
				context.report_ambiguous_id_error(lhs.get<ast::expr_identifier_ptr>()->identifier);
			}
			auto rhs = parse_primary_expression(stream, end, context);
			precedence rhs_prec;

			while (
				stream != end
				&& (rhs_prec = get_binary_precedence(stream->kind)) < op_prec
			)
			{
				rhs = parse_expression_helper(std::move(rhs), stream, end, context, rhs_prec);
				resolve_expr(rhs);
			}

			lhs = make_expr_binary_op(
				{ lhs.get_tokens_begin(), stream },
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
	ctx::parse_context &context
)
{
	bz::vector<ast::expression> exprs = {};

	exprs.emplace_back(parse_expression(stream, end, context, no_comma));

	while (stream != end && stream->kind == lex::token::comma)
	{
		++stream; // ','
		exprs.emplace_back(parse_expression(stream, end, context, no_comma));
	}

	return exprs;
}

static ast::expression parse_expression(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context,
	precedence prec
)
{
	auto lhs = parse_primary_expression(stream, end, context);
	if (lhs.kind() == ast::expression::null)
	{
		assert(context.has_errors());
		return ast::expression();
	}
	if (stream == end)
	{
		if (
			lhs.expr_type.type_kind == ast::expression::function_name
			&& lhs.expr_type.expr_type.kind() == ast::typespec::null
		)
		{
			assert(lhs.is<ast::expr_identifier>());
			context.report_ambiguous_id_error(lhs.get<ast::expr_identifier_ptr>()->identifier);
		}
		return lhs;
	}
	else
	{
		auto result = parse_expression_helper(std::move(lhs), stream, end, context, prec);
		return result;
	}
}

// ================================================================
// ------------------------ type parsing --------------------------
// ================================================================

static ast::typespec parse_typespec(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	if (stream == end)
	{
		context.report_error(stream, "expected a type");
		return ast::typespec();
	}

	switch (stream->kind)
	{
	case lex::token::identifier:
	{
		auto const id = stream;
		++stream;
		if (id->value == "void")
		{
			return ast::make_ts_void();
		}
		else
		{
			auto const info = context.get_type_info(id->value);
			if (info)
			{
				return ast::make_ts_base_type(info);
			}
			else
			{
				context.report_error(id, "undeclared typename");
				return ast::typespec();
			}
		}
	}

	case lex::token::kw_const:
		++stream; // 'const'
		return ast::make_ts_constant(parse_typespec(stream, end, context));

	case lex::token::star:
		++stream; // '*'
		return ast::make_ts_pointer(parse_typespec(stream, end, context));

	case lex::token::ampersand:
		++stream; // '&'
		return ast::make_ts_reference(parse_typespec(stream, end, context));

	case lex::token::kw_function:
	{
		++stream; // 'function'
		context.assert_token(stream, lex::token::paren_open);

		bz::vector<ast::typespec> param_types = {};
		// not the nicest line... there's a while here: vv
		if (stream->kind != lex::token::paren_close) while (stream != end)
		{
			param_types.push_back(parse_typespec(stream, end, context));
			if (stream->kind != lex::token::paren_close)
			{
				context.assert_token(stream, lex::token::comma);
			}
			else
			{
				break;
			}
		}
		assert(stream != end);
		context.assert_token(stream, lex::token::paren_close);
		context.assert_token(stream, lex::token::arrow);

		auto ret_type = parse_typespec(stream, end, context);

		return make_ts_function(std::move(ret_type), std::move(param_types));
	}

	case lex::token::square_open:
	{
		++stream; // '['

		auto [inner_stream, inner_end] = get_paren_matched_range(stream, end);

		bz::vector<ast::typespec> types = {};
		// not the nicest line... there's a while after the if
		if (inner_stream != inner_end) while (inner_stream != inner_end)
		{
			types.push_back(parse_typespec(inner_stream, inner_end, context));
			if (inner_stream->kind != lex::token::square_close)
			{
				context.assert_token(inner_stream, lex::token::comma);
			}
			else
			{
				break;
			}
		}

		return make_ts_tuple(std::move(types));
	}

	default:
		context.report_error(stream, "expected a type");
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
	ctx::parse_context &context
)
{
	switch (ts.kind())
	{
	case ast::typespec::index<ast::ts_unresolved>:
	{
		auto &unresolved_ts = ts.get<ast::ts_unresolved_ptr>();
		auto stream = unresolved_ts->tokens.begin;
		auto const end = unresolved_ts->tokens.end;
		ts = parse_typespec(stream, end, context);
		break;
	}

	default:
		break;
	}
}

static void add_expr_type(
	ast::typespec &var_type,
	ast::expression const &expr,
	ctx::parse_context &context
)
{
	if (
		var_type.is<ast::ts_reference>()
		&& expr.expr_type.type_kind != ast::expression::lvalue
		&& expr.expr_type.type_kind != ast::expression::lvalue_reference
	)
	{
		context.report_error(
			expr,
			bz::format(
				"cannot bind reference to an {}",
				expr.expr_type.type_kind == ast::expression::rvalue
				? "rvalue" : "rvalue reference"
			)
		);
		return;
	}

	ast::typespec *var_it = [&]() -> ast::typespec * {
		if (var_type.is<ast::ts_reference>())
		{
			return &var_type.get<ast::ts_reference_ptr>()->base;
		}
		else if (var_type.is<ast::ts_constant>())
		{
			return &var_type.get<ast::ts_constant_ptr>()->base;
		}
		else
		{
			return &var_type;
		}
	}();

	ast::typespec const *expr_it = &expr.expr_type.expr_type;

	auto const advance = [](auto &it)
	{
		switch (it->kind())
		{
		case ast::typespec::index<ast::ts_pointer>:
			it = &it->template get<ast::ts_pointer_ptr>()->base;
			break;
		case ast::typespec::index<ast::ts_constant>:
			it = &it->template get<ast::ts_constant_ptr>()->base;
			break;
		default:
			assert(false);
			break;
		}
	};

	if (var_it->kind() == ast::typespec::index<ast::ts_base_type>)
	{
		if (!context.is_convertible(expr.expr_type, var_type))
		{
			context.report_error(
				expr,
				bz::format("cannot convert '{}' to '{}'", expr.expr_type.expr_type, var_type)
			);
		}
		return;
	}

	while (var_it->kind() != ast::typespec::null)
	{
		if (
			var_it->kind() == ast::typespec::index<ast::ts_base_type>
			&& expr_it->kind() == ast::typespec::index<ast::ts_base_type>
		)
		{
			if (
				var_it->get<ast::ts_base_type_ptr>()->info
				!= expr_it->get<ast::ts_base_type_ptr>()->info
			)
			{
				context.report_error(
					expr,
					bz::format("cannot convert '{}' to '{}'", expr.expr_type.expr_type, var_type)
				);
				return;
			}
		}
		else if (
			var_it->kind() == ast::typespec::index<ast::ts_base_type>
			&& expr_it->kind() == ast::typespec::index<ast::ts_base_type>
		)
		{
			context.report_error(
				expr,
				bz::format("cannot convert '{}' to '{}'", expr.expr_type.expr_type, var_type)
			);
			return;
		}
		else if (var_it->kind() == expr_it->kind())
		{
			advance(var_it);
			advance(expr_it);
		}
		else if (var_it->kind() == ast::typespec::index<ast::ts_constant>)
		{
			advance(var_it);
		}
		else
		{
			context.report_error(
				expr,
				bz::format("cannot convert '{}' to '{}'", expr.expr_type.expr_type, var_type)
			);
			return;
		}
	}

	assert(var_it->kind() == ast::typespec::null);
	*var_it = *expr_it;
}

void resolve(
	ast::decl_variable &var_decl,
	ctx::parse_context &context
)
{
	if (var_decl.var_type.kind() != ast::typespec::null)
	{
		resolve(var_decl.var_type, context);
	}
	var_decl.var_type = add_prototype_to_type(var_decl.prototype, var_decl.var_type);
	if (var_decl.init_expr.has_value())
	{
		resolve(*var_decl.init_expr, context);
		add_expr_type(var_decl.var_type, *var_decl.init_expr, context);
		if (ast::is_complete(var_decl.var_type) && !ast::is_instantiable(var_decl.var_type))
		{
			context.report_error(
				var_decl,
				bz::format("type '{}' is not instantiable", var_decl.var_type)
			);
		}
	}
}

void resolve_symbol_helper(
	ast::function_body &func_body,
	ctx::parse_context &context
)
{
	assert(context.scope_decls.size() == 0);
	for (auto &p : func_body.params)
	{
		resolve(p, context);
	}
	// we need to add local variables, so we can use them in the return type
	context.add_scope();
	for (auto &p : func_body.params)
	{
		context.add_local_variable(p);
	}
	resolve(func_body.return_type, context);
	context.remove_scope();
}

void resolve_symbol(
	ast::function_body &func_body,
	ctx::parse_context &context
)
{
	if (context.scope_decls.size() != 0)
	{
		ctx::parse_context inner_context(context.file_id, context.global_ctx);
		inner_context.global_decls = context.global_decls;
		resolve_symbol_helper(func_body, inner_context);
	}
	else
	{
		resolve_symbol_helper(func_body, context);
	}
}

void resolve_helper(
	ast::function_body &func_body,
	ctx::parse_context &context
)
{
	assert(context.scope_decls.size() == 0);
	if (func_body.body.has_value())
	{
		for (auto &p : func_body.params)
		{
			resolve(p, context);
		}
		// functions parameters are added seperately, after all of them have been resolved
		context.add_scope();
		for (auto &p : func_body.params)
		{
			context.add_local_variable(p);
		}
		resolve(func_body.return_type, context);
		for (auto &stmt : *func_body.body)
		{
			resolve(stmt, context);
		}
		context.remove_scope();
	}
	else
	{
		resolve_symbol_helper(func_body, context);
	}
}

void resolve(
	ast::function_body &func_body,
	ctx::parse_context &context
)
{
	if (context.scope_decls.size() != 0)
	{
		ctx::parse_context inner_context(context.file_id, context.global_ctx);
		inner_context.global_decls = context.global_decls;
		resolve_helper(func_body, inner_context);
	}
	else
	{
		resolve_helper(func_body, context);
	}
}

void resolve(
	ast::decl_struct &,
	ctx::parse_context &
)
{
}

void resolve(
	ast::declaration &decl,
	ctx::parse_context &context
)
{
	switch (decl.kind())
	{
	case ast::declaration::index<ast::decl_variable>:
	{
		resolve(*decl.get<ast::decl_variable_ptr>(), context);
		break;
	}
	case ast::declaration::index<ast::decl_function>:
		resolve(decl.get<ast::decl_function_ptr>()->body, context);
		break;
	case ast::declaration::index<ast::decl_operator>:
		resolve(decl.get<ast::decl_operator_ptr>()->body, context);
		break;
	case ast::declaration::index<ast::decl_struct>:
	default:
		break;
	}
}

void resolve(
	ast::statement &stmt,
	ctx::parse_context &context
)
{
	switch (stmt.kind())
	{
	case ast::statement::index<ast::stmt_if>:
	{
		auto &if_stmt = *stmt.get<ast::stmt_if_ptr>();
		resolve(if_stmt.condition, context);
		resolve(if_stmt.then_block, context);
		if (if_stmt.else_block.has_value())
		{
			resolve(*if_stmt.else_block, context);
		}
		break;
	}
	case ast::statement::index<ast::stmt_while>:
	{
		auto &while_stmt = *stmt.get<ast::stmt_while_ptr>();
		resolve(while_stmt.condition, context);
		resolve(while_stmt.while_block, context);
		break;
	}
	case ast::statement::index<ast::stmt_for>:
		assert(false);
		break;
	case ast::statement::index<ast::stmt_return>:
		resolve(stmt.get<ast::stmt_return_ptr>()->expr, context);
		break;
	case ast::statement::index<ast::stmt_no_op>:
		break;
	case ast::statement::index<ast::stmt_compound>:
	{
		auto &comp_stmt = *stmt.get<ast::stmt_compound_ptr>();
		context.add_scope();
		for (auto &s : comp_stmt.statements)
		{
			resolve(s, context);
		}
		context.remove_scope();
		break;
	}
	case ast::statement::index<ast::stmt_expression>:
		resolve(stmt.get<ast::stmt_expression_ptr>()->expr, context);
		break;
	case ast::statement::index<ast::decl_variable>:
	{
		auto &var_decl = *stmt.get<ast::decl_variable_ptr>();
		resolve(var_decl, context);
		context.add_local_variable(var_decl);
		break;
	}
	case ast::statement::index<ast::decl_function>:
	case ast::statement::index<ast::decl_operator>:
	case ast::statement::index<ast::decl_struct>:
	default:
		assert(false);
		break;
	}
}


void resolve(ast::expression &expr, ctx::parse_context &context)
{
	if (expr.is<ast::expr_unresolved>())
	{
		auto unresolved_expr = *expr.get<ast::expr_unresolved_ptr>();
		auto stream = unresolved_expr.expr.begin;
		auto const end = unresolved_expr.expr.end;
		auto new_expr = parse_expression(stream, end, context, precedence{});
		if (stream != end)
		{
			context.report_error(stream, stream, end, "expected ';'");
		}
		expr = std::move(new_expr);
	}
	return;
}
