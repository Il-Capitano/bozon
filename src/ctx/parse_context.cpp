#include "parse_context.h"
#include "ast/statement.h"
#include "ast/typespec.h"
#include "global_context.h"
#include "builtin_operators.h"
#include "lex/lexer.h"
#include "escape_sequences.h"
#include "resolve/consteval.h"
#include "resolve/statement_resolver.h"
#include "resolve/match_to_type.h"
#include "resolve/match_expression.h"
#include "comptime/codegen.h"
#include "comptime/executor_context.h"
#include "comptime/codegen_context.h"

namespace ctx
{

static ast::expression make_expr_function_call_from_body(
	lex::src_tokens const &src_tokens,
	ast::function_body *body,
	ast::arena_vector<ast::expression> args,
	parse_context &context,
	ast::resolve_order resolve_order = ast::resolve_order::regular
);

static ast::expression make_swap_expression(
	lex::src_tokens const &src_tokens,
	ast::typespec_view type,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
);
static ast::expression make_optional_extract_value_expression(
	lex::src_tokens const &src_tokens,
	ast::expression optional_value,
	parse_context &context
);
static ast::expression make_array_value_init_expression(
	lex::src_tokens const &src_tokens,
	ast::typespec_view type,
	ast::expression value,
	parse_context &context
);
static ast::expression make_destruct_expression(
	ast::typespec_view type,
	ast::expression value,
	parse_context &context
);
static ast::expression make_move_destruct_expression(
	ast::typespec_view type,
	ast::expression value,
	parse_context &context
);

static ast::arena_vector<ast::expression> expand_params(ast::arena_vector<ast::expression> params)
{
	ast::arena_vector<ast::expression> result;
	if (params.empty())
	{
		return result;
	}

	auto const result_size = params.transform([](auto const &expr) {
		return expr.template is<ast::expanded_variadic_expression>()
			? expr.template get<ast::expanded_variadic_expression>().exprs.size()
			: 1;
	}).sum();
	result.reserve(result_size);

	for (auto &param : params)
	{
		if (param.is<ast::expanded_variadic_expression>())
		{
			result.append_move(param.get<ast::expanded_variadic_expression>().exprs);
		}
		else
		{
			result.push_back(std::move(param));
		}
	}

	return result;
}

parse_context::parse_context(global_context &_global_ctx)
	: global_ctx(_global_ctx),
	  is_aggressive_consteval_enabled(_global_ctx.is_aggressive_consteval_enabled())
{}

parse_context::parse_context(parse_context const &other, local_copy_t)
	: global_ctx(other.global_ctx),
	  // generic_functions(other.generic_functions),
	  // generic_function_scope_start(other.generic_function_scope_start),
	  current_global_scope(other.current_global_scope),
	  current_local_scope(other.current_local_scope),
	  current_function(nullptr),
	  resolve_queue(other.resolve_queue),
	  variadic_info(other.variadic_info),
	  is_aggressive_consteval_enabled(other.is_aggressive_consteval_enabled),
	  in_loop(other.in_loop),
	  parsing_variadic_expansion(other.parsing_variadic_expansion),
	  in_unevaluated_context(other.in_unevaluated_context),
	  in_unresolved_context(other.in_unresolved_context),
	  parsing_template_argument(other.parsing_template_argument)
{}

parse_context::parse_context(parse_context const &other, global_copy_t)
	: global_ctx(other.global_ctx),
	  current_global_scope(other.current_global_scope),
	  resolve_queue(other.resolve_queue),
	  is_aggressive_consteval_enabled(other.is_aggressive_consteval_enabled)
{}

ast::type_info *parse_context::get_builtin_type_info(uint32_t kind) const
{
	return this->global_ctx.get_builtin_type_info(kind);
}

ast::type_info *parse_context::get_usize_type_info(void) const
{
	return this->global_ctx.get_usize_type_info();
}

ast::type_info *parse_context::get_isize_type_info(void) const
{
	return this->global_ctx.get_isize_type_info();
}

ast::typespec_view parse_context::get_builtin_type(bz::u8string_view name) const
{
	return this->global_ctx.get_builtin_type(name);
}

ast::decl_function *parse_context::get_builtin_function(uint32_t kind) const
{
	return this->global_ctx.get_builtin_function(kind);
}

bz::array_view<uint32_t const> parse_context::get_builtin_universal_functions(bz::u8string_view id)
{
	return this->global_ctx.get_builtin_universal_functions(id);
}

ast::type_prototype_set_t &parse_context::get_type_prototype_set(void)
{
	return *this->global_ctx.type_prototype_set;
}

[[nodiscard]] parse_context::loop_info_t parse_context::push_loop(void) noexcept
{
	auto const prev_in_loop = this->in_loop;
	this->in_loop = true;
	return { prev_in_loop };
}

void parse_context::pop_loop(loop_info_t prev_info) noexcept
{
	this->in_loop = prev_info.in_loop;
}

bool parse_context::is_in_loop(void) const noexcept
{
	return this->in_loop;
}

[[nodiscard]] parse_context::variadic_resolve_info_t parse_context::push_variadic_resolver(void) noexcept
{
	auto result = this->variadic_info;
	this->variadic_info = { true, false, 0, 0, {} };
	return result;
}

void parse_context::pop_variadic_resolver(variadic_resolve_info_t const &prev_info) noexcept
{
	this->variadic_info = prev_info;
}

[[nodiscard]] bool parse_context::push_parsing_variadic_expansion(void) noexcept
{
	auto result = this->parsing_variadic_expansion;
	this->parsing_variadic_expansion = true;
	return result;
}

void parse_context::pop_parsing_variadic_expansion(bool prev_value) noexcept
{
	this->parsing_variadic_expansion = prev_value;
}

[[nodiscard]] bool parse_context::push_unevaluated_context(void) noexcept
{
	auto const result = this->in_unevaluated_context;
	this->in_unevaluated_context = true;
	return result;
}

void parse_context::pop_unevaluated_context(bool prev_value) noexcept
{
	this->in_unevaluated_context = prev_value;
}

[[nodiscard]] bool parse_context::push_unresolved_context(void) noexcept
{
	auto const result = this->in_unresolved_context;
	this->in_unresolved_context = true;
	return result;
}

void parse_context::pop_unresolved_context(bool prev_value) noexcept
{
	this->in_unresolved_context = prev_value;
}

void parse_context::push_parsing_template_argument(void) noexcept
{
	++this->parsing_template_argument;
}

void parse_context::pop_parsing_template_argument(void) noexcept
{
	bz_assert(this->parsing_template_argument > 0);
	--this->parsing_template_argument;
}

bool parse_context::is_parsing_template_argument(void) const noexcept
{
	return this->parsing_template_argument != 0;
}

bool parse_context::register_variadic(lex::src_tokens const &src_tokens, ast::variadic_var_decl_ref variadic_decl)
{
	if (!this->variadic_info.is_resolving_variadic)
	{
		this->report_error(src_tokens, "a variadic variable cannot be used in this context");
		return false;
	}
	else if (this->variadic_info.found_variadic)
	{
		if (this->variadic_info.variadic_size != variadic_decl.variadic_decls.size())
		{
			this->report_error(
				src_tokens,
				bz::format(
					"variadic expression length {} doesn't match previous length of {}",
					variadic_decl.variadic_decls.size(), this->variadic_info.variadic_size
				),
				{ this->make_note(
					this->variadic_info.first_variadic_src_tokens,
					bz::format("an expression with variadic length {} was previously used here", this->variadic_info.variadic_size)
				) }
			);
			return false;
		}
		else
		{
			bz_assert(variadic_decl.variadic_decls.empty() || this->variadic_info.variadic_index < variadic_decl.variadic_decls.size());
			return true;
		}
	}
	else
	{
		bz_assert(this->variadic_info.variadic_index == 0);
		this->variadic_info.found_variadic = true;
		this->variadic_info.first_variadic_src_tokens = src_tokens;
		this->variadic_info.variadic_size = static_cast<uint32_t>(variadic_decl.variadic_decls.size());
		return true;
	}
}

bool parse_context::register_variadic(lex::src_tokens const &src_tokens, ast::variadic_var_decl const &variadic_decl)
{
	return this->register_variadic(src_tokens, ast::variadic_var_decl_ref{ variadic_decl.original_decl, variadic_decl.variadic_decls });
}

uint32_t parse_context::get_variadic_index(void) const
{
	return this->variadic_info.variadic_index;
}

[[nodiscard]] ast::function_body *parse_context::push_current_function(ast::function_body *new_function) noexcept
{
	auto const result = this->current_function;
	this->current_function = new_function;
	return result;
}

void parse_context::pop_current_function(ast::function_body *prev_function) noexcept
{
	this->current_function = prev_function;
}

[[nodiscard]] parse_context::global_local_scope_pair_t parse_context::push_global_scope(ast::scope_t *new_scope) noexcept
{
	auto prev_scopes = global_local_scope_pair_t{
		this->current_global_scope,
		this->current_local_scope,
		std::move(this->current_unresolved_locals)
	};
	bz_assert(new_scope->is_global());
	this->current_global_scope = new_scope;
	this->current_local_scope = {};
	this->current_unresolved_locals = {};
	return prev_scopes;
}

void parse_context::pop_global_scope(global_local_scope_pair_t prev_scopes) noexcept
{
	this->current_global_scope = prev_scopes.global_scope;
	this->current_local_scope  = prev_scopes.local_scope;
	this->current_unresolved_locals = std::move(prev_scopes.unresolved_locals);
}

void parse_context::push_local_scope(ast::scope_t *new_scope) noexcept
{
	bz_assert(new_scope->is_local());
	bz_assert(new_scope->get_local().parent == this->get_current_enclosing_scope());
	this->current_local_scope = { new_scope, new_scope->get_local().symbols.size() };
}

static auto var_decl_range(bz::array_view<ast::local_symbol_t const> symbols)
{
	return symbols
		.filter([](auto const &symbol) {
			return symbol.template is<ast::decl_variable *>()
				|| (
					symbol.template is<ast::variadic_var_decl>()
					&& symbol.template get<ast::variadic_var_decl>().variadic_decls.not_empty()
				);
		})
		.transform([](auto const &symbol) {
			return symbol.template is<ast::decl_variable *>()
				? symbol.template get<ast::decl_variable *>()
				: symbol.template get<ast::variadic_var_decl>().original_decl;
		});
}

void parse_context::pop_local_scope(bool report_unused) noexcept
{
	bz_assert(this->current_local_scope.scope != nullptr);
	bz_assert(this->current_local_scope.scope->is_local());

	if (report_unused && is_warning_enabled(warning_kind::unused_variable))
	{
		auto const &symbols = this->current_local_scope.scope->get_local().symbols.slice(0, this->current_local_scope.symbol_count);
		for (auto const var_decl : var_decl_range(symbols))
		{
			if (!var_decl->is_used() && !var_decl->is_maybe_unused() && var_decl->get_id().values.not_empty())
			{
				this->report_warning(
					warning_kind::unused_variable,
					var_decl->src_tokens,
					bz::format("unused variable '{}'", var_decl->get_id().format_as_unqualified()),
					{ this->make_note_with_suggestion_before(
						{}, var_decl->get_id().tokens.end - 1, "_",
						"prefix variable name with an underscore to suppress this warning"
					) }
				);
			}
		}
	}

	if (this->move_scopes.not_empty() && this->move_scopes.back().move_branches.not_empty())
	{
		auto &current_move_scope = this->move_scopes.back().move_branches.back();
		auto const &symbols = this->current_local_scope.scope->get_local().symbols.slice(0, this->current_local_scope.symbol_count);
		for (auto const var_decl : var_decl_range(symbols))
		{
			current_move_scope.erase_value(var_decl);
		}
	}

	auto const parent = this->current_local_scope.scope->get_local().parent;
	bz_assert(parent.scope != nullptr);
	if (parent.scope == this->current_global_scope)
	{
		this->current_local_scope = {};
	}
	else
	{
		this->current_local_scope = parent;
	}
}

static ast::scope_t *get_global_scope(ast::enclosing_scope_t scope, ast::scope_t *builtin_global_scope) noexcept
{
	bz_assert(scope.scope != nullptr);

	auto parent = scope;
	do
	{
		scope = parent;
		if (parent.scope->is_local())
		{
			parent = parent.scope->get_local().parent;
		}
		else
		{
			parent = parent.scope->get_global().parent;
		}
	} while (parent.scope != builtin_global_scope && parent.scope != nullptr);

	bz_assert(scope.scope != nullptr);
	bz_assert(scope.scope->is_global());
	return scope.scope;
}

[[nodiscard]] parse_context::global_local_scope_pair_t parse_context::push_enclosing_scope(ast::enclosing_scope_t new_scope) noexcept
{
	auto const prev_scopes = global_local_scope_pair_t{
		this->current_global_scope,
		this->current_local_scope,
		std::move(this->current_unresolved_locals)
	};

	this->current_global_scope = get_global_scope(new_scope, this->global_ctx._builtin_global_scope);
	this->current_local_scope = new_scope;
	this->current_unresolved_locals = {};

	return prev_scopes;
}

void parse_context::pop_enclosing_scope(global_local_scope_pair_t prev_scopes) noexcept
{
	this->current_global_scope = prev_scopes.global_scope;
	this->current_local_scope  = prev_scopes.local_scope;
	this->current_unresolved_locals = std::move(prev_scopes.unresolved_locals);
}

bz::array_view<bz::u8string_view const> parse_context::get_current_enclosing_id_scope(void) const noexcept
{
	if (this->current_local_scope.scope != nullptr)
	{
		if (this->current_local_scope.scope->is_local())
		{
			return {};
		}
		else
		{
			return this->current_local_scope.scope->get_global().id_scope;
		}
	}
	else
	{
		return this->current_global_scope->get_global().id_scope;
	}
}

ast::enclosing_scope_t parse_context::get_current_enclosing_scope(void) const noexcept
{
	if (this->current_local_scope.scope == nullptr)
	{
		return { this->current_global_scope, 0 };
	}
	else
	{
		return this->current_local_scope;
	}
}

bool parse_context::has_common_global_scope(ast::enclosing_scope_t scope) const noexcept
{
	while (scope.scope != nullptr)
	{
		if (scope.scope == this->current_global_scope)
		{
			return true;
		}
		else if (scope.scope->is_local())
		{
			scope = scope.scope->get_local().parent;
		}
		else
		{
			scope = scope.scope->get_global().parent;
		}
	}

	return false;
}

void parse_context::push_move_scope(lex::src_tokens const &src_tokens) noexcept
{
	this->move_scopes.push_back({ src_tokens, {} });
}

void parse_context::pop_move_scope(void) noexcept
{
	bz_assert(this->move_scopes.not_empty());

	auto const &src_tokens = this->move_scopes.back().src_tokens;
	auto const has_multiple_scopes = this->move_scopes.size() > 1;
	bz_assert(!has_multiple_scopes || this->move_scopes[this->move_scopes.size() - 2].move_branches.not_empty());
	auto &result_set = has_multiple_scopes
		? this->move_scopes[this->move_scopes.size() - 2].move_branches.back()
		: this->move_scopes.back().move_branches[0];
	auto const original_size = has_multiple_scopes ? result_set.size() : 0;
	bz_assert(this->move_scopes.back().move_branches.not_empty());
	// append the first branch as-is
	if (has_multiple_scopes)
	{
		result_set.append(this->move_scopes.back().move_branches[0]);
	}
	// the rest of the variable decls should only be added if it's not already in result_set
	for (auto const &branch : this->move_scopes.back().move_branches.slice(1))
	{
		for (auto const var_decl : branch)
		{
			if (!result_set.slice(original_size).contains(var_decl))
			{
				result_set.push_back(var_decl);
			}
			else
			{
				// if multiple branches contain the same decl, update the move_position to the outer expression
				var_decl->move_position = src_tokens;
			}
		}
	}

	for (auto const decl : result_set.slice(original_size))
	{
		decl->flags |= ast::decl_variable::moved;
	}

	this->move_scopes.pop_back();
}

void parse_context::push_new_move_branch(void) noexcept
{
	bz_assert(this->move_scopes.not_empty());
	if (this->move_scopes.back().move_branches.not_empty())
	{
		for (auto const decl : this->move_scopes.back().move_branches.back())
		{
			// remove moved flag
			decl->flags &= ~ast::decl_variable::moved;
		}
	}
	this->move_scopes.back().move_branches.emplace_back();
}

void parse_context::register_move(lex::src_tokens const &src_tokens, ast::decl_variable *decl) noexcept
{
	decl->move_position = src_tokens;
	bz_assert(!decl->is_moved());
	decl->flags |= ast::decl_variable::moved;
	if (this->move_scopes.not_empty())
	{
		bz_assert(this->move_scopes.back().move_branches.not_empty());
		this->move_scopes.back().move_branches.back().push_back(decl);
	}
}

void parse_context::register_move_construction(ast::decl_variable *decl) noexcept
{
	decl->flags |= ast::decl_variable::ever_moved_from;
}

static source_highlight get_function_parameter_types_note(lex::src_tokens const &src_tokens, bz::array_view<ast::expression const> args)
{
	if (args.size() == 0)
	{
		return parse_context::make_note(src_tokens, "function argument list is empty");
	}

	auto message = [&]() {
		bz::u8string result = bz::format("function argument types are '{}'", args[0].get_expr_type());
		for (auto const &arg : args.slice(1))
		{
			auto const arg_type = arg.get_expr_type();
			result += bz::format(", '{}'", arg_type);
		}
		return result;
	}();

	return parse_context::make_note(src_tokens, std::move(message));
}

static void add_generic_requirement_notes(bz::vector<source_highlight> &notes, parse_context const &context)
{
	if (context.resolve_queue.size() == 0)
	{
		return;
	}

	auto &dep = context.resolve_queue.back();
	if (dep.requested.is<ast::function_body *>())
	{
		auto const &body = *dep.requested.get<ast::function_body *>();
		if (body.is_generic_specialization())
		{
			notes.emplace_back(context.make_note(
				body.src_tokens, bz::format("in generic instantiation of '{}'", body.get_signature())
			));
		}
	}
	else if (dep.requested.is<ast::type_info *>())
	{
		auto const &info = *dep.requested.get<ast::type_info *>();
		if (info.is_generic_instantiation())
		{
			notes.emplace_back(context.make_note(
				info.src_tokens, bz::format("in generic instantiation of 'struct {}'", info.get_typename_as_string())
			));
		}
	}

	auto const generic_required_from =
		dep.requested.is<ast::function_body *>() ? dep.requested.get<ast::function_body *>()->generic_required_from.as_array_view() :
		dep.requested.is<ast::type_info *>() ? dep.requested.get<ast::type_info *>()->generic_required_from.as_array_view() :
		bz::array_view<ast::generic_required_from_t>();

	for (auto const &required_from : generic_required_from.reversed())
	{
		if (required_from.src_tokens.pivot == nullptr)
		{
			notes.emplace_back(context.make_note("required from unknown location"));
		}
		else if (required_from.body_or_info.is_null())
		{
			notes.emplace_back(context.make_note(required_from.src_tokens, "required from here"));
		}
		else if (required_from.body_or_info.is<ast::function_body *>())
		{
			auto const body = required_from.body_or_info.get<ast::function_body *>();
			notes.emplace_back(context.make_note(
				required_from.src_tokens,
				bz::format("required from generic instantiation of '{}'", body->get_signature())
			));
		}
		else
		{
			bz_assert(required_from.body_or_info.is<ast::type_info *>());
			auto const info = required_from.body_or_info.get<ast::type_info *>();
			notes.emplace_back(context.make_note(
				required_from.src_tokens,
				bz::format("required from generic instantiation of 'struct {}'", info->get_typename_as_string())
			));
		}
	}
}

static ast::arena_vector<ast::generic_required_from_t> get_generic_requirements(
	lex::src_tokens const &src_tokens,
	parse_context &context
)
{
	bz_assert(src_tokens.pivot != nullptr);
	ast::arena_vector<ast::generic_required_from_t> result;
	if (!context.resolve_queue.empty())
	{
		// inherit dependencies from parent function
		auto &dep = context.resolve_queue.back();
		if (dep.requested.is<ast::function_body *>())
		{
			auto const body = dep.requested.get<ast::function_body *>();
			result = body->generic_required_from;
			if (body->is_generic_specialization())
			{
				result.push_back({ src_tokens, dep.requested.get<ast::function_body *>() });
			}
			else
			{
				result.push_back({ src_tokens, {} });
			}
		}
		else if (dep.requested.is<ast::type_info *>())
		{
			auto const info = dep.requested.get<ast::type_info *>();
			result = info->generic_required_from;
			if (info->is_generic_instantiation())
			{
				result.push_back({ src_tokens, dep.requested.get<ast::type_info *>() });
			}
			else
			{
				result.push_back({ src_tokens, {} });
			}
		}
		else
		{
			result.push_back({ src_tokens, {} });
		}
	}
	else
	{
		result.push_back({ src_tokens, {} });
	}
	bz_assert(result.front().src_tokens.pivot != nullptr);
	return result;
}

void parse_context::report_error(lex::token_pos it) const
{
	this->report_error(it, bz::format("unexpected token '{}'", it->value));
}

void parse_context::report_error(
	lex::token_pos it,
	bz::u8string message,
	bz::vector<source_highlight> notes, bz::vector<source_highlight> suggestions
) const
{
	this->report_error({ it, it, it + 1 }, std::move(message), std::move(notes), std::move(suggestions));
}

void parse_context::report_error(
	lex::src_tokens const &src_tokens,
	bz::u8string message,
	bz::vector<source_highlight> notes, bz::vector<source_highlight> suggestions
) const
{
	add_generic_requirement_notes(notes, *this);
	this->global_ctx.report_error(error{
		warning_kind::_last,
		{
			src_tokens.pivot->src_pos.file_id, src_tokens.pivot->src_pos.line,
			src_tokens.begin->src_pos.begin, src_tokens.pivot->src_pos.begin, (src_tokens.end - 1)->src_pos.end,
			suggestion_range{}, suggestion_range{},
			std::move(message),
		},
		std::move(notes), std::move(suggestions)
	});
}

void parse_context::report_error(
	bz::u8string message,
	bz::vector<source_highlight> notes,
	bz::vector<source_highlight> suggestions
) const
{
	this->global_ctx.report_error(std::move(message), std::move(notes), std::move(suggestions));
}

void parse_context::report_paren_match_error(
	lex::token_pos it, lex::token_pos open_paren_it,
	bz::vector<source_highlight> notes, bz::vector<source_highlight> suggestions
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
		case lex::token::angle_open:
			return it->kind == lex::token::eof
				? bz::u8string("expected closing > before end-of-file")
				: bz::format("expected closing > before '{}'", it->value);
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

template<typename T>
static bz::vector<source_highlight> get_circular_notes(T *decl, parse_context const &context)
{
	bz::vector<source_highlight> notes = {};
	int count = 0;
	for (auto const &dep : context.resolve_queue.reversed())
	{
		if (!notes.empty())
		{
			static_assert(decltype(parse_context::resolve_queue_t::requested)::variant_count == 6);
			if (dep.requested.is<ast::function_body *>())
			{
				auto const func_body = dep.requested.get<ast::function_body *>();
				if (func_body->is_generic_specialization())
				{
					notes.back().message = bz::format("required from generic instantiation of '{}'", func_body->get_signature());
				}
				else
				{
					notes.back().message = bz::format("required from instantiation of '{}'", func_body->get_signature());
				}
			}
			else if (dep.requested.is<ast::decl_function_alias *>())
			{
				notes.back().message = bz::format(
					"required from instantiation of alias 'function {}'",
					dep.requested.get<ast::decl_function_alias *>()->id.format_as_unqualified()
				);
			}
			else if (dep.requested.is<ast::decl_type_alias *>())
			{
				notes.back().message = bz::format(
					"required from instantiation of type alias '{}'",
					dep.requested.get<ast::decl_type_alias *>()->id.format_as_unqualified()
				);
			}
			else if (dep.requested.is<ast::type_info *>())
			{
				notes.back().message = bz::format(
					"required from instantiation of type 'struct {}'",
					dep.requested.get<ast::type_info *>()->get_typename_as_string()
				);
			}
			else if (dep.requested.is<ast::decl_enum *>())
			{
				notes.back().message = bz::format(
					"required from instantiation of type 'enum {}'",
					dep.requested.get<ast::decl_enum *>()->id.format_as_unqualified()
				);
			}
		}
		if (dep.requested == decl)
		{
			++count;
			if (count == 2)
			{
				break;
			}
		}
		if (dep.requester.pivot == nullptr)
		{
			notes.emplace_back(context.make_note("required from unknown location"));
		}
		else
		{
			notes.emplace_back(context.make_note(dep.requester, "required from here"));
		}
	}
	return notes;
}

void parse_context::report_circular_dependency_error(ast::function_body &func_body) const
{
	auto notes = get_circular_notes(&func_body, *this);

	if (func_body.is_intrinsic())
	{
		this->report_error(
			bz::format("circular dependency encountered while resolving '{}'", func_body.get_signature()),
			std::move(notes)
		);
	}
	else
	{
		this->report_error(
			func_body.src_tokens,
			bz::format("circular dependency encountered while resolving '{}'", func_body.get_signature()),
			std::move(notes)
		);
	}
}

void parse_context::report_circular_dependency_error(ast::decl_function_alias &alias_decl) const
{
	auto notes = get_circular_notes(&alias_decl, *this);

	this->report_error(
		alias_decl.src_tokens,
		bz::format("circular dependency encountered while resolving alias 'function {}'", alias_decl.id.format_as_unqualified()),
		std::move(notes)
	);
}

void parse_context::report_circular_dependency_error(ast::decl_type_alias &alias_decl) const
{
	auto notes = get_circular_notes(&alias_decl, *this);

	this->report_error(
		alias_decl.src_tokens,
		bz::format("circular dependency encountered while resolving type alias '{}'", alias_decl.id.format_as_unqualified()),
		std::move(notes)
	);
}

void parse_context::report_circular_dependency_error(ast::type_info &info) const
{
	auto notes = get_circular_notes(&info, *this);

	this->report_error(
		info.src_tokens,
		bz::format("circular dependency encountered while resolving type 'struct {}'", info.get_typename_as_string()),
		std::move(notes)
	);
}

void parse_context::report_circular_dependency_error(ast::decl_enum &enum_decl) const
{
	auto notes = get_circular_notes(&enum_decl, *this);

	this->report_error(
		enum_decl.src_tokens,
		bz::format("circular dependency encountered while resolving type 'enum {}'", enum_decl.id.format_as_unqualified()),
		std::move(notes)
	);
}

void parse_context::report_circular_dependency_error(ast::decl_variable &var_decl) const
{
	auto notes = get_circular_notes(&var_decl, *this);

	this->report_error(
		var_decl.src_tokens,
		bz::format("circular dependency encountered while resolving variable '{}'", var_decl.get_id().format_as_unqualified()),
		std::move(notes)
	);
}

void parse_context::report_warning(
	warning_kind kind,
	lex::token_pos it,
	bz::u8string message,
	bz::vector<source_highlight> notes, bz::vector<source_highlight> suggestions
) const
{
	this->report_warning(kind, { it, it, it + 1 }, std::move(message), std::move(notes), std::move(suggestions));
}

void parse_context::report_warning(
	warning_kind kind,
	lex::src_tokens const &src_tokens,
	bz::u8string message,
	bz::vector<source_highlight> notes, bz::vector<source_highlight> suggestions
) const
{
	add_generic_requirement_notes(notes, *this);
	this->global_ctx.report_warning(error{
		kind,
		{
			src_tokens.pivot->src_pos.file_id, src_tokens.pivot->src_pos.line,
			src_tokens.begin->src_pos.begin, src_tokens.pivot->src_pos.begin, (src_tokens.end - 1)->src_pos.end,
			suggestion_range{}, suggestion_range{},
			std::move(message),
		},
		std::move(notes), std::move(suggestions)
	});
}

void parse_context::report_parenthesis_suppressed_warning(
	int parens_count,
	warning_kind kind,
	lex::token_pos it,
	bz::u8string message,
	bz::vector<source_highlight> notes, bz::vector<source_highlight> suggestions
) const
{
	auto const open_paren  = bz::u8string(static_cast<size_t>(parens_count), '(');
	auto const close_paren = bz::u8string(static_cast<size_t>(parens_count), ')');
	notes.emplace_back(this->make_note_with_suggestion_around(
		{},
		it, open_paren, it + 1, close_paren,
		"put parenthesis around the expression to suppress this warning"
	));

	this->report_warning(kind, it, std::move(message), std::move(notes), std::move(suggestions));
}

void parse_context::report_parenthesis_suppressed_warning(
	int parens_count,
	warning_kind kind,
	lex::src_tokens const &src_tokens,
	bz::u8string message,
	bz::vector<source_highlight> notes, bz::vector<source_highlight> suggestions
) const
{
	auto const open_paren  = bz::u8string(static_cast<size_t>(parens_count), '(');
	auto const close_paren = bz::u8string(static_cast<size_t>(parens_count), ')');
	notes.emplace_back(this->make_note_with_suggestion_around(
		{},
		src_tokens.begin, open_paren, src_tokens.end, close_paren,
		"put parenthesis around the expression to suppress this warning"
	));

	this->report_warning(kind, src_tokens, std::move(message), std::move(notes), std::move(suggestions));
}

[[nodiscard]] source_highlight parse_context::make_note(uint32_t file_id, uint32_t line, bz::u8string message)
{
	return source_highlight{
		file_id, line,
		char_pos(), char_pos(), char_pos(),
		{}, {},
		std::move(message)
	};
}

[[nodiscard]] source_highlight parse_context::make_note(lex::token_pos it, bz::u8string message)
{
	return source_highlight{
		it->src_pos.file_id, it->src_pos.line,
		it->src_pos.begin, it->src_pos.begin, it->src_pos.end,
		{}, {},
		std::move(message)
	};
}

[[nodiscard]] source_highlight parse_context::make_note(lex::src_tokens const &src_tokens, bz::u8string message)
{
	return source_highlight{
		src_tokens.pivot->src_pos.file_id, src_tokens.pivot->src_pos.line,
		src_tokens.begin->src_pos.begin, src_tokens.pivot->src_pos.begin, (src_tokens.end - 1)->src_pos.end,
		{}, {},
		std::move(message)
	};
}

[[nodiscard]] source_highlight parse_context::make_note(
	lex::token_pos it, bz::u8string message,
	char_pos suggestion_pos, bz::u8string suggestion_str
)
{
	return source_highlight{
		it->src_pos.file_id, it->src_pos.line,
		it->src_pos.begin, it->src_pos.begin, it->src_pos.end,
		{ char_pos(), char_pos(), suggestion_pos, std::move(suggestion_str) },
		{},
		std::move(message)
	};
}

[[nodiscard]] source_highlight parse_context::make_note(bz::u8string message)
{
	return source_highlight{
		global_context::compiler_file_id, 0,
		char_pos(), char_pos(), char_pos(),
		{}, {},
		std::move(message)
	};
}

[[nodiscard]] source_highlight parse_context::make_paren_match_note(
	lex::token_pos it, lex::token_pos open_paren_it
)
{
	if (open_paren_it->kind == lex::token::curly_open)
	{
		return make_note(open_paren_it, "to match this:");
	}

	bz_assert(
		open_paren_it->kind == lex::token::paren_open
		|| open_paren_it->kind == lex::token::square_open
		|| open_paren_it->kind == lex::token::angle_open
	);
	auto const suggestion_str = open_paren_it->kind == lex::token::paren_open ? ")"
		: open_paren_it->kind == lex::token::square_open ? "]"
		: ">";
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
		case lex::token::angle_close:
			if ((open_paren_it - 1)->kind == lex::token::angle_open && (open_paren_it - 1)->src_pos.end == open_paren_it->src_pos.begin)
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

[[nodiscard]] source_highlight parse_context::make_note_with_suggestion_before(
	lex::src_tokens const &src_tokens,
	lex::token_pos it, bz::u8string suggestion,
	bz::u8string message
)
{
	bz_assert(it != nullptr);
	if (src_tokens.pivot == nullptr)
	{
		return source_highlight{
			it->src_pos.file_id, it->src_pos.line,
			char_pos(), char_pos(), char_pos(),
			{ char_pos(), char_pos(), it->src_pos.begin, std::move(suggestion) },
			{},
			std::move(message)
		};
	}
	else
	{
		return source_highlight{
			src_tokens.pivot->src_pos.file_id, src_tokens.pivot->src_pos.line,
			src_tokens.begin->src_pos.begin, src_tokens.pivot->src_pos.begin, (src_tokens.end - 1)->src_pos.end,
			{ char_pos(), char_pos(), it->src_pos.begin, std::move(suggestion) },
			{},
			std::move(message)
		};
	}
}

[[nodiscard]] source_highlight parse_context::make_note_with_suggestion_around(
	lex::src_tokens const &src_tokens,
	lex::token_pos begin, bz::u8string first_suggestion,
	lex::token_pos end, bz::u8string second_suggestion,
	bz::u8string message
)
{
	bz_assert(begin != nullptr && end != nullptr);
	if (src_tokens.pivot == nullptr)
	{
		return source_highlight{
			begin->src_pos.file_id, begin->src_pos.line,
			char_pos(), char_pos(), char_pos(),
			{ char_pos(), char_pos(), begin->src_pos.begin, std::move(first_suggestion) },
			{ char_pos(), char_pos(), (end - 1)->src_pos.end, std::move(second_suggestion) },
			std::move(message)
		};
	}
	else
	{
		return source_highlight{
			src_tokens.pivot->src_pos.file_id, src_tokens.pivot->src_pos.line,
			src_tokens.begin->src_pos.begin, src_tokens.pivot->src_pos.begin, (src_tokens.end - 1)->src_pos.end,
			{ char_pos(), char_pos(), begin->src_pos.begin, std::move(first_suggestion) },
			{ char_pos(), char_pos(), (end - 1)->src_pos.end, std::move(second_suggestion) },
			std::move(message)
		};
	}
}


[[nodiscard]] source_highlight parse_context::make_suggestion_before(
	lex::token_pos it,
	char_pos erase_begin, char_pos erase_end,
	bz::u8string suggestion_str,
	bz::u8string message
)
{
	return source_highlight{
		it->src_pos.file_id, it->src_pos.line,
		char_pos(), char_pos(), char_pos(),
		{ erase_begin, erase_end, it->src_pos.begin, std::move(suggestion_str) },
		{},
		std::move(message)
	};
}

[[nodiscard]] source_highlight parse_context::make_suggestion_before(
	lex::token_pos first_it,
	char_pos first_erase_begin, char_pos first_erase_end,
	bz::u8string first_suggestion_str,
	lex::token_pos second_it,
	char_pos second_erase_begin, char_pos second_erase_end,
	bz::u8string second_suggestion_str,
	bz::u8string message
)
{
	return source_highlight{
		first_it->src_pos.file_id, first_it->src_pos.line,
		char_pos(), char_pos(), char_pos(),
		{ first_erase_begin, first_erase_end, first_it->src_pos.begin, std::move(first_suggestion_str) },
		{ second_erase_begin, second_erase_end, second_it->src_pos.begin, std::move(second_suggestion_str) },
		std::move(message)
	};
}

[[nodiscard]] source_highlight parse_context::make_suggestion_after(
	lex::token_pos it,
	char_pos erase_begin, char_pos erase_end,
	bz::u8string suggestion_str,
	bz::u8string message
)
{
	return source_highlight{
		it->src_pos.file_id, it->src_pos.line,
		char_pos(), char_pos(), char_pos(),
		{ erase_begin, erase_end, it->src_pos.end, std::move(suggestion_str) },
		{},
		std::move(message)
	};
}

[[nodiscard]] source_highlight parse_context::make_suggestion_around(
	lex::token_pos first,
	char_pos first_erase_begin, char_pos first_erase_end,
	bz::u8string first_suggestion_str,
	lex::token_pos last,
	char_pos second_erase_begin, char_pos second_erase_end,
	bz::u8string last_suggestion_str,
	bz::u8string message
)
{
	return source_highlight{
		first->src_pos.file_id, first->src_pos.line,
		char_pos(), char_pos(), char_pos(),
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
			? bz::vector<source_highlight>{ make_suggestion_after(stream - 1, ";", "add ';' here:") }
			: bz::vector<source_highlight>{};
		this->report_error(
			stream,
			stream->kind == lex::token::eof
			? bz::format("expected {} before end-of-file", lex::get_token_name_for_message(kind))
			: bz::format("expected {}", lex::get_token_name_for_message(kind)),
			{}, std::move(suggestions)
		);
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
		this->report_error(
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
		);
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

bool parse_context::has_main(void) const
{
	return this->global_ctx._main != nullptr;
}

ast::function_body *parse_context::get_main(void) const
{
	return this->global_ctx._main;
}

void parse_context::set_main(ast::function_body *body)
{
	bz_assert(!this->has_main());
	this->global_ctx._main = body;
}

/*
static bz::u8string format_array(bz::array_view<bz::u8string_view const> ids)
{
	if (ids.empty())
	{
		return "[]";
	}
	bz::u8string result = bz::format("[ '{}'", ids[0]);
	for (auto const id : ids.slice(1))
	{
		result += bz::format(", '{}'", id);
	}
	result += " ]";
	return result;
}
*/

[[nodiscard]] size_t parse_context::add_unresolved_scope(void)
{
	return this->current_unresolved_locals.size();
}

void parse_context::remove_unresolved_scope(size_t prev_size)
{
	this->current_unresolved_locals.resize(prev_size);
}

void parse_context::add_unresolved_local(ast::identifier const &id)
{
	bz_assert(!id.is_qualified);
	if (id.values.not_empty())
	{
		bz_assert(id.values.size() == 1);
		this->current_unresolved_locals.push_back(id.values[0]);
	}
}

void parse_context::add_unresolved_var_decl(ast::decl_variable const &var_decl)
{
	if (var_decl.tuple_decls.empty())
	{
		this->add_unresolved_local(var_decl.get_id());
	}
	else
	{
		auto it = var_decl.tuple_decls.begin();
		auto const end = var_decl.tuple_decls.end();
		for (; it != end; ++it)
		{
			if (it->is_variadic())
			{
				break;
			}
			this->add_unresolved_var_decl(*it);
		}
		if (it != end && it->is_variadic())
		{
			this->add_unresolved_var_decl(*it);
		}
		else if (var_decl.original_tuple_variadic_decl != nullptr)
		{
			this->add_unresolved_var_decl(*var_decl.original_tuple_variadic_decl);
		}
	}
}

void parse_context::add_local_variable(ast::decl_variable &var_decl)
{
	bz_assert(this->current_local_scope.scope != nullptr);
	bz_assert(this->current_local_scope.scope->is_local());
	auto &current_scope = this->current_local_scope.scope->get_local();
	if (var_decl.tuple_decls.empty())
	{
		var_decl.flags &= ~ast::decl_variable::used;
		current_scope.add_variable(var_decl);
		this->current_local_scope.symbol_count += 1;
		bz_assert(current_scope.symbols.size() == this->current_local_scope.symbol_count);
	}
	else
	{
		auto it = var_decl.tuple_decls.begin();
		auto const end = var_decl.tuple_decls.end();
		for (; it != end; ++it)
		{
			if (it->is_variadic())
			{
				break;
			}
			this->add_local_variable(*it);
		}
		if (it != end && it->is_variadic())
		{
			auto const variadic_decls = bz::basic_range{ it, end }
				.transform([](auto &decl) { return &decl; })
				.collect<ast::arena_vector>();
			this->add_local_variable(*it, std::move(variadic_decls));
		}
		else if (var_decl.original_tuple_variadic_decl != nullptr)
		{
			this->add_local_variable(*var_decl.original_tuple_variadic_decl, {});
		}
	}
}

void parse_context::add_local_variable(ast::decl_variable &original_decl, ast::arena_vector<ast::decl_variable *> variadic_decls)
{
	bz_assert(this->current_local_scope.scope != nullptr);
	bz_assert(this->current_local_scope.scope->is_local());
	auto &current_scope = this->current_local_scope.scope->get_local();
	if (original_decl.tuple_decls.empty())
	{
		current_scope.add_variable(original_decl, std::move(variadic_decls));
		this->current_local_scope.symbol_count += 1;
	}
	else
	{
		for (size_t i = 0; i < original_decl.tuple_decls.size(); ++i)
		{
			this->add_local_variable(
				original_decl.tuple_decls[i],
				variadic_decls.transform([i](auto const decl) {
					bz_assert(!decl->tuple_decls.empty());
					return &decl->tuple_decls[i];
				}).collect<ast::arena_vector>()
			);
		}
	}
}

void parse_context::add_local_function(ast::decl_function &func_decl)
{
	bz_assert(this->current_local_scope.scope != nullptr);
	bz_assert(this->current_local_scope.scope->is_local());
	auto &current_scope = this->current_local_scope.scope->get_local();
	current_scope.add_function(func_decl);
	this->current_local_scope.symbol_count += 1;
}

void parse_context::add_local_operator(ast::decl_operator &op_decl)
{
	this->report_error(op_decl.body.src_tokens, "operator declarations are not allowed in local scope");
}

void parse_context::add_local_type_alias(ast::decl_type_alias &type_alias)
{
	bz_assert(this->current_local_scope.scope != nullptr);
	bz_assert(this->current_local_scope.scope->is_local());
	auto &current_scope = this->current_local_scope.scope->get_local();
	current_scope.add_type_alias(type_alias);
	this->current_local_scope.symbol_count += 1;
}

void parse_context::add_function_for_compilation(ast::function_body &func_body) const
{
	this->global_ctx.add_compile_function(func_body);
}

static ast::typespec get_function_type(ast::function_body &body)
{
	auto const &return_type = body.return_type;
	auto param_types = body.params.transform([](auto &p) { return p.get_type(); }).collect<ast::arena_vector>();
	return ast::make_function_typespec(body.src_tokens, std::move(param_types), return_type, body.cc);
}

static ast::expression make_variable_expression(
	lex::src_tokens const &src_tokens,
	ast::decl_variable *var_decl,
	ast::expr_t result_expr,
	parse_context &context
)
{
	if (var_decl->is_moved() && !context.in_unevaluated_context)
	{
		context.report_error(
			src_tokens,
			bz::format("variable '{}' has been moved, and is no longer usable", var_decl->get_id().format_as_unqualified()),
			{ context.make_note(var_decl->move_position, "variable was moved here") }
		);
		return ast::make_error_expression(src_tokens, std::move(result_expr));
	}

	auto id_type_kind = ast::expression_type_kind::lvalue;
	ast::typespec_view id_type = var_decl->get_type();
	if (id_type.is<ast::ts_lvalue_reference>())
	{
		id_type_kind = ast::expression_type_kind::lvalue_reference;
		id_type = id_type.get<ast::ts_lvalue_reference>();
	}
	else if (var_decl->is_tuple_outer_ref())
	{
		id_type_kind = ast::expression_type_kind::lvalue_reference;
	}
	else if (id_type.is<ast::ts_move_reference>())
	{
		id_type_kind = ast::expression_type_kind::lvalue_reference;
		id_type = id_type.get<ast::ts_move_reference>();
	}

	if (id_type.is_empty())
	{
		bz_assert(context.has_errors());
		return ast::make_error_expression(src_tokens, std::move(result_expr));
	}
	else if (id_type.is<ast::ts_consteval>() && var_decl->init_expr.is_constant())
	{
		auto &init_expr = var_decl->init_expr;
		bz_assert(init_expr.is_constant());
		ast::typespec result_type = id_type.get<ast::ts_consteval>();
		result_type.add_layer<ast::ts_const>();
		return ast::make_constant_expression(
			src_tokens,
			id_type_kind, std::move(result_type),
			init_expr.get_constant_value(),
			std::move(result_expr)
		);
	}
	else if (id_type.is<ast::ts_consteval>())
	{
		ast::typespec result_type = id_type.get<ast::ts_consteval>();
		result_type.add_layer<ast::ts_const>();
		return ast::make_dynamic_expression(
			src_tokens,
			id_type_kind, std::move(result_type),
			std::move(result_expr),
			ast::destruct_operation()
		);
	}
	else if (id_type.is_typename())
	{
		auto &init_expr = var_decl->init_expr;
		bz_assert(init_expr.is_typename());
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::type_name, ast::make_typename_typespec(nullptr),
			ast::constant_value(init_expr.get_typename()),
			std::move(result_expr)
		);
	}
	else
	{
		return ast::make_dynamic_expression(
			src_tokens,
			id_type_kind, id_type,
			std::move(result_expr),
			ast::destruct_operation()
		);
	}
}

static ast::expression make_variable_expression(
	lex::src_tokens const &src_tokens,
	ast::identifier id,
	ast::decl_variable *var_decl,
	int loop_boundary_count,
	bool is_local,
	parse_context &context
)
{
	return make_variable_expression(
		src_tokens,
		var_decl,
		ast::make_expr_variable_name(std::move(id), var_decl, loop_boundary_count, is_local),
		context
	);
}

struct function_overload_set_decls
{
	ast::arena_vector<ast::decl_function *> func_decls;
	ast::arena_vector<ast::decl_function_alias *> alias_decls;
};

static ast::expression make_function_name_expression(
	lex::src_tokens const &src_tokens,
	ast::identifier id,
	ast::decl_function *func_decl
)
{
	if (func_decl->body.is_generic())
	{
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::function_name, ast::typespec(),
			ast::constant_value(),
			ast::make_expr_function_name(std::move(id), func_decl)
		);
	}
	else
	{
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::function_name, get_function_type(func_decl->body),
			ast::constant_value(&func_decl->body),
			ast::make_expr_function_name(std::move(id), func_decl)
		);
	}
}

static ast::expression make_function_name_expression(
	lex::src_tokens const &src_tokens,
	ast::identifier id,
	ast::decl_function_alias *alias_decl
)
{
	if (alias_decl->aliased_decls.size() == 1 && !alias_decl->aliased_decls[0]->body.is_generic())
	{
		auto const decl = alias_decl->aliased_decls[0];
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::function_alias_name, get_function_type(decl->body),
			ast::constant_value(&decl->body),
			ast::make_expr_function_alias_name(std::move(id), alias_decl)
		);
	}
	else
	{
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::function_alias_name, ast::typespec(),
			ast::constant_value(),
			ast::make_expr_function_alias_name(std::move(id), alias_decl)
		);
	}
}

