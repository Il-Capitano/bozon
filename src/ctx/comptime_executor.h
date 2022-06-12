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

	register_malloc,
	register_free,
	check_leaks,

	index_check_unsigned,
	index_check_signed,

	comptime_malloc_check,
	comptime_memcpy_check,
	comptime_memmove_check,
	comptime_memset_check,

	i8_divide_check,
	i16_divide_check,
	i32_divide_check,
	i64_divide_check,
	u8_divide_check,
	u16_divide_check,
	u32_divide_check,
	u64_divide_check,

	i8_modulo_check,
	i16_modulo_check,
	i32_modulo_check,
	i64_modulo_check,
	u8_modulo_check,
	u16_modulo_check,
	u32_modulo_check,
	u64_modulo_check,

	exp_f32_check,   exp_f64_check,
	exp2_f32_check,  exp2_f64_check,
	expm1_f32_check, expm1_f64_check,
	log_f32_check,   log_f64_check,
	log10_f32_check, log10_f64_check,
	log2_f32_check,  log2_f64_check,
	log1p_f32_check, log1p_f64_check,

	sqrt_f32_check,  sqrt_f64_check,
	pow_f32_check,   pow_f64_check,
	cbrt_f32_check,  cbrt_f64_check,
	hypot_f32_check, hypot_f64_check,

	sin_f32_check,   sin_f64_check,
	cos_f32_check,   cos_f64_check,
	tan_f32_check,   tan_f64_check,
	asin_f32_check,  asin_f64_check,
	acos_f32_check,  acos_f64_check,
	atan_f32_check,  atan_f64_check,
	atan2_f32_check, atan2_f64_check,

	sinh_f32_check,  sinh_f64_check,
	cosh_f32_check,  cosh_f64_check,
	tanh_f32_check,  tanh_f64_check,
	asinh_f32_check, asinh_f64_check,
	acosh_f32_check, acosh_f64_check,
	atanh_f32_check, atanh_f64_check,

	erf_f32_check,    erf_f64_check,
	erfc_f32_check,   erfc_f64_check,
	tgamma_f32_check, tgamma_f64_check,
	lgamma_f32_check, lgamma_f64_check,

	_last,
};

struct comptime_function
{
	comptime_function_kind kind;
	ast::function_body    *func_body;
};

struct function_definition_and_declaration_pair
{
	llvm::Function *definition;
	llvm::Module   *module;
	llvm::Function *declaration;
};

struct variable_definition_and_declaration_pair
{
	llvm::GlobalVariable *definition;
	llvm::Module         *module;
	llvm::GlobalVariable *declaration;
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

	bc::value_and_type_pair get_variable(ast::decl_variable const *var_decl);
	void add_variable(ast::decl_variable const *var_decl, llvm::Value *val, llvm::Type *type);

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
	llvm::Value *create_alloca_without_lifetime_start(llvm::Type *t);
	llvm::Value *create_alloca_without_lifetime_start(llvm::Type *t, size_t align);
	llvm::Value *create_string(bz::u8string_view str);

