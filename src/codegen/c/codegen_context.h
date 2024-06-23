#ifndef CODEGEN_C_CODEGEN_CONTEXT_H
#define CODEGEN_C_CODEGEN_CONTEXT_H

#include "core.h"
#include "types.h"
#include "expr_value.h"
#include "codegen/target.h"
#include "ast/statement_forward.h"
#include "ast/expression.h"
#include "ast/identifier.h"
#include "ctx/context_forward.h"

namespace codegen::c
{

struct destruct_operation_info_t
{
	ast::destruct_operation const *destruct_op;
	expr_value value;
	bz::optional<expr_value> condition;
	bz::optional<expr_value> move_destruct_indicator;
	bz::optional<expr_value> rvalue_array_elem_ptr;
};

struct codegen_context
{
	codegen_context(ctx::global_context &global_ctx, target_properties props);

	struct struct_info_t
	{
		type::struct_reference struct_ref;
		type::typedef_reference typedef_ref;
		bool is_unresolved;
	};

	struct builtin_types_t
	{
		type::typedef_reference void_ = type::typedef_reference::invalid();
		type::typedef_reference int8_ = type::typedef_reference::invalid();
		type::typedef_reference int16_ = type::typedef_reference::invalid();
		type::typedef_reference int32_ = type::typedef_reference::invalid();
		type::typedef_reference int64_ = type::typedef_reference::invalid();
		type::typedef_reference uint8_ = type::typedef_reference::invalid();
		type::typedef_reference uint16_ = type::typedef_reference::invalid();
		type::typedef_reference uint32_ = type::typedef_reference::invalid();
		type::typedef_reference uint64_ = type::typedef_reference::invalid();
		type::typedef_reference float32_ = type::typedef_reference::invalid();
		type::typedef_reference float64_ = type::typedef_reference::invalid();
		type::typedef_reference char_ = type::typedef_reference::invalid();
		type::typedef_reference bool_ = type::typedef_reference::invalid();
		type::struct_reference slice = type::struct_reference::invalid();
	};

	struct global_variable_t
	{
		bz::u8string expr_string;
		type var_type;
	};

	struct function_info_t
	{
		bz::u8string name;
	};

	struct loop_info_t
	{
		size_t destructor_stack_begin;
	};

	struct if_info_t
	{
		bool in_if;
	};

	struct while_info_t
	{
		bool needs_break_goto;
		bool may_need_continue_goto;
		bool needs_continue_goto;
		size_t break_goto_index;
		size_t continue_goto_index;
	};

	struct switch_info_t
	{
		bool in_switch;
		uint32_t loop_level;
	};

	struct current_function_info_t
	{
		ast::function_body *func_body = nullptr;
		bz::u8string body_string = "";
		bz::u8string decl_string = "";
		uint32_t counter = 0;
		uint32_t indent_level = 0;
		bz::optional<expr_value> return_value = {};
		bz::vector<bz::u8string> temporary_expressions;
		expr_value temp_void_value = {};
		expr_value temp_false_value = {};
		expr_value temp_true_value = {};
		uint32_t temp_signed_zero_value_index = 0;
		uint32_t temp_signed_one_value_index = 0;
		uint32_t temp_unsigned_zero_value_index = 0;
		uint32_t temp_unsigned_one_value_index = 0;

		size_t value_reference_stack_size = 0;
		bz::array<expr_value, 4> value_references = {};

		std::unordered_map<ast::decl_variable const *, expr_value> local_variables;
		std::unordered_map<ast::decl_variable const *, expr_value> move_destruct_indicators;
		bz::vector<destruct_operation_info_t> destructor_calls;
		loop_info_t loop_info = { .destructor_stack_begin = 0 };

		if_info_t if_info = { .in_if = false };
		uint32_t loop_level = 0;
		while_info_t while_info = {
			.needs_break_goto = false,
			.may_need_continue_goto = false,
			.needs_continue_goto = false,
			.break_goto_index = 0,
			.continue_goto_index = 0,
		};
		switch_info_t switch_info = { .in_switch = false, .loop_level = 0 };
	};

