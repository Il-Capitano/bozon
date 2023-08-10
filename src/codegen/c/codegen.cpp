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

static void write_float32(bz::u8string &buffer, float32_t value)
{
	if (std::isnan(value))
	{
		if (std::signbit(value))
		{
			buffer += "(-0.0f / 0.0f)";
		}
		else
		{
			buffer += "(0.0f / 0.0f)";
		}
	}
	else if (std::isinf(value))
	{
		if (std::signbit(value))
		{
			buffer += "(-1.0f / 0.0f)";
		}
		else
		{
			buffer += "(1.0f / 0.0f)";
		}
	}
	else
	{
		buffer += bz::format("{}f", value);
	}
}

static void write_float64(bz::u8string &buffer, float64_t value)
{
	if (std::isnan(value))
	{
		if (std::signbit(value))
		{
			buffer += "(-0.0 / 0.0)";
		}
		else
		{
			buffer += "(0.0 / 0.0)";
		}
	}
	else if (std::isinf(value))
	{
		if (std::signbit(value))
		{
			buffer += "(-1.0 / 0.0)";
		}
		else
		{
			buffer += "(1.0 / 0.0)";
		}
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
				buffer += ", ";
			}
			else if constexpr (bz::meta::is_same<T, uint64_t>)
			{
				buffer += bz::format("{}u, ", value);
			}
			else if constexpr (bz::meta::is_same<T, float32_t>)
			{
				write_float32(buffer, value);
				buffer += ", ";
			}
			else if constexpr (bz::meta::is_same<T, float64_t>)
			{
				write_float64(buffer, value);
				buffer += ", ";
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
		write_float32(buffer, value.get_float32());
		break;
	case ast::constant_value::float64:
		write_float64(buffer, value.get_float64());
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
	{
		auto const &name = context.get_function(*value.get_function()).name;
		buffer += '&';
		buffer += name;
		break;
	}
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
	bz_assert(var_decl.is_global_storage());
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

static expr_value generate_expression(
	ast::expression const &expr,
	codegen_context &context,
	bz::optional<expr_value> result_dest
);

static void generate_statement(ast::statement const &stmt, codegen_context &context);

static expr_value generate_expression(
	ast::expr_variable_name const &var_name,
	codegen_context &context
)
{
	return context.get_variable(*var_name.decl);
}

static expr_value generate_expression(
	ast::expr_function_name const &,
	codegen_context &context
)
{
	// this is always a constant expression
	bz_unreachable;
}

static expr_value generate_expression(
	ast::expr_function_alias_name const &,
	codegen_context &context
)
{
	// this is always a constant expression
	bz_unreachable;
}

static expr_value generate_expression(
	ast::expr_function_overload_set const &,
	codegen_context &context
)
{
	// this is always a constant expression
	bz_unreachable;
}

static expr_value generate_expression(
	ast::expr_struct_name const &,
	codegen_context &context
)
{
	// this is always a constant expression
	bz_unreachable;
}

static expr_value generate_expression(
	ast::expr_enum_name const &,
	codegen_context &context
)
{
	// this is always a constant expression
	bz_unreachable;
}

static expr_value generate_expression(
	ast::expr_type_alias_name const &,
	codegen_context &context
)
{
	// this is always a constant expression
	bz_unreachable;
}

static expr_value generate_expression(
	ast::expr_integer_literal const &,
	codegen_context &context
)
{
	// this is always a constant expression
	bz_unreachable;
}

static expr_value generate_expression(
	ast::expr_null_literal const &,
	codegen_context &context
)
{
	// this is always a constant expression
	bz_unreachable;
}

static expr_value generate_expression(
	ast::expr_enum_literal const &,
	codegen_context &context
)
{
	// this is always a constant expression
	bz_unreachable;
}

static expr_value generate_expression(
	ast::expr_typed_literal const &,
	codegen_context &context
)
{
	// this is always a constant expression
	bz_unreachable;
}

static expr_value generate_expression(
	ast::expr_placeholder_literal const &,
	codegen_context &context
)
{
	// this is always a constant expression
	bz_unreachable;
}

static expr_value generate_expression(
	ast::expr_typename_literal const &,
	codegen_context &context
)
{
	// this is always a constant expression
	bz_unreachable;
}

static expr_value generate_expression(
	ast::expr_tuple const &tuple_expr,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	for (auto const i : bz::iota(0, tuple_expr.elems.size()))
	{
		if (result_dest.has_value())
		{
			auto const elem_result_address = context.create_struct_gep(result_dest.get(), i);
			generate_expression(tuple_expr.elems[i], context, elem_result_address);
		}
		else
		{
			generate_expression(tuple_expr.elems[i], context, {});
		}
	}

	return result_dest.has_value() ? result_dest.get() : context.get_void_value();
}

static expr_value generate_expression(
	ast::expr_unary_op const &,
	codegen_context &context,
	bz::optional<expr_value> result_dest
);

static expr_value generate_expression(
	ast::expr_binary_op const &,
	codegen_context &context,
	bz::optional<expr_value> result_dest
);

static expr_value generate_expression(
	ast::expr_tuple_subscript const &,
	codegen_context &context,
	bz::optional<expr_value> result_dest
);

static expr_value generate_expression(
	ast::expr_rvalue_tuple_subscript const &,
	codegen_context &context,
	bz::optional<expr_value> result_dest
);

static expr_value generate_expression(
	ast::expr_subscript const &,
	codegen_context &context
);

static expr_value generate_expression(
	ast::expr_rvalue_array_subscript const &,
	codegen_context &context
);

static expr_value generate_expression(
	ast::expr_function_call const &,
	codegen_context &context,
	bz::optional<expr_value> result_dest
);

static expr_value generate_expression(
	ast::expr_indirect_function_call const &,
	codegen_context &context,
	bz::optional<expr_value> result_dest
);

static expr_value generate_expression(
	ast::expr_cast const &,
	codegen_context &context,
	bz::optional<expr_value> result_dest
);

static expr_value generate_expression(
	ast::expr_bit_cast const &,
	codegen_context &context,
	bz::optional<expr_value> result_dest
);

static expr_value generate_expression(
	ast::expr_optional_cast const &,
	codegen_context &context,
	bz::optional<expr_value> result_dest
);

static expr_value generate_expression(
	ast::expr_take_reference const &,
	codegen_context &context
);

static expr_value generate_expression(
	ast::expr_take_move_reference const &,
	codegen_context &context
);

static expr_value generate_expression(
	ast::expr_aggregate_init const &,
	codegen_context &context,
	bz::optional<expr_value> result_dest
);

static expr_value generate_expression(
	ast::expr_array_value_init const &,
	codegen_context &context,
	bz::optional<expr_value> result_dest
);

static expr_value generate_expression(
	ast::expr_aggregate_default_construct const &,
	codegen_context &context,
	bz::optional<expr_value> result_dest
);

static expr_value generate_expression(
	ast::expr_array_default_construct const &,
	codegen_context &context,
	bz::optional<expr_value> result_dest
);

static expr_value generate_expression(
	ast::expr_optional_default_construct const &,
	codegen_context &context,
	bz::optional<expr_value> result_dest
);

static expr_value generate_expression(
	ast::expr_builtin_default_construct const &,
	codegen_context &context,
	bz::optional<expr_value> result_dest
);

static expr_value generate_expression(
	ast::expr_aggregate_copy_construct const &,
	codegen_context &context,
	bz::optional<expr_value> result_dest
);

static expr_value generate_expression(
	ast::expr_array_copy_construct const &,
	codegen_context &context,
	bz::optional<expr_value> result_dest
);

static expr_value generate_expression(
	ast::expr_optional_copy_construct const &,
	codegen_context &context,
	bz::optional<expr_value> result_dest
);

static expr_value generate_expression(
	ast::expr_trivial_copy_construct const &,
	codegen_context &context,
	bz::optional<expr_value> result_dest
);

static expr_value generate_expression(
	ast::expr_aggregate_move_construct const &,
	codegen_context &context,
	bz::optional<expr_value> result_dest
);

static expr_value generate_expression(
	ast::expr_array_move_construct const &,
	codegen_context &context,
	bz::optional<expr_value> result_dest
);

static expr_value generate_expression(
	ast::expr_optional_move_construct const &,
	codegen_context &context,
	bz::optional<expr_value> result_dest
);

static expr_value generate_expression(
	ast::expr_trivial_relocate const &,
	codegen_context &context,
	bz::optional<expr_value> result_dest
);

static expr_value generate_expression(
	ast::expr_aggregate_destruct const &,
	codegen_context &context
);

static expr_value generate_expression(
	ast::expr_array_destruct const &,
	codegen_context &context
);

static expr_value generate_expression(
	ast::expr_optional_destruct const &,
	codegen_context &context
);

static expr_value generate_expression(
	ast::expr_base_type_destruct const &,
	codegen_context &context
);

static expr_value generate_expression(
	ast::expr_destruct_value const &,
	codegen_context &context
);

static expr_value generate_expression(
	ast::expr_aggregate_swap const &,
	codegen_context &context
);

static expr_value generate_expression(
	ast::expr_array_swap const &,
	codegen_context &context
);

static expr_value generate_expression(
	ast::expr_optional_swap const &,
	codegen_context &context
);

static expr_value generate_expression(
	ast::expr_base_type_swap const &,
	codegen_context &context
);

static expr_value generate_expression(
	ast::expr_trivial_swap const &,
	codegen_context &context
);

static expr_value generate_expression(
	ast::expr_aggregate_assign const &,
	codegen_context &context
);

static expr_value generate_expression(
	ast::expr_array_assign const &,
	codegen_context &context
);

static expr_value generate_expression(
	ast::expr_optional_assign const &,
	codegen_context &context
);

static expr_value generate_expression(
	ast::expr_optional_null_assign const &,
	codegen_context &context
);

static expr_value generate_expression(
	ast::expr_optional_value_assign const &,
	codegen_context &context
);

static expr_value generate_expression(
	ast::expr_optional_reference_value_assign const &,
	codegen_context &context
);

static expr_value generate_expression(
	ast::expr_base_type_assign const &,
	codegen_context &context
);

static expr_value generate_expression(
	ast::expr_trivial_assign const &,
	codegen_context &context
);

static expr_value generate_expression(
	ast::expr_member_access const &,
	codegen_context &context
);

static expr_value generate_expression(
	ast::expr_optional_extract_value const &,
	codegen_context &context,
	bz::optional<expr_value> result_dest
);

static expr_value generate_expression(
	ast::expr_rvalue_member_access const &,
	codegen_context &context,
	bz::optional<expr_value> result_dest
);

static expr_value generate_expression(
	ast::expr_type_member_access const &,
	codegen_context &context
);

static expr_value generate_expression(
	ast::expr_compound const &,
	codegen_context &context,
	bz::optional<expr_value> result_dest
);

static expr_value generate_expression(
	ast::expr_if const &,
	codegen_context &context,
	bz::optional<expr_value> result_dest
);

static expr_value generate_expression(
	ast::expr_if_consteval const &,
	codegen_context &context,
	bz::optional<expr_value> result_dest
);

static expr_value generate_expression(
	ast::expr_switch const &,
	codegen_context &context,
	bz::optional<expr_value> result_dest
);

static expr_value generate_expression(
	ast::expr_break const &,
	codegen_context &context
);

static expr_value generate_expression(
	ast::expr_continue const &,
	codegen_context &context
);

static expr_value generate_expression(
	ast::expr_unreachable const &,
	codegen_context &context
);

static expr_value generate_expression(
	ast::expr_generic_type_instantiation const &,
	codegen_context &context,
	bz::optional<expr_value> result_dest
);

static expr_value generate_expression(
	ast::expr_bitcode_value_reference const &bitcode_value_reference,
	codegen_context &context
)
{
	return context.get_value_reference(bitcode_value_reference.index);
}

static expr_value generate_expression(
	ast::expression const &original_expr,
	ast::expr_t const &expr,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	switch (expr.kind())
	{
	static_assert(ast::expr_t::variant_count == 72);
	case ast::expr_t::index<ast::expr_variable_name>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_variable_name>(), context);
	case ast::expr_t::index<ast::expr_function_name>:
		return generate_expression(expr.get<ast::expr_function_name>(), context);
	case ast::expr_t::index<ast::expr_function_alias_name>:
		return generate_expression(expr.get<ast::expr_function_alias_name>(), context);
	case ast::expr_t::index<ast::expr_function_overload_set>:
		return generate_expression(expr.get<ast::expr_function_overload_set>(), context);
	case ast::expr_t::index<ast::expr_struct_name>:
		return generate_expression(expr.get<ast::expr_struct_name>(), context);
	case ast::expr_t::index<ast::expr_enum_name>:
		return generate_expression(expr.get<ast::expr_enum_name>(), context);
	case ast::expr_t::index<ast::expr_type_alias_name>:
		return generate_expression(expr.get<ast::expr_type_alias_name>(), context);
	case ast::expr_t::index<ast::expr_integer_literal>:
		return generate_expression(expr.get<ast::expr_integer_literal>(), context);
	case ast::expr_t::index<ast::expr_null_literal>:
		return generate_expression(expr.get<ast::expr_null_literal>(), context);
	case ast::expr_t::index<ast::expr_enum_literal>:
		return generate_expression(expr.get<ast::expr_enum_literal>(), context);
	case ast::expr_t::index<ast::expr_typed_literal>:
		return generate_expression(expr.get<ast::expr_typed_literal>(), context);
	case ast::expr_t::index<ast::expr_placeholder_literal>:
		return generate_expression(expr.get<ast::expr_placeholder_literal>(), context);
	case ast::expr_t::index<ast::expr_typename_literal>:
		return generate_expression(expr.get<ast::expr_typename_literal>(), context);
	case ast::expr_t::index<ast::expr_tuple>:
		return generate_expression(expr.get<ast::expr_tuple>(), context, result_dest);
	case ast::expr_t::index<ast::expr_unary_op>:
		return generate_expression(expr.get<ast::expr_unary_op>(), context, result_dest);
	case ast::expr_t::index<ast::expr_binary_op>:
		return generate_expression(expr.get<ast::expr_binary_op>(), context, result_dest);
	case ast::expr_t::index<ast::expr_tuple_subscript>:
		return generate_expression(expr.get<ast::expr_tuple_subscript>(), context, result_dest);
	case ast::expr_t::index<ast::expr_rvalue_tuple_subscript>:
		return generate_expression(expr.get<ast::expr_rvalue_tuple_subscript>(), context, result_dest);
	case ast::expr_t::index<ast::expr_subscript>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_subscript>(), context);
	case ast::expr_t::index<ast::expr_rvalue_array_subscript>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_rvalue_array_subscript>(), context);
	case ast::expr_t::index<ast::expr_function_call>:
		return generate_expression(expr.get<ast::expr_function_call>(), context, result_dest);
	case ast::expr_t::index<ast::expr_indirect_function_call>:
		return generate_expression(expr.get<ast::expr_indirect_function_call>(), context, result_dest);
	case ast::expr_t::index<ast::expr_cast>:
		return generate_expression(expr.get<ast::expr_cast>(), context, result_dest);
	case ast::expr_t::index<ast::expr_bit_cast>:
		return generate_expression(expr.get<ast::expr_bit_cast>(), context, result_dest);
	case ast::expr_t::index<ast::expr_optional_cast>:
		return generate_expression(expr.get<ast::expr_optional_cast>(), context, result_dest);
	case ast::expr_t::index<ast::expr_take_reference>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_take_reference>(), context);
	case ast::expr_t::index<ast::expr_take_move_reference>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_take_move_reference>(), context);
	case ast::expr_t::index<ast::expr_aggregate_init>:
		return generate_expression(expr.get<ast::expr_aggregate_init>(), context, result_dest);
	case ast::expr_t::index<ast::expr_array_value_init>:
		return generate_expression(expr.get<ast::expr_array_value_init>(), context, result_dest);
	case ast::expr_t::index<ast::expr_aggregate_default_construct>:
		return generate_expression(expr.get<ast::expr_aggregate_default_construct>(), context, result_dest);
	case ast::expr_t::index<ast::expr_array_default_construct>:
		return generate_expression(expr.get<ast::expr_array_default_construct>(), context, result_dest);
	case ast::expr_t::index<ast::expr_optional_default_construct>:
		return generate_expression(expr.get<ast::expr_optional_default_construct>(), context, result_dest);
	case ast::expr_t::index<ast::expr_builtin_default_construct>:
		return generate_expression(expr.get<ast::expr_builtin_default_construct>(), context, result_dest);
	case ast::expr_t::index<ast::expr_aggregate_copy_construct>:
		return generate_expression(expr.get<ast::expr_aggregate_copy_construct>(), context, result_dest);
	case ast::expr_t::index<ast::expr_array_copy_construct>:
		return generate_expression(expr.get<ast::expr_array_copy_construct>(), context, result_dest);
	case ast::expr_t::index<ast::expr_optional_copy_construct>:
		return generate_expression(expr.get<ast::expr_optional_copy_construct>(), context, result_dest);
	case ast::expr_t::index<ast::expr_trivial_copy_construct>:
		return generate_expression(expr.get<ast::expr_trivial_copy_construct>(), context, result_dest);
	case ast::expr_t::index<ast::expr_aggregate_move_construct>:
		return generate_expression(expr.get<ast::expr_aggregate_move_construct>(), context, result_dest);
	case ast::expr_t::index<ast::expr_array_move_construct>:
		return generate_expression(expr.get<ast::expr_array_move_construct>(), context, result_dest);
	case ast::expr_t::index<ast::expr_optional_move_construct>:
		return generate_expression(expr.get<ast::expr_optional_move_construct>(), context, result_dest);
	case ast::expr_t::index<ast::expr_trivial_relocate>:
		return generate_expression(expr.get<ast::expr_trivial_relocate>(), context, result_dest);
	case ast::expr_t::index<ast::expr_aggregate_destruct>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_aggregate_destruct>(), context);
	case ast::expr_t::index<ast::expr_array_destruct>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_array_destruct>(), context);
	case ast::expr_t::index<ast::expr_optional_destruct>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_optional_destruct>(), context);
	case ast::expr_t::index<ast::expr_base_type_destruct>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_base_type_destruct>(), context);
	case ast::expr_t::index<ast::expr_destruct_value>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_destruct_value>(), context);
	case ast::expr_t::index<ast::expr_aggregate_swap>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_aggregate_swap>(), context);
	case ast::expr_t::index<ast::expr_array_swap>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_array_swap>(), context);
	case ast::expr_t::index<ast::expr_optional_swap>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_optional_swap>(), context);
	case ast::expr_t::index<ast::expr_base_type_swap>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_base_type_swap>(), context);
	case ast::expr_t::index<ast::expr_trivial_swap>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_trivial_swap>(), context);
	case ast::expr_t::index<ast::expr_aggregate_assign>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_aggregate_assign>(), context);
	case ast::expr_t::index<ast::expr_array_assign>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_array_assign>(), context);
	case ast::expr_t::index<ast::expr_optional_assign>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_optional_assign>(), context);
	case ast::expr_t::index<ast::expr_optional_null_assign>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_optional_null_assign>(), context);
	case ast::expr_t::index<ast::expr_optional_value_assign>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_optional_value_assign>(), context);
	case ast::expr_t::index<ast::expr_optional_reference_value_assign>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_optional_reference_value_assign>(), context);
	case ast::expr_t::index<ast::expr_base_type_assign>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_base_type_assign>(), context);
	case ast::expr_t::index<ast::expr_trivial_assign>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_trivial_assign>(), context);
	case ast::expr_t::index<ast::expr_member_access>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_member_access>(), context);
	case ast::expr_t::index<ast::expr_optional_extract_value>:
		return generate_expression(expr.get<ast::expr_optional_extract_value>(), context, result_dest);
	case ast::expr_t::index<ast::expr_rvalue_member_access>:
		return generate_expression(expr.get<ast::expr_rvalue_member_access>(), context, result_dest);
	case ast::expr_t::index<ast::expr_type_member_access>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_type_member_access>(), context);
	case ast::expr_t::index<ast::expr_compound>:
		return generate_expression(expr.get<ast::expr_compound>(), context, result_dest);
	case ast::expr_t::index<ast::expr_if>:
		return generate_expression(expr.get<ast::expr_if>(), context, result_dest);
	case ast::expr_t::index<ast::expr_if_consteval>:
		return generate_expression(expr.get<ast::expr_if_consteval>(), context, result_dest);
	case ast::expr_t::index<ast::expr_switch>:
		return generate_expression(expr.get<ast::expr_switch>(), context, result_dest);
	case ast::expr_t::index<ast::expr_break>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_break>(), context);
	case ast::expr_t::index<ast::expr_continue>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_continue>(), context);
	case ast::expr_t::index<ast::expr_unreachable>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_unreachable>(), context);
	case ast::expr_t::index<ast::expr_generic_type_instantiation>:
		return generate_expression(expr.get<ast::expr_generic_type_instantiation>(), context, result_dest);
	case ast::expr_t::index<ast::expr_bitcode_value_reference>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_bitcode_value_reference>(), context);
	default:
		bz_unreachable;
	}
}

