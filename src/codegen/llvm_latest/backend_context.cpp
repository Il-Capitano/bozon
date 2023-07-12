#include "backend_context.h"
#include "global_data.h"
#include "ctx/global_context.h"
#include "bitcode_context.h"
#include "emit_bitcode.h"
#include "colors.h"

#include <llvm/Support/Host.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Analysis/LoopAnalysisManager.h>
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Bitcode/BitcodeWriter.h>

#if LLVM_VERSION_MAJOR != 16
#error LLVM 16 is required
#endif // LLVM 16

namespace codegen::llvm_latest
{

static bz::array<llvm::Type *, static_cast<int>(ast::type_info::null_t_) + 1>
get_llvm_builtin_types(llvm::LLVMContext &context)
{
	auto const i8_ptr = llvm::Type::getInt8PtrTy(context);
	auto const str_t  = llvm::StructType::create("builtin.str", i8_ptr, i8_ptr);
	auto const null_t = llvm::StructType::create(context, {}, "builtin.__null_t");
	return {
		llvm::Type::getInt8Ty(context),   // int8_
		llvm::Type::getInt16Ty(context),  // int16_
		llvm::Type::getInt32Ty(context),  // int32_
		llvm::Type::getInt64Ty(context),  // int64_
		llvm::Type::getInt8Ty(context),   // uint8_
		llvm::Type::getInt16Ty(context),  // uint16_
		llvm::Type::getInt32Ty(context),  // uint32_
		llvm::Type::getInt64Ty(context),  // uint64_
		llvm::Type::getFloatTy(context),  // float32_
		llvm::Type::getDoubleTy(context), // float64_
		llvm::Type::getInt32Ty(context),  // char_
		str_t,                            // str_
		llvm::Type::getInt1Ty(context),   // bool_
		null_t,                           // null_t_
	};
}

backend_context::backend_context(ctx::global_context &global_ctx, bz::u8string_view target_triple, output_code_kind output_code, bool &error)
	: _llvm_context(),
	  _module("test", this->_llvm_context),
	  _target(nullptr),
	  _target_machine(nullptr),
	  _data_layout(),
	  _llvm_builtin_types(get_llvm_builtin_types(this->_llvm_context)),
	  _platform_abi(abi::platform_abi::generic),
	  _output_code(output_code)
{
	this->_llvm_context.setDiscardValueNames(discard_llvm_value_names);

	auto const llvm_target_triple = llvm::Triple::normalize(std::string(target_triple.data(), target_triple.size()));
	llvm::InitializeAllDisassemblers();
	llvm::InitializeAllTargetInfos();
	llvm::InitializeAllTargets();
	llvm::InitializeAllTargetMCs();
	llvm::InitializeAllAsmParsers();
	llvm::InitializeAllAsmPrinters();

	{
		// set --x86-asm-syntax for LLVM
		char const *args[] = {
			"bozon",
			x86_asm_syntax == x86_asm_syntax_kind::att ? "--x86-asm-syntax=att" : "--x86-asm-syntax=intel"
		};
		if (!llvm::cl::ParseCommandLineOptions(std::size(args), args))
		{
			bz_unreachable;
		}
	}

	std::string target_error = "";
	this->_target = llvm::TargetRegistry::lookupTarget(llvm_target_triple, target_error);
	if (this->_target == nullptr)
	{
		constexpr std::string_view default_start = "No available targets are compatible with triple \"";
		bz::vector<ctx::source_highlight> notes;
		if (do_verbose)
		{
			bz::u8string message = "available targets are: ";
			bool is_first = true;
			for (auto const &target : llvm::TargetRegistry::targets())
			{
				if (is_first)
				{
					message += bz::format("'{}'", target.getName());
					is_first = false;
				}
				else
				{
					message += bz::format(", '{}'", target.getName());
				}
			}
			notes.emplace_back(global_ctx.make_note(std::move(message)));
		}
		if (target_error.substr(0, default_start.length()) == default_start)
		{
			global_ctx.report_error(bz::format(
				"'{}' is not an available target", llvm_target_triple.c_str()
			), std::move(notes));
		}
		else
		{
			global_ctx.report_error(target_error.c_str(), std::move(notes));
		}
		error = true;
		return;
	}

	auto const cpu = "generic";
	auto const features = "";

	llvm::TargetOptions options;
	auto const rm = llvm::Reloc::Model::PIC_;
	auto const codegen_opt_level = [&]() {
		auto const machine_code_opt_level = global_ctx.get_machine_code_opt_level();
		if (machine_code_opt_level.has_value())
		{
			switch (*machine_code_opt_level)
			{
			case 0:
				return llvm::CodeGenOpt::None;
			case 1:
				return llvm::CodeGenOpt::Less;
			case 2:
				return llvm::CodeGenOpt::Default;
			default:
				return llvm::CodeGenOpt::Aggressive;
			}
		}
		else if (size_opt_level != 0)
		{
			return llvm::CodeGenOpt::Default;
		}
		else
		{
			switch (opt_level)
			{
			case 0:
				return llvm::CodeGenOpt::None;
			case 1:
				return llvm::CodeGenOpt::Less;
			case 2:
				return llvm::CodeGenOpt::Default;
			default:
				return llvm::CodeGenOpt::Aggressive;
			}
		}
	}();

	this->_target_machine.reset(this->_target->createTargetMachine(
		llvm_target_triple, cpu, features, options, rm, std::nullopt, codegen_opt_level
	));
	bz_assert(this->_target_machine);

	this->_data_layout = this->_target_machine->createDataLayout();
	this->_module.setDataLayout(*this->_data_layout);
	this->_module.setTargetTriple(llvm_target_triple);

	auto const triple = llvm::Triple(llvm_target_triple);
	auto const os = triple.getOS();
	auto const arch = triple.getArch();

	if (os == llvm::Triple::Win32 && arch == llvm::Triple::x86_64)
	{
		this->_platform_abi = abi::platform_abi::microsoft_x64;
	}
	else if (os == llvm::Triple::Linux && arch == llvm::Triple::x86_64)
	{
		this->_platform_abi = abi::platform_abi::systemv_amd64;
	}
	else
	{
		this->_platform_abi = abi::platform_abi::generic;
	}

	if (this->_platform_abi == abi::platform_abi::generic)
	{
		global_ctx.report_warning(
			ctx::warning_kind::unknown_target,
			bz::format(
				"target '{}' has limited support right now, external function calls may not work as intended",
				llvm_target_triple.c_str()
			)
		);
	}
}

static llvm::PassBuilder get_pass_builder(llvm::TargetMachine *tm)
{
	auto tuning_options = llvm::PipelineTuningOptions();
	// we could later add command line options to set different tuning options
	return llvm::PassBuilder(tm, tuning_options);
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

static void emit_struct_symbols_helper(bz::array_view<ast::statement const> decls, bitcode_context &context)
{
	for (auto const &struct_decl : filter_struct_decls(decls))
	{
		emit_global_type_symbol(struct_decl.info, context);

		if (struct_decl.info.kind == ast::type_info::aggregate)
		{
			if (struct_decl.info.is_generic())
			{
				for (auto const &instantiation_decl : struct_decl.info.generic_instantiations)
				{
					if (instantiation_decl->state == ast::resolve_state::all)
					{
						emit_struct_symbols_helper(instantiation_decl->body.get<bz::vector<ast::statement>>(), context);
					}
				}
			}
			else
			{
				if (struct_decl.info.state == ast::resolve_state::all)
				{
					emit_struct_symbols_helper(struct_decl.info.body.get<bz::vector<ast::statement>>(), context);
				}
			}
		}
	}
}

static void emit_structs_helper(bz::array_view<ast::statement const> decls, bitcode_context &context)
{
	for (auto const &struct_decl : filter_struct_decls(decls))
	{
		emit_global_type(struct_decl.info, context);

		if (struct_decl.info.kind == ast::type_info::aggregate)
		{
			if (struct_decl.info.is_generic())
			{
				for (auto const &instantiation_decl : struct_decl.info.generic_instantiations)
				{
					if (instantiation_decl->state == ast::resolve_state::all)
					{
						emit_structs_helper(instantiation_decl->body.get<bz::vector<ast::statement>>(), context);
					}
				}
			}
			else
			{
				if (struct_decl.info.state == ast::resolve_state::all)
				{
					emit_structs_helper(struct_decl.info.body.get<bz::vector<ast::statement>>(), context);
				}
			}
		}
	}
}

static void emit_variables_helper(bz::array_view<ast::statement const> decls, bitcode_context &context)
{
	for (auto const &var_decl : filter_var_decls(decls))
	{
		if (var_decl.is_global())
		{
			emit_global_variable(var_decl, context);
		}
	}

	for (auto const &struct_decl : filter_struct_decls(decls))
	{
		if (struct_decl.info.kind == ast::type_info::aggregate)
		{
			if (struct_decl.info.is_generic())
			{
				for (auto const &instantiation_decl : struct_decl.info.generic_instantiations)
				{
					if (instantiation_decl->state == ast::resolve_state::all)
					{
						emit_variables_helper(instantiation_decl->body.get<bz::vector<ast::statement>>(), context);
					}
				}
			}
			else
			{
				if (struct_decl.info.state == ast::resolve_state::all)
				{
					emit_variables_helper(struct_decl.info.body.get<bz::vector<ast::statement>>(), context);
				}
			}
		}
	}
}

[[nodiscard]] bool backend_context::generate_and_output_code(
	ctx::global_context &global_ctx,
	bz::optional<bz::u8string_view> output_path
)
{
	if (!this->emit_bitcode(global_ctx))
	{
		return false;
	}

	if (!this->optimize())
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

[[nodiscard]] bool backend_context::emit_bitcode(ctx::global_context &global_ctx)
{
	bitcode_context context(global_ctx, *this, &this->_module);

	llvm::LoopAnalysisManager loop_analysis_manager;
	llvm::FunctionAnalysisManager function_analysis_manager;
	llvm::CGSCCAnalysisManager cgscc_analysis_manager;
	llvm::ModuleAnalysisManager module_analysis_manager;

	auto builder = get_pass_builder(this->_target_machine.get());

	if (opt_level != 0 || size_opt_level != 0)
	{
		builder.registerModuleAnalyses(module_analysis_manager);
		builder.registerCGSCCAnalyses(cgscc_analysis_manager);
		builder.registerFunctionAnalyses(function_analysis_manager);
		builder.registerLoopAnalyses(loop_analysis_manager);
		builder.crossRegisterProxies(
			loop_analysis_manager,
			function_analysis_manager,
			cgscc_analysis_manager,
			module_analysis_manager
		);
	}

	auto const llvm_opt_level = [&]() {
		if (size_opt_level != 0)
		{
			return size_opt_level == 1 ? llvm::OptimizationLevel::Os : llvm::OptimizationLevel::Oz;
		}
		else if (opt_level != 0)
		{
			return
				opt_level == 1 ? llvm::OptimizationLevel::O1 :
				opt_level == 2 ? llvm::OptimizationLevel::O2 :
				llvm::OptimizationLevel::O3;
		}
		else
		{
			return llvm::OptimizationLevel::O0;
		}
	}();

	auto function_pass_manager = llvm_opt_level != llvm::OptimizationLevel::O0
		? builder.buildFunctionSimplificationPipeline(llvm_opt_level, llvm::ThinOrFullLTOPhase::None)
		: llvm::FunctionPassManager();

	if (llvm_opt_level != llvm::OptimizationLevel::O0)
	{
		context.function_analysis_manager = &function_analysis_manager;
		context.function_pass_manager = &function_pass_manager;
	}

	// add declarations to the module
	bz_assert(global_ctx._compile_decls.var_decls.size() == 0);
	for (auto const &file : global_ctx._src_files)
	{
		emit_struct_symbols_helper(file->_declarations, context);
	}
	for (auto const &file : global_ctx._src_files)
	{
		emit_structs_helper(file->_declarations, context);
	}
	for (auto const &file : global_ctx._src_files)
	{
		emit_variables_helper(file->_declarations, context);
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
			context.ensure_function_emission(func);
		}
	}

	emit_necessary_functions(context);

	return !global_ctx.has_errors();
}

[[nodiscard]] bool backend_context::optimize(void)
{
	if (max_opt_iter_count == 0)
	{
		return true;
	}

	auto &module = this->_module;

	auto const llvm_opt_level = [&]() {
		if (size_opt_level != 0)
		{
			return size_opt_level == 1 ? llvm::OptimizationLevel::Os : llvm::OptimizationLevel::Oz;
		}
		else
		{
			switch (opt_level)
			{
			case 0:
				return llvm::OptimizationLevel::O0;
			case 1:
				return llvm::OptimizationLevel::O1;
			case 2:
				return llvm::OptimizationLevel::O2;
			default:
				return llvm::OptimizationLevel::O3;
			}
		}
	}();

	for ([[maybe_unused]] auto const _ : bz::iota(0, max_opt_iter_count))
	{
		llvm::LoopAnalysisManager loop_analysis_manager;
		llvm::FunctionAnalysisManager function_analysis_manager;
		llvm::CGSCCAnalysisManager cgscc_analysis_manager;
		llvm::ModuleAnalysisManager module_analysis_manager;

		auto builder = get_pass_builder(this->_target_machine.get());

		builder.registerModuleAnalyses(module_analysis_manager);
		builder.registerCGSCCAnalyses(cgscc_analysis_manager);
		builder.registerFunctionAnalyses(function_analysis_manager);
		builder.registerLoopAnalyses(loop_analysis_manager);
		builder.crossRegisterProxies(
			loop_analysis_manager,
			function_analysis_manager,
			cgscc_analysis_manager,
			module_analysis_manager
		);

		auto pass_manager = llvm_opt_level == llvm::OptimizationLevel::O0
			? builder.buildO0DefaultPipeline(llvm_opt_level)
			: builder.buildPerModuleDefaultPipeline(llvm_opt_level);

		pass_manager.run(module, module_analysis_manager);
	}

	// always return true
	return true;
}

[[nodiscard]] bool backend_context::emit_file(ctx::global_context &global_ctx, bz::u8string_view output_path)
{
	// only here for debug purposes, the '--emit' option does not control this
#ifndef NDEBUG
	if (debug_ir_output)
	{
		std::error_code ec;
		llvm::raw_fd_ostream file("debug_output.ll", ec, llvm::sys::fs::OF_Text);
		if (!ec)
		{
			this->_module.print(file, nullptr);
		}
		else
		{
			bz::print(stderr, "{}unable to write output.ll{}\n", colors::bright_red, colors::clear);
		}
	}
#endif // !NDEBUG

	switch (this->_output_code)
	{
	case output_code_kind::obj:
		return this->emit_obj(global_ctx, output_path);
	case output_code_kind::asm_:
		return this->emit_asm(global_ctx, output_path);
	case output_code_kind::llvm_bc:
		return this->emit_llvm_bc(global_ctx, output_path);
	case output_code_kind::llvm_ir:
		return this->emit_llvm_ir(global_ctx, output_path);
	case output_code_kind::null:
		return true;
	}
	bz_unreachable;
}

[[nodiscard]] bool backend_context::emit_obj(ctx::global_context &global_ctx, bz::u8string_view output_path)
{
	if (output_path != "-" && !output_path.ends_with(".o"))
	{
		global_ctx.report_warning(
			ctx::warning_kind::bad_file_extension,
			bz::format("object output file '{}' doesn't have the file extension '.o'", output_path)
		);
	}

	// passing "-" to raw_fd_ostream should output to stdout and not create a new file
	// http://llvm.org/doxygen/classllvm_1_1raw__fd__ostream.html#af5462bc0fe5a61eccc662708da280e64
	std::error_code ec;
	llvm::raw_fd_ostream dest(
		llvm::StringRef(output_path.data(), output_path.size()),
		ec, llvm::sys::fs::OF_None
	);

	if (ec)
	{
		global_ctx.report_error(bz::format(
			"unable to open output file '{}', reason: '{}'",
			output_path, ec.message().c_str()
		));
		return false;
	}

	if (output_path == "-")
	{
		global_ctx.report_warning(
			ctx::warning_kind::binary_stdout,
			"outputting binary file to stdout"
		);
	}

	auto &module = this->_module;
	llvm::legacy::PassManager pass_manager;
	auto const target_machine = this->_target_machine.get();
	auto const res = target_machine->addPassesToEmitFile(pass_manager, dest, nullptr, llvm::CGFT_ObjectFile);
	if (res)
	{
		global_ctx.report_error("object file emission is not supported");
		return false;
	}

	pass_manager.run(module);
	return true;
}

[[nodiscard]] bool backend_context::emit_asm(ctx::global_context &global_ctx, bz::u8string_view output_path)
{
	if (output_path != "-" && !output_path.ends_with(".s"))
	{
		global_ctx.report_warning(
			ctx::warning_kind::bad_file_extension,
			bz::format("assembly output file '{}' doesn't have the file extension '.s'", output_path)
		);
	}

	// passing "-" to raw_fd_ostream should output to stdout and not create a new file
	// http://llvm.org/doxygen/classllvm_1_1raw__fd__ostream.html#af5462bc0fe5a61eccc662708da280e64
	std::error_code ec;
	llvm::raw_fd_ostream dest(
		llvm::StringRef(output_path.data(), output_path.size()),
		ec, llvm::sys::fs::OF_None
	);

	if (ec)
	{
		global_ctx.report_error(bz::format(
			"unable to open output file '{}', reason: '{}'",
			output_path, ec.message().c_str()
		));
		return false;
	}

	auto &module = this->_module;
	llvm::legacy::PassManager pass_manager;
	auto const target_machine = this->_target_machine.get();
	auto const res = target_machine->addPassesToEmitFile(pass_manager, dest, nullptr, llvm::CGFT_AssemblyFile);
	if (res)
	{
		global_ctx.report_error("assembly file emission is not supported");
		return false;
	}

	pass_manager.run(module);
	dest.flush();

	return true;
}

[[nodiscard]] bool backend_context::emit_llvm_bc(ctx::global_context &global_ctx, bz::u8string_view output_path)
{
	auto &module = this->_module;
	if (output_path != "-" && !output_path.ends_with(".bc"))
	{
		global_ctx.report_warning(
			ctx::warning_kind::bad_file_extension,
			bz::format("LLVM bitcode output file '{}' doesn't have the file extension '.bc'", output_path)
		);
	}

	// passing "-" to raw_fd_ostream should output to stdout and not create a new file
	// http://llvm.org/doxygen/classllvm_1_1raw__fd__ostream.html#af5462bc0fe5a61eccc662708da280e64
	std::error_code ec;
	llvm::raw_fd_ostream dest(
		llvm::StringRef(output_path.data(), output_path.size()),
		ec, llvm::sys::fs::OF_None
	);

	if (ec)
	{
		global_ctx.report_error(bz::format(
			"unable to open output file '{}', reason: '{}'",
			output_path, ec.message().c_str()
		));
		return false;
	}

	if (output_path == "-")
	{
		global_ctx.report_warning(
			ctx::warning_kind::binary_stdout,
			"outputting binary file to stdout"
		);
	}

	llvm::WriteBitcodeToFile(module, dest);
	return true;
}

[[nodiscard]] bool backend_context::emit_llvm_ir(ctx::global_context &global_ctx, bz::u8string_view output_path)
{
	auto &module = this->_module;
	if (output_path != "-" && !output_path.ends_with(".ll"))
	{
		global_ctx.report_warning(
			ctx::warning_kind::bad_file_extension,
			bz::format("LLVM IR output file '{}' doesn't have the file extension '.ll'", output_path)
		);
	}

	// passing "-" to raw_fd_ostream should output to stdout and not create a new file
	// http://llvm.org/doxygen/classllvm_1_1raw__fd__ostream.html#af5462bc0fe5a61eccc662708da280e64
	std::error_code ec;
	llvm::raw_fd_ostream dest(
		llvm::StringRef(output_path.data(), output_path.size()),
		ec, llvm::sys::fs::OF_None
	);

	if (ec)
	{
		global_ctx.report_error(bz::format(
			"unable to open output file '{}', reason: '{}'",
			output_path, ec.message().c_str()
		));
		return false;
	}

	module.print(dest, nullptr);
	return true;
}

} // namespace codegen::llvm_latest
