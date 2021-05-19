#include "statement_parser.h"
#include "ast/expression.h"
#include "ast/typespec.h"
#include "expression_parser.h"
#include "consteval.h"
#include "parse_common.h"
#include "token_info.h"
#include "ctx/global_context.h"

namespace parse
{

// parse functions can't be static, because they are referenced in parse_common.h

static void resolve_attributes(
	ast::statement_view stmt,
	ctx::parse_context &context
);

static bz::u8string get_static_assert_expression(ast::constant_expression const &cond)
{
	if (cond.expr.is<ast::expr_binary_op>())
	{
		auto const &binary_op = cond.expr.get<ast::expr_binary_op>();
		switch (binary_op.op)
		{
		case lex::token::equals:
		case lex::token::not_equals:
		case lex::token::less_than:
		case lex::token::less_than_eq:
		case lex::token::greater_than:
		case lex::token::greater_than_eq:
		case lex::token::bool_and:
		case lex::token::bool_xor:
		case lex::token::bool_or:
		{
			auto const op_str = token_info[binary_op.op].token_value;
			auto const &lhs = binary_op.lhs;
			bz_assert(lhs.is<ast::constant_expression>());
			auto const lhs_str = ast::get_value_string(lhs.get<ast::constant_expression>().value);
			if (lhs_str == "")
			{
				return "";
			}
			auto const &rhs = binary_op.rhs;
			bz_assert(rhs.is<ast::constant_expression>());
			auto const rhs_str = ast::get_value_string(rhs.get<ast::constant_expression>().value);
			if (rhs_str == "")
			{
				return "";
			}
			return bz::format("{} {} {}", lhs_str, op_str, rhs_str);
		}

		default:
			return "";
		}
	}
	else if (cond.expr.is<ast::expr_literal>())
	{
		return ast::get_value_string(cond.value);
	}
	else
	{
		return "";
	}
}

static void resolve_stmt_static_assert_impl(
	ast::stmt_static_assert &static_assert_stmt,
	ctx::parse_context &context
)
{
	bz_assert(static_assert_stmt.condition.is_null());
	bz_assert(static_assert_stmt.condition.src_tokens.begin == nullptr);
	bz_assert(static_assert_stmt.message.is_null());
	bz_assert(static_assert_stmt.message.src_tokens.begin == nullptr);

	auto const static_assert_pos = static_assert_stmt.static_assert_pos;
	auto const [begin, end] = static_assert_stmt.arg_tokens;
	auto stream = begin;
	auto args = parse_expression_comma_list(stream, end, context);
	if (stream != end)
	{
		auto const open_paren = begin - 1;
		if (open_paren->kind == lex::token::paren_open)
		{
			context.assert_token(stream, lex::token::paren_close);
		}
		else
		{
			context.report_error(stream);
		}
	}
	if (args.size() != 1 && args.size() != 2)
	{
		auto const src_tokens = begin == end
			? lex::src_tokens{ static_assert_pos, static_assert_pos, static_assert_pos + 1 }
			: lex::src_tokens{ begin, begin, end };
		context.report_error(
			src_tokens,
			bz::format(
				"static_assert expects 1 or 2 arguments, but was given {}",
				args.size()
			)
		);
		return;
	}

	{
		bool good = true;
		auto const check_type = [&](
			ast::expression const &expr,
			uint32_t base_type_kind,
			bz::u8string_view message
		) {
			if (!expr.is<ast::constant_expression>())
			{
				return;
			}

			auto const [type, _] = expr.get_expr_type_and_kind();
			auto const without_const = ast::remove_const_or_consteval(type);
			if (
				!without_const.is<ast::ts_base_type>()
				|| without_const.get<ast::ts_base_type>().info->kind != base_type_kind
			)
			{
				good = false;
				context.report_error(expr, message);
			}
		};

		static_assert_stmt.condition = std::move(args[0]);
		if (static_assert_stmt.condition.is_error())
		{
			good = false;
		}
		else
		{
			auto bool_type = ast::make_base_type_typespec({}, context.get_builtin_type_info(ast::type_info::bool_));
			context.match_expression_to_type(static_assert_stmt.condition, bool_type);
			consteval_try(static_assert_stmt.condition, context);
			if (static_assert_stmt.condition.has_consteval_failed())
			{
				good = false;
				context.report_error(
					static_assert_stmt.condition,
					"condition for static_assert must be a constant expression",
					get_consteval_fail_notes(static_assert_stmt.condition)
				);
			}
		}

		check_type(
			static_assert_stmt.condition,
			ast::type_info::bool_,
			"condition for static_assert must have type 'bool'"
		);

		if (args.size() == 2)
		{
			static_assert_stmt.message = std::move(args[1]);
			if (static_assert_stmt.message.is_error())
			{
				good = false;
			}
			else
			{
				auto str_type = ast::make_base_type_typespec({}, context.get_builtin_type_info(ast::type_info::str_));
				context.match_expression_to_type(static_assert_stmt.message, str_type);
				consteval_try(static_assert_stmt.message, context);
				if (static_assert_stmt.message.has_consteval_failed())
				{
					good = false;
					context.report_error(
						static_assert_stmt.message,
						"message in static_assert must be a constant expression",
						get_consteval_fail_notes(static_assert_stmt.message)
					);
				}
			}

			check_type(
				static_assert_stmt.message,
				ast::type_info::str_,
				"message in static_assert must have type 'str'"
			);
		}

		if (!good)
		{
			return;
		}
	}

	auto &cond_const_expr = static_assert_stmt.condition.get<ast::constant_expression>();
	bz_assert(cond_const_expr.value.kind() == ast::constant_value::boolean);
	auto const cond = cond_const_expr.value.get<ast::constant_value::boolean>();

	if (!cond)
	{
		auto const expression_string = get_static_assert_expression(cond_const_expr);
		bz::u8string error_message = "static assertion failed";
		if (expression_string != "")
		{
			error_message += bz::format(" due to requirement '{}'", expression_string);
		}
		if (static_assert_stmt.message.not_null() && static_assert_stmt.message.not_error())
		{
			auto &message_const_expr = static_assert_stmt.message.get<ast::constant_expression>();
			bz_assert(message_const_expr.value.kind() == ast::constant_value::string);
			auto const message = message_const_expr.value.get<ast::constant_value::string>().as_string_view();
			error_message += bz::format(", message: '{}'", message);
		}
		context.report_error(static_assert_stmt.condition, std::move(error_message));
	}
}

static void resolve_stmt_static_assert(
	ast::stmt_static_assert &static_assert_stmt,
	ctx::parse_context &context
)
{
	auto const original_file_info = context.get_current_file_info();
	auto const stmt_file_id = static_assert_stmt.static_assert_pos->src_pos.file_id;
	if (original_file_info.file_id != stmt_file_id)
	{
		context.set_current_file(stmt_file_id);
	}
	resolve_stmt_static_assert_impl(static_assert_stmt, context);
	context.set_current_file_info(original_file_info);
}

template<bool is_global>
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

	if constexpr (is_global)
	{
		return ast::make_stmt_static_assert(static_assert_pos, args);
	}
	else
	{
		auto result = ast::make_stmt_static_assert(static_assert_pos, args);
		bz_assert(result.is<ast::stmt_static_assert>());
		resolve_stmt_static_assert(result.get<ast::stmt_static_assert>(), context);
		return result;
	}
}

template ast::statement parse_stmt_static_assert<false>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template ast::statement parse_stmt_static_assert<true>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

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
				std::move(tuple_decls)
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
				std::move(tuple_decls)
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
			return ast::decl_variable(
				{ prototype_begin, id == end ? prototype_begin : id, stream },
				prototype,
				ast::var_id_and_type(
					id->kind == lex::token::identifier ? ast::make_identifier(id) : ast::identifier{},
					ast::make_unresolved_typespec({ stream, stream })
				)
			);
		}

		++stream; // ':'
		auto const type = get_expression_tokens<
			lex::token::assign,
			lex::token::comma,
			lex::token::paren_close,
			lex::token::square_close
		>(stream, end, context);

		return ast::decl_variable(
			{ prototype_begin, id, stream },
			prototype,
			ast::var_id_and_type(
				id->kind == lex::token::identifier ? ast::make_identifier(id) : ast::identifier{},
				ast::make_unresolved_typespec(type)
			)
		);
	}
}

static void resolve_typespec(
	ast::typespec &ts,
	ctx::parse_context &context,
	precedence prec
)
{
	if (!ts.is<ast::ts_unresolved>())
	{
		return;
	}
	auto [stream, end] = ts.get<ast::ts_unresolved>().tokens;
	auto type = parse_expression(stream, end, context, prec);
	if (stream != end)
	{
		context.report_error({ stream, stream, end });
	}

	consteval_try(type, context);
	if (type.not_error() && !type.has_consteval_succeeded())
	{
		auto notes = get_consteval_fail_notes(type);
		notes.push_front(context.make_note(type.src_tokens, "type must be a constant expression"));
		context.report_error(
			type.src_tokens,
			"expected a type",
			std::move(notes)
		);
		ts.clear();
	}
	else if (type.not_error() && !type.is_typename())
	{
		context.report_error(type, "expected a type");
		ts.clear();
	}
	else if (type.is_typename())
	{
		ts = std::move(type.get_typename());
	}
	else
	{
		ts.clear();
	}
}

static void apply_prototype(
	lex::token_range prototype,
	ast::decl_variable &var_decl,
	ctx::parse_context &context
)
{
	if (!var_decl.tuple_decls.empty())
	{
		for (auto &decl : var_decl.tuple_decls)
		{
			apply_prototype(prototype, decl, context);
		}
	}
	else
	{
		ast::expression type = ast::make_constant_expression(
			var_decl.src_tokens,
			ast::expression_type_kind::type_name,
			ast::make_typename_typespec(nullptr),
			ast::constant_value(var_decl.get_type()),
			ast::expr_t{}
		);
		for (auto op = prototype.end; op != prototype.begin;)
		{
			--op;
			auto const src_tokens = lex::src_tokens{ op, op, var_decl.src_tokens.end };
			type = context.make_unary_operator_expression(src_tokens, op->kind, std::move(type));
		}
		if (type.is_typename())
		{
			var_decl.get_type() = std::move(type.get_typename());
		}
		else
		{
			var_decl.state = ast::resolve_state::error;
		}
	}
}

