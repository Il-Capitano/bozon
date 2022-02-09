#include "comptime_executor.h"
#include "ast/statement.h"
#include "global_context.h"
#include "parse_context.h"
#include "bc/comptime/comptime_emit_bitcode.h"
#include "resolve/statement_resolver.h"
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Transforms/Utils.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include "colors.h"

namespace ctx
{

static int get_unique_id()
{
	static int i = 0;
	return i++;
}

static bz::vector<comptime_function> create_empty_comptime_functions(void)
{
	bz::vector<comptime_function> result;
	result.resize(static_cast<uint32_t>(comptime_function_kind::_last) - static_cast<uint32_t>(comptime_function_kind::_first));
	for (auto const kind : bz::iota(0, static_cast<uint32_t>(comptime_function_kind::_last)))
	{
		result[kind] = { static_cast<comptime_function_kind>(kind), nullptr, nullptr };
	}
	return result;
}

comptime_executor_context::comptime_executor_context(global_context &_global_ctx)
	: global_ctx(_global_ctx),
	  builder(_global_ctx._llvm_context),
	  comptime_functions(create_empty_comptime_functions())
{}

comptime_executor_context::~comptime_executor_context(void)
{
	if (this->engine != nullptr)
	{
		this->engine->runFunction(this->get_comptime_function(comptime_function_kind::cleanup), {});
	}
	this->engine.reset();
}

std::unique_ptr<llvm::Module> comptime_executor_context::create_module(void) const
{
	auto const module_name = bz::format("comptime_module_{}", get_unique_id());
	auto module = std::make_unique<llvm::Module>(
		llvm::StringRef(module_name.data_as_char_ptr(), module_name.size()),
		this->get_llvm_context()
	);
	module->setDataLayout(this->get_data_layout());
	auto const is_native_target = target == "" || target == "native";
	auto const target_triple = is_native_target
		? llvm::sys::getDefaultTargetTriple()
		: std::string(target.data_as_char_ptr(), target.size());
	module->setTargetTriple(target_triple);
	return module;
}

[[nodiscard]] llvm::Module *comptime_executor_context::push_module(llvm::Module *module)
{
	auto const result = this->current_module;
	this->current_module = module;
	return result;
}

void comptime_executor_context::pop_module(llvm::Module *prev_module)
{
	this->current_module = prev_module;
}

ast::type_info *comptime_executor_context::get_builtin_type_info(uint32_t kind)
{
	return this->global_ctx.get_builtin_type_info(kind);
}

ast::typespec_view comptime_executor_context::get_builtin_type(bz::u8string_view name)
{
	return this->global_ctx.get_builtin_type(name);
}

ast::function_body *comptime_executor_context::get_builtin_function(uint32_t kind)
{
	return &this->global_ctx.get_builtin_function(kind)->body;
}

variable_ptr_type_pair comptime_executor_context::get_variable(ast::decl_variable const *var_decl) const
{
	auto const it = this->vars_.find(var_decl);
	return it == this->vars_.end() ? variable_ptr_type_pair{ nullptr, nullptr } : it->second;
}

void comptime_executor_context::add_variable(ast::decl_variable const *var_decl, llvm::Value *val, llvm::Type *type)
{
	this->vars_.insert_or_assign(var_decl, variable_ptr_type_pair{ val, type });
}

void comptime_executor_context::add_global_variable(ast::decl_variable const *var_decl)
{
	if (this->vars_.find(var_decl) == this->vars_.end())
	{
		bc::comptime::emit_global_variable(*var_decl, *this);
	}
}

llvm::Type *comptime_executor_context::get_base_type(ast::type_info *info)
{
	if (info->state != ast::resolve_state::all)
	{
		bz_assert(this->current_parse_ctx != nullptr);
		this->current_parse_ctx->add_to_resolve_queue({}, *info);
		resolve::resolve_type_info(*info, *this->current_parse_ctx);
		this->current_parse_ctx->pop_resolve_queue();
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
		bz_assert(func_body->state != ast::resolve_state::error);
		bz_assert(func_body->state >= ast::resolve_state::symbol);
		auto module = this->create_module();
		auto const prev_module = this->push_module(module.get());
		auto const fn = bc::comptime::add_function_to_module(func_body, *this);
		this->pop_module(prev_module);
		bz_assert(this->modules_and_functions.find(func_body) == this->modules_and_functions.end());
		this->modules_and_functions[func_body] = { std::move(module), fn };
		return fn;
	}
	else
	{
		return it->second;
	}
}

comptime_executor_context::module_function_pair
comptime_executor_context::get_module_and_function(ast::function_body *func_body)
{
	auto const it = this->modules_and_functions.find(func_body);
	if (it == this->modules_and_functions.end())
	{
		bz_assert(func_body->state != ast::resolve_state::error);
		bz_assert(func_body->state >= ast::resolve_state::symbol);
		auto module = this->create_module();
		auto const prev_module = this->push_module(module.get());
		auto const fn = bc::comptime::add_function_to_module(func_body, *this);
		this->pop_module(prev_module);
		return { std::move(module), fn };
	}
	else
	{
		bz_assert(it != this->modules_and_functions.end());
		auto result = std::move(it->second);
		this->modules_and_functions.erase(it);
		return result;
	}
}

llvm::LLVMContext &comptime_executor_context::get_llvm_context(void) const noexcept
{
	return this->global_ctx._llvm_context;
}

llvm::DataLayout &comptime_executor_context::get_data_layout(void) const noexcept
{
	return this->global_ctx._data_layout.get();
}

llvm::Module &comptime_executor_context::get_module(void) const noexcept
{
	bz_assert(this->current_module != nullptr);
	return *this->current_module;
}

abi::platform_abi comptime_executor_context::get_platform_abi(void) const noexcept
{
	return this->global_ctx._platform_abi;
}

size_t comptime_executor_context::get_size(ast::typespec_view ts)
{
	auto const llvm_t = bc::get_llvm_type(ts, *this);
	return this->get_size(llvm_t);
}

size_t comptime_executor_context::get_align(ast::typespec_view ts)
{
	auto const llvm_t = bc::get_llvm_type(ts, *this);
	return this->get_align(llvm_t);
}

size_t comptime_executor_context::get_size(llvm::Type *t) const
{
	return this->global_ctx._data_layout->getTypeAllocSize(t);
}

size_t comptime_executor_context::get_align(llvm::Type *t) const
{
	return this->global_ctx._data_layout->getPrefTypeAlign(t).value();
}

size_t comptime_executor_context::get_offset(llvm::Type *t, size_t elem) const
{
	bz_assert(t->isStructTy());
	return this->global_ctx._data_layout->getStructLayout(static_cast<llvm::StructType *>(t))->getElementOffset(elem);
}


size_t comptime_executor_context::get_register_size(void) const
{
	switch (this->global_ctx._platform_abi)
	{
	case abi::platform_abi::generic:
	{
		static size_t register_size = this->global_ctx._data_layout->getLargestLegalIntTypeSizeInBits() / 8;
		return register_size;
	}
	case abi::platform_abi::microsoft_x64:
		bz_assert(this->global_ctx._data_layout->getLargestLegalIntTypeSizeInBits() == 64);
		return 8;
	case abi::platform_abi::systemv_amd64:
		bz_assert(this->global_ctx._data_layout->getLargestLegalIntTypeSizeInBits() == 64);
		return 8;
	}
	bz_unreachable;
}

llvm::BasicBlock *comptime_executor_context::add_basic_block(bz::u8string_view name)
{
	return llvm::BasicBlock::Create(
		this->global_ctx._llvm_context,
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
	auto const symbol_name = bz::format(".str.{}", get_unique_id());
	auto const symbol_name_ref = llvm::StringRef(symbol_name.data_as_char_ptr(), symbol_name.size());
	return this->builder.CreateGlobalString(str_ref, symbol_name_ref, 0, &this->get_module());
}

llvm::Value *comptime_executor_context::create_bitcast(bc::val_ptr val, llvm::Type *dest_type)
{
	if (val.kind == bc::val_ptr::reference)
	{
		auto const dest_ptr = this->builder.CreateBitCast(
			val.val, llvm::PointerType::get(dest_type, 0)
		);
		return this->create_load(dest_ptr);
	}
	else
	{
		auto const src_value = val.get_value(this->builder);
		auto const dest_ptr = this->create_alloca(dest_type);
		auto const cast_ptr = this->builder.CreateBitCast(
			dest_ptr, llvm::PointerType::get(val.get_type(), 0)
		);
		this->builder.CreateStore(src_value, cast_ptr);
		return this->create_load(dest_ptr);
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

llvm::Value *comptime_executor_context::create_load(llvm::Value *ptr, bz::u8string_view name)
{
	auto const name_ref = llvm::StringRef(name.data(), name.size());
	bz_assert(ptr->getType()->isPointerTy());
	return this->builder.CreateLoad(ptr->getType()->getPointerElementType(), ptr, name_ref);
}

llvm::Value *comptime_executor_context::create_gep(llvm::Value *ptr, uint64_t idx, bz::u8string_view name)
{
	auto const name_ref = llvm::StringRef(name.data(), name.size());
	bz_assert(ptr->getType()->isPointerTy());
	return this->builder.CreateConstGEP1_64(ptr->getType()->getPointerElementType(), ptr, idx, name_ref);
}

llvm::Value *comptime_executor_context::create_gep(llvm::Value *ptr, uint64_t idx0, uint64_t idx1, bz::u8string_view name)
{
	auto const name_ref = llvm::StringRef(name.data(), name.size());
	bz_assert(ptr->getType()->isPointerTy());
	return this->builder.CreateConstGEP2_64(ptr->getType()->getPointerElementType(), ptr, idx0, idx1, name_ref);
}

llvm::Value *comptime_executor_context::create_gep(llvm::Value *ptr, llvm::Value *idx, bz::u8string_view name)
{
	auto const name_ref = llvm::StringRef(name.data(), name.size());
	bz_assert(ptr->getType()->isPointerTy());
	return this->builder.CreateGEP(ptr->getType()->getPointerElementType(), ptr, idx, name_ref);
}

llvm::Value *comptime_executor_context::create_gep(llvm::Value *ptr, bz::array_view<llvm::Value * const> indices, bz::u8string_view name)
{
	auto const name_ref = llvm::StringRef(name.data(), name.size());
	bz_assert(ptr->getType()->isPointerTy());
	return this->builder.CreateGEP(ptr->getType()->getPointerElementType(), ptr, llvm::ArrayRef(indices.data(), indices.size()), name_ref);
}

llvm::Value *comptime_executor_context::create_struct_gep(llvm::Value *ptr, uint64_t idx, bz::u8string_view name)
{
	auto const name_ref = llvm::StringRef(name.data(), name.size());
	bz_assert(ptr->getType()->isPointerTy());
	return this->builder.CreateStructGEP(ptr->getType()->getPointerElementType(), ptr, idx, name_ref);
}

llvm::Value *comptime_executor_context::create_array_gep(llvm::Value *ptr, llvm::Value *idx, bz::u8string_view name)
{
	auto const name_ref = llvm::StringRef(name.data(), name.size());
	bz_assert(ptr->getType()->isPointerTy());
	auto const zero_value = llvm::ConstantInt::get(this->get_uint64_t(), 0);
	return this->builder.CreateGEP(ptr->getType()->getPointerElementType(), ptr, { zero_value, idx }, name_ref);
}

llvm::Type *comptime_executor_context::get_builtin_type(uint32_t kind) const
{
	bz_assert(kind <= ast::type_info::null_t_);
	return this->global_ctx._llvm_builtin_types[kind];
}

llvm::Type *comptime_executor_context::get_int8_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::int8_)]; }

llvm::Type *comptime_executor_context::get_int16_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::int16_)]; }

