#include "bitcode_context.h"
#include "global_context.h"
#include "bc/runtime/runtime_emit_bitcode.h"

namespace ctx
{

bitcode_context::bitcode_context(global_context &_global_ctx, llvm::Module *_module)
	: global_ctx(_global_ctx),
	  module(_module),
	  builder(_global_ctx._llvm_context)
{}

ast::type_info *bitcode_context::get_builtin_type_info(uint32_t kind)
{
	return this->global_ctx.get_builtin_type_info(kind);
}

ast::typespec_view bitcode_context::get_builtin_type(bz::u8string_view name)
{
	return this->global_ctx.get_builtin_type(name);
}

ast::function_body *bitcode_context::get_builtin_function(uint32_t kind)
{
	return &this->global_ctx.get_builtin_function(kind)->body;
}

bc::value_and_type_pair bitcode_context::get_variable(ast::decl_variable const *var_decl) const
{
	auto const it = this->vars_.find(var_decl);
	return it == this->vars_.end() ? bc::value_and_type_pair{ nullptr, nullptr } : it->second;
}

void bitcode_context::add_variable(ast::decl_variable const *var_decl, llvm::Value *val, llvm::Type *type)
{
	this->vars_.insert_or_assign(var_decl, bc::value_and_type_pair{ val, type });
}

llvm::Type *bitcode_context::get_base_type(ast::type_info const *info) const
{
	auto const it = this->types_.find(info);
	return it == this->types_.end() ? nullptr : it->second;
}
void bitcode_context::add_base_type(ast::type_info const *info, llvm::Type *type)
{
	this->types_.insert_or_assign(info, type);
}

llvm::Function *bitcode_context::get_function(ast::function_body *func_body)
{
	auto it = this->funcs_.find(func_body);
	if (it == this->funcs_.end())
	{
		bc::runtime::add_function_to_module(func_body, *this);
		this->ensure_function_emission(func_body);
		it = this->funcs_.find(func_body);
		bz_assert(it != this->funcs_.end());
		return it->second;
	}
	else
	{
		this->ensure_function_emission(func_body);
		return it->second;
	}
}

llvm::LLVMContext &bitcode_context::get_llvm_context(void) const noexcept
{
	return this->global_ctx._llvm_context;
}

llvm::DataLayout &bitcode_context::get_data_layout(void) const noexcept
{
	return this->global_ctx._data_layout.get();
}

llvm::Module &bitcode_context::get_module(void) const noexcept
{
	return *this->module;
}

abi::platform_abi bitcode_context::get_platform_abi(void) const noexcept
{
	return this->global_ctx._platform_abi;
}

size_t bitcode_context::get_size(llvm::Type *t) const
{
	return this->global_ctx._data_layout->getTypeAllocSize(t);
}

size_t bitcode_context::get_align(llvm::Type *t) const
{
	return this->global_ctx._data_layout->getPrefTypeAlign(t).value();
}

size_t bitcode_context::get_offset(llvm::Type *t, size_t elem) const
{
	bz_assert(t->isStructTy());
	return this->global_ctx._data_layout->getStructLayout(static_cast<llvm::StructType *>(t))->getElementOffset(elem);
}


size_t bitcode_context::get_register_size(void) const
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

llvm::BasicBlock *bitcode_context::add_basic_block(bz::u8string_view name)
{
	return llvm::BasicBlock::Create(
		this->global_ctx._llvm_context,
		llvm::StringRef(name.data(), name.length()),
		this->current_function.second
	);
}

llvm::Value *bitcode_context::create_alloca(llvm::Type *t)
{
	auto const bb = this->builder.GetInsertBlock();
	this->builder.SetInsertPoint(this->alloca_bb);
	auto const result = this->builder.CreateAlloca(t);
	this->builder.SetInsertPoint(bb);
	auto const size = this->get_size(t);
	this->start_lifetime(result, size);
	this->push_end_lifetime_call(result, size);
	return result;
}

llvm::Value *bitcode_context::create_alloca(llvm::Type *t, size_t align)
{
	auto const bb = this->builder.GetInsertBlock();
	this->builder.SetInsertPoint(this->alloca_bb);
	auto const result = this->builder.CreateAlloca(t);
	result->setAlignment(llvm::Align(align));
	this->builder.SetInsertPoint(bb);
	auto const size = this->get_size(t);
	this->start_lifetime(result, size);
	this->push_end_lifetime_call(result, size);
	return result;
}

llvm::Value *bitcode_context::create_alloca_without_lifetime_start(llvm::Type *t)
{
	auto const bb = this->builder.GetInsertBlock();
	this->builder.SetInsertPoint(this->alloca_bb);
	auto const result = this->builder.CreateAlloca(t);
	this->builder.SetInsertPoint(bb);
	return result;
}

llvm::Value *bitcode_context::create_alloca_without_lifetime_start(llvm::Type *t, size_t align)
{
	auto const bb = this->builder.GetInsertBlock();
	this->builder.SetInsertPoint(this->alloca_bb);
	auto const result = this->builder.CreateAlloca(t);
	result->setAlignment(llvm::Align(align));
	this->builder.SetInsertPoint(bb);
	return result;
}

llvm::Value *bitcode_context::create_string(bz::u8string_view str)
{
	auto const str_ref = llvm::StringRef(str.data(), str.size());
	return this->builder.CreateGlobalString(str_ref, ".str", 0, &this->get_module());
}

llvm::Value *bitcode_context::create_bitcast(bc::val_ptr val, llvm::Type *dest_type)
{
	if (val.kind == bc::val_ptr::reference)
	{
		auto const dest_ptr = this->builder.CreateBitCast(
			val.val, llvm::PointerType::get(dest_type, 0)
		);
		return this->create_load(dest_type, dest_ptr);
	}
	else
	{
		auto const src_value = val.get_value(this->builder);
		auto const dest_ptr = this->create_alloca(dest_type);
		auto const cast_ptr = this->builder.CreateBitCast(
			dest_ptr, llvm::PointerType::get(val.get_type(), 0)
		);
		this->builder.CreateStore(src_value, cast_ptr);
		return this->create_load(dest_type, dest_ptr);
	}
}

llvm::Value *bitcode_context::create_cast_to_int(bc::val_ptr val)
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

llvm::Value *bitcode_context::create_load(llvm::Type *type, llvm::Value *ptr, bz::u8string_view name)
{
	auto const name_ref = llvm::StringRef(name.data(), name.size());
	bz_assert(ptr->getType()->isPointerTy());
	return this->builder.CreateLoad(type, ptr, name_ref);
}

llvm::Value *bitcode_context::create_gep(llvm::Type *type, llvm::Value *ptr, uint64_t idx, bz::u8string_view name)
{
	auto const name_ref = llvm::StringRef(name.data(), name.size());
	bz_assert(ptr->getType()->isPointerTy());
	return this->builder.CreateConstGEP1_64(type, ptr, idx, name_ref);
}

llvm::Value *bitcode_context::create_gep(llvm::Type *type, llvm::Value *ptr, uint64_t idx0, uint64_t idx1, bz::u8string_view name)
{
	auto const name_ref = llvm::StringRef(name.data(), name.size());
	bz_assert(ptr->getType()->isPointerTy());
	return this->builder.CreateConstGEP2_64(type, ptr, idx0, idx1, name_ref);
}

llvm::Value *bitcode_context::create_gep(llvm::Type *type, llvm::Value *ptr, llvm::Value *idx, bz::u8string_view name)
{
	auto const name_ref = llvm::StringRef(name.data(), name.size());
	bz_assert(ptr->getType()->isPointerTy());
	return this->builder.CreateGEP(type, ptr, idx, name_ref);
}

llvm::Value *bitcode_context::create_gep(llvm::Type *type, llvm::Value *ptr, bz::array_view<llvm::Value * const> indices, bz::u8string_view name)
{
	auto const name_ref = llvm::StringRef(name.data(), name.size());
	bz_assert(ptr->getType()->isPointerTy());
	return this->builder.CreateGEP(type, ptr, llvm::ArrayRef(indices.data(), indices.size()), name_ref);
}

llvm::Value *bitcode_context::create_struct_gep(llvm::Type *type, llvm::Value *ptr, uint64_t idx, bz::u8string_view name)
{
	auto const name_ref = llvm::StringRef(name.data(), name.size());
	bz_assert(ptr->getType()->isPointerTy());
	return this->builder.CreateStructGEP(type, ptr, idx, name_ref);
}

llvm::Value *bitcode_context::create_array_gep(llvm::Type *type, llvm::Value *ptr, llvm::Value *idx, bz::u8string_view name)
{
	auto const name_ref = llvm::StringRef(name.data(), name.size());
	bz_assert(ptr->getType()->isPointerTy());
	auto const zero_value = llvm::ConstantInt::get(this->get_uint64_t(), 0);
	return this->builder.CreateGEP(type, ptr, { zero_value, idx }, name_ref);
}

llvm::CallInst *bitcode_context::create_call(
	[[maybe_unused]] lex::src_tokens const &src_tokens,
	[[maybe_unused]] ast::function_body *func_body,
	llvm::Function *fn,
	llvm::ArrayRef<llvm::Value *> args
)
{
	auto const call = this->builder.CreateCall(fn, args);
	call->setCallingConv(fn->getCallingConv());
	return call;
}

llvm::CallInst *bitcode_context::create_call(
	llvm::Function *fn,
	llvm::ArrayRef<llvm::Value *> args
)
{
	auto const call = this->builder.CreateCall(fn, args);
	call->setCallingConv(fn->getCallingConv());
	return call;
}

llvm::Type *bitcode_context::get_builtin_type(uint32_t kind) const
{
	bz_assert(kind <= ast::type_info::null_t_);
	return this->global_ctx._llvm_builtin_types[kind];
}

llvm::Type *bitcode_context::get_int8_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::int8_)]; }

