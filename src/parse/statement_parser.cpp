#include "statement_parser.h"
#include "ast/expression.h"
#include "ast/typespec.h"
#include "expression_parser.h"
#include "parse_common.h"
#include "token_info.h"
#include "resolve/statement_resolver.h"

namespace parse
{

// parse functions can't be static, because they are referenced in parse_common.h

ast::statement parse_stmt_static_assert(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream != end);
	bz_assert(stream->kind == lex::token::kw_static_assert);
	auto const static_assert_pos = stream;
	++stream; // 'static_assert'
	auto const open_paren = context.assert_token(stream, lex::token::paren_open);
	auto const args = get_expression_tokens_without_error<
		lex::token::paren_close
	>(stream, end, context);
	if (stream != end && stream->kind == lex::token::paren_close)
	{
		++stream; // ')'
	}
	else
	{
		if (open_paren->kind == lex::token::paren_open)
		{
			context.report_paren_match_error(stream, open_paren);
		}
	}
	context.assert_token(stream, lex::token::semi_colon);

	return ast::make_stmt_static_assert(static_assert_pos, args, context.get_current_enclosing_scope());
}

static ast::decl_variable parse_decl_variable_id_and_type(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context,
	bool needs_id = true
)
{
	auto const prototype_begin = stream;
	while (stream != end && is_unary_type_op(stream->kind))
	{
		++stream;
	}
	auto const prototype = lex::token_range{ prototype_begin, stream };

	if (stream != end && stream->kind == lex::token::square_open)
	{
		auto const open_square = stream;
		++stream; // '['
		auto [inner_stream, inner_end] = get_paren_matched_range(stream, end, context);
		ast::arena_vector<ast::decl_variable> tuple_decls;
		if (inner_stream == inner_end)
		{
			return ast::decl_variable(
				{ prototype_begin, open_square, stream },
				prototype,
				std::move(tuple_decls),
				context.get_current_enclosing_scope()
			);
		}
		else
		{
			do
			{
				tuple_decls.emplace_back(parse_decl_variable_id_and_type(inner_stream, inner_end, context, needs_id));
			} while (inner_stream != inner_end && inner_stream->kind == lex::token::comma && (++inner_stream, true));
			return ast::decl_variable(
				{ prototype_begin, open_square, stream },
				prototype,
				std::move(tuple_decls),
				context.get_current_enclosing_scope()
			);
		}
	}
	else
	{
		auto const id = [&]() {
			if (needs_id)
			{
				return context.assert_token(stream, lex::token::identifier);
			}
			else if (stream != end && stream->kind == lex::token::identifier)
			{
				auto const id = stream;
				++stream;
				return id;
			}
			else
			{
				return stream;
			}
		}();

		if (stream == end || stream->kind != lex::token::colon)
		{
			if (!needs_id && id == stream)
			{
				// if no identifier was provided a type must be provided
				// e.g. function foo(a, b,, c) is not allowed
				context.report_error(stream, "expected an identifier or ':'");
			}
			auto result = ast::decl_variable(
				{ prototype_begin, id == end ? prototype_begin : id, stream },
				prototype,
				ast::var_id_and_type(
					id->kind == lex::token::identifier ? ast::make_identifier(id) : ast::identifier{},
					ast::type_as_expression(ast::make_auto_typespec(id))
				),
				context.get_current_enclosing_scope()
			);
			if (id->kind == lex::token::identifier && result.get_id().values.back().starts_with('_'))
			{
				result.flags |= ast::decl_variable::maybe_unused;
			}
			return result;
		}

		++stream; // ':'
		auto const type = get_expression_tokens<
			lex::token::assign,
			lex::token::comma,
			lex::token::paren_close,
			lex::token::square_close
		>(stream, end, context);

		auto result = ast::decl_variable(
			{ prototype_begin, id, stream },
			prototype,
			ast::var_id_and_type(
				id->kind == lex::token::identifier ? ast::make_identifier(id) : ast::identifier{},
				ast::type_as_expression(ast::make_unresolved_typespec(type))
			),
			context.get_current_enclosing_scope()
		);
		if (id->kind == lex::token::identifier && result.get_id().values.back().starts_with('_'))
		{
			result.flags |= ast::decl_variable::maybe_unused;
		}
		return result;
	}
}

