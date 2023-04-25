#include "src_file.h"
#include "ctx/global_context.h"
#include "ctx/lex_context.h"
#include "ctx/parse_context.h"
#include "ctx/bitcode_context.h"
#include "lex/lexer.h"
#include "parse/statement_parser.h"
#include "resolve/consteval.h"
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

/*
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
*/

static void add_import_decls(src_file &file, bz::array_view<bz::u8string_view const> scope, ast::global_scope_symbol_list_t const &import_symbols)
{
	ast::arena_vector<bz::u8string_view> id_buffer;
	bz::array<bz::u8string_view, 8> stack_id_buffer;

	auto const scope_size = scope.size();

	if (scope_size >= stack_id_buffer.size())
	{
		id_buffer = scope;
	}
	else
	{
		for (auto const i : bz::iota(0, scope_size))
		{
			stack_id_buffer[i] = scope[i];
		}
	}

	auto const get_id = [&](bz::array_view<bz::u8string_view const> symbol_id) -> bz::array_view<bz::u8string_view const> {
		if (scope_size + symbol_id.size() <= stack_id_buffer.size())
		{
			for (auto const i : bz::iota(0, symbol_id.size()))
			{
				stack_id_buffer[scope_size + i] = symbol_id[i];
			}

			return bz::array_view(stack_id_buffer.data(), stack_id_buffer.size()).slice(0, scope_size + symbol_id.size());
		}
		else
		{
			auto const start_size = id_buffer.size();
			id_buffer.resize(scope_size + symbol_id.size());
			// initialize id_buffer
			if (start_size < scope_size)
			{
				for (auto const i : bz::iota(0, scope_size))
				{
					id_buffer[i] = scope[i];
				}
			}

			for (auto const i : bz::iota(0, symbol_id.size()))
			{
				id_buffer[scope_size + i] = symbol_id[i];
			}

			return id_buffer;
		}
	};

	for (auto const &func_set : import_symbols.function_sets)
	{
		for (auto const func_decl : func_set.func_decls)
		{
			file._global_scope.get_global().all_symbols.add_function(get_id(func_decl->id.values), *func_decl);
		}
		for (auto const alias_decl : func_set.alias_decls)
		{
			file._global_scope.get_global().all_symbols.add_function_alias(get_id(alias_decl->id.values), *alias_decl);
		}
	}

	for (auto const &op_set : import_symbols.operator_sets)
	{
		for (auto const op_decl : op_set.op_decls)
		{
			file._global_scope.get_global().all_symbols.add_operator(*op_decl);
		}
	}

	for (auto const var_decl : import_symbols.variables)
	{
		file._global_scope.get_global().all_symbols.add_variable(get_id(var_decl->get_id().values), *var_decl);
	}

	for (auto const &variadic_var_decl : import_symbols.variadic_variables)
	{
		file._global_scope.get_global().all_symbols.add_variable(
			get_id(variadic_var_decl.original_decl->get_id().values),
			*variadic_var_decl.original_decl,
			variadic_var_decl.variadic_decls
		);
	}

	for (auto const type_alias_decl : import_symbols.type_aliases)
	{
		file._global_scope.get_global().all_symbols.add_type_alias(get_id(type_alias_decl->id.values), *type_alias_decl);
	}

	for (auto const struct_decl : import_symbols.structs)
	{
		file._global_scope.get_global().all_symbols.add_struct(get_id(struct_decl->id.values), *struct_decl);
	}

	for (auto const enum_decl : import_symbols.enums)
	{
		file._global_scope.get_global().all_symbols.add_enum(get_id(enum_decl->id.values), *enum_decl);
	}

	static_assert(sizeof (ast::global_scope_t) == 624);
}


src_file::src_file(fs::path const &file_path, uint32_t file_id, bz::vector<bz::u8string> scope, bool is_library_file)
	: _stage(constructed),
	  _is_library_file(is_library_file),
	  _file_id(file_id),
	  _file_path(fs::canonical(file_path)),
	  _file(), _tokens(),
	  _declarations{},
	  _global_scope{},
	  _scope_container(std::move(scope)),
	  _scope()
{
	this->_file_path.make_preferred();
	this->_scope = this->_scope_container.transform([](auto const &s) { return s.as_string_view(); }).collect();
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

	if (global_ctx._builtin_global_scope != &this->_global_scope)
	{
		this->_global_scope = ctx::get_default_decls(global_ctx._builtin_global_scope);
	}
	else
	{
		this->_global_scope = ast::make_global_scope({});
	}

	ctx::parse_context context(global_ctx);
	context.current_global_scope = &this->_global_scope;

	this->_declarations = parse::parse_global_statements(stream, end, context);

	bz::vector<ast::decl_import const *> imports = {};

	for (auto &decl : this->_declarations)
	{
		static_assert(sizeof (ast::global_scope_t) == 624);
		static_assert(ast::statement_types::size() == 17);
		switch (decl.kind())
		{
		case ast::statement::index<ast::decl_variable>:
		{
			auto &var_decl = decl.get<ast::decl_variable>();
			this->_global_scope.get_global().add_variable({}, var_decl);
			break;
		}
		case ast::statement::index<ast::decl_function>:
		{
			auto &func_decl = decl.get<ast::decl_function>();
			this->_global_scope.get_global().add_function({}, func_decl);
			break;
		}
		case ast::statement::index<ast::decl_operator>:
		{
			auto &op_decl = decl.get<ast::decl_operator>();
			this->_global_scope.get_global().add_operator(op_decl);
			break;
		}
		case ast::statement::index<ast::decl_function_alias>:
		{
			auto &alias_decl = decl.get<ast::decl_function_alias>();
			this->_global_scope.get_global().add_function_alias({}, alias_decl);
			break;
		}
		case ast::statement::index<ast::decl_operator_alias>:
		{
			auto &alias_decl = decl.get<ast::decl_operator_alias>();
			this->_global_scope.get_global().add_operator_alias(alias_decl);
			break;
		}
		case ast::statement::index<ast::decl_type_alias>:
		{
			auto &alias_decl = decl.get<ast::decl_type_alias>();
			this->_global_scope.get_global().add_type_alias({}, alias_decl);
			break;
		}
		case ast::statement::index<ast::decl_struct>:
		{
			auto &struct_decl = decl.get<ast::decl_struct>();
			this->_global_scope.get_global().add_struct({}, struct_decl);
			break;
		}
		case ast::statement::index<ast::decl_enum>:
		{
			auto &enum_decl = decl.get<ast::decl_enum>();
			this->_global_scope.get_global().add_enum({}, enum_decl);
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
		auto const import_file_ids = global_ctx.add_module(this->_file_id, import->id);
		for (auto const &[id, scope] : import_file_ids)
		{
			auto const &import_decls = global_ctx.get_file_global_scope(id);
			auto const &import_symbols = import_decls.get_global().export_symbols;
				add_import_decls(*this, scope, import_symbols);
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
	context.current_global_scope = &this->_global_scope;

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
	context.current_global_scope = &this->_global_scope;

	for (auto &decl : this->_declarations)
	{
		resolve::consteval_try_without_error_decl(decl, context);
	}
}
