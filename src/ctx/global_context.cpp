#include "global_context.h"
#include "bitcode_context.h"
#include "ast/statement.h"
#include "cl_options.h"
#include "bc/runtime/runtime_emit_bitcode.h"
#include "colors.h"

#include <cassert>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/AggressiveInstCombine/AggressiveInstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Transforms/Utils.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/MC/MCAsmInfo.h>

namespace ctx
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

decl_set get_default_decls(void)
{
	return {
		{}, // var_decls
		{}, // func_sets
		{}, // op_sets
		{}, // type_aliases
		{}, // types
	};
}

global_context::global_context(void)
	: _compile_decls{},
	  _errors{},
	  _builtin_type_infos(ast::make_builtin_type_infos()),
	  _builtin_types{},
	  _builtin_functions{},
	  _builtin_universal_functions(ast::make_builtin_universal_functions()),
	  _llvm_context(),
	  _module("test", this->_llvm_context),
	  _target(nullptr),
	  _target_machine(nullptr),
	  _data_layout(),
	  _llvm_builtin_types(get_llvm_builtin_types(this->_llvm_context)),
	  _comptime_executor(*this)
{}

ast::type_info *global_context::get_builtin_type_info(uint32_t kind)
{
	bz_assert(kind <= ast::type_info::null_t_);
	return &this->_builtin_type_infos[kind];
}

ast::type_info *global_context::get_usize_type_info(void) const
{
	bz_assert(this->_builtin_types[ast::type_info::null_t_ + 1].name == "usize");
	bz_assert(this->_builtin_types[ast::type_info::null_t_ + 1].type.is<ast::ts_base_type>());
	return this->_builtin_types[ast::type_info::null_t_ + 1].type.get<ast::ts_base_type>().info;
}

ast::type_info *global_context::get_isize_type_info(void) const
{
	bz_assert(this->_builtin_types[ast::type_info::null_t_ + 2].name == "isize");
	bz_assert(this->_builtin_types[ast::type_info::null_t_ + 2].type.is<ast::ts_base_type>());
	return this->_builtin_types[ast::type_info::null_t_ + 2].type.get<ast::ts_base_type>().info;
}

ast::typespec_view global_context::get_builtin_type(bz::u8string_view name)
{
	auto const it = std::find_if(this->_builtin_types.begin(), this->_builtin_types.end(), [name](auto const &builtin_type) {
		return builtin_type.name == name;
	});
	if (it != this->_builtin_types.end())
	{
		return it->type;
	}
	else
	{
		return {};
	}
}

ast::function_body *global_context::get_builtin_function(uint32_t kind)
{
	static_assert(ast::function_body::_builtin_last - ast::function_body::_builtin_first == 115);
	bz_assert(kind < this->_builtin_functions.size());
	if (kind == ast::function_body::builtin_str_eq)
	{
		bz_assert(this->_builtin_str_eq_func != nullptr);
		return this->_builtin_str_eq_func;
	}
	else if (kind == ast::function_body::builtin_str_neq)
	{
		bz_assert(this->_builtin_str_neq_func != nullptr);
		return this->_builtin_str_neq_func;
	}
	else if (kind == ast::function_body::builtin_str_length)
	{
		bz_assert(this->_builtin_str_length_func != nullptr);
		return this->_builtin_str_length_func;
	}
	else if (kind == ast::function_body::builtin_str_starts_with)
	{
		bz_assert(this->_builtin_str_starts_with_func != nullptr);
		return this->_builtin_str_starts_with_func;
	}
	else if (kind == ast::function_body::builtin_str_ends_with)
	{
		bz_assert(this->_builtin_str_ends_with_func != nullptr);
		return this->_builtin_str_ends_with_func;
	}

	return &this->_builtin_functions[kind];
}

bz::array_view<uint32_t const> global_context::get_builtin_universal_functions(bz::u8string_view id)
{
	auto const it = std::find_if(
		this->_builtin_universal_functions.begin(), this->_builtin_universal_functions.end(),
		[id](auto const &set) {
			return id == set.id;
		}
	);
	if (it != this->_builtin_universal_functions.end())
	{
		return it->func_kinds;
	}
	else
	{
		return {};
	}
}


bool global_context::has_errors(void) const
{
	for (auto &err : this->_errors)
	{
		if (err.is_error() || (is_warning_error(err.kind)))
		{
			return true;
		}
	}
	return false;
}

