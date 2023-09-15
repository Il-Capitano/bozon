#include "expression_parser.h"
#include "statement_parser.h"
#include "parse_common.h"
#include "global_data.h"
#include "ctx/builtin_operators.h"

namespace parse
{

ast::expression parse_expression_without_semi_colon(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context,
	precedence prec
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
	// top level switch expression
	case lex::token::kw_switch:
		return parse_switch_expression(stream, end, context);
	default:
		return parse_expression(stream, end, context, prec);
	}
}

void consume_semi_colon_at_end_of_expression(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context,
	ast::expression const &expr
)
{
	if (expr.is_constant_or_dynamic())
	{
		expr.get_expr().visit(bz::overload{
			[&](ast::expr_compound const &compound_expr) {
				if (expr.paren_level == 0)
				{
					if (compound_expr.final_expr.src_tokens.begin == nullptr)
					{
						return;
					}
					auto dummy_stream = compound_expr.final_expr.src_tokens.end;
					consume_semi_colon_at_end_of_expression(dummy_stream, end, context, compound_expr.final_expr);
				}
				else
				{
					bz_assert(expr.src_tokens.end == stream);
					context.assert_token(stream, lex::token::semi_colon);
				}
			},
			[&](ast::expr_if const &if_expr) {
				if (expr.paren_level == 0)
				{
					auto if_dummy_stream = if_expr.then_block.src_tokens.end;
					consume_semi_colon_at_end_of_expression(if_dummy_stream, end, context, if_expr.then_block);
					auto else_dummy_stream = if_expr.else_block.src_tokens.end;
					if (else_dummy_stream != nullptr)
					{
						consume_semi_colon_at_end_of_expression(else_dummy_stream, end, context, if_expr.else_block);
					}
				}
				else
				{
					bz_assert(expr.src_tokens.end == stream);
					context.assert_token(stream, lex::token::semi_colon);
				}
			},
			[&](ast::expr_if_consteval const &if_expr) {
				if (expr.paren_level == 0)
				{
					auto if_dummy_stream = if_expr.then_block.src_tokens.end;
					consume_semi_colon_at_end_of_expression(if_dummy_stream, end, context, if_expr.then_block);
					auto else_dummy_stream = if_expr.else_block.src_tokens.end;
					if (else_dummy_stream != nullptr)
					{
						consume_semi_colon_at_end_of_expression(else_dummy_stream, end, context, if_expr.else_block);
					}
				}
				else
				{
					bz_assert(expr.src_tokens.end == stream);
					context.assert_token(stream, lex::token::semi_colon);
				}
			},
			[&](ast::expr_switch const &) {
				// nothing here
			},
			[&](auto const &) {
				context.assert_token(stream, lex::token::semi_colon);
			}
		});
	}
	else if (expr.is_unresolved())
	{
		expr.get_unresolved_expr().visit(bz::overload{
			[&](ast::expr_compound const &compound_expr) {
				if (expr.paren_level == 0)
				{
					if (compound_expr.final_expr.src_tokens.begin == nullptr)
					{
						return;
					}
					auto dummy_stream = compound_expr.final_expr.src_tokens.end;
					consume_semi_colon_at_end_of_expression(dummy_stream, end, context, compound_expr.final_expr);
				}
				else
				{
					bz_assert(expr.src_tokens.end == stream);
					context.assert_token(stream, lex::token::semi_colon);
				}
			},
			[&](ast::expr_if const &if_expr) {
				if (expr.paren_level == 0)
				{
					auto if_dummy_stream = if_expr.then_block.src_tokens.end;
					consume_semi_colon_at_end_of_expression(if_dummy_stream, end, context, if_expr.then_block);
					auto else_dummy_stream = if_expr.else_block.src_tokens.end;
					if (else_dummy_stream != nullptr)
					{
						consume_semi_colon_at_end_of_expression(else_dummy_stream, end, context, if_expr.else_block);
					}
				}
				else
				{
					bz_assert(expr.src_tokens.end == stream);
					context.assert_token(stream, lex::token::semi_colon);
				}
			},
			[&](ast::expr_if_consteval const &if_expr) {
				if (expr.paren_level == 0)
				{
					auto if_dummy_stream = if_expr.then_block.src_tokens.end;
					consume_semi_colon_at_end_of_expression(if_dummy_stream, end, context, if_expr.then_block);
					auto else_dummy_stream = if_expr.else_block.src_tokens.end;
					if (else_dummy_stream != nullptr)
					{
						consume_semi_colon_at_end_of_expression(else_dummy_stream, end, context, if_expr.else_block);
					}
				}
				else
				{
					bz_assert(expr.src_tokens.end == stream);
					context.assert_token(stream, lex::token::semi_colon);
				}
			},
			[&](ast::expr_switch const &) {
				// nothing
			},
			[&](auto const &) {
				context.assert_token(stream, lex::token::semi_colon);
			}
		});
	}
	else
	{
		// nothing
	}
}

