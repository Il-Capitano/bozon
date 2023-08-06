#include "backend_context.h"
#include "codegen_context.h"
#include "codegen.h"
#include "codegen/target.h"
#include "ctx/global_context.h"

namespace codegen::c
{

backend_context::backend_context(void)
{}

[[nodiscard]] bool backend_context::generate_and_output_code(
	ctx::global_context &global_ctx,
	bz::optional<bz::u8string_view> output_path
)
{
	if (!this->generate_code(global_ctx))
	{
		return false;
	}

	if (output_path.has_value())
	{
		if (!this->emit_file(global_ctx, output_path.get()))
		{
			return false;
		}
	}

	return true;
}

static auto filter_struct_decls(bz::array_view<ast::statement const> decls)
{
	return decls
		.filter([](auto const &stmt) { return stmt.template is<ast::decl_struct>(); })
		.transform([](auto const &stmt) -> ast::decl_struct const & { return stmt.template get<ast::decl_struct>(); });
}

static auto filter_var_decls(bz::array_view<ast::statement const> decls)
{
	return decls
		.filter([](auto const &stmt) { return stmt.template is<ast::decl_variable>(); })
		.transform([](auto const &stmt) -> ast::decl_variable const & { return stmt.template get<ast::decl_variable>(); });
}

static void generate_structs_helper(bz::array_view<ast::statement const> decls, codegen_context &context)
{
	for (auto const &struct_decl : filter_struct_decls(decls))
	{
		if (struct_decl.info.is_generic())
		{
			for (auto const &instantiation_info : struct_decl.info.generic_instantiations)
			{
				generate_struct(*instantiation_info, context);
				if (instantiation_info->kind == ast::type_info::aggregate && instantiation_info->state == ast::resolve_state::all)
				{
					generate_structs_helper(instantiation_info->body.get<bz::vector<ast::statement>>(), context);
				}
			}
		}
		else
		{
			generate_struct(struct_decl.info, context);
			if (struct_decl.info.kind == ast::type_info::aggregate && struct_decl.info.state == ast::resolve_state::all)
			{
				generate_structs_helper(struct_decl.info.body.get<bz::vector<ast::statement>>(), context);
			}
		}
	}
}

static void generate_variables_helper(bz::array_view<ast::statement const> decls, codegen_context &context)
{
	for (auto const &var_decl : filter_var_decls(decls))
	{
		if (var_decl.is_global())
		{
			generate_global_variable(var_decl, context);
		}
	}

	for (auto const &struct_decl : filter_struct_decls(decls))
	{
		if (struct_decl.info.is_generic())
		{
			for (auto const &instantiation_info : struct_decl.info.generic_instantiations)
			{
				if (instantiation_info->kind == ast::type_info::aggregate && instantiation_info->state == ast::resolve_state::all)
				{
					generate_variables_helper(instantiation_info->body.get<bz::vector<ast::statement>>(), context);
				}
			}
		}
		else if (struct_decl.info.kind == ast::type_info::aggregate && struct_decl.info.state == ast::resolve_state::all)
		{
			generate_variables_helper(struct_decl.info.body.get<bz::vector<ast::statement>>(), context);
		}
	}
}

bool backend_context::generate_code(ctx::global_context &global_ctx)
{
	auto context = codegen_context(global_ctx.target_triple.get_target_properties());

	bz_assert(global_ctx._compile_decls.var_decls.size() == 0);
	for (auto const &file : global_ctx._src_files)
	{
		generate_structs_helper(file->_declarations, context);
	}
	for (auto const &file : global_ctx._src_files)
	{
		generate_variables_helper(file->_declarations, context);
	}
	for (auto const func : global_ctx._compile_decls.funcs)
	{
		if (
			func->is_external_linkage()
			&& !(
				global_ctx._main == nullptr
				&& func->symbol_name == "main"
			)
		)
		{
			context.ensure_function_generation(func);
		}
	}

	generate_necessary_functions(context);

	this->code_string = context.get_code_string();
	return true;
}

bool backend_context::emit_file(ctx::global_context &global_ctx, bz::u8string_view output_path)
{
	if (output_path != "-" && !output_path.ends_with(".c"))
	{
		global_ctx.report_warning(
			ctx::warning_kind::bad_file_extension,
			bz::format("C output file '{}' doesn't have the file extension '.c'", output_path)
		);
	}

	if (output_path == "-")
	{
		bz::print(stdout, this->code_string);
		return true;
	}

	auto const output_path_sv = std::string_view(output_path.data(), output_path.size());
	auto file = std::ofstream(std::string(output_path_sv));

	if (!file)
	{
		return false;
	}

	bz::print(file, this->code_string);

	return true;
}

} // namespace codegen::c