	struct u8string_hash
	{
		size_t operator () (bz::u8string_view s) const
		{
			return std::hash<std::string_view>()(std::string_view(s.data(), s.size()));
		}
	};

	size_t counter = 0;

	type_set_t type_set;
	std::unordered_map<ast::type_info const *, struct_info_t> struct_infos;
	builtin_types_t builtin_types;
	std::unordered_map<ast::decl_variable const *, global_variable_t> global_variables;
	std::unordered_map<ast::function_body const *, function_info_t> functions;
	bz::vector<bz::u8string> panic_strings_storage;
	std::unordered_map<bz::u8string_view, bz::u8string, u8string_hash> string_literals;

	bz::vector<ast::function_body *> functions_to_compile;
	current_function_info_t current_function_info;

	bz::u8string_view indentation;
	bz::u8string struct_forward_declarations_string;
	bz::u8string typedefs_string;
	bz::u8string struct_bodies_string;
	bz::u8string function_declarations_string;
	bz::u8string variables_string;
	bz::u8string function_bodies_string;

	bz::vector<bz::u8string> included_headers;

	uint32_t short_size;
	uint32_t int_size;
	uint32_t long_size;
	uint32_t long_long_size;
	uint32_t pointer_size;
	comptime::memory::endianness_kind endianness;
	ctx::global_context &global_ctx;

	struct local_name_and_index_pair
	{
		bz::u8string name;
		uint32_t index;
	};

	ast::function_body *get_builtin_function(uint32_t kind) const;
	bz::u8string get_location_string(lex::src_tokens const &src_tokens) const;

	bool is_little_endian(void) const;
	bool is_big_endian(void) const;

	size_t get_unique_number(void);
	bz::u8string make_type_name(void);
	bz::u8string make_type_name(ast::type_info const &info);
	bz::u8string get_member_name(size_t index) const;
	bz::u8string make_global_variable_name(void);
	bz::u8string make_global_variable_name(ast::decl_variable const &var_decl);
	bz::u8string make_function_name(ast::function_body const &func_body);
	local_name_and_index_pair make_local_name(void);
	bz::u8string make_local_name(uint32_t index) const;
	bz::u8string make_goto_label(size_t index) const;
	void add_libc_header(bz::u8string_view header);

	type get_struct(ast::type_info const &info, bool resolve = true);

	type get_void(void) const;
	type get_int8(void) const;
	type get_int16(void) const;
	type get_int32(void) const;
	type get_int64(void) const;
	type get_isize(void) const;
	type get_uint8(void) const;
	type get_uint16(void) const;
	type get_uint32(void) const;
	type get_uint64(void) const;
	type get_usize(void) const;
	type get_float32(void) const;
	type get_float64(void) const;
	type get_char(void) const;
	type get_bool(void) const;
	type get_slice(void) const;

	type get_c_int(void) const;
	type get_c_unsigned_int(void) const;

	type add_pointer(type t, type_modifier modifier_kind);
	type add_pointer(type t);
	type add_const_pointer(type t);
	bool is_pointer(type t) const;
	std::pair<type, type_modifier> remove_pointer(type t) const;

	bool is_struct(type t) const;
	struct_type_t const *maybe_get_struct(type t) const;
	bool is_array(type t) const;
	array_type_t const *maybe_get_array(type t) const;
	bool is_slice(type t) const;
	slice_type_t const *maybe_get_slice(type t) const;
	bool is_function(type t) const;
	function_type_t const *maybe_get_function(type t) const;

	bool is_pointer_or_function(type t) const;

	using struct_infos_iterator = std::unordered_map<ast::type_info const *, struct_info_t>::iterator;
	std::pair<bool, struct_infos_iterator> should_resolve_struct(ast::type_info const &info);

	void generate_struct_body(type::struct_reference struct_ref);
	void generate_struct_forward_declaration(type::struct_reference struct_ref);

