#include "src_manager.h"
#include "bc/emit_bitcode.h"

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

[[nodiscard]] bool src_manager::tokenize(void)
{
	for (auto &file : this->_src_files)
	{
		if (!file.read_file())
		{
			bz::printf("error: unable to read file {}\n", file.get_file_name());
			bz::print("exiting...\n");
			return false;
		}

		if (!file.tokenize())
		{
			auto const error_count = this->global_ctx.get_error_count();
			bz::printf(
				"{} {} occurred while tokenizing {}\n",
				error_count, error_count == 1 ? "error" : "errors",
				file.get_file_name()
			);
			file.report_and_clear_errors_and_warnings();
			bz::print("exiting...\n");
			return false;
		}
		else
		{
			// report warnings if any
			file.report_and_clear_errors_and_warnings();
		}
	}

	return true;
}

[[nodiscard]] bool src_manager::first_pass_parse(void)
{
	for (auto &file : this->_src_files)
	{
		if (!file.first_pass_parse())
		{
			auto const error_count = this->global_ctx.get_error_count();
			bz::printf(
				"{} {} occurred while first pass of parsing {}\n",
				error_count, error_count == 1 ? "error" : "errors",
				file.get_file_name()
			);
			file.report_and_clear_errors_and_warnings();
			bz::print("exiting...\n");
			return false;
		}
		else
		{
			// report warnings if any
			file.report_and_clear_errors_and_warnings();
		}
	}

	return true;
}

[[nodiscard]] bool src_manager::resolve(void)
{
	for (auto &file : this->_src_files)
	{
		if (!file.resolve())
		{
			auto const error_count = this->global_ctx.get_error_count();
			bz::printf(
				"{} {} occurred while resolving {}\n",
				error_count, error_count == 1 ? "error" : "errors",
				file.get_file_name()
			);
			file.report_and_clear_errors_and_warnings();
			bz::print("exiting...\n");
			return false;
		}
		else
		{
			// report warnings if any
			file.report_and_clear_errors_and_warnings();
		}
	}

	return true;
}

[[nodiscard]] bool src_manager::emit_bitcode(void)
{
	bitcode_context context(this->global_ctx);

	// add declarations to the module
	for ([[maybe_unused]] auto const var_decl : this->global_ctx._compile_decls.var_decls)
	{
		bz_assert(false);
	}
	for (auto const func_decl : this->global_ctx._compile_decls.func_decls)
	{
		bc::add_function_to_module(func_decl->body, func_decl->identifier->value, context);
	}
	for (auto const op_decl : this->global_ctx._compile_decls.op_decls)
	{
		bc::add_function_to_module(op_decl->body, bz::format("__operator_{}", op_decl->op->kind), context);
	}

	// add the definitions to the module
	for ([[maybe_unused]] auto const var_decl : this->global_ctx._compile_decls.var_decls)
	{
		bz_assert(false);
	}
	for (auto const func_decl : this->global_ctx._compile_decls.func_decls)
	{
		if (func_decl->body.body.has_value())
		{
			bc::emit_function_bitcode(func_decl->body, context);
		}
	}
	for (auto const op_decl : this->global_ctx._compile_decls.op_decls)
	{
		if (op_decl->body.body.has_value())
		{
			bc::emit_function_bitcode(op_decl->body, context);
		}
	}

//	context.module.print(llvm::errs(), nullptr);
	{
		std::error_code ec;
		llvm::raw_fd_ostream file("output.ll", ec, llvm::sys::fs::OF_Text);
		bz_assert(!ec);
		context.module.print(file, nullptr);
	}

	auto const target_triple = llvm::sys::getDefaultTargetTriple();
	llvm::InitializeAllTargetInfos();
	llvm::InitializeAllTargets();
	llvm::InitializeAllTargetMCs();
	llvm::InitializeAllAsmParsers();
	llvm::InitializeAllAsmPrinters();

	std::string error = "";
	auto const target = llvm::TargetRegistry::lookupTarget(target_triple, error);
	bz_assert(target);
	auto const cpu = "generic";
	auto const features = "";

	llvm::TargetOptions options;
	auto rm = llvm::Optional<llvm::Reloc::Model>();
	auto const target_machine = target->createTargetMachine(target_triple, cpu, features, options, rm);
	context.module.setDataLayout(target_machine->createDataLayout());
	context.module.setTargetTriple(target_triple);
	auto const output_file = "output.o";
	std::error_code ec;
	llvm::raw_fd_ostream dest(output_file, ec, llvm::sys::fs::OF_None);
	bz_assert(!ec);

	llvm::legacy::PassManager pass;
	auto const file_type = llvm::CGFT_ObjectFile;
	auto const res = target_machine->addPassesToEmitFile(pass, dest, nullptr, file_type);
	bz_assert(!res);
	pass.run(context.module);
	dest.flush();

	return true;
}


} // namespace ctx
