#include "src_manager.h"
#include "command_parse_context.h"
#include "bc/emit_bitcode.h"
#include "cl_options.h"

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/IR/LegacyPassManager.h>

namespace ctx
{

void src_manager::report_and_clear_errors_and_warnings(void)
{
	for (auto &err : this->_global_ctx.get_errors_and_warnings())
	{
		print_error_or_warning(err, this->_global_ctx);
	}
	this->_global_ctx.clear_errors_and_warnings();
}

[[nodiscard]] bool src_manager::parse_command_line(int argc, char const **argv)
{
	auto const args = cl::get_args(argc, argv);
	if (args.size() == 1)
	{
		print_help();
		compile_until = compilation_phase::parse_command_line;
		return true;
	}

	command_parse_context context(args, this->_global_ctx);
	::parse_command_line(context);

	auto const good = !this->_global_ctx.has_errors();

	if (!good)
	{
		return false;
	}
	else
	{
		if (display_help)
		{
			print_help();
			compile_until = compilation_phase::parse_command_line;
		}
		else if (display_version)
		{
			bz::print(version_info);
			compile_until = compilation_phase::parse_command_line;
		}
		else if (display_warning_help)
		{
			print_warning_help();
			compile_until = compilation_phase::parse_command_line;
		}
		return true;
	}
}

[[nodiscard]] bool src_manager::parse_global_symbols(void)
{
	if (source_file == "")
	{
		this->_global_ctx.report_error(error{
			warning_kind::_last,
			global_context::compiler_file_id, 0,
			char_pos(), char_pos(), char_pos(),
			"no source file was provided",
			{}, {}
		});
		return false;
	}
	this->add_file(source_file);
	if (output_file_name == "")
	{
		auto const slash = source_file.rfind('/');
		auto const dot = source_file.rfind('.');
		output_file_name = bz::u8string_view(slash + 1, dot);
		output_file_name += ".o";
	}

	auto &file = this->_src_files.front();
	if (!file.parse_global_symbols())
	{
		if (file._stage == src_file::constructed)
		{
			this->_global_ctx.report_error(error{
				warning_kind::_last,
				global_context::compiler_file_id, 0,
				char_pos(), char_pos(), char_pos(),
				bz::format("unable to read file '{}'", convert_string_for_message(file.get_file_name())),
				{}, {}
			});
		}
		return false;
	}

	return true;
}

[[nodiscard]] bool src_manager::parse(void)
{
	for (auto &file : this->_src_files)
	{
		if (!file.parse())
		{
			return false;
		}
	}

	return true;
}

[[nodiscard]] bool src_manager::emit_bitcode(void)
{
	bitcode_context context(this->_global_ctx);

	// add declarations to the module
	for ([[maybe_unused]] auto const var_decl : this->_global_ctx._compile_decls.var_decls)
	{
		bz_unreachable;
	}
	for (auto const func : this->_global_ctx._compile_decls.funcs)
	{
		func->resolve_symbol_name();
		bc::add_function_to_module(func, context);
	}

	// add the definitions to the module
	for ([[maybe_unused]] auto const var_decl : this->_global_ctx._compile_decls.var_decls)
	{
		bz_unreachable;
	}
	for (auto const func_decl : this->_global_ctx._compile_decls.funcs)
	{
		if (func_decl->body.not_null())
		{
			bc::emit_function_bitcode(*func_decl, context);
		}
	}

//	context.module.print(llvm::errs(), nullptr);
	auto &module = this->_global_ctx._module;

	{
		std::error_code ec;
		llvm::raw_fd_ostream file("output.ll", ec, llvm::sys::fs::OF_Text);
		bz_assert(!ec);
		module.print(file, nullptr);
	}

	auto const output_file = output_file_name.as_string_view();
	std::error_code ec;
	llvm::raw_fd_ostream dest(
		llvm::StringRef(output_file.data(), output_file.size()),
		ec, llvm::sys::fs::OF_None
	);
	bz_assert(!ec);

	auto const target_machine = this->_global_ctx._target_machine;

	llvm::legacy::PassManager pass;
	auto const file_type = llvm::CGFT_ObjectFile;
	auto const res = target_machine->addPassesToEmitFile(pass, dest, nullptr, file_type);
	bz_assert(!res);
	pass.run(module);
	dest.flush();

	return true;
}

} // namespace ctx