ast::expression parse_top_level_expression(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	auto expr = parse_expression_without_semi_colon(stream, end, context, precedence{});
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
	auto const prev_size = context.add_unresolved_scope();
	ast::arena_vector<ast::statement> statements;
	while (stream != end && stream->kind != lex::token::curly_close)
	{
		if (!statements.empty() && statements.back().is<ast::stmt_expression>())
		{
			consume_semi_colon_at_end_of_expression(
				stream, end, context,
				statements.back().get<ast::stmt_expression>().expr
			);
		}

		if (stream == end || stream->kind == lex::token::curly_close)
		{
			statements.emplace_back();
			break;
		}
		statements.emplace_back(parse_local_statement_without_semi_colon(stream, end, context));
	}
	context.remove_unresolved_scope(prev_size);
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
			ast::constant_value_storage::get_void(),
			ast::make_expr_compound(decltype(statements)(), ast::expression())
		);
	}
	else if (statements.back().is_null())
	{
		statements.pop_back();
		return ast::make_unresolved_expression(
			{ begin, begin, stream },
			ast::make_unresolved_expr_compound(std::move(statements), ast::expression())
		);
	}
	else if (!statements.back().is<ast::stmt_expression>())
	{
		return ast::make_unresolved_expression(
			{ begin, begin, stream },
			ast::make_unresolved_expr_compound(std::move(statements), ast::expression())
		);
	}
	else
	{
		auto expr = std::move(statements.back().get<ast::stmt_expression>().expr);
		statements.pop_back();
		return ast::make_unresolved_expression(
			{ begin, begin, stream },
			ast::make_unresolved_expr_compound(std::move(statements), std::move(expr))
		);
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
	auto const is_if_consteval = stream != end && stream->kind == lex::token::kw_consteval;
	if (is_if_consteval)
	{
		++stream; // 'consteval'
	}
	auto condition = parse_parenthesized_condition(stream, end, context);

	auto const prev_unresolved_context = is_if_consteval && context.push_unresolved_context();

	auto then_block = parse_expression_without_semi_colon(stream, end, context, precedence{});
	if (
		stream != end
		&& !then_block.is_special_top_level()
		&& stream->kind == lex::token::semi_colon
		&& (stream + 1) != end
		&& (stream + 1)->kind == lex::token::kw_else
	)
	{
		++stream; // ';'
	}
	ast::expression else_block;
	if (stream != end && stream->kind == lex::token::kw_else)
	{
		++stream; // 'else'
		else_block = parse_expression_without_semi_colon(stream, end, context, no_comma);
		if (!else_block.is_special_top_level() && stream->kind == lex::token::semi_colon)
		{
			++stream; // ';'
		}
	}
	auto const src_tokens = lex::src_tokens{ begin, begin, stream };

	if (is_if_consteval)
	{
		context.pop_unresolved_context(prev_unresolved_context);
	}

	if (else_block.is_null())
	{
		if (then_block.not_error())
		{
			consume_semi_colon_at_end_of_expression(stream, end, context, then_block);
			return ast::make_unresolved_expression(
				src_tokens,
				is_if_consteval
					? ast::make_unresolved_expr_if_consteval(std::move(condition), std::move(then_block))
					: ast::make_unresolved_expr_if(std::move(condition), std::move(then_block))
			);
		}
		else
		{
			return ast::make_error_expression(
				src_tokens,
				is_if_consteval
					? ast::make_expr_if_consteval(std::move(condition), std::move(then_block))
					: ast::make_expr_if(std::move(condition), std::move(then_block))
			);
		}
	}
	else if (then_block.not_error() && else_block.not_error())
	{
		return ast::make_unresolved_expression(
			src_tokens,
			is_if_consteval
				? ast::make_unresolved_expr_if_consteval(std::move(condition), std::move(then_block), std::move(else_block))
				: ast::make_unresolved_expr_if(std::move(condition), std::move(then_block), std::move(else_block))
		);
	}
	else
	{
		bz_assert(context.has_errors());
		return ast::make_error_expression(
			src_tokens,
			is_if_consteval
				? ast::make_expr_if_consteval(std::move(condition), std::move(then_block), std::move(else_block))
				: ast::make_expr_if(std::move(condition), std::move(then_block), std::move(else_block))
		);
	}
}