	type::struct_reference add_struct(ast::type_info const &info, struct_infos_iterator it, struct_type_t struct_type);
	type::struct_reference add_unresolved_struct(ast::type_info const &info);
	type::struct_reference add_struct_forward_declaration(ast::type_info const &info);
	type::struct_reference add_struct(struct_type_t struct_type);
	type::typedef_reference add_typedef(typedef_type_t typedef_type);
	type::array_reference add_array(array_type_t array_type);
	type::slice_reference add_slice(slice_type_t slice_type);
	type::function_reference add_function(function_type_t function_type);

	struct_type_t const &get_struct(type::struct_reference struct_ref) const;
	typedef_type_t const &get_typedef(type::typedef_reference typedef_ref) const;
	array_type_t const &get_array(type::array_reference array_ref) const;
	slice_type_t const &get_slice(type::slice_reference slice_ref) const;
	function_type_t const &get_function(type::function_reference function_ref) const;

	type::typedef_reference add_builtin_type(
		ast::type_info const &info,
		bz::u8string_view typedef_type_name,
		bz::u8string_view aliased_type_name
	);
	type::typedef_reference add_char_typedef(ast::type_info const &info, bz::u8string_view typedef_type_name);
	type::typedef_reference add_builtin_typedef(bz::u8string typedef_type_name, typedef_type_t typedef_type);

	bz::u8string to_string(type t) const;

	bz::u8string_view add_panic_string(bz::u8string s);
	bz::u8string_view create_cstring(bz::u8string_view s);
	void add_global_variable(ast::decl_variable const &var_decl, type var_type, bz::u8string_view initializer);
	global_variable_t const &get_global_variable(ast::decl_variable const &var_decl) const;

	void ensure_function_generation(ast::function_body &func_body);
	void reset_current_function(ast::function_body &func_body);
	function_info_t const &get_function(ast::function_body &func_body);
	ast::function_body &get_main_func_body(void) const;
	bz::u8string_view get_libc_macro_name(ast::function_body &func_body);

	void add_indentation(void);
	bz::u8string to_string(expr_value const &value) const;
	bz::u8string to_string_lhs(expr_value const &value, precedence prec) const;
	bz::u8string to_string_rhs(expr_value const &value, precedence prec) const;
	bz::u8string to_string_unary(expr_value const &value, precedence prec) const;

	bz::u8string to_string_binary(expr_value const &lhs, expr_value const &rhs, bz::u8string_view op, precedence prec) const;
	bz::u8string to_string_binary(expr_value const &lhs, bz::u8string_view rhs, bz::u8string_view op, precedence prec) const;
	bz::u8string to_string_unary_prefix(expr_value const &value, bz::u8string_view op) const;
	bz::u8string to_string_unary_suffix(expr_value const &value, bz::u8string_view op) const;

	bz::u8string to_string_arg(expr_value const &value) const;
	bz::u8string to_string_struct_literal(type aggregate_type, bz::array_view<expr_value const> values) const;

	void add_expression(bz::u8string_view expr_string);
	expr_value add_uninitialized_value(type expr_type);
	expr_value add_value_expression(bz::u8string_view expr_string, type expr_type);
	expr_value add_reference_expression(bz::u8string_view expr_string, type expr_type, bool is_const);
	expr_value add_temporary_expression(
		bz::u8string expr_string,
		type expr_type,
		bool is_const,
		bool is_variable,
		bool is_rvalue,
		precedence prec
	);
	expr_value make_value_expression(uint32_t value_index, type value_type) const;
	expr_value make_reference_expression(uint32_t value_index, type value_type, bool is_const) const;
	expr_value make_temporary_expression(
		uint32_t value_index,
		type value_type,
		bool is_const,
		bool is_variable,
		bool is_rvalue,
		precedence prec
	) const;

	[[nodiscard]] if_info_t begin_if(expr_value condition);
	[[nodiscard]] if_info_t begin_if_not(expr_value condition);
	[[nodiscard]] if_info_t begin_if(bz::u8string_view condition);
	void begin_else(void);
	void begin_else_if(expr_value condition);
	void begin_else_if(bz::u8string_view condition);
	void end_if(if_info_t prev_if_info);

