#ifndef CODEGEN_LLVM_LATEST_BITCODE_CONTEXT_H
#define CODEGEN_LLVM_LATEST_BITCODE_CONTEXT_H

#include "core.h"

#include "ast/typespec.h"
#include "ast/expression.h"
#include "ast/statement.h"
#include "abi/platform_abi.h"
#include "abi/platform_function_call.h"
#include "val_ptr.h"
#include "common.h"
#include "backend_context.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/PassManager.h>
#include <unordered_map>
#include <optional>

namespace ctx
{

struct global_context;

}

namespace codegen::llvm_latest
{

struct bitcode_context
{
	bitcode_context(ctx::global_context &_global_ctx, backend_context &_codegen_ctx, llvm::Module *_module);

	ast::type_info *get_builtin_type_info(uint32_t kind);
	ast::function_body *get_builtin_function(uint32_t kind);

	value_and_type_pair get_variable(ast::decl_variable const *var_decl) const;
	void add_variable(ast::decl_variable const *var_decl, llvm::Value *val, llvm::Type *type);

	llvm::Type *get_base_type(ast::type_info const *info) const;
	void add_base_type(ast::type_info const *info, llvm::Type *type);

	llvm::Function *get_function(ast::function_body *func_body);

	llvm::LLVMContext &get_llvm_context(void) const noexcept;
	llvm::DataLayout const &get_data_layout(void) const noexcept;
	llvm::Module &get_module(void) const noexcept;
	abi::platform_abi get_platform_abi(void) const noexcept;

	size_t get_size(llvm::Type *t) const;
	size_t get_align(llvm::Type *t) const;
	size_t get_offset(llvm::Type *t, size_t elem) const;
	size_t get_register_size(void) const;

	abi::pass_kind get_pass_kind(ast::typespec_view ts) const;
	abi::pass_kind get_pass_kind(ast::typespec_view ts, llvm::Type *llvm_type) const;

	llvm::BasicBlock *add_basic_block(bz::u8string_view name);
	llvm::Value *create_alloca(llvm::Type *t);
	llvm::Value *create_alloca(llvm::Type *t, llvm::Value *init_val);
	llvm::Value *create_alloca(llvm::Type *t, size_t align);
	llvm::Value *create_alloca_without_lifetime_start(llvm::Type *t);
	llvm::Value *create_alloca_without_lifetime_start(llvm::Type *t, llvm::Value *init_val);
	llvm::Value *create_alloca_without_lifetime_start(llvm::Type *t, size_t align);
	llvm::Value *create_string(bz::u8string_view str);

	llvm::Value *create_bitcast(val_ptr val, llvm::Type *dest_type);
	llvm::Value *create_cast_to_int(val_ptr val);
	llvm::Value *create_load(llvm::Type *type, llvm::Value *ptr, bz::u8string_view name = "");
	llvm::Value *create_gep(llvm::Type *type, llvm::Value *ptr, uint64_t idx, bz::u8string_view name = "");
	llvm::Value *create_gep(llvm::Type *type, llvm::Value *ptr, uint64_t idx0, uint64_t idx1, bz::u8string_view name = "");
	llvm::Value *create_gep(llvm::Type *type, llvm::Value *ptr, llvm::Value *idx, bz::u8string_view name = "");
	llvm::Value *create_gep(llvm::Type *type, llvm::Value *ptr, bz::array_view<llvm::Value * const> indces, bz::u8string_view name = "");
	llvm::Value *create_struct_gep(llvm::Type *type, llvm::Value *ptr, uint64_t idx, bz::u8string_view name = "");
	llvm::Value *create_array_gep(llvm::Type *type, llvm::Value *ptr, llvm::Value *idx, bz::u8string_view name = "");
	llvm::CallInst *create_call(
		lex::src_tokens const &src_tokens,
		ast::function_body *func_body,
		llvm::Function *fn,
		llvm::ArrayRef<llvm::Value *> args = std::nullopt
	);
	llvm::CallInst *create_call(
		llvm::Function *fn,
		llvm::ArrayRef<llvm::Value *> args = std::nullopt
	);
	llvm::CallInst *create_call(
		llvm::FunctionCallee fn,
		llvm::CallingConv::ID calling_convention,
		llvm::ArrayRef<llvm::Value *> args = std::nullopt
	);

