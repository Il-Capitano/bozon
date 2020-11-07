#include "global_context.h"
#include "src_manager.h"
#include "ast/statement.h"

#include <cassert>
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
	return ast::function_body{
		std::move(params),
		std::move(return_type),
		ast::function_body::body_t{},
		"",
		symbol_name,
		lex::src_tokens{},
		ast::resolve_state::symbol,
		abi::calling_convention::c,
		ast::function_body::external_linkage | ast::function_body::intrinsic,
		kind
	};
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

global_context::global_context(src_manager &manager)
	: _src_manager(manager),
	  _compile_decls{},
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

uint32_t global_context::add_file_to_compile(lex::token_pos it, bz::u8string_view file_name)
{
	auto &file = this->_src_manager.add_file(file_name);
	bz_assert(file._file_name == file_name);
	if (file._stage == src_file::constructed)
	{
		if (!file.parse_global_symbols())
		{
			if (file._stage == src_file::constructed)
			{
				this->report_error(error{
					warning_kind::_last,
					it->src_pos.file_id, it->src_pos.line,
					it->src_pos.begin, it->src_pos.begin, it->src_pos.end,
					bz::format("unable to find module '{}'", it->value),
					{}, {}
				});
			}
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

#define add_builtin(pos, kind, symbol_name, ...) \
((void)([]() { static_assert(kind == pos); }), create_builtin_function(kind, symbol_name, __VA_ARGS__))
	static std::array<
		ast::function_body,
		ast::function_body::_builtin_last - ast::function_body::_builtin_first
	> builtin_functions = {
		add_builtin(0, ast::function_body::builtin_str_eq,  "__bozon_builtin_str_eq",  bool_type, str_type, str_type),
		add_builtin(1, ast::function_body::builtin_str_neq, "__bozon_builtin_str_neq", bool_type, str_type, str_type),
		add_builtin(2, ast::function_body::builtin_str_begin_ptr, "", uint8_const_ptr_type, str_type),
		add_builtin(3, ast::function_body::builtin_str_end_ptr,   "", uint8_const_ptr_type, str_type),
		add_builtin(4, ast::function_body::builtin_str_from_ptrs, "", str_type, uint8_const_ptr_type, uint8_const_ptr_type),
	};
#undef add_builtin

	bz_assert(kind < builtin_functions.size() && builtin_functions[kind].intrinsic_kind == kind);
	return &builtin_functions[kind];
}

} // namespace ctx