static expr_value generate_expression(
	ast::expression const &original_expr,
	ast::constant_expression const &const_expr,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	auto const value_string = generate_constant_value(const_expr.value, const_expr.type, context);

	if (result_dest.has_value())
	{
		auto const &result_value = result_dest.get();
		context.add_expression(bz::format(
			"{} = {}",
			context.to_string_lhs(result_value, precedence::assignment),
			value_string
		));
		return result_value;
	}
	else
	{
		auto const expr_type = get_type(const_expr.type, context);
		return context.add_value_expression(value_string, expr_type);
	}
}

static expr_value generate_expression(
	ast::expression const &original_expr,
	ast::dynamic_expression const &dyn_expr,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	if (
		!result_dest.has_value()
		&& dyn_expr.kind == ast::expression_type_kind::rvalue
		&& (
			(dyn_expr.destruct_op.not_null() && !dyn_expr.destruct_op.is<ast::trivial_destruct_self>())
			|| dyn_expr.expr.is<ast::expr_compound>()
			|| dyn_expr.expr.is<ast::expr_if>()
			|| dyn_expr.expr.is<ast::expr_switch>()
			|| dyn_expr.expr.is<ast::expr_tuple>()
		)
	)
	{
		result_dest = context.add_uninitialized_value(get_type(dyn_expr.type, context));
	}

	auto const result = generate_expression(original_expr, dyn_expr.expr, context, result_dest);
	if (dyn_expr.destruct_op.not_null() || dyn_expr.destruct_op.move_destructed_decl != nullptr)
	{
		context.push_self_destruct_operation(dyn_expr.destruct_op, result);
	}

	return result;
}