static void resolve_variable_type(
	ast::decl_variable &var_decl,
	ctx::parse_context &context
)
{
	bz_assert(var_decl.state == ast::resolve_state::resolving_symbol);
	if (!var_decl.tuple_decls.empty())
	{
		for (auto &decl : var_decl.tuple_decls)
		{
			bz_assert(decl.state < ast::resolve_state::symbol);
			decl.state = ast::resolve_state::resolving_symbol;
			resolve_variable_type(decl, context);
			apply_prototype(var_decl.get_prototype_range(), decl, context);
			if (decl.state != ast::resolve_state::error)
			{
				decl.state = ast::resolve_state::symbol;
			}
			else
			{
				var_decl.state = ast::resolve_state::error;
			}
		}
		if (var_decl.state != ast::resolve_state::error)
		{
			var_decl.get_type() = ast::make_tuple_typespec(
				{},
				var_decl.tuple_decls.transform([](auto const &decl) -> ast::typespec {
					return decl.get_type();
				}
			).collect<bz::vector>());
		}
		return;
	}

	if (!var_decl.get_type().is<ast::ts_unresolved>())
	{
		return;
	}
	auto [stream, end] = var_decl.get_type().get<ast::ts_unresolved>().tokens;
	auto type = stream == end
		? ast::make_constant_expression(
			{},
			ast::expression_type_kind::type_name,
			ast::make_typename_typespec(nullptr),
			ast::constant_value(ast::make_auto_typespec(nullptr)),
			ast::make_expr_identifier(ast::identifier{})
		)
		: parse_expression(stream, end, context, no_assign);
	consteval_try(type, context);
	if (type.not_error() && !type.has_consteval_succeeded())
	{
		context.report_error(
			type.src_tokens,
			"variable type must be a constant expression",
			get_consteval_fail_notes(type)
		);
		var_decl.clear_type();
		var_decl.state = ast::resolve_state::error;
	}
	else if (type.not_error() && !type.is_typename())
	{
		if (stream != end && is_binary_operator(stream->kind))
		{
			bz_assert(stream->kind != lex::token::assign);
			context.report_error(
				{ stream, stream, end },
				"expected ';' or '=' at the end of a type",
				{ context.make_note(
					stream,
					bz::format("'operator {}' is not allowed in a variable declaration's type", stream->value)
				) }
			);
		}
		else if (stream != end)
		{
			context.report_error({ stream, stream, end });
		}

		context.report_error(type.src_tokens, "expected a type");
		var_decl.clear_type();
		var_decl.state = ast::resolve_state::error;
	}
	else if (type.is_typename())
	{
		auto const prototype = var_decl.get_prototype_range();
		for (auto op = prototype.end; op != prototype.begin;)
		{
			--op;
			auto const src_tokens = lex::src_tokens{ op, op, type.src_tokens.end };
			type = context.make_unary_operator_expression(src_tokens, op->kind, std::move(type));
		}
		if (type.is_typename())
		{
			var_decl.get_type() = std::move(type.get_typename());
		}
		else
		{
			var_decl.clear_type();
			var_decl.state = ast::resolve_state::error;
		}
	}
	else
	{
		var_decl.clear_type();
		var_decl.state = ast::resolve_state::error;
	}
}

static void resolve_variable_init_expr_and_match_type(
	ast::decl_variable &var_decl,
	ctx::parse_context &context
)
{
	bz_assert(!var_decl.get_type().is_empty());
	if (var_decl.init_expr.not_null())
	{
		if (var_decl.init_expr.is<ast::unresolved_expression>())
		{
			auto const begin = var_decl.init_expr.src_tokens.begin;
			auto const end   = var_decl.init_expr.src_tokens.end;
			auto stream = begin;
			var_decl.init_expr = parse_expression(stream, end, context, no_comma);
			if (stream != end)
			{
				if (stream->kind == lex::token::comma)
				{
					auto const suggestion_end = (end - 1)->kind == lex::token::semi_colon ? end - 1 : end;
					context.report_error(
						stream,
						"'operator ,' is not allowed in variable initialization expression",
						{}, { context.make_suggestion_around(
							begin,          ctx::char_pos(), ctx::char_pos(), "(",
							suggestion_end, ctx::char_pos(), ctx::char_pos(), ")",
							"put parenthesis around the initialization expression"
						) }
					);
				}
				else
				{
					context.assert_token(stream, lex::token::semi_colon);
				}
			}
		}
		context.match_expression_to_variable(var_decl.init_expr, var_decl);
	}
	else if (var_decl.init_expr.src_tokens.pivot != nullptr)
	{
		if (!ast::is_complete(var_decl.get_type()))
		{
			var_decl.clear_type();
		}
		var_decl.state = ast::resolve_state::error;
	}
	else
	{
		if (!ast::is_complete(var_decl.get_type()))
		{
			context.report_error(
				var_decl.src_tokens,
				bz::format(
					"a variable with an incomplete type '{}' must be initialized",
					var_decl.get_type()
				)
			);
			var_decl.clear_type();
			var_decl.state = ast::resolve_state::error;
		}
		else if (var_decl.get_type().is<ast::ts_const>())
		{
			context.report_error(
				var_decl.src_tokens,
				"a variable with a 'const' type must be initialized"
			);
			var_decl.state = ast::resolve_state::error;
		}
		else if (var_decl.get_type().is<ast::ts_consteval>())
		{
			context.report_error(
				var_decl.src_tokens,
				"a variable with a 'consteval' type must be initialized"
			);
			var_decl.state = ast::resolve_state::error;
		}
		else if (var_decl.get_type().is<ast::ts_base_type>())
		{
			auto const info = var_decl.get_type().get<ast::ts_base_type>().info;
			auto const def_ctor = info->default_constructor != nullptr
				? info->default_constructor
				: info->default_default_constructor.get();
			if (def_ctor != nullptr)
			{
				var_decl.init_expr = ast::make_dynamic_expression(
					var_decl.src_tokens,
					ast::expression_type_kind::rvalue,
					var_decl.get_type(),
					ast::make_expr_function_call(
						var_decl.src_tokens,
						bz::vector<ast::expression>{},
						def_ctor,
						ast::resolve_order::regular
					)
				);
				parse::consteval_guaranteed(var_decl.init_expr, context);
			}
		}
	}
	if (
		!var_decl.get_type().is_empty()
		&& !context.is_instantiable(var_decl.get_type())
		&& var_decl.state != ast::resolve_state::error
	)
	{
		auto const var_decl_src_tokens = var_decl.get_type().get_src_tokens();
		auto const src_tokens = [&]() {
			if (var_decl_src_tokens.pivot != nullptr)
			{
				return var_decl_src_tokens;
			}
			else if (var_decl.get_id().tokens.begin != nullptr)
			{
				return lex::src_tokens{
					var_decl.get_id().tokens.begin,
					var_decl.get_id().tokens.begin,
					var_decl.get_id().tokens.end
				};
			}
			else if (var_decl.init_expr.src_tokens.pivot != nullptr)
			{
				return var_decl.init_expr.src_tokens;
			}
			else
			{
				return var_decl.src_tokens;
			}
		}();
		bz_assert(src_tokens.pivot != nullptr);
		context.report_error(src_tokens, bz::format("variable type '{}' is not instantiable", var_decl.get_type()));
		var_decl.state = ast::resolve_state::error;
		var_decl.clear_type();
	}
}

static void resolve_variable_symbol_impl(
	ast::decl_variable &var_decl,
	ctx::parse_context &context
)
{
	var_decl.state = ast::resolve_state::resolving_symbol;
	resolve_variable_type(var_decl, context);
	if (var_decl.state == ast::resolve_state::error)
	{
		return;
	}

	if (!ast::is_complete(var_decl.get_type()) || var_decl.get_type().is<ast::ts_consteval>())
	{
		var_decl.state = ast::resolve_state::resolving_all;
		resolve_variable_init_expr_and_match_type(var_decl, context);
		if (var_decl.state == ast::resolve_state::error)
		{
			return;
		}
		var_decl.state = ast::resolve_state::all;
	}
	else
	{
		var_decl.state = ast::resolve_state::symbol;
	}
}

void resolve_variable_symbol(
	ast::decl_variable &var_decl,
	ctx::parse_context &context
)
{
	if (var_decl.state >= ast::resolve_state::symbol || var_decl.state == ast::resolve_state::error)
	{
		return;
	}
	else if (var_decl.state == ast::resolve_state::resolving_symbol)
	{
		context.report_circular_dependency_error(var_decl);
		var_decl.state = ast::resolve_state::error;
		return;
	}

	auto const original_file_info = context.get_current_file_info();
	auto const stmt_file_id = var_decl.src_tokens.pivot->src_pos.file_id;
	if (original_file_info.file_id != stmt_file_id)
	{
		context.set_current_file(stmt_file_id);
	}
	resolve_variable_symbol_impl(var_decl, context);
	context.set_current_file_info(original_file_info);
}

static void resolve_variable_impl(
	ast::decl_variable &var_decl,
	ctx::parse_context &context
)
{
	if (var_decl.state < ast::resolve_state::symbol)
	{
		var_decl.state = ast::resolve_state::resolving_symbol;
		resolve_variable_type(var_decl, context);
		if (var_decl.state == ast::resolve_state::error)
		{
			return;
		}
	}
	var_decl.state = ast::resolve_state::resolving_all;
	resolve_variable_init_expr_and_match_type(var_decl, context);
	if (var_decl.state == ast::resolve_state::error)
	{
		return;
	}
	var_decl.state = ast::resolve_state::all;
}

void resolve_variable(
	ast::decl_variable &var_decl,
	ctx::parse_context &context
)
{
	if (var_decl.state >= ast::resolve_state::all || var_decl.state == ast::resolve_state::error)
	{
		return;
	}
	else if (var_decl.state == ast::resolve_state::resolving_symbol || var_decl.state == ast::resolve_state::resolving_all)
	{
		context.report_circular_dependency_error(var_decl);
		var_decl.state = ast::resolve_state::error;
		return;
	}

	auto const original_file_info = context.get_current_file_info();
	auto const stmt_file_id = var_decl.src_tokens.pivot->src_pos.file_id;
	if (original_file_info.file_id != stmt_file_id)
	{
		context.set_current_file(stmt_file_id);
	}
	resolve_variable_impl(var_decl, context);
	context.set_current_file_info(original_file_info);
}

template<bool is_global>
ast::statement parse_decl_variable(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream != end);
	bz_assert(
		stream->kind == lex::token::kw_let
		|| stream->kind == lex::token::kw_const
		|| stream->kind == lex::token::kw_consteval
	);
	auto const begin_token = stream;
	if (stream->kind == lex::token::kw_let)
	{
		++stream;
	}

	auto result_decl = parse_decl_variable_id_and_type(stream, end, context);
	if (stream != end && stream->kind == lex::token::assign)
	{
		++stream; // '='
		auto const init_expr = get_expression_tokens<>(stream, end, context);
		auto const end_token = stream;
		context.assert_token(stream, lex::token::semi_colon);
		if constexpr (is_global)
		{
			auto result = ast::statement(ast::make_ast_unique<ast::decl_variable>(std::move(result_decl)));
			bz_assert(result.is<ast::decl_variable>());
			auto &var_decl = result.get<ast::decl_variable>();
			var_decl.init_expr = ast::make_unresolved_expression({ init_expr.begin, init_expr.begin, init_expr.end });
			var_decl.src_tokens = { begin_token, var_decl.src_tokens.pivot, end_token };
			auto const id_tokens = var_decl.id_and_type.id.tokens;
			bz_assert(id_tokens.end - id_tokens.begin <= 1);
			if (id_tokens.begin != nullptr)
			{
				var_decl.id_and_type.id = context.make_qualified_identifier(id_tokens.begin);
			}
			else
			{
				var_decl.id_and_type.id.is_qualified = true;
			}
			return result;
		}
		else
		{
			auto result = ast::statement(ast::make_ast_unique<ast::decl_variable>(std::move(result_decl)));
			bz_assert(result.is<ast::decl_variable>());
			auto &var_decl = result.get<ast::decl_variable>();
			var_decl.init_expr = ast::make_unresolved_expression({ init_expr.begin, init_expr.begin, init_expr.end });
			var_decl.src_tokens = { begin_token, var_decl.src_tokens.pivot, end_token };
			resolve_variable(var_decl, context);
			context.add_local_variable(var_decl);
			return result;
		}
	}
	else
	{
		auto const end_token = stream;
		context.assert_token(stream, lex::token::semi_colon);
		if constexpr (is_global)
		{
			auto result = ast::statement(ast::make_ast_unique<ast::decl_variable>(std::move(result_decl)));
			bz_assert(result.is<ast::decl_variable>());
			auto &var_decl = result.get<ast::decl_variable>();
			var_decl.src_tokens = { begin_token, var_decl.src_tokens.pivot, end_token };
			auto const id_tokens = var_decl.id_and_type.id.tokens;
			bz_assert(id_tokens.end - id_tokens.begin <= 1);
			if (id_tokens.begin != nullptr)
			{
				var_decl.id_and_type.id = context.make_qualified_identifier(id_tokens.begin);
			}
			else
			{
				var_decl.id_and_type.id.is_qualified = true;
			}
			return result;
		}
		else
		{
			auto result = ast::statement(ast::make_ast_unique<ast::decl_variable>(std::move(result_decl)));
			bz_assert(result.is<ast::decl_variable>());
			auto &var_decl = result.get<ast::decl_variable>();
			var_decl.src_tokens = { begin_token, var_decl.src_tokens.pivot, end_token };
			resolve_variable(var_decl, context);
			context.add_local_variable(var_decl);
			return result;
		}
	}
}

