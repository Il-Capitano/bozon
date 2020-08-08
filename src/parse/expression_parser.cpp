#include "expression_parser.h"
#include "statement_parser.h"
#include "parse_common.h"

namespace parse
{

ast::expression parse_expression_without_semi_colon(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	switch (stream->kind)
	{
	// top level compound expression
	case lex::token::curly_open:
		return parse_compound_expression(stream, end, context);
	// top level if expression
	case lex::token::kw_if:
		return parse_if_expression(stream, end, context);
	default:
		return parse_expression(stream, end, context, precedence{}, true);
	}
}

void consume_semi_colon_at_end_of_expression(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context,
	ast::expression const &expression
)
{
	if (!expression.is_constant_or_dynamic())
	{
		context.assert_token(stream, lex::token::semi_colon);
		return;
	}

	auto &expr = expression.get_expr();
	expr.visit(bz::overload{
		[&](ast::expr_compound const &compound_expr) {
			if (compound_expr.final_expr.src_tokens.begin == nullptr)
			{
				return;
			}

			if (expression.src_tokens.begin->kind == lex::token::curly_open)
			{
				auto dummy_stream = compound_expr.final_expr.src_tokens.end;
				consume_semi_colon_at_end_of_expression(
					dummy_stream, end, context,
					compound_expr.final_expr
				);
			}
			else
			{
				auto dummy_stream = expression.src_tokens.end;
				context.assert_token(dummy_stream, lex::token::semi_colon);
			}
		},
		[&](ast::expr_if const &if_expr) {
			if (expression.src_tokens.begin->kind == lex::token::kw_if)
			{
				auto if_dummy_stream = if_expr.if_block.src_tokens.end;
				consume_semi_colon_at_end_of_expression(
					if_dummy_stream, end, context,
					if_expr.if_block
				);
				auto else_dummy_stream = if_expr.else_block.src_tokens.end;
				if (else_dummy_stream != nullptr)
				{
					consume_semi_colon_at_end_of_expression(
						else_dummy_stream, end, context,
						if_expr.else_block
					);
				}
			}
			else
			{
				auto dummy_stream = expression.src_tokens.end;
				context.assert_token(dummy_stream, lex::token::semi_colon);
			}
		},
		[&](auto const &) {
			context.assert_token(stream, lex::token::semi_colon);
		}
	});
}

ast::expression parse_top_level_expression(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	auto const expr = parse_expression_without_semi_colon(stream, end, context);
	consume_semi_colon_at_end_of_expression(stream, end, context, expr);
	return expr;
}

ast::expression parse_compound_expression(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream->kind == lex::token::curly_open);
	auto const begin = stream;
	++stream; // '{'
	context.add_scope();
	bz::vector<ast::statement> statements;
	while (stream != end && stream->kind != lex::token::curly_close)
	{
		if (!statements.empty() && statements.back().is<ast::stmt_expression>())
		{
			consume_semi_colon_at_end_of_expression(
				stream, end, context,
				statements.back().get<ast::stmt_expression_ptr>()->expr
			);
		}

		if (stream == end || stream->kind == lex::token::curly_close)
		{
			statements.emplace_back();
			break;
		}
		statements.emplace_back(parse_local_statement_without_semi_colon(stream, end, context));
	}
	context.remove_scope();
	if (stream != end && stream->kind == lex::token::curly_close)
	{
		++stream; // '}'
	}
	else
	{
		context.report_paren_match_error(stream, begin);
	}

