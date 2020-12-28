#include "parse_context.h"
#include "global_context.h"
#include "built_in_operators.h"
#include "lex/lexer.h"
#include "escape_sequences.h"
#include "parse/statement_parser.h"

namespace ctx
{

parse_context::parse_context(global_context &_global_ctx)
	: global_ctx(_global_ctx),
	  global_decls(nullptr),
	  scope_decls{},
	  resolve_queue{}
{}

void parse_context::report_error(lex::token_pos it) const
{
	this->global_ctx.report_error(ctx::make_error(it));
}

void parse_context::report_error(
	lex::token_pos it,
	bz::u8string message,
	bz::vector<ctx::note> notes, bz::vector<ctx::suggestion> suggestions
) const
{
	this->global_ctx.report_error(ctx::make_error(
		it, std::move(message),
		std::move(notes), std::move(suggestions)
	));
}

void parse_context::report_error(
	lex::src_tokens src_tokens,
	bz::u8string message,
	bz::vector<ctx::note> notes, bz::vector<ctx::suggestion> suggestions
) const
{
	this->global_ctx.report_error(ctx::make_error(
		src_tokens.begin, src_tokens.pivot, src_tokens.end, std::move(message),
		std::move(notes), std::move(suggestions)
	));
}

void parse_context::report_paren_match_error(
	lex::token_pos it, lex::token_pos open_paren_it,
	bz::vector<ctx::note> notes, bz::vector<ctx::suggestion> suggestions
) const
{
	auto const message = [&]() {
		switch (open_paren_it->kind)
		{
		case lex::token::paren_open:
			return it->kind == lex::token::eof
				? bz::u8string("expected closing ) before end-of-file")
				: bz::format("expected closing ) before '{}'", it->value);
		case lex::token::square_open:
			return it->kind == lex::token::eof
				? bz::u8string("expected closing ] before end-of-file")
				: bz::format("expected closing ] before '{}'", it->value);
		case lex::token::curly_open:
			return it->kind == lex::token::eof
				? bz::u8string("expected closing } before end-of-file")
				: bz::format("expected closing } before '{}'", it->value);
		default:
			bz_unreachable;
		}
	}();
	notes.push_front(this->make_paren_match_note(it, open_paren_it));
	this->report_error(
		it, message,
		std::move(notes), std::move(suggestions)
	);
}

void parse_context::report_circular_dependency_error(ast::function_body &func_body) const
{
	bz::vector<note> notes = {};
	int count = 0;
	for (auto const &dep : bz::reversed(this->resolve_queue))
	{
		if (dep.requested == &func_body)
		{
			++count;
			if (count == 2)
			{
				break;
			}
		}
		notes.emplace_back(make_note(dep.requester, "required from here"));
	}

	this->report_error(
		func_body.src_tokens,
		bz::format("circular dependency encountered while resolving {}", func_body.get_signature()),
		std::move(notes)
	);
}

void parse_context::report_warning(
	warning_kind kind,
	lex::token_pos it,
	bz::u8string message,
	bz::vector<ctx::note> notes, bz::vector<ctx::suggestion> suggestions
) const
{
	this->global_ctx.report_warning(ctx::make_warning(
		kind,
		it, std::move(message),
		std::move(notes), std::move(suggestions)
	));
}

void parse_context::report_warning(
	warning_kind kind,
	lex::src_tokens src_tokens,
	bz::u8string message,
	bz::vector<ctx::note> notes, bz::vector<ctx::suggestion> suggestions
) const
{
	this->global_ctx.report_warning(ctx::make_warning(
		kind,
		src_tokens.begin, src_tokens.pivot, src_tokens.end, std::move(message),
		std::move(notes), std::move(suggestions)
	));
}

void parse_context::report_parenthesis_suppressed_warning(
	int parens_count,
	warning_kind kind,
	lex::token_pos it,
	bz::u8string message,
	bz::vector<ctx::note> notes, bz::vector<ctx::suggestion> suggestions
) const
{
	auto const open_paren  = bz::u8string(static_cast<size_t>(parens_count), '(');
	auto const close_paren = bz::u8string(static_cast<size_t>(parens_count), ')');
	notes.emplace_back(ctx::make_note_with_suggestion(
		{},
		it, open_paren, it + 1, close_paren,
		"put parenthesis around the expression to suppress this warning"
	));

	this->global_ctx.report_warning(ctx::make_warning(
		kind,
		it, std::move(message),
		std::move(notes), std::move(suggestions)
	));
}

void parse_context::report_parenthesis_suppressed_warning(
	int parens_count,
	warning_kind kind,
	lex::src_tokens src_tokens,
	bz::u8string message,
	bz::vector<ctx::note> notes, bz::vector<ctx::suggestion> suggestions
) const
{
	auto const open_paren  = bz::u8string(static_cast<size_t>(parens_count), '(');
	auto const close_paren = bz::u8string(static_cast<size_t>(parens_count), ')');
	notes.emplace_back(ctx::make_note_with_suggestion(
		{},
		src_tokens.begin, open_paren, src_tokens.end, close_paren,
		"put parenthesis around the expression to suppress this warning"
	));

	this->global_ctx.report_warning(ctx::make_warning(
		kind,
		src_tokens.begin, src_tokens.pivot, src_tokens.end, std::move(message),
		std::move(notes), std::move(suggestions)
	));
}

note parse_context::make_note(uint32_t file_id, uint32_t line, bz::u8string message)
{
	return note{
		file_id, line,
		char_pos(), char_pos(), char_pos(),
		{}, {},
		std::move(message)
	};
}

note parse_context::make_note(lex::token_pos it, bz::u8string message)
{
	return note{
		it->src_pos.file_id, it->src_pos.line,
		it->src_pos.begin, it->src_pos.begin, it->src_pos.end,
		{}, {},
		std::move(message)
	};
}

note parse_context::make_note(lex::src_tokens src_tokens, bz::u8string message)
{
	return note{
		src_tokens.pivot->src_pos.file_id, src_tokens.pivot->src_pos.line,
		src_tokens.begin->src_pos.begin, src_tokens.pivot->src_pos.begin, (src_tokens.end - 1)->src_pos.end,
		{}, {},
		std::move(message)
	};
}

[[nodiscard]] ctx::note parse_context::make_note(
	lex::token_pos it, bz::u8string message,
	ctx::char_pos suggestion_pos, bz::u8string suggestion_str
)
{
	return ctx::note{
		it->src_pos.file_id, it->src_pos.line,
		it->src_pos.begin, it->src_pos.begin, it->src_pos.end,
		{ ctx::char_pos(), ctx::char_pos(), suggestion_pos, std::move(suggestion_str) },
		{},
		std::move(message)
	};
}

[[nodiscard]] ctx::note parse_context::make_paren_match_note(
	lex::token_pos it, lex::token_pos open_paren_it
)
{
	if (open_paren_it->kind == lex::token::curly_open)
	{
		return make_note(open_paren_it, "to match this:");
	}

	bz_assert(open_paren_it->kind == lex::token::paren_open || open_paren_it->kind == lex::token::square_open);
	auto const suggestion_str = open_paren_it->kind == lex::token::paren_open ? ")" : "]";
	auto const [suggested_paren_pos, suggested_paren_line] = [&]() {
		switch (it->kind)
		{
		case lex::token::paren_close:
			if ((open_paren_it - 1)->kind == lex::token::paren_open && (open_paren_it - 1)->src_pos.end == open_paren_it->src_pos.begin)
			{
				return std::make_pair(it->src_pos.begin, it->src_pos.line);
			}
			else
			{
				return std::make_pair((it - 1)->src_pos.end, (it - 1)->src_pos.line);
			}
		case lex::token::square_close:
			if ((open_paren_it - 1)->kind == lex::token::square_open && (open_paren_it - 1)->src_pos.end == open_paren_it->src_pos.begin)
			{
				return std::make_pair(it->src_pos.begin, it->src_pos.line);
			}
			else
			{
				return std::make_pair((it - 1)->src_pos.end, (it - 1)->src_pos.line);
			}
		case lex::token::semi_colon:
			return std::make_pair(it->src_pos.begin, it->src_pos.line);
		default:
			return std::make_pair((it - 1)->src_pos.end, (it - 1)->src_pos.line);
		}
	}();
	auto const open_paren_line = open_paren_it->src_pos.line;
	bz_assert(open_paren_line <= suggested_paren_line);
	if (suggested_paren_line - open_paren_line > 1)
	{
		return make_note(open_paren_it, "to match this:");
	}
	else
	{
		return make_note(open_paren_it, "to match this:", suggested_paren_pos, suggestion_str);
	}
}


[[nodiscard]] suggestion parse_context::make_suggestion_before(
	lex::token_pos it,
	char_pos erase_begin, char_pos erase_end,
	bz::u8string suggestion_str,
	bz::u8string message
)
{
	return suggestion{
		it->src_pos.file_id, it->src_pos.line,
		{ erase_begin, erase_end, it->src_pos.begin, std::move(suggestion_str) },
		{},
		std::move(message)
	};
}

[[nodiscard]] suggestion parse_context::make_suggestion_after(
	lex::token_pos it,
	char_pos erase_begin, char_pos erase_end,
	bz::u8string suggestion_str,
	bz::u8string message
)
{
	return suggestion{
		it->src_pos.file_id, it->src_pos.line,
		{ erase_begin, erase_end, it->src_pos.end, std::move(suggestion_str) },
		{},
		std::move(message)
	};
}

[[nodiscard]] suggestion parse_context::make_suggestion_around(
	lex::token_pos first,
	char_pos first_erase_begin, char_pos first_erase_end,
	bz::u8string first_suggestion_str,
	lex::token_pos last,
	char_pos second_erase_begin, char_pos second_erase_end,
	bz::u8string last_suggestion_str,
	bz::u8string message
)
{
	return suggestion{
		first->src_pos.file_id, first->src_pos.line,
		{ first_erase_begin, first_erase_end, first->src_pos.begin, std::move(first_suggestion_str) },
		{ second_erase_begin, second_erase_end, (last - 1)->src_pos.end, std::move(last_suggestion_str) },
		std::move(message)
	};
}

bool parse_context::has_errors(void) const
{
	return this->global_ctx.has_errors();
}

lex::token_pos parse_context::assert_token(lex::token_pos &stream, uint32_t kind) const
{
	if (stream->kind != kind)
	{
		auto suggestions = kind == lex::token::semi_colon
			? bz::vector<suggestion>{ make_suggestion_after(stream - 1, ";", "add ';' here:") }
			: bz::vector<suggestion>{};
		this->global_ctx.report_error(ctx::make_error(
			stream,
			stream->kind == lex::token::eof
			? bz::format("expected {} before end-of-file", lex::get_token_name_for_message(kind))
			: bz::format("expected {}", lex::get_token_name_for_message(kind)),
			{}, std::move(suggestions)
		));
		return stream;
	}
	else
	{
		auto t = stream;
		++stream;
		return t;
	}
}

lex::token_pos parse_context::assert_token(lex::token_pos &stream, uint32_t kind1, uint32_t kind2) const
{
	if (stream->kind != kind1 && stream->kind != kind2)
	{
		this->global_ctx.report_error(ctx::make_error(
			stream,
			stream->kind == lex::token::eof
			? bz::format(
				"expected {} or {} before end-of-file",
				lex::get_token_name_for_message(kind1), lex::get_token_name_for_message(kind2)
			)
			: bz::format(
				"expected {} or {}",
				lex::get_token_name_for_message(kind1), lex::get_token_name_for_message(kind2)
			)
		));
		return stream;
	}
	else
	{
		auto t = stream;
		++stream;
		return t;
	}
}

void parse_context::report_ambiguous_id_error(lex::token_pos id) const
{
	this->report_error(id, bz::format("identifier '{}' is ambiguous", id->value));
}


void parse_context::add_scope(void)
{
	this->scope_decls.push_back({});
}

void parse_context::remove_scope(void)
{
	bz_assert(!this->scope_decls.empty());
	if (is_warning_enabled(warning_kind::unused_variable))
	{
		for (auto const var_decl : this->scope_decls.back().var_decls)
		{
			if (!var_decl->is_used && var_decl->identifier->kind == lex::token::identifier)
			{
				this->report_warning(
					warning_kind::unused_variable,
					*var_decl,
					bz::format("unused variable '{}'", var_decl->identifier->value)
				);
			}
		}
	}
	this->scope_decls.pop_back();
}


void parse_context::add_local_variable(ast::decl_variable &var_decl)
{
	bz_assert(this->scope_decls.size() != 0);
	this->scope_decls.back().var_decls.push_back(&var_decl);
}

void parse_context::add_local_function(ast::decl_function &func_decl)
{
	bz_assert(this->scope_decls.size() != 0);
	this->scope_decls.back().add_function(func_decl);
}

void parse_context::add_local_operator(ast::decl_operator &op_decl)
{
	bz_assert(this->scope_decls.size() != 0);
	this->scope_decls.back().add_operator(op_decl);
}

/*
void parse_context::add_local_struct(ast::decl_struct &struct_decl)
{
	bz_assert(this->scope_decls.size() != 0);
	this->scope_decls.back().types.push_back({ struct_decl.identifier->value, &struct_decl.info });
}

auto parse_context::get_identifier_decl(lex::token_pos id) const
	-> bz::variant<ast::decl_variable const *, ast::decl_function const *>
{
	// ==== local decls ====
	// we go in reverse through the scopes and the variables
	// in case there's shadowing
	for (
		auto scope = this->scope_decls.rbegin();
		scope != this->scope_decls.rend();
		++scope
	)
	{
		auto const var = std::find_if(
			scope->var_decls.rbegin(), scope->var_decls.rend(),
			[id = id->value](auto const &var) {
				return var->identifier->value == id;
			}
		);
		if (var != scope->var_decls.rend())
		{
			return *var;
		}

		auto const fn_set = std::find_if(
			scope->func_sets.begin(), scope->func_sets.end(),
			[id = id->value](auto const &fn_set) {
				return fn_set.id == id;
			}
		);
		if (fn_set != scope->func_sets.end())
		{
			if (fn_set->func_decls.size() == 1)
			{
				return fn_set->func_decls[0];
			}
			else
			{
				bz_assert(!fn_set->func_decls.empty());
				return static_cast<ast::decl_function const *>(nullptr);
			}
		}
	}

	// ==== export (global) decls ====
	auto &export_decls = this->global_ctx._export_decls;
	auto const var = std::find_if(
		export_decls.var_decls.begin(), export_decls.var_decls.end(),
		[id = id->value](auto const &var) {
			return id == var->identifier->value;
		}
	);
	if (var != export_decls.var_decls.end())
	{
		return *var;
	}

	auto const fn_set = std::find_if(
		export_decls.func_sets.begin(), export_decls.func_sets.end(),
		[id = id->value](auto const &fn_set) {
			return id == fn_set.id;
		}
	);
	if (fn_set != export_decls.func_sets.end())
	{
		if (fn_set->func_decls.size() == 1)
		{
			return fn_set->func_decls[0];
		}
		else
		{
			bz_assert(!fn_set->func_decls.empty());
			return static_cast<ast::decl_function const *>(nullptr);
		}
	}
	return {};
}
*/

void parse_context::add_function_for_compilation(ast::function_body &func_body) const
{
	this->global_ctx.add_compile_function(func_body);
}

/*
ast::expression::expr_type_t parse_context::get_identifier_type(lex::token_pos id) const
{
	auto decl = this->get_identifier_decl(id);
	switch (decl.index())
	{
	case decl.index_of<ast::decl_variable const *>:
	{
		auto const var = decl.get<ast::decl_variable const *>();
		return {
			var->var_type.is<ast::ts_lvalue_reference>()
			? ast::expression::lvalue_reference
			: ast::expression::lvalue,
			ast::remove_lvalue_reference(var->var_type)
		};
	}
	case decl.index_of<ast::decl_function const *>:
	{
		auto const fn = decl.get<ast::decl_function const *>();
		auto fn_t = [&]() -> ast::typespec {
			if (fn == nullptr)
			{
				return ast::typespec();
			}
			bz::vector<ast::typespec> arg_types = {};
			for (auto &p : fn->body.params)
			{
				arg_types.emplace_back(p.var_type);
			}
			return ast::make_ts_function({ nullptr, nullptr }, nullptr, fn->body.return_type, arg_types);
		}();
		return { ast::expression::function_name, fn_t };
	}
	case decl.null:
		this->report_error(id, "undeclared identifier");
		return { ast::expression::rvalue, ast::typespec() };
	default:
		bz_unreachable;
	}
}
*/


static ast::typespec get_function_type(ast::function_body &body)
{
	auto const &return_type = body.return_type;
	bz::vector<ast::typespec> param_types;
	param_types.reserve(body.params.size());
	for (auto &p : body.params)
	{
		param_types.emplace_back(p.var_type);
	}
	return ast::typespec({ ast::ts_function{ {}, std::move(param_types), return_type } });
}

ast::expression parse_context::make_identifier_expression(lex::token_pos id) const
{
	// ==== local decls ====
	// we go in reverse through the scopes and the variables
	// in case there's shadowing
	lex::src_tokens const src_tokens = { id, id, id + 1 };
	auto const id_value = id->value;

	bool is_function_set = false;

	for (
		auto scope = this->scope_decls.rbegin();
		scope != this->scope_decls.rend();
		++scope
	)
	{
		auto const var = std::find_if(
			scope->var_decls.rbegin(), scope->var_decls.rend(),
			[id = id_value](auto const &var) {
				return var->identifier != nullptr && var->identifier->value == id;
			}
		);
		if (var != scope->var_decls.rend())
		{
			(*var)->is_used = true;
			auto id_type_kind = ast::expression_type_kind::lvalue;
			ast::typespec_view id_type = (*var)->var_type;
			if (id_type.is<ast::ts_lvalue_reference>())
			{
				id_type_kind = ast::expression_type_kind::lvalue_reference;
				id_type = (*var)->var_type.get<ast::ts_lvalue_reference>();
			}

			if (id_type.is_empty())
			{
				bz_assert(this->has_errors());
				return ast::expression(src_tokens);
			}
			else if (id_type.is<ast::ts_consteval>())
			{
				auto &init_expr = (*var)->init_expr;
				bz_assert(init_expr.is<ast::constant_expression>());
				ast::typespec result_type = id_type.get<ast::ts_consteval>();
				result_type.add_layer<ast::ts_const>(nullptr);
				return ast::make_constant_expression(
					src_tokens,
					id_type_kind, std::move(result_type),
					init_expr.get<ast::constant_expression>().value,
					ast::make_expr_identifier(id, *var)
				);
			}
			else
			{
				return ast::make_dynamic_expression(
					src_tokens,
					id_type_kind, id_type,
					ast::make_expr_identifier(id, *var)
				);
			}
		}

		auto const fn_set = std::find_if(
			scope->func_sets.begin(), scope->func_sets.end(),
			[id = id_value](auto const &fn_set) {
				return fn_set.id == id;
			}
		);
		if (fn_set != scope->func_sets.end())
		{
			is_function_set = true;
			break;
		}

		auto const type = std::find_if(
			scope->types.rbegin(), scope->types.rend(),
			[id = id_value](auto const &type_info) {
				return type_info.id == id;
			}
		);
		if (type != scope->types.rend())
		{
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::type_name,
				ast::make_typename_typespec(nullptr),
				type->ts,
				ast::make_expr_identifier(id)
			);
		}
	}