static ast::expression make_function_name_expression(
	lex::src_tokens const &src_tokens,
	ast::identifier id,
	function_overload_set_decls const &fn_set
)
{
	if (fn_set.alias_decls.size() == 0 && fn_set.func_decls.size() == 1)
	{
		auto const decl = fn_set.func_decls[0];
		return make_function_name_expression(src_tokens, std::move(id), decl);
	}
	else if (fn_set.alias_decls.size() == 1 && fn_set.func_decls.size() == 0)
	{
		auto const decl = fn_set.alias_decls[0];
		return make_function_name_expression(src_tokens, std::move(id), decl);
	}
	else if (fn_set.alias_decls.size() == 0 && fn_set.func_decls.size() == 0)
	{
		return ast::make_error_expression(src_tokens, ast::make_expr_function_name(std::move(id), nullptr));
	}
	else
	{
		auto set = ast::function_set_t();
		set.stmts.reserve(fn_set.func_decls.size() + fn_set.alias_decls.size());
		set.stmts.append(fn_set.func_decls.transform([](auto const func_decl) { return ast::statement_view(func_decl); }));
		set.stmts.append(fn_set.alias_decls.transform([](auto const alias_decl) { return ast::statement_view(alias_decl); }));
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::function_overload_set, ast::typespec(),
			ast::constant_value(),
			ast::make_expr_function_overload_set(std::move(id), std::move(set))
		);
	}
}

static ast::expression make_type_expression(
	lex::src_tokens const &src_tokens,
	ast::identifier id,
	ast::decl_struct *type,
	parse_context &context
)
{
	auto &info = type->info;
	if (!info.is_generic())
	{
		context.add_to_resolve_queue(src_tokens, info);
		resolve::resolve_type_info_symbol(info, context);
		context.pop_resolve_queue();
	}
	if (info.state != ast::resolve_state::error)
	{
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::type_name,
			ast::make_typename_typespec(nullptr),
			ast::constant_value(ast::make_base_type_typespec(src_tokens, &info)),
			ast::make_expr_struct_name(std::move(id), type)
		);
	}
	else
	{
		return ast::make_error_expression(src_tokens, ast::make_expr_struct_name(std::move(id), type));
	}
}

static ast::expression make_enum_type_expression(
	lex::src_tokens const &src_tokens,
	ast::identifier id,
	ast::decl_enum *decl
)
{
	if (decl->state != ast::resolve_state::error)
	{
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::type_name,
			ast::make_typename_typespec(nullptr),
			ast::constant_value(ast::make_enum_typespec(src_tokens, decl)),
			ast::make_expr_enum_name(std::move(id), decl)
		);
	}
	else
	{
		return ast::make_error_expression(src_tokens, ast::make_expr_enum_name(std::move(id), decl));
	}
}

static ast::expression make_type_alias_expression(
	lex::src_tokens const &src_tokens,
	ast::identifier id,
	ast::decl_type_alias *type_alias
)
{
	auto const type = type_alias->get_type();
	if (type.not_empty())
	{
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::type_name,
			ast::make_typename_typespec(nullptr),
			ast::constant_value(type),
			ast::make_expr_type_alias_name(std::move(id), type_alias)
		);
	}
	else
	{
		return ast::make_error_expression(src_tokens, ast::make_expr_type_alias_name(std::move(id), type_alias));
	}
}

using symbol_t = bz::variant<
	ast::decl_variable *,
	ast::variadic_var_decl_ref,
	ast::decl_function *,
	ast::decl_function_alias *,
	function_overload_set_decls,
	ast::decl_type_alias *,
	ast::decl_struct *,
	ast::decl_enum *
>;

static symbol_t symbol_ref_from_local_symbol(ast::local_symbol_t const &symbol)
{
	static_assert(ast::local_symbol_t::variant_count == 7);
	switch (symbol.index())
	{
	case ast::local_symbol_t::index_of<ast::decl_variable *>:
		return symbol.get_variable();
	case ast::local_symbol_t::index_of<ast::variadic_var_decl>:
	{
		auto const &variadic_decl = symbol.get_variadic_variable();
		return ast::variadic_var_decl_ref{ variadic_decl.original_decl, variadic_decl.variadic_decls };
	}
	case ast::local_symbol_t::index_of<ast::decl_function *>:
		return symbol.get_function();
	case ast::local_symbol_t::index_of<ast::decl_function_alias *>:
		return symbol.get_function_alias();
	case ast::local_symbol_t::index_of<ast::decl_type_alias *>:
		return symbol.get_type_alias();
	case ast::local_symbol_t::index_of<ast::decl_struct *>:
		return symbol.get_struct();
	case ast::local_symbol_t::index_of<ast::decl_enum *>:
		return symbol.get_enum();
	default:
		bz_unreachable;
	}
}

static ast::expression expression_from_symbol(
	lex::src_tokens const &src_tokens,
	ast::identifier id,
	symbol_t const &symbol,
	int loop_boundary_count,
	bool is_local,
	parse_context &context
)
{
	static_assert(symbol_t::variant_count == 8);

	switch (symbol.index())
	{
	case symbol_t::index_of<ast::decl_variable *>:
	{
		auto const var_decl = symbol.get<ast::decl_variable *>();
		if (var_decl->state < ast::resolve_state::symbol)
		{
			context.add_to_resolve_queue(src_tokens, *var_decl);
			resolve::resolve_variable_symbol(*var_decl, context);
			context.pop_resolve_queue();
		}
		var_decl->flags |= ast::decl_variable::used;
		return make_variable_expression(
			src_tokens,
			std::move(id),
			symbol.get<ast::decl_variable *>(),
			loop_boundary_count,
			is_local,
			context
		);
	}
	case symbol_t::index_of<ast::variadic_var_decl_ref>:
	{
		auto const &variadic_decl = symbol.get<ast::variadic_var_decl_ref>();
		variadic_decl.original_decl->flags |= ast::decl_variable::used;
		if (context.parsing_variadic_expansion)
		{
			return ast::make_unresolved_expression(
				src_tokens,
				ast::make_unresolved_expr_unresolved_identifier(std::move(id))
			);
		}

		auto const is_valid = context.register_variadic(src_tokens, variadic_decl);
		if (!is_valid)
		{
			return ast::make_error_expression(src_tokens, ast::make_expr_variable_name(std::move(id), nullptr, 0, false));
		}
		else if (variadic_decl.variadic_decls.empty())
		{
			return ast::make_unresolved_expression(src_tokens);
		}
		else
		{
			return make_variable_expression(
				src_tokens,
				std::move(id),
				variadic_decl.variadic_decls[context.get_variadic_index()],
				loop_boundary_count,
				is_local,
				context
			);
		}
	}
	case symbol_t::index_of<ast::decl_function *>:
	{
		auto const func_decl = symbol.get<ast::decl_function *>();
		return make_function_name_expression(src_tokens, std::move(id), func_decl);
	}
	case symbol_t::index_of<ast::decl_function_alias *>:
	{
		auto const alias_decl = symbol.get<ast::decl_function_alias *>();
		context.add_to_resolve_queue(src_tokens, *alias_decl);
		resolve::resolve_function_alias(*alias_decl, context);
		context.pop_resolve_queue();
		return make_function_name_expression(src_tokens, std::move(id), alias_decl);
	}
	case symbol_t::index_of<function_overload_set_decls>:
	{
		auto const &func_set = symbol.get<function_overload_set_decls>();
		for (auto const alias_decl : func_set.alias_decls)
		{
			context.add_to_resolve_queue(src_tokens, *alias_decl);
			resolve::resolve_function_alias(*alias_decl, context);
			context.pop_resolve_queue();
		}
		return make_function_name_expression(src_tokens, std::move(id), func_set);
	}
	case symbol_t::index_of<ast::decl_type_alias *>:
	{
		auto const alias_decl = symbol.get<ast::decl_type_alias *>();
		if (alias_decl->state != ast::resolve_state::all)
		{
			context.add_to_resolve_queue(src_tokens, *alias_decl);
			resolve::resolve_type_alias(*alias_decl, context);
			context.pop_resolve_queue();
		}
		return make_type_alias_expression(src_tokens, std::move(id), symbol.get<ast::decl_type_alias *>());
	}
	case symbol_t::index_of<ast::decl_struct *>:
		return make_type_expression(src_tokens, std::move(id), symbol.get<ast::decl_struct *>(), context);
	case symbol_t::index_of<ast::decl_enum *>:
		return make_enum_type_expression(src_tokens, std::move(id), symbol.get<ast::decl_enum *>());
	default:
		bz_unreachable;
	}
}

static source_highlight get_ambiguous_note(symbol_t const &symbol)
{
	static_assert(symbol_t::variant_count == 8);
	switch (symbol.index())
	{
	case symbol_t::index_of<ast::decl_variable *>:
	{
		auto const var_decl = symbol.get<ast::decl_variable *>();
		return parse_context::make_note(
			var_decl->src_tokens,
			bz::format("it may refer to the variable '{}'", var_decl->get_id().format_as_unqualified())
		);
	}
	case symbol_t::index_of<ast::variadic_var_decl_ref>:
	{
		auto const &[var_decl, _] = symbol.get<ast::variadic_var_decl_ref>();
		return parse_context::make_note(
			var_decl->src_tokens,
			bz::format("it may refer to the variable '{}'", var_decl->get_id().format_as_unqualified())
		);
	}
	case symbol_t::index_of<ast::decl_function *>:
	{
		auto const func_decl = symbol.get<ast::decl_function *>();
		return parse_context::make_note(
			func_decl->body.src_tokens,
			bz::format("it may refer to '{}'", func_decl->body.get_signature())
		);
	}
	case symbol_t::index_of<ast::decl_function_alias *>:
	{
		auto const alias_decl = symbol.get<ast::decl_function_alias *>();
		return parse_context::make_note(
			alias_decl->src_tokens,
			bz::format("it may refer to the alias 'function {}'", alias_decl->id.format_as_unqualified())
		);
	}
	case symbol_t::index_of<function_overload_set_decls>:
	{
		auto const &[func_decls, alias_decls] = symbol.get<function_overload_set_decls>();
		if (func_decls.not_empty())
		{
			return parse_context::make_note(
				func_decls.front()->body.src_tokens,
				bz::format("it may refer to '{}'", func_decls.front()->body.get_signature())
			);
		}
		else
		{
			bz_assert(alias_decls.not_empty());
			return parse_context::make_note(
				alias_decls.front()->src_tokens,
				bz::format("it may refer to the alias 'function {}'", alias_decls.front()->id.format_as_unqualified())
			);
		}
	}
	case symbol_t::index_of<ast::decl_type_alias *>:
	{
		auto const alias_decl = symbol.get<ast::decl_type_alias *>();
		return parse_context::make_note(
			alias_decl->src_tokens,
			bz::format("it may refer to the alias 'type {}'", alias_decl->id.format_as_unqualified())
		);
	}
	case symbol_t::index_of<ast::decl_struct *>:
	{
		auto const struct_decl = symbol.get<ast::decl_struct *>();
		return parse_context::make_note(
			struct_decl->info.src_tokens,
			bz::format("it may refer to the type 'struct {}'", struct_decl->id.format_as_unqualified())
		);
	}
	case symbol_t::index_of<ast::decl_enum *>:
	{
		auto const enum_decl = symbol.get<ast::decl_enum *>();
		return parse_context::make_note(
			enum_decl->src_tokens,
			bz::format("it may refer to the type 'enum {}'", enum_decl->id.format_as_unqualified())
		);
	}
	default:
		bz_unreachable;
	}
}

static ast::function_overload_set *find_function_set_by_qualified_id(
	bz::array_view<ast::function_overload_set> func_sets,
	ast::identifier const &id
)
{
	auto const it = std::find_if(
		func_sets.begin(), func_sets.end(),
		[&id](auto const &set) {
			bz_assert(set.id.is_qualified);
			return set.id == id;
		}
	);
	if (it != func_sets.end())
	{
		return &*it;
	}
	else
	{
		return nullptr;
	}
}

static ast::decl_variable *find_variable_by_qualified_id(
	bz::array_view<ast::decl_variable *> variables,
	ast::identifier const &id
)
{
	auto const it = std::find_if(
		variables.begin(), variables.end(),
		[&id](auto const decl) {
			return decl->get_id() == id;
		}
	);
	if (it != variables.end())
	{
		return *it;
	}
	else
	{
		return nullptr;
	}
}

static ast::variadic_var_decl_ref find_variadic_variable_by_qualified_id(
	bz::array_view<ast::variadic_var_decl> variadic_variables,
	ast::identifier const &id
)
{
	auto const it = std::find_if(
		variadic_variables.begin(), variadic_variables.end(),
		[&id](auto const variadic_var) {
			return variadic_var.original_decl->get_id() == id;
		}
	);
	if (it != variadic_variables.end())
	{
		return { it->original_decl, it->variadic_decls };
	}
	else
	{
		return {};
	}
}

static ast::decl_type_alias *find_type_alias_by_qualified_id(
	bz::array_view<ast::decl_type_alias *> type_aliases,
	ast::identifier const &id
)
{
	auto const it = std::find_if(
		type_aliases.begin(), type_aliases.end(),
		[&id](auto const alias) {
			return alias->id == id;
		}
	);
	if (it != type_aliases.end())
	{
		return *it;
	}
	else
	{
		return nullptr;
	}
}

static ast::decl_struct *find_struct_by_qualified_id(
	bz::array_view<ast::decl_struct *> structs,
	ast::identifier const &id
)
{
	auto const it = std::find_if(
		structs.begin(), structs.end(),
		[&id](auto const decl) {
			return decl->id == id;
		}
	);
	if (it != structs.end())
	{
		return *it;
	}
	else
	{
		return nullptr;
	}
}

static ast::decl_enum *find_enum_by_qualified_id(
	bz::array_view<ast::decl_enum *> enums,
	ast::identifier const &id
)
{
	auto const it = std::find_if(
		enums.begin(), enums.end(),
		[&id](auto const decl) {
			return decl->id == id;
		}
	);
	if (it != enums.end())
	{
		return *it;
	}
	else
	{
		return nullptr;
	}
}

static bool unqualified_equals(
	ast::identifier const &lhs,
	ast::identifier const &rhs,
	bz::array_view<bz::u8string_view const> rhs_id_scope
)
{
	bz_assert(!rhs.is_qualified);
	if (rhs.values.size() > lhs.values.size())
	{
		return false;
	}
	else if (rhs.values.size() + rhs_id_scope.size() < lhs.values.size())
	{
		return false;
	}
	else
	{
		auto const lhs_size = lhs.values.size();
		auto const rhs_size = rhs.values.size();
		return lhs.values.slice(lhs_size - rhs_size, lhs_size) == rhs.values.as_array_view()
			&& lhs.values.slice(0, lhs_size - rhs_size) == rhs_id_scope.slice(0, lhs_size - rhs_size);
	}
}

static auto get_function_set_range_by_unqualified_id(
	bz::array_view<ast::function_overload_set> func_sets,
	ast::identifier const &id,
	bz::array_view<bz::u8string_view const> id_scope
)
{
	return func_sets.filter(
		[&id, id_scope](auto const &set) {
			return unqualified_equals(set.id, id, id_scope);
		}
	);
}

