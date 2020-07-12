#include "bitcode_context.h"
#include "global_context.h"

namespace ctx
{

static std::array<llvm::Type *, static_cast<int>(ast::type_info::bool_) + 1>
get_built_in_types(global_context &context)
{
	return {
		llvm::Type::getInt8Ty(context._llvm_context),   // int8_
		llvm::Type::getInt16Ty(context._llvm_context),  // int16_
		llvm::Type::getInt32Ty(context._llvm_context),  // int32_
		llvm::Type::getInt64Ty(context._llvm_context),  // int64_
		llvm::Type::getInt8Ty(context._llvm_context),   // uint8_
		llvm::Type::getInt16Ty(context._llvm_context),  // uint16_
		llvm::Type::getInt32Ty(context._llvm_context),  // uint32_
		llvm::Type::getInt64Ty(context._llvm_context),  // uint64_
		llvm::Type::getFloatTy(context._llvm_context),  // float32_
		llvm::Type::getDoubleTy(context._llvm_context), // float64_
		llvm::Type::getInt32Ty(context._llvm_context),  // char_
		llvm::StructType::get(
			llvm::Type::getInt8PtrTy(context._llvm_context),
			llvm::Type::getInt8PtrTy(context._llvm_context)
		),                                              // str_
		llvm::Type::getInt1Ty(context._llvm_context),   // bool_
	};
}

bitcode_context::bitcode_context(global_context &_global_ctx)
	: global_ctx(_global_ctx),
	  vars_{},
	  funcs_{},
	  types_{},
	  current_function{ nullptr, nullptr },
	  builder(_global_ctx._llvm_context),
	  built_in_types(get_built_in_types(_global_ctx))
{}

llvm::Value *bitcode_context::get_variable(ast::decl_variable const *var_decl) const
{
	auto const it = this->vars_.find(var_decl);
	return it == this->vars_.end() ? nullptr : it->second;;
}

void bitcode_context::add_variable(ast::decl_variable const *var_decl, llvm::Value *val)
{
	this->vars_.insert_or_assign(var_decl, val);
}

llvm::Function *bitcode_context::get_function(ast::function_body const *func_body) const
{
	auto const it = this->funcs_.find(func_body);
	return it == this->funcs_.end() ? nullptr : it->second;
}

void bitcode_context::add_function(ast::function_body const *func_body, llvm::Function *fn)
{
	this->funcs_.insert_or_assign(func_body, fn);
}

llvm::LLVMContext &bitcode_context::get_llvm_context(void) const noexcept
{
	return this->global_ctx._llvm_context;
}

llvm::Module &bitcode_context::get_module(void) const noexcept
{
	return this->global_ctx._module;
}

llvm::BasicBlock *bitcode_context::add_basic_block(bz::u8string_view name)
{
	return llvm::BasicBlock::Create(
		this->global_ctx._llvm_context,
		llvm::StringRef(name.data(), name.length()),
		this->current_function.second
	);
}

llvm::Type *bitcode_context::get_built_in_type(uint32_t kind) const
{
	bz_assert(kind <= ast::type_info::bool_);
	return this->built_in_types[static_cast<int>(kind)];
}

llvm::Type *bitcode_context::get_int8_t(void) const
{ return this->built_in_types[static_cast<int>(ast::type_info::int8_)]; }

llvm::Type *bitcode_context::get_int16_t(void) const
{ return this->built_in_types[static_cast<int>(ast::type_info::int16_)]; }

llvm::Type *bitcode_context::get_int32_t(void) const
{ return this->built_in_types[static_cast<int>(ast::type_info::int32_)]; }

llvm::Type *bitcode_context::get_int64_t(void) const
{ return this->built_in_types[static_cast<int>(ast::type_info::int64_)]; }

llvm::Type *bitcode_context::get_uint8_t(void) const
{ return this->built_in_types[static_cast<int>(ast::type_info::uint8_)]; }

llvm::Type *bitcode_context::get_uint16_t(void) const
{ return this->built_in_types[static_cast<int>(ast::type_info::uint16_)]; }

llvm::Type *bitcode_context::get_uint32_t(void) const
{ return this->built_in_types[static_cast<int>(ast::type_info::uint32_)]; }

llvm::Type *bitcode_context::get_uint64_t(void) const
{ return this->built_in_types[static_cast<int>(ast::type_info::uint64_)]; }

llvm::Type *bitcode_context::get_float32_t(void) const
{ return this->built_in_types[static_cast<int>(ast::type_info::float32_)]; }

llvm::Type *bitcode_context::get_float64_t(void) const
{ return this->built_in_types[static_cast<int>(ast::type_info::float64_)]; }

llvm::Type *bitcode_context::get_str_t(void) const
{ return this->built_in_types[static_cast<int>(ast::type_info::str_)]; }

llvm::Type *bitcode_context::get_char_t(void) const
{ return this->built_in_types[static_cast<int>(ast::type_info::char_)]; }

llvm::Type *bitcode_context::get_bool_t(void) const
{ return this->built_in_types[static_cast<int>(ast::type_info::bool_)]; }


bool bitcode_context::has_terminator(void) const
{
	auto const current_bb = this->builder.GetInsertBlock();
	return current_bb->size() != 0 && current_bb->back().isTerminator();
}

bool bitcode_context::has_terminator(llvm::BasicBlock *bb)
{
	return bb->size() != 0 && bb->back().isTerminator();
}

} // namespace ctx
