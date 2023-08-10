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
	codegen_context(target_properties props);

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
	};

	struct global_variable_t
	{
		bz::u8string name;
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

	struct current_function_info_t
	{
		ast::function_body *func_body = nullptr;
		bz::u8string body_string = "";
		bz::u8string decl_string = "";
		uint32_t counter = 0;
		uint32_t indent_level = 0;
		bz::optional<expr_value> return_value = {};

		size_t value_reference_stack_size = 0;
		bz::array<expr_value, 4> value_references = {};

		std::unordered_map<ast::decl_variable const *, expr_value> local_variables;
		std::unordered_map<ast::decl_variable const *, expr_value> move_destruct_indicators;
		bz::vector<destruct_operation_info_t> destructor_calls;
		loop_info_t loop_info;
	};

	size_t counter = 0;

	type_set_t type_set;
	std::unordered_map<ast::type_info const *, struct_info_t> struct_infos;
	builtin_types_t builtin_types;
	std::unordered_map<ast::decl_variable const *, global_variable_t> global_variables;
	std::unordered_map<ast::function_body const *, function_info_t> functions;

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

	struct local_name_and_index_pair
	{
		bz::u8string name;
		uint32_t index;
	};

	size_t get_unique_number(void);
	bz::u8string make_type_name(void);
	bz::u8string make_type_name(ast::type_info const &info);
	bz::u8string get_member_name(size_t index) const;
	bz::u8string make_global_variable_name(void);
	bz::u8string make_global_variable_name(ast::decl_variable const &var_decl);
	bz::u8string make_function_name(ast::function_body const &func_body);
	local_name_and_index_pair make_local_name(void);
	bz::u8string make_local_name(uint32_t index) const;
	void add_libc_header(bz::u8string_view header);

	type get_struct(ast::type_info const &info, bool resolve = true);

	type get_void(void) const;
	type get_int8(void) const;
	type get_int16(void) const;
	type get_int32(void) const;
	type get_int64(void) const;
	type get_uint8(void) const;
	type get_uint16(void) const;
	type get_uint32(void) const;
	type get_uint64(void) const;
	type get_float32(void) const;
	type get_float64(void) const;
	type get_char(void) const;
	type get_bool(void) const;

	type add_pointer(type t, type_modifier modifier_kind);
	type add_pointer(type t);
	type add_const_pointer(type t);
	bool is_pointer(type t) const;
	std::pair<type, type_modifier> remove_pointer(type t) const;

	bool is_struct(type t) const;
	struct_type_t const *maybe_get_struct(type t) const;
	bool is_array(type t) const;
	array_type_t const *maybe_get_array(type t) const;

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
	type::function_reference add_function(function_type_t function_type);

	struct_type_t const &get_struct(type::struct_reference struct_ref) const;
	typedef_type_t const &get_typedef(type::typedef_reference typedef_ref) const;
	array_type_t const &get_array(type::array_reference array_ref) const;
	function_type_t const &get_function(type::function_reference function_ref) const;

	type::typedef_reference add_builtin_type(
		ast::type_info const &info,
		bz::u8string_view typedef_type_name,
		bz::u8string_view aliased_type_name
	);
	type::typedef_reference add_char_typedef(ast::type_info const &info, bz::u8string_view typedef_type_name);
	type::typedef_reference add_builtin_typedef(bz::u8string typedef_type_name, typedef_type_t typedef_type);

	bz::u8string to_string(type t) const;

	bz::u8string create_cstring(bz::u8string_view s);
	void add_global_variable(ast::decl_variable const &var_decl, type var_type, bz::u8string_view initializer);
	global_variable_t const &get_global_variable(ast::decl_variable const &var_decl) const;

	void ensure_function_generation(ast::function_body *func_body);
	void reset_current_function(ast::function_body &func_body);
	function_info_t const &get_function(ast::function_body &func_body);

	void add_indentation(void);
	bz::u8string to_string(expr_value const &value) const;
	bz::u8string to_string_lhs(expr_value const &value, precedence prec) const;
	bz::u8string to_string_rhs(expr_value const &value, precedence prec) const;
	bz::u8string to_string_unary(expr_value const &value, precedence prec) const;
	void add_expression(bz::u8string_view expr_string);
	expr_value add_uninitialized_value(type expr_type);
	expr_value add_value_expression(bz::u8string_view expr_string, type expr_type);
	expr_value add_reference_expression(bz::u8string_view expr_string, type expr_type, bool is_const);
	expr_value make_value_expression(uint32_t value_index, type value_type) const;
	expr_value make_reference_expression(uint32_t value_index, type value_type, bool is_const) const;

	void begin_if(expr_value condition);
	void begin_if_not(expr_value condition);
	void begin_if(bz::u8string_view condition);
	void end_if(void);

	void begin_while(expr_value condition);
	void begin_while(bz::u8string_view condition);
	void end_while(void);

	void add_return(void);
	void add_return(expr_value value);
	void add_return(bz::u8string_view value);

	void add_local_variable(ast::decl_variable const &var_decl, expr_value value);
	expr_value get_variable(ast::decl_variable const &var_decl);

	expr_value get_void_value(void) const;
	expr_value create_struct_gep(expr_value value, size_t index);
	expr_value create_struct_gep_value(expr_value value, size_t index);
	expr_value create_dereference(expr_value value);

	void push_destruct_operation(ast::destruct_operation const &destruct_op);
	void push_self_destruct_operation(ast::destruct_operation const &destruct_op, expr_value value);
	void push_variable_destruct_operation(
		ast::destruct_operation const &destruct_op,
		expr_value value,
		bz::optional<expr_value> move_destruct_indicator = {}
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
