#include "comptime_executor.h"
#include "global_context.h"
#include "parse_context.h"
#include "bc/comptime/comptime_emit_bitcode.h"
#include "parse/statement_parser.h"
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>

namespace ctx
{

comptime_executor_context::comptime_executor_context(parse_context &_parse_ctx)
	: parse_ctx(_parse_ctx),
	  current_module(nullptr),
	  vars_{},
	  types_{},
	  functions_to_compile{},
	  current_function{ nullptr, nullptr },
	  alloca_bb(nullptr),
	  output_pointer(nullptr),
	  builder(_parse_ctx.global_ctx._llvm_context),
	  error_strings{},
	  error_string_ptr(nullptr),
	  error_token_begin(nullptr),
	  error_token_pivot(nullptr),
	  error_token_end(nullptr),
	  error_string_ptr_getter(nullptr),
	  error_token_begin_getter(nullptr),
	  error_token_pivot_getter(nullptr),
	  error_token_end_getter(nullptr),
	  engine(nullptr)
{}

ast::type_info *comptime_executor_context::get_builtin_type_info(uint32_t kind)
{
	return this->parse_ctx.global_ctx.get_builtin_type_info(kind);
}

ast::typespec_view comptime_executor_context::get_builtin_type(bz::u8string_view name)
{
	return this->parse_ctx.global_ctx.get_builtin_type(name);
}

ast::function_body *comptime_executor_context::get_builtin_function(uint32_t kind)
{
	return this->parse_ctx.global_ctx.get_builtin_function(kind);
}

llvm::Value *comptime_executor_context::get_variable(ast::decl_variable const *var_decl) const
{
	auto const it = this->vars_.find(var_decl);
	return it == this->vars_.end() ? nullptr : it->second;
}

void comptime_executor_context::add_variable(ast::decl_variable const *var_decl, llvm::Value *val)
{
	this->vars_.insert_or_assign(var_decl, val);
}

llvm::Type *comptime_executor_context::get_base_type(ast::type_info *info)
{
	if (info->state != ast::resolve_state::all)
	{
		this->parse_ctx.add_to_resolve_queue({}, *info);
		parse::resolve_type_info(*info, this->parse_ctx);
		this->parse_ctx.pop_resolve_queue();
	}
	auto const it = this->types_.find(info);
	if (it == this->types_.end())
	{
		auto const name = llvm::StringRef(info->symbol_name.data_as_char_ptr(), info->symbol_name.size());
		auto const type = llvm::StructType::create(this->get_llvm_context(), name);
		this->add_base_type(info, type);
		bc::comptime::resolve_global_type(info, type, *this);
		return type;
	}
	else
	{
		return it->second;
	}
}

void comptime_executor_context::add_base_type(ast::type_info const *info, llvm::Type *type)
{
	this->types_.insert_or_assign(info, type);
}

llvm::Function *comptime_executor_context::get_function(ast::function_body *func_body)
{
	this->ensure_function_emission(func_body);
	auto it = this->funcs_.find(func_body);
	if (it == this->funcs_.end())
	{
		auto const result = bc::comptime::add_function_to_module(func_body, *this);
		return result;
	}
	else
	{
		return it->second;
	}
}

llvm::LLVMContext &comptime_executor_context::get_llvm_context(void) const noexcept
{
	return this->parse_ctx.global_ctx._llvm_context;
}

llvm::DataLayout &comptime_executor_context::get_data_layout(void) const noexcept
{
	return this->parse_ctx.global_ctx._data_layout.get();
}

llvm::Module &comptime_executor_context::get_module(void) const noexcept
{
	return *this->current_module;
}

abi::platform_abi comptime_executor_context::get_platform_abi(void) const noexcept
{
	return this->parse_ctx.global_ctx._platform_abi;
}

size_t comptime_executor_context::get_size(llvm::Type *t) const
{
	return this->parse_ctx.global_ctx._data_layout->getTypeAllocSize(t);
}

size_t comptime_executor_context::get_align(llvm::Type *t) const
{
	return this->parse_ctx.global_ctx._data_layout->getPrefTypeAlign(t).value();
#if LLVM_VERSION_MAJOR < 11
#error LLVM 11 is required
#endif // llvm < 11
}

size_t comptime_executor_context::get_offset(llvm::Type *t, size_t elem) const
{
	bz_assert(t->isStructTy());
	return this->parse_ctx.global_ctx._data_layout->getStructLayout(static_cast<llvm::StructType *>(t))->getElementOffset(elem);
}


size_t comptime_executor_context::get_register_size(void) const
{
	switch (this->parse_ctx.global_ctx._platform_abi)
	{
	case abi::platform_abi::generic:
	{
		static size_t register_size = this->parse_ctx.global_ctx._data_layout->getLargestLegalIntTypeSizeInBits() / 8;
		return register_size;
	}
	case abi::platform_abi::microsoft_x64:
		bz_assert(this->parse_ctx.global_ctx._data_layout->getLargestLegalIntTypeSizeInBits() == 64);
		return 8;
	case abi::platform_abi::systemv_amd64:
		bz_assert(this->parse_ctx.global_ctx._data_layout->getLargestLegalIntTypeSizeInBits() == 64);
		return 8;
	}
	bz_unreachable;
}

llvm::BasicBlock *comptime_executor_context::add_basic_block(bz::u8string_view name)
{
	return llvm::BasicBlock::Create(
		this->parse_ctx.global_ctx._llvm_context,
		llvm::StringRef(name.data(), name.length()),
		this->current_function.second
	);
}

llvm::Value *comptime_executor_context::create_alloca(llvm::Type *t)
{
	auto const bb = this->builder.GetInsertBlock();
	this->builder.SetInsertPoint(this->alloca_bb);
	auto const result = this->builder.CreateAlloca(t);
	this->builder.SetInsertPoint(bb);
	return result;
}

llvm::Value *comptime_executor_context::create_alloca(llvm::Type *t, size_t align)
{
	auto const bb = this->builder.GetInsertBlock();
	this->builder.SetInsertPoint(this->alloca_bb);
	auto const result = this->builder.CreateAlloca(t);
	result->setAlignment(llvm::Align(align));
	this->builder.SetInsertPoint(bb);
	return result;
}

llvm::Value *comptime_executor_context::create_string(bz::u8string_view str)
{
	auto const str_ref = llvm::StringRef(str.data(), str.size());
	return this->builder.CreateGlobalString(str_ref, ".str", 0, &this->get_module());
}

llvm::Value *comptime_executor_context::create_bitcast(bc::val_ptr val, llvm::Type *dest_type)
{
	if (val.kind == bc::val_ptr::reference)
	{
		auto const dest_ptr = this->builder.CreateBitCast(
			val.val, llvm::PointerType::get(dest_type, 0)
		);
		return this->builder.CreateLoad(dest_ptr);
	}
	else
	{
		auto const src_value = val.get_value(this->builder);
		auto const dest_ptr = this->create_alloca(dest_type);
		auto const cast_ptr = this->builder.CreateBitCast(
			dest_ptr, llvm::PointerType::get(val.get_type(), 0)
		);
		this->builder.CreateStore(src_value, cast_ptr);
		return this->builder.CreateLoad(dest_ptr);
	}
}

llvm::Value *comptime_executor_context::create_cast_to_int(bc::val_ptr val)
{
	auto const dest_type = [&]() -> llvm::Type * {
		auto const val_t = val.get_type();
		switch (this->get_size(val_t))
		{
		case 1:
			return this->get_int8_t();
		case 2:
			return this->get_int16_t();
		case 3:
			return llvm::IntegerType::getIntNTy(this->get_llvm_context(), 24);
		case 4:
			return this->get_int32_t();
		case 5:
			return llvm::IntegerType::getIntNTy(this->get_llvm_context(), 40);
		case 6:
			return llvm::IntegerType::getIntNTy(this->get_llvm_context(), 48);
		case 7:
			return llvm::IntegerType::getIntNTy(this->get_llvm_context(), 56);
		case 8:
			return this->get_int64_t();
		default:
			bz_unreachable;
		}
	}();
	return this->create_bitcast(val, dest_type);
}

llvm::Type *comptime_executor_context::get_builtin_type(uint32_t kind) const
{
	bz_assert(kind <= ast::type_info::null_t_);
	return this->parse_ctx.global_ctx._llvm_builtin_types[kind];
}

llvm::Type *comptime_executor_context::get_int8_t(void) const
{ return this->parse_ctx.global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::int8_)]; }