template<parse_scope scope>
ast::statement parse_decl_variable(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream != end);
	bz_assert(
		stream->kind == lex::token::kw_let
		|| stream->kind == lex::token::kw_extern
		|| stream->kind == lex::token::kw_const
		|| stream->kind == lex::token::kw_consteval
	);
	auto const begin_token = stream;
	if (stream->kind == lex::token::kw_let)
	{
		++stream;
	}
	bool const is_extern = (scope == parse_scope::global || scope == parse_scope::struct_body) && stream->kind == lex::token::kw_extern;
	if (stream->kind == lex::token::kw_extern)
	{
		if (scope != parse_scope::global && scope != parse_scope::struct_body)
		{
			context.report_error(stream, "'extern' is not allowed for local variables");
		}
		++stream;
	}

	auto result_decl = parse_decl_variable_id_and_type(stream, end, context);
	if (stream != end && stream->kind == lex::token::assign)
	{
		++stream; // '='
		auto const init_expr = get_expression_tokens<>(stream, end, context);
		auto const end_token = stream;
		context.assert_token(stream, lex::token::semi_colon);
		if (is_extern)
		{
			context.report_error(lex::src_tokens::from_range(init_expr), "a variable declared 'extern' cannot have an initializer");
		}
		if constexpr (scope == parse_scope::global || scope == parse_scope::struct_body)
		{
			auto result = ast::statement(ast::make_ast_unique<ast::decl_variable>(std::move(result_decl)));
			bz_assert(result.is<ast::decl_variable>());
			auto &var_decl = result.get<ast::decl_variable>();
			var_decl.flags |= ast::decl_variable::global;
			var_decl.flags |= ast::decl_variable::global_storage;
			if (is_extern)
			{
				var_decl.flags |= ast::decl_variable::external_linkage;
				var_decl.flags |= ast::decl_variable::extern_;
			}
			var_decl.init_expr = ast::make_unresolved_expression({ init_expr.begin, init_expr.begin, init_expr.end });
			auto const pivot = var_decl.src_tokens.pivot >= end_token ? begin_token : var_decl.src_tokens.pivot;
			var_decl.src_tokens = { begin_token, pivot, end_token };
			auto const id_tokens = var_decl.id_and_type.id.tokens;
			bz_assert(id_tokens.end - id_tokens.begin <= 1);
			if constexpr (scope == parse_scope::global)
			{
				if (id_tokens.begin != nullptr)
				{
					var_decl.id_and_type.id = context.make_qualified_identifier(id_tokens.begin);
				}
				else
				{
					var_decl.id_and_type.id.is_qualified = true;
				}
			}
			return result;
		}
		else
		{
			auto result = ast::statement(ast::make_ast_unique<ast::decl_variable>(std::move(result_decl)));
			bz_assert(result.is<ast::decl_variable>());
			auto &var_decl = result.get<ast::decl_variable>();
			auto const [begin, end] = init_expr;
			auto stream = begin;
			var_decl.init_expr = parse_expression(stream, end, context, no_comma);
			if (stream != end && stream->kind == lex::token::comma)
			{
				auto const suggestion_end = (end - 1)->kind == lex::token::semi_colon ? end - 1 : end;
				context.report_error(
					stream,
					"'operator ,' is not allowed in variable initialization expression",
					{}, { context.make_suggestion_around(
						begin, ctx::char_pos(), ctx::char_pos(), "(",
						suggestion_end, ctx::char_pos(), ctx::char_pos(), ")",
						"put parenthesis around the initialization expression"
					) }
				);
			}
			else if (stream != end)
			{
				context.report_error(lex::src_tokens::from_range({ stream, end }));
			}
			auto const pivot = var_decl.src_tokens.pivot >= end_token ? begin_token : var_decl.src_tokens.pivot;
			var_decl.src_tokens = { begin_token, pivot, end_token };
			context.add_unresolved_var_decl(var_decl);
			return result;
		}
	}
	else
	{
		auto const end_token = stream;
		context.assert_token(stream, lex::token::semi_colon);
		if constexpr (scope == parse_scope::global || scope == parse_scope::struct_body)
		{
			auto result = ast::statement(ast::make_ast_unique<ast::decl_variable>(std::move(result_decl)));
			bz_assert(result.is<ast::decl_variable>());
			auto &var_decl = result.get<ast::decl_variable>();
			var_decl.flags |= ast::decl_variable::global;
			var_decl.flags |= ast::decl_variable::global_storage;
			if (is_extern)
			{
				var_decl.flags |= ast::decl_variable::external_linkage;
				var_decl.flags |= ast::decl_variable::extern_;
			}
			auto const pivot = var_decl.src_tokens.pivot >= end_token ? begin_token : var_decl.src_tokens.pivot;
			var_decl.src_tokens = { begin_token, pivot, end_token };
			auto const id_tokens = var_decl.id_and_type.id.tokens;
			bz_assert(id_tokens.end - id_tokens.begin <= 1);
			if constexpr (scope == parse_scope::global)
			{
				if (id_tokens.begin != nullptr)
				{
					var_decl.id_and_type.id = context.make_qualified_identifier(id_tokens.begin);
				}
				else
				{
					var_decl.id_and_type.id.is_qualified = true;
				}
			}
			return result;
		}
		else
		{
			auto result = ast::statement(ast::make_ast_unique<ast::decl_variable>(std::move(result_decl)));
			bz_assert(result.is<ast::decl_variable>());
			auto &var_decl = result.get<ast::decl_variable>();
			auto const pivot = var_decl.src_tokens.pivot >= end_token ? begin_token : var_decl.src_tokens.pivot;
			var_decl.src_tokens = { begin_token, pivot, end_token };
			context.add_unresolved_var_decl(var_decl);
			return result;
		}
	}
}

template ast::statement parse_decl_variable<parse_scope::global>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template ast::statement parse_decl_variable<parse_scope::struct_body>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template ast::statement parse_decl_variable<parse_scope::local>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template<parse_scope scope>
ast::statement parse_decl_type_alias(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream != end);
	bz_assert(stream->kind == lex::token::kw_type);
	auto const begin_token = stream;
	++stream; // type
	auto const id = context.assert_token(stream, lex::token::identifier);
	context.assert_token(stream, lex::token::assign);
	auto const alias_tokens = get_expression_tokens<>(stream, end, context);
	auto const end_token = stream;
	context.assert_token(stream, lex::token::semi_colon);
	if constexpr (scope == parse_scope::global)
	{
		auto result = ast::make_decl_type_alias(
			lex::src_tokens{ begin_token, id, end_token },
			context.make_qualified_identifier(id),
			ast::make_unresolved_expression({ alias_tokens.begin, alias_tokens.begin, alias_tokens.end }),
			context.get_current_enclosing_scope()
		);
		bz_assert(result.is<ast::decl_type_alias>());
		auto &type_alias = result.get<ast::decl_type_alias>();
		type_alias.flags |= ast::decl_type_alias::global;
		return result;
	}
	else if constexpr (scope == parse_scope::struct_body)
	{
		auto result = ast::make_decl_type_alias(
			lex::src_tokens{ begin_token, id, end_token },
			ast::make_identifier(id),
			ast::make_unresolved_expression({ alias_tokens.begin, alias_tokens.begin, alias_tokens.end }),
			context.get_current_enclosing_scope()
		);
		bz_assert(result.is<ast::decl_type_alias>());
		auto &type_alias = result.get<ast::decl_type_alias>();
		type_alias.flags |= ast::decl_type_alias::global;
		return result;
	}
	else
	{
		auto result = ast::make_decl_type_alias(
			lex::src_tokens{ begin_token, id, end_token },
			ast::make_identifier(id),
			ast::make_unresolved_expression({ alias_tokens.begin, alias_tokens.begin, alias_tokens.end }),
			context.get_current_enclosing_scope()
		);
		bz_assert(result.is<ast::decl_type_alias>());
		auto &type_alias = result.get<ast::decl_type_alias>();
		context.add_unresolved_local(type_alias.id);
		return result;
	}
}