template ast::statement parse_decl_variable<false>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template ast::statement parse_decl_variable<true>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

static void resolve_type_alias_impl(
	ast::decl_type_alias &alias_decl,
	ctx::parse_context &context
)
{
	alias_decl.state = ast::resolve_state::resolving_all;

	bz_assert(alias_decl.alias_expr.is<ast::unresolved_expression>());
	auto const begin = alias_decl.alias_expr.src_tokens.begin;
	auto const end   = alias_decl.alias_expr.src_tokens.end;
	auto stream = begin;
	alias_decl.state = ast::resolve_state::resolving_all;
	alias_decl.alias_expr = parse_expression(stream, end, context, no_comma);
	if (stream != end)
	{
		if (stream->kind == lex::token::comma)
		{
			auto const suggestion_end = (end - 1)->kind == lex::token::semi_colon ? end - 1 : end;
			context.report_error(
				stream,
				"'operator ,' is not allowed in type alias expression",
				{}, { context.make_suggestion_around(
					begin,          ctx::char_pos(), ctx::char_pos(), "(",
					suggestion_end, ctx::char_pos(), ctx::char_pos(), ")",
					"put parenthesis around the expression"
				) }
			);
		}
		else
		{
			context.assert_token(stream, lex::token::semi_colon);
		}
	}
	else if (alias_decl.alias_expr.is_error())
	{
		alias_decl.state = ast::resolve_state::error;
		return;
	}
	consteval_try(alias_decl.alias_expr, context);

	if (!alias_decl.alias_expr.has_consteval_succeeded())
	{
		context.report_error(
			alias_decl.alias_expr,
			"type alias expression must be a constant expression",
			get_consteval_fail_notes(alias_decl.alias_expr)
		);
		alias_decl.state = ast::resolve_state::error;
		return;
	}

	auto const &value = alias_decl.alias_expr.get<ast::constant_expression>().value;
	if (value.is<ast::constant_value::type>())
	{
		alias_decl.state = ast::resolve_state::all;
	}
	else
	{
		context.report_error(alias_decl.alias_expr, "type alias value must be a type");
		alias_decl.state = ast::resolve_state::error;
		return;
	}
}

void resolve_type_alias(
	ast::decl_type_alias &alias_decl,
	ctx::parse_context &context
)
{
	if (alias_decl.state >= ast::resolve_state::all || alias_decl.state == ast::resolve_state::error)
	{
		return;
	}
	else if (alias_decl.state == ast::resolve_state::resolving_all)
	{
		context.report_circular_dependency_error(alias_decl);
		alias_decl.state = ast::resolve_state::error;
		return;
	}

	auto const original_file_info = context.get_current_file_info();
	auto const stmt_file_id = alias_decl.id.tokens.begin->src_pos.file_id;
	if (original_file_info.file_id != stmt_file_id)
	{
		context.set_current_file(stmt_file_id);
	}
	resolve_type_alias_impl(alias_decl, context);
	context.set_current_file_info(original_file_info);
}

template<bool is_global>
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
	if constexpr (is_global)
	{
		return ast::make_decl_type_alias(
			lex::src_tokens{ begin_token, id, end_token },
			context.make_qualified_identifier(id),
			ast::make_unresolved_expression({ alias_tokens.begin, alias_tokens.begin, alias_tokens.end })
		);
	}
	else
	{
		auto result = ast::make_decl_type_alias(
			lex::src_tokens{ begin_token, id, end_token },
			ast::make_identifier(id),
			ast::make_unresolved_expression({ alias_tokens.begin, alias_tokens.begin, alias_tokens.end })
		);
		bz_assert(result.is<ast::decl_type_alias>());
		auto &type_alias = result.get<ast::decl_type_alias>();
		resolve_type_alias(type_alias, context);
		if (type_alias.state != ast::resolve_state::error)
		{
			context.add_local_type_alias(type_alias);
		}
		return result;
	}
}

template ast::statement parse_decl_type_alias<false>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template ast::statement parse_decl_type_alias<true>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

static void resolve_function_alias_impl(
	ast::decl_function_alias &alias_decl,
	ctx::parse_context &context
)
{
	auto const begin = alias_decl.alias_expr.src_tokens.begin;
	auto const end   = alias_decl.alias_expr.src_tokens.end;
	auto stream = begin;
	alias_decl.state = ast::resolve_state::resolving_all;
	alias_decl.alias_expr = parse_expression(stream, end, context, no_comma);
	if (stream != end)
	{
		if (stream->kind == lex::token::comma)
		{
			auto const suggestion_end = (end - 1)->kind == lex::token::semi_colon ? end - 1 : end;
			context.report_error(
				stream,
				"'operator ,' is not allowed in function alias expression",
				{}, { context.make_suggestion_around(
					begin,          ctx::char_pos(), ctx::char_pos(), "(",
					suggestion_end, ctx::char_pos(), ctx::char_pos(), ")",
					"put parenthesis around the expression"
				) }
			);
		}
		else
		{
			context.assert_token(stream, lex::token::semi_colon);
		}
	}
	consteval_try(alias_decl.alias_expr, context);

	if (!alias_decl.alias_expr.has_consteval_succeeded())
	{
		context.report_error(
			alias_decl.alias_expr,
			"function alias expression must be a constant expression",
			get_consteval_fail_notes(alias_decl.alias_expr)
		);
		alias_decl.state = ast::resolve_state::error;
		return;
	}

	auto const &value = alias_decl.alias_expr.get<ast::constant_expression>().value;
	if (value.is<ast::constant_value::function>())
	{
		auto const func_body = value.get<ast::constant_value::function>();
		bz_assert(alias_decl.aliased_bodies.empty());
		alias_decl.aliased_bodies = { func_body };
		alias_decl.state = ast::resolve_state::all;
	}
	else if (value.is<ast::constant_value::unqualified_function_set_id>() || value.is<ast::constant_value::qualified_function_set_id>())
	{
		auto const func_set_id = value.is<ast::constant_value::unqualified_function_set_id>()
			? value.get<ast::constant_value::unqualified_function_set_id>().as_array_view()
			: value.get<ast::constant_value::qualified_function_set_id>().as_array_view();
		bz_assert(alias_decl.aliased_bodies.empty());
		alias_decl.aliased_bodies = value.is<ast::constant_value::unqualified_function_set_id>()
			? context.get_function_bodies_from_unqualified_id(alias_decl.alias_expr.src_tokens, func_set_id)
			: context.get_function_bodies_from_qualified_id(alias_decl.alias_expr.src_tokens, func_set_id);
		if (alias_decl.state != ast::resolve_state::error && !alias_decl.aliased_bodies.empty())
		{
			alias_decl.state = ast::resolve_state::all;
		}
		else
		{
			alias_decl.state = ast::resolve_state::error;
		}
	}
	else
	{
		context.report_error(alias_decl.alias_expr, "function alias value must be a function");
		alias_decl.state = ast::resolve_state::error;
		return;
	}
}

void resolve_function_alias(
	ast::decl_function_alias &alias_decl,
	ctx::parse_context &context
)
{
	if (alias_decl.state >= ast::resolve_state::all || alias_decl.state == ast::resolve_state::error)
	{
		return;
	}
	else if (alias_decl.state != ast::resolve_state::none)
	{
		bz_assert(alias_decl.state == ast::resolve_state::resolving_all);
		context.report_circular_dependency_error(alias_decl);
		alias_decl.state = ast::resolve_state::error;
		return;
	}

	auto const original_file_info = context.get_current_file_info();
	auto const stmt_file_id = alias_decl.id.tokens.begin->src_pos.file_id;
	if (original_file_info.file_id != stmt_file_id)
	{
		context.set_current_file(stmt_file_id);
	}
	resolve_function_alias_impl(alias_decl, context);
	context.set_current_file_info(original_file_info);
}