bool global_context::has_warnings(void) const
{
	for (auto &err : this->_errors)
	{
		if (err.is_warning())
		{
			return true;
		}
	}
	return false;
}

size_t global_context::get_error_count(void) const
{
	size_t count = 0;
	for (auto &err : this->_errors)
	{
		if (err.is_error())
		{
			++count;
		}
	}
	return count;
}

size_t global_context::get_warning_count(void) const
{
	size_t count = 0;
	for (auto &err : this->_errors)
	{
		if (err.is_warning())
		{
			++count;
		}
	}
	return count;
}

void global_context::add_compile_function(ast::function_body &func_body)
{
	this->_compile_decls.funcs.push_back(&func_body);
}

static std::pair<fs::path, bool> search_for_source_file(ast::identifier const &id, fs::path const &current_path)
{
	bz::u8string module_file_name;
	bool first = true;
	for (auto const value : id.values)
	{
		if (first)
		{
			first = false;
		}
		else
		{
			module_file_name += '/';
		}
		module_file_name += value;
	}
	module_file_name += ".bz";
	std::string_view std_sv(module_file_name.data_as_char_ptr(), module_file_name.size());
	if (!id.is_qualified)
	{
		auto same_dir_module = current_path / std_sv;
		if (fs::exists(same_dir_module))
		{
			return { std::move(same_dir_module), false };
		}
	}

	for (auto const &import_dir : import_dirs)
	{
		std::string_view import_dir_sv(import_dir.data_as_char_ptr(), import_dir.size());
		auto module = fs::path(import_dir_sv) / std_sv;
		if (fs::exists(module))
		{
			return { std::move(module), true };
		}
	}
	return {};
}

uint32_t global_context::add_module(uint32_t current_file_id, ast::identifier const &id)
{
	auto &current_file = this->get_src_file(current_file_id);
	auto const [file_path, is_library_file] = search_for_source_file(id, current_file.get_file_path().parent_path());
	if (file_path.empty())
	{
		this->report_error(error{
			warning_kind::_last,
			{
				id.tokens.begin->src_pos.file_id, id.tokens.begin->src_pos.line,
				id.tokens.begin->src_pos.begin, id.tokens.begin->src_pos.begin, (id.tokens.end - 1)->src_pos.end,
				suggestion_range{}, suggestion_range{},
				bz::format("unable to find module '{}'", id.as_string()),
			},
			{}, {}
		});
		return std::numeric_limits<uint32_t>::max();
	}
	auto scope = [&, is_library_file = is_library_file]() {
		if (is_library_file)
		{
			return bz::vector(id.values.begin(), id.values.end() - 1);
		}
		else
		{
			auto result = this->get_src_file(current_file_id)._scope;
			result.append(bz::array_view(id.values.begin(), id.values.end() - 1));
			return result;
		}
	}();

	auto &file = [&, &file_path = file_path, is_library_file = is_library_file]() -> auto & {
		auto const file_it = std::find_if(
			this->_src_files.begin(), this->_src_files.end(),
			[&](auto const &src_file) {
				return fs::equivalent(src_file.get_file_path(), file_path);
			}
		);
		if (file_it == this->_src_files.end())
		{
			return this->_src_files.emplace_back(file_path, this->_src_files.size(), std::move(scope), is_library_file || current_file._is_library_file);
		}
		else
		{
			return *file_it;
		}
	}();

	if (file._stage == src_file::constructed)
	{
		if (!file.parse_global_symbols(*this))
		{
			return std::numeric_limits<uint32_t>::max();
		}
	}
	else
	{
		bz_assert(file._stage == src_file::parsed_global_symbols);
	}
	return file._file_id;
}

decl_set const &global_context::get_file_export_decls(uint32_t file_id)
{
	return this->get_src_file(file_id)._export_decls;
}

bool global_context::add_comptime_checking_function(bz::u8string_view kind, ast::function_body *func_body)
{
	auto const it = std::find_if(
		comptime_function_info.begin(), comptime_function_info.end(),
		[kind](auto const &info) { return info.name == kind; }
	);
	if (it != comptime_function_info.end())
	{
		this->_comptime_executor.set_comptime_function(it->kind, func_body);
		return true;
	}
	else
	{
		return false;
	}
}

bool global_context::add_comptime_checking_variable(bz::u8string_view kind, ast::decl_variable *var_decl)
{
	if (kind == "errors_var")
	{
		this->_comptime_executor.errors_array = var_decl;
		return true;
	}
	else if (kind == "call_stack_var")
	{
		this->_comptime_executor.call_stack = var_decl;
		return true;
	}
	else
	{
		return false;
	}
}