	if (is_function_set)
	{
		int fn_set_count = 0;
		using iter_t = decltype(this->scope_decls[0].func_sets)::const_iterator;
		iter_t final_set = nullptr;
		for (auto &scope : this->scope_decls)
		{
			auto const fn_set = std::find_if(
				scope.func_sets.begin(), scope.func_sets.end(),
				[id = id_value](auto const &fn_set) {
					return fn_set.id == id;
				}
			);
			if (fn_set != scope.func_sets.end())
			{
				++fn_set_count;
				if (fn_set_count >= 2 || fn_set->func_decls.size() >= 2)
				{
					static_assert(bz::meta::is_same<decltype(id_value), bz::u8string_view const>);
					return ast::make_constant_expression(
						src_tokens,
						ast::expression_type_kind::function_name, ast::typespec(),
						ast::constant_value(id_value),
						ast::make_expr_identifier(id)
					);
				}
				final_set = fn_set;
			}
		}

		auto &global_decls = *this->global_decls;
		auto const global_fn_set = std::find_if(
			global_decls.func_sets.begin(), global_decls.func_sets.end(),
			[id = id_value](auto const &fn_set) {
				return id == fn_set.id;
			}
		);
		if (global_fn_set != global_decls.func_sets.end())
		{
			++fn_set_count;
			if (fn_set_count >= 2 || global_fn_set->func_decls.size() >= 2)
			{
				static_assert(bz::meta::is_same<decltype(id_value), bz::u8string_view const>);
				return ast::make_constant_expression(
					src_tokens,
					ast::expression_type_kind::function_name, ast::typespec(),
					ast::constant_value(id_value),
					ast::make_expr_identifier(id)
				);
			}
			else
			{
				bz_assert(global_fn_set->func_decls.size() == 1);
				bz_assert(final_set == nullptr);
				auto &body = global_fn_set->func_decls[0].second->body;
				return ast::make_constant_expression(
					src_tokens,
					ast::expression_type_kind::function_name, get_function_type(body),
					ast::constant_value(&body),
					ast::make_expr_identifier(id)
				);
			}
		}
		else
		{
			bz_assert(final_set != nullptr);
			bz_assert(final_set->func_decls.size() == 1);
			auto &body = final_set->func_decls[0].second->body;
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::function_name, get_function_type(body),
				ast::constant_value(&body),
				ast::make_expr_identifier(id)
			);
		}
	}

	// ==== export (global) decls ====
	auto &global_decls = *this->global_decls;
	auto const var = std::find_if(
		global_decls.var_decls.begin(), global_decls.var_decls.end(),
		[id = id_value](auto const &var) {
			return var->identifier != nullptr && id == var->identifier->value;
		}
	);
	if (var != global_decls.var_decls.end())
	{
		(*var)->is_used = true;
		auto id_type_kind = ast::expression_type_kind::lvalue;
		ast::typespec_view id_type = (*var)->var_type;
		if (id_type.is<ast::ts_lvalue_reference>())
		{
			id_type_kind = ast::expression_type_kind::lvalue_reference;
			id_type = (*var)->var_type.get<ast::ts_lvalue_reference>();
		}

		if (id_type.is_empty())
		{
			bz_assert(this->has_errors());
			return ast::expression(src_tokens);
		}
		else if (id_type.is<ast::ts_consteval>())
		{
			auto &init_expr = (*var)->init_expr;
			bz_assert(init_expr.is<ast::constant_expression>());
			ast::typespec result_type = id_type.get<ast::ts_consteval>();
			result_type.add_layer<ast::ts_const>(nullptr);
			return ast::make_constant_expression(
				src_tokens,
				id_type_kind, std::move(result_type),
				init_expr.get<ast::constant_expression>().value,
				ast::make_expr_identifier(id, *var)
			);
		}
		else
		{
			return ast::make_dynamic_expression(
				src_tokens,
				id_type_kind, id_type,
				ast::make_expr_identifier(id, *var)
			);
		}
	}

	auto const fn_set = std::find_if(
		global_decls.func_sets.begin(), global_decls.func_sets.end(),
		[id = id_value](auto const &fn_set) {
			return id == fn_set.id;
		}
	);
	if (fn_set != global_decls.func_sets.end())
	{
		auto const id_type_kind = ast::expression_type_kind::function_name;
		if (fn_set->func_decls.size() == 1)
		{
			auto &body = fn_set->func_decls[0].second->body;
			return ast::make_constant_expression(
				src_tokens,
				id_type_kind, get_function_type(body),
				ast::constant_value(&body),
				ast::make_expr_identifier(id)
			);
		}
		else
		{
			return ast::make_constant_expression(
				src_tokens,
				id_type_kind, ast::typespec(),
				ast::constant_value(fn_set->id.as_string_view()),
				ast::make_expr_identifier(id)
			);
		}
	}

	auto const type = std::find_if(
		global_decls.types.begin(), global_decls.types.end(),
		[id = id_value](auto const &type_info) {
			return type_info.id == id;
		}
	);
	if (type != global_decls.types.end())
	{
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::type_name,
			ast::make_typename_typespec(nullptr),
			type->ts,
			ast::make_expr_identifier(id)
		);
	}

	// special case for void type
	if (id_value == "void")
	{
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::type_name,
			ast::make_typename_typespec(nullptr),
			ast::typespec({ ast::ts_void{ id } }),
			ast::make_expr_identifier(id)
		);
	}
	else if (id_value.starts_with("__builtin"))
	{
		using T = std::pair<bz::u8string_view, uint32_t>;
		static constexpr bz::array builtins = {
			T{ "__builtin_str_begin_ptr", ast::function_body::builtin_str_begin_ptr },
			T{ "__builtin_str_end_ptr",   ast::function_body::builtin_str_end_ptr   },
			T{ "__builtin_str_from_ptrs", ast::function_body::builtin_str_from_ptrs },
		};
		auto const it = std::find_if(builtins.begin(), builtins.end(), [id_value](auto const &p) {
			return p.first == id_value;
		});
		if (it != builtins.end())
		{
			auto const func_body = this->get_builtin_function(it->second);
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::function_name,
				get_function_type(*func_body),
				ast::constant_value(func_body),
				ast::make_expr_identifier(id)
			);
		}
	}

	this->report_error(id, bz::format("undeclared identifier '{}'", id_value));
	return ast::expression(src_tokens);
}

