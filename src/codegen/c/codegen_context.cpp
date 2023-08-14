#include "codegen_context.h"
#include "codegen.h"
#include "ast/statement.h"

namespace codegen::c
{

codegen_context::codegen_context(target_properties props)
	: indentation("\t")
{
	bz_assert(props.c_short_size.has_value());
	bz_assert(props.c_int_size.has_value());
	bz_assert(props.c_long_size.has_value());
	bz_assert(props.c_long_long_size.has_value());
	this->short_size = props.c_short_size.get();
	this->int_size = props.c_int_size.get();
	this->long_size = props.c_long_size.get();
	this->long_long_size = props.c_long_long_size.get();

	this->builtin_types.void_ = this->type_set.add_unique_typedef({
		.aliased_type = {},
	});
	this->type_set.add_typedef_type_name(this->builtin_types.void_, "void");
}

size_t codegen_context::get_unique_number(void)
{
	auto const result = this->counter;
	this->counter += 1;
	return result;
}

bz::u8string codegen_context::make_type_name(void)
{
	return bz::format("t_{:x}", this->get_unique_number());
}

static ast::attribute const &get_libc_struct_attribute(ast::type_info const &info)
{
	bz_assert(info.is_libc_struct());
	return info.attributes.filter([](ast::attribute const &attr) {
		return attr.name->value == "__libc_struct";
	}).front();
}

bz::u8string codegen_context::make_type_name(ast::type_info const &info)
{
	if (info.is_libc_struct())
	{
		auto const &attribute = get_libc_struct_attribute(info);
		bz_assert(attribute.args.size() == 2);
		auto const header = attribute.args[0].get_constant_value().get_string();
		this->add_libc_header(header);
		return attribute.args[1].get_constant_value().get_string();
	}
	else
	{
		bz_assert(info.type_name.values.not_empty());
		return bz::format("t_{}_{:x}", info.type_name.values.back(), this->get_unique_number());
	}
}

bz::u8string codegen_context::get_member_name(size_t index) const
{
	return bz::format("m_{:x}", index);
}

bz::u8string codegen_context::make_global_variable_name(void)
{
	return bz::format("gv_{:x}", this->get_unique_number());
}

static ast::attribute const &get_libc_variable_attribute(ast::decl_variable const &var_decl)
{
	bz_assert(var_decl.is_libc_variable());
	return var_decl.attributes.filter([](ast::attribute const &attr) {
		return attr.name->value == "__libc_variable";
	}).front();
}

bz::u8string codegen_context::make_global_variable_name(ast::decl_variable const &var_decl)
{
	if (var_decl.is_libc_variable())
	{
		auto const &attribute = get_libc_variable_attribute(var_decl);
		bz_assert(attribute.args.size() == 2);
		auto const header = attribute.args[0].get_constant_value().get_string();
		this->add_libc_header(header);
		return attribute.args[1].get_constant_value().get_string();
	}
	else if (var_decl.is_external_linkage())
	{
		if (var_decl.symbol_name == "")
		{
			bz::u8string result = "";
			for (auto const &value : var_decl.get_id().values)
			{
				result += value;
				result += '_';
			}
			result += bz::format("{:x}", this->get_unique_number());
			return result;
		}
		else
		{
			return var_decl.symbol_name;
		}
	}
	else
	{
		bz_assert(var_decl.get_id().values.not_empty());
		return bz::format("gv_{}_{:x}", var_decl.get_id().values.back(), this->get_unique_number());
	}
}

static ast::attribute const &get_libc_function_attribute(ast::function_body const &func_body)
{
	bz_assert(func_body.is_libc_function());
	return func_body.attributes.filter([](ast::attribute const &attr) {
		return attr.name->value == "__libc_function";
	}).front();
}

bz::u8string codegen_context::make_function_name(ast::function_body const &func_body)
{
	if (func_body.is_libc_function())
	{
		auto const &attribute = get_libc_function_attribute(func_body);
		bz_assert(attribute.args.size() == 2);
		auto const header = attribute.args[0].get_constant_value().get_string();
		this->add_libc_header(header);
		return attribute.args[1].get_constant_value().get_string();
	}
	else if (func_body.is_external_linkage() && func_body.symbol_name != "")
	{
		return func_body.symbol_name;
	}
	else if (func_body.function_name_or_operator_kind.is<ast::identifier>())
	{
		auto const &id = func_body.function_name_or_operator_kind.get<ast::identifier>();
		bz_assert(id.values.not_empty());
		return bz::format("f_{}_{:x}", id.values.back(), this->get_unique_number());
	}
	else
	{
		auto const op_kind = func_body.function_name_or_operator_kind.get<uint32_t>();
		return bz::format("o_{}_{:x}", op_kind, this->get_unique_number());
	}
}

codegen_context::local_name_and_index_pair codegen_context::make_local_name(void)
{
	auto const index = this->current_function_info.counter;
	this->current_function_info.counter += 1;
	return { bz::format("v_{:x}", index), index };
}

bz::u8string codegen_context::make_local_name(uint32_t index) const
{
	return bz::format("v_{:x}", index);
}

void codegen_context::add_libc_header(bz::u8string_view header)
{
	if (!this->included_headers.contains(header))
	{
		this->included_headers.push_back(header);
	}
}

type codegen_context::get_struct(ast::type_info const &info, bool resolve)
{
	if (auto const it = this->struct_infos.find(&info); it != this->struct_infos.end())
	{
		return it->second.typedef_ref != type::typedef_reference::invalid()
			? type(it->second.typedef_ref)
			: type(it->second.struct_ref);
	}

	if (resolve || info.kind == ast::type_info::forward_declaration)
	{
		return generate_struct(info, *this);
	}
	else
	{
		return type(this->add_unresolved_struct(info));
	}
}

type codegen_context::get_void(void) const
{
	return type(this->builtin_types.void_);
}

type codegen_context::get_int8(void) const
{
	return type(this->builtin_types.int8_);
}

type codegen_context::get_int16(void) const
{
	return type(this->builtin_types.int16_);
}

type codegen_context::get_int32(void) const
{
	return type(this->builtin_types.int32_);
}

type codegen_context::get_int64(void) const
{
	return type(this->builtin_types.int64_);
}

type codegen_context::get_uint8(void) const
{
	return type(this->builtin_types.uint8_);
}

type codegen_context::get_uint16(void) const
{
	return type(this->builtin_types.uint16_);
}

type codegen_context::get_uint32(void) const
{
	return type(this->builtin_types.uint32_);
}

type codegen_context::get_uint64(void) const
{
	return type(this->builtin_types.uint64_);
}

type codegen_context::get_float32(void) const
{
	return type(this->builtin_types.float32_);
}

type codegen_context::get_float64(void) const
{
	return type(this->builtin_types.float64_);
}

type codegen_context::get_char(void) const
{
	return type(this->builtin_types.char_);
}

type codegen_context::get_bool(void) const
{
	return type(this->builtin_types.bool_);
}

type codegen_context::add_pointer(type t, type_modifier modifier_kind)
{
	if (t.modifier_info.push(modifier_kind))
	{
		return t;
	}
	else
	{
		auto const temp_typedef = this->add_typedef({ .aliased_type = t });

		auto result = type(temp_typedef);
		[[maybe_unused]] auto const success = result.modifier_info.push(modifier_kind);
		bz_assert(success);
		return result;
	}
}

type codegen_context::add_pointer(type t)
{
	return this->add_pointer(t, type_modifier::pointer);
}

type codegen_context::add_const_pointer(type t)
{
	return this->add_pointer(t, type_modifier::const_pointer);
}

bool codegen_context::is_pointer(type t) const
{
	return t.is_pointer()
		|| (t.is_typedef() && this->is_pointer(this->get_typedef(t.get_typedef()).aliased_type));
}

std::pair<type, type_modifier> codegen_context::remove_pointer(type t) const
{
	while (t.is_typedef())
	{
		auto const &typedef_type = this->get_typedef(t.get_typedef());
		t = typedef_type.aliased_type;
	}

	bz_assert(t.is_pointer());
	return t.get_pointer();
}

bool codegen_context::is_struct(type t) const
{
	while (t.is_typedef())
	{
		auto const &typedef_type = this->get_typedef(t.get_typedef());
		t = typedef_type.aliased_type;
	}

	return t.is_struct();
}

struct_type_t const *codegen_context::maybe_get_struct(type t) const
{
	while (t.is_typedef())
	{
		auto const &typedef_type = this->get_typedef(t.get_typedef());
		t = typedef_type.aliased_type;
	}

	if (t.is_struct())
	{
		return &this->get_struct(t.get_struct());
	}
	else
	{
		return nullptr;
	}
}

bool codegen_context::is_array(type t) const
{
	while (t.is_typedef())
	{
		auto const &typedef_type = this->get_typedef(t.get_typedef());
		t = typedef_type.aliased_type;
	}

	return t.is_array();
}

array_type_t const *codegen_context::maybe_get_array(type t) const
{
	while (t.is_typedef())
	{
		auto const &typedef_type = this->get_typedef(t.get_typedef());
		t = typedef_type.aliased_type;
	}

	if (t.is_array())
	{
		return &this->get_array(t.get_array());
	}
	else
	{
		return nullptr;
	}
}

std::pair<bool, codegen_context::struct_infos_iterator> codegen_context::should_resolve_struct(ast::type_info const &info)
{
	auto const it = this->struct_infos.find(&info);
	return { it == this->struct_infos.end() || it->second.is_unresolved, it };
}

void codegen_context::generate_struct_body(type::struct_reference struct_ref)
{
	auto const &inserted_struct = this->type_set.get_struct_type(struct_ref);
	auto const struct_name = this->type_set.get_struct_type_name(struct_ref);

	this->struct_forward_declarations_string += bz::format("typedef struct {0} {0};\n", struct_name);

	this->struct_bodies_string += bz::format("struct {}\n", struct_name);
	this->struct_bodies_string += "{\n";

	// empty struct is a GNU extension for C, so it's better to avoid it if we can
	if (inserted_struct.members.empty())
	{
		this->struct_bodies_string += this->indentation;
		this->struct_bodies_string += bz::format("{} a;\n", this->to_string(this->get_uint8()));
	}
	else
	{
		for (auto const i : bz::iota(0, inserted_struct.members.size()))
		{
			this->struct_bodies_string += this->indentation;
			this->struct_bodies_string += bz::format("{} {};\n", this->to_string(inserted_struct.members[i]), this->get_member_name(i));
		}
	}
	this->struct_bodies_string += "};\n";
}

void codegen_context::generate_struct_forward_declaration(type::struct_reference struct_ref)
{
	auto const struct_name = this->type_set.get_struct_type_name(struct_ref);

	this->struct_forward_declarations_string += bz::format("typedef struct {0} {0};\n", struct_name);
}

type::struct_reference codegen_context::add_struct(ast::type_info const &info, struct_infos_iterator it, struct_type_t struct_type)
{
	if (it == this->struct_infos.end())
	{
		it = this->struct_infos.find(&info);
	}
	bz_assert(it != this->struct_infos.end() || !this->struct_infos.contains(&info));
	auto const ref = [&]() {
		if (it == this->struct_infos.end())
		{
			auto const ref = this->type_set.add_unique_struct(std::move(struct_type));
			this->type_set.add_struct_type_name(ref, this->make_type_name(info));
			this->struct_infos.insert({
				&info,
				struct_info_t{
					.struct_ref = ref,
					.typedef_ref = type::typedef_reference::invalid(),
					.is_unresolved = false,
				},
			});
			return ref;
		}
		else
		{
			auto const ref = it->second.struct_ref;
			this->type_set.modify_struct(ref, std::move(struct_type));
			return ref;
		}
	}();

	if (!info.is_libc_struct())
	{
		this->generate_struct_body(ref);
	}

	return ref;
}

type::struct_reference codegen_context::add_unresolved_struct(ast::type_info const &info)
{
	bz_assert(!this->struct_infos.contains(&info));
	auto const ref = this->type_set.add_unique_struct({});
	this->type_set.add_struct_type_name(ref, this->make_type_name(info));
	this->struct_infos.insert({
		&info,
		struct_info_t{
			.struct_ref = ref,
			.typedef_ref = type::typedef_reference::invalid(),
			.is_unresolved = true,
		},
	});

	return ref;
}

type::struct_reference codegen_context::add_struct_forward_declaration(ast::type_info const &info)
{
	if (auto const it = this->struct_infos.find(&info); it != this->struct_infos.end())
	{
		return it->second.struct_ref;
	}
	auto const ref = this->type_set.add_unique_struct({});
	this->type_set.add_struct_type_name(ref, this->make_type_name(info));
	this->struct_infos.insert({
		&info,
		struct_info_t{
			.struct_ref = ref,
			.typedef_ref = type::typedef_reference::invalid(),
			.is_unresolved = false,
		},
	});

	if (!info.is_libc_struct())
	{
		this->generate_struct_forward_declaration(ref);
	}

	return ref;
}

type::struct_reference codegen_context::add_struct(struct_type_t struct_type)
{
	auto const [ref, inserted] = this->type_set.add_struct_type(std::move(struct_type));
	if (!inserted)
	{
		return ref;
	}

	this->type_set.add_struct_type_name(ref, this->make_type_name());

	auto const &inserted_struct = this->type_set.get_struct_type(ref);
	auto const struct_name = this->type_set.get_struct_type_name(ref);

	this->struct_forward_declarations_string += bz::format("typedef struct {0} {0};\n", struct_name);

	this->struct_bodies_string += bz::format("struct {}\n", struct_name);
	this->struct_bodies_string += "{\n";
	for (auto const i : bz::iota(0, inserted_struct.members.size()))
	{
		this->struct_bodies_string += this->indentation;
		this->struct_bodies_string += bz::format("{} {};\n", this->to_string(inserted_struct.members[i]), this->get_member_name(i));
	}
	this->struct_bodies_string += "};\n";

	return ref;
}

type::typedef_reference codegen_context::add_typedef(typedef_type_t typedef_type)
{
	auto const [ref, inserted] = this->type_set.add_typedef_type(std::move(typedef_type));
	if (!inserted)
	{
		return ref;
	}

	this->type_set.add_typedef_type_name(ref, this->make_type_name());

	auto const &inserted_typedef = this->type_set.get_typedef_type(ref);
	auto const typedef_name = this->type_set.get_typedef_type_name(ref);
	this->typedefs_string += bz::format("typedef {} {};\n", this->to_string(inserted_typedef.aliased_type), typedef_name);

	return ref;
}

type::array_reference codegen_context::add_array(array_type_t array_type)
{
	auto const [ref, inserted] = this->type_set.add_array_type(std::move(array_type));
	if (!inserted)
	{
		return ref;
	}

	this->type_set.add_array_type_name(ref, this->make_type_name());

	auto const &inserted_array = this->type_set.get_array_type(ref);
	auto const array_name = this->type_set.get_array_type_name(ref);

	this->struct_forward_declarations_string += bz::format("typedef struct {0} {0};\n", array_name);

	this->struct_bodies_string += bz::format("struct {}\n", array_name);
	this->struct_bodies_string += "{\n";
	this->struct_bodies_string += this->indentation;
	this->struct_bodies_string += bz::format("{} a[{}];\n", this->to_string(inserted_array.elem_type), inserted_array.size);
	this->struct_bodies_string += "};\n";

	return ref;
}

type::function_reference codegen_context::add_function(function_type_t function_type)
{
	auto const [ref, inserted] = this->type_set.add_function_type(std::move(function_type));
	if (!inserted)
	{
		return ref;
	}

	this->type_set.add_function_type_name(ref, this->make_type_name());

	auto const &inserted_function = this->type_set.get_function_type(ref);
	auto const function_name = this->type_set.get_function_type_name(ref);

	this->typedefs_string += bz::format("typedef {} (*{})(", this->to_string(inserted_function.return_type), function_name);
	if (inserted_function.param_types.not_empty())
	{
		this->typedefs_string += this->to_string(inserted_function.param_types[0]);
		for (auto const param_type : inserted_function.param_types.slice(1))
		{
			this->typedefs_string += ", ";
			this->typedefs_string += this->to_string(param_type);
		}
	}
	this->typedefs_string += ");\n";

	return ref;
}

struct_type_t const &codegen_context::get_struct(type::struct_reference struct_ref) const
{
	return this->type_set.get_struct_type(struct_ref);
}

typedef_type_t const &codegen_context::get_typedef(type::typedef_reference typedef_ref) const
{
	return this->type_set.get_typedef_type(typedef_ref);
}

array_type_t const &codegen_context::get_array(type::array_reference array_ref) const
{
	return this->type_set.get_array_type(array_ref);
}

function_type_t const &codegen_context::get_function(type::function_reference function_ref) const
{
	return this->type_set.get_function_type(function_ref);
}

type::typedef_reference codegen_context::add_builtin_type(
	ast::type_info const &info,
	bz::u8string_view typedef_type_name,
	bz::u8string_view aliased_type_name
)
{
	auto const struct_ref = this->type_set.add_unique_struct({});
	this->type_set.add_struct_type_name(struct_ref, aliased_type_name);
	auto const typedef_ref = this->add_builtin_typedef(typedef_type_name, { .aliased_type = type(struct_ref) });
	this->struct_infos.insert({
		&info,
		struct_info_t{
			.struct_ref = struct_ref,
			.typedef_ref = typedef_ref,
			.is_unresolved = false,
		},
	});

	this->typedefs_string += bz::format("typedef {} {};\n", aliased_type_name, typedef_type_name);

	return typedef_ref;
}

type::typedef_reference codegen_context::add_char_typedef(ast::type_info const &info, bz::u8string_view typedef_type_name)
{
	bz_assert(this->builtin_types.uint32_ != type::typedef_reference::invalid());
	auto const uint32_name = this->type_set.get_typedef_type_name(this->builtin_types.uint32_);

	auto const ref = this->type_set.add_unique_typedef({ .aliased_type = this->get_uint32() });
	this->type_set.add_typedef_type_name(ref, typedef_type_name);
	this->struct_infos.insert({
		&info,
		struct_info_t{
			.struct_ref = type::struct_reference::invalid(),
			.typedef_ref = ref,
			.is_unresolved = false,
		},
	});

	this->typedefs_string += bz::format("typedef {} {};\n", uint32_name, typedef_type_name);

	return ref;
}

type::typedef_reference codegen_context::add_builtin_typedef(bz::u8string typedef_type_name, typedef_type_t typedef_type)
{
	auto const ref = this->type_set.add_unique_typedef(std::move(typedef_type));
	this->type_set.add_typedef_type_name(ref, std::move(typedef_type_name));
	return ref;
}

bz::u8string codegen_context::to_string(type t) const
{
	bz::u8string result = "";
	switch (t.terminator.index())
	{
	static_assert(type::type_terminator_t::variant_count == 4);
	case type::type_terminator_t::index_of<type::struct_reference>:
		result += this->type_set.get_struct_type_name(t.terminator.get<type::struct_reference>());
		break;
	case type::type_terminator_t::index_of<type::typedef_reference>:
		result += this->type_set.get_typedef_type_name(t.terminator.get<type::typedef_reference>());
		break;
	case type::type_terminator_t::index_of<type::array_reference>:
		result += this->type_set.get_array_type_name(t.terminator.get<type::array_reference>());
		break;
	case type::type_terminator_t::index_of<type::function_reference>:
		result += this->type_set.get_function_type_name(t.terminator.get<type::function_reference>());
		break;
	}

	for (auto const kind : t.modifier_info.reversed_range())
	{
		if (kind == type_modifier::pointer)
		{
			result += '*';
		}
		else
		{
			result += " const *";
		}
	}

	return result;
}

bz::u8string codegen_context::create_cstring(bz::u8string_view s)
{
	auto name = this->make_global_variable_name();
	auto const uint8_name = this->type_set.get_typedef_type_name(this->builtin_types.uint8_);

	this->variables_string += bz::format("static {} const * const {} = ({0} const *)\"", uint8_name, name);

	auto it = s.begin();
	auto const end = s.end();
	auto begin = it;
	for (; it != end; ++it)
	{
		auto const c = *it;
		// https://en.cppreference.com/w/c/language/escape
		if (c < ' ' || c == '\x7f' || c == '\'' || c == '\"' || c == '\\')
		{
			this->variables_string += bz::u8string_view(begin, it);
			switch (c)
			{
			case '\'':
				this->variables_string += "\\\'";
				break;
			case '\"':
				this->variables_string += "\\\"";
				break;
			case '\\':
				this->variables_string += "\\\\";
				break;
			case '\a':
				this->variables_string += "\\a";
				break;
			case '\b':
				this->variables_string += "\\b";
				break;
			case '\f':
				this->variables_string += "\\f";
				break;
			case '\n':
				this->variables_string += "\\n";
				break;
			case '\r':
				this->variables_string += "\\r";
				break;
			case '\t':
				this->variables_string += "\\t";
				break;
			case '\v':
				this->variables_string += "\\v";
				break;
			default:
				this->variables_string += bz::format("\\x{:02x}", c);
				break;
			}
			begin = it + 1;
		}
		else if (c > '\x7f' && c <= 0xffff)
		{
			this->variables_string += bz::u8string_view(begin, it);
			this-> variables_string += bz::format("\\u{:04x}", c);
			begin = it + 1;
		}
		else if (c > 0xffff)
		{
			this->variables_string += bz::u8string_view(begin, it);
			this-> variables_string += bz::format("\\U{:08x}", c);
			begin = it + 1;
		}
	}
	this->variables_string += bz::u8string_view(begin, it);

	this->variables_string += "\";\n";

	return name;
}

void codegen_context::add_global_variable(ast::decl_variable const &var_decl, type var_type, bz::u8string_view initializer)
{
	bz_assert(!this->global_variables.contains(&var_decl));
	auto name = this->make_global_variable_name(var_decl);

	if (!var_decl.is_libc_variable())
	{
		auto const visibility = var_decl.is_extern() ? "extern "
			: var_decl.is_external_linkage() ? ""
			: "static ";
		auto const mutability = var_decl.get_type().is_mut() ? "" : " const";
		this->variables_string += bz::format("{}{}{} {}", visibility, this->to_string(var_type), mutability, name);
		if (initializer != "")
		{
			this->variables_string += " = ";
			this->variables_string += initializer;
		}
		this->variables_string += ";\n";
	}

	this->global_variables.insert({ &var_decl, global_variable_t{ std::move(name), var_type } });
}

codegen_context::global_variable_t const &codegen_context::get_global_variable(ast::decl_variable const &var_decl) const
{
	bz_assert(this->global_variables.contains(&var_decl));
	return this->global_variables.at(&var_decl);
}

void codegen_context::ensure_function_generation(ast::function_body *func_body)
{
	// no need to emit functions with no definition
	if (func_body->body.is_null())
	{
		return;
	}

	if (!func_body->is_bitcode_emitted())
	{
		this->functions_to_compile.push_back(func_body);
	}
}

void codegen_context::reset_current_function(ast::function_body &func_body)
{
	this->current_function_info = current_function_info_t();
	this->current_function_info.func_body = &func_body;
}

codegen_context::function_info_t const &codegen_context::get_function(ast::function_body &func_body)
{
	if (auto const it = this->functions.find(&func_body); it != this->functions.end())
	{
		return it->second;
	}

	auto const [it, inserted] = this->functions.insert({
		&func_body,
		function_info_t{
			.name = this->make_function_name(func_body),
		}
	});
	return it->second;
}

void codegen_context::add_indentation(void)
{
	for ([[maybe_unused]] auto const _ : bz::iota(0, this->current_function_info.indent_level))
	{
		this->current_function_info.body_string += this->indentation;
	}
}

bz::u8string codegen_context::to_string(expr_value const &value) const
{
	bz::u8string result = "";
	if (value.needs_dereference)
	{
		result += '*';
	}

	result += this->make_local_name(value.value_index);

	return result;
}

bz::u8string codegen_context::to_string_lhs(expr_value const &value, precedence prec) const
{
	bz::u8string result = "";

	auto const needs_parens = needs_parenthesis_binary_lhs(value, prec);
	if (needs_parens)
	{
		result += '(';
	}

	if (value.needs_dereference)
	{
		result += '*';
	}

	result += this->make_local_name(value.value_index);

	if (needs_parens)
	{
		result += ')';
	}

	return result;
}

bz::u8string codegen_context::to_string_rhs(expr_value const &value, precedence prec) const
{
	bz::u8string result = "";

	auto const needs_parens = needs_parenthesis_binary_rhs(value, prec);
	if (needs_parens)
	{
		result += '(';
	}

	if (value.needs_dereference)
	{
		result += '*';
	}

	result += this->make_local_name(value.value_index);

	if (needs_parens)
	{
		result += ')';
	}

	return result;
}

bz::u8string codegen_context::to_string_unary(expr_value const &value, precedence prec) const
{
	bz::u8string result = "";

	auto const needs_parens = needs_parenthesis_unary(value, prec);
	if (needs_parens)
	{
		result += '(';
	}

	if (value.needs_dereference)
	{
		result += '*';
	}

	result += this->make_local_name(value.value_index);

	if (needs_parens)
	{
		result += ')';
	}

	return result;
}

bz::u8string codegen_context::to_string_binary(expr_value const &lhs, expr_value const &rhs, bz::u8string_view op, precedence prec) const
{
	bz::u8string result = "";

	auto const [lhs_needs_parens, rhs_needs_parens] = needs_parenthesis_binary(lhs, rhs, prec);

	// lhs
	{
		if (lhs_needs_parens)
		{
			result += '(';
		}

		if (lhs.needs_dereference)
		{
			result += '*';
		}

		result += this->make_local_name(lhs.value_index);

		if (lhs_needs_parens)
		{
			result += ')';
		}
	}

	result += ' ';
	result += op;
	result += ' ';

	// rhs
	{
		if (rhs_needs_parens)
		{
			result += '(';
		}

		if (rhs.needs_dereference)
		{
			result += '*';
		}

		result += this->make_local_name(rhs.value_index);

		if (rhs_needs_parens)
		{
			result += ')';
		}
	}

	return result;
}

bz::u8string codegen_context::to_string_unary_prefix(expr_value const &value, bz::u8string_view op) const
{
	bz::u8string result = op;

	auto const needs_parens = needs_parenthesis_unary(value, precedence::prefix);
	if (needs_parens)
	{
		result += '(';
	}

	if (value.needs_dereference)
	{
		result += '*';
	}

	result += this->make_local_name(value.value_index);

	if (needs_parens)
	{
		result += ')';
	}

	return result;
}

bz::u8string codegen_context::to_string_unary_suffix(expr_value const &value, bz::u8string_view op) const
{
	bz::u8string result = "";

	auto const needs_parens = needs_parenthesis_unary(value, precedence::suffix);
	if (needs_parens)
	{
		result += '(';
	}

	if (value.needs_dereference)
	{
		result += '*';
	}

	result += this->make_local_name(value.value_index);

	if (needs_parens)
	{
		result += ')';
	}

	result += op;

	return result;
}

void codegen_context::add_expression(bz::u8string_view expr_string)
{
	this->add_indentation();
	this->current_function_info.body_string += expr_string;
	this->current_function_info.body_string += ";\n";
}

expr_value codegen_context::add_uninitialized_value(type expr_type)
{
	auto const [name, index] = this->make_local_name();
	this->add_indentation();
	auto const type_string = this->to_string(expr_type);
	this->current_function_info.body_string += bz::format(
		"{} {};\n",
		type_string, name
	);

	return this->make_value_expression(index, expr_type);
}

expr_value codegen_context::add_value_expression(bz::u8string_view expr_string, type expr_type)
{
	auto const [name, index] = this->make_local_name();
	this->add_indentation();
	auto const type_string = this->to_string(expr_type);
	this->current_function_info.body_string += bz::format(
		"{} {} = {};\n",
		type_string, name, expr_string
	);

	return this->make_value_expression(index, expr_type);
}

expr_value codegen_context::add_reference_expression(bz::u8string_view expr_string, type expr_type, bool is_const)
{
	auto const [name, index] = this->make_local_name();
	this->add_indentation();
	auto const type_string = this->to_string(is_const ? this->add_const_pointer(expr_type) : this->add_pointer(expr_type));
	this->current_function_info.body_string += bz::format(
		"{} {} = {};\n",
		type_string, name, expr_string
	);

	return this->make_reference_expression(index, expr_type, is_const);
}

expr_value codegen_context::make_value_expression(uint32_t value_index, type value_type) const
{
	return expr_value{
		.value_index = value_index,
		.needs_dereference = false,
		.is_const = false,
		.prec = precedence::identifier,
		.value_type = value_type,
	};
}

expr_value codegen_context::make_reference_expression(uint32_t value_index, type value_type, bool is_const) const
{
	return expr_value{
		.value_index = value_index,
		.needs_dereference = true,
		.is_const = is_const,
		.prec = precedence::identifier,
		.value_type = value_type,
	};
}

void codegen_context::begin_if(expr_value condition)
{
	return this->begin_if(this->to_string(condition));
}

void codegen_context::begin_if_not(expr_value condition)
{
	bz::u8string condition_string = "!";
	condition_string += this->to_string_unary(condition, precedence::prefix);
	return this->begin_if(condition_string);
}

void codegen_context::begin_if(bz::u8string_view condition)
{
	this->add_indentation();
	this->current_function_info.body_string += "if (";
	this->current_function_info.body_string += condition;
	this->current_function_info.body_string += ")\n";
	this->add_indentation();
	this->current_function_info.body_string += "{\n";
	this->current_function_info.indent_level += 1;
}

void codegen_context::end_if(void)
{
	this->current_function_info.indent_level -= 1;
	this->add_indentation();
	this->current_function_info.body_string += "}\n";
}

void codegen_context::begin_while(expr_value condition)
{
	return this->begin_while(this->to_string(condition));
}

void codegen_context::begin_while(bz::u8string_view condition)
{
	this->add_indentation();
	this->current_function_info.body_string += "while (";
	this->current_function_info.body_string += condition;
	this->current_function_info.body_string += ")\n";
	this->add_indentation();
	this->current_function_info.body_string += "{\n";
	this->current_function_info.indent_level += 1;
}

void codegen_context::end_while(void)
{
	this->current_function_info.indent_level -= 1;
	this->add_indentation();
	this->current_function_info.body_string += "}\n";
}

void codegen_context::add_return(void)
{
	this->add_expression("return");
}

void codegen_context::add_return(expr_value value)
{
	return this->add_return(this->to_string(value));
}

void codegen_context::add_return(bz::u8string_view value)
{
	this->add_expression(bz::format("return {}", value));
}

void codegen_context::add_local_variable(ast::decl_variable const &var_decl, expr_value value)
{
	bz_assert(!this->current_function_info.local_variables.contains(&var_decl));
	this->current_function_info.local_variables.insert({ &var_decl, value });
}

expr_value codegen_context::get_void_value(void) const
{
	return this->make_value_expression(0, this->get_void());
}

static void remove_needs_dereference(expr_value &value, codegen_context &context)
{
	value.needs_dereference = false;
	value.value_type = context.add_pointer(value.value_type, value.is_const ? type_modifier::const_pointer : type_modifier::pointer);
	value.is_const = false;
}

expr_value codegen_context::create_struct_gep(expr_value value, size_t index)
{
	auto const type = value.get_type();
	if (auto const struct_type = this->maybe_get_struct(type))
	{
		bz_assert(index < struct_type->members.size());
		auto const result_type = struct_type->members[index];
		auto const use_arrow = value.needs_dereference;
		remove_needs_dereference(value, *this);

		// address of has a lower precedence than suffix, so this is fine
		bz::u8string member_access_string = "&";
		member_access_string += this->to_string_unary(value, precedence::suffix);
		if (use_arrow)
		{
			member_access_string += "->";
		}
		else
		{
			member_access_string += '.';
		}
		member_access_string += this->get_member_name(index);
		return this->add_reference_expression(member_access_string, result_type, value.is_const);
	}
	else if (auto const array_type = this->maybe_get_array(type))
	{
		bz_assert(index <= array_type->size);
		auto const result_type = array_type->elem_type;
		auto const use_arrow = value.needs_dereference;
		remove_needs_dereference(value, *this);

		bz::u8string member_access_string = this->to_string_unary(value, precedence::suffix);
		if (use_arrow)
		{
			member_access_string += bz::format("->a + {}", index);
		}
		else
		{
			member_access_string += bz::format(".a + {}", index);
		}
		return this->add_reference_expression(member_access_string, result_type, value.is_const);
	}
	else
	{
		bz_unreachable;
	}
}

expr_value codegen_context::create_struct_gep_value(expr_value value, size_t index)
{
	auto const type = value.get_type();
	if (auto const struct_type = this->maybe_get_struct(type))
	{
		bz_assert(index < struct_type->members.size());
		auto const result_type = struct_type->members[index];
		auto const use_arrow = value.needs_dereference;
		remove_needs_dereference(value, *this);

		// address of has a lower precedence than suffix, so this is fine
		bz::u8string member_access_string = this->to_string_unary(value, precedence::suffix);
		if (use_arrow)
		{
			member_access_string += "->";
		}
		else
		{
			member_access_string += '.';
		}
		member_access_string += this->get_member_name(index);
		return this->add_value_expression(member_access_string, result_type);
	}
	else if (auto const array_type = this->maybe_get_array(type))
	{
		bz_assert(index < array_type->size);
		auto const result_type = array_type->elem_type;
		auto const use_arrow = value.needs_dereference;
		remove_needs_dereference(value, *this);

		bz::u8string member_access_string = this->to_string_unary(value, precedence::suffix);
		if (use_arrow)
		{
			member_access_string += bz::format("->a[{}]", index);
		}
		else
		{
			member_access_string += bz::format(".a[{}]", index);
		}
		return this->add_value_expression(member_access_string, result_type);
	}
	else
	{
		bz_unreachable;
	}
}

expr_value codegen_context::create_dereference(expr_value value)
{
	bz_assert(this->is_pointer(value.get_type()));
	auto const [result_type, modifier] = this->remove_pointer(value.get_type());
	if (!value.needs_dereference)
	{
		return this->make_reference_expression(value.value_index, result_type, modifier == type_modifier::const_pointer);
	}
	else
	{
		return this->add_reference_expression(this->to_string_rhs(value, precedence::assignment), result_type, modifier == type_modifier::const_pointer);
	}
}

expr_value codegen_context::create_address_of(expr_value value)
{
	auto const result_type = value.is_const ? this->add_const_pointer(value.get_type()) : this->add_pointer(value.get_type());
	if (value.needs_dereference)
	{
		return this->make_value_expression(value.value_index, result_type);
	}
	else
	{
		return this->add_value_expression(this->to_string_unary_prefix(value, "&"), result_type);
	}
}

void codegen_context::create_binary_operation(expr_value const &lhs, expr_value const &rhs, bz::u8string_view op, precedence prec)
{
	this->add_expression(this->to_string_binary(lhs, rhs, op, prec));
}

void codegen_context::push_destruct_operation(ast::destruct_operation const &destruct_op)
{
	auto const move_destruct_indicator = this->get_move_destruct_indicator(destruct_op.move_destructed_decl);
	if (move_destruct_indicator.has_value() || destruct_op.not_null())
	{
		this->current_function_info.destructor_calls.push_back({
			.destruct_op = &destruct_op,
			.value = this->get_void_value(),
			.condition = {},
			.move_destruct_indicator = move_destruct_indicator,
			.rvalue_array_elem_ptr = {},
		});
	}
}

void codegen_context::push_self_destruct_operation(ast::destruct_operation const &destruct_op, expr_value value)
{
	auto const move_destruct_indicator = this->get_move_destruct_indicator(destruct_op.move_destructed_decl);
	if (move_destruct_indicator.has_value() || destruct_op.not_null())
	{
		this->current_function_info.destructor_calls.push_back({
			.destruct_op = &destruct_op,
			.value = value,
			.condition = {},
			.move_destruct_indicator = move_destruct_indicator,
			.rvalue_array_elem_ptr = {},
		});
	}
}

void codegen_context::push_variable_destruct_operation(
	ast::destruct_operation const &destruct_op,
	expr_value value,
	bz::optional<expr_value> move_destruct_indicator
)
{
	this->current_function_info.destructor_calls.push_back({
		.destruct_op = &destruct_op,
		.value = value,
		.condition = move_destruct_indicator,
		.move_destruct_indicator = {},
		.rvalue_array_elem_ptr = {},
	});
}

expr_value codegen_context::add_move_destruct_indicator(ast::decl_variable const &decl)
{
	auto const result = this->add_value_expression("1", this->get_bool());
	this->current_function_info.move_destruct_indicators.insert({ &decl, result });
	return result;
}

bz::optional<expr_value> codegen_context::get_move_destruct_indicator(ast::decl_variable const *decl) const
{
	if (decl == nullptr)
	{
		return {};
	}

	auto const it = this->current_function_info.move_destruct_indicators.find(decl);
	if (it == this->current_function_info.move_destruct_indicators.end())
	{
		return {};
	}
	else
	{
		return it->second;
	}
}

bz::optional<expr_value> codegen_context::get_move_destruct_indicator(ast::decl_variable const &decl) const
{
	auto const it = this->current_function_info.move_destruct_indicators.find(&decl);
	if (it == this->current_function_info.move_destruct_indicators.end())
	{
		return {};
	}
	else
	{
		return it->second;
	}
}

void codegen_context::generate_destruct_operations(size_t destruct_calls_start_index)
{
	for (auto const index : bz::iota(
		destruct_calls_start_index,
		this->current_function_info.destructor_calls.size()
	).reversed())
	{
		auto const &info = this->current_function_info.destructor_calls[index];
		generate_destruct_operation(info, *this);
	}
}

void codegen_context::generate_loop_destruct_operations(void)
{
	this->generate_destruct_operations(this->current_function_info.loop_info.destructor_stack_begin);
}

void codegen_context::generate_all_destruct_operations(void)
{
	this->generate_destruct_operations(0);
}

[[nodiscard]] codegen_context::expression_scope_info_t codegen_context::push_expression_scope(void)
{
	return {
		.destructor_calls_size = this->current_function_info.destructor_calls.size()
	};
}

void codegen_context::pop_expression_scope(expression_scope_info_t prev_info)
{
	this->generate_destruct_operations(prev_info.destructor_calls_size);
	this->current_function_info.destructor_calls.resize(prev_info.destructor_calls_size);
}

[[nodiscard]] codegen_context::loop_info_t codegen_context::push_loop(void)
{
	auto const prev_info = this->current_function_info.loop_info;
	this->current_function_info.loop_info = {
		.destructor_stack_begin = this->current_function_info.destructor_calls.size()
	};
	return prev_info;
}

void codegen_context::pop_loop(loop_info_t prev_info)
{
	this->current_function_info.loop_info = prev_info;
}

[[nodiscard]] expr_value codegen_context::push_value_reference(expr_value new_value)
{
	auto const index = this->current_function_info.value_reference_stack_size % this->current_function_info.value_references.size();
	this->current_function_info.value_reference_stack_size += 1;
	auto const result = this->current_function_info.value_references[index];
	this->current_function_info.value_references[index] = new_value;
	return result;
}

void codegen_context::pop_value_reference(expr_value prev_value)
{
	bz_assert(this->current_function_info.value_reference_stack_size > 0);
	this->current_function_info.value_reference_stack_size -= 1;
	auto const index = this->current_function_info.value_reference_stack_size % this->current_function_info.value_references.size();
	this->current_function_info.value_references[index] = prev_value;
}

expr_value codegen_context::get_value_reference(size_t index)
{
	bz_assert(index < this->current_function_info.value_reference_stack_size);
	bz_assert(index < this->current_function_info.value_references.size());
	auto const stack_index = (this->current_function_info.value_reference_stack_size - index - 1) % this->current_function_info.value_references.size();
	return this->current_function_info.value_references[stack_index];
}

bz::u8string codegen_context::get_code_string(void) const
{
	bz::u8string result = "";

	for (auto const &header : this->included_headers)
	{
		result += bz::format("#include <{}>\n", header);
	}

	result += this->struct_forward_declarations_string;
	result += this->typedefs_string;
	result += this->struct_bodies_string;
	result += this->function_declarations_string;
	result += this->variables_string;
	result += this->function_bodies_string;

	return result;
}

} // namespace codegen::c