static auto get_variable_range_by_unqualified_id(
	bz::array_view<ast::decl_variable *> variables,
	ast::identifier const &id,
	bz::array_view<bz::u8string_view const> id_scope
)
{
	return variables.filter(
		[&id, id_scope](auto const decl) {
			return unqualified_equals(decl->get_id(), id, id_scope);
		}
	);
}

static auto get_variadic_variable_range_by_unqualified_id(
	bz::array_view<ast::variadic_var_decl> variadic_variables,
	ast::identifier const &id,
	bz::array_view<bz::u8string_view const> id_scope
)
{
	return variadic_variables.filter(
		[&id, id_scope](auto const &variadic_var) {
			return unqualified_equals(variadic_var.original_decl->get_id(), id, id_scope);
		}
	);
}

static auto get_type_alias_range_by_unqualified_id(
	bz::array_view<ast::decl_type_alias *> type_aliases,
	ast::identifier const &id,
	bz::array_view<bz::u8string_view const> id_scope
)
{
	return type_aliases.filter(
		[&id, id_scope](auto const alias) {
			return unqualified_equals(alias->id, id, id_scope);
		}
	);
}

static auto get_struct_range_by_unqualified_id(
	bz::array_view<ast::decl_struct *> structs,
	ast::identifier const &id,
	bz::array_view<bz::u8string_view const> id_scope
)
{
	return structs.filter(
		[&id, id_scope](auto const decl) {
			return unqualified_equals(decl->id, id, id_scope);
		}
	);
}

static auto get_enum_range_by_unqualified_id(
	bz::array_view<ast::decl_enum *> enums,
	ast::identifier const &id,
	bz::array_view<bz::u8string_view const> id_scope
)
{
	return enums.filter(
		[&id, id_scope](auto const decl) {
			return unqualified_equals(decl->id, id, id_scope);
		}
	);
}

static void try_find_function_set_by_qualified_id(
	symbol_t &result,
	ast::global_scope_t &scope,
	ast::identifier const &id,
	parse_context &context
)
{
	auto const func_set = find_function_set_by_qualified_id(scope.function_sets, id);
	if (func_set != nullptr && result.not_null())
	{
		context.report_error(
			lex::src_tokens::from_range(id.tokens),
			bz::format("identifier '{}' is ambiguous", id.as_string()),
			{
				get_ambiguous_note(result),
				func_set->func_decls.not_empty()
				? context.make_note(
					func_set->func_decls.front()->body.src_tokens,
					bz::format("it may refer to '{}'", func_set->func_decls.front()->body.get_signature())
				)
				: context.make_note(
					func_set->alias_decls.front()->src_tokens,
					bz::format("it may refer to the alias 'function {}'", id.format_as_unqualified())
				)
			}
		);
	}
	else if (func_set != nullptr)
	{
		auto &decls = result.emplace<function_overload_set_decls>();
		decls.func_decls.append(func_set->func_decls);
		decls.alias_decls.append(func_set->alias_decls);
	}
}

static void try_find_variable_by_qualified_id(
	symbol_t &result,
	ast::global_scope_t &scope,
	ast::identifier const &id,
	parse_context &context
)
{
	auto const var_decl = find_variable_by_qualified_id(scope.variables, id);
	if (var_decl != nullptr && result.not_null())
	{
		context.report_error(
			lex::src_tokens::from_range(id.tokens),
			bz::format("identifier '{}' is ambiguous", id.as_string()),
			{
				get_ambiguous_note(result),
				context.make_note(
					var_decl->src_tokens,
					bz::format("it may refer to the variable '{}'", id.format_as_unqualified())
				)
			}
		);
	}
	else if (var_decl != nullptr)
	{
		result = var_decl;
	}
}

static void try_find_variadic_variable_by_qualified_id(
	symbol_t &result,
	ast::global_scope_t &scope,
	ast::identifier const &id,
	parse_context &context
)
{
	auto const var_decl = find_variadic_variable_by_qualified_id(scope.variadic_variables, id);
	if (var_decl.original_decl != nullptr && result.not_null())
	{
		context.report_error(
			lex::src_tokens::from_range(id.tokens),
			bz::format("identifier '{}' is ambiguous", id.as_string()),
			{
				get_ambiguous_note(result),
				context.make_note(
					var_decl.original_decl->src_tokens,
					bz::format("it may refer to the variable '{}'", id.format_as_unqualified())
				)
			}
		);
	}
	else if (var_decl.original_decl != nullptr)
	{
		result = var_decl;
	}
}

static void try_find_type_alias_by_qualified_id(
	symbol_t &result,
	ast::global_scope_t &scope,
	ast::identifier const &id,
	parse_context &context
)
{
	auto const alias_decl = find_type_alias_by_qualified_id(scope.type_aliases, id);
	if (alias_decl != nullptr && result.not_null())
	{
		context.report_error(
			lex::src_tokens::from_range(id.tokens),
			bz::format("identifier '{}' is ambiguous", id.as_string()),
			{
				get_ambiguous_note(result),
				context.make_note(
					alias_decl->src_tokens,
					bz::format("it may refer to the alias 'type {}'", id.format_as_unqualified())
				)
			}
		);
	}
	else if (alias_decl != nullptr)
	{
		result = alias_decl;
	}
}

static void try_find_struct_by_qualified_id(
	symbol_t &result,
	ast::global_scope_t &scope,
	ast::identifier const &id,
	parse_context &context
)
{
	auto const struct_decl = find_struct_by_qualified_id(scope.structs, id);
	if (struct_decl != nullptr && result.not_null())
	{
		context.report_error(
			lex::src_tokens::from_range(id.tokens),
			bz::format("identifier '{}' is ambiguous", id.as_string()),
			{
				get_ambiguous_note(result),
				context.make_note(
					struct_decl->info.src_tokens,
					bz::format("it may refer to the type 'struct {}'", id.format_as_unqualified())
				)
			}
		);
	}
	else if (struct_decl != nullptr)
	{
		result = struct_decl;
	}
}

static void try_find_enum_by_qualified_id(
	symbol_t &result,
	ast::global_scope_t &scope,
	ast::identifier const &id,
	parse_context &context
)
{
	auto const enum_decl = find_enum_by_qualified_id(scope.enums, id);
	if (enum_decl != nullptr && result.not_null())
	{
		context.report_error(
			lex::src_tokens::from_range(id.tokens),
			bz::format("identifier '{}' is ambiguous", id.as_string()),
			{
				get_ambiguous_note(result),
				context.make_note(
					enum_decl->src_tokens,
					bz::format("it may refer to the type 'enum {}'", id.format_as_unqualified())
				)
			}
		);
	}
	else if (enum_decl != nullptr)
	{
		result = enum_decl;
	}
}

static void try_find_function_set_by_unqualified_id(
	symbol_t &result,
	ast::global_scope_t &scope,
	ast::identifier const &id,
	parse_context &context
)
{
	for (auto &func_set : get_function_set_range_by_unqualified_id(scope.function_sets, id, scope.id_scope))
	{
		if (result.not_null() && !result.is<function_overload_set_decls>())
		{
			context.report_error(
				lex::src_tokens::from_range(id.tokens),
				bz::format("identifier '{}' is ambiguous", id.as_string()),
				{
					get_ambiguous_note(result),
					func_set.func_decls.not_empty()
					? context.make_note(
						func_set.func_decls.front()->body.src_tokens,
						bz::format("it may refer to '{}'", func_set.func_decls.front()->body.get_signature())
					)
					: context.make_note(
						func_set.alias_decls.front()->src_tokens,
						bz::format("it may refer to the alias 'function {}'", func_set.id.format_as_unqualified())
					)
				}
			);
			return;
		}
		else
		{
			auto &decls = result.is_null()
				? result.emplace<function_overload_set_decls>()
				: result.get<function_overload_set_decls>();
			decls.func_decls.append(func_set.func_decls);
			decls.alias_decls.append(func_set.alias_decls);
		}
	}
}

static void try_find_variable_by_unqualified_id(
	symbol_t &result,
	ast::global_scope_t &scope,
	ast::identifier const &id,
	parse_context &context
)
{
	for (auto const var_decl : get_variable_range_by_unqualified_id(scope.variables, id, scope.id_scope))
	{
		if (result.not_null())
		{
			context.report_error(
				lex::src_tokens::from_range(id.tokens),
				bz::format("identifier '{}' is ambiguous", id.as_string()),
				{
					get_ambiguous_note(result),
					context.make_note(
						var_decl->src_tokens,
						bz::format("it may refer to the variable '{}'", var_decl->get_id().format_as_unqualified())
					)
				}
			);
			return;
		}
		else
		{
			result = var_decl;
		}
	}
}

static void try_find_variadic_variable_by_unqualified_id(
	symbol_t &result,
	ast::global_scope_t &scope,
	ast::identifier const &id,
	parse_context &context
)
{
	for (auto const &variadic_var_decl : get_variadic_variable_range_by_unqualified_id(scope.variadic_variables, id, scope.id_scope))
	{
		if (result.not_null())
		{
			context.report_error(
				lex::src_tokens::from_range(id.tokens),
				bz::format("identifier '{}' is ambiguous", id.as_string()),
				{
					get_ambiguous_note(result),
					context.make_note(
						variadic_var_decl.original_decl->src_tokens,
						bz::format("it may refer to the variable '{}'", variadic_var_decl.original_decl->get_id().format_as_unqualified())
					)
				}
			);
			return;
		}
		else
		{
			result = ast::variadic_var_decl_ref{ variadic_var_decl.original_decl, variadic_var_decl.variadic_decls };
		}
	}
}

static void try_find_type_alias_by_unqualified_id(
	symbol_t &result,
	ast::global_scope_t &scope,
	ast::identifier const &id,
	parse_context &context
)
{
	for (auto const alias_decl : get_type_alias_range_by_unqualified_id(scope.type_aliases, id, scope.id_scope))
	{
		if (result.not_null())
		{
			context.report_error(
				lex::src_tokens::from_range(id.tokens),
				bz::format("identifier '{}' is ambiguous", id.as_string()),
				{
					get_ambiguous_note(result),
					context.make_note(
						alias_decl->src_tokens,
						bz::format("it may refer to the alias 'type {}'", alias_decl->id.format_as_unqualified())
					)
				}
			);
			return;
		}
		else
		{
			result = alias_decl;
		}
	}
}

static void try_find_struct_by_unqualified_id(
	symbol_t &result,
	ast::global_scope_t &scope,
	ast::identifier const &id,
	parse_context &context
)
{
	for (auto const struct_decl : get_struct_range_by_unqualified_id(scope.structs, id, scope.id_scope))
	{
		if (result.not_null())
		{
			context.report_error(
				lex::src_tokens::from_range(id.tokens),
				bz::format("identifier '{}' is ambiguous", id.as_string()),
				{
					get_ambiguous_note(result),
					context.make_note(
						struct_decl->info.src_tokens,
						bz::format("it may refer to the type 'struct {}'", struct_decl->id.format_as_unqualified())
					)
				}
			);
			return;
		}
		else
		{
			result = struct_decl;
		}
	}
}

static void try_find_enum_by_unqualified_id(
	symbol_t &result,
	ast::global_scope_t &scope,
	ast::identifier const &id,
	parse_context &context
)
{
	for (auto const enum_decl : get_enum_range_by_unqualified_id(scope.enums, id, scope.id_scope))
	{
		if (result.not_null())
		{
			context.report_error(
				lex::src_tokens::from_range(id.tokens),
				bz::format("identifier '{}' is ambiguous", id.as_string()),
				{
					get_ambiguous_note(result),
					context.make_note(
						enum_decl->src_tokens,
						bz::format("it may refer to the type 'enum {}'", enum_decl->id.format_as_unqualified())
					)
				}
			);
			return;
		}
		else
		{
			result = enum_decl;
		}
	}
}

static symbol_t find_id_in_global_scope(ast::global_scope_t &scope, ast::identifier const &id, parse_context &context)
{
	if (id.values.empty())
	{
		return {};
	}

	if (id.is_qualified)
	{
		if (scope.parent.scope != nullptr)
		{
			// in this case the scope must be inside a struct, meaning the symbols can't be accessed with a qualified lookup
			return {};
		}

		symbol_t result;

		static_assert(sizeof (ast::global_scope_t) == 200);
		static_assert(symbol_t::variant_count == 8);
		try_find_function_set_by_qualified_id(result, scope, id, context);
		try_find_variable_by_qualified_id(result, scope, id, context);
		try_find_variadic_variable_by_qualified_id(result, scope, id, context);
		try_find_type_alias_by_qualified_id(result, scope, id, context);
		try_find_struct_by_qualified_id(result, scope, id, context);
		try_find_enum_by_qualified_id(result, scope, id, context);

		return result;
	}
	else
	{
		symbol_t result;

		static_assert(sizeof (ast::global_scope_t) == 200);
		static_assert(symbol_t::variant_count == 8);
		try_find_function_set_by_unqualified_id(result, scope, id, context);
		try_find_variable_by_unqualified_id(result, scope, id, context);
		try_find_variadic_variable_by_unqualified_id(result, scope, id, context);
		try_find_type_alias_by_unqualified_id(result, scope, id, context);
		try_find_struct_by_unqualified_id(result, scope, id, context);
		try_find_enum_by_unqualified_id(result, scope, id, context);

		return result;
	}
}

struct id_search_result_t
{
	symbol_t symbol;
	int loop_boundary_count;
	bool is_local;
};

static id_search_result_t find_id_in_scope(ast::enclosing_scope_t scope, ast::identifier const &id, parse_context &context)
{
	int loop_boundary_count = 0;
	bool is_local = true;
	while (scope.scope != nullptr)
	{
		if (scope.scope->is_local())
		{
			auto const res = scope.scope->get_local().find_by_id(id, scope.symbol_count);
			if (res != nullptr)
			{
				return { symbol_ref_from_local_symbol(*res), loop_boundary_count, is_local };
			}

			if (scope.scope->get_local().is_loop_scope)
			{
				loop_boundary_count += 1;
			}
			scope = scope.scope->get_local().parent;
		}
		else if (scope.scope->is_global())
		{
			is_local = false;
			auto result = find_id_in_global_scope(scope.scope->get_global(), id, context);
			if (result.not_null())
			{
				return { result, loop_boundary_count, is_local };
			}

			scope = scope.scope->get_global().parent;
		}
		else
		{
			bz_unreachable;
		}
	}
	return {};
}

ast::expression parse_context::make_identifier_expression(ast::identifier id)
{
	// ==== local decls ====
	// we go in reverse through the scopes and the variables
	// in case there's shadowing
	auto const src_tokens = lex::src_tokens::from_range(id.tokens);

	if (!id.is_qualified && id.values.size() == 1 && this->current_unresolved_locals.contains(id.values[0]))
	{
		return ast::make_unresolved_expression(
			src_tokens,
			ast::make_unresolved_expr_unresolved_identifier(std::move(id))
		);
	}

	auto const [symbol, loop_boundary_count, is_local] = find_id_in_scope(this->get_current_enclosing_scope(), id, *this);

	if (
		this->in_unresolved_context
		&& (
			symbol.is_null()
			|| !symbol.is<ast::decl_struct *>()
			|| !symbol.get<ast::decl_struct *>()->info.is_generic()
		)
	)
	{
		return ast::make_unresolved_expression(
			src_tokens,
			ast::make_unresolved_expr_unresolved_identifier(std::move(id))
		);
	}

	if (symbol.not_null())
	{
		return expression_from_symbol(src_tokens, std::move(id), symbol, loop_boundary_count, is_local, *this);
	}

	// builtin types
	// qualification doesn't matter here, they act as globally defined types
	if (id.values.size() == 1)
	{
		auto const id_value = id.values.front();
		if (auto const builtin_type = this->get_builtin_type(id_value); builtin_type.not_empty())
		{
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::type_name,
				ast::make_typename_typespec(nullptr),
				ast::constant_value(builtin_type),
				ast::make_expr_struct_name(std::move(id), nullptr)
			);
		}
		else if (id_value.starts_with("__builtin"))
		{
			auto const it = std::find_if(ast::intrinsic_info.begin(), ast::intrinsic_info.end(), [id_value](auto const &p) {
				return p.func_name == id_value;
			});
			if (it != ast::intrinsic_info.end())
			{
				auto const func_decl = this->get_builtin_function(it->kind);
				if (func_decl->body.is_export())
				{
					return ast::make_constant_expression(
						src_tokens,
						ast::expression_type_kind::function_name,
						get_function_type(func_decl->body),
						ast::constant_value(&func_decl->body),
						ast::make_expr_function_name(std::move(id), func_decl)
					);
				}
			}
		}
	}

	if (this->in_unresolved_context)
	{
		return ast::make_unresolved_expression(src_tokens, ast::make_unresolved_expr_unresolved_identifier(std::move(id)));
	}
	else
	{
		this->report_error(src_tokens, bz::format("undeclared identifier '{}'", id.as_string()));
		return ast::make_error_expression(src_tokens, ast::make_expr_variable_name(std::move(id), nullptr, 0, false));
	}
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

template<size_t Base>
static std::pair<uint64_t, bool> parse_int(bz::u8string_view s)
{
	static_assert(Base == 2 || Base == 8 || Base == 10 || Base == 16);
	uint64_t result = 0;
	for (auto const c : s)
	{
		if (c == '\'')
		{
			continue;
		}

		auto const digit_value = [c]() -> uint64_t {
			switch (c)
			{
			case '0': case '1':
				if constexpr (Base == 2)
				{
					return c - '0';
				}
				[[fallthrough]];
			case '2': case '3': case '4': case '5': case '6': case '7':
				if constexpr (Base == 8)
				{
					return c - '0';
				}
				[[fallthrough]];
			case '8': case '9':
				if constexpr (Base == 10 || Base == 16)
				{
					return c - '0';
				}
				[[fallthrough]];
			case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
				if constexpr (Base == 16)
				{
					return 10 + (c - 'a');
				}
				[[fallthrough]];
			case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
				if constexpr (Base == 16)
				{
					return 10 + (c - 'A');
				}
				[[fallthrough]];
			default:
				bz_unreachable;
			}
		}();

		if (
			result > std::numeric_limits<uint64_t>::max() / Base
			|| Base * result > std::numeric_limits<uint64_t>::max() - digit_value
		)
		{
			return { result, false };
		}

		result *= Base;
		result += digit_value;
	}
	return { result, true };
}

static ast::expression get_literal_expr(
	lex::src_tokens const &src_tokens,
	uint64_t value,
	bz::u8string_view postfix,
	bool default_is_signed,
	parse_context const &context
)
{
	if (postfix == "" || postfix == "u" || postfix == "i")
	{
		auto const [default_type_info, wide_default_type_info] = [&]() -> std::pair<ast::type_info *, ast::type_info *> {
			if ((default_is_signed && postfix == "") || postfix == "i")
			{
				return {
					context.get_builtin_type_info(ast::type_info::int32_),
					context.get_builtin_type_info(ast::type_info::int64_)
				};
			}
			else
			{
				return {
					context.get_builtin_type_info(ast::type_info::uint32_),
					context.get_builtin_type_info(ast::type_info::uint64_)
				};
			}
		}();
		auto const [default_max_value, wide_default_max_value] = [&]() -> std::pair<uint64_t, uint64_t> {
			if (default_is_signed || postfix == "i")
			{
				return {
					static_cast<uint64_t>(std::numeric_limits<int32_t>::max()),
					static_cast<uint64_t>(std::numeric_limits<int64_t>::max()),
				};
			}
			else
			{
				return {
					static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()),
					static_cast<uint64_t>(std::numeric_limits<uint64_t>::max()),
				};
			}
		}();

		auto const kind = (default_is_signed && postfix == "") ? ast::literal_kind::integer :
			postfix == "i" ? ast::literal_kind::signed_integer :
			ast::literal_kind::unsigned_integer;

		if (value <= default_max_value)
		{
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::integer_literal,
				ast::make_base_type_typespec(src_tokens, default_type_info),
				ast::is_signed_integer_kind(default_type_info->kind)
					? ast::constant_value(static_cast<int64_t>(value))
					: ast::constant_value(value),
				ast::make_expr_integer_literal(kind)
			);
		}
		else if (value <= wide_default_max_value)
		{
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::integer_literal,
				ast::make_base_type_typespec(src_tokens, wide_default_type_info),
				ast::is_signed_integer_kind(wide_default_type_info->kind)
					? ast::constant_value(static_cast<int64_t>(value))
					: ast::constant_value(value),
				ast::make_expr_integer_literal(kind)
			);
		}
		else
		{
			auto const info = context.get_builtin_type_info(ast::type_info::uint64_);
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::integer_literal,
				ast::make_base_type_typespec(src_tokens, info),
				ast::constant_value(value),
				ast::make_expr_integer_literal(kind)
			);
		}
	}
	else
	{
		struct T
		{
			ast::type_info *info;
			bz::u8string_view type_name;
			uint64_t max_value;
		};

		auto const [info, type_name, max_value] = [&]() -> T {
			if (postfix == "i8")
			{
				return {
					context.get_builtin_type_info(ast::type_info::int8_),
					"int8",
					static_cast<uint64_t>(std::numeric_limits<int8_t>::max())
				};
			}
			else if (postfix == "u8")
			{
				return {
					context.get_builtin_type_info(ast::type_info::uint8_),
					"uint8",
					static_cast<uint64_t>(std::numeric_limits<uint8_t>::max())
				};
			}
			else if (postfix == "i16")
			{
				return {
					context.get_builtin_type_info(ast::type_info::int16_),
					"int16",
					static_cast<uint64_t>(std::numeric_limits<int16_t>::max())
				};
			}
			else if (postfix == "u16")
			{
				return {
					context.get_builtin_type_info(ast::type_info::uint16_),
					"uint16",
					static_cast<uint64_t>(std::numeric_limits<uint16_t>::max())
				};
			}
			else if (postfix == "i32")
			{
				return {
					context.get_builtin_type_info(ast::type_info::int32_),
					"int32",
					static_cast<uint64_t>(std::numeric_limits<int32_t>::max())
				};
			}
			else if (postfix == "u32")
			{
				return {
					context.get_builtin_type_info(ast::type_info::uint32_),
					"uint32",
					static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())
				};
			}
			else if (postfix == "i64")
			{
				return {
					context.get_builtin_type_info(ast::type_info::int64_),
					"int64",
					static_cast<uint64_t>(std::numeric_limits<int64_t>::max())
				};
			}
			else if (postfix == "u64")
			{
				return {
					context.get_builtin_type_info(ast::type_info::uint64_),
					"uint64",
					static_cast<uint64_t>(std::numeric_limits<uint64_t>::max())
				};
			}
			else if (postfix == "iz")
			{
				switch (context.global_ctx.get_data_layout().getPointerSize())
				{
				case 8:
					return {
						context.get_builtin_type_info(ast::type_info::int64_),
						"int64",
						static_cast<uint64_t>(std::numeric_limits<int64_t>::max())
					};
				case 4:
					return {
						context.get_builtin_type_info(ast::type_info::int32_),
						"int32",
						static_cast<uint64_t>(std::numeric_limits<int32_t>::max())
					};
				case 2:
					return {
						context.get_builtin_type_info(ast::type_info::int16_),
						"int16",
						static_cast<uint64_t>(std::numeric_limits<int16_t>::max())
					};
				case 1:
					return {
						context.get_builtin_type_info(ast::type_info::int8_),
						"int8",
						static_cast<uint64_t>(std::numeric_limits<int8_t>::max())
					};
				default:
					bz_unreachable;
				}
			}
			else if (postfix == "uz")
			{
				switch (context.global_ctx.get_data_layout().getPointerSize())
				{
				case 8:
					return {
						context.get_builtin_type_info(ast::type_info::uint64_),
						"uint64",
						static_cast<uint64_t>(std::numeric_limits<uint64_t>::max())
					};
				case 4:
					return {
						context.get_builtin_type_info(ast::type_info::uint32_),
						"uint32",
						static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())
					};
				case 2:
					return {
						context.get_builtin_type_info(ast::type_info::uint16_),
						"uint16",
						static_cast<uint64_t>(std::numeric_limits<uint16_t>::max())
					};
				case 1:
					return {
						context.get_builtin_type_info(ast::type_info::uint8_),
						"uint8",
						static_cast<uint64_t>(std::numeric_limits<uint8_t>::max())
					};
				default:
					bz_unreachable;
				}
			}

			return { nullptr, "", 0 };
		}();

		if (info == nullptr)
		{
			context.report_error(src_tokens, bz::format("unknown postfix '{}'", postfix));
			// fall back to a base case here
			return get_literal_expr(src_tokens, value, "", true, context);
		}

		if (value > max_value)
		{
			context.report_error(src_tokens, bz::format("literal value is too large to fit in type '{}'", type_name));
			value = 0;
		}

		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			ast::make_base_type_typespec(src_tokens, info),
			ast::is_signed_integer_kind(info->kind)
				? ast::constant_value(static_cast<int64_t>(value))
				: ast::constant_value(value),
			ast::make_expr_typed_literal(src_tokens.pivot)
		);
	}
}

ast::expression parse_context::make_literal(lex::token_pos literal) const
{
	lex::src_tokens const src_tokens = { literal, literal, literal + 1 };

	bz_assert(literal->kind != lex::token::string_literal);
	switch (literal->kind)
	{
	case lex::token::integer_literal:
	{
		auto const number_string = literal->value;
		auto [value, good] = parse_int<10>(number_string);

		if (!good)
		{
			this->report_error(
				literal,
				"literal value is too large, even for 'uint64'",
				{ this->make_note(bz::format("maximum value for 'uint64' is {}", std::numeric_limits<uint64_t>::max())) }
			);
			value = 0;
		}

		auto const postfix = literal->postfix;

		return get_literal_expr(src_tokens, value, postfix, true, *this);
	}
	case lex::token::hex_literal:
	case lex::token::oct_literal:
	case lex::token::bin_literal:
	{
		// number_string_ contains the leading 0x or 0X
		auto const number_string_ = literal->value;
		bz_assert(number_string_.starts_with('0'));
		auto const number_string = number_string_.substring(2);
		auto [value, good] =
			literal->kind == lex::token::hex_literal ? parse_int<16>(number_string) :
			literal->kind == lex::token::oct_literal ? parse_int<8>(number_string) :
			parse_int<2>(number_string);

		if (!good)
		{
			this->report_error(literal, "literal value is too large, even for 'uint64'");
			value = 0;
		}

		auto const postfix = literal->postfix;

		return get_literal_expr(src_tokens, value, postfix, false, *this);
	}
	case lex::token::floating_point_literal:
	{
		bz::u8string number_string = literal->value;
		number_string.erase('\'');

		auto const postfix = literal->postfix;
		if (postfix == "f32")
		{
			auto const num = bz::parse_float(number_string);

			if (!num.has_value())
			{
				bz::vector<source_highlight> notes;
				if (do_verbose)
				{
					notes.push_back(make_note("at most 9 significant digits are allowed for 'float32'"));
				}
				this->report_error(literal, "unable to parse 'float32' literal value, it has too many digits", std::move(notes));
			}
			else if (!std::isfinite(num.get()))
			{
				this->report_warning(
					warning_kind::float_overflow, literal,
					bz::format("'float32' literal value was parsed as {}", num.get())
				);
			}

			auto const value = num.has_value() ? num.get() : 0.0f;
			auto const info = this->get_builtin_type_info(ast::type_info::float32_);
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::rvalue,
				ast::make_base_type_typespec(src_tokens, info),
				ast::constant_value(value),
				ast::make_expr_typed_literal(literal)
			);
		}
		else
		{
			if (postfix != "" && postfix != "f64")
			{
				this->report_error(literal, bz::format("unknown postfix '{}'", postfix));
			}
			auto const num = bz::parse_double(number_string);

			if (!num.has_value())
			{
				bz::vector<source_highlight> notes;
				if (do_verbose)
				{
					notes.push_back(make_note("at most 17 significant digits are allowed for 'float64'"));
				}
				this->report_error(literal, "unable to parse 'float64' literal value, it has too many digits", std::move(notes));
			}
			else if (!std::isfinite(num.get()))
			{
				this->report_warning(
					warning_kind::float_overflow, literal,
					bz::format("'float64' literal value was parsed as {}", num.get())
				);
			}

			auto const value = num.has_value() ? num.get() : 0.0;
			auto const info = this->get_builtin_type_info(ast::type_info::float64_);
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::rvalue,
				ast::make_base_type_typespec(src_tokens, info),
				ast::constant_value(value),
				ast::make_expr_typed_literal(literal)
			);
		}
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
			bz::vector<source_highlight> notes = {};

			if (bz::is_in_unicode_surrogate_range(value))
			{
				notes.push_back(this->make_note(bz::format("U+{04x} is in a unicode surrogate range", value)));
			}
			else
			{
				notes.push_back(this->make_note(bz::format("largest unicode code point value is U+{04x}", bz::max_unicode_value)));
			}

			this->report_error(
				literal,
				bz::format(
					"'\\U{:08x}' is not a valid character",
					value
				),
				std::move(notes)
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
			ast::make_base_type_typespec(src_tokens, this->get_builtin_type_info(ast::type_info::char_)),
			ast::constant_value(value),
			ast::make_expr_typed_literal(literal)
		);
	}
	case lex::token::kw_true:
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			ast::make_base_type_typespec(src_tokens, this->get_builtin_type_info(ast::type_info::bool_)),
			ast::constant_value(true),
			ast::make_expr_typed_literal(literal)
		);
	case lex::token::kw_false:
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			ast::make_base_type_typespec(src_tokens, this->get_builtin_type_info(ast::type_info::bool_)),
			ast::constant_value(false),
			ast::make_expr_typed_literal(literal)
		);
	case lex::token::kw_null:
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::null_literal,
			ast::make_base_type_typespec(src_tokens, this->get_builtin_type_info(ast::type_info::null_t_)),
			ast::constant_value::get_null(),
			ast::make_expr_null_literal()
		);
	case lex::token::placeholder_literal:
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::placeholder_literal,
			ast::make_void_typespec(src_tokens.pivot),
			ast::constant_value(),
			ast::make_expr_placeholder_literal()
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
		if (it->kind == lex::token::raw_string_literal)
		{
			result += it->value;
		}
		else
		{
			result += get_string_value(it);
		}
	}

	auto const postfix = (end - 1)->postfix;
	if (postfix != "")
	{
		this->report_error({ begin, begin, end }, bz::format("unknown postfix '{}'", postfix));
	}

	return ast::make_constant_expression(
		{ begin, begin, end },
		ast::expression_type_kind::rvalue,
		ast::make_base_type_typespec({ begin, begin, end }, this->get_builtin_type_info(ast::type_info::str_)),
		ast::constant_value(result),
		ast::make_expr_typed_literal(lex::token_range{ begin, end })
	);
}

