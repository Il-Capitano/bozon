#include "expression_parser.h"
#include "statement_parser.h"
#include "consteval.h"
#include "parse_common.h"
#include "global_data.h"
#include "ctx/builtin_operators.h"

namespace parse
{

static ast::expression parse_expression_impl(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context,
	precedence prec
);

ast::expression parse_expression_without_semi_colon(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	switch (stream->kind)
	{
	// top level compound expression
	case lex::token::curly_open:
	{
		auto result = parse_compound_expression(stream, end, context);
		consteval_guaranteed(result, context);
		return result;
	}
	// top level if expression
	case lex::token::kw_if:
	{
		auto result = parse_if_expression(stream, end, context);
		consteval_guaranteed(result, context);
		return result;
	}
	// top level switch expression
	case lex::token::kw_switch:
	{
		auto result = parse_switch_expression(stream, end, context);
		consteval_guaranteed(result, context);
		return result;
	}
	default:
		// parse_expression already calls consteval_guaranteed
		return parse_expression(stream, end, context, precedence{});
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
			if (expression.src_tokens.begin->kind == lex::token::curly_open)
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
				bz_assert(expression.src_tokens.end == stream);
				context.assert_token(stream, lex::token::semi_colon);
			}
		},
		[&](ast::expr_if const &if_expr) {
			if (expression.src_tokens.begin->kind == lex::token::kw_if)
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
				bz_assert(expression.src_tokens.end == stream);
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
		|| statements.back().get<ast::stmt_expression>().expr.is_none()
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
		auto expr = std::move(statements.back().get<ast::stmt_expression>().expr);
		statements.pop_back();
		if (expr.get_expr_type_and_kind().second == ast::expression_type_kind::tuple)
		{
			auto type = ast::make_auto_typespec(nullptr);
			context.match_expression_to_type(expr, type);
		}
		if (expr.is<ast::constant_expression>() && statements.size() == 0)
		{
			auto &const_expr = expr.get<ast::constant_expression>();
			auto result_type = const_expr.type;
			auto result_kind = const_expr.kind;
			if (result_kind == ast::expression_type_kind::tuple)
			{
				result_kind = ast::expression_type_kind::rvalue;
			}
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
			auto [type, kind] = expr.get_expr_type_and_kind();
			if (kind == ast::expression_type_kind::tuple)
			{
				kind = ast::expression_type_kind::rvalue;
			}
			ast::typespec result_type = type;
			return ast::make_dynamic_expression(
				{ begin, begin, stream },
				kind, std::move(result_type),
				ast::make_expr_compound(std::move(statements), std::move(expr))
			);
		}
		else
		{
			bz_assert(expr.is_error());
			return ast::make_error_expression({ begin, begin, stream }, ast::make_expr_compound(std::move(statements), std::move(expr)));
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

	{
		auto bool_type = ast::make_base_type_typespec({}, context.get_builtin_type_info(ast::type_info::bool_));
		context.match_expression_to_type(condition, bool_type);
	}

	auto then_block = parse_expression_without_semi_colon(stream, end, context);
	if (
		stream != end
		&& !then_block.is_top_level_compound_or_if()
		&& stream->kind == lex::token::semi_colon
		&& (stream + 1) != end
		&& (stream + 1)->kind == lex::token::kw_else
	)
	{
		++stream; // ';'
	}
	if (stream != end && stream->kind == lex::token::kw_else)
	{
		++stream; // 'else'
		auto else_block = parse_expression_without_semi_colon(stream, end, context);
		if (!else_block.is_top_level_compound_or_if() && stream->kind == lex::token::semi_colon)
		{
			++stream; // ';'
		}
		auto const src_tokens = lex::src_tokens{ begin, begin, stream };
		if (then_block.is_noreturn() && else_block.is_noreturn())
		{
			return ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::noreturn,
				ast::make_void_typespec(nullptr),
				ast::make_expr_if(
					std::move(condition),
					std::move(then_block),
					std::move(else_block)
				)
			);
		}
		else if (then_block.is_none() || else_block.is_none())
		{
			return ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::none,
				ast::make_void_typespec(nullptr),
				ast::make_expr_if(
					std::move(condition),
					std::move(then_block),
					std::move(else_block)
				)
			);
		}
		else if (then_block.not_error() && else_block.not_error())
		{
			return ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::if_expr,
				ast::typespec(),
				ast::make_expr_if(
					std::move(condition),
					std::move(then_block),
					std::move(else_block)
				)
			);
		}
		else
		{
			bz_assert(context.has_errors());
			return ast::make_error_expression(
				src_tokens,
				ast::make_expr_if(std::move(condition), std::move(then_block), std::move(else_block))
			);
		}
	}
	else
	{
		auto const src_tokens = lex::src_tokens{ begin, begin, stream };
		if (then_block.not_error())
		{
			consume_semi_colon_at_end_of_expression(stream, end, context, then_block);
			return ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::none,
				ast::make_void_typespec(nullptr),
				ast::make_expr_if(std::move(condition), std::move(then_block))
			);
		}
		else
		{
			return ast::make_error_expression(
				src_tokens,
				ast::make_expr_if(std::move(condition), std::move(then_block))
			);
		}
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
	ast::typespec match_type = ast::remove_const_or_consteval(matched_expr.get_expr_type_and_kind().first);
	if (!match_type.is<ast::ts_base_type>())
	{
		if (do_verbose)
		{
			context.report_error(
				matched_expr, bz::format("invalid type '{}' for switch expression", match_type),
				{ context.make_note("only integral types can be used in switch expressions") }
			);
		}
		else
		{
			context.report_error(matched_expr, bz::format("invalid type '{}' for switch expression", match_type));
		}
		get_tokens_in_curly<>(stream, end, context);
		return ast::make_error_expression(
			{ begin, begin, stream },
			ast::make_expr_switch(std::move(matched_expr), ast::expression(), ast::arena_vector<ast::switch_case>{})
		);
	}
	else if (
		auto const info = match_type.get<ast::ts_base_type>().info;
		!ctx::is_integer_kind(info->kind)
		&& info->kind != ast::type_info::char_
		&& info->kind != ast::type_info::bool_
	)
	{
		if (do_verbose)
		{
			context.report_error(
				matched_expr, bz::format("invalid type '{}' for switch expression", match_type),
				{ context.make_note("only integral types can be used in switch expressions") }
			);
		}
		else
		{
			context.report_error(matched_expr, bz::format("invalid type '{}' for switch expression", match_type));
		}
		get_tokens_in_curly<>(stream, end, context);
		return ast::make_error_expression(
			{ begin, begin, stream },
			ast::make_expr_switch(std::move(matched_expr), ast::expression(), ast::arena_vector<ast::switch_case>{})
		);
	}

	ast::arena_vector<ast::switch_case> cases;
	ast::expression default_case;

	do
	{
		if (stream->kind == lex::token::kw_else)
		{
			++stream; // 'else'
			context.assert_token(stream, lex::token::fat_arrow);
			default_case = parse_expression(stream, end, context, no_comma);
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
			} while (case_stream != case_end && case_stream->kind == lex::token::comma && (++case_stream, true));
			context.assert_token(stream, lex::token::fat_arrow);
			for (auto &value : case_values)
			{
				context.match_expression_to_type(value, match_type);
				consteval_try(value, context);
			}
			auto case_expr = parse_expression(stream, end, context, no_comma);
			cases.push_back({ std::move(case_values), std::move(case_expr) });
		}
	} while (stream != end && stream->kind == lex::token::comma && (++stream, true));

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

	bool const is_good = cases.is_all([](auto const &switch_case) {
		return switch_case.expr.not_error()
			&& switch_case.values.is_all([](auto const &value) {
				return value.not_error();
			});
	});

	bool const is_all_unique = [&]() {
		ast::arena_vector<ast::expression const *> case_values;
		for (auto const &switch_case : cases)
		{
			for (auto const &value : switch_case.values)
			{
				if (value.is_error())
				{
					return true;
				}
				bz_assert(value.is<ast::constant_expression>());
				case_values.push_back(&value);
			}
		}
		for (size_t i = 0; i < case_values.size() - 1; ++i)
		{
			auto const &lhs = *case_values[i];
			auto const &lhs_value = lhs.get<ast::constant_expression>().value;
			for (size_t j = i + 1; j < case_values.size(); ++j)
			{
				auto const &rhs = *case_values[j];
				auto const &rhs_value = rhs.get<ast::constant_expression>().value;
				if (lhs_value == rhs_value)
				{
					switch (rhs_value.kind())
					{
					case ast::constant_value::sint:
						context.report_error(
							rhs,
							bz::format("duplicate value '{}' in switch expression", lhs_value.get<ast::constant_value::sint>()),
							{ context.make_note(lhs, "value previously used here") }
						);
						break;
					case ast::constant_value::uint:
						context.report_error(
							rhs,
							bz::format("duplicate value '{}' in switch expression", lhs_value.get<ast::constant_value::uint>()),
							{ context.make_note(lhs, "value previously used here") }
						);
						break;
					case ast::constant_value::u8char:
						context.report_error(
							rhs,
							bz::format(
								"duplicate value '{}' in switch expression",
								lhs_value.get<ast::constant_value::u8char>()
							),
							{ context.make_note(lhs, "value previously used here") }
						);
						break;
					case ast::constant_value::boolean:
						context.report_error(
							rhs,
							bz::format("duplicate value '{}' in switch expression", lhs_value.get<ast::constant_value::boolean>()),
							{ context.make_note(lhs, "value previously used here") }
						);
						break;
					default:
						bz_unreachable;
					}
					return false;
				}
			}
		}
		return true;
	}();

	lex::src_tokens src_tokens = { begin, begin, stream };
	if (!is_all_unique || !is_good)
	{
		return ast::make_error_expression(
			src_tokens,
			ast::make_expr_switch(std::move(matched_expr), std::move(default_case), std::move(cases))
		);
	}

	auto const case_count = cases.empty() ? 0 :
		cases.transform([](auto const &case_) {
			return case_.values.size();
		}).sum();
	auto const match_type_info = match_type.get<ast::ts_base_type>().info;
	auto const max_case_count = [&]() -> uint64_t {
		switch (match_type_info->kind)
		{
		case ast::type_info::bool_:
			return 2;
		case ast::type_info::int8_:
		case ast::type_info::uint8_:
			return std::numeric_limits<uint8_t>::max();
		case ast::type_info::int16_:
		case ast::type_info::uint16_:
			return std::numeric_limits<uint16_t>::max();
		case ast::type_info::int32_:
		case ast::type_info::uint32_:
		case ast::type_info::char_:
			return std::numeric_limits<uint32_t>::max();
		case ast::type_info::int64_:
		case ast::type_info::uint64_:
			return std::numeric_limits<uint64_t>::max();
		default:
			bz_unreachable;
		}
	}();

	if (case_count < max_case_count && default_case.is_null())
	{
		context.report_warning(
			ctx::warning_kind::non_exhaustive_switch,
			src_tokens,
			"switch expression doesn't cover all possible values and doesn't have an else case"
		);
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::none,
			ast::make_void_typespec(nullptr),
			ast::make_expr_switch(
				std::move(matched_expr),
				std::move(default_case),
				std::move(cases)
			)
		);
	}
	else if (case_count == max_case_count && default_case.not_null())
	{
		context.report_warning(
			ctx::warning_kind::unneeded_else,
			default_case.src_tokens,
			"else case is not needed as all possible values are already covered"
		);
	}

	auto const expr_kind = cases
		.transform([](auto const &case_) {
			bz_assert(case_.expr.is_constant_or_dynamic());
			auto const kind = case_.expr.template is<ast::constant_expression>()
				? case_.expr.template get<ast::constant_expression>().kind
				: case_.expr.template get<ast::dynamic_expression>().kind;
			switch (kind)
			{
			case ast::expression_type_kind::none:
			case ast::expression_type_kind::noreturn:
				return kind;
			default:
				return ast::expression_type_kind::switch_expr;
			}
		})
		.reduce(ast::expression_type_kind::noreturn, [](auto const lhs, auto const rhs) {
			switch (lhs)
			{
			case ast::expression_type_kind::switch_expr:
				return lhs;
			case ast::expression_type_kind::none:
				switch (rhs)
				{
				case ast::expression_type_kind::switch_expr:
				case ast::expression_type_kind::none:
					return rhs;
				default:
					return lhs;
				}
			case ast::expression_type_kind::noreturn:
				return rhs;
			default:
				bz_unreachable;
			}
		});

	if (expr_kind == ast::expression_type_kind::switch_expr)
	{
		return ast::make_dynamic_expression(
			src_tokens,
			expr_kind,
			ast::typespec(),
			ast::make_expr_switch(
				std::move(matched_expr),
				std::move(default_case),
				std::move(cases)
			)
		);
	}
	else
	{
		return ast::make_dynamic_expression(
			src_tokens,
			expr_kind,
			ast::make_void_typespec(nullptr),
			ast::make_expr_switch(
				std::move(matched_expr),
				std::move(default_case),
				std::move(cases)
			)
		);
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

	bz::vector<uint64_t> sizes = {};
	for (auto &size_expr : elems)
	{
		if (size_expr.is_error())
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

	if (type.is_error())
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
		else if (elem_type.is<ast::ts_lvalue_reference>())
		{
			context.report_error(type, "array element type cannot be a reference type");
		}
		else if (elem_type.is<ast::ts_auto_reference>())
		{
			context.report_error(type, "array element type cannot be an auto reference type");
		}
		else if (elem_type.is<ast::ts_auto_reference_const>())
		{
			context.report_error(type, "array element type cannot be an auto reference-const type");
		}
	}

	lex::src_tokens const src_tokens = { begin_token, begin_token, end };
	if (!good)
	{
		return ast::make_error_expression(src_tokens);
	}

	auto result_type = std::move(type.get_typename());
	for (auto const size : sizes.reversed())
	{
		result_type = ast::make_array_typespec(src_tokens, size, std::move(result_type));
	}
	return ast::make_constant_expression(
		src_tokens,
		ast::expression_type_kind::type_name,
		ast::make_typename_typespec(nullptr),
		ast::constant_value(std::move(result_type)),
		ast::expr_t{}
	);
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

	if (type.is_error())
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
		if (elem_type.is<ast::ts_consteval>())
		{
			good = false;
			auto const consteval_pos = type.src_tokens.pivot != nullptr
				&& type.src_tokens.pivot->kind == lex::token::kw_consteval
					? type.src_tokens.pivot
					: lex::token_pos(nullptr);
			auto const [consteval_begin, consteval_end] = consteval_pos == nullptr
				? std::make_pair(ctx::char_pos(), ctx::char_pos())
				: std::make_pair(consteval_pos->src_pos.begin, consteval_pos->src_pos.end);
			context.report_error(
				type, "array slice element type cannot be 'consteval'",
				{}, { context.make_suggestion_before(
					begin_token, ctx::char_pos(), ctx::char_pos(), "consteval ",
					consteval_pos, consteval_begin, consteval_end, "const",
					"make the array slice type 'consteval'"
				) }
			);
		}
	}

	lex::src_tokens const src_tokens = { begin_token, begin_token, end };
	if (!good)
	{
		return ast::make_error_expression(src_tokens);
	}

	return ast::make_constant_expression(
		src_tokens,
		ast::expression_type_kind::type_name,
		ast::make_typename_typespec(nullptr),
		ast::constant_value(ast::make_array_slice_typespec(src_tokens, std::move(type.get_typename()))),
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
		return ast::make_error_expression({ stream, stream, stream + 1});
	}

	switch (stream->kind)
	{
	case lex::token::scope:
	case lex::token::identifier:
	{
		auto const begin_token = stream;
		bool const is_qualified = stream->kind == lex::token::scope;
		bool is_last_scope = !is_qualified;
		while (stream != end)
		{
			if (is_last_scope && stream->kind == lex::token::identifier)
			{
				++stream;
			}
			else if (!is_last_scope && stream->kind == lex::token::scope)
			{
				++stream;
			}
			else
			{
				break;
			}
			is_last_scope = !is_last_scope;
		}
		if (is_last_scope)
		{
			context.assert_token(stream, lex::token::identifier);
			return ast::make_error_expression(
				{ begin_token, begin_token, stream },
				ast::make_expr_identifier(ast::make_identifier({ begin_token, stream }))
			);
		}
		auto const end_token = stream;
		return context.make_identifier_expression(ast::make_identifier({ begin_token, end_token }));
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
	case lex::token::kw_unreachable:
	{
		auto const literal = stream;
		++stream;
		return context.make_literal(literal);
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
			ast::constant_value(ast::make_auto_typespec(auto_pos)),
			ast::make_expr_identifier(ast::make_identifier(auto_pos))
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
			ast::constant_value(ast::make_typename_typespec(typename_pos)),
			ast::make_expr_identifier(ast::make_identifier(typename_pos))
		);
	}

	case lex::token::paren_open:
	{
		auto const paren_begin = stream;
		++stream;
		auto [inner_stream, inner_end] = get_paren_matched_range(stream, end, context);
		auto expr = parse_expression_impl(inner_stream, inner_end, context, precedence{});
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
		if (is_unary_operator(stream->kind))
		{
			auto const op = stream;
			auto const prec = get_unary_precedence(op->kind);
			++stream;
			auto expr = parse_expression_impl(stream, end, context, prec);

			return context.make_unary_operator_expression({ op, op, stream }, op->kind, std::move(expr));
		}
		else
		{
			context.report_error(stream, "expected a primary expression");
			return ast::make_error_expression({ stream, stream, stream + 1 });
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
				auto const src_tokens = lex::src_tokens{ lhs.get_tokens_begin(), op, stream };
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

static ast::expression parse_expression_impl(
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
	auto result = parse_expression_helper(std::move(lhs), stream, end, context, prec);
	return result;
}

ast::expression parse_expression(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context,
	precedence prec
)
{
	auto result = parse_expression_impl(stream, end, context, prec);
	consteval_guaranteed(result, context);
	return result;
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

	exprs.emplace_back(parse_expression(stream, end, context, no_comma));

	while (stream != end && stream->kind == lex::token::comma)
	{
		++stream; // ','
		exprs.emplace_back(parse_expression(stream, end, context, no_comma));
	}

	return exprs;
}

} // namespace parse
