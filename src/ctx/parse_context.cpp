#include "parse_context.h"
#include "ast/typespec.h"
#include "global_context.h"
#include "builtin_operators.h"
#include "lex/lexer.h"
#include "escape_sequences.h"
#include "parse/statement_parser.h"
#include "parse/consteval.h"

namespace ctx
{

static ast::expression make_expr_function_call_from_body(
	lex::src_tokens src_tokens,
	ast::function_body *body,
	bz::vector<ast::expression> params,
	parse_context &context,
	ast::resolve_order resolve_order = ast::resolve_order::regular
);

parse_context::parse_context(global_context &_global_ctx)
	: global_ctx(_global_ctx)
{}

parse_context::parse_context(parse_context const &other, local_copy_t)
	: global_ctx(other.global_ctx),
	  global_decls(other.global_decls),
	  scope_decls{},
	  // generic_functions(other.generic_functions),
	  // generic_function_scope_start(other.generic_function_scope_start),
	  current_file_id(other.current_file_id),
	  current_scope(other.current_scope),
	  current_function(nullptr),
	  resolve_queue(other.resolve_queue)
{
	this->scope_decls.resize(other.scope_decls.size());
	for (auto const [self_scope, other_scope] : bz::zip(this->scope_decls, other.scope_decls))
	{
		self_scope.func_sets = other_scope.func_sets;
		self_scope.op_sets = other_scope.op_sets;
		self_scope.type_aliases = other_scope.type_aliases;
		self_scope.types = other_scope.types;
		// self_scope.var_decls isn't copied
	}
}

parse_context::parse_context(parse_context const &other, global_copy_t)
	: global_ctx(other.global_ctx),
	  global_decls(other.global_decls),
	  current_file_id(other.current_file_id),
	  current_scope(other.current_scope)
{}

ast::type_info *parse_context::get_builtin_type_info(uint32_t kind) const
{
	return this->global_ctx.get_builtin_type_info(kind);
}

ast::typespec_view parse_context::get_builtin_type(bz::u8string_view name) const
{
	return this->global_ctx.get_builtin_type(name);
}

ast::function_body *parse_context::get_builtin_function(uint32_t kind) const
{
	return this->global_ctx.get_builtin_function(kind);
}

bz::array_view<uint32_t const> parse_context::get_builtin_universal_functions(bz::u8string_view id)
{
	return this->global_ctx.get_builtin_universal_functions(id);
}

static void add_generic_requirement_notes(bz::vector<source_highlight> &notes, parse_context const &context)
{
	if (context.resolve_queue.size() == 0 || !context.resolve_queue.back().requested.is<ast::function_body *>())
	{
		return;
	}

	auto const &body = *context.resolve_queue.back().requested.get<ast::function_body *>();
	if (!body.is_generic_specialization())
	{
		return;
	}

	if (body.is_intrinsic())
	{
		// intrinsics don't have a definition, so the source position can't be reported
		notes.emplace_back(context.make_note(bz::format("in generic instantiation of '{}'", body.get_signature())));
	}
	else
	{
		auto const file_id = body.src_tokens.pivot->src_pos.file_id;
		if (do_verbose || !context.global_ctx.is_library_file(file_id))
		{
			notes.emplace_back(context.make_note(
				body.src_tokens, bz::format("in generic instantiation of '{}'", body.get_signature())
			));
		}
		else
		{
			auto const line = body.src_tokens.pivot->src_pos.line;
			notes.emplace_back(context.make_note(
				file_id, line, bz::format("in generic instantiation of '{}'", body.get_signature())
			));
		}
	}

	for (auto const &required_from : body.generic_required_from.reversed())
	{
		if (required_from.second == nullptr)
		{
			notes.emplace_back(context.make_note(required_from.first, "required from here"));
		}
		else
		{
			auto const file_id = required_from.first.pivot->src_pos.file_id;
			if (do_verbose || !context.global_ctx.is_library_file(file_id))
			{
				notes.emplace_back(context.make_note(
					required_from.first,
					bz::format("required from generic instantiation of '{}'", required_from.second->get_signature())
				));
			}
			else
			{
				auto const line = required_from.first.pivot->src_pos.line;
				notes.emplace_back(context.make_note(
					file_id, line,
					bz::format("required from generic instantiation of '{}'", required_from.second->get_signature())
				));
			}
		}
	}
}

static bz::vector<std::pair<lex::src_tokens, ast::function_body *>> get_generic_requirements(
	lex::src_tokens src_tokens,
	parse_context &context
)
{
	bz::vector<std::pair<lex::src_tokens, ast::function_body *>> result;
	auto it = context.resolve_queue.rbegin();
	auto const end = context.resolve_queue.rend();
	auto const is_generic_specialization_dep = [](auto const &dep) {
		auto const body = dep.requested.template get_if<ast::function_body *>();
		return body != nullptr && (*body)->is_generic_specialization();
	};
	if (it != end && is_generic_specialization_dep(*it))
	{
		// we need to accumulate everything on the front, but prefer using push_back for the first
		// few elements that are known
		if (it->requester.pivot != nullptr)
		{
			result.push_back({ it->requester, nullptr });
		}
		result.push_back({ src_tokens, it->requested.get<ast::function_body *>() });
		++it;
		for (; it != end && is_generic_specialization_dep(*it); ++it)
		{
			auto &dep = *it;
			auto const body = it->requested.get<ast::function_body *>();
			bz_assert(body != nullptr);
			result.front().second = body;
			result.push_front({ dep.requester, nullptr });
		}
	}
	else
	{
		result.push_back({ src_tokens, nullptr });
	}
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
	lex::src_tokens src_tokens,
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
					dep.requested.get<ast::type_info *>()->type_name.format_as_unqualified()
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
		bz::format("circular dependency encountered while resolving type 'struct {}'", info.type_name.format_as_unqualified()),
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
	lex::src_tokens src_tokens,
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
	lex::src_tokens src_tokens,
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

[[nodiscard]] source_highlight parse_context::make_note(lex::src_tokens src_tokens, bz::u8string message)
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

[[nodiscard]] source_highlight parse_context::make_note_with_suggestion_around(
	lex::src_tokens src_tokens,
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

void parse_context::set_current_file(uint32_t file_id)
{
	this->current_file_id = file_id;
	auto &file = this->global_ctx.get_src_file(file_id);
	this->current_scope   = file._scope;
	this->global_decls    = &file._global_decls;
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


void parse_context::add_scope(void)
{
	this->scope_decls.push_back({});
	this->generic_function_scope_start.push_back(this->generic_functions.size());
}

void parse_context::remove_scope(void)
{
	bz_assert(!this->scope_decls.empty());
	if (is_warning_enabled(warning_kind::unused_variable))
	{
		for (auto const var_decl : this->scope_decls.back().var_decls)
		{
			if (!var_decl->is_used() && !var_decl->is_maybe_unused() && !var_decl->get_id().values.empty())
			{
				this->report_warning(
					warning_kind::unused_variable,
					*var_decl,
					bz::format("unused variable '{}'", var_decl->get_id().format_as_unqualified())
				);
			}
		}
	}
	auto const generic_functions_start_index = this->generic_function_scope_start.back();
	for (std::size_t i = generic_functions_start_index ; i < this->generic_functions.size(); ++i)
	{
		bz_assert(!this->generic_functions[i]->is_generic());
		this->add_to_resolve_queue({}, *this->generic_functions[i]);
		parse::resolve_function({}, *this->generic_functions[i], *this);
		this->pop_resolve_queue();
	}
	this->generic_functions.resize(generic_functions_start_index);
	this->generic_function_scope_start.pop_back();
	this->scope_decls.pop_back();
}


void parse_context::add_local_variable(ast::decl_variable &var_decl)
{
	bz_assert(this->scope_decls.size() != 0);
	if (var_decl.tuple_decls.empty())
	{
		this->scope_decls.back().var_decls.push_back(&var_decl);
	}
	else
	{
		for (auto &decl : var_decl.tuple_decls)
		{
			this->add_local_variable(decl);
		}
	}
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

void parse_context::add_local_type_alias(ast::decl_type_alias &type_alias)
{
	bz_assert(this->scope_decls.size() != 0);
	this->scope_decls.back().add_type_alias(type_alias);
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
		param_types.emplace_back(p.get_type());
	}
	return ast::typespec({ ast::ts_function{ {}, std::move(param_types), return_type } });
}

static ast::decl_variable *find_var_decl_in_local_scope(decl_set &scope, ast::identifier const &id)
{
	if (!id.is_qualified && id.values.size() == 1)
	{
		auto const it = std::find_if(
			scope.var_decls.rbegin(), scope.var_decls.rend(),
			[&id](auto const var) {
				bz_assert(!var->get_id().is_qualified && var->get_id().values.size() <= 1);
				return !var->get_id().values.empty() && var->get_id().values.front() == id.values.front();
			}
		);
		return it == scope.var_decls.rend() ? nullptr : *it;
	}
	else
	{
		return nullptr;
	}
}

static function_overload_set *find_func_set_in_local_scope(decl_set &scope, ast::identifier const &id)
{
	if (!id.is_qualified && id.values.size() == 1)
	{
		auto const it = std::find_if(
			scope.func_sets.begin(), scope.func_sets.end(),
			[&id](auto const &fn_set) {
				bz_assert(!fn_set.id.is_qualified && fn_set.id.values.size() == 1);
				return fn_set.id.values.front() == id.values.front();
			}
		);
		return it == scope.func_sets.end() ? nullptr : &*it;
	}
	else
	{
		return nullptr;
	}
}

static ast::decl_struct *find_type_in_local_scope(decl_set &scope, ast::identifier const &id)
{
	if (!id.is_qualified && id.values.size() == 1)
	{
		auto const it = std::find_if(
			scope.types.rbegin(), scope.types.rend(),
			[&id](auto const &type) {
				bz_assert(!type->id.is_qualified && type->id.values.size() == 1);
				return type->id.values.front() == id.values.front();
			}
		);
		return it == scope.types.rend() ? nullptr : *it;
	}
	else
	{
		return nullptr;
	}
}

static ast::decl_type_alias *find_type_alias_in_local_scope(decl_set &scope, ast::identifier const &id)
{
	if (!id.is_qualified && id.values.size() == 1)
	{
		auto const it = std::find_if(
			scope.type_aliases.rbegin(), scope.type_aliases.rend(),
			[&id](auto const &type_alias) {
				bz_assert(!type_alias->id.is_qualified && type_alias->id.values.size() == 1);
				return type_alias->id.values.front() == id.values.front();
			}
		);
		return it == scope.type_aliases.rend() ? nullptr : *it;
	}
	else
	{
		return nullptr;
	}
}

static bool is_unqualified_equals(
	ast::identifier const &lhs,
	ast::identifier const &rhs,
	bz::array_view<bz::u8string_view const> current_scope
)
{
	bz_assert(lhs.is_qualified);
	bz_assert(!rhs.is_qualified);
	auto const rhs_size = rhs.values.size();
	return lhs.values.size() >= rhs_size
		&& lhs.values.size() <= rhs_size + current_scope.size()
		&& std::equal(lhs.values.end() - rhs_size, lhs.values.end(), rhs.values.begin())
		&& std::equal(lhs.values.begin(), lhs.values.end() - rhs_size, current_scope.begin());
}

static ast::decl_variable *find_var_decl_in_global_scope(
	decl_set &global_decls,
	ast::identifier const &id,
	bz::array_view<bz::u8string_view const> current_scope
)
{
	if (id.is_qualified)
	{
		auto const it = std::find_if(
			global_decls.var_decls.begin(), global_decls.var_decls.end(),
			[&id](auto const var) {
				bz_assert(var->get_id().is_qualified);
				return var->get_id() == id;
			}
		);
		return it == global_decls.var_decls.end() ? nullptr : *it;
	}
	else
	{
		return global_decls.var_decls
			.filter([&id, current_scope](auto const var) { return is_unqualified_equals(var->get_id(), id, current_scope); })
			.max(nullptr, [](auto const lhs, auto const rhs) {
				if (rhs == nullptr)
				{
					return false;
				}
				else if (lhs == nullptr)
				{
					return true;
				}
				else
				{
					return lhs->get_id().values.size() < rhs->get_id().values.size();
				}
			});
	}
}

static std::pair<function_overload_set *, size_t> find_func_set_in_global_scope(
	decl_set &global_decls,
	ast::identifier const &id,
	bz::array_view<bz::u8string_view const> current_scope
)
{
	if (id.is_qualified)
	{
		auto const it = std::find_if(
			global_decls.func_sets.begin(), global_decls.func_sets.end(),
			[&id](auto const &var) {
				bz_assert(var.id.is_qualified);
				return var.id == id;
			}
		);
		if (it == global_decls.func_sets.end())
		{
			return { nullptr, 0 };
		}
		else
		{
			return { &*it, 1 };
		}
	}
	else
	{
		auto const range = global_decls.func_sets
			.transform([](auto &set) { return &set; })
			.filter([&id, current_scope](auto const set) { return is_unqualified_equals(set->id, id, current_scope); });
		if (range.at_end())
		{
			return { nullptr, 0 };
		}
		else
		{
			return { range.front(), range.count() };
		}
	}
}

static ast::decl_struct *find_type_in_global_scope(
	decl_set &global_decls,
	ast::identifier const &id,
	bz::array_view<bz::u8string_view const> current_scope
)
{
	if (id.is_qualified)
	{
		auto const it = std::find_if(
			global_decls.types.begin(), global_decls.types.end(),
			[&id](auto const type) {
				bz_assert(type->id.is_qualified);
				return type->id == id;
			}
		);
		return it == global_decls.types.end() ? nullptr : *it;
	}
	else
	{
		return global_decls.types
			.filter([&id, current_scope](auto const type) { return is_unqualified_equals(type->id, id, current_scope); })
			.max(nullptr, [](auto const lhs, auto const rhs) {
				if (rhs == nullptr)
				{
					return false;
				}
				else if (lhs == nullptr)
				{
					return true;
				}
				else
				{
					return lhs->id.values.size() < rhs->id.values.size();
				}
			});
	}
}

static ast::decl_type_alias *find_type_alias_in_global_scope(
	decl_set &global_decls,
	ast::identifier const &id,
	bz::array_view<bz::u8string_view const> current_scope
)
{
	if (id.is_qualified)
	{
		auto const it = std::find_if(
			global_decls.type_aliases.begin(), global_decls.type_aliases.end(),
			[&id](auto const type_alias) {
				bz_assert(type_alias->id.is_qualified);
				return type_alias->id == id;
			}
		);
		return it == global_decls.type_aliases.end() ? nullptr : *it;
	}
	else
	{
		return global_decls.type_aliases
			.filter([&id, current_scope](auto const type_alias) { return is_unqualified_equals(type_alias->id, id, current_scope); })
			.max(nullptr, [](auto const lhs, auto const rhs) {
				if (rhs == nullptr)
				{
					return false;
				}
				else if (lhs == nullptr)
				{
					return true;
				}
				else
				{
					return lhs->id.values.size() < rhs->id.values.size();
				}
			});
	}
}

static ast::expression make_variable_expression(
	lex::src_tokens src_tokens,
	ast::identifier id,
	ast::decl_variable *var_decl,
	parse_context &context
)
{
	var_decl->flags |= ast::decl_variable::used;
	auto id_type_kind = ast::expression_type_kind::lvalue;
	ast::typespec_view id_type = var_decl->get_type();
	if (id_type.is<ast::ts_lvalue_reference>())
	{
		id_type_kind = ast::expression_type_kind::lvalue_reference;
		id_type = id_type.get<ast::ts_lvalue_reference>();
	}

	if (id_type.is_empty())
	{
		bz_assert(context.has_errors());
		return ast::make_error_expression(src_tokens, ast::make_expr_identifier(std::move(id)));
	}
	else if (id_type.is<ast::ts_consteval>())
	{
		auto &init_expr = var_decl->init_expr;
		bz_assert(init_expr.is<ast::constant_expression>());
		ast::typespec result_type = id_type.get<ast::ts_consteval>();
		result_type.add_layer<ast::ts_const>(nullptr);
		return ast::make_constant_expression(
			src_tokens,
			id_type_kind, std::move(result_type),
			init_expr.get<ast::constant_expression>().value,
			ast::make_expr_identifier(std::move(id), var_decl)
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
			ast::make_expr_identifier(std::move(id), var_decl)
		);
	}
	else
	{
		return ast::make_dynamic_expression(
			src_tokens,
			id_type_kind, id_type,
			ast::make_expr_identifier(std::move(id), var_decl)
		);
	}
}

static ast::expression make_function_name_expression(
	lex::src_tokens src_tokens,
	ast::identifier id,
	function_overload_set *fn_set,
	parse_context &
)
{
	if (
		fn_set->func_decls.size() + fn_set->alias_decls
				.transform([](auto const &decl) {
					return decl.template get<ast::decl_function_alias>().aliased_bodies.size();
				}).sum()
			>= 2
	)
	{
		bz_assert(!fn_set->id.is_qualified && fn_set->id.values.size() == 1);
		auto id_value = ast::constant_value();
		id_value.emplace<ast::constant_value::unqualified_function_set_id>(id.values);
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::function_name, ast::typespec(),
			ast::constant_value(std::move(id_value)),
			ast::make_expr_identifier(id)
		);
	}
	auto const body = fn_set->func_decls.empty()
		? fn_set->alias_decls.front().get<ast::decl_function_alias>().aliased_bodies.front()
		: &fn_set->func_decls.front().get<ast::decl_function>().body;
	return ast::make_constant_expression(
		src_tokens,
		ast::expression_type_kind::function_name, get_function_type(*body),
		ast::constant_value(body),
		ast::make_expr_identifier(id)
	);
}

static ast::expression make_type_expression(
	lex::src_tokens src_tokens,
	ast::identifier id,
	ast::decl_struct *type,
	parse_context &context
)
{
	auto &info = type->info;
	context.add_to_resolve_queue(src_tokens, info);
	parse::resolve_type_info_symbol(info, context);
	context.pop_resolve_queue();
	if (info.state != ast::resolve_state::error)
	{
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::type_name,
			ast::make_typename_typespec(nullptr),
			ast::constant_value(ast::make_base_type_typespec(src_tokens, &info)),
			ast::make_expr_identifier(std::move(id))
		);
	}
	else
	{
		return ast::make_error_expression(src_tokens, ast::make_expr_identifier(ast::expr_identifier(std::move(id))));
	}
}

static ast::expression make_type_alias_expression(
	lex::src_tokens src_tokens,
	ast::identifier id,
	ast::decl_type_alias *type_alias,
	parse_context &
)
{
	auto const type = type_alias->get_type();
	if (type.has_value())
	{
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::type_name,
			ast::make_typename_typespec(nullptr),
			ast::constant_value(type),
			ast::make_expr_identifier(std::move(id))
		);
	}
	else
	{
		return ast::make_error_expression(src_tokens, ast::make_expr_identifier(ast::expr_identifier(std::move(id))));
	}
}

ast::expression parse_context::make_identifier_expression(ast::identifier id)
{
	// ==== local decls ====
	// we go in reverse through the scopes and the variables
	// in case there's shadowing
	lex::src_tokens const src_tokens = { id.tokens.begin, id.tokens.begin, id.tokens.end };

	for (auto &scope : this->scope_decls.reversed())
	{
		if (auto const var_decl = find_var_decl_in_local_scope(scope, id))
		{
			return make_variable_expression(src_tokens, std::move(id), var_decl, *this);
		}

		if (auto const fn_set = find_func_set_in_local_scope(scope, id))
		{
			return make_function_name_expression(src_tokens, std::move(id), fn_set, *this);
		}

		if (auto const type = find_type_in_local_scope(scope, id))
		{
			return make_type_expression(src_tokens, std::move(id), type, *this);
		}

		if (auto const type_alias = find_type_alias_in_local_scope(scope, id))
		{
			return make_type_alias_expression(src_tokens, std::move(id), type_alias, *this);
		}
	}

	// ==== export (global) decls ====
	auto &global_decls = *this->global_decls;
	if (auto const var_decl = find_var_decl_in_global_scope(global_decls, id, this->current_scope))
	{
		// global variables need to be resolved here
		this->add_to_resolve_queue(src_tokens, *var_decl);
		parse::resolve_variable(*var_decl, *this);
		this->pop_resolve_queue();
		var_decl->flags |= ast::decl_variable::used;
		auto id_type_kind = ast::expression_type_kind::lvalue;
		ast::typespec_view id_type = var_decl->get_type();
		if (id_type.is<ast::ts_lvalue_reference>())
		{
			id_type_kind = ast::expression_type_kind::lvalue_reference;
			id_type = id_type.get<ast::ts_lvalue_reference>();
		}

		if (id_type.is_empty())
		{
			bz_assert(this->has_errors());
			return ast::make_error_expression(src_tokens, ast::make_expr_identifier(ast::expr_identifier(std::move(id))));
		}
		else if (id_type.is<ast::ts_consteval>() && var_decl->init_expr.is<ast::constant_expression>())
		{
			auto &init_expr = var_decl->init_expr;
			bz_assert(init_expr.is<ast::constant_expression>());
			ast::typespec result_type = id_type.get<ast::ts_consteval>();
			result_type.add_layer<ast::ts_const>(nullptr);
			return ast::make_constant_expression(
				src_tokens,
				id_type_kind, std::move(result_type),
				init_expr.get<ast::constant_expression>().value,
				ast::make_expr_identifier(std::move(id), var_decl)
			);
		}
		else
		{
			return ast::make_dynamic_expression(
				src_tokens,
				id_type_kind, id_type,
				ast::make_expr_identifier(std::move(id), var_decl)
			);
		}
	}

	if (
		auto const [fn_set, set_count] = find_func_set_in_global_scope(global_decls, id, this->current_scope);
		set_count > 1
	)
	{
		auto value = ast::constant_value();
		if (id.is_qualified)
		{
			value.emplace<ast::constant_value::qualified_function_set_id>(id.values);
		}
		else
		{
			value.emplace<ast::constant_value::unqualified_function_set_id>(id.values);
		}
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::function_name, ast::typespec(),
			std::move(value),
			ast::make_expr_identifier(std::move(id))
		);
	}
	else if (set_count == 1)
	{
		for (auto &alias_decl : fn_set->alias_decls)
		{
			this->add_to_resolve_queue(src_tokens, alias_decl.get<ast::decl_function_alias>());
			parse::resolve_function_alias(alias_decl.get<ast::decl_function_alias>(), *this);
			this->pop_resolve_queue();
		}
		auto const id_type_kind = ast::expression_type_kind::function_name;
		auto const func_count = fn_set->func_decls.size()
			+ fn_set->alias_decls
				.transform([](auto const &decl) { return decl.template get<ast::decl_function_alias>().aliased_bodies.size(); })
				.sum();
		if (func_count == 0)
		{
			return ast::make_error_expression(src_tokens, ast::make_expr_identifier(ast::expr_identifier(std::move(id))));
		}
		else if (func_count == 1)
		{
			auto const body = fn_set->func_decls.empty()
				? fn_set->alias_decls.front().get<ast::decl_function_alias>().aliased_bodies.front()
				: &fn_set->func_decls.front().get<ast::decl_function>().body;
			return ast::make_constant_expression(
				src_tokens,
				id_type_kind, get_function_type(*body),
				ast::constant_value(body),
				ast::make_expr_identifier(std::move(id))
			);
		}
		else
		{
			auto value = ast::constant_value();
			if (id.is_qualified)
			{
				value.emplace<ast::constant_value::qualified_function_set_id>(id.values);
			}
			else
			{
				value.emplace<ast::constant_value::unqualified_function_set_id>(id.values);
			}
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::function_name, ast::typespec(),
				std::move(value),
				ast::make_expr_identifier(std::move(id))
			);
		}
	}

	if (auto const type = find_type_in_global_scope(global_decls, id, this->current_scope))
	{
		auto &info = type->info;
		this->add_to_resolve_queue(src_tokens, info);
		parse::resolve_type_info_symbol(info, *this);
		this->pop_resolve_queue();
		if (info.state != ast::resolve_state::error)
		{
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::type_name,
				ast::make_typename_typespec(nullptr),
				ast::constant_value(ast::make_base_type_typespec(src_tokens, &info)),
				ast::make_expr_identifier(id)
			);
		}
		else
		{
			return ast::make_error_expression(src_tokens, ast::make_expr_identifier(ast::expr_identifier(std::move(id))));
		}
	}

	if (auto const type_alias = find_type_alias_in_global_scope(global_decls, id, this->current_scope))
	{
		this->add_to_resolve_queue(src_tokens, *type_alias);
		parse::resolve_type_alias(*type_alias, *this);
		this->pop_resolve_queue();
		auto const type = type_alias->get_type();
		if (type.has_value())
		{
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::type_name,
				ast::make_typename_typespec(nullptr),
				ast::constant_value(type),
				ast::make_expr_identifier(id)
			);
		}
		else
		{
			return ast::make_error_expression(src_tokens, ast::make_expr_identifier(ast::expr_identifier(std::move(id))));
		}
	}

	// builtin types
	// qualification doesn't matter here, they act as globally defined types
	if (id.values.size() == 1)
	{
		auto const id_value = id.values.front();
		if (auto const builtin_type = this->get_builtin_type(id_value); builtin_type.has_value())
		{
			ast::typespec type = builtin_type;
			type.nodes.back().visit(bz::overload{
				[&](ast::ts_base_type &base_type) {
					base_type.src_tokens = src_tokens;
				},
				[&](ast::ts_void &void_t) {
					void_t.void_pos = id.tokens.begin;
				},
				[](auto const &) {
					bz_unreachable;
				}
			});
			return ast::make_constant_expression(
				src_tokens,
				ast::expression_type_kind::type_name,
				ast::make_typename_typespec(nullptr),
				ast::constant_value(std::move(type)),
				ast::make_expr_identifier(id)
			);
		}
		else if (id_value.starts_with("__builtin"))
		{
			auto const it = std::find_if(ast::intrinsic_info.begin(), ast::intrinsic_info.end(), [id_value](auto const &p) {
				return p.func_name == id_value;
			});
			if (it != ast::intrinsic_info.end())
			{
				auto const func_body = this->get_builtin_function(it->kind);
				return ast::make_constant_expression(
					src_tokens,
					ast::expression_type_kind::function_name,
					get_function_type(*func_body),
					ast::constant_value(func_body),
					ast::make_expr_identifier(id)
				);
			}
		}
	}

	this->report_error(src_tokens, bz::format("undeclared identifier '{}'", id.as_string()));
	return ast::make_error_expression(src_tokens, ast::make_expr_identifier(ast::expr_identifier(std::move(id))));
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
				type_info = this->get_builtin_type_info(default_type_info);
			}
			else if (num <= static_cast<uint64_t>(std::numeric_limits<wide_default_type>::max()))
			{
				value = static_cast<wide_default_type>(num);
				type_info = this->get_builtin_type_info(default_type_info + 1);
			}
			else
			{
				value = num;
				type_info = this->get_builtin_type_info(ast::type_info::uint64_);
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
    type_info = this->get_builtin_type_info(ast::type_info::type##_);                         \
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
		else if (postfix == "uz")
		{
			uint64_t max_value;
			switch (this->global_ctx.get_data_layout().getPointerSize())
			{
			case 8:
				max_value = static_cast<uint64_t>(std::numeric_limits<uint64_t>::max());
				value = static_cast<uint64_t>(static_cast<uint64_t>(num));
				type_info = this->get_builtin_type_info(ast::type_info::uint64_);
				break;
			case 4:
				max_value = static_cast<uint64_t>(std::numeric_limits<uint32_t>::max());
				value = static_cast<uint64_t>(static_cast<uint32_t>(num));
				type_info = this->get_builtin_type_info(ast::type_info::uint32_);
				break;
			case 2:
				max_value = static_cast<uint64_t>(std::numeric_limits<uint16_t>::max());
				value = static_cast<uint64_t>(static_cast<uint16_t>(num));
				type_info = this->get_builtin_type_info(ast::type_info::uint16_);
				break;
			case 1:
				max_value = static_cast<uint64_t>(std::numeric_limits<uint8_t>::max());
				value = static_cast<uint64_t>(static_cast<uint8_t>(num));
				type_info = this->get_builtin_type_info(ast::type_info::uint8_);
				break;
			default:
				bz_unreachable;
			}
			if (num > max_value)
			{
				this->report_error(literal, "literal value is too large to fit in type 'usize'");
			}
		}
		else if (postfix == "iz")
		{
			uint64_t max_value;
			switch (this->global_ctx.get_data_layout().getPointerSize())
			{
			case 8:
				max_value = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
				value = static_cast<int64_t>(static_cast<int64_t>(num));
				type_info = this->get_builtin_type_info(ast::type_info::int64_);
				break;
			case 4:
				max_value = static_cast<uint64_t>(std::numeric_limits<int32_t>::max());
				value = static_cast<int64_t>(static_cast<int32_t>(num));
				type_info = this->get_builtin_type_info(ast::type_info::int32_);
				break;
			case 2:
				max_value = static_cast<uint64_t>(std::numeric_limits<int16_t>::max());
				value = static_cast<int64_t>(static_cast<int16_t>(num));
				type_info = this->get_builtin_type_info(ast::type_info::int16_);
				break;
			case 1:
				max_value = static_cast<uint64_t>(std::numeric_limits<int8_t>::max());
				value = static_cast<int64_t>(static_cast<int8_t>(num));
				type_info = this->get_builtin_type_info(ast::type_info::int8_);
				break;
			default:
				bz_unreachable;
			}
			if (num > max_value)
			{
				this->report_error(literal, "literal value is too large to fit in type 'isize'");
			}
		}
		else
		{
			if (num <= static_cast<uint64_t>(std::numeric_limits<default_type>::max()))
			{
				value = static_cast<wide_default_type>(num);
				type_info = this->get_builtin_type_info(default_type_info);
			}
			else if (num <= static_cast<uint64_t>(std::numeric_limits<wide_default_type>::max()))
			{
				value = static_cast<wide_default_type>(num);
				type_info = this->get_builtin_type_info(default_type_info + 1);
			}
			else
			{
				value = num;
				type_info = this->get_builtin_type_info(ast::type_info::uint64_);
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
				return ast::make_error_expression(src_tokens, ast::make_expr_literal(literal));
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
			type_info = this->get_builtin_type_info(ast::type_info::float64_);
		}
		else if (postfix == "f32")
		{
			value = static_cast<float32_t>(num);
			type_info = this->get_builtin_type_info(ast::type_info::float32_);
		}
		else
		{
			value = num;
			type_info = this->get_builtin_type_info(ast::type_info::float64_);
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
			ast::typespec({ ast::ts_base_type{ {}, this->get_builtin_type_info(ast::type_info::char_) } }),
			ast::constant_value(value),
			ast::make_expr_literal(literal)
		);
	}
	case lex::token::kw_true:
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			ast::typespec({ ast::ts_base_type{ {}, this->get_builtin_type_info(ast::type_info::bool_) } }),
			ast::constant_value(true),
			ast::make_expr_literal(literal)
		);
	case lex::token::kw_false:
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			ast::typespec({ ast::ts_base_type{ {}, this->get_builtin_type_info(ast::type_info::bool_) } }),
			ast::constant_value(false),
			ast::make_expr_literal(literal)
		);
	case lex::token::kw_null:
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			ast::typespec({ ast::ts_base_type{ {}, this->get_builtin_type_info(ast::type_info::null_t_) } }),
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
		ast::typespec({ ast::ts_base_type{ {}, this->get_builtin_type_info(ast::type_info::str_) } }),
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
			if (e.is_error())
			{
				return true;
			}
		}
		return false;
	}();

	if (has_error)
	{
		return ast::make_error_expression(src_tokens, ast::make_expr_tuple(std::move(elems)));
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

static bool is_builtin_type(ast::typespec_view ts)
{
	switch (ts.kind())
	{
	case ast::typespec_node_t::index_of<ast::ts_const>:
		return is_builtin_type(ts.get<ast::ts_const>());
	case ast::typespec_node_t::index_of<ast::ts_base_type>:
	{
		auto &base = ts.get<ast::ts_base_type>();
		bz_assert(base.info->kind != ast::type_info::forward_declaration);
		return base.info->kind != ast::type_info::aggregate;
	}
	case ast::typespec_node_t::index_of<ast::ts_pointer>:
	case ast::typespec_node_t::index_of<ast::ts_function>:
	case ast::typespec_node_t::index_of<ast::ts_tuple>:
	case ast::typespec_node_t::index_of<ast::ts_array>:
	case ast::typespec_node_t::index_of<ast::ts_array_slice>:
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
	if (expr.is_if_expr())
	{
		auto const &if_expr = expr.get_if_expr();
		return is_implicitly_convertible(dest, if_expr.then_block, context)
			&& is_implicitly_convertible(dest, if_expr.else_block, context);
	}
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

struct match_level_t : public bz::variant<int, bz::vector<match_level_t>>
{
	using base_t = bz::variant<int, bz::vector<match_level_t>>;
	using base_t::variant;
	~match_level_t(void) noexcept = default;
};

static void format_match_level_impl(match_level_t const &match_level, bz::u8string &result)
{
	if (match_level.is<int>())
	{
		result += bz::format("{}", match_level.get<int>());
	}
	else if (match_level.is<bz::vector<match_level_t>>())
	{
		auto const &vec = match_level.get<bz::vector<match_level_t>>();
		result += "[";
		bool first = true;
		for (auto const &ml : vec)
		{
			if (first)
			{
				first = false;
			}
			else
			{
				result += ", ";
			}
			format_match_level_impl(ml, result);
		}
		result += "]";
	}
	else
	{
		result += "null";
	}
}

[[maybe_unused]] static bz::u8string format_match_level(match_level_t const &match_level)
{
	bz::u8string result;
	format_match_level_impl(match_level, result);
	return result;
}

// returns -1 if lhs < rhs, 0 if lhs == rhs or they are ambiguous and 1 if lhs > rhs
static int match_level_compare(match_level_t const &lhs, match_level_t const &rhs)
{
	if (lhs.is_null() && rhs.is_null())
	{
		return 0;
	}
	else if (lhs.is_null())
	{
		return -1;
	}
	else if (rhs.is_null())
	{
		return 1;
	}

	bz_assert(lhs.index() == rhs.index());
	if (lhs.is<int>() && rhs.is<int>())
	{
		auto const lhs_int = lhs.get<int>();
		auto const rhs_int = rhs.get<int>();
		return lhs_int < rhs_int ? -1 : lhs_int == rhs_int ? 0 : 1;
	}
	else if (lhs.is<bz::vector<match_level_t>>() && rhs.is<bz::vector<match_level_t>>())
	{
		auto const &lhs_vec = lhs.get<bz::vector<match_level_t>>();
		auto const &rhs_vec = rhs.get<bz::vector<match_level_t>>();
		bz_assert(lhs_vec.size() == rhs_vec.size());
		bool has_less_than = false;
		for (auto const &[lhs_val, rhs_val] : bz::zip(lhs_vec, rhs_vec))
		{
			auto const cmp_res = match_level_compare(lhs_val, rhs_val);
			if (cmp_res < 0)
			{
				has_less_than = true;
			}
			else if (cmp_res > 0 && has_less_than)
			{
				// ambiguous
				return 0;
			}
			else if (cmp_res > 0)
			{
				return 1;
			}
		}
		return has_less_than ? -1 : 0;
	}
	bz_unreachable;
	return 1;
}

static bool operator < (match_level_t const &lhs, match_level_t const &rhs)
{
	return match_level_compare(lhs, rhs) < 0;
}

static match_level_t operator + (match_level_t lhs, int rhs)
{
	if (lhs.is<int>())
	{
		lhs.get<int>() += rhs;
	}
	else if (lhs.is<bz::vector<match_level_t>>())
	{
		for (auto &val : lhs.get<bz::vector<match_level_t>>())
		{
			val = std::move(val) + rhs;
		}
	}
	return lhs;
}

static match_level_t get_strict_typename_match_level(
	ast::typespec_view dest,
	ast::typespec_view source
)
{
	if (!ast::is_complete(source))
	{
		return match_level_t{};
	}

	int result = 0;
	while (dest.kind() == source.kind() && dest.is_safe_blind_get() && source.is_safe_blind_get())
	{
		dest = dest.blind_get();
		source = source.blind_get();
		++result;
	}

	if (dest.is<ast::ts_typename>())
	{
		return result;
	}
	else if (dest.kind() != source.kind())
	{
		return match_level_t{};
	}
	else if (dest.is<ast::ts_array>())
	{
		return get_strict_typename_match_level(dest.get<ast::ts_array>().elem_type, source.get<ast::ts_array>().elem_type) + result;
	}
	else if (dest.is<ast::ts_array_slice>())
	{
		return get_strict_typename_match_level(dest.get<ast::ts_array_slice>().elem_type, source.get<ast::ts_array_slice>().elem_type) + result;
	}
	else
	{
		bz_unreachable;
	}
}

static match_level_t get_strict_type_match_level(
	ast::typespec_view dest,
	ast::typespec_view source,
	bool accept_void
)
{
	bz_assert(ast::is_complete(source));
	int result = 0;
	while (dest.kind() == source.kind() && dest.is_safe_blind_get() && source.is_safe_blind_get())
	{
		dest = dest.blind_get();
		source = source.blind_get();
		++result;
	}
	bz_assert(!dest.is<ast::ts_unresolved>());
	bz_assert(!source.is<ast::ts_unresolved>());

	if (dest.is<ast::ts_auto>() && !source.is<ast::ts_const>())
	{
		bz_assert(!source.is<ast::ts_consteval>());
		return result + 1;
	}
	else if (dest == source)
	{
		return result + 2;
	}
	else if (accept_void && dest.is<ast::ts_void>() && !source.is<ast::ts_const>())
	{
		return result;
	}
	else
	{
		return match_level_t{};
	}
}

static match_level_t get_type_match_level(
	ast::typespec_view dest,
	ast::expression &expr,
	parse_context &context
)
{
	// six base cases:
	// *T
	//     -> if expr is of pointer type strict match U to T
	//     -> else expr is some base type, try implicitly casting it
	//     -> special case for *void
	// *const T
	//     -> same as before, but U doesn't have to be const (no need to strict match), +1 match level if U is const
	// &T
	//     -> expr must be an lvalue
	//     -> strict match type of expr to T
	//     -> +1 match level because it matched the refererence
	// &const T
	//     -> expr must be an lvalue
	//     -> type of expr doesn't need to be const, +1 match level if U is const
	//     -> +1 match level because it matched the refererence
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
		parse::consteval_try(expr, context);
		if (!expr.is<ast::constant_expression>())
		{
			return match_level_t{};
		}
		else
		{
			return get_type_match_level(dest.get<ast::ts_consteval>(), expr, context);
		}
	}

	if (expr.is_if_expr())
	{
		auto &if_expr = expr.get_if_expr();
		if (ast::is_complete(dest))
		{
			auto then_result = get_type_match_level(dest, if_expr.then_block, context);
			auto else_result = get_type_match_level(dest, if_expr.else_block, context);
			if (then_result.is_null() || else_result.is_null())
			{
				return match_level_t{};
			}
			else if (then_result < else_result)
			{
				return then_result;
			}
			else
			{
				return else_result;
			}
		}
		else
		{
			auto then_result = get_type_match_level(dest, if_expr.then_block, context);
			auto else_result = get_type_match_level(dest, if_expr.else_block, context);
			if (then_result.is_null() || else_result.is_null())
			{
				return match_level_t{};
			}

			if (ast::is_complete(dest))
			{
				// return the worse match
				if (then_result < else_result)
				{
					return then_result;
				}
				else
				{
					return else_result;
				}
			}

			ast::typespec then_matched_type = dest;
			ast::typespec else_matched_type = dest;
			context.match_expression_to_type(if_expr.then_block, then_matched_type);
			context.match_expression_to_type(if_expr.else_block, else_matched_type);

			if (then_matched_type == else_matched_type)
			{
				// return the worse match
				if (then_result < else_result)
				{
					return then_result;
				}
				else
				{
					return else_result;
				}
			}

			auto after_match_then_result = get_type_match_level(else_matched_type, if_expr.then_block, context);
			auto after_match_else_result = get_type_match_level(then_matched_type, if_expr.else_block, context);
			if (after_match_then_result.is_null() && after_match_else_result.is_null())
			{
				return match_level_t{};
			}
			else if (after_match_then_result.is_null())
			{
				// return the worse match
				if (then_result < after_match_else_result)
				{
					return then_result;
				}
				else
				{
					return after_match_else_result;
				}
			}
			else if (after_match_else_result.is_null())
			{
				// return the worse match
				if (after_match_then_result < else_result)
				{
					return after_match_then_result;
				}
				else
				{
					return else_result;
				}
			}
			else
			{
				return match_level_t{};
			}
		}
	}
	else if (expr.is_switch_expr())
	{
		bz_unreachable;
	}
	else if (expr.is_typename())
	{
		if (!dest.is_typename())
		{
			return match_level_t{};
		}

		return get_strict_typename_match_level(dest, expr.get_typename());
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
					return get_strict_type_match_level(inner_dest.get<ast::ts_const>(), inner_expr_type.get<ast::ts_const>(), true) + 2;
				}
				else
				{
					return get_strict_type_match_level(inner_dest.get<ast::ts_const>(), inner_expr_type, true) + 1;
				}
			}
			else
			{
				return get_strict_type_match_level(inner_dest, inner_expr_type, true) + 2;
			}
		}
		// special case for null
		else if (
			expr_type_without_const.is<ast::ts_base_type>()
			&& expr_type_without_const.get<ast::ts_base_type>().info->kind == ast::type_info::null_t_
		)
		{
			if (ast::is_complete(dest))
			{
				// a conversion takes place, so its match level is 0
				return 0;
			}
			else
			{
				return match_level_t{};
			}
		}
	}
	else if (dest.is<ast::ts_lvalue_reference>())
	{
		if (expr_type_kind != ast::expression_type_kind::lvalue && expr_type_kind != ast::expression_type_kind::lvalue_reference)
		{
			return match_level_t{};
		}

		auto const inner_dest = dest.get<ast::ts_lvalue_reference>();
		if (inner_dest.is<ast::ts_const>())
		{
			if (expr_type.is<ast::ts_const>())
			{
				return get_strict_type_match_level(inner_dest.get<ast::ts_const>(), expr_type_without_const, false) + 5;
			}
			else
			{
				return get_strict_type_match_level(inner_dest.get<ast::ts_const>(), expr_type_without_const, false) + 2;
			}
		}
		else
		{
			return get_strict_type_match_level(inner_dest, expr_type, false) + 5;
		}
	}
	else if (dest.is<ast::ts_auto_reference>())
	{
		auto const inner_dest = dest.get<ast::ts_auto_reference>();
		if (inner_dest.is<ast::ts_const>())
		{
			if (expr_type.is<ast::ts_const>())
			{
				return get_strict_type_match_level(inner_dest.get<ast::ts_const>(), expr_type_without_const, false) + 4;
			}
			else
			{
				return get_strict_type_match_level(inner_dest.get<ast::ts_const>(), expr_type_without_const, false) + 1;
			}
		}
		else
		{
			return get_strict_type_match_level(inner_dest, expr_type, false) + 4;
		}
	}
	else if (dest.is<ast::ts_auto_reference_const>())
	{
		auto const inner_dest = dest.get<ast::ts_auto_reference_const>();
		bz_assert(!inner_dest.is<ast::ts_const>());
		return get_strict_type_match_level(inner_dest, expr_type_without_const, false) + 3;
	}

	// only implicit type conversions are left
	// + 2 needs to be added everywhere, because it didn't match reference and reference const qualifier
	if (dest.is<ast::ts_auto>() && expr.is_tuple())
	{
		auto &tuple_expr = expr.get_tuple();
		match_level_t result = bz::vector<match_level_t>{};
		auto &result_vec = result.get<bz::vector<match_level_t>>();
		for (auto &elem : tuple_expr.elems)
		{
			result_vec.push_back(get_type_match_level(dest, elem, context));
		}
		return result;
	}
	else if (
		dest.is<ast::ts_array_slice>()
		&& (expr_type_without_const.is<ast::ts_array>() || (expr_type_without_const.is<ast::ts_array_slice>()))
	)
	{
		auto const dest_elem_t = dest.get<ast::ts_array_slice>().elem_type.as_typespec_view();
		auto const expr_elem_t = expr_type_without_const.is<ast::ts_array>()
			? expr_type_without_const.get<ast::ts_array>().elem_type.as_typespec_view()
			: expr_type_without_const.get<ast::ts_array_slice>().elem_type.as_typespec_view();
		auto const is_const_expr_elem_t= expr_type_without_const.is<ast::ts_array>()
			? expr_type.is<ast::ts_const>()
			: expr_elem_t.is<ast::ts_const>();
		auto const expr_elem_t_without_const = ast::remove_const_or_consteval(expr_elem_t);
		int const is_slice = expr_type_without_const.is<ast::ts_array_slice>() ? 1 : 0;
		if (dest_elem_t.is<ast::ts_const>())
		{
			if (is_const_expr_elem_t)
			{
				return get_strict_type_match_level(dest_elem_t.get<ast::ts_const>(), expr_elem_t_without_const, false) + (2 + is_slice);
			}
			else
			{
				return get_strict_type_match_level(dest_elem_t.get<ast::ts_const>(), expr_elem_t_without_const, false) + (1 + is_slice);
			}
		}
		else
		{
			if (is_const_expr_elem_t)
			{
				return match_level_t{};
			}
			else
			{
				return get_strict_type_match_level(dest_elem_t, expr_elem_t_without_const, false) + (2 + is_slice);
			}
		}
	}
	else if (dest.is<ast::ts_tuple>() && expr.is_tuple())
	{
		auto const &dest_tuple_types = dest.get<ast::ts_tuple>().types;
		auto &expr_tuple_elems = expr.get_tuple().elems;
		if (dest_tuple_types.size() == expr_tuple_elems.size())
		{
			match_level_t result = bz::vector<match_level_t>{};
			auto &result_vec = result.get<bz::vector<match_level_t>>();
			for (auto const &[elem, type] : bz::zip(expr_tuple_elems, dest_tuple_types))
			{
				result_vec.push_back(get_type_match_level(type, elem, context));
			}
			return result;
		}
	}
	else if (dest.is<ast::ts_auto>())
	{
		return get_strict_type_match_level(dest, expr_type_without_const, false);
	}
	else if (dest == expr_type_without_const)
	{
		return 2;
	}
	else if (is_implicitly_convertible(dest, expr, context))
	{
		return 1;
	}
	return match_level_t{};
}