static expr_value generate_expression(
	ast::expression const &expr,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	switch (expr.kind())
	{
	case ast::expression::index_of<ast::constant_expression>:
		return generate_expression(expr, expr.get_constant(), context, result_dest);
	case ast::expression::index_of<ast::dynamic_expression>:
		return generate_expression(expr, expr.get_dynamic(), context, result_dest);
	case ast::expression::index_of<ast::error_expression>:
		bz_unreachable;
	default:
		bz_unreachable;
	}
}

static void generate_statement(ast::stmt_while const &while_stmt, codegen_context &context)
{
	auto const prev_loop_info = context.push_loop();
	context.begin_while("1");

	// condition
	{
		auto const prev_info = context.push_expression_scope();
		auto const condition = generate_expression(while_stmt.condition, context, {});
		context.pop_expression_scope(prev_info);

		context.begin_if_not(condition);
		context.add_expression("break");
		context.end_if();
	}

	// body
	{
		auto const prev_info = context.push_expression_scope();
		generate_expression(while_stmt.while_block, context, {});
		context.pop_expression_scope(prev_info);
	}

	context.end_while();
	context.pop_loop(prev_loop_info);
}

static void generate_statement(ast::stmt_for const &for_stmt, codegen_context &context)
{
	auto const init_prev_info = context.push_expression_scope();
	if (for_stmt.init.not_null())
	{
		generate_statement(for_stmt.init, context);
	}

	auto const prev_loop_info = context.push_loop();
	context.begin_while("1");

	// condition
	if (for_stmt.condition.not_null())
	{
		auto const prev_info = context.push_expression_scope();
		auto const condition = generate_expression(for_stmt.condition, context, {});
		context.pop_expression_scope(prev_info);

		context.begin_if_not(condition);
		context.add_expression("break");
		context.end_if();
	}

	// body
	{
		auto const prev_info = context.push_expression_scope();
		generate_expression(for_stmt.for_block, context, {});
		context.pop_expression_scope(prev_info);
	}

	// iteration
	if (for_stmt.iteration.not_null())
	{
		auto const prev_info = context.push_expression_scope();
		generate_expression(for_stmt.iteration, context, {});
		context.pop_expression_scope(prev_info);
	}

	context.end_while();
	context.pop_loop(prev_loop_info);
	context.pop_expression_scope(init_prev_info);
}

