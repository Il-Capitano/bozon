#ifndef CTX_BITCODE_CONTEXT_H
#define CTX_BITCODE_CONTEXT_H

#include "core.h"

#include "ast/typespec.h"
#include "ast/expression.h"
#include "ast/statement.h"

#include <llvm/IR/IRBuilder.h>

namespace ctx
{

struct global_context;

struct bitcode_context
{
	bitcode_context(global_context &_global_ctx);

	llvm::Value *get_variable_val(ast::decl_variable const *var_decl) const;

	global_context &global_ctx;
	bz::vector<std::pair<ast::decl_variable const *, llvm::Value *>> var_ids;
	llvm::LLVMContext llvm_context;
	llvm::IRBuilder<> builder;
	llvm::Module module;
};

} // namespace ctx

#endif // CTX_BITCODE_CONTEXT_H
