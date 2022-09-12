#ifndef CTX_GLOBAL_CONTEXT_H
#define CTX_GLOBAL_CONTEXT_H

#include "core.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Value.h>
#include <llvm/Target/TargetMachine.h>

#include "global_data.h"
#include "lex/token.h"
#include "error.h"
#include "ast/typespec.h"
#include "ast/expression.h"
#include "ast/statement.h"
#include "ast/scope.h"
#include "src_file.h"
#include "abi/platform_abi.h"
#include "resolve/attribute_resolver.h"

#include "comptime_executor.h"

namespace ctx
{

struct decl_list
{
	bz::vector<ast::decl_variable *> var_decls;
	bz::vector<ast::function_body *> funcs;
	bz::vector<ast::type_info     *> type_infos;
};

ast::scope_t get_default_decls(ast::scope_t *builtin_global_scope, bz::array_view<bz::u8string_view const> id_scope);
llvm::PassManagerBuilder create_pass_manager_builder(void);

struct global_context
{
	static constexpr uint32_t compiler_file_id     = std::numeric_limits<uint32_t>::max();
	static constexpr uint32_t command_line_file_id = std::numeric_limits<uint32_t>::max() - 1;

	decl_list         _compile_decls;
	bz::vector<error> _errors;

	bz::vector<ast::type_info>              _builtin_type_infos;
	bz::vector<ast::type_and_name_pair>     _builtin_types;
	bz::vector<ast::universal_function_set> _builtin_universal_functions;
	bz::vector<resolve::attribute_info_t>   _builtin_attributes;

	bz::vector<ast::decl_function *> _builtin_functions;
	bz::vector<ast::decl_operator *> _builtin_operators;

	ast::scope_t *_builtin_global_scope;

	ast::function_body *_comptime_compile_error_src_tokens_func   = nullptr;
	ast::function_body *_comptime_compile_warning_src_tokens_func = nullptr;
	ast::function_body *_comptime_create_global_string_func       = nullptr;

	std::list<src_file> _src_files;

	llvm::LLVMContext _llvm_context;
	llvm::Module      _module;
	llvm::Target const *_target;
	std::unique_ptr<llvm::TargetMachine> _target_machine;
	bz::optional<llvm::DataLayout>       _data_layout;
	bz::array<llvm::Type *, static_cast<int>(ast::type_info::null_t_) + 1> _llvm_builtin_types;
	abi::platform_abi _platform_abi;

	comptime_executor_context _comptime_executor;

	global_context(void);
	global_context(global_context const &) = delete;
	global_context(global_context &&)      = delete;
	global_context &operator = (global_context const &) = delete;
	global_context &operator = (global_context &&)      = delete;

	ast::type_info *get_builtin_type_info(uint32_t kind);
	ast::type_info *get_usize_type_info(void) const;
	ast::type_info *get_isize_type_info(void) const;
	ast::typespec_view get_builtin_type(bz::u8string_view name);
	ast::decl_function *get_builtin_function(uint32_t kind);
	bz::array_view<uint32_t const> get_builtin_universal_functions(bz::u8string_view id);
	resolve::attribute_info_t *get_builtin_attribute(bz::u8string_view name);

	void report_error_or_warning(error &&err);

	void report_error(error &&err);
	void report_error(bz::u8string message, bz::vector<source_highlight> notes = {}, bz::vector<source_highlight> suggestions = {});
	void report_error(
		lex::src_tokens const &src_tokens, bz::u8string message,
		bz::vector<source_highlight> notes = {},
		bz::vector<source_highlight> suggestions = {}
	);

	void report_warning(error &&err);
	void report_warning(warning_kind kind, bz::u8string message);

	[[nodiscard]] static source_highlight make_note(bz::u8string message);

	[[nodiscard]] static source_highlight make_note(lex::src_tokens const &src_tokens, bz::u8string message);

	src_file &get_src_file(uint32_t file_id) noexcept;
	src_file const &get_src_file(uint32_t file_id) const noexcept;

	char_pos get_file_begin(uint32_t file_id) const noexcept;

	std::pair<char_pos, char_pos> get_file_begin_and_end(uint32_t file_id) const noexcept;

	bool is_library_file(uint32_t file_id) const noexcept;

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

	uint32_t add_module(uint32_t current_file_id, ast::identifier const &id);
	ast::scope_t *get_file_export_decls(uint32_t file_id);

	bz::u8string get_file_name(uint32_t file_id);
	bz::u8string get_location_string(lex::token_pos t);

	bool add_comptime_checking_function(bz::u8string_view kind, ast::function_body *func_body);
	bool add_comptime_checking_variable(bz::u8string_view kind, ast::decl_variable *var_decl);
	bool add_builtin_function(ast::decl_function *func_decl);
	bool add_builtin_operator(ast::decl_operator *op_decl);

	bool is_aggressive_consteval_enabled(void) const;


	llvm::DataLayout const &get_data_layout(void) const
	{ return this->_module.getDataLayout(); }

	void report_and_clear_errors_and_warnings(void);

	[[nodiscard]] bool parse_command_line(int argc, char const * const*argv);
	[[nodiscard]] bool initialize_llvm(void);
	[[nodiscard]] bool initialize_builtins(void);
	[[nodiscard]] bool parse_global_symbols(void);
	[[nodiscard]] bool parse(void);
	[[nodiscard]] bool emit_bitcode(void);
	[[nodiscard]] bool optimize(void);
	[[nodiscard]] bool emit_file(void);

	bool emit_obj(void);
	bool emit_asm(void);
	bool emit_llvm_bc(void);
	bool emit_llvm_ir(void);
};

} // namespace ctx

#endif // CTX_GLOBAL_CONTEXT_H