bool global_context::add_builtin_function(bz::u8string_view kind, ast::function_body *func_body)
{
	static_assert(ast::function_body::_builtin_last - ast::function_body::_builtin_first == 115);
	if (kind == "str_eq")
	{
		if (this->_builtin_str_eq_func != nullptr)
		{
			return false;
		}
		func_body->intrinsic_kind = ast::function_body::builtin_str_eq;
		this->_builtin_str_eq_func = func_body;
		return true;
	}
	else if (kind == "str_neq")
	{
		if (this->_builtin_str_neq_func != nullptr)
		{
			return false;
		}
		func_body->intrinsic_kind = ast::function_body::builtin_str_neq;
		this->_builtin_str_neq_func = func_body;
		return true;
	}
	else if (kind == "str_length")
	{
		if (this->_builtin_str_length_func != nullptr)
		{
			return false;
		}
		func_body->intrinsic_kind = ast::function_body::builtin_str_length;
		this->_builtin_str_length_func = func_body;
		return true;
	}
	else if (kind == "str_starts_with")
	{
		if (this->_builtin_str_starts_with_func != nullptr)
		{
			return false;
		}
		func_body->intrinsic_kind = ast::function_body::builtin_str_starts_with;
		this->_builtin_str_starts_with_func = func_body;
		return true;
	}
	else if (kind == "str_ends_with")
	{
		if (this->_builtin_str_ends_with_func != nullptr)
		{
			return false;
		}
		func_body->intrinsic_kind = ast::function_body::builtin_str_ends_with;
		this->_builtin_str_ends_with_func = func_body;
		return true;
	}
	else
	{
		return false;
	}
}


void global_context::report_and_clear_errors_and_warnings(void)
{
	for (auto &err : this->_errors)
	{
		print_error_or_warning(err, *this);
	}
	this->clear_errors_and_warnings();
}

[[nodiscard]] bool global_context::parse_command_line(int argc, char const * const*argv)
{
	if (argc == 1)
	{
		ctcli::print_options_help<>("bozon", "source-file", 2, 24, 80);
		compile_until = compilation_phase::parse_command_line;
		return true;
	}

	auto const errors = ctcli::parse_command_line(argc, argv);
	for (auto const &err : errors)
	{
		this->report_error(error{
			warning_kind::_last,
			{
				command_line_file_id,
				static_cast<uint32_t>(err.flag_position),
				char_pos(), char_pos(), char_pos(),
				suggestion_range{}, suggestion_range{},
				err.message,
			},
			{}, {}
		});
	}

	auto &positional_args = ctcli::positional_arguments<ctcli::options_id_t::def>;
	if (positional_args.size() >= 2)
	{
		this->report_error("only one source file may be provided");
		return false;
	}

	if (positional_args.size() == 1)
	{
		source_file = positional_args[0];
	}

	if (!errors.empty())
	{
		return false;
	}
	else
	{
		if (ctcli::print_help_if_needed("bozon", "source-file", 2, 24, 80))
		{
			compile_until = compilation_phase::parse_command_line;
		}
		else if (display_version)
		{
			print_version_info();
			compile_until = compilation_phase::parse_command_line;
		}
		return true;
	}
}

[[nodiscard]] bool global_context::initialize_llvm(void)
{
	auto const is_native_target = target == "" || target == "native";
	auto const target_triple = is_native_target
		? llvm::Triple::normalize(llvm::sys::getDefaultTargetTriple())
		: llvm::Triple::normalize(std::string(target.data_as_char_ptr(), target.size()));
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
	this->_target = llvm::TargetRegistry::lookupTarget(target_triple, target_error);
	if (this->_target == nullptr)
	{
		constexpr std::string_view default_start = "No available targets are compatible with triple \"";
		bz::vector<source_highlight> notes;
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
			notes.emplace_back(this->make_note(std::move(message)));
		}
		if (target_error.substr(0, default_start.length()) == default_start)
		{
			this->report_error(bz::format(
				"'{}' is not an available target", target_triple.c_str()
			), std::move(notes));
		}
		else
		{
			this->report_error(target_error.c_str(), std::move(notes));
		}
		return false;
	}

	auto const triple = llvm::Triple(target_triple);
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
		this->report_warning(
			warning_kind::unknown_target,
			bz::format(
				"target '{}' has limited support right now, external function calls may not work as intended",
				target_triple.c_str()
			)
		);
	}

	auto const cpu = "generic";
	auto const features = "";

	llvm::TargetOptions options;
	auto rm = llvm::Optional<llvm::Reloc::Model>();
	this->_target_machine.reset(this->_target->createTargetMachine(target_triple, cpu, features, options, rm));
	bz_assert(this->_target_machine);
	this->_data_layout = this->_target_machine->createDataLayout();
	this->_module.setDataLayout(*this->_data_layout);
	this->_module.setTargetTriple(target_triple);

	return true;
}