template ast::statement parse_decl_type_alias<parse_scope::global>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template ast::statement parse_decl_type_alias<parse_scope::struct_body>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template ast::statement parse_decl_type_alias<parse_scope::local>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

static ast::arena_vector<ast::decl_variable> parse_parameter_list(
	lex::token_range param_range,
	ctx::parse_context &context,
	uint32_t expected_end_kind
)
{
	ast::arena_vector<ast::decl_variable> result;
	auto stream = param_range.begin;
	auto const end = param_range.end;
	while (stream != end)
	{
		auto const begin = stream;
		result.emplace_back(parse_decl_variable_id_and_type(
			stream, end,
			context, false
		));
		auto &param_decl = result.back();
		param_decl.flags |= ast::decl_variable::parameter;
		if (param_decl.get_id().values.empty())
		{
			param_decl.flags |= ast::decl_variable::maybe_unused;
		}
		if (stream != end)
		{
			// stream can never be lex::token::paren_close, but the error message is nicer this way
			context.assert_token(stream, lex::token::comma, expected_end_kind);
		}
		if (stream == begin)
		{
			context.report_error(stream);
			++stream;
		}
	}

	return result;
}

static ast::function_body parse_function_body(
	lex::src_tokens const &src_tokens,
	bz::variant<ast::identifier, uint32_t> func_name_or_op_kind,
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	ast::function_body result = {};
	result.scope = ast::make_local_scope(context.get_current_enclosing_scope(), false);
	auto const open_paren = context.assert_token(stream, lex::token::paren_open);
	auto const param_range = get_expression_tokens<
		lex::token::paren_close
	>(stream, end, context);

	if (param_range.end != end && param_range.end->kind == lex::token::paren_close)
	{
		++stream; // ')'
	}
	else if (open_paren->kind == lex::token::paren_open)
	{
		context.report_paren_match_error(param_range.end, open_paren);
	}

	result.src_tokens = src_tokens;
	result.function_name_or_operator_kind = std::move(func_name_or_op_kind);
	result.params = parse_parameter_list(param_range, context, lex::token::paren_close);

	if (stream != end && stream->kind == lex::token::arrow)
	{
		++stream; // '->'
		auto const ret_type = get_expression_tokens<
			lex::token::curly_open
		>(stream, end, context);
		result.return_type = ast::make_unresolved_typespec(ret_type);
	}
	else if (stream != end && stream->kind == lex::token::assign)
	{
		++stream; // '='
		auto const delete_or_default = context.assert_token(stream, lex::token::kw_default, lex::token::kw_delete);
		if (delete_or_default->kind == lex::token::kw_default)
		{
			result.flags |= ast::function_body::defaulted;
		}
		else if (delete_or_default->kind == lex::token::kw_delete)
		{
			result.flags |= ast::function_body::deleted;
		}
		context.assert_token(stream, lex::token::semi_colon);
		return result;
	}
	else if (stream != end)
	{
		result.return_type = ast::make_void_typespec(stream);
	}

	if (stream != end && stream->kind == lex::token::curly_open)
	{
		++stream; // '{'
		auto const body_tokens = get_tokens_in_curly<>(stream, end, context);
		result.body = body_tokens;
	}
	else if (stream == end || stream->kind != lex::token::semi_colon)
	{
		for (auto &var_decl : result.params)
		{
			var_decl.flags |= ast::decl_variable::used;
		}
		context.assert_token(stream, lex::token::curly_open, lex::token::semi_colon);
	}
	else
	{
		for (auto &var_decl : result.params)
		{
			var_decl.flags |= ast::decl_variable::used;
		}
		++stream; // ';'
		result.flags |= ast::function_body::external_linkage;
	}

	return result;
}

