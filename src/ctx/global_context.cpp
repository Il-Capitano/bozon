#include "global_context.h"
#include "command_parse_context.h"
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

template<typename ...Ts>
static ast::function_body create_builtin_function(
	uint32_t kind,
	bz::u8string_view symbol_name,
	ast::typespec return_type,
	Ts ...arg_types
)
{
	static_assert((bz::meta::is_same<Ts, ast::typespec> && ...));
	bz::vector<ast::decl_variable> params;
	params.reserve(sizeof... (Ts));
	((params.emplace_back(
		lex::token_range{}, lex::token_pos{}, lex::token_range{},
		std::move(arg_types)
	)), ...);
	auto const is_generic = [&]() {
		for (auto const &param : params)
		{
			if (!ast::is_complete(param.var_type))
			{
				return true;
			}
		}
		return false;
	}();
	ast::function_body result;
	result.params = std::move(params);
	result.return_type = std::move(return_type);
	result.symbol_name = symbol_name;
	result.state = ast::resolve_state::symbol;
	result.cc = abi::calling_convention::c;
	result.flags =
		ast::function_body::external_linkage
		| ast::function_body::intrinsic
		| (is_generic ? ast::function_body::generic : 0);
	result.intrinsic_kind = kind;
	return result;
}

static_assert(ast::type_info::int8_    ==  0);
static_assert(ast::type_info::int16_   ==  1);
static_assert(ast::type_info::int32_   ==  2);
static_assert(ast::type_info::int64_   ==  3);
static_assert(ast::type_info::uint8_   ==  4);
static_assert(ast::type_info::uint16_  ==  5);
static_assert(ast::type_info::uint32_  ==  6);
static_assert(ast::type_info::uint64_  ==  7);
static_assert(ast::type_info::float32_ ==  8);
static_assert(ast::type_info::float64_ ==  9);
static_assert(ast::type_info::char_    == 10);
static_assert(ast::type_info::str_     == 11);
static_assert(ast::type_info::bool_    == 12);
static_assert(ast::type_info::null_t_  == 13);


static auto get_default_type_infos(void)
{
	return bz::array{
		ast::type_info::make_built_in("int8",     ast::type_info::int8_),
		ast::type_info::make_built_in("int16",    ast::type_info::int16_),
		ast::type_info::make_built_in("int32",    ast::type_info::int32_),
		ast::type_info::make_built_in("int64",    ast::type_info::int64_),
		ast::type_info::make_built_in("uint8",    ast::type_info::uint8_),
		ast::type_info::make_built_in("uint16",   ast::type_info::uint16_),
		ast::type_info::make_built_in("uint32",   ast::type_info::uint32_),
		ast::type_info::make_built_in("uint64",   ast::type_info::uint64_),
		ast::type_info::make_built_in("float32",  ast::type_info::float32_),
		ast::type_info::make_built_in("float64",  ast::type_info::float64_),
		ast::type_info::make_built_in("char",     ast::type_info::char_),
		ast::type_info::make_built_in("str",      ast::type_info::str_),
		ast::type_info::make_built_in("bool",     ast::type_info::bool_),
		ast::type_info::make_built_in("__null_t", ast::type_info::null_t_),
	};
}

static auto default_type_infos = get_default_type_infos();

static bz::vector<typespec_with_name> get_default_types(void)
{
	bz::vector<typespec_with_name> result;
	result.reserve(ast::type_info::null_t_ + 1);
	for (size_t i = 0; i <= ast::type_info::null_t_; ++i)
	{
		auto &type_info = default_type_infos[i];
		auto const name = ast::type_info::decode_symbol_name(type_info.symbol_name);
		result.emplace_back(name, ast::make_base_type_typespec({}, &type_info));
	}
	return result;
}