llvm::Type *comptime_executor_context::get_int32_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::int32_)]; }

llvm::Type *comptime_executor_context::get_int64_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::int64_)]; }

llvm::Type *comptime_executor_context::get_uint8_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::uint8_)]; }

llvm::Type *comptime_executor_context::get_uint16_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::uint16_)]; }

llvm::Type *comptime_executor_context::get_uint32_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::uint32_)]; }

llvm::Type *comptime_executor_context::get_uint64_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::uint64_)]; }

llvm::Type *comptime_executor_context::get_float32_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::float32_)]; }

llvm::Type *comptime_executor_context::get_float64_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::float64_)]; }

llvm::Type *comptime_executor_context::get_str_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::str_)]; }

llvm::Type *comptime_executor_context::get_char_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::char_)]; }

llvm::Type *comptime_executor_context::get_bool_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::bool_)]; }

llvm::Type *comptime_executor_context::get_null_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::null_t_)]; }

llvm::Type *comptime_executor_context::get_usize_t(void) const
{
	switch (this->global_ctx.get_data_layout().getPointerSize())
	{
	case 8:
		return this->get_uint64_t();
	case 4:
		return this->get_uint32_t();
	case 2:
		return this->get_uint16_t();
	case 1:
		return this->get_uint8_t();
	default:
		bz_unreachable;
	}
}

