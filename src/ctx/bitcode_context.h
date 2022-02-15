#ifndef CTX_BITCODE_CONTEXT_H
#define CTX_BITCODE_CONTEXT_H

#include "core.h"

#include "ast/typespec.h"
#include "ast/expression.h"
#include "ast/statement.h"
#include "abi/platform_abi.h"
#include "abi/platform_function_call.h"
#include "bc/val_ptr.h"
#include "bc/common.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Type.h>
#include <unordered_map>

namespace ctx
{

struct global_context;

struct bitcode_context
{
	bitcode_context(global_context &_global_ctx, llvm::Module *_module);

	ast::type_info *get_builtin_type_info(uint32_t kind);
	ast::typespec_view get_builtin_type(bz::u8string_view name);
	ast::function_body *get_builtin_function(uint32_t kind);

	llvm::Value *get_variable(ast::decl_variable const *var_decl) const;
	void add_variable(ast::decl_variable const *var_decl, llvm::Value *val);

	llvm::Type *get_base_type(ast::type_info const *info) const;
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

	template<abi::platform_abi abi>
	abi::pass_kind get_pass_kind(ast::typespec_view ts) const
	{
		if (ast::is_non_trivial(ts))
		{
			return abi::pass_kind::non_trivial;
		}
		else
		{
			auto const llvm_type = bc::get_llvm_type(ts, *this);
			return abi::get_pass_kind<abi>(llvm_type, this->get_data_layout(), this->get_llvm_context());
		}
	}

	template<abi::platform_abi abi>
	abi::pass_kind get_pass_kind(ast::typespec_view ts, llvm::Type *llvm_type) const
	{
		if (ast::is_non_trivial(ts))
		{
			return abi::pass_kind::non_trivial;
		}
		else
		{
			return abi::get_pass_kind<abi>(llvm_type, this->get_data_layout(), this->get_llvm_context());
		}
	}

	llvm::BasicBlock *add_basic_block(bz::u8string_view name);
	llvm::Value *create_alloca(llvm::Type *t);
	llvm::Value *create_alloca(llvm::Type *t, size_t align);
	llvm::Value *create_alloca_without_lifetime_start(llvm::Type *t);
	llvm::Value *create_alloca_without_lifetime_start(llvm::Type *t, size_t align);
	llvm::Value *create_string(bz::u8string_view str);

	llvm::Value *create_bitcast(bc::val_ptr val, llvm::Type *dest_type);
	llvm::Value *create_cast_to_int(bc::val_ptr val);
	llvm::Value *create_load(llvm::Value *ptr, bz::u8string_view name = "");
	llvm::Value *create_gep(llvm::Value *ptr, uint64_t idx, bz::u8string_view name = "");
	llvm::Value *create_gep(llvm::Value *ptr, uint64_t idx0, uint64_t idx1, bz::u8string_view name = "");
	llvm::Value *create_gep(llvm::Value *ptr, llvm::Value *idx, bz::u8string_view name = "");
	llvm::Value *create_gep(llvm::Value *ptr, bz::array_view<llvm::Value * const> indces, bz::u8string_view name = "");
	llvm::Value *create_struct_gep(llvm::Value *ptr, uint64_t idx, bz::u8string_view name = "");
	llvm::Value *create_array_gep(llvm::Value *ptr, llvm::Value *idx, bz::u8string_view name = "");

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
	llvm::StructType *get_slice_t(llvm::Type *elem_type) const;
	llvm::StructType *get_tuple_t(bz::array_view<llvm::Type * const> types) const;

	bool has_terminator(void) const;
	static bool has_terminator(llvm::BasicBlock *bb);

	void start_lifetime(llvm::Value *ptr, size_t size);
	void end_lifetime(llvm::Value *ptr, size_t size);

	void push_expression_scope(void);
	void pop_expression_scope(void);

	void push_destructor_call(llvm::Function *dtor_func, llvm::Value *ptr);
	void emit_destructor_calls(void);
	void emit_loop_destructor_calls(void);
	void emit_all_destructor_calls(void);

	void push_end_lifetime_call(llvm::Value *ptr, size_t size);
	void emit_end_lifetime_calls(void);
	void emit_loop_end_lifetime_calls(void);
	void emit_all_end_lifetime_calls(void);

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
		lex::src_tokens src_tokens, bz::u8string message,
		bz::vector<source_highlight> notes = {},
		bz::vector<source_highlight> suggestions = {}
	) const;
	[[nodiscard]] static source_highlight make_note(lex::src_tokens src_tokens, bz::u8string message);
	[[nodiscard]] static source_highlight make_note(bz::u8string message);


	global_context &global_ctx;
	llvm::Module *module;

	std::unordered_map<ast::decl_variable const *, llvm::Value    *> vars_{};
	std::unordered_map<ast::type_info     const *, llvm::Type     *> types_{};
	std::unordered_map<ast::function_body const *, llvm::Function *> funcs_{};

	bz::vector<ast::function_body *> functions_to_compile{};

	bz::vector<bz::vector<std::pair<llvm::Function *, llvm::Value *>>> destructor_calls{};
	bz::vector<bz::vector<std::pair<llvm::Value *, size_t>>> end_lifetime_calls{};

	std::pair<ast::function_body const *, llvm::Function *> current_function = { nullptr, nullptr };
	llvm::BasicBlock *alloca_bb = nullptr;
	llvm::Value *output_pointer = nullptr;
	loop_info_t loop_info = {};

	llvm::IRBuilder<> builder;
};

} // namespace ctx

#endif // CTX_BITCODE_CONTEXT_H
