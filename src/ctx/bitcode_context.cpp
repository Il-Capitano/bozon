#include "bitcode_context.h"
#include "global_context.h"

namespace ctx
{

bitcode_context::bitcode_context(global_context &_global_ctx)
	: global_ctx(_global_ctx),
	  var_ids{},
	  llvm_context(),
	  builder(this->llvm_context),
	  module("test", this->llvm_context)
{}

llvm::Value *bitcode_context::get_variable_val(ast::decl_variable const *var_decl) const
{
	auto const it = std::find_if(
		this->var_ids.begin(), this->var_ids.end(),
		[var_decl](auto const decl_ptr) {
			return var_decl == decl_ptr.first;
		}
	);
	assert(it != this->var_ids.end());
	return it->second;
}

} // namespace ctx