llvm::Type *comptime_executor_context::get_isize_t(void) const
{
	switch (this->global_ctx.get_data_layout().getPointerSize())
	{
	case 8:
		return this->get_int64_t();
	case 4:
		return this->get_int32_t();
	case 2:
		return this->get_int16_t();
	case 1:
		return this->get_int8_t();
	default:
		bz_unreachable;
	}
}

llvm::StructType *comptime_executor_context::get_slice_t(llvm::Type *elem_type) const
{
	auto const elem_ptr_type = llvm::PointerType::get(elem_type, 0);
	return llvm::StructType::get(elem_ptr_type, elem_ptr_type);
}

llvm::StructType *comptime_executor_context::get_tuple_t(bz::array_view<llvm::Type * const> types) const
{
	return llvm::StructType::get(this->get_llvm_context(), llvm::ArrayRef(types.data(), types.data() + types.size()));
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

bool comptime_executor_context::do_error_checking(void) const
{
	auto const current_function = this->current_function.first;
	return current_function == nullptr
		|| (
			!current_function->is_no_comptime_checking()
			&& current_function->src_tokens.pivot != nullptr
			&& current_function->src_tokens.pivot->src_pos.file_id != this->comptime_checking_file_id
		);
}

void comptime_executor_context::push_expression_scope(void)
{
	this->destructor_calls.emplace_back();
}

void comptime_executor_context::pop_expression_scope(void)
{
	if (!this->has_terminator())
	{
		this->emit_destructor_calls();
	}
	this->destructor_calls.pop_back();
}

void comptime_executor_context::push_destructor_call(lex::src_tokens src_tokens, ast::function_body *dtor_func, llvm::Value *ptr)
{
	bz_assert(!this->destructor_calls.empty());
	this->destructor_calls.back().push_back({ src_tokens, dtor_func, ptr });
}

void comptime_executor_context::emit_destructor_calls(void)
{
	bz_assert(!this->has_terminator());
	bz_assert(!this->destructor_calls.empty());
	for (auto const &[src_tokens, func, val] : this->destructor_calls.back().reversed())
	{
		auto const error_count = bc::comptime::emit_push_call(src_tokens, func, *this);
		this->builder.CreateCall(this->get_function(func), val);
		bc::comptime::emit_pop_call(error_count, *this);
	}
}

void comptime_executor_context::emit_loop_destructor_calls(void)
{
	bz_assert(!this->has_terminator());
	bz_assert(!this->destructor_calls.empty());
	for (auto const &scope_calls : this->destructor_calls.slice(this->loop_info.destructor_stack_begin).reversed())
	{
		for (auto const &[src_tokens, func, val] : scope_calls.reversed())
		{
			auto const error_count = bc::comptime::emit_push_call(src_tokens, func, *this);
			this->builder.CreateCall(this->get_function(func), val);
			bc::comptime::emit_pop_call(error_count, *this);
		}
	}
}

void comptime_executor_context::emit_all_destructor_calls(void)
{
	bz_assert(!this->has_terminator());
	bz_assert(!this->destructor_calls.empty());
	for (auto const &scope_calls : this->destructor_calls.reversed())
	{
		for (auto const &[src_tokens, func, val] : scope_calls.reversed())
		{
			auto const error_count = bc::comptime::emit_push_call(src_tokens, func, *this);
			this->builder.CreateCall(this->get_function(func), val);
			bc::comptime::emit_pop_call(error_count, *this);
		}
	}
}

[[nodiscard]] comptime_executor_context::loop_info_t
comptime_executor_context::push_loop(llvm::BasicBlock *break_bb, llvm::BasicBlock *continue_bb) noexcept
{
	auto const result = this->loop_info;
	this->loop_info.break_bb = break_bb;
	this->loop_info.continue_bb = continue_bb;
	this->loop_info.destructor_stack_begin = this->destructor_calls.size();
	return result;
}

void comptime_executor_context::pop_loop(loop_info_t info) noexcept
{
	this->loop_info = info;
}

void comptime_executor_context::ensure_function_emission(ast::function_body *body)
{
	if (!body->has_builtin_implementation() || body->body.not_null())
	{
		if (!body->is_comptime_bitcode_emitted())
		{
			this->functions_to_compile.push_back(body);
		}
	}
}

bool comptime_executor_context::resolve_function(ast::function_body *body)
{
	if (body->body.is_null())
	{
		return body->has_builtin_implementation();
	}
	bz_assert(this->current_parse_ctx != nullptr);
	this->current_parse_ctx->add_to_resolve_queue({}, *body);
	resolve::resolve_function({}, *body, *this->current_parse_ctx);
	this->current_parse_ctx->pop_resolve_queue();
	return body->state != ast::resolve_state::error;
}

llvm::Function *comptime_executor_context::get_comptime_function(comptime_function_kind kind)
{
	bz_assert(this->comptime_functions[static_cast<uint32_t>(kind)].llvm_func != nullptr);
	return this->comptime_functions[static_cast<uint32_t>(kind)].llvm_func;
}

void comptime_executor_context::set_comptime_function(comptime_function_kind kind, ast::function_body *func_body)
{
	bz_assert(this->comptime_functions[static_cast<uint32_t>(kind)].func_body == nullptr);
	bz_assert(this->comptime_functions[static_cast<uint32_t>(kind)].llvm_func == nullptr);
	this->comptime_functions[static_cast<uint32_t>(kind)].func_body = func_body;
}

void comptime_executor_context::set_comptime_function(comptime_function_kind kind, llvm::Function *llvm_func)
{
	bz_assert(this->comptime_functions[static_cast<uint32_t>(kind)].func_body != nullptr);
	bz_assert(this->comptime_functions[static_cast<uint32_t>(kind)].llvm_func == nullptr);
	this->comptime_functions[static_cast<uint32_t>(kind)].llvm_func = llvm_func;
}

static ast::constant_value constant_value_from_generic_value(llvm::GenericValue const &value, ast::typespec_view result_type)
{
	ast::constant_value result;
	ast::remove_const_or_consteval(result_type).visit(bz::overload{
		[&](ast::ts_base_type const &base_t) {
			switch (base_t.info->kind)
			{
			case ast::type_info::int8_:
			case ast::type_info::int16_:
			case ast::type_info::int32_:
			case ast::type_info::int64_:
				result.emplace<ast::constant_value::sint>(value.IntVal.getSExtValue());
				break;
			case ast::type_info::uint8_:
			case ast::type_info::uint16_:
			case ast::type_info::uint32_:
			case ast::type_info::uint64_:
				result.emplace<ast::constant_value::uint>(value.IntVal.getLimitedValue());
				break;
			case ast::type_info::float32_:
				result.emplace<ast::constant_value::float32>(value.FloatVal);
				break;
			case ast::type_info::float64_:
				result.emplace<ast::constant_value::float64>(value.DoubleVal);
				break;
			case ast::type_info::char_:
				result.emplace<ast::constant_value::u8char>(static_cast<bz::u8char>(value.IntVal.getLimitedValue()));
				break;
			case ast::type_info::str_:
				bz_assert(value.AggregateVal.size() == 2);
				result.emplace<ast::constant_value::string>(bz::u8string_view(
					static_cast<char const *>(value.AggregateVal[0].PointerVal),
					static_cast<char const *>(value.AggregateVal[1].PointerVal)
				));
				break;
			case ast::type_info::bool_:
				result.emplace<ast::constant_value::boolean>(value.IntVal.getBoolValue());
				break;
			case ast::type_info::null_t_:
				result.emplace<ast::constant_value::null>();
				break;
			case ast::type_info::aggregate:
			{
				result.emplace<ast::constant_value::aggregate>(
					bz::zip(value.AggregateVal, base_t.info->member_variables)
					.transform([](auto const &pair) { return constant_value_from_generic_value(pair.first, pair.second->get_type()); })
					.collect()
				);
				break;
			}
			case ast::type_info::forward_declaration:
				bz_unreachable;
			}
		},
		[&](ast::ts_void const &) {
			result.emplace<ast::constant_value::void_>();
		},
		[](ast::ts_function const &) {
			bz_unreachable;
		},
		[&](ast::ts_array const &array_t) {
			result.emplace<ast::constant_value::array>(
				bz::basic_range(value.AggregateVal.begin(), value.AggregateVal.end())
				.transform([&](auto const &val) { return constant_value_from_generic_value(val, array_t.elem_type); })
				.collect()
			);
		},
		[](ast::ts_array_slice const &) {
			bz_unreachable;
		},
		[&](ast::ts_tuple const &tuple_t) {
			result.emplace<ast::constant_value::tuple>(
				bz::zip(value.AggregateVal, tuple_t.types)
				.transform([](auto const &pair) { return constant_value_from_generic_value(pair.first, pair.second); })
				.collect()
			);
		},
		[&](ast::ts_pointer const &) {
			if (value.PointerVal == nullptr)
			{
				result.emplace<ast::constant_value::null>();
			}
			else
			{
				// nothing
			}
		},
		[](ast::ts_lvalue_reference const &) {
			bz_unreachable;
		},
		[](ast::ts_move_reference const &) {
			bz_unreachable;
		},
		[](ast::ts_auto_reference const &) {
			bz_unreachable;
		},
		[](ast::ts_auto_reference_const const &) {
			bz_unreachable;
		},
		[](ast::ts_variadic const &) {
			bz_unreachable;
		},
		[](ast::ts_unresolved const &) {
			bz_unreachable;
		},
		[](ast::ts_const const &) {
			bz_unreachable;
		},
		[](ast::ts_consteval const &) {
			bz_unreachable;
		},
		[](ast::ts_auto const &) {
			bz_unreachable;
		},
		[](ast::ts_typename const &) {
			bz_unreachable;
		},
	});
	return result;
}

static ast::constant_value constant_value_from_global_getters(
	ast::typespec_view result_type,
	bz::vector<llvm::Function *>::const_iterator &getter_it,
	comptime_executor_context &context
)
{
	return ast::remove_const_or_consteval(result_type).visit(bz::overload{
		[&](ast::ts_base_type const &base_t) -> ast::constant_value {
			if (base_t.info->kind == ast::type_info::aggregate)
			{
				ast::constant_value result;
				auto &agg = result.emplace<ast::constant_value::aggregate>();
				agg.reserve(base_t.info->member_variables.size());
				for (auto const decl : base_t.info->member_variables)
				{
					agg.push_back(constant_value_from_global_getters(decl->get_type(), getter_it, context));
				}
				return result;
			}
			else if (base_t.info->kind == ast::type_info::str_)
			{
				auto const begin_ptr = context.engine->runFunction(*getter_it, {});
				++getter_it;
				auto const end_ptr = context.engine->runFunction(*getter_it, {});
				++getter_it;
				ast::constant_value result;
				result.emplace<ast::constant_value::string>() = bz::u8string_view(
					static_cast<char const *>(begin_ptr.PointerVal),
					static_cast<char const *>(end_ptr.PointerVal)
				);
				return result;
			}
			else
			{
				auto const call_result = context.engine->runFunction(*getter_it, {});
				++getter_it;
				return constant_value_from_generic_value(call_result, result_type);
			}
		},
		[&](ast::ts_array const &array_t) -> ast::constant_value {
			ast::constant_value result;
			result.emplace<ast::constant_value::array>();
			auto &arr = result.get<ast::constant_value::array>();
			arr.reserve(array_t.size);
			for ([[maybe_unused]] auto const _ : bz::iota(0, array_t.size))
			{
				arr.push_back(constant_value_from_global_getters(array_t.elem_type, getter_it, context));
			}
			return result;
		},
		[&](ast::ts_tuple const &tuple_t) -> ast::constant_value {
			ast::constant_value result;
			result.emplace<ast::constant_value::tuple>();
			auto &tuple = result.get<ast::constant_value::tuple>();
			tuple.reserve(tuple_t.types.size());
			for (auto const &type : tuple_t.types)
			{
				tuple.push_back(constant_value_from_global_getters(type, getter_it, context));
			}
			return result;
		},
		[&](auto const &) -> ast::constant_value {
			auto const call_result = context.engine->runFunction(*getter_it, {});
			++getter_it;
			return constant_value_from_generic_value(call_result, result_type);
		}
	});
}

std::pair<ast::constant_value, bz::vector<error>> comptime_executor_context::execute_function(
	lex::src_tokens src_tokens,
	ast::function_body *body,
	bz::array_view<ast::expression const> params
)
{
	bz_assert(this->destructor_calls.empty());

	(void)this->resolve_function(body);
	std::pair<ast::constant_value, bz::vector<error>> result;
	if (body->state == ast::resolve_state::error)
	{
		return result;
	}
	else if (body->state != ast::resolve_state::all && !body->has_builtin_implementation())
	{
		result.second.push_back(error{
			warning_kind::_last,
			source_highlight{
				src_tokens.pivot->src_pos.file_id, src_tokens.pivot->src_pos.line,
				src_tokens.begin->src_pos.begin, src_tokens.pivot->src_pos.begin, (src_tokens.end - 1)->src_pos.end,
				suggestion_range{}, suggestion_range{},
				bz::format("unable to call external function '{}' in a constant expression", body->get_signature())
			},
			{}, {}
		});
		return result;
	}
	else
	{
		this->initialize_engine();
		auto module = this->create_module();
		auto const prev_module = this->push_module(module.get());

		auto const start_index = this->functions_to_compile.size();
		auto const [fn, global_result_getters] = bc::comptime::create_function_for_comptime_execution(body, params, *this);
		if (!bc::comptime::emit_necessary_functions(start_index, *this))
		{
			this->functions_to_compile.resize(start_index);
			this->pop_module(prev_module);
			return result;
		}

		// bz::log("{}>>>>>>>> verifying module <<<<<<<<{}\n", colors::bright_red, colors::clear);
		// llvm::verifyModule(*this->current_module, &llvm::dbgs());
		this->add_module(std::move(module));
		auto const call_result = this->engine->runFunction(fn, {});

		if (!this->has_error())
		{
			if (global_result_getters.empty())
			{
				result.first = constant_value_from_generic_value(call_result, body->return_type);
			}
			else
			{
				auto getter_it = global_result_getters.cbegin();
				result.first = constant_value_from_global_getters(body->return_type, getter_it, *this);
			}
		}
		result.second.append_move(this->consume_errors());

		this->functions_to_compile.resize(start_index);
		this->pop_module(prev_module);
		return result;
	}
}

std::pair<ast::constant_value, bz::vector<error>> comptime_executor_context::execute_compound_expression(ast::expr_compound &expr)
{
	bz_assert(this->destructor_calls.empty());
	this->initialize_engine();
	auto module = this->create_module();
	auto const prev_module = this->push_module(module.get());

	std::pair<ast::constant_value, bz::vector<error>> result;

	auto const start_index = this->functions_to_compile.size();
	auto const [fn, global_result_getters] = bc::comptime::create_function_for_comptime_execution(expr, *this);
	if (!bc::comptime::emit_necessary_functions(start_index, *this))
	{
		this->functions_to_compile.resize(start_index);
		this->pop_module(prev_module);
		return result;
	}

	// bz::log("{}>>>>>>>> verifying {} <<<<<<<<{}\n", colors::bright_red, module_name, colors::clear);
	// llvm::verifyModule(*this->current_module, &llvm::dbgs());
	this->add_module(std::move(module));
	auto const call_result = this->engine->runFunction(fn, {});
	if (!this->has_error())
	{
		if (expr.final_expr.is_null())
		{
			result.first.emplace<ast::constant_value::void_>();
		}
		else if (global_result_getters.empty())
		{
			auto const result_type = expr.final_expr.get_expr_type_and_kind().first;
			if (result_type.is_typename())
			{
				// nothing, compound expressions can have type results as long as the expression itself can
				// be evaluated at compile time
				bz_assert(expr.final_expr.is<ast::constant_expression>());
				result.first = ast::constant_value(expr.final_expr.get_typename());
			}
			else
			{
				result.first = constant_value_from_generic_value(call_result, ast::remove_const_or_consteval(result_type));
			}
		}
		else
		{
			auto const result_type = expr.final_expr.get_expr_type_and_kind().first;
			auto getter_it = global_result_getters.cbegin();
			result.first = constant_value_from_global_getters(result_type, getter_it, *this);
		}
	}
	result.second.append_move(this->consume_errors());
	this->functions_to_compile.resize(start_index);
	this->pop_module(prev_module);
	return result;
}

struct str_t
{
	uint8_t const *begin;
	uint8_t const *end;
};

static bool bozon_is_option_set_impl(char const *begin, char const *end)
{
	auto const s = bz::u8string_view(begin, end);
	return defines.contains(s);
}

static void bozon_print_stdout(str_t s)
{
	fwrite(s.begin, 1, s.end - s.begin, stdout);
}

static void bozon_println_stdout(str_t s)
{
	fwrite(s.begin, 1, s.end - s.begin, stdout);
	char new_line = '\n';
	fwrite(&new_line, 1, 1, stdout);
}

static void bozon_debug_print(char const *s)
{
	bz::log("{}\n", s);
}

static void *bozon_builtin_comptime_malloc(uint64_t size) noexcept
{
	auto const result = std::malloc(size);
	// bz::log("allocating {} bytes at {}\n", size, result);
	return result;
}

static void bozon_builtin_comptime_free(void *ptr) noexcept
{
	// bz::log("freeing {}\n", ptr);
	std::free(ptr);
}

void comptime_executor_context::initialize_engine(void)
{
	if (this->engine == nullptr)
	{
		this->engine = this->create_engine(this->create_module());

		this->pass_manager.add(llvm::createInstructionCombiningPass());
		this->pass_manager.add(llvm::createGVNPass());
		this->pass_manager.add(llvm::createPromoteMemoryToRegisterPass());
		this->pass_manager.add(llvm::createReassociatePass());
		this->pass_manager.add(llvm::createCFGSimplificationPass());
		// this->pass_manager.add(llvm::createInstructionCombiningPass());
		this->pass_manager.add(llvm::createMemCpyOptPass());

		this->add_base_functions_to_engine();

		this->engine->addGlobalMapping("__bozon_builtin_is_option_set_impl", reinterpret_cast<uint64_t>(&bozon_is_option_set_impl));
		this->engine->addGlobalMapping("__bozon_builtin_print_stdout",       reinterpret_cast<uint64_t>(&bozon_print_stdout));
		this->engine->addGlobalMapping("__bozon_builtin_println_stdout",     reinterpret_cast<uint64_t>(&bozon_println_stdout));
		this->engine->addGlobalMapping("__bozon_builtin_comptime_malloc",    reinterpret_cast<uint64_t>(&bozon_builtin_comptime_malloc));
		this->engine->addGlobalMapping("__bozon_builtin_comptime_free",      reinterpret_cast<uint64_t>(&bozon_builtin_comptime_free));
		this->engine->addGlobalMapping("__bozon_builtin_debug_print",        reinterpret_cast<uint64_t>(&bozon_debug_print));
	}
}

std::unique_ptr<llvm::ExecutionEngine> comptime_executor_context::create_engine(std::unique_ptr<llvm::Module> module)
{
	if (debug_comptime_ir_output)
	{
		std::error_code ec;
		auto output_file = llvm::raw_fd_ostream("comptime_output.ll", ec, llvm::sys::fs::OF_Text);
		if (!ec)
		{
			module->print(output_file, nullptr);
		}
	}
	std::string err;
	llvm::EngineBuilder builder(std::move(module));

	auto const is_native =
		(
			target == "" || target == "native"
			|| llvm::Triple::normalize(std::string(target.data_as_char_ptr(), target.size())) == llvm::Triple::normalize(llvm::sys::getDefaultTargetTriple())
		)
		&& this->get_platform_abi() != abi::platform_abi::generic;
	auto const engine_kind = force_use_jit || (is_native && !use_interpreter) ? llvm::EngineKind::JIT : llvm::EngineKind::Interpreter;

	builder
		.setEngineKind(engine_kind)
		.setErrorStr(&err)
		.setOptLevel(llvm::CodeGenOpt::None);
	auto const result = builder.create();
	if (result == nullptr)
	{
		bz::log("execution engine creation failed: {}\n", err.c_str());
		exit(1);
	}
	return std::unique_ptr<llvm::ExecutionEngine>(result);
}

void comptime_executor_context::add_base_functions_to_engine(void)
{
	static_assert(sizeof (void *) == sizeof (uint64_t));

	bz_assert(this->functions_to_compile.empty());

	{
		auto module = this->create_module();
		auto const prev_module = this->push_module(module.get());
		bz_assert(this->errors_array != nullptr);
		bc::comptime::emit_global_variable(*this->errors_array, *this);
		bz_assert(this->call_stack != nullptr);
		bc::comptime::emit_global_variable(*this->call_stack, *this);
		bz_assert(this->global_strings != nullptr);
		bc::comptime::emit_global_variable(*this->global_strings, *this);
		bz_assert(this->malloc_infos != nullptr);
		bc::comptime::emit_global_variable(*this->malloc_infos, *this);

		this->pop_module(prev_module);
		this->add_module(std::move(module));
	}

	for (auto &func : this->comptime_functions)
	{
		bz_assert(func.func_body != nullptr);
		bz_assert(func.llvm_func == nullptr);
		auto module = this->create_module();
		auto const prev_module = this->push_module(module.get());
		func.llvm_func = bc::comptime::add_function_to_module(func.func_body, *this);
		this->functions_to_compile.push_back(func.func_body);
		this->pop_module(prev_module);
		bz_assert(this->modules_and_functions.find(func.func_body) == this->modules_and_functions.end());
		this->modules_and_functions[func.func_body] = { std::move(module), func.llvm_func };
	}

	[[maybe_unused]] auto const emit_result = bc::comptime::emit_necessary_functions(0, *this);
	bz_assert(emit_result);
	this->functions_to_compile.clear();
}

void comptime_executor_context::add_module(std::unique_ptr<llvm::Module> module)
{
	// this->pass_manager.run(*module);
	this->pass_manager.run(*module);
	if (debug_comptime_ir_output)
	{
		std::error_code ec;
		auto output_file = llvm::raw_fd_ostream("comptime_output.ll", ec, llvm::sys::fs::OF_Text | llvm::sys::fs::OF_Append);
		if (!ec)
		{
			module->print(output_file, nullptr);
		}
		output_file.flush();
	}
	this->engine->addModule(std::move(module));
}

bool comptime_executor_context::has_error(void)
{
	if (this->engine == nullptr)
	{
		return false;
	}
	else
	{
		return this->engine->runFunction(this->get_comptime_function(comptime_function_kind::has_errors), {}).IntVal.getBoolValue();
	}
}

template<typename Ret, typename ...Args>
static Ret call_llvm_func(llvm::ExecutionEngine *engine, llvm::Function *llvm_func, Args ...args)
{
	auto const func_ptr = engine->getFunctionAddress(llvm_func->getName().str());
	if (func_ptr != 0)
	{
		return reinterpret_cast<Ret (*)(Args...)>(func_ptr)(args...);
	}
	else
	{
		auto const run_result = engine->runFunction(
			llvm_func,
			{
				[&]() {
					llvm::GenericValue value;
					if constexpr (std::is_pointer_v<Args>)
					{
						value.PointerVal = args;
					}
					else
					{
						value.IntVal = llvm::APInt(sizeof args * 8, args);
					}
					return value;
				}()...
			}
		);
		if constexpr (std::is_void_v<Ret>)
		{
			return;
		}
		else
		{
			static_assert(std::is_integral_v<Ret>);
			return static_cast<Ret>(run_result.IntVal.getLimitedValue());
		}
	}
}

bz::vector<error> comptime_executor_context::consume_errors(void)
{
	bz_assert(this->engine != nullptr);
	auto const error_count = this->engine->runFunction(this->get_comptime_function(comptime_function_kind::get_error_count), {})
		.IntVal.getLimitedValue();
	bz::vector<error> result;
	result.reserve(error_count);
	for (uint64_t i = 0; i < error_count; ++i)
	{
		auto const error_kind = [&]() -> uint32_t {
			auto const llvm_func = this->get_comptime_function(comptime_function_kind::get_error_kind_by_index);
			return call_llvm_func<uint32_t>(this->engine.get(), llvm_func, i);
		}();
		auto const src_tokens = [&]() -> lex::src_tokens {
			auto const begin_llvm_func = this->get_comptime_function(comptime_function_kind::get_error_begin_by_index);
			auto const begin_ptr_int_val = call_llvm_func<uint64_t>(this->engine.get(), begin_llvm_func, i);
			auto const pivot_llvm_func = this->get_comptime_function(comptime_function_kind::get_error_pivot_by_index);
			auto const pivot_ptr_int_val = call_llvm_func<uint64_t>(this->engine.get(), pivot_llvm_func, i);
			auto const end_llvm_func = this->get_comptime_function(comptime_function_kind::get_error_end_by_index);
			auto const end_ptr_int_val = call_llvm_func<uint64_t>(this->engine.get(), end_llvm_func, i);

			return lex::src_tokens{
				lex::token_pos(reinterpret_cast<lex::token const *>(begin_ptr_int_val)),
				lex::token_pos(reinterpret_cast<lex::token const *>(pivot_ptr_int_val)),
				lex::token_pos(reinterpret_cast<lex::token const *>(end_ptr_int_val)),
			};
		}();
		auto error_message = [&]() -> bz::u8string {
			auto const size_llvm_func = this->get_comptime_function(comptime_function_kind::get_error_message_size_by_index);
			auto const size = call_llvm_func<uint64_t>(this->engine.get(), size_llvm_func, i);
			bz::u8string message;
			message.resize(size);
			auto const get_message_llvm_func = this->get_comptime_function(comptime_function_kind::get_error_message_by_index);
			call_llvm_func<void>(this->engine.get(), get_message_llvm_func, i, message.data());
			return message;
		}();
		auto const call_stack_notes = [&]() {
			bz::vector<source_highlight> notes;
			auto const size_llvm_func = this->get_comptime_function(comptime_function_kind::get_error_call_stack_size_by_index);
			auto const call_stack_size = call_llvm_func<uint64_t>(this->engine.get(), size_llvm_func, i);
			notes.reserve(call_stack_size);
			llvm::GenericValue call_stack_index;
			for (uint64_t j = call_stack_size; j != 0;)
			{
				--j;
				auto const ptr_llvm_func = this->get_comptime_function(comptime_function_kind::get_error_call_stack_element_by_index);
				auto const call_ptr_int_val = call_llvm_func<uint64_t>(this->engine.get(), ptr_llvm_func, i, j);
				bz_assert(call_ptr_int_val != 0);
				auto const call_ptr = reinterpret_cast<comptime_func_call const *>(call_ptr_int_val);
				notes.emplace_back(this->make_note(
					call_ptr->src_tokens,
					bz::format("in call to '{}'", call_ptr->func_body->get_signature())
				));
			}
			return notes;
		}();
		result.push_back({
			static_cast<warning_kind>(error_kind),
			source_highlight{
				src_tokens.pivot->src_pos.file_id, src_tokens.pivot->src_pos.line,
				src_tokens.begin->src_pos.begin, src_tokens.pivot->src_pos.begin, (src_tokens.end - 1)->src_pos.end,
				{}, {},
				std::move(error_message)
			},
			std::move(call_stack_notes), {}
		});
	}
	this->engine->runFunction(this->get_comptime_function(comptime_function_kind::clear_errors), {});
	return result;
}

source_highlight const *comptime_executor_context::insert_error(lex::src_tokens src_tokens, bz::u8string message)
{
	auto const &result = this->execution_errors.emplace_back(source_highlight{
		src_tokens.pivot->src_pos.file_id, src_tokens.pivot->src_pos.line,
		src_tokens.begin->src_pos.begin, src_tokens.pivot->src_pos.begin, (src_tokens.end - 1)->src_pos.end,
		suggestion_range{}, suggestion_range{},
		std::move(message)
	});
	return &result;
}

comptime_func_call const *comptime_executor_context::insert_call(lex::src_tokens src_tokens, ast::function_body const *body)
{
	auto const &result = this->execution_calls.emplace_back(comptime_func_call{ src_tokens, body });
	return &result;
}

error comptime_executor_context::make_error(
	bz::u8string message,
	bz::vector<source_highlight> notes, bz::vector<source_highlight> suggestions
)
{
	return error{
		warning_kind::_last,
		{
			global_context::compiler_file_id, 0,
			char_pos(), char_pos(), char_pos(),
			suggestion_range{}, suggestion_range{},
			std::move(message),
		},
		std::move(notes), std::move(suggestions)
	};
}

error comptime_executor_context::make_error(
	lex::src_tokens src_tokens, bz::u8string message,
	bz::vector<source_highlight> notes, bz::vector<source_highlight> suggestions
)
{
	return error{
		warning_kind::_last,
		{
			src_tokens.pivot->src_pos.file_id, src_tokens.pivot->src_pos.line,
			src_tokens.begin->src_pos.begin, src_tokens.pivot->src_pos.begin, (src_tokens.end - 1)->src_pos.end,
			suggestion_range{}, suggestion_range{},
			std::move(message),
		},
		std::move(notes), std::move(suggestions)
	};
}

source_highlight comptime_executor_context::make_note(lex::src_tokens src_tokens, bz::u8string message)
{
	return source_highlight{
		src_tokens.pivot->src_pos.file_id, src_tokens.pivot->src_pos.line,
		src_tokens.begin->src_pos.begin, src_tokens.pivot->src_pos.begin, (src_tokens.end - 1)->src_pos.end,
		suggestion_range{}, suggestion_range{},
		std::move(message),
	};
}

} // namespace ctx
