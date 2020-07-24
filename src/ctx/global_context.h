#ifndef CTX_GLOBAL_CONTEXT_H
#define CTX_GLOBAL_CONTEXT_H

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Value.h>
#include <llvm/Target/TargetMachine.h>

#include "core.h"

#include "global_data.h"
#include "lex/token.h"
#include "error.h"
#include "ast/typespec.h"
#include "ast/expression.h"
#include "ast/statement.h"

#include "decl_set.h"

namespace ctx
{

struct decl_list
{
	bz::vector<ast::decl_variable *> var_decls;
	bz::vector<ast::decl_function *> func_decls;
	bz::vector<ast::decl_operator *> op_decls;
	bz::vector<ast::type_info     *> type_infos;
};

struct global_context
{
	decl_set  _export_decls;
	decl_list _compile_decls;
	bz::vector<error> _errors;

	llvm::LLVMContext _llvm_context;
	llvm::Module _module;
	llvm::TargetMachine *_target_machine;
	std::array<llvm::Type *, static_cast<int>(ast::type_info::null_t_) + 1> _llvm_built_in_types;

	global_context(void);

	void report_error(error &&err)
	{
		bz_assert(err.kind == warning_kind::_last);
		this->_errors.emplace_back(std::move(err));
	}

	void report_warning(error &&err)
	{
		bz_assert(err.kind != warning_kind::_last);
		if (is_warning_enabled(err.kind))
		{
			this->_errors.emplace_back(std::move(err));
		}
	}

	bool has_errors(void) const;
	bool has_warnings(void) const;

	bool has_errors_or_warnings(void) const
	{ return !this->_errors.empty(); }

	void clear_errors_and_warnings(void)
	{ this->_errors.clear(); }

	bz::array_view<const error> get_errors_and_warnings(void) const
	{ return this->_errors; }

	size_t get_error_count(void) const;
	size_t get_warning_count(void) const;

	size_t get_error_and_warning_count(void) const
	{ return this->_errors.size(); }

	void add_export_declaration(ast::statement &decl);

	void add_export_variable(ast::decl_variable &var_decl);
	void add_compile_variable(ast::decl_variable &var_decl);

	void add_export_function(ast::decl_function &func_decl);
	void add_compile_function(ast::decl_function &func_decl);

	void add_export_operator(ast::decl_operator &op_decl);
	void add_compile_operator(ast::decl_operator &op_decl);

	void add_export_struct(ast::decl_struct &struct_decl);
	void add_compile_struct(ast::decl_struct &struct_decl);

	ast::type_info *get_base_type_info(uint32_t kind) const;

	llvm::DataLayout const &get_data_layout(void) const
	{ return this->_module.getDataLayout(); }
};

} // namespace ctx

#endif // CTX_GLOBAL_CONTEXT_H