ast::expression parse_switch_expression(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream != end);
	bz_assert(stream->kind == lex::token::kw_switch);
	auto const begin = stream;
	++stream; // 'switch'
	auto matched_expr = parse_parenthesized_condition(stream, end, context);
	auto const open_curly = context.assert_token(stream, lex::token::curly_open);

	ast::arena_vector<ast::switch_case> cases;
	ast::expression default_case;

	do
	{
		// allow empty switch and trailing commas
		if (stream->kind == lex::token::curly_close)
		{
			break;
		}

		if (stream->kind == lex::token::kw_else)
		{
			++stream; // 'else'
			context.assert_token(stream, lex::token::fat_arrow);
			if (default_case.not_null())
			{
				auto new_default_case = parse_expression(stream, end, context, no_comma);
				context.report_error(
					new_default_case.src_tokens,
					"an else case has already been provided for this switch expression",
					{ context.make_note(default_case.src_tokens, "previous else case was here") }
				);
			}
			else
			{
				default_case = parse_expression(stream, end, context, no_comma);
			}
		}
		else
		{
			ast::arena_vector<ast::expression> case_values;
			auto [case_stream, case_end] = get_expression_tokens<lex::token::curly_close, lex::token::fat_arrow>(
				stream, end, context
			);
			do
			{
				case_values.emplace_back(parse_expression(case_stream, end, context, no_comma));
			} while (case_stream != case_end && case_stream->kind == lex::token::comma && (++case_stream, case_stream != case_end));
			context.assert_token(stream, lex::token::fat_arrow);
			auto case_expr = parse_expression(stream, end, context, no_comma);
			cases.push_back({ std::move(case_values), std::move(case_expr) });
		}
	} while (stream != end && stream->kind == lex::token::comma && (++stream, stream != end));

	if (stream != end && stream->kind == lex::token::curly_close)
	{
		++stream; // '}'
	}
	else if (stream != end && open_curly->kind == lex::token::curly_open)
	{
		context.report_paren_match_error(stream, open_curly);
	}
	else
	{
		context.assert_token(stream, lex::token::curly_close);
	}

	lex::src_tokens src_tokens = { begin, begin, stream };
	return ast::make_unresolved_expression(
		src_tokens,
		ast::make_unresolved_expr_switch(std::move(matched_expr), std::move(default_case), std::move(cases))
	);
}

static ast::expression parse_array_type(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert((stream - 1)->kind == lex::token::square_open);
	auto const begin_token = stream - 1;
	auto sizes = parse_expression_comma_list(stream, end, context);

	bz_assert(stream != end);
	if (stream->kind != lex::token::colon)
	{
		context.report_error(stream, "expected ',' or ':'");
		stream = search_token(lex::token::colon, stream, end);
		bz_assert(stream != end);
	}

	++stream; // ':'
	auto type = parse_expression(stream, end, context, no_comma);
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

	lex::src_tokens const src_tokens = { begin_token, begin_token, end };
	if (good)
	{
		return ast::make_unresolved_expression(
			src_tokens,
			ast::make_unresolved_expr_unresolved_array_type(std::move(sizes), std::move(type))
		);
	}
	else
	{
		return ast::make_error_expression(src_tokens);
	}
}

static ast::expression parse_array_slice_type(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream->kind == lex::token::colon);
	bz_assert((stream - 1)->kind == lex::token::square_open);
	auto const begin_token = stream - 1;
	++stream;
	auto type = parse_expression(stream, end, context, no_comma);
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

	lex::src_tokens const src_tokens = { begin_token, begin_token, end };
	if (good)
	{
		return ast::make_unresolved_expression(
			src_tokens,
			ast::make_unresolved_expr_unresolved_array_type(ast::arena_vector<ast::expression>(), std::move(type))
		);
	}
	else
	{
		return ast::make_error_expression(src_tokens);
	}
}

