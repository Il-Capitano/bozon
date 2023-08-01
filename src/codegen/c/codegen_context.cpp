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

bz::u8string codegen_context::get_member_name(size_t index)
{
	return bz::format("m_{:x}", index);
}

type codegen_context::get_struct(ast::type_info const &info, bool resolve)
{
	if (auto const it = this->struct_infos.find(&info); it != this->struct_infos.end())
	{
		return it->second.typedef_ref != type::typedef_reference::invalid()
			? type(it->second.typedef_ref)
			: type(it->second.struct_ref);
	}

	if (resolve)
	{
		return generate_struct(info, *this);
	}
	else
	{
		bz_unreachable;
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

type codegen_context::remove_pointer(type t) const
{
	if (t.is_pointer())
	{
		return t.get_pointer();
	}
	else
	{
		bz_assert(t.is_typedef());
		auto const &typedef_type = this->get_typedef(t.get_typedef());
		bz_assert(typedef_type.aliased_type.is_pointer());
		return typedef_type.aliased_type.get_pointer();
	}
}

type::struct_reference codegen_context::add_struct(ast::type_info const &info, struct_type_t struct_type)
{
	auto const [ref, inserted] = this->type_set.add_struct_type(std::move(struct_type));
	if (!inserted)
	{
		return ref;
	}

	this->type_set.add_struct_type_name(ref, this->make_type_name());
	this->struct_infos.insert({ &info, struct_info_t{ .struct_ref = ref, .typedef_ref = type::typedef_reference::invalid() } });

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

type::struct_reference codegen_context::add_struct_forward_declaration(ast::type_info const &info)
{
	this->type_set.add_struct_type_name(ref, this->make_type_name());
	this->struct_infos.insert({ &info, struct_info_t{ .struct_ref = ref, .typedef_ref = type::typedef_reference::invalid() } });
	auto const ref = this->type_set.add_unique_struct({});

	auto const struct_name = this->type_set.get_struct_type_name(ref);

	this->struct_forward_declarations_string += bz::format("typedef struct {0} {0};\n", struct_name);

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
	this->struct_infos.insert({ &info, struct_info_t{ struct_ref, typedef_ref } });

	this->typedefs_string += bz::format("typedef {} {};\n", aliased_type_name, typedef_type_name);

	return typedef_ref;
}

type::typedef_reference codegen_context::add_char_typedef(ast::type_info const &info, bz::u8string_view typedef_type_name)
{
	bz_assert(this->builtin_types.uint32_ != type::typedef_reference::invalid());
	auto const uint32_name = this->type_set.get_typedef_type_name(this->builtin_types.uint32_);

	auto const ref = this->type_set.add_unique_typedef({ .aliased_type = this->get_uint32() });
	this->type_set.add_typedef_type_name(ref, typedef_type_name);
	this->struct_infos.insert({ &info, struct_info_t{ .struct_ref = type::struct_reference::invalid(), .typedef_ref = ref } });

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

bz::u8string codegen_context::get_code_string(void) const
{
	bz::u8string result = "";
	result += this->struct_forward_declarations_string;
	result += this->typedefs_string;
	result += this->struct_bodies_string;
	return result;
}

} // namespace codegen::c