[[nodiscard]] bool global_context::initialize_builtins(void)
{
	auto const pointer_size = this->_data_layout->getPointerSize();
	this->_builtin_types     = ast::make_builtin_types    (this->_builtin_type_infos, pointer_size);
	this->_builtin_functions = ast::make_builtin_functions(this->_builtin_type_infos, pointer_size);

	auto const builtins_file_path = fs::path("./bozon-stdlib/__builtins.bz");
	auto &builtins_file = this->_src_files.emplace_back(
		builtins_file_path, this->_src_files.size(), bz::vector<bz::u8string_view>{}, true
	);
	if (!builtins_file.parse_global_symbols(*this))
	{
		return false;
	}
	if (!builtins_file.parse(*this))
	{
		return false;
	}

	auto const comptime_checking_file_path = fs::path("./bozon-stdlib/__comptime_checking.bz");
	auto &comptime_checking_file = this->_src_files.emplace_back(
		comptime_checking_file_path, this->_src_files.size(), bz::vector<bz::u8string_view>{}, true
	);
	if (!comptime_checking_file.parse_global_symbols(*this))
	{
		return false;
	}

	return true;
}

[[nodiscard]] bool global_context::parse_global_symbols(void)
{
	if (source_file == "")
	{
		this->report_error("no source file was provided");
		return false;
	}
	else if (source_file != "-" && !source_file.ends_with(".bz"))
	{
		this->report_error("source file name must end in '.bz'");
		return false;
	}

	auto const source_file_path = fs::path(std::string_view(source_file.data_as_char_ptr(), source_file.size()));
	auto &file = this->_src_files.emplace_back(source_file_path, this->_src_files.size(), bz::vector<bz::u8string_view>{}, false);
	if (!file.parse_global_symbols(*this))
	{
		return false;
	}

	return true;
}

[[nodiscard]] bool global_context::parse(void)
{
	for (auto &file : this->_src_files)
	{
		if (!file.parse(*this))
		{
			return false;
		}
	}

	/*
	this->_comptime_executor.vars_.clear();
	if (is_optimization_enabled(bc::optimization_kind::aggressive_consteval))
	{
		for (auto &file : this->_src_files)
		{
			file.aggressive_consteval(*this);
		}
	}
	*/

	return true;
}

[[nodiscard]] bool global_context::emit_bitcode(void)
{
	bitcode_context context(*this, &this->_module);

	// add declarations to the module
	bz_assert(this->_compile_decls.var_decls.size() == 0);
	for (auto const &file : this->_src_files)
	{
		for (auto const type : file._global_decls.types)
		{
			bc::runtime::emit_global_type_symbol(*type, context);
		}
		for (auto const type : file._global_decls.types)
		{
			bc::runtime::emit_global_type(*type, context);
		}
		for (auto const decl : file._global_decls.var_decls)
		{
			bc::runtime::emit_global_variable(*decl, context);
		}
	}
	for (auto const func : this->_compile_decls.funcs)
	{
		if (func->is_external_linkage())
		{
			context.ensure_function_emission(func);
		}
	}

	bc::runtime::emit_necessary_functions(context);

	this->optimize();

	return true;
}

[[nodiscard]] bool global_context::emit_file(void)
{
	// only here for debug purposes, the '--emit' option does not control this
	if (debug_ir_output)
	{
		std::error_code ec;
		llvm::raw_fd_ostream file("output.ll", ec, llvm::sys::fs::OF_Text);
		if (!ec)
		{
			this->_module.print(file, nullptr);
		}
		else
		{
			bz::print(stderr, "{}unable to write output.ll{}\n", colors::bright_red, colors::clear);
		}
	}

	switch (emit_file_type)
	{
	case emit_type::obj:
		return this->emit_obj();
	case emit_type::asm_:
		return this->emit_asm();
	case emit_type::llvm_bc:
		return this->emit_llvm_bc();
	case emit_type::llvm_ir:
		return this->emit_llvm_ir();
	}
	bz_unreachable;
}