static bz::u8char get_character(bz::u8string_view::const_iterator &it)
{
	auto const c = *it;
	if (c == '\\')
	{
		++it;
		return get_escape_sequence(it);
	}
	else
	{
		++it;
		return c;
	}
}

ast::expression parse_context::make_literal(lex::token_pos literal) const
{
	lex::src_tokens const src_tokens = { literal, literal, literal + 1 };
	auto const get_int_value_and_type_info = [this, literal]<size_t N>(
		bz::u8string_view postfix,
		uint64_t num,
		bz::meta::index_constant<N>
	) -> std::pair<ast::constant_value, ast::type_info *> {
		constexpr auto default_type_info = static_cast<uint32_t>(N);
		using default_type = ast::type_from_type_info_t<default_type_info>;
		static_assert(
			bz::meta::is_same<default_type, int32_t>
			|| bz::meta::is_same<default_type, uint32_t>
		);
		using wide_default_type = ast::type_from_type_info_t<default_type_info + 1>;
		static_assert(
			bz::meta::is_same<wide_default_type, int64_t>
			|| bz::meta::is_same<wide_default_type, uint64_t>
		);

		std::pair<ast::constant_value, ast::type_info *> return_value{};
		auto &value = return_value.first;
		auto &type_info = return_value.second;
		if (postfix == "" || postfix == "u" || postfix == "i")
		{
			bz_assert(postfix != "u" || (bz::meta::is_same<default_type, uint32_t>));
			bz_assert(postfix != "i" || (bz::meta::is_same<default_type,  int32_t>));
			if (num <= static_cast<uint64_t>(std::numeric_limits<default_type>::max()))
			{
				value = static_cast<wide_default_type>(num);
				type_info = this->get_base_type_info(default_type_info);
			}
			else if (num <= static_cast<uint64_t>(std::numeric_limits<wide_default_type>::max()))
			{
				value = static_cast<wide_default_type>(num);
				type_info = this->get_base_type_info(default_type_info + 1);
			}
			else
			{
				value = num;
				type_info = this->get_base_type_info(ast::type_info::uint64_);
			}
		}
#define postfix_check(postfix_str, type, wide_type)                                           \
if (postfix == postfix_str)                                                                   \
{                                                                                             \
    if (num > static_cast<uint64_t>(std::numeric_limits<type##_t>::max()))                    \
    {                                                                                         \
        this->report_error(literal, "literal value is too large to fit in type '" #type "'"); \
    }                                                                                         \
    value = static_cast<wide_type>(static_cast<type##_t>(num));                               \
    type_info = this->get_base_type_info(ast::type_info::type##_);                            \
}
		else postfix_check("i8",  int8,   int64_t)
		else postfix_check("i16", int16,  int64_t)
		else postfix_check("i32", int32,  int64_t)
		else postfix_check("i64", int64,  int64_t)
		else postfix_check("u8",  uint8,  uint64_t)
		else postfix_check("u16", uint16, uint64_t)
		else postfix_check("u32", uint32, uint64_t)
		else postfix_check("u64", uint64, uint64_t)
#undef postfix_check
		else
		{
			if (num <= static_cast<uint64_t>(std::numeric_limits<default_type>::max()))
			{
				value = static_cast<wide_default_type>(num);
				type_info = this->get_base_type_info(default_type_info);
			}
			else if (num <= static_cast<uint64_t>(std::numeric_limits<wide_default_type>::max()))
			{
				value = static_cast<wide_default_type>(num);
				type_info = this->get_base_type_info(default_type_info + 1);
			}
			else
			{
				value = num;
				type_info = this->get_base_type_info(ast::type_info::uint64_);
			}
			this->report_error(literal, bz::format("unknown postfix '{}'", postfix));
		}
		return return_value;
	};

	bz_assert(literal->kind != lex::token::string_literal);
	switch (literal->kind)
	{
	case lex::token::integer_literal:
	{
		auto const number_string = literal->value;
		uint64_t num = 0;
		for (auto c : number_string)
		{
			if (c == '\'')
			{
				continue;
			}

			bz_assert(c >= '0' && c <= '9');
			auto const num10 = num * 10;
			if (
				num > std::numeric_limits<uint64_t>::max() / 10
				|| num10 > std::numeric_limits<uint64_t>::max() - (c - '0')
			)
			{
				this->report_error(literal, "literal value is too large, even for 'uint64'");
				return ast::expression(src_tokens);
			}
			num = num10 + (c - '0');
		}

		auto const postfix = literal->postfix;
		auto const [value, type_info] = [&]() {
			if (postfix == "u")
			{
				return get_int_value_and_type_info(
					postfix,
					num,
					bz::meta::index_constant<ast::type_info::uint32_>{}
				);
			}
			else
			{
				return get_int_value_and_type_info(
					postfix,
					num,
					bz::meta::index_constant<ast::type_info::int32_>{}
				);
			}
		}();

		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			ast::typespec({ ast::ts_base_type{ {}, type_info } }),
			value,
			ast::make_expr_literal(literal)
		);
	}
	case lex::token::floating_point_literal:
	{
		auto const number_string = literal->value;
		double num = 0.0;
		bool decimal_state = false;
		double decimal_multiplier = 1.0;
		for (auto const c : number_string)
		{
			if (c == '\'')
			{
				continue;
			}

			if (c == '.')
			{
				decimal_state = true;
				continue;
			}

			if (decimal_state)
			{
				bz_assert(c >= '0' && c <= '9');
				decimal_multiplier /= 10.0;
				num += static_cast<double>(c - '0') * decimal_multiplier;
			}
			else
			{
				bz_assert(c >= '0' && c <= '9');
				num *= 10.0;
				num += static_cast<double>(c - '0');
			}
		}

		auto const postfix = literal->postfix;
		ast::constant_value value{};
		ast::type_info *type_info;
		if (postfix == "" || postfix == "f64")
		{
			value = num;
			type_info = this->get_base_type_info(ast::type_info::float64_);
		}
		else if (postfix == "f32")
		{
			value = static_cast<float32_t>(num);
			type_info = this->get_base_type_info(ast::type_info::float32_);
		}
		else
		{
			value = num;
			type_info = this->get_base_type_info(ast::type_info::float64_);
			this->report_error(literal, bz::format("unknown postfix '{}'", postfix));
		}

		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			ast::typespec({ ast::ts_base_type{ {}, type_info } }),
			value,
			ast::make_expr_literal(literal)
		);
	}
	case lex::token::hex_literal:
	{
		// number_string_ contains the leading 0x or 0X
		auto const number_string_ = literal->value;
		bz_assert(number_string_.substring(0, 2) == "0x" || number_string_.substring(0, 2) == "0X");
		auto const number_string = number_string_.substring(2, size_t(-1));
		uint64_t num = 0;
		for (auto c : number_string)
		{
			if (c == '\'')
			{
				continue;
			}

			bz_assert((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
			auto const num16 = num * 16;
			auto const char_value = (c >= '0' && c <= '9') ? c - '0'
				: (c >= 'a' && c <= 'f') ? (c - 'a') + 10
				:/* (c >= 'A' && c <= 'F') ? */(c - 'A') + 10;
			if (
				num > std::numeric_limits<uint64_t>::max() / 16
				|| num16 > std::numeric_limits<uint64_t>::max() - char_value
			)
			{
				this->report_error(literal, "literal value is too large, even for 'uint64'");
				break;
			}
			num = num16 + char_value;
		}

		auto const postfix = literal->postfix;
		auto const [value, type_info] = [&]() {
			if (postfix == "i")
			{
				return get_int_value_and_type_info(
					postfix,
					num,
					bz::meta::index_constant<ast::type_info::int32_>{}
				);
			}
			else
			{
				return get_int_value_and_type_info(
					postfix,
					num,
					bz::meta::index_constant<ast::type_info::uint32_>{}
				);
			}
		}();

		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			ast::typespec({ ast::ts_base_type{ {}, type_info } }),
			value,
			ast::make_expr_literal(literal)
		);
	}
	case lex::token::oct_literal:
	{
		// number_string_ contains the leading 0o or 0O
		auto const number_string_ = literal->value;
		bz_assert(number_string_.substring(0, 2) == "0o" || number_string_.substring(0, 2) == "0O");
		auto const number_string = number_string_.substring(2, size_t(-1));
		uint64_t num = 0;
		for (auto c : number_string)
		{
			if (c == '\'')
			{
				continue;
			}

			bz_assert(c >= '0' && c <= '7');
			auto const num8 = num * 8;
			auto const char_value = c - '0';
			if (
				num > std::numeric_limits<uint64_t>::max() / 8
				|| num8 > std::numeric_limits<uint64_t>::max() - char_value
			)
			{
				this->report_error(literal, "literal value is too large, even for 'uint64'");
				break;
			}
			num = num8 + char_value;
		}

		auto const postfix = literal->postfix;
		auto const [value, type_info] = [&]() {
			if (postfix == "i")
			{
				return get_int_value_and_type_info(
					postfix,
					num,
					bz::meta::index_constant<ast::type_info::int32_>{}
				);
			}
			else
			{
				return get_int_value_and_type_info(
					postfix,
					num,
					bz::meta::index_constant<ast::type_info::uint32_>{}
				);
			}
		}();

		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			ast::typespec({ ast::ts_base_type{ {}, type_info } }),
			value,
			ast::make_expr_literal(literal)
		);
	}
	case lex::token::bin_literal:
	{
		// number_string_ contains the leading 0b or 0B
		auto const number_string_ = literal->value;
		bz_assert(number_string_.substring(0, 2) == "0b" || number_string_.substring(0, 2) == "0B");
		auto const number_string = number_string_.substring(2, size_t(-1));
		uint64_t num = 0;
		for (auto c : number_string)
		{
			if (c == '\'')
			{
				continue;
			}

			bz_assert(c >= '0' && c <= '1');
			auto const num2 = num * 2;
			auto const char_value = c - '0';
			if (
				num > std::numeric_limits<uint64_t>::max() / 2
				|| num2 > std::numeric_limits<uint64_t>::max() - char_value
			)
			{
				this->report_error(literal, "literal value is too large, even for 'uint64'");
				break;
			}
			num = num2 + char_value;
		}

		auto const postfix = literal->postfix;
		auto const [value, type_info] = [&]() {
			if (postfix == "i")
			{
				return get_int_value_and_type_info(
					postfix,
					num,
					bz::meta::index_constant<ast::type_info::int32_>{}
				);
			}
			else
			{
				return get_int_value_and_type_info(
					postfix,
					num,
					bz::meta::index_constant<ast::type_info::uint32_>{}
				);
			}
		}();

		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			ast::typespec({ ast::ts_base_type{ {}, type_info } }),
			value,
			ast::make_expr_literal(literal)
		);
	}
	case lex::token::character_literal:
	{
		auto const char_string = literal->value;
		auto it = char_string.begin();
		auto const end = char_string.end();
		auto const value = get_character(it);
		bz_assert(it == end);

		if (!bz::is_valid_unicode_value(value))
		{
			this->report_error(
				literal,
				bz::format(
					"the value \\U{:8x} is not a valid character, maximum value is \\U0010ffff",
					value
				)
			);
		}

		auto const postfix = literal->postfix;
		if (postfix != "")
		{
			this->report_error(literal, bz::format("unknown postfix '{}'", postfix));
		}

		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			ast::typespec({ ast::ts_base_type{ {}, this->get_base_type_info(ast::type_info::char_) } }),
			ast::constant_value(value),
			ast::make_expr_literal(literal)
		);
	}
	case lex::token::kw_true:
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			ast::typespec({ ast::ts_base_type{ {}, this->get_base_type_info(ast::type_info::bool_) } }),
			ast::constant_value(true),
			ast::make_expr_literal(literal)
		);
	case lex::token::kw_false:
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			ast::typespec({ ast::ts_base_type{ {}, this->get_base_type_info(ast::type_info::bool_) } }),
			ast::constant_value(false),
			ast::make_expr_literal(literal)
		);
	case lex::token::kw_null:
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			ast::typespec({ ast::ts_base_type{ {}, this->get_base_type_info(ast::type_info::null_t_) } }),
			ast::constant_value(ast::internal::null_t{}),
			ast::make_expr_literal(literal)
		);
	default:
		bz_unreachable;
	}
}

