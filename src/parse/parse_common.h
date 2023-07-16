#ifndef PARSE_PARSE_COMMON_H
#define PARSE_PARSE_COMMON_H

#include "core.h"
#include "lex/token.h"
#include "token_info.h"
#include "escape_sequences.h"
#include "ctx/parse_context.h"

namespace parse
{

enum class parse_scope
{
	global, struct_body, local
};

ast::expression parse_parenthesized_condition(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template<uint32_t ...stop_tokens>
inline lex::token_range get_tokens_in_curly(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	auto const open_curly = stream - 1;
	auto const begin = stream;
	size_t level = 0;
	auto const is_valid_kind = [](uint32_t kind) {
		return !((kind == lex::token::curly_close) || ... || (kind == stop_tokens));
	};

	while (stream != end && (level != 0 || is_valid_kind(stream->kind)))
	{
		if (stream->kind == lex::token::curly_open)
		{
			++level;
		}
		else if (stream->kind == lex::token::curly_close)
		{
			--level;
		}
		++stream;
	}

	if (stream == end)
	{
		if (open_curly->kind == lex::token::curly_open)
		{
			context.report_paren_match_error(stream, open_curly);
		}
		return {};
	}
	else
	{
		bz_assert(stream->kind == lex::token::curly_close);
		auto const end_ = stream;
		++stream; // '}'
		return { begin, end_ };
	}
}

template<uint32_t ...stop_tokens>
inline lex::token_range get_expression_tokens_without_error(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	auto const begin = stream;
	auto const is_valid_kind = [](uint32_t kind) {
		return is_valid_expression_or_type_token(kind)
			&& !((kind == stop_tokens) || ...);
	};
	size_t level = 0;

	while (stream != end && (level != 0 || is_valid_kind(stream->kind)))
	{
		switch (stream->kind)
		{
		case lex::token::paren_open:
		{
			++stream; // '('
			get_expression_tokens_without_error<
				lex::token::paren_close, lex::token::square_close
			>(stream, end, context);
			if (stream != end && stream->kind == lex::token::paren_close)
			{
				++stream;
			}
			break;
		}
		case lex::token::square_open:
		{
			++stream; // '['
			get_expression_tokens_without_error<
				lex::token::paren_close, lex::token::square_close
			>(stream, end, context);
			if (stream != end && stream->kind == lex::token::square_close)
			{
				++stream;
			}
			break;
		}
		case lex::token::curly_open:
			++level;
			++stream; // '{'
			break;
		case lex::token::curly_close:
			--level;
			++stream; // '}'
			break;
		case lex::token::paren_close:
			++stream; // ')'
			break;
		case lex::token::square_close:
			++stream; // ']'
			break;
		default:
			++stream;
			break;
		}
	}

	return { begin, stream };
}

template<uint32_t ...stop_tokens>
inline lex::token_range get_expression_tokens(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	auto const begin = stream;
	auto const is_valid_kind = [](uint32_t kind) {
		return is_valid_expression_or_type_token(kind)
			&& !((kind == stop_tokens) || ...);
	};

	while (stream != end && is_valid_kind(stream->kind))
	{
		switch (stream->kind)
		{
		case lex::token::paren_open:
		{
			++stream; // '('
			get_expression_tokens_without_error<
				lex::token::paren_close, lex::token::square_close
			>(stream, end, context);
			if (stream != end && stream->kind == lex::token::paren_close)
			{
				++stream;
			}
			break;
		}
		case lex::token::square_open:
		{
			++stream; // '['
			get_expression_tokens_without_error<
				lex::token::paren_close, lex::token::square_close
			>(stream, end, context);
			if (stream != end && stream->kind == lex::token::square_close)
			{
				++stream;
			}
			break;
		}
		case lex::token::curly_open:
			++stream; // '{'
			get_tokens_in_curly<>(stream, end, context);
			break;
		case lex::token::paren_close:
			context.report_error(stream, "stray )");
			++stream; // ')'
			break;
		case lex::token::square_close:
			context.report_error(stream, "stray ]");
			++stream; // ']'
			break;
		default:
			++stream;
			break;
		}
	}

	return { begin, stream };
}

inline lex::token_range get_paren_matched_range(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	auto const open = stream - 1;
	switch (open->kind)
	{
	case lex::token::paren_open:
	{
		auto range = get_expression_tokens<
			lex::token::paren_close,
			lex::token::square_close
		>(stream, end, context);

		if (stream != end && stream->kind == lex::token::paren_close)
		{
			++stream;
		}
		else
		{
			context.report_paren_match_error(stream, open);
		}
		return range;
	}
	case lex::token::square_open:
	{
		auto range = get_expression_tokens<
			lex::token::paren_close,
			lex::token::square_close
		>(stream, end, context);

		if (stream != end && stream->kind == lex::token::square_close)
		{
			++stream;
		}
		else
		{
			context.report_paren_match_error(stream, open);
		}
		return range;
	}
	default:
		bz_unreachable;
		return {};
	}
}

inline lex::token_pos search_token(uint32_t kind, lex::token_pos begin, lex::token_pos end)
{
	int paren_level = 0;
	for (auto it = begin; paren_level >= 0 && it != end; ++it)
	{
		if (paren_level == 0 && it->kind == kind)
		{
			return it;
		}
		switch (it->kind)
		{
		case lex::token::paren_open:
		case lex::token::square_open:
		case lex::token::curly_open:
			++paren_level;
			break;
		case lex::token::paren_close:
		case lex::token::square_close:
		case lex::token::curly_close:
			--paren_level;
			break;
		default:
			break;
		}
	}
	return end;
}

inline bz::u8char get_character(bz::u8string_view::const_iterator &it)
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

inline abi::calling_convention get_calling_convention(lex::token_pos it, ctx::parse_context &context)
{
	auto const string_value = [&]() -> bz::u8string {
		if (it->kind == lex::token::raw_string_literal)
		{
			return it->value;
		}
		else
		{
			// copy pasted from parse_context.cpp
			bz::u8string result = "";
			auto const value = it->value;
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
		}
	}();

	static_assert(static_cast<int>(abi::calling_convention::_last) == 3);
	if (string_value == "c")
	{
		return abi::calling_convention::c;
	}
	else if (string_value == "fast")
	{
		return abi::calling_convention::fast;
	}
	else if (string_value == "std")
	{
		return abi::calling_convention::std;
	}
	else
	{
		auto notes = [&]() -> bz::vector<ctx::source_highlight> {
			if (global_data::do_verbose)
			{
				return { context.make_note(
					it->src_pos.file_id, it->src_pos.line,
					"available calling conventions are 'c', 'fast' and 'std'"
				) };
			}
			else
			{
				return {};
			}
		}();
		context.report_error(it, bz::format("invalid calling convention '{}'", string_value), std::move(notes));
		// return c by default
		return abi::calling_convention::c;
	}
}


using parse_fn_t = ast::statement (*)(lex::token_pos &, lex::token_pos, ctx::parse_context &);

struct statement_parser
{
	enum : uint32_t
	{
		global      = bit_at<0>,
		local       = bit_at<1>,
		struct_body = bit_at<2>,
	};

	uint32_t   kind;
	uint32_t   flags;
	parse_fn_t parse_fn;

	constexpr bool is_global(void) const noexcept
	{ return (this->flags & global) != 0; }

	constexpr bool is_local(void) const noexcept
	{ return (this->flags & local) != 0; }

	constexpr bool is_struct_body(void) const noexcept
	{ return (this->flags & struct_body) != 0; }
};

ast::statement parse_stmt_static_assert(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template<parse_scope scope>
ast::statement parse_decl_variable(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template<parse_scope scope>
ast::statement parse_decl_type_alias(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template<parse_scope scope>
ast::statement parse_decl_function_or_alias(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template<parse_scope scope>
ast::statement parse_consteval_decl(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template<parse_scope scope>
ast::statement parse_decl_operator_or_alias(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template<parse_scope scope>
ast::statement parse_decl_struct(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template<parse_scope scope>
ast::statement parse_decl_enum(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template<parse_scope scope>
ast::statement parse_attribute_statement(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

ast::statement parse_stmt_while(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

ast::statement parse_stmt_for_or_foreach(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

ast::statement parse_stmt_return(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

ast::statement parse_stmt_defer(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

ast::statement parse_stmt_no_op(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

ast::statement parse_stmt_expression(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

template<parse_scope scope>
ast::statement parse_export_statement(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

ast::statement parse_local_export_statement(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

ast::statement parse_decl_import(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

ast::statement default_parse_type_info_statement(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

constexpr bz::array statement_parsers = {
	statement_parser{ lex::token::kw_static_assert, statement_parser::local | statement_parser::global | statement_parser::struct_body, &parse_stmt_static_assert },

	statement_parser{ lex::token::kw_let,           statement_parser::global, &parse_decl_variable<parse_scope::global>,          },
	statement_parser{ lex::token::kw_extern,        statement_parser::global, &parse_decl_variable<parse_scope::global>,          },
	statement_parser{ lex::token::kw_mut,           statement_parser::global, &parse_decl_variable<parse_scope::global>,          },
	statement_parser{ lex::token::kw_consteval,     statement_parser::global, &parse_consteval_decl<parse_scope::global>,         },
	statement_parser{ lex::token::kw_type,          statement_parser::global, &parse_decl_type_alias<parse_scope::global>,        },
	statement_parser{ lex::token::kw_function,      statement_parser::global, &parse_decl_function_or_alias<parse_scope::global>, },
	statement_parser{ lex::token::kw_operator,      statement_parser::global, &parse_decl_operator_or_alias<parse_scope::global>, },
	statement_parser{ lex::token::kw_struct,        statement_parser::global, &parse_decl_struct<parse_scope::global>,            },
	statement_parser{ lex::token::kw_enum,          statement_parser::global, &parse_decl_enum<parse_scope::global>,              },
	statement_parser{ lex::token::at,               statement_parser::global, &parse_attribute_statement<parse_scope::global>,    },
	statement_parser{ lex::token::kw_export,        statement_parser::global, &parse_export_statement<parse_scope::global>,       },
	statement_parser{ lex::token::kw_import,        statement_parser::global, &parse_decl_import,                                 },

	statement_parser{ lex::token::kw_let,           statement_parser::struct_body, &parse_decl_variable<parse_scope::struct_body>,          },
	statement_parser{ lex::token::kw_extern,        statement_parser::struct_body, &parse_decl_variable<parse_scope::struct_body>,          },
	statement_parser{ lex::token::kw_mut,           statement_parser::struct_body, &parse_decl_variable<parse_scope::struct_body>,          },
	statement_parser{ lex::token::kw_consteval,     statement_parser::struct_body, &parse_consteval_decl<parse_scope::struct_body>,         },
	statement_parser{ lex::token::kw_type,          statement_parser::struct_body, &parse_decl_type_alias<parse_scope::struct_body>,        },
	statement_parser{ lex::token::kw_function,      statement_parser::struct_body, &parse_decl_function_or_alias<parse_scope::struct_body>, },
	statement_parser{ lex::token::kw_operator,      statement_parser::struct_body, &parse_decl_operator_or_alias<parse_scope::struct_body>, },
	statement_parser{ lex::token::kw_struct,        statement_parser::struct_body, &parse_decl_struct<parse_scope::struct_body>,            },
	statement_parser{ lex::token::kw_enum,          statement_parser::struct_body, &parse_decl_enum<parse_scope::struct_body>,              },
	statement_parser{ lex::token::at,               statement_parser::struct_body, &parse_attribute_statement<parse_scope::struct_body>,    },
	statement_parser{ lex::token::kw_export,        statement_parser::struct_body, &parse_export_statement<parse_scope::struct_body>,       },

	statement_parser{ lex::token::kw_let,           statement_parser::local, &parse_decl_variable<parse_scope::local>,          },
	statement_parser{ lex::token::kw_mut,           statement_parser::local, &parse_decl_variable<parse_scope::local>,          },
	statement_parser{ lex::token::kw_consteval,     statement_parser::local, &parse_consteval_decl<parse_scope::local>,         },
	statement_parser{ lex::token::kw_type,          statement_parser::local, &parse_decl_type_alias<parse_scope::local>,        },
	statement_parser{ lex::token::kw_function,      statement_parser::local, &parse_decl_function_or_alias<parse_scope::local>, },
	statement_parser{ lex::token::at,               statement_parser::local, &parse_attribute_statement<parse_scope::local>,    },
	statement_parser{ lex::token::kw_while,         statement_parser::local, &parse_stmt_while,                                 },
	statement_parser{ lex::token::kw_for,           statement_parser::local, &parse_stmt_for_or_foreach,                        },
	statement_parser{ lex::token::kw_return,        statement_parser::local, &parse_stmt_return,                                },
	statement_parser{ lex::token::kw_defer,         statement_parser::local, &parse_stmt_defer,                                 },
	statement_parser{ lex::token::semi_colon,       statement_parser::local, &parse_stmt_no_op,                                 },
	statement_parser{ lex::token::kw_export,        statement_parser::local, &parse_local_export_statement,                     },
};

template<uint32_t flag>
constexpr auto generate_parsers(void)
{
	constexpr auto parser_count = []() {
		size_t count = 0;
		for (auto const p : statement_parsers)
		{
			if ((p.flags & flag) != 0)
			{
				++count;
			}
		}
		return count;
	}();
	static_assert(parser_count != 0, "no statement parser found");
	using result_t = bz::array<statement_parser, parser_count>;
	result_t result{};

	size_t i = 0;
	for (auto const p : statement_parsers)
	{
		if ((p.flags & flag) != 0)
		{
			result[i] = p;
			++i;
		}
	}
	bz_assert(i == result.size());
	return result;
}

constexpr auto global_statement_parsers      = generate_parsers<statement_parser::global>();
constexpr auto local_statement_parsers       = generate_parsers<statement_parser::local>();
constexpr auto struct_body_statement_parsers = generate_parsers<statement_parser::struct_body>();

ast::expression parse_top_level_expression(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

ast::expression parse_compound_expression(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);
ast::expression parse_if_expression(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);
ast::expression parse_switch_expression(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

ast::identifier get_identifier(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
);

} // namespace parse

#endif // PARSE_PARSE_COMMON_H
