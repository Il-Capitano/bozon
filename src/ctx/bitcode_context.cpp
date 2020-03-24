#include "bitcode_context.h"
#include "global_context.h"

namespace ctx
{

bitcode_context::bitcode_context(global_context &_global_ctx)
	: global_ctx(_global_ctx),
	  vars{},
	  llvm_context(),
	  builder(this->llvm_context),
	  module("test", this->llvm_context),
	  current_function(nullptr),
	  built_in_types{
		  llvm::Type::getInt8Ty(this->llvm_context),   // int8_
		  llvm::Type::getInt16Ty(this->llvm_context),  // int16_
		  llvm::Type::getInt32Ty(this->llvm_context),  // int32_
		  llvm::Type::getInt64Ty(this->llvm_context),  // int64_
		  llvm::Type::getInt8Ty(this->llvm_context),   // uint8_
		  llvm::Type::getInt16Ty(this->llvm_context),  // uint16_
		  llvm::Type::getInt32Ty(this->llvm_context),  // uint32_
		  llvm::Type::getInt64Ty(this->llvm_context),  // uint64_
		  llvm::Type::getFloatTy(this->llvm_context),  // float32_
		  llvm::Type::getDoubleTy(this->llvm_context), // float64_
		  llvm::Type::getInt32Ty(this->llvm_context),  // char_
		  llvm::StructType::get(
			  llvm::Type::getInt8PtrTy(this->llvm_context),
			  llvm::Type::getInt8PtrTy(this->llvm_context)
		  ),                                           // str_
		  llvm::Type::getInt1Ty(this->llvm_context),   // bool_
	  }
{}

llvm::Value *bitcode_context::get_variable_val(ast::decl_variable const *var_decl) const
{
	auto const it = std::find_if(
		this->vars.begin(), this->vars.end(),
		[var_decl](auto const decl_ptr) {
			return var_decl == decl_ptr.first;
		}
	);
	assert(it != this->vars.end());
	return it->second;
}

llvm::BasicBlock *bitcode_context::add_basic_block(bz::string_view name)
{
	return llvm::BasicBlock::Create(
		this->llvm_context,
		llvm::StringRef(name.data(), name.length()),
		this->current_function
	);
}

llvm::Type *bitcode_context::get_built_in_type(ast::type_info::type_kind kind) const
{
	assert(kind <= ast::type_info::type_kind::bool_);
	return this->built_in_types[static_cast<int>(kind)];
}

llvm::Type *bitcode_context::get_int8_t(void) const
{ return this->built_in_types[static_cast<int>(ast::type_info::type_kind::int8_)]; }

llvm::Type *bitcode_context::get_int16_t(void) const
{ return this->built_in_types[static_cast<int>(ast::type_info::type_kind::int16_)]; }

llvm::Type *bitcode_context::get_int32_t(void) const
{ return this->built_in_types[static_cast<int>(ast::type_info::type_kind::int32_)]; }

llvm::Type *bitcode_context::get_int64_t(void) const
{ return this->built_in_types[static_cast<int>(ast::type_info::type_kind::int64_)]; }

llvm::Type *bitcode_context::get_uint8_t(void) const
{ return this->built_in_types[static_cast<int>(ast::type_info::type_kind::uint8_)]; }

llvm::Type *bitcode_context::get_uint16_t(void) const
{ return this->built_in_types[static_cast<int>(ast::type_info::type_kind::uint16_)]; }

llvm::Type *bitcode_context::get_uint32_t(void) const
{ return this->built_in_types[static_cast<int>(ast::type_info::type_kind::uint32_)]; }

llvm::Type *bitcode_context::get_uint64_t(void) const
{ return this->built_in_types[static_cast<int>(ast::type_info::type_kind::uint64_)]; }

llvm::Type *bitcode_context::get_float32_t(void) const
{ return this->built_in_types[static_cast<int>(ast::type_info::type_kind::float32_)]; }

llvm::Type *bitcode_context::get_float64_t(void) const
{ return this->built_in_types[static_cast<int>(ast::type_info::type_kind::float64_)]; }

llvm::Type *bitcode_context::get_str_t(void) const
{ return this->built_in_types[static_cast<int>(ast::type_info::type_kind::str_)]; }

llvm::Type *bitcode_context::get_char_t(void) const
{ return this->built_in_types[static_cast<int>(ast::type_info::type_kind::char_)]; }

llvm::Type *bitcode_context::get_bool_t(void) const
{ return this->built_in_types[static_cast<int>(ast::type_info::type_kind::bool_)]; }


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