static void generate_statement(ast::stmt_foreach const &foreach_stmt, codegen_context &context)
{
	auto const outer_prev_info = context.push_expression_scope();

	generate_statement(foreach_stmt.range_var_decl, context);
	generate_statement(foreach_stmt.iter_var_decl, context);
	generate_statement(foreach_stmt.end_var_decl, context);

	auto const prev_loop_info = context.push_loop();
	context.begin_while("1");

	// condition
	{
		auto const prev_info = context.push_expression_scope();
		auto const condition = generate_expression(foreach_stmt.condition, context, {});
		context.pop_expression_scope(prev_info);

		context.begin_if_not(condition);
		context.add_expression("break");
		context.end_if();
	}

	// body
	{
		auto const iter_prev_info = context.push_expression_scope();
		generate_statement(foreach_stmt.iter_deref_var_decl, context);
		generate_expression(foreach_stmt.for_block, context, {});
		context.pop_expression_scope(iter_prev_info);
	}

	// iteration
	{
		auto const prev_info = context.push_expression_scope();
		generate_expression(foreach_stmt.iteration, context, {});
		context.pop_expression_scope(prev_info);
	}

	context.end_while();
	context.pop_loop(prev_loop_info);
	context.pop_expression_scope(outer_prev_info);
}

static void generate_statement(ast::stmt_return const &return_stmt, codegen_context &context)
{
	if (return_stmt.expr.is_null())
	{
		context.generate_all_destruct_operations();
		context.add_return();
	}
	else if (context.current_function_info.return_value.has_value())
	{
		auto const prev_info = context.push_expression_scope();
		generate_expression(return_stmt.expr, context, context.current_function_info.return_value);
		context.pop_expression_scope(prev_info);
		context.generate_all_destruct_operations();
		context.add_return();
	}
	else
	{
		auto const prev_info = context.push_expression_scope();
		auto const return_value = generate_expression(return_stmt.expr, context, context.current_function_info.return_value);
		context.pop_expression_scope(prev_info);
		context.generate_all_destruct_operations();
		context.add_return(return_value);
	}
}