template<parse_scope scope>
ast::statement parse_decl_function_or_alias(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream != end);
	bz_assert(stream->kind == lex::token::kw_function || stream->kind == lex::token::kw_consteval);
	auto const begin = stream;
	if (stream->kind == lex::token::kw_consteval)
	{
		++stream;
		bz_assert(stream != end);
		bz_assert(stream->kind == lex::token::kw_function);
	}
	++stream; // 'function'
	auto const cc_specified = stream->kind == lex::token::string_literal || stream->kind == lex::token::raw_string_literal;
	auto const cc_token = stream;
	auto const cc = cc_specified ? get_calling_convention(stream, context) : abi::default_cc;
	if (cc_specified)
	{
		// only a single token may be used for calling convention
		// e.g. `function "" "c" foo` is invalid even though `"" "c" == "c"`
		++stream;
	}
	auto const id = context.assert_token(stream, lex::token::identifier);
	auto const src_tokens = id->kind == lex::token::identifier
		? lex::src_tokens{ begin, id, stream }
		: lex::src_tokens{ begin, begin, stream };
	if (stream->kind == lex::token::assign)
	{
		++stream; // '='
		auto const alias_expr = get_expression_tokens<>(stream, end, context);
		context.assert_token(stream, lex::token::semi_colon);
		auto func_id = scope == parse_scope::global
			? context.make_qualified_identifier(id)
			: ast::make_identifier(id);
		if (begin->kind == lex::token::kw_consteval)
		{
			context.report_error(begin, "a function alias cannot be declared as 'consteval'");
		}
		if (cc_specified)
		{
			context.report_error(cc_token, "calling convention cannot be specified for a function alias");
		}
		return ast::make_decl_function_alias(
			src_tokens,
			std::move(func_id),
			ast::make_unresolved_expression({ alias_expr.begin, alias_expr.begin, alias_expr.end }),
			context.get_current_enclosing_scope()
		);
	}
	else
	{
		auto const func_name = scope == parse_scope::global
			? context.make_qualified_identifier(id)
			: ast::make_identifier(id);
		auto body = parse_function_body(src_tokens, std::move(func_name), stream, end, context);
		body.cc = cc;
		if (scope == parse_scope::global && id->value == "main")
		{
			body.flags |= ast::function_body::main;
			body.flags |= ast::function_body::external_linkage;
		}
		if (begin->kind == lex::token::kw_consteval)
		{
			body.flags |= ast::function_body::only_consteval;
		}

		if constexpr (scope == parse_scope::global || scope == parse_scope::struct_body)
		{
			auto result = ast::make_decl_function(context.make_qualified_identifier(id), std::move(body));
			return result;
		}
		else
		{
			auto result = ast::make_decl_function(ast::make_identifier(id), std::move(body));
			bz_assert(result.is<ast::decl_function>());
			auto &func_decl = result.get<ast::decl_function>();
			func_decl.body.flags |= ast::function_body::local;
			context.add_unresolved_local(func_name);
			return result;
		}
	}
}

template ast::statement parse_decl_function_or_alias<parse_scope::global>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template ast::statement parse_decl_function_or_alias<parse_scope::struct_body>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template ast::statement parse_decl_function_or_alias<parse_scope::local>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template<parse_scope scope>
ast::statement parse_consteval_decl(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream != end);
	bz_assert(stream->kind == lex::token::kw_consteval);
	if (stream + 1 == end)
	{
		return parse_decl_variable<scope>(stream, end, context);
	}
	auto const next_token_kind = (stream + 1)->kind;
	if (next_token_kind == lex::token::kw_function)
	{
		return parse_decl_function_or_alias<scope>(stream, end, context);
	}
	else
	{
		return parse_decl_variable<scope>(stream, end, context);
	}
}

template ast::statement parse_consteval_decl<parse_scope::global>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template ast::statement parse_consteval_decl<parse_scope::struct_body>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template ast::statement parse_consteval_decl<parse_scope::local>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template<parse_scope scope>
ast::statement parse_decl_operator(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream != end);
	bz_assert(stream->kind == lex::token::kw_operator);
	auto const begin = stream;
	++stream; // 'operator'
	auto const op = stream;
	if (!is_overloadable_operator(op->kind))
	{
		context.report_error(
			op,
			is_operator(op->kind)
			? bz::format("'operator {}' is not overloadable", op->value)
			: bz::u8string("expected an overloadable operator")
		);
	}
	else
	{
		++stream;
	}

	if (op->kind == lex::token::paren_open)
	{
		context.assert_token(stream, lex::token::paren_close);
	}
	else if (op->kind == lex::token::square_open)
	{
		context.assert_token(stream, lex::token::square_close);
	}

	auto const src_tokens = is_operator(op->kind)
		? lex::src_tokens{ begin, op, stream }
		: lex::src_tokens{ begin, begin, begin + 1 };

	auto body = parse_function_body(src_tokens, op->kind, stream, end, context);

	if constexpr (scope == parse_scope::global || scope == parse_scope::struct_body)
	{
		return ast::make_decl_operator(context.get_current_enclosing_id_scope(), op, std::move(body));
	}
	else
	{
		auto result = ast::make_decl_operator(context.get_current_enclosing_id_scope(), op, std::move(body));
		bz_assert(result.is<ast::decl_operator>());
		auto &op_decl = result.get<ast::decl_operator>();
		op_decl.body.flags |= ast::function_body::local;
		resolve::resolve_function(result, op_decl.body, context);
		if (op_decl.body.state != ast::resolve_state::error)
		{
			context.add_local_operator(op_decl);
		}
		return result;
	}
}

template ast::statement parse_decl_operator<parse_scope::global>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template ast::statement parse_decl_operator<parse_scope::struct_body>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template ast::statement parse_decl_operator<parse_scope::local>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

static ast::statement parse_type_info_destructor(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream->kind == lex::token::identifier && stream->value == "destructor");
	auto const begin_token = stream;
	++stream; // 'destructor'

	auto result = ast::make_decl_function(
		ast::identifier{},
		parse_function_body({ begin_token, begin_token, begin_token + 1 }, {}, stream, end, context)
	);
	auto &body = result.get<ast::decl_function>().body;
	body.flags |= ast::function_body::destructor;
	return result;
}

static ast::statement parse_type_info_constructor(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream->kind == lex::token::identifier && stream->value == "constructor");
	auto const begin_token = stream;
	++stream; // 'constructor'

	auto result = ast::make_decl_function(
		ast::identifier{},
		parse_function_body({ begin_token, begin_token, begin_token + 1 }, {}, stream, end, context)
	);
	auto &body = result.get<ast::decl_function>().body;
	body.flags |= ast::function_body::constructor;
	return result;
}

static ast::statement parse_type_info_member_variable(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	auto const begin_token = stream;
	if (stream->kind != lex::token::dot)
	{
		if (
			stream->kind == lex::token::identifier
			&& stream + 1 != end && (stream + 1)->kind == lex::token::colon
		)
		{
			context.report_error(
				stream, "expected '.'",
				{}, { context.make_suggestion_before(stream, ".", "add '.' here") }
			);
		}
		else
		{
			context.assert_token(stream, lex::token::dot);
			++stream;
			return ast::statement();
		}
	}
	else
	{
		++stream; // '.'
	}

	bz_assert(stream != end);
	if (stream->kind != lex::token::identifier)
	{
		context.assert_token(stream, lex::token::identifier);
		return ast::statement();
	}
	auto result = ast::statement(ast::make_ast_unique<ast::decl_variable>(parse_decl_variable_id_and_type(stream, end, context)));
	context.assert_token(stream, lex::token::semi_colon);
	auto &var_decl = result.get<ast::decl_variable>();
	var_decl.flags |= ast::decl_variable::member;
	var_decl.src_tokens.begin = begin_token;
	return result;
}