llvm::Type *comptime_executor_context::get_int16_t(void) const
{ return this->parse_ctx.global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::int16_)]; }

llvm::Type *comptime_executor_context::get_int32_t(void) const
{ return this->parse_ctx.global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::int32_)]; }

llvm::Type *comptime_executor_context::get_int64_t(void) const
{ return this->parse_ctx.global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::int64_)]; }

llvm::Type *comptime_executor_context::get_uint8_t(void) const
{ return this->parse_ctx.global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::uint8_)]; }

llvm::Type *comptime_executor_context::get_uint16_t(void) const
{ return this->parse_ctx.global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::uint16_)]; }

llvm::Type *comptime_executor_context::get_uint32_t(void) const
{ return this->parse_ctx.global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::uint32_)]; }

llvm::Type *comptime_executor_context::get_uint64_t(void) const
{ return this->parse_ctx.global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::uint64_)]; }

llvm::Type *comptime_executor_context::get_float32_t(void) const
{ return this->parse_ctx.global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::float32_)]; }

llvm::Type *comptime_executor_context::get_float64_t(void) const
{ return this->parse_ctx.global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::float64_)]; }

llvm::Type *comptime_executor_context::get_str_t(void) const
{ return this->parse_ctx.global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::str_)]; }

llvm::Type *comptime_executor_context::get_char_t(void) const
{ return this->parse_ctx.global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::char_)]; }

