#include "parser.h"
#include "token_info.h"

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

	case lex::token::kw_auto:
	{
		auto const auto_pos = stream;
		auto const src_tokens = lex::src_tokens{ auto_pos, auto_pos, auto_pos + 1 };
		++stream; // auto
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::type_name,
			ast::typespec(),
			ast::make_ts_auto(src_tokens, auto_pos),
			ast::make_expr_identifier(auto_pos)
		);
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
// -------------------------- resolving ---------------------------
// ================================================================

static ast::typespec add_prototype_to_type(
	lex::token_range prototype_range,
	ast::typespec const &type,
	ctx::parse_context &context
)
{
	ast::expression result_expr = ast::make_constant_expression(
		type.src_tokens,
		ast::expression_type_kind::type_name,
		ast::typespec(),
		type,
		ast::expr_t{}
	);
	for (auto it = prototype_range.end; it != prototype_range.begin;)
	{
		--it;
		auto const src_tokens = result_expr.get_tokens_end() == nullptr
			? lex::src_tokens{ it, it, it + 1 }
			: lex::src_tokens{ it, it, result_expr.get_tokens_end() };
		if (result_expr.get_tokens_end() == nullptr)
		{
			result_expr.src_tokens = src_tokens;
		}
		result_expr = context.make_unary_operator_expression(
			src_tokens, it, std::move(result_expr)
		);
	}

	if (!result_expr.is_typename())
	{
		return ast::typespec(result_expr.src_tokens);
	}
	else
	{
		return std::move(result_expr.get_typename());
	}
}