ast::statement default_parse_type_info_statement(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream != end);
	if (stream->kind == lex::token::identifier && stream->value == "destructor")
	{
		return parse_type_info_destructor(stream, end, context);
	}
	else if (stream->kind == lex::token::identifier && stream->value == "constructor")
	{
		return parse_type_info_constructor(stream, end, context);
	}
	else
	{
		return parse_type_info_member_variable(stream, end, context);
	}
}

static ast::statement parse_decl_struct_impl(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream->kind == lex::token::kw_struct);
	auto const begin_token = stream;
	++stream;
	bz_assert(stream != end || stream->kind != lex::token::identifier);
	auto const id = context.assert_token(stream, lex::token::identifier);
	auto const src_tokens = lex::src_tokens{ begin_token, (id == stream ? begin_token : id), stream };

	lex::token_range generic_param_range = {};
	ast::arena_vector<ast::decl_variable> generic_params;

	if (stream != end && stream->kind == lex::token::angle_open)
	{
		auto const open_angle = stream;
		++stream; // '<'
		generic_param_range = get_expression_tokens<lex::token::angle_close>(stream, end, context);
		if (generic_param_range.begin == generic_param_range.end)
		{
			context.report_error(stream, "expected generic type parameters");
		}
		if (generic_param_range.end != end && generic_param_range.end->kind == lex::token::angle_close)
		{
			++stream; // ')'
		}
		else if (open_angle->kind == lex::token::angle_open)
		{
			context.report_paren_match_error(generic_param_range.end, open_angle);
		}
		generic_params = parse_parameter_list(generic_param_range, context, lex::token::angle_close);
	}

	if (stream != end && stream->kind == lex::token::curly_open)
	{
		++stream; // '{'
		auto const range = get_tokens_in_curly<>(stream, end, context);
		if (generic_params.not_empty())
		{
			return ast::make_decl_struct(
				src_tokens,
				context.make_qualified_identifier(id),
				range,
				std::move(generic_params),
				context.get_current_enclosing_scope()
			);
		}
		else
		{
			return ast::make_decl_struct(
				src_tokens,
				context.make_qualified_identifier(id),
				range,
				context.get_current_enclosing_scope()
			);
		}
	}
	else if (stream == end || stream->kind != lex::token::semi_colon)
	{
		context.assert_token(stream, lex::token::curly_open, lex::token::semi_colon);
		return {};
	}
	else
	{
		if (generic_params.not_empty())
		{
			context.report_error(stream, "a generic type must have a body");
		}
		++stream; // ';'
		return ast::make_decl_struct(
			src_tokens,
			context.make_qualified_identifier(id),
			context.get_current_enclosing_scope()
		);
	}
}

template<parse_scope scope>
ast::statement parse_decl_struct(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	auto result = parse_decl_struct_impl(stream, end, context);
	if (result.is_null())
	{
		return result;
	}

	if constexpr (scope == parse_scope::global || scope == parse_scope::struct_body)
	{
		return result;
	}
	else
	{
		bz_assert(result.is<ast::decl_struct>());
		auto &struct_decl = result.get<ast::decl_struct>();
		context.add_to_resolve_queue({}, struct_decl.info);
		resolve::resolve_type_info(struct_decl.info, context);
		context.pop_resolve_queue();
		return result;
	}
}

template ast::statement parse_decl_struct<parse_scope::global>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template ast::statement parse_decl_struct<parse_scope::struct_body>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

static ast::decl_enum::name_value_pair parse_enum_name_value_pair(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream != end);
	auto const id = context.assert_token(stream, lex::token::identifier);
	if (id->kind != lex::token::identifier)
	{
		return {};
	}

	if (stream == end || stream->kind != lex::token::assign)
	{
		return ast::decl_enum::name_value_pair{
			id, {}, ast::expression()
		};
	}

	++stream; // '='
	auto value_expr = parse_expression(stream, end, context, no_comma);

	return ast::decl_enum::name_value_pair{
		id, {}, std::move(value_expr)
	};
}

static ast::statement parse_decl_enum_impl(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream->kind == lex::token::kw_enum);
	auto const begin_token = stream;
	++stream; // 'enum'
	bz_assert(stream != end || stream->kind != lex::token::identifier);
	auto const id = context.assert_token(stream, lex::token::identifier);
	auto const src_tokens = lex::src_tokens{ begin_token, (id == stream ? begin_token : id), stream };

	ast::typespec underlying_type;
	if (stream != end && stream->kind == lex::token::colon)
	{
		++stream;
		auto const underlying_type_tokens = get_expression_tokens<
			lex::token::curly_open
		>(stream, end, context);
		underlying_type = ast::make_unresolved_typespec(underlying_type_tokens);
	}

	if (stream != end && stream->kind == lex::token::curly_open)
	{
		++stream; // '{'
		auto const range = get_tokens_in_curly<>(stream, end, context);

		auto inner_stream = range.begin;
		auto const inner_end = range.end;

		auto values = ast::arena_vector<ast::decl_enum::name_value_pair>();

		while (inner_stream != inner_end)
		{
			values.push_back(parse_enum_name_value_pair(inner_stream, inner_end, context));
			if (values.back().id == nullptr)
			{
				values.pop_back();
				break;
			}

			auto const duplicate_it = std::find_if(
				values.begin(), values.end() - 1,
				[current_id = values.back().id->value](auto const &value) { return value.id->value == current_id; }
			);
			if (duplicate_it != values.end() - 1)
			{
				context.report_error(
					values.back().id,
					bz::format("duplicate enum member name '{}'", duplicate_it->id->value),
					{ context.make_note(duplicate_it->id, "member was previously defined here") }
				);
				values.pop_back();
			}

			if (inner_stream != inner_end)
			{
				context.assert_token(inner_stream, lex::token::comma);
			}
		}

		return ast::make_decl_enum(
			src_tokens,
			context.make_qualified_identifier(id),
			std::move(underlying_type),
			std::move(values),
			context.get_current_enclosing_scope()
		);
	}
	else
	{
		context.report_error(stream, "an enum must have a body");
		return ast::make_decl_enum(
			src_tokens,
			context.make_qualified_identifier(id),
			std::move(underlying_type),
			ast::arena_vector<ast::decl_enum::name_value_pair>(),
			context.get_current_enclosing_scope()
		);
	}
}