ast::expression parse_context::make_string_literal(lex::token_pos const begin, lex::token_pos const end) const
{
	bz_assert(end > begin);
	auto it = begin;
	auto const get_string_value = [](lex::token_pos token) -> bz::u8string {
		bz::u8string result = "";
		auto const value = token->value;
		auto it = value.begin();
		auto const end = value.end();

		while (it != end)
		{
			auto const slash = value.find(it, '\\');
			result += bz::u8string_view(it, slash);
			if (slash == end)
			{
				break;
			}
			it = slash;
			result += get_character(it);
		}

		return result;
	};

	bz::u8string result = "";
	for (; it != end; ++it)
	{
		result += get_string_value(it);
	}

	auto const postfix = (end - 1)->postfix;
	if (postfix != "")
	{
		this->report_error({ begin, begin, end }, bz::format("unknown postfix '{}'", postfix));
	}

	return ast::make_constant_expression(
		{ begin, begin, end },
		ast::expression_type_kind::rvalue,
		ast::typespec({ ast::ts_base_type{ {}, this->get_base_type_info(ast::type_info::str_) } }),
		ast::constant_value(result),
		ast::make_expr_literal(lex::token_range{ begin, end })
	);
}

ast::expression parse_context::make_tuple(lex::src_tokens src_tokens, bz::vector<ast::expression> elems) const
{
	auto const is_typename = [&]() {
		for (auto const &e : elems)
		{
			if (!e.is_typename())
			{
				return false;
			}
		}
		return true;
	}();
	auto const has_error = [&]() {
		for (auto const &e : elems)
		{
			if (e.is_null())
			{
				return true;
			}
		}
		return false;
	}();

	if (has_error)
	{
		return ast::expression(src_tokens);
	}
	else if (is_typename)
	{
		bz::vector<ast::typespec> types = {};
		types.reserve(elems.size());
		for (auto const &e : elems)
		{
			types.emplace_back(e.get_typename());
		}
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::type_name,
			ast::typespec(),
			ast::constant_value(ast::make_tuple_typespec(src_tokens, std::move(types))),
			ast::make_expr_tuple(std::move(elems))
		);
	}
	else
	{
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::tuple,
			ast::typespec(),
			ast::make_expr_tuple(std::move(elems))
		);
	}
}

static bool is_built_in_type(ast::typespec_view ts)
{
	switch (ts.kind())
	{
	case ast::typespec_node_t::index_of<ast::ts_const>:
		return is_built_in_type(ts.get<ast::ts_const>());
	case ast::typespec_node_t::index_of<ast::ts_base_type>:
	{
		auto &base = ts.get<ast::ts_base_type>();
		return (base.info->flags & ast::type_info_flags::built_in) != 0;
	}
	case ast::typespec_node_t::index_of<ast::ts_pointer>:
	case ast::typespec_node_t::index_of<ast::ts_function>:
	case ast::typespec_node_t::index_of<ast::ts_tuple>:
		return true;
	default:
		return false;
	}
}