static bz::array<llvm::Type *, static_cast<int>(ast::type_info::null_t_) + 1>
get_llvm_built_in_types(llvm::LLVMContext &context)
{
	auto const i8_ptr = llvm::Type::getInt8PtrTy(context);
	auto const str_t = llvm::StructType::create("built_in.str", i8_ptr, i8_ptr);
	auto const null_t = llvm::StructType::create(context, {}, "built_in.__null_t");
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
		get_default_types() // types
	};
}

global_context::global_context(void)
	: _compile_decls{},
	  _errors{},
	  _llvm_context(),
	  _module("test", this->_llvm_context),
	  _comptime_module("comptime_module", this->_llvm_context),
	  _target_machine(nullptr),
	  _data_layout(),
	  _llvm_built_in_types(get_llvm_built_in_types(this->_llvm_context))
{}


bool global_context::has_errors(void) const
{
	for (auto &err : this->_errors)
	{
		if (err.is_error())
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

static fs::path search_for_source_file(bz::u8string_view module_name, fs::path const &current_path)
{
	std::string_view std_sv(module_name.data(), module_name.size());
	auto same_dir_module = current_path / std_sv;
	same_dir_module += ".bz";
	if (fs::exists(same_dir_module))
	{
		return same_dir_module;
	}

	for (auto const &import_dir : import_dirs)
	{
		std::string_view import_dir_sv(import_dir.data_as_char_ptr(), import_dir.size());
		auto module = fs::path(import_dir_sv) / std_sv;
		module += ".bz";
		if (fs::exists(module))
		{
			return module;
		}
	}
	return {};
}

uint32_t global_context::add_module(lex::token_pos it, uint32_t current_file_id, bz::u8string_view file_name)
{
	auto const file_path = search_for_source_file(file_name, this->get_src_file(current_file_id).get_file_path().parent_path());
	if (file_path.empty())
	{
		this->report_error(error{
			warning_kind::_last,
			it->src_pos.file_id, it->src_pos.line,
			it->src_pos.begin, it->src_pos.begin, it->src_pos.end,
			bz::format("unable to find module '{}'", it->value),
			{}, {}
		});
		return std::numeric_limits<uint32_t>::max();
	}

	auto &file = [&]() -> auto & {
		auto const file_it = std::find_if(
			this->_src_files.begin(), this->_src_files.end(),
			[&](auto const &src_file) {
				return fs::equivalent(src_file.get_file_path(), file_path);
			}
		);
		if (file_it == this->_src_files.end())
		{
			return this->_src_files.emplace_back(file_path, this->_src_files.size(), *this);
		}
		else
		{
			return *file_it;
		}
	}();

	if (file._stage == src_file::constructed)
	{
		if (!file.parse_global_symbols())
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

ctx::decl_set const &global_context::get_file_export_decls(uint32_t file_id)
{
	return this->get_src_file(file_id)._export_decls;
}

ast::type_info *global_context::get_base_type_info(uint32_t kind) const
{
	bz_assert(kind <= ast::type_info::null_t_);
	return &default_type_infos[kind];
}

ast::function_body *global_context::get_builtin_function(uint32_t kind) const
{
	auto const bool_type = ast::make_base_type_typespec({}, this->get_base_type_info(ast::type_info::bool_));
	auto const str_type  = ast::make_base_type_typespec({}, this->get_base_type_info(ast::type_info::str_));
	auto const uint8_const_ptr_type = [&]() {
		ast::typespec result = ast::make_base_type_typespec({}, this->get_base_type_info(ast::type_info::uint8_));
		result.add_layer<ast::ts_const>(nullptr);
		result.add_layer<ast::ts_pointer>(nullptr);
		return result;
	}();
	auto const auto_ptr_type = [&]() {
		ast::typespec result = ast::make_auto_typespec({});
		result.add_layer<ast::ts_pointer>(nullptr);
		return result;
	}();
	auto const auto_const_ptr_type = [&]() {
		ast::typespec result = ast::make_auto_typespec({});
		result.add_layer<ast::ts_const>(nullptr);
		result.add_layer<ast::ts_pointer>(nullptr);
		return result;
	}();

#define add_builtin(pos, kind, symbol_name, ...) \
((void)([]() { static_assert(kind == pos); }), create_builtin_function(kind, symbol_name, __VA_ARGS__))
	static bz::array<
		ast::function_body,
		ast::function_body::_builtin_last - ast::function_body::_builtin_first
	> builtin_functions = {
		add_builtin(0, ast::function_body::builtin_str_eq,  "__bozon_builtin_str_eq",  bool_type, str_type, str_type),
		add_builtin(1, ast::function_body::builtin_str_neq, "__bozon_builtin_str_neq", bool_type, str_type, str_type),
		add_builtin(2, ast::function_body::builtin_str_begin_ptr,         "", uint8_const_ptr_type, str_type),
		add_builtin(3, ast::function_body::builtin_str_end_ptr,           "", uint8_const_ptr_type, str_type),
		add_builtin(4, ast::function_body::builtin_str_from_ptrs,         "", str_type, uint8_const_ptr_type, uint8_const_ptr_type),
		add_builtin(5, ast::function_body::builtin_slice_from_ptrs,       "", {}, auto_ptr_type, auto_ptr_type),
		add_builtin(6, ast::function_body::builtin_slice_from_const_ptrs, "", {}, auto_const_ptr_type, auto_const_ptr_type),
	};
#undef add_builtin

	bz_assert(kind < builtin_functions.size() && builtin_functions[kind].intrinsic_kind == kind);
	return &builtin_functions[kind];
}


void global_context::report_and_clear_errors_and_warnings(void)
{
	for (auto &err : this->_errors)
	{
		print_error_or_warning(err, *this);
	}
	this->clear_errors_and_warnings();
}

[[nodiscard]] bool global_context::parse_command_line(int argc, char const **argv)
{
	auto const args = cl::get_args(argc, argv);
	if (args.size() == 1)
	{
		print_help();
		compile_until = compilation_phase::parse_command_line;
		return true;
	}

	command_parse_context context(args, *this);
	::parse_command_line(context);

	auto const good = !this->has_errors();

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
		? llvm::sys::getDefaultTargetTriple()
		: std::string(target.data_as_char_ptr(), target.size());
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
	auto const target = llvm::TargetRegistry::lookupTarget(target_triple, target_error);
	if (target == nullptr)
	{
		constexpr std::string_view default_start = "No available targets are compatible with triple \"";
		bz::vector<ctx::note> notes;
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
	this->_target_machine.reset(target->createTargetMachine(target_triple, cpu, features, options, rm));
	bz_assert(this->_target_machine);
	this->_data_layout = this->_target_machine->createDataLayout();
	this->_module.setDataLayout(*this->_data_layout);
	this->_module.setTargetTriple(target_triple);

	return true;
}

[[nodiscard]] bool global_context::parse_global_symbols(void)
{
	if (source_file == "")
	{
		this->report_error("no source file was provided");
		return false;
	}

	auto const source_file_path = fs::path(std::string_view(source_file.data_as_char_ptr(), source_file.size()));
	auto &file = this->_src_files.emplace_back(source_file_path, this->_src_files.size(), *this);
	if (!file.parse_global_symbols())
	{
		return false;
	}

	return true;
}

[[nodiscard]] bool global_context::parse(void)
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

[[nodiscard]] bool global_context::emit_bitcode(void)
{
	bitcode_context context(*this, this->_module);

	// add declarations to the module
	bz_assert(this->_compile_decls.var_decls.size() == 0);
	bc::runtime::add_builtin_functions(context);
	for (auto const func : this->_compile_decls.funcs)
	{
		func->resolve_symbol_name();
		bc::runtime::add_function_to_module(func, context);
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
			bz::print("{}unable to write output.ll{}\n", colors::bright_red, colors::clear);
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
			auto const slash_it = source_file.rfind('/');
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