static bool resolve_function_parameters_helper(
	ast::statement_view func_stmt,
	ast::function_body &func_body,
	ctx::parse_context &context
)
{
	bz_assert(func_body.state == ast::resolve_state::resolving_parameters || func_body.state == ast::resolve_state::resolving_symbol);
	bool good = true;
	bool is_generic = false;
	for (auto &p : func_body.params)
	{
		if (p.state == ast::resolve_state::none)
		{
			p.state = ast::resolve_state::resolving_symbol;
			resolve_variable_type(p, context);
			p.state = ast::resolve_state::symbol;
		}
		if (p.get_type().is_empty())
		{
			good = false;
		}
		else if (ast::is_generic_parameter(p))
		{
			is_generic = true;
		}
	}
	if (is_generic && !func_body.is_generic_specialization())
	{
		func_body.flags |= ast::function_body::generic;
	}

	if (
		good
		&& func_stmt.is<ast::decl_operator>()
		&& func_stmt.get<ast::decl_operator>().op->kind == lex::token::assign
	)
	{
		bz_assert(func_body.params.size() == 2);
		auto const lhs_t = func_body.params[0].get_type().as_typespec_view();
		auto const rhs_t = func_body.params[1].get_type().as_typespec_view();
		if (
			lhs_t.is<ast::ts_lvalue_reference>()
			&& lhs_t.get<ast::ts_lvalue_reference>().is<ast::ts_base_type>()
			&& ((
				rhs_t.is<ast::ts_lvalue_reference>()
				&& rhs_t.get<ast::ts_lvalue_reference>().is<ast::ts_const>()
				&& rhs_t.get<ast::ts_lvalue_reference>().get<ast::ts_const>() == lhs_t.get<ast::ts_lvalue_reference>()
			) || (
				ast::remove_const_or_consteval(rhs_t) == lhs_t.get<ast::ts_lvalue_reference>()
			))
		)
		{
			auto const info = lhs_t.get<ast::ts_lvalue_reference>().get<ast::ts_base_type>().info;
			if (rhs_t.is<ast::ts_lvalue_reference>())
			{
				info->op_assign = &func_body;
			}
			else
			{
				info->op_move_assign = &func_body;
			}
		}
	}
	else if (good && func_body.is_destructor())
	{
		if (func_body.params.size() != 1)
		{
			context.report_error(
				func_body.src_tokens,
				bz::format(
					"destructor of type '{}' must have one parameter",
					ast::type_info::decode_symbol_name(func_body.get_destructor_of()->symbol_name)
				)
			);
			return false;
		}

		if (func_body.is_generic())
		{
			func_body.flags &= ~ast::function_body::generic;

			auto const param_type = func_body.params[0].get_type().as_typespec_view();
			// if the parameter is generic, then it must be &auto
			// either as `destructor(&self)` or `destructor(self: &auto)`
			if (
				param_type.nodes.size() != 2
				|| !param_type.nodes[0].is<ast::ts_lvalue_reference>()
				|| !param_type.nodes[1].is<ast::ts_auto>()
			)
			{
				auto const destructor_of_type = ast::type_info::decode_symbol_name(func_body.get_destructor_of()->symbol_name);
				context.report_error(
					func_body.params[0].src_tokens,
					bz::format(
						"invalid parameter type '{}' in destructor of type '{}'",
						param_type, destructor_of_type
					),
					{ context.make_note(bz::format("it must be either '&auto' or '&{}'", destructor_of_type)) }
				);
				return false;
			}

			auto const auto_pos = func_body.params[0].get_type().nodes[1].get<ast::ts_auto>().auto_pos;
			auto const param_type_src_tokens = auto_pos == nullptr
				? lex::src_tokens{}
				: lex::src_tokens{ auto_pos, auto_pos, auto_pos + 1 };
			func_body.params[0].get_type().nodes[1] = ast::ts_base_type{ param_type_src_tokens, func_body.get_destructor_of() };
		}
		else
		{
			auto const param_type = func_body.params[0].get_type().as_typespec_view();
			// if the parameter is non-generic, then it must be &<type>
			if (
				param_type.nodes.size() != 2
				|| !param_type.nodes[0].is<ast::ts_lvalue_reference>()
				|| !param_type.nodes[1].is<ast::ts_base_type>()
				|| param_type.nodes[1].get<ast::ts_base_type>().info != func_body.get_destructor_of()
			)
			{
				auto const destructor_of_type = ast::type_info::decode_symbol_name(func_body.get_destructor_of()->symbol_name);
				context.report_error(
					func_body.params[0].src_tokens,
					bz::format(
						"invalid parameter type '{}' in destructor of type '{}'",
						param_type, destructor_of_type
					),
					{ context.make_note(bz::format("it must be either '&auto' or '&{}'", destructor_of_type)) }
				);
				return false;
			}
		}
	}
	else if (good && func_body.is_constructor())
	{
		if (func_body.params.empty())
		{
			func_body.get_constructor_of()->default_constructor = &func_body;
		}
		else if (
			func_body.params.size() == 1
			&& func_body.params[0].get_type().nodes.size() == 3
			&& func_body.params[0].get_type().nodes[0].is<ast::ts_lvalue_reference>()
			&& func_body.params[0].get_type().nodes[1].is<ast::ts_const>()
			&& func_body.params[0].get_type().nodes[2].is<ast::ts_base_type>()
			&& func_body.params[0].get_type().nodes[2].get<ast::ts_base_type>().info == func_body.get_constructor_of()
		)
		{
			func_body.get_constructor_of()->copy_constructor = &func_body;
		}
	}
	return good;
}

static void resolve_function_parameters_impl(
	ast::statement_view func_stmt,
	ast::function_body &func_body,
	ctx::parse_context &context
)
{
	func_body.state = ast::resolve_state::resolving_parameters;
	if (resolve_function_parameters_helper(func_stmt, func_body, context))
	{
		func_body.state = ast::resolve_state::parameters;
	}
	else
	{
		func_body.state = ast::resolve_state::error;
	}
}

void resolve_function_parameters(
	ast::statement_view func_stmt,
	ast::function_body &func_body,
	ctx::parse_context &context
)
{
	if (func_body.state >= ast::resolve_state::parameters || func_body.state == ast::resolve_state::error)
	{
		return;
	}
	else if (func_body.state == ast::resolve_state::resolving_parameters || func_body.state == ast::resolve_state::resolving_symbol)
	{
		context.report_circular_dependency_error(func_body);
		func_body.state = ast::resolve_state::error;
		return;
	}

	bz::optional<ctx::parse_context> new_context{};
	auto context_ptr = [&]() {
		if (func_body.is_local())
		{
			auto const var_count = context.scope_decls
				.transform([](auto const &decl_set) { return decl_set.var_decls.size(); })
				.sum();
			if (var_count == 0)
			{
				return &context;
			}
			else
			{
				new_context.emplace(context, ctx::parse_context::local_copy_t{});
				for (auto &decl_set : new_context->scope_decls)
				{
					decl_set.var_decls.clear();
				}
				return &new_context.get();
			}
		}
		else
		{
			if (context.scope_decls.empty())
			{
				return &context;
			}
			else
			{
				new_context.emplace(context, ctx::parse_context::global_copy_t{});
				for (auto &decl_set : new_context->scope_decls)
				{
					decl_set.var_decls.clear();
				}
				return &new_context.get();
			}
		}
	}();

	auto const original_file_info = context_ptr->get_current_file_info();
	auto const stmt_file_id = func_body.src_tokens.pivot->src_pos.file_id;
	if (original_file_info.file_id != stmt_file_id)
	{
		context_ptr->set_current_file(stmt_file_id);
	}
	resolve_function_parameters_impl(func_stmt, func_body, *context_ptr);
	context_ptr->set_current_file_info(original_file_info);
}

static bool resolve_function_return_type_helper(
	ast::function_body &func_body,
	ctx::parse_context &context
)
{
	bz_assert(func_body.state == ast::resolve_state::resolving_symbol);
	for (auto &p : func_body.params)
	{
		context.add_local_variable(p);
	}

	resolve_typespec(func_body.return_type, context, precedence{});
	bz_assert(!func_body.return_type.is<ast::ts_unresolved>());
	if (func_body.is_destructor())
	{
		if (!func_body.return_type.is_empty() && !func_body.return_type.is<ast::ts_void>())
		{
			auto const destructor_of_type = ast::type_info::decode_symbol_name(func_body.get_destructor_of()->symbol_name);
			context.report_error(
				func_body.return_type.get_src_tokens(),
				bz::format("return type must be 'void' for destructor of type '{}'", destructor_of_type)
			);
			return false;
		}
		return !func_body.return_type.is_empty();
	}
	else if (func_body.is_constructor())
	{
		func_body.return_type = ast::make_base_type_typespec({}, func_body.get_constructor_of());
		return true;
	}
	else
	{
		return !func_body.return_type.is_empty();
	}
}

static bool is_valid_main(ast::function_body const &body)
{
	if (body.is_generic())
	{
		return false;
	}

	if (!(
		body.return_type.is<ast::ts_void>()
		|| (
			body.return_type.is<ast::ts_base_type>()
			&& body.return_type.get<ast::ts_base_type>().info->kind == ast::type_info::int32_
		)
	))
	{
		return false;
	}

	if (body.params.size() == 0)
	{
		return true;
	}
	else if (body.params.size() > 1)
	{
		return false;
	}

	for (auto const &param : body.params)
	{
		auto const param_t = ast::remove_const_or_consteval(param.get_type());
		if (!param_t.is<ast::ts_array_slice>())
		{
			return false;
		}
		auto const slice_t = ast::remove_const_or_consteval(param_t.get<ast::ts_array_slice>().elem_type);
		if (!(
			slice_t.is<ast::ts_void>()
			|| (
				slice_t.is<ast::ts_base_type>()
				&& slice_t.get<ast::ts_base_type>().info->kind == ast::type_info::str_
			)
		))
		{
			return false;
		}
	}
	return true;
}

static void report_invalid_main_error(ast::function_body const &body, ctx::parse_context &context)
{
	if (body.is_generic())
	{
		context.report_error(
			body.src_tokens, "invalid declaration for main function",
			{ context.make_note(body.src_tokens, "main function can't be generic") }
		);
		return;
	}

	if (!(
		body.return_type.is<ast::ts_void>()
		|| (
			body.return_type.is<ast::ts_base_type>()
			&& body.return_type.get<ast::ts_base_type>().info->kind == ast::type_info::int32_
		)
	))
	{
		auto const ret_t_src_tokens = body.return_type.get_src_tokens();
		bz_assert(ret_t_src_tokens.pivot != nullptr);
		context.report_error(
			body.src_tokens, "invalid declaration for main function",
			{ context.make_note(ret_t_src_tokens, "main function's return type must be 'void' or 'int32'") }
		);
		return;
	}

	if (body.params.size() == 0)
	{
		bz_unreachable;
	}
	else if (body.params.size() > 1)
	{
		context.report_error(
			body.src_tokens, "invalid declaration for main function",
			{ context.make_note(body.src_tokens, "main function must have at most one parameter") }
		);
		return;
	}

	for (auto const &param : body.params)
	{
		auto const param_t = ast::remove_const_or_consteval(param.get_type());
		if (!param_t.is<ast::ts_array_slice>())
		{
			context.report_error(
				body.src_tokens, "invalid declaration for main function",
				{ context.make_note(param.src_tokens, "parameter type must be '[: const str]'") }
			);
			return;
		}
		auto const slice_t = ast::remove_const_or_consteval(param_t.get<ast::ts_array_slice>().elem_type);
		if (!(
			slice_t.is<ast::ts_void>()
			|| (
				slice_t.is<ast::ts_base_type>()
				&& slice_t.get<ast::ts_base_type>().info->kind == ast::type_info::str_
			)
		))
		{
			context.report_error(
				body.src_tokens, "invalid declaration for main function",
				{ context.make_note(param.src_tokens, "parameter type must be '[: const str]'") }
			);
			return;
		}
	}
	bz_unreachable;
}

// resolves the function symbol, but doesn't modify scope
static bool resolve_function_symbol_helper(
	ast::statement_view func_stmt,
	ast::function_body &func_body,
	ctx::parse_context &context
)
{
	bz_assert(func_body.state == ast::resolve_state::resolving_symbol);
	auto const parameters_good = resolve_function_parameters_helper(func_stmt, func_body, context);
	if (func_body.is_generic())
	{
		return parameters_good;
	}
	auto const return_type_good = resolve_function_return_type_helper(func_body, context);
	for (auto &p : func_body.params)
	{
		p.flags |= ast::decl_variable::used;
	}
	auto const good = parameters_good && return_type_good;
	if (!good)
	{
		return false;
	}

	if (func_body.is_main() && !is_valid_main(func_body))
	{
		report_invalid_main_error(func_body, context);
	}
	func_body.resolve_symbol_name();
	context.add_function_for_compilation(func_body);
	return true;
}

static void resolve_function_symbol_impl(
	ast::statement_view func_stmt,
	ast::function_body &func_body,
	ctx::parse_context &context
)
{
	func_body.state = ast::resolve_state::resolving_symbol;
	context.add_scope();
	if (resolve_function_symbol_helper(func_stmt, func_body, context))
	{
		func_body.state = func_body.is_generic() ? ast::resolve_state::parameters : ast::resolve_state::symbol;
	}
	else
	{
		func_body.state = ast::resolve_state::error;
	}
	context.remove_scope();
}