static bool is_implicitly_convertible(
	ast::typespec_view dest,
	ast::expression const &expr,
	[[maybe_unused]] parse_context &context
)
{
	bz_assert(!dest.is<ast::ts_const>());
	bz_assert(!dest.is<ast::ts_consteval>());
	auto const [expr_type, expr_type_kind] = expr.get_expr_type_and_kind();
	auto const expr_type_without_const = ast::remove_const_or_consteval(expr_type);
	if (dest.is<ast::ts_base_type>() && expr_type_without_const.is<ast::ts_base_type>())
	{
		auto const dest_info = dest.get<ast::ts_base_type>().info;
		auto const expr_info = expr_type_without_const.get<ast::ts_base_type>().info;
		if (
			(is_signed_integer_kind(dest_info->kind) && is_signed_integer_kind(expr_info->kind))
			|| (is_unsigned_integer_kind(dest_info->kind) && is_unsigned_integer_kind(expr_info->kind))
		)
		{
			return dest_info->kind >= expr_info->kind;
		}
	}
	return false;
}

static int get_strict_type_match_level(
	ast::typespec_view dest,
	ast::typespec_view source,
	bool accept_void
)
{
	bz_assert(ast::is_complete(source));
	while (dest.kind() == source.kind() && dest.is_safe_blind_get() && source.is_safe_blind_get())
	{
		dest = dest.blind_get();
		source = source.blind_get();
	}
	bz_assert(!dest.is<ast::ts_unresolved>());
	bz_assert(!source.is<ast::ts_unresolved>());

	if (accept_void && dest.is<ast::ts_void>())
	{
		return 1;
	}
	else if (dest.is<ast::ts_auto>())
	{
		return 1;
	}
	else if (dest == source)
	{
		return 0;
	}
	else
	{
		return -1;
	}
}

static int get_type_match_level(
	ast::typespec_view dest,
	ast::expression const &expr,
	parse_context &context
)
{
	// six base cases:
	// *T
	//     -> if expr is of pointer type strict match U to T
	//     -> else expr is some base type, try implicitly casting it
	//     -> special case for *void
	// *const T
	//     -> same as before, but U doesn't have to be const (no need to strict match), +1 match level if U is not const
	// &T
	//     -> expr must be an lvalue
	//     -> strict match type of expr to T
	// &const T
	//     -> expr must be an lvalue
	//     -> type of expr doesn't need to be const, +1 match level if U is not const
	// T
	//     -> if type of expr is T, then there's nothing to do
	//     -> else try to implicitly cast expr to T
	// const T -> match to T (no need to worry about const)
	if (dest.is<ast::ts_const>())
	{
		return get_type_match_level(dest.get<ast::ts_const>(), expr, context);
	}
	else if (dest.is<ast::ts_consteval>())
	{
		if (!expr.is<ast::constant_expression>())
		{
			return -1;
		}
		else
		{
			return get_type_match_level(dest.get<ast::ts_consteval>(), expr, context);
		}
	}

	auto const [expr_type, expr_type_kind] = expr.get_expr_type_and_kind();
	auto const expr_type_without_const = ast::remove_const_or_consteval(expr_type);

	if (dest.is<ast::ts_pointer>())
	{
		if (expr_type_without_const.is<ast::ts_pointer>())
		{
			auto const inner_dest = dest.get<ast::ts_pointer>();
			auto const inner_expr_type = expr_type_without_const.get<ast::ts_pointer>();
			if (inner_dest.is<ast::ts_const>())
			{
				if (inner_expr_type.is<ast::ts_const>())
				{
					return get_strict_type_match_level(inner_dest.get<ast::ts_const>(), inner_expr_type.get<ast::ts_const>(), true);
				}
				else
				{
					auto const strict_match_result = get_strict_type_match_level(inner_dest.get<ast::ts_const>(), inner_expr_type, true);
					return strict_match_result == -1 ? -1 : strict_match_result + 1;
				}
			}
			else
			{
				return get_strict_type_match_level(inner_dest, inner_expr_type, true);
			}
		}
		// special case for null
		else if (expr_type_without_const.is<ast::ts_base_type>() && expr_type_without_const.get<ast::ts_base_type>().info->kind == ast::type_info::null_t_)
		{
			return 1;
		}
	}
	else if (dest.is<ast::ts_lvalue_reference>())
	{
		if (expr_type_kind != ast::expression_type_kind::lvalue && expr_type_kind != ast::expression_type_kind::lvalue_reference)
		{
			return -1;
		}

		auto const inner_dest = dest.get<ast::ts_lvalue_reference>();
		if (inner_dest.is<ast::ts_const>())
		{
			if (expr_type.is<ast::ts_const>())
			{
				return get_strict_type_match_level(inner_dest.get<ast::ts_const>(), expr_type_without_const, false);
			}
			else
			{
				auto const strict_match_result = get_strict_type_match_level(inner_dest.get<ast::ts_const>(), expr_type_without_const, false);
				return strict_match_result == -1 ? -1 : strict_match_result + 1;
			}
		}
		else
		{
			return get_strict_type_match_level(inner_dest, expr_type, false);
		}
	}

	// only implicit type conversions are left
	if (dest == expr_type_without_const)
	{
		return 0;
	}
	else if (is_implicitly_convertible(dest, expr, context))
	{
		return 1;
	}
	return -1;
}

static int get_type_match_level_old(
	ast::typespec_view dest,
	ast::expression const &expr,
	parse_context &context
)
{
	int result = 0;

	auto const [expr_type, expr_type_kind] = expr.get_expr_type_and_kind();

	auto dest_it = ast::remove_const_or_consteval(dest);
	auto src_it = expr_type.as_typespec_view();

	if (dest_it.is_empty() || src_it.is_empty())
	{
		bz_assert(expr_type_kind == ast::expression_type_kind::none || context.has_errors());
		return -1;
	}

	if (dest_it.is<ast::ts_base_type>())
	{
		src_it = ast::remove_const_or_consteval(src_it);
		// if the argument is just a base type, return 1 if there's a conversion, and -1 otherwise
		// TODO: use is_convertible here...
		if (
			src_it.is<ast::ts_base_type>()
			&& src_it.get<ast::ts_base_type>().info == dest_it.get<ast::ts_base_type>().info
		)
		{
			return 1;
		}
		else
		{
			return -1;
		}
	}
	else if (dest_it.is<ast::ts_void>())
	{
		bz_unreachable;
	}
	else if (dest_it.is<ast::ts_tuple>())
	{
		bz_unreachable;
	}
	else if (dest_it.is<ast::ts_function>())
	{
		bz_unreachable;
	}
	else if (dest_it.is<ast::ts_array>())
	{
		src_it = ast::remove_const_or_consteval(src_it);
		if (!src_it.is<ast::ts_array>())
		{
			return -1;
		}
		auto &dest_arr = dest_it.get<ast::ts_array>();
		auto &src_arr  = src_it.get<ast::ts_array>();
		if (
			dest_arr.sizes.size() != src_arr.sizes.size()
			|| !std::equal(
				dest_arr.sizes.begin(), dest_arr.sizes.end(),
				src_arr.sizes.begin()
			)
		)
		{
			return -1;
		}

		// TODO: use is_convertible here...
		return dest_arr.elem_type == src_arr.elem_type ? 1 : -1;
	}
	else if (dest_it.is<ast::ts_lvalue_reference>())
	{
		// if the source is not an lvalue, return -1
		if (
			expr_type_kind != ast::expression_type_kind::lvalue
			&& expr_type_kind != ast::expression_type_kind::lvalue_reference
		)
		{
			return -1;
		}

		dest_it = ast::remove_lvalue_reference(dest_it);
		// if the dest is not a &const return 0 if the src and dest types match, -1 otherwise
		if (!dest_it.is<ast::ts_const>())
		{
			return dest_it == src_it ? 0 : -1;
		}
		else
		{
			dest_it = dest_it.get<ast::ts_const>();
			// if the source is not const increment the result
			if (src_it.is<ast::ts_const>())
			{
				src_it = src_it.get<ast::ts_const>();
			}
			else
			{
				++result;
			}
		}
	}
	else // if (dest_it.is<ast::ts_pointer>())
	{
		result = 1; // not a reference, so result starts at 1
		// advance src_it if it's const
		src_it = ast::remove_const_or_consteval(src_it);
		// return -1 if the source is not a pointer
		if (!src_it.is<ast::ts_pointer>())
		{
			return -1;
		}

		// advance src_it and dest_it
		src_it = src_it.get<ast::ts_pointer>();
		dest_it = dest_it.get<ast::ts_pointer>();

		// if the dest is not a *const return 1 if the src and dest types match, -1 otherwise
		if (!dest_it.is<ast::ts_const>())
		{
			return dest_it == src_it ? 0 : -1;
		}
		else
		{
			dest_it = dest_it.get<ast::ts_const>();
			// if the source is not const increment the result
			if (src_it.is<ast::ts_const>())
			{
				src_it = src_it.get<ast::ts_const>();
			}
			else
			{
				++result;
			}
		}
	}

	// we can only get here if the dest type is &const or *const
	auto const advance = [](ast::typespec_view &ts)
	{
		bz_assert(ts.is<ast::ts_const>() || ts.is<ast::ts_pointer>());
		ts = ts.blind_get();
	};

	while (true)
	{
		if (dest_it.is<ast::ts_base_type>())
		{
			if (
				src_it.is<ast::ts_base_type>()
				&& src_it.get<ast::ts_base_type>().info == dest_it.get<ast::ts_base_type>().info
			)
			{
				return result;
			}
			else
			{
				return -1;
			}
		}
		else if (dest_it.is<ast::ts_array>())
		{
			if (!src_it.is<ast::ts_array>())
			{
				return -1;
			}
			auto &dest_arr = dest_it.get<ast::ts_array>();
			auto &src_arr  = src_it.get<ast::ts_array>();
			if (
				dest_arr.sizes.size() != src_arr.sizes.size()
				|| !std::equal(
					dest_arr.sizes.begin(), dest_arr.sizes.end(),
					src_arr.sizes.begin()
				)
			)
			{
				return -1;
			}

			return dest_arr.elem_type == src_arr.elem_type ? result : -1;
		}
		else if (dest_it.is<ast::ts_void>())
		{
			++result;
			return result;
		}
		else if (dest_it.is<ast::ts_function>())
		{
			bz_unreachable;
		}
		else if (dest_it.is<ast::ts_tuple>())
		{
			bz_unreachable;
		}
		else if (dest_it.kind() == src_it.kind())
		{
			advance(dest_it);
			advance(src_it);
		}
		else if (dest_it.is<ast::ts_const>())
		{
			++result;
			advance(dest_it);
		}
		else
		{
			return -1;
		}
	}
}

