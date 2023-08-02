#include "codegen.h"
#include "ast/statement.h"

namespace codegen::c
{

static type get_type(ast::typespec_view ts, codegen_context &context, bool resolve_structs = true)
{
	static_assert(ast::typespec_types::size() == 19);

	ts = ts.remove_any_mut();

	if (ts.modifiers.empty() || ts.is_optional_function())
	{
		switch (ts.terminator_kind())
		{
		case ast::terminator_typespec_node_t::index_of<ast::ts_base_type>:
		{
			auto const &info = *ts.get<ast::ts_base_type>().info;

			bz_assert(!resolve_structs || info.state >= ast::resolve_state::members);
			bz_assert(!resolve_structs || info.prototype != nullptr);
			return context.get_struct(info, resolve_structs);
		}
		case ast::terminator_typespec_node_t::index_of<ast::ts_enum>:
		{
			auto const decl = ts.get<ast::ts_enum>().decl;
			return get_type(decl->underlying_type, context, resolve_structs);
		}
		case ast::terminator_typespec_node_t::index_of<ast::ts_void>:
		{
			return context.get_void();
		}
		case ast::terminator_typespec_node_t::index_of<ast::ts_function>:
		{
			auto const &func_type = ts.get<ast::ts_function>();
			auto const return_type = get_type(func_type.return_type, context, false);
			auto param_types = func_type.param_types.transform([&context](auto const &param_type) {
				return get_type(param_type, context, false);
			}).collect<ast::arena_vector>();
			return type(context.add_function({ return_type, std::move(param_types) }));
		}
		case ast::terminator_typespec_node_t::index_of<ast::ts_array>:
		{
			auto const &array_type = ts.get<ast::ts_array>();
			auto const elem_type = get_type(array_type.elem_type, context, resolve_structs);
			return type(context.add_array({ elem_type, array_type.size }));
		}
		case ast::terminator_typespec_node_t::index_of<ast::ts_array_slice>:
		{
			auto const &slice_type = ts.get<ast::ts_array_slice>();
			auto const elem_type = get_type(slice_type.elem_type, context, false);
			auto const elem_pointer_type = slice_type.elem_type.is_mut()
				? context.add_pointer(elem_type)
				: context.add_const_pointer(elem_type);
			return type(context.add_struct({ .members = { elem_pointer_type, elem_pointer_type } }));
		}
		case ast::terminator_typespec_node_t::index_of<ast::ts_tuple>:
		{
			auto elem_types = ts.get<ast::ts_tuple>().types.transform([&context, resolve_structs](auto const &elem_type) {
				return get_type(elem_type, context, resolve_structs);
			}).collect<ast::arena_vector>();
			return type(context.add_struct({ .members = std::move(elem_types) }));
		}

		case ast::terminator_typespec_node_t::index_of<ast::ts_auto>:
			bz_unreachable;
		case ast::terminator_typespec_node_t::index_of<ast::ts_unresolved>:
			bz_unreachable;
		case ast::terminator_typespec_node_t::index_of<ast::ts_typename>:
			bz_unreachable;
		default:
			bz_unreachable;
		}
	}
	else if (ts.is_any_reference() || ts.is_pointer() || ts.is_optional_pointer() || ts.is_optional_reference())
	{
		ts = ts.is_any_reference() ? ts.get_any_reference()
			: ts.is_pointer() ? ts.get_pointer()
			: ts.is_optional_pointer() ? ts.get_optional_pointer()
			: ts.get_optional_reference();
		if (ts.is_mut())
		{
			auto result = get_type(ts.get_mut(), context, false);
			return context.add_pointer(result);
		}
		else
		{
			auto result = get_type(ts, context, false);
			return context.add_const_pointer(result);
		}
	}
	else
	{
		bz_assert(ts.is_optional());
		auto const elem_type = get_type(ts.get_optional(), context, resolve_structs);
		auto const bool_type = context.get_bool();
		auto const ref = context.add_struct({ .members = { elem_type, bool_type } });
		return type(ref);
	}
}

static type::typedef_reference add_int_type(ast::type_info const &info, bz::u8string_view name, size_t size, bool is_signed, codegen_context &context)
{
	auto const c_int_type_name = [&]() -> bz::u8string_view {
		if (size == 1)
		{
			return is_signed ? "signed char" : "unsigned char";
		}
		else if (context.short_size == size)
		{
			return is_signed ? "short" : "unsigned short";
		}
		else if (context.int_size == size)
		{
			return is_signed ? "int" : "unsigned int";
		}
		else if (context.long_size == size)
		{
			return is_signed ? "long" : "unsigned long";
		}
		else if (context.long_long_size == size)
		{
			return is_signed ? "long long" : "unsigned long long";
		}
		else
		{
			bz_unreachable;
		}
	}();
	return context.add_builtin_type(info, name, c_int_type_name);
}

type generate_struct(ast::type_info const &info, codegen_context &context)
{
	if (info.is_libc_internal())
	{
		return type();
	}

	switch (info.kind)
	{
	case ast::type_info::int8_:
		bz_assert(context.builtin_types.int8_ == type::typedef_reference::invalid());
		context.builtin_types.int8_ = add_int_type(info, "t_int8", 1, true, context);
		return type(context.builtin_types.int8_);
	case ast::type_info::int16_:
		bz_assert(context.builtin_types.int16_ == type::typedef_reference::invalid());
		context.builtin_types.int16_ = add_int_type(info, "t_int16", 2, true, context);
		return type(context.builtin_types.int16_);
	case ast::type_info::int32_:
		bz_assert(context.builtin_types.int32_ == type::typedef_reference::invalid());
		context.builtin_types.int32_ = add_int_type(info, "t_int32", 4, true, context);
		return type(context.builtin_types.int32_);
	case ast::type_info::int64_:
		bz_assert(context.builtin_types.int64_ == type::typedef_reference::invalid());
		context.builtin_types.int64_ = add_int_type(info, "t_int64", 8, true, context);
		return type(context.builtin_types.int64_);
	case ast::type_info::uint8_:
		bz_assert(context.builtin_types.uint8_ == type::typedef_reference::invalid());
		context.builtin_types.uint8_ = add_int_type(info, "t_uint8", 1, false, context);
		return type(context.builtin_types.uint8_);
	case ast::type_info::uint16_:
		bz_assert(context.builtin_types.uint16_ == type::typedef_reference::invalid());
		context.builtin_types.uint16_ = add_int_type(info, "t_uint16", 2, false, context);
		return type(context.builtin_types.uint16_);
	case ast::type_info::uint32_:
		bz_assert(context.builtin_types.uint32_ == type::typedef_reference::invalid());
		context.builtin_types.uint32_ = add_int_type(info, "t_uint32", 4, false, context);
		return type(context.builtin_types.uint32_);
	case ast::type_info::uint64_:
		bz_assert(context.builtin_types.uint64_ == type::typedef_reference::invalid());
		context.builtin_types.uint64_ = add_int_type(info, "t_uint64", 8, false, context);
		return type(context.builtin_types.uint64_);
	case ast::type_info::float32_:
		bz_assert(context.builtin_types.float32_ == type::typedef_reference::invalid());
		context.builtin_types.float32_ = context.add_builtin_type(info, "t_float32", "float");
		return type(context.builtin_types.float32_);
	case ast::type_info::float64_:
		bz_assert(context.builtin_types.float64_ == type::typedef_reference::invalid());
		context.builtin_types.float64_ = context.add_builtin_type(info, "t_float64", "double");
		return type(context.builtin_types.float64_);
	case ast::type_info::char_:
		bz_assert(context.builtin_types.char_ == type::typedef_reference::invalid());
		context.builtin_types.char_ = context.add_char_typedef(info, "t_char");
		return type(context.builtin_types.char_);
	case ast::type_info::bool_:
		bz_assert(context.builtin_types.bool_ == type::typedef_reference::invalid());
		context.builtin_types.bool_ = context.add_builtin_type(info, "t_bool", "_Bool");
		return type(context.builtin_types.bool_);

	case ast::type_info::str_:
	case ast::type_info::null_t_:
	case ast::type_info::aggregate:
	{
		if (info.state == ast::resolve_state::none)
		{
			return type();
		}
		auto const [should_resolve, it] = context.should_resolve_struct(info);
		if (!should_resolve)
		{
			return type(it->second.struct_ref);
		}

		auto members = info.member_variables.transform([&context](auto const &member) {
			return get_type(member->get_type(), context);
		}).collect<ast::arena_vector>();
		auto const struct_ref = context.add_struct(info, it, { .members = std::move(members) });
		return type(struct_ref);
	}

	case ast::type_info::forward_declaration:
	{
		auto const struct_ref = context.add_struct_forward_declaration(info);
		return type(struct_ref);
	}
	default:
		bz_unreachable;
	}
}

static void generate_constant_value(
	bz::u8string &buffer,
	ast::constant_value const &value,
	ast::typespec_view type,
	codegen_context &context
);

static void write_sint(bz::u8string &buffer, int64_t value)
{
	if (value == std::numeric_limits<int64_t>::min())
	{
		static_assert(std::numeric_limits<int64_t>::min() + 1 == -9223372036854775807);
		buffer += "(-9223372036854775807 - 1)";
	}
	else
	{
		buffer += bz::format("{}", value);
	}
}

static bool is_zero_value(ast::constant_value const &value)
{
	switch (value.kind())
	{
	static_assert(ast::constant_value::variant_count == 19);
	case ast::constant_value::sint:
		return value.get_sint() == 0;
	case ast::constant_value::uint:
		return value.get_uint() == 0;
	case ast::constant_value::float32:
		return bit_cast<uint32_t>(value.get_float32()) == 0;
	case ast::constant_value::float64:
		return bit_cast<uint64_t>(value.get_float64()) == 0;
	case ast::constant_value::u8char:
		return value.get_u8char() == 0;
	case ast::constant_value::string:
		return value.get_string() == "";
	case ast::constant_value::boolean:
		return value.get_boolean() == false;
	case ast::constant_value::null:
		return true;
	case ast::constant_value::void_:
		return true;
	case ast::constant_value::enum_:
		return value.get_enum().value == 0;
	case ast::constant_value::array:
		return value.get_array().is_all([](auto const &value) { return is_zero_value(value); });
	case ast::constant_value::sint_array:
		return value.get_sint_array().is_all([](auto const value) { return value == 0; });
	case ast::constant_value::uint_array:
		return value.get_sint_array().is_all([](auto const value) { return value == 0; });
	case ast::constant_value::float32_array:
		return value.get_float32_array().is_all([](auto const value) { return bit_cast<uint32_t>(value) == 0; });
	case ast::constant_value::float64_array:
		return value.get_float64_array().is_all([](auto const value) { return bit_cast<uint64_t>(value) == 0; });
	case ast::constant_value::tuple:
		return value.get_tuple().is_all([](auto const &value) { return is_zero_value(value); });
	case ast::constant_value::function:
		return false;
	case ast::constant_value::aggregate:
		return value.get_aggregate().is_all([](auto const &value) { return is_zero_value(value); });
	case ast::constant_value::type:
		bz_unreachable;
	default:
		bz_unreachable;
	}
}

static void generate_nonzero_constant_array_value(
	bz::u8string &buffer,
	bz::array_view<ast::constant_value const> values,
	ast::typespec_view array_type_,
	codegen_context &context
)
{
	bz_assert(array_type_.is<ast::ts_array>());
	auto const &array_type = array_type_.get<ast::ts_array>();
	buffer += bz::format("({})", context.to_string(get_type(array_type_, context)));
	buffer += "{ ";
	if (array_type.elem_type.is<ast::ts_array>())
	{
		bz_assert(values.size() % array_type.size == 0);
		auto const stride = values.size() / array_type.size;
		for (size_t i = 0; i < array_type.size; i += stride)
		{
			auto const sub_array = values.slice(i, i + stride);
			generate_nonzero_constant_array_value(buffer, sub_array, array_type.elem_type, context);
			buffer += ", ";
		}
	}
	else
	{
		for (auto const i : bz::iota(0, array_type.size))
		{
			generate_constant_value(buffer, values[i], array_type.elem_type, context);
			buffer += ", ";
		}
	}
	buffer += '}';
}

static void generate_constant_array_value(
	bz::u8string &buffer,
	bz::array_view<ast::constant_value const> values,
	ast::typespec_view array_type,
	codegen_context &context
)
{
	if (values.is_all([](auto const &value) { return is_zero_value(value); }))
	{
		buffer += bz::format("({})", context.to_string(get_type(array_type, context)));
		buffer += "{0}";
	}
	else
	{
		generate_nonzero_constant_array_value(buffer, values, array_type, context);
	}
}

template<typename T>
static void generate_nonzero_constant_numeric_array_value(
	bz::u8string &buffer,
	bz::array_view<T const> values,
	ast::typespec_view array_type_,
	codegen_context &context
)
{
	bz_assert(array_type_.is<ast::ts_array>());
	auto const &array_type = array_type_.get<ast::ts_array>();
	buffer += bz::format("({})", context.to_string(get_type(array_type_, context)));
	buffer += "{ ";
	if (array_type.elem_type.is<ast::ts_array>())
	{
		bz_assert(values.size() % array_type.size == 0);
		auto const stride = values.size() / array_type.size;
		for (size_t i = 0; i < array_type.size; i += stride)
		{
			auto const sub_array = values.slice(i, i + stride);
			generate_nonzero_constant_numeric_array_value<T>(buffer, sub_array, array_type.elem_type, context);
			buffer += ", ";
		}
	}
	else
	{
		for (auto const value : values)
		{
			if constexpr (bz::meta::is_same<T, int64_t>)
			{
				write_sint(buffer, value);
			}
			else if constexpr (bz::meta::is_same<T, uint64_t>)
			{
				buffer += bz::format("{}u, ", value);
			}
			else if constexpr (bz::meta::is_same<T, float32_t>)
			{
				buffer += bz::format("{}f, ", value);
			}
			else if constexpr (bz::meta::is_same<T, float64_t>)
			{
				buffer += bz::format("{}, ", value);
			}
			else
			{
				static_assert(bz::meta::always_false<T>);
			}
		}
	}
	buffer += '}';
}

static void generate_constant_sint_array_value(
	bz::u8string &buffer,
	bz::array_view<int64_t const> values,
	ast::typespec_view array_type,
	codegen_context &context
)
{
	if (values.is_all([](auto const value) { return value == 0; }))
	{
		buffer += bz::format("({})", context.to_string(get_type(array_type, context)));
		buffer += "{0}";
	}
	else
	{
		generate_nonzero_constant_numeric_array_value(buffer, values, array_type, context);
	}
}

static void generate_constant_uint_array_value(
	bz::u8string &buffer,
	bz::array_view<uint64_t const> values,
	ast::typespec_view array_type,
	codegen_context &context
)
{
	if (values.is_all([](auto const value) { return value == 0; }))
	{
		buffer += bz::format("({})", context.to_string(get_type(array_type, context)));
		buffer += "{0}";
	}
	else
	{
		generate_nonzero_constant_numeric_array_value(buffer, values, array_type, context);
	}
}

static void generate_constant_float32_array_value(
	bz::u8string &buffer,
	bz::array_view<float32_t const> values,
	ast::typespec_view array_type,
	codegen_context &context
)
{
	if (values.is_all([](auto const value) { return bit_cast<uint32_t>(value) == 0; }))
	{
		buffer += bz::format("({})", context.to_string(get_type(array_type, context)));
		buffer += "{0}";
	}
	else
	{
		generate_nonzero_constant_numeric_array_value(buffer, values, array_type, context);
	}
}

static void generate_constant_float64_array_value(
	bz::u8string &buffer,
	bz::array_view<float64_t const> values,
	ast::typespec_view array_type,
	codegen_context &context
)
{
	if (values.is_all([](auto const value) { return bit_cast<uint64_t>(value) == 0; }))
	{
		buffer += bz::format("({})", context.to_string(get_type(array_type, context)));
		buffer += "{0}";
	}
	else
	{
		generate_nonzero_constant_numeric_array_value(buffer, values, array_type, context);
	}
}

static void generate_constant_value(
	bz::u8string &buffer,
	ast::constant_value const &value,
	ast::typespec_view type,
	codegen_context &context
)
{
	type = type.remove_any_mut();
	switch (value.kind())
	{
	case ast::constant_value::sint:
		write_sint(buffer, value.get_sint());
		break;
	case ast::constant_value::uint:
		buffer += bz::format("{}u", value.get_uint());
		break;
	case ast::constant_value::float32:
		buffer += bz::format("{}f", value.get_float32());
		break;
	case ast::constant_value::float64:
		buffer += bz::format("{}", value.get_float64());
		break;
	case ast::constant_value::u8char:
	{
		auto const c = value.get_u8char();
		// ascii character
		if (c <= '\x7f')
		{
			// https://en.cppreference.com/w/c/language/escape
			if (c < ' ' || c == '\x7f' || c == '\'' || c == '\"' || c == '\\')
			{
				switch (c)
				{
				case '\'':
					buffer += "'\\\''";
					break;
				case '\"':
					buffer += "'\\\"'";
					break;
				case '\\':
					buffer += "'\\\\'";
					break;
				case '\a':
					buffer += "'\\a'";
					break;
				case '\b':
					buffer += "'\\b'";
					break;
				case '\f':
					buffer += "'\\f'";
					break;
				case '\n':
					buffer += "'\\n'";
					break;
				case '\r':
					buffer += "'\\r'";
					break;
				case '\t':
					buffer += "'\\t'";
					break;
				case '\v':
					buffer += "'\\v'";
					break;
				default:
					buffer += bz::format("'\\x{:02x}'", c);
					break;
				}
			}
			else
			{
				buffer += bz::format("'{:c}'", c);
			}
		}
		else
		{
			buffer += bz::format("0x{:04x}", c);
		}
		break;
	}
	case ast::constant_value::string:
	{
		auto const s = value.get_string();
		auto const cstr = context.create_cstring(s);
		buffer += bz::format("({})", context.to_string(get_type(type, context)));
		buffer += '{';
		buffer += bz::format(" {0}, {0} + {}", cstr, s.size());
		buffer += '}';
		break;
	}
	case ast::constant_value::boolean:
		if (value.get_boolean())
		{
			buffer += '1';
		}
		else
		{
			buffer += '0';
		}
		break;
	case ast::constant_value::null:
		if (type.is_optional_pointer_like())
		{
			buffer += '0';
		}
		else
		{
			// empty braces is a GNU extension
			buffer += bz::format("({})", context.to_string(get_type(type, context)));
			buffer += "{0}";
		}
		break;
	case ast::constant_value::void_:
		buffer += "(void)0";
		break;
	case ast::constant_value::enum_:
	{
		auto const &enum_val = value.get_enum();
		bz_assert(enum_val.decl->underlying_type.is<ast::ts_base_type>());
		if (ast::is_signed_integer_kind(enum_val.decl->underlying_type.get<ast::ts_base_type>().info->kind))
		{
			auto const int_val = static_cast<int64_t>(enum_val.value);
			write_sint(buffer, int_val);
		}
		else
		{
			auto const int_val = enum_val.value;
			buffer += bz::format("{}u", int_val);
		}
		break;
	}
	case ast::constant_value::array:
		generate_constant_array_value(buffer, value.get_array(), type, context);
		break;
	case ast::constant_value::sint_array:
		generate_constant_sint_array_value(buffer, value.get_sint_array(), type, context);
		break;
	case ast::constant_value::uint_array:
		generate_constant_uint_array_value(buffer, value.get_uint_array(), type, context);
		break;
	case ast::constant_value::float32_array:
		generate_constant_float32_array_value(buffer, value.get_float32_array(), type, context);
		break;
	case ast::constant_value::float64_array:
		generate_constant_float64_array_value(buffer, value.get_float64_array(), type, context);
		break;
	case ast::constant_value::tuple:
	{
		auto const tuple_elems = value.get_tuple();
		bz_assert(type.is<ast::ts_tuple>());
		auto const &tuple_type = type.get<ast::ts_tuple>();
		bz_assert(tuple_elems.size() == tuple_type.types.size());
		buffer += bz::format("({})", context.to_string(get_type(type, context)));
		buffer += "{ ";
		for (auto const i : bz::iota(0, tuple_elems.size()))
		{
			generate_constant_value(buffer, tuple_elems[i], tuple_type.types[i], context);
			buffer += ", ";
		}
		buffer += '}';
		break;
	}
	case ast::constant_value::function:
		// TODO
		bz_unreachable;
	case ast::constant_value::type:
		bz_unreachable;
	case ast::constant_value::aggregate:
	{
		auto const aggregate = value.get_aggregate();
		bz_assert(type.is<ast::ts_base_type>());
		auto const info = type.get<ast::ts_base_type>().info;
		bz_assert(aggregate.size() == info->member_variables.size());
		buffer += bz::format("({})", context.to_string(get_type(type, context)));
		buffer += "{ ";
		for (auto const i : bz::iota(0, aggregate.size()))
		{
			generate_constant_value(buffer, aggregate[i], info->member_variables[i]->get_type(), context);
			buffer += ", ";
		}
		buffer += '}';
		break;
	}
	default:
		bz_unreachable;
	}
}

static bz::u8string generate_constant_value(ast::constant_value const &value, ast::typespec_view type, codegen_context &context)
{
	bz::u8string result = "";
	generate_constant_value(result, value, type, context);
	return result;
}

void generate_global_variable(ast::decl_variable const &var_decl, codegen_context &context)
{
	if (var_decl.is_libc_internal())
	{
		return;
	}

	auto const var_type = get_type(var_decl.get_type(), context);
	if (var_decl.init_expr.is_constant())
	{
		auto const initializer = generate_constant_value(var_decl.init_expr.get_constant_value(), var_decl.get_type(), context);
		context.add_global_variable(var_decl, var_type, initializer);
	}
	else
	{
		context.add_global_variable(var_decl, var_type, "");
	}
}

} // namespace codegen::c