void resolve_function_symbol(
	ast::statement_view func_stmt,
	ast::function_body &func_body,
	ctx::parse_context &context
)
{
	if (func_body.state >= ast::resolve_state::symbol || func_body.state == ast::resolve_state::error)
	{
		return;
	}
	else if (func_body.state == ast::resolve_state::resolving_parameters || func_body.state == ast::resolve_state::resolving_symbol)
	{
		context.report_circular_dependency_error(func_body);
		func_body.state = ast::resolve_state::error;
		return;
	}

	bz::optional<ctx::parse_context> new_context{};
	auto context_ptr = [&]() {
		if (func_body.is_local())
		{
			auto const var_count = context.scope_decls
				.transform([](auto const &decl_set) { return decl_set.var_decls.size(); })
				.sum();
			if (var_count == 0)
			{
				return &context;
			}
			else
			{
				new_context.emplace(context, ctx::parse_context::local_copy_t{});
				for (auto &decl_set : new_context->scope_decls)
				{
					decl_set.var_decls.clear();
				}
				return &new_context.get();
			}
		}
		else
		{
			if (context.scope_decls.empty())
			{
				return &context;
			}
			else
			{
				new_context.emplace(context, ctx::parse_context::global_copy_t{});
				for (auto &decl_set : new_context->scope_decls)
				{
					decl_set.var_decls.clear();
				}
				return &new_context.get();
			}
		}
	}();

	auto const original_file_info = context_ptr->get_current_file_info();
	auto const stmt_file_id = func_body.src_tokens.pivot->src_pos.file_id;
	if (original_file_info.file_id != stmt_file_id)
	{
		context_ptr->set_current_file(stmt_file_id);
	}
	resolve_function_symbol_impl(func_stmt, func_body, *context_ptr);
	context_ptr->set_current_file_info(original_file_info);
}

static void resolve_function_impl(
	ast::statement_view func_stmt,
	ast::function_body &func_body,
	ctx::parse_context &context
)
{
	if (func_body.state <= ast::resolve_state::parameters)
	{
		func_body.state = ast::resolve_state::resolving_symbol;
		context.add_scope();
		if (!resolve_function_symbol_helper(func_stmt, func_body, context))
		{
			func_body.state = ast::resolve_state::error;
			context.remove_scope();
			return;
		}
		else if (func_body.is_generic())
		{
			func_body.state = ast::resolve_state::parameters;
			context.remove_scope();
			return;
		}
		else
		{
			func_body.state = ast::resolve_state::symbol;
			context.remove_scope();
		}
	}

	if (func_body.body.is_null())
	{
		return;
	}

	auto const prev_function = context.current_function;
	context.current_function = &func_body;
	context.add_scope();
	for (auto &p : func_body.params)
	{
		context.add_local_variable(p);
		p.flags &= ~(ast::decl_variable::used);
	}

	func_body.state = ast::resolve_state::resolving_all;

	bz_assert(func_body.body.is<lex::token_range>());
	auto [stream, end] = func_body.body.get<lex::token_range>();
	func_body.body = parse_local_statements(stream, end, context);
	func_body.state = ast::resolve_state::all;

	context.remove_scope();
	context.current_function = prev_function;
}

void resolve_function(
	ast::statement_view func_stmt,
	ast::function_body &func_body,
	ctx::parse_context &context
)
{
	if (func_body.state >= ast::resolve_state::all || func_body.state == ast::resolve_state::error)
	{
		return;
	}
	else if (
		func_body.state == ast::resolve_state::resolving_parameters
		|| func_body.state == ast::resolve_state::resolving_symbol
		|| func_body.state == ast::resolve_state::resolving_all
	)
	{
		context.report_circular_dependency_error(func_body);
		return;
	}

	bz::optional<ctx::parse_context> new_context{};
	auto context_ptr = [&]() {
		if (func_body.is_local())
		{
			auto const var_count = context.scope_decls
				.transform([](auto const &decl_set) { return decl_set.var_decls.size(); })
				.sum();
			if (var_count == 0)
			{
				return &context;
			}
			else
			{
				new_context.emplace(context, ctx::parse_context::local_copy_t{});
				for (auto &decl_set : new_context->scope_decls)
				{
					decl_set.var_decls.clear();
				}
				return &new_context.get();
			}
		}
		else
		{
			if (context.scope_decls.empty())
			{
				return &context;
			}
			else
			{
				new_context.emplace(context, ctx::parse_context::global_copy_t{});
				for (auto &decl_set : new_context->scope_decls)
				{
					decl_set.var_decls.clear();
				}
				return &new_context.get();
			}
		}
	}();

	auto const original_file_info = context_ptr->get_current_file_info();
	// this check is needed because of generic built-in functions like __builtin_slice_size
	if (func_body.src_tokens.pivot != nullptr)
	{
		auto const stmt_file_id = func_body.src_tokens.pivot->src_pos.file_id;
		if (original_file_info.file_id != stmt_file_id)
		{
			context_ptr->set_current_file(stmt_file_id);
		}
	}
	resolve_function_impl(func_stmt, func_body, *context_ptr);
	context_ptr->set_current_file_info(original_file_info);
}

static ast::function_body parse_function_body(
	lex::src_tokens src_tokens,
	bz::variant<ast::identifier, uint32_t> func_name_or_op_kind,
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	ast::function_body result = {};
	auto const open_paren = context.assert_token(stream, lex::token::paren_open);
	auto [param_stream, param_end] = get_expression_tokens<
		lex::token::paren_close
	>(stream, end, context);

	if (param_end != end && param_end->kind == lex::token::paren_close)
	{
		++stream; // ')'
	}
	else if (open_paren->kind == lex::token::paren_open)
	{
		context.report_paren_match_error(param_end, open_paren);
	}

	result.src_tokens = src_tokens;
	result.function_name_or_operator_kind = std::move(func_name_or_op_kind);
	while (param_stream != param_end)
	{
		auto const begin = param_stream;
		result.params.emplace_back(parse_decl_variable_id_and_type(
			param_stream, param_end,
			context, false
		));
		auto &param_decl = result.params.back();
		if (param_decl.get_id().values.empty())
		{
			param_decl.flags |= ast::decl_variable::maybe_unused;
		}
		if (param_stream != param_end)
		{
			context.assert_token(param_stream, lex::token::comma, lex::token::paren_close);
		}
		if (param_stream == begin)
		{
			context.report_error(param_stream);
			++param_stream;
		}
	}

	if (stream != end && stream->kind == lex::token::arrow)
	{
		++stream; // '->'
		auto const ret_type = get_expression_tokens<
			lex::token::curly_open
		>(stream, end, context);
		result.return_type = ast::make_unresolved_typespec(ret_type);
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

template<bool is_global>
ast::statement parse_decl_function_or_alias(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream != end);
	bz_assert(stream->kind == lex::token::kw_function);
	auto const begin = stream;
	++stream; // 'function'
	auto const id = context.assert_token(stream, lex::token::identifier);
	auto const src_tokens = id->kind == lex::token::identifier
		? lex::src_tokens{ begin, id, stream }
		: lex::src_tokens{ begin, begin, stream };
	if (stream->kind == lex::token::assign)
	{
		++stream; // '='
		auto const alias_expr = get_expression_tokens<>(stream, end, context);
		context.assert_token(stream, lex::token::semi_colon);
		auto func_id = is_global
			? context.make_qualified_identifier(id)
			: ast::make_identifier(id);
		return ast::make_decl_function_alias(
			src_tokens,
			std::move(func_id),
			ast::make_unresolved_expression({ alias_expr.begin, alias_expr.begin, alias_expr.end })
		);
	}
	else
	{
		auto const func_name = is_global
			? context.make_qualified_identifier(id)
			: ast::make_identifier(id);
		auto body = parse_function_body(src_tokens, std::move(func_name), stream, end, context);
		if (id->value == "main")
		{
			body.flags |= ast::function_body::main;
			body.flags |= ast::function_body::external_linkage;
		}

		if constexpr (is_global)
		{
			return ast::make_decl_function(context.make_qualified_identifier(id), std::move(body));
		}
		else
		{
			auto result = ast::make_decl_function(ast::make_identifier(id), std::move(body));
			bz_assert(result.is<ast::decl_function>());
			auto &func_decl = result.get<ast::decl_function>();
			func_decl.body.flags |= ast::function_body::local;
			resolve_function(result, func_decl.body, context);
			if (func_decl.body.state != ast::resolve_state::error)
			{
				context.add_local_function(func_decl);
			}
			return result;
		}
	}
}

template ast::statement parse_decl_function_or_alias<false>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template ast::statement parse_decl_function_or_alias<true>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template<bool is_global>
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

	if constexpr (is_global)
	{
		return ast::make_decl_operator(context.current_scope, op, std::move(body));
	}
	else
	{
		auto result = ast::make_decl_operator(context.current_scope, op, std::move(body));
		bz_assert(result.is<ast::decl_operator>());
		auto &op_decl = result.get<ast::decl_operator>();
		op_decl.body.flags |= ast::function_body::local;
		resolve_function(result, op_decl.body, context);
		if (op_decl.body.state != ast::resolve_state::error)
		{
			context.add_local_operator(op_decl);
		}
		return result;
	}
}

template ast::statement parse_decl_operator<false>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template ast::statement parse_decl_operator<true>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

static void resolve_type_info_symbol_impl(
	ast::type_info &info,
	[[maybe_unused]] ctx::parse_context &context
)
{
	if (info.type_name.is_qualified)
	{
		info.symbol_name = bz::format("struct.{}", info.type_name.format_as_unqualified());
	}
	else
	{
		info.symbol_name = bz::format("non_global_struct.{}", info.type_name.format_as_unqualified());
	}
	info.state = ast::resolve_state::symbol;
}

void resolve_type_info_symbol(
	ast::type_info &info,
	ctx::parse_context &context
)
{
	if (info.state >= ast::resolve_state::symbol || info.state == ast::resolve_state::error)
	{
		return;
	}
	bz_assert(info.state != ast::resolve_state::resolving_symbol);

	auto const original_file_info = context.get_current_file_info();
	auto const stmt_file_id = info.src_tokens.pivot->src_pos.file_id;
	if (original_file_info.file_id != stmt_file_id)
	{
		context.set_current_file(stmt_file_id);
	}
	resolve_type_info_symbol_impl(info, context);
	context.set_current_file_info(original_file_info);
}

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
		}
	}
	else
	{
		++stream; // '.'
	}

	bz_assert(stream != end);
	auto const id = context.assert_token(stream, lex::token::identifier);
	if (id->kind != lex::token::identifier)
	{
		return ast::statement();
	}
	bz_assert(stream != end);
	context.assert_token(stream, lex::token::colon);
	auto type = parse_expression(stream, end, context, no_assign);
	context.assert_token(stream, lex::token::semi_colon);
	consteval_try(type, context);
	if (type.not_error() && !type.has_consteval_succeeded())
	{
		context.report_error(type, "struct member type must be a constant expression", get_consteval_fail_notes(type));
		return ast::statement();
	}
	else if (type.not_error() && !type.is_typename())
	{
		context.report_error(type, "expected a type");
		return ast::statement();
	}
	else if (type.is_error())
	{
		return ast::statement();
	}
	else if (!context.is_instantiable(type.get_typename()))
	{
		context.report_error(type, "struct member type is not instantiable");
		return ast::statement();
	}
	else
	{
		auto result = ast::make_decl_variable(
			lex::src_tokens{ begin_token, id, stream },
			lex::token_range{},
			ast::var_id_and_type(ast::make_identifier(id), std::move(type.get_typename()))
		);
		auto &var_decl = result.get<ast::decl_variable>();
		var_decl.flags |= ast::decl_variable::member;
		return result;
	}
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