static void match_expression_to_type(
	ast::typespec &ts,
	ast::expression const &expr,
	parse_context &context
)
{
}

struct match_level
{
	int min;
	int sum;
};

static match_level get_function_call_match_level(
	ast::statement *func_stmt,
	ast::function_body &func_body,
	bz::array_view<const ast::expression> params,
	parse_context &context,
	lex::src_tokens src_tokens
)
{
	if (func_body.params.size() != params.size())
	{
		return { -1, -1 };
	}

	if (func_body.state < ast::resolve_state::symbol)
	{
		context.add_to_resolve_queue(src_tokens, func_body);
		parse::resolve_function_symbol(func_stmt, func_body, context);
		context.pop_resolve_queue();
	}

	if (func_body.state < ast::resolve_state::symbol)
	{
		return { -1, -1 };
	}

	match_level result = { std::numeric_limits<int>::max(), 0 };

	auto const add_to_result = [&](int match)
	{
		if (result.min == -1)
		{
			return;
		}
		else if (match == -1)
		{
			result = { -1, -1 };
		}
		else
		{
			if (match < result.min)
			{
				result.min = match;
			}
			result.sum += match;
		}
	};

	auto params_it = func_body.params.begin();
	auto call_it  = params.begin();
	auto const types_end = func_body.params.end();
	for (; params_it != types_end; ++call_it, ++params_it)
	{
		add_to_result(get_type_match_level(params_it->var_type, *call_it, context));
	}
	return result;
}

static match_level get_function_call_match_level(
	ast::statement *func_stmt,
	ast::function_body &func_body,
	ast::expression const &expr,
	parse_context &context,
	lex::src_tokens src_tokens
)
{
	if (func_body.params.size() != 1)
	{
		return { -1, -1 };
	}


	if (func_body.state < ast::resolve_state::symbol)
	{
		context.add_to_resolve_queue(src_tokens, func_body);
		parse::resolve_function_symbol(func_stmt, func_body, context);
		context.pop_resolve_queue();
	}

	if (func_body.state < ast::resolve_state::symbol)
	{
		return { -1, -1 };
	}

	auto const match_level = get_type_match_level(func_body.params[0].var_type, expr, context);
	return { match_level, match_level };
}

static match_level get_function_call_match_level(
	ast::statement *func_stmt,
	ast::function_body &func_body,
	ast::expression const &lhs,
	ast::expression const &rhs,
	parse_context &context,
	lex::src_tokens src_tokens
)
{
	if (func_body.params.size() != 2)
	{
		return { -1, -1 };
	}


	if (func_body.state < ast::resolve_state::symbol)
	{
		context.add_to_resolve_queue(src_tokens, func_body);
		parse::resolve_function_symbol(func_stmt, func_body, context);
		context.pop_resolve_queue();
	}

	if (func_body.state < ast::resolve_state::symbol)
	{
		return { -1, -1 };
	}

	auto const lhs_level = get_type_match_level(func_body.params[0].var_type, lhs, context);
	auto const rhs_level = get_type_match_level(func_body.params[0].var_type, rhs, context);
	if (lhs_level == -1 || rhs_level == -1)
	{
		return { -1, -1 };
	}
	return { std::min(lhs_level, rhs_level), lhs_level + rhs_level };
}

/*
static error get_bad_call_error(
	ast::decl_function *func,
	ast::expr_function_call const &func_call
)
{
	if (func->body.params.size() != func_call.params.size())
	{
		return make_error(
			func_call,
			bz::format(
				"expected {} {} for call to '{}', but was given {}",
				func->body.params.size(), func->body.params.size() == 1 ? "argument" : "arguments",
				func->identifier->value, func_call.params.size()
			),
			{
				make_note(
					func->identifier,
					bz::format("see declaration of '{}':", func->identifier->value)
				)
			}
		);
	}

	auto params_it = func->body.params.begin();
	auto call_it  = func_call.params.begin();
	auto const types_end = func->body.params.end();
//	auto const call_end  = func_call.params.end();
	for (; params_it != types_end; ++call_it, ++params_it)
	{
		bz_assert(!call_it->is<ast::tuple_expression>());
		if (get_type_match_level(params_it->var_type, *call_it) == -1)
		{
			auto const [call_it_type, _] = call_it->get_expr_type_and_kind();
			return make_error(
				*call_it,
				bz::format(
					"unable to convert argument {} from '{}' to '{}'",
					(call_it - func_call.params.begin()) + 1,
					call_it_type, params_it->var_type
				)
			);
		}
	}

	bz_unreachable;
}
*/

static auto find_best_match(
	bz::vector<std::pair<match_level, ast::function_body *>> const &possible_funcs,
	size_t scope_decl_count
) -> std::pair<match_level, ast::function_body *>
{
	std::pair<match_level, ast::function_body *> min_scope_match = { { -1, -1 }, nullptr };

	{
		auto it = possible_funcs.begin();
		auto const end = possible_funcs.begin() + scope_decl_count;
		for (; it != end; ++it)
		{
			if (
				min_scope_match.first.min == -1
				|| it->first.min < min_scope_match.first.min
				|| (it->first.min == min_scope_match.first.min && it->first.sum < min_scope_match.first.sum)
			)
			{
				min_scope_match = *it;
			}
		}
	}

	std::pair<match_level, ast::function_body *> min_global_match = { { -1, -1 }, nullptr };
	{
		auto it = possible_funcs.begin() + scope_decl_count;
		auto const end = possible_funcs.end();
		for (; it != end; ++it)
		{
			if (
				min_global_match.first.min == -1
				|| it->first.min < min_global_match.first.min
				|| (it->first.min == min_global_match.first.min && it->first.sum < min_global_match.first.sum)
			)
			{
				min_global_match = *it;
			}
		}
	}

	if (min_scope_match.first.min == -1 && min_global_match.first.min == -1)
	{
		return { { -1, -1 }, nullptr };
	}
	// there's no global match
	else if (min_global_match.first.min == -1)
	{
		return min_scope_match;
	}
	// there's a better scope match
	else if (
		min_scope_match.first.min != -1
		&& (
			min_scope_match.first.min < min_global_match.first.min
			|| (min_scope_match.first.min == min_global_match.first.min && min_scope_match.first.sum <= min_global_match.first.sum)
		)
	)
	{
		return min_scope_match;
	}
	// the global match is the best
	else
	{
		// we need to check for ambiguity here
		bz::vector<std::pair<match_level, ast::function_body *>> possible_min_matches = {};
		for (
			auto it = possible_funcs.begin() + scope_decl_count;
			it != possible_funcs.end();
			++it
		)
		{
			if (it->first.min == min_global_match.first.min && it->first.sum == min_global_match.first.sum)
			{
				possible_min_matches.push_back(*it);
			}
		}

		bz_assert(possible_min_matches.size() != 0);
		if (possible_min_matches.size() == 1)
		{
			return min_global_match;
		}
		else
		{
			// TODO: report ambiguous call error somehow
			return { { -1, -1 }, nullptr };
			bz_unreachable;
		}
	}
}

ast::expression parse_context::make_unary_operator_expression(
	lex::src_tokens src_tokens,
	lex::token_pos op,
	ast::expression expr
)
{
	if (expr.is_null())
	{
		bz_assert(this->has_errors());
		return ast::expression(src_tokens);
	}

	if (is_unary_type_op(op->kind) && expr.is_typename())
	{
		auto result = make_built_in_type_operation(op, std::move(expr), *this);
		result.src_tokens = src_tokens;
		return result;
	}
	else if (is_unary_type_op(op->kind) && !is_unary_built_in_operator(op->kind))
	{
		bz_assert(!is_overloadable_operator(op->kind));
		this->report_error(expr, bz::format("expected a type after '{}'", op->value));
	}

	auto [type, type_kind] = expr.get_expr_type_and_kind();
	// if it's a non-overloadable operator or a built-in with a built-in type operand,
	// user-defined operators shouldn't be looked at
	if (
		!is_unary_overloadable_operator(op->kind)
		|| (is_built_in_type(ast::remove_const_or_consteval(type)) && is_unary_built_in_operator(op->kind))
	)
	{
		auto result = make_built_in_operation(op, std::move(expr), *this);
		result.src_tokens = src_tokens;
		return result;
	}

	auto const report_undeclared_error = [&, this, type = &type]()
	{
		this->report_error(
			src_tokens,
			bz::format(
				"undeclared unary operator {} with type '{}'",
				op->value, *type
			)
		);
	};

	bz::vector<std::pair<match_level, ast::function_body *>> possible_funcs = {};

	// we go through the scope decls for a matching declaration
	for (
		auto scope = this->scope_decls.rbegin();
		scope != this->scope_decls.rend();
		++scope
	)
	{
		auto const set = std::find_if(
			scope->op_sets.begin(), scope->op_sets.end(),
			[op = op->kind](auto const &op_set) {
				return op == op_set.op;
			}
		);
		if (set != scope->op_sets.end())
		{
			for (auto &op : set->op_decls)
			{
				auto &body = op.second->body;
				auto const match_level = get_function_call_match_level(op.first, body, expr, *this, src_tokens);
				if (match_level.min != -1)
				{
					possible_funcs.push_back({ match_level, &body });
				}
			}
		}
	}

	auto const scope_decl_count = possible_funcs.size();

	auto &global_decls = *this->global_decls;
	auto const global_set = std::find_if(
		global_decls.op_sets.begin(), global_decls.op_sets.end(),
		[op = op->kind](auto const &op_set) {
			return op == op_set.op;
		}
	);
	if (global_set != global_decls.op_sets.end())
	{
		for (auto &op : global_set->op_decls)
		{
			auto &body = op.second->body;
			auto const match_level = get_function_call_match_level(op.first, body, expr, *this, src_tokens);
			if (match_level.min != -1)
			{
				possible_funcs.push_back({ match_level, &body });
			}
		}
	}

	auto const best = find_best_match(possible_funcs, scope_decl_count);
	if (best.first.min == -1)
	{
		report_undeclared_error();
		return ast::expression(src_tokens);
	}
	else
	{
		auto &ret_t = best.second->return_type;
		auto return_type_kind = ast::expression_type_kind::rvalue;
		auto return_type = ast::remove_const_or_consteval(ret_t);
		if (ret_t.is<ast::ts_lvalue_reference>())
		{
			return_type_kind = ast::expression_type_kind::lvalue_reference;
			return_type = ret_t.get<ast::ts_lvalue_reference>();
		}
		bz::vector<ast::expression> params = {};
		params.emplace_back(std::move(expr));
		return ast::make_dynamic_expression(
			src_tokens,
			return_type_kind, return_type,
			ast::make_expr_function_call(src_tokens, std::move(params), best.second)
		);
	}
}