	[[nodiscard]] while_info_t begin_while(expr_value condition, bool may_need_continue_goto = false);
	[[nodiscard]] while_info_t begin_while(bz::u8string_view condition, bool may_need_continue_goto = false);
	void end_while(while_info_t prev_while_info);

	[[nodiscard]] switch_info_t begin_switch(expr_value value);
	[[nodiscard]] switch_info_t begin_switch(bz::u8string_view value);
	void add_case_label(bz::u8string_view value);
	void begin_case(void);
	void begin_default_case(void);
	void end_case(void);
	void end_switch(switch_info_t prev_switch_info);

	bz::optional<size_t> get_break_goto_index(void);
	bz::optional<size_t> get_continue_goto_index(void);

	void add_return(void);
	void add_return(expr_value value);
	void add_return(bz::u8string_view value);

	void add_local_variable(ast::decl_variable const &var_decl, expr_value value);
	expr_value get_variable(ast::decl_variable const &var_decl);

	expr_value get_void_value(void) const;
	expr_value get_false_value(void) const;
	expr_value get_true_value(void) const;
	expr_value get_zero_value(type int_type) const;
	expr_value get_signed_zero_value(type int_type) const;
	expr_value get_unsigned_zero_value(type int_type) const;
	expr_value get_one_value(type int_type) const;
	expr_value get_signed_one_value(type int_type) const;
	expr_value get_unsigned_one_value(type int_type) const;
	expr_value get_signed_value(int64_t value, type int_type);
	expr_value get_unsigned_value(uint64_t value, type int_type);
	expr_value get_null_pointer_value(void) const;

	expr_value create_struct_gep(expr_value value, size_t index);
	expr_value create_struct_gep_pointer(expr_value value, size_t index);
	expr_value create_struct_gep_value(expr_value value, size_t index);
	expr_value create_array_gep(expr_value value, expr_value index);
	expr_value create_array_gep_pointer(expr_value value, expr_value index);
	expr_value create_array_slice_gep(expr_value begin_ptr, expr_value index);
	expr_value create_array_slice_gep_pointer(expr_value begin_ptr, expr_value index);
	expr_value create_struct_literal(type aggregate_type, bz::array_view<expr_value const> values);

	expr_value create_plus(expr_value const &value);
	expr_value create_minus(expr_value const &value);
	expr_value create_dereference(expr_value const &value);
	expr_value create_address_of(expr_value const &value);
	expr_value create_cast(expr_value const &value, type dest_type);
	expr_value create_bool_not(expr_value const &value);
	expr_value create_bit_not(expr_value const &value);
	expr_value create_sizeof(type t);

	expr_value create_equals(expr_value const &lhs, expr_value const &rhs);
	expr_value create_not_equals(expr_value const &lhs, expr_value const &rhs);
	expr_value create_equality(expr_value const &lhs, expr_value const &rhs, bz::u8string_view op);
	expr_value create_relational(expr_value const &lhs, expr_value const &rhs, bz::u8string_view op);
	expr_value create_logical_and(expr_value const &lhs, expr_value const &rhs);
	expr_value create_logical_or(expr_value const &lhs, expr_value const &rhs);
	expr_value create_plus(expr_value const &lhs, expr_value const &rhs);
	expr_value create_plus(expr_value const &lhs, expr_value const &rhs, type result_type);
	expr_value create_pointer_plus(expr_value const &pointer, expr_value const &offset);
	void create_plus_eq(expr_value const &lhs, expr_value const &rhs);
	void create_pointer_plus_eq(expr_value const &pointer, expr_value const &offset);
	expr_value create_minus(expr_value const &lhs, expr_value const &rhs);
	expr_value create_minus(expr_value const &lhs, expr_value const &rhs, type result_type);
	expr_value create_pointer_minus(expr_value const &pointer, expr_value const &offset);
	void create_minus_eq(expr_value const &lhs, expr_value const &rhs);
	void create_pointer_minus_eq(expr_value const &pointer, expr_value const &offset);
	expr_value create_multiply(expr_value const &lhs, expr_value const &rhs);
	void create_multiply_eq(expr_value const &lhs, expr_value const &rhs);
	expr_value create_divide(expr_value const &lhs, expr_value const &rhs);
	void create_divide_eq(expr_value const &lhs, expr_value const &rhs);
	expr_value create_modulo(expr_value const &lhs, expr_value const &rhs);
	void create_modulo_eq(expr_value const &lhs, expr_value const &rhs);
	expr_value create_bit_op(expr_value const &lhs, expr_value const &rhs, bz::u8string_view op, precedence prec);
	void create_bit_op_eq(expr_value const &lhs, expr_value const &rhs, bz::u8string_view op);
	expr_value create_bitshift(expr_value const &lhs, expr_value const &rhs, bz::u8string_view op);
	void create_bitshift_eq(expr_value const &lhs, expr_value const &rhs, bz::u8string_view op);