ast::expression parse_context::make_tuple(lex::src_tokens const &src_tokens, ast::arena_vector<ast::expression> elems) const
{
	if (
		this->in_unresolved_context
		|| this->current_unresolved_locals.not_empty()
		|| elems.is_any([](auto &expr) { return expr.is_unresolved(); })
	)
	{
		return ast::make_unresolved_expression(src_tokens, ast::make_unresolved_expr_tuple(std::move(elems)));
	}

	elems = expand_params(std::move(elems));

	if (elems.is_any([](auto &expr) { return expr.is_error(); }))
	{
		return ast::make_error_expression(src_tokens, ast::make_expr_tuple(std::move(elems)));
	}
	else if (elems.is_all([](auto &expr) { return expr.is_typename(); }))
	{
		ast::arena_vector<ast::typespec> types = {};
		auto const size = elems.size();
		types.reserve(size);
		for (auto const i : bz::iota(0, size))
		{
			types.emplace_back(elems[i].get_typename());
			if (types.back().is<ast::ts_variadic>() && i != size - 1)
			{
				this->report_error(
					elems[i].src_tokens,
					bz::format("variadic type '{}' in tuple must be the last element", types.back())
				);
				types.back().remove_layer();
			}
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
			ast::make_expr_tuple(std::move(elems)),
			ast::destruct_operation()
		);
	}
}

ast::expression parse_context::make_unreachable(lex::token_pos t)
{
	auto const panic_fn_body = &this->get_builtin_function(ast::function_body::builtin_panic)->body;
	auto message = bz::format("unreachable hit at {}:{}", this->global_ctx.get_file_name(t->src_pos.file_id), t->src_pos.line);
	auto const src_tokens = lex::src_tokens::from_single_token(t);

	ast::arena_vector<ast::expression> args = {};
	args.push_back(ast::make_constant_expression(
		src_tokens,
		ast::expression_type_kind::rvalue,
		ast::make_base_type_typespec(src_tokens, this->get_builtin_type_info(ast::type_info::str_)),
		ast::constant_value(std::move(message)),
		ast::make_expr_typed_literal(lex::token_range{ t, t + 1 })
	));
	auto panic_fn_call_expr = ast::make_dynamic_expression(
		src_tokens,
		ast::expression_type_kind::none,
		ast::make_void_typespec(nullptr),
		ast::make_expr_function_call(src_tokens, std::move(args), panic_fn_body, ast::resolve_order::regular),
		ast::destruct_operation()
	);
	return ast::make_dynamic_expression(
		src_tokens,
		ast::expression_type_kind::noreturn,
		ast::make_void_typespec(nullptr),
		ast::make_expr_unreachable(std::move(panic_fn_call_expr)),
		ast::destruct_operation()
	);
}

static bool is_builtin_type(ast::typespec_view ts)
{
	ts = ast::remove_const_or_consteval(ts);
	return ts.is<ast::ts_pointer>()
		|| ts.is<ast::ts_function>()
		|| ts.is<ast::ts_tuple>()
		|| ts.is<ast::ts_array>()
		|| ts.is<ast::ts_array_slice>()
		|| ts.is<ast::ts_optional>()
		|| (ts.is<ast::ts_base_type>() && ts.get<ast::ts_base_type>().info->kind != ast::type_info::aggregate);
}


struct possible_func_t
{
	resolve::match_level_t match_level;
	ast::statement_view stmt;
	ast::function_body *func_body;
};

static std::pair<ast::statement_view, ast::function_body *> find_best_match(
	lex::src_tokens const &src_tokens,
	bz::array_view<possible_func_t const> possible_funcs,
	bz::array_view<ast::expression const> args,
	parse_context &context
)
{
	bz_assert(!possible_funcs.empty());
	auto const max_match_it = std::max_element(possible_funcs.begin(), possible_funcs.end(), [](auto const &lhs, auto const &rhs) {
		return lhs.match_level < rhs.match_level;
	});
	bz_assert(max_match_it != possible_funcs.end());
	if (max_match_it->match_level.not_null())
	{
		// search for possible ambiguity
		auto filtered_funcs = possible_funcs
			.filter([&](auto const &func) {
				return &*max_match_it == &func || resolve::match_level_compare(max_match_it->match_level, func.match_level) <= 0;
			});
		if (filtered_funcs.count() == 1)
		{
			return { max_match_it->stmt, max_match_it->func_body };
		}
		else
		{
			bz::vector<source_highlight> notes;
			notes.reserve(possible_funcs.size() + 1);
			notes.emplace_back(get_function_parameter_types_note(src_tokens, args));
			for (auto &func : filtered_funcs)
			{
				notes.emplace_back(context.make_note(
					func.func_body->src_tokens, func.func_body->get_candidate_message()
				));
				if (func.stmt.is<ast::decl_function_alias>())
				{
					auto &alias = func.stmt.get<ast::decl_function_alias>();
					notes.emplace_back(context.make_note(
						alias.src_tokens, bz::format("via alias 'function {}'", alias.id.format_as_unqualified())
					));
				}
			}
			context.report_error(src_tokens, "function call is ambiguous", std::move(notes));
			return { {}, nullptr };
		}
	}

	// report all failed function error
	bz::vector<source_highlight> notes;
	notes.reserve(possible_funcs.size() + 1);
	notes.emplace_back(get_function_parameter_types_note(src_tokens, args));
	bool builtin_operator_found = false;
	for (auto &func : possible_funcs)
	{
		if (!do_verbose && func.func_body->is_builtin_operator())
		{
			if (builtin_operator_found)
			{
				continue;
			}
			builtin_operator_found = true;
			notes.emplace_back(context.make_note(
				bz::format(
					"candidate: the builtin 'operator {}'",
					token_info[func.func_body->function_name_or_operator_kind.get<uint32_t>()].token_value
				)
			));
		}
		else
		{
			notes.emplace_back(context.make_note(
				func.func_body->src_tokens, func.func_body->get_candidate_message()
			));
		}
		if (func.stmt.is<ast::decl_function_alias>())
		{
			auto &alias = func.stmt.get<ast::decl_function_alias>();
			notes.emplace_back(context.make_note(
				alias.src_tokens, bz::format("via alias 'function {}'", alias.id.format_as_unqualified())
			));
		}
	}
	context.report_error(src_tokens, "couldn't match the function call to any of the candidates", std::move(notes));
	return { {}, nullptr };
}

static void expand_variadic_params(ast::arena_vector<ast::decl_variable> &params, size_t params_count)
{
	if (params.empty() || !params.back().get_type().is<ast::ts_variadic>())
	{
		return;
	}
	if (params_count < params.size())
	{
		params.pop_back();
		return;
	}
	bz_assert(params_count >= params.size());
	bz_assert(!params.empty());
	bz_assert(params.back().get_type().is<ast::ts_variadic>());
	auto const diff = params_count - params.size();
	params.reserve(params_count);
	auto &params_back = params.back();
	params_back.get_type().remove_layer();
	for (size_t i = 0; i < diff; ++i)
	{
		params.push_back(params_back);
	}
}

static ast::expression make_expr_function_call_from_body(
	lex::src_tokens const &src_tokens,
	ast::function_body *body,
	ast::arena_vector<ast::expression> args,
	parse_context &context,
	ast::resolve_order resolve_order
)
{
	if (body->is_generic())
	{
		auto required_from = get_generic_requirements(src_tokens, context);
		bz_assert(required_from.front().src_tokens.pivot != nullptr);
		auto generic_params = body->get_params_copy_for_generic_specialization();
		expand_variadic_params(generic_params, args.size());
		for (auto const [arg, generic_param] : bz::zip(args, generic_params))
		{
			resolve::match_expression_to_variable(arg, generic_param, context);
			bz_assert(!generic_param.get_type().is<ast::ts_variadic>());
			if (ast::is_generic_parameter(generic_param))
			{
				generic_param.init_expr = arg;
			}
		}
		auto [result_body, message] = body->add_specialized_body(std::move(generic_params), std::move(required_from));
		if (result_body == nullptr)
		{
			if (message != "")
			{
				context.report_error(src_tokens, std::move(message));
			}
			return ast::make_error_expression(src_tokens, ast::make_expr_function_call(src_tokens, std::move(args), body, resolve_order));
		}
		body = result_body;
		context.add_to_resolve_queue(src_tokens, *body);
		bz_assert(!body->is_generic());
		if (body != context.current_function && !context.generic_functions.contains(body))
		{
			context.generic_functions.push_back(body);
		}
	}
	else
	{
		// expand_function_body_params(body, params.size()); // this is not needed here as variadic functions are always generic
		context.add_to_resolve_queue(src_tokens, *body);
		for (auto const [arg, func_body_param] : bz::zip(args, body->params))
		{
			resolve::match_expression_to_variable(arg, func_body_param, context);
		}
	}
	resolve::resolve_function_symbol({}, *body, context);
	context.pop_resolve_queue();
	if (body->state == ast::resolve_state::error)
	{
		return ast::make_error_expression(src_tokens, ast::make_expr_function_call(src_tokens, std::move(args), body, resolve_order));
	}
	else if (body->is_deleted())
	{
		context.report_error(
			src_tokens,
			bz::format("calling explicitly deleted function '{}'", body->get_signature()),
			{ context.make_note(body->src_tokens, "function was explicitly deleted here") }
		);
		return ast::make_error_expression(src_tokens, ast::make_expr_function_call(src_tokens, std::move(args), body, resolve_order));
	}

	if (body->is_intrinsic() && body->intrinsic_kind == ast::function_body::builtin_destruct_value)
	{
		bz_assert(args.size() == 1);
		auto const [expr_type, expr_type_kind] = args[0].get_expr_type_and_kind();
		bz_assert(expr_type_kind == ast::expression_type_kind::lvalue_reference);
		auto destruct_call = make_destruct_expression(
			expr_type,
			ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::lvalue_reference,
				expr_type,
				ast::make_expr_bitcode_value_reference(),
				ast::destruct_operation()
			),
			context
		);
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::none, ast::make_void_typespec(nullptr),
			ast::make_expr_destruct_value(std::move(args[0]), std::move(destruct_call)),
			ast::destruct_operation()
		);
	}
	else if (body->is_intrinsic() && body->intrinsic_kind == ast::function_body::builtin_swap)
	{
		bz_assert(args.size() == 2);
		ast::typespec const expr_type = args[0].get_expr_type();
		return make_swap_expression(src_tokens, expr_type, std::move(args[0]), std::move(args[1]), context);
	}
	else if (body->is_intrinsic() && body->intrinsic_kind == ast::function_body::builtin_optional_get_value)
	{
		bz_assert(args.size() == 1);
		return make_optional_extract_value_expression(src_tokens, std::move(args[0]), context);
	}
	else if (body->is_intrinsic() && body->intrinsic_kind == ast::function_body::create_initialized_array)
	{
		bz_assert(args.size() == 2);
		bz_assert(args[0].is_typename());
		auto const &type = args[0].get_typename();
		context.add_self_destruction(args[1]);
		return make_array_value_init_expression(src_tokens, type, std::move(args[1]), context);
	}
	else if (body->is_intrinsic() && body->intrinsic_kind == ast::function_body::bit_cast)
	{
		bz_assert(args.size() == 2);
		bz_assert(args[0].is_typename());
		auto &type = args[0].get_typename();
		bz_assert(body->return_type == type);
		context.add_self_destruction(args[1]);
		return context.make_bit_cast_expression(src_tokens, std::move(args[1]), std::move(type));
	}
	else if (body->is_default_default_constructor() || (body->is_default_constructor() && body->is_defaulted()))
	{
		bz_assert(args.size() == 0);
		auto const type = ast::make_base_type_typespec(src_tokens, body->get_constructor_of());
		return context.make_default_construction(src_tokens, type);
	}
	else if (body->is_default_copy_constructor() || (body->is_copy_constructor() && body->is_defaulted()))
	{
		bz_assert(args.size() == 1);
		args[0].src_tokens = src_tokens;
		return context.make_copy_construction(std::move(args[0]));
	}
	else if (body->is_default_move_constructor() || (body->is_move_constructor() && body->is_defaulted()))
	{
		bz_assert(args.size() == 1);
		args[0].src_tokens = src_tokens;
		return context.make_move_construction(std::move(args[0]));
	}
	else if (
		body->is_builtin_assign()
		|| body->is_default_op_assign()
		|| body->is_default_op_move_assign()
		|| (body->is_copy_assign_op() && body->is_defaulted())
		|| (body->is_move_assign_op() && body->is_defaulted())
	)
	{
		bz_assert(args.size() == 2);
		return context.make_default_assignment(src_tokens, std::move(args[0]), std::move(args[1]));
	}
	else if (
		body->has_builtin_implementation()
		&& !(body->is_intrinsic() && body->intrinsic_kind == ast::function_body::builtin_inplace_construct)
	)
	{
		bz_assert(args.size() == body->params.size());
		for (auto const i : bz::iota(0, args.size()))
		{
			auto const &param_type = body->params[i].get_type();
			if (!param_type.is<ast::ts_lvalue_reference>() && !param_type.is<ast::ts_move_reference>())
			{
				context.add_self_destruction(args[i]);
			}
		}
	}

	auto &ret_t = body->return_type;
	if (ret_t.is_typename())
	{
		auto result = ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::type_name, ast::make_typename_typespec(nullptr),
			ast::make_expr_function_call(src_tokens, std::move(args), body, resolve_order),
			ast::destruct_operation()
		);
		resolve::consteval_try(result, context);
		return result;
	}

	auto return_type_kind = ast::expression_type_kind::rvalue;
	auto return_type = ast::remove_const_or_consteval(ret_t);
	if (ret_t.is<ast::ts_lvalue_reference>())
	{
		return_type_kind = ast::expression_type_kind::lvalue_reference;
		return_type = ret_t.get<ast::ts_lvalue_reference>();
	}
	else if (ret_t.is<ast::ts_void>())
	{
		return_type_kind = ast::expression_type_kind::none;
	}
	return ast::make_dynamic_expression(
		src_tokens,
		return_type_kind, return_type,
		ast::make_expr_function_call(src_tokens, std::move(args), body, resolve_order),
		ast::destruct_operation()
	);
}

static ast::expression make_expr_function_call_from_body(
	lex::src_tokens const &src_tokens,
	ast::function_body *body,
	ast::arena_vector<ast::expression> args,
	ast::constant_value value,
	parse_context &context,
	ast::resolve_order resolve_order = ast::resolve_order::regular
)
{
	if (body->is_generic())
	{
		auto required_from = get_generic_requirements(src_tokens, context);
		bz_assert(required_from.front().src_tokens.pivot != nullptr);
		auto generic_params = body->get_params_copy_for_generic_specialization();
		expand_variadic_params(generic_params, args.size());
		context.add_to_resolve_queue(src_tokens, *body);
		for (auto const [arg, generic_param] : bz::zip(args, generic_params))
		{
			resolve::match_expression_to_variable(arg, generic_param, context);
			bz_assert(!generic_param.get_type().is<ast::ts_variadic>());
			if (ast::is_generic_parameter(generic_param))
			{
				generic_param.init_expr = arg;
			}
		}
		context.pop_resolve_queue();
		auto [result_body, message] = body->add_specialized_body(std::move(generic_params), std::move(required_from));
		if (result_body == nullptr)
		{
			context.report_error(src_tokens, std::move(message));
			return ast::make_error_expression(src_tokens, ast::make_expr_function_call(src_tokens, std::move(args), body, resolve_order));
		}
		body = result_body;
		context.add_to_resolve_queue(src_tokens, *body);
		bz_assert(!body->is_generic());
		if (body != context.current_function && !context.generic_functions.contains(body))
		{
			context.generic_functions.push_back(body);
		}
	}
	else
	{
		context.add_to_resolve_queue(src_tokens, *body);
		for (auto const [arg, func_body_param] : bz::zip(args, body->params))
		{
			resolve::match_expression_to_variable(arg, func_body_param, context);
		}
	}
	resolve::resolve_function_symbol({}, *body, context);
	context.pop_resolve_queue();
	if (body->state == ast::resolve_state::error)
	{
		return ast::make_error_expression(src_tokens, ast::make_expr_function_call(src_tokens, std::move(args), body, resolve_order));
	}

	auto &ret_t = body->return_type;
	auto return_type_kind = ast::expression_type_kind::rvalue;
	auto return_type = ast::remove_const_or_consteval(ret_t);
	if (ret_t.is<ast::ts_lvalue_reference>())
	{
		return_type_kind = ast::expression_type_kind::lvalue_reference;
		return_type = ret_t.get<ast::ts_lvalue_reference>();
	}
	else if (ret_t.is<ast::ts_void>())
	{
		return_type_kind = ast::expression_type_kind::none;
	}
	return ast::make_constant_expression(
		src_tokens,
		return_type_kind, return_type,
		std::move(value),
		ast::make_expr_function_call(src_tokens, std::move(args), body, resolve_order)
	);
}

static void get_possible_funcs_for_operator_helper(
	bz::vector<possible_func_t> &result,
	lex::src_tokens const &src_tokens,
	uint32_t op,
	ast::expression &expr,
	ast::global_scope_t &scope,
	parse_context &context
)
{
	auto const id_scope = scope.id_scope;
	for (
		auto const &op_set :
		scope.operator_sets.filter([op, id_scope](auto const &op_set) {
			return op_set.op == op
				&& op_set.id_scope.size() <= id_scope.size()
				&& op_set.id_scope == id_scope.slice(0, op_set.id_scope.size());
		}
	))
	{
		for (auto const op : op_set.op_decls)
		{
			if (!result.transform([](auto const &possible_func) { return possible_func.func_body; }).contains(&op->body))
			{
				auto match_level = resolve::get_function_call_match_level(op, op->body, expr, context, src_tokens);
				result.push_back({ std::move(match_level), op, &op->body });
			}
		}
	}
}

static void get_possible_funcs_for_operator_helper(
	bz::vector<possible_func_t> &result,
	lex::src_tokens const &src_tokens,
	uint32_t op,
	ast::expression &expr,
	ast::enclosing_scope_t scope,
	parse_context &context
)
{
	while (scope.scope != nullptr)
	{
		if (scope.scope->is_local())
		{
			// we do nothing, because operators can't be local
			static_assert(ast::local_symbol_t::variant_count == 7);
			scope = scope.scope->get_local().parent;
		}
		else
		{
			bz_assert(scope.scope->is_global());
			get_possible_funcs_for_operator_helper(result, src_tokens, op, expr, scope.scope->get_global(), context);
			scope = scope.scope->get_global().parent;
		}
	}
}

static bz::vector<possible_func_t> get_possible_funcs_for_operator(
	lex::src_tokens const &src_tokens,
	uint32_t op,
	ast::expression &expr,
	parse_context &context
)
{
	bz::vector<possible_func_t> possible_funcs = {};

	get_possible_funcs_for_operator_helper(possible_funcs, src_tokens, op, expr, context.get_current_enclosing_scope(), context);
	auto const expr_type = ast::remove_const_or_consteval(expr.get_expr_type());
	if (expr_type.is<ast::ts_base_type>())
	{
		auto const info = expr_type.get<ast::ts_base_type>().info;
		context.resolve_type(src_tokens, info);
		get_possible_funcs_for_operator_helper(possible_funcs, src_tokens, op, expr, info->get_scope(), context);
	}
	else if (expr_type.is<ast::ts_enum>())
	{
		auto const decl = expr_type.get<ast::ts_enum>().decl;
		context.resolve_type(src_tokens, decl);
		get_possible_funcs_for_operator_helper(possible_funcs, src_tokens, op, expr, decl->get_scope(), context);
	}

	return possible_funcs;
}

ast::expression parse_context::make_unary_operator_expression(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression expr
)
{
	if (expr.is_error())
	{
		bz_assert(this->has_errors());
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else if (this->in_unresolved_context || expr.is_unresolved() || (op_kind == lex::token::dot_dot_dot && !expr.is_typename()))
	{
		return ast::make_unresolved_expression(src_tokens, ast::make_unresolved_expr_unary_op(op_kind, std::move(expr)));
	}

	if (is_unary_type_op(op_kind) && expr.is_typename())
	{
		auto result = make_builtin_type_operation(src_tokens, op_kind, std::move(expr), *this);
		return result;
	}
	else if (is_unary_type_op(op_kind) && !is_unary_builtin_operator(op_kind))
	{
		bz_assert(!is_overloadable_operator(op_kind));
		this->report_error(expr, bz::format("expected a type after '{}'", token_info[op_kind].token_value));
	}

	// if it's a non-overloadable operator user-defined operators shouldn't be looked at
	if (!is_unary_overloadable_operator(op_kind))
	{
		return make_builtin_operation(src_tokens, op_kind, std::move(expr), *this);
	}

	auto const possible_funcs = get_possible_funcs_for_operator(src_tokens, op_kind, expr, *this);
	if (possible_funcs.empty())
	{
		this->report_error(
			src_tokens,
			bz::format(
				"no candidate found for unary 'operator {}' with type '{}'",
				token_info[op_kind].token_value, expr.get_expr_type()
			)
		);
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else
	{
		auto const [_, best_body] = find_best_match(src_tokens, possible_funcs, expr, *this);
		if (best_body == nullptr)
		{
			return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
		}
		else
		{
			if (best_body->is_builtin_operator() && expr.is_constant() && expr.is_integer_literal())
			{
				auto result = make_unary_literal_operation(
					src_tokens,
					best_body->function_name_or_operator_kind.get<uint32_t>(),
					expr,
					*this
				);

				if (result.not_null())
				{
					return result;
				}
			}

			ast::arena_vector<ast::expression> args;
			args.emplace_back(std::move(expr));
			return make_expr_function_call_from_body(src_tokens, best_body, std::move(args), *this);
		}
	}
}

static void get_possible_funcs_for_operator_helper(
	bz::vector<possible_func_t> &result,
	lex::src_tokens const &src_tokens,
	uint32_t op,
	ast::expression &lhs,
	ast::expression &rhs,
	ast::global_scope_t &scope,
	parse_context &context
)
{
	auto const id_scope = scope.id_scope;
	for (
		auto const &op_set :
		scope.operator_sets.filter([op, id_scope](auto const &op_set) {
			return op_set.op == op
				&& op_set.id_scope.size() <= id_scope.size()
				&& op_set.id_scope == id_scope.slice(0, op_set.id_scope.size());
		}
	))
	{
		for (auto const op_decl : op_set.op_decls)
		{
			if (!result.transform([](auto const &possible_func) { return possible_func.func_body; }).contains(&op_decl->body))
			{
				auto match_level = resolve::get_function_call_match_level(op_decl, op_decl->body, lhs, rhs, context, src_tokens);
				result.push_back({ std::move(match_level), op_decl, &op_decl->body });
			}
		}
	}
}

static void get_possible_funcs_for_operator_helper(
	bz::vector<possible_func_t> &result,
	lex::src_tokens const &src_tokens,
	uint32_t op,
	ast::expression &lhs,
	ast::expression &rhs,
	ast::enclosing_scope_t scope,
	parse_context &context
)
{
	while (scope.scope != nullptr)
	{
		if (scope.scope->is_local())
		{
			// we do nothing, because operators can't be local
			static_assert(ast::local_symbol_t::variant_count == 7);
			scope = scope.scope->get_local().parent;
		}
		else
		{
			bz_assert(scope.scope->is_global());
			get_possible_funcs_for_operator_helper(result, src_tokens, op, lhs, rhs, scope.scope->get_global(), context);
			scope = scope.scope->get_global().parent;
		}
	}
}

static bz::vector<possible_func_t> get_possible_funcs_for_operator(
	lex::src_tokens const &src_tokens,
	uint32_t op,
	ast::expression &lhs,
	ast::expression &rhs,
	parse_context &context
)
{
	bz::vector<possible_func_t> possible_funcs = {};

	get_possible_funcs_for_operator_helper(possible_funcs, src_tokens, op, lhs, rhs, context.get_current_enclosing_scope(), context);
	auto const lhs_type = ast::remove_const_or_consteval(lhs.get_expr_type());
	if (lhs_type.is<ast::ts_base_type>())
	{
		auto const info = lhs_type.get<ast::ts_base_type>().info;
		context.resolve_type(src_tokens, info);
		get_possible_funcs_for_operator_helper(possible_funcs, src_tokens, op, lhs, rhs, info->get_scope(), context);
	}
	else if (lhs_type.is<ast::ts_enum>())
	{
		auto const decl = lhs_type.get<ast::ts_enum>().decl;
		context.resolve_type(src_tokens, decl);
		get_possible_funcs_for_operator_helper(possible_funcs, src_tokens, op, lhs, rhs, decl->get_scope(), context);
	}
	auto const rhs_type = ast::remove_const_or_consteval(rhs.get_expr_type());
	if (rhs_type.is<ast::ts_base_type>())
	{
		auto const info = rhs_type.get<ast::ts_base_type>().info;
		context.resolve_type(src_tokens, info);
		get_possible_funcs_for_operator_helper(possible_funcs, src_tokens, op, lhs, rhs, info->get_scope(), context);
	}
	else if (rhs_type.is<ast::ts_enum>())
	{
		auto const decl = rhs_type.get<ast::ts_enum>().decl;
		context.resolve_type(src_tokens, decl);
		get_possible_funcs_for_operator_helper(possible_funcs, src_tokens, op, lhs, rhs, decl->get_scope(), context);
	}

	return possible_funcs;
}

ast::expression parse_context::make_binary_operator_expression(
	lex::src_tokens const &src_tokens,
	uint32_t op_kind,
	ast::expression lhs,
	ast::expression rhs
)
{
	if (lhs.is_error() || rhs.is_error())
	{
		bz_assert(this->has_errors());
		return ast::make_error_expression(src_tokens, ast::make_expr_binary_op(op_kind, std::move(lhs), std::move(rhs)));
	}
	else if (this->in_unresolved_context || lhs.is_unresolved() || rhs.is_unresolved())
	{
		return ast::make_unresolved_expression(
			src_tokens,
			ast::make_unresolved_expr_binary_op(op_kind, std::move(lhs), std::move(rhs))
		);
	}

	if (op_kind == lex::token::kw_as)
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
			return ast::make_error_expression(src_tokens, ast::make_expr_binary_op(op_kind, std::move(lhs), std::move(rhs)));
		}

		return this->make_cast_expression(src_tokens, std::move(lhs), std::move(rhs.get_typename()));
	}

	if (is_binary_type_op(op_kind) && lhs.is_typename() && rhs.is_typename())
	{
		auto result = make_builtin_type_operation(src_tokens, op_kind, std::move(lhs), std::move(rhs), *this);
		result.src_tokens = src_tokens;
		return result;
	}
	else if (is_binary_type_op(op_kind) && !is_binary_builtin_operator(op_kind))
	{
		// there's no such operator ('as' is handled earlier)
		bz_unreachable;
	}

	// if it's a non-overloadable operator user-defined operators shouldn't be looked at
	if (!is_binary_overloadable_operator(op_kind))
	{
		return make_builtin_operation(src_tokens, op_kind, std::move(lhs), std::move(rhs), *this);
	}

	auto const possible_funcs = get_possible_funcs_for_operator(src_tokens, op_kind, lhs, rhs, *this);

	if (possible_funcs.empty())
	{
		this->report_error(
			src_tokens,
			bz::format(
				"no candidate found for binary 'operator {}' with types '{}' and '{}'",
				token_info[op_kind].token_value, lhs.get_expr_type(), rhs.get_expr_type()
			)
		);
		return ast::make_error_expression(src_tokens, ast::make_expr_binary_op(op_kind, std::move(lhs), std::move(rhs)));
	}
	else
	{
		ast::arena_vector<ast::expression> args;
		args.reserve(2);
		args.emplace_back(std::move(lhs));
		args.emplace_back(std::move(rhs));
		auto const [_, best_body] = find_best_match(src_tokens, possible_funcs, args, *this);
		if (best_body == nullptr)
		{
			return ast::make_error_expression(src_tokens, ast::make_expr_binary_op(op_kind, std::move(args[0]), std::move(args[1])));
		}
		else
		{
			if (
				best_body->is_builtin_operator()
				&& args[0].is_constant() && args[1].is_constant()
				&& args[0].is_integer_literal() && args[1].is_integer_literal()
			)
			{
				auto result = make_binary_literal_operation(
					src_tokens,
					best_body->function_name_or_operator_kind.get<uint32_t>(),
					args[0], args[1],
					*this
				);

				if (result.not_null())
				{
					return result;
				}
			}

			auto const resolve_order = get_binary_precedence(op_kind).is_left_associative
				? ast::resolve_order::regular
				: ast::resolve_order::reversed;
			return make_expr_function_call_from_body(src_tokens, best_body, std::move(args), *this, resolve_order);
		}
	}
}

static bz::vector<possible_func_t> get_possible_funcs_for_unqualified_id(
	ast::function_set_t const &unqualified_function_set,
	lex::src_tokens const &src_tokens,
	bz::array_view<ast::expression> params,
	parse_context &context
)
{
	bz::vector<possible_func_t> possible_funcs = {};
	auto const size = unqualified_function_set.stmts
		.transform([](ast::statement_view const stmt) -> size_t {
			if (stmt.is<ast::decl_function>())
			{
				return 1;
			}
			else
			{
				bz_assert(stmt.is<ast::decl_function_alias>());
				return stmt.get<ast::decl_function_alias>().aliased_decls.size();
			}
		}).sum();
	possible_funcs.reserve(size);
	for (auto const &stmt : unqualified_function_set.stmts)
	{
		if (stmt.is<ast::decl_function>())
		{
			auto &body = stmt.get<ast::decl_function>().body;
			auto match_level = resolve::get_function_call_match_level(stmt, body, params, context, src_tokens);
			possible_funcs.push_back({ std::move(match_level), stmt, &body });
		}
		else
		{
			bz_assert(stmt.is<ast::decl_function_alias>());
			for (auto const decl : stmt.get<ast::decl_function_alias>().aliased_decls)
			{
				auto match_level = resolve::get_function_call_match_level(decl, decl->body, params, context, src_tokens);
				possible_funcs.push_back({ std::move(match_level), stmt, &decl->body });
			}
		}
	}
	return possible_funcs;
}

static bz::vector<possible_func_t> get_possible_funcs_for_alias(
	ast::decl_function_alias *alias_decl,
	lex::src_tokens const &src_tokens,
	bz::array_view<ast::expression> params,
	parse_context &context
)
{
	return alias_decl->aliased_decls
		.transform([&](auto const decl) -> possible_func_t {
			auto match_level = resolve::get_function_call_match_level(decl, decl->body, params, context, src_tokens);
			return { std::move(match_level), ast::statement_view(alias_decl), &decl->body };
		})
		.collect();
}

