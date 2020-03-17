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
	  current_function(nullptr)
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

} // namespace ctx
