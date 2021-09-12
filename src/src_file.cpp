#include "src_file.h"
#include "ctx/global_context.h"
#include "ctx/lex_context.h"
#include "ctx/parse_context.h"
#include "ctx/bitcode_context.h"
#include "lex/lexer.h"
#include "parse/consteval.h"
#include "parse/statement_parser.h"
#include "resolve/statement_resolver.h"

static bz::u8string read_text_from_file(std::istream &file)
{
	std::vector<char> file_content{
		std::istreambuf_iterator<char>(file),
		std::istreambuf_iterator<char>()
	};
	auto const file_content_view = bz::u8string_view(
		file_content.data(),
		file_content.data() + file_content.size()
	);


	bz::u8string file_str = file_content_view;
	file_str.erase('\r');
	return file_str;
}

static void report_redeclaration_error(lex::src_tokens src_tokens, ctx::symbol_t *prev_symbol, ctx::global_context &global_ctx)
{
	if (prev_symbol != nullptr)
	{
		global_ctx.report_error(
			src_tokens,
			bz::format("redeclaration of global identifier '{}'", ctx::get_symbol_id(*prev_symbol).format_as_unqualified()),
			{ global_ctx.make_note(
				ctx::get_symbol_src_tokens(*prev_symbol),
				"previous declaration was here"
			) }
		);
	}
}


src_file::src_file(fs::path file_path, uint32_t file_id, bz::vector<bz::u8string_view> scope, bool is_library_file)
	: _stage(constructed),
	  _is_library_file(is_library_file),
	  _file_id(file_id),
	  _file_path(std::move(file_path)),
	  _file(), _tokens(),
	  _declarations{},
	  _global_decls{},
	  _scope(std::move(scope))
{}


static void add_to_global_decls(src_file &file, ctx::decl_set const &set, ctx::global_context &global_ctx)
{
	for (auto const &symbol : set.symbols)
	{
		auto const prev_symbol = file._global_decls.add_symbol(symbol);
		report_redeclaration_error(ctx::get_symbol_src_tokens(symbol), prev_symbol, global_ctx);
	}

	for (auto const &op_set : set.op_sets)
	{
		[[maybe_unused]] auto const symbol_ptr = file._global_decls.add_operator_set(op_set);
		bz_assert(symbol_ptr == nullptr);
	}
}

[[nodiscard]] bool src_file::read_file(ctx::global_context &global_ctx)
{
	bz_assert(this->_stage == constructed);

	if (this->_file_path.generic_string() == "-")
	{
		this->_file = read_text_from_file(std::cin);
	}
	else
	{
		std::ifstream file(this->_file_path);

		if (!file.good())
		{
			bz::u8string const file_name = this->_file_path.generic_string().c_str();
			global_ctx.report_error(bz::format("unable to read file '{}'", file_name));
			return false;
		}

		this->_file = read_text_from_file(file);
	}

	if (!this->_file.verify())
	{
		bz::u8string const file_name = this->_file_path.generic_string().c_str();
		global_ctx.report_error(bz::format("'{}' is not a valid UTF-8 file", file_name));
		return false;
	}
	this->_stage = file_read;
	return true;
}

[[nodiscard]] bool src_file::tokenize(ctx::global_context &global_ctx)
{
	bz_assert(this->_stage == file_read);

	ctx::lex_context context(global_ctx);
	this->_tokens = lex::get_tokens(this->_file, this->_file_id, context);
	this->_stage = tokenized;

	return !global_ctx.has_errors();
}