static bz::vector<possible_func_t> get_possible_funcs_for_qualified_id(
	ast::function_set_t const &qualified_function_set,
	lex::src_tokens const &src_tokens,
	bz::array_view<ast::expression> params,
	parse_context &context
)
{
	bz::vector<possible_func_t> possible_funcs = {};

	auto const size = qualified_function_set.stmts
		.transform([](ast::statement_view const stmt) -> size_t {
			if (stmt.is<ast::decl_function>())
			{
				return 1;
			}
			else
			{
				bz_assert(stmt.is<ast::decl_function_alias>());
				return stmt.get<ast::decl_function_alias>().aliased_decls.size();
			}
		}).sum();
	possible_funcs.reserve(size);
	for (auto const &stmt : qualified_function_set.stmts)
	{
		if (stmt.is<ast::decl_function>())
		{
			auto &body = stmt.get<ast::decl_function>().body;
			auto match_level = resolve::get_function_call_match_level(stmt, body, params, context, src_tokens);
			possible_funcs.push_back({ std::move(match_level), stmt, &body });
		}
		else
		{
			bz_assert(stmt.is<ast::decl_function_alias>());
			for (auto const decl : stmt.get<ast::decl_function_alias>().aliased_decls)
			{
				auto match_level = resolve::get_function_call_match_level(decl, decl->body, params, context, src_tokens);
				possible_funcs.push_back({ std::move(match_level), stmt, &decl->body });
			}
		}
	}

	return possible_funcs;
}

static ast::expression make_base_type_constructor_call_expression(
	lex::src_tokens const &src_tokens,
	ast::typespec_view called_type,
	ast::arena_vector<ast::expression> args,
	parse_context &context
)
{
	auto const info = called_type.get<ast::ts_base_type>().info;
	context.resolve_type_members(src_tokens, info);

	if (info->is_generic())
	{
		context.report_error(
			src_tokens,
			bz::format("cannot call constructor of generic type '{}'", called_type)
		);
		return ast::make_error_expression(
			src_tokens,
			ast::make_expr_function_call(src_tokens, std::move(args), nullptr, ast::resolve_order::regular)
		);
	}

	auto const possible_funcs = info->constructors
		.transform([&](auto const ctor_decl) {
			return possible_func_t{
				resolve::get_function_call_match_level(ctor_decl, ctor_decl->body, args, context, src_tokens),
				ctor_decl, &ctor_decl->body
			};
		})
		.collect<ast::arena_vector>();

	if (
		possible_funcs.is_all([](auto const &possible_func) {
			return possible_func.match_level.is_null();
		})
		&& args.size() == 1
	)
	{
		// function style casting
		return context.make_cast_expression(src_tokens, std::move(args[0]), called_type);
	}
	else if (possible_funcs.not_empty())
	{
		auto const [_, best_body] = find_best_match(src_tokens, possible_funcs, args, context);
		if (best_body == nullptr)
		{
			return ast::make_error_expression(
				src_tokens,
				ast::make_expr_function_call(src_tokens, std::move(args), nullptr, ast::resolve_order::regular)
			);
		}
		else
		{
			return make_expr_function_call_from_body(src_tokens, best_body, std::move(args), context);
		}
	}
	else
	{
		context.report_error(
			src_tokens,
			bz::format("no constructors found for type '{}'", called_type)
		);
		return ast::make_error_expression(
			src_tokens,
			ast::make_expr_function_call(src_tokens, std::move(args), nullptr, ast::resolve_order::regular)
		);
	}
}

static ast::expression make_constructor_call_expression(
	lex::src_tokens const &src_tokens,
	ast::expression called,
	ast::arena_vector<ast::expression> args,
	parse_context &context
)
{
	auto const called_type = called.get_typename().as_typespec_view();
	if (called_type.is<ast::ts_base_type>())
	{
		return make_base_type_constructor_call_expression(src_tokens, called_type, std::move(args), context);
	}
	else if (args.empty())
	{
		if (context.is_default_constructible(src_tokens, called_type))
		{
			return context.make_default_construction(src_tokens, called_type);
		}
		else
		{
			context.report_error(
				src_tokens,
				bz::format("type '{}' is not default constructible", called_type)
			);
			return ast::make_error_expression(
				src_tokens,
				ast::make_expr_function_call(src_tokens, std::move(args), nullptr, ast::resolve_order::regular)
			);
		}
	}
	else if (args.size() == 1)
	{
		auto const [type, kind] = args[0].get_expr_type_and_kind();
		if (ast::remove_const_or_consteval(type) == called_type)
		{
			if (ast::is_lvalue(kind))
			{
				args[0].src_tokens = src_tokens;
				return context.make_copy_construction(std::move(args[0]));
			}
			else
			{
				args[0].src_tokens = src_tokens;
				return std::move(args[0]);
			}
		}
		else
		{
			return context.make_cast_expression(src_tokens, std::move(args[0]), called_type);
		}
	}

	context.report_error(
		src_tokens,
		bz::format("no constructors found for type '{}'", called_type)
	);
	return ast::make_error_expression(
		src_tokens,
		ast::make_expr_function_call(src_tokens, std::move(args), nullptr, ast::resolve_order::regular)
	);
}

static ast::expression add_dynamic_called_to_function_call(
	ast::expression called,
	ast::expression function_call,
	parse_context &context
)
{
	if (called.is_dynamic())
	{
		return context.make_binary_operator_expression(
			function_call.src_tokens,
			lex::token::comma,
			std::move(called),
			std::move(function_call)
		);
	}
	else
	{
		return function_call;
	}
}

ast::expression parse_context::make_function_call_expression(
	lex::src_tokens const &src_tokens,
	ast::expression called,
	ast::arena_vector<ast::expression> args
)
{
	args = expand_params(std::move(args));
	if (called.is_error() || args.is_any([](auto const &arg) { return arg.is_error(); }))
	{
		bz_assert(this->has_errors());
		return ast::make_error_expression(
			src_tokens,
			ast::make_expr_function_call(src_tokens, std::move(args), nullptr, ast::resolve_order::regular)
		);
	}
	else if (
		this->in_unresolved_context
		|| this->current_unresolved_locals.not_empty()
		|| called.is_unresolved()
		|| args.is_any([](auto const &arg) { return arg.is_unresolved(); })
	)
	{
		return ast::make_unresolved_expression(
			src_tokens,
			ast::make_unresolved_expr_unresolved_function_call(std::move(called), std::move(args))
		);
	}

	if (called.is_function_name())
	{
		auto const func_decl = called.get_function_name().decl;
		if (resolve::get_function_call_match_level(func_decl, func_decl->body, args, *this, src_tokens).is_null())
		{
			if (func_decl->body.state != ast::resolve_state::error)
			{
				this->report_error(
					src_tokens,
					"couldn't match the function call to the function",
					{
						get_function_parameter_types_note(src_tokens, args),
						this->make_note(func_decl->body.src_tokens, func_decl->body.get_candidate_message())
					}
				);
			}
			return ast::make_error_expression(
				src_tokens,
				ast::make_expr_function_call(src_tokens, std::move(args), &func_decl->body, ast::resolve_order::regular)
			);
		}

		return add_dynamic_called_to_function_call(
			std::move(called),
			make_expr_function_call_from_body(src_tokens, &func_decl->body, std::move(args), *this),
			*this
		);
	}
	else if (called.is_function_alias_name())
	{
		auto const alias_decl = called.get_function_alias_name().decl;
		auto const possible_funcs = get_possible_funcs_for_alias(alias_decl, src_tokens, args, *this);
		if (possible_funcs.empty())
		{
			return ast::make_error_expression(
				src_tokens,
				ast::make_expr_function_call(src_tokens, std::move(args), nullptr, ast::resolve_order::regular)
			);
		}

		auto const [_, best_body] = find_best_match(src_tokens, possible_funcs, args, *this);
		if (best_body == nullptr)
		{
			return ast::make_error_expression(
				src_tokens,
				ast::make_expr_function_call(src_tokens, std::move(args), nullptr, ast::resolve_order::regular)
			);
		}
		else
		{
			return add_dynamic_called_to_function_call(
				std::move(called),
				make_expr_function_call_from_body(src_tokens, best_body, std::move(args), *this),
				*this
			);
		}
	}
	else if (called.is_function_overload_set())
	{
		auto const &overload_set = called.get_function_overload_set();
		auto const possible_funcs = overload_set.id.is_qualified
			? get_possible_funcs_for_qualified_id(overload_set.set, src_tokens, args, *this)
			: get_possible_funcs_for_unqualified_id(overload_set.set, src_tokens, args, *this);

		if (possible_funcs.empty())
		{
			this->report_error(src_tokens, bz::format("no candidate found for function call to '{}'", overload_set.id.as_string()));
			return ast::make_error_expression(
				src_tokens,
				ast::make_expr_function_call(src_tokens, std::move(args), nullptr, ast::resolve_order::regular)
			);
		}
		else
		{
			auto const [_, best_body] = find_best_match(src_tokens, possible_funcs, args, *this);
			if (best_body == nullptr)
			{
				return ast::make_error_expression(
					src_tokens,
					ast::make_expr_function_call(src_tokens, std::move(args), nullptr, ast::resolve_order::regular)
				);
			}
			else
			{
				return add_dynamic_called_to_function_call(
					std::move(called),
					make_expr_function_call_from_body(src_tokens, best_body, std::move(args), *this),
					*this
				);
			}
		}
	}
	else if (called.is_typename())
	{
		bz_assert(called.is_constant());
		return make_constructor_call_expression(src_tokens, std::move(called), std::move(args), *this);
	}
	else if (
		auto const expr_type = ast::remove_const_or_consteval(called.get_expr_type());
		expr_type.is<ast::ts_function>()
	)
	{
		auto const &fn_type = expr_type.get<ast::ts_function>();
		if (args.size() != fn_type.param_types.size())
		{
			this->report_error(
				src_tokens,
				bz::format(
					"indirect call to a function of type '{}' expects {} arguments, but {} were provided",
					expr_type, fn_type.param_types.size(), args.size()
				)
			);
			return ast::make_error_expression(
				src_tokens,
				ast::make_expr_function_call(src_tokens, std::move(args), nullptr, ast::resolve_order::regular)
			);
		}

		for (auto const &[arg, param_type] : bz::zip(args, fn_type.param_types))
		{
			bz_assert(ast::is_complete(param_type));
			ast::typespec param_type_copy = param_type;
			resolve::match_expression_to_type(arg, param_type_copy, *this);
			bz_assert(param_type == param_type_copy);
		}

		if (fn_type.return_type.is<ast::ts_void>())
		{
			return ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::none, fn_type.return_type,
				ast::make_expr_indirect_function_call(src_tokens, std::move(called), std::move(args)),
				ast::destruct_operation()
			);
		}
		else if (fn_type.return_type.is<ast::ts_lvalue_reference>())
		{
			return ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::lvalue_reference, fn_type.return_type.get<ast::ts_lvalue_reference>(),
				ast::make_expr_indirect_function_call(src_tokens, std::move(called), std::move(args)),
				ast::destruct_operation()
			);
		}
		else
		{
			return ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::rvalue, fn_type.return_type,
				ast::make_expr_indirect_function_call(src_tokens, std::move(called), std::move(args)),
				ast::destruct_operation()
			);
		}
	}
	else
	{
		// function call operator
		this->report_error(src_tokens, "operator () not yet implemented");
		return ast::make_error_expression(
			src_tokens,
			ast::make_expr_function_call(src_tokens, std::move(args), nullptr, ast::resolve_order::regular)
		);
	}
}

static void get_possible_funcs_for_universal_function_call_helper(
	bz::vector<possible_func_t> &result,
	lex::src_tokens const &src_tokens,
	ast::identifier const &id,
	bz::array_view<ast::expression> params,
	ast::global_scope_t &scope,
	parse_context &context
)
{
	if (id.is_qualified)
	{
		auto const func_set = find_function_set_by_qualified_id(scope.function_sets, id);
		if (func_set != nullptr)
		{
			for (auto const func : func_set->func_decls.filter(
				[&result](auto const func) {
					return !result.member<&possible_func_t::func_body>().contains(&func->body);
				})
			)
			{
				auto match_level = resolve::get_function_call_match_level(func, func->body, params, context, src_tokens);
				result.push_back({ std::move(match_level), func, &func->body });
			}

			for (auto const alias : func_set->alias_decls)
			{
				context.add_to_resolve_queue(src_tokens, *alias);
				resolve::resolve_function_alias(*alias, context);
				context.pop_resolve_queue();
				for (auto const decl : alias->aliased_decls.filter(
					[&result](auto const decl) {
						return !result.member<&possible_func_t::func_body>().contains(&decl->body);
					}
				))
				{
					auto match_level = resolve::get_function_call_match_level(decl, decl->body, params, context, src_tokens);
					result.push_back({ std::move(match_level), alias, &decl->body });
				}
			}
		}
	}
	else
	{
		for (auto const &func_set : get_function_set_range_by_unqualified_id(scope.function_sets, id, scope.id_scope))
		{
			for (auto const func : func_set.func_decls.filter(
				[&result](auto const func) {
					return !result.member<&possible_func_t::func_body>().contains(&func->body);
				})
			)
			{
				auto match_level = resolve::get_function_call_match_level(func, func->body, params, context, src_tokens);
				result.push_back({ std::move(match_level), func, &func->body });
			}

			for (auto const alias : func_set.alias_decls)
			{
				context.add_to_resolve_queue(src_tokens, *alias);
				resolve::resolve_function_alias(*alias, context);
				context.pop_resolve_queue();
				for (auto const decl : alias->aliased_decls.filter(
					[&result](auto const decl) {
						return !result.member<&possible_func_t::func_body>().contains(&decl->body);
					}
				))
				{
					auto match_level = resolve::get_function_call_match_level(decl, decl->body, params, context, src_tokens);
					result.push_back({ std::move(match_level), alias, &decl->body });
				}
			}
		}
	}
}

static void get_possible_funcs_for_universal_function_call_helper(
	bz::vector<possible_func_t> &result,
	lex::src_tokens const &src_tokens,
	ast::identifier const &id,
	bz::array_view<ast::expression> params,
	ast::enclosing_scope_t scope,
	parse_context &context
)
{
	while (scope.scope != nullptr)
	{
		if (scope.scope->is_local() && result.not_empty())
		{
			break;
		}
		else if (scope.scope->is_local())
		{
			scope = scope.scope->get_local().parent;
		}
		else
		{
			get_possible_funcs_for_universal_function_call_helper(result, src_tokens, id, params, scope.scope->get_global(), context);
			scope = scope.scope->get_global().parent;
		}
	}
}

static bz::vector<possible_func_t> get_possible_funcs_for_universal_function_call(
	lex::src_tokens const &src_tokens,
	ast::identifier const &id,
	bz::array_view<ast::expression> params,
	parse_context &context
)
{
	bz::vector<possible_func_t> possible_funcs = {};

	get_possible_funcs_for_universal_function_call_helper(
		possible_funcs, src_tokens, id, params, context.get_current_enclosing_scope(), context
	);

	if (params.not_empty())
	{
		auto const type = ast::remove_const_or_consteval(params.front().get_expr_type());
		if (type.is<ast::ts_base_type>())
		{
			auto const info = type.get<ast::ts_base_type>().info;
			context.resolve_type(src_tokens, info);
			// TODO: don't use info->enclosing_scope here, because that includes non-exported symbols too
			get_possible_funcs_for_universal_function_call_helper(
				possible_funcs, src_tokens, id, params, info->get_scope(), context
			);
		}
		else if (type.is<ast::ts_enum>())
		{
			auto const decl = type.get<ast::ts_enum>().decl;
			context.resolve_type(src_tokens, decl);
			// TODO: don't use info->enclosing_scope here, because that includes non-exported symbols too
			get_possible_funcs_for_universal_function_call_helper(
				possible_funcs, src_tokens, id, params, decl->get_scope(), context
			);
		}
	}

	if (id.values.size() == 1)
	{
		auto const kinds = context.get_builtin_universal_functions(id.values.front());
		for (auto const decl : kinds.transform([&](auto const kind) { return context.global_ctx.get_builtin_function(kind); }))
		{
			auto match_level = resolve::get_function_call_match_level(decl, decl->body, params, context, src_tokens);
			possible_funcs.push_back({ std::move(match_level), decl, &decl->body });
		}
	}

	return possible_funcs;
}

ast::expression parse_context::make_universal_function_call_expression(
	lex::src_tokens const &src_tokens,
	ast::expression base,
	ast::identifier id,
	ast::arena_vector<ast::expression> args
)
{
	if (this->in_unresolved_context || base.is_unresolved() || args.is_any([](auto &arg) { return arg.is_unresolved(); }))
	{
		return ast::make_unresolved_expression(
			src_tokens,
			ast::make_unresolved_expr_unresolved_universal_function_call(std::move(base), std::move(id), std::move(args))
		);
	}

	args = expand_params(args);
	if (base.is_error())
	{
		bz_assert(this->has_errors());
		args.push_front(std::move(base));
		return ast::make_error_expression(
			src_tokens,
			ast::make_expr_function_call(src_tokens, std::move(args), nullptr, ast::resolve_order::regular)
		);
	}

	for (auto &arg : args)
	{
		if (arg.is_error())
		{
			bz_assert(this->has_errors());
			args.push_front(std::move(base));
			return ast::make_error_expression(
				src_tokens,
				ast::make_expr_function_call(src_tokens, std::move(args), nullptr, ast::resolve_order::regular)
			);
		}
	}

	args.push_front(std::move(base));
	auto const possible_funcs = get_possible_funcs_for_universal_function_call(src_tokens, id, args, *this);
	if (possible_funcs.empty())
	{
		this->report_error(src_tokens, bz::format("no candidate found for universal function call to '{}'", id.as_string()));
		return ast::make_error_expression(
			src_tokens,
			ast::make_expr_function_call(src_tokens, std::move(args), nullptr, ast::resolve_order::regular)
		);
	}
	else
	{
		auto const [_, best_body] = find_best_match(src_tokens, possible_funcs, args, *this);
		if (best_body == nullptr)
		{
			return ast::make_error_expression(
				src_tokens,
				ast::make_expr_function_call(src_tokens, std::move(args), nullptr, ast::resolve_order::regular)
			);
		}
		else if (
			best_body->is_intrinsic()
			&& best_body->intrinsic_kind == ast::function_body::builtin_slice_size
			&& ast::remove_const_or_consteval(args.front().get_expr_type()).is<ast::ts_array>()
		)
		{
			auto const &array_t = ast::remove_const_or_consteval(args.front().get_expr_type()).get<ast::ts_array>();
			ast::constant_value size;
			size.emplace<ast::constant_value::uint>(array_t.size);
			return make_expr_function_call_from_body(src_tokens, best_body, std::move(args), std::move(size), *this);
		}
		else
		{
			return make_expr_function_call_from_body(src_tokens, best_body, std::move(args), *this);
		}
	}
}

ast::expression parse_context::make_subscript_operator_expression(
	lex::src_tokens const &src_tokens,
	ast::expression called,
	ast::arena_vector<ast::expression> args
)
{
	if (called.is_error() || args.is_any([](auto const &arg) { return arg.is_error(); }))
	{
		bz_assert(this->has_errors());
		return ast::make_error_expression(src_tokens, ast::make_expr_subscript(std::move(called), ast::expression()));
	}
	else if (
		this->in_unresolved_context
		|| called.is_unresolved()
		|| args.is_any([](auto const &arg) { return arg.is_unresolved(); })
	)
	{
		return ast::make_unresolved_expression(
			src_tokens,
			ast::make_unresolved_expr_unresolved_subscript(std::move(called), std::move(args))
		);
	}

	if (called.is_typename())
	{
		ast::typespec_view const type = called.get_typename();
		auto const type_without_const = ast::remove_const_or_consteval(type);
		if (!type_without_const.is<ast::ts_base_type>())
		{
			this->report_error(src_tokens, bz::format("invalid type '{}' for struct initializer", type));
			return ast::make_error_expression(src_tokens, ast::make_expr_aggregate_init(type, std::move(args)));
		}

		auto const info = type_without_const.get<ast::ts_base_type>().info;
		this->resolve_type_members(src_tokens, info);
		if (info->kind != ast::type_info::aggregate)
		{
			this->report_error(src_tokens, bz::format("invalid type '{}' for struct initializer", type));
			return ast::make_error_expression(src_tokens, ast::make_expr_aggregate_init(type, std::move(args)));
		}
		else if (
			!this->has_common_global_scope(info->get_scope())
			&& info->member_variables.is_any([](auto const member) { return member->get_unqualified_id_value().starts_with('_'); })
		)
		{
			auto notes = info->member_variables
				.filter([](auto const &member) { return member->get_unqualified_id_value().starts_with('_'); })
				.transform([this, type](auto const member) {
					return this->make_note(
						member->src_tokens,
						bz::format("member '{}' in type '{}' is inaccessible in this context", member->get_unqualified_id_value(), type)
					);
				})
				.collect(info->member_variables.size());
			if (do_verbose)
			{
				notes.push_back(this->make_note("members whose names start with '_' are only accessible in the same file"));
			}
			this->report_error(src_tokens, bz::format("invalid type '{}' for struct initializer", type), std::move(notes));
			return ast::make_error_expression(src_tokens, ast::make_expr_aggregate_init(type, std::move(args)));
		}
		else if (info->member_variables.size() != args.size())
		{
			auto const member_size = info->member_variables.size();
			auto const args_size = args.size();
			if (member_size < args_size)
			{
				this->report_error(
					src_tokens,
					bz::format("too many initializers for type '{}'", type),
					{ this->make_note(
						info->src_tokens,
						bz::format("'struct {}' is defined here", info->get_typename_as_string())
					) }
				);
				return ast::make_error_expression(src_tokens, ast::make_expr_aggregate_init(type, std::move(args)));
			}
			else if (member_size - args_size == 1)
			{
				this->report_error(
					src_tokens,
					bz::format("missing initializer for field '{}' in type '{}'", info->member_variables.back()->get_unqualified_id_value(), type),
					{ this->make_note(
						info->src_tokens,
						bz::format("'struct {}' is defined here", info->get_typename_as_string())
					) }
				);
				return ast::make_error_expression(src_tokens, ast::make_expr_aggregate_init(type, std::move(args)));
			}
			else
			{
				auto const message = [&]() {
					bz::u8string result = "missing initializers for fields ";
					result += bz::format("'{}'", info->member_variables[args_size]->get_unqualified_id_value());
					for (size_t i = args_size + 1; i < member_size - 1; ++i)
					{
						result += bz::format(", '{}'", info->member_variables[i]->get_unqualified_id_value());
					}
					result += bz::format(" and '{}' in type '{}'", info->member_variables.back()->get_unqualified_id_value(), type);
					return result;
				}();
				this->report_error(
					src_tokens,
					std::move(message),
					{ this->make_note(
						info->src_tokens,
						bz::format("'struct {}' is defined here", info->get_typename_as_string())
					) }
				);
				return ast::make_error_expression(src_tokens, ast::make_expr_aggregate_init(type, std::move(args)));
			}
		}

		bool is_good = true;
		for (auto const [expr, member] : bz::zip(args, info->member_variables))
		{
			resolve::match_expression_to_type(expr, member->get_type(), *this);
			is_good = is_good && expr.not_error();
		}
		if (!is_good)
		{
			return ast::make_error_expression(src_tokens, ast::make_expr_aggregate_init(type, std::move(args)));
		}

		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			type_without_const,
			ast::make_expr_aggregate_init(type_without_const, std::move(args)),
			ast::destruct_operation()
		);
	}
	else
	{
		if (args.size() == 0)
		{
			this->report_error(src_tokens, "subscript expression expects at least one index");
			return ast::make_error_expression(src_tokens, ast::make_expr_subscript(std::move(called), ast::expression()));
		}
		for (auto &arg : args)
		{
			if (arg.is_error())
			{
				bz_assert(this->has_errors());
				return ast::make_error_expression(src_tokens, ast::make_expr_subscript(std::move(called), std::move(arg)));
			}

			auto const [type, kind] = called.get_expr_type_and_kind();
			auto const constless_type = ast::remove_const_or_consteval(type);
			if (
				constless_type.is<ast::ts_array>()
				|| constless_type.is<ast::ts_array_slice>()
				|| constless_type.is<ast::ts_tuple>()
				|| called.is_tuple()
			)
			{
				called = make_builtin_subscript_operator(src_tokens, std::move(called), std::move(arg), *this);
			}
			else
			{
				auto const possible_funcs = get_possible_funcs_for_operator(src_tokens, lex::token::square_open, called, arg, *this);
				if (possible_funcs.empty())
				{
					this->report_error(
						src_tokens,
						bz::format(
							"no candidate found for binary 'operator []' with types '{}' and '{}'",
							type, arg.get_expr_type()
						)
					);
					return ast::make_error_expression(src_tokens, ast::make_expr_subscript(std::move(called), std::move(arg)));
				}
				else
				{
					ast::arena_vector<ast::expression> args;
					args.reserve(2);
					args.emplace_back(std::move(called));
					args.emplace_back(std::move(arg));
					auto const [best_stmt, best_body] = find_best_match(src_tokens, possible_funcs, args, *this);
					if (best_body == nullptr)
					{
						return ast::make_error_expression(src_tokens, ast::make_expr_subscript(std::move(args[0]), std::move(args[1])));
					}
					else
					{
						called = make_expr_function_call_from_body(
							src_tokens, best_body, std::move(args),
							*this, ast::resolve_order::regular
						);
					}
				}
			}
		}
		return called;
	}
}

ast::expression parse_context::make_cast_expression(
	lex::src_tokens const &src_tokens,
	ast::expression expr,
	ast::typespec type
)
{
	if (expr.is_error() || type.is_empty())
	{
		bz_assert(this->has_errors());
		return ast::make_error_expression(src_tokens, ast::make_expr_cast(std::move(expr), std::move(type)));
	}

	if (expr.is_if_expr())
	{
		auto &if_expr = expr.get_if_expr();
		auto const then_block_src_tokens = if_expr.then_block.src_tokens;
		if_expr.then_block = this->make_cast_expression(then_block_src_tokens, std::move(if_expr.then_block), type);
		auto const else_block_src_tokens = if_expr.else_block.src_tokens;
		if_expr.else_block = this->make_cast_expression(else_block_src_tokens, std::move(if_expr.else_block), std::move(type));

		expr.src_tokens = src_tokens;
		return expr;
	}
	else if (expr.is_switch_expr())
	{
		auto &switch_expr = expr.get_switch_expr();
		if (switch_expr.default_case.not_null())
		{
			auto const default_case_src_tokens = switch_expr.default_case.src_tokens;
			switch_expr.default_case = this->make_cast_expression(default_case_src_tokens, std::move(switch_expr.default_case), type);
		}

		for (auto &[_, case_expr] : switch_expr.cases)
		{
			auto const case_src_tokens = case_expr.src_tokens;
			case_expr = this->make_cast_expression(case_src_tokens, std::move(case_expr), type);
		}

		expr.src_tokens = src_tokens;
		return expr;
	}
	else if (expr.is_enum_literal())
	{
		auto const id = expr.get_enum_literal().id;
		auto const id_value = id->value;
		if (!type.is<ast::ts_enum>())
		{
			this->report_error(
				src_tokens,
				bz::format("invalid conversion of enum literal '.{}' to non-enum type '{}'", id_value, type)
			);
			return ast::make_error_expression(src_tokens, ast::make_expr_cast(std::move(expr), std::move(type)));
		}

		auto const decl = type.get<ast::ts_enum>().decl;
		this->resolve_type(src_tokens, decl);

		auto const it = std::find_if(
			decl->values.begin(), decl->values.end(),
			[id_value](auto const &value) {
				return value.id->value == id_value;
			}
		);

		if (it == decl->values.end())
		{
			this->report_error(
				src_tokens,
				bz::format("invalid conversion of enum literal '.{}' to enum type '{}'", id_value, type),
				{ make_note(decl->src_tokens, bz::format("enum type '{}' has no member named '{}'", type, id_value)) }
			);
			return ast::make_error_expression(src_tokens, ast::make_expr_cast(std::move(expr), std::move(type)));
		}

		auto const &value = *it;

		auto &inner_expr = expr.get_enum_literal_expr();

		inner_expr = ast::make_constant_expression(
			inner_expr.src_tokens,
			ast::expression_type_kind::rvalue, type,
			value.value.is<int64_t>()
				? ast::constant_value::get_enum(decl, value.value.get<int64_t>())
				: ast::constant_value::get_enum(decl, value.value.get<uint64_t>()),
			ast::make_expr_enum_literal(id)
		);
		return expr;
	}
	else
	{
		auto const [expr_type, expr_type_kind] = expr.get_expr_type_and_kind();

		if (ast::remove_const_or_consteval(expr_type) == type)
		{
			if (ast::is_lvalue(expr_type_kind))
			{
				expr.src_tokens = src_tokens;
				return this->make_copy_construction(std::move(expr));
			}
			else
			{
				expr.src_tokens = src_tokens;
				return expr;
			}
		}
		else if (is_builtin_type(expr_type))
		{
			auto result = make_builtin_cast(src_tokens, std::move(expr), std::move(type), *this);
			result.src_tokens = src_tokens;
			return result;
		}
		else
		{
			this->report_error(src_tokens, bz::format("invalid cast to type '{}'", type));
			return ast::make_error_expression(src_tokens, ast::make_expr_cast(std::move(expr), std::move(type)));
		}
	}
}

// just an arbitrary number, most types should fit into it
constexpr size_t bit_cast_small_type_size = 64;

static void fill_small_type_padding_array(
	bz::array_view<bool> is_padding,
	ast::type_prototype const *elem_type,
	size_t size
);

static void fill_small_type_padding_single(
	bz::array_view<bool> is_padding,
	ast::type_prototype const *type
)
{
	bz_assert(is_padding.size() == type->size);
	if (!type->has_padding())
	{
		return;
	}
	else if (type->is_aggregate())
	{
		auto const elem_types = type->get_aggregate_types();
		auto const offsets = type->get_aggregate_offsets();

		if (elem_types.empty())
		{
			bz_assert(type->size == 1);
			is_padding[0] = true;
			return;
		}

		auto const elem_count = elem_types.size();
		for (auto const i : bz::iota(0, elem_count))
		{
			auto const elem_type = elem_types[i];
			auto const offset = offsets[i];
			fill_small_type_padding_single(is_padding.slice(offset, offset + elem_type->size), elem_type);
			auto const next_offset = i + 1 == elem_count ? type->size : offsets[i + 1];
			if (offset + elem_type->size != next_offset)
			{
				for (auto &value : is_padding.slice(offset + elem_type->size, next_offset))
				{
					value = true;
				}
			}
		}
	}
	else if (type->is_array())
	{
		fill_small_type_padding_array(is_padding, type->get_array_element_type(), type->get_array_size());
	}
	else
	{
		// nothing
	}
}

