#include "src_file.h"
#include "ctx/global_context.h"
#include "ctx/lex_context.h"
#include "ctx/parse_context.h"
#include "ctx/bitcode_context.h"
#include "lex/lexer.h"
#include "parse/statement_parser.h"

static bz::u8string read_text_from_file(std::ifstream &file)
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


src_file::src_file(fs::path file_path, uint32_t file_id, bool is_library_file)
	: _stage(constructed),
	  _is_library_file(is_library_file),
	  _file_id(file_id),
	  _file_path(std::move(file_path)),
	  _file(), _tokens(),
	  _declarations{},
	  _global_decls{}
{}


void src_file::add_to_global_decls(ctx::decl_set const &set)
{
	this->_global_decls.var_decls.append(set.var_decls);
	this->_global_decls.types.append(set.types);

	for (auto const &func_set : set.func_sets)
	{
		this->_global_decls.add_function_set(func_set);
	}

	for (auto const &op_set : set.op_sets)
	{
		this->_global_decls.add_operator_set(op_set);
	}
}

[[nodiscard]] bool src_file::read_file(ctx::global_context &global_ctx)
{
	bz_assert(this->_stage == constructed);
	std::ifstream file(this->_file_path);

	if (!file.good())
	{
		bz::u8string const file_name = this->_file_path.generic_string().c_str();
		global_ctx.report_error(bz::format("unable to read file '{}'", file_name));
		return false;
	}

	this->_file = read_text_from_file(file);
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
			this->_global_decls.var_decls.push_back(&var_decl);
			break;
		}
		case ast::statement::index<ast::decl_function>:
		{
			auto &body = decl.get<ast::decl_function>().body;
			this->_global_decls.add_function(decl);
			if (body.is_export())
			{
				this->_export_decls.add_function(decl);
			}
			break;
		}
		case ast::statement::index<ast::decl_operator>:
		{
			auto &body = decl.get<ast::decl_operator>().body;
			this->_global_decls.add_operator(decl);
			if (body.is_export())
			{
				this->_export_decls.add_operator(decl);
			}
			break;
		}
		case ast::statement::index<ast::decl_function_alias>:
		{
			this->_global_decls.add_function_alias(decl);
			auto const &alias = decl.get<ast::decl_function_alias>();
			if (alias.is_export)
			{
				this->_export_decls.add_function_alias(decl);
			}
			break;
		}
		case ast::statement::index<ast::decl_type_alias>:
		{
			this->_global_decls.add_type_alias(decl);
			auto const &alias = decl.get<ast::decl_type_alias>();
			if (alias.is_export)
			{
				this->_export_decls.add_type_alias(decl);
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
		auto const import_file_id = global_ctx.add_module(import->identifier, this->_file_id, import->identifier->value);
		if (import_file_id != std::numeric_limits<uint32_t>::max())
		{
			auto const &import_decls = global_ctx.get_file_export_decls(import_file_id);
			this->add_to_global_decls(import_decls);
		}
	}

	return !global_ctx.has_errors();
}

[[nodiscard]] bool src_file::parse(ctx::global_context &global_ctx)
{
	bz_assert(this->_stage == parsed_global_symbols);

	ctx::parse_context context(global_ctx);
	context.global_decls = &this->_global_decls;

	for (auto &decl : this->_declarations)
	{
		parse::resolve_global_statement(decl, context);
	}
	for (std::size_t i = 0; i < context.generic_functions.size(); ++i)
	{
		context.add_to_resolve_queue({}, *context.generic_functions[i]);
		parse::resolve_function({}, *context.generic_functions[i], context);
		context.pop_resolve_queue();
	}

	this->_stage = parsed;
	return !global_ctx.has_errors();
}