	if (statements.size() == 0)
	{
		return ast::make_constant_expression(
			{ begin, begin, stream },
			ast::expression_type_kind::none, ast::make_void_typespec(nullptr),
			ast::constant_value(),
			ast::make_expr_compound(std::move(statements), ast::expression())
		);
	}
	else if (statements.back().is_null())
	{
		statements.pop_back();
		return ast::make_dynamic_expression(
			{ begin, begin, stream },
			ast::expression_type_kind::none, ast::make_void_typespec(nullptr),
			ast::make_expr_compound(std::move(statements), ast::expression())
		);
	}
	else if (
		!statements.back().is<ast::stmt_expression>()
		|| statements.back().get<ast::stmt_expression_ptr>()->expr.is_none()
	)
	{
		return ast::make_dynamic_expression(
			{ begin, begin, stream },
			ast::expression_type_kind::none, ast::make_void_typespec(nullptr),
			ast::make_expr_compound(std::move(statements), ast::expression())
		);
	}
	else
	{
		auto expr = std::move(statements.back().get<ast::stmt_expression_ptr>()->expr);
		statements.pop_back();
		if (expr.is<ast::constant_expression>() && statements.size() == 0)
		{
			auto &const_expr = expr.get<ast::constant_expression>();
			auto result_type = const_expr.type;
			auto const result_kind = const_expr.kind;
			auto result_value = const_expr.value;
			return ast::make_constant_expression(
				{ begin, begin, stream },
				result_kind, std::move(result_type),
				std::move(result_value),
				ast::make_expr_compound(std::move(statements), std::move(expr))
			);
		}
		else if (expr.is_constant_or_dynamic())
		{
			auto const [type, kind] = expr.get_expr_type_and_kind();
			auto result_type = type;
			return ast::make_dynamic_expression(
				{ begin, begin, stream },
				kind, std::move(result_type),
				ast::make_expr_compound(std::move(statements), std::move(expr))
			);
		}
		else
		{
			bz_assert(expr.is_null());
			return ast::expression({ begin, begin, stream });
		}
	}
}

ast::expression parse_if_expression(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream != end);
	bz_assert(stream->kind == lex::token::kw_if);
	auto const begin = stream;
	++stream; // 'if'
	auto condition = parse_parenthesized_condition(stream, end, context);
	auto if_block = parse_expression_without_semi_colon(stream, end, context);
	if (!if_block.is_compound_or_if() && stream->kind == lex::token::semi_colon)
	{
		++stream; // ';'
	}
	if (stream != end && stream->kind == lex::token::kw_else)
	{
		++stream; // 'else'
		auto else_block = parse_expression_without_semi_colon(stream, end, context);
		if (!else_block.is_compound_or_if() && stream->kind == lex::token::semi_colon)
		{
			++stream; // ';'
		}
		auto const src_tokens = lex::src_tokens{ begin, begin, stream };
		if (if_block.is_constant_or_dynamic() && else_block.is_constant_or_dynamic())
		{
			auto const [if_type, if_kind] = if_block.get_expr_type_and_kind();
			auto const [else_type, else_kind] = else_block.get_expr_type_and_kind();
			if (ast::remove_const_or_consteval(if_type) != ast::remove_const_or_consteval(else_type))
			{
				context.report_error(
					src_tokens,
					bz::format(
						"mismatched types in if expression, '{}' and '{}'",
						if_type, else_type
					)
				);
				return ast::expression(src_tokens);
			}
			// TODO: this is not clean, nor right when there are rvalue references
			auto const common_kind = std::max(if_kind, else_kind);
			ast::typespec_view const common_type = common_kind == ast::expression_type_kind::lvalue_reference
				? ast::typespec_view(if_type.is<ast::ts_const>() ? if_type : else_type)
				: ast::remove_const_or_consteval(if_type);

			return ast::make_dynamic_expression(
				src_tokens,
				common_kind, common_type,
				ast::make_expr_if(
					std::move(condition),
					std::move(if_block),
					std::move(else_block)
				)
			);
		}
		else
		{
			bz_assert(context.has_errors());
			return ast::expression(src_tokens);
		}
	}
	else
	{
		auto const src_tokens = lex::src_tokens{ begin, begin, stream };
		if (if_block.not_null())
		{
			return ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::none, ast::make_void_typespec(nullptr),
				ast::make_expr_if(std::move(condition), std::move(if_block))
			);
		}
		else
		{
			return ast::expression(src_tokens);
		}
	}
}