template<parse_scope scope>
ast::statement parse_decl_enum(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	auto result = parse_decl_enum_impl(stream, end, context);
	if (result.is_null())
	{
		return result;
	}

	if constexpr (scope == parse_scope::global || scope == parse_scope::struct_body)
	{
		bz_assert(result.is<ast::decl_enum>());
		auto &enum_decl = result.get<ast::decl_enum>();
		enum_decl.flags |= ast::decl_enum::global;
		return result;
	}
	else
	{
		bz_assert(result.is<ast::decl_enum>());
		auto &enum_decl = result.get<ast::decl_enum>();
		context.add_to_resolve_queue({}, enum_decl);
		resolve::resolve_enum(enum_decl, context);
		context.pop_resolve_queue();
		return result;
	}
}

template ast::statement parse_decl_enum<parse_scope::global>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template ast::statement parse_decl_enum<parse_scope::struct_body>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template ast::statement parse_decl_enum<parse_scope::local>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

ast::statement parse_decl_import(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream != end);
	bz_assert(stream->kind == lex::token::kw_import);
	++stream; // import

	auto id = get_identifier(stream, end, context);
	context.assert_token(stream, lex::token::semi_colon);
	if (id.values.empty())
	{
		return ast::statement();
	}
	else
	{
		return ast::make_decl_import(std::move(id));
	}
}

template<parse_scope scope>
ast::statement parse_attribute_statement(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream != end);
	bz_assert(stream->kind == lex::token::at);
	ast::arena_vector<ast::attribute> attributes = {};
	while (stream != end && stream->kind == lex::token::at)
	{
		++stream; // '@'
		auto const name = context.assert_token(stream, lex::token::identifier);
		if (stream->kind == lex::token::paren_open)
		{
			auto const paren_open = stream;
			++stream;
			auto const args_range = get_expression_tokens<
				lex::token::paren_close
			>(stream, end, context);
			if (stream != end && stream->kind == lex::token::paren_close)
			{
				++stream;
			}
			else
			{
				context.report_paren_match_error(stream, paren_open);
			}
			attributes.emplace_back(name, args_range, ast::arena_vector<ast::expression>{});
		}
		else
		{
			attributes.emplace_back(name, lex::token_range{}, ast::arena_vector<ast::expression>{});
		}
	}

	constexpr auto parser_fn = scope == parse_scope::global ? &parse_global_statement
		: scope == parse_scope::struct_body ? &parse_struct_body_statement
		: &parse_local_statement;
	auto statement = parser_fn(stream, end, context);

	switch (statement.kind())
	{
	case ast::statement::index<ast::decl_variable>:
		statement.get<ast::decl_variable>().attributes = std::move(attributes);
		break;
	case ast::statement::index<ast::decl_function>:
		statement.get<ast::decl_function>().attributes = std::move(attributes);
		break;
	case ast::statement::index<ast::decl_operator>:
		statement.get<ast::decl_operator>().attributes = std::move(attributes);
		break;

	default:
		for (auto const &attribute : attributes)
		{
			context.report_warning(
				ctx::warning_kind::unknown_attribute,
				attribute.name,
				bz::format("unknown attribute '@{}'", attribute.name->value)
			);
		}
		break;
	}
	return statement;
}

template ast::statement parse_attribute_statement<parse_scope::global>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template ast::statement parse_attribute_statement<parse_scope::struct_body>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template ast::statement parse_attribute_statement<parse_scope::local>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

ast::statement parse_export_statement(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream != end);
	bz_assert(stream->kind == lex::token::kw_export);
	++stream; // 'export'
	auto const after_export_token = stream;

	auto result = parse_global_statement(stream, end, context);
	if (result.not_null())
	{
		result.visit(bz::overload{
			[](ast::decl_function &func_decl) {
				func_decl.body.flags |= ast::function_body::module_export;
			},
			[](ast::decl_operator &op_decl) {
				op_decl.body.flags |= ast::function_body::module_export;
			},
			[](ast::decl_function_alias &alias_decl) {
				alias_decl.is_export = true;
			},
			[](ast::decl_type_alias &alias_decl) {
				alias_decl.flags |= ast::decl_type_alias::module_export;
			},
			[](ast::decl_variable &var_decl) {
				var_decl.flags |= ast::decl_variable::module_export;
			},
			[](ast::decl_struct &struct_decl) {
				struct_decl.info.flags |= ast::type_info::module_export;
			},
			[](ast::decl_enum &enum_decl) {
				enum_decl.flags |= ast::decl_enum::module_export;
			},
			[&](auto const &) {
				context.report_error(after_export_token, "invalid statement to be exported");
			}
		});
	}
	return result;
}

