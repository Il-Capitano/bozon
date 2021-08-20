#ifndef CTX_COMPTIME_EXECUTOR_H
#define CTX_COMPTIME_EXECUTOR_H

#include "ast/constant_value.h"
#include "ast/expression.h"
#include "ast/statement.h"
#include "abi/platform_abi.h"
#include "abi/platform_function_call.h"
#include "bc/val_ptr.h"
#include "bc/common.h"

#include <llvm/IR/Module.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/IR/LegacyPassManager.h>

namespace ctx
{

struct global_context;
struct parse_context;

struct comptime_func_call
{
	lex::src_tokens src_tokens;
	ast::function_body const *func_body;
};

enum class comptime_function_kind : uint32_t
{
	_first = 0,
	cleanup = 0,
	get_error_count,
	get_error_kind_by_index,
	get_error_begin_by_index,
	get_error_pivot_by_index,
	get_error_end_by_index,
	get_error_message_size_by_index,
	get_error_message_by_index,
	get_error_call_stack_size_by_index,
	get_error_call_stack_element_by_index,
	has_errors,
	add_error,
	push_call,
	pop_call,
	clear_errors,
	index_check_unsigned,
	index_check_signed,
	comptime_malloc_check,

	_last,
};

struct comptime_function
{
	comptime_function_kind kind;
	ast::function_body    *func_body;
	llvm::Function        *llvm_func;
};

struct comptime_executor_context
{
	comptime_executor_context(global_context &_global_ctx);

	comptime_executor_context(comptime_executor_context const &) = delete;
	comptime_executor_context(comptime_executor_context &&)      = delete;
	comptime_executor_context &operator = (comptime_executor_context const &) = delete;
	comptime_executor_context &operator = (comptime_executor_context &&)      = delete;

	~comptime_executor_context(void);

	std::unique_ptr<llvm::Module> create_module(void) const;
	[[nodiscard]] llvm::Module *push_module(llvm::Module *module);
	void pop_module(llvm::Module *prev_module);

	ast::type_info *get_builtin_type_info(uint32_t kind);
	ast::typespec_view get_builtin_type(bz::u8string_view name);
	ast::function_body *get_builtin_function(uint32_t kind);

	llvm::Value *get_variable(ast::decl_variable const *var_decl) const;
	void add_variable(ast::decl_variable const *var_decl, llvm::Value *val);

	void add_global_variable(ast::decl_variable const *var_decl);

	llvm::Type *get_base_type(ast::type_info *info);
	void add_base_type(ast::type_info const *info, llvm::Type *type);

	llvm::Function *get_function(ast::function_body *func_body);

	using module_function_pair = std::pair<std::unique_ptr<llvm::Module>, llvm::Function *>;
	module_function_pair get_module_and_function(ast::function_body *func_body);

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

	template<abi::platform_abi abi>
	abi::pass_kind get_pass_kind(ast::typespec_view ts)
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
	abi::pass_kind get_pass_kind(ast::typespec_view ts, llvm::Type *llvm_type)
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
	llvm::Type *get_usize_t(void) const;
	llvm::Type *get_isize_t(void) const;
	llvm::StructType *get_slice_t(llvm::Type *elem_type) const;
	llvm::StructType *get_tuple_t(bz::array_view<llvm::Type * const> types) const;

	bool has_terminator(void) const;
	static bool has_terminator(llvm::BasicBlock *bb);


	void push_expression_scope(void);
	void pop_expression_scope(void);

	void push_destructor_call(lex::src_tokens src_tokens, ast::function_body *dtor_func, llvm::Value *ptr);
	void emit_destructor_calls(void);
	void emit_loop_destructor_calls(void);
	void emit_all_destructor_calls(void);

	struct loop_info_t
	{
		llvm::BasicBlock *break_bb;
		llvm::BasicBlock *continue_bb;
		size_t destructor_stack_begin;
	};

	[[nodiscard]] loop_info_t push_loop(llvm::BasicBlock *break_bb, llvm::BasicBlock *continue_bb) noexcept;
	void pop_loop(loop_info_t info) noexcept;


	void ensure_function_emission(ast::function_body *func);
	[[nodiscard]] bool resolve_function(ast::function_body *body);

