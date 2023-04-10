#ifndef CTX_GLOBAL_CONTEXT_H
#define CTX_GLOBAL_CONTEXT_H

#include "core.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Value.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/Target/TargetMachine.h>

#include "global_data.h"
#include "lex/token.h"
#include "error.h"
#include "ast/typespec.h"
#include "ast/expression.h"
#include "ast/statement.h"
#include "ast/scope.h"
#include "ast/type_prototype.h"
#include "src_file.h"
#include "abi/platform_abi.h"
#include "resolve/attribute_resolver.h"
#include "comptime/codegen_context_forward.h"

namespace ctx
{

struct decl_list
{
	bz::vector<ast::decl_variable *> var_decls;
	bz::vector<ast::function_body *> funcs;
	bz::vector<ast::type_info     *> type_infos;
};

ast::scope_t get_default_decls(ast::scope_t *builtin_global_scope);

struct global_context
{
	static constexpr uint32_t compiler_file_id     = std::numeric_limits<uint32_t>::max();
	static constexpr uint32_t command_line_file_id = std::numeric_limits<uint32_t>::max() - 1;

	decl_list         _compile_decls;
	bz::vector<error> _errors;

	bz::vector<ast::universal_function_set> _builtin_universal_functions;
	bz::vector<resolve::attribute_info_t>   _builtin_attributes;

	bz::vector<ast::type_info *>     _builtin_type_infos;
	ast::decl_type_alias           * _builtin_isize_type_alias = nullptr;
	ast::decl_type_alias           * _builtin_usize_type_alias = nullptr;
	bz::vector<ast::decl_function *> _builtin_functions;
	bz::vector<ast::decl_operator *> _builtin_operators;

	ast::scope_t *_builtin_global_scope;

	ast::function_body *_main = nullptr;

	bz::vector<fs::path> _import_dirs;
	bz::vector<std::unique_ptr<src_file>> _src_files;
	std::unordered_map<fs::path, src_file *> _src_files_map;

	bz::vector<std::unique_ptr<char[]>> _src_scope_fragments;
	bz::vector<bz::vector<bz::u8string_view>> _src_scopes_storage;

	std::unique_ptr<ast::type_prototype_set_t> type_prototype_set = nullptr;
	llvm::LLVMContext _llvm_context;
	llvm::Module      _module;
	llvm::Target const *_target;
	std::unique_ptr<llvm::TargetMachine> _target_machine;
	bz::optional<llvm::DataLayout>       _data_layout;
	bz::array<llvm::Type *, static_cast<int>(ast::type_info::null_t_) + 1> _llvm_builtin_types;
	abi::platform_abi _platform_abi;

	std::unique_ptr<comptime::codegen_context> comptime_codegen_context;

	global_context(void);
	global_context(global_context const &) = delete;
	global_context(global_context &&)      = delete;
	global_context &operator = (global_context const &) = delete;
	global_context &operator = (global_context &&)      = delete;
	~global_context(void) noexcept;

	template<typename ...Args>
	src_file &emplace_src_file(Args &&...args)
	{
		auto &file = *this->_src_files.push_back(std::make_unique<src_file>(std::forward<Args>(args)...));
		this->_src_files_map.insert({ file.get_file_path(), &file });
		return file;
	}

	src_file *get_src_file(fs::path const &file_path);

	bz::array_view<bz::u8string_view const> get_scope_in_persistent_storage(bz::array_view<bz::u8string const> scope);

	ast::type_info *get_builtin_type_info(uint32_t kind) const;
	ast::type_info *get_usize_type_info(void) const;
	ast::type_info *get_isize_type_info(void) const;
	ast::decl_function *get_builtin_function(uint32_t kind);
	bz::array_view<uint32_t const> get_builtin_universal_functions(bz::u8string_view id);
	resolve::attribute_info_t *get_builtin_attribute(bz::u8string_view name);
	size_t get_sizeof(ast::typespec_view ts);
	size_t get_alignof(ast::typespec_view ts);

	comptime::codegen_context &get_codegen_context(void);

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

	struct module_info_t
	{
		uint32_t id;
		bz::array_view<bz::u8string_view const> scope;
	};

	bz::vector<module_info_t> add_module(uint32_t current_file_id, ast::identifier const &id);
	ast::scope_t &get_file_global_scope(uint32_t file_id);

	bz::u8string get_file_name(uint32_t file_id);
	bz::u8string get_location_string(lex::token_pos t);

	bool add_builtin_function(ast::decl_function *func_decl);
	bool add_builtin_operator(ast::decl_operator *op_decl);
	bool add_builtin_type_alias(ast::decl_type_alias *alias_decl);
	bool add_builtin_type_info(ast::type_info *info);
	ast::type_info *get_usize_type_info_for_builtin_alias(void) const;
	ast::type_info *get_isize_type_info_for_builtin_alias(void) const;

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