ast::statement parse_local_export_statement(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream != end);
	bz_assert(stream->kind == lex::token::kw_export);
	context.report_error(stream, "'export' is not allowed for local declarations");
	++stream; // 'export'
	return parse_local_statement(stream, end, context);
}

ast::statement parse_stmt_while(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream != end);
	bz_assert(stream->kind == lex::token::kw_while);
	++stream; // 'while'
	auto const prev_in_loop = context.push_loop();
	auto condition = parse_parenthesized_condition(stream, end, context);
	auto body = parse_expression_without_semi_colon(stream, end, context, no_comma);
	consume_semi_colon_at_end_of_expression(stream, end, context, body);
	context.pop_loop(prev_in_loop);
	return ast::make_stmt_while(std::move(condition), std::move(body));
}

static ast::statement parse_stmt_for_impl(
	lex::token_pos &stream, lex::token_pos end,
	lex::token_pos open_paren,
	ctx::parse_context &context
)
{
	// 'for' and '(' have already been consumed
	auto const prev_size = context.add_unresolved_scope();

	if (stream == end)
	{
		context.report_error(stream, "expected initialization statement or ';'");
		return {};
	}
	auto init_stmt = stream->kind == lex::token::semi_colon
		? (++stream, ast::statement())
		: parse_local_statement(stream, end, context);
	if (stream == end)
	{
		context.report_error(stream, "expected loop condition or ';'");
		return {};
	}
	auto const prev_in_loop = context.push_loop();
	auto condition = stream->kind == lex::token::semi_colon
		? ast::expression()
		: parse_expression(stream, end, context, precedence{});
	if (context.assert_token(stream, lex::token::semi_colon)->kind != lex::token::semi_colon)
	{
		get_expression_tokens<
			lex::token::curly_open,
			lex::token::kw_if,
			lex::token::paren_close
		>(stream, end, context);
	}

	if (stream == end)
	{
		context.report_error(stream, "expected iteration expression or closing )");
		return {};
	}
	auto iteration = stream->kind == lex::token::paren_close
		? ast::expression()
		: parse_expression(stream, end, context, precedence{});
	if (stream != end && stream->kind == lex::token::paren_close)
	{
		++stream; // ')'
	}
	else if (open_paren->kind == lex::token::paren_open)
	{
		context.report_paren_match_error(stream, open_paren);
		get_expression_tokens<
			lex::token::curly_open,
			lex::token::kw_if,
			lex::token::paren_close
		>(stream, end, context);
	}
	else
	{
		get_expression_tokens<
			lex::token::curly_open,
			lex::token::kw_if,
			lex::token::paren_close
		>(stream, end, context);
	}

	auto body = parse_expression_without_semi_colon(stream, end, context, no_comma);
	consume_semi_colon_at_end_of_expression(stream, end, context, body);

	context.pop_loop(prev_in_loop);
	context.remove_unresolved_scope(prev_size);

	return ast::make_stmt_for(
		std::move(init_stmt),
		std::move(condition),
		std::move(iteration),
		std::move(body)
	);
}

static ast::statement parse_stmt_foreach_impl(
	lex::token_pos &stream, lex::token_pos end,
	lex::token_pos open_paren, lex::token_pos in_pos,
	ctx::parse_context &context
)
{
	// 'for' and '(' have already been consumed
	if (stream->kind != lex::token::kw_let && stream->kind != lex::token::kw_const)
	{
		context.report_error(stream, "expected a variable declaration");
	}
	else if (stream->kind == lex::token::kw_let)
	{
		++stream; // 'let'
	}
	auto iter_deref_var_decl_stmt = ast::statement(ast::make_ast_unique<ast::decl_variable>(
		parse_decl_variable_id_and_type(stream, end, context)
	));

	if (stream->kind != lex::token::kw_in)
	{
		context.report_error({ stream, stream, in_pos });
		stream = in_pos;
	}
	++stream; // 'in'

	auto range_expr = parse_expression(stream, end, context, precedence{});
	if (stream != end && stream->kind == lex::token::paren_close)
	{
		++stream; // ')'
	}
	else if (open_paren->kind == lex::token::paren_open)
	{
		context.report_paren_match_error(stream, open_paren);
		get_expression_tokens<
			lex::token::curly_open,
			lex::token::kw_if,
			lex::token::paren_close
		>(stream, end, context);
	}
	else
	{
		get_expression_tokens<
			lex::token::curly_open,
			lex::token::kw_if,
			lex::token::paren_close
		>(stream, end, context);
	}

	auto const outer_prev_size = context.add_unresolved_scope();

	auto range_var_type = ast::make_auto_typespec(nullptr);
	range_var_type.add_layer<ast::ts_auto_reference_const>();
	auto const range_expr_src_tokens = range_expr.src_tokens;
	auto range_var_decl_stmt = ast::make_decl_variable(
		range_expr_src_tokens,
		lex::token_range{},
		ast::var_id_and_type(ast::identifier{}, ast::type_as_expression(std::move(range_var_type))),
		std::move(range_expr),
		context.get_current_enclosing_scope()
	);
	bz_assert(range_var_decl_stmt.is<ast::decl_variable>());
	auto &range_var_decl = range_var_decl_stmt.get<ast::decl_variable>();
	range_var_decl.id_and_type.id.tokens = { range_expr_src_tokens.begin, range_expr_src_tokens.end };
	range_var_decl.id_and_type.id.values = { "" };
	range_var_decl.id_and_type.id.is_qualified = false;

	auto const prev_in_loop = context.push_loop();
	auto const inner_prev_size = context.add_unresolved_scope();

	bz_assert(iter_deref_var_decl_stmt.is<ast::decl_variable>());
	auto &iter_deref_var_decl = iter_deref_var_decl_stmt.get<ast::decl_variable>();
	context.add_unresolved_var_decl(iter_deref_var_decl);

	auto body = parse_expression_without_semi_colon(stream, end, context, no_comma);
	consume_semi_colon_at_end_of_expression(stream, end, context, body);

	context.remove_unresolved_scope(inner_prev_size);
	context.pop_loop(prev_in_loop);
	context.remove_unresolved_scope(outer_prev_size);

	return ast::make_stmt_foreach(
		std::move(range_var_decl_stmt),
		std::move(iter_deref_var_decl_stmt),
		std::move(body)
	);
}