static void strict_match_expression_to_type_impl(
	ast::expression &expr,
	ast::typespec &dest_container,
	ast::typespec_view source,
	ast::typespec_view dest,
	bool accept_void,
	parse_context &context
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
		auto const src_tokens = expr.src_tokens;
		expr = context.make_cast_expression(src_tokens, std::move(expr), dest_container);
	}
	else if (dest.is<ast::ts_auto>() && !source.is<ast::ts_const>())
	{
		bz_assert(!source.is<ast::ts_consteval>());
		dest_container.copy_from(dest, source);
	}
	else if (dest == source)
	{
		// nothing
	}
	else
	{
		bz_assert(expr.not_error());
		context.report_error(
			expr,
			bz::format("cannot convert expression from type '{}' to '{}'", expr.get_expr_type_and_kind().first, dest_container)
		);
		if (!ast::is_complete(dest_container))
		{
			dest_container.clear();
		}
		expr.to_error();
	}
}

static void match_typename_to_type_impl(
	ast::expression &expr,
	ast::typespec &dest_container,
	ast::typespec_view source,
	ast::typespec_view dest,
	parse_context &context
)
{
	// this function doesn't modify dest, it just checks if there's an error or not
	bz_assert(expr.is_typename());
	bz_assert(dest.is_typename());

	if (!ast::is_complete(source))
	{
		context.report_error(
			expr,
			bz::format("couldn't match non-complete type '{}' to typename type '{}'", expr.get_typename(), dest_container)
		);
		expr.to_error();
		dest_container.clear();
		return;
	}

	while (dest.kind() == source.kind() && dest.is_safe_blind_get() && source.is_safe_blind_get())
	{
		dest = dest.blind_get();
		source = source.blind_get();
	}

	if (dest.is<ast::ts_typename>())
	{
		return;
	}
	else if (dest.kind() != source.kind())
	{
		context.report_error(expr, bz::format("couldn't match type '{}' to typename type '{}'", expr.get_typename(), dest_container));
		expr.to_error();
		dest_container.clear();
		return;
	}
	else if (dest.is<ast::ts_array>())
	{
		match_typename_to_type_impl(
			expr, dest_container,
			source.get<ast::ts_array>().elem_type, dest.get<ast::ts_array>().elem_type,
			context
		);
		return;
	}
	else if (dest.is<ast::ts_array_slice>())
	{
		match_typename_to_type_impl(
			expr, dest_container,
			source.get<ast::ts_array_slice>().elem_type, dest.get<ast::ts_array_slice>().elem_type,
			context
		);
		return;
	}
	else
	{
		bz_unreachable;
	}
}