bool global_context::emit_obj(void)
{
	bz::u8string const &output_file = output_file_name != ""
		? output_file_name
		: []() {
			auto const slash_it = source_file.rfind_any("/\\");
			auto const dot = source_file.rfind('.');
			bz_assert(dot != bz::u8iterator{});
			return bz::format("{}.o", bz::u8string(
				slash_it == bz::u8iterator{} ? source_file.begin() : slash_it + 1,
				dot
			));
		}();

	if (output_file != "-" && !output_file.ends_with(".o"))
	{
		this->report_warning(
			warning_kind::bad_file_extension,
			bz::format("object output file '{}' doesn't have the file extension '.o'", output_file)
		);
	}

	// passing "-" to raw_fd_ostream should output to stdout and not create a new file
	// http://llvm.org/doxygen/classllvm_1_1raw__fd__ostream.html#af5462bc0fe5a61eccc662708da280e64
	std::error_code ec;
	llvm::raw_fd_ostream dest(
		llvm::StringRef(output_file.data_as_char_ptr(), output_file.size()),
		ec, llvm::sys::fs::OF_None
	);
	if (output_file == "-")
	{
		this->report_warning(
			warning_kind::binary_stdout,
			"outputting binary file to stdout"
		);
	}

	if (ec)
	{
		this->report_error(bz::format(
			"unable to open output file '{}', reason: '{}'",
			output_file, ec.message().c_str()
		));
		return false;
	}

	auto &module = this->_module;
	llvm::legacy::PassManager pass_manager;
	auto const target_machine = this->_target_machine.get();
	auto const res = target_machine->addPassesToEmitFile(pass_manager, dest, nullptr, llvm::CGFT_ObjectFile);
	if (res)
	{
		this->report_error("object file emission is not supported");
		return false;
	}

	pass_manager.run(module);
	return true;
}

bool global_context::emit_asm(void)
{
	bz::u8string const &output_file = output_file_name != ""
		? output_file_name
		: []() {
			auto const slash_it = source_file.rfind('/');
			auto const dot = source_file.rfind('.');
			bz_assert(dot != bz::u8iterator{});
			return bz::format("{}.s", bz::u8string(
				slash_it == bz::u8iterator{} ? source_file.begin() : slash_it + 1,
				dot
			));
		}();

	if (output_file != "-" && !output_file.ends_with(".s"))
	{
		this->report_warning(
			warning_kind::bad_file_extension,
			bz::format("assembly output file '{}' doesn't have the file extension '.s'", output_file)
		);
	}

	// passing "-" to raw_fd_ostream should output to stdout and not create a new file
	// http://llvm.org/doxygen/classllvm_1_1raw__fd__ostream.html#af5462bc0fe5a61eccc662708da280e64
	std::error_code ec;
	llvm::raw_fd_ostream dest(
		llvm::StringRef(output_file.data_as_char_ptr(), output_file.size()),
		ec, llvm::sys::fs::OF_None
	);

	if (ec)
	{
		this->report_error(bz::format(
			"unable to open output file '{}', reason: '{}'",
			output_file, ec.message().c_str()
		));
		return false;
	}

	auto &module = this->_module;
	llvm::legacy::PassManager pass_manager;
	auto const target_machine = this->_target_machine.get();
	auto const res = target_machine->addPassesToEmitFile(pass_manager, dest, nullptr, llvm::CGFT_AssemblyFile);
	if (res)
	{
		this->report_error("assembly file emission is not supported");
		return false;
	}

	pass_manager.run(module);
	dest.flush();

	return true;
}

bool global_context::emit_llvm_bc(void)
{
	auto &module = this->_module;
	bz::u8string const &output_file = output_file_name != ""
		? output_file_name
		: []() {
			auto const slash_it = source_file.rfind('/');
			auto const dot = source_file.rfind('.');
			bz_assert(dot != bz::u8iterator{});
			return bz::format("{}.bc", bz::u8string(
				slash_it == bz::u8iterator{} ? source_file.begin() : slash_it + 1,
				dot
			));
		}();

	if (output_file != "-" && !output_file.ends_with(".bc"))
	{
		this->report_warning(
			warning_kind::bad_file_extension,
			bz::format("LLVM bitcode output file '{}' doesn't have the file extension '.bc'", output_file)
		);
	}

	// passing "-" to raw_fd_ostream should output to stdout and not create a new file
	// http://llvm.org/doxygen/classllvm_1_1raw__fd__ostream.html#af5462bc0fe5a61eccc662708da280e64
	std::error_code ec;
	llvm::raw_fd_ostream dest(
		llvm::StringRef(output_file.data_as_char_ptr(), output_file.size()),
		ec, llvm::sys::fs::OF_None
	);
	if (output_file == "-")
	{
		this->report_warning(
			warning_kind::binary_stdout,
			"outputting binary file to stdout"
		);
	}

	if (ec)
	{
		this->report_error(bz::format(
			"unable to open output file '{}', reason: '{}'",
			output_file, ec.message().c_str()
		));
		return false;
	}

	llvm::WriteBitcodeToFile(module, dest);
	return true;
}