static void generate_statement(ast::stmt_defer const &defer_stmt, codegen_context &context)
{
	context.push_destruct_operation(defer_stmt.deferred_expr);
}

static void generate_statement(ast::stmt_no_op const &, codegen_context &)
{
	// nothing
}

static void generate_statement(ast::stmt_expression const &expr_stmt, codegen_context &context)
{
	auto const prev_info = context.push_expression_scope();
	generate_expression(expr_stmt.expr, context, {});
	context.pop_expression_scope(prev_info);
}

static void add_variable_helper(
	ast::decl_variable const &var_decl,
	expr_value value,
	codegen_context &context
)
{
	if (var_decl.tuple_decls.empty())
	{
		context.add_local_variable(var_decl, value);
		if (var_decl.is_ever_moved_from())
		{
			auto const indicator = context.add_move_destruct_indicator(var_decl);
			context.push_variable_destruct_operation(var_decl.destruction, value, indicator);
		}
		else if (!var_decl.get_type().is_any_reference() && !var_decl.is_tuple_outer_ref())
		{
			context.push_variable_destruct_operation(var_decl.destruction, value);
		}
	}
	else
	{
		if (var_decl.get_type().is_any_reference())
		{
			value = context.create_dereference(value);
		}
		for (auto const &[elem_decl, i] : var_decl.tuple_decls.enumerate())
		{
			auto const elem_value = elem_decl.get_type().is_any_reference()
				? context.create_struct_gep_value(value, i)
				: context.create_struct_gep(value, i);
			add_variable_helper(elem_decl, elem_value, context);
		}
	}
}