static ast::expression parse_array_type(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert((stream - 1)->kind == lex::token::square_open);
	auto const begin_token = stream - 1;
	auto elems = parse_expression_comma_list(stream, end, context);

	bz_assert(stream != end);
	if (stream->kind != lex::token::colon)
	{
		context.report_error(stream, "expected ',' or ':'");
		stream = search_token(lex::token::colon, stream, end);
		bz_assert(stream != end);
	}

	++stream; // ':'
	auto type = parse_expression(stream, end, context, no_comma, true);
	bool good = true;
	if (stream != end)
	{
		good = false;
		if (stream->kind == lex::token::comma)
		{
			context.report_paren_match_error(
				stream, begin_token,
				{ context.make_note(stream, "operator , is not allowed in array element type") }
			);
		}
		else
		{
			context.report_paren_match_error(stream, begin_token);
		}
	}

	bz::vector<uint64_t> sizes = {};
	for (auto &size_expr : elems)
	{
		if (size_expr.is_null())
		{
			continue;
		}
		auto const report_error = [&size_expr, &context, &good](bz::u8string message) {
			good = false;
			context.report_error(size_expr, std::move(message));
		};

		if (!size_expr.is<ast::constant_expression>())
		{
			report_error("array size must be a constant expression");
		}
		else
		{
			auto &size_const_expr = size_expr.get<ast::constant_expression>();
			switch (size_const_expr.value.kind())
			{
			case ast::constant_value::sint:
			{
				auto const size = size_const_expr.value.get<ast::constant_value::sint>();
				if (size <= 0)
				{
					report_error(bz::format(
						"array size must be a positive integer, the given size {} is invalid",
						size
					));
				}
				else
				{
					sizes.push_back(static_cast<uint64_t>(size));
				}
				break;
			}
			case ast::constant_value::uint:
			{
				auto const size = size_const_expr.value.get<ast::constant_value::uint>();
				if (size == 0)
				{
					report_error(bz::format(
						"array size must be a positive integer, the given size {} is invalid",
						size
					));
				}
				else
				{
					sizes.push_back(size);
				}
				break;
			}
			default:
				report_error("array size must be an integer");
				break;
			}
		}
	}

	if (type.is_null())
	{
		good = false;
	}
	else if (!type.is_typename())
	{
		good = false;
		context.report_error(type, "expected a type as the array element type");
	}
	else
	{
		auto &elem_type = type.get_typename();
		if (elem_type.is<ast::ts_const>())
		{
			good = false;
			auto const const_pos = type.src_tokens.pivot != nullptr
				&& type.src_tokens.pivot->kind == lex::token::kw_const
					? type.src_tokens.pivot
					: lex::token_pos(nullptr);
			auto const [const_begin, const_end] = const_pos == nullptr
				? std::make_pair(ctx::char_pos(), ctx::char_pos())
				: std::make_pair(const_pos->src_pos.begin, (const_pos + 1)->src_pos.begin);
			context.report_error(
				type, "array element type cannot be 'const'",
				{}, { context.make_suggestion_before(
					begin_token, const_begin, const_end,
					"const ", "make the array type 'const'"
				) }
			);
		}
		else if (elem_type.is<ast::ts_consteval>())
		{
			good = false;
			auto const consteval_pos = type.src_tokens.pivot != nullptr
				&& type.src_tokens.pivot->kind == lex::token::kw_consteval
					? type.src_tokens.pivot
					: lex::token_pos(nullptr);
			auto const [consteval_begin, consteval_end] = consteval_pos == nullptr
				? std::make_pair(ctx::char_pos(), ctx::char_pos())
				: std::make_pair(consteval_pos->src_pos.begin, (consteval_pos + 1)->src_pos.begin);
			context.report_error(
				type, "array element type cannot be 'consteval'",
				{}, { context.make_suggestion_before(
					begin_token, consteval_begin, consteval_end,
					"consteval ", "make the array type 'consteval'"
				) }
			);
		}
	}

	lex::src_tokens const src_tokens = { begin_token, begin_token, end };
	if (!good)
	{
		return ast::expression(src_tokens);
	}

	return ast::make_constant_expression(
		src_tokens,
		ast::expression_type_kind::type_name,
		ast::make_typename_typespec(nullptr),
		ast::make_array_typespec(src_tokens, std::move(sizes), std::move(type.get_typename())),
		ast::expr_t{}
	);
}