	llvm::Function *get_comptime_function(comptime_function_kind kind);
	void set_comptime_function(comptime_function_kind kind, ast::function_body *func_body);
	void set_comptime_function(comptime_function_kind kind, llvm::Function *llvm_func);


	std::pair<ast::constant_value, bz::vector<error>> execute_function(
		lex::src_tokens src_tokens,
		ast::function_body *body,
		bz::array_view<ast::expression const> params
	);
	std::pair<ast::constant_value, bz::vector<error>> execute_compound_expression(ast::expr_compound &expr);
	void initialize_engine(void);
	std::unique_ptr<llvm::ExecutionEngine> create_engine(std::unique_ptr<llvm::Module> module);
	void add_base_functions_to_engine(void);
	void add_module(std::unique_ptr<llvm::Module> module);

	bool has_error(void);
	bz::vector<error> consume_errors(void);

	source_highlight const *insert_error(lex::src_tokens src_tokens, bz::u8string message);
	comptime_func_call const *insert_call(lex::src_tokens src_tokens, ast::function_body const *body);

	[[nodiscard]] static error make_error(
		bz::u8string message,
		bz::vector<source_highlight> notes = {}, bz::vector<source_highlight> suggestions = {}
	);
	[[nodiscard]] static error make_error(
		lex::src_tokens src_tokens, bz::u8string message,
		bz::vector<source_highlight> notes = {}, bz::vector<source_highlight> suggestions = {}
	);
	[[nodiscard]] static source_highlight make_note(lex::src_tokens src_tokens, bz::u8string message);


	global_context &global_ctx;
	parse_context *current_parse_ctx = nullptr;
	llvm::Module *current_module = nullptr;

	std::unordered_map<ast::decl_variable const *, llvm::Value    *> vars_{};
	std::unordered_map<ast::type_info     const *, llvm::Type     *> types_{};
	std::unordered_map<ast::function_body const *, llvm::Function *> funcs_{};

	std::unordered_map<ast::function_body const *, module_function_pair> modules_and_functions{};

	bz::vector<ast::function_body *> functions_to_compile{};

	struct destructor_call_t
	{
		lex::src_tokens src_tokens;
		ast::function_body *dtor_func;
		llvm::Value *ptr;
	};

	bz::vector<bz::vector<destructor_call_t>> destructor_calls{};


	std::pair<ast::function_body const *, llvm::Function *> current_function = { nullptr, nullptr };
	llvm::BasicBlock *error_bb    = nullptr;
	llvm::BasicBlock *alloca_bb   = nullptr;
	llvm::Value *output_pointer   = nullptr;
	loop_info_t loop_info = {};
	llvm::IRBuilder<> builder;

	std::list<source_highlight>   execution_errors{}; // a list is used, so pointers are stable
	std::list<comptime_func_call> execution_calls{};  // a list is used, so pointers are stable

	ast::decl_variable *errors_array   = nullptr;
	ast::decl_variable *call_stack     = nullptr;
	ast::decl_variable *global_strings = nullptr;
	bz::vector<comptime_function> comptime_functions;

	llvm::TargetMachine *target_machine = nullptr;
	llvm::legacy::PassManager pass_manager{};
	std::unique_ptr<llvm::ExecutionEngine> engine{};
};

struct comptime_function_info_t
{
	bz::u8string_view name;
	comptime_function_kind kind;
};

#define def_element(kind) \
comptime_function_info_t{ #kind, comptime_function_kind::kind }

constexpr bz::array comptime_function_info = {
	def_element(cleanup),
	def_element(get_error_count),
	def_element(get_error_kind_by_index),
	def_element(get_error_begin_by_index),
	def_element(get_error_pivot_by_index),
	def_element(get_error_end_by_index),
	def_element(get_error_message_size_by_index),
	def_element(get_error_message_by_index),
	def_element(get_error_call_stack_size_by_index),
	def_element(get_error_call_stack_element_by_index),
	def_element(has_errors),
	def_element(add_error),
	def_element(push_call),
	def_element(pop_call),
	def_element(clear_errors),
	def_element(index_check_unsigned),
	def_element(index_check_signed),
	def_element(comptime_malloc_check),
};
static_assert(comptime_function_info.size() == static_cast<uint32_t>(comptime_function_kind::_last));

#undef def_element

} // namespace ctx

#endif // CTX_COMPTIME_EXECUTOR_H