ast::expression parse_context::make_binary_operator_expression(
	lex::src_tokens src_tokens,
	lex::token_pos op,
	ast::expression lhs,
	ast::expression rhs
)
{
	if (lhs.is_null() || rhs.is_null())
	{
		bz_assert(this->has_errors());
		return ast::expression(src_tokens);
	}

	if (op->kind == lex::token::kw_as)
	{
		bool good = true;
		if (lhs.is_typename())
		{
			this->report_error(lhs, "left-hand-side of type cast must not be a type");
			good = false;
		}
		if (!rhs.is_typename())
		{
			this->report_error(rhs, "right-hand-side of type cast must be a type");
			good = false;
		}
		if (!good)
		{
			return ast::expression(src_tokens);
		}

		return this->make_cast_expression(src_tokens, op, std::move(lhs), std::move(rhs.get_typename()));
	}

	if (is_binary_type_op(op->kind) && lhs.is_typename() && rhs.is_typename())
	{
		auto result = make_built_in_type_operation(op, std::move(lhs), std::move(rhs), *this);
		result.src_tokens = src_tokens;
		return result;
	}
	else if (is_binary_type_op(op->kind) && !is_binary_built_in_operator(op->kind))
	{
		// there's no operator such as this ('as' is handled earlier)
		bz_unreachable;
	}

	auto const [lhs_type, lhs_type_kind] = lhs.get_expr_type_and_kind();
	auto const [rhs_type, rhs_type_kind] = rhs.get_expr_type_and_kind();
	// if it's a non-overloadable operator or a built-in with a built-in type operand,
	// user-defined operators shouldn't be looked at
	if (
		!is_binary_overloadable_operator(op->kind)
		|| (
			is_built_in_type(ast::remove_const_or_consteval(lhs_type))
			&& is_built_in_type(ast::remove_const_or_consteval(rhs_type))
			&& is_binary_built_in_operator(op->kind)
		)
	)
	{
		auto result = make_built_in_operation(op, std::move(lhs), std::move(rhs), *this);
		result.src_tokens = src_tokens;
		return result;
	}

	auto const report_undeclared_error = [&, this, lhs_type = &lhs_type, rhs_type = &rhs_type]()
	{
		if (op->kind == lex::token::square_open)
		{
			this->report_error(
				src_tokens,
				bz::format(
					"undeclared binary operator [] with types '{}' and '{}'",
					*lhs_type, *rhs_type
				)
			);
		}
		else
		{
			this->report_error(
				src_tokens,
				bz::format(
					"undeclared binary operator {} with types '{}' and '{}'",
					op->value, *lhs_type, *rhs_type
				)
			);
		}
	};

	bz::vector<std::pair<match_level, ast::function_body *>> possible_funcs = {};

	// we go through the scope decls for a matching declaration
	for (
		auto scope = this->scope_decls.rbegin();
		scope != this->scope_decls.rend();
		++scope
	)
	{
		auto const set = std::find_if(
			scope->op_sets.begin(), scope->op_sets.end(),
			[op = op->kind](auto const &op_set) {
				return op == op_set.op;
			}
		);
		if (set != scope->op_sets.end())
		{
			for (auto &op : set->op_decls)
			{
				auto &body = op.second->body;
				auto const match_level = get_function_call_match_level(op.first, body, lhs, rhs, *this, src_tokens);
				if (match_level.min != -1)
				{
					possible_funcs.push_back({ match_level, &body });
				}
			}
		}
	}

	auto const scope_decl_count = possible_funcs.size();

	auto &global_decls = *this->global_decls;
	auto const global_set = std::find_if(
		global_decls.op_sets.begin(), global_decls.op_sets.end(),
		[op = op->kind](auto const &op_set) {
			return op == op_set.op;
		}
	);
	if (global_set != global_decls.op_sets.end())
	{
		for (auto &op : global_set->op_decls)
		{
			auto &body = op.second->body;
			auto const match_level = get_function_call_match_level(op.first, body, lhs, rhs, *this, src_tokens);
			if (match_level.min != -1)
			{
				possible_funcs.push_back({ match_level, &body });
			}
		}
	}

	auto const best = find_best_match(possible_funcs, scope_decl_count);
	if (best.first.min == -1)
	{
		report_undeclared_error();
		return ast::expression(src_tokens);
	}
	else
	{
		auto &ret_t = best.second->return_type;
		auto return_type_kind = ast::expression_type_kind::rvalue;
		auto return_type = ast::remove_const_or_consteval(ret_t);
		if (ret_t.is<ast::ts_lvalue_reference>())
		{
			return_type_kind = ast::expression_type_kind::lvalue_reference;
			return_type = ret_t.get<ast::ts_lvalue_reference>();
		}
		bz::vector<ast::expression> params = {};
		params.reserve(2);
		params.emplace_back(std::move(lhs));
		params.emplace_back(std::move(rhs));
		return ast::make_dynamic_expression(
			src_tokens,
			return_type_kind, return_type,
			ast::make_expr_function_call(src_tokens, std::move(params), best.second)
		);
	}
}

ast::expression parse_context::make_function_call_expression(
	lex::src_tokens src_tokens,
	ast::expression called,
	bz::vector<ast::expression> params
)
{
	if (called.is_null())
	{
		bz_assert(this->has_errors());
		return ast::expression(src_tokens);
	}

	for (auto &p : params)
	{
		if (p.is_null())
		{
			bz_assert(this->has_errors());
			return ast::expression(src_tokens);
		}
	}

	auto const [called_type, called_type_kind] = called.get_expr_type_and_kind();

	if (called_type_kind == ast::expression_type_kind::function_name)
	{
		bz_assert(called.is<ast::constant_expression>());
		auto &const_called = called.get<ast::constant_expression>();
		if (const_called.value.kind() == ast::constant_value::function)
		{
			auto const func_body = const_called.value.get<ast::constant_value::function>();
			if (get_function_call_match_level(nullptr, *func_body, params, *this, src_tokens).sum == -1)
			{
				if (func_body->state != ast::resolve_state::error)
				{
					this->report_error(
						src_tokens,
						"couldn't match the function call to any of the overloads"
					);
				}
				return ast::expression(src_tokens);
			}
			auto &ret_t = func_body->return_type;
			auto return_type_kind = ast::expression_type_kind::rvalue;
			auto return_type = ast::remove_const_or_consteval(ret_t);
			if (ret_t.is<ast::ts_lvalue_reference>())
			{
				return_type_kind = ast::expression_type_kind::lvalue_reference;
				return_type = ret_t.get<ast::ts_lvalue_reference>();
			}
			return ast::make_dynamic_expression(
				src_tokens,
				return_type_kind, return_type,
				ast::make_expr_function_call(src_tokens, std::move(params), func_body)
			);
		}
		else
		{
			bz_assert(const_called.value.kind() == ast::constant_value::function_set_id);
			auto const id = const_called.value.get<ast::constant_value::function_set_id>();
			bz::vector<std::pair<match_level, ast::function_body *>> possible_funcs = {};
			// we go through the scope decls for a matching declaration
			for (
				auto scope = this->scope_decls.rbegin();
				scope != this->scope_decls.rend();
				++scope
			)
			{
				auto const set = std::find_if(
					scope->func_sets.begin(), scope->func_sets.end(),
					[id](auto const &fn_set) {
						return id == fn_set.id;
					}
				);
				if (set != scope->func_sets.end())
				{
					for (auto &fn : set->func_decls)
					{
						auto &body = fn.second->body;
						auto const match_level = get_function_call_match_level(fn.first, body, params, *this, src_tokens);
						if (match_level.min != -1)
						{
							possible_funcs.push_back({ match_level, &body });
						}
					}
				}
			}

			auto const scope_decl_count = possible_funcs.size();

			auto &global_decls = *this->global_decls;
			auto const global_set = std::find_if(
				global_decls.func_sets.begin(), global_decls.func_sets.end(),
				[id](auto const &fn_set) {
					return id == fn_set.id;
				}
			);
			if (global_set != global_decls.func_sets.end())
			{
				for (auto &fn : global_set->func_decls)
				{
					auto &body = fn.second->body;
					auto const match_level = get_function_call_match_level(fn.first, body, params, *this, src_tokens);
					if (match_level.min != -1)
					{
						possible_funcs.push_back({ match_level, &body });
					}
				}
			}

			auto const best = find_best_match(possible_funcs, scope_decl_count);
			if (best.first.min == -1)
			{
				this->report_error(
					src_tokens,
					"couldn't match the function call to any of the overloads"
				);
				return ast::expression(src_tokens);
			}
			else
			{
				auto &ret_t = best.second->return_type;
				auto return_type_kind = ast::expression_type_kind::rvalue;
				auto return_type = ast::remove_const_or_consteval(ret_t);
				if (ret_t.is<ast::ts_lvalue_reference>())
				{
					return_type_kind = ast::expression_type_kind::lvalue_reference;
					return_type = ret_t.get<ast::ts_lvalue_reference>();
				}
				return ast::make_dynamic_expression(
					src_tokens,
					return_type_kind, return_type,
					ast::make_expr_function_call(src_tokens, std::move(params), best.second)
				);
			}
		}
	}
	else
	{
		// function call operator
		this->report_error(src_tokens, "operator () not yet implemented");
		return ast::expression(src_tokens);
	}
}

ast::expression parse_context::make_subscript_operator_expression(
	lex::src_tokens src_tokens,
	ast::expression called,
	bz::vector<ast::expression> args
)
{
	if (called.is_null() || args.size() == 0)
	{
		bz_assert(this->has_errors());
		return ast::expression(src_tokens);
	}

	for (auto &arg : args)
	{
		if (arg.is_null())
		{
			bz_assert(this->has_errors());
			return ast::expression(src_tokens);
		}
	}

	auto const [type, kind] = called.get_expr_type_and_kind();
	auto const constless_type = ast::remove_const_or_consteval(type);
	if (constless_type.is<ast::ts_array>() || constless_type.is<ast::ts_tuple>() || kind == ast::expression_type_kind::tuple)
	{
		return make_built_in_subscript_operator(src_tokens, std::move(called), std::move(args), *this);
	}

	this->report_error(src_tokens, "operator [] not yet implemented");
	return ast::expression(src_tokens);
}

