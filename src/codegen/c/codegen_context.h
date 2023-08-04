#ifndef CODEGEN_C_CODEGEN_CONTEXT_H
#define CODEGEN_C_CODEGEN_CONTEXT_H

#include "core.h"
#include "types.h"
#include "expr_value.h"
#include "codegen/target.h"
#include "ast/statement_forward.h"
#include "ast/identifier.h"
#include "ctx/context_forward.h"

namespace codegen::c
{

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

	size_t counter = 0;

	type_set_t type_set;
	std::unordered_map<ast::type_info const *, struct_info_t> struct_infos;
	builtin_types_t builtin_types;
	std::unordered_map<ast::decl_variable const *, global_variable_t> global_variables;
	std::unordered_map<ast::function_body const *, function_info_t> functions;

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

	size_t get_unique_number(void);
	bz::u8string make_type_name(void);
	bz::u8string make_type_name(ast::type_info const &info);
	bz::u8string get_member_name(size_t index);
	bz::u8string make_global_variable_name(void);
	bz::u8string make_global_variable_name(ast::decl_variable const &var_decl);
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
	type remove_pointer(type t) const;

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

	bz::u8string get_code_string(void) const;
};

} // namespace codegen::c

#endif // CODEGEN_C_CODEGEN_CONTEXT_H