void resolve(
	ast::typespec &ts,
	precedence prec,
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
		auto expr = parse_expression(stream, end, context, prec);

		if (expr.not_null() && stream != end)
		{
			bz_assert(stream < end);
			if (is_binary_operator(stream->kind))
			{
				bz_assert(stream->kind != lex::token::assign);
				bz_assert(prec.value != precedence{}.value);
				context.report_error(
					{ stream, stream, end },
					"expected ';' or '=' at the end of a type",
					{ context.make_note(
						stream,
						bz::format("operator {} is not allowed in a variable declaration's type", stream->value)
					) }
				);
			}
			else
			{
				context.report_error({ stream, stream, end }, bz::format("unexpected token{}", end - stream == 1 ? "" : "s"));
			}
			expr.clear();
		}

		if (expr.is_null())
		{
			bz_assert(context.has_errors());
			ts = ast::typespec(expr.src_tokens);
			break;
		}
		if (!expr.is<ast::constant_expression>())
		{
			context.report_error(expr, "expected a type");
			ts = ast::typespec(expr.src_tokens);
			break;
		}
		auto &const_expr = expr.get<ast::constant_expression>();
		if (const_expr.kind != ast::expression_type_kind::type_name)
		{
			context.report_error(expr, "expected a type");
			ts = ast::typespec(expr.src_tokens);
			break;
		}
		bz_assert(const_expr.value.kind() == ast::constant_value::type);
		ts = std::move(const_expr.value.get<ast::constant_value::type>());
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
		resolve(var_decl.var_type, no_assign, context);
	}
	var_decl.var_type = add_prototype_to_type(var_decl.prototype_range, var_decl.var_type, context);

	// if the variable is a base type we need to resolve it (if it's not already resolved)
	if (
		auto &var_type_without_const = ast::remove_const_or_consteval(var_decl.var_type);
		var_type_without_const.is<ast::ts_base_type>()
	)
	{
		auto &base_t = *var_type_without_const.get<ast::ts_base_type_ptr>();
		resolve(*base_t.info, context);
	}

	if (var_decl.init_expr.is<ast::unresolved_expression>())
	{
		auto const begin = var_decl.init_expr.src_tokens.begin;
		auto const end   = var_decl.init_expr.src_tokens.end;
		auto stream = begin;
		context.parenthesis_suppressed_value = 0;
		var_decl.init_expr = parse_expression(stream, end, context, no_comma, false);
		if (stream == end)
		{
			context.match_expression_to_type(var_decl.init_expr, var_decl.var_type);
		}
		else if (stream->kind == lex::token::comma)
		{
			var_decl.init_expr.clear();
			var_decl.var_type.clear();
			context.report_error(
				{ stream, stream, end },
				"expected ';' at the end of an expression",
				{ context.make_note(stream, "operator , is not allowed in variable declaration") },
				{ context.make_suggestion_around(
					begin, "(", end - 1, ")",
					"put parenthesis around the expression"
				) }
			);
		}
		else
		{
			var_decl.init_expr.clear();
			var_decl.var_type.clear();
			context.report_error(
				{ stream, stream, end },
				"expected ';' at the end of an expression",
				{},
				{ context.make_suggestion_after(stream - 1, ";", "put ';' here:") }
			);
		}
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
	// if llvm_func is set, the symbol has already been resolved
	if (func_body.llvm_func != nullptr)
	{
		return;
	}

	// reset has_errors
	context._has_errors = false;
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
	resolve(func_body.return_type, precedence{}, context);
	context.remove_scope();

	func_body.llvm_func = context.make_llvm_func_for_symbol(func_body, func_body.symbol_name);
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
		resolve(func_body.return_type, precedence{}, context);

		if (func_body.llvm_func == nullptr)
		{
			func_body.llvm_func = context.make_llvm_func_for_symbol(func_body, func_body.symbol_name);
		}
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
	ast::statement &stmt,
	ctx::parse_context &context
)
{
	for (auto &atr : stmt.get_attributes())
	{
		auto [stream, end] = atr.arg_tokens;
		atr.args = parse_expression_comma_list(stream, end, context);
		if (stream != end)
		{
			context.report_error({ stream, stream, end }, "expected ',' or closing )");
		}
	}

	switch (stmt.kind())
	{
	case ast::statement::index<ast::stmt_if>:
	{
		context.add_scope();
		auto &if_stmt = *stmt.get<ast::stmt_if_ptr>();
		resolve(if_stmt.condition, context);
		resolve(if_stmt.then_block, context);
		context.remove_scope();
		if (if_stmt.else_block.has_value())
		{
			context.add_scope();
			resolve(*if_stmt.else_block, context);
			context.remove_scope();
		}
		break;
	}
	case ast::statement::index<ast::stmt_while>:
	{
		context.add_scope();
		auto &while_stmt = *stmt.get<ast::stmt_while_ptr>();
		resolve(while_stmt.condition, context);
		resolve(while_stmt.while_block, context);
		context.remove_scope();
		break;
	}
	case ast::statement::index<ast::stmt_for>:
	{
		auto &for_stmt = *stmt.get<ast::stmt_for_ptr>();
		context.add_scope();
		if (for_stmt.init.not_null())
		{
			resolve(for_stmt.init, context);
		}
		if (for_stmt.condition.not_null())
		{
			resolve(for_stmt.condition, context);
		}
		if (for_stmt.iteration.not_null())
		{
			resolve(for_stmt.iteration, context);
		}
		resolve(for_stmt.for_block, context);
		context.remove_scope();
		break;
	}
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
	case ast::statement::index<ast::stmt_static_assert>:
	{
		auto &static_assert_stmt = *stmt.get<ast::stmt_static_assert_ptr>();
		auto const begin = static_assert_stmt.arg_tokens.begin;
		auto const end = static_assert_stmt.arg_tokens.end;
		if (begin == end)
		{
			context.report_error(begin, "static_assert expects 1 or 2 arguments, but got 0");
			break;
		}
		auto stream = begin;
		auto args = parse_expression_comma_list(stream, end, context);
		if (stream != end)
		{
			context.report_error({ stream, stream, end }, "expected ',' or closing )");
		}
		if (args.size() != 1 && args.size() != 2)
		{
			context.report_error(
				{ begin, begin, end },
				bz::format("static_assert expects 1 or 2 arguments, but got {}", args.size())
			);
			break;
		}
		else
		{
			auto const verify_condition = [&]() {
				auto &condition = args[0];
				if (condition.is_null())
				{
					bz_assert(context.has_errors());
					return false;
				}
				else if (!condition.is<ast::constant_expression>())
				{
					context.report_error(
						condition,
						"condition for static assertion must be a constant expression"
					);
					return false;
				}
				else
				{
					auto &const_expr = condition.get<ast::constant_expression>();
					if (const_expr.value.kind() != ast::constant_value::boolean)
					{
						context.report_error(
							condition,
							bz::format(
								"condition for static assertion must be of type 'bool', but got '{}'",
								const_expr.type
							)
						);
						return false;
					}
					static_assert_stmt.condition = std::move(condition);
					return true;
				}
			};

			auto const verify_message = [&]() {
				bz_assert(args.size() == 2);
				auto &message = args[1];
				if (message.is_null())
				{
					bz_assert(context.has_errors());
					return false;
				}
				else if (!message.is<ast::constant_expression>())
				{
					context.report_error(
						message,
						"message for static assertion must be a constant expression"
					);
					return false;
				}
				else
				{
					auto &const_expr = message.get<ast::constant_expression>();
					if (const_expr.value.kind() != ast::constant_value::string)
					{
						context.report_error(
							message,
							bz::format(
								"message for static assertion must be of type 'str', but got '{}'",
								const_expr.type
							)
						);
						return false;
					}
					static_assert_stmt.message = std::move(message);
					return true;
				}
			};

			auto const condition_good = verify_condition();
			auto const message_good = args.size() != 2 || verify_message();

			if (!condition_good)
			{
				bz_assert(context.has_errors());
				break;
			}

			auto const condition_value = static_assert_stmt.condition.get<ast::constant_expression>()
				.value.get<ast::constant_value::boolean>();
			if (!condition_value)
			{
				if (args.size() == 1 || !message_good)
				{
					context.report_error(args[0], "static assertion failed");
				}
				else
				{
					auto const message = static_assert_stmt.message.get<ast::constant_expression>()
						.value.get<ast::constant_value::string>().as_string_view();
					context.report_error(args[0], bz::format("static assertion failed, message: {}", message));
				}
			}
		}
		break;
	}
	case ast::statement::index<ast::decl_variable>:
	{
		auto &var_decl = *stmt.get<ast::decl_variable_ptr>();
		resolve(var_decl, context);
		if (context.scope_decls.size() != 0)
		{
			context.add_local_variable(var_decl);
		}
		break;
	}
	case ast::statement::index<ast::decl_function>:
	{
		// this is a declaration inside a scope
		auto &func_decl = *stmt.get<ast::decl_function_ptr>();
		resolve(func_decl.body, context);
		if (context.scope_decls.size() != 0)
		{
			context.add_local_function(func_decl);
		}
		break;
	}
	case ast::statement::index<ast::decl_operator>:
	{
		// this is a declaration inside a scope
		auto &op_decl = *stmt.get<ast::decl_operator_ptr>();
		resolve(op_decl.body, context);
		if (context.scope_decls.size() != 0)
		{
			context.add_local_operator(op_decl);
		}
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
				{ stream, stream, end }, "expected ';' at the end of an expression",
				{}, { context.make_suggestion_after(stream - 1, ";", "add ';' here:") }
			);
		}
		expr = std::move(new_expr);
	}
	return;
}