	llvm::Value *create_bitcast(bc::val_ptr val, llvm::Type *dest_type);
	llvm::Value *create_cast_to_int(bc::val_ptr val);
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
		llvm::ArrayRef<llvm::Value *> args = llvm::None
	);
	llvm::CallInst *create_call(
		llvm::Function *fn,
		llvm::ArrayRef<llvm::Value *> args = llvm::None
	);

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
	llvm::PointerType *get_opaque_pointer_t(void) const;

	bool has_terminator(void) const;
	static bool has_terminator(llvm::BasicBlock *bb);
	bool do_error_checking(void) const;

	void start_lifetime(llvm::Value *ptr, size_t size);
	void end_lifetime(llvm::Value *ptr, size_t size);

	void push_expression_scope(void);
	void pop_expression_scope(void);

	void push_destruct_operation(ast::destruct_operation const &destruct_op);
	void push_self_destruct_operation(ast::destruct_operation const &destruct_op, llvm::Value *ptr, llvm::Type *type);
	void emit_destruct_operations(void);
	void emit_loop_destruct_operations(void);
	void emit_all_destruct_operations(void);

	void push_end_lifetime_call(llvm::Value *ptr, size_t size);
	void emit_end_lifetime_calls(void);
	void emit_loop_end_lifetime_calls(void);
	void emit_all_end_lifetime_calls(void);

	[[nodiscard]] bc::val_ptr push_value_reference(bc::val_ptr new_value);
	void pop_value_reference(bc::val_ptr prev_value);
	bc::val_ptr get_value_reference(void);

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


	std::pair<ast::constant_value, bz::vector<error>> execute_function(
		lex::src_tokens const &src_tokens,
		ast::expr_function_call &func_call
	);
	std::pair<ast::constant_value, bz::vector<error>> execute_compound_expression(ast::expr_compound &expr);
	void initialize_optimizer(void);
	void initialize_engine(void);
	std::unique_ptr<llvm::ExecutionEngine> create_engine(std::unique_ptr<llvm::Module> module);
	void add_base_functions_to_engine(void);
	void add_module(std::unique_ptr<llvm::Module> module);

	bool has_error(void);
	bz::vector<error> consume_errors(void);

	source_highlight const *insert_error(lex::src_tokens const &src_tokens, bz::u8string message);
	comptime_func_call const *insert_call(lex::src_tokens const &src_tokens, ast::function_body const *body);

	[[nodiscard]] static error make_error(
		bz::u8string message,
		bz::vector<source_highlight> notes = {}, bz::vector<source_highlight> suggestions = {}
	);
	[[nodiscard]] static error make_error(
		lex::src_tokens const &src_tokens, bz::u8string message,
		bz::vector<source_highlight> notes = {}, bz::vector<source_highlight> suggestions = {}
	);
	[[nodiscard]] static source_highlight make_note(lex::src_tokens const &src_tokens, bz::u8string message);


	global_context &global_ctx;
	parse_context *current_parse_ctx = nullptr;
	llvm::Module *current_module = nullptr;

	std::unordered_map<ast::decl_variable const *, bc::value_and_type_pair> vars_{};
	std::unordered_map<ast::decl_variable const *, variable_definition_and_declaration_pair> global_vars_{};
	std::unordered_map<ast::type_info     const *, llvm::Type *> types_{};
	std::unordered_map<ast::function_body const *, function_definition_and_declaration_pair> funcs_{};

	std::unordered_map<ast::function_body const *, module_function_pair> modules_and_functions{};

	bz::vector<ast::function_body *> functions_to_compile{};

	struct destruct_operation_info_t
	{
		ast::destruct_operation const *destruct_op;
		llvm::Value *ptr;
		llvm::Type *type;
	};

	struct end_lifetime_info_t
	{
		llvm::Value *ptr;
		size_t size;
	};

	bz::vector<bz::vector<destruct_operation_info_t>> destructor_calls{};
	bz::vector<bz::vector<end_lifetime_info_t>> end_lifetime_calls{};


	std::pair<ast::function_body const *, llvm::Function *> current_function = { nullptr, nullptr };
	llvm::BasicBlock *error_bb    = nullptr;
	llvm::BasicBlock *alloca_bb   = nullptr;
	llvm::Value *output_pointer   = nullptr;
	loop_info_t loop_info = {};
	bc::val_ptr current_value_reference;

	llvm::IRBuilder<> builder;

	std::list<source_highlight>   execution_errors{}; // a list is used, so pointers are stable
	std::list<comptime_func_call> execution_calls{};  // a list is used, so pointers are stable

	ast::decl_variable *errors_array   = nullptr;
	ast::decl_variable *call_stack     = nullptr;
	ast::decl_variable *global_strings = nullptr;
	ast::decl_variable *malloc_infos   = nullptr;
	bz::vector<comptime_function> comptime_functions;
	uint32_t comptime_checking_file_id = 0;

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

	def_element(register_malloc),
	def_element(register_free),
	def_element(check_leaks),

	def_element(index_check_unsigned),
	def_element(index_check_signed),

	def_element(comptime_malloc_check),
	def_element(comptime_memcpy_check),
	def_element(comptime_memmove_check),
	def_element(comptime_memset_check),

	def_element(i8_divide_check),
	def_element(i16_divide_check),
	def_element(i32_divide_check),
	def_element(i64_divide_check),
	def_element(u8_divide_check),
	def_element(u16_divide_check),
	def_element(u32_divide_check),
	def_element(u64_divide_check),

	def_element(i8_modulo_check),
	def_element(i16_modulo_check),
	def_element(i32_modulo_check),
	def_element(i64_modulo_check),
	def_element(u8_modulo_check),
	def_element(u16_modulo_check),
	def_element(u32_modulo_check),
	def_element(u64_modulo_check),

	def_element(exp_f32_check),
	def_element(exp_f64_check),
	def_element(exp2_f32_check),
	def_element(exp2_f64_check),
	def_element(expm1_f32_check),
	def_element(expm1_f64_check),
	def_element(log_f32_check),
	def_element(log_f64_check),
	def_element(log10_f32_check),
	def_element(log10_f64_check),
	def_element(log2_f32_check),
	def_element(log2_f64_check),
	def_element(log1p_f32_check),
	def_element(log1p_f64_check),

	def_element(sqrt_f32_check),
	def_element(sqrt_f64_check),
	def_element(pow_f32_check),
	def_element(pow_f64_check),
	def_element(cbrt_f32_check),
	def_element(cbrt_f64_check),
	def_element(hypot_f32_check),
	def_element(hypot_f64_check),

	def_element(sin_f32_check),
	def_element(sin_f64_check),
	def_element(cos_f32_check),
	def_element(cos_f64_check),
	def_element(tan_f32_check),
	def_element(tan_f64_check),
	def_element(asin_f32_check),
	def_element(asin_f64_check),
	def_element(acos_f32_check),
	def_element(acos_f64_check),
	def_element(atan_f32_check),
	def_element(atan_f64_check),
	def_element(atan2_f32_check),
	def_element(atan2_f64_check),

	def_element(sinh_f32_check),
	def_element(sinh_f64_check),
	def_element(cosh_f32_check),
	def_element(cosh_f64_check),
	def_element(tanh_f32_check),
	def_element(tanh_f64_check),
	def_element(asinh_f32_check),
	def_element(asinh_f64_check),
	def_element(acosh_f32_check),
	def_element(acosh_f64_check),
	def_element(atanh_f32_check),
	def_element(atanh_f64_check),

	def_element(erf_f32_check),
	def_element(erf_f64_check),
	def_element(erfc_f32_check),
	def_element(erfc_f64_check),
	def_element(tgamma_f32_check),
	def_element(tgamma_f64_check),
	def_element(lgamma_f32_check),
	def_element(lgamma_f64_check),
};
static_assert(comptime_function_info.size() == static_cast<uint32_t>(comptime_function_kind::_last));

#undef def_element

} // namespace ctx

#endif // CTX_COMPTIME_EXECUTOR_H