static void generate_statement(ast::decl_variable const &var_decl, codegen_context &context)
{
	if (var_decl.is_global_storage())
	{
		bz_assert(var_decl.init_expr.is_constant());
		bz_assert(var_decl.get_type().is<ast::ts_consteval>());
		generate_global_variable(var_decl, context);
		auto const &info = context.get_global_variable(var_decl);
		auto value = context.add_reference_expression(bz::format("&{}", info.name), info.var_type, true);
		add_variable_helper(var_decl, value, context);
	}
	else if (var_decl.get_type().is_typename())
	{
		return;
	}
	else
	{
		auto const type = get_type(var_decl.get_type(), context);
		auto const alloca = context.add_uninitialized_value(type);
		if (var_decl.init_expr.not_null())
		{
			auto const prev_info = context.push_expression_scope();
			generate_expression(var_decl.init_expr, context, alloca);
			context.pop_expression_scope(prev_info);
		}
		add_variable_helper(var_decl, alloca, context);
	}
}

static void generate_statement(ast::statement const &stmt, codegen_context &context)
{
	switch (stmt.kind())
	{
	static_assert(ast::statement::variant_count == 17);
	case ast::statement::index<ast::stmt_while>:
		generate_statement(stmt.get<ast::stmt_while>(), context);
		break;
	case ast::statement::index<ast::stmt_for>:
		generate_statement(stmt.get<ast::stmt_for>(), context);
		break;
	case ast::statement::index<ast::stmt_foreach>:
		generate_statement(stmt.get<ast::stmt_foreach>(), context);
		break;
	case ast::statement::index<ast::stmt_return>:
		generate_statement(stmt.get<ast::stmt_return>(), context);
		break;
	case ast::statement::index<ast::stmt_defer>:
		generate_statement(stmt.get<ast::stmt_defer>(), context);
		break;
	case ast::statement::index<ast::stmt_no_op>:
		generate_statement(stmt.get<ast::stmt_no_op>(), context);
		break;
	case ast::statement::index<ast::stmt_expression>:
		generate_statement(stmt.get<ast::stmt_expression>(), context);
		break;
	case ast::statement::index<ast::stmt_static_assert>:
		// nothing
		break;

	case ast::statement::index<ast::decl_variable>:
		generate_statement(stmt.get<ast::decl_variable>(), context);
		break;

	case ast::statement::index<ast::decl_function>:
	case ast::statement::index<ast::decl_operator>:
	case ast::statement::index<ast::decl_function_alias>:
	case ast::statement::index<ast::decl_operator_alias>:
	case ast::statement::index<ast::decl_struct>:
	case ast::statement::index<ast::decl_enum>:
	case ast::statement::index<ast::decl_import>:
	case ast::statement::index<ast::decl_type_alias>:
		break;
	default:
		bz_unreachable;
	}
}