llvm::Type *comptime_executor_context::get_bool_t(void) const
{ return this->parse_ctx.global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::bool_)]; }

llvm::Type *comptime_executor_context::get_null_t(void) const
{ return this->parse_ctx.global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::null_t_)]; }

llvm::StructType *comptime_executor_context::get_slice_t(llvm::Type *elem_type) const
{
	auto const elem_ptr_type = llvm::PointerType::get(elem_type, 0);
	return llvm::StructType::get(elem_ptr_type, elem_ptr_type);
}


bool comptime_executor_context::has_terminator(void) const
{
	auto const current_bb = this->builder.GetInsertBlock();
	return current_bb->size() != 0 && current_bb->back().isTerminator();
}

bool comptime_executor_context::has_terminator(llvm::BasicBlock *bb)
{
	return bb->size() != 0 && bb->back().isTerminator();
}

void comptime_executor_context::ensure_function_emission(ast::function_body *func)
{
	// no need to emit functions with no definition
	if (func->body.is_null())
	{
		return;
	}

	if (!this->functions_to_compile.contains(func))
	{
		this->functions_to_compile.push_back(func);
	}
}

std::pair<ast::constant_value, bz::vector<error>> comptime_executor_context::execute_function(
	lex::src_tokens src_tokens,
	ast::function_body *body,
	bz::array_view<ast::constant_value const> params
)
{
	auto const original_module = this->current_module;
	auto module = std::make_unique<llvm::Module>("comptime_module", this->get_llvm_context());
	this->current_module = module.get();
	this->ensure_function_emission(body);
	bc::comptime::emit_necessary_functions(*this);

	std::pair<ast::constant_value, bz::vector<error>> result;
	if (body->state == ast::resolve_state::error)
	{
		this->current_module = original_module;
		return result;
	}
	else if (body->state != ast::resolve_state::all)
	{
		result.second.push_back(this->make_error(
			src_tokens,
			bz::format("cannot call external function '{}' in a constant expression", body->get_signature())
		));
		this->current_module = original_module;
		return result;
	}
	else
	{
		auto const fn = bc::comptime::create_function_for_comptime_execution(body, params, *this);
		this->add_module(std::move(module));
		this->engine->runFunction(
			fn,
			{
				llvm::GenericValue(&result.first),
				llvm::GenericValue(&result.second),
				llvm::GenericValue(&this->parse_ctx)
			}
		);
		this->current_module = original_module;
		return result;
	}
}

std::unique_ptr<llvm::ExecutionEngine> comptime_executor_context::create_engine(std::unique_ptr<llvm::Module> module)
{
	std::string err;
	llvm::EngineBuilder builder(std::move(module));
	builder
		.setErrorStr(&err)
		.setOptLevel(llvm::CodeGenOpt::None);
	auto const is_native_target = target == "" || target == "native";
	auto const target_triple = is_native_target
		? llvm::sys::getDefaultTargetTriple()
		: std::string(target.data_as_char_ptr(), target.size());
	auto const cpu = "generic";
	auto const features = "";
	llvm::TargetOptions options;
	auto rm = llvm::Optional<llvm::Reloc::Model>();
	auto const target_machine = this->parse_ctx.global_ctx._target->createTargetMachine(
		target_triple,
		cpu, features, options, rm
	);
	auto const result = builder.create(std::move(target_machine));
	if (result == nullptr)
	{
		bz::log("execution engine creation failed: {}\n", err.c_str());
		exit(1);
	}
	return std::unique_ptr<llvm::ExecutionEngine>(result);
}