static void match_expression_to_type_impl(
	ast::expression &expr,
	ast::typespec &dest_container,
	ast::typespec_view dest,
	parse_context &context
)
{
	// a slightly different implementation of get_type_match_level
	if (expr.is_if_expr())
	{
		auto &if_expr = expr.get_if_expr();
		if (ast::is_complete(dest))
		{
			match_expression_to_type_impl(if_expr.then_block, dest_container, dest, context);
			match_expression_to_type_impl(if_expr.else_block, dest_container, dest, context);
			return;
		}
		else
		{
			ast::typespec then_matched_type = dest_container;
			ast::typespec else_matched_type = dest_container;
			context.match_expression_to_type(if_expr.then_block, then_matched_type);
			context.match_expression_to_type(if_expr.else_block, else_matched_type);

			if (then_matched_type.is_empty() || else_matched_type.is_empty())
			{
				expr.to_error();
				if (!ast::is_complete(dest_container))
				{
					dest_container.clear();
				}
				return;
			}

			if (then_matched_type == else_matched_type)
			{
				match_expression_to_type_impl(if_expr.then_block, dest_container, dest, context);
				match_expression_to_type_impl(if_expr.else_block, dest_container, dest, context);
				expr.set_type(std::move(then_matched_type));
				return;
			}

			auto after_match_then_result = get_type_match_level(else_matched_type, if_expr.then_block, context);
			auto after_match_else_result = get_type_match_level(then_matched_type, if_expr.else_block, context);
			if (after_match_then_result.is_null() && after_match_else_result.is_null())
			{
				context.report_error(
					expr,
					bz::format("couldn't match the two branches of the if expression at the same time to type '{}'", dest_container),
					{
						context.make_note(
							if_expr.then_block,
							bz::format("resulting type from matching the then branch is '{}'", then_matched_type)
						),
						context.make_note(
							if_expr.else_block,
							bz::format("resulting type from matching the else branch is '{}'", else_matched_type)
						)
					}
				);
				expr.to_error();
				if (!ast::is_complete(dest_container))
				{
					dest_container.clear();
				}
				return;
			}
			else if (after_match_then_result.is_null())
			{
				// match to the then branch first
				match_expression_to_type_impl(if_expr.then_block, dest_container, dest, context);
				match_expression_to_type_impl(if_expr.else_block, dest_container, dest, context);
				expr.set_type(std::move(then_matched_type));
				return;
			}
			else if (after_match_else_result.is_null())
			{
				// match to the else branch first
				match_expression_to_type_impl(if_expr.else_block, dest_container, dest, context);
				match_expression_to_type_impl(if_expr.then_block, dest_container, dest, context);
				expr.set_type(std::move(else_matched_type));
				return;
			}
			else
			{
				context.report_error(
					expr,
					bz::format("matching the two branches of the if expression to type '{}' is ambiguous", dest_container),
					{
						context.make_note(
							if_expr.then_block,
							bz::format("resulting type from matching the then branch is '{}'", then_matched_type)
						),
						context.make_note(
							if_expr.else_block,
							bz::format("resulting type from matching the else branch is '{}'", else_matched_type)
						)
					}
				);
				expr.to_error();
				if (!ast::is_complete(dest_container))
				{
					dest_container.clear();
				}
				return;
			}
		}
	}
	else if (expr.is_switch_expr())
	{
		bz_unreachable;
	}
	else if (expr.is_typename())
	{
		if (!dest.is_typename())
		{
			context.report_error(expr, bz::format("cannot match type '{}' to a non-typename type '{}'", expr.get_typename(), dest_container));
			expr.to_error();
			if (!ast::is_complete(dest_container))
			{
				dest_container.clear();
			}
			return;
		}

		match_typename_to_type_impl(expr, dest_container, expr.get_typename(), dest, context);
		return;
	}

	if (dest.is<ast::ts_const>())
	{
		match_expression_to_type_impl(expr, dest_container, dest.get<ast::ts_const>(), context);
		return;
	}
	else if (dest.is<ast::ts_consteval>())
	{
		parse::consteval_try(expr, context);
		if (!expr.is<ast::constant_expression>())
		{
			context.report_error(expr, "expression must be a constant expression");
			if (!ast::is_complete(dest_container))
			{
				dest_container.clear();
			}
			expr.to_error();
			return;
		}
		else
		{
			match_expression_to_type_impl(expr, dest_container, dest.get<ast::ts_consteval>(), context);
			parse::consteval_try(expr, context);
			if (!expr.is<ast::constant_expression>())
			{
				context.report_error(expr, "expression must be a constant expression");
				if (!ast::is_complete(dest_container))
				{
					dest_container.clear();
				}
				expr.to_error();
				return;
			}
			return;
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
					strict_match_expression_to_type_impl(
						expr, dest_container,
						inner_expr_type.get<ast::ts_const>(), inner_dest.get<ast::ts_const>(),
						true, context
					);
					return;
				}
				else
				{
					strict_match_expression_to_type_impl(
						expr, dest_container,
						inner_expr_type, inner_dest.get<ast::ts_const>(),
						true, context
					);
					return;
				}
			}
			else
			{
				strict_match_expression_to_type_impl(
					expr, dest_container,
					inner_expr_type, inner_dest,
					true, context
				);
				return;
			}
		}
		// special case for null
		else if (expr_type_without_const.is<ast::ts_base_type>() && expr_type_without_const.get<ast::ts_base_type>().info->kind == ast::type_info::null_t_)
		{
			if (ast::is_complete(dest))
			{
				expr = context.make_cast_expression(expr.src_tokens, std::move(expr), dest);
				return;
			}
			else
			{
				bz_assert(!ast::is_complete(dest_container));
				context.report_error(expr, bz::format("cannot convert 'null' to incomplete type '{}'", dest_container));
				dest_container.clear();
				expr.to_error();
				return;
			}
		}
	}
	else if (dest.is<ast::ts_lvalue_reference>())
	{
		if (expr_type_kind != ast::expression_type_kind::lvalue && expr_type_kind != ast::expression_type_kind::lvalue_reference)
		{
			context.report_error(expr, bz::format("cannot bind an rvalue to an lvalue reference '{}'", dest_container));
			if (!ast::is_complete(dest_container))
			{
				dest_container.clear();
			}
			expr.to_error();
			return;
		}

		auto const inner_dest = dest.get<ast::ts_lvalue_reference>();
		if (inner_dest.is<ast::ts_const>())
		{
			strict_match_expression_to_type_impl(
				expr, dest_container,
				expr_type_without_const, inner_dest.get<ast::ts_const>(),
				false, context
			);
			if (expr.not_error() && expr_type_kind == ast::expression_type_kind::lvalue)
			{
				ast::typespec result_type = expr_type;
				expr = ast::make_dynamic_expression(
					expr.src_tokens,
					ast::expression_type_kind::lvalue_reference, std::move(result_type),
					ast::make_expr_take_reference(std::move(expr))
				);
			}
			return;
		}
		else
		{
			strict_match_expression_to_type_impl(
				expr, dest_container,
				expr_type, inner_dest,
				false, context
			);
			return;
		}
	}
	else if (dest.is<ast::ts_auto_reference>())
	{
		bz_assert(dest_container.nodes.front().is<ast::ts_auto_reference>());
		auto const inner_dest = dest.get<ast::ts_auto_reference>();
		if (inner_dest.is<ast::ts_const>())
		{
			strict_match_expression_to_type_impl(
				expr, dest_container,
				expr_type_without_const, inner_dest.get<ast::ts_const>(),
				false, context
			);
			if (expr.not_error() && expr_type_kind == ast::expression_type_kind::lvalue)
			{
				ast::typespec result_type = expr_type;
				expr = ast::make_dynamic_expression(
					expr.src_tokens,
					ast::expression_type_kind::lvalue_reference, std::move(result_type),
					ast::make_expr_take_reference(std::move(expr))
				);
			}
		}
		else
		{
			strict_match_expression_to_type_impl(
				expr, dest_container,
				expr_type, inner_dest,
				false, context
			);
		}
		if (expr_type_kind == ast::expression_type_kind::lvalue || expr_type_kind == ast::expression_type_kind::lvalue_reference)
		{
			auto const ref_pos = dest_container.nodes.front().get<ast::ts_auto_reference>().auto_reference_pos;
			dest_container.nodes.front().emplace<ast::ts_lvalue_reference>(ref_pos);
		}
		else
		{
			dest_container.remove_layer();
		}
		return;
	}
	else if (dest.is<ast::ts_auto_reference_const>())
	{
		bz_assert(dest_container.nodes.front().is<ast::ts_auto_reference_const>());
		auto const inner_dest = dest.get<ast::ts_auto_reference_const>();
		bz_assert(!inner_dest.is<ast::ts_const>());
		strict_match_expression_to_type_impl(
			expr, dest_container,
			expr_type_without_const, inner_dest,
			false, context
		);
		if (expr.not_error() && expr_type_kind == ast::expression_type_kind::lvalue)
		{
			ast::typespec result_type = expr_type;
			expr = ast::make_dynamic_expression(
				expr.src_tokens,
				ast::expression_type_kind::lvalue_reference, std::move(result_type),
				ast::make_expr_take_reference(std::move(expr))
			);
		}

		if (
			expr_type.is<ast::ts_const>()
			&& (
				expr_type_kind == ast::expression_type_kind::lvalue
				|| expr_type_kind == ast::expression_type_kind::lvalue_reference
			)
		)
		{
			auto const ref_pos = dest_container.nodes.front().get<ast::ts_auto_reference_const>().auto_reference_const_pos;
			dest_container.nodes.front().emplace<ast::ts_const>(ref_pos);
			dest_container.add_layer<ast::ts_lvalue_reference>(ref_pos);
		}
		else if (ast::is_lvalue(expr_type_kind))
		{
			auto const ref_pos = dest_container.nodes.front().get<ast::ts_auto_reference_const>().auto_reference_const_pos;
			dest_container.nodes.front().emplace<ast::ts_lvalue_reference>(ref_pos);
		}
		else
		{
			dest_container.remove_layer();
		}
		return;
	}

	// only implicit type conversions are left
	if (dest.is<ast::ts_auto>() && ast::is_complete(expr_type_without_const))
	{
		dest_container.copy_from(dest, expr_type_without_const);
		dest = ast::remove_const_or_consteval(dest_container);
	}

	if (expr.is<ast::constant_expression>() && expr.is_tuple())
	{
		auto &const_expr = expr.get<ast::constant_expression>();
		auto kind = const_expr.kind;
		auto type = std::move(const_expr.type);
		auto inner_expr = std::move(const_expr.expr);
		expr.emplace<ast::dynamic_expression>(kind, std::move(type), std::move(inner_expr));
		expr.consteval_state = ast::expression::consteval_never_tried;
	}

	if (dest.is<ast::ts_auto>() && expr.is_tuple())
	{
		auto &tuple_expr = expr.get_tuple();
		ast::typespec tuple_type = ast::make_tuple_typespec(dest.get_src_tokens(), {});
		auto &tuple_type_vec = tuple_type.nodes.front().get<ast::ts_tuple>().types;
		tuple_type_vec.resize(tuple_expr.elems.size());
		for (auto &type : tuple_type_vec)
		{
			type = ast::make_auto_typespec(nullptr);
		}
		for (auto const &[expr, type] : bz::zip(tuple_expr.elems, tuple_type_vec))
		{
			match_expression_to_type_impl(expr, type, type, context);
		}
		expr.set_type(tuple_type);
		dest_container.move_from(dest, tuple_type);
		return;
	}
	else if (dest.is<ast::ts_tuple>() && expr.is_tuple())
	{
		bz_assert(dest_container.nodes.back().is<ast::ts_tuple>());
		auto &dest_tuple_types = dest_container.nodes.back().get<ast::ts_tuple>().types;
		auto &expr_tuple_elems = expr.get_tuple().elems;
		if (dest_tuple_types.size() == expr_tuple_elems.size())
		{
			for (auto const &[expr, type] : bz::zip(expr_tuple_elems, dest_tuple_types))
			{
				match_expression_to_type_impl(expr, type, type, context);
			}
			expr.set_type(dest);
			return;
		}
	}
	else if (dest.is<ast::ts_array_slice>())
	{
		bz_assert(dest_container.nodes.back().is<ast::ts_array_slice>());
		auto &dest_elem_container = dest_container.nodes.back().get<ast::ts_array_slice>().elem_type;
		auto const dest_elem_t = dest_elem_container.as_typespec_view();
		auto const expr_elem_t = expr_type_without_const.is<ast::ts_array>()
			? expr_type_without_const.get<ast::ts_array>().elem_type.as_typespec_view()
			: expr_type_without_const.get<ast::ts_array_slice>().elem_type.as_typespec_view();
		auto const is_const_expr_elem_t= expr_type_without_const.is<ast::ts_array>()
			? expr_type.is<ast::ts_const>()
			: expr_elem_t.is<ast::ts_const>();
		auto const expr_elem_t_without_const = ast::remove_const_or_consteval(expr_elem_t);
		if (dest_elem_t.is<ast::ts_const>())
		{
			strict_match_expression_to_type_impl(
				expr, dest_elem_container,
				expr_elem_t_without_const, dest_elem_t.get<ast::ts_const>(),
				false, context
			);
		}
		else if (!is_const_expr_elem_t)
		{
			strict_match_expression_to_type_impl(
				expr, dest_elem_container,
				expr_elem_t_without_const, dest_elem_t,
				false, context
			);
		}

		if (expr.not_error() && expr_type_without_const.is<ast::ts_array>())
		{
			auto const src_tokens = expr.src_tokens;
			expr = context.make_cast_expression(src_tokens, std::move(expr), dest_container);
		}
		return;
	}
	else if (dest == expr_type_without_const)
	{
		if (expr_type_kind == ast::expression_type_kind::rvalue)
		{
			return;
		}

		if (dest.is<ast::ts_base_type>())
		{
			auto &info = *dest.get<ast::ts_base_type>().info;
			if (info.kind != ast::type_info::aggregate)
			{
				return;
			}
			auto const copy_ctor = info.copy_constructor == nullptr ? info.default_copy_constructor.get() : info.copy_constructor;
			auto const src_tokens = expr.src_tokens;
			bz::vector<ast::expression> params;
			params.emplace_back(std::move(expr));
			expr = make_expr_function_call_from_body(src_tokens, copy_ctor, std::move(params), context);
		}
		else if (dest.is<ast::ts_array>())
		{
			// bz_unreachable;
		}
		else if (dest.is<ast::ts_tuple>())
		{
			// bz_unreachable;
		}
		return;
	}
	else if (is_implicitly_convertible(dest, expr, context))
	{
		expr = context.make_cast_expression(expr.src_tokens, std::move(expr), dest);
		return;
	}

	context.report_error(expr, bz::format("cannot implicitly convert expression from type '{}' to '{}'", expr_type, dest_container));
	if (!ast::is_complete(dest_container))
	{
		dest_container.clear();
	}
	expr.to_error();
}