static void generate_function(ast::function_body &func_body, codegen_context &context)
{
	bz_assert(!func_body.is_bitcode_emitted());
	context.reset_current_function(func_body);

	auto const &func_name = context.get_function(func_body).name;
	if (func_body.is_libc_function())
	{
		return;
	}

	auto const return_by_pointer = !func_body.return_type.is<ast::ts_void>() && !ast::is_trivially_relocatable(func_body.return_type);
	auto const static_prefix = func_body.is_external_linkage() ? "" : "static ";

	auto const return_type = return_by_pointer ? context.get_void() : get_type(func_body.return_type, context);
	auto &bodies_string = context.function_bodies_string;
	auto &decls_string = context.function_declarations_string;
	auto const return_type_string = context.to_string(return_type);
	bodies_string += bz::format("{}{} {}(", static_prefix, return_type_string, func_name);
	decls_string += bz::format("{}{} {}(", static_prefix, return_type_string, func_name);

	context.current_function_info.indent_level = 1;

	auto const prev_info = context.push_expression_scope();

	if (return_by_pointer)
	{
		auto const [name, index] = context.make_local_name();
		auto const return_value_type = get_type(func_body.return_type, context);
		auto const type_string = context.to_string(context.add_pointer(return_value_type));
		bodies_string += bz::format("{} {}", type_string, name);
		decls_string += type_string;
		context.current_function_info.return_value = context.make_reference_expression(index, return_value_type, false);
	}

	bool first = !return_by_pointer;
	for (auto const &param : func_body.params)
	{
		if (ast::is_generic_parameter(param))
		{
			generate_statement(param, context);
		}
		// second parameter of main would be generated as 'unsigned char const * const *', which is invalid
		else if (func_body.is_external_linkage() && func_body.symbol_name == "main" && !first)
		{
			auto const [name, index] = context.make_local_name();
			auto const param_type = get_type(param.get_type(), context);
			bodies_string += ", char const * const * ";
			bodies_string += name;
			decls_string += ", char const * const *";
			auto const cast_expr = bz::format("({}){}", context.to_string(param_type), name);
			auto const var_value = context.add_value_expression(cast_expr, param_type);
			context.add_local_variable(param, var_value);
			continue;
		}

		auto const [name, index] = context.make_local_name();
		auto const param_type = get_type(param.get_type(), context);

		if (first)
		{
			first = false;
		}
		else
		{
			bodies_string += ", ";
			decls_string += ", ";
		}

		if (param.get_type().is_any_reference() || ast::is_trivially_relocatable(param.get_type()))
		{
			auto const param_type_string = context.to_string(param_type);
			bodies_string += bz::format("{} {}", param_type_string, name);
			decls_string += param_type_string;
			auto const var_value = context.make_value_expression(index, param_type);
			add_variable_helper(param, var_value, context);
		}
		else
		{
			auto const param_type_string = context.to_string(context.add_pointer(param_type));
			bodies_string += bz::format("{} {}", param_type_string, name);
			decls_string += param_type_string;
			auto const var_value = context.make_reference_expression(index, param_type, false);
			add_variable_helper(param, var_value, context);
		}
	}

	// if no parameters were emitted, we need to add 'void'
	if (first)
	{
		bodies_string += "void";
		decls_string += "void";
	}

	bodies_string += ")\n";
	bodies_string += "{\n";
	decls_string += ");\n";

	for (auto const &stmt : func_body.get_statements())
	{
		generate_statement(stmt, context);
	}

	context.pop_expression_scope(prev_info);

	bodies_string += context.current_function_info.body_string;
	bodies_string += "}\n";
}

