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
#include "src_file.h"

#include "decl_set.h"

namespace ctx
{

struct decl_list
{
	bz::vector<ast::decl_variable *> var_decls;
	bz::vector<ast::function_body *> funcs;
	bz::vector<ast::type_info     *> type_infos;
};

decl_set get_default_decls(void);

struct global_context
{
	static constexpr uint32_t compiler_file_id     = std::numeric_limits<uint32_t>::max();
	static constexpr uint32_t command_line_file_id = std::numeric_limits<uint32_t>::max() - 1;

	decl_list _compile_decls;
	bz::vector<error> _errors;

	bz::vector<src_file const *> src_files;

	llvm::LLVMContext _llvm_context;
	llvm::Module _module;
	llvm::TargetMachine *_target_machine;
	abi::platform_abi _platform_abi;
	std::array<llvm::Type *, static_cast<int>(ast::type_info::null_t_) + 1> _llvm_built_in_types;

	global_context(void);

	void report_error(error &&err)
	{
		bz_assert(err.is_error());
		this->_errors.emplace_back(std::move(err));
	}

	void report_warning(error &&err)
	{
		bz_assert(err.is_warning());
		if (is_warning_enabled(err.kind))
		{
			this->_errors.emplace_back(std::move(err));
		}
	}

	bz::u8string_view get_file_name(uint32_t file_id) const noexcept
	{
		if (file_id == command_line_file_id)
		{
			return "<command-line>";
		}
		bz_assert(file_id < this->src_files.size());
		return this->src_files[file_id]->get_file_name();
	}

	char_pos get_file_begin(uint32_t file_id) const noexcept
	{
		if (file_id == command_line_file_id)
		{
			return char_pos();
		}
		bz_assert(file_id < this->src_files.size());
		return this->src_files[file_id]->_file.begin();
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

	void add_compile_variable(ast::decl_variable &var_decl);
	void add_compile_function(ast::function_body &func_body);
	void add_compile_struct(ast::decl_struct &struct_decl);


	ast::type_info *get_base_type_info(uint32_t kind) const;

	llvm::DataLayout const &get_data_layout(void) const
	{ return this->_module.getDataLayout(); }
};

} // namespace ctx

#endif // CTX_GLOBAL_CONTEXT_H