static void fill_small_type_padding_array(
	bz::array_view<bool> is_padding,
	ast::type_prototype const *elem_type,
	size_t size
)
{
	auto const elem_size = elem_type->size;
	fill_small_type_padding_single(is_padding.slice(0, elem_size), elem_type);

	for (auto const i : bz::iota(elem_size, size * elem_size))
	{
		is_padding[i] = is_padding[i - elem_size];
	}
}

static bool check_small_type_bit_cast_paddings(
	ast::type_prototype const *value_type,
	ast::type_prototype const *result_type
)
{
	bz::array<bool, bit_cast_small_type_size> value_type_buffer{};
	bz::array<bool, bit_cast_small_type_size> result_type_buffer{};

	auto const value_type_is_padding = bz::array_view(value_type_buffer.data(), value_type_buffer.size()).slice(0, value_type->size);
	auto const result_type_is_padding = bz::array_view(result_type_buffer.data(), result_type_buffer.size()).slice(0, result_type->size);

	fill_small_type_padding_single(value_type_is_padding, value_type);
	fill_small_type_padding_single(result_type_is_padding, result_type);

	for (auto const i : bz::iota(0, value_type_is_padding.size()))
	{
		if (value_type_is_padding[i] && !result_type_is_padding[i])
		{
			return false;
		}
	}
	return true;
}

struct offset_padding_size_pair
{
	uint32_t offset;
	uint32_t padding_size;
};

static void get_type_paddings_helper(bz::vector<offset_padding_size_pair> &result, uint32_t current_offset, ast::type_prototype const *type)
{
	if (!type->has_padding())
	{
		return;
	}
	else if (type->is_aggregate())
	{
		auto const elem_types = type->get_aggregate_types();
		auto const offsets = type->get_aggregate_offsets();

		if (elem_types.empty())
		{
			bz_assert(type->size == 1);
			result.push_back({ .offset = current_offset, .padding_size = 1 });
			return;
		}

		auto const elem_count = elem_types.size();
		for (auto const i : bz::iota(0, elem_count))
		{
			auto const elem_type = elem_types[i];
			auto const offset = offsets[i];
			get_type_paddings_helper(result, current_offset + offset, elem_type);
			auto const next_offset = i + 1 == elem_count ? type->size : offsets[i + 1];
			if (offset + elem_type->size != next_offset)
			{
				result.push_back({
					.offset = current_offset + static_cast<uint32_t>(offset + elem_type->size),
					.padding_size = static_cast<uint32_t>(next_offset - (offset + elem_type->size)),
				});
			}
		}
	}
	else if (type->is_array())
	{
		auto const elem_type = type->get_array_element_type();
		auto const size = type->get_array_size();

		auto const result_start_size = result.size();
		get_type_paddings_helper(result, current_offset, elem_type);
		auto const new_paddings_count = result.size() - result_start_size;
		bz_assert(new_paddings_count != 0);
		auto const new_result_size = result.size() + (size - 1) * new_paddings_count;
		result.reserve(new_result_size);
		for (auto const i : bz::iota(result_start_size, new_result_size - new_paddings_count))
		{
			auto const prev_padding = result[i];
			result.push_back({
				.offset = prev_padding.offset + static_cast<uint32_t>(elem_type->size),
				.padding_size = prev_padding.padding_size,
			});
		}
	}
	else
	{
		bz_unreachable;
	}
}

static bz::vector<offset_padding_size_pair> get_type_paddings(ast::type_prototype const *type)
{
	bz::vector<offset_padding_size_pair> result;
	get_type_paddings_helper(result, 0, type);
	return result;
}

static bool check_large_type_bit_cast_paddings(
	ast::type_prototype const *value_type,
	ast::type_prototype const *result_type
)
{
	auto const value_type_paddings = get_type_paddings(value_type);
	auto const result_type_paddings = get_type_paddings(result_type);

	auto value_it = value_type_paddings.begin();
	auto const value_end = value_type_paddings.end();
	auto result_it = result_type_paddings.begin();
	auto const result_end = value_type_paddings.end();

	while (result_it != result_end && value_it != value_end)
	{
		auto has_padding_until = value_it->offset;
		while (has_padding_until < value_it->offset + value_it->padding_size)
		{
			if (result_it == result_end || result_it->offset > has_padding_until)
			{
				return false;
			}

			has_padding_until = std::max(has_padding_until, result_it->offset + result_it->padding_size);
			++result_it;
		}

		++value_it;
	}
	return value_it == value_end;
}

static bool check_bit_cast_type_paddings(
	ast::type_prototype const *value_type,
	ast::type_prototype const *result_type
)
{
	if (value_type == result_type)
	{
		return true;
	}

	auto const value_type_has_padding = value_type->has_padding();
	auto const result_type_has_padding = result_type->has_padding();
	if (!value_type_has_padding)
	{
		return true;
	}
	else if (!result_type_has_padding && value_type_has_padding)
	{
		return false;
	}
	else if (value_type->size <= bit_cast_small_type_size)
	{
		return check_small_type_bit_cast_paddings(value_type, result_type);
	}
	else
	{
		return check_large_type_bit_cast_paddings(value_type, result_type);
	}
}

ast::expression parse_context::make_bit_cast_expression(
	lex::src_tokens const &src_tokens,
	ast::expression expr,
	ast::typespec result_type
)
{
	auto const expr_type = expr.get_expr_type();

	if (!this->is_trivial(src_tokens, expr_type))
	{
		this->report_error(src_tokens, bz::format("value type '{}' is not trivial in bit cast", expr_type));
		return ast::make_error_expression(src_tokens, ast::make_expr_bit_cast(std::move(expr), std::move(result_type)));
	}
	else if (!this->is_trivial(src_tokens, result_type))
	{
		this->report_error(src_tokens, bz::format("result type '{}' is not trivial in bit cast", result_type));
		return ast::make_error_expression(src_tokens, ast::make_expr_bit_cast(std::move(expr), std::move(result_type)));
	}

	auto const expr_prototype = ast::get_type_prototype(expr_type, this->get_type_prototype_set());
	auto const dest_prototype = ast::get_type_prototype(result_type, this->get_type_prototype_set());

	if (expr_prototype->size != dest_prototype->size)
	{
		this->report_error(
			src_tokens,
			bz::format(
				"value type '{}' and result type '{}' have different sizes {} and {}",
				expr_type, result_type, expr_prototype->size, dest_prototype->size
			)
		);
		return ast::make_error_expression(src_tokens, ast::make_expr_bit_cast(std::move(expr), std::move(result_type)));
	}
	else if (!check_bit_cast_type_paddings(expr_prototype, dest_prototype))
	{
		return ast::make_error_expression(src_tokens, ast::make_expr_bit_cast(std::move(expr), std::move(result_type)));
	}
	else
	{
		auto expr_result_type = result_type;
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::rvalue, std::move(expr_result_type),
			ast::make_expr_bit_cast(std::move(expr), std::move(result_type)),
			ast::destruct_operation()
		);
	}
}

ast::expression parse_context::make_optional_cast_expression(ast::expression expr)
{
	auto const [expr_type, expr_type_kind] = expr.get_expr_type_and_kind();
	bz_assert(ast::is_rvalue_or_literal(expr_type_kind));
	ast::typespec result_type = expr_type;
	result_type.add_layer<ast::ts_optional>();
	ast::typespec optional_cast_type = result_type;
	return ast::make_dynamic_expression(
		expr.src_tokens,
		ast::expression_type_kind::rvalue, std::move(result_type),
		ast::make_expr_optional_cast(std::move(expr), std::move(optional_cast_type)),
		ast::destruct_operation()
	);
}

ast::expression parse_context::make_member_access_expression(
	lex::src_tokens const &src_tokens,
	ast::expression base,
	lex::token_pos member
)
{
	if (base.is_error())
	{
		bz_assert(this->has_errors());
		return ast::make_error_expression(src_tokens, ast::make_expr_member_access(std::move(base), 0));
	}
	else if (this->in_unresolved_context || base.is_unresolved())
	{
		return ast::make_unresolved_expression(
			src_tokens,
			ast::make_unresolved_expr_unresolved_member_access(std::move(base), member)
		);
	}

	if (base.is_typename())
	{
		auto const type = ast::remove_const_or_consteval(ast::remove_lvalue_reference(base.get_typename().as_typespec_view()));
		if (type.is<ast::ts_base_type>())
		{
			auto const info = type.get<ast::ts_base_type>().info;
			this->resolve_type_members(src_tokens, info);
			bz_assert(info->scope.is_global());
			auto id = ast::make_identifier(member);
			auto const symbol = find_id_in_global_scope(info->scope.get_global(), id, *this);

			if (symbol.is_null())
			{
				this->report_error(member, bz::format("no member named '{}' in type '{}'", member->value, type));
				return ast::make_error_expression(src_tokens, ast::make_expr_type_member_access(std::move(base), member, nullptr));
			}
			else
			{
				return expression_from_symbol(src_tokens, std::move(id), symbol, 0, false, *this);
			}
		}
		else if (type.is<ast::ts_enum>())
		{
			auto const decl = type.get<ast::ts_enum>().decl;
			this->resolve_type(src_tokens, decl);
			auto const member_value = member->value;

			auto const result_it = std::find_if(
				decl->values.begin(), decl->values.end(),
				[member_value](auto const &value) {
					return value.id->value == member_value;
				}
			);
			if (result_it == decl->values.end())
			{
				this->report_error(member, bz::format("no value named '{}' in enum '{}'", member->value, type));
				return ast::make_error_expression(src_tokens, ast::make_expr_type_member_access(std::move(base), member, nullptr));
			}
			else
			{
				bz_assert(result_it->value.not_null());
				auto value = result_it->value.is<int64_t>()
					? ast::constant_value::get_enum(decl, result_it->value.get<int64_t>())
					: ast::constant_value::get_enum(decl, result_it->value.get<uint64_t>());
				return ast::make_constant_expression(
					src_tokens,
					ast::expression_type_kind::rvalue, type,
					std::move(value),
					ast::make_expr_type_member_access(std::move(base), member, nullptr)
				);
			}
		}
		else
		{
			this->report_error(member, bz::format("no member named '{}' in type '{}'", member->value, type));
			return ast::make_error_expression(src_tokens, ast::make_expr_type_member_access(std::move(base), member, nullptr));
		}
	}

	auto const [base_type, base_type_kind] = base.get_expr_type_and_kind();
	auto const base_t = ast::remove_const_or_consteval(base_type);
	if (base_t.is<ast::ts_base_type>())
	{
		this->resolve_type_members(src_tokens, base_t.get<ast::ts_base_type>().info);
	}
	auto const members = [&]() -> bz::array_view<ast::decl_variable *> {
		if (base_t.is<ast::ts_base_type>())
		{
			return base_t.get<ast::ts_base_type>().info->member_variables.as_array_view();
		}
		else
		{
			return {};
		}
	}();
	auto const type_global_scope = [&]() -> ast::enclosing_scope_t {
		if (base_t.is<ast::ts_base_type>())
		{
			return base_t.get<ast::ts_base_type>().info->get_scope();
		}
		else
		{
			return {};
		}
	}();
	auto const it = std::find_if(
		members.begin(), members.end(),
		[member_value = member->value](auto const member_variable) {
			return member_value == member_variable->get_unqualified_id_value();
		}
	);
	if (it == members.end())
	{
		this->report_error(member, bz::format("no member named '{}' in value of type '{}'", member->value, base_t));
		return ast::make_error_expression(src_tokens, ast::make_expr_member_access(std::move(base), 0));
	}
	else if (
		auto const member_ptr = *it;
		!this->has_common_global_scope(type_global_scope) && member_ptr->get_unqualified_id_value().starts_with('_')
	)
	{
		auto notes = [&]() -> bz::vector<source_highlight> {
			if (do_verbose)
			{
				return {
					this->make_note(member_ptr->src_tokens, "member is declared here"),
					this->make_note("members whose names start with '_' are only accessible in the same file")
				};
			}
			else
			{
				return { this->make_note(member_ptr->src_tokens, "member is declared here") };
			}
		}();
		this->report_error(
			member, bz::format("member '{}' in value of type '{}' is inaccessible in this context", member->value, base_t),
			std::move(notes)
		);
		// no need to return here, the type of the member is available so the expression doesn't have to be in an error state
		// return ast::make_error_expression(src_tokens, ast::make_expr_member_access(std::move(base), 0));
	}
	auto const index = static_cast<uint32_t>(it - members.begin());
	auto result_type = (*it)->get_type();
	if (
		!result_type.is<ast::ts_const>()
		&& !result_type.is<ast::ts_lvalue_reference>()
		&& base_type.is<ast::ts_const>()
	)
	{
		result_type.add_layer<ast::ts_const>();
	}

	if (base_type_kind == ast::expression_type_kind::rvalue)
	{
		bz_assert(base_t.is<ast::ts_base_type>());
		if (base_t.get<ast::ts_base_type>().info->destructor != nullptr)
		{
			auto const dtor = base_t.get<ast::ts_base_type>().info->destructor;
			this->report_error(
				src_tokens,
				bz::format("accessing member '{}' of a value of type '{}' is not allowed", member->value, base_t),
				{ this->make_note(
					dtor->body.src_tokens,
					bz::format("type '{}' has a non-default destructor defined here", base_t)
				) }
			);
			return ast::make_error_expression(src_tokens, ast::make_expr_member_access(std::move(base), index));
		}

		auto const elem_refs = bz::iota(0, members.size())
			.transform([&](size_t const i) {
				if (i == index)
				{
					return ast::make_dynamic_expression(
						src_tokens,
						ast::expression_type_kind::rvalue_reference, result_type,
						ast::make_expr_bitcode_value_reference(),
						ast::destruct_operation()
					);
				}

				auto const elem_t = members[i]->get_type().as_typespec_view();
				if (elem_t.is<ast::ts_lvalue_reference>() || this->is_trivially_destructible(base.src_tokens, elem_t))
				{
					return ast::expression();
				}

				auto result = ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue_reference, elem_t,
					ast::make_expr_bitcode_value_reference(),
					ast::destruct_operation()
				);
				this->add_self_destruction(result);
				return result;
			})
			.collect<ast::arena_vector>();

		auto const result_kind = result_type.is<ast::ts_lvalue_reference>()
			? ast::expression_type_kind::lvalue_reference
			: ast::expression_type_kind::rvalue_reference;

		if (result_type.is<ast::ts_lvalue_reference>())
		{
			result_type.remove_layer();
		}
		return ast::make_dynamic_expression(
			src_tokens,
			result_kind, std::move(result_type),
			ast::make_expr_rvalue_member_access(std::move(base), std::move(elem_refs), index),
			ast::destruct_operation()
		);
	}
	else
	{
		auto const result_kind = result_type.is<ast::ts_lvalue_reference>()
			? ast::expression_type_kind::lvalue_reference
			: base_type_kind;

		if (result_type.is<ast::ts_lvalue_reference>())
		{
			result_type.remove_layer();
		}
		return ast::make_dynamic_expression(
			src_tokens,
			result_kind, std::move(result_type),
			ast::make_expr_member_access(std::move(base), index),
			ast::destruct_operation()
		);
	}
}

ast::expression parse_context::make_generic_type_instantiation_expression(
	lex::src_tokens const &src_tokens,
	ast::expression base,
	ast::arena_vector<ast::expression> args
)
{
	args = expand_params(std::move(args));
	if (base.is_error() || args.is_any([](auto const &arg) { return arg.is_error(); }))
	{
		return ast::make_error_expression(src_tokens);
	}
	else if (
		this->in_unresolved_context
		|| base.is_unresolved()
		|| args.is_any([](auto const &arg) { return arg.is_unresolved(); })
	)
	{
		return ast::make_unresolved_expression(
			src_tokens,
			ast::make_unresolved_expr_unresolved_generic_type_instantiation(std::move(base), std::move(args))
		);
	}

	if (!base.is_generic_type())
	{
		if (base.is_typename())
		{
			this->report_error(base.src_tokens, bz::format("type '{}' is not generic", base.get_typename()));
		}
		else
		{
			this->report_error(base.src_tokens, "expression is not a generic type");
		}
		return ast::make_error_expression(src_tokens);
	}

	auto info = base.get_generic_type();
	this->add_to_resolve_queue(src_tokens, *info);
	resolve::resolve_type_info_parameters(*info, *this);
	this->pop_resolve_queue();

	auto required_from = get_generic_requirements(src_tokens, *this);
	auto generic_params = info->get_params_copy_for_generic_instantiation();
	expand_variadic_params(generic_params, args.size());
	if (generic_params.size() != args.size())
	{
		this->report_error(src_tokens, "number of arguments doesn't match the number of parameters");
		return ast::make_error_expression(src_tokens);
	}
	bool good = true;
	for (auto const [arg, generic_param] : bz::zip(args, generic_params))
	{
		resolve::match_expression_to_variable(arg, generic_param, *this);
		resolve::consteval_try(arg, *this);
		bz_assert(!generic_param.get_type().is<ast::ts_variadic>());
		good &= arg.not_error();
		if (arg.not_error())
		{
			generic_param.init_expr = std::move(arg);
		}
	}
	if (!good)
	{
		return ast::make_error_expression(src_tokens);
	}

	info = info->add_generic_instantiation(std::move(generic_params), std::move(required_from));
	bz_assert(!info->is_generic());

	return ast::make_constant_expression(
		src_tokens,
		ast::expression_type_kind::type_name, ast::make_typename_typespec(nullptr),
		ast::constant_value(ast::make_base_type_typespec(src_tokens, info)),
		ast::make_expr_generic_type_instantiation(info)
	);
}

static ast::expression make_tuple_default_construction(
	lex::src_tokens const &src_tokens,
	ast::typespec_view type,
	parse_context &context
)
{
	bz_assert(type.is<ast::ts_tuple>());
	auto const types = type.get<ast::ts_tuple>().types.as_array_view();
	if (!context.is_default_constructible(src_tokens, type))
	{
		context.report_error(
			src_tokens,
			bz::format("tuple type '{}' is not default constructible", type),
			types
				.filter([&](auto const &type) { return !context.is_default_constructible(src_tokens, type); })
				.transform([&](auto const &elem_type) {
					return parse_context::make_note(
						src_tokens,
						bz::format("tuple element type '{}' is not default constructible", elem_type)
					);
				})
				.collect()
		);
		return ast::make_error_expression(
			src_tokens,
			ast::make_expr_aggregate_default_construct(type, ast::arena_vector<ast::expression>())
		);
	}

	auto const default_construct_exprs = types
		.transform([&](auto const &elem_type) { return context.make_default_construction(src_tokens, elem_type); })
		.collect<ast::arena_vector>();
	return ast::make_dynamic_expression(
		src_tokens,
		ast::expression_type_kind::rvalue, type,
		ast::make_expr_aggregate_default_construct(type, std::move(default_construct_exprs)),
		ast::destruct_operation()
	);
}

static ast::expression make_array_default_construction(
	lex::src_tokens const &src_tokens,
	ast::typespec_view type,
	parse_context &context
)
{
	bz_assert(type.is<ast::ts_array>());
	auto const elem_type = type.get<ast::ts_array>().elem_type.as_typespec_view();
	if (!context.is_default_constructible(src_tokens, type))
	{
		bz_assert(!context.is_default_constructible(src_tokens, elem_type));
		context.report_error(
			src_tokens,
			bz::format("array type '{}' is not default constructible", type),
			{ context.make_note(
				src_tokens,
				bz::format("array element type '{}' is not default constructible", elem_type)
			) }
		);
		return ast::make_error_expression(
			src_tokens,
			ast::make_expr_array_default_construct(type, ast::expression())
		);
	}

	return ast::make_dynamic_expression(
		src_tokens,
		ast::expression_type_kind::rvalue, type,
		ast::make_expr_array_default_construct(type, context.make_default_construction(src_tokens, elem_type)),
		ast::destruct_operation()
	);
}

static ast::expression make_optional_default_construction(
	lex::src_tokens const &src_tokens,
	ast::typespec_view type,
	parse_context &
)
{
	bz_assert(type.is<ast::ts_optional>());
	return ast::make_dynamic_expression(
		src_tokens,
		ast::expression_type_kind::rvalue, type,
		ast::make_expr_optional_default_construct(type),
		ast::destruct_operation()
	);
}

static ast::expression make_builtin_default_construction(
	lex::src_tokens const &src_tokens,
	ast::typespec_view type,
	parse_context &context
)
{
	if (!context.is_default_constructible(src_tokens, type))
	{
		context.report_error(
			src_tokens,
			bz::format("type '{}' is not default constructible", type)
		);
		return ast::make_error_expression(
			src_tokens,
			ast::make_expr_builtin_default_construct(type)
		);
	}

	if (type.is<ast::ts_array_slice>())
	{
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::rvalue, type,
			ast::make_expr_builtin_default_construct(type),
			ast::destruct_operation()
		);
	}
	else
	{
		bz_assert(type.is<ast::ts_base_type>());
		auto const info = type.get<ast::ts_base_type>().info;
		auto value = [&]() {
			switch (info->kind)
			{
			case ast::type_info::int8_:
			case ast::type_info::int16_:
			case ast::type_info::int32_:
			case ast::type_info::int64_:
				return ast::constant_value(int64_t());
			case ast::type_info::uint8_:
			case ast::type_info::uint16_:
			case ast::type_info::uint32_:
			case ast::type_info::uint64_:
				return ast::constant_value(uint64_t());
			case ast::type_info::float32_:
				return ast::constant_value(float32_t());
			case ast::type_info::float64_:
				return ast::constant_value(float64_t());
			case ast::type_info::char_:
				return ast::constant_value(bz::u8char());
			case ast::type_info::str_:
				return ast::constant_value(bz::u8string());
			case ast::type_info::bool_:
				return ast::constant_value(bool());
			case ast::type_info::null_t_:
				return ast::constant_value::get_null();
			default:
				bz_unreachable;
			}
		}();
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::rvalue, type,
			std::move(value),
			ast::make_expr_builtin_default_construct(type)
		);
	}
}

static ast::expression make_struct_default_construction(
	lex::src_tokens const &src_tokens,
	ast::typespec_view type,
	parse_context &context
)
{
	bz_assert(type.is<ast::ts_base_type>());
	auto const info = type.get<ast::ts_base_type>().info;

	if (!info->is_default_constructible())
	{
		context.report_error(
			src_tokens,
			bz::format("type '{}' is not default constructible", type)
		);
		return ast::make_error_expression(
			src_tokens,
			ast::make_expr_aggregate_default_construct(type, ast::arena_vector<ast::expression>())
		);
	}

	if (info->default_constructor != nullptr)
	{
		return make_expr_function_call_from_body(
			src_tokens,
			&info->default_constructor->body,
			{},
			context,
			ast::resolve_order::regular
		);
	}
	else
	{
		auto const member_default_construction_exprs = info->member_variables
			.transform([&](auto const member) { return context.make_default_construction(src_tokens, member->get_type()); })
			.collect<ast::arena_vector>();
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::rvalue, type,
			ast::make_expr_aggregate_default_construct(type, std::move(member_default_construction_exprs)),
			ast::destruct_operation()
		);
	}
}

ast::expression parse_context::make_default_construction(lex::src_tokens const &src_tokens, ast::typespec_view type)
{
	type = ast::remove_const_or_consteval(type);

	if (type.is<ast::ts_tuple>())
	{
		return make_tuple_default_construction(src_tokens, type, *this);
	}
	else if (type.is<ast::ts_array>())
	{
		return make_array_default_construction(src_tokens, type, *this);
	}
	else if (type.is<ast::ts_optional>())
	{
		return make_optional_default_construction(src_tokens, type, *this);
	}
	else if (type.is<ast::ts_array_slice>())
	{
		return make_builtin_default_construction(src_tokens, type, *this);
	}
	else if (type.is<ast::ts_base_type>())
	{
		auto const info = type.get<ast::ts_base_type>().info;
		if (info->kind == ast::type_info::aggregate || info->kind == ast::type_info::forward_declaration)
		{
			return make_struct_default_construction(src_tokens, type, *this);
		}
		else
		{
			return make_builtin_default_construction(src_tokens, type, *this);
		}
	}
	else
	{
		bz_unreachable;
	}
}

static ast::expression make_tuple_copy_construction(
	ast::typespec_view tuple_type,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(tuple_type.is<ast::ts_tuple>());
	if (!context.is_copy_constructible(expr.src_tokens, tuple_type))
	{
		context.report_error(
			expr.src_tokens,
			bz::format("value of type '{}' is not copy constructible", tuple_type),
			tuple_type.get<ast::ts_tuple>().types
				.filter([&](auto const &elem_type) {
					return !context.is_copy_constructible(expr.src_tokens, elem_type);
				})
				.transform([&expr](auto const &elem_type) {
					return parse_context::make_note(
						expr.src_tokens,
						bz::format("element type '{}' is not copy constructible", elem_type)
					);
				})
				.collect()
		);
		return ast::make_error_expression(
			expr.src_tokens,
			ast::make_expr_aggregate_copy_construct(std::move(expr), ast::arena_vector<ast::expression>())
		);
	}

	auto const src_tokens = expr.src_tokens;
	ast::typespec type = tuple_type;
	auto elem_copy_exprs = tuple_type.get<ast::ts_tuple>().types
		.transform([&](auto const &elem_type) {
			return context.make_copy_construction(ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::lvalue_reference,
				elem_type,
				ast::make_expr_bitcode_value_reference(),
				ast::destruct_operation()
			));
		})
		.collect<ast::arena_vector>();
	return ast::make_dynamic_expression(
		src_tokens,
		ast::expression_type_kind::rvalue, std::move(type),
		ast::make_expr_aggregate_copy_construct(std::move(expr), std::move(elem_copy_exprs)),
		ast::destruct_operation()
	);
}

static ast::expression make_array_copy_construction(
	ast::typespec_view array_type,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(array_type.is<ast::ts_array>());
	if (!context.is_copy_constructible(expr.src_tokens, array_type))
	{
		context.report_error(
			expr.src_tokens,
			bz::format("value of type '{}' is not copy constructible", array_type),
			{
				context.make_note(
					expr.src_tokens,
					bz::format("array element type '{}' is not copy constructible", array_type.get<ast::ts_array>().elem_type)
				)
			}
		);
		return ast::make_error_expression(
			expr.src_tokens,
			ast::make_expr_array_copy_construct(std::move(expr), ast::expression())
		);
	}

	ast::typespec type = array_type;
	auto elem_copy_expr = context.make_copy_construction(ast::make_dynamic_expression(
		expr.src_tokens,
		ast::expression_type_kind::lvalue_reference,
		array_type.get<ast::ts_array>().elem_type,
		ast::make_expr_bitcode_value_reference(),
		ast::destruct_operation()
	));
	return ast::make_dynamic_expression(
		expr.src_tokens,
		ast::expression_type_kind::rvalue, std::move(type),
		ast::make_expr_array_copy_construct(std::move(expr), std::move(elem_copy_expr)),
		ast::destruct_operation()
	);
}

static ast::expression make_optional_copy_construction(
	ast::typespec_view optional_type,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(optional_type.is<ast::ts_optional>());
	bz_assert(!optional_type.is_optional_pointer_like());
	auto const value_type = optional_type.get<ast::ts_optional>();

	if (!context.is_copy_constructible(expr.src_tokens, value_type))
	{
		context.report_error(
			expr.src_tokens,
			bz::format("value of type '{}' is not copy constructible", optional_type),
			{
				context.make_note(
					expr.src_tokens,
					bz::format("optional value type '{}' is not copy constructible", value_type)
				)
			}
		);
		return ast::make_error_expression(
			expr.src_tokens,
			ast::make_expr_optional_copy_construct(std::move(expr), ast::expression())
		);
	}

	auto value_copy_expr = context.make_copy_construction(ast::make_dynamic_expression(
		expr.src_tokens,
		ast::expression_type_kind::lvalue_reference, value_type,
		ast::make_expr_bitcode_value_reference(),
		ast::destruct_operation()
	));
	ast::typespec type = optional_type;
	return ast::make_dynamic_expression(
		expr.src_tokens,
		ast::expression_type_kind::rvalue, std::move(type),
		ast::make_expr_optional_copy_construct(std::move(expr), std::move(value_copy_expr)),
		ast::destruct_operation()
	);
}

static ast::expression make_struct_copy_construction(
	ast::typespec_view struct_type,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(struct_type.is<ast::ts_base_type>());
	auto const info = struct_type.get<ast::ts_base_type>().info;

	if (info->kind == ast::type_info::forward_declaration)
	{
		context.report_error(
			expr.src_tokens,
			bz::format("cannot copy value of incomplete type '{}'", struct_type)
		);
		auto const src_tokens = expr.src_tokens;
		return ast::make_error_expression(
			src_tokens,
			ast::make_expr_aggregate_copy_construct(std::move(expr), ast::arena_vector<ast::expression>())
		);
	}
	else if (!info->is_copy_constructible())
	{
		context.report_error(
			expr.src_tokens,
			bz::format("value of type '{}' is not copy constructible", struct_type),
			info->member_variables
				.filter([&](auto const &member) {
					return !context.is_copy_constructible(expr.src_tokens, member->get_type());
				})
				.transform([](auto const &member) {
					return parse_context::make_note(
						member->src_tokens,
						bz::format(
							"member '{}' of type '{}' is not copy constructible",
							member->get_id().format_as_unqualified(), member->get_type())
					);
				})
				.collect()
		);
		auto const src_tokens = expr.src_tokens;
		return ast::make_error_expression(
			src_tokens,
			ast::make_expr_aggregate_copy_construct(std::move(expr), ast::arena_vector<ast::expression>())
		);
	}

	if (info->copy_constructor != nullptr)
	{
		auto const src_tokens = expr.src_tokens;
		ast::arena_vector<ast::expression> args;
		args.push_back(std::move(expr));
		return make_expr_function_call_from_body(
			src_tokens,
			&info->copy_constructor->body,
			std::move(args),
			context
		);
	}
	else
	{
		bz_assert(info->default_copy_constructor != nullptr);
		auto const src_tokens = expr.src_tokens;
		ast::typespec type = struct_type;
		auto elem_copy_exprs = info->member_variables
			.transform([](auto const member_ptr) -> auto const & {
				return member_ptr->get_type();
			})
			.transform([&](auto const &member_type) {
				return context.make_copy_construction(ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::lvalue_reference,
					member_type,
					ast::make_expr_bitcode_value_reference(),
					ast::destruct_operation()
				));
			})
			.collect<ast::arena_vector>();
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::rvalue, std::move(type),
			ast::make_expr_aggregate_copy_construct(std::move(expr), std::move(elem_copy_exprs)),
			ast::destruct_operation()
		);
	}
}