void generate_necessary_functions(codegen_context &context)
{
	for (size_t i = 0; i < context.functions_to_compile.size(); ++i)
	{
		auto const func_body = context.functions_to_compile[i];
		if (func_body->is_bitcode_emitted())
		{
			continue;
		}
		generate_function(*func_body, context);
	}
}

void generate_destruct_operation(destruct_operation_info_t const &destruct_op_info, codegen_context &context)
{
	auto const &condition = destruct_op_info.condition;
	auto const move_destruct_indicator = destruct_op_info.move_destruct_indicator;

	if (
		destruct_op_info.destruct_op == nullptr
		|| destruct_op_info.destruct_op->is_null()
		|| destruct_op_info.destruct_op->is<ast::trivial_destruct_self>()
	)
	{
		// nothing
	}
	else if (auto const &destruct_op = *destruct_op_info.destruct_op; destruct_op.is<ast::destruct_variable>())
	{
		bz_assert(destruct_op.get<ast::destruct_variable>().destruct_call->not_null());
		if (condition.has_value())
		{
			context.begin_if(condition.get());

			auto const prev_info = context.push_expression_scope();
			generate_expression(*destruct_op.get<ast::destruct_variable>().destruct_call, context, {});
			context.pop_expression_scope(prev_info);

			context.end_if();
		}
		else
		{
			auto const prev_info = context.push_expression_scope();
			generate_expression(*destruct_op.get<ast::destruct_variable>().destruct_call, context, {});
			context.pop_expression_scope(prev_info);
		}
	}
	else if (destruct_op.is<ast::destruct_self>())
	{
		if (condition.has_value())
		{
			context.begin_if(condition.get());

			auto const prev_info = context.push_expression_scope();
			generate_expression(*destruct_op.get<ast::destruct_self>().destruct_call, context, {});
			context.pop_expression_scope(prev_info);

			context.end_if();
		}
		else
		{
			auto const prev_info = context.push_expression_scope();
			generate_expression(*destruct_op.get<ast::destruct_self>().destruct_call, context, {});
			context.pop_expression_scope(prev_info);
		}
	}
	else if (destruct_op.is<ast::defer_expression>())
	{
		bz_assert(!condition.has_value());
		auto const prev_info = context.push_expression_scope();
		generate_expression(*destruct_op.get<ast::defer_expression>().expr, context, {});
		context.pop_expression_scope(prev_info);
	}
	else if (destruct_op.is<ast::destruct_rvalue_array>())
	{
		// TODO
		bz_unreachable;
	}
	else
	{
		static_assert(ast::destruct_operation::variant_count == 5);
	}

	if (move_destruct_indicator.has_value())
	{
		auto assign_string = context.to_string_lhs(move_destruct_indicator.get(), precedence::assignment);
		assign_string += " = 0";
		context.add_expression(assign_string);
	}
}

} // namespace codegen::c