static void add_type_info_members(
	ast::type_info &info,
	ctx::parse_context &context
)
{
	auto &info_body = info.body.get<bz::vector<ast::statement>>();
	for (auto &stmt : info_body)
	{
		if (stmt.is<ast::decl_function>())
		{
			auto &body = stmt.get<ast::decl_function>().body;
			if (body.is_destructor())
			{
				if (info.destructor != nullptr)
				{
					auto const type_name = ast::type_info::decode_symbol_name(info.symbol_name);
					context.report_error(
						body.src_tokens, bz::format("redefinition of destructor for type '{}'", type_name),
						{ context.make_note(info.destructor->src_tokens, "previously defined here") }
					);
				}

				body.constructor_or_destructor_of = &info;
				info.destructor = &body;
			}
			else if (body.is_constructor())
			{
				if (body.return_type.is<ast::ts_unresolved>())
				{
					auto const tokens = body.return_type.get<ast::ts_unresolved>().tokens;
					auto const constructor_of_type = ast::type_info::decode_symbol_name(info.symbol_name);
					context.report_error(
						{ tokens.begin, tokens.begin, tokens.end },
						"a return type cannot be provided for a constructor",
						{ context.make_note(
							body.src_tokens,
							bz::format("in constructor for type '{}'", constructor_of_type)
						) }
					);
				}

				body.constructor_or_destructor_of = &info;
				info.constructors.push_back(&body);
			}
		}
		else if (stmt.is<ast::decl_variable>())
		{
			auto &var_decl = stmt.get<ast::decl_variable>();
			if (var_decl.is_member())
			{
				info.member_variables.push_back(&var_decl);
			}
		}
	}

	for (auto const member : info.member_variables)
	{
		resolve_variable(*member, context);
	}
}

static void resolve_type_info_impl(
	ast::type_info &info,
	ctx::parse_context &context
)
{
	if (info.state < ast::resolve_state::symbol)
	{
		resolve_type_info_symbol_impl(info, context);
	}
	if (info.state == ast::resolve_state::error || info.kind == ast::type_info::forward_declaration)
	{
		return;
	}

	info.state = ast::resolve_state::resolving_all;
	bz_assert(info.body.is<lex::token_range>());
	auto [stream, end] = info.body.get<lex::token_range>();

	info.body.emplace<bz::vector<ast::statement>>();
	auto &info_body = info.body.get<bz::vector<ast::statement>>();
	info_body = parse_struct_body_statements(stream, end, context);

	add_type_info_members(info, context);

	if (info.state == ast::resolve_state::error)
	{
		return;
	}
	else
	{
		info.state = ast::resolve_state::all;
	}

	for (auto &stmt : info_body)
	{
		resolve_global_statement(stmt, context);
	}
}

void resolve_type_info(
	ast::type_info &info,
	ctx::parse_context &context
)
{
	if (info.state >= ast::resolve_state::all || info.state == ast::resolve_state::error)
	{
		return;
	}
	else if (info.state == ast::resolve_state::resolving_all)
	{
		context.report_circular_dependency_error(info);
		info.state = ast::resolve_state::error;
		return;
	}

	auto const original_file_info = context.get_current_file_info();
	auto const stmt_file_id = info.src_tokens.pivot->src_pos.file_id;
	if (original_file_info.file_id != stmt_file_id)
	{
		context.set_current_file(stmt_file_id);
	}
	resolve_type_info_impl(info, context);
	context.set_current_file_info(original_file_info);
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

	if (stream != end && stream->kind == lex::token::curly_open)
	{
		++stream; // '{'
		auto const range = get_tokens_in_curly<>(stream, end, context);
		return ast::make_decl_struct(src_tokens, context.make_qualified_identifier(id), range);
	}
	else if (stream == end || stream->kind != lex::token::semi_colon)
	{
		context.assert_token(stream, lex::token::curly_open, lex::token::semi_colon);
		return {};
	}
	else
	{
		++stream; // ';'
		return ast::make_decl_struct(src_tokens, context.make_qualified_identifier(id));
	}
}

template<bool is_global>
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

	if constexpr (is_global)
	{
		return result;
	}
	else
	{
		bz_assert(result.is<ast::decl_struct>());
		auto &struct_decl = result.get<ast::decl_struct>();
		context.add_to_resolve_queue({}, struct_decl.info);
		resolve_type_info(struct_decl.info, context);
		context.pop_resolve_queue();
		return result;
	}
}

template ast::statement parse_decl_struct<false>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template ast::statement parse_decl_struct<true>(
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

template<bool is_global>
ast::statement parse_attribute_statement(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	bz_assert(stream != end);
	bz_assert(stream->kind == lex::token::at);
	bz::vector<ast::attribute> attributes = {};
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
			attributes.emplace_back(name, args_range, bz::vector<ast::expression>{});
		}
		else
		{
			attributes.emplace_back(name, lex::token_range{}, bz::vector<ast::expression>{});
		}
	}

	constexpr auto parser_fn = is_global ? &parse_global_statement : &parse_local_statement;
	auto statement = parser_fn(stream, end, context);
	if constexpr (is_global)
	{
		statement.set_attributes_without_resolve(std::move(attributes));
	}
	else
	{
		statement.set_attributes_without_resolve(std::move(attributes));
		resolve_attributes(statement, context);
	}
	return statement;
}