static match_level_t get_function_call_match_level(
	ast::statement_view func_stmt,
	ast::function_body &func_body,
	bz::array_view<ast::expression> params,
	parse_context &context,
	lex::src_tokens src_tokens
)
{
	if (func_body.params.size() != params.size())
	{
		return match_level_t{};
	}

	if (func_body.state < ast::resolve_state::parameters)
	{
		context.add_to_resolve_queue(src_tokens, func_body);
		parse::resolve_function_parameters(func_stmt, func_body, context);
		context.pop_resolve_queue();
	}

	if (func_body.state < ast::resolve_state::parameters)
	{
		return match_level_t{};
	}

	match_level_t result = bz::vector<match_level_t>{};
	auto &result_vec = result.get<bz::vector<match_level_t>>();

	auto const add_to_result = [&](match_level_t match)
	{
		if (match.not_null())
		{
			result_vec.push_back(std::move(match));
		}
	};

	auto params_it = func_body.params.begin();
	auto call_it  = params.begin();
	auto const types_end = func_body.params.end();
	for (; params_it != types_end; ++call_it, ++params_it)
	{
		add_to_result(get_type_match_level(params_it->get_type(), *call_it, context));
	}
	if (result_vec.size() != func_body.params.size())
	{
		result.clear();
	}
	return result;
}