void comptime_executor_context::add_base_functions_to_module(llvm::Module &module)
{
	static_assert(sizeof (void *) == sizeof (uint64_t));
	auto const intptr_type = llvm::Type::getInt64Ty(this->get_llvm_context());
	this->error_string_ptr  = module.getOrInsertGlobal("__error_string_ptr",  intptr_type);
	this->error_token_begin = module.getOrInsertGlobal("__error_token_begin", intptr_type);
	this->error_token_pivot = module.getOrInsertGlobal("__error_token_pivot", intptr_type);
	this->error_token_end   = module.getOrInsertGlobal("__error_token_end",   intptr_type);

	auto const getter_func_t = llvm::FunctionType::get(intptr_type, false);

	{ // error_string_ptr_getter function
		this->error_string_ptr_getter = llvm::Function::Create(
			getter_func_t, llvm::Function::InternalLinkage,
			"__error_string_ptr_getter"
		);
		auto const bb = llvm::BasicBlock::Create(this->get_llvm_context(), "entry", this->error_string_ptr_getter);
		this->builder.SetInsertPoint(bb);
		auto const ptr_val = this->builder.CreateLoad(this->error_string_ptr);
		this->builder.CreateRet(ptr_val);
	}

	{ // error_token_begin_getter function
		this->error_token_begin_getter = llvm::Function::Create(
			getter_func_t, llvm::Function::InternalLinkage,
			"__error_token_begin_getter"
		);
		auto const bb = llvm::BasicBlock::Create(this->get_llvm_context(), "entry", this->error_token_begin_getter);
		this->builder.SetInsertPoint(bb);
		auto const ptr_val = this->builder.CreateLoad(this->error_token_begin);
		this->builder.CreateRet(ptr_val);
	}

	{ // error_token_pivot_getter function
		this->error_token_pivot_getter = llvm::Function::Create(
			getter_func_t, llvm::Function::InternalLinkage,
			"__error_token_pivot_getter"
		);
		auto const bb = llvm::BasicBlock::Create(this->get_llvm_context(), "entry", this->error_token_pivot_getter);
		this->builder.SetInsertPoint(bb);
		auto const ptr_val = this->builder.CreateLoad(this->error_token_pivot);
		this->builder.CreateRet(ptr_val);
	}

	{ // error_token_end_getter function
		this->error_token_end_getter = llvm::Function::Create(
			getter_func_t, llvm::Function::InternalLinkage,
			"__error_token_end_getter"
		);
		auto const bb = llvm::BasicBlock::Create(this->get_llvm_context(), "entry", this->error_token_end_getter);
		this->builder.SetInsertPoint(bb);
		auto const ptr_val = this->builder.CreateLoad(this->error_token_end);
		this->builder.CreateRet(ptr_val);
	}
}

void comptime_executor_context::add_module(std::unique_ptr<llvm::Module> module)
{
	if (this->engine == nullptr)
	{
		this->add_base_functions_to_module(*module);
		this->engine = this->create_engine(std::move(module));
	}
	else
	{
		this->engine->addModule(std::move(module));
	}
}

bz::u8string_view comptime_executor_context::get_error_message(void)
{
	if (this->engine == nullptr)
	{
		return bz::u8string_view{};
	}
	else
	{
		bz_assert(this->error_string_ptr_getter != nullptr);
		auto const string_ptr_int_val = this->engine->runFunction(this->error_string_ptr_getter, {});
		auto const string_ptr = reinterpret_cast<bz::u8string const *>(string_ptr_int_val.IntVal.getLimitedValue());
		if (string_ptr == nullptr)
		{
			return bz::u8string_view{};
		}
		else
		{
			return *string_ptr;
		}
	}
}

lex::src_tokens comptime_executor_context::get_error_position(void)
{
	if (this->engine == nullptr)
	{
		return {};
	}
	else
	{
		bz_assert(this->error_token_begin_getter != nullptr);
		bz_assert(this->error_token_pivot_getter != nullptr);
		bz_assert(this->error_token_end_getter   != nullptr);
		auto const begin_ptr_int_val = this->engine->runFunction(this->error_token_begin_getter, {});
		auto const pivot_ptr_int_val = this->engine->runFunction(this->error_token_pivot_getter, {});
		auto const end_ptr_int_val   = this->engine->runFunction(this->error_token_end_getter,   {});
		auto const begin_ptr = reinterpret_cast<lex::token_pos::const_pointer>(begin_ptr_int_val.IntVal.getLimitedValue());
		auto const pivot_ptr = reinterpret_cast<lex::token_pos::const_pointer>(pivot_ptr_int_val.IntVal.getLimitedValue());
		auto const end_ptr   = reinterpret_cast<lex::token_pos::const_pointer>(end_ptr_int_val  .IntVal.getLimitedValue());
		bz_assert(
			(begin_ptr != nullptr && pivot_ptr != nullptr && end_ptr != nullptr)
			|| (begin_ptr == nullptr && pivot_ptr == nullptr && end_ptr == nullptr)
		);
		return lex::src_tokens{ begin_ptr, pivot_ptr, end_ptr };
	}
}

error comptime_executor_context::make_error(
	lex::src_tokens src_tokens, bz::u8string message,
	bz::vector<note> notes, bz::vector<suggestion> suggestions
)
{
	return error{
		warning_kind::_last,
		src_tokens.pivot->src_pos.file_id, src_tokens.pivot->src_pos.line,
		src_tokens.begin->src_pos.begin, src_tokens.pivot->src_pos.begin, (src_tokens.end - 1)->src_pos.end,
		std::move(message),
		std::move(notes), std::move(suggestions)
	};
}

} // namespace ctx
