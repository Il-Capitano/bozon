#ifndef CTX_COMPTIME_EXECUTOR_H
#define CTX_COMPTIME_EXECUTOR_H

#include "ast/constant_value.h"
#include "ast/expression.h"
#include "ast/statement.h"
#include "abi/platform_abi.h"
#include "bc/val_ptr.h"

#include <llvm/IR/Module.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>

namespace ctx
{

struct global_context;
struct parse_context;

struct comptime_executor_context
{
	comptime_executor_context(parse_context &_parse_ctx);

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


	std::pair<ast::constant_value, bz::vector<error>> execute_function(
		lex::src_tokens src_tokens,
		ast::function_body *body,
		bz::array_view<ast::constant_value const> params
	);
	std::unique_ptr<llvm::ExecutionEngine> create_engine(std::unique_ptr<llvm::Module> module);
	void add_base_functions_to_module(llvm::Module &module);
	void add_module(std::unique_ptr<llvm::Module> module);

	static error make_error(
		lex::src_tokens src_tokens, bz::u8string message,
		bz::vector<note> notes = {}, bz::vector<suggestion> suggestions = {}
	);


	parse_context &parse_ctx;
	llvm::Module *current_module;

	std::unordered_map<ast::decl_variable const *, llvm::Value    *> vars_;
	std::unordered_map<ast::type_info     const *, llvm::Type     *> types_;
	std::unordered_map<ast::function_body const *, llvm::Function *> funcs_;

	bz::vector<ast::function_body *> functions_to_compile;

	std::pair<ast::function_body const *, llvm::Function *> current_function;
	llvm::BasicBlock *alloca_bb;
	llvm::Value *output_pointer;
	llvm::IRBuilder<> builder;

	llvm::Function *error_string_ptr_getter;
	llvm::Function *error_token_begin_getter;
	llvm::Function *error_token_pivot_getter;
	llvm::Function *error_token_end_getter;

	std::unique_ptr<llvm::ExecutionEngine> engine;
};

} // namespace ctx

#endif // CTX_COMPTIME_EXECUTOR_H
