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


src_file::src_file(fs::path file_path, uint32_t file_id, ctx::global_context &global_ctx)
	: _stage(constructed),
	  _file_id(file_id),
	  _file_path(std::move(file_path)),
	  _file(), _tokens(),
	  _global_ctx(global_ctx),
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

[[nodiscard]] bool src_file::read_file(void)
{
	bz_assert(this->_stage == constructed);
	std::ifstream file(this->_file_path);

	if (!file.good())
	{
		bz::u8string const file_name = this->_file_path.generic_string().c_str();
		this->_global_ctx.report_error(bz::format("unable to read file '{}'", file_name));
		return false;
	}

	this->_file = read_text_from_file(file);
	if (!this->_file.verify())
	{
		bz::u8string const file_name = this->_file_path.generic_string().c_str();
		this->_global_ctx.report_error(bz::format("'{}' is not a valid UTF-8 file", file_name));
		return false;
	}
	this->_stage = file_read;
	return true;
}

[[nodiscard]] bool src_file::tokenize(void)
{
	bz_assert(this->_stage == file_read);

	ctx::lex_context context(this->_global_ctx);
	this->_tokens = lex::get_tokens(this->_file, this->_file_id, context);
	this->_stage = tokenized;

	return !this->_global_ctx.has_errors();
}

[[nodiscard]] bool src_file::parse_global_symbols(void)
{
	switch (this->_stage)
	{
	case constructed:
		if (!this->read_file())
		{
			return false;
		}
		[[fallthrough]];
	case file_read:
		if (!this->tokenize())
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

	ctx::parse_context context(this->_global_ctx);

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
		auto const import_file_id = this->_global_ctx.add_module(import->identifier, this->_file_id, import->identifier->value);
		if (import_file_id != std::numeric_limits<uint32_t>::max())
		{
			auto const &import_decls = this->_global_ctx.get_file_export_decls(import_file_id);
			this->add_to_global_decls(import_decls);
		}
	}

	return !this->_global_ctx.has_errors();
}

[[nodiscard]] bool src_file::parse(void)
{
	bz_assert(this->_stage == parsed_global_symbols);

	ctx::parse_context context(this->_global_ctx);
	context.global_decls = &this->_global_decls;

	for (auto &decl : this->_declarations)
	{
		parse::resolve_global_statement(decl, context);
	}

	this->_stage = parsed;
	return !this->_global_ctx.has_errors();
}

/*
[[nodiscard]] bool src_file::first_pass_parse(void)
{
	bz_assert(this->_stage == tokenized);
	bz_assert(this->_tokens.size() != 0);
	bz_assert(this->_tokens.back().kind == lex::token::eof);
	auto stream = this->_tokens.cbegin();
	auto end    = this->_tokens.cend() - 1;

	ctx::first_pass_parse_context context(this->_global_ctx);

	while (stream != end)
	{
		this->_declarations.emplace_back(parse_top_level_statement(stream, end, context));
	}

	for (auto &decl : this->_declarations)
	{
		if (decl.not_null())
		{
			this->_global_ctx.add_export_declaration(decl);
		}
	}

	this->_stage = first_pass_parsed;
	return !this->_global_ctx.has_errors();
}

[[nodiscard]] bool src_file::resolve(void)
{
	bz_assert(this->_stage == first_pass_parsed);

	ctx::parse_context context(this->_global_ctx);

	for (auto &decl : this->_declarations)
	{
		::resolve(decl, context);
	}

	this->_stage = resolved;
	return !this->_global_ctx.has_errors();
}
*/

/*
[[nodiscard]] bool src_file::emit_bitcode(ctx::bitcode_context &context)
{
	// add the declarations to the module
	for (auto &decl : this->_declarations)
	{
		switch (decl.kind())
		{
		case ast::declaration::index<ast::decl_variable>:
			bz_unreachable;
			break;
		case ast::declaration::index<ast::decl_function>:
		{
			auto &func_decl = *decl.get<ast::decl_function_ptr>();
			bc::add_function_to_module(func_decl.body, func_decl.identifier->value, context);
			break;
		}
		case ast::declaration::index<ast::decl_operator>:
		{
			auto &op_decl = *decl.get<ast::decl_operator_ptr>();
			bc::add_function_to_module(op_decl.body, bz::format("__operator_{}", op_decl.op->kind), context);
			break;
		}
		case ast::declaration::index<ast::decl_struct>:
			bz_unreachable;
			break;
		default:
			bz_unreachable;
			break;
		}
	}
	// add the definitions to the module
	for (auto &decl : this->_declarations)
	{
		switch (decl.kind())
		{
		case ast::declaration::index<ast::decl_variable>:
			bz_unreachable;
			break;
		case ast::declaration::index<ast::decl_function>:
		{
			auto &func_decl = *decl.get<ast::decl_function_ptr>();
			if (func_decl.body.body.has_value())
			{
				bc::emit_function_bitcode(func_decl.body, context);
			}
			break;
		}
		case ast::declaration::index<ast::decl_operator>:
		{
			auto &op_decl = *decl.get<ast::decl_operator_ptr>();
			if (op_decl.body.body.has_value())
			{
				bc::emit_function_bitcode(op_decl.body, context);
			}
			break;
		}
		case ast::declaration::index<ast::decl_struct>:
			bz_unreachable;
			break;
		default:
			bz_unreachable;
			break;
		}
	}

	return true;
}
*/