static match_level_t get_function_call_match_level(
	ast::statement_view func_stmt,
	ast::function_body &func_body,
	ast::expression &expr,
	parse_context &context,
	lex::src_tokens src_tokens
)
{
	if (func_body.params.size() != 1)
	{
		return match_level_t{};
	}


	if (func_body.state < ast::resolve_state::parameters)
	{
		context.add_to_resolve_queue(src_tokens, func_body);
		parse::resolve_function_parameters(func_stmt, func_body, context);
		context.pop_resolve_queue();
	}

	if (func_body.state < ast::resolve_state::parameters)
	{
		return match_level_t{};
	}

	return get_type_match_level(func_body.params[0].get_type(), expr, context);
}

static match_level_t get_function_call_match_level(
	ast::statement_view func_stmt,
	ast::function_body &func_body,
	ast::expression &lhs,
	ast::expression &rhs,
	parse_context &context,
	lex::src_tokens src_tokens
)
{
	if (func_body.params.size() != 2)
	{
		return match_level_t{};
	}


	if (func_body.state < ast::resolve_state::parameters)
	{
		context.add_to_resolve_queue(src_tokens, func_body);
		parse::resolve_function_parameters(func_stmt, func_body, context);
		context.pop_resolve_queue();
	}

	if (func_body.state < ast::resolve_state::parameters)
	{
		return match_level_t{};
	}

	match_level_t result = bz::vector<match_level_t>{};
	auto &result_vec = result.get<bz::vector<match_level_t>>();
	result_vec.push_back(get_type_match_level(func_body.params[0].get_type(), lhs, context));
	result_vec.push_back(get_type_match_level(func_body.params[1].get_type(), rhs, context));
	if (result_vec[0].is_null() || result_vec[1].is_null())
	{
		result.clear();
	}
	return result;
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

struct possible_func_t
{
	match_level_t match_level;
	ast::statement_view stmt;
	ast::function_body *func_body;
};

static std::pair<ast::statement_view, ast::function_body *> find_best_match(
	lex::src_tokens src_tokens,
	bz::array_view<const possible_func_t> possible_funcs,
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
			.filter([&](auto const &func) { return &*max_match_it == &func || match_level_compare(max_match_it->match_level, func.match_level) == 0; });
		if (filtered_funcs.count() == 1)
		{
			return { max_match_it->stmt, max_match_it->func_body };
		}
		else
		{
			bz::vector<source_highlight> notes;
			notes.reserve(possible_funcs.size());
			for (auto &func : filtered_funcs)
			{
				if (func.func_body->src_tokens.pivot == nullptr)
				{
					notes.emplace_back(context.make_note(func.func_body->get_candidate_message()));
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
			context.report_error(src_tokens, "function call is ambiguous", std::move(notes));
			return { {}, nullptr };
		}
	}

	// report all failed function error
	bz::vector<source_highlight> notes;
	notes.reserve(possible_funcs.size());
	for (auto &func : possible_funcs)
	{
		if (func.func_body->src_tokens.pivot == nullptr)
		{
			notes.emplace_back(context.make_note(func.func_body->get_candidate_message()));
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

static ast::expression make_expr_function_call_from_body(
	lex::src_tokens src_tokens,
	ast::function_body *body,
	bz::vector<ast::expression> params,
	parse_context &context,
	ast::resolve_order resolve_order
)
{
	if (body->is_generic())
	{
		auto required_from = get_generic_requirements(src_tokens, context);
		auto specialized_body = body->get_copy_for_generic_specialization(std::move(required_from));
		context.add_to_resolve_queue(src_tokens, *specialized_body);
		for (auto const [param, func_body_param] : bz::zip(params, specialized_body->params))
		{
			context.match_expression_to_variable(param, func_body_param);
			if (ast::is_generic_parameter(func_body_param))
			{
				func_body_param.init_expr = param;
			}
		}
		context.pop_resolve_queue();
		body = body->add_specialized_body(std::move(specialized_body));
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
		for (auto const [param, func_body_param] : bz::zip(params, body->params))
		{
			context.match_expression_to_variable(param, func_body_param);
		}
	}
	parse::resolve_function_symbol({}, *body, context);
	context.pop_resolve_queue();
	if (body->state == ast::resolve_state::error)
	{
		return ast::make_error_expression(src_tokens, ast::make_expr_function_call(src_tokens, std::move(params), body, resolve_order));
	}

	auto &ret_t = body->return_type;
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
		ast::make_expr_function_call(src_tokens, std::move(params), body, resolve_order)
	);
}

static ast::expression make_expr_function_call_from_body(
	lex::src_tokens src_tokens,
	ast::function_body *body,
	bz::vector<ast::expression> params,
	ast::constant_value value,
	parse_context &context,
	ast::resolve_order resolve_order = ast::resolve_order::regular
)
{
	if (body->is_generic())
	{
		auto required_from = get_generic_requirements(src_tokens, context);
		auto specialized_body = body->get_copy_for_generic_specialization(std::move(required_from));
		context.add_to_resolve_queue(src_tokens, *specialized_body);
		for (auto const [param, func_body_param] : bz::zip(params, specialized_body->params))
		{
			context.match_expression_to_variable(param, func_body_param);
			if (ast::is_generic_parameter(func_body_param))
			{
				func_body_param.init_expr = param;
			}
		}
		context.pop_resolve_queue();
		body = body->add_specialized_body(std::move(specialized_body));
		context.add_to_resolve_queue(src_tokens, *body);
		bz_assert(!body->is_generic());
		if (!context.generic_functions.contains(body))
		{
			context.generic_functions.push_back(body);
		}
	}
	else
	{
		context.add_to_resolve_queue(src_tokens, *body);
		for (auto const [param, func_body_param] : bz::zip(params, body->params))
		{
			context.match_expression_to_variable(param, func_body_param);
		}
	}
	parse::resolve_function_symbol({}, *body, context);
	context.pop_resolve_queue();
	if (body->state == ast::resolve_state::error)
	{
		return ast::make_error_expression(src_tokens, ast::make_expr_function_call(src_tokens, std::move(params), body, resolve_order));
	}

	auto &ret_t = body->return_type;
	auto return_type_kind = ast::expression_type_kind::rvalue;
	auto return_type = ast::remove_const_or_consteval(ret_t);
	if (ret_t.is<ast::ts_lvalue_reference>())
	{
		return_type_kind = ast::expression_type_kind::lvalue_reference;
		return_type = ret_t.get<ast::ts_lvalue_reference>();
	}
	return ast::make_constant_expression(
		src_tokens,
		return_type_kind, return_type,
		std::move(value),
		ast::make_expr_function_call(src_tokens, std::move(params), body, resolve_order)
	);
}

static bz::vector<possible_func_t> get_possible_funcs_for_operator(
	uint32_t op,
	lex::src_tokens src_tokens,
	ast::expression &expr,
	parse_context &context
)
{
	auto const expr_scope = [&]() -> bz::array_view<bz::u8string_view const> {
		auto const expr_t = ast::remove_const_or_consteval(expr.get_expr_type_and_kind().first);
		if (expr_t.is<ast::ts_base_type>())
		{
			auto const &id = expr_t.get<ast::ts_base_type>().info->type_name;
			if (id.is_qualified)
			{
				return id.values;
			}
		}
		return {};
	}();
	auto const current_scope = context.current_scope;

	bz::vector<possible_func_t> possible_funcs = {};
	auto const compare_scopes = [](
		bz::array_view<bz::u8string_view const> op_scope,
		bz::array_view<bz::u8string_view const> current_scope
	) {
		return op_scope.size() <= current_scope.size() && std::equal(op_scope.begin(), op_scope.end(), current_scope.begin());
	};

	// we go through the scope decls for a matching declaration
	for (auto &scope : context.scope_decls.reversed())
	{
		auto const filtered_sets = scope.op_sets
			.filter([op](auto const &op_set) {
				return op == op_set.op;
			});
		for (auto const &op_set : filtered_sets)
		{
			for (auto &op : op_set.op_decls)
			{
				auto &body = op.get<ast::decl_operator>().body;
				auto match_level = get_function_call_match_level(op, body, expr, context, src_tokens);
				possible_funcs.push_back({ std::move(match_level), op, &body });
			}
		}
	}

	auto &global_decls = *context.global_decls;
	auto const filtered_sets = global_decls.op_sets
		.filter([op, expr_scope, current_scope, compare_scopes](auto const &op_set) {
			return op == op_set.op
				&& (
					compare_scopes(op_set.scope, current_scope)
					|| compare_scopes(op_set.scope, expr_scope)
				);
		});
	for (auto const &op_set : filtered_sets)
	{
		for (auto &op : op_set.op_decls)
		{
			auto &body = op.get<ast::decl_operator>().body;
			auto match_level = get_function_call_match_level(op, body, expr, context, src_tokens);
			possible_funcs.push_back({ std::move(match_level), op, &body });
		}
	}

	return possible_funcs;
}

ast::expression parse_context::make_unary_operator_expression(
	lex::src_tokens src_tokens,
	uint32_t op_kind,
	ast::expression expr
)
{
	if (expr.is_error())
	{
		bz_assert(this->has_errors());
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
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

	auto [type, type_kind] = expr.get_expr_type_and_kind();
	// if it's a non-overloadable operator or a built-in with a built-in type operand,
	// user-defined operators shouldn't be looked at
	if (
		!is_unary_overloadable_operator(op_kind)
		|| (is_builtin_type(ast::remove_const_or_consteval(type)) && is_unary_builtin_operator(op_kind))
	)
	{
		auto result = make_builtin_operation(src_tokens, op_kind, std::move(expr), *this);
		result.src_tokens = src_tokens;
		return result;
	}

	auto const possible_funcs = get_possible_funcs_for_operator(op_kind, src_tokens, expr, *this);
	if (possible_funcs.empty())
	{
		this->report_error(
			src_tokens,
			bz::format(
				"no candidate found for unary 'operator {}' with type '{}'",
				token_info[op_kind].token_value, expr.get_expr_type_and_kind().first
			)
		);
		return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
	}
	else
	{
		auto const [_, best_body] = find_best_match(src_tokens, possible_funcs, *this);
		if (best_body == nullptr)
		{
			return ast::make_error_expression(src_tokens, ast::make_expr_unary_op(op_kind, std::move(expr)));
		}
		else
		{
			bz::vector<ast::expression> params;
			params.emplace_back(std::move(expr));
			return make_expr_function_call_from_body(src_tokens, best_body, std::move(params), *this);
		}
	}
}

static bz::vector<possible_func_t> get_possible_funcs_for_operator(
	uint32_t op,
	lex::src_tokens src_tokens,
	ast::expression &lhs,
	ast::expression &rhs,
	parse_context &context
)
{
	auto const lhs_t = ast::remove_const_or_consteval(lhs.get_expr_type_and_kind().first);
	auto const lhs_scope = [&]() -> bz::array_view<bz::u8string_view const> {
		if (lhs_t.is<ast::ts_base_type>())
		{
			auto const &id = lhs_t.get<ast::ts_base_type>().info->type_name;
			if (id.is_qualified)
			{
				return id.values;
			}
		}
		return {};
	}();
	auto const rhs_t = ast::remove_const_or_consteval(rhs.get_expr_type_and_kind().first);
	auto const rhs_scope = [&]() -> bz::array_view<bz::u8string_view const> {
		if (rhs_t.is<ast::ts_base_type>())
		{
			auto const &id = rhs_t.get<ast::ts_base_type>().info->type_name;
			if (id.is_qualified)
			{
				return id.values;
			}
		}
		return {};
	}();
	auto const current_scope = context.current_scope;

	bz::vector<possible_func_t> possible_funcs = {};
	auto const compare_scopes = [](
		bz::array_view<bz::u8string_view const> op_scope,
		bz::array_view<bz::u8string_view const> current_scope
	) {
		return op_scope.size() <= current_scope.size() && std::equal(op_scope.begin(), op_scope.end(), current_scope.begin());
	};

	// we go through the scope decls for a matching declaration
	for (auto &scope : context.scope_decls.reversed())
	{
		auto const filtered_sets = scope.op_sets
			.filter([op](auto const &op_set) {
				return op == op_set.op;
			});
		for (auto const &op_set : filtered_sets)
		{
			for (auto &op : op_set.op_decls)
			{
				auto &body = op.get<ast::decl_operator>().body;
				auto match_level = get_function_call_match_level(op, body, lhs, rhs, context, src_tokens);
				possible_funcs.push_back({ std::move(match_level), op, &body });
			}
		}
	}

	auto &global_decls = *context.global_decls;
	auto const filtered_sets = global_decls.op_sets
		.filter([op, lhs_scope, rhs_scope, current_scope, compare_scopes](auto const &op_set) {
			return op == op_set.op
				&& (
					compare_scopes(op_set.scope, current_scope)
					|| compare_scopes(op_set.scope, lhs_scope)
					|| compare_scopes(op_set.scope, rhs_scope)
				);
		});
	for (auto const &op_set : filtered_sets)
	{
		for (auto &op_decl : op_set.op_decls)
		{
			auto &body = op_decl.get<ast::decl_operator>().body;
			auto match_level = get_function_call_match_level(op_decl, body, lhs, rhs, context, src_tokens);
			possible_funcs.push_back({ std::move(match_level), op_decl, &body });
		}
	}

	if (op == lex::token::assign && lhs_t.is<ast::ts_base_type>())
	{
		auto const lhs_info = lhs_t.get<ast::ts_base_type>().info;
		if (lhs_info->op_assign == nullptr && lhs_info->op_move_assign == nullptr)
		{
			auto &op_assign = *lhs_info->default_op_assign;
			auto op_assign_match_level = get_function_call_match_level({}, op_assign, lhs, rhs, context, src_tokens);
			possible_funcs.push_back({ std::move(op_assign_match_level), {}, &op_assign });
			auto &op_move_assign = *lhs_info->default_op_move_assign;
			auto op_move_assign_match_level = get_function_call_match_level({}, op_move_assign, lhs, rhs, context, src_tokens);
			possible_funcs.push_back({ std::move(op_move_assign_match_level), {}, &op_move_assign });
		}
	}

	return possible_funcs;
}

ast::expression parse_context::make_binary_operator_expression(
	lex::src_tokens src_tokens,
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
		// there's no operator such as this ('as' is handled earlier)
		bz_unreachable;
	}

	auto const [lhs_type, lhs_type_kind] = lhs.get_expr_type_and_kind();
	auto const [rhs_type, rhs_type_kind] = rhs.get_expr_type_and_kind();
	// if it's a non-overloadable operator or a built-in with a built-in type operand,
	// user-defined operators shouldn't be looked at
	if (
		!is_binary_overloadable_operator(op_kind)
		|| (
			is_builtin_type(ast::remove_const_or_consteval(lhs_type))
			&& is_builtin_type(ast::remove_const_or_consteval(rhs_type))
			&& is_binary_builtin_operator(op_kind)
		)
	)
	{
		auto result = make_builtin_operation(src_tokens, op_kind, std::move(lhs), std::move(rhs), *this);
		result.src_tokens = src_tokens;
		return result;
	}

	auto const possible_funcs = get_possible_funcs_for_operator(op_kind, src_tokens, lhs, rhs, *this);
	if (possible_funcs.empty())
	{
		this->report_error(
			src_tokens,
			bz::format(
				"no candidate found for binary 'operator {}' with types '{}' and '{}'",
				token_info[op_kind].token_value, lhs.get_expr_type_and_kind().first, rhs.get_expr_type_and_kind().first
			)
		);
		return ast::make_error_expression(src_tokens, ast::make_expr_binary_op(op_kind, std::move(lhs), std::move(rhs)));
	}
	else
	{
		auto const [best_stmt, best_body] = find_best_match(src_tokens, possible_funcs, *this);
		if (best_body == nullptr)
		{
			return ast::make_error_expression(src_tokens, ast::make_expr_binary_op(op_kind, std::move(lhs), std::move(rhs)));
		}
		else
		{
			bz::vector<ast::expression> params;
			params.emplace_back(std::move(lhs));
			params.emplace_back(std::move(rhs));
			auto const resolve_order = get_binary_precedence(op_kind).is_left_associative
				? ast::resolve_order::regular
				: ast::resolve_order::reversed;
			return make_expr_function_call_from_body(src_tokens, best_body, std::move(params), *this, resolve_order);
		}
	}
}

static bz::vector<possible_func_t> get_possible_funcs_for_unqualified_id(
	bz::array_view<bz::u8string_view const> id,
	lex::src_tokens src_tokens,
	bz::array_view<ast::expression> params,
	parse_context &context
)
{
	if (id.size() == 1)
	{
		for (auto &scope : context.scope_decls.reversed())
		{
			auto const set = std::find_if(
				scope.func_sets.begin(), scope.func_sets.end(),
				[id](auto const &fn_set) {
					bz_assert(!fn_set.id.is_qualified && fn_set.id.values.size() == 1);
					return id.front() == fn_set.id.values.front();
				}
			);
			if (set != scope.func_sets.end())
			{
				bz::vector<possible_func_t> possible_funcs = {};
				for (auto &fn : set->func_decls)
				{
					auto &body = fn.get<ast::decl_function>().body;
					auto match_level = get_function_call_match_level(fn, body, params, context, src_tokens);
					possible_funcs.push_back({ std::move(match_level), fn, &body });
				}
				for (auto &alias_decl : set->alias_decls)
				{
					auto &alias = alias_decl.get<ast::decl_function_alias>();
					context.add_to_resolve_queue(src_tokens, alias);
					parse::resolve_function_alias(alias, context);
					context.pop_resolve_queue();
					for (auto const body : alias.aliased_bodies)
					{
						auto match_level = get_function_call_match_level({}, *body, params, context, src_tokens);
						possible_funcs.push_back({ std::move(match_level), alias_decl, body });
					}
				}
				return possible_funcs;
			}
		}
	}

	bz::vector<possible_func_t> possible_funcs = {};
	context.global_decls->func_sets
		.filter([&context, id](auto const &fn_set) {
			auto const id_size = id.size();
			return fn_set.id.values.size() >= id_size
				&& fn_set.id.values.size() <= id_size + context.current_scope.size()
				&& std::equal(fn_set.id.values.end() - id_size, fn_set.id.values.end(), id.begin())
				&& std::equal(fn_set.id.values.begin(), fn_set.id.values.end() - id_size, context.current_scope.begin());
		})
		.for_each([&](auto const &fn_set) {
			for (auto &fn : fn_set.func_decls)
			{
				auto &body = fn.template get<ast::decl_function>().body;
				auto match_level = get_function_call_match_level(fn, body, params, context, src_tokens);
				possible_funcs.push_back({ std::move(match_level), fn, &body });
			}
			for (auto &alias_decl : fn_set.alias_decls)
			{
				auto &alias = alias_decl.template get<ast::decl_function_alias>();
				context.add_to_resolve_queue(src_tokens, alias);
				parse::resolve_function_alias(alias, context);
				context.pop_resolve_queue();
				for (auto const body : alias.aliased_bodies)
				{
					auto match_level = get_function_call_match_level({}, *body, params, context, src_tokens);
					possible_funcs.push_back({ std::move(match_level), alias_decl, body });
				}
			}
		});
	return possible_funcs;
}

static bz::vector<possible_func_t> get_possible_funcs_for_qualified_id(
	bz::array_view<bz::u8string_view const> id,
	lex::src_tokens src_tokens,
	bz::array_view<ast::expression> params,
	parse_context &context
)
{
	bz::vector<possible_func_t> possible_funcs = {};
	context.global_decls->func_sets
		.filter([id](auto const &fn_set) {
			return fn_set.id.values.size() == id.size()
				&& std::equal(fn_set.id.values.begin(), fn_set.id.values.end(), id.begin());
		})
		.for_each([&](auto const &fn_set) {
			for (auto &fn : fn_set.func_decls)
			{
				auto &body = fn.template get<ast::decl_function>().body;
				auto match_level = get_function_call_match_level(fn, body, params, context, src_tokens);
				possible_funcs.push_back({ std::move(match_level), fn, &body });
			}
			for (auto &alias_decl : fn_set.alias_decls)
			{
				auto &alias = alias_decl.template get<ast::decl_function_alias>();
				context.add_to_resolve_queue(src_tokens, alias);
				parse::resolve_function_alias(alias, context);
				context.pop_resolve_queue();
				for (auto const body : alias.aliased_bodies)
				{
					auto match_level = get_function_call_match_level({}, *body, params, context, src_tokens);
					possible_funcs.push_back({ std::move(match_level), alias_decl, body });
				}
			}
		});
	return possible_funcs;
}

ast::expression parse_context::make_function_call_expression(
	lex::src_tokens src_tokens,
	ast::expression called,
	bz::vector<ast::expression> params
)
{
	if (called.is_error())
	{
		bz_assert(this->has_errors());
		return ast::make_error_expression(
			src_tokens,
			ast::make_expr_function_call(src_tokens, std::move(params), nullptr, ast::resolve_order::regular)
		);
	}

	for (auto &p : params)
	{
		if (p.is_error())
		{
			bz_assert(this->has_errors());
			return ast::make_error_expression(
				src_tokens,
				ast::make_expr_function_call(src_tokens, std::move(params), nullptr, ast::resolve_order::regular)
			);
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
			if (get_function_call_match_level({}, *func_body, params, *this, src_tokens).is_null())
			{
				if (func_body->state != ast::resolve_state::error)
				{
					if (func_body->is_intrinsic())
					{
						this->report_error(
							src_tokens,
							"couldn't match the function call to the function",
							{ this->make_note(func_body->get_candidate_message()) }
						);
					}
					else
					{
						this->report_error(
							src_tokens,
							"couldn't match the function call to the function",
							{ this->make_note(func_body->src_tokens, func_body->get_candidate_message()) }
						);
					}
				}
				return ast::make_error_expression(
					src_tokens,
					ast::make_expr_function_call(src_tokens, std::move(params), func_body, ast::resolve_order::regular)
				);
			}

			return make_expr_function_call_from_body(src_tokens, func_body, std::move(params), *this);
		}
		else
		{
			bz_assert(
				const_called.value.is<ast::constant_value::unqualified_function_set_id>()
				|| const_called.value.is<ast::constant_value::qualified_function_set_id>()
			);
			auto const possible_funcs = const_called.value.is<ast::constant_value::unqualified_function_set_id>()
				? get_possible_funcs_for_unqualified_id(
					const_called.value.get<ast::constant_value::unqualified_function_set_id>(),
					src_tokens, params, *this
				)
				: get_possible_funcs_for_qualified_id(
					const_called.value.get<ast::constant_value::qualified_function_set_id>(),
					src_tokens, params, *this
				);

			if (possible_funcs.empty())
			{
				auto const func_id_value = [&]() {
					auto const id_values = const_called.value.is<ast::constant_value::unqualified_function_set_id>()
						? const_called.value.get<ast::constant_value::unqualified_function_set_id>().as_array_view()
						: const_called.value.get<ast::constant_value::qualified_function_set_id>().as_array_view();
					bz::u8string result;
					bool skip_scope = const_called.value.is<ast::constant_value::unqualified_function_set_id>();
					for (auto const id : id_values)
					{
						if (skip_scope)
						{
							skip_scope = false;
						}
						else
						{
							result += "::";
						}
						result += id;
					}
					return result;
				}();
				this->report_error(src_tokens, bz::format("no candidate found for function call to '{}'", func_id_value));
				return ast::make_error_expression(
					src_tokens,
					ast::make_expr_function_call(src_tokens, std::move(params), nullptr, ast::resolve_order::regular)
				);
			}
			else
			{
				auto const [_, best_body] = find_best_match(src_tokens, possible_funcs, *this);
				if (best_body == nullptr)
				{
					return ast::make_error_expression(
						src_tokens,
						ast::make_expr_function_call(src_tokens, std::move(params), nullptr, ast::resolve_order::regular)
					);
				}
				else
				{
					return make_expr_function_call_from_body(src_tokens, best_body, std::move(params), *this);
				}
			}
		}
	}
	else if (called.is_typename())
	{
		auto const called_type = ast::remove_const_or_consteval(called.get_typename());
		if (called_type.is<ast::ts_base_type>())
		{
			auto const info = called_type.get<ast::ts_base_type>().info;
			parse::resolve_type_info(*info, *this);
		}
		auto const possible_funcs = called_type.is<ast::ts_base_type>()
			? called_type.get<ast::ts_base_type>().info->constructors
				.transform([&](auto const &ptr) {
					return possible_func_t{ get_function_call_match_level({}, *ptr, params, *this, src_tokens), {}, ptr.get() };
				})
				.collect<ast::arena_vector>()
			: ast::arena_vector<possible_func_t>{};
		if (possible_funcs.empty())
		{
			this->report_error(
				src_tokens,
				bz::format("no constructors found for type '{}'", called_type)
			);
			return ast::make_error_expression(
				src_tokens,
				ast::make_expr_function_call(src_tokens, std::move(params), nullptr, ast::resolve_order::regular)
			);
		}
		else
		{
			auto const [_, best_body] = find_best_match(src_tokens, possible_funcs, *this);
			if (best_body == nullptr)
			{
				return ast::make_error_expression(
					src_tokens,
					ast::make_expr_function_call(src_tokens, std::move(params), nullptr, ast::resolve_order::regular)
				);
			}
			else
			{
				return make_expr_function_call_from_body(src_tokens, best_body, std::move(params), *this);
			}
		}
	}
	else
	{
		// function call operator
		this->report_error(src_tokens, "operator () not yet implemented");
		return ast::make_error_expression(
			src_tokens,
			ast::make_expr_function_call(src_tokens, std::move(params), nullptr, ast::resolve_order::regular)
		);
	}
}

static bz::vector<possible_func_t> get_possible_funcs_for_universal_function_call(
	lex::src_tokens src_tokens,
	ast::identifier const &id,
	bz::array_view<bz::u8string_view const> base_scope,
	bz::array_view<ast::expression> params,
	parse_context &context
)
{
	if (!id.is_qualified && id.values.size() == 1)
	{
		for (auto &scope : context.scope_decls.reversed())
		{
			auto const set = std::find_if(
				scope.func_sets.begin(), scope.func_sets.end(),
				[id](auto const &fn_set) {
					bz_assert(!fn_set.id.is_qualified && fn_set.id.values.size() == 1);
					return id.values.front() == fn_set.id.values.front();
				}
			);
			if (set != scope.func_sets.end())
			{
				bz::vector<possible_func_t> possible_funcs = {};
				for (auto &fn : set->func_decls)
				{
					auto &body = fn.get<ast::decl_function>().body;
					auto match_level = get_function_call_match_level(fn, body, params, context, src_tokens);
					possible_funcs.push_back({ std::move(match_level), fn, &body });
				}
				for (auto &alias_decl : set->alias_decls)
				{
					auto &alias = alias_decl.get<ast::decl_function_alias>();
					context.add_to_resolve_queue(src_tokens, alias);
					parse::resolve_function_alias(alias, context);
					context.pop_resolve_queue();
					for (auto const body : alias.aliased_bodies)
					{
						auto match_level = get_function_call_match_level({}, *body, params, context, src_tokens);
						possible_funcs.push_back({ std::move(match_level), alias_decl, body });
					}
				}
				return possible_funcs;
			}
		}
	}

	bz::vector<possible_func_t> possible_funcs = {};
	if (id.values.size() == 1)
	{
		auto const kinds = context.get_builtin_universal_functions(id.values.front());
		for (auto const body : kinds.transform([&](auto const kind) { return context.global_ctx.get_builtin_function(kind); }))
		{
			auto match_level = get_function_call_match_level({}, *body, params, context, src_tokens);
			possible_funcs.push_back({ std::move(match_level), {}, body });
		}
	}
	if (id.is_qualified)
	{
		context.global_decls->func_sets
			.filter([&id](auto const &fn_set) {
				return id == fn_set.id;
			})
			.for_each([&](auto const &fn_set) {
				for (auto &fn : fn_set.func_decls)
				{
					auto &body = fn.template get<ast::decl_function>().body;
					auto match_level = get_function_call_match_level(fn, body, params, context, src_tokens);
					possible_funcs.push_back({ std::move(match_level), fn, &body });
				}
				for (auto &alias_decl : fn_set.alias_decls)
				{
					auto &alias = alias_decl.template get<ast::decl_function_alias>();
					context.add_to_resolve_queue(src_tokens, alias);
					parse::resolve_function_alias(alias, context);
					context.pop_resolve_queue();
					for (auto const body : alias.aliased_bodies)
					{
						auto match_level = get_function_call_match_level({}, *body, params, context, src_tokens);
						possible_funcs.push_back({ std::move(match_level), alias_decl, body });
					}
				}
			});
		return possible_funcs;
	}
	else
	{
		context.global_decls->func_sets
			.filter([&context, &id, base_scope](auto const &fn_set) {
				return is_unqualified_equals(fn_set.id, id, context.current_scope)
					|| is_unqualified_equals(fn_set.id, id, base_scope);
			})
			.for_each([&](auto const &fn_set) {
				for (auto &fn : fn_set.func_decls)
				{
					auto &body = fn.template get<ast::decl_function>().body;
					auto match_level = get_function_call_match_level(fn, body, params, context, src_tokens);
					possible_funcs.push_back({ std::move(match_level), fn, &body });
				}
				for (auto &alias_decl : fn_set.alias_decls)
				{
					auto &alias = alias_decl.template get<ast::decl_function_alias>();
					context.add_to_resolve_queue(src_tokens, alias);
					parse::resolve_function_alias(alias, context);
					context.pop_resolve_queue();
					for (auto const body : alias.aliased_bodies)
					{
						auto match_level = get_function_call_match_level({}, *body, params, context, src_tokens);
						possible_funcs.push_back({ std::move(match_level), alias_decl, body });
					}
				}
			});
		return possible_funcs;
	}
}

ast::expression parse_context::make_universal_function_call_expression(
	lex::src_tokens src_tokens,
	ast::expression base,
	ast::identifier id,
	bz::vector<ast::expression> params
)
{
	if (base.is_error())
	{
		bz_assert(this->has_errors());
		params.push_front(std::move(base));
		return ast::make_error_expression(
			src_tokens,
			ast::make_expr_function_call(src_tokens, std::move(params), nullptr, ast::resolve_order::regular)
		);
	}

	for (auto &p : params)
	{
		if (p.is_error())
		{
			bz_assert(this->has_errors());
			params.push_front(std::move(base));
			return ast::make_error_expression(
				src_tokens,
				ast::make_expr_function_call(src_tokens, std::move(params), nullptr, ast::resolve_order::regular)
			);
		}
	}

	auto const base_scope = [&]() -> bz::array_view<bz::u8string_view const> {
		auto const base_t = ast::remove_const_or_consteval(base.get_expr_type_and_kind().first);
		if (base_t.is<ast::ts_base_type>())
		{
			auto const &id = base_t.get<ast::ts_base_type>().info->type_name;
			if (id.is_qualified)
			{
				return id.values;
			}
		}
		return {};
	}();

	params.push_front(std::move(base));
	auto const possible_funcs = get_possible_funcs_for_universal_function_call(src_tokens, id, base_scope, params, *this);
	if (possible_funcs.empty())
	{
		this->report_error(src_tokens, bz::format("no candidate found for universal function call to '{}'", id.as_string()));
		return ast::make_error_expression(
			src_tokens,
			ast::make_expr_function_call(src_tokens, std::move(params), nullptr, ast::resolve_order::regular)
		);
	}
	else
	{
		auto const [_, best_body] = find_best_match(src_tokens, possible_funcs, *this);
		if (best_body == nullptr)
		{
			return ast::make_error_expression(
				src_tokens,
				ast::make_expr_function_call(src_tokens, std::move(params), nullptr, ast::resolve_order::regular)
			);
		}
		else if (
			best_body->is_intrinsic()
			&& best_body->intrinsic_kind == ast::function_body::builtin_slice_size
			&& ast::remove_const_or_consteval(params.front().get_expr_type_and_kind().first).is<ast::ts_array>()
		)
		{
			auto const &array_t = ast::remove_const_or_consteval(params.front().get_expr_type_and_kind().first).get<ast::ts_array>();
			ast::constant_value size;
			size.emplace<ast::constant_value::uint>(array_t.size);
			return make_expr_function_call_from_body(src_tokens, best_body, std::move(params), std::move(size), *this);
		}
		else
		{
			return make_expr_function_call_from_body(src_tokens, best_body, std::move(params), *this);
		}
	}
}

ast::expression parse_context::make_subscript_operator_expression(
	lex::src_tokens src_tokens,
	ast::expression called,
	bz::vector<ast::expression> args
)
{
	if (called.is_error())
	{
		bz_assert(this->has_errors());
		return ast::make_error_expression(src_tokens, ast::make_expr_subscript(std::move(called), ast::expression()));
	}

	if (called.is_typename())
	{
		ast::typespec_view const type = called.get_typename();
		auto const type_without_const = ast::remove_const_or_consteval(type);
		if (!type_without_const.is<ast::ts_base_type>())
		{
			this->report_error(src_tokens, bz::format("invalid type '{}' for struct initializer", type));
			return ast::make_error_expression(src_tokens, ast::make_expr_struct_init(std::move(args), type));
		}

		auto const info = type_without_const.get<ast::ts_base_type>().info;
		if (info->state != ast::resolve_state::all)
		{
			this->add_to_resolve_queue(called.src_tokens, *info);
			parse::resolve_type_info(*info, *this);
			this->pop_resolve_queue();
		}
		if (info->kind != ast::type_info::aggregate)
		{
			this->report_error(src_tokens, bz::format("invalid type '{}' for struct initializer", type));
			return ast::make_error_expression(src_tokens, ast::make_expr_struct_init(std::move(args), type));
		}
		else if (
			this->current_file_id != info->file_id
			&& info->member_variables.is_any([](auto const &member) { return member.identifier.starts_with('_'); })
		)
		{
			auto notes = info->member_variables
				.filter([](auto const &member) { return member.identifier.starts_with('_'); })
				.transform([this, type](auto const &member) {
					return this->make_note(
						member.src_tokens,
						bz::format("member '{}' in type '{}' is inaccessible in this context", member.identifier, type)
					);
				})
				.collect();
			if (do_verbose)
			{
				notes.push_back(this->make_note("members whose names start with '_' are only accessible in the same file"));
			}
			this->report_error(src_tokens, bz::format("invalid type '{}' for struct initializer", type), std::move(notes));
			return ast::make_error_expression(src_tokens, ast::make_expr_struct_init(std::move(args), type));
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
						bz::format("'struct {}' is defined here", info->type_name.format_as_unqualified())
					) }
				);
				return ast::make_error_expression(src_tokens, ast::make_expr_struct_init(std::move(args), type));
			}
			else if (member_size - args_size == 1)
			{
				this->report_error(
					src_tokens,
					bz::format("missing initializer for field '{}' in type '{}'", info->member_variables.back().identifier, type),
					{ this->make_note(
						info->src_tokens,
						bz::format("'struct {}' is defined here", info->type_name.format_as_unqualified())
					) }
				);
				return ast::make_error_expression(src_tokens, ast::make_expr_struct_init(std::move(args), type));
			}
			else
			{
				auto const message = [&]() {
					bz::u8string result = "missing initializers for fields ";
					result += bz::format("'{}'", info->member_variables[args_size].identifier);
					for (size_t i = args_size + 1; i < member_size - 1; ++i)
					{
						result += bz::format(", '{}'", info->member_variables[i].identifier);
					}
					result += bz::format(" and '{}' in type '{}'", info->member_variables.back().identifier, type);
					return result;
				}();
				this->report_error(
					src_tokens,
					std::move(message),
					{ this->make_note(
						info->src_tokens,
						bz::format("'struct {}' is defined here", info->type_name.format_as_unqualified())
					) }
				);
				return ast::make_error_expression(src_tokens, ast::make_expr_struct_init(std::move(args), type));
			}
		}

		bool is_good = true;
		for (auto const [expr, member] : bz::zip(args, info->member_variables))
		{
			this->match_expression_to_type(expr, member.type);
			is_good = is_good && expr.not_error();
		}
		if (!is_good)
		{
			return ast::make_error_expression(src_tokens, ast::make_expr_struct_init(std::move(args), type));
		}

		return ast::make_dynamic_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			type_without_const,
			ast::make_expr_struct_init(std::move(args), type_without_const)
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
				|| kind == ast::expression_type_kind::tuple
			)
			{
				called = make_builtin_subscript_operator(src_tokens, std::move(called), std::move(arg), *this);
			}
			else
			{
				auto const possible_funcs = get_possible_funcs_for_operator(lex::token::square_open, src_tokens, called, arg, *this);
				if (possible_funcs.empty())
				{
					this->report_error(
						src_tokens,
						bz::format(
							"no candidate found for binary 'operator []' with types '{}' and '{}'",
							type, arg.get_expr_type_and_kind().first
						)
					);
					return ast::make_error_expression(src_tokens, ast::make_expr_subscript(std::move(called), std::move(arg)));
				}
				else
				{
					auto const [best_stmt, best_body] = find_best_match(src_tokens, possible_funcs, *this);
					if (best_body == nullptr)
					{
						return ast::make_error_expression(src_tokens, ast::make_expr_subscript(std::move(called), std::move(arg)));
					}
					else
					{
						bz::vector<ast::expression> params;
						params.reserve(2);
						params.emplace_back(std::move(called));
						params.emplace_back(std::move(arg));
						called = make_expr_function_call_from_body(
							src_tokens, best_body, std::move(params),
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
	lex::src_tokens src_tokens,
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
		if_expr.then_block = this->make_cast_expression(src_tokens, std::move(if_expr.then_block), type);
		if_expr.else_block = this->make_cast_expression(src_tokens, std::move(if_expr.else_block), std::move(type));
		return expr;
	}

	auto const [expr_type, expr_type_kind] = expr.get_expr_type_and_kind();

	if (is_builtin_type(expr_type))
	{
		auto result = make_builtin_cast(src_tokens, std::move(expr), std::move(type), *this);
		result.src_tokens = src_tokens;
		return result;
	}
	else
	{
		this->report_error(src_tokens, "invalid cast");
		return ast::make_error_expression(src_tokens, ast::make_expr_cast(std::move(expr), std::move(type)));
	}
}

ast::expression parse_context::make_member_access_expression(
	lex::src_tokens src_tokens,
	ast::expression base,
	lex::token_pos member
)
{
	if (base.is_error())
	{
		bz_assert(this->has_errors());
		return ast::make_error_expression(src_tokens, ast::make_expr_member_access(std::move(base), 0));
	}

	auto const [base_type, base_type_kind] = base.get_expr_type_and_kind();
	auto const base_t = ast::remove_const_or_consteval(base_type);
	if (
		auto const info = base_t.is<ast::ts_base_type>() ? base_t.get<ast::ts_base_type>().info : nullptr;
		info != nullptr && info->state != ast::resolve_state::all
	)
	{
		this->add_to_resolve_queue(src_tokens, *info);
		parse::resolve_type_info(*info, *this);
		this->pop_resolve_queue();
	}
	auto const members = [&]() -> bz::array_view<ast::member_variable> {
		if (base_t.is<ast::ts_base_type>())
		{
			return base_t.get<ast::ts_base_type>().info->member_variables.as_array_view();
		}
		else
		{
			return {};
		}
	}();
	auto const type_file_id = [&]() -> uint32_t {
		if (base_t.is<ast::ts_base_type>())
		{
			return base_t.get<ast::ts_base_type>().info->file_id;
		}
		else
		{
			return 0;
		}
	}();
	auto const it = std::find_if(
		members.begin(), members.end(),
		[member_value = member->value](auto const &member_variable) {
			return member_value == member_variable.identifier;
		}
	);
	if (it == members.end())
	{
		this->report_error(member, bz::format("no member named '{}' in type '{}'", member->value, base_t));
		return ast::make_error_expression(src_tokens, ast::make_expr_member_access(std::move(base), 0));
	}
	else if (this->current_file_id != type_file_id && it->identifier.starts_with('_'))
	{
		auto notes = [&]() -> bz::vector<source_highlight> {
			if (do_verbose)
			{
				return {
					this->make_note(it->src_tokens, "member is declared here"),
					this->make_note("members whose names start with '_' are only accessible in the same file")
				};
			}
			else
			{
				return { this->make_note(it->src_tokens, "member is declared here") };
			}
		}();
		this->report_error(
			member, bz::format("member '{}' in type '{}' is inaccessible in this context", member->value, base_t),
			std::move(notes)
		);
		// no need to return here, the type of the member is available so the expression doesn't have to be in an error state
		// return ast::make_error_expression(src_tokens, ast::make_expr_member_access(std::move(base), 0));
	}
	auto const index = static_cast<uint32_t>(it - members.begin());
	auto result_type = it->type;
	if (
		!result_type.is<ast::ts_const>()
		&& !result_type.is<ast::ts_lvalue_reference>()
		&& base_type.is<ast::ts_const>()
	)
	{
		result_type.add_layer<ast::ts_const>(nullptr);
	}

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
		ast::make_expr_member_access(std::move(base), index)
	);
}

void parse_context::match_expression_to_type(
	ast::expression &expr,
	ast::typespec &dest_type
)
{
	if (dest_type.is_empty())
	{
		expr.to_error();
	}
	else if (expr.is_error())
	{
		if (!ast::is_complete(dest_type))
		{
			dest_type.clear();
		}
	}
	else
	{
		match_expression_to_type_impl(expr, dest_type, dest_type, *this);
		parse::consteval_guaranteed(expr, *this);
	}
}

static void set_type(ast::decl_variable &var_decl, ast::typespec_view type)
{
	if (type.is_empty())
	{
		return;
	}
	if (var_decl.tuple_decls.empty())
	{
		var_decl.get_type() = type;
	}
	else
	{
		bz_assert(type.is<ast::ts_tuple>());
		auto const &inner_types = type.get<ast::ts_tuple>().types;
		bz_assert(inner_types.size() == var_decl.tuple_decls.size());
		for (auto const &[inner_decl, inner_type] : bz::zip(var_decl.tuple_decls, inner_types))
		{
			set_type(inner_decl, inner_type);
		}
	}
}

void parse_context::match_expression_to_variable(
	ast::expression &expr,
	ast::decl_variable &var_decl
)
{
	if (var_decl.tuple_decls.empty())
	{
		this->match_expression_to_type(expr, var_decl.get_type());
	}
	else
	{
		this->match_expression_to_type(expr, var_decl.get_type());
		set_type(var_decl, var_decl.get_type());
	}
}

bool parse_context::is_instantiable(ast::typespec_view ts)
{
	if (ts.nodes.size() == 0)
	{
		return false;
	}

	for (auto const &node : ts.nodes)
	{
		// 1 means it's instantiable
		// 0 means it's not yet decidable
		// -1 means it's not instantiable
		auto const result = node.visit(bz::overload{
			[this](ast::ts_base_type const &base_type) {
				if (base_type.info->state != ast::resolve_state::all && base_type.info->state != ast::resolve_state::error)
				{
					this->add_to_resolve_queue(base_type.src_tokens, *base_type.info);
					parse::resolve_type_info(*base_type.info, *this);
					this->pop_resolve_queue();
				}
				return base_type.info->state == ast::resolve_state::all ? 1 : -1;
			},
			[](ast::ts_void const &) {
				return -1;
			},
			[](ast::ts_function const &) {
				// functions are pointers under the hood, so we don't really care about
				// parameters or return types being instantiable here;  return types are checked
				// when the function is called
				return 1;
			},
			[this](ast::ts_array const &array_t) {
				return this->is_instantiable(array_t.elem_type) ? 1 : -1;
			},
			[this](ast::ts_array_slice const &array_slice_t) {
				// array slice type needs to be sized, because pointer arithmetic is required
				// when accessing elements
				return this->is_instantiable(array_slice_t.elem_type) ? 1 : -1;
			},
			[this](ast::ts_tuple const &tuple_t) {
				for (auto &t : tuple_t.types)
				{
					if (!this->is_instantiable(t))
					{
						return -1;
					}
				}
				return 1;
			},
			[](ast::ts_auto const &) {
				return -1;
			},
			[](ast::ts_typename const &) {
				return -1;
			},
			[](ast::ts_const const &) {
				return 0;
			},
			[](ast::ts_consteval const &) {
				return 0;
			},
			[](ast::ts_pointer const &) {
				return 1;
			},
			[](ast::ts_lvalue_reference const &) {
				return 1;
			},
			[](ast::ts_auto_reference const &) {
				return -1;
			},
			[](ast::ts_auto_reference_const const &) {
				return -1;
			},
			[](ast::ts_unresolved const &) {
				bz_unreachable;
				return -1;
			}
		});
		if (result != 0)
		{
			return result == 1;
		}
	}
	return false;
}

size_t parse_context::get_sizeof(ast::typespec_view ts)
{
	// constexpr uint64_t invalid_size = std::numeric_limits<uint64_t>::max();
	return this->global_ctx._comptime_executor.get_size(ts);
}

bz::vector<ast::function_body *> parse_context::get_function_bodies_from_unqualified_id(
	lex::src_tokens requester,
	bz::array_view<bz::u8string_view const> id
)
{
	if (id.size() == 1)
	{
		for (auto &scope : this->scope_decls.reversed())
		{
			auto const set = std::find_if(
				scope.func_sets.begin(), scope.func_sets.end(),
				[id](auto const &fn_set) {
					bz_assert(!fn_set.id.is_qualified && fn_set.id.values.size() == 1);
					return id.front() == fn_set.id.values.front();
				}
			);
			if (set != scope.func_sets.end())
			{
				bz::vector<ast::function_body *> result;
				result.append(set->func_decls.transform([](auto const view) { return &view.template get<ast::decl_function>().body; }));
				for (auto const &alias : set->alias_decls)
				{
					this->add_to_resolve_queue(requester, alias.get<ast::decl_function_alias>());
					parse::resolve_function_alias(alias.get<ast::decl_function_alias>(), *this);
					this->pop_resolve_queue();
					result.append(alias.get<ast::decl_function_alias>().aliased_bodies);
				}
				return result;
			}
		}
	}

	bz::vector<ast::function_body *> result;

	auto &global_decls = *this->global_decls;
	auto const filtered_sets = global_decls.func_sets
		.transform([](auto &set) { return &set; })
		.filter([this, id](auto const set) {
			auto const set_id_size = set->id.values.size();
			auto const id_size = id.size();
			return set_id_size >= id_size
				&& set_id_size <= id_size + this->current_scope.size()
				&& std::equal(set->id.values.end() - id_size, set->id.values.end(), id.begin())
				&& std::equal(set->id.values.begin(), set->id.values.end() - id_size, this->current_scope.begin());
		});
	for (auto const set : filtered_sets)
	{
		result.append(set->func_decls.transform([](auto const view) { return &view.template get<ast::decl_function>().body; }));
		for (auto const &alias : set->alias_decls)
		{
			this->add_to_resolve_queue(requester, alias.get<ast::decl_function_alias>());
			parse::resolve_function_alias(alias.get<ast::decl_function_alias>(), *this);
			this->pop_resolve_queue();
			result.append(alias.get<ast::decl_function_alias>().aliased_bodies);
		}
	}
	return result;
}

bz::vector<ast::function_body *> parse_context::get_function_bodies_from_qualified_id(
	lex::src_tokens requester,
	bz::array_view<bz::u8string_view const> id
)
{
	bz::vector<ast::function_body *> result;

	auto &global_decls = *this->global_decls;
	auto const set = std::find_if(
		global_decls.func_sets.begin(), global_decls.func_sets.end(),
		[id](auto const &fn_set) {
			return id.size() == fn_set.id.values.size()
				&& std::equal(id.begin(), id.end(), fn_set.id.values.begin());
		}
	);
	if (set != global_decls.func_sets.end())
	{
		result.append(set->func_decls.transform([](auto const view) { return &view.template get<ast::decl_function>().body; }));
		for (auto const &alias : set->alias_decls)
		{
			this->add_to_resolve_queue(requester, alias.get<ast::decl_function_alias>());
			parse::resolve_function_alias(alias.get<ast::decl_function_alias>(), *this);
			this->pop_resolve_queue();
			result.append(alias.get<ast::decl_function_alias>().aliased_bodies);
		}
	}
	return result;
}

ast::identifier parse_context::make_qualified_identifier(lex::token_pos id)
{
	ast::identifier result;
	result.is_qualified = true;
	result.values = this->current_scope;
	result.values.push_back(id->value);
	result.tokens = { id, id + 1 };
	return result;
}

ast::constant_value parse_context::execute_function(
	lex::src_tokens src_tokens,
	ast::function_body *body,
	bz::array_view<ast::constant_value const> params
)
{
	auto const original_parse_ctx = this->global_ctx._comptime_executor.current_parse_ctx;
	this->global_ctx._comptime_executor.current_parse_ctx = this;
	auto [result, errors] = this->global_ctx._comptime_executor.execute_function(
		src_tokens,
		body,
		params
	);
	this->global_ctx._comptime_executor.current_parse_ctx = original_parse_ctx;
	if (!errors.empty())
	{
		for (auto &error : errors)
		{
			error.notes.push_back(this->make_note(
				src_tokens,
				bz::format("while evaluating call to '{}' in a constant expression", body->get_signature())
			));
			this->global_ctx.report_error(std::move(error));
		}
	}
	return result;
}

ast::constant_value parse_context::execute_compound_expression(
	lex::src_tokens src_tokens,
	ast::expr_compound &expr
)
{
	auto const original_parse_ctx = this->global_ctx._comptime_executor.current_parse_ctx;
	this->global_ctx._comptime_executor.current_parse_ctx = this;
	auto [result, errors] = this->global_ctx._comptime_executor.execute_compound_expression(expr);
	this->global_ctx._comptime_executor.current_parse_ctx = original_parse_ctx;
	if (!errors.empty())
	{
		for (auto &error : errors)
		{
			error.notes.push_back(this->make_note(src_tokens, "while evaluating compound expression in a constant expression"));
			this->global_ctx.report_error(std::move(error));
		}
	}
	return result;
}

/*
auto parse_context::get_cast_body_and_type(ast::expr_cast const &cast)
	-> std::pair<ast::function_body *, ast::expression::expr_type_t>
{
	auto res = get_builtin_cast_type(cast.expr.expr_type, cast.type, *this);
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

} // namespace ctx
