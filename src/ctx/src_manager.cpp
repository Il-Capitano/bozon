#include "src_manager.h"
#include "command_parse_context.h"
#include "bc/emit_bitcode.h"
#include "cl_options.h"
#include "colors.h"

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Transforms/Utils.h>

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
			if (do_verbose)
			{
				print_verbose_help();
			}
			else
			{
				print_help();
			}
			compile_until = compilation_phase::parse_command_line;
		}
		else if (display_version)
		{
			bz::print(version_info);
			compile_until = compilation_phase::parse_command_line;
		}
		else if (display_opt_help)
		{
			print_opt_help();
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

[[nodiscard]] bool src_manager::initialize_llvm(void)
{
	auto const target_triple = target == "" || target == "native"
		? llvm::sys::getDefaultTargetTriple()
		: std::string(target.data_as_char_ptr(), target.size());
	llvm::InitializeAllTargetInfos();
	llvm::InitializeAllTargets();
	llvm::InitializeAllTargetMCs();
	llvm::InitializeAllAsmParsers();
	llvm::InitializeAllAsmPrinters();

	std::string target_error = "";
	auto const target = llvm::TargetRegistry::lookupTarget(target_triple, target_error);
	if (target == nullptr)
	{
		constexpr std::string_view default_start = "No available targets are compatible with triple \"";
		if (target_error.substr(0, default_start.length()) == default_start)
		{
			this->_global_ctx.report_error(bz::format(
				"'{}' is not an available target",
				convert_string_for_message(target_triple.c_str())
			));
		}
		else
		{
			this->_global_ctx.report_error(target_error.c_str());
		}
		return false;
	}
	auto const cpu = "generic";
	auto const features = "";

	llvm::TargetOptions options;
	auto rm = llvm::Optional<llvm::Reloc::Model>();
	this->_global_ctx._target_machine.reset(target->createTargetMachine(target_triple, cpu, features, options, rm));
	bz_assert(this->_global_ctx._target_machine);
	this->_global_ctx._data_layout = this->_global_ctx._target_machine->createDataLayout();
	this->_global_ctx._module.setDataLayout(*this->_global_ctx._data_layout);
	this->_global_ctx._module.setTargetTriple(target_triple);

	auto const triple = this->_global_ctx._target_machine->getTargetTriple();
	auto const os = triple.getOS();
	auto const arch = triple.getArch();

	if (os == llvm::Triple::Win32 && arch == llvm::Triple::x86_64)
	{
		this->_global_ctx._platform_abi = abi::platform_abi::microsoft_x64;
	}
	else
	{
		this->_global_ctx._platform_abi = abi::platform_abi::generic;
	}
	return true;
}

[[nodiscard]] bool src_manager::parse_global_symbols(void)
{
	if (source_file == "")
	{
		this->_global_ctx.report_error("no source file was provided");
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
			this->_global_ctx.report_error(bz::format(
				"unable to read file '{}'",
				convert_string_for_message(file.get_file_name())
			));
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
	bz_assert(this->_global_ctx._compile_decls.var_decls.size() == 0);
	for (auto const func : this->_global_ctx._compile_decls.funcs)
	{
		func->resolve_symbol_name();
		bc::add_function_to_module(func, context);
	}

	// add the definitions to the module
	for (auto const func_decl : this->_global_ctx._compile_decls.funcs)
	{
		if (func_decl->body.not_null())
		{
			bc::emit_function_bitcode(*func_decl, context);
		}
	}

	this->optimize();

	return true;
}

[[nodiscard]] bool src_manager::emit_file(void)
{
	auto &module = this->_global_ctx._module;
	auto const output_file = output_file_name.as_string_view();

	std::error_code ec;
	llvm::raw_fd_ostream dest(
		llvm::StringRef(output_file.data(), output_file.size()),
		ec, llvm::sys::fs::OF_None
	);
	if (ec)
	{
		this->_global_ctx.report_error(bz::format(
			"unable to open output file '{}', reason: '{}'",
			output_file, ec.message().c_str()
		));
		return false;
	}

	// only here for debug purposes, the '--emit' option does not control this
	if (debug_ir_output)
	{
		std::error_code ec;
		llvm::raw_fd_ostream file("output.ll", ec, llvm::sys::fs::OF_Text);
		if (!ec)
		{
			module.print(file, nullptr);
		}
		else
		{
			bz::print("{}unable to write output.ll{}\n", colors::bright_red, colors::clear);
		}
	}

	if (emit_file_type == emit_type::llvm_bc || emit_file_type == emit_type::llvm_ir)
	{
		this->_global_ctx.report_error("llvm bitcode and ir emission is not yet supported");
		return false;
	}
	else
	{
		bz_assert(emit_file_type == emit_type::object || emit_file_type == emit_type::asm_);
		llvm::legacy::PassManager pass_manager;
		auto const file_type = emit_file_type == emit_type::object
			? llvm::CGFT_ObjectFile
			: llvm::CGFT_AssemblyFile;

		auto const target_machine = this->_global_ctx._target_machine.get();
		auto const res = target_machine->addPassesToEmitFile(pass_manager, dest, nullptr, file_type);
		if (res)
		{
			this->_global_ctx.report_error(bz::format(
				"{} file emission is not supported",
				file_type == llvm::CGFT_ObjectFile ? "object" : "assembly"
			));
			return false;
		}

		pass_manager.run(module);
		dest.flush();

		return true;
	}
}

void src_manager::optimize(void)
{
	if (!is_any_optimization_enabled())
	{
		return;
	}

	auto &module = this->_global_ctx._module;
	llvm::legacy::PassManager opt_pass_manager;

#define add_opt(kind, llvm_func)                              \
do {                                                          \
    if (is_optimization_enabled(bc::optimization_kind::kind)) \
    {                                                         \
        opt_pass_manager.add(llvm_func());                    \
    }                                                         \
}                                                             \
while (false)

	// optimizations
	add_opt(instcombine, llvm::createInstructionCombiningPass);
	add_opt(mem2reg,     llvm::createPromoteMemoryToRegisterPass);
	add_opt(simplifycfg, llvm::createCFGSimplificationPass);
	add_opt(reassociate, llvm::createReassociatePass);
	add_opt(gvn,         llvm::createGVNPass);

	{
		int i = 0;
		// opt_pass_manager.run returns true if any of the passes modified the code
		while (i < 4 && opt_pass_manager.run(module))
		{
			++i;
		}
	}
}

} // namespace ctx