	val_ptr get_struct_element(val_ptr value, uint64_t idx);

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
	llvm::Type *get_usize_t(void) const;
	llvm::Type *get_isize_t(void) const;
	llvm::StructType *get_slice_t(void) const;
	llvm::StructType *get_tuple_t(bz::array_view<llvm::Type * const> types) const;
	llvm::PointerType *get_opaque_pointer_t(void) const;

	bool has_terminator(void) const;
	static bool has_terminator(llvm::BasicBlock *bb);

	void start_lifetime(llvm::Value *ptr, size_t size);
	void end_lifetime(llvm::Value *ptr, size_t size);

	struct expression_scope_info_t
	{
	};

	[[nodiscard]] expression_scope_info_t push_expression_scope(void);
	void pop_expression_scope(expression_scope_info_t prev_info);

	llvm::Value *add_move_destruct_indicator(ast::decl_variable const *decl);
	llvm::Value *get_move_destruct_indicator(ast::decl_variable const *decl) const;

	void push_destruct_operation(ast::destruct_operation const &destruct_op);
	void push_variable_destruct_operation(ast::destruct_operation const &destruct_op, llvm::Value *move_destruct_indicator = nullptr);
	void push_self_destruct_operation(ast::destruct_operation const &destruct_op, llvm::Value *ptr, llvm::Type *type);
	void push_rvalue_array_destruct_operation(ast::destruct_operation const &destruct_op, llvm::Value *ptr, llvm::Type *type, llvm::Value *rvalue_array_elem_ptr);
	void emit_destruct_operations(void);
	void emit_loop_destruct_operations(void);
	void emit_all_destruct_operations(void);

	void push_end_lifetime_call(llvm::Value *ptr, size_t size);
	void emit_end_lifetime_calls(void);
	void emit_loop_end_lifetime_calls(void);
	void emit_all_end_lifetime_calls(void);

	[[nodiscard]] val_ptr push_value_reference(val_ptr new_value);
	void pop_value_reference(val_ptr prev_value);
	val_ptr get_value_reference(size_t index);

	struct loop_info_t
	{
		llvm::BasicBlock *break_bb;
		llvm::BasicBlock *continue_bb;
		size_t destructor_stack_begin;
	};

	[[nodiscard]] loop_info_t push_loop(llvm::BasicBlock *break_bb, llvm::BasicBlock *continue_bb) noexcept;
	void pop_loop(loop_info_t info) noexcept;

	void ensure_function_emission(ast::function_body *func);


	void report_error(
		lex::src_tokens const &src_tokens, bz::u8string message,
		bz::vector<ctx::source_highlight> notes = {},
		bz::vector<ctx::source_highlight> suggestions = {}
	) const;
	[[nodiscard]] static ctx::source_highlight make_note(lex::src_tokens const &src_tokens, bz::u8string message);
	[[nodiscard]] static ctx::source_highlight make_note(bz::u8string message);


	ctx::global_context &global_ctx;
	backend_context &backend_ctx;
	llvm::Module *module;

	std::unordered_map<ast::decl_variable const *, llvm::Value *> move_destruct_indicators{};
	std::unordered_map<ast::decl_variable const *, value_and_type_pair> vars_{};
	std::unordered_map<ast::type_info     const *, llvm::Type     *> types_{};
	std::unordered_map<ast::function_body const *, llvm::Function *> funcs_{};

	bz::vector<ast::function_body *> functions_to_compile{};

	struct destruct_operation_info_t
	{
		ast::destruct_operation const *destruct_op;
		llvm::Value *ptr;
		llvm::Type *type;
		llvm::Value *condition;
		llvm::Value *move_destruct_indicator;
		llvm::Value *rvalue_array_elem_ptr;
	};

	struct end_lifetime_info_t
	{
		llvm::Value *ptr;
		size_t size;
	};

	bz::vector<bz::vector<destruct_operation_info_t>> destructor_calls{};
	bz::vector<bz::vector<end_lifetime_info_t>> end_lifetime_calls{};

	std::pair<ast::function_body const *, llvm::Function *> current_function = { nullptr, nullptr };
	llvm::BasicBlock *alloca_bb = nullptr;
	llvm::Value *output_pointer = nullptr;
	loop_info_t loop_info = {};
	bz::array<val_ptr, 4> current_value_references;
	size_t current_value_reference_stack_size = 0;

	llvm::IRBuilder<> builder;

	llvm::FunctionAnalysisManager *function_analysis_manager = nullptr;
	llvm::FunctionPassManager *function_pass_manager = nullptr;
};

} // namespace codegen::llvm_latest

#endif // CODEGEN_LLVM_LATEST_BITCODE_CONTEXT_H