// explicit intantiation of parse_attribute_statement, because it is referenced in parse_common.h
template ast::statement parse_attribute_statement<false>(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template ast::statement parse_attribute_statement<true>(
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
				func_decl.body.flags |= ast::function_body::external_linkage;
			},
			[](ast::decl_operator &op_decl) {
				op_decl.body.flags |= ast::function_body::module_export;
				op_decl.body.flags |= ast::function_body::external_linkage;
			},
			[](ast::decl_function_alias &alias_decl) {
				alias_decl.is_export = true;
			},
			[](ast::decl_type_alias &alias_decl) {
				alias_decl.is_export = true;
			},
			[](ast::decl_variable &var_decl) {
				var_decl.flags |= ast::decl_variable::module_export;
				var_decl.flags |= ast::decl_variable::external_linkage;
			},
			[](ast::decl_struct &struct_decl) {
				struct_decl.info.is_export = true;
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
	auto const begin = stream;
	++stream; // 'while'
	auto condition = parse_parenthesized_condition(stream, end, context);
	if (condition.not_error())
	{
		auto const [type, _] = condition.get_expr_type_and_kind();
		if (
			auto const type_without_const = ast::remove_const_or_consteval(type);
			!type_without_const.is<ast::ts_base_type>()
			|| type_without_const.get<ast::ts_base_type>().info->kind != ast::type_info::bool_
		)
		{
			context.report_error(condition, "condition for while statement must have type 'bool'");
		}
	}
	auto body = parse_local_statement(stream, end, context);
	return ast::make_stmt_while(
		lex::token_range{ begin, stream },
		std::move(condition), std::move(body)
	);
}

static ast::statement parse_stmt_for_impl(
	lex::token_pos &stream, lex::token_pos end,
	lex::token_pos open_paren,
	ctx::parse_context &context
)
{
	// 'for' and '(' have already been consumed
	context.add_scope();

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

	if (condition.not_null())
	{
		auto bool_type = ast::make_base_type_typespec({}, context.get_builtin_type_info(ast::type_info::bool_));
		context.match_expression_to_type(condition, bool_type);
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

	auto body = parse_local_statement(stream, end, context);

	context.remove_scope();

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

	context.add_scope();

	auto range_var_type = ast::make_auto_typespec(nullptr);
	range_var_type.add_layer<ast::ts_auto_reference_const>();
	auto const range_expr_src_tokens = range_expr.src_tokens;
	auto range_var_decl_stmt = ast::make_decl_variable(
		range_expr_src_tokens,
		lex::token_range{},
		ast::var_id_and_type(ast::identifier{}, std::move(range_var_type)),
		std::move(range_expr)
	);
	bz_assert(range_var_decl_stmt.is<ast::decl_variable>());
	auto &range_var_decl = range_var_decl_stmt.get<ast::decl_variable>();
	range_var_decl.id_and_type.id.tokens = { range_expr_src_tokens.begin, range_expr_src_tokens.end };
	range_var_decl.id_and_type.id.values = { "" };
	range_var_decl.id_and_type.id.is_qualified = false;
	resolve_variable(range_var_decl, context);
	range_var_decl.flags |= ast::decl_variable::used;
	context.add_local_variable(range_var_decl);

	if (range_var_decl.id_and_type.var_type.is_empty())
	{
		context.report_error(range_expr.src_tokens, "invalid range in foreach loop");
		context.remove_scope();
		return {};
	}

	auto range_begin_expr = [&]() {
		if (range_var_decl.id_and_type.var_type.is_empty())
		{
			return ast::make_error_expression(range_expr_src_tokens);
		}
		auto const type_kind = range_var_decl.id_and_type.var_type.is<ast::ts_lvalue_reference>()
			? ast::expression_type_kind::lvalue_reference
			: ast::expression_type_kind::lvalue;
		auto const type = ast::remove_lvalue_reference(range_var_decl.id_and_type.var_type);

		auto range_var_expr = ast::make_dynamic_expression(
			range_expr_src_tokens,
			type_kind, type,
			ast::make_expr_identifier(ast::identifier{}, &range_var_decl)
		);
		return context.make_universal_function_call_expression(
			range_expr_src_tokens,
			std::move(range_var_expr),
			ast::make_identifier("begin"), {}
		);
	}();

	auto iter_var_decl_stmt = ast::make_decl_variable(
		range_expr_src_tokens,
		lex::token_range{},
		ast::var_id_and_type(ast::identifier{}, ast::make_auto_typespec(nullptr)),
		std::move(range_begin_expr)
	);
	bz_assert(iter_var_decl_stmt.is<ast::decl_variable>());
	auto &iter_var_decl = iter_var_decl_stmt.get<ast::decl_variable>();
	iter_var_decl.id_and_type.id.tokens = { range_expr_src_tokens.begin, range_expr_src_tokens.end };
	iter_var_decl.id_and_type.id.values = { "" };
	iter_var_decl.id_and_type.id.is_qualified = false;
	resolve_variable(iter_var_decl, context);
	iter_var_decl.flags |= ast::decl_variable::used;
	context.add_local_variable(iter_var_decl);

	auto range_end_expr = [&]() {
		if (range_var_decl.id_and_type.var_type.is_empty())
		{
			return ast::make_error_expression(range_expr_src_tokens);
		}
		auto const type_kind = range_var_decl.id_and_type.var_type.is<ast::ts_lvalue_reference>()
			? ast::expression_type_kind::lvalue_reference
			: ast::expression_type_kind::lvalue;
		auto const type = ast::remove_lvalue_reference(range_var_decl.id_and_type.var_type);

		auto range_var_expr = ast::make_dynamic_expression(
			range_expr_src_tokens,
			type_kind, type,
			ast::make_expr_identifier(ast::identifier{}, &range_var_decl)
		);
		return context.make_universal_function_call_expression(
			range_expr_src_tokens,
			std::move(range_var_expr),
			ast::make_identifier("end"), {}
		);
	}();

	auto end_var_decl_stmt = ast::make_decl_variable(
		range_expr_src_tokens,
		lex::token_range{},
		ast::var_id_and_type(ast::identifier{}, ast::make_auto_typespec(nullptr)),
		std::move(range_end_expr)
	);
	bz_assert(end_var_decl_stmt.is<ast::decl_variable>());
	auto &end_var_decl = end_var_decl_stmt.get<ast::decl_variable>();
	end_var_decl.id_and_type.id.tokens = { range_expr_src_tokens.begin, range_expr_src_tokens.end };
	end_var_decl.id_and_type.id.values = { "" };
	end_var_decl.id_and_type.id.is_qualified = false;
	resolve_variable(end_var_decl, context);
	end_var_decl.flags |= ast::decl_variable::used;
	context.add_local_variable(end_var_decl);

	auto condition = [&]() {
		if (iter_var_decl.id_and_type.var_type.is_empty() || end_var_decl.id_and_type.var_type.is_empty())
		{
			return ast::make_error_expression(range_expr_src_tokens);
		}
		auto iter_var_expr = ast::make_dynamic_expression(
			range_expr_src_tokens,
			ast::expression_type_kind::lvalue, iter_var_decl.id_and_type.var_type,
			ast::make_expr_identifier(ast::identifier{}, &iter_var_decl)
		);
		auto end_var_expr = ast::make_dynamic_expression(
			range_expr_src_tokens,
			ast::expression_type_kind::lvalue, end_var_decl.id_and_type.var_type,
			ast::make_expr_identifier(ast::identifier{}, &end_var_decl)
		);
		return context.make_binary_operator_expression(
			range_expr_src_tokens,
			lex::token::not_equals,
			std::move(iter_var_expr),
			std::move(end_var_expr)
		);
	}();

	auto iteration = [&]() {
		if (iter_var_decl.id_and_type.var_type.is_empty())
		{
			return ast::make_error_expression(range_expr_src_tokens);
		}
		auto iter_var_expr = ast::make_dynamic_expression(
			range_expr_src_tokens,
			ast::expression_type_kind::lvalue, iter_var_decl.id_and_type.var_type,
			ast::make_expr_identifier(ast::identifier{}, &iter_var_decl)
		);
		return context.make_unary_operator_expression(
			range_expr_src_tokens,
			lex::token::plus_plus,
			std::move(iter_var_expr)
		);
	}();

	context.add_scope();

	auto iter_deref_expr = [&]() {
		if (iter_var_decl.id_and_type.var_type.is_empty())
		{
			return ast::make_error_expression(range_expr_src_tokens);
		}
		auto iter_var_expr = ast::make_dynamic_expression(
			range_expr_src_tokens,
			ast::expression_type_kind::lvalue, iter_var_decl.id_and_type.var_type,
			ast::make_expr_identifier(ast::identifier{}, &iter_var_decl)
		);
		return context.make_unary_operator_expression(
			range_expr_src_tokens,
			lex::token::dereference,
			std::move(iter_var_expr)
		);
	}();
	bz_assert(iter_deref_var_decl_stmt.is<ast::decl_variable>());
	auto &iter_deref_var_decl = iter_deref_var_decl_stmt.get<ast::decl_variable>();
	iter_deref_var_decl.init_expr = std::move(iter_deref_expr);
	resolve_variable(iter_deref_var_decl, context);
	context.add_local_variable(iter_deref_var_decl);

	auto body = parse_local_statement(stream, end, context);

	context.remove_scope();
	context.remove_scope();

	return ast::make_stmt_foreach(
		std::move(range_var_decl_stmt),
		std::move(iter_var_decl_stmt),
		std::move(end_var_decl_stmt),
		std::move(iter_deref_var_decl_stmt),
		std::move(condition),
		std::move(iteration),
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
	++stream; // 'return'
	if (stream != end && stream->kind == lex::token::semi_colon)
	{
		if (!context.current_function->return_type.is<ast::ts_void>())
		{
			context.report_error(stream, "a function with a non-void return type must return a value");
		}
		return ast::make_stmt_return();
	}
	auto expr = parse_expression(stream, end, context, precedence{});
	context.assert_token(stream, lex::token::semi_colon);
	bz_assert(context.current_function != nullptr);
	bz_assert(ast::is_complete(context.current_function->return_type));
	context.match_expression_to_type(expr, context.current_function->return_type);
	return ast::make_stmt_return(std::move(expr));
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
		auto const original_file_info = context.get_current_file_info();
		if (stream->src_pos.file_id != original_file_info.file_id)
		{
			context.set_current_file(stream->src_pos.file_id);
		}
		auto result = parse_fn(stream, end, context);
		context.set_current_file_info(original_file_info);
		return result;
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

static constexpr auto parse_local_statement_without_semi_colon_default_parser = +[](
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	auto const begin = stream;
	auto const result = ast::make_stmt_expression(
		parse_expression_without_semi_colon(stream, end, context)
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
		local_statement_parsers, parse_local_statement_without_semi_colon_default_parser
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

static void apply_attribute(
	ast::stmt_while &,
	ast::attribute const &attribute,
	ctx::parse_context &context
)
{
	context.report_warning(
		ctx::warning_kind::unknown_attribute,
		attribute.name,
		bz::format("unknown attribute '{}'", attribute.name->value)
	);
}

static void apply_attribute(
	ast::stmt_for &,
	ast::attribute const &attribute,
	ctx::parse_context &context
)
{
	context.report_warning(
		ctx::warning_kind::unknown_attribute,
		attribute.name,
		bz::format("unknown attribute '{}'", attribute.name->value)
	);
}

static void apply_attribute(
	ast::stmt_foreach &,
	ast::attribute const &attribute,
	ctx::parse_context &context
)
{
	context.report_warning(
		ctx::warning_kind::unknown_attribute,
		attribute.name,
		bz::format("unknown attribute '{}'", attribute.name->value)
	);
}

static void apply_attribute(
	ast::stmt_return &,
	ast::attribute const &attribute,
	ctx::parse_context &context
)
{
	context.report_warning(
		ctx::warning_kind::unknown_attribute,
		attribute.name,
		bz::format("unknown attribute '{}'", attribute.name->value)
	);
}

static void apply_attribute(
	ast::stmt_no_op &,
	ast::attribute const &attribute,
	ctx::parse_context &context
)
{
	context.report_warning(
		ctx::warning_kind::unknown_attribute,
		attribute.name,
		bz::format("unknown attribute '{}'", attribute.name->value)
	);
}

static void apply_attribute(
	ast::stmt_static_assert &,
	ast::attribute const &attribute,
	ctx::parse_context &context
)
{
	context.report_warning(
		ctx::warning_kind::unknown_attribute,
		attribute.name,
		bz::format("unknown attribute '{}'", attribute.name->value)
	);
}

static void apply_attribute(
	ast::stmt_expression &,
	ast::attribute const &attribute,
	ctx::parse_context &context
)
{
	context.report_warning(
		ctx::warning_kind::unknown_attribute,
		attribute.name,
		bz::format("unknown attribute '{}'", attribute.name->value)
	);
}

static void apply_maybe_unused_to_var_decl(ast::decl_variable &var_decl)
{
	var_decl.flags |= ast::decl_variable::maybe_unused;
	for (auto &decl : var_decl.tuple_decls)
	{
		apply_maybe_unused_to_var_decl(decl);
	}
}

static void apply_attribute(
	ast::decl_variable &var_decl,
	ast::attribute &attribute,
	ctx::parse_context &context
)
{
	if (attribute.name->value == "maybe_unused")
	{
		if (attribute.args.size() != 0)
		{
			context.report_error(
				{ attribute.arg_tokens.begin, attribute.arg_tokens.begin, attribute.arg_tokens.end },
				"@maybe_unused expects no arguments"
			);
		}
		apply_maybe_unused_to_var_decl(var_decl);
	}
	else if (attribute.name->value == "comptime_error_checking")
	{
		if (attribute.args.size() != 1)
		{
			context.report_error(attribute.name, "@comptime_error_checking expects exactly one argument");
			return;
		}

		{
			consteval_try(attribute.args[0], context);
			auto const [type, _] = attribute.args[0].get_expr_type_and_kind();
			auto const type_without_const = ast::remove_const_or_consteval(type);
			if (
				!type_without_const.is<ast::ts_base_type>()
				|| type_without_const.get<ast::ts_base_type>().info->kind != ast::type_info::str_
			)
			{
				context.report_error(attribute.args[0], "kind in @comptime_error_checking must have type 'str'");
				return;
			}
		}

		auto const kind = attribute.args[0]
			.get<ast::constant_expression>().value
			.get<ast::constant_value::string>().as_string_view();

		if (!context.global_ctx.add_comptime_checking_variable(kind, &var_decl))
		{
			context.report_error(attribute.args[0], bz::format("invalid kind '{}' for @comptime_error_checking", kind));
		}
	}
	else if (attribute.name->value == "no_runtime_emit")
	{
		if (attribute.args.size() != 0)
		{
			context.report_error(
				{ attribute.arg_tokens.begin, attribute.arg_tokens.begin, attribute.arg_tokens.end },
				"@no_runtime_emit expects no arguments"
			);
		}
		var_decl.flags |= ast::decl_variable::no_runtime_emit;
	}
	else
	{
		context.report_warning(
			ctx::warning_kind::unknown_attribute,
			attribute.name,
			bz::format("unknown attribute '{}'", attribute.name->value)
		);
	}
}

static void apply_extern(
	ast::function_body &func_body,
	ast::attribute const &attribute,
	ctx::parse_context &context
)
{
	bz_assert(attribute.name->value == "extern");
	if (attribute.args.size() != 1 && attribute.args.size() != 2)
	{
		context.report_error(attribute.name, "@extern expects one or two arguments");
		return;
	}

	bool good = true;
	// symbol name
	{
		auto const [type, _] = attribute.args[0].get_expr_type_and_kind();
		auto const type_without_const = ast::remove_const_or_consteval(type);
		if (
			!type_without_const.is<ast::ts_base_type>()
			|| type_without_const.get<ast::ts_base_type>().info->kind != ast::type_info::str_
		)
		{
			context.report_error(attribute.args[0], "symbol name in @extern must have type 'str'");
			good = false;
		}
	}

	// calling convention
	if (attribute.args.size() == 2)
	{
		auto const [type, _] = attribute.args[1].get_expr_type_and_kind();
		auto const type_without_const = ast::remove_const_or_consteval(type);
		if (
			!type_without_const.is<ast::ts_base_type>()
			|| type_without_const.get<ast::ts_base_type>().info->kind != ast::type_info::str_
		)
		{
			context.report_error(attribute.args[0], "calling convention in @extern must have type 'str'");
			good = false;
		}
	}

	if (!good)
	{
		return;
	}

	auto const extern_name = attribute.args[0]
		.get<ast::constant_expression>().value
		.get<ast::constant_value::string>().as_string_view();
	auto const cc = [&]() {
		if (attribute.args.size() != 2)
		{
			return abi::calling_convention::c;
		}
		auto const cc_name = attribute.args[1]
			.get<ast::constant_expression>().value
			.get<ast::constant_value::string>().as_string_view();
		if (cc_name == "c")
		{
			return abi::calling_convention::c;
		}
		else if (cc_name == "std")
		{
			return abi::calling_convention::std;
		}
		else if (cc_name == "fast")
		{
			return abi::calling_convention::fast;
		}
		else
		{
			context.report_error(
				attribute.args[1],
				bz::format("unknown calling convention '{}'", cc_name)
			);
			return abi::calling_convention::c;
		}
	}();
	func_body.flags |= ast::function_body::external_linkage;
	func_body.symbol_name = extern_name;
	func_body.cc = cc;
	for (auto const &specialization : func_body.generic_specializations)
	{
		specialization->flags |= ast::function_body::external_linkage;
		specialization->symbol_name = extern_name;
		specialization->cc = cc;
	}
}

static void apply_symbol_name(
	ast::function_body &func_body,
	ast::attribute &attribute,
	ctx::parse_context &context
)
{
	bz_assert(attribute.name->value == "symbol_name");
	if (attribute.args.size() != 1)
	{
		context.report_error(attribute.name, "@symbol_name expects exactly one argument");
		return;
	}

	bz_assert(func_body.state >= ast::resolve_state::parameters);
	if (func_body.is_generic())
	{
		context.report_error(attribute.name, "@symbol_name cannot be applied to generic functions");
		return;
	}

	// symbol name
	{
		consteval_try(attribute.args[0], context);
		auto const [type, _] = attribute.args[0].get_expr_type_and_kind();
		auto const type_without_const = ast::remove_const_or_consteval(type);
		if (
			!type_without_const.is<ast::ts_base_type>()
			|| type_without_const.get<ast::ts_base_type>().info->kind != ast::type_info::str_
		)
		{
			context.report_error(attribute.args[0], "name in @symbol_name must have type 'str'");
			return;
		}
	}

	auto const symbol_name = attribute.args[0]
		.get<ast::constant_expression>().value
		.get<ast::constant_value::string>().as_string_view();

	func_body.symbol_name = symbol_name;
}

static void apply_no_comptime_checking(
	ast::function_body &func_body,
	ast::attribute &attribute,
	ctx::parse_context &context
)
{
	if (!attribute.args.empty())
	{
		context.report_error(attribute.name, "@no_comptime_checking expects no arguments");
	}

	func_body.flags |= ast::function_body::no_comptime_checking;
	for (auto const &specialization : func_body.generic_specializations)
	{
		specialization->flags |= ast::function_body::no_comptime_checking;
	}
}

static void apply_comptime_error_checking(
	ast::function_body &func_body,
	ast::attribute &attribute,
	ctx::parse_context &context
)
{
	if (attribute.args.size() != 1)
	{
		context.report_error(attribute.name, "@comptime_error_checking expects exactly one argument");
		return;
	}

	{
		consteval_try(attribute.args[0], context);
		auto const [type, _] = attribute.args[0].get_expr_type_and_kind();
		auto const type_without_const = ast::remove_const_or_consteval(type);
		if (
			!type_without_const.is<ast::ts_base_type>()
			|| type_without_const.get<ast::ts_base_type>().info->kind != ast::type_info::str_
		)
		{
			context.report_error(attribute.args[0], "kind in @comptime_error_checking must have type 'str'");
			return;
		}
	}

	auto const kind = attribute.args[0]
		.get<ast::constant_expression>().value
		.get<ast::constant_value::string>().as_string_view();

	if (!context.global_ctx.add_comptime_checking_function(kind, &func_body))
	{
		context.report_error(attribute.args[0], bz::format("invalid kind '{}' for @comptime_error_checking", kind));
	}
	func_body.flags |= ast::function_body::no_comptime_checking;
}

static void apply_builtin(
	ast::function_body &func_body,
	ast::attribute &attribute,
	ctx::parse_context &context
)
{
	if (attribute.args.size() != 1)
	{
		context.report_error(attribute.name, "@__builtin expects exactly one argument");
		return;
	}

	{
		consteval_try(attribute.args[0], context);
		auto const [type, _] = attribute.args[0].get_expr_type_and_kind();
		auto const type_without_const = ast::remove_const_or_consteval(type);
		if (
			!type_without_const.is<ast::ts_base_type>()
			|| type_without_const.get<ast::ts_base_type>().info->kind != ast::type_info::str_
		)
		{
			context.report_error(attribute.args[0], "kind in @__builtin must have type 'str'");
			return;
		}
	}

	auto const kind = attribute.args[0]
		.get<ast::constant_expression>().value
		.get<ast::constant_value::string>().as_string_view();

	if (!context.global_ctx.add_builtin_function(kind, &func_body))
	{
		context.report_error(attribute.args[0], bz::format("invalid kind '{}' for @__builtin", kind));
	}
	func_body.flags |= ast::function_body::intrinsic;
}

static void apply_attribute(
	ast::decl_function &func_decl,
	ast::attribute &attribute,
	ctx::parse_context &context
)
{
	auto const attr_name = attribute.name->value;
	if (attr_name == "extern")
	{
		apply_extern(func_decl.body, attribute, context);
	}
	else if (attr_name == "cdecl")
	{
		if (attribute.args.size() != 0)
		{
			context.report_error(
				{ attribute.arg_tokens.begin, attribute.arg_tokens.begin, attribute.arg_tokens.end },
				"@cdecl expects no arguments"
			);
		}
		if (func_decl.body.cc != abi::calling_convention::bozon)
		{
			context.report_error(
				attribute.name,
				"calling convention has already been set for this function"
			);
		}
		else
		{
			func_decl.body.cc = abi::calling_convention::c;
			for (auto const &specialization : func_decl.body.generic_specializations)
			{
				specialization->cc = abi::calling_convention::c;
			}
		}
	}
	else if (attr_name == "symbol_name")
	{
		apply_symbol_name(func_decl.body, attribute, context);
	}
	else if (attr_name == "no_comptime_checking")
	{
		apply_no_comptime_checking(func_decl.body, attribute, context);
	}
	else if (attr_name == "comptime_error_checking")
	{
		apply_comptime_error_checking(func_decl.body, attribute, context);
	}
	else if (attr_name == "__builtin")
	{
		apply_builtin(func_decl.body, attribute, context);
	}
	else
	{
		context.report_warning(
			ctx::warning_kind::unknown_attribute,
			attribute.name,
			bz::format("unknown attribute '{}'", attr_name)
		);
	}
}

static void apply_attribute(
	ast::decl_operator &op_decl,
	ast::attribute &attribute,
	ctx::parse_context &context
)
{
	auto const attr_name = attribute.name->value;
	if (attr_name == "extern")
	{
		apply_extern(op_decl.body, attribute, context);
	}
	else if (attr_name == "symbol_name")
	{
		apply_symbol_name(op_decl.body, attribute, context);
	}
	else if (attr_name == "no_comptime_checking")
	{
		apply_no_comptime_checking(op_decl.body, attribute, context);
	}
	else if (attr_name == "comptime_error_checking")
	{
		apply_comptime_error_checking(op_decl.body, attribute, context);
	}
	else if (attr_name == "builtin")
	{
		apply_builtin(op_decl.body, attribute, context);
	}
	else
	{
		context.report_warning(
			ctx::warning_kind::unknown_attribute,
			attribute.name,
			bz::format("unknown attribute '{}'", attr_name)
		);
	}
}

static void apply_attribute(
	ast::decl_function_alias &,
	ast::attribute const &attribute,
	ctx::parse_context &context
)
{
	context.report_warning(
		ctx::warning_kind::unknown_attribute,
		attribute.name,
		bz::format("unknown attribute '{}'", attribute.name->value)
	);
}

static void apply_attribute(
	ast::decl_type_alias &,
	ast::attribute const &attribute,
	ctx::parse_context &context
)
{
	context.report_warning(
		ctx::warning_kind::unknown_attribute,
		attribute.name,
		bz::format("unknown attribute '{}'", attribute.name->value)
	);
}

static void apply_attribute(
	ast::decl_struct &,
	ast::attribute const &attribute,
	ctx::parse_context &context
)
{
	context.report_warning(
		ctx::warning_kind::unknown_attribute,
		attribute.name,
		bz::format("unknown attribute '{}'", attribute.name->value)
	);
}

static void apply_attribute(
	ast::decl_import &,
	ast::attribute const &attribute,
	ctx::parse_context &context
)
{
	context.report_warning(
		ctx::warning_kind::unknown_attribute,
		attribute.name,
		bz::format("unknown attribute '{}'", attribute.name->value)
	);
}


static void apply_attribute(
	ast::statement_view stmt,
	ast::attribute &attribute,
	ctx::parse_context &context
)
{
	stmt.visit([&](auto &s) {
		apply_attribute(s, attribute, context);
	});
}

static void resolve_attributes(
	ast::statement_view stmt,
	ctx::parse_context &context
)
{
	auto const attributes = stmt.get_attributes();
	for (auto &attribute : attributes)
	{
		if (attribute.args.size() != 0)
		{
			// attributes have already been resolved
			return;
		}
		auto [stream, end] = attribute.arg_tokens;
		if (stream != end)
		{
			attribute.args = parse_expression_comma_list(stream, end, context);
			if (stream != end)
			{
				context.report_error({ stream, stream, end });
			}

			for (auto &arg : attribute.args)
			{
				parse::consteval_try(arg, context);
				if (arg.not_error() && !arg.is<ast::constant_expression>())
				{
					context.report_error(arg, "attribute argument must be a constant expression");
				}
			}
		}

		apply_attribute(stmt, attribute, context);
	}
}

void resolve_global_statement(
	ast::statement &stmt,
	ctx::parse_context &context
)
{
	stmt.visit(bz::overload{
		[&](ast::decl_function &func_decl) {
			context.add_to_resolve_queue({}, func_decl.body);
			resolve_function(stmt, func_decl.body, context);
			context.pop_resolve_queue();
		},
		[&](ast::decl_operator &op_decl) {
			context.add_to_resolve_queue({}, op_decl.body);
			resolve_function(stmt, op_decl.body, context);
			context.pop_resolve_queue();
		},
		[&](ast::decl_function_alias &alias_decl) {
			context.add_to_resolve_queue({}, alias_decl);
			resolve_function_alias(alias_decl, context);
			context.pop_resolve_queue();
		},
		[&](ast::decl_type_alias &alias_decl) {
			context.add_to_resolve_queue({}, alias_decl);
			resolve_type_alias(alias_decl, context);
			context.pop_resolve_queue();
		},
		[&](ast::decl_struct &struct_decl) {
			context.add_to_resolve_queue({}, struct_decl.info);
			resolve_type_info(struct_decl.info, context);
			context.pop_resolve_queue();
		},
		[&](ast::decl_variable &var_decl) {
			context.add_to_resolve_queue({}, var_decl);
			resolve_variable(var_decl, context);
			context.pop_resolve_queue();
			if (!var_decl.is_member() && var_decl.state != ast::resolve_state::error && var_decl.init_expr.not_null())
			{
				consteval_try(var_decl.init_expr, context);
				if (var_decl.init_expr.not_error() && !var_decl.init_expr.has_consteval_succeeded())
				{
					context.report_error(
						var_decl.src_tokens,
						"a global variable must be initialized by a constant expression",
						get_consteval_fail_notes(var_decl.init_expr)
					);
				}
			}
		},
		[&](ast::stmt_static_assert &static_assert_stmt) {
			resolve_stmt_static_assert(static_assert_stmt, context);
		},
		[](ast::decl_import) {
			// nothing
		},
		[&](auto &) {
			bz_unreachable;
		}
	});
	resolve_attributes(stmt, context);
}

} // namespace parse