ast::expression parse_context::make_cast_expression(
	lex::src_tokens src_tokens,
	lex::token_pos op,
	ast::expression expr,
	ast::typespec type
)
{
	if (expr.is_null() || type.is_empty())
	{
		bz_assert(this->has_errors());
		return ast::expression(src_tokens);
	}

	auto const [expr_type, expr_type_kind] = expr.get_expr_type_and_kind();

	if (is_built_in_type(expr_type))
	{
		auto result = make_built_in_cast(src_tokens, op, std::move(expr), std::move(type), *this);
		result.src_tokens = src_tokens;
		return result;
	}
	else
	{
		this->report_error(src_tokens, "invalid cast");
		return ast::expression(src_tokens);
	}
}

void parse_context::match_expression_to_type(
	ast::expression &expr,
	ast::typespec &dest_type
)
{
	if (dest_type.is_empty())
	{
		expr.clear();
		return;
	}
	if (expr.is_null())
	{
		dest_type.clear();
		bz_assert(this->has_errors());
		return;
	}

	// check for an overloaded function
	if (expr.is_overloaded_function())
	{
		bz_assert(expr.is<ast::constant_expression>());
		auto &const_expr = expr.get<ast::constant_expression>();
		bz_assert(const_expr.kind == ast::expression_type_kind::function_name);
//		bz_assert(false, "overloaded function not handled in match_expresstion_to_type");
		bz_assert(const_expr.expr.is<ast::expr_identifier>());
		this->report_ambiguous_id_error(const_expr.expr.get<ast::expr_identifier_ptr>()->identifier);
		dest_type.clear();
		return;
	}
	else if (expr.is_typename())
	{
		this->report_error(expr, "a type cannot be used in this context");
		dest_type.clear();
		return;
	}

	if (dest_type.is<ast::ts_consteval>() && !expr.is<ast::constant_expression>())
	{
		this->report_error(expr, "expression must be a constant expression");
		dest_type.clear();
		return;
	}

	auto [expr_type, expr_type_kind] = expr.get_expr_type_and_kind();
	if (expr_type_kind != ast::expression_type_kind::tuple && !ast::is_complete(expr_type))
	{
		if (!ast::is_complete(dest_type))
		{
			dest_type.clear();
		}
		return;
	}

	auto expr_it = expr_type.as_typespec_view();
	auto dest_it = ast::remove_const_or_consteval(dest_type);

	auto const strict_match = [&, &expr_type = expr_type]() {
		bool loop = true;
		while (loop)
		{
			switch (dest_it.kind())
			{
			case ast::typespec_node_t::index_of<ast::ts_base_type>:
				if (expr_it != dest_it)
				{
					this->report_error(
						expr,
						bz::format("cannot convert expression from type '{}' to '{}'", expr_type, dest_type)
					);
					if (!ast::is_complete(dest_type))
					{
						dest_type.clear();
					}
				}
				loop = false;
				break;
			case ast::typespec_node_t::index_of<ast::ts_const>:
				dest_it = dest_it.get<ast::ts_const>();
				if (!(expr_it.is<ast::ts_const>() || expr_it.is<ast::ts_consteval>()))
				{
					this->report_error(
						expr,
						bz::format("cannot convert expression from type '{}' to '{}'", expr_type, dest_type)
					);
					if (!ast::is_complete(dest_type))
					{
						dest_type.clear();
					}
					loop = false;
				}
				else
				{
					expr_it = ast::remove_const_or_consteval(expr_it);
				}
				break;
			case ast::typespec_node_t::index_of<ast::ts_consteval>:
				bz_unreachable;
			case ast::typespec_node_t::index_of<ast::ts_pointer>:
				dest_it = dest_it.get<ast::ts_pointer>();
				if (!expr_it.is<ast::ts_pointer>())
				{
					this->report_error(
						expr,
						bz::format("cannot convert expression from type '{}' to '{}'", expr_type, dest_type)
					);
					dest_type.clear();
					loop = false;
				}
				else
				{
					expr_it = expr_it.get<ast::ts_pointer>();
				}
				break;
			case ast::typespec_node_t::index_of<ast::ts_auto>:
				dest_type.copy_from(dest_it, expr_it);
				loop = false;
				break;
			case ast::typespec_node_t::index_of<ast::ts_void>:
				loop = false;
				break;
			case ast::typespec_node_t::index_of<ast::ts_typename>:
				bz_unreachable;
			default:
				bz_unreachable;
			}
		}
	};

	if (dest_it.is<ast::ts_lvalue_reference>())
	{
		if (
			expr_type_kind != ast::expression_type_kind::lvalue
			&& expr_type_kind != ast::expression_type_kind::lvalue_reference
		)
		{
			this->report_error(expr, "cannot bind an rvalue to an lvalue reference");
			dest_type.clear();
			return;
		}

		dest_it = dest_it.get<ast::ts_lvalue_reference>();

		if (dest_it.is<ast::ts_const>())
		{
			dest_it = dest_it.get<ast::ts_const>();
			expr_it = ast::remove_const_or_consteval(expr_it);
		}
		else if (expr_it.is<ast::ts_const>())
		{
			this->report_error(
				expr,
				bz::format(
					"cannot bind an expression of type '{}' to a reference of type '{}'",
					expr_it, dest_it
				)
			);
			dest_type.clear();
			return;
		}

		strict_match();
	}
	else if (dest_it.is<ast::ts_pointer>())
	{
		expr_it = ast::remove_const_or_consteval(expr_it);
		dest_it = dest_it.get<ast::ts_pointer>();
		if (!expr_it.is<ast::ts_pointer>())
		{
			// check if expr is a null literal
			if (
				expr_it.is<ast::ts_base_type>()
				&& expr_it.get<ast::ts_base_type>().info->kind == ast::type_info::null_t_
			)
			{
				bz_assert(expr.is<ast::constant_expression>());
				if (!ast::is_complete(dest_type))
				{
					this->report_error(expr, "variable type cannot be deduced");
					dest_type.clear();
					return;
				}
				else
				{
					expr.get<ast::constant_expression>().type = ast::remove_const_or_consteval(dest_type);
					return;
				}
			}
			else
			{
				this->report_error(
					expr,
					bz::format("cannot convert expression from type '{}' to '{}'", expr_type, dest_type)
				);
				dest_type.clear();
				return;
			}
		}
		else
		{
			expr_it = expr_it.get<ast::ts_pointer>();
			if (dest_it.is<ast::ts_const>())
			{
				dest_it = dest_it.get<ast::ts_const>();
				expr_it = ast::remove_const_or_consteval(expr_it);
			}
			else if (expr_it.is<ast::ts_const>())
			{
				this->report_error(
					expr,
					bz::format("cannot convert expression from type '{}' to '{}'", expr_type, dest_type)
				);
				dest_type.clear();
				return;
			}
		}

		strict_match();
	}
	else if (dest_it.is<ast::ts_base_type>())
	{
		expr_it = ast::remove_const_or_consteval(expr_it);
		if (!expr_it.is<ast::ts_base_type>())
		{
			this->report_error(
				expr,
				bz::format("cannot convert expression from type '{}' to '{}'", expr_type, dest_type)
			);
			dest_type.clear();
			return;
		}

		auto const dest_info = dest_it.get<ast::ts_base_type>().info;
		auto const expr_info = expr_it.get<ast::ts_base_type>().info;

		if (dest_info != expr_info)
		{
			this->report_error(
				expr,
				bz::format("cannot convert expression from type '{}' to '{}'", expr_type, dest_type)
			);
			dest_type.clear();
			return;
		}
	}
	else if (ast::is_complete(expr_type))
	{
		bz_assert(dest_it.is<ast::ts_auto>());
		expr_it = ast::remove_const_or_consteval(expr_it);

		if (expr_it.is<ast::ts_void>())
		{
			this->report_error(
				expr,
				bz::format("cannot convert expression from type '{}' to '{}'", expr_type, dest_type)
			);
			dest_type.clear();
			return;
		}

		dest_type.copy_from(dest_it, expr_it);
	}
	else
	{
		bz_assert(dest_it.is<ast::ts_auto>());
		bz_assert(!ast::is_complete(expr_type));
		bz_assert(expr_type_kind == ast::expression_type_kind::tuple);
		bz_assert(expr.get_expr().is<ast::expr_tuple>());
		auto &tuple_expr = *expr.get_expr().get<ast::expr_tuple_ptr>();
		bz::vector<ast::typespec> types = {};
		types.reserve(tuple_expr.elems.size());
		for (size_t i = 0; i < tuple_expr.elems.size(); ++i)
		{
			types.emplace_back(ast::make_auto_typespec(nullptr));
		}
		for (auto const [elem_expr, type] : bz::zip(tuple_expr.elems, types))
		{
			static_assert(bz::meta::is_same<decltype(type), ast::typespec &>);
			this->match_expression_to_type(elem_expr, type);
		}
		auto const auto_pos = dest_it.get<ast::ts_auto>().auto_pos;
		auto const src_tokens = auto_pos == nullptr
			? lex::src_tokens{}
			: lex::src_tokens{ auto_pos, auto_pos, auto_pos + 1 };
		dest_type.move_from(dest_it, ast::make_tuple_typespec(src_tokens, std::move(types)));
	}
}

/*
auto parse_context::get_cast_body_and_type(ast::expr_cast const &cast)
	-> std::pair<ast::function_body *, ast::expression::expr_type_t>
{
	auto res = get_built_in_cast_type(cast.expr.expr_type, cast.type, *this);
	if (res.expr_type.kind() == ast::typespec::null)
	{
		this->report_error(cast, bz::format("invalid cast from '{}' to '{}'", cast.expr.expr_type.expr_type, cast.type));
	}
	return { nullptr, std::move(res) };
}
*/
/*
bool parse_context::is_convertible(ast::expression::expr_type_t const &from, ast::typespec const &to)
{
	return are_directly_matchable_types(from, to);
}
*/

ast::type_info *parse_context::get_base_type_info(uint32_t kind) const
{ return this->global_ctx.get_base_type_info(kind); }

ast::function_body *parse_context::get_builtin_function(uint32_t kind) const
{
	return this->global_ctx.get_builtin_function(kind);
}

} // namespace ctx
