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

static void resolve(
	ast::decl_variable &var_decl,
	ctx::parse_context &context,
	bool is_func_param = false
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

	if (!is_func_param && var_decl.init_expr.is_null())
	{
		if (
			var_decl.var_type.is<ast::ts_constant>()
			|| var_decl.var_type.is<ast::ts_consteval>()
			|| var_decl.var_type.is<ast::ts_reference>()
		)
		{
			auto const var_type = [&]() -> bz::u8string_view {
				switch (var_decl.var_type.kind())
				{
				case ast::typespec::index<ast::ts_constant>:
					return "'const'";
				case ast::typespec::index<ast::ts_consteval>:
					return "'consteval'";
				case ast::typespec::index<ast::ts_reference>:
					return "reference";
				default:
					bz_assert(false);
					return "";
				}
			}();
			context.report_error(var_decl, bz::format("a {} variable must be initialized", var_type));
			var_decl.var_type.clear();
		}
	}
	else if (var_decl.init_expr.is<ast::unresolved_expression>())
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

static void resolve_symbol_helper(
	ast::function_body &func_body,
	ctx::parse_context &context
)
{
	bz_assert(context.scope_decls.size() == 0);
	bz_assert(func_body.llvm_func == nullptr);

	// reset has_errors
	context._has_errors = false;
	for (auto &p : func_body.params)
	{
		resolve(p, context, true);
	}
	// we need to add local variables, so we can use them in the return type
	context.add_scope();
	for (auto &p : func_body.params)
	{
		context.add_local_variable(p);
	}
	resolve(func_body.return_type, precedence{}, context);

	if (func_body.symbol_name == "")
	{
		bz_assert(func_body.function_name.size() != 0);
		auto const first_char = *func_body.function_name.begin();
		auto const is_op = first_char >= '1' && first_char <= '9';
		auto symbol_name = bz::format("{}.{}", is_op ? "op" : "func", func_body.function_name);
		symbol_name += bz::format("..{}", func_body.params.size());
		for (auto &p : func_body.params)
		{
			symbol_name += '.';
			symbol_name += ast::get_symbol_name_for_type(p.var_type);
		}
		symbol_name += '.';
		symbol_name += ast::get_symbol_name_for_type(func_body.return_type);

		func_body.symbol_name = std::move(symbol_name);
	}

	func_body.llvm_func = context.make_llvm_func_for_symbol(func_body, func_body.symbol_name);
	context.remove_scope();
}

void resolve_symbol(
	ast::function_body &func_body,
	ctx::parse_context &context
)
{
	if (func_body.is_symbol_resolved())
	{
		return;
	}

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

static void resolve_helper(
	ast::function_body &func_body,
	ctx::parse_context &context
)
{
	bz_assert(context.scope_decls.size() == 0);
	if (func_body.body.has_value())
	{
		if (func_body.not_symbol_resolved())
		{
			for (auto &p : func_body.params)
			{
				resolve(p, context, true);
			}
		}
		// functions parameters are added seperately, after all of them have been resolved
		context.add_scope();
		for (auto &p : func_body.params)
		{
			context.add_local_variable(p);
		}
		if (func_body.not_symbol_resolved())
		{
			resolve(func_body.return_type, precedence{}, context);
		}

		if (func_body.not_symbol_resolved() && func_body.symbol_name == "")
		{
			bz_assert(func_body.function_name.size() != 0);
			auto const first_char = *func_body.function_name.begin();
			auto const is_op = first_char >= '1' && first_char <= '9';
			auto symbol_name = bz::format("{}.{}", is_op ? "op" : "func", func_body.function_name);
			symbol_name += bz::format("..{}", func_body.params.size());
			for (auto &p : func_body.params)
			{
				symbol_name += '.';
				symbol_name += ast::get_symbol_name_for_type(p.var_type);
			}
			symbol_name += '.';
			symbol_name += ast::get_symbol_name_for_type(func_body.return_type);

			func_body.symbol_name = std::move(symbol_name);
		}

		if (func_body.not_symbol_resolved())
		{
			func_body.llvm_func = context.make_llvm_func_for_symbol(func_body, func_body.symbol_name);
		}
		else
		{
			bz_assert(([&]() {
				auto const symbol_name = func_body.symbol_name.as_string_view();
				return func_body.llvm_func->getName()
					== llvm::StringRef(symbol_name.data(), symbol_name.size());
			}()));
		}

		for (auto &stmt : *func_body.body)
		{
			resolve(stmt, context);
		}
		context.remove_scope();
	}
	else if (func_body.not_symbol_resolved())
	{
		resolve_symbol_helper(func_body, context);
	}
}

static void resolve(
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

// returns true, if the attribute was consumed
static bool check_attribute(
	ast::statement &stmt,
	ast::attribute const &atr,
	ctx::parse_context &context
)
{
	switch (stmt.kind())
	{
	case ast::statement::index<ast::stmt_if>:
		return false;
	case ast::statement::index<ast::stmt_while>:
		return false;
	case ast::statement::index<ast::stmt_for>:
		return false;
	case ast::statement::index<ast::stmt_return>:
		return false;
	case ast::statement::index<ast::stmt_no_op>:
		return false;
	case ast::statement::index<ast::stmt_compound>:
		return false;
	case ast::statement::index<ast::stmt_static_assert>:
		return false;
	case ast::statement::index<ast::stmt_expression>:
		return false;
	case ast::statement::index<ast::decl_variable>:
		return false;
	case ast::statement::index<ast::decl_function>:
		if (atr.name->value == "extern")
		{
			auto &func_body = stmt.get<ast::decl_function_ptr>()->body;
			if (func_body.body.has_value())
			{
				context.report_error(atr.name, "a functions marked as 'extern' can't have a definition");
				return true;
			}
			if (atr.args.size() != 1)
			{
				context.report_error(atr.name, "'extern' expects exactly one argument");
				return true;
			}
			bz_assert(atr.args[0].is<ast::constant_expression>());
			auto &arg = atr.args[0].get<ast::constant_expression>();
			if (arg.value.kind() != ast::constant_value::string)
			{
				context.report_error(atr.args[0], "argument of 'extern' must be of type 'str'");
				return true;
			}
			auto const extern_name = arg.value.get<ast::constant_value::string>().as_string_view();
			func_body.symbol_name = extern_name;
			if (func_body.llvm_func != nullptr)
			{
				func_body.llvm_func->setName(llvm::StringRef(extern_name.data(), extern_name.size()));
			}
			return true;
		}
		return false;
	case ast::statement::index<ast::decl_operator>:
		if (atr.name->value == "extern")
		{
			auto &func_body = stmt.get<ast::decl_operator_ptr>()->body;
			if (func_body.body.has_value())
			{
				context.report_error(atr.name, "a functions marked as 'extern' can't have a definition");
				return true;
			}
			if (atr.args.size() != 1)
			{
				context.report_error(atr.name, "'extern' expects exactly one argument");
				return true;
			}
			bz_assert(atr.args[0].is<ast::constant_expression>());
			auto &arg = atr.args[0].get<ast::constant_expression>();
			if (arg.value.kind() != ast::constant_value::string)
			{
				context.report_error(atr.args[0], "argument of 'extern' must be of type 'str'");
				return true;
			}
			auto const extern_name = arg.value.get<ast::constant_value::string>().as_string_view();
			func_body.symbol_name = extern_name;
			if (func_body.llvm_func != nullptr)
			{
				func_body.llvm_func->setName(llvm::StringRef(extern_name.data(), extern_name.size()));
			}
			return true;
		}
		return false;
	case ast::statement::index<ast::decl_struct>:
		return false;
	default:
		return false;
	}
}

static void resolve_attributes(
	ast::statement &stmt,
	ctx::parse_context &context
)
{
	auto &attributes = stmt.get_attributes();

	auto const begin = attributes.begin();
	auto const end   = attributes.end();
	for (auto it = begin; it != end; ++it)
	{
		auto &atr = *it;

		// check if it's a duplicate
		bool duplicate = false;
		auto const name = atr.name->value;
		for (auto inner_it = begin; inner_it != it; ++inner_it)
		{
			auto const inner_name = inner_it->name->value;
			if (name == inner_name)
			{
				context.report_error(
					atr.name,
					bz::format("duplicate attribute '{}'", name),
					{ context.make_note(inner_it->name, "previous instance here:") }
				);
				duplicate = true;
				break;
			}
		}
		if (duplicate)
		{
			continue;
		}

		auto [stream, end] = atr.arg_tokens;
		// stream == end is handled in parse_expression_comma_list
		atr.args = parse_expression_comma_list(stream, end, context);
		bool good = true;

		if (stream != end)
		{
			context.report_error({ stream, stream, end }, "expected ',' or closing )");
			good = false;
		}

		for (auto &arg : atr.args)
		{
			if (arg.is_null())
			{
				good = false;
			}
			else if (!arg.is<ast::constant_expression>())
			{
				context.report_error(arg, "argument of attribute must be a constant expression");
				good = false;
			}
		}

		if (good && !check_attribute(stmt, atr, context))
		{
			context.report_warning(atr.name, bz::format("unknown attribute '{}'", atr.name->value));
		}
	}
}


static void resolve_stmt_if(ast::statement &stmt, ctx::parse_context &context)
{
	bz_assert(stmt.kind() == ast::statement::index<ast::stmt_if>);
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
}

static void resolve_stmt_while(ast::statement &stmt, ctx::parse_context &context)
{
	bz_assert(stmt.kind() == ast::statement::index<ast::stmt_while>);
	context.add_scope();
	auto &while_stmt = *stmt.get<ast::stmt_while_ptr>();
	resolve(while_stmt.condition, context);
	resolve(while_stmt.while_block, context);
	context.remove_scope();
}

static void resolve_stmt_for(ast::statement &stmt, ctx::parse_context &context)
{
	bz_assert(stmt.kind() == ast::statement::index<ast::stmt_for>);
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
}

static void resolve_stmt_return(ast::statement &stmt, ctx::parse_context &context)
{
	bz_assert(stmt.kind() == ast::statement::index<ast::stmt_return>);
	auto &ret_expr = stmt.get<ast::stmt_return_ptr>()->expr;
	if (ret_expr.not_null())
	{
		resolve(ret_expr, context);
	}
}

static void resolve_stmt_no_op(ast::statement &, ctx::parse_context &)
{}

static void resolve_stmt_compound(ast::statement &stmt, ctx::parse_context &context)
{
	bz_assert(stmt.kind() == ast::statement::index<ast::stmt_compound>);
	auto &comp_stmt = *stmt.get<ast::stmt_compound_ptr>();
	context.add_scope();
	for (auto &s : comp_stmt.statements)
	{
		resolve(s, context);
	}
	context.remove_scope();
}

static void resolve_stmt_static_assert(ast::statement &stmt, ctx::parse_context &context)
{
	bz_assert(stmt.kind() == ast::statement::index<ast::stmt_static_assert>);
	auto &static_assert_stmt = *stmt.get<ast::stmt_static_assert_ptr>();
	auto const begin = static_assert_stmt.arg_tokens.begin;
	auto const end = static_assert_stmt.arg_tokens.end;
	if (begin == end)
	{
		context.report_error(begin, "static_assert expects 1 or 2 arguments, but got 0");
		return;
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
		return;
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
			return;
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
}

static void resolve_stmt_expression(ast::statement &stmt, ctx::parse_context &context)
{
	bz_assert(stmt.kind() == ast::statement::index<ast::stmt_expression>);
	resolve(stmt.get<ast::stmt_expression_ptr>()->expr, context);
}

static void resolve_decl_variable(ast::statement &stmt, ctx::parse_context &context)
{
	bz_assert(stmt.kind() == ast::statement::index<ast::decl_variable>);
	auto &var_decl = *stmt.get<ast::decl_variable_ptr>();
	resolve(var_decl, context);
	if (context.scope_decls.size() != 0)
	{
		context.add_local_variable(var_decl);
	}
}

static void resolve_decl_function(ast::statement &stmt, ctx::parse_context &context)
{
	bz_assert(stmt.kind() == ast::statement::index<ast::decl_function>);
	auto &func_decl = *stmt.get<ast::decl_function_ptr>();
	if (func_decl.body.symbol_name == "" && func_decl.body.function_name == "main")
	{
		func_decl.body.symbol_name = "main";
	}
	resolve(func_decl.body, context);
	if (context.scope_decls.size() != 0)
	{
		context.add_local_function(func_decl);
	}
}

static void resolve_decl_operator(ast::statement &stmt, ctx::parse_context &context)
{
	bz_assert(stmt.kind() == ast::statement::index<ast::decl_operator>);
	auto &op_decl = *stmt.get<ast::decl_operator_ptr>();
	resolve(op_decl.body, context);
	if (context.scope_decls.size() != 0)
	{
		context.add_local_operator(op_decl);
	}
}

static void resolve_decl_struct(ast::statement &stmt, ctx::parse_context &context)
{
	bz_assert(false);
}


using resolver_fn_t = void (*)(ast::statement &stmt, ctx::parse_context &context);

struct statement_resolver
{
	size_t        kind;
	resolver_fn_t resolve_fn;
};

constexpr auto statement_resolvers = []() {
	std::array result = {
		statement_resolver{ ast::statement::index<ast::stmt_if>,            &resolve_stmt_if            },
		statement_resolver{ ast::statement::index<ast::stmt_while>,         &resolve_stmt_while         },
		statement_resolver{ ast::statement::index<ast::stmt_for>,           &resolve_stmt_for           },
		statement_resolver{ ast::statement::index<ast::stmt_return>,        &resolve_stmt_return        },
		statement_resolver{ ast::statement::index<ast::stmt_no_op>,         &resolve_stmt_no_op         },
		statement_resolver{ ast::statement::index<ast::stmt_compound>,      &resolve_stmt_compound      },
		statement_resolver{ ast::statement::index<ast::stmt_static_assert>, &resolve_stmt_static_assert },
		statement_resolver{ ast::statement::index<ast::stmt_expression>,    &resolve_stmt_expression    },
		statement_resolver{ ast::statement::index<ast::decl_variable>,      &resolve_decl_variable      },
		statement_resolver{ ast::statement::index<ast::decl_function>,      &resolve_decl_function      },
		statement_resolver{ ast::statement::index<ast::decl_operator>,      &resolve_decl_operator      },
		statement_resolver{ ast::statement::index<ast::decl_struct>,        &resolve_decl_struct        },
	};

	constexpr_bubble_sort(
		result,
		[](auto const &lhs, auto const &rhs) {
			return lhs.kind < rhs.kind;
		},
		[](auto &lhs, auto &rhs) {
			auto const tmp = lhs;
			lhs = rhs;
			rhs = tmp;
		}
	);

	return result;
}();

static_assert(statement_resolvers.size() == ast::statement_types::size());

void resolve(ast::statement &stmt, ctx::parse_context &context)
{
	resolve_attributes(stmt, context);

	[&]<size_t ...Ns>(bz::meta::index_sequence<Ns...>) {
		auto const kind = stmt.kind();
		((
			statement_resolvers[Ns].kind == kind
			? (void)(statement_resolvers[Ns].resolve_fn(stmt, context))
			: (void)0
		), ...);
	}(bz::meta::make_index_sequence<statement_resolvers.size()>{});
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
