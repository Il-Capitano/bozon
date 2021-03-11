#ifndef CTX_COMPTIME_EXECUTOR_H
#define CTX_COMPTIME_EXECUTOR_H

#include "ast/constant_value.h"
#include "ast/expression.h"
#include "ast/statement.h"
#include "abi/platform_abi.h"
#include "bc/val_ptr.h"

#include <llvm/IR/Module.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/IR/LegacyPassManager.h>

namespace ctx
{

struct global_context;
struct parse_context;

struct comptime_executor_context
{
	comptime_executor_context(global_context &_global_ctx);

	comptime_executor_context(comptime_executor_context const &) = delete;
	comptime_executor_context(comptime_executor_context &&)      = delete;
	comptime_executor_context &operator = (comptime_executor_context const &) = delete;
	comptime_executor_context &operator = (comptime_executor_context &&)      = delete;

	~comptime_executor_context(void);

	ast::type_info *get_builtin_type_info(uint32_t kind);
	ast::typespec_view get_builtin_type(bz::u8string_view name);
	ast::function_body *get_builtin_function(uint32_t kind);

	llvm::Value *get_variable(ast::decl_variable const *var_decl) const;
	void add_variable(ast::decl_variable const *var_decl, llvm::Value *val);

	llvm::Type *get_base_type(ast::type_info *info);
	void add_base_type(ast::type_info const *info, llvm::Type *type);

	llvm::Function *get_function(ast::function_body *func_body);

	llvm::LLVMContext &get_llvm_context(void) const noexcept;
	llvm::DataLayout &get_data_layout(void) const noexcept;
	llvm::Module &get_module(void) const noexcept;
	abi::platform_abi get_platform_abi(void) const noexcept;

	size_t get_size(ast::typespec_view ts);
	size_t get_align(ast::typespec_view ts);

	size_t get_size(llvm::Type *t) const;
	size_t get_align(llvm::Type *t) const;
	size_t get_offset(llvm::Type *t, size_t elem) const;
	size_t get_register_size(void) const;

	llvm::BasicBlock *add_basic_block(bz::u8string_view name);
	llvm::Value *create_alloca(llvm::Type *t);
	llvm::Value *create_alloca(llvm::Type *t, size_t align);
	llvm::Value *create_string(bz::u8string_view str);

	llvm::Value *create_bitcast(bc::val_ptr val, llvm::Type *dest_type);
	llvm::Value *create_cast_to_int(bc::val_ptr val);

	llvm::Type *get_builtin_type(uint32_t kind) const;
	llvm::Type *get_int8_t(void) const;
	llvm::Type *get_int16_t(void) const;
	llvm::Type *get_int32_t(void) const;
	llvm::Type *get_int64_t(void) const;
	llvm::Type *get_uint8_t(void) const;
	llvm::Type *get_uint16_t(void) const;
	llvm::Type *get_uint32_t(void) const;
	llvm::Type *get_uint64_t(void) const;
	llvm::Type *get_float32_t(void) const;
	llvm::Type *get_float64_t(void) const;
	llvm::Type *get_str_t(void) const;
	llvm::Type *get_char_t(void) const;
	llvm::Type *get_bool_t(void) const;
	llvm::Type *get_null_t(void) const;
	llvm::StructType *get_slice_t(llvm::Type *elem_type) const;

	bool has_terminator(void) const;
	static bool has_terminator(llvm::BasicBlock *bb);

	void ensure_function_emission(ast::function_body *func);
	[[nodiscard]] bool resolve_function(ast::function_body *body);


	std::pair<ast::constant_value, bz::vector<error>> execute_function(
		lex::src_tokens src_tokens,
		ast::function_body *body,
		bz::array_view<ast::constant_value const> params
	);
	std::pair<ast::constant_value, bz::vector<error>> execute_compound_expression(ast::expr_compound &expr);
	void initialize_engine(void);
	std::unique_ptr<llvm::ExecutionEngine> create_engine(std::unique_ptr<llvm::Module> module);
	void add_base_functions_to_module(llvm::Module &module);
	void add_module(std::unique_ptr<llvm::Module> module);

	bool has_error(void);
	bz::vector<std::pair<warning_kind, source_highlight const *>> consume_errors(void);

	source_highlight const *insert_error(lex::src_tokens src_tokens, bz::u8string message);

	[[nodiscard]] static error make_error(
		bz::u8string message,
		bz::vector<source_highlight> notes = {}, bz::vector<source_highlight> suggestions = {}
	);
	[[nodiscard]] static error make_error(
		lex::src_tokens src_tokens, bz::u8string message,
		bz::vector<source_highlight> notes = {}, bz::vector<source_highlight> suggestions = {}
	);


	global_context &global_ctx;
	parse_context *current_parse_ctx;
	llvm::Module *current_module;

	std::unordered_map<ast::decl_variable const *, llvm::Value    *> vars_;
	std::unordered_map<ast::type_info     const *, llvm::Type     *> types_;
	std::unordered_map<ast::function_body const *, llvm::Function *> funcs_;

	bz::vector<ast::function_body *> functions_to_compile;

	std::pair<ast::function_body const *, llvm::Function *> current_function;
	llvm::BasicBlock *error_bb;
	llvm::BasicBlock *alloca_bb;
	llvm::Value *output_pointer;
	llvm::IRBuilder<> builder;

	std::list<source_highlight> execution_errors;  // a list is used, so pointers to the errors are stable
	ast::decl_variable *errors_array;
	std::pair<ast::function_body *, llvm::Function *> free_errors_func;
	std::pair<ast::function_body *, llvm::Function *> get_error_count_func;
	std::pair<ast::function_body *, llvm::Function *> get_error_kind_by_index_func;
	std::pair<ast::function_body *, llvm::Function *> get_error_ptr_by_index_func;
	std::pair<ast::function_body *, llvm::Function *> has_errors_func;
	std::pair<ast::function_body *, llvm::Function *> add_error_func;
	std::pair<ast::function_body *, llvm::Function *> clear_errors_func;

	llvm::TargetMachine *target_machine;
	llvm::legacy::PassManager pass_manager;
	std::unique_ptr<llvm::ExecutionEngine> engine;
};

} // namespace ctx

#endif // CTX_COMPTIME_EXECUTOR_H
