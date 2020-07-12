#include "parser.h"
#include "token_info.h"

static ast::typespec parse_typespec(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

// ================================================================
// ---------------------- expression parsing ----------------------
// ================================================================




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
	bz_assert(paren_level == 0);
	bz_assert(
		(stream - 1)->kind == lex::token::paren_close
		|| (stream - 1)->kind == lex::token::square_close
		|| (stream - 1)->kind == lex::token::curly_close
	);
	return { begin, stream - 1 };
};

static ast::expression parse_expression(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context,
	precedence prec,
	bool set_parenthesis_suppress_value = true
);

static bz::vector<ast::expression> parse_expression_comma_list(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

static ast::expression parse_primary_expression(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	if (stream == end)
	{
		context.report_error(stream, "expected primary expression");
		return ast::expression({ stream, stream, stream + 1});
	}

	switch (stream->kind)
	{
	case lex::token::identifier:
	{
		auto const id = stream;
		++stream;
		return context.make_identifier_expression(id);
	}

	// literals
	case lex::token::integer_literal:
	case lex::token::floating_point_literal:
	case lex::token::hex_literal:
	case lex::token::oct_literal:
	case lex::token::bin_literal:
	case lex::token::character_literal:
	case lex::token::kw_true:
	case lex::token::kw_false:
	case lex::token::kw_null:
	{
		auto const literal = stream;
		++stream;
		return context.make_literal(literal);
	}

	case lex::token::string_literal:
	{
		// consecutive string literals are concatenated
		auto const first = stream;
		++stream;
		while (stream != end && (stream - 1)->postfix == "" && stream->kind == lex::token::string_literal)
		{
			++stream;
		}
		return context.make_string_literal(first, stream);
	}

	case lex::token::paren_open:
	{
		auto const paren_begin = stream;
		++stream;
		auto [inner_stream, inner_end] = get_paren_matched_range(stream, end);
		if (stream == end)
		{
			context.parenthesis_suppressed_value = std::min(context.parenthesis_suppressed_value + 1, 2);
		}
		else
		{
			context.parenthesis_suppressed_value = 1;
		}
		auto expr = parse_expression(inner_stream, inner_end, context, precedence{});
		context.parenthesis_suppressed_value = 0;
		if (inner_stream != inner_end && inner_stream->kind != lex::token::paren_close)
		{
			context.report_error(
				inner_stream, "expected closing )",
				{ ctx::make_note(paren_begin, "to match this:") }
			);
		}
		if (expr.src_tokens.begin != nullptr)
		{
			expr.src_tokens.begin = paren_begin;
			expr.src_tokens.end = stream;
		}
		return expr;
	}

	// tuple
	case lex::token::square_open:
	{
		auto const begin_token = stream;
		++stream;
		auto [inner_stream, inner_end] = get_paren_matched_range(stream, end);
		auto elems = parse_expression_comma_list(inner_stream, inner_end, context);
		if (inner_stream != inner_end)
		{
			context.report_error(
				inner_stream, "expected closing ]",
				{ ctx::make_note(begin_token, "to match this:") }
			);
		}
		auto const end_token = stream;

		return context.make_tuple({ begin_token, begin_token, end_token }, std::move(elems));
	}

	// unary operators
	default:
		if (is_unary_operator(stream->kind))
		{
			auto const op = stream;
			auto const prec = get_unary_precedence(op->kind);
			++stream;
			auto const original_paren_suppress_value = context.parenthesis_suppressed_value;
			context.parenthesis_suppressed_value = 0;
			auto expr = parse_expression(stream, end, context, prec, false);

			context.parenthesis_suppressed_value = original_paren_suppress_value;
			return context.make_unary_operator_expression({ op, op, stream }, op, std::move(expr));
		}
		else
		{
			context.report_error(stream, "expected primary expression");
			return ast::expression({ stream, stream, stream + 1 });
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

	while (
		stream != end
		// not really clean... we assign to both op and op_prec here
		&& (op_prec = get_binary_or_call_precedence((op = stream)->kind)) <= prec
	)
	{
		++stream;

		switch (op->kind)
		{
		// function call operator
		case lex::token::paren_open:
		{
			auto [inner_stream, inner_end] = get_paren_matched_range(stream, end);
			auto params = parse_expression_comma_list(inner_stream, inner_end, context);
			if (inner_stream != inner_end)
			{
				context.report_error(inner_stream, "expected ',' or closing )");
			}

			lhs = context.make_function_call_expression(
				{ lhs.get_tokens_begin(), op, stream },
				std::move(lhs), std::move(params)
			);
			break;
		}

		// subscript operator
		case lex::token::square_open:
		{
			bz_assert(false);
			auto [inner_stream, inner_end] = get_paren_matched_range(stream, end);
			auto rhs = parse_expression(inner_stream, inner_end, context, precedence{});
			if (inner_stream != inner_end)
			{
				context.report_error(inner_stream, "expected closing ]", { ctx::make_note(op, "to match this:") });
			}

			lhs = context.make_binary_operator_expression(
				{ lhs.get_tokens_begin(), op, stream },
				op, std::move(lhs), std::move(rhs)
			);
			break;
		}

		// type cast
		case lex::token::kw_as:
		{
			auto const original_suppress_value = context.parenthesis_suppressed_value;
			context.parenthesis_suppressed_value = 0;
			auto type = parse_typespec(stream, end, context);
			context.parenthesis_suppressed_value = stream == end ? original_suppress_value : 0;
			lhs = context.make_cast_expression(
				{ lhs.get_tokens_begin(), op, stream },
				op, std::move(lhs), std::move(type)
			);
			break;
		}

		// any other operator
		default:
		{
			auto const original_suppress_value = context.parenthesis_suppressed_value;
			context.parenthesis_suppressed_value = 0;
			auto rhs = parse_primary_expression(stream, end, context);
			precedence rhs_prec;

			while (
				stream != end
				&& (rhs_prec = get_binary_or_call_precedence(stream->kind)) < op_prec
			)
			{
				rhs = parse_expression_helper(std::move(rhs), stream, end, context, rhs_prec);
			}

			context.parenthesis_suppressed_value = stream == end ? original_suppress_value : 0;
			lhs = context.make_binary_operator_expression(
				{ lhs.get_tokens_begin(), op, stream },
				op, std::move(lhs), std::move(rhs)
			);
			context.parenthesis_suppressed_value = original_suppress_value;
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

	if (stream == end)
	{
		return exprs;
	}

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
	precedence prec,
	bool set_parenthesis_suppress_value
)
{
	if (set_parenthesis_suppress_value)
	{
		context.parenthesis_suppressed_value = std::max(context.parenthesis_suppressed_value, 1);
	}
	else
	{
		context.parenthesis_suppressed_value = 0;
	}
	auto const original_paren_suppress_value = context.parenthesis_suppressed_value;
	auto lhs = parse_primary_expression(stream, end, context);
	context.parenthesis_suppressed_value = original_paren_suppress_value;
	return parse_expression_helper(std::move(lhs), stream, end, context, prec);
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
			return ast::make_ts_void({ id, id, id + 1 }, id);
		}
		else
		{
			auto const info = context.get_type_info(id->value);
			if (info)
			{
				return ast::make_ts_base_type({ id, id, id + 1 }, id, info);
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
		return ast::make_ts_constant(
			{ stream - 1, stream - 1, stream },
			stream - 1, parse_typespec(stream, end, context)
		);

	case lex::token::star:
		++stream; // '*'
		return ast::make_ts_pointer(
			{ stream - 1, stream - 1, stream },
			stream - 1, parse_typespec(stream, end, context)
		);

	case lex::token::ampersand:
		++stream; // '&'
		return ast::make_ts_reference(
			{ stream - 1, stream - 1, stream },
			stream - 1, parse_typespec(stream, end, context)
		);

	case lex::token::kw_function:
	{
		auto const begin_token = stream;
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
		bz_assert(stream != end);
		context.assert_token(stream, lex::token::paren_close);
		context.assert_token(stream, lex::token::arrow);

		auto ret_type = parse_typespec(stream, end, context);

		auto const end_token = stream;

		return ast::make_ts_function(
			{ begin_token, begin_token, end_token },
			begin_token,
			std::move(ret_type),
			std::move(param_types)
		);
	}

	case lex::token::square_open:
	{
		auto const begin_token = stream;
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

		auto const end_token = inner_end;

		return ast::make_ts_tuple({ begin_token, begin_token, end_token }, begin_token, std::move(types));
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
			bz_assert(false);
			break;
		}
	};

	ast::typespec ret_type;
	ast::typespec *ret_type_it = &ret_type;

	while (proto_it->not_null())
	{
#define x(type)                                                            \
if (proto_it->is<type>() || type_it->is<type>())                           \
{                                                                          \
    ret_type_it->emplace<type##_ptr>(std::make_unique<type>(               \
        type_it->is<type>()                                                \
        ? type_it->get_tokens_pivot()                                      \
        : proto_it->get_tokens_pivot(),                                    \
        ast::typespec())                                                   \
    );                                                                     \
    ret_type_it = &ret_type_it->get<type##_ptr>()->base;                   \
    if (proto_it->kind() == ast::typespec::index<type>) advance(proto_it); \
    if (type_it->kind() == ast::typespec::index<type>) advance(type_it);   \
}

	x(ast::ts_reference)
	else x(ast::ts_constant)
	else x(ast::ts_pointer)
	else
	{
		bz_assert(false);
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

static void resolve(
	ast::type_info &info,
	ctx::parse_context &context
)
{
	// if it's resolved we don't need to do anything
	if ((info.flags & ast::type_info::resolved) != 0)
	{
		return;
	}

	context.add_scope();
	for (auto &member_decl : info.member_decls)
	{
		resolve(member_decl, context);
	}
	context.remove_scope();

	info.flags |= ast::type_info::resolved;
	info.flags |= ast::type_info::instantiable;
}

void resolve(
	ast::decl_variable &var_decl,
	ctx::parse_context &context
)
{
	if (var_decl.var_type.not_null())
	{
		resolve(var_decl.var_type, context);
	}
	var_decl.var_type = add_prototype_to_type(var_decl.prototype, var_decl.var_type);

	// if the variable is a base type we need to resolve it (if it's not already resolved)
	if (
		auto &var_type_without_const = ast::remove_const(var_decl.var_type);
		var_type_without_const.is<ast::ts_base_type>()
	)
	{
		auto &base_t = *var_type_without_const.get<ast::ts_base_type_ptr>();
		resolve(*base_t.info, context);
	}

	if (var_decl.init_expr.is<ast::unresolved_expression>())
	{
		resolve(var_decl.init_expr, context);
		context.match_expression_to_type(var_decl.init_expr, var_decl.var_type);
	}

	if (ast::is_complete(var_decl.var_type) && !ast::is_instantiable(var_decl.var_type))
	{
		context.report_error(
			var_decl,
			bz::format("type '{}' is not instantiable", var_decl.var_type)
		);
	}
}

void resolve_symbol_helper(
	ast::function_body &func_body,
	ctx::parse_context &context
)
{
	bz_assert(context.scope_decls.size() == 0);
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
		ctx::parse_context inner_context(context.global_ctx);
//		inner_context.global_decls = context.global_decls;
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
	bz_assert(context.scope_decls.size() == 0);
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
		ctx::parse_context inner_context(context.global_ctx);
//		inner_context.global_decls = context.global_decls;
		resolve_helper(func_body, inner_context);
	}
	else
	{
		resolve_helper(func_body, context);
	}
}

void resolve(
	ast::decl_struct &struct_decl,
	ctx::parse_context &context
)
{
	resolve(struct_decl.info, context);
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
		resolve(*decl.get<ast::decl_struct_ptr>(), context);
		break;
	default:
		bz_assert(false);
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
		bz_assert(false);
		break;
	case ast::statement::index<ast::stmt_return>:
	{
		auto &ret_expr = stmt.get<ast::stmt_return_ptr>()->expr;
		if (ret_expr.not_null())
		{
			resolve(ret_expr, context);
		}
		break;
	}
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
	{
		// this is a declaration inside a scope
		auto &func_decl = *stmt.get<ast::decl_function_ptr>();
		resolve(func_decl.body, context);
		context.add_local_function(func_decl);
		break;
	}
	case ast::statement::index<ast::decl_operator>:
	{
		// this is a declaration inside a scope
		auto &op_decl = *stmt.get<ast::decl_operator_ptr>();
		resolve(op_decl.body, context);
		context.add_local_operator(op_decl);
		break;
	}
	case ast::statement::index<ast::decl_struct>:
	default:
		bz_assert(false);
		break;
	}
}


void resolve(ast::expression &expr, ctx::parse_context &context)
{
	if (expr.is<ast::unresolved_expression>())
	{
		auto stream = expr.src_tokens.begin;
		bz_assert(stream != nullptr);
		auto const end = expr.src_tokens.end;
		auto new_expr = parse_expression(stream, end, context, precedence{});
		if (stream != end)
		{
			context.report_error(
				{ stream, stream, end }, "expected ';'",
				{}, { ctx::make_suggestion_after(stream - 1, ";", "add ';' here:") }
			);
		}
		expr = std::move(new_expr);
	}
	return;
}