llvm::Type *bitcode_context::get_int16_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::int16_)]; }

llvm::Type *bitcode_context::get_int32_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::int32_)]; }

llvm::Type *bitcode_context::get_int64_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::int64_)]; }

llvm::Type *bitcode_context::get_uint8_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::uint8_)]; }

llvm::Type *bitcode_context::get_uint16_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::uint16_)]; }

llvm::Type *bitcode_context::get_uint32_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::uint32_)]; }

llvm::Type *bitcode_context::get_uint64_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::uint64_)]; }

llvm::Type *bitcode_context::get_float32_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::float32_)]; }

llvm::Type *bitcode_context::get_float64_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::float64_)]; }

llvm::Type *bitcode_context::get_str_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::str_)]; }

llvm::Type *bitcode_context::get_char_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::char_)]; }

llvm::Type *bitcode_context::get_bool_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::bool_)]; }

llvm::Type *bitcode_context::get_null_t(void) const
{ return this->global_ctx._llvm_builtin_types[static_cast<int>(ast::type_info::null_t_)]; }

llvm::Type *bitcode_context::get_usize_t(void) const
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

llvm::Type *bitcode_context::get_isize_t(void) const
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

llvm::StructType *bitcode_context::get_slice_t(llvm::Type *elem_type) const
{
	auto const elem_ptr_type = llvm::PointerType::get(elem_type, 0);
	return llvm::StructType::get(elem_ptr_type, elem_ptr_type);
}

llvm::StructType *bitcode_context::get_tuple_t(bz::array_view<llvm::Type * const> types) const
{
	return llvm::StructType::get(this->get_llvm_context(), llvm::ArrayRef(types.data(), types.data() + types.size()));
}

llvm::PointerType *bitcode_context::get_opaque_pointer_t(void) const
{
	return llvm::PointerType::get(this->get_llvm_context(), 0);
}


bool bitcode_context::has_terminator(void) const
{
	auto const current_bb = this->builder.GetInsertBlock();
	return current_bb->size() != 0 && current_bb->back().isTerminator();
}

bool bitcode_context::has_terminator(llvm::BasicBlock *bb)
{
	return bb->size() != 0 && bb->back().isTerminator();
}

void bitcode_context::start_lifetime(llvm::Value *ptr, size_t size)
{
	auto const func = this->get_function(this->get_builtin_function(ast::function_body::lifetime_start));
	auto const size_val = llvm::ConstantInt::get(this->get_uint64_t(), size);
	auto const void_ptr = this->builder.CreatePointerCast(ptr, llvm::PointerType::getInt8PtrTy(this->get_llvm_context()));
	this->builder.CreateCall(func, { size_val, void_ptr });
}

void bitcode_context::end_lifetime(llvm::Value *ptr, size_t size)
{
	auto const func = this->get_function(this->get_builtin_function(ast::function_body::lifetime_end));
	auto const size_val = llvm::ConstantInt::get(this->get_uint64_t(), size);
	auto const void_ptr = this->builder.CreatePointerCast(ptr, llvm::PointerType::getInt8PtrTy(this->get_llvm_context()));
	this->builder.CreateCall(func, { size_val, void_ptr });
}

void bitcode_context::push_expression_scope(void)
{
	this->destructor_calls.emplace_back();
	this->end_lifetime_calls.emplace_back();
}

void bitcode_context::pop_expression_scope(void)
{
	if (!this->has_terminator())
	{
		this->emit_destructor_calls();
		this->emit_end_lifetime_calls();
	}
	this->destructor_calls.pop_back();
	this->end_lifetime_calls.pop_back();
}

void bitcode_context::push_destructor_call(llvm::Function *dtor_func, llvm::Value *ptr)
{
	bz_assert(!this->destructor_calls.empty());
	this->destructor_calls.back().push_back({ dtor_func, ptr });
}