ast::expression parse_context::make_copy_construction(ast::expression expr)
{
	auto const type = ast::remove_const_or_consteval(expr.get_expr_type());

	if (this->is_trivially_copy_constructible(expr.src_tokens, type))
	{
		auto const src_tokens = expr.src_tokens;
		ast::typespec result_type = type;
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::rvalue, std::move(result_type),
			ast::make_expr_trivial_copy_construct(std::move(expr)),
			ast::destruct_operation()
		);
	}
	else if (type.is<ast::ts_tuple>())
	{
		return make_tuple_copy_construction(type, std::move(expr), *this);
	}
	else if (type.is<ast::ts_array>())
	{
		return make_array_copy_construction(type, std::move(expr), *this);
	}
	else if (type.is<ast::ts_optional>())
	{
		return make_optional_copy_construction(type, std::move(expr), *this);
	}
	else if (type.is<ast::ts_base_type>())
	{
		auto const info = type.get<ast::ts_base_type>().info;
		bz_assert(info->kind == ast::type_info::aggregate || info->kind == ast::type_info::forward_declaration);
		return make_struct_copy_construction(type, std::move(expr), *this);
	}
	else
	{
		bz_unreachable;
	}
}

static ast::expression make_tuple_move_construction(
	ast::typespec_view tuple_type,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(tuple_type.is<ast::ts_tuple>());
	if (!context.is_move_constructible(expr.src_tokens, tuple_type))
	{
		context.report_error(
			expr.src_tokens,
			bz::format("value of type '{}' is not move constructible", tuple_type),
			tuple_type.get<ast::ts_tuple>().types
				.filter([&](auto const &elem_type) {
					return !context.is_move_constructible(expr.src_tokens, elem_type);
				})
				.transform([&expr](auto const &elem_type) {
					return parse_context::make_note(
						expr.src_tokens,
						bz::format("element type '{}' is not move constructible", elem_type)
					);
				})
				.collect()
		);
		return ast::make_error_expression(
			expr.src_tokens,
			ast::make_expr_aggregate_move_construct(std::move(expr), ast::arena_vector<ast::expression>())
		);
	}

	auto const src_tokens = expr.src_tokens;
	ast::typespec type = tuple_type;
	auto elem_move_exprs = tuple_type.get<ast::ts_tuple>().types
		.transform([&](auto const &elem_type) {
			return context.make_move_construction(ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::rvalue_reference,
				elem_type,
				ast::make_expr_bitcode_value_reference(),
				ast::destruct_operation()
			));
		})
		.collect<ast::arena_vector>();
	return ast::make_dynamic_expression(
		src_tokens,
		ast::expression_type_kind::rvalue, std::move(type),
		ast::make_expr_aggregate_move_construct(std::move(expr), std::move(elem_move_exprs)),
		ast::destruct_operation()
	);
}

static ast::expression make_array_move_construction(
	ast::typespec_view array_type,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(array_type.is<ast::ts_array>());
	if (!context.is_move_constructible(expr.src_tokens, array_type))
	{
		context.report_error(
			expr.src_tokens,
			bz::format("value of type '{}' is not move constructible", array_type),
			{
				context.make_note(
					expr.src_tokens,
					bz::format("array element type '{}' is not move constructible", array_type.get<ast::ts_array>().elem_type)
				)
			}
		);
		return ast::make_error_expression(
			expr.src_tokens,
			ast::make_expr_array_move_construct(std::move(expr), ast::expression())
		);
	}

	ast::typespec type = array_type;
	auto elem_move_expr = context.make_move_construction(ast::make_dynamic_expression(
		expr.src_tokens,
		ast::expression_type_kind::rvalue_reference,
		array_type.get<ast::ts_array>().elem_type,
		ast::make_expr_bitcode_value_reference(),
		ast::destruct_operation()
	));
	return ast::make_dynamic_expression(
		expr.src_tokens,
		ast::expression_type_kind::rvalue, std::move(type),
		ast::make_expr_array_move_construct(std::move(expr), std::move(elem_move_expr)),
		ast::destruct_operation()
	);
}

static ast::expression make_optional_move_construction(
	ast::typespec_view optional_type,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(optional_type.is<ast::ts_optional>());
	bz_assert(!optional_type.is_optional_pointer_like());
	auto const value_type = optional_type.get<ast::ts_optional>();

	if (!context.is_move_constructible(expr.src_tokens, value_type))
	{
		context.report_error(
			expr.src_tokens,
			bz::format("value of type '{}' is not move constructible", optional_type),
			{
				context.make_note(
					expr.src_tokens,
					bz::format("optional value type '{}' is not move constructible", value_type)
				)
			}
		);
		return ast::make_error_expression(
			expr.src_tokens,
			ast::make_expr_optional_move_construct(std::move(expr), ast::expression())
		);
	}

	auto value_move_expr = context.make_move_construction(ast::make_dynamic_expression(
		expr.src_tokens,
		ast::expression_type_kind::rvalue_reference, value_type,
		ast::make_expr_bitcode_value_reference(),
		ast::destruct_operation()
	));
	ast::typespec type = optional_type;
	return ast::make_dynamic_expression(
		expr.src_tokens,
		ast::expression_type_kind::rvalue, std::move(type),
		ast::make_expr_optional_move_construct(std::move(expr), std::move(value_move_expr)),
		ast::destruct_operation()
	);
}

static ast::expression make_struct_move_construction(
	ast::typespec_view struct_type,
	ast::expression expr,
	parse_context &context
)
{
	bz_assert(struct_type.is<ast::ts_base_type>());
	auto const info = struct_type.get<ast::ts_base_type>().info;

	if (info->kind == ast::type_info::forward_declaration)
	{
		context.report_error(
			expr.src_tokens,
			bz::format("cannot move value of incomplete type '{}'", struct_type)
		);
		auto const src_tokens = expr.src_tokens;
		return ast::make_error_expression(
			src_tokens,
			ast::make_expr_aggregate_move_construct(std::move(expr), ast::arena_vector<ast::expression>())
		);
	}
	else if (!info->is_move_constructible())
	{
		context.report_error(
			expr.src_tokens,
			bz::format("value of type '{}' is not move constructible", struct_type),
			info->member_variables
				.filter([&](auto const &member) {
					return !context.is_move_constructible(expr.src_tokens, member->get_type());
				})
				.transform([](auto const &member) {
					return parse_context::make_note(
						member->src_tokens,
						bz::format(
							"member '{}' of type '{}' is not move constructible",
							member->get_id().format_as_unqualified(), member->get_type())
					);
				})
				.collect()
		);
		auto const src_tokens = expr.src_tokens;
		return ast::make_error_expression(
			src_tokens,
			ast::make_expr_aggregate_move_construct(std::move(expr), ast::arena_vector<ast::expression>())
		);
	}

	if (info->move_constructor != nullptr)
	{
		context.add_self_move_destruction(expr);
		auto const src_tokens = expr.src_tokens;
		ast::arena_vector<ast::expression> args;
		args.push_back(std::move(expr));
		return make_expr_function_call_from_body(
			src_tokens,
			&info->move_constructor->body,
			std::move(args),
			context
		);
	}
	else
	{
		bz_assert(info->default_move_constructor != nullptr);
		auto const src_tokens = expr.src_tokens;
		ast::typespec type = struct_type;
		auto elem_move_exprs = info->member_variables
			.transform([](auto const member_ptr) -> auto const & {
				return member_ptr->get_type();
			})
			.transform([&](auto const &member_type) {
				return context.make_move_construction(ast::make_dynamic_expression(
					src_tokens,
					ast::expression_type_kind::rvalue_reference,
					member_type,
					ast::make_expr_bitcode_value_reference(),
					ast::destruct_operation()
				));
			})
			.collect<ast::arena_vector>();
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::rvalue, std::move(type),
			ast::make_expr_aggregate_move_construct(std::move(expr), std::move(elem_move_exprs)),
			ast::destruct_operation()
		);
	}
}

ast::expression parse_context::make_move_construction(ast::expression expr)
{
	auto const [expr_type, expr_type_kind] = expr.get_expr_type_and_kind();
	auto const type = ast::remove_const_or_consteval(expr_type);

	if (expr_type_kind == ast::expression_type_kind::moved_lvalue && !this->in_unevaluated_context)
	{
		bz_assert(expr.get_expr().is<ast::expr_unary_op>());
		auto const op = expr.get_expr().get<ast::expr_unary_op>().op;
		bz_assert(op == lex::token::kw_move || op == lex::token::kw_unsafe_move);
		if (op == lex::token::kw_move)
		{
			bz_assert(expr.get_expr().get<ast::expr_unary_op>().expr.get_expr().is<ast::expr_variable_name>());
			auto const decl = expr.get_expr().get<ast::expr_unary_op>().expr.get_expr().get<ast::expr_variable_name>().decl;
			this->register_move_construction(decl);
			bz_assert(expr.is_dynamic());
			expr.get_dynamic().destruct_op.move_destructed_decl = decl;
		}
	}

	if (this->is_trivially_relocatable(expr.src_tokens, type))
	{
		bz_assert(this->is_trivially_move_destructible(expr.src_tokens, type));
		auto const src_tokens = expr.src_tokens;
		ast::typespec result_type = type;
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::rvalue, std::move(result_type),
			ast::make_expr_trivial_relocate(std::move(expr)),
			ast::destruct_operation()
		);
	}
	else if (type.is<ast::ts_tuple>())
	{
		return make_tuple_move_construction(type, std::move(expr), *this);
	}
	else if (type.is<ast::ts_array>())
	{
		return make_array_move_construction(type, std::move(expr), *this);
	}
	else if (type.is<ast::ts_optional>())
	{
		return make_optional_move_construction(type, std::move(expr), *this);
	}
	else if (type.is<ast::ts_base_type>())
	{
		auto const info = type.get<ast::ts_base_type>().info;
		bz_assert(!info->is_trivially_relocatable());
		bz_assert(info->kind == ast::type_info::aggregate || info->kind == ast::type_info::forward_declaration);
		return make_struct_move_construction(type, std::move(expr), *this);
	}
	else
	{
		bz_unreachable;
	}
}

static ast::expression make_tuple_assignment(
	lex::src_tokens const &src_tokens,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
)
{
	auto const lhs_type = lhs.get_expr_type();
	auto const rhs_type = ast::remove_const_or_consteval(rhs.get_expr_type());
	bz_assert(lhs_type.is<ast::ts_tuple>());
	bz_assert(rhs_type.is<ast::ts_tuple>());

	auto const &lhs_tuple_type = lhs_type.get<ast::ts_tuple>();
	auto const &rhs_tuple_type = rhs_type.get<ast::ts_tuple>();

	if (lhs_tuple_type.types.size() != rhs_tuple_type.types.size())
	{
		context.report_error(
			src_tokens,
			bz::format(
				"tuple types '{}' and '{}' have a different number of elements ({} and {}) in assignment",
				lhs_type, rhs_type, lhs_tuple_type.types.size(), rhs_tuple_type.types.size()
			)
		);
		return ast::make_error_expression(
			src_tokens,
			ast::make_expr_binary_op(lex::token::assign, std::move(lhs), std::move(rhs))
		);
	}

	bz_assert(lhs.get_expr_type_and_kind().second == ast::expression_type_kind::lvalue_reference);
	auto const [rhs_type_with_const, rhs_expr_type_kind] = rhs.get_expr_type_and_kind();
	auto const rhs_elem_expr_type_kind = rhs_expr_type_kind == ast::expression_type_kind::lvalue_reference
		? ast::expression_type_kind::lvalue_reference
		: ast::expression_type_kind::rvalue_reference;
	auto assign_exprs = bz::iota(0, lhs_tuple_type.types.size())
		.transform([&, &rhs_type_with_const = rhs_type_with_const](auto const i) {
			ast::typespec lhs_elem_type = lhs_tuple_type.types[i];
			ast::typespec rhs_elem_type = rhs_tuple_type.types[i];

			if (rhs_type_with_const.is<ast::ts_const>())
			{
				rhs_elem_type.add_layer<ast::ts_const>();
			}

			return context.make_binary_operator_expression(
				src_tokens,
				lex::token::assign,
				ast::make_dynamic_expression(
					lhs.src_tokens,
					ast::expression_type_kind::lvalue_reference, std::move(lhs_elem_type),
					ast::make_expr_bitcode_value_reference(1),
					ast::destruct_operation()
				),
				ast::make_dynamic_expression(
					rhs.src_tokens,
					rhs_elem_expr_type_kind, std::move(rhs_elem_type),
					ast::make_expr_bitcode_value_reference(0),
					ast::destruct_operation()
				)
			);
		})
		.collect<ast::arena_vector>();

	ast::typespec result_type = lhs_type;
	return ast::make_dynamic_expression(
		src_tokens,
		ast::expression_type_kind::lvalue_reference, std::move(result_type),
		ast::make_expr_aggregate_assign(std::move(lhs), std::move(rhs), std::move(assign_exprs)),
		ast::destruct_operation()
	);
}

static ast::expression make_array_assignment(
	lex::src_tokens const &src_tokens,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
)
{
	auto const lhs_type = lhs.get_expr_type();
	auto const rhs_type = ast::remove_const_or_consteval(rhs.get_expr_type());
	bz_assert(lhs_type.is<ast::ts_array>());
	bz_assert(rhs_type.is<ast::ts_array>());

	auto const &lhs_array_type = lhs_type.get<ast::ts_array>();
	auto const &rhs_array_type = rhs_type.get<ast::ts_array>();

	if (lhs_array_type.size != rhs_array_type.size)
	{
		context.report_error(
			src_tokens,
			bz::format(
				"array types '{}' and '{}' have different sizes ({} and {}) in assignment",
				lhs_type, rhs_type, lhs_array_type.size, rhs_array_type.size
			)
		);
		return ast::make_error_expression(
			src_tokens,
			ast::make_expr_binary_op(lex::token::assign, std::move(lhs), std::move(rhs))
		);
	}

	bz_assert(lhs.get_expr_type_and_kind().second == ast::expression_type_kind::lvalue_reference);
	auto const [rhs_type_with_const, rhs_expr_type_kind] = rhs.get_expr_type_and_kind();
	auto const rhs_elem_expr_type_kind = rhs_expr_type_kind == ast::expression_type_kind::lvalue_reference
		? ast::expression_type_kind::lvalue_reference
		: ast::expression_type_kind::rvalue_reference;
	ast::typespec lhs_elem_type = lhs_array_type.elem_type;
	ast::typespec rhs_elem_type = rhs_array_type.elem_type;

	if (rhs_type_with_const.is<ast::ts_const>())
	{
		rhs_elem_type.add_layer<ast::ts_const>();
	}

	auto assign_expr = context.make_binary_operator_expression(
		src_tokens,
		lex::token::assign,
		ast::make_dynamic_expression(
			lhs.src_tokens,
			ast::expression_type_kind::lvalue_reference, std::move(lhs_elem_type),
			ast::make_expr_bitcode_value_reference(1),
			ast::destruct_operation()
		),
		ast::make_dynamic_expression(
			rhs.src_tokens,
			rhs_elem_expr_type_kind, std::move(rhs_elem_type),
			ast::make_expr_bitcode_value_reference(0),
			ast::destruct_operation()
		)
	);

	ast::typespec result_type = lhs_type;
	return ast::make_dynamic_expression(
		src_tokens,
		ast::expression_type_kind::lvalue_reference, std::move(result_type),
		ast::make_expr_array_assign(std::move(lhs), std::move(rhs), std::move(assign_expr)),
		ast::destruct_operation()
	);
}

static ast::expression make_optional_assignment(
	lex::src_tokens const &src_tokens,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
)
{
	auto const lhs_type = lhs.get_expr_type();
	auto const rhs_type = ast::remove_const_or_consteval(rhs.get_expr_type());
	bz_assert(lhs_type.is<ast::ts_optional>());
	bz_assert(rhs_type.is<ast::ts_optional>());

	bz_assert(lhs.get_expr_type_and_kind().second == ast::expression_type_kind::lvalue_reference);
	auto const [rhs_type_with_const, rhs_expr_type_kind] = rhs.get_expr_type_and_kind();
	auto const lhs_value_type = lhs_type.get<ast::ts_optional>();
	ast::typespec rhs_value_type = rhs_type.get<ast::ts_optional>();

	if (lhs_type.is_optional_reference())
	{
		bz_assert(lhs_type != ast::remove_const_or_consteval(rhs_type));
		context.report_error(
			src_tokens,
			bz::format(
				"mismatched reference types '{}' and '{}' in assignment to an optional reference value of type '{}'",
				lhs_value_type, rhs_value_type, lhs_type
			)
		);
		return ast::make_error_expression(
			src_tokens,
			ast::make_expr_binary_op(lex::token::assign, std::move(lhs), std::move(rhs))
		);
	}

	auto const rhs_value_expr_type_kind = rhs_expr_type_kind == ast::expression_type_kind::lvalue_reference
		? ast::expression_type_kind::lvalue_reference
		: ast::expression_type_kind::rvalue_reference;

	if (rhs_type_with_const.is<ast::ts_const>())
	{
		rhs_value_type.add_layer<ast::ts_const>();
	}

	auto value_assign_expr = context.make_binary_operator_expression(
		src_tokens,
		lex::token::assign,
		ast::make_dynamic_expression(
			lhs.src_tokens,
			ast::expression_type_kind::lvalue_reference, lhs_value_type,
			ast::make_expr_bitcode_value_reference(1),
			ast::destruct_operation()
		),
		ast::make_dynamic_expression(
			rhs.src_tokens,
			rhs_value_expr_type_kind, rhs_value_type,
			ast::make_expr_bitcode_value_reference(0),
			ast::destruct_operation()
		)
	);
	auto value_construct_expr = [&]() -> ast::expression {
		if (lhs_value_type == rhs_value_type)
		{
			auto rhs_ref = ast::make_dynamic_expression(
				rhs.src_tokens,
				rhs_value_expr_type_kind, rhs_value_type,
				ast::make_expr_bitcode_value_reference(),
				ast::destruct_operation()
			);
			return rhs_value_expr_type_kind == ast::expression_type_kind::lvalue_reference
				? context.make_copy_construction(std::move(rhs_ref))
				: context.make_move_construction(std::move(rhs_ref));
		}
		else
		{
			auto rhs_ref = ast::make_dynamic_expression(
				rhs.src_tokens,
				rhs_value_expr_type_kind, rhs_value_type,
				ast::make_expr_bitcode_value_reference(),
				ast::destruct_operation()
			);
			bz_assert(ast::is_complete(lhs_value_type));
			ast::typespec lhs_value_type_copy = lhs_value_type;
			resolve::match_expression_to_type(rhs_ref, lhs_value_type_copy, context);
			return rhs_ref;
		}
	}();
	auto value_destruct_expr = make_destruct_expression(
		lhs_value_type,
		ast::make_dynamic_expression(
			lhs.src_tokens,
			ast::expression_type_kind::lvalue_reference, lhs_value_type,
			ast::make_expr_bitcode_value_reference(),
			ast::destruct_operation()
		),
		context
	);

	ast::typespec result_type = lhs_type;
	return ast::make_dynamic_expression(
		src_tokens,
		ast::expression_type_kind::lvalue_reference, std::move(result_type),
		ast::make_expr_optional_assign(
			std::move(lhs),
			std::move(rhs),
			std::move(value_assign_expr),
			std::move(value_construct_expr),
			std::move(value_destruct_expr)
		),
		ast::destruct_operation()
	);
}

static ast::expression make_optional_null_assignment(
	lex::src_tokens const &src_tokens,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
)
{
	auto const lhs_type = lhs.get_expr_type();
	bz_assert(lhs_type.is<ast::ts_optional>());

	bz_assert(lhs.get_expr_type_and_kind().second == ast::expression_type_kind::lvalue_reference);
	auto const lhs_value_type = lhs_type.get<ast::ts_optional>();

	auto value_destruct_expr = make_destruct_expression(
		lhs_value_type,
		ast::make_dynamic_expression(
			lhs.src_tokens,
			ast::expression_type_kind::lvalue_reference, lhs_value_type,
			ast::make_expr_bitcode_value_reference(),
			ast::destruct_operation()
		),
		context
	);

	ast::typespec result_type = lhs_type;
	return ast::make_dynamic_expression(
		src_tokens,
		ast::expression_type_kind::lvalue_reference, std::move(result_type),
		ast::make_expr_optional_null_assign(
			std::move(lhs),
			std::move(rhs),
			std::move(value_destruct_expr)
		),
		ast::destruct_operation()
	);
}

static ast::expression make_optional_value_assignment(
	lex::src_tokens const &src_tokens,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
)
{
	auto const lhs_type = lhs.get_expr_type();
	bz_assert(lhs_type.is<ast::ts_optional>());

	bz_assert(lhs.get_expr_type_and_kind().second == ast::expression_type_kind::lvalue_reference);
	auto const lhs_value_type = lhs_type.get<ast::ts_optional>();
	auto const [rhs_value_type, rhs_expr_type_kind] = rhs.get_expr_type_and_kind();

	if (lhs_type.is_optional_reference())
	{
		if (rhs_expr_type_kind != ast::expression_type_kind::lvalue_reference)
		{
			context.report_error(
				src_tokens,
				bz::format(
					"invalid assignment from an rvalue of type '{}' to an optional reference value of type '{}'",
					rhs_value_type, lhs_type
				)
			);
			return ast::make_error_expression(
				src_tokens,
				ast::make_expr_binary_op(lex::token::assign, std::move(lhs), std::move(rhs))
			);
		}
		else if (lhs_value_type.get<ast::ts_lvalue_reference>() != rhs_value_type)
		{
			context.report_error(
				src_tokens,
				bz::format(
					"mismatched reference types '{}' and '&{}' in assignment to an optional reference value of type '{}'",
					lhs_value_type, rhs_value_type, lhs_type
				)
			);
			return ast::make_error_expression(
				src_tokens,
				ast::make_expr_binary_op(lex::token::assign, std::move(lhs), std::move(rhs))
			);
		}
		else
		{
			return ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::lvalue_reference, lhs_type,
				ast::make_expr_optional_reference_value_assign(std::move(lhs), std::move(rhs)),
				ast::destruct_operation()
			);
		}
	}

	auto const rhs_value_expr_type_kind = rhs_expr_type_kind == ast::expression_type_kind::lvalue_reference
		? ast::expression_type_kind::lvalue_reference
		: ast::expression_type_kind::rvalue_reference;

	auto value_assign_expr = context.make_binary_operator_expression(
		src_tokens,
		lex::token::assign,
		ast::make_dynamic_expression(
			lhs.src_tokens,
			ast::expression_type_kind::lvalue_reference, lhs_value_type,
			ast::make_expr_bitcode_value_reference(1),
			ast::destruct_operation()
		),
		ast::make_dynamic_expression(
			rhs.src_tokens,
			rhs_value_expr_type_kind, rhs_value_type,
			ast::make_expr_bitcode_value_reference(0),
			ast::destruct_operation()
		)
	);
	auto value_construct_expr = [&, &rhs_value_type = rhs_value_type]() -> ast::expression {
		if (lhs_value_type == rhs_value_type)
		{
			auto rhs_ref = ast::make_dynamic_expression(
				rhs.src_tokens,
				rhs_value_expr_type_kind, rhs_value_type,
				ast::make_expr_bitcode_value_reference(),
				ast::destruct_operation()
			);
			return rhs_value_expr_type_kind == ast::expression_type_kind::lvalue_reference
				? context.make_copy_construction(std::move(rhs_ref))
				: context.make_move_construction(std::move(rhs_ref));
		}
		else
		{
			auto rhs_ref = ast::make_dynamic_expression(
				rhs.src_tokens,
				rhs_value_expr_type_kind, rhs_value_type,
				ast::make_expr_bitcode_value_reference(),
				ast::destruct_operation()
			);
			bz_assert(ast::is_complete(lhs_value_type));
			ast::typespec lhs_value_type_copy = lhs_value_type;
			resolve::match_expression_to_type(rhs_ref, lhs_value_type_copy, context);
			return rhs_ref;
		}
	}();

	ast::typespec result_type = lhs_type;
	return ast::make_dynamic_expression(
		src_tokens,
		ast::expression_type_kind::lvalue_reference, std::move(result_type),
		ast::make_expr_optional_value_assign(
			std::move(lhs),
			std::move(rhs),
			std::move(value_assign_expr),
			std::move(value_construct_expr)
		),
		ast::destruct_operation()
	);
}

static ast::expression make_struct_assignment(
	lex::src_tokens const &src_tokens,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
)
{
	auto const type = lhs.get_expr_type();
	bz_assert(type.is<ast::ts_base_type>());

	bz_assert(lhs.get_expr_type_and_kind().second == ast::expression_type_kind::lvalue_reference);
	auto const [rhs_type_with_const, rhs_expr_type_kind] = rhs.get_expr_type_and_kind();
	bz_assert(
		rhs_expr_type_kind == ast::expression_type_kind::lvalue_reference
		|| rhs_expr_type_kind == ast::expression_type_kind::rvalue
	);

	auto lhs_destruct_expr = make_destruct_expression(
		type,
		ast::make_dynamic_expression(
			lhs.src_tokens,
			ast::expression_type_kind::lvalue_reference, type,
			ast::make_expr_bitcode_value_reference(),
			ast::destruct_operation()
		),
		context
	);

	auto const rhs_value_ref_type_kind = rhs_expr_type_kind == ast::expression_type_kind::lvalue_reference
		? ast::expression_type_kind::lvalue_reference
		: ast::expression_type_kind::rvalue_reference;
	auto rhs_value_ref = ast::make_dynamic_expression(
		rhs.src_tokens,
		rhs_value_ref_type_kind, rhs_type_with_const,
		ast::make_expr_bitcode_value_reference(),
		ast::destruct_operation()
	);
	auto rhs_copy_expr = rhs_expr_type_kind == ast::expression_type_kind::lvalue_reference
		? context.make_copy_construction(std::move(rhs_value_ref))
		: context.make_move_construction(std::move(rhs_value_ref));

	ast::typespec result_type = type;
	return ast::make_dynamic_expression(
		src_tokens,
		ast::expression_type_kind::lvalue_reference, std::move(result_type),
		ast::make_expr_base_type_assign(std::move(lhs), std::move(rhs), std::move(lhs_destruct_expr), std::move(rhs_copy_expr)),
		ast::destruct_operation()
	);
}

ast::expression parse_context::make_default_assignment(lex::src_tokens const &src_tokens, ast::expression lhs, ast::expression rhs)
{
	auto const lhs_type = ast::remove_const_or_consteval(lhs.get_expr_type());
	auto const rhs_type = ast::remove_const_or_consteval(rhs.get_expr_type());
	auto const are_types_equal = lhs_type == rhs_type;

	if (are_types_equal && this->is_trivial(src_tokens, lhs_type))
	{
		ast::typespec result_type = lhs_type;
		this->add_self_destruction(rhs);
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::lvalue_reference, std::move(result_type),
			ast::make_expr_trivial_assign(std::move(lhs), std::move(rhs)),
			ast::destruct_operation()
		);
	}
	else if (lhs_type.is<ast::ts_tuple>() && rhs_type.is<ast::ts_tuple>())
	{
		return make_tuple_assignment(src_tokens, std::move(lhs), std::move(rhs), *this);
	}
	else if (lhs_type.is<ast::ts_array>() && rhs_type.is<ast::ts_array>())
	{
		return make_array_assignment(src_tokens, std::move(lhs), std::move(rhs), *this);
	}
	else if (lhs_type.is<ast::ts_optional>() && rhs_type.is<ast::ts_optional>())
	{
		return make_optional_assignment(src_tokens, std::move(lhs), std::move(rhs), *this);
	}
	else if (lhs_type.is<ast::ts_optional>())
	{
		if (rhs_type.is<ast::ts_base_type>() && rhs_type.get<ast::ts_base_type>().info->kind == ast::type_info::null_t_)
		{
			return make_optional_null_assignment(src_tokens, std::move(lhs), std::move(rhs), *this);
		}
		else
		{
			return make_optional_value_assignment(src_tokens, std::move(lhs), std::move(rhs), *this);
		}
	}
	else if (!are_types_equal)
	{
		this->report_error(src_tokens, bz::format("invalid assignment with types '{}' and '{}'", lhs_type, rhs_type));
		return ast::make_error_expression(
			src_tokens,
			ast::make_expr_binary_op(lex::token::assign, std::move(lhs), std::move(rhs))
		);
	}
	else if (lhs_type.is<ast::ts_base_type>())
	{
		auto const info = lhs_type.get<ast::ts_base_type>().info;
		if (info->kind == ast::type_info::forward_declaration)
		{
			this->report_error(src_tokens, bz::format("invalid assignment of incomplete type '{}'", lhs_type));
			return ast::make_error_expression(
				src_tokens,
				ast::make_expr_binary_op(lex::token::assign, std::move(lhs), std::move(rhs))
			);
		}
		else
		{
			bz_assert(info->kind == ast::type_info::aggregate);
			return make_struct_assignment(src_tokens, std::move(lhs), std::move(rhs), *this);
		}
	}
	else
	{
		bz_unreachable;
	}
}

static ast::expression make_tuple_swap(
	lex::src_tokens const &src_tokens,
	ast::typespec_view type,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
)
{
	bz_assert(type.is<ast::ts_tuple>());
	auto const types = type.get<ast::ts_tuple>().types.as_array_view();

	auto swap_exprs = types
		.transform([&](auto const &elem_type) {
			return make_swap_expression(
				src_tokens,
				elem_type,
				ast::make_dynamic_expression(
					lhs.src_tokens,
					ast::expression_type_kind::lvalue_reference, elem_type,
					ast::make_expr_bitcode_value_reference(1),
					ast::destruct_operation()
				),
				ast::make_dynamic_expression(
					lhs.src_tokens,
					ast::expression_type_kind::lvalue_reference, elem_type,
					ast::make_expr_bitcode_value_reference(0),
					ast::destruct_operation()
				),
				context
			);
		})
		.collect<ast::arena_vector>();

	return ast::make_dynamic_expression(
		src_tokens,
		ast::expression_type_kind::none, ast::make_void_typespec(nullptr),
		ast::make_expr_aggregate_swap(std::move(lhs), std::move(rhs), std::move(swap_exprs)),
		ast::destruct_operation()
	);
}

