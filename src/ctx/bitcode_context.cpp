#include "bitcode_context.h"
#include "global_context.h"

namespace ctx
{

bitcode_context::bitcode_context(global_context &_global_ctx, llvm::Module &_module)
	: global_ctx(_global_ctx),
	  module(_module),
	  vars_{},
	  types_{},
	  functions_to_compile{},
	  current_function{ nullptr, nullptr },
	  alloca_bb(nullptr),
	  output_pointer(nullptr),
	  builder(_global_ctx._llvm_context)
{}

llvm::Value *bitcode_context::get_variable(ast::decl_variable const *var_decl) const
{
	auto const it = this->vars_.find(var_decl);
	return it == this->vars_.end() ? nullptr : it->second;
}

void bitcode_context::add_variable(ast::decl_variable const *var_decl, llvm::Value *val)
{
	this->vars_.insert_or_assign(var_decl, val);
}

llvm::Function *bitcode_context::get_function(ast::function_body const *func_body)
{
	auto it = this->funcs_.find(func_body);
	if (it == this->funcs_.end())
	{
		return nullptr;
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

llvm::Module &bitcode_context::get_module(void) const noexcept
{
	return this->module;
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
#if LLVM_VERSION_MAJOR >= 11
	return this->global_ctx._data_layout->getPrefTypeAlign(t).value();
#else
	return this->global_ctx._data_layout->getPrefTypeAlignment(t);
#endif // llvm 11
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
		return this->global_ctx._data_layout->getLargestLegalIntTypeSizeInBits() / 8;
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
	return result;
}

llvm::Value *bitcode_context::create_alloca(llvm::Type *t, size_t align)
{
	auto const bb = this->builder.GetInsertBlock();
	this->builder.SetInsertPoint(this->alloca_bb);
	auto const result = this->builder.CreateAlloca(t);
	result->setAlignment(llvm::Align(align));
	this->builder.SetInsertPoint(bb);
	return result;
}

llvm::Value *bitcode_context::create_bitcast(bc::val_ptr val, llvm::Type *dest_type)
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

llvm::Type *bitcode_context::get_built_in_type(uint32_t kind) const
{
	bz_assert(kind <= ast::type_info::null_t_);
	return this->global_ctx._llvm_built_in_types[kind];
}

llvm::Type *bitcode_context::get_int8_t(void) const
{ return this->global_ctx._llvm_built_in_types[static_cast<int>(ast::type_info::int8_)]; }

llvm::Type *bitcode_context::get_int16_t(void) const
{ return this->global_ctx._llvm_built_in_types[static_cast<int>(ast::type_info::int16_)]; }

llvm::Type *bitcode_context::get_int32_t(void) const
{ return this->global_ctx._llvm_built_in_types[static_cast<int>(ast::type_info::int32_)]; }

llvm::Type *bitcode_context::get_int64_t(void) const
{ return this->global_ctx._llvm_built_in_types[static_cast<int>(ast::type_info::int64_)]; }

llvm::Type *bitcode_context::get_uint8_t(void) const
{ return this->global_ctx._llvm_built_in_types[static_cast<int>(ast::type_info::uint8_)]; }

llvm::Type *bitcode_context::get_uint16_t(void) const
{ return this->global_ctx._llvm_built_in_types[static_cast<int>(ast::type_info::uint16_)]; }

llvm::Type *bitcode_context::get_uint32_t(void) const
{ return this->global_ctx._llvm_built_in_types[static_cast<int>(ast::type_info::uint32_)]; }

llvm::Type *bitcode_context::get_uint64_t(void) const
{ return this->global_ctx._llvm_built_in_types[static_cast<int>(ast::type_info::uint64_)]; }

llvm::Type *bitcode_context::get_float32_t(void) const
{ return this->global_ctx._llvm_built_in_types[static_cast<int>(ast::type_info::float32_)]; }

llvm::Type *bitcode_context::get_float64_t(void) const
{ return this->global_ctx._llvm_built_in_types[static_cast<int>(ast::type_info::float64_)]; }

llvm::Type *bitcode_context::get_str_t(void) const
{ return this->global_ctx._llvm_built_in_types[static_cast<int>(ast::type_info::str_)]; }

llvm::Type *bitcode_context::get_char_t(void) const
{ return this->global_ctx._llvm_built_in_types[static_cast<int>(ast::type_info::char_)]; }

llvm::Type *bitcode_context::get_bool_t(void) const
{ return this->global_ctx._llvm_built_in_types[static_cast<int>(ast::type_info::bool_)]; }

llvm::Type *bitcode_context::get_null_t(void) const
{ return this->global_ctx._llvm_built_in_types[static_cast<int>(ast::type_info::null_t_)]; }

ast::function_body *bitcode_context::get_builtin_function(uint32_t kind) const
{ return this->global_ctx.get_builtin_function(kind); }


bool bitcode_context::has_terminator(void) const
{
	auto const current_bb = this->builder.GetInsertBlock();
	return current_bb->size() != 0 && current_bb->back().isTerminator();
}

bool bitcode_context::has_terminator(llvm::BasicBlock *bb)
{
	return bb->size() != 0 && bb->back().isTerminator();
}

void bitcode_context::ensure_function_emission(ast::function_body const *func)
{
	// no need to emit functions with no definition
	if (func->body.is_null())
	{
		return;
	}

	auto const it = std::find(this->functions_to_compile.begin(), this->functions_to_compile.end(), func);
	if (it == this->functions_to_compile.end())
	{
		this->functions_to_compile.push_back(func);
	}
}

} // namespace ctx