	expr_value create_prefix_unary_operation(
		expr_value const &value,
		bz::u8string_view op,
		type result_type
	);
	expr_value create_temporary_prefix_unary_operation(
		expr_value const &value,
		bz::u8string_view op,
		type result_type,
		bool is_const,
		bool is_variable,
		bool is_rvalue
	);
	void create_prefix_unary_operation(
		expr_value const &value,
		bz::u8string_view op
	);

	expr_value create_binary_operation(
		expr_value const &lhs,
		expr_value const &rhs,
		bz::u8string_view op,
		precedence prec,
		type result_type
	);
	expr_value create_temporary_binary_operation(
		expr_value const &lhs,
		expr_value const &rhs,
		bz::u8string_view op,
		precedence prec,
		type result_type
	);
	expr_value create_binary_operation(
		expr_value const &lhs,
		bz::u8string_view rhs_string,
		bz::u8string_view op,
		precedence prec,
		type result_type
	);
	void create_binary_operation(expr_value const &lhs, expr_value const &rhs, bz::u8string_view op, precedence prec);
	void create_binary_operation(expr_value const &lhs, bz::u8string_view rhs_string, bz::u8string_view op, precedence prec);
	void create_assignment(expr_value const &lhs, expr_value const &rhs);
	void create_assignment(expr_value const &lhs, bz::u8string_view rhs_string);

	expr_value create_trivial_copy(expr_value const &value);
	expr_value create_rvalue_materialization(expr_value const &value);

	void create_unreachable(void);
	void create_trap(void);

	void push_destruct_operation(ast::destruct_operation const &destruct_op);
	void push_self_destruct_operation(ast::destruct_operation const &destruct_op, expr_value value);
	void push_variable_destruct_operation(
		ast::destruct_operation const &destruct_op,
		expr_value value,
		bz::optional<expr_value> move_destruct_indicator = {}
	);
	void push_rvalue_array_destruct_operation(
		ast::destruct_operation const &destruct_op,
		expr_value value,
		expr_value rvalue_array_elem_ptr
	);

	expr_value add_move_destruct_indicator(ast::decl_variable const &decl);
	bz::optional<expr_value> get_move_destruct_indicator(ast::decl_variable const *decl) const;
	bz::optional<expr_value> get_move_destruct_indicator(ast::decl_variable const &decl) const;

	void generate_destruct_operations(size_t destruct_calls_start_index);
	void generate_loop_destruct_operations(void);
	void generate_all_destruct_operations(void);

	struct expression_scope_info_t
	{
		size_t destructor_calls_size;
	};

	[[nodiscard]] expression_scope_info_t push_expression_scope(void);
	void pop_expression_scope(expression_scope_info_t prev_info);
	[[nodiscard]] loop_info_t push_loop(void);
	void pop_loop(loop_info_t prev_info);

	[[nodiscard]] expr_value push_value_reference(expr_value new_value);
	void pop_value_reference(expr_value prev_value);
	expr_value get_value_reference(size_t index);

	bz::u8string get_code_string(void) const;
};

} // namespace codegen::c

#endif // CODEGEN_C_CODEGEN_CONTEXT_H