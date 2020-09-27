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
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>

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
			this->_global_ctx.report_error(error{
				warning_kind::_last,
				global_context::compiler_file_id, 0,
				char_pos(), char_pos(), char_pos(),
				bz::format("'{}' is not an available target", convert_string_for_message(target_triple.c_str())),
				{}, {}
			});
		}
		else
		{
			this->_global_ctx.report_error(error{
				warning_kind::_last,
				global_context::compiler_file_id, 0,
				char_pos(), char_pos(), char_pos(),
				target_error.c_str(),
				{}, {}
			});
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

//	context.module.print(llvm::errs(), nullptr);
	auto &module = this->_global_ctx._module;

	auto const output_file = output_file_name.as_string_view();
	std::error_code ec;
	llvm::raw_fd_ostream dest(
		llvm::StringRef(output_file.data(), output_file.size()),
		ec, llvm::sys::fs::OF_None
	);
	bz_assert(!ec);

	auto const target_machine = this->_global_ctx._target_machine.get();


	llvm::legacy::PassManager pass_manager;

	// optimizations
	pass_manager.add(llvm::createInstructionCombiningPass());
	pass_manager.add(llvm::createReassociatePass());
	pass_manager.add(llvm::createGVNPass());
	pass_manager.add(llvm::createCFGSimplificationPass());
	pass_manager.add(llvm::createInstructionCombiningPass());
//	pass_manager.add(llvm::createReassociatePass());
	pass_manager.add(llvm::createGVNPass());
	pass_manager.add(llvm::createCFGSimplificationPass());

	// object file emission
	auto const file_type = []() {
		switch (emit_file_type)
		{
		case emit_type::object:
			return llvm::CGFT_ObjectFile;
		case emit_type::asm_:
			return llvm::CGFT_AssemblyFile;
		case emit_type::llvm_bc:
		case emit_type::llvm_ir:
			bz_unreachable;
		}
		bz_unreachable;
	}();
	auto const res = target_machine->addPassesToEmitFile(pass_manager, dest, nullptr, file_type);
	bz_assert(!res);
	pass_manager.run(module);
	dest.flush();

	// only here for debug purposes, the '--emit' option does not control this
	{
		std::error_code ec;
		llvm::raw_fd_ostream file("output.ll", ec, llvm::sys::fs::OF_Text);
		bz_assert(!ec);
		module.print(file, nullptr);
	}

	return true;
}

} // namespace ctx