void bitcode_context::emit_destructor_calls(void)
{
	bz_assert(!this->has_terminator());
	bz_assert(!this->destructor_calls.empty());
	for (auto const &[func, val] : this->destructor_calls.back().reversed())
	{
		this->builder.CreateCall(func, val);
	}
}

void bitcode_context::emit_loop_destructor_calls(void)
{
	bz_assert(!this->has_terminator());
	bz_assert(!this->destructor_calls.empty());
	for (auto const &scope_calls : this->destructor_calls.slice(this->loop_info.destructor_stack_begin).reversed())
	{
		for (auto const &[func, val] : scope_calls.reversed())
		{
			this->builder.CreateCall(func, val);
		}
	}
}

void bitcode_context::emit_all_destructor_calls(void)
{
	bz_assert(!this->has_terminator());
	bz_assert(!this->destructor_calls.empty());
	for (auto const &scope_calls : this->destructor_calls.reversed())
	{
		for (auto const &[func, val] : scope_calls.reversed())
		{
			this->builder.CreateCall(func, val);
		}
	}
}

void bitcode_context::push_end_lifetime_call(llvm::Value *ptr, size_t size)
{
	bz_assert(!this->end_lifetime_calls.empty());
	this->end_lifetime_calls.back().push_back({ ptr, size });
}

void bitcode_context::emit_end_lifetime_calls(void)
{
	bz_assert(!this->has_terminator());
	bz_assert(!this->end_lifetime_calls.empty());
	for (auto const &[ptr, size] : this->end_lifetime_calls.back().reversed())
	{
		this->end_lifetime(ptr, size);
	}
}

void bitcode_context::emit_loop_end_lifetime_calls(void)
{
	bz_assert(!this->has_terminator());
	bz_assert(!this->destructor_calls.empty());
	for (auto const &scope_calls : this->end_lifetime_calls.slice(this->loop_info.destructor_stack_begin).reversed())
	{
		for (auto const &[ptr, size] : scope_calls.reversed())
		{
			this->end_lifetime(ptr, size);
		}
	}
}

void bitcode_context::emit_all_end_lifetime_calls(void)
{
	bz_assert(!this->has_terminator());
	bz_assert(!this->end_lifetime_calls.empty());
	for (auto const &scope_calls : this->end_lifetime_calls.reversed())
	{
		for (auto const &[ptr, size] : scope_calls.reversed())
		{
			this->end_lifetime(ptr, size);
		}
	}
}

[[nodiscard]] bitcode_context::loop_info_t
bitcode_context::push_loop(llvm::BasicBlock *break_bb, llvm::BasicBlock *continue_bb) noexcept
{
	auto const result = this->loop_info;
	this->loop_info.break_bb = break_bb;
	this->loop_info.continue_bb = continue_bb;
	this->loop_info.destructor_stack_begin = this->destructor_calls.size();
	return result;
}

void bitcode_context::pop_loop(loop_info_t info) noexcept
{
	this->loop_info = info;
}

void bitcode_context::ensure_function_emission(ast::function_body *func)
{
	// no need to emit functions with no definition
	if (func->body.is_null())
	{
		return;
	}

	if (!func->is_bitcode_emitted())
	{
		this->functions_to_compile.push_back(func);
	}
}

void bitcode_context::report_error(
	lex::src_tokens src_tokens, bz::u8string message,
	bz::vector<source_highlight> notes,
	bz::vector<source_highlight> suggestions
) const
{
	this->global_ctx.report_error(error{
		warning_kind::_last,
		{
			src_tokens.pivot->src_pos.file_id, src_tokens.pivot->src_pos.line,
			src_tokens.begin->src_pos.begin, src_tokens.pivot->src_pos.begin, (src_tokens.end - 1)->src_pos.end,
			suggestion_range{}, suggestion_range{},
			std::move(message),
		},
		std::move(notes), std::move(suggestions)
	});
}

[[nodiscard]] source_highlight bitcode_context::make_note(lex::src_tokens src_tokens, bz::u8string message)
{
	return source_highlight{
		src_tokens.pivot->src_pos.file_id, src_tokens.pivot->src_pos.line,
		src_tokens.begin->src_pos.begin, src_tokens.pivot->src_pos.begin, (src_tokens.end - 1)->src_pos.end,
		{}, {},
		std::move(message)
	};
}

[[nodiscard]] source_highlight bitcode_context::make_note(bz::u8string message)
{
	return source_highlight{
		global_context::compiler_file_id, 0,
		char_pos(), char_pos(), char_pos(),
		{}, {},
		std::move(message)
	};
}

} // namespace ctx
