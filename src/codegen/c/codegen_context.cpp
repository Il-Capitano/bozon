#include "codegen_context.h"
#include "codegen.h"
#include "ast/statement.h"
#include "ctx/global_context.h"

namespace codegen::c
{

codegen_context::codegen_context(ctx::global_context &global_ctx, target_properties props)
	: indentation("\t"),
	  global_ctx(global_ctx)
{
	bz_assert(props.c_short_size.has_value());
	bz_assert(props.c_int_size.has_value());
	bz_assert(props.c_long_size.has_value());
	bz_assert(props.c_long_long_size.has_value());
	bz_assert(props.pointer_size.has_value());
	bz_assert(props.endianness.has_value());
	this->short_size = props.c_short_size.get();
	this->int_size = props.c_int_size.get();
	this->long_size = props.c_long_size.get();
	this->long_long_size = props.c_long_long_size.get();
	this->pointer_size = props.pointer_size.get();
	this->endianness = props.endianness.get();

	this->builtin_types.void_ = this->type_set.add_unique_typedef({
		.aliased_type = {},
	});
	this->type_set.add_typedef_type_name(this->builtin_types.void_, "void");

	auto const void_pointer = this->add_const_pointer(type(this->builtin_types.void_));
	this->builtin_types.slice = this->add_struct({
		.members = { void_pointer, void_pointer },
	});
}

ast::function_body *codegen_context::get_builtin_function(uint32_t kind) const
{
	auto const decl = this->global_ctx.get_builtin_function(kind);
	return decl == nullptr ? nullptr : &decl->body;
}

bz::u8string codegen_context::get_location_string(lex::src_tokens const &src_tokens) const
{
	return this->global_ctx.get_location_string(src_tokens.pivot);
}

bool codegen_context::is_little_endian(void) const
{
	return this->endianness == comptime::memory::endianness_kind::little;
}

bool codegen_context::is_big_endian(void) const
{
	return this->endianness == comptime::memory::endianness_kind::big;
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
	else if (func_body.function_name_or_operator_kind.is<uint32_t>())
	{
		auto const op_kind = func_body.function_name_or_operator_kind.get<uint32_t>();
		return bz::format("o_{}_{:x}", op_kind, this->get_unique_number());
	}
	else
	{
		return bz::format("f_{:x}", this->get_unique_number());
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

bz::u8string codegen_context::make_goto_label(size_t index) const
{
	return bz::format("l_{:x}", index);
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

type codegen_context::get_isize(void) const
{
	switch (this->pointer_size)
	{
	case 1:
		return this->get_int8();
	case 2:
		return this->get_int16();
	case 4:
		return this->get_int32();
	case 8:
		return this->get_int64();
	default:
		bz_unreachable;
	}
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

type codegen_context::get_usize(void) const
{
	switch (this->pointer_size)
	{
	case 1:
		return this->get_uint8();
	case 2:
		return this->get_uint16();
	case 4:
		return this->get_uint32();
	case 8:
		return this->get_uint64();
	default:
		bz_unreachable;
	}
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

type codegen_context::get_slice(void) const
{
	return type(this->builtin_types.slice);
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

bool codegen_context::is_slice(type t) const
{
	while (t.is_typedef())
	{
		auto const &typedef_type = this->get_typedef(t.get_typedef());
		t = typedef_type.aliased_type;
	}

	return t.is_slice();
}

slice_type_t const *codegen_context::maybe_get_slice(type t) const
{
	while (t.is_typedef())
	{
		auto const &typedef_type = this->get_typedef(t.get_typedef());
		t = typedef_type.aliased_type;
	}

	if (t.is_slice())
	{
		return &this->get_slice(t.get_slice());
	}
	else
	{
		return nullptr;
	}
}

bool codegen_context::is_function(type t) const
{
	while (t.is_typedef())
	{
		auto const &typedef_type = this->get_typedef(t.get_typedef());
		t = typedef_type.aliased_type;
	}

	return t.is_function();
}

function_type_t const *codegen_context::maybe_get_function(type t) const
{
	while (t.is_typedef())
	{
		auto const &typedef_type = this->get_typedef(t.get_typedef());
		t = typedef_type.aliased_type;
	}

	if (t.is_function())
	{
		return &this->get_function(t.get_function());
	}
	else
	{
		return nullptr;
	}
}

bool codegen_context::is_pointer_or_function(type t) const
{
	while (t.is_typedef())
	{
		auto const &typedef_type = this->get_typedef(t.get_typedef());
		t = typedef_type.aliased_type;
	}

	return t.is_pointer() || t.is_function();
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

type::slice_reference codegen_context::add_slice(slice_type_t slice_type)
{
	auto const [ref, inserted] = this->type_set.add_slice_type(std::move(slice_type));
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
	else
	{
		this->typedefs_string += "void";
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

slice_type_t const &codegen_context::get_slice(type::slice_reference slice_ref) const
{
	return this->type_set.get_slice_type(slice_ref);
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
	static_assert(type::type_terminator_t::variant_count == 5);
	case type::type_terminator_t::index_of<type::struct_reference>:
		result += this->type_set.get_struct_type_name(t.terminator.get<type::struct_reference>());
		break;
	case type::type_terminator_t::index_of<type::typedef_reference>:
		result += this->type_set.get_typedef_type_name(t.terminator.get<type::typedef_reference>());
		break;
	case type::type_terminator_t::index_of<type::array_reference>:
		result += this->type_set.get_array_type_name(t.terminator.get<type::array_reference>());
		break;
	case type::type_terminator_t::index_of<type::slice_reference>:
		result += this->type_set.get_struct_type_name(this->builtin_types.slice);
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

bz::u8string_view codegen_context::add_panic_string(bz::u8string s)
{
	return this->panic_strings_storage.push_back(std::move(s));
}

bz::u8string_view codegen_context::create_cstring(bz::u8string_view s)
{
	auto const [literals_it, inserted] = this->string_literals.insert({ s, bz::u8string() });
	if (!inserted)
	{
		return literals_it->second;
	}

	literals_it->second = this->make_global_variable_name();
	auto const &name = literals_it->second;
	auto const uint8_name = this->type_set.get_typedef_type_name(this->builtin_types.uint8_);

	this->variables_string += bz::format("static {} const {}[] = \"", uint8_name, name);

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

void codegen_context::ensure_function_generation(ast::function_body &func_body)
{
	if (!func_body.is_bitcode_emitted())
	{
		this->functions_to_compile.push_back(&func_body);
	}
}

void codegen_context::reset_current_function(ast::function_body &func_body)
{
	this->current_function_info = current_function_info_t();
	this->current_function_info.func_body = &func_body;
}

codegen_context::function_info_t const &codegen_context::get_function(ast::function_body &func_body_)
{
	bz_assert(!func_body_.is_intrinsic() || func_body_.intrinsic_kind != ast::function_body::builtin_call_main || this->global_ctx._main != nullptr);
	auto &func_body = func_body_.is_intrinsic() && func_body_.intrinsic_kind == ast::function_body::builtin_call_main
		? *this->global_ctx._main
		: func_body_;
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
	this->ensure_function_generation(func_body);
	return it->second;
}

ast::function_body &codegen_context::get_main_func_body(void) const
{
	bz_assert(this->global_ctx._main != nullptr);
	return *this->global_ctx._main;
}

static ast::attribute const &get_libc_macro_attribute(ast::function_body const &func_body)
{
	bz_assert(func_body.is_libc_macro());
	return func_body.attributes.filter([](ast::attribute const &attr) {
		return attr.name->value == "__libc_macro";
	}).front();
}

bz::u8string_view codegen_context::get_libc_macro_name(ast::function_body &func_body)
{
	auto const &attribute = get_libc_macro_attribute(func_body);
	bz_assert(attribute.args.size() == 2);
	auto const header = attribute.args[0].get_constant_value().get_string();
	this->add_libc_header(header);
	return attribute.args[1].get_constant_value().get_string();
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

bz::u8string codegen_context::to_string_binary(expr_value const &lhs, bz::u8string_view rhs, bz::u8string_view op, precedence prec) const
{
	bz::u8string result = "";

	auto const lhs_needs_parens = needs_parenthesis_binary_lhs(lhs, prec);

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
	result += rhs;

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

bz::u8string codegen_context::to_string_arg(expr_value const &value) const
{
	return this->to_string_lhs(value, precedence::comma);
}

bz::u8string codegen_context::to_string_struct_literal(type aggregate_type, bz::array_view<expr_value const> values) const
{
	bz::u8string result = "(";
	result += this->to_string(aggregate_type);
	if (values.empty())
	{
		result += "){0}";
	}
	else
	{
		result += "){ ";
		for (auto const &value : values)
		{
			result += this->to_string_arg(value);
			result += ", ";
		}
		result += '}';
	}
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

[[nodiscard]] codegen_context::if_info_t codegen_context::begin_if(expr_value condition)
{
	return this->begin_if(this->to_string(condition));
}

[[nodiscard]] codegen_context::if_info_t codegen_context::begin_if_not(expr_value condition)
{
	bz::u8string condition_string = "!";
	condition_string += this->to_string_unary(condition, precedence::prefix);
	return this->begin_if(condition_string);
}

[[nodiscard]] codegen_context::if_info_t codegen_context::begin_if(bz::u8string_view condition)
{
	this->add_indentation();
	this->current_function_info.body_string += "if (";
	this->current_function_info.body_string += condition;
	this->current_function_info.body_string += ")\n";
	this->add_indentation();
	this->current_function_info.body_string += "{\n";
	this->current_function_info.indent_level += 1;

	auto const result = this->current_function_info.if_info;
	this->current_function_info.if_info = { .in_if = true };
	return result;
}

void codegen_context::begin_else(void)
{
	bz_assert(this->current_function_info.if_info.in_if);
	this->current_function_info.indent_level -= 1;

	this->add_indentation();
	this->current_function_info.body_string += "}\n";
	this->add_indentation();
	this->current_function_info.body_string += "else\n";
	this->add_indentation();
	this->current_function_info.body_string += "{\n";

	this->current_function_info.indent_level += 1;
}

void codegen_context::begin_else_if(expr_value condition)
{
	bz_assert(this->current_function_info.if_info.in_if);
	return this->begin_else_if(this->to_string(condition));
}

void codegen_context::begin_else_if(bz::u8string_view condition)
{
	bz_assert(this->current_function_info.if_info.in_if);
	this->current_function_info.indent_level -= 1;

	this->add_indentation();
	this->current_function_info.body_string += "}\n";
	this->add_indentation();
	this->current_function_info.body_string += "else if (";
	this->current_function_info.body_string += condition;
	this->current_function_info.body_string += ")\n";
	this->add_indentation();
	this->current_function_info.body_string += "{\n";

	this->current_function_info.indent_level += 1;
}

void codegen_context::end_if(if_info_t prev_if_info)
{
	bz_assert(this->current_function_info.if_info.in_if);
	this->current_function_info.if_info = prev_if_info;

	this->current_function_info.indent_level -= 1;
	this->add_indentation();
	this->current_function_info.body_string += "}\n";
}

[[nodiscard]] codegen_context::while_info_t codegen_context::begin_while(expr_value condition, bool may_need_continue_goto)
{
	return this->begin_while(this->to_string(condition));
}

[[nodiscard]] codegen_context::while_info_t codegen_context::begin_while(bz::u8string_view condition, bool may_need_continue_goto)
{
	this->add_indentation();
	this->current_function_info.body_string += "while (";
	this->current_function_info.body_string += condition;
	this->current_function_info.body_string += ")\n";
	this->add_indentation();
	this->current_function_info.body_string += "{\n";
	this->current_function_info.indent_level += 1;

	auto const result = this->current_function_info.while_info;
	this->current_function_info.loop_level += 1;
	this->current_function_info.while_info = {
		.needs_break_goto = false,
		.may_need_continue_goto = may_need_continue_goto,
		.needs_continue_goto = false,
		.break_goto_index = 0,
		.continue_goto_index = 0,
	};
	return result;
}

void codegen_context::end_while(while_info_t prev_while_info)
{
	this->current_function_info.indent_level -= 1;
	this->add_indentation();
	this->current_function_info.body_string += "}\n";

	if (this->current_function_info.while_info.needs_break_goto)
	{
		this->add_indentation();
		this->current_function_info.body_string += this->make_goto_label(this->current_function_info.while_info.break_goto_index);
		this->current_function_info.body_string += ":;\n";
	}

	bz_assert(this->current_function_info.loop_level != 0);
	this->current_function_info.loop_level -= 1;
	this->current_function_info.while_info = prev_while_info;
}

[[nodiscard]] codegen_context::switch_info_t codegen_context::begin_switch(expr_value value)
{
	return this->begin_switch(this->to_string(value));
}

[[nodiscard]] codegen_context::switch_info_t codegen_context::begin_switch(bz::u8string_view value)
{
	this->add_indentation();
	this->current_function_info.body_string += "switch (";
	this->current_function_info.body_string += value;
	this->current_function_info.body_string += ")\n";
	this->add_indentation();
	this->current_function_info.body_string += "{\n";

	auto const result = this->current_function_info.switch_info;
	this->current_function_info.switch_info = { .in_switch = true, .loop_level = this->current_function_info.loop_level };
	return result;
}

void codegen_context::add_case_label(bz::u8string_view value)
{
	bz_assert(this->current_function_info.switch_info.in_switch);
	this->add_indentation();
	this->current_function_info.body_string += "case ";
	this->current_function_info.body_string += value;
	this->current_function_info.body_string += ":\n";
}

void codegen_context::begin_case(void)
{
	bz_assert(this->current_function_info.switch_info.in_switch);
	this->add_indentation();
	this->current_function_info.body_string += "{\n";
	this->current_function_info.indent_level += 1;
}

void codegen_context::begin_default_case(void)
{
	bz_assert(this->current_function_info.switch_info.in_switch);
	this->add_indentation();
	this->current_function_info.body_string += "default:\n";
	this->add_indentation();
	this->current_function_info.body_string += "{\n";
	this->current_function_info.indent_level += 1;
}

void codegen_context::end_case(void)
{
	bz_assert(this->current_function_info.switch_info.in_switch);
	this->add_indentation();
	this->current_function_info.body_string += "break;\n";
	this->current_function_info.indent_level -= 1;
	this->add_indentation();
	this->current_function_info.body_string += "}\n";
}

void codegen_context::end_switch(switch_info_t prev_switch_info)
{
	bz_assert(this->current_function_info.switch_info.in_switch);
	this->current_function_info.switch_info = prev_switch_info;

	this->add_indentation();
	this->current_function_info.body_string += "}\n";
}

bz::optional<size_t> codegen_context::get_break_goto_index(void)
{
	if (
		this->current_function_info.switch_info.in_switch
		&& this->current_function_info.switch_info.loop_level == this->current_function_info.loop_level
	)
	{
		if (this->current_function_info.while_info.needs_break_goto)
		{
			return this->current_function_info.while_info.break_goto_index;
		}
		else
		{
			this->current_function_info.while_info.needs_break_goto = true;
			this->current_function_info.while_info.break_goto_index = this->get_unique_number();
			return this->current_function_info.while_info.break_goto_index;
		}
	}
	else
	{
		return {};
	}
}

bz::optional<size_t> codegen_context::get_continue_goto_index(void)
{
	if (this->current_function_info.while_info.needs_continue_goto)
	{
		return this->current_function_info.while_info.continue_goto_index;
	}
	else if (this->current_function_info.while_info.may_need_continue_goto)
	{
		this->current_function_info.while_info.needs_continue_goto = true;
		this->current_function_info.while_info.continue_goto_index = this->get_unique_number();
		return this->current_function_info.while_info.continue_goto_index;
	}
	else
	{
		return {};
	}
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

expr_value codegen_context::get_variable(ast::decl_variable const &var_decl)
{
	auto const local_it = this->current_function_info.local_variables.find(&var_decl);
	if (local_it != this->current_function_info.local_variables.end())
	{
		return local_it->second;
	}

	auto const global_it = this->global_variables.find(&var_decl);
	bz_assert(global_it != this->global_variables.end());
	auto const &[name, type] = global_it->second;
	return this->add_reference_expression(bz::format("&{}", name), type, !var_decl.get_type().is_mut());
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
	bz_assert(!this->is_slice(value.get_type()));
	auto const type = value.get_type();
	if (auto const struct_type = this->maybe_get_struct(type))
	{
		bz_assert(index < struct_type->members.size());
		auto const result_type = struct_type->members[index];
		auto const use_arrow = value.needs_dereference;
		auto const is_const = value.is_const;
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
		return this->add_reference_expression(member_access_string, result_type, is_const);
	}
	else if (auto const array_type = this->maybe_get_array(type))
	{
		bz_assert(index <= array_type->size);
		auto const result_type = array_type->elem_type;
		auto const use_arrow = value.needs_dereference;
		auto const is_const = value.is_const;
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
		return this->add_reference_expression(member_access_string, result_type, is_const);
	}
	else
	{
		bz_unreachable;
	}
}

expr_value codegen_context::create_struct_gep_pointer(expr_value value, size_t index)
{
	bz_assert(!this->is_slice(value.get_type()));
	auto const type = value.get_type();
	if (auto const struct_type = this->maybe_get_struct(type))
	{
		bz_assert(index < struct_type->members.size());
		auto const member_type = struct_type->members[index];
		auto const use_arrow = value.needs_dereference;
		auto const is_const = value.is_const;
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
		auto const result_type = is_const ? this->add_const_pointer(member_type) : this->add_pointer(member_type);
		return this->add_value_expression(member_access_string, result_type);
	}
	else if (auto const array_type = this->maybe_get_array(type))
	{
		bz_assert(index <= array_type->size);
		auto const elem_type = array_type->elem_type;
		auto const use_arrow = value.needs_dereference;
		auto const is_const = value.is_const;
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
		auto const result_type = is_const ? this->add_const_pointer(elem_type) : this->add_pointer(elem_type);
		return this->add_value_expression(member_access_string, result_type);
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
	else if (auto const slice_type = this->maybe_get_slice(type))
	{
		bz_assert(index < 2);
		auto const result_type = this->add_pointer(
			slice_type->elem_type,
			slice_type->is_const ? type_modifier::const_pointer : type_modifier::pointer
		);
		auto const use_arrow = value.needs_dereference;
		remove_needs_dereference(value, *this);

		bz::u8string member_access_string = "(";
		member_access_string += this->to_string(result_type);
		member_access_string += ')';
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
		return this->add_value_expression(member_access_string, result_type);
	}
	else
	{
		bz_unreachable;
	}
}

expr_value codegen_context::create_array_gep(expr_value value, expr_value index)
{
	bz_assert(this->is_array(value.get_type()));
	auto const array_type = this->maybe_get_array(value.get_type());
	auto const result_type = array_type->elem_type;
	auto const use_arrow = value.needs_dereference;
	auto const is_const = value.is_const;
	remove_needs_dereference(value, *this);

	bz::u8string member_access_string = this->to_string_unary(value, precedence::suffix);
	if (use_arrow)
	{
		member_access_string += "->a + ";
	}
	else
	{
		member_access_string += ".a + ";
	}
	member_access_string += this->to_string_rhs(index, precedence::addition);
	return this->add_reference_expression(member_access_string, result_type, is_const);
}

expr_value codegen_context::create_array_gep_pointer(expr_value value, expr_value index)
{
	bz_assert(this->is_array(value.get_type()));
	auto const array_type = this->maybe_get_array(value.get_type());
	auto const elem_type = array_type->elem_type;
	auto const use_arrow = value.needs_dereference;
	auto const is_const = value.is_const;
	remove_needs_dereference(value, *this);

	bz::u8string member_access_string = this->to_string_unary(value, precedence::suffix);
	if (use_arrow)
	{
		member_access_string += "->a + ";
	}
	else
	{
		member_access_string += ".a + ";
	}
	member_access_string += this->to_string_rhs(index, precedence::addition);
	auto const result_type = is_const ? this->add_const_pointer(elem_type) : this->add_pointer(elem_type);
	return this->add_value_expression(member_access_string, result_type);
}

expr_value codegen_context::create_array_slice_gep(expr_value begin_ptr, expr_value index)
{
	bz_assert(this->is_pointer(begin_ptr.get_type()));
	auto const [result_type, result_modifier] = this->remove_pointer(begin_ptr.get_type());
	auto const result_ptr = this->to_string_binary(begin_ptr, index, "+", precedence::addition);
	return this->add_reference_expression(result_ptr, result_type, result_modifier == type_modifier::const_pointer);
}

expr_value codegen_context::create_array_slice_gep_pointer(expr_value begin_ptr, expr_value index)
{
	bz_assert(this->is_pointer(begin_ptr.get_type()));
	auto const result_ptr = this->to_string_binary(begin_ptr, index, "+", precedence::addition);
	return this->add_value_expression(result_ptr, begin_ptr.get_type());
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

expr_value codegen_context::create_prefix_unary_operation(
	expr_value const &value,
	bz::u8string_view op,
	type result_type
)
{
	return this->add_value_expression(this->to_string_unary_prefix(value, op), result_type);
}

void codegen_context::create_prefix_unary_operation(
	expr_value const &value,
	bz::u8string_view op
)
{
	this->add_expression(this->to_string_unary_prefix(value, op));
}

expr_value codegen_context::create_binary_operation(
	expr_value const &lhs,
	expr_value const &rhs,
	bz::u8string_view op,
	precedence prec,
	type result_type
)
{
	return this->add_value_expression(this->to_string_binary(lhs, rhs, op, prec), result_type);
}

expr_value codegen_context::create_binary_operation(
	expr_value const &lhs,
	bz::u8string_view rhs_string,
	bz::u8string_view op,
	precedence prec,
	type result_type
)
{
	auto const expr_string = bz::format("({} {} {})", this->to_string_lhs(lhs, prec), op, rhs_string);
	return this->add_value_expression(expr_string, result_type);
}

void codegen_context::create_binary_operation(expr_value const &lhs, expr_value const &rhs, bz::u8string_view op, precedence prec)
{
	this->add_expression(this->to_string_binary(lhs, rhs, op, prec));
}

void codegen_context::create_binary_operation(expr_value const &lhs, bz::u8string_view rhs_string, bz::u8string_view op, precedence prec)
{
	auto const expr_string = bz::format("{} {} {}", this->to_string_lhs(lhs, prec), op, rhs_string);
	this->add_expression(expr_string);
}

void codegen_context::create_assignment(expr_value const &lhs, expr_value const &rhs)
{
	return this->create_binary_operation(lhs, rhs, "=", precedence::assignment);
}

void codegen_context::create_assignment(expr_value const &lhs, bz::u8string_view rhs_string)
{
	return this->create_binary_operation(lhs, rhs_string, "=", precedence::assignment);
}

expr_value codegen_context::create_trivial_copy(expr_value const &value)
{
	return this->add_value_expression(this->to_string_rhs(value, precedence::assignment), value.get_type());
}

void codegen_context::create_unreachable(void)
{
	this->add_expression("bozon_unreachable()");
}

void codegen_context::create_trap(void)
{
	this->add_expression("bozon_trap()");
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

void codegen_context::push_rvalue_array_destruct_operation(
	ast::destruct_operation const &destruct_op,
	expr_value value,
	expr_value rvalue_array_elem_ptr
)
{
	auto const move_destruct_indicator = this->get_move_destruct_indicator(destruct_op.move_destructed_decl);
	if (move_destruct_indicator.has_value() || destruct_op.not_null())
	{
		this->current_function_info.destructor_calls.push_back({
			.destruct_op = &destruct_op,
			.value = value,
			.condition = {},
			.move_destruct_indicator = move_destruct_indicator,
			.rvalue_array_elem_ptr = rvalue_array_elem_ptr,
		});
	}
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

static constexpr bz::u8string_view builtin_defines = R"(#if defined(__has_builtin)
	#define BOZON_HAS_BUILTIN(x) __has_builtin(x)
#else
	#define BOZON_HAS_BUILTIN(x) 0
#endif
#if BOZON_HAS_BUILTIN(__builtin_unreachable)
	#define bozon_unreachable() (__builtin_unreachable())
#elif defined(_MSC_VER)
	#define bozon_unreachable() (__assume(0))
#endif
#if BOZON_HAS_BUILTIN(__builtin_trap)
	#define bozon_trap() (__builtin_trap())
#endif
#if !defined(bozon_unreachable)
	#define bozon_unreachable() ((void)0)
#endif
#if !defined(bozon_trap)
	#include <stdlib.h>
	#define bozon_trap() (abort())
#endif
)";
static constexpr bz::u8string_view builtin_functions = R"(static t_int8 bozon_neg_i8(t_int8 n)
{
	return n == (t_int8)((t_uint8)1 << 7) ? n : -n;
}
static t_int16 bozon_neg_i16(t_int16 n)
{
	return n == (t_int16)((t_uint16)1 << 15) ? n : -n;
}
static t_int32 bozon_neg_i32(t_int32 n)
{
	return n == (t_int32)((t_uint32)1 << 31) ? n : -n;
}
static t_int64 bozon_neg_i64(t_int64 n)
{
	return n == (t_int64)((t_uint64)1 << 63) ? n : -n;
}
static t_int8 bozon_div_i8(t_int8 lhs, t_int8 rhs)
{
	return lhs == (t_int8)((t_uint8)1 << 7) && rhs == -1 ? lhs : lhs / rhs;
}
static t_int16 bozon_div_i16(t_int16 lhs, t_int16 rhs)
{
	return lhs == (t_int16)((t_uint16)1 << 15) && rhs == -1 ? lhs : lhs / rhs;
}
static t_int32 bozon_div_i32(t_int32 lhs, t_int32 rhs)
{
	return lhs == (t_int32)((t_uint32)1 << 31) && rhs == -1 ? lhs : lhs / rhs;
}
static t_int64 bozon_div_i64(t_int64 lhs, t_int64 rhs)
{
	return lhs == (t_int64)((t_uint64)1 << 63) && rhs == -1 ? lhs : lhs / rhs;
}
static t_int8 bozon_abs_i8(t_int8 n)
{
	if (n == (t_int8)(1ull << 7))
	{
		return n;
	}
	else if (n < 0)
	{
		return -n;
	}
	else
	{
		return n;
	}
}
static t_int16 bozon_abs_i16(t_int16 n)
{
	if (n == (t_int16)(1ull << 15))
	{
		return n;
	}
	else if (n < 0)
	{
		return -n;
	}
	else
	{
		return n;
	}
}
static t_int32 bozon_abs_i32(t_int32 n)
{
	if (n == (t_int32)(1ull << 31))
	{
		return n;
	}
	else if (n < 0)
	{
		return -n;
	}
	else
	{
		return n;
	}
}
static t_int64 bozon_abs_i64(t_int64 n)
{
	if (n == (t_int64)(1ull << 63))
	{
		return n;
	}
	else if (n < 0)
	{
		return -n;
	}
	else
	{
		return n;
	}
}
static t_uint8 bozon_bitreverse_u8(t_uint8 n)
{
#if BOZON_HAS_BUILTIN(__builtin_bitreverse8)
	return __builtin_bitreverse8(n);
#else
	n = (t_uint8)((t_uint8)(n & (t_uint8)0xf0) >> 4) | (t_uint8)((t_uint8)(n & (t_uint8)0x0f) << 4);
	n = (t_uint8)((t_uint8)(n & (t_uint8)0xcc) >> 2) | (t_uint8)((t_uint8)(n & (t_uint8)0x33) << 2);
	n = (t_uint8)((t_uint8)(n & (t_uint8)0xaa) >> 1) | (t_uint8)((t_uint8)(n & (t_uint8)0x55) << 1);
	return n;
#endif
}
static t_uint16 bozon_bitreverse_u16(t_uint16 n)
{
#if BOZON_HAS_BUILTIN(__builtin_bitreverse16)
	return __builtin_bitreverse16(n);
#elif BOZON_HAS_BUILTIN(__builtin_bswap16)
	n = __builtin_bswap16(n);
	n = (t_uint16)((t_uint16)(n & (t_uint16)0xf0f0) >> 4) | (t_uint16)((t_uint16)(n & (t_uint16)0x0f0f) << 4);
	n = (t_uint16)((t_uint16)(n & (t_uint16)0xcccc) >> 2) | (t_uint16)((t_uint16)(n & (t_uint16)0x3333) << 2);
	n = (t_uint16)((t_uint16)(n & (t_uint16)0xaaaa) >> 1) | (t_uint16)((t_uint16)(n & (t_uint16)0x5555) << 1);
	return n;
#else
	n = (t_uint16)((t_uint16)(n & (t_uint16)0xff00) >> 8) | (t_uint16)((t_uint16)(n & (t_uint16)0x00ff) << 8);
	n = (t_uint16)((t_uint16)(n & (t_uint16)0xf0f0) >> 4) | (t_uint16)((t_uint16)(n & (t_uint16)0x0f0f) << 4);
	n = (t_uint16)((t_uint16)(n & (t_uint16)0xcccc) >> 2) | (t_uint16)((t_uint16)(n & (t_uint16)0x3333) << 2);
	n = (t_uint16)((t_uint16)(n & (t_uint16)0xaaaa) >> 1) | (t_uint16)((t_uint16)(n & (t_uint16)0x5555) << 1);
	return n;
#endif
}
static t_uint32 bozon_bitreverse_u32(t_uint32 n)
{
#if BOZON_HAS_BUILTIN(__builtin_bitreverse32)
	return __builtin_bitreverse32(n);
#elif BOZON_HAS_BUILTIN(__builtin_bswap32)
	n = __builtin_bswap32(n);
	n = ((n & (t_uint32)0xf0f0f0f0) >>  4) | ((n & (t_uint32)0x0f0f0f0f) <<  4);
	n = ((n & (t_uint32)0xcccccccc) >>  2) | ((n & (t_uint32)0x33333333) <<  2);
	n = ((n & (t_uint32)0xaaaaaaaa) >>  1) | ((n & (t_uint32)0x55555555) <<  1);
	return n;
#else
	n = ((n & (t_uint32)0xffff0000) >> 16) | ((n & (t_uint32)0x0000ffff) << 16);
	n = ((n & (t_uint32)0xff00ff00) >>  8) | ((n & (t_uint32)0x00ff00ff) <<  8);
	n = ((n & (t_uint32)0xf0f0f0f0) >>  4) | ((n & (t_uint32)0x0f0f0f0f) <<  4);
	n = ((n & (t_uint32)0xcccccccc) >>  2) | ((n & (t_uint32)0x33333333) <<  2);
	n = ((n & (t_uint32)0xaaaaaaaa) >>  1) | ((n & (t_uint32)0x55555555) <<  1);
	return n;
#endif
}
static t_uint64 bozon_bitreverse_u64(t_uint64 n)
{
#if BOZON_HAS_BUILTIN(__builtin_bitreverse64)
	return __builtin_bitreverse64(n);
#elif BOZON_HAS_BUILTIN(__builtin_bswap64)
	n = __builtin_bswap64(n);
	n = ((n & (t_uint64)0xf0f0f0f0f0f0f0f0) >>  4) | ((n & (t_uint64)0x0f0f0f0f0f0f0f0f) <<  4);
	n = ((n & (t_uint64)0xcccccccccccccccc) >>  2) | ((n & (t_uint64)0x3333333333333333) <<  2);
	n = ((n & (t_uint64)0xaaaaaaaaaaaaaaaa) >>  1) | ((n & (t_uint64)0x5555555555555555) <<  1);
	return n;
#else
	n = ((n & (t_uint64)0xffffffff00000000) >> 32) | ((n & (t_uint64)0x00000000ffffffff) << 32);
	n = ((n & (t_uint64)0xffff0000ffff0000) >> 16) | ((n & (t_uint64)0x0000ffff0000ffff) << 16);
	n = ((n & (t_uint64)0xff00ff00ff00ff00) >>  8) | ((n & (t_uint64)0x00ff00ff00ff00ff) <<  8);
	n = ((n & (t_uint64)0xf0f0f0f0f0f0f0f0) >>  4) | ((n & (t_uint64)0x0f0f0f0f0f0f0f0f) <<  4);
	n = ((n & (t_uint64)0xcccccccccccccccc) >>  2) | ((n & (t_uint64)0x3333333333333333) <<  2);
	n = ((n & (t_uint64)0xaaaaaaaaaaaaaaaa) >>  1) | ((n & (t_uint64)0x5555555555555555) <<  1);
	return n;
#endif
}
static t_uint8 bozon_popcount_u8(t_uint8 n)
{
	t_uint8 const int_size = 1;
	if (int_size <= sizeof (unsigned int))
	{
#if BOZON_HAS_BUILTIN(__builtin_popcount)
		return (t_uint8)__builtin_popcount(n);
#endif
	}
	else if (int_size <= sizeof (unsigned long))
	{
#if BOZON_HAS_BUILTIN(__builtin_popcountl)
		return (t_uint8)__builtin_popcountl(n);
#endif
	}
	else
	{
#if BOZON_HAS_BUILTIN(__builtin_popcountll)
		return (t_uint8)__builtin_popcountll(n);
#endif
	}
	t_uint8 c = 0;
	for (; n; ++c)
	{
		n &= n - 1;
	}
	return c;
}
static t_uint16 bozon_popcount_u16(t_uint16 n)
{
	t_uint16 const int_size = 2;
	if (int_size <= sizeof (unsigned int))
	{
#if BOZON_HAS_BUILTIN(__builtin_popcount)
		return (t_uint16)__builtin_popcount(n);
#endif
	}
	else if (int_size <= sizeof (unsigned long))
	{
#if BOZON_HAS_BUILTIN(__builtin_popcountl)
		return (t_uint16)__builtin_popcountl(n);
#endif
	}
	else
	{
#if BOZON_HAS_BUILTIN(__builtin_popcountll)
		return (t_uint16)__builtin_popcountll(n);
#endif
	}
	t_uint16 c = 0;
	for (; n; ++c)
	{
		n &= n - 1;
	}
	return c;
}
static t_uint32 bozon_popcount_u32(t_uint32 n)
{
	t_uint16 const int_size = 4;
	if (int_size <= sizeof (unsigned int))
	{
#if BOZON_HAS_BUILTIN(__builtin_popcount)
		return (t_uint32)__builtin_popcount(n);
#endif
	}
	else if (int_size <= sizeof (unsigned long))
	{
#if BOZON_HAS_BUILTIN(__builtin_popcountl)
		return (t_uint32)__builtin_popcountl(n);
#endif
	}
	else
	{
#if BOZON_HAS_BUILTIN(__builtin_popcountll)
		return (t_uint32)__builtin_popcountll(n);
#endif
	}
	t_uint32 c = 0;
	for (; n; ++c)
	{
		n &= n - 1;
	}
	return c;
}
static t_uint64 bozon_popcount_u64(t_uint64 n)
{
	t_uint16 const int_size = 8;
	if (int_size <= sizeof (unsigned int))
	{
#if BOZON_HAS_BUILTIN(__builtin_popcount)
		return (t_uint64)__builtin_popcount(n);
#endif
	}
	else if (int_size <= sizeof (unsigned long))
	{
#if BOZON_HAS_BUILTIN(__builtin_popcountl)
		return (t_uint64)__builtin_popcountl(n);
#endif
	}
	else
	{
#if BOZON_HAS_BUILTIN(__builtin_popcountll)
		return (t_uint64)__builtin_popcountll(n);
#endif
	}
	t_uint64 c = 0;
	for (; n; ++c)
	{
		n &= n - 1;
	}
	return c;
}
static t_uint16 bozon_byteswap_u16(t_uint16 n)
{
#if BOZON_HAS_BUILTIN(__builtin_bswap16)
	return __builtin_bswap16(n);
#else
	n = (t_uint16)((t_uint16)(n & (t_uint16)0xff00) >> 8) | (t_uint16)((t_uint16)(n & (t_uint16)0x00ff) << 8);
	return n;
#endif
}
static t_uint32 bozon_byteswap_u32(t_uint32 n)
{
#if BOZON_HAS_BUILTIN(__builtin_bswap32)
	return __builtin_bswap32(n);
#else
	n = ((n & (t_uint32)0xffff0000) >> 16) | ((n & (t_uint32)0x0000ffff) << 16);
	n = ((n & (t_uint32)0xff00ff00) >>  8) | ((n & (t_uint32)0x00ff00ff) <<  8);
	return n;
#endif
}
static t_uint64 bozon_byteswap_u64(t_uint64 n)
{
#if BOZON_HAS_BUILTIN(__builtin_bswap64)
	return __builtin_bswap64(n);
#else
	n = ((n & (t_uint64)0xffffffff00000000) >> 32) | ((n & (t_uint64)0x00000000ffffffff) << 32);
	n = ((n & (t_uint64)0xffff0000ffff0000) >> 16) | ((n & (t_uint64)0x0000ffff0000ffff) << 16);
	n = ((n & (t_uint64)0xff00ff00ff00ff00) >>  8) | ((n & (t_uint64)0x00ff00ff00ff00ff) <<  8);
	return n;
#endif
}
static t_uint8 bozon_clz_u8(t_uint8 n)
{
	t_uint8 const int_size = 1;
	if (int_size <= sizeof (unsigned int))
	{
#if BOZON_HAS_BUILTIN(__builtin_clz)
		return (t_uint8)((t_uint8)__builtin_clz(n) - 8 * (sizeof (unsigned int) - int_size));
#endif
	}
	else if (int_size <= sizeof (unsigned long))
	{
#if BOZON_HAS_BUILTIN(__builtin_clzl)
		return (t_uint8)((t_uint8)__builtin_clzl(n) - 8 * (sizeof (unsigned long) - int_size));
#endif
	}
	else
	{
#if BOZON_HAS_BUILTIN(__builtin_clzll)
		return (t_uint8)((t_uint8)__builtin_clzll(n) - 8 * (sizeof (unsigned long long) - int_size));
#endif
	}
	t_uint8 c = 8 * int_size - 1;
	t_uint8 i = 4 * int_size;
	for (; i != 0; i /= 2)
	{
		if (n >= (t_uint8)((t_uint8)1 << i))
		{
			n >>= i;
			c -= i;
		}
	}
	return c;
}
static t_uint16 bozon_clz_u16(t_uint16 n)
{
	t_uint16 const int_size = 2;
	if (int_size <= sizeof (unsigned int))
	{
#if BOZON_HAS_BUILTIN(__builtin_clz)
		return (t_uint16)((t_uint16)__builtin_clz(n) - 8 * (sizeof (unsigned int) - int_size));
#endif
	}
	else if (int_size <= sizeof (unsigned long))
	{
#if BOZON_HAS_BUILTIN(__builtin_clzl)
		return (t_uint16)((t_uint16)__builtin_clzl(n) - 8 * (sizeof (unsigned long) - int_size));
#endif
	}
	else
	{
#if BOZON_HAS_BUILTIN(__builtin_clzll)
		return (t_uint16)((t_uint16)__builtin_clzll(n) - 8 * (sizeof (unsigned long long) - int_size));
#endif
	}
	t_uint16 c = 8 * int_size - 1;
	t_uint16 i = 4 * int_size;
	for (; i != 0; i /= 2)
	{
		if (n >= (t_uint16)((t_uint16)1 << i))
		{
			n >>= i;
			c -= i;
		}
	}
	return c;
}
static t_uint32 bozon_clz_u32(t_uint32 n)
{
	t_uint32 const int_size = 4;
	if (int_size <= sizeof (unsigned int))
	{
#if BOZON_HAS_BUILTIN(__builtin_clz)
		return (t_uint32)((t_uint32)__builtin_clz(n) - 8 * (sizeof (unsigned int) - int_size));
#endif
	}
	else if (int_size <= sizeof (unsigned long))
	{
#if BOZON_HAS_BUILTIN(__builtin_clzl)
		return (t_uint32)((t_uint32)__builtin_clzl(n) - 8 * (sizeof (unsigned long) - int_size));
#endif
	}
	else
	{
#if BOZON_HAS_BUILTIN(__builtin_clzll)
		return (t_uint32)((t_uint32)__builtin_clzll(n) - 8 * (sizeof (unsigned long long) - int_size));
#endif
	}
	t_uint32 c = 8 * int_size - 1;
	t_uint32 i = 4 * int_size;
	for (; i != 0; i /= 2)
	{
		if (n >= ((t_uint32)1 << i))
		{
			n >>= i;
			c -= i;
		}
	}
	return c;
}
static t_uint64 bozon_clz_u64(t_uint64 n)
{
	t_uint64 const int_size = 8;
	if (int_size <= sizeof (unsigned int))
	{
#if BOZON_HAS_BUILTIN(__builtin_clz)
		return (t_uint64)((t_uint64)__builtin_clz(n) - 8 * (sizeof (unsigned int) - int_size));
#endif
	}
	else if (int_size <= sizeof (unsigned long))
	{
#if BOZON_HAS_BUILTIN(__builtin_clzl)
		return (t_uint64)((t_uint64)__builtin_clzl(n) - 8 * (sizeof (unsigned long) - int_size));
#endif
	}
	else
	{
#if BOZON_HAS_BUILTIN(__builtin_clzll)
		return (t_uint64)((t_uint64)__builtin_clzll(n) - 8 * (sizeof (unsigned long long) - int_size));
#endif
	}
	t_uint64 c = 8 * int_size - 1;
	t_uint64 i = 4 * int_size;
	for (; i != 0; i /= 2)
	{
		if (n >= ((t_uint64)1 << i))
		{
			n >>= i;
			c -= i;
		}
	}
	return c;
}
static t_uint8 bozon_ctz_u8(t_uint8 n)
{
	t_uint8 const int_size = 1;
	if (int_size <= sizeof (unsigned int))
	{
#if BOZON_HAS_BUILTIN(__builtin_ctz)
		return (t_uint8)__builtin_ctz(n);
#endif
	}
	else if (int_size <= sizeof (unsigned long))
	{
#if BOZON_HAS_BUILTIN(__builtin_ctzl)
		return (t_uint8)__builtin_ctzl(n);
#endif
	}
	else
	{
#if BOZON_HAS_BUILTIN(__builtin_ctzll)
		return (t_uint8)__builtin_ctzll(n);
#endif
	}
	t_uint8 c = 0;
	for (; c < 8 * int_size; ++c)
	{
		if ((n & (t_uint8)((t_uint8)1 << c)) != 0)
		{
			return c;
		}
	}
	return 8 * int_size;
}
static t_uint16 bozon_ctz_u16(t_uint16 n)
{
	t_uint16 const int_size = 2;
	if (int_size <= sizeof (unsigned int))
	{
#if BOZON_HAS_BUILTIN(__builtin_ctz)
		return (t_uint16)__builtin_ctz(n);
#endif
	}
	else if (int_size <= sizeof (unsigned long))
	{
#if BOZON_HAS_BUILTIN(__builtin_ctzl)
		return (t_uint16)__builtin_ctzl(n);
#endif
	}
	else
	{
#if BOZON_HAS_BUILTIN(__builtin_ctzll)
		return (t_uint16)__builtin_ctzll(n);
#endif
	}
	t_uint16 c = 0;
	for (; c < 8 * int_size; ++c)
	{
		if ((n & (t_uint16)((t_uint16)1 << c)) != 0)
		{
			return c;
		}
	}
	return 8 * int_size;
}
static t_uint32 bozon_ctz_u32(t_uint32 n)
{
	t_uint32 const int_size = 4;
	if (int_size <= sizeof (unsigned int))
	{
#if BOZON_HAS_BUILTIN(__builtin_ctz)
		return (t_uint32)__builtin_ctz(n);
#endif
	}
	else if (int_size <= sizeof (unsigned long))
	{
#if BOZON_HAS_BUILTIN(__builtin_ctzl)
		return (t_uint32)__builtin_ctzl(n);
#endif
	}
	else
	{
#if BOZON_HAS_BUILTIN(__builtin_ctzll)
		return (t_uint32)__builtin_ctzll(n);
#endif
	}
	t_uint32 c = 0;
	for (; c < 8 * int_size; ++c)
	{
		if ((n & ((t_uint32)1 << c)) != 0)
		{
			return c;
		}
	}
	return 8 * int_size;
}
static t_uint64 bozon_ctz_u64(t_uint64 n)
{
	t_uint64 const int_size = 8;
	if (int_size <= sizeof (unsigned int))
	{
#if BOZON_HAS_BUILTIN(__builtin_ctz)
		return (t_uint64)__builtin_ctz(n);
#endif
	}
	else if (int_size <= sizeof (unsigned long))
	{
#if BOZON_HAS_BUILTIN(__builtin_ctzl)
		return (t_uint64)__builtin_ctzl(n);
#endif
	}
	else
	{
#if BOZON_HAS_BUILTIN(__builtin_ctzll)
		return (t_uint64)__builtin_ctzll(n);
#endif
	}
	t_uint64 c = 0;
	for (; c < 8 * int_size; ++c)
	{
		if ((n & ((t_uint64)1 << c)) != 0)
		{
			return c;
		}
	}
	return 8 * int_size;
}
static t_uint8 bozon_fshl_u8(t_uint8 a, t_uint8 b, t_uint8 amount)
{
	amount %= 8;
	return amount == 0 ? a : (t_uint8)((a << amount) | (b >> (8 - amount)));
}
static t_uint16 bozon_fshl_u16(t_uint16 a, t_uint16 b, t_uint16 amount)
{
	amount %= 16;
	return amount == 0 ? a : (t_uint16)((a << amount) | (b >> (16 - amount)));
}
static t_uint32 bozon_fshl_u32(t_uint32 a, t_uint32 b, t_uint32 amount)
{
	amount %= 32;
	return amount == 0 ? a : ((a << amount) | (b >> (32 - amount)));
}
static t_uint64 bozon_fshl_u64(t_uint64 a, t_uint64 b, t_uint64 amount)
{
	amount %= 64;
	return amount == 0 ? a : ((a << amount) | (b >> (64 - amount)));
}
static t_uint8 bozon_fshr_u8(t_uint8 a, t_uint8 b, t_uint8 amount)
{
	amount %= 8;
	return amount == 0 ? b : (t_uint8)((b >> amount) | (a << (8 - amount)));
}
static t_uint16 bozon_fshr_u16(t_uint16 a, t_uint16 b, t_uint16 amount)
{
	amount %= 16;
	return amount == 0 ? b : (t_uint16)((b >> amount) | (a << (16 - amount)));
}
static t_uint32 bozon_fshr_u32(t_uint32 a, t_uint32 b, t_uint32 amount)
{
	amount %= 32;
	return amount == 0 ? b : ((b >> amount) | (a << (32 - amount)));
}
static t_uint64 bozon_fshr_u64(t_uint64 a, t_uint64 b, t_uint64 amount)
{
	amount %= 64;
	return amount == 0 ? b : ((b >> amount) | (a << (64 - amount)));
}
)";

bz::u8string codegen_context::get_code_string(void) const
{
	bz::u8string result = "";

	for (auto const &header : this->included_headers)
	{
		result += bz::format("#include <{}>\n", header);
	}

	result += builtin_defines;

	result += this->struct_forward_declarations_string;
	result += this->typedefs_string;
	result += this->struct_bodies_string;
	result += this->function_declarations_string;
	result += this->variables_string;

	result += builtin_functions;

	result += this->function_bodies_string;

	return result;
}

} // namespace codegen::c