static ast::expression parse_primary_expression(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	if (stream == end)
	{
		context.report_error(stream, "expected a primary expression");
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
		++stream; // 'auto'
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::type_name,
			ast::make_typename_typespec(nullptr),
			ast::make_auto_typespec(auto_pos),
			ast::make_expr_identifier(auto_pos)
		);
	}

	case lex::token::kw_typename:
	{
		auto const typename_pos = stream;
		auto const src_tokens = lex::src_tokens{ typename_pos, typename_pos, typename_pos + 1 };
		++stream; // 'typename'
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::type_name,
			ast::make_typename_typespec(nullptr),
			ast::make_typename_typespec(typename_pos),
			ast::make_expr_identifier(typename_pos)
		);
	}

	case lex::token::paren_open:
	{
		auto const paren_begin = stream;
		++stream;
		auto [inner_stream, inner_end] = get_paren_matched_range(stream, end, context);
		if (stream == end)
		{
			context.parenthesis_suppressed_value = std::min(context.parenthesis_suppressed_value + 1, 2);
		}
		else
		{
			context.parenthesis_suppressed_value = 1;
		}
		auto expr = parse_expression(inner_stream, inner_end, context, precedence{}, true);
		context.parenthesis_suppressed_value = 0;
		if (inner_stream != inner_end && inner_stream->kind != lex::token::paren_close)
		{
			context.report_paren_match_error(inner_stream, paren_begin);
		}
		if (expr.src_tokens.begin != nullptr)
		{
			expr.src_tokens.begin = paren_begin;
			expr.src_tokens.end = stream;
		}
		return expr;
	}

	// tuple or array type
	case lex::token::square_open:
	{
		auto const begin_token = stream;
		++stream; // '['
		auto [inner_stream, inner_end] = get_paren_matched_range(stream, end, context);
		auto const colon_or_end = search_token(lex::token::colon, inner_stream, inner_end);
		if (colon_or_end != inner_end)
		{
			return parse_array_type(inner_stream, inner_end, context);
		}
		else
		{
			auto const elems = parse_expression_comma_list(inner_stream, inner_end, context);
			auto const end_token = stream;
			if (inner_stream != inner_end)
			{
				context.report_paren_match_error(inner_stream, begin_token);
				return ast::expression({ begin_token, begin_token, end_token });
			}
			return context.make_tuple({ begin_token, begin_token, end_token }, std::move(elems));
		}
	}

	case lex::token::curly_open:
		return parse_compound_expression(stream, end, context);
	case lex::token::kw_if:
		return parse_if_expression(stream, end, context);

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
}

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
			auto [inner_stream, inner_end] = get_paren_matched_range(stream, end, context);
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
			auto [inner_stream, inner_end] = get_paren_matched_range(stream, end, context);
			auto args = parse_expression_comma_list(inner_stream, inner_end, context);
			if (inner_stream != inner_end)
			{
				context.report_paren_match_error(inner_stream, op);
			}
			else if (args.size() == 0)
			{
				context.report_error(inner_end, "subscript expression expects at least one index");
			}

			lhs = context.make_subscript_operator_expression(
				{ lhs.get_tokens_begin(), op, stream },
				std::move(lhs), std::move(args)
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

ast::expression parse_expression(
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

bz::vector<ast::expression> parse_expression_comma_list(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz::vector<ast::expression> exprs = {};

	if (stream == end)
	{
		return exprs;
	}

	exprs.emplace_back(parse_expression(stream, end, context, no_comma, true));

	while (stream != end && stream->kind == lex::token::comma)
	{
		++stream; // ','
		exprs.emplace_back(parse_expression(stream, end, context, no_comma, true));
	}

	return exprs;
}

} // namespace parse