bool global_context::emit_llvm_ir(void)
{
	auto &module = this->_module;
	bz::u8string const &output_file = output_file_name != ""
		? output_file_name
		: []() {
			auto const slash_it = source_file.rfind('/');
			auto const dot = source_file.rfind('.');
			bz_assert(dot != bz::u8iterator{});
			return bz::format("{}.ll", bz::u8string(
				slash_it == bz::u8iterator{} ? source_file.begin() : slash_it + 1,
				dot
			));
		}();

	if (output_file != "-" && !output_file.ends_with(".ll"))
	{
		this->report_warning(
			warning_kind::bad_file_extension,
			bz::format("LLVM IR output file '{}' doesn't have the file extension '.ll'", output_file)
		);
	}

	// passing "-" to raw_fd_ostream should output to stdout and not create a new file
	// http://llvm.org/doxygen/classllvm_1_1raw__fd__ostream.html#af5462bc0fe5a61eccc662708da280e64
	std::error_code ec;
	llvm::raw_fd_ostream dest(
		llvm::StringRef(output_file.data_as_char_ptr(), output_file.size()),
		ec, llvm::sys::fs::OF_None
	);

	if (ec)
	{
		this->report_error(bz::format(
			"unable to open output file '{}', reason: '{}'",
			output_file, ec.message().c_str()
		));
		return false;
	}

	module.print(dest, nullptr);
	return true;
}

void global_context::optimize(void)
{
	if (!is_any_optimization_enabled())
	{
		return;
	}

	auto &module = this->_module;
	llvm::legacy::PassManager opt_pass_manager;
	// reassociation pass seems to be buggy, it thinks it modified the code even when it didn't
	llvm::legacy::PassManager reassociate_pass_manager;

#define add_opt(kind, pass)                               \
if (is_optimization_enabled(bc::optimization_kind::kind)) \
{                                                         \
    opt_pass_manager.add(pass);                           \
}

	// optimizations
	add_opt(instcombine,            llvm::createInstructionCombiningPass())
	add_opt(mem2reg,                llvm::createPromoteMemoryToRegisterPass())
	add_opt(simplifycfg,            llvm::createCFGSimplificationPass())
	add_opt(gvn,                    llvm::createGVNPass())
	add_opt(inline_,                llvm::createFunctionInliningPass())
	add_opt(sccp,                   llvm::createSCCPPass())
	// dead code elimination should come after sccp as suggested in LLVM docs
	// http://llvm.org/docs/Passes.html#passes-sccp
	add_opt(adce,                   llvm::createAggressiveDCEPass())
	else add_opt(dce,               llvm::createDeadCodeEliminationPass())
	add_opt(aggressive_instcombine, llvm::createAggressiveInstCombinerPass())

	if (is_optimization_enabled(bc::optimization_kind::reassociate))
	{
		reassociate_pass_manager.add(llvm::createReassociatePass());
	}
	if (is_optimization_enabled(bc::optimization_kind::instcombine))
	{
		// we need to add instcombine here too, as ressociate and instcombine have
		// conflicting optimizations
		// e.g. instcombine: %1 = mul i32 %0, 4    ->    %1 = shl i32 %0, 2
		//      reassociate: %1 = shl i32 %0, 2    ->    %1 = mul i32 %0, 4
		reassociate_pass_manager.add(llvm::createInstructionCombiningPass());
	}

#undef add_opt

	{
		size_t const max_iter = max_opt_iter_count;
		size_t i = 0;
		// opt_pass_manager.run returns true if any of the passes modified the code
		while (i < max_iter && opt_pass_manager.run(module))
		{
			reassociate_pass_manager.run(module);
			++i;
		}
	}
}

} // namespace ctx