static ast::expression parse_primary_expression(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	if (stream == end)
	{
		context.report_error(stream, "expected a primary expression");
		return ast::make_error_expression({ stream, stream, stream + 1});
	}

	switch (stream->kind)
	{
	case lex::token::scope:
	case lex::token::identifier:
	{
		auto const begin_token = stream;
		bool const is_qualified = stream->kind == lex::token::scope;
		if (is_qualified)
		{
			++stream; // '::'
			if (stream == end || stream->kind != lex::token::identifier)
			{
				bz_assert(stream == end || stream->kind != lex::token::angle_open);
				// either an identifier or a '<' is expected
				context.assert_token(stream, lex::token::identifier, lex::token::angle_open);
				return ast::make_error_expression({ begin_token, begin_token, stream });
			}
		}
		bz_assert(stream != end && stream->kind == lex::token::identifier);
		++stream; // id
		while (
			stream != end
			&& stream->kind == lex::token::scope
			&& stream + 1 != end
			&& (stream + 1)->kind == lex::token::identifier
		)
		{
			++stream; // '::'
			++stream; // id
		}
		return context.make_identifier_expression(ast::make_identifier({ begin_token, stream }));
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
	case lex::token::placeholder_literal:
	{
		auto const literal = stream;
		++stream;
		return context.make_literal(literal);
	}
	case lex::token::dot:
	{
		auto const dot_pos = stream;
		++stream;
		if (stream != end && stream->kind == lex::token::identifier)
		{
			auto const id = stream;
			++stream;
			return ast::make_constant_expression(
				lex::src_tokens::from_range({ dot_pos, stream }),
				ast::expression_type_kind::enum_literal,
				ast::typespec(),
				ast::constant_value_storage(),
				ast::make_expr_enum_literal(id)
			);
		}
		else
		{
			context.report_error(stream, "expected an identifier after '.'");
			return parse_primary_expression(stream, end, context);
		}
	}
	case lex::token::dot_dot:
	{
		auto const dot_dot_pos = stream;
		++stream;
		if (
			stream == end
			|| (!is_unary_operator(stream->kind) && is_operator(stream->kind))
			|| stream->kind == lex::token::paren_close
			|| stream->kind == lex::token::square_close
			|| stream->kind == lex::token::curly_close
		)
		{
			return context.make_range_unbounded_expression(lex::src_tokens::from_single_token(dot_dot_pos));
		}

		auto range_end = parse_expression(stream, end, context, dot_dot_prec);
		return context.make_integer_range_to_expression({ dot_dot_pos, dot_dot_pos, stream }, std::move(range_end));
	}
	case lex::token::dot_dot_eq:
	{
		auto const dot_dot_eq_pos = stream;
		++stream;
		auto range_end = parse_expression(stream, end, context, dot_dot_prec);
		return context.make_integer_range_to_inclusive_expression({ dot_dot_eq_pos, dot_dot_eq_pos, stream }, std::move(range_end));
	}

	case lex::token::kw_unreachable:
	{
		auto const t = stream;
		++stream;
		return context.make_unreachable(t);
	}
	case lex::token::kw_break:
	{
		auto const t = stream;
		++stream;
		if (!context.is_in_loop())
		{
			context.report_error(t, "'break' is only allowed inside loops");
			return ast::make_error_expression({ t, t, t + 1 }, ast::make_expr_break());
		}
		else
		{
			return ast::make_dynamic_expression(
				lex::src_tokens::from_single_token(t),
				ast::expression_type_kind::noreturn, ast::make_void_typespec(nullptr),
				ast::make_expr_break(),
				ast::destruct_operation()
			);
		}
	}
	case lex::token::kw_continue:
	{
		auto const t = stream;
		++stream;
		if (!context.is_in_loop())
		{
			context.report_error(t, "'continue' is only allowed inside loops");
			return ast::make_error_expression({ t, t, t + 1 }, ast::make_expr_continue());
		}
		else
		{
			return ast::make_dynamic_expression(
				lex::src_tokens::from_single_token(t),
				ast::expression_type_kind::noreturn, ast::make_void_typespec(nullptr),
				ast::make_expr_continue(),
				ast::destruct_operation()
			);
		}
	}

	case lex::token::string_literal:
	case lex::token::raw_string_literal:
	{
		// consecutive string literals are concatenated
		auto const first = stream;
		++stream;
		while (
			stream != end && (stream - 1)->postfix == ""
			&& (stream->kind == lex::token::string_literal || stream->kind == lex::token::raw_string_literal)
		)
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
			ast::constant_value_storage(ast::make_auto_typespec(auto_pos)),
			ast::make_expr_typename_literal()
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
			ast::constant_value_storage(ast::make_typename_typespec(typename_pos)),
			ast::make_expr_typename_literal()
		);
	}

	case lex::token::kw_function:
	{
		auto const begin = stream;
		++stream; // 'function'
		auto const cc_specified = stream->kind == lex::token::string_literal || stream->kind == lex::token::raw_string_literal;
		auto const cc = cc_specified ? get_calling_convention(stream, context) : abi::default_cc;
		if (cc_specified)
		{
			// only a single token may be used for calling convention
			// e.g. `function "" "c" foo` is invalid even though `"" "c" == "c"`
			++stream;
		}

		context.assert_token(stream, lex::token::paren_open);
		auto [inner_stream, inner_end] = get_paren_matched_range(stream, end, context);
		auto param_types = parse_expression_comma_list(inner_stream, inner_end, context);
		if (inner_stream != inner_end)
		{
			context.report_error(inner_stream, "expected ',' or closing )");
		}

		if (stream != end && stream->kind == lex::token::arrow)
		{
			++stream; // '->'
			auto return_type = parse_expression(stream, end, context, no_comma);
			auto const src_tokens = lex::src_tokens{ begin, begin, stream };
			return ast::make_unresolved_expression(
				src_tokens,
				ast::make_unresolved_expr_unresolved_function_type(
					std::move(param_types), std::move(return_type), cc
				)
			);
		}
		else
		{
			auto const src_tokens = lex::src_tokens{ begin, begin, stream };
			auto return_type = ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::type_name,
				ast::make_typename_typespec(nullptr),
				ast::constant_value_storage(ast::make_void_typespec(nullptr)),
				ast::make_expr_typename_literal()
			);
			return ast::make_unresolved_expression(
				src_tokens,
				ast::make_unresolved_expr_unresolved_function_type(
					std::move(param_types), std::move(return_type), cc
				)
			);
		}
	}

	case lex::token::paren_open:
	{
		auto const paren_begin = stream;
		++stream;
		auto [inner_stream, inner_end] = get_paren_matched_range(stream, end, context);
		auto expr = parse_expression(inner_stream, inner_end, context, precedence{});
		expr.paren_level += 1;
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

	// tuple, tuple type or array type or array slice type
	case lex::token::square_open:
	{
		auto const begin_token = stream;
		++stream; // '['
		auto [inner_stream, inner_end] = get_paren_matched_range(stream, end, context);
		// array slice
		if (inner_stream->kind == lex::token::colon)
		{
			return parse_array_slice_type(inner_stream, inner_end, context);
		}
		else if (auto const colon_or_end = search_token(lex::token::colon, inner_stream, inner_end); colon_or_end != inner_end)
		{
			return parse_array_type(inner_stream, inner_end, context);
		}
		else
		{
			auto elems = parse_expression_comma_list(inner_stream, inner_end, context);
			auto const end_token = stream;
			if (inner_stream != inner_end)
			{
				context.report_paren_match_error(inner_stream, begin_token);
				return ast::make_error_expression({ begin_token, begin_token, end_token }, ast::make_expr_tuple(std::move(elems)));
			}
			return context.make_tuple({ begin_token, begin_token, end_token }, std::move(elems));
		}
	}

	case lex::token::curly_open:
		return parse_compound_expression(stream, end, context);
	case lex::token::kw_if:
		return parse_if_expression(stream, end, context);
	case lex::token::kw_switch:
		return parse_switch_expression(stream, end, context);

	// unary operators
	default:
	{
		if (!is_unary_operator(stream->kind))
		{
			context.report_error(stream, "expected a primary expression");
			return ast::make_error_expression({ stream, stream, stream + 1 });
		}

		auto const op = stream;
		if (op->kind == lex::token::dot_dot_dot)
		{
			auto const prec = get_unary_precedence(op->kind);
			++stream;
			auto const prev_parsing_variadic = context.push_parsing_variadic_expansion();
			auto expr = parse_expression(stream, end, context, prec);
			context.pop_parsing_variadic_expansion(prev_parsing_variadic);
			return context.make_unary_operator_expression({ op, op, stream }, op->kind, std::move(expr));
		}
		else if (op->kind == lex::token::kw_consteval)
		{
			// if it's consteval, always return an unresolved expression
			auto const prec = get_unary_precedence(op->kind);
			++stream;
			auto expr = parse_expression(stream, end, context, prec);
			return ast::make_unresolved_expression(
				lex::src_tokens{ op, op, stream },
				ast::make_unresolved_expr_unary_op(op->kind, std::move(expr))
			);
		}
		else if (is_unary_has_unevaluated_context(op->kind))
		{
			auto const prec = get_unary_precedence(op->kind);
			++stream;
			auto const prev_value = context.push_unevaluated_context();
			auto expr = parse_expression(stream, end, context, prec);
			context.pop_unevaluated_context(prev_value);
			return context.make_unary_operator_expression({ op, op, stream }, op->kind, std::move(expr));
		}
		else
		{
			auto const prec = get_unary_precedence(op->kind);
			++stream;
			auto expr = parse_expression(stream, end, context, prec);
			return context.make_unary_operator_expression({ op, op, stream }, op->kind, std::move(expr));
		}
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
		// this is a really messy condition, basically we look for the beginning of a generic type instantiation,
		// e.g. `std::vector<int32>` or `std::vector::<int32>`
		// also if the operator is '>', we have to check whether we are parsing a template argument
		&& (
			(lhs.is_generic_type() && stream->kind == lex::token::angle_open)
			|| (stream->kind == lex::token::scope && stream + 1 != end && (stream + 1)->kind == lex::token::angle_open)
			// not really clean... we assign to both op and op_prec here
			|| (
				(op_prec = get_binary_or_call_precedence((op = stream)->kind)) <= prec
				&& (op->kind != lex::token::angle_close || !context.is_parsing_template_argument())
			)
		)
	)
	{
		if (
			(lhs.is_generic_type() && stream->kind == lex::token::angle_open)
			|| (stream->kind == lex::token::scope && stream + 1 != end && (stream + 1)->kind == lex::token::angle_open)
		)
		{
			auto const angle_open_it = stream->kind == lex::token::scope ? stream + 1 : stream;
			if (stream->kind == lex::token::scope)
			{
				++stream; // '::'
				++stream; // '<'
			}
			else
			{
				++stream; // '<'
			}
			context.push_parsing_template_argument();
			auto args = parse_expression_comma_list(stream, end, context);
			if (context.assert_token(stream, lex::token::angle_close)->kind != lex::token::angle_close)
			{
				context.report_paren_match_error(stream, angle_open_it);
			}
			context.pop_parsing_template_argument();
			lhs = context.make_generic_type_instantiation_expression(
				{ lhs.src_tokens.begin, angle_open_it, stream },
				std::move(lhs), std::move(args)
			);
			continue;
		}

		++stream;

		switch (op->kind)
		{
		case lex::token::arrow:
		{
			auto const src_tokens = lhs.src_tokens;
			lhs = context.make_unary_operator_expression(src_tokens, lex::token::dereference, std::move(lhs));
			[[fallthrough]];
		}
		case lex::token::dot:
		{
			auto id = get_identifier(stream, end, context);
			if (id.values.empty())
			{
				lhs.to_error();
				break;
			}
			else if (!id.is_qualified && id.values.size() == 1 && (stream == end || stream->kind != lex::token::paren_open))
			{
				auto const src_tokens = lex::src_tokens{ lhs.get_tokens_begin(), id.tokens.begin, stream };
				bz_assert(id.tokens.begin->kind == lex::token::identifier);
				lhs = context.make_member_access_expression(src_tokens, std::move(lhs), id.tokens.begin);
				break;
			}
			else
			{
				auto const open_paren = context.assert_token(stream, lex::token::paren_open);
				if (open_paren->kind != lex::token::paren_open)
				{
					lhs.to_error();
					break;
				}
				auto [inner_stream, inner_end] = get_paren_matched_range(stream, end, context);
				auto params = parse_expression_comma_list(inner_stream, inner_end, context);
				if (inner_stream != inner_end)
				{
					context.report_error(inner_stream, "expected ',' or closing )");
				}

				auto const src_tokens = lex::src_tokens{ lhs.get_tokens_begin(), open_paren, stream };
				lhs = context.make_universal_function_call_expression(src_tokens, std::move(lhs), std::move(id), std::move(params));
				break;
			}
		}
		// function call operator
		case lex::token::paren_open:
		{
			auto [inner_stream, inner_end] = get_paren_matched_range(stream, end, context);
			auto params = parse_expression_comma_list(inner_stream, inner_end, context);
			if (inner_stream != inner_end)
			{
				context.report_error(inner_stream, "expected ',' or closing )");
			}

			auto const src_tokens = lex::src_tokens{ lhs.get_tokens_begin(), op, stream };
			lhs = context.make_function_call_expression(src_tokens, std::move(lhs), std::move(params));
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

			auto const src_tokens = lex::src_tokens{ lhs.get_tokens_begin(), op, stream };
			lhs = context.make_subscript_operator_expression(src_tokens, std::move(lhs), std::move(args));
			break;
		}

		// range operator
		case lex::token::dot_dot:
		{
			if (
				stream == end
				|| (!is_unary_operator(stream->kind) && is_operator(stream->kind))
				|| stream->kind == lex::token::paren_close
				|| stream->kind == lex::token::square_close
				|| stream->kind == lex::token::curly_close
			)
			{
				auto const src_tokens = lex::src_tokens{ lhs.src_tokens.begin, op, stream };
				lhs = context.make_integer_range_from_expression(src_tokens, std::move(lhs));
				break;
			}

			auto rhs = parse_primary_expression(stream, end, context);
			precedence rhs_prec;

			while (
				stream != end
				&& (rhs_prec = get_binary_or_call_precedence(stream->kind)) < op_prec
			)
			{
				rhs = parse_expression_helper(std::move(rhs), stream, end, context, rhs_prec);
			}

			auto const src_tokens = lex::src_tokens{ lhs.get_tokens_begin(), op, stream };
			lhs = context.make_integer_range_expression(src_tokens, std::move(lhs), std::move(rhs));
			break;
		}
		case lex::token::dot_dot_eq:
		{
			auto rhs = parse_primary_expression(stream, end, context);
			precedence rhs_prec;

			while (
				stream != end
				&& (rhs_prec = get_binary_or_call_precedence(stream->kind)) < op_prec
			)
			{
				rhs = parse_expression_helper(std::move(rhs), stream, end, context, rhs_prec);
			}

			auto const src_tokens = lex::src_tokens{ lhs.get_tokens_begin(), op, stream };
			lhs = context.make_integer_range_inclusive_expression(src_tokens, std::move(lhs), std::move(rhs));
			break;
		}

		// any other operator
		default:
		{
			auto rhs = parse_primary_expression(stream, end, context);
			precedence rhs_prec;

			while (
				stream != end
				&& (rhs_prec = get_binary_or_call_precedence(stream->kind)) < op_prec
			)
			{
				rhs = parse_expression_helper(std::move(rhs), stream, end, context, rhs_prec);
			}

			auto const src_tokens = lex::src_tokens{ lhs.get_tokens_begin(), op, stream };
			lhs = context.make_binary_operator_expression(src_tokens, op->kind, std::move(lhs), std::move(rhs));
			break;
		}
		}
	}

	return lhs;
}

ast::expression parse_expression(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context,
	precedence prec
)
{
	auto const start_it = stream;
	auto lhs = parse_primary_expression(stream, end, context);
	if (stream != end && stream == start_it)
	{
		bz_assert(lhs.is_error());
		++stream;
		lhs = parse_primary_expression(stream, end, context);
	}
	return parse_expression_helper(std::move(lhs), stream, end, context, prec);
}

ast::arena_vector<ast::expression> parse_expression_comma_list(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	ast::arena_vector<ast::expression> exprs = {};

	if (stream == end)
	{
		return exprs;
	}

	exprs.emplace_back(parse_expression(stream, end, context, no_comma));

	while (stream != end && stream->kind == lex::token::comma)
	{
		++stream; // ','
		// allow trailing comma
		if (stream == end)
		{
			break;
		}
		exprs.emplace_back(parse_expression(stream, end, context, no_comma));
	}

	return exprs;
}

} // namespace parse
