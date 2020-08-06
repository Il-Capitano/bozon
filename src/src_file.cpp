#include "src_file.h"
#include "ctx/global_context.h"
#include "bc/emit_bitcode.h"
#include "parse/statement_parser.h"

static bz::u8string read_text_from_file(std::ifstream &file)
{
	std::string file_content{
		std::istreambuf_iterator<char>(file),
		std::istreambuf_iterator<char>()
	};
	auto const file_content_view = bz::u8string_view(
		file_content.data(),
		file_content.data() + file_content.size()
	);

	bz::u8string file_str;
	file_str.reserve(file_content_view.size());
	for (auto it = file_content_view.begin(); it != file_content_view.end();  ++it)
	{
		// we use '\n' for line endings and not '\r\n' or '\r'
		if (*it == '\r')
		{
			file_str += '\n';
			if (it + 1 != file_content_view.end() && *(it + 1) == '\n')
			{
				++it;
			}
		}
		else
		{
			file_str += *it;
		}
	}

	bz_assert(file_str.verify());
	return file_str;
}


src_file::src_file(bz::u8string_view file_name, ctx::global_context &global_ctx)
	: _stage(constructed),
	  _file_id(static_cast<uint32_t>(global_ctx.src_files.size())),
	  _file_name(file_name),
	  _file(), _tokens(),
	  _global_ctx(global_ctx),
	  _declarations{},
	  _global_decls{}
{
	global_ctx.src_files.push_back(this);
}


void src_file::report_and_clear_errors_and_warnings(void)
{
	if (this->_global_ctx.has_errors_or_warnings())
	{
		for (auto &err : this->_global_ctx.get_errors_and_warnings())
		{
			print_error_or_warning(this->_file.begin(), this->_file.end(), err, this->_global_ctx);
		}
		this->_global_ctx.clear_errors_and_warnings();
	}
}

[[nodiscard]] bool src_file::read_file(void)
{
	bz_assert(this->_stage == constructed);
	auto file_name = this->_file_name;
	file_name += '\0';
	std::ifstream file(reinterpret_cast<char const *>(file_name.data()));

	if (!file.good())
	{
		return false;
	}

	this->_file = read_text_from_file(file);
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
	bz_assert(this->_stage == tokenized);
	bz_assert(this->_tokens.size() != 0);
	bz_assert(this->_tokens.back().kind == lex::token::eof);
	auto stream = this->_tokens.cbegin();
	auto end    = this->_tokens.cend() - 1;

	ctx::parse_context context(this->_global_ctx);

	this->_declarations = parse::parse_global_statements(stream, end, context);

	this->_global_decls = ctx::get_default_decls();
	for (auto &decl : this->_declarations)
	{
		switch (decl.kind())
		{
		case ast::statement::index<ast::decl_variable>:
		{
			auto &var_decl = *decl.get<ast::decl_variable_ptr>();
			this->_global_decls.var_decls.push_back(&var_decl);
			break;
		}
		case ast::statement::index<ast::decl_function>:
		{
			auto &func_decl = *decl.get<ast::decl_function_ptr>();
			this->_global_decls.add_function(func_decl);
			break;
		}
		case ast::statement::index<ast::decl_operator>:
		{
			auto &op_decl = *decl.get<ast::decl_operator_ptr>();
			this->_global_decls.add_operator(op_decl);
			break;
		}
		default:
			break;
		}
	}

	this->_stage = parsed_global_symbols;
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
			bz_assert(false);
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
			bz_assert(false);
			break;
		default:
			bz_assert(false);
			break;
		}
	}
	// add the definitions to the module
	for (auto &decl : this->_declarations)
	{
		switch (decl.kind())
		{
		case ast::declaration::index<ast::decl_variable>:
			bz_assert(false);
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
			bz_assert(false);
			break;
		default:
			bz_assert(false);
			break;
		}
	}

	return true;
}
*/
