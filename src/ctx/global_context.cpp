#include "global_context.h"
#include "parser.h"

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
	result.reserve(ast::type_info::null_t_);
	for (size_t i = 0; i < ast::type_info::null_t_; ++i)
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
	auto const null_t = llvm::StructType::create(context, {}, "built_in.null_t");
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

global_context::global_context(void)
	: _export_decls{
		  {}, // var_decls
		  {}, // func_sets
		  {}, // op_sets
		  get_default_types() // type_infos
	  },
	  _compile_decls{},
	  _errors{},
	  _llvm_context(),
	  _module("test", this->_llvm_context),
	  _target_machine(nullptr),
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
	this->_module.setDataLayout(this->_target_machine->createDataLayout());
	this->_module.setTargetTriple(target_triple);
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

void global_context::add_export_declaration(ast::statement &decl)
{
	switch (decl.kind())
	{
	case ast::statement::index<ast::decl_variable>:
		this->add_export_variable(*decl.get<ast::decl_variable_ptr>());
		break;
	case ast::statement::index<ast::decl_function>:
		this->add_export_function(*decl.get<ast::decl_function_ptr>());
		break;
	case ast::statement::index<ast::decl_operator>:
		this->add_export_operator(*decl.get<ast::decl_operator_ptr>());
		break;
	case ast::statement::index<ast::decl_struct>:
		bz_assert(false);
		break;
	case ast::statement::index<ast::stmt_static_assert>:
		break;
	default:
		bz_assert(false);
		break;
	}
}

void global_context::add_export_variable(ast::decl_variable &var_decl)
{
	this->_export_decls.var_decls.push_back(&var_decl);
	this->_compile_decls.var_decls.push_back(&var_decl);
}

void global_context::add_compile_variable(ast::decl_variable &var_decl)
{
	this->_compile_decls.var_decls.push_back(&var_decl);
}

void global_context::add_export_function(ast::decl_function &func_decl)
{
	auto &func_sets = this->_export_decls.func_sets;
	auto set = std::find_if(
		func_sets.begin(), func_sets.end(),
		[id = func_decl.identifier->value](auto const &overload_set) {
			return id == overload_set.id;
		}
	);
	if (set == func_sets.end())
	{
		func_sets.push_back({ func_decl.identifier->value, { &func_decl } });
	}
	else
	{
		// we don't check for conflicts just yet
		// these should be added after the first pass parsing stage
		// and resolved after, where redeclaration checks are made
		set->func_decls.push_back(&func_decl);
	}

	this->_compile_decls.func_decls.push_back(&func_decl);
}

void global_context::add_compile_function(ast::decl_function &func_decl)
{
	this->_compile_decls.func_decls.push_back(&func_decl);
}

void global_context::add_export_operator(ast::decl_operator &op_decl)
{
	auto &op_sets = this->_export_decls.op_sets;
	auto set = std::find_if(
		op_sets.begin(), op_sets.end(),
		[op = op_decl.op->kind](auto const &overload_set) {
			return op == overload_set.op;
		}
	);
	if (set == op_sets.end())
	{
		op_sets.push_back({ op_decl.op->kind, { &op_decl } });
	}
	else
	{
		// we don't check for conflicts just yet
		// these should be added after the first pass parsing stage
		// and resolved after, where redeclaration checks are made
		set->op_decls.push_back(&op_decl);
	}

	this->_compile_decls.op_decls.push_back(&op_decl);
}

void global_context::add_compile_operator(ast::decl_operator &op_decl)
{
	this->_compile_decls.op_decls.push_back(&op_decl);
}

/*
void global_context::add_export_struct(ast::decl_struct &struct_decl)
{
	this->_export_decls.types.push_back({ struct_decl.identifier->value, &struct_decl.info });
	this->_compile_decls.types.push_back(&struct_decl.info);
}

void global_context::add_compile_struct(ast::decl_struct &struct_decl)
{
	this->_compile_decls.type_infos.push_back(&struct_decl.info);
}
*/

ast::type_info *global_context::get_base_type_info(uint32_t kind) const
{
	bz_assert(kind <= ast::type_info::null_t_);
	return &default_type_infos[kind];
}

/*
static int get_type_match_level(
	ast::typespec const &dest,
	ast::expression::expr_type_t const &src
)
{
	int result = 0;

	auto dest_it = &dest;
	auto src_it = &src.expr_type;

	auto const advance = [](ast::typespec const *&ts)
	{
		switch (ts->kind())
		{
		case ast::typespec::index<ast::ts_reference>:
			ts = &ts->get<ast::ts_reference_ptr>()->base;
			break;
		case ast::typespec::index<ast::ts_constant>:
			ts = &ts->get<ast::ts_constant_ptr>()->base;
			break;
		case ast::typespec::index<ast::ts_pointer>:
			ts = &ts->get<ast::ts_pointer_ptr>()->base;
			break;
		default:
			bz_assert(false);
			break;
		}
	};

	if (dest_it->kind() != ast::typespec::index<ast::ts_reference>)
	{
		++result;
		if (dest_it->kind() == ast::typespec::index<ast::ts_constant>)
		{
			advance(dest_it);
		}
		if (src_it->kind() == ast::typespec::index<ast::ts_constant>)
		{
			advance(src_it);
		}

		if (dest_it->kind() == ast::typespec::index<ast::ts_base_type>)
		{
			// TODO: use is_convertible
			if (
				src_it->kind() != ast::typespec::index<ast::ts_base_type>
				|| src_it->get<ast::ts_base_type_ptr>()->info != dest_it->get<ast::ts_base_type_ptr>()->info
			)
			{
				return -1;
			}
			else
			{
				return result;
			}
		}
	}
	// TODO: rvalue references...
	else
	{
		if (
			src.type_kind != ast::expression::lvalue
			&& src.type_kind != ast::expression::lvalue_reference
		)
		{
			return -1;
		}
		advance(dest_it);
	}

	while (true)
	{
		if (dest_it->kind() == ast::typespec::index<ast::ts_base_type>)
		{
			if (
				src_it->kind() != ast::typespec::index<ast::ts_base_type>
				|| src_it->get<ast::ts_base_type_ptr>()->info != dest_it->get<ast::ts_base_type_ptr>()->info
			)
			{
				return -1;
			}
			else
			{
				return result;
			}
		}
		else if (dest_it->kind() == src_it->kind())
		{
			advance(dest_it);
			advance(src_it);
		}
		else if (dest_it->kind() == ast::typespec::index<ast::ts_constant>)
		{
			advance(dest_it);
			++result;
		}
		else
		{
			return -1;
		}
	}
}
*/

} // namespace ctx