[[nodiscard]] bool src_file::parse_global_symbols(ctx::global_context &global_ctx)
{
	switch (this->_stage)
	{
	case constructed:
		if (!this->read_file(global_ctx))
		{
			return false;
		}
		[[fallthrough]];
	case file_read:
		if (!this->tokenize(global_ctx))
		{
			return false;
		}
		[[fallthrough]];
	case tokenized:
		break;
	case parsed_global_symbols:
	case parsed:
		bz_unreachable;
	}
	bz_assert(this->_stage == tokenized);
	bz_assert(this->_tokens.size() != 0);
	bz_assert(this->_tokens.back().kind == lex::token::eof);
	auto stream = this->_tokens.cbegin();
	auto end    = this->_tokens.cend() - 1;

	ctx::parse_context context(global_ctx);

	this->_declarations = parse::parse_global_statements(stream, end, context);

	bz::vector<ast::decl_import const *> imports = {};

	this->_global_decls = ctx::get_default_decls();
	for (auto &decl : this->_declarations)
	{
		switch (decl.kind())
		{
		case ast::statement::index<ast::decl_variable>:
		{
			auto &var_decl = decl.get<ast::decl_variable>();
			auto const prev_symbol = this->_global_decls.add_variable(var_decl);
			report_redeclaration_error(var_decl.src_tokens, prev_symbol, global_ctx);
			if (prev_symbol == nullptr && var_decl.is_module_export())
			{
				[[maybe_unused]] auto const export_prev_symbol = this->_export_decls.add_variable(var_decl);
				bz_assert(export_prev_symbol == nullptr);
			}
			break;
		}
		case ast::statement::index<ast::decl_function>:
		{
			auto &body = decl.get<ast::decl_function>().body;
			auto const prev_symbol = this->_global_decls.add_function(decl);
			report_redeclaration_error(body.src_tokens, prev_symbol, global_ctx);
			if (prev_symbol == nullptr && body.is_export())
			{
				[[maybe_unused]] auto const export_prev_symbol = this->_export_decls.add_function(decl);
				bz_assert(export_prev_symbol == nullptr);
			}
			break;
		}
		case ast::statement::index<ast::decl_operator>:
		{
			auto &body = decl.get<ast::decl_operator>().body;
			[[maybe_unused]] auto const prev_symbol = this->_global_decls.add_operator(decl);
			bz_assert(prev_symbol == nullptr);
			if (body.is_export())
			{
				[[maybe_unused]] auto const export_prev_symbol = this->_export_decls.add_operator(decl);
				bz_assert(export_prev_symbol == nullptr);
			}
			break;
		}
		case ast::statement::index<ast::decl_function_alias>:
		{
			auto &alias = decl.get<ast::decl_function_alias>();
			auto const prev_symbol = this->_global_decls.add_function_alias(alias);
			report_redeclaration_error(alias.src_tokens, prev_symbol, global_ctx);
			if (prev_symbol == nullptr && alias.is_export)
			{
				[[maybe_unused]] auto const export_prev_symbol = this->_export_decls.add_function_alias(alias);
				bz_assert(export_prev_symbol == nullptr);
			}
			break;
		}
		case ast::statement::index<ast::decl_type_alias>:
		{
			auto &alias = decl.get<ast::decl_type_alias>();
			auto const prev_symbol = this->_global_decls.add_type_alias(alias);
			report_redeclaration_error(alias.src_tokens, prev_symbol, global_ctx);
			if (prev_symbol == nullptr && alias.is_export)
			{
				[[maybe_unused]] auto const export_prev_symbol = this->_export_decls.add_type_alias(alias);
				bz_assert(export_prev_symbol == nullptr);
			}
			break;
		}
		case ast::statement::index<ast::decl_struct>:
		{
			auto &struct_decl = decl.get<ast::decl_struct>();
			auto const prev_symbol = this->_global_decls.add_type(struct_decl);
			report_redeclaration_error(struct_decl.info.src_tokens, prev_symbol, global_ctx);
			if (prev_symbol == nullptr && struct_decl.info.is_module_export())
			{
				[[maybe_unused]] auto const export_prev_symbol = this->_export_decls.add_type(struct_decl);
				bz_assert(export_prev_symbol == nullptr);
			}
			break;
		}
		case ast::statement::index<ast::decl_import>:
		{
			imports.push_back(&decl.get<ast::decl_import>());
			break;
		}
		default:
			break;
		}
	}
	this->_stage = parsed_global_symbols;

	for (auto const import : imports)
	{
		auto const import_file_id = global_ctx.add_module(this->_file_id, import->id);
		if (import_file_id != std::numeric_limits<uint32_t>::max())
		{
			auto const &import_decls = global_ctx.get_file_export_decls(import_file_id);
			add_to_global_decls(*this, import_decls, global_ctx);
		}
	}

	return !global_ctx.has_errors();
}

[[nodiscard]] bool src_file::parse(ctx::global_context &global_ctx)
{
	if (this->_stage > parsed_global_symbols)
	{
		return true;
	}
	bz_assert(this->_stage == parsed_global_symbols);

	ctx::parse_context context(global_ctx);
	context.global_decls = &this->_global_decls;

	for (auto &decl : this->_declarations)
	{
		resolve::resolve_global_statement(decl, context);
	}
	for (std::size_t i = 0; i < context.generic_functions.size(); ++i)
	{
		context.add_to_resolve_queue({}, *context.generic_functions[i]);
		resolve::resolve_function({}, *context.generic_functions[i], context);
		context.pop_resolve_queue();
	}

	this->_stage = parsed;
	return !global_ctx.has_errors();
}

void src_file::aggressive_consteval(ctx::global_context &global_ctx)
{
	bz_assert(this->_stage == parsed);
	ctx::parse_context context(global_ctx);
	context.global_decls = &this->_global_decls;

	for (auto &decl : this->_declarations)
	{
		parse::consteval_try_without_error_decl(decl, context);
	}
}