static ast::expression make_array_swap(
	lex::src_tokens const &src_tokens,
	ast::typespec_view type,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
)
{
	bz_assert(type.is<ast::ts_array>());
	auto const elem_type = type.get<ast::ts_array>().elem_type.as_typespec_view();

	auto swap_expr = make_swap_expression(
		src_tokens,
		elem_type,
		ast::make_dynamic_expression(
			lhs.src_tokens,
			ast::expression_type_kind::lvalue_reference, elem_type,
			ast::make_expr_bitcode_value_reference(1),
			ast::destruct_operation()
		),
		ast::make_dynamic_expression(
			lhs.src_tokens,
			ast::expression_type_kind::lvalue_reference, elem_type,
			ast::make_expr_bitcode_value_reference(0),
			ast::destruct_operation()
		),
		context
	);

	return ast::make_dynamic_expression(
		src_tokens,
		ast::expression_type_kind::none, ast::make_void_typespec(nullptr),
		ast::make_expr_array_swap(std::move(lhs), std::move(rhs), std::move(swap_expr)),
		ast::destruct_operation()
	);
}

static ast::expression make_optional_swap(
	lex::src_tokens const &src_tokens,
	ast::typespec_view type,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
)
{
	bz_assert(type.is<ast::ts_optional>());
	bz_assert(!type.is_optional_pointer_like());
	auto const value_type = type.get<ast::ts_optional>();

	auto value_swap_expr = make_swap_expression(
		src_tokens,
		value_type,
		ast::make_dynamic_expression(
			lhs.src_tokens,
			ast::expression_type_kind::lvalue_reference, value_type,
			ast::make_expr_bitcode_value_reference(1),
			ast::destruct_operation()
		),
		ast::make_dynamic_expression(
			lhs.src_tokens,
			ast::expression_type_kind::lvalue_reference, value_type,
			ast::make_expr_bitcode_value_reference(0),
			ast::destruct_operation()
		),
		context
	);
	auto lhs_move_expr = context.make_move_construction(
		ast::make_dynamic_expression(
			lhs.src_tokens,
			ast::expression_type_kind::rvalue_reference, value_type,
			ast::make_expr_bitcode_value_reference(),
			ast::destruct_operation()
		)
	);
	auto rhs_move_expr = context.make_move_construction(
		ast::make_dynamic_expression(
			rhs.src_tokens,
			ast::expression_type_kind::rvalue_reference, value_type,
			ast::make_expr_bitcode_value_reference(),
			ast::destruct_operation()
		)
	);

	return ast::make_dynamic_expression(
		src_tokens,
		ast::expression_type_kind::none, ast::make_void_typespec(nullptr),
		ast::make_expr_optional_swap(
			std::move(lhs),
			std::move(rhs),
			std::move(value_swap_expr),
			std::move(lhs_move_expr),
			std::move(rhs_move_expr)
		),
		ast::destruct_operation()
	);
}

static ast::expression make_base_type_swap(
	lex::src_tokens const &src_tokens,
	ast::typespec_view type,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
)
{
	bz_assert(type.is<ast::ts_base_type>());

	auto lhs_move_expr = context.make_move_construction(
		ast::make_dynamic_expression(
			lhs.src_tokens,
			ast::expression_type_kind::rvalue_reference, type,
			ast::make_expr_bitcode_value_reference(),
			ast::destruct_operation()
		)
	);
	auto rhs_move_expr = context.make_move_construction(
		ast::make_dynamic_expression(
			rhs.src_tokens,
			ast::expression_type_kind::rvalue_reference, type,
			ast::make_expr_bitcode_value_reference(),
			ast::destruct_operation()
		)
	);
	auto temp_move_expr = context.make_move_construction(
		ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::rvalue_reference, type,
			ast::make_expr_bitcode_value_reference(),
			ast::destruct_operation()
		)
	);

	return ast::make_dynamic_expression(
		src_tokens,
		ast::expression_type_kind::none, ast::make_void_typespec(nullptr),
		ast::make_expr_base_type_swap(
			std::move(lhs),
			std::move(rhs),
			std::move(lhs_move_expr),
			std::move(rhs_move_expr),
			std::move(temp_move_expr)
		),
		ast::destruct_operation()
	);
}

static ast::expression make_swap_expression(
	lex::src_tokens const &src_tokens,
	ast::typespec_view type,
	ast::expression lhs,
	ast::expression rhs,
	parse_context &context
)
{
	if (!type.is<ast::ts_array>() && context.is_trivially_relocatable(src_tokens, type))
	{
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::none, ast::make_void_typespec(nullptr),
			ast::make_expr_trivial_swap(std::move(lhs), std::move(rhs)),
			ast::destruct_operation()
		);
	}
	else if (type.is<ast::ts_tuple>())
	{
		return make_tuple_swap(src_tokens, type, std::move(lhs), std::move(rhs), context);
	}
	else if (type.is<ast::ts_array>())
	{
		return make_array_swap(src_tokens, type, std::move(lhs), std::move(rhs), context);
	}
	else if (type.is<ast::ts_optional>())
	{
		return make_optional_swap(src_tokens, type, std::move(lhs), std::move(rhs), context);
	}
	else if (type.is<ast::ts_base_type>())
	{
		return make_base_type_swap(src_tokens, type, std::move(lhs), std::move(rhs), context);
	}
	else
	{
		bz_unreachable;
	}
}

static ast::expression make_optional_extract_value_expression(
	lex::src_tokens const &src_tokens,
	ast::expression optional_value,
	parse_context &context
)
{
	auto const value_type = optional_value.get_expr_type().get<ast::ts_optional>();
	if (value_type.is<ast::ts_lvalue_reference>())
	{
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::lvalue_reference, value_type.get<ast::ts_lvalue_reference>(),
			ast::make_expr_optional_extract_value(std::move(optional_value), ast::expression()),
			ast::destruct_operation()
		);
	}
	else
	{
		auto value_move_expr = context.make_move_construction(
			ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::rvalue_reference, value_type,
				ast::make_expr_bitcode_value_reference(),
				ast::destruct_operation()
			)
		);

		ast::typespec result_type = value_type;
		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::rvalue, std::move(result_type),
			ast::make_expr_optional_extract_value(std::move(optional_value), std::move(value_move_expr)),
			ast::destruct_operation()
		);
	}
}

static ast::expression make_array_value_init_expression(
	lex::src_tokens const &src_tokens,
	ast::typespec_view type,
	ast::expression value,
	parse_context &context
)
{
	auto const [value_type, value_kind] = value.get_expr_type_and_kind();

	auto copy_expr = context.make_copy_construction(ast::make_dynamic_expression(
		src_tokens,
		ast::expression_type_kind::lvalue_reference, value_type,
		ast::make_expr_bitcode_value_reference(),
		ast::destruct_operation()
	));

	return ast::make_dynamic_expression(
		src_tokens,
		ast::expression_type_kind::rvalue, type,
		ast::make_expr_array_value_init(type, std::move(value), std::move(copy_expr)),
		ast::destruct_operation()
	);
}

static ast::expression make_base_type_destruct_expression(
	ast::typespec_view type,
	ast::expression value,
	parse_context &context
)
{
	bz_assert(type.is<ast::ts_base_type>());
	auto const src_tokens = value.src_tokens;
	auto const info = type.get<ast::ts_base_type>().info;
	context.resolve_type_members(src_tokens, info);
	if (info->state < ast::resolve_state::members)
	{
		return ast::make_error_expression(
			src_tokens,
			ast::make_expr_base_type_destruct(std::move(value), ast::expression(), ast::arena_vector<ast::expression>())
		);
	}

	auto destruct_call = [&]() {
		if (info->destructor == nullptr)
		{
			return ast::expression();
		}

		auto const body = &info->destructor->body;
		auto args = ast::arena_vector<ast::expression>();
		args.push_back(ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::lvalue_reference, type,
			ast::make_expr_bitcode_value_reference(),
			ast::destruct_operation()
		));
		return make_expr_function_call_from_body(src_tokens, body, std::move(args), context);
	}();

	auto member_destruct_calls = info->member_variables
		.transform([&](auto const member) {
			auto const member_type = ast::remove_const_or_consteval(ast::remove_lvalue_reference(member->get_type()));
			auto value_ref = ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::lvalue_reference, member_type,
				ast::make_expr_bitcode_value_reference(),
				ast::destruct_operation()
			);
			return make_destruct_expression(ast::remove_const_or_consteval(member->get_type()), std::move(value_ref), context);
		})
		.collect<ast::arena_vector>();
	return ast::make_dynamic_expression(
		src_tokens,
		ast::expression_type_kind::none, ast::make_void_typespec(nullptr),
		ast::make_expr_base_type_destruct(std::move(value), std::move(destruct_call), std::move(member_destruct_calls)),
		ast::destruct_operation()
	);
}

static ast::expression make_tuple_destruct_expression(
	ast::typespec_view type,
	ast::expression value,
	parse_context &context
)
{
	bz_assert(type.is<ast::ts_tuple>());
	auto const src_tokens = value.src_tokens;
	auto elem_destruct_calls = type.get<ast::ts_tuple>().types
		.transform([&](auto const &elem_type) {
			auto const decayed_elem_type = ast::remove_const_or_consteval(ast::remove_lvalue_reference(elem_type));
			auto value_ref = ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::lvalue_reference, decayed_elem_type,
				ast::make_expr_bitcode_value_reference(),
				ast::destruct_operation()
			);
			return make_destruct_expression(ast::remove_const_or_consteval(elem_type), std::move(value_ref), context);
		})
		.collect<ast::arena_vector>();
	return ast::make_dynamic_expression(
		src_tokens,
		ast::expression_type_kind::none, ast::make_void_typespec(nullptr),
		ast::make_expr_aggregate_destruct(std::move(value), std::move(elem_destruct_calls)),
		ast::destruct_operation()
	);
}

static ast::expression make_array_destruct_expression(
	ast::typespec_view type,
	ast::expression value,
	parse_context &context
)
{
	bz_assert(type.is<ast::ts_array>());
	auto const src_tokens = value.src_tokens;
	auto const elem_type = type.get<ast::ts_array>().elem_type.as_typespec_view();
	auto value_ref = ast::make_dynamic_expression(
		src_tokens,
		ast::expression_type_kind::lvalue_reference, elem_type,
		ast::make_expr_bitcode_value_reference(),
		ast::destruct_operation()
	);
	auto elem_destruct_call = make_destruct_expression(elem_type, std::move(value_ref), context);
	return ast::make_dynamic_expression(
		src_tokens,
		ast::expression_type_kind::none, ast::make_void_typespec(nullptr),
		ast::make_expr_array_destruct(std::move(value), std::move(elem_destruct_call)),
		ast::destruct_operation()
	);
}

static ast::expression make_optional_destruct_expression(
	ast::typespec_view type,
	ast::expression value,
	parse_context &context
)
{
	bz_assert(type.is<ast::ts_optional>());
	bz_assert(!type.is_optional_pointer_like());
	auto const src_tokens = value.src_tokens;
	auto const value_type = type.get<ast::ts_optional>();
	auto value_ref = ast::make_dynamic_expression(
		src_tokens,
		ast::expression_type_kind::lvalue_reference, value_type,
		ast::make_expr_bitcode_value_reference(),
		ast::destruct_operation()
	);
	auto value_destruct_call = make_destruct_expression(value_type, std::move(value_ref), context);
	return ast::make_dynamic_expression(
		src_tokens,
		ast::expression_type_kind::none, ast::make_void_typespec(nullptr),
		ast::make_expr_optional_destruct(std::move(value), std::move(value_destruct_call)),
		ast::destruct_operation()
	);
}

static ast::expression make_destruct_expression(
	ast::typespec_view type,
	ast::expression value,
	parse_context &context
)
{
	if (context.is_trivially_destructible(value.src_tokens, type))
	{
		return ast::expression();
	}

	if (type.is<ast::ts_base_type>())
	{
		return make_base_type_destruct_expression(type, std::move(value), context);
	}
	else if (type.is<ast::ts_tuple>())
	{
		return make_tuple_destruct_expression(type, std::move(value), context);
	}
	else if (type.is<ast::ts_array>())
	{
		return make_array_destruct_expression(type, std::move(value), context);
	}
	else if (type.is<ast::ts_optional>())
	{
		return make_optional_destruct_expression(type, std::move(value), context);
	}
	else
	{
		bz_unreachable;
	}
}

static ast::expression make_base_type_move_destruct_expression(
	ast::typespec_view type,
	ast::expression value,
	parse_context &context
)
{
	bz_assert(type.is<ast::ts_base_type>());
	auto const src_tokens = value.src_tokens;
	auto const info = type.get<ast::ts_base_type>().info;
	context.resolve_type_members(src_tokens, info);
	if (info->state < ast::resolve_state::members)
	{
		return ast::make_error_expression(
			src_tokens,
			ast::make_expr_base_type_destruct(std::move(value), ast::expression(), ast::arena_vector<ast::expression>())
		);
	}

	auto destruct_call = [&]() {
		if (info->move_destructor == nullptr)
		{
			return ast::expression();
		}

		auto const body = &info->move_destructor->body;
		auto args = ast::arena_vector<ast::expression>();
		args.push_back(ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::rvalue_reference, type,
			ast::make_expr_bitcode_value_reference(),
			ast::destruct_operation()
		));
		return make_expr_function_call_from_body(src_tokens, body, std::move(args), context);
	}();

	auto member_destruct_calls = info->member_variables
		.transform([&](auto const member) {
			auto const member_type = ast::remove_const_or_consteval(ast::remove_lvalue_reference(member->get_type()));
			auto value_ref = ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::rvalue_reference, member_type,
				ast::make_expr_bitcode_value_reference(),
				ast::destruct_operation()
			);
			return make_move_destruct_expression(ast::remove_const_or_consteval(member->get_type()), std::move(value_ref), context);
		})
		.collect<ast::arena_vector>();
	return ast::make_dynamic_expression(
		src_tokens,
		ast::expression_type_kind::none, ast::make_void_typespec(nullptr),
		ast::make_expr_base_type_destruct(std::move(value), std::move(destruct_call), std::move(member_destruct_calls)),
		ast::destruct_operation()
	);
}

static ast::expression make_tuple_move_destruct_expression(
	ast::typespec_view type,
	ast::expression value,
	parse_context &context
)
{
	bz_assert(type.is<ast::ts_tuple>());
	auto const src_tokens = value.src_tokens;
	auto elem_destruct_calls = type.get<ast::ts_tuple>().types
		.transform([&](auto const &elem_type) {
			auto const decayed_elem_type = ast::remove_const_or_consteval(ast::remove_lvalue_reference(elem_type));
			auto value_ref = ast::make_dynamic_expression(
				src_tokens,
				ast::expression_type_kind::rvalue_reference, decayed_elem_type,
				ast::make_expr_bitcode_value_reference(),
				ast::destruct_operation()
			);
			return make_move_destruct_expression(ast::remove_const_or_consteval(elem_type), std::move(value_ref), context);
		})
		.collect<ast::arena_vector>();
	return ast::make_dynamic_expression(
		src_tokens,
		ast::expression_type_kind::none, ast::make_void_typespec(nullptr),
		ast::make_expr_aggregate_destruct(std::move(value), std::move(elem_destruct_calls)),
		ast::destruct_operation()
	);
}

static ast::expression make_array_move_destruct_expression(
	ast::typespec_view type,
	ast::expression value,
	parse_context &context
)
{
	bz_assert(type.is<ast::ts_array>());
	auto const src_tokens = value.src_tokens;
	auto const elem_type = type.get<ast::ts_array>().elem_type.as_typespec_view();
	auto value_ref = ast::make_dynamic_expression(
		src_tokens,
		ast::expression_type_kind::rvalue_reference, elem_type,
		ast::make_expr_bitcode_value_reference(),
		ast::destruct_operation()
	);
	auto elem_destruct_call = make_move_destruct_expression(elem_type, std::move(value_ref), context);
	return ast::make_dynamic_expression(
		src_tokens,
		ast::expression_type_kind::none, ast::make_void_typespec(nullptr),
		ast::make_expr_array_destruct(std::move(value), std::move(elem_destruct_call)),
		ast::destruct_operation()
	);
}

static ast::expression make_optional_move_destruct_expression(
	ast::typespec_view type,
	ast::expression value,
	parse_context &context
)
{
	bz_assert(type.is<ast::ts_optional>());
	bz_assert(!type.is_optional_pointer_like());
	auto const src_tokens = value.src_tokens;
	auto const value_type = type.get<ast::ts_optional>();
	auto value_ref = ast::make_dynamic_expression(
		src_tokens,
		ast::expression_type_kind::rvalue_reference, value_type,
		ast::make_expr_bitcode_value_reference(),
		ast::destruct_operation()
	);
	auto value_destruct_call = make_move_destruct_expression(value_type, std::move(value_ref), context);
	return ast::make_dynamic_expression(
		src_tokens,
		ast::expression_type_kind::none, ast::make_void_typespec(nullptr),
		ast::make_expr_optional_destruct(std::move(value), std::move(value_destruct_call)),
		ast::destruct_operation()
	);
}

static ast::expression make_move_destruct_expression(
	ast::typespec_view type,
	ast::expression value,
	parse_context &context
)
{
	if (context.is_trivially_move_destructible(value.src_tokens, type))
	{
		return ast::expression();
	}

	if (type.is<ast::ts_base_type>())
	{
		return make_base_type_move_destruct_expression(type, std::move(value), context);
	}
	else if (type.is<ast::ts_tuple>())
	{
		return make_tuple_move_destruct_expression(type, std::move(value), context);
	}
	else if (type.is<ast::ts_array>())
	{
		return make_array_move_destruct_expression(type, std::move(value), context);
	}
	else if (type.is<ast::ts_optional>())
	{
		return make_optional_move_destruct_expression(type, std::move(value), context);
	}
	else
	{
		bz_unreachable;
	}
}

void parse_context::add_self_destruction(ast::expression &expr)
{
	if (this->in_unevaluated_context || !expr.is_dynamic())
	{
		return;
	}
	else if (expr.is_tuple())
	{
		auto &tuple = expr.get_tuple();
		for (auto &elem : tuple.elems)
		{
			this->add_self_destruction(elem);
		}
	}
	else if (expr.is_if_expr())
	{
		auto &if_expr = expr.get_if_expr();
		this->add_self_destruction(if_expr.then_block);
		this->add_self_destruction(if_expr.else_block);
	}
	else if (expr.is_switch_expr())
	{
		auto &switch_expr = expr.get_switch_expr();
		for (auto &[_, case_expr] : switch_expr.cases)
		{
			this->add_self_destruction(case_expr);
		}
		this->add_self_destruction(switch_expr.default_case);
	}
	else if (
		auto const expr_kind = expr.get_expr_type_and_kind().second;
		expr_kind == ast::expression_type_kind::rvalue
		|| expr_kind == ast::expression_type_kind::rvalue_reference
	)
	{
		if (this->is_trivially_destructible(expr.src_tokens, expr.get_expr_type()))
		{
			expr.get_dynamic().destruct_op = ast::trivial_destruct_self();
		}
		else
		{
			auto const type = ast::remove_const_or_consteval(expr.get_expr_type());
			auto value_ref = ast::make_dynamic_expression(
				expr.src_tokens,
				ast::expression_type_kind::lvalue_reference, type,
				ast::make_expr_bitcode_value_reference(),
				ast::destruct_operation()
			);
			expr.get_dynamic().destruct_op = ast::destruct_self(make_destruct_expression(type, std::move(value_ref), *this));
		}
	}
}

void parse_context::add_self_move_destruction(ast::expression &expr)
{
	if (this->in_unevaluated_context || !expr.is_dynamic())
	{
		return;
	}
	else if (expr.is_tuple())
	{
		auto &tuple = expr.get_tuple();
		for (auto &elem : tuple.elems)
		{
			this->add_self_move_destruction(elem);
		}
	}
	else if (expr.is_if_expr())
	{
		auto &if_expr = expr.get_if_expr();
		this->add_self_move_destruction(if_expr.then_block);
		this->add_self_move_destruction(if_expr.else_block);
	}
	else if (expr.is_switch_expr())
	{
		auto &switch_expr = expr.get_switch_expr();
		for (auto &[_, case_expr] : switch_expr.cases)
		{
			this->add_self_move_destruction(case_expr);
		}
		this->add_self_move_destruction(switch_expr.default_case);
	}
	else if (!this->is_trivially_move_destructible(expr.src_tokens, expr.get_expr_type()))
	{
		auto const type = ast::remove_const_or_consteval(expr.get_expr_type());
		auto value_ref = ast::make_dynamic_expression(
			expr.src_tokens,
			ast::expression_type_kind::lvalue_reference, type,
			ast::make_expr_bitcode_value_reference(),
			ast::destruct_operation()
		);
		auto const decl = expr.get_dynamic().destruct_op.move_destructed_decl;
		expr.get_dynamic().destruct_op = ast::destruct_self(make_move_destruct_expression(type, std::move(value_ref), *this));
		bz_assert(decl == expr.get_dynamic().destruct_op.move_destructed_decl);
	}
	else
	{
		expr.get_dynamic().destruct_op = ast::trivial_destruct_self();
	}
}

static ast::expression make_variable_destruction_expression(ast::decl_variable *var_decl, parse_context &context)
{
	auto const type = ast::remove_const_or_consteval(var_decl->get_type());
	bz_assert(!context.is_trivially_destructible(var_decl->src_tokens, type));
	return make_destruct_expression(
		type,
		ast::make_dynamic_expression(
			var_decl->src_tokens,
			ast::expression_type_kind::lvalue_reference, ast::remove_lvalue_reference(type),
			ast::make_expr_variable_name(ast::identifier(), var_decl, 0, false),
			ast::destruct_operation()
		),
		context
	);
}

ast::destruct_operation parse_context::make_variable_destruction(ast::decl_variable *var_decl)
{
	auto result = ast::destruct_operation();
	result.emplace<ast::destruct_variable>(make_variable_destruction_expression(var_decl, *this));
	return result;
}

ast::destruct_operation parse_context::make_rvalue_array_destruction(lex::src_tokens const &src_tokens, ast::typespec_view type)
{
	bz_assert(type.is<ast::ts_array>());
	auto const elem_type = type.get<ast::ts_array>().elem_type.as_typespec_view();

	if (this->is_trivially_destructible(src_tokens, elem_type))
	{
		return ast::destruct_operation();
	}

	auto result = ast::destruct_operation();
	result.emplace<ast::destruct_rvalue_array>(make_destruct_expression(
		elem_type,
		ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::lvalue_reference, elem_type,
			ast::make_expr_bitcode_value_reference(),
			ast::destruct_operation()
		),
		*this
	));
	return result;
}

void parse_context::resolve_type(lex::src_tokens const &src_tokens, ast::type_info *info)
{
	if (info->state != ast::resolve_state::error && info->state < ast::resolve_state::all)
	{
		this->add_to_resolve_queue(src_tokens, *info);
		resolve::resolve_type_info(*info, *this);
		this->pop_resolve_queue();
	}
}

void parse_context::resolve_type_members(lex::src_tokens const &src_tokens, ast::type_info *info)
{
	if (info->state != ast::resolve_state::error && info->state < ast::resolve_state::members)
	{
		this->add_to_resolve_queue(src_tokens, *info);
		resolve::resolve_type_info_members(*info, *this);
		this->pop_resolve_queue();
	}
}

void parse_context::resolve_type(lex::src_tokens const &src_tokens, ast::decl_enum *decl)
{
	if (decl->state != ast::resolve_state::error && decl->state < ast::resolve_state::all)
	{
		this->add_to_resolve_queue(src_tokens, *decl);
		resolve::resolve_enum(*decl, *this);
		this->pop_resolve_queue();
	}
}

template<
	bool (ast::type_info::*base_type_property_func)(void) const,
	bool default_value, typename ...exception_types
>
static bool type_property_helper(lex::src_tokens const &src_tokens, ast::typespec_view ts, parse_context &context)
{
	ts = remove_const_or_consteval(ts);
	if ((ts.is<exception_types>() || ...))
	{
		return !default_value;
	}
	else if (ts.is<ast::ts_base_type>())
	{
		auto const info = ts.get<ast::ts_base_type>().info;
		context.resolve_type_members(src_tokens, info);
		return (ts.get<ast::ts_base_type>().info->*base_type_property_func)();
	}
	else if (ts.is<ast::ts_tuple>())
	{
		return ts.get<ast::ts_tuple>().types.is_all([&](auto const &type) {
			return type_property_helper<
				base_type_property_func,
				default_value,
				exception_types...
			>(src_tokens, type, context);
		});
	}
	else if (ts.is<ast::ts_array>())
	{
		return type_property_helper<
			base_type_property_func,
			default_value,
			exception_types...
		>(src_tokens, ts.get<ast::ts_array>().elem_type, context);
	}
	else if (ts.is<ast::ts_optional>())
	{
		if (ts.is_optional_pointer_like())
		{
			return bz::meta::is_in_types<ast::ts_pointer, exception_types...> ? !default_value : default_value;
		}
		else
		{
			return type_property_helper<
				base_type_property_func,
				default_value,
				exception_types...
			>(src_tokens, ts.get<ast::ts_optional>(), context);
		}
	}
	else
	{
		return default_value;
	}
}

bool parse_context::is_default_constructible(lex::src_tokens const &src_tokens, ast::typespec_view ts)
{
	return type_property_helper<
		&ast::type_info::is_default_constructible,
		false, ast::ts_optional, ast::ts_array_slice
	>(src_tokens, ts, *this);
}

bool parse_context::is_copy_constructible(lex::src_tokens const &src_tokens, ast::typespec_view ts)
{
	return type_property_helper<
		&ast::type_info::is_copy_constructible,
		false, ast::ts_enum, ast::ts_pointer, ast::ts_array_slice, ast::ts_function
	>(src_tokens, ts, *this);
}

bool parse_context::is_trivially_copy_constructible(lex::src_tokens const &src_tokens, ast::typespec_view ts)
{
	return type_property_helper<
		&ast::type_info::is_trivially_copy_constructible,
		false, ast::ts_enum, ast::ts_pointer, ast::ts_array_slice, ast::ts_function
	>(src_tokens, ts, *this);
}

bool parse_context::is_move_constructible(lex::src_tokens const &src_tokens, ast::typespec_view ts)
{
	return type_property_helper<
		&ast::type_info::is_move_constructible,
		false, ast::ts_enum, ast::ts_pointer, ast::ts_array_slice, ast::ts_function
	>(src_tokens, ts, *this);
}

bool parse_context::is_trivially_move_constructible(lex::src_tokens const &src_tokens, ast::typespec_view ts)
{
	return type_property_helper<
		&ast::type_info::is_trivially_move_constructible,
		false, ast::ts_enum, ast::ts_pointer, ast::ts_array_slice, ast::ts_function
	>(src_tokens, ts, *this);
}

bool parse_context::is_trivially_destructible(lex::src_tokens const &src_tokens, ast::typespec_view ts)
{
	return type_property_helper<
		&ast::type_info::is_trivially_destructible,
		true
	>(src_tokens, ts, *this);
}

bool parse_context::is_trivially_move_destructible(lex::src_tokens const &src_tokens, ast::typespec_view ts)
{
	return type_property_helper<
		&ast::type_info::is_trivially_move_destructible,
		true
	>(src_tokens, ts, *this);
}

bool parse_context::is_trivially_relocatable(lex::src_tokens const &src_tokens, ast::typespec_view ts)
{
	return type_property_helper<
		&ast::type_info::is_trivially_relocatable,
		false, ast::ts_enum, ast::ts_pointer, ast::ts_array_slice, ast::ts_function
	>(src_tokens, ts, *this);
}

bool parse_context::is_trivial(lex::src_tokens const &src_tokens, ast::typespec_view ts)
{
	return type_property_helper<
		&ast::type_info::is_trivial,
		false, ast::ts_enum, ast::ts_pointer, ast::ts_array_slice, ast::ts_function
	>(src_tokens, ts, *this);
}

bool parse_context::is_instantiable(lex::src_tokens const &src_tokens, ast::typespec_view ts)
{
	if (ts.is_empty())
	{
		return false;
	}

	ts = ast::remove_const_or_consteval(ts);
	if (ts.is<ast::ts_base_type>())
	{
		auto const info = ts.get<ast::ts_base_type>().info;
		this->resolve_type_members(src_tokens, info);
		return info->state >= ast::resolve_state::members;
	}
	else if (ts.is<ast::ts_enum>())
	{
		return true;
	}
	else if (ts.is<ast::ts_array>())
	{
		return this->is_instantiable(src_tokens, ts.get<ast::ts_array>().elem_type);
	}
	else if (ts.is<ast::ts_array_slice>())
	{
		// array slice type needs to be sized, because pointer arithmetic is required
		// when accessing elements
		return this->is_instantiable(src_tokens, ts.get<ast::ts_array_slice>().elem_type);
	}
	else if (ts.is<ast::ts_optional>())
	{
		return this->is_instantiable(src_tokens, ts.get<ast::ts_optional>());
	}
	else if (ts.is<ast::ts_tuple>())
	{
		return ts.get<ast::ts_tuple>().types.is_all([&](auto const &type) {
			return this->is_instantiable(src_tokens, type);
		});
	}
	else if (
		ts.is<ast::ts_pointer>()
		|| ts.is<ast::ts_lvalue_reference>()
		|| ts.is<ast::ts_move_reference>()
		|| ts.is<ast::ts_function>()
	)
	{
		return true;
	}
	else
	{
		return false;
	}
}

size_t parse_context::get_sizeof(ast::typespec_view ts)
{
	return this->global_ctx.get_sizeof(ts);
}

ast::identifier parse_context::make_qualified_identifier(lex::token_pos id)
{
	ast::identifier result;
	result.is_qualified = true;
	result.values = this->get_current_enclosing_id_scope();
	result.values.push_back(id->value);
	result.tokens = { id, id + 1 };
	return result;
}

ast::constant_value parse_context::execute_expression(ast::expression &expr)
{
	auto &codegen_context = this->global_ctx.get_codegen_context();

	auto const prev_context = codegen_context.parse_ctx;
	codegen_context.parse_ctx = this;

	auto const func = comptime::generate_code_for_expression(expr, codegen_context);

	auto executor = comptime::executor_context(&codegen_context);
	auto result = executor.execute_expression(expr, func);
	bz_assert(result.not_null() || executor.diagnostics.not_empty());

	for (auto &diagnostic : executor.diagnostics)
	{
		this->global_ctx.report_error_or_warning(std::move(diagnostic));
	}

	codegen_context.parse_ctx = prev_context;

	return result;
}

ast::constant_value parse_context::execute_expression_without_error(ast::expression &expr)
{
	auto &codegen_context = this->global_ctx.get_codegen_context();
	auto const func = comptime::generate_code_for_expression(expr, codegen_context);

	auto executor = comptime::executor_context(&codegen_context);
	auto result = executor.execute_expression(expr, func);

	return result;
}

} // namespace ctx