ast::statement parse_stmt_for_or_foreach(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream != end);
	bz_assert(stream->kind == lex::token::kw_for);
	++stream; // 'for'
	auto const open_paren = context.assert_token(stream, lex::token::paren_open);
	auto const in_pos = search_token(lex::token::kw_in, stream, end);
	if (in_pos != end)
	{
		return parse_stmt_foreach_impl(stream, end, open_paren, in_pos, context);
	}
	else
	{
		return parse_stmt_for_impl(stream, end, open_paren, context);
	}
}

ast::statement parse_stmt_return(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream != end);
	bz_assert(stream->kind == lex::token::kw_return);
	auto const return_pos = stream;
	++stream; // 'return'
	if (stream != end && stream->kind == lex::token::semi_colon)
	{
		return ast::make_stmt_return(return_pos);
	}
	auto expr = parse_expression(stream, end, context, precedence{});
	context.assert_token(stream, lex::token::semi_colon);
	return ast::make_stmt_return(return_pos, std::move(expr));
}

ast::statement parse_stmt_defer(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream != end);
	bz_assert(stream->kind == lex::token::kw_defer);
	auto const defer_pos = stream;
	++stream; // 'defer'

	auto const deferred_expr = parse_expression(stream, end, context, precedence{});
	context.assert_token(stream, lex::token::semi_colon);
	return ast::make_stmt_defer(defer_pos, std::move(deferred_expr));
}

ast::statement parse_stmt_no_op(
	lex::token_pos &stream, lex::token_pos end,
	[[maybe_unused]] ctx::parse_context &context
)
{
	bz_assert(stream != end);
	bz_assert(stream->kind == lex::token::semi_colon);
	++stream; // ';'
	return ast::make_stmt_no_op();
}

ast::statement parse_stmt_expression(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	auto expr = parse_top_level_expression(stream, end, context);
	if (expr.is<ast::expanded_variadic_expression>())
	{
		context.report_error(expr.src_tokens, "expanded variadic expression is not allowed as a top-level expression");
	}
	else if (expr.is_placeholder_literal())
	{
		context.report_error(expr.src_tokens, "placeholder literal is not allowed as a top-level expression");
	}
	else if (expr.is_enum_literal())
	{
		context.report_error(expr.src_tokens, "enum literal is not allowed as a top-level expression");
	}
	return ast::make_stmt_expression(std::move(expr));
}

static ast::statement default_global_statement_parser(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	context.report_error(stream);
	++stream;
	return parse_global_statement(stream, end, context);
}

static ast::statement default_local_statement_parser(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	auto const begin = stream;
	auto expr_stmt = parse_stmt_expression(stream, end, context);
	if (stream == begin)
	{
		context.report_error(stream);
		++stream;
		return parse_local_statement(stream, end, context);
	}
	else
	{
		return expr_stmt;
	}
}


ast::statement parse_global_statement(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	constexpr auto parse_fn = create_parse_fn<
		lex::token_pos, ctx::parse_context,
		global_statement_parsers, &default_global_statement_parser
	>();
	if (stream == end)
	{
		context.report_error(
			stream,
			stream->kind == lex::token::eof
			? "expected a statement before end-of-file"
			: "expected a statement"
		);
		return {};
	}
	else
	{
		return parse_fn(stream, end, context);
	}
}

ast::statement parse_local_statement(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	constexpr auto parse_fn = create_parse_fn<
		lex::token_pos, ctx::parse_context,
		local_statement_parsers, &default_local_statement_parser
	>();
	if (stream == end)
	{
		context.report_error(stream, "expected a statement");
		return {};
	}
	else
	{
		return parse_fn(stream, end, context);
	}
}

ast::statement parse_struct_body_statement(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	constexpr auto parse_fn = create_parse_fn<
		lex::token_pos, ctx::parse_context,
		struct_body_statement_parsers, &default_parse_type_info_statement
	>();
	if (stream == end)
	{
		return {};
	}
	else
	{
		return parse_fn(stream, end, context);
	}
}

static ast::statement parse_local_statement_without_semi_colon_default_parser(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	auto const begin = stream;
	auto const result = ast::make_stmt_expression(
		parse_expression_without_semi_colon(stream, end, context, precedence{})
	);
	if (stream == begin)
	{
		context.report_error(stream);
		++stream;
	}
	return result;
};

ast::statement parse_local_statement_without_semi_colon(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	constexpr auto parse_fn = create_parse_fn<
		lex::token_pos, ctx::parse_context,
		local_statement_parsers, &parse_local_statement_without_semi_colon_default_parser
	>();
	if (stream == end)
	{
		context.report_error(stream, "expected a statement");
		return {};
	}
	else
	{
		return parse_fn(stream, end, context);
	}
}

bz::vector<ast::statement> parse_global_statements(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz::vector<ast::statement> result = {};
	while (stream != end)
	{
		result.emplace_back(parse_global_statement(stream, end, context));
	}
	return result;
}

bz::vector<ast::statement> parse_local_statements(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz::vector<ast::statement> result = {};
	while (stream != end)
	{
		result.emplace_back(parse_local_statement(stream, end, context));
	}
	return result;
}

bz::vector<ast::statement> parse_struct_body_statements(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz::vector<ast::statement> result = {};
	while (stream != end)
	{
		result.emplace_back(parse_struct_body_statement(stream, end, context));
		if (result.back().is_null())
		{
			result.pop_back();
		}
	}
	return result;
}

} // namespace parse
