#include "global_context.h"
#include "src_manager.h"

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


auto get_default_type_infos(void)
{
	return std::array{
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

static std::array<llvm::Type *, static_cast<int>(ast::type_info::null_t_) + 1>
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

global_context::global_context(src_manager &_src_manager)
	: _src_manager(_src_manager),
	  _compile_decls{},
	  _errors{},
	  _llvm_context(),
	  _module("test", this->_llvm_context),
	  _target_machine(nullptr),
	  _data_layout(),
	  _llvm_built_in_types(get_llvm_built_in_types(this->_llvm_context))
{
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
	this->_target_machine = target->createTargetMachine(target_triple, cpu, features, options, rm);
	bz_assert(this->_target_machine);
	this->_data_layout = this->_target_machine->createDataLayout();
	this->_module.setDataLayout(*this->_data_layout);
	this->_module.setTargetTriple(target_triple);

	auto const triple = llvm::Triple(target_triple);
	auto const os = triple.getOS();
	auto const arch = triple.getArch();

	if (os == llvm::Triple::Win32 && arch == llvm::Triple::x86_64)
	{
		this->_platform_abi = abi::platform_abi::microsoft_x64;
	}
	else
	{
		this->_platform_abi = abi::platform_abi::generic;
	}
}


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

uint32_t global_context::add_file_to_compile(lex::token_pos it, bz::u8string_view file_name)
{
	auto &file = this->_src_manager.add_file(file_name);
	bz_assert(file._file_name == file_name);
	if (file._stage == src_file::constructed)
	{
		if (!file.read_file())
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
		if (!file.tokenize())
		{
			return std::numeric_limits<uint32_t>::max();
		}
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
	return this->src_files[file_id]->_export_decls;
}

ast::type_info *global_context::get_base_type_info(uint32_t kind) const
{
	bz_assert(kind <= ast::type_info::null_t_);
	return &default_type_infos[kind];
}

} // namespace ctx
