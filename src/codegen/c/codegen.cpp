#include "codegen.h"
#include "ast/statement.h"
#include "global_data.h"

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
			// can be optional function as well
			auto const &func_type = ts.terminator->get<ast::ts_function>();
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
			auto const is_const = !slice_type.elem_type.is_mut();
			return type(context.add_slice({ .elem_type = elem_type, .is_const = is_const }));
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
		// make sure we test for 'int' first, because of arithmetic type promotion rules
		// on some systems it can happen, that 'short' is the same size as 'int', but
		// it has a lower conversion rank, therefore 'unsigned short' would be implicitly
		// cast to 'int' in arithmetic operations, which would be bad
		else if (context.int_size == size)
		{
			return is_signed ? "int" : "unsigned int";
		}
		else if (context.short_size == size)
		{
			return is_signed ? "short" : "unsigned short";
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

static void generate_constant_value_string(
	bz::u8string &buffer,
	ast::constant_value const &value,
	ast::typespec_view type,
	bool use_struct_literals,
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
	case ast::constant_value_kind::sint:
		return value.get_sint() == 0;
	case ast::constant_value_kind::uint:
		return value.get_uint() == 0;
	case ast::constant_value_kind::float32:
		return bit_cast<uint32_t>(value.get_float32()) == 0;
	case ast::constant_value_kind::float64:
		return bit_cast<uint64_t>(value.get_float64()) == 0;
	case ast::constant_value_kind::u8char:
		return value.get_u8char() == 0;
	case ast::constant_value_kind::string:
		return value.get_string() == "";
	case ast::constant_value_kind::boolean:
		return value.get_boolean() == false;
	case ast::constant_value_kind::null:
		return true;
	case ast::constant_value_kind::void_:
		return true;
	case ast::constant_value_kind::enum_:
		return value.get_enum().value == 0;
	case ast::constant_value_kind::array:
		return value.get_array().is_all([](auto const &value) { return is_zero_value(value); });
	case ast::constant_value_kind::sint_array:
		return value.get_sint_array().is_all([](auto const value) { return value == 0; });
	case ast::constant_value_kind::uint_array:
		return value.get_uint_array().is_all([](auto const value) { return value == 0; });
	case ast::constant_value_kind::float32_array:
		return value.get_float32_array().is_all([](auto const value) { return bit_cast<uint32_t>(value) == 0; });
	case ast::constant_value_kind::float64_array:
		return value.get_float64_array().is_all([](auto const value) { return bit_cast<uint64_t>(value) == 0; });
	case ast::constant_value_kind::tuple:
		return value.get_tuple().is_all([](auto const &value) { return is_zero_value(value); });
	case ast::constant_value_kind::function:
		return false;
	case ast::constant_value_kind::aggregate:
		return value.get_aggregate().is_all([](auto const &value) { return is_zero_value(value); });
	case ast::constant_value_kind::type:
		bz_unreachable;
	default:
		bz_unreachable;
	}
}

static void generate_nonzero_constant_array_value(
	bz::u8string &buffer,
	bz::array_view<ast::constant_value const> values,
	ast::typespec_view array_type_,
	bool use_struct_literals,
	codegen_context &context
)
{
	bz_assert(array_type_.is<ast::ts_array>());
	auto const &array_type = array_type_.get<ast::ts_array>();
	if (use_struct_literals)
	{
		buffer += bz::format("({})", context.to_string(get_type(array_type_, context)));
	}
	buffer += "{{ ";
	if (array_type.elem_type.is<ast::ts_array>())
	{
		bz_assert(values.size() % array_type.size == 0);
		auto const stride = values.size() / array_type.size;
		for (size_t i = 0; i < values.size(); i += stride)
		{
			auto const sub_array = values.slice(i, i + stride);
			generate_nonzero_constant_array_value(buffer, sub_array, array_type.elem_type, use_struct_literals, context);
			buffer += ", ";
		}
	}
	else
	{
		for (auto const i : bz::iota(0, array_type.size))
		{
			generate_constant_value_string(buffer, values[i], array_type.elem_type, use_struct_literals, context);
			buffer += ", ";
		}
	}
	buffer += "}}";
}

static void generate_constant_array_value(
	bz::u8string &buffer,
	bz::array_view<ast::constant_value const> values,
	ast::typespec_view array_type,
	bool use_struct_literals,
	codegen_context &context
)
{
	if (values.is_all([](auto const &value) { return is_zero_value(value); }))
	{
		if (use_struct_literals)
		{
			buffer += bz::format("({})", context.to_string(get_type(array_type, context)));
		}
		buffer += "{0}";
	}
	else
	{
		generate_nonzero_constant_array_value(buffer, values, array_type, use_struct_literals, context);
	}
}

template<typename T>
static void generate_nonzero_constant_numeric_array_value(
	bz::u8string &buffer,
	bz::array_view<T const> values,
	ast::typespec_view array_type_,
	bool use_struct_literals,
	codegen_context &context
)
{
	bz_assert(array_type_.is<ast::ts_array>());
	auto const &array_type = array_type_.get<ast::ts_array>();
	if (use_struct_literals)
	{
		buffer += bz::format("({})", context.to_string(get_type(array_type_, context)));
	}
	buffer += "{{ ";
	if (array_type.elem_type.is<ast::ts_array>())
	{
		bz_assert(values.size() % array_type.size == 0);
		auto const stride = values.size() / array_type.size;
		for (size_t i = 0; i < values.size(); i += stride)
		{
			auto const sub_array = values.slice(i, i + stride);
			generate_nonzero_constant_numeric_array_value<T>(buffer, sub_array, array_type.elem_type, use_struct_literals, context);
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
	buffer += "}}";
}

static void generate_constant_sint_array_value(
	bz::u8string &buffer,
	bz::array_view<int64_t const> values,
	ast::typespec_view array_type,
	bool use_struct_literals,
	codegen_context &context
)
{
	if (values.is_all([](auto const value) { return value == 0; }))
	{
		if (use_struct_literals)
		{
			buffer += bz::format("({})", context.to_string(get_type(array_type, context)));
		}
		buffer += "{0}";
	}
	else
	{
		generate_nonzero_constant_numeric_array_value(buffer, values, array_type, use_struct_literals, context);
	}
}

static void generate_constant_uint_array_value(
	bz::u8string &buffer,
	bz::array_view<uint64_t const> values,
	ast::typespec_view array_type,
	bool use_struct_literals,
	codegen_context &context
)
{
	if (values.is_all([](auto const value) { return value == 0; }))
	{
		if (use_struct_literals)
		{
			buffer += bz::format("({})", context.to_string(get_type(array_type, context)));
		}
		buffer += "{0}";
	}
	else
	{
		generate_nonzero_constant_numeric_array_value(buffer, values, array_type, use_struct_literals, context);
	}
}

static void generate_constant_float32_array_value(
	bz::u8string &buffer,
	bz::array_view<float32_t const> values,
	ast::typespec_view array_type,
	bool use_struct_literals,
	codegen_context &context
)
{
	if (values.is_all([](auto const value) { return bit_cast<uint32_t>(value) == 0; }))
	{
		if (use_struct_literals)
		{
			buffer += bz::format("({})", context.to_string(get_type(array_type, context)));
		}
		buffer += "{0}";
	}
	else
	{
		generate_nonzero_constant_numeric_array_value(buffer, values, array_type, use_struct_literals, context);
	}
}

static void generate_constant_float64_array_value(
	bz::u8string &buffer,
	bz::array_view<float64_t const> values,
	ast::typespec_view array_type,
	bool use_struct_literals,
	codegen_context &context
)
{
	if (values.is_all([](auto const value) { return bit_cast<uint64_t>(value) == 0; }))
	{
		if (use_struct_literals)
		{
			buffer += bz::format("({})", context.to_string(get_type(array_type, context)));
		}
		buffer += "{0}";
	}
	else
	{
		generate_nonzero_constant_numeric_array_value(buffer, values, array_type, use_struct_literals, context);
	}
}

static void generate_constant_value_string(
	bz::u8string &buffer,
	ast::constant_value const &value,
	ast::typespec_view type,
	bool use_struct_literals,
	codegen_context &context
)
{
	type = type.remove_any_mut();
	switch (value.kind())
	{
	case ast::constant_value_kind::sint:
		write_sint(buffer, value.get_sint());
		break;
	case ast::constant_value_kind::uint:
		buffer += bz::format("{}u", value.get_uint());
		break;
	case ast::constant_value_kind::float32:
		write_float32(buffer, value.get_float32());
		break;
	case ast::constant_value_kind::float64:
		write_float64(buffer, value.get_float64());
		break;
	case ast::constant_value_kind::u8char:
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
	case ast::constant_value_kind::string:
	{
		auto const s = value.get_string();
		auto const cstr = context.create_cstring(s);
		if (use_struct_literals)
		{
			buffer += bz::format("({})", context.to_string(get_type(type, context)));
		}
		buffer += '{';
		buffer += bz::format(" {0}, {0} + {} ", cstr, s.size());
		buffer += '}';
		break;
	}
	case ast::constant_value_kind::boolean:
		if (value.get_boolean())
		{
			buffer += '1';
		}
		else
		{
			buffer += '0';
		}
		break;
	case ast::constant_value_kind::null:
		if (type.is_optional_pointer_like())
		{
			buffer += '0';
		}
		else
		{
			// empty braces is a GNU extension
			if (use_struct_literals)
			{
				buffer += bz::format("({})", context.to_string(get_type(type, context)));
			}
			buffer += "{0}";
		}
		break;
	case ast::constant_value_kind::void_:
		buffer += "(void)0";
		break;
	case ast::constant_value_kind::enum_:
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
	case ast::constant_value_kind::array:
		generate_constant_array_value(buffer, value.get_array(), type, use_struct_literals, context);
		break;
	case ast::constant_value_kind::sint_array:
		generate_constant_sint_array_value(buffer, value.get_sint_array(), type, use_struct_literals, context);
		break;
	case ast::constant_value_kind::uint_array:
		generate_constant_uint_array_value(buffer, value.get_uint_array(), type, use_struct_literals, context);
		break;
	case ast::constant_value_kind::float32_array:
		generate_constant_float32_array_value(buffer, value.get_float32_array(), type, use_struct_literals, context);
		break;
	case ast::constant_value_kind::float64_array:
		generate_constant_float64_array_value(buffer, value.get_float64_array(), type, use_struct_literals, context);
		break;
	case ast::constant_value_kind::tuple:
	{
		auto const tuple_elems = value.get_tuple();
		bz_assert(type.is<ast::ts_tuple>());
		auto const &tuple_type = type.get<ast::ts_tuple>();
		bz_assert(tuple_elems.size() == tuple_type.types.size());
		if (use_struct_literals)
		{
			buffer += bz::format("({})", context.to_string(get_type(type, context)));
		}
		buffer += "{ ";
		for (auto const i : bz::iota(0, tuple_elems.size()))
		{
			generate_constant_value_string(buffer, tuple_elems[i], tuple_type.types[i], use_struct_literals, context);
			buffer += ", ";
		}
		buffer += '}';
		break;
	}
	case ast::constant_value_kind::function:
	{
		auto const &name = context.get_function(*value.get_function()).name;
		buffer += '&';
		buffer += name;
		break;
	}
	case ast::constant_value_kind::type:
		bz_unreachable;
	case ast::constant_value_kind::aggregate:
	{
		auto const aggregate = value.get_aggregate();
		bz_assert(type.is<ast::ts_base_type>());
		auto const info = type.get<ast::ts_base_type>().info;
		bz_assert(aggregate.size() == info->member_variables.size());
		if (use_struct_literals)
		{
			buffer += bz::format("({})", context.to_string(get_type(type, context)));
		}
		buffer += "{ ";
		for (auto const i : bz::iota(0, aggregate.size()))
		{
			generate_constant_value_string(buffer, aggregate[i], info->member_variables[i]->get_type(), use_struct_literals, context);
			buffer += ", ";
		}
		buffer += '}';
		break;
	}
	default:
		bz_unreachable;
	}
}

static bz::u8string generate_constant_value_string(
	ast::constant_value const &value,
	ast::typespec_view type,
	codegen_context &context,
	bool use_struct_literals = true
)
{
	bz::u8string result = "";
	generate_constant_value_string(result, value, type, use_struct_literals, context);
	return result;
}

void generate_global_variable(ast::decl_variable const &var_decl, codegen_context &context)
{
	bz_assert(var_decl.is_global_storage());
	if (var_decl.is_libc_internal())
	{
		return;
	}
	else if (var_decl.global_tuple_decl_parent != nullptr)
	{
		return generate_global_variable(*var_decl.global_tuple_decl_parent, context);
	}

	auto const var_type = get_type(var_decl.get_type(), context);
	if (var_decl.init_expr.is_constant())
	{
		auto const initializer = generate_constant_value_string(
			var_decl.init_expr.get_constant_value(),
			var_decl.get_type(),
			context,
			false
		);
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

static expr_value generate_trivial_function_call(
	bz::u8string_view func_string,
	bz::array_view<expr_value const> args,
	type return_type,
	codegen_context &context,
	bz::optional<expr_value> result_dest
);
static expr_value generate_function_call(
	bz::u8string_view func_string,
	bz::array_view<expr_value const> args,
	ast::typespec_view return_type,
	codegen_context &context,
	bz::optional<expr_value> result_dest
);

static expr_value value_or_result_dest(expr_value value, bz::optional<expr_value> result_dest, codegen_context &context)
{
	if (result_dest.has_value())
	{
		auto const &result_value = result_dest.get();
		context.create_assignment(result_value, value);
		return result_value;
	}
	else
	{
		return value;
	}
}

static expr_value value_or_result_dest(
	bz::u8string_view value_string,
	type value_type,
	bz::optional<expr_value> result_dest,
	codegen_context &context
)
{
	if (result_dest.has_value())
	{
		auto const &result_value = result_dest.get();
		context.create_assignment(result_value, value_string);
		return result_value;
	}
	else
	{
		return context.add_value_expression(value_string, value_type);
	}
}

static void generate_panic_call(
	lex::src_tokens const &src_tokens,
	bz::u8string_view message,
	codegen_context &context
)
{
	auto const panic_handler_func_body = context.get_builtin_function(ast::function_body::builtin_panic_handler);
	if (panic_handler_func_body == nullptr)
	{
		context.create_trap();
		return;
	}

	bz_assert(panic_handler_func_body->params.size() == 1);
	bz_assert(panic_handler_func_body->params[0].get_type().is<ast::ts_base_type>());
	bz_assert(panic_handler_func_body->params[0].get_type().get<ast::ts_base_type>().info->kind == ast::type_info::str_);
	auto const &panic_handler_func = context.get_function(*panic_handler_func_body);

	auto const panic_message = context.add_panic_string(bz::format(
		"panic called from {}: {}",
		context.get_location_string(src_tokens), message
	));
	auto const cstring_name = context.create_cstring(panic_message);
	auto const str_type = get_type(panic_handler_func_body->params[0].get_type(), context);
	bz::u8string arg_string = bz::format("({})", context.to_string(str_type));
	arg_string += '{';
	arg_string += bz::format(" {}, {} + {} ", cstring_name, cstring_name, panic_message.size());
	arg_string += '}';
	auto const arg = context.add_value_expression(arg_string, str_type);

	generate_trivial_function_call(
		panic_handler_func.name,
		arg,
		context.get_void(),
		context,
		{}
	);
	// just to be sure...
	context.create_trap();
}

static expr_value get_optional_value(expr_value const &opt_value, codegen_context &context)
{
	if (context.is_pointer_or_function(opt_value.get_type()))
	{
		return opt_value;
	}
	else
	{
		return context.create_struct_gep(opt_value, 0);
	}
}

static expr_value get_optional_has_value(expr_value const &opt_value, codegen_context &context)
{
	if (context.is_pointer_or_function(opt_value.get_type()))
	{
		return context.create_not_equals(opt_value, context.get_null_pointer_value());
	}
	else
	{
		return context.create_struct_gep_value(opt_value, 1);
	}
}

static expr_value get_pointer_is_null(expr_value const &pointer_value, codegen_context &context)
{
	return context.create_equals(pointer_value, context.get_null_pointer_value());
}

static void set_optional_has_value(expr_value const &opt_value, bool has_value, codegen_context &context)
{
	if (context.is_pointer_or_function(opt_value.get_type()))
	{
		if (!has_value)
		{
			context.create_assignment(opt_value, "0");
		}
	}
	else
	{
		context.create_assignment(context.create_struct_gep(opt_value, 1), has_value ? "1" : "0");
	}
}

static void set_optional_has_value(expr_value const &opt_value, expr_value const &has_value, codegen_context &context)
{
	bz_assert(!context.is_pointer_or_function(opt_value.get_type()));
	context.create_assignment(context.create_struct_gep(opt_value, 1), has_value);
}

static void generate_optional_get_value_check(
	lex::src_tokens const &src_tokens,
	expr_value const &opt_value,
	codegen_context &context
)
{
	if (global_data::panic_on_null_get_value)
	{
		auto const has_value = get_optional_has_value(opt_value, context);
		auto const prev_if_info = context.begin_if_not(has_value);
		generate_panic_call(src_tokens, "'get_value' called on a null optional", context);
		context.end_if(prev_if_info);
	}
}

static void generate_null_pointer_arithmetic_check(
	lex::src_tokens const &src_tokens,
	expr_value const &pointer_value,
	codegen_context &context
)
{
	if (global_data::panic_on_null_pointer_arithmetic)
	{
		auto const is_null = get_pointer_is_null(pointer_value, context);
		auto const prev_if_info = context.begin_if(is_null);
		generate_panic_call(src_tokens, "null value used in pointer arithmetic", context);
		context.end_if(prev_if_info);
	}
}

static void generate_null_pointer_arithmetic_check(
	lex::src_tokens const &src_tokens,
	expr_value const &pointer_value,
	expr_value const &offset,
	codegen_context &context
)
{
	if (global_data::panic_on_null_pointer_arithmetic)
	{
		auto const is_null = get_pointer_is_null(pointer_value, context);
		auto const offset_is_not_zero = context.create_not_equals(offset, context.get_zero_value(offset.get_type()));
		auto const is_error = context.create_logical_and(is_null, offset_is_not_zero);
		auto const prev_if_info = context.begin_if(is_error);
		generate_panic_call(src_tokens, "null value used in pointer arithmetic", context);
		context.end_if(prev_if_info);
	}
}

static void generate_null_pointer_arithmetic_check(
	lex::src_tokens const &lhs_src_tokens,
	lex::src_tokens const &rhs_src_tokens,
	expr_value const &lhs_value,
	expr_value const &rhs_value,
	codegen_context &context
)
{
	if (global_data::panic_on_null_pointer_arithmetic)
	{
		auto const lhs_has_value = get_optional_has_value(lhs_value, context);
		auto const rhs_has_value = get_optional_has_value(rhs_value, context);
		auto const lhs_is_null = get_pointer_is_null(lhs_value, context);
		auto const rhs_is_null = get_pointer_is_null(rhs_value, context);

		auto const only_lhs_is_null = context.create_logical_and(lhs_is_null, rhs_has_value);
		auto const only_rhs_is_null = context.create_logical_and(lhs_has_value, rhs_is_null);

		auto const prev_if_info = context.begin_if(only_lhs_is_null);
		generate_panic_call(lhs_src_tokens, "null value used in pointer arithmetic", context);
		context.begin_else_if(only_rhs_is_null);
		generate_panic_call(rhs_src_tokens, "null value used in pointer arithmetic", context);
		context.end_if(prev_if_info);
	}
}

struct loop_info_t
{
	expr_value index;
	codegen_context::while_info_t prev_while_info;
};

static loop_info_t create_loop_start(size_t size, codegen_context &context)
{
	auto const index = context.add_value_expression("0", context.get_usize());
	auto const condition = context.create_relational(index, context.get_unsigned_value(size, index.get_type()), "<");
	auto const prev_while_info = context.begin_while(condition);

	return { index, prev_while_info };
}

static void create_loop_end(loop_info_t loop_info, codegen_context &context)
{
	context.create_prefix_unary_operation(loop_info.index, "++");
	context.end_while(loop_info.prev_while_info);
}

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
			auto const elem_result_dest = context.create_struct_gep(result_dest.get(), i);
			if (tuple_expr.elems[i].get_expr_type().is_any_reference())
			{
				auto const ref_value = generate_expression(tuple_expr.elems[i], context, {});
				context.create_assignment(elem_result_dest, context.create_address_of(ref_value));
			}
			else
			{
				generate_expression(tuple_expr.elems[i], context, elem_result_dest);
			}
		}
		else
		{
			generate_expression(tuple_expr.elems[i], context, {});
		}
	}

	return result_dest.has_value() ? result_dest.get() : context.get_void_value();
}

static expr_value generate_builtin_unary_address_of(
	ast::expression const &expr,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	auto const value = generate_expression(expr, context, {});
	auto const value_ptr = context.create_address_of(value);
	return value_or_result_dest(value_ptr, result_dest, context);
}

static expr_value generate_expression(
	ast::expr_unary_op const &unary_op,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	switch (unary_op.op)
	{
	case lex::token::address_of:
		return generate_builtin_unary_address_of(unary_op.expr, context, result_dest);
	case lex::token::kw_move:
	case lex::token::kw_unsafe_move:
		bz_assert(!result_dest.has_value());
		return generate_expression(unary_op.expr, context, {});
	default:
		bz_unreachable;
	}
}

static expr_value generate_builtin_binary_bool_and(
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	if (!result_dest.has_value())
	{
		result_dest = context.add_uninitialized_value(context.get_bool());
	}
	auto const &result_value = result_dest.get();

	auto const lhs_prev_info = context.push_expression_scope();
	generate_expression(lhs, context, result_value);
	context.pop_expression_scope(lhs_prev_info);

	auto const prev_if_info = context.begin_if(result_value);
	auto const rhs_prev_info = context.push_expression_scope();
	generate_expression(rhs, context, result_value);
	context.pop_expression_scope(rhs_prev_info);
	context.end_if(prev_if_info);

	return result_value;
}

static expr_value generate_builtin_binary_bool_xor(
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	auto const lhs_value = generate_expression(lhs, context, {});
	auto const rhs_value = generate_expression(rhs, context, {});
	auto const result_value = context.create_not_equals(lhs_value, rhs_value);
	return value_or_result_dest(result_value, result_dest, context);
}

static expr_value generate_builtin_binary_bool_or(
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	if (!result_dest.has_value())
	{
		result_dest = context.add_uninitialized_value(context.get_bool());
	}
	auto const &result_value = result_dest.get();

	auto const lhs_prev_info = context.push_expression_scope();
	generate_expression(lhs, context, result_value);
	context.pop_expression_scope(lhs_prev_info);

	auto const prev_if_info = context.begin_if_not(result_value);
	auto const rhs_prev_info = context.push_expression_scope();
	generate_expression(rhs, context, result_value);
	context.pop_expression_scope(rhs_prev_info);
	context.end_if(prev_if_info);

	return result_value;
}

static expr_value generate_expression(
	ast::expr_binary_op const &binary_op,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	switch (binary_op.op)
	{
	case lex::token::comma:
		generate_expression(binary_op.lhs, context, {});
		return generate_expression(binary_op.rhs, context, result_dest);
	case lex::token::bool_and:
		return generate_builtin_binary_bool_and(binary_op.lhs, binary_op.rhs, context, result_dest);
	case lex::token::bool_xor:
		return generate_builtin_binary_bool_xor(binary_op.lhs, binary_op.rhs, context, result_dest);
	case lex::token::bool_or:
		return generate_builtin_binary_bool_or(binary_op.lhs, binary_op.rhs, context, result_dest);
	default:
		bz_unreachable;
	}
}

static expr_value generate_expression(
	ast::expr_tuple_subscript const &tuple_subscript,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	bz_assert(tuple_subscript.index.is<ast::constant_expression>());
	auto const &index_value = tuple_subscript.index.get<ast::constant_expression>().value;
	bz_assert(index_value.is_uint() || index_value.is_sint());
	auto const index_int_value = index_value.is_uint()
		? index_value.get_uint()
		: static_cast<uint64_t>(index_value.get_sint());

	expr_value result = {};
	for (auto const i : bz::iota(0, tuple_subscript.base.elems.size()))
	{
		if (i == index_int_value)
		{
			result = generate_expression(tuple_subscript.base.elems[i], context, result_dest);
		}
		else
		{
			generate_expression(tuple_subscript.base.elems[i], context, {});
		}
	}
	return result;
}

static expr_value generate_expression(
	ast::expr_rvalue_tuple_subscript const &rvalue_tuple_subscript,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	bz_assert(rvalue_tuple_subscript.index.is_constant());
	auto const &index_value = rvalue_tuple_subscript.index.get_constant_value();
	bz_assert(index_value.is_uint() || index_value.is_sint());
	auto const index_int_value = index_value.is_uint()
		? index_value.get_uint()
		: static_cast<uint64_t>(index_value.get_sint());

	auto const base_value = generate_expression(rvalue_tuple_subscript.base, context, {});

	auto const is_reference_result = rvalue_tuple_subscript.elem_refs[index_int_value].get_expr_type().is_reference();
	bz_assert(!result_dest.has_value() || !is_reference_result);
	expr_value result = {};
	for (auto const i : bz::iota(0, rvalue_tuple_subscript.elem_refs.size()))
	{
		if (rvalue_tuple_subscript.elem_refs[i].is_null())
		{
			continue;
		}

		auto const elem_ptr = [&]() {
			if (i == index_int_value && is_reference_result)
			{
				auto const ref_ref = context.create_struct_gep_value(base_value, i);
				return context.create_dereference(ref_ref);
			}
			else
			{
				return context.create_struct_gep(base_value, i);
			}
		}();
		auto const prev_value = context.push_value_reference(elem_ptr);
		if (i == index_int_value)
		{
			auto const prev_info = context.push_expression_scope();
			result = generate_expression(rvalue_tuple_subscript.elem_refs[i], context, result_dest);
			context.pop_expression_scope(prev_info);
		}
		else
		{
			generate_expression(rvalue_tuple_subscript.elem_refs[i], context, {});
		}
		context.pop_value_reference(prev_value);
	}
	return result;
}

static expr_value generate_expression(
	ast::expr_subscript const &subscript,
	codegen_context &context
)
{
	auto const base_type = subscript.base.get_expr_type().remove_mut_reference();
	if (base_type.is<ast::ts_array>())
	{
		auto const array = generate_expression(subscript.base, context, {});
		auto index = generate_expression(subscript.index, context, {});
		return context.create_array_gep(array, index);
	}
	else if (base_type.is<ast::ts_array_slice>())
	{
		auto const slice = generate_expression(subscript.base, context, {});
		auto const index = generate_expression(subscript.index, context, {});

		auto const begin_ptr = context.create_struct_gep_value(slice, 0);
		return context.create_array_slice_gep(begin_ptr, index);
	}
	else
	{
		bz_assert(base_type.is<ast::ts_tuple>());
		auto const tuple = generate_expression(subscript.base, context, {});
		bz_assert(subscript.index.is_constant());
		auto const &index_value = subscript.index.get_constant_value();
		bz_assert(index_value.is_uint() || index_value.is_sint());
		auto const index_int_value = index_value.is_uint()
			? index_value.get_uint()
			: static_cast<uint64_t>(index_value.get_sint());

		auto const &types = base_type.get<ast::ts_tuple>().types;
		if (types[index_int_value].is_reference())
		{
			auto const ref_value = context.create_struct_gep_value(tuple, index_int_value);
			return context.create_dereference(ref_value);
		}
		else
		{
			return context.create_struct_gep(tuple, index_int_value);
		}
	}
}

static expr_value generate_expression(
	ast::expr_rvalue_array_subscript const &rvalue_array_subscript,
	codegen_context &context
)
{
	bz_assert(rvalue_array_subscript.base.get_expr_type().is<ast::ts_array>());
	auto const array = generate_expression(rvalue_array_subscript.base, context, {});
	auto const index = generate_expression(rvalue_array_subscript.index, context, {});
	bz_assert(rvalue_array_subscript.index.get_expr_type().remove_any_mut().is<ast::ts_base_type>());

	auto const result_value = context.create_array_gep(array, index);
	context.push_rvalue_array_destruct_operation(
		rvalue_array_subscript.elem_destruct_op,
		array,
		context.create_address_of(result_value)
	);
	return result_value;
}

static expr_value generate_builtin_function_call(
	bz::u8string_view func_name,
	ast::expr_function_call const &func_call,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	if (func_call.params.size() == 1)
	{
		auto const arg_value = generate_expression(func_call.params[0], context, {});
		return generate_trivial_function_call(func_name, arg_value, arg_value.get_type(), context, result_dest);
	}
	else if (func_call.params.size() == 2)
	{
		auto const arg1_value = generate_expression(func_call.params[0], context, {});
		auto const arg2_value = generate_expression(func_call.params[1], context, {});
		return generate_trivial_function_call(func_name, { arg1_value, arg2_value }, arg1_value.get_type(), context, result_dest);
	}
	else if (func_call.params.size() == 3)
	{
		auto const arg1_value = generate_expression(func_call.params[0], context, {});
		auto const arg2_value = generate_expression(func_call.params[1], context, {});
		auto const arg3_value = generate_expression(func_call.params[1], context, {});
		return generate_trivial_function_call(func_name, { arg1_value, arg2_value, arg3_value }, arg1_value.get_type(), context, result_dest);
	}
	else
	{
		bz_unreachable;
	}
}

static bool needs_cast_for_overflow(uint32_t int_kind, codegen_context &context)
{
	if (ast::is_signed_integer_kind(int_kind))
	{
		return true;
	}

	// types smaller than 'int' are casted up to 'int' for arithmetic operations,
	// so we need to cast every type to at least 'unsigned int'

	auto const operand_size = [&]() -> uint32_t {
		switch (int_kind)
		{
		case ast::type_info::uint8_:
			return 1;
		case ast::type_info::uint16_:
			return 2;
		case ast::type_info::uint32_:
			return 4;
		case ast::type_info::uint64_:
			return 8;
		default:
			return 8;
		}
	}();
	return operand_size < context.int_size;
}

static type get_unsigned_type_for_overflow(uint32_t int_kind, codegen_context &context)
{
	auto const get_unsigned_int_type = [&]() {
		switch (context.int_size)
		{
		case 1:
			return context.get_uint8();
		case 2:
			return context.get_uint16();
		case 4:
			return context.get_uint32();
		case 8:
			return context.get_uint64();
		default:
			return context.get_uint64();
		}
	};
	switch (int_kind)
	{
	case ast::type_info::int8_:
	case ast::type_info::uint8_:
		if (1 < context.int_size)
		{
			return get_unsigned_int_type();
		}
		else
		{
			return context.get_uint8();
		}
	case ast::type_info::int16_:
	case ast::type_info::uint16_:
		if (2 < context.int_size)
		{
			return get_unsigned_int_type();
		}
		else
		{
			return context.get_uint16();
		}
	case ast::type_info::int32_:
	case ast::type_info::uint32_:
		if (4 < context.int_size)
		{
			return get_unsigned_int_type();
		}
		else
		{
			return context.get_uint32();
		}
	case ast::type_info::int64_:
	case ast::type_info::uint64_:
	default:
		if (8 < context.int_size)
		{
			return get_unsigned_int_type();
		}
		else
		{
			return context.get_uint64();
		}
	}
}

static expr_value generate_builtin_unary_plus(
	ast::expression const &expr,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	auto const value = generate_expression(expr, context, {});
	auto const result_value = context.create_plus(value);
	return value_or_result_dest(result_value, result_dest, context);
}

static expr_value generate_builtin_unary_minus(
	ast::expression const &expr,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	bz_assert(expr.get_expr_type().is<ast::ts_base_type>());
	auto const kind = expr.get_expr_type().get<ast::ts_base_type>().info->kind;
	auto const is_int = ast::is_signed_integer_kind(kind);
	auto const value = generate_expression(expr, context, {});
	if (is_int)
	{
		auto const func_name = [&]() -> bz::u8string_view {
			switch (kind)
			{
			case ast::type_info::int8_:
				return "bozon_neg_i8";
			case ast::type_info::int16_:
				return "bozon_neg_i16";
			case ast::type_info::int32_:
				return "bozon_neg_i32";
			case ast::type_info::int64_:
				return "bozon_neg_i64";
			default:
				bz_unreachable;
			}
		}();
		return generate_trivial_function_call(func_name, value, value.get_type(), context, result_dest);
	}
	else
	{
		auto const result_value = context.create_minus(value);
		return value_or_result_dest(result_value, result_dest, context);
	}
}

static expr_value generate_builtin_unary_dereference(
	lex::src_tokens const &src_tokens,
	ast::expression const &expr,
	codegen_context &context
)
{
	auto const value = generate_expression(expr, context, {});
	if (global_data::panic_on_null_dereference && expr.get_expr_type().is_optional_pointer())
	{
		auto const prev_if_info = context.begin_if(get_pointer_is_null(value, context));
		generate_panic_call(src_tokens, "null pointer dereferenced", context);
		context.end_if(prev_if_info);
	}
	return context.create_dereference(value);
}

static expr_value generate_builtin_unary_bit_not(
	ast::expression const &expr,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	bz_assert(expr.get_expr_type().is<ast::ts_base_type>());
	auto const kind = expr.get_expr_type().get<ast::ts_base_type>().info->kind;
	auto const value = generate_expression(expr, context, {});
	auto const result_value = needs_cast_for_overflow(kind, context)
		? context.create_cast(context.create_bit_not(value), value.get_type())
		: context.create_bit_not(value);
	return value_or_result_dest(result_value, result_dest, context);
}

static expr_value generate_builtin_unary_bool_not(
	ast::expression const &expr,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	auto const value = generate_expression(expr, context, {});
	auto const result_value = context.create_bool_not(value);
	return value_or_result_dest(result_value, result_dest, context);
}

static expr_value generate_builtin_unary_plus_plus(
	ast::expression const &expr,
	codegen_context &context
)
{
	auto const value = generate_expression(expr, context, {});
	auto const expr_type = expr.get_expr_type().remove_any_mut_reference();
	if (expr_type.is<ast::ts_base_type>() && needs_cast_for_overflow(expr_type.get<ast::ts_base_type>().info->kind, context))
	{
		auto const kind = expr_type.get<ast::ts_base_type>().info->kind;
		auto const unsigned_type = get_unsigned_type_for_overflow(kind, context);
		auto const cast_value = context.create_cast(value, unsigned_type);
		auto const plus_value = context.create_plus(cast_value, context.get_unsigned_one_value(unsigned_type));
		auto const result_value = context.create_cast(plus_value, value.get_type());
		context.create_assignment(value, result_value);
		return value;
	}
	else
	{
		if (expr_type.is_optional_pointer())
		{
			generate_null_pointer_arithmetic_check(expr.src_tokens, value, context);
		}
		context.create_prefix_unary_operation(value, "++");
		return value;
	}
}

static expr_value generate_builtin_unary_minus_minus(
	ast::expression const &expr,
	codegen_context &context
)
{
	auto const value = generate_expression(expr, context, {});
	auto const expr_type = expr.get_expr_type().remove_any_mut_reference();
	if (expr_type.is<ast::ts_base_type>() && needs_cast_for_overflow(expr_type.get<ast::ts_base_type>().info->kind, context))
	{
		auto const kind = expr_type.get<ast::ts_base_type>().info->kind;
		auto const unsigned_type = get_unsigned_type_for_overflow(kind, context);
		auto const cast_value = context.create_cast(value, unsigned_type);
		auto const minus_value = context.create_minus(cast_value, context.get_unsigned_one_value(unsigned_type));
		auto const result_value = context.create_cast(minus_value, value.get_type());
		context.create_assignment(value, result_value);
		return value;
	}
	else
	{
		if (expr_type.is_optional_pointer())
		{
			generate_null_pointer_arithmetic_check(expr.src_tokens, value, context);
		}
		context.create_prefix_unary_operation(value, "--");
		return value;
	}
}

static expr_value generate_builtin_binary_plus(
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	auto const lhs_value = generate_expression(lhs, context, {});
	auto const rhs_value = generate_expression(rhs, context, {});

	auto const lhs_type = lhs.get_expr_type();
	auto const rhs_type = rhs.get_expr_type();
	if (
		lhs_type.is<ast::ts_base_type>()
		&& rhs_type.is<ast::ts_base_type>()
		&& needs_cast_for_overflow(lhs_type.get<ast::ts_base_type>().info->kind, context)
	)
	{
		auto const kind = lhs_type.get<ast::ts_base_type>().info->kind;
		auto const unsigned_type = get_unsigned_type_for_overflow(kind, context);
		auto const lhs_cast = context.create_cast(lhs_value, unsigned_type);
		auto const rhs_cast = context.create_cast(rhs_value, unsigned_type);
		auto const plus_value = context.create_plus(lhs_cast, rhs_cast);
		auto const result_value = context.create_cast(plus_value, lhs_value.get_type());
		return value_or_result_dest(result_value, result_dest, context);
	}

	bool needs_cast = false;
	auto const result_type = [&]() {
		if (context.is_pointer(lhs_value.get_type()))
		{
			return lhs_value.get_type();
		}
		else if (context.is_pointer(rhs_value.get_type()))
		{
			return rhs_value.get_type();
		}
		else
		{
			auto const lhs_type = lhs.get_expr_type();
			auto const rhs_type = rhs.get_expr_type();
			bz_assert(lhs_type.is<ast::ts_base_type>() && rhs_type.is<ast::ts_base_type>());
			auto const lhs_kind = lhs_type.get<ast::ts_base_type>().info->kind;
			auto const rhs_kind = rhs_type.get<ast::ts_base_type>().info->kind;

			if (lhs_kind == ast::type_info::char_)
			{
				needs_cast = rhs_kind == ast::type_info::uint64_ || rhs_kind == ast::type_info::int64_;
				return lhs_value.get_type();
			}
			else if (rhs_kind == ast::type_info::char_)
			{
				needs_cast = rhs_kind == ast::type_info::uint64_ || rhs_kind == ast::type_info::int64_;
				return rhs_value.get_type();
			}
			else
			{
				bz_assert(lhs_value.get_type() == rhs_value.get_type());
				return lhs_value.get_type();
			}
		}
	}();

	if (needs_cast)
	{
		auto const plus_value = context.create_plus(lhs_value, rhs_value);
		auto const result_value = context.create_cast(plus_value, result_type);
		return value_or_result_dest(result_value, result_dest, context);
	}
	else
	{
		expr_value result_value;
		if (lhs_type.is_optional_pointer())
		{
			generate_null_pointer_arithmetic_check(lhs.src_tokens, lhs_value, rhs_value, context);
			result_value = context.create_pointer_plus(lhs_value, rhs_value);
		}
		else if (rhs_type.is_optional_pointer())
		{
			generate_null_pointer_arithmetic_check(rhs.src_tokens, rhs_value, lhs_value, context);
			result_value = context.create_pointer_plus(rhs_value, lhs_value);
		}
		else
		{
			result_value = context.create_plus(lhs_value, rhs_value, result_type);
		}
		return value_or_result_dest(result_value, result_dest, context);
	}
}

static expr_value generate_builtin_binary_plus_eq(
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context
)
{
	auto const rhs_value = generate_expression(rhs, context, {});
	auto const lhs_value = generate_expression(lhs, context, {});

	auto const lhs_type = lhs.get_expr_type().remove_mut_reference();
	auto const rhs_type = rhs.get_expr_type();
	if (
		lhs_type.is<ast::ts_base_type>()
		&& rhs_type.is<ast::ts_base_type>()
		&& needs_cast_for_overflow(lhs_type.get<ast::ts_base_type>().info->kind, context)
	)
	{
		auto const kind = lhs_type.get<ast::ts_base_type>().info->kind;
		auto const unsigned_type = get_unsigned_type_for_overflow(kind, context);
		auto const lhs_cast = context.create_cast(lhs_value, unsigned_type);
		auto const rhs_cast = context.create_cast(rhs_value, unsigned_type);
		auto const plus_value = context.create_plus(lhs_cast, rhs_cast);
		auto const result_value = context.create_cast(plus_value, lhs_value.get_type());
		context.create_assignment(lhs_value, result_value);
		return lhs_value;
	}
	else
	{
		if (lhs_type.is_optional_pointer())
		{
			generate_null_pointer_arithmetic_check(lhs.src_tokens, lhs_value, rhs_value, context);
			context.create_pointer_plus_eq(lhs_value, rhs_value);
		}
		else
		{
			context.create_plus_eq(lhs_value, rhs_value);
		}
		return lhs_value;
	}
}

static expr_value generate_builtin_binary_minus(
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	auto const lhs_value = generate_expression(lhs, context, {});
	auto const rhs_value = generate_expression(rhs, context, {});

	auto const lhs_type = lhs.get_expr_type();
	auto const rhs_type = rhs.get_expr_type();
	if (
		lhs_type.is<ast::ts_base_type>()
		&& rhs_type.is<ast::ts_base_type>()
		&& needs_cast_for_overflow(lhs_type.get<ast::ts_base_type>().info->kind, context)
	)
	{
		auto const kind = lhs_type.get<ast::ts_base_type>().info->kind;
		auto const unsigned_type = get_unsigned_type_for_overflow(kind, context);
		auto const lhs_cast = context.create_cast(lhs_value, unsigned_type);
		auto const rhs_cast = context.create_cast(rhs_value, unsigned_type);
		auto const minus_value = context.create_minus(lhs_cast, rhs_cast);
		auto const result_value = context.create_cast(minus_value, lhs_value.get_type());
		return value_or_result_dest(result_value, result_dest, context);
	}

	bool needs_cast = false;
	auto const result_type = [&]() {
		auto const lhs_is_pointer = context.is_pointer(lhs_value.get_type());
		if (lhs_is_pointer && context.is_pointer(rhs_value.get_type()))
		{
			return context.get_isize();
		}
		else if (lhs_is_pointer)
		{
			return lhs_value.get_type();
		}
		else
		{
			auto const lhs_type = lhs.get_expr_type();
			auto const rhs_type = rhs.get_expr_type();
			bz_assert(lhs_type.is<ast::ts_base_type>() && rhs_type.is<ast::ts_base_type>());
			auto const lhs_kind = lhs_type.get<ast::ts_base_type>().info->kind;
			auto const rhs_kind = rhs_type.get<ast::ts_base_type>().info->kind;

			if (lhs_kind == ast::type_info::char_ && rhs_kind == ast::type_info::char_)
			{
				needs_cast = true;
				return context.get_int32();
			}
			else if (lhs_kind == ast::type_info::char_)
			{
				needs_cast = rhs_kind == ast::type_info::uint64_ || rhs_kind == ast::type_info::int64_;
				return lhs_value.get_type();
			}
			else
			{
				bz_assert(lhs_value.get_type() == rhs_value.get_type());
				return lhs_value.get_type();
			}
		}
	}();

	if (needs_cast)
	{
		auto const minus_value = context.create_minus(lhs_value, rhs_value);
		auto const result_value = context.create_cast(minus_value, result_type);
		return value_or_result_dest(result_value, result_dest, context);
	}
	else
	{
		expr_value result_value;
		if (lhs_type.is_optional_pointer() && rhs_type.is_optional_pointer())
		{
			generate_null_pointer_arithmetic_check(lhs.src_tokens, rhs.src_tokens, lhs_value, rhs_value, context);
			result_value = context.create_minus(lhs_value, rhs_value, result_type);
		}
		else if (lhs_type.is_optional_pointer())
		{
			generate_null_pointer_arithmetic_check(lhs.src_tokens, lhs_value, rhs_value, context);
			result_value = context.create_pointer_minus(lhs_value, rhs_value);
		}
		else
		{
			result_value = context.create_minus(lhs_value, rhs_value, result_type);
		}
		return value_or_result_dest(result_value, result_dest, context);
	}
}

static expr_value generate_builtin_binary_minus_eq(
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context
)
{
	auto const rhs_value = generate_expression(rhs, context, {});
	auto const lhs_value = generate_expression(lhs, context, {});

	auto const lhs_type = lhs.get_expr_type().remove_mut_reference();
	auto const rhs_type = rhs.get_expr_type();
	if (
		lhs_type.is<ast::ts_base_type>()
		&& rhs_type.is<ast::ts_base_type>()
		&& needs_cast_for_overflow(lhs_type.get<ast::ts_base_type>().info->kind, context)
	)
	{
		auto const kind = lhs_type.get<ast::ts_base_type>().info->kind;
		auto const unsigned_type = get_unsigned_type_for_overflow(kind, context);
		auto const lhs_cast = context.create_cast(lhs_value, unsigned_type);
		auto const rhs_cast = context.create_cast(rhs_value, unsigned_type);
		auto const minus_value = context.create_minus(lhs_cast, rhs_cast);
		auto const result_value = context.create_cast(minus_value, lhs_value.get_type());
		context.create_assignment(lhs_value, result_value);
		return lhs_value;
	}
	else
	{
		if (lhs_type.is_optional_pointer())
		{
			generate_null_pointer_arithmetic_check(lhs.src_tokens, lhs_value, rhs_value, context);
			context.create_pointer_minus_eq(lhs_value, rhs_value);
		}
		else
		{
			context.create_minus_eq(lhs_value, rhs_value);
		}
		return lhs_value;
	}
}

static expr_value generate_builtin_binary_multiply(
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	auto const lhs_value = generate_expression(lhs, context, {});
	auto const rhs_value = generate_expression(rhs, context, {});

	bz_assert(rhs.get_expr_type().is<ast::ts_base_type>());
	auto const kind = rhs.get_expr_type().get<ast::ts_base_type>().info->kind;
	if (needs_cast_for_overflow(kind, context))
	{
		auto const unsigned_type = get_unsigned_type_for_overflow(kind, context);
		auto const lhs_cast = context.create_cast(lhs_value, unsigned_type);
		auto const rhs_cast = context.create_cast(rhs_value, unsigned_type);
		auto const multiply_value = context.create_multiply(lhs_cast, rhs_cast);
		auto const result_value = context.create_cast(multiply_value, lhs_value.get_type());
		return value_or_result_dest(result_value, result_dest, context);
	}
	else
	{
		auto const result_value = context.create_multiply(lhs_value, rhs_value);
		return value_or_result_dest(result_value, result_dest, context);
	}
}

static expr_value generate_builtin_binary_multiply_eq(
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context
)
{
	auto const rhs_value = generate_expression(rhs, context, {});
	auto const lhs_value = generate_expression(lhs, context, {});

	bz_assert(rhs.get_expr_type().is<ast::ts_base_type>());
	auto const kind = rhs.get_expr_type().get<ast::ts_base_type>().info->kind;
	if (needs_cast_for_overflow(kind, context))
	{
		auto const unsigned_type = get_unsigned_type_for_overflow(kind, context);
		auto const lhs_cast = context.create_cast(lhs_value, unsigned_type);
		auto const rhs_cast = context.create_cast(rhs_value, unsigned_type);
		auto const multiply_value = context.create_multiply(lhs_cast, rhs_cast);
		auto const result_value = context.create_cast(multiply_value, lhs_value.get_type());
		context.create_assignment(lhs_value, result_value);
		return lhs_value;
	}
	else
	{
		context.create_multiply_eq(lhs_value, rhs_value);
		return lhs_value;
	}
}

static expr_value generate_builtin_binary_divide(
	lex::src_tokens const &src_tokens,
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	auto const lhs_value = generate_expression(lhs, context, {});
	auto const rhs_value = generate_expression(rhs, context, {});

	bz_assert(rhs.get_expr_type().is<ast::ts_base_type>());
	auto const kind = rhs.get_expr_type().get<ast::ts_base_type>().info->kind;

	if (global_data::panic_on_int_divide_by_zero && ast::is_integer_kind(kind))
	{
		auto const is_rhs_zero = context.create_equals(rhs_value, context.get_zero_value(lhs_value.get_type()));
		auto const prev_if_info = context.begin_if(is_rhs_zero);

		generate_panic_call(src_tokens, "integer division by zero", context);

		context.end_if(prev_if_info);
	}

	if (ast::is_signed_integer_kind(kind))
	{
		auto const func_name = [&]() -> bz::u8string_view {
			switch (kind)
			{
			case ast::type_info::int8_:
				return "bozon_div_i8";
			case ast::type_info::int16_:
				return "bozon_div_i16";
			case ast::type_info::int32_:
				return "bozon_div_i32";
			case ast::type_info::int64_:
				return "bozon_div_i64";
			default:
				bz_unreachable;
			}
		}();
		return generate_trivial_function_call(func_name, { lhs_value, rhs_value }, lhs_value.get_type(), context, result_dest);
	}
	else
	{
		auto const result_value = context.create_divide(lhs_value, rhs_value);
		return value_or_result_dest(result_value, result_dest, context);
	}
}

static expr_value generate_builtin_binary_divide_eq(
	lex::src_tokens const &src_tokens,
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context
)
{
	auto const rhs_value = generate_expression(rhs, context, {});
	auto const lhs_value = generate_expression(lhs, context, {});

	bz_assert(rhs.get_expr_type().is<ast::ts_base_type>());
	auto const kind = rhs.get_expr_type().get<ast::ts_base_type>().info->kind;

	if (global_data::panic_on_int_divide_by_zero && ast::is_integer_kind(kind))
	{
		auto const is_rhs_zero = context.create_equals(rhs_value, context.get_zero_value(lhs_value.get_type()));
		auto const prev_if_info = context.begin_if(is_rhs_zero);

		generate_panic_call(src_tokens, "integer division by zero", context);

		context.end_if(prev_if_info);
	}

	if (ast::is_signed_integer_kind(kind))
	{
		auto const func_name = [&]() -> bz::u8string_view {
			switch (kind)
			{
			case ast::type_info::int8_:
				return "bozon_div_i8";
			case ast::type_info::int16_:
				return "bozon_div_i16";
			case ast::type_info::int32_:
				return "bozon_div_i32";
			case ast::type_info::int64_:
				return "bozon_div_i64";
			default:
				bz_unreachable;
			}
		}();
		return generate_trivial_function_call(func_name, { lhs_value, rhs_value }, lhs_value.get_type(), context, lhs_value);
	}
	else
	{
		context.create_divide_eq(lhs_value, rhs_value);
		return lhs_value;
	}
}

static expr_value generate_builtin_binary_modulo(
	lex::src_tokens const &src_tokens,
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	auto const lhs_value = generate_expression(lhs, context, {});
	auto const rhs_value = generate_expression(rhs, context, {});

	bz_assert(rhs.get_expr_type().is<ast::ts_base_type>());
	auto const kind = rhs.get_expr_type().get<ast::ts_base_type>().info->kind;

	if (global_data::panic_on_int_divide_by_zero)
	{
		auto const is_rhs_zero = context.create_equals(rhs_value, context.get_zero_value(rhs_value.get_type()));
		auto const prev_if_info = context.begin_if(is_rhs_zero);

		generate_panic_call(src_tokens, "integer division by zero", context);

		context.end_if(prev_if_info);
	}

	if (ast::is_signed_integer_kind(kind))
	{
		auto const func_name = [&]() -> bz::u8string_view {
			switch (kind)
			{
			case ast::type_info::int8_:
				return "bozon_rem_i8";
			case ast::type_info::int16_:
				return "bozon_rem_i16";
			case ast::type_info::int32_:
				return "bozon_rem_i32";
			case ast::type_info::int64_:
				return "bozon_rem_i64";
			default:
				bz_unreachable;
			}
		}();
		return generate_trivial_function_call(func_name, { lhs_value, rhs_value }, lhs_value.get_type(), context, result_dest);
	}
	else
	{
		auto const result_value = context.create_modulo(lhs_value, rhs_value);
		return value_or_result_dest(result_value, result_dest, context);
	}
}

static expr_value generate_builtin_binary_modulo_eq(
	lex::src_tokens const &src_tokens,
	ast::expression const &lhs,
	ast::expression const &rhs,
	codegen_context &context
)
{
	auto const rhs_value = generate_expression(rhs, context, {});
	auto const lhs_value = generate_expression(lhs, context, {});

	bz_assert(rhs.get_expr_type().is<ast::ts_base_type>());
	auto const kind = rhs.get_expr_type().get<ast::ts_base_type>().info->kind;

	if (global_data::panic_on_int_divide_by_zero)
	{
		auto const is_rhs_zero = context.create_equals(rhs_value, context.get_zero_value(rhs_value.get_type()));
		auto const prev_if_info = context.begin_if(is_rhs_zero);

		generate_panic_call(src_tokens, "integer division by zero", context);

		context.end_if(prev_if_info);
	}

	if (ast::is_signed_integer_kind(kind))
	{
		auto const func_name = [&]() -> bz::u8string_view {
			switch (kind)
			{
			case ast::type_info::int8_:
				return "bozon_rem_i8";
			case ast::type_info::int16_:
				return "bozon_rem_i16";
			case ast::type_info::int32_:
				return "bozon_rem_i32";
			case ast::type_info::int64_:
				return "bozon_rem_i64";
			default:
				bz_unreachable;
			}
		}();
		return generate_trivial_function_call(func_name, { lhs_value, rhs_value }, lhs_value.get_type(), context, lhs_value);
	}
	else
	{
		context.create_modulo_eq(lhs_value, rhs_value);
		return lhs_value;
	}
}

static expr_value generate_builtin_binary_equality(
	ast::expression const &lhs,
	ast::expression const &rhs,
	bz::u8string_view op,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	auto const lhs_value = generate_expression(lhs, context, {});
	auto const rhs_value = generate_expression(rhs, context, {});

	auto const lhs_t = lhs.get_expr_type().remove_reference();
	auto const rhs_t = rhs.get_expr_type().remove_reference();

	if (
		(lhs_t.is<ast::ts_optional>() && rhs_t.is<ast::ts_base_type>())
		|| (lhs_t.is<ast::ts_base_type>() && rhs_t.is<ast::ts_optional>())
	)
	{
		auto const &optional_value = lhs_t.is_optional() ? lhs_value : rhs_value;
		auto const &optional_type = lhs_t.is_optional() ? lhs_t : rhs_t;
		if (optional_type.is_optional_pointer_like())
		{
			auto const result_value = context.create_equality(optional_value, context.get_null_pointer_value(), op);
			return value_or_result_dest(result_value, result_dest, context);
		}
		else if (op == "!=")
		{
			auto const has_value = get_optional_has_value(optional_value, context);
			return value_or_result_dest(has_value, result_dest, context);
		}
		else
		{
			bz_assert(op == "==");
			auto const has_value = get_optional_has_value(optional_value, context);
			auto const result_value = context.create_bool_not(has_value);
			return value_or_result_dest(result_value, result_dest, context);
		}
	}
	else
	{
		auto const result_value = context.create_equality(lhs_value, rhs_value, op);
		return value_or_result_dest(result_value, result_dest, context);
	}
}

static expr_value generate_builtin_binary_compare(
	ast::expression const &lhs,
	ast::expression const &rhs,
	bz::u8string_view op,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	auto const lhs_value = generate_expression(lhs, context, {});
	auto const rhs_value = generate_expression(rhs, context, {});
	auto const result_value = context.create_relational(lhs_value, rhs_value, op);
	return value_or_result_dest(result_value, result_dest, context);
}

static expr_value generate_builtin_binary_bit_op(
	ast::expression const &lhs,
	ast::expression const &rhs,
	bz::u8string_view op,
	precedence prec,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	auto const lhs_value = generate_expression(lhs, context, {});
	auto const rhs_value = generate_expression(rhs, context, {});
	auto const result_value = context.create_bit_op(lhs_value, rhs_value, op, prec);
	return value_or_result_dest(result_value, result_dest, context);
}

static expr_value generate_builtin_binary_bit_eq_op(
	ast::expression const &lhs,
	ast::expression const &rhs,
	bz::u8string_view op,
	codegen_context &context
)
{
	auto const rhs_value = generate_expression(rhs, context, {});
	auto const lhs_value = generate_expression(lhs, context, {});
	context.create_bit_op_eq(lhs_value, rhs_value, op);
	return lhs_value;
}

static expr_value generate_builtin_binary_bitshift(
	ast::expression const &lhs,
	ast::expression const &rhs,
	bz::u8string_view op,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	bz_assert(lhs.get_expr_type().is<ast::ts_base_type>());
	auto const kind = lhs.get_expr_type().get<ast::ts_base_type>().info->kind;
	auto const lhs_value = generate_expression(lhs, context, {});
	auto const rhs_value = generate_expression(rhs, context, {});

	auto const result_value = needs_cast_for_overflow(kind, context)
		? context.create_cast(context.create_bitshift(lhs_value, rhs_value, op), lhs_value.get_type())
		: context.create_bitshift(lhs_value, rhs_value, op);
	return value_or_result_dest(result_value, result_dest, context);
}

static expr_value generate_builtin_binary_bitshift_eq(
	ast::expression const &lhs,
	ast::expression const &rhs,
	bz::u8string_view op,
	codegen_context &context
)
{
	auto const rhs_value = generate_expression(rhs, context, {});
	auto const lhs_value = generate_expression(lhs, context, {});
	context.create_bitshift_eq(lhs_value, rhs_value, op);
	return lhs_value;
}

enum class range_kind
{
	regular,
	from,
	to,
	unbounded,
};

static range_kind range_kind_from_name(bz::u8string_view struct_name)
{
	if (struct_name == "__integer_range")
	{
		return range_kind::regular;
	}
	else if (struct_name == "__integer_range_from")
	{
		return range_kind::from;
	}
	else if (struct_name == "__integer_range_to")
	{
		return range_kind::to;
	}
	else if (struct_name == "__range_unbounded")
	{
		return range_kind::unbounded;
	}
	else
	{
		bz_unreachable;
	}
}

static expr_value generate_builtin_subscript_range(
	ast::expression const &lhs,
	ast::expression const &rhs,
	ast::typespec_view result_type,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	auto const lhs_type = lhs.get_expr_type().remove_mut_reference();
	auto const rhs_type = rhs.get_expr_type();
	auto const lhs_value = generate_expression(lhs, context, {});
	auto const rhs_value = generate_expression(rhs, context, {});

	bz_assert(rhs_type.is<ast::ts_base_type>());
	bz_assert(rhs_type.get<ast::ts_base_type>().info->type_name.values.size() == 1);
	auto const kind = range_kind_from_name(rhs_type.get<ast::ts_base_type>().info->type_name.values[0]);

	struct begin_end_pair_t
	{
		expr_value begin;
		expr_value end;
	};

	auto const [begin_index, end_index] = [&]() -> begin_end_pair_t {
		switch (kind)
		{
		case range_kind::regular:
			return {
				.begin = context.create_struct_gep(rhs_value, 0),
				.end   = context.create_struct_gep(rhs_value, 1),
			};
		case range_kind::from:
			return {
				.begin = context.create_struct_gep(rhs_value, 0),
				.end   = context.get_void_value(),
			};
		case range_kind::to:
			return {
				.begin = context.get_void_value(),
				.end   = context.create_struct_gep(rhs_value, 0),
			};
		case range_kind::unbounded:
			return {
				.begin = context.get_void_value(),
				.end   = context.get_void_value(),
			};
		}
	}();

	if (lhs_type.is<ast::ts_array_slice>())
	{
		auto const lhs_begin_ptr = context.create_struct_gep_value(lhs_value, 0);
		auto const lhs_end_ptr   = context.create_struct_gep_value(lhs_value, 1);

		auto const [begin_ptr, end_ptr] = [&]() -> begin_end_pair_t {
			switch (kind)
			{
			case range_kind::regular:
			{
				auto const begin_ptr = context.create_array_slice_gep_pointer(lhs_begin_ptr, begin_index);
				auto const end_ptr   = context.create_array_slice_gep_pointer(lhs_begin_ptr,   end_index);
				return {
					.begin = begin_ptr,
					.end   = end_ptr,
				};
			}
			case range_kind::from:
			{
				auto const begin_ptr = context.create_array_slice_gep_pointer(lhs_begin_ptr, begin_index);
				return {
					.begin = begin_ptr,
					.end   = lhs_end_ptr,
				};
			}
			case range_kind::to:
			{
				auto const end_ptr = context.create_array_slice_gep_pointer(lhs_begin_ptr, end_index);
				return {
					.begin = lhs_begin_ptr,
					.end   = end_ptr,
				};
			}
			case range_kind::unbounded:
				return {
					.begin = lhs_begin_ptr,
					.end   = lhs_end_ptr,
				};
			}
		}();

		if (result_dest.has_value())
		{
			auto const &result_value = result_dest.get();
			auto const slice_literal = context.create_struct_literal(result_value.get_type(), { begin_ptr, end_ptr });
			context.create_assignment(result_value, slice_literal);
			return result_value;
		}
		else
		{
			auto const slice_type = get_type(lhs_type, context);
			return context.create_struct_literal(slice_type, { begin_ptr, end_ptr });
		}
	}
	else if (lhs_type.is<ast::ts_array>())
	{
		auto const size = lhs_type.get<ast::ts_array>().size;

		auto const [begin_ptr, end_ptr] = [&]() -> begin_end_pair_t {
			switch (kind)
			{
			case range_kind::regular:
			{
				auto const begin_ptr = context.create_array_gep_pointer(lhs_value, begin_index);
				auto const end_ptr   = context.create_array_gep_pointer(lhs_value, end_index);
				return {
					.begin = begin_ptr,
					.end   = end_ptr,
				};
			}
			case range_kind::from:
			{
				auto const begin_ptr = context.create_array_gep_pointer(lhs_value, begin_index);
				auto const end_ptr   = context.create_struct_gep_pointer(lhs_value, size);
				return {
					.begin = begin_ptr,
					.end   = end_ptr,
				};
			}
			case range_kind::to:
			{
				auto const begin_ptr = context.create_struct_gep_pointer(lhs_value, 0);
				auto const end_ptr   = context.create_array_gep_pointer(lhs_value, end_index);
				return {
					.begin = begin_ptr,
					.end   = end_ptr,
				};
			}
			case range_kind::unbounded:
			{
				auto const begin_ptr = context.create_struct_gep_pointer(lhs_value, 0);
				auto const end_ptr   = context.create_struct_gep_pointer(lhs_value, size);
				return {
					.begin = begin_ptr,
					.end   = end_ptr,
				};
			}
			}
		}();

		if (result_dest.has_value())
		{
			auto const &result_value = result_dest.get();
			auto const slice_literal = context.to_string_struct_literal(result_value.get_type(), { begin_ptr, end_ptr });
			context.create_assignment(result_value, slice_literal);
			return result_value;
		}
		else
		{
			auto const slice_type = get_type(result_type, context);
			return context.create_struct_literal(slice_type, { begin_ptr, end_ptr });
		}
	}
	else
	{
		bz_unreachable;
	}
}

static expr_value generate_libc_math_function_call(
	bz::u8string_view func_name,
	ast::expr_function_call const &func_call,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	context.add_libc_header("math.h");
	bz_assert(func_call.params.size() == 1 || func_call.params.size() == 2);
	if (func_call.params.size() == 1)
	{
		auto const arg_value = generate_expression(func_call.params[0], context, {});
		return generate_trivial_function_call(func_name, arg_value, arg_value.get_type(), context, result_dest);
	}
	else if (func_call.params.size() == 2)
	{
		auto const arg1_value = generate_expression(func_call.params[0], context, {});
		auto const arg2_value = generate_expression(func_call.params[1], context, {});
		return generate_trivial_function_call(func_name, { arg1_value, arg2_value }, arg1_value.get_type(), context, result_dest);
	}
	else
	{
		bz_unreachable;
	}
}

static bz::optional<expr_value> generate_intrinsic_function_call(
	ast::expr_function_call const &func_call,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	switch (func_call.func_body->intrinsic_kind)
	{
	static_assert(ast::function_body::_builtin_last - ast::function_body::_builtin_first == 263);
	static_assert(ast::function_body::_builtin_default_constructor_last - ast::function_body::_builtin_default_constructor_first == 14);
	static_assert(ast::function_body::_builtin_unary_operator_last - ast::function_body::_builtin_unary_operator_first == 7);
	static_assert(ast::function_body::_builtin_binary_operator_last - ast::function_body::_builtin_binary_operator_first == 28);
	case ast::function_body::builtin_str_length:
		// implemented in __builtins.bz
		bz_unreachable;
	case ast::function_body::builtin_str_starts_with:
		// implemented in __builtins.bz
		bz_unreachable;
	case ast::function_body::builtin_str_ends_with:
		// implemented in __builtins.bz
		bz_unreachable;
	case ast::function_body::builtin_str_begin_ptr:
	{
		bz_assert(func_call.params.size() == 1);
		auto const s = generate_expression(func_call.params[0], context, {});
		auto const result_value = context.create_struct_gep_value(s, 0);
		return value_or_result_dest(result_value, result_dest, context);
	}
	case ast::function_body::builtin_str_end_ptr:
	{
		bz_assert(func_call.params.size() == 1);
		auto const s = generate_expression(func_call.params[0], context, {});
		auto const result_value = context.create_struct_gep_value(s, 1);
		return value_or_result_dest(result_value, result_dest, context);
	}
	case ast::function_body::builtin_str_size:
		// implemented in __builtins.bz
		bz_unreachable;
	case ast::function_body::builtin_str_from_ptrs:
	{
		bz_assert(func_call.params.size() == 2);
		auto const begin_ptr = generate_expression(func_call.params[0], context, {});
		auto const end_ptr   = generate_expression(func_call.params[1], context, {});
		if (result_dest.has_value())
		{
			auto const &result_value = result_dest.get();
			auto const struct_literal = context.to_string_struct_literal(result_value.get_type(), { begin_ptr, end_ptr });
			context.create_assignment(result_value, struct_literal);
			return result_value;
		}
		else
		{
			auto const result_type = get_type(func_call.func_body->return_type, context);
			return context.create_struct_literal(result_type, { begin_ptr, end_ptr });
		}
	}
	case ast::function_body::builtin_slice_begin_ptr:
	case ast::function_body::builtin_slice_begin_mut_ptr:
	{
		bz_assert(func_call.params.size() == 1);
		auto const slice = generate_expression(func_call.params[0], context, {});
		auto const result_value = context.create_struct_gep_value(slice, 0);
		return value_or_result_dest(result_value, result_dest, context);
	}
	case ast::function_body::builtin_slice_end_ptr:
	case ast::function_body::builtin_slice_end_mut_ptr:
	{
		bz_assert(func_call.params.size() == 1);
		auto const slice = generate_expression(func_call.params[0], context, {});
		auto const result_value = context.create_struct_gep_value(slice, 1);
		return value_or_result_dest(result_value, result_dest, context);
	}
	case ast::function_body::builtin_slice_size:
		// implemented in __builtins.bz
		bz_unreachable;
	case ast::function_body::builtin_slice_from_ptrs:
	case ast::function_body::builtin_slice_from_mut_ptrs:
	{
		bz_assert(func_call.params.size() == 2);
		auto const begin_ptr = generate_expression(func_call.params[0], context, {});
		auto const end_ptr   = generate_expression(func_call.params[1], context, {});
		if (result_dest.has_value())
		{
			auto const &result_value = result_dest.get();
			auto const slice_literal = context.to_string_struct_literal(result_value.get_type(), { begin_ptr, end_ptr });
			context.create_assignment(result_value, slice_literal);
			return result_value;
		}
		else
		{
			auto const result_type = get_type(func_call.func_body->return_type, context);
			return context.create_struct_literal(result_type, { begin_ptr, end_ptr });
		}
	}
	case ast::function_body::builtin_array_begin_ptr:
	case ast::function_body::builtin_array_begin_mut_ptr:
	{
		bz_assert(func_call.params.size() == 1);
		auto const array = generate_expression(func_call.params[0], context, {});
		auto const result_value = context.create_struct_gep_pointer(array, 0);
		return value_or_result_dest(result_value, result_dest, context);
	}
	case ast::function_body::builtin_array_end_ptr:
	case ast::function_body::builtin_array_end_mut_ptr:
	{
		bz_assert(func_call.params.size() == 1);
		auto const array = generate_expression(func_call.params[0], context, {});
		bz_assert(context.is_array(array.get_type()));
		auto const size = context.maybe_get_array(array.get_type())->size;
		auto const result_value = context.create_struct_gep_pointer(array, size);
		return value_or_result_dest(result_value, result_dest, context);
	}
	case ast::function_body::builtin_array_size:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::builtin_integer_range_i8:
	case ast::function_body::builtin_integer_range_i16:
	case ast::function_body::builtin_integer_range_i32:
	case ast::function_body::builtin_integer_range_i64:
	case ast::function_body::builtin_integer_range_u8:
	case ast::function_body::builtin_integer_range_u16:
	case ast::function_body::builtin_integer_range_u32:
	case ast::function_body::builtin_integer_range_u64:
	case ast::function_body::builtin_integer_range_inclusive_i8:
	case ast::function_body::builtin_integer_range_inclusive_i16:
	case ast::function_body::builtin_integer_range_inclusive_i32:
	case ast::function_body::builtin_integer_range_inclusive_i64:
	case ast::function_body::builtin_integer_range_inclusive_u8:
	case ast::function_body::builtin_integer_range_inclusive_u16:
	case ast::function_body::builtin_integer_range_inclusive_u32:
	case ast::function_body::builtin_integer_range_inclusive_u64:
	{
		bz_assert(func_call.params.size() == 2);
		auto const begin_value = generate_expression(func_call.params[0], context, {});
		auto const end_value   = generate_expression(func_call.params[1], context, {});

		if (result_dest.has_value())
		{
			auto const &result_value = result_dest.get();
			auto const init_string = context.to_string_struct_literal(result_value.get_type(), { begin_value, end_value });
			context.create_assignment(result_dest.get(), init_string);
			return result_dest.get();
		}
		else
		{
			auto const result_type = get_type(func_call.func_body->return_type, context);
			return context.create_struct_literal(result_type, { begin_value, end_value });
		}
	}
	case ast::function_body::builtin_integer_range_from_i8:
	case ast::function_body::builtin_integer_range_from_i16:
	case ast::function_body::builtin_integer_range_from_i32:
	case ast::function_body::builtin_integer_range_from_i64:
	case ast::function_body::builtin_integer_range_from_u8:
	case ast::function_body::builtin_integer_range_from_u16:
	case ast::function_body::builtin_integer_range_from_u32:
	case ast::function_body::builtin_integer_range_from_u64:
	{
		bz_assert(func_call.params.size() == 1);
		auto const begin_value = generate_expression(func_call.params[0], context, {});

		if (result_dest.has_value())
		{
			auto const &result_value = result_dest.get();
			auto const init_string = context.to_string_struct_literal(result_value.get_type(), begin_value);
			context.create_assignment(result_dest.get(), init_string);
			return result_dest.get();
		}
		else
		{
			auto const result_type = get_type(func_call.func_body->return_type, context);
			return context.create_struct_literal(result_type, begin_value);
		}
	}
	case ast::function_body::builtin_integer_range_to_i8:
	case ast::function_body::builtin_integer_range_to_i16:
	case ast::function_body::builtin_integer_range_to_i32:
	case ast::function_body::builtin_integer_range_to_i64:
	case ast::function_body::builtin_integer_range_to_u8:
	case ast::function_body::builtin_integer_range_to_u16:
	case ast::function_body::builtin_integer_range_to_u32:
	case ast::function_body::builtin_integer_range_to_u64:
	case ast::function_body::builtin_integer_range_to_inclusive_i8:
	case ast::function_body::builtin_integer_range_to_inclusive_i16:
	case ast::function_body::builtin_integer_range_to_inclusive_i32:
	case ast::function_body::builtin_integer_range_to_inclusive_i64:
	case ast::function_body::builtin_integer_range_to_inclusive_u8:
	case ast::function_body::builtin_integer_range_to_inclusive_u16:
	case ast::function_body::builtin_integer_range_to_inclusive_u32:
	case ast::function_body::builtin_integer_range_to_inclusive_u64:
	{
		bz_assert(func_call.params.size() == 1);
		auto const end_value = generate_expression(func_call.params[0], context, {});

		if (result_dest.has_value())
		{
			auto const &result_value = result_dest.get();
			auto const init_string = context.to_string_struct_literal(result_value.get_type(), end_value);
			context.create_assignment(result_dest.get(), init_string);
			return result_dest.get();
		}
		else
		{
			auto const result_type = get_type(func_call.func_body->return_type, context);
			return context.create_struct_literal(result_type, end_value);
		}
	}
	case ast::function_body::builtin_range_unbounded:
	{
		bz_assert(func_call.params.size() == 0);
		if (result_dest.has_value())
		{
			auto const &result_value = result_dest.get();
			auto const type = result_value.get_type();
			auto const init_string = context.to_string_struct_literal(type, {});
			context.create_assignment(result_value, init_string);
			return result_value;
		}
		else
		{
			auto const type = get_type(func_call.func_body->return_type, context);
			return context.create_struct_literal(type, {});
		}
	}
	case ast::function_body::builtin_integer_range_begin_value:
	case ast::function_body::builtin_integer_range_inclusive_begin_value:
	{
		bz_assert(func_call.params.size() == 1);
		auto const range_value = generate_expression(func_call.params[0], context, {});
		auto const begin_value = context.create_struct_gep_value(range_value, 0);
		return value_or_result_dest(begin_value, result_dest, context);
	}
	case ast::function_body::builtin_integer_range_end_value:
	case ast::function_body::builtin_integer_range_inclusive_end_value:
	{
		bz_assert(func_call.params.size() == 1);
		auto const range_value = generate_expression(func_call.params[0], context, {});
		auto const end_value = context.create_struct_gep_value(range_value, 1);
		return value_or_result_dest(end_value, result_dest, context);
	}
	case ast::function_body::builtin_integer_range_from_begin_value:
	{
		bz_assert(func_call.params.size() == 1);
		auto const range_value = generate_expression(func_call.params[0], context, {});
		auto const begin_value = context.create_struct_gep_value(range_value, 0);
		return value_or_result_dest(begin_value, result_dest, context);
	}
	case ast::function_body::builtin_integer_range_to_end_value:
	case ast::function_body::builtin_integer_range_to_inclusive_end_value:
	{
		bz_assert(func_call.params.size() == 1);
		auto const range_value = generate_expression(func_call.params[0], context, {});
		auto const end_value = context.create_struct_gep_value(range_value, 0);
		return value_or_result_dest(end_value, result_dest, context);
	}
	case ast::function_body::builtin_integer_range_begin_iterator:
	{
		bz_assert(func_call.params.size() == 1);
		auto const range_value = generate_expression(func_call.params[0], context, {});
		auto const begin_value = context.create_struct_gep_value(range_value, 0);

		if (result_dest.has_value())
		{
			auto const &result_value = result_dest.get();
			auto const init_string = context.to_string_struct_literal(result_value.get_type(), begin_value);
			context.create_assignment(result_value, init_string);
			return result_value;
		}
		else
		{
			auto const result_type = get_type(func_call.func_body->return_type, context);
			return context.create_struct_literal(result_type, begin_value);
		}
	}
	case ast::function_body::builtin_integer_range_end_iterator:
	{
		bz_assert(func_call.params.size() == 1);
		auto const range_value = generate_expression(func_call.params[0], context, {});
		auto const end_value = context.create_struct_gep_value(range_value, 1);

		if (result_dest.has_value())
		{
			auto const &result_value = result_dest.get();
			auto const init_string = context.to_string_struct_literal(result_value.get_type(), end_value);
			context.create_assignment(result_value, init_string);
			return result_value;
		}
		else
		{
			auto const result_type = get_type(func_call.func_body->return_type, context);
			return context.create_struct_literal(result_type, end_value);
		}
	}
	case ast::function_body::builtin_integer_range_iterator_dereference:
	{
		bz_assert(func_call.params.size() == 1);
		auto const it_value = generate_expression(func_call.params[0], context, {});
		auto const integer_value = context.create_struct_gep_value(it_value, 0);
		return value_or_result_dest(integer_value, result_dest, context);
	}
	case ast::function_body::builtin_integer_range_iterator_equals:
	{
		bz_assert(func_call.params.size() == 2);
		auto const lhs_it_value = generate_expression(func_call.params[0], context, {});
		auto const rhs_it_value = generate_expression(func_call.params[1], context, {});
		auto const lhs_integer_value = context.create_struct_gep_value(lhs_it_value, 0);
		auto const rhs_integer_value = context.create_struct_gep_value(rhs_it_value, 0);
		auto const result_value = context.create_equals(lhs_integer_value, rhs_integer_value);
		return value_or_result_dest(result_value, result_dest, context);
	}
	case ast::function_body::builtin_integer_range_iterator_not_equals:
	{
		bz_assert(func_call.params.size() == 2);
		auto const lhs_it_value = generate_expression(func_call.params[0], context, {});
		auto const rhs_it_value = generate_expression(func_call.params[1], context, {});
		auto const lhs_integer_value = context.create_struct_gep_value(lhs_it_value, 0);
		auto const rhs_integer_value = context.create_struct_gep_value(rhs_it_value, 0);
		auto const result_value = context.create_not_equals(lhs_integer_value, rhs_integer_value);
		return value_or_result_dest(result_value, result_dest, context);
	}
	case ast::function_body::builtin_integer_range_iterator_plus_plus:
	{
		bz_assert(func_call.params.size() == 1);
		auto const it_value = generate_expression(func_call.params[0], context, {});
		auto const integer_value_ref = context.create_struct_gep(it_value, 0);
		context.create_prefix_unary_operation(integer_value_ref, "++");
		return it_value;
	}
	case ast::function_body::builtin_integer_range_iterator_minus_minus:
	{
		bz_assert(func_call.params.size() == 1);
		auto const it_value = generate_expression(func_call.params[0], context, {});
		auto const integer_value_ref = context.create_struct_gep(it_value, 0);
		context.create_prefix_unary_operation(integer_value_ref, "--");
		return it_value;
	}
	case ast::function_body::builtin_integer_range_inclusive_begin_iterator:
	{
		bz_assert(func_call.params.size() == 1);
		auto const range_value = generate_expression(func_call.params[0], context, {});
		auto const begin_value = context.create_struct_gep_value(range_value, 0);
		auto const end_value = context.create_struct_gep_value(range_value, 1);
		auto const false_value = context.get_false_value();

		if (result_dest.has_value())
		{
			auto const &result_value = result_dest.get();
			auto const init_string = context.to_string_struct_literal(result_value.get_type(), { begin_value, end_value, false_value });
			context.create_assignment(result_value, init_string);
			return result_value;
		}
		else
		{
			auto const result_type = get_type(func_call.func_body->return_type, context);
			return context.create_struct_literal(result_type, { begin_value, end_value, false_value });
		}
	}
	case ast::function_body::builtin_integer_range_inclusive_end_iterator:
	{
		bz_assert(func_call.params.size() == 1);
		generate_expression(func_call.params[0], context, {});

		if (result_dest.has_value())
		{
			auto const &result_value = result_dest.get();
			auto const init_string = context.to_string_struct_literal(result_value.get_type(), {});
			context.create_assignment(result_value, init_string);
			return result_value;
		}
		else
		{
			auto const result_type = get_type(func_call.func_body->return_type, context);
			return context.create_struct_literal(result_type, {});
		}
	}
	case ast::function_body::builtin_integer_range_inclusive_iterator_dereference:
	{
		bz_assert(func_call.params.size() == 1);
		auto const it_value = generate_expression(func_call.params[0], context, {});
		auto const integer_value = context.create_struct_gep_value(it_value, 0);
		return value_or_result_dest(integer_value, result_dest, context);
	}
	case ast::function_body::builtin_integer_range_inclusive_iterator_left_equals:
	{
		bz_assert(func_call.params.size() == 2);
		auto const it_value = generate_expression(func_call.params[0], context, {});
		generate_expression(func_call.params[1], context, {});
		auto const at_end = context.create_struct_gep_value(it_value, 2);
		return value_or_result_dest(at_end, result_dest, context);
	}
	case ast::function_body::builtin_integer_range_inclusive_iterator_right_equals:
	{
		bz_assert(func_call.params.size() == 2);
		generate_expression(func_call.params[0], context, {});
		auto const it_value = generate_expression(func_call.params[1], context, {});
		auto const at_end = context.create_struct_gep_value(it_value, 2);
		return value_or_result_dest(at_end, result_dest, context);
	}
	case ast::function_body::builtin_integer_range_inclusive_iterator_left_not_equals:
	{
		bz_assert(func_call.params.size() == 2);
		auto const it_value = generate_expression(func_call.params[0], context, {});
		generate_expression(func_call.params[1], context, {});
		auto const at_end = context.create_struct_gep_value(it_value, 2);
		auto const result = context.create_bool_not(at_end);
		return value_or_result_dest(result, result_dest, context);
	}
	case ast::function_body::builtin_integer_range_inclusive_iterator_right_not_equals:
	{
		bz_assert(func_call.params.size() == 2);
		generate_expression(func_call.params[0], context, {});
		auto const it_value = generate_expression(func_call.params[1], context, {});
		auto const at_end = context.create_struct_gep_value(it_value, 2);
		auto const result = context.create_bool_not(at_end);
		return value_or_result_dest(result, result_dest, context);
	}
	case ast::function_body::builtin_integer_range_inclusive_iterator_plus_plus:
	{
		bz_assert(func_call.params.size() == 1);
		auto const it_value = generate_expression(func_call.params[0], context, {});
		auto const integer_value_ref = context.create_struct_gep(it_value, 0);
		auto const end_value = context.create_struct_gep_value(it_value, 1);

		auto const is_at_end = context.create_equals(integer_value_ref, end_value);
		auto const prev_if_info = context.begin_if(is_at_end);

		context.create_assignment(context.create_struct_gep(it_value, 2), "1");

		context.begin_else();

		context.create_prefix_unary_operation(integer_value_ref, "++");

		context.end_if(prev_if_info);

		return it_value;
	}
	case ast::function_body::builtin_integer_range_from_begin_iterator:
	{
		bz_assert(func_call.params.size() == 1);
		auto const range_value = generate_expression(func_call.params[0], context, {});
		auto const begin_value = context.create_struct_gep_value(range_value, 0);

		if (result_dest.has_value())
		{
			auto const &result_value = result_dest.get();
			auto const init_string = context.to_string_struct_literal(result_value.get_type(), begin_value);
			context.create_assignment(result_value, init_string);
			return result_value;
		}
		else
		{
			auto const result_type = get_type(func_call.func_body->return_type, context);
			return context.create_struct_literal(result_type, begin_value);
		}
	}
	case ast::function_body::builtin_integer_range_from_end_iterator:
	{
		bz_assert(func_call.params.size() == 1);
		generate_expression(func_call.params[0], context, {});

		if (result_dest.has_value())
		{
			auto const &result_value = result_dest.get();
			auto const init_string = context.to_string_struct_literal(result_value.get_type(), {});
			context.create_assignment(result_value, init_string);
			return result_value;
		}
		else
		{
			auto const result_type = get_type(func_call.func_body->return_type, context);
			return context.create_struct_literal(result_type, {});
		}
	}
	case ast::function_body::builtin_integer_range_from_iterator_dereference:
	{
		bz_assert(func_call.params.size() == 1);
		auto const it_value = generate_expression(func_call.params[0], context, {});
		auto const integer_value = context.create_struct_gep_value(it_value, 0);
		return value_or_result_dest(integer_value, result_dest, context);
	}
	case ast::function_body::builtin_integer_range_from_iterator_left_equals:
	case ast::function_body::builtin_integer_range_from_iterator_right_equals:
	{
		bz_assert(func_call.params.size() == 2);
		generate_expression(func_call.params[0], context, {});
		generate_expression(func_call.params[1], context, {});
		return value_or_result_dest("0", context.get_bool(), result_dest, context);
	}
	case ast::function_body::builtin_integer_range_from_iterator_left_not_equals:
	case ast::function_body::builtin_integer_range_from_iterator_right_not_equals:
	{
		bz_assert(func_call.params.size() == 2);
		generate_expression(func_call.params[0], context, {});
		generate_expression(func_call.params[1], context, {});
		return value_or_result_dest("1", context.get_bool(), result_dest, context);
	}
	case ast::function_body::builtin_integer_range_from_iterator_plus_plus:
	{
		bz_assert(func_call.params.size() == 1);
		auto const it_value = generate_expression(func_call.params[0], context, {});
		auto const integer_value_ref = context.create_struct_gep(it_value, 0);
		context.create_prefix_unary_operation(integer_value_ref, "++");
		return it_value;
	}
	case ast::function_body::builtin_optional_get_value_ref:
	case ast::function_body::builtin_optional_get_mut_value_ref:
	{
		bz_assert(func_call.params.size() == 1);
		auto const value = generate_expression(func_call.params[0], context, {});
		generate_optional_get_value_check(func_call.src_tokens, value, context);
		bz_assert(!result_dest.has_value());
		return get_optional_value(value, context);
	}
	case ast::function_body::builtin_optional_get_value:
		// this is handled as a separate expression, not a function call
		bz_unreachable;
	case ast::function_body::builtin_pointer_cast:
	{
		bz_assert(func_call.params.size() == 2);
		bz_assert(func_call.params[0].is_typename());
		auto const pointer_value = generate_expression(func_call.params[1], context, {});
		auto const result_type = get_type(func_call.params[0].get_typename(), context);
		auto const result_value = context.create_cast(pointer_value, result_type);
		return value_or_result_dest(result_value, result_dest, context);
	}
	case ast::function_body::builtin_pointer_to_int:
	{
		bz_assert(func_call.params.size() == 1);
		auto const pointer_value = generate_expression(func_call.params[0], context, {});
		auto const result_value = context.create_cast(pointer_value, context.get_usize());
		return value_or_result_dest(result_value, result_dest, context);
	}
	case ast::function_body::builtin_int_to_pointer:
	{
		bz_assert(func_call.params.size() == 2);
		bz_assert(func_call.params[0].is_typename());
		auto const int_value = generate_expression(func_call.params[1], context, {});
		auto const result_type = get_type(func_call.params[0].get_typename(), context);
		auto const result_value = context.create_cast(int_value, result_type);
		return value_or_result_dest(result_value, result_dest, context);
	}
	case ast::function_body::builtin_enum_value:
		bz_assert(func_call.params.size() == 1);
		return generate_expression(func_call.params[0], context, result_dest);
	case ast::function_body::builtin_destruct_value:
		// this is handled as a separate expression, not a function call
		bz_unreachable;
	case ast::function_body::builtin_inplace_construct:
	{
		bz_assert(func_call.params.size() == 2);
		auto const dest_ptr = generate_expression(func_call.params[0], context, {});
		generate_expression(func_call.params[1], context, context.create_dereference(dest_ptr));
		bz_assert(!result_dest.has_value());
		return context.get_void_value();
	}
	case ast::function_body::builtin_swap:
		// this is handled as a separate expression, not a function call
		bz_unreachable;
	case ast::function_body::builtin_is_comptime:
		return value_or_result_dest("0", context.get_bool(), result_dest, context);
	case ast::function_body::builtin_is_option_set:
		return {};
	case ast::function_body::builtin_panic:
	{
		auto const panic_handler_func_body = context.get_builtin_function(ast::function_body::builtin_panic_handler);
		if (panic_handler_func_body == nullptr)
		{
			context.create_trap();
			return context.get_void_value();
		}

		bz_assert(panic_handler_func_body->params.size() == 1);
		bz_assert(panic_handler_func_body->params[0].get_type().is<ast::ts_base_type>());
		bz_assert(panic_handler_func_body->params[0].get_type().get<ast::ts_base_type>().info->kind == ast::type_info::str_);
		auto const &panic_handler_func = context.get_function(*panic_handler_func_body);

		bz_assert(func_call.params.size() == 1);
		auto const arg = generate_expression(func_call.params[0], context, {});

		generate_trivial_function_call(
			panic_handler_func.name,
			arg,
			context.get_void(),
			context,
			{}
		);
		// just to be sure...
		context.create_trap();
		return context.get_void_value();
	}
	case ast::function_body::builtin_panic_handler:
		// implemented in <target>/__builtins.bz
		bz_unreachable;
	case ast::function_body::builtin_call_main:
		return {};
	case ast::function_body::comptime_malloc:
		return {};
	case ast::function_body::comptime_free:
		return {};
	case ast::function_body::comptime_print:
		return {};
	case ast::function_body::comptime_compile_error:
		return {};
	case ast::function_body::comptime_compile_warning:
		return {};
	case ast::function_body::comptime_add_global_array_data:
		return {};
	case ast::function_body::comptime_create_global_string:
		return {};
	case ast::function_body::comptime_concatenate_strs:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::typename_as_str:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_mut:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_consteval:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_pointer:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_optional:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_reference:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_move_reference:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_slice:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_array:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_tuple:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_enum:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::remove_mut:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::remove_consteval:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::remove_pointer:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::remove_optional:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::remove_reference:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::remove_move_reference:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::slice_value_type:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::array_value_type:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::tuple_value_type:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::concat_tuple_types:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::enum_underlying_type:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_default_constructible:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_copy_constructible:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_trivially_copy_constructible:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_move_constructible:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_trivially_move_constructible:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_trivially_destructible:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_trivially_move_destructible:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_trivially_relocatable:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::is_trivial:
		// this is guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::create_initialized_array:
		// this is handled as a separate expression, not a function call
		bz_unreachable;
	case ast::function_body::trivially_copy_values:
	{
		bz_assert(func_call.params.size() == 3);
		auto const dest = generate_expression(func_call.params[0], context, {});
		auto const source = generate_expression(func_call.params[1], context, {});
		auto const count = generate_expression(func_call.params[2], context, {});

		// the behaviour of memcpy is undefined if either dest or source is invalid, even if size is zero.
		// this is different from the llvm.memcpy.* intrinsic, which is a no-op if size is zero.
		// we follow the semantics of llvm.memcpy.*
		auto const count_is_not_zero = context.create_not_equals(count, context.get_unsigned_zero_value(count.get_type()));
		auto const prev_if_info = context.begin_if(count_is_not_zero);

		bz_assert(context.is_pointer(dest.get_type()));
		auto const [type, _] = context.remove_pointer(dest.get_type());
		auto const type_size = context.create_sizeof(type);
		auto const size = context.create_multiply(count, type_size);

		context.add_libc_header("string.h");
		generate_trivial_function_call("memcpy", { dest, source, size }, context.get_void(), context, {});
		context.end_if(prev_if_info);

		bz_assert(!result_dest.has_value());
		return context.get_void_value();
	}
	case ast::function_body::trivially_copy_overlapping_values:
	{
		bz_assert(func_call.params.size() == 3);
		auto const dest = generate_expression(func_call.params[0], context, {});
		auto const source = generate_expression(func_call.params[1], context, {});
		auto const count = generate_expression(func_call.params[2], context, {});

		// the behaviour of memmove is undefined if either dest or source is invalid, even if size is zero.
		// this is different from the llvm.memmove.* intrinsic, which is a no-op if size is zero.
		// we follow the semantics of llvm.memmove.*
		auto const count_is_not_zero = context.create_not_equals(count, context.get_unsigned_zero_value(count.get_type()));
		auto const prev_if_info = context.begin_if(count_is_not_zero);

		bz_assert(context.is_pointer(dest.get_type()));
		auto const [type, _] = context.remove_pointer(dest.get_type());
		auto const type_size = context.create_sizeof(type);
		auto const size = context.create_multiply(count, type_size);

		context.add_libc_header("string.h");
		generate_trivial_function_call("memmove", { dest, source, size }, context.get_void(), context, {});
		context.end_if(prev_if_info);

		bz_assert(!result_dest.has_value());
		return context.get_void_value();
	}
	case ast::function_body::trivially_relocate_values:
	{
		bz_assert(func_call.params.size() == 3);
		auto const dest = generate_expression(func_call.params[0], context, {});
		auto const source = generate_expression(func_call.params[1], context, {});
		auto const count = generate_expression(func_call.params[2], context, {});

		// the behaviour of memmove is undefined if either dest or source is invalid, even if size is zero.
		// this is different from the llvm.memmove.* intrinsic, which is a no-op if size is zero.
		// we follow the semantics of llvm.memmove.*
		auto const count_is_not_zero = context.create_not_equals(count, context.get_unsigned_zero_value(count.get_type()));
		auto const prev_if_info = context.begin_if(count_is_not_zero);

		bz_assert(context.is_pointer(dest.get_type()));
		auto const [type, _] = context.remove_pointer(dest.get_type());
		auto const type_size = context.create_sizeof(type);
		auto const size = context.create_multiply(count, type_size);

		context.add_libc_header("string.h");
		generate_trivial_function_call("memmove", { dest, source, size }, context.get_void(), context, {});
		context.end_if(prev_if_info);

		bz_assert(!result_dest.has_value());
		return context.get_void_value();
	}
	case ast::function_body::trivially_set_values:
	{
		bz_assert(func_call.params.size() == 3);
		auto const dest = generate_expression(func_call.params[0], context, {});
		auto const value = generate_expression(func_call.params[1], context, {});
		auto const count = generate_expression(func_call.params[2], context, {});
		auto const type = value.get_type();

		if (type == context.get_uint8())
		{
			// the behaviour of memset is undefined if dest is invalid, even if size is zero.
			// this is different from the llvm.memset.* intrinsic, which is a no-op if size is zero.
			// we follow the semantics of llvm.memset.*
			auto const count_is_not_zero = context.create_not_equals(count, context.get_unsigned_zero_value(count.get_type()));
			auto const prev_if_info = context.begin_if(count_is_not_zero);

			context.add_libc_header("string.h");
			generate_trivial_function_call("memset", { dest, value, count }, context.get_void(), context, {});

			context.end_if(prev_if_info);
		}
		else
		{
			// we create a loop using pointers as iterators.
			// in this case, if the dest pointer is null and count is zero, we would apply a 0 offset to a null pointer,
			// which is undefined behaviour. therefore we check for a zero count beforehand.
			auto const count_is_not_zero = context.create_not_equals(count, context.get_unsigned_zero_value(count.get_type()));
			auto const prev_if_info = context.begin_if(count_is_not_zero);

			auto const it = context.create_trivial_copy(dest);
			auto const end = context.create_plus(dest, count, dest.get_type());

			auto const condition = context.create_not_equals(it, end);
			auto const prev_while_info = context.begin_while(condition);

			context.create_assignment(context.create_dereference(it), value);

			context.create_prefix_unary_operation(it, "++");
			context.end_while(prev_while_info);
			context.end_if(prev_if_info);
		}

		bz_assert(!result_dest.has_value());
		return context.get_void_value();
	}
	case ast::function_body::bit_cast:
		// this is handled as a separate expression, not a function call
		bz_unreachable;
	case ast::function_body::trap:
		context.create_trap();
		return context.get_void_value();
	case ast::function_body::memcpy:
	{
		bz_assert(func_call.params.size() == 3);
		auto const dest = generate_expression(func_call.params[0], context, {});
		auto const source = generate_expression(func_call.params[1], context, {});
		auto const size = generate_expression(func_call.params[2], context, {});

		// the behaviour of memcpy is undefined if either dest or source is invalid, even if size is zero.
		// this is different from the llvm.memcpy.* intrinsic, which is a no-op if size is zero.
		// we follow the semantics of llvm.memcpy.*
		auto const size_is_not_zero = context.create_not_equals(size, context.get_unsigned_zero_value(size.get_type()));
		auto const prev_if_info = context.begin_if(size_is_not_zero);

		context.add_libc_header("string.h");
		generate_trivial_function_call("memcpy", { dest, source, size }, context.get_void(), context, {});
		context.end_if(prev_if_info);

		bz_assert(!result_dest.has_value());
		return context.get_void_value();
	}
	case ast::function_body::memmove:
	{
		bz_assert(func_call.params.size() == 3);
		auto const dest = generate_expression(func_call.params[0], context, {});
		auto const source = generate_expression(func_call.params[1], context, {});
		auto const size = generate_expression(func_call.params[2], context, {});

		// the behaviour of memmove is undefined if either dest or source is invalid, even if size is zero.
		// this is different from the llvm.memmove.* intrinsic, which is a no-op if size is zero.
		// we follow the semantics of llvm.memmove.*
		auto const size_is_not_zero = context.create_not_equals(size, context.get_unsigned_zero_value(size.get_type()));
		auto const prev_if_info = context.begin_if(size_is_not_zero);

		context.add_libc_header("string.h");
		generate_trivial_function_call("memmove", { dest, source, size }, context.get_void(), context, {});
		context.end_if(prev_if_info);

		bz_assert(!result_dest.has_value());
		return context.get_void_value();
	}
	case ast::function_body::memset:
	{
		bz_assert(func_call.params.size() == 3);
		auto const dest = generate_expression(func_call.params[0], context, {});
		auto const value = generate_expression(func_call.params[1], context, {});
		auto const size = generate_expression(func_call.params[2], context, {});

		// the behaviour of memset is undefined if dest is invalid, even if size is zero.
		// this is different from the llvm.memset.* intrinsic, which is a no-op if size is zero.
		// we follow the semantics of llvm.memset.*
		auto const size_is_not_zero = context.create_not_equals(size, context.get_unsigned_zero_value(size.get_type()));
		auto const prev_if_info = context.begin_if(size_is_not_zero);

		context.add_libc_header("string.h");
		generate_trivial_function_call("memset", { dest, value, size }, context.get_void(), context, {});
		context.end_if(prev_if_info);

		bz_assert(!result_dest.has_value());
		return context.get_void_value();
	}
	case ast::function_body::abs_i8:
		return generate_builtin_function_call("bozon_abs_i8", func_call, context, result_dest);
	case ast::function_body::abs_i16:
		return generate_builtin_function_call("bozon_abs_i16", func_call, context, result_dest);
	case ast::function_body::abs_i32:
		return generate_builtin_function_call("bozon_abs_i32", func_call, context, result_dest);
	case ast::function_body::abs_i64:
		return generate_builtin_function_call("bozon_abs_i64", func_call, context, result_dest);
	case ast::function_body::abs_f32:
		return generate_libc_math_function_call("fabsf", func_call, context, result_dest);
	case ast::function_body::abs_f64:
		return generate_libc_math_function_call("fabs", func_call, context, result_dest);
	case ast::function_body::min_i8:
	case ast::function_body::min_i16:
	case ast::function_body::min_i32:
	case ast::function_body::min_i64:
	case ast::function_body::min_u8:
	case ast::function_body::min_u16:
	case ast::function_body::min_u32:
	case ast::function_body::min_u64:
	{
		bz_assert(func_call.params.size() == 2);
		auto const a = generate_expression(func_call.params[0], context, {});
		auto const b = generate_expression(func_call.params[1], context, {});
		if (!result_dest.has_value())
		{
			result_dest = context.add_uninitialized_value(a.get_type());
		}
		auto const &result_value = result_dest.get();

		auto const prev_if_info = context.begin_if(context.create_relational(a, b, "<"));
		context.create_assignment(result_value, a);
		context.begin_else();
		context.create_assignment(result_value, b);
		context.end_if(prev_if_info);
		return result_value;
	}
	case ast::function_body::fmin_f32:
		return generate_libc_math_function_call("fminf", func_call, context, result_dest);
	case ast::function_body::fmin_f64:
		return generate_libc_math_function_call("fmin", func_call, context, result_dest);
	case ast::function_body::max_i8:
	case ast::function_body::max_i16:
	case ast::function_body::max_i32:
	case ast::function_body::max_i64:
	case ast::function_body::max_u8:
	case ast::function_body::max_u16:
	case ast::function_body::max_u32:
	case ast::function_body::max_u64:
	{
		bz_assert(func_call.params.size() == 2);
		auto const a = generate_expression(func_call.params[0], context, {});
		auto const b = generate_expression(func_call.params[1], context, {});
		if (!result_dest.has_value())
		{
			result_dest = context.add_uninitialized_value(a.get_type());
		}
		auto const &result_value = result_dest.get();

		auto const prev_if_info = context.begin_if(context.create_relational(a, b, ">"));
		context.create_assignment(result_value, a);
		context.begin_else();
		context.create_assignment(result_value, b);
		context.end_if(prev_if_info);
		return result_value;
	}
	case ast::function_body::fmax_f32:
		return generate_libc_math_function_call("fmaxf", func_call, context, result_dest);
	case ast::function_body::fmax_f64:
		return generate_libc_math_function_call("fmax", func_call, context, result_dest);
	case ast::function_body::exp_f32:
		return generate_libc_math_function_call("expf", func_call, context, result_dest);
	case ast::function_body::exp_f64:
		return generate_libc_math_function_call("exp", func_call, context, result_dest);
	case ast::function_body::exp2_f32:
		return generate_libc_math_function_call("exp2f", func_call, context, result_dest);
	case ast::function_body::exp2_f64:
		return generate_libc_math_function_call("exp2", func_call, context, result_dest);
	case ast::function_body::expm1_f32:
		return generate_libc_math_function_call("expm1f", func_call, context, result_dest);
	case ast::function_body::expm1_f64:
		return generate_libc_math_function_call("expm1", func_call, context, result_dest);
	case ast::function_body::log_f32:
		return generate_libc_math_function_call("logf", func_call, context, result_dest);
	case ast::function_body::log_f64:
		return generate_libc_math_function_call("log", func_call, context, result_dest);
	case ast::function_body::log10_f32:
		return generate_libc_math_function_call("log10f", func_call, context, result_dest);
	case ast::function_body::log10_f64:
		return generate_libc_math_function_call("log10", func_call, context, result_dest);
	case ast::function_body::log2_f32:
		return generate_libc_math_function_call("log2f", func_call, context, result_dest);
	case ast::function_body::log2_f64:
		return generate_libc_math_function_call("log2", func_call, context, result_dest);
	case ast::function_body::log1p_f32:
		return generate_libc_math_function_call("log1pf", func_call, context, result_dest);
	case ast::function_body::log1p_f64:
		return generate_libc_math_function_call("log1p", func_call, context, result_dest);
	case ast::function_body::sqrt_f32:
		return generate_libc_math_function_call("sqrtf", func_call, context, result_dest);
	case ast::function_body::sqrt_f64:
		return generate_libc_math_function_call("sqrt", func_call, context, result_dest);
	case ast::function_body::pow_f32:
		return generate_libc_math_function_call("powf", func_call, context, result_dest);
	case ast::function_body::pow_f64:
		return generate_libc_math_function_call("pow", func_call, context, result_dest);
	case ast::function_body::cbrt_f32:
		return generate_libc_math_function_call("cbrtf", func_call, context, result_dest);
	case ast::function_body::cbrt_f64:
		return generate_libc_math_function_call("cbrt", func_call, context, result_dest);
	case ast::function_body::hypot_f32:
		return generate_libc_math_function_call("hypotf", func_call, context, result_dest);
	case ast::function_body::hypot_f64:
		return generate_libc_math_function_call("hypot", func_call, context, result_dest);
	case ast::function_body::sin_f32:
		return generate_libc_math_function_call("sinf", func_call, context, result_dest);
	case ast::function_body::sin_f64:
		return generate_libc_math_function_call("sin", func_call, context, result_dest);
	case ast::function_body::cos_f32:
		return generate_libc_math_function_call("cosf", func_call, context, result_dest);
	case ast::function_body::cos_f64:
		return generate_libc_math_function_call("cos", func_call, context, result_dest);
	case ast::function_body::tan_f32:
		return generate_libc_math_function_call("tanf", func_call, context, result_dest);
	case ast::function_body::tan_f64:
		return generate_libc_math_function_call("tan", func_call, context, result_dest);
	case ast::function_body::asin_f32:
		return generate_libc_math_function_call("asinf", func_call, context, result_dest);
	case ast::function_body::asin_f64:
		return generate_libc_math_function_call("asin", func_call, context, result_dest);
	case ast::function_body::acos_f32:
		return generate_libc_math_function_call("acosf", func_call, context, result_dest);
	case ast::function_body::acos_f64:
		return generate_libc_math_function_call("acos", func_call, context, result_dest);
	case ast::function_body::atan_f32:
		return generate_libc_math_function_call("atanf", func_call, context, result_dest);
	case ast::function_body::atan_f64:
		return generate_libc_math_function_call("atan", func_call, context, result_dest);
	case ast::function_body::atan2_f32:
		return generate_libc_math_function_call("atan2f", func_call, context, result_dest);
	case ast::function_body::atan2_f64:
		return generate_libc_math_function_call("atan2", func_call, context, result_dest);
	case ast::function_body::sinh_f32:
		return generate_libc_math_function_call("sinhf", func_call, context, result_dest);
	case ast::function_body::sinh_f64:
		return generate_libc_math_function_call("sinh", func_call, context, result_dest);
	case ast::function_body::cosh_f32:
		return generate_libc_math_function_call("coshf", func_call, context, result_dest);
	case ast::function_body::cosh_f64:
		return generate_libc_math_function_call("cosh", func_call, context, result_dest);
	case ast::function_body::tanh_f32:
		return generate_libc_math_function_call("tanhf", func_call, context, result_dest);
	case ast::function_body::tanh_f64:
		return generate_libc_math_function_call("tanh", func_call, context, result_dest);
	case ast::function_body::asinh_f32:
		return generate_libc_math_function_call("asinhf", func_call, context, result_dest);
	case ast::function_body::asinh_f64:
		return generate_libc_math_function_call("asinh", func_call, context, result_dest);
	case ast::function_body::acosh_f32:
		return generate_libc_math_function_call("acoshf", func_call, context, result_dest);
	case ast::function_body::acosh_f64:
		return generate_libc_math_function_call("acosh", func_call, context, result_dest);
	case ast::function_body::atanh_f32:
		return generate_libc_math_function_call("atanhf", func_call, context, result_dest);
	case ast::function_body::atanh_f64:
		return generate_libc_math_function_call("atanh", func_call, context, result_dest);
	case ast::function_body::erf_f32:
		return generate_libc_math_function_call("erff", func_call, context, result_dest);
	case ast::function_body::erf_f64:
		return generate_libc_math_function_call("erf", func_call, context, result_dest);
	case ast::function_body::erfc_f32:
		return generate_libc_math_function_call("erfcf", func_call, context, result_dest);
	case ast::function_body::erfc_f64:
		return generate_libc_math_function_call("erfc", func_call, context, result_dest);
	case ast::function_body::tgamma_f32:
		return generate_libc_math_function_call("tgammaf", func_call, context, result_dest);
	case ast::function_body::tgamma_f64:
		return generate_libc_math_function_call("tgamma", func_call, context, result_dest);
	case ast::function_body::lgamma_f32:
		return generate_libc_math_function_call("lgammaf", func_call, context, result_dest);
	case ast::function_body::lgamma_f64:
		return generate_libc_math_function_call("lgamma", func_call, context, result_dest);
	case ast::function_body::bitreverse_u8:
		return generate_builtin_function_call("bozon_bitreverse_u8", func_call, context, result_dest);
	case ast::function_body::bitreverse_u16:
		return generate_builtin_function_call("bozon_bitreverse_u16", func_call, context, result_dest);
	case ast::function_body::bitreverse_u32:
		return generate_builtin_function_call("bozon_bitreverse_u32", func_call, context, result_dest);
	case ast::function_body::bitreverse_u64:
		return generate_builtin_function_call("bozon_bitreverse_u64", func_call, context, result_dest);
	case ast::function_body::popcount_u8:
		return generate_builtin_function_call("bozon_popcount_u8", func_call, context, result_dest);
	case ast::function_body::popcount_u16:
		return generate_builtin_function_call("bozon_popcount_u16", func_call, context, result_dest);
	case ast::function_body::popcount_u32:
		return generate_builtin_function_call("bozon_popcount_u32", func_call, context, result_dest);
	case ast::function_body::popcount_u64:
		return generate_builtin_function_call("bozon_popcount_u64", func_call, context, result_dest);
	case ast::function_body::byteswap_u16:
		return generate_builtin_function_call("bozon_byteswap_u16", func_call, context, result_dest);
	case ast::function_body::byteswap_u32:
		return generate_builtin_function_call("bozon_byteswap_u32", func_call, context, result_dest);
	case ast::function_body::byteswap_u64:
		return generate_builtin_function_call("bozon_byteswap_u64", func_call, context, result_dest);
	case ast::function_body::clz_u8:
		return generate_builtin_function_call("bozon_clz_u8", func_call, context, result_dest);
	case ast::function_body::clz_u16:
		return generate_builtin_function_call("bozon_clz_u16", func_call, context, result_dest);
	case ast::function_body::clz_u32:
		return generate_builtin_function_call("bozon_clz_u32", func_call, context, result_dest);
	case ast::function_body::clz_u64:
		return generate_builtin_function_call("bozon_clz_u64", func_call, context, result_dest);
	case ast::function_body::ctz_u8:
		return generate_builtin_function_call("bozon_ctz_u8", func_call, context, result_dest);
	case ast::function_body::ctz_u16:
		return generate_builtin_function_call("bozon_ctz_u16", func_call, context, result_dest);
	case ast::function_body::ctz_u32:
		return generate_builtin_function_call("bozon_ctz_u32", func_call, context, result_dest);
	case ast::function_body::ctz_u64:
		return generate_builtin_function_call("bozon_ctz_u64", func_call, context, result_dest);
	case ast::function_body::fshl_u8:
		return generate_builtin_function_call("bozon_fshl_u8", func_call, context, result_dest);
	case ast::function_body::fshl_u16:
		return generate_builtin_function_call("bozon_fshl_u16", func_call, context, result_dest);
	case ast::function_body::fshl_u32:
		return generate_builtin_function_call("bozon_fshl_u32", func_call, context, result_dest);
	case ast::function_body::fshl_u64:
		return generate_builtin_function_call("bozon_fshl_u64", func_call, context, result_dest);
	case ast::function_body::fshr_u8:
		return generate_builtin_function_call("bozon_fshr_u8", func_call, context, result_dest);
	case ast::function_body::fshr_u16:
		return generate_builtin_function_call("bozon_fshr_u16", func_call, context, result_dest);
	case ast::function_body::fshr_u32:
		return generate_builtin_function_call("bozon_fshr_u32", func_call, context, result_dest);
	case ast::function_body::fshr_u64:
		return generate_builtin_function_call("bozon_fshr_u64", func_call, context, result_dest);
	case ast::function_body::i8_default_constructor:
	case ast::function_body::i16_default_constructor:
	case ast::function_body::i32_default_constructor:
	case ast::function_body::i64_default_constructor:
	case ast::function_body::u8_default_constructor:
	case ast::function_body::u16_default_constructor:
	case ast::function_body::u32_default_constructor:
	case ast::function_body::u64_default_constructor:
	case ast::function_body::f32_default_constructor:
	case ast::function_body::f64_default_constructor:
	case ast::function_body::char_default_constructor:
	case ast::function_body::str_default_constructor:
	case ast::function_body::bool_default_constructor:
	case ast::function_body::null_t_default_constructor:
		// these are guaranteed to be constant evaluated
		bz_unreachable;
	case ast::function_body::builtin_unary_plus:
		bz_assert(func_call.params.size() == 1);
		return generate_builtin_unary_plus(func_call.params[0], context, result_dest);
	case ast::function_body::builtin_unary_minus:
		bz_assert(func_call.params.size() == 1);
		return generate_builtin_unary_minus(func_call.params[0], context, result_dest);
	case ast::function_body::builtin_unary_dereference:
		bz_assert(func_call.params.size() == 1);
		bz_assert(!result_dest.has_value());
		return generate_builtin_unary_dereference(func_call.src_tokens, func_call.params[0], context);
	case ast::function_body::builtin_unary_bit_not:
		bz_assert(func_call.params.size() == 1);
		return generate_builtin_unary_bit_not(func_call.params[0], context, result_dest);
	case ast::function_body::builtin_unary_bool_not:
		bz_assert(func_call.params.size() == 1);
		return generate_builtin_unary_bool_not(func_call.params[0], context, result_dest);
	case ast::function_body::builtin_unary_plus_plus:
		bz_assert(func_call.params.size() == 1);
		bz_assert(!result_dest.has_value());
		return generate_builtin_unary_plus_plus(func_call.params[0], context);
	case ast::function_body::builtin_unary_minus_minus:
		bz_assert(func_call.params.size() == 1);
		bz_assert(!result_dest.has_value());
		return generate_builtin_unary_minus_minus(func_call.params[0], context);
	case ast::function_body::builtin_binary_assign:
		// assignment is handled as a separate expression
		bz_unreachable;
	case ast::function_body::builtin_binary_plus:
		bz_assert(func_call.params.size() == 2);
		return generate_builtin_binary_plus(func_call.params[0], func_call.params[1], context, result_dest);
	case ast::function_body::builtin_binary_plus_eq:
		bz_assert(func_call.params.size() == 2);
		bz_assert(!result_dest.has_value());
		return generate_builtin_binary_plus_eq(func_call.params[0], func_call.params[1], context);
	case ast::function_body::builtin_binary_minus:
		bz_assert(func_call.params.size() == 2);
		return generate_builtin_binary_minus(func_call.params[0], func_call.params[1], context, result_dest);
	case ast::function_body::builtin_binary_minus_eq:
		bz_assert(func_call.params.size() == 2);
		bz_assert(!result_dest.has_value());
		return generate_builtin_binary_minus_eq(func_call.params[0], func_call.params[1], context);
	case ast::function_body::builtin_binary_multiply:
		bz_assert(func_call.params.size() == 2);
		return generate_builtin_binary_multiply(func_call.params[0], func_call.params[1], context, result_dest);
	case ast::function_body::builtin_binary_multiply_eq:
		bz_assert(func_call.params.size() == 2);
		bz_assert(!result_dest.has_value());
		return generate_builtin_binary_multiply_eq(func_call.params[0], func_call.params[1], context);
	case ast::function_body::builtin_binary_divide:
		bz_assert(func_call.params.size() == 2);
		return generate_builtin_binary_divide(func_call.src_tokens, func_call.params[0], func_call.params[1], context, result_dest);
	case ast::function_body::builtin_binary_divide_eq:
		bz_assert(func_call.params.size() == 2);
		bz_assert(!result_dest.has_value());
		return generate_builtin_binary_divide_eq(func_call.src_tokens, func_call.params[0], func_call.params[1], context);
	case ast::function_body::builtin_binary_modulo:
		bz_assert(func_call.params.size() == 2);
		return generate_builtin_binary_modulo(func_call.src_tokens, func_call.params[0], func_call.params[1], context, result_dest);
	case ast::function_body::builtin_binary_modulo_eq:
		bz_assert(func_call.params.size() == 2);
		bz_assert(!result_dest.has_value());
		return generate_builtin_binary_modulo_eq(func_call.src_tokens, func_call.params[0], func_call.params[1], context);
	case ast::function_body::builtin_binary_equals:
		bz_assert(func_call.params.size() == 2);
		return generate_builtin_binary_equality(
			func_call.params[0],
			func_call.params[1],
			"==",
			context,
			result_dest
		);
	case ast::function_body::builtin_binary_not_equals:
		bz_assert(func_call.params.size() == 2);
		return generate_builtin_binary_equality(
			func_call.params[0],
			func_call.params[1],
			"!=",
			context,
			result_dest
		);
	case ast::function_body::builtin_binary_less_than:
		bz_assert(func_call.params.size() == 2);
		return generate_builtin_binary_compare(
			func_call.params[0],
			func_call.params[1],
			"<",
			context,
			result_dest
		);
	case ast::function_body::builtin_binary_less_than_eq:
		bz_assert(func_call.params.size() == 2);
		return generate_builtin_binary_compare(
			func_call.params[0],
			func_call.params[1],
			"<=",
			context,
			result_dest
		);
	case ast::function_body::builtin_binary_greater_than:
		bz_assert(func_call.params.size() == 2);
		return generate_builtin_binary_compare(
			func_call.params[0],
			func_call.params[1],
			">",
			context,
			result_dest
		);
	case ast::function_body::builtin_binary_greater_than_eq:
		bz_assert(func_call.params.size() == 2);
		return generate_builtin_binary_compare(
			func_call.params[0],
			func_call.params[1],
			">=",
			context,
			result_dest
		);
	case ast::function_body::builtin_binary_bit_and:
		bz_assert(func_call.params.size() == 2);
		return generate_builtin_binary_bit_op(
			func_call.params[0],
			func_call.params[1],
			"&",
			precedence::bitwise_and,
			context,
			result_dest
		);
	case ast::function_body::builtin_binary_bit_and_eq:
		bz_assert(func_call.params.size() == 2);
		bz_assert(!result_dest.has_value());
		return generate_builtin_binary_bit_eq_op(
			func_call.params[0],
			func_call.params[1],
			"&=",
			context
		);
	case ast::function_body::builtin_binary_bit_xor:
		bz_assert(func_call.params.size() == 2);
		return generate_builtin_binary_bit_op(
			func_call.params[0],
			func_call.params[1],
			"^",
			precedence::bitwise_xor,
			context,
			result_dest
		);
	case ast::function_body::builtin_binary_bit_xor_eq:
		bz_assert(func_call.params.size() == 2);
		bz_assert(!result_dest.has_value());
		return generate_builtin_binary_bit_eq_op(
			func_call.params[0],
			func_call.params[1],
			"^=",
			context
		);
	case ast::function_body::builtin_binary_bit_or:
		bz_assert(func_call.params.size() == 2);
		return generate_builtin_binary_bit_op(
			func_call.params[0],
			func_call.params[1],
			"|",
			precedence::bitwise_or,
			context,
			result_dest
		);
	case ast::function_body::builtin_binary_bit_or_eq:
		bz_assert(func_call.params.size() == 2);
		bz_assert(!result_dest.has_value());
		return generate_builtin_binary_bit_eq_op(
			func_call.params[0],
			func_call.params[1],
			"|=",
			context
		);
	case ast::function_body::builtin_binary_bit_left_shift:
		bz_assert(func_call.params.size() == 2);
		return generate_builtin_binary_bitshift(
			func_call.params[0],
			func_call.params[1],
			"<<",
			context,
			result_dest
		);
	case ast::function_body::builtin_binary_bit_left_shift_eq:
		bz_assert(func_call.params.size() == 2);
		bz_assert(!result_dest.has_value());
		return generate_builtin_binary_bitshift_eq(
			func_call.params[0],
			func_call.params[1],
			"<<=",
			context
		);
	case ast::function_body::builtin_binary_bit_right_shift:
		bz_assert(func_call.params.size() == 2);
		return generate_builtin_binary_bitshift(
			func_call.params[0],
			func_call.params[1],
			">>",
			context,
			result_dest
		);
	case ast::function_body::builtin_binary_bit_right_shift_eq:
		bz_assert(func_call.params.size() == 2);
		bz_assert(!result_dest.has_value());
		return generate_builtin_binary_bitshift_eq(
			func_call.params[0],
			func_call.params[1],
			">>=",
			context
		);
	case ast::function_body::builtin_binary_subscript:
		bz_assert(func_call.params.size() == 2);
		return generate_builtin_subscript_range(
			func_call.params[0],
			func_call.params[1],
			func_call.func_body->return_type,
			context,
			result_dest
		);
	default:
		bz_unreachable;
	}
}

static expr_value generate_trivial_function_call(
	bz::u8string_view func_string,
	bz::array_view<expr_value const> args,
	type return_type,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	bz::u8string call_string = func_string;
	call_string += '(';

	bool first = true;

	for (auto const &arg : args)
	{
		if (first)
		{
			first = false;
		}
		else
		{
			call_string += ", ";
		}

		call_string += context.to_string_arg(arg);
	}

	call_string += ')';

	if (return_type == context.get_void())
	{
		context.add_expression(call_string);
		bz_assert(!result_dest.has_value());
		return context.get_void_value();
	}
	else
	{
		return value_or_result_dest(call_string, return_type, result_dest, context);
	}
}

static expr_value generate_function_call(
	bz::u8string_view func_string,
	bz::array_view<expr_value const> args,
	ast::typespec_view return_type,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	auto const needs_result_address = !return_type.is<ast::ts_void>()
		&& !return_type.is_any_reference()
		&& !ast::is_trivially_relocatable(return_type);

	if (needs_result_address && !result_dest.has_value())
	{
		result_dest = context.add_uninitialized_value(get_type(return_type, context));
	}

	bz::u8string call_string = func_string;
	call_string += '(';

	if (needs_result_address)
	{
		call_string += context.to_string_arg(context.create_address_of(result_dest.get()));
	}
	bool first = !needs_result_address;

	for (auto const &arg : args)
	{
		if (first)
		{
			first = false;
		}
		else
		{
			call_string += ", ";
		}

		call_string += context.to_string_arg(arg);
	}

	call_string += ')';

	if (needs_result_address)
	{
		context.add_expression(call_string);
		return result_dest.get();
	}
	else if (return_type.is<ast::ts_void>())
	{
		context.add_expression(call_string);
		return context.get_void_value();
	}
	else if (return_type.is_any_reference())
	{
		bz_assert(!result_dest.has_value());
		auto const pointer_value = context.add_value_expression(call_string, get_type(return_type, context));
		return context.create_dereference(pointer_value);
	}
	else
	{
		return value_or_result_dest(call_string, get_type(return_type, context), result_dest, context);
	}
}

static expr_value generate_expression(
	ast::expr_function_call const &func_call,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	if (func_call.func_body->is_intrinsic() && func_call.func_body->body.is_null())
	{
		auto const maybe_result = generate_intrinsic_function_call(func_call, context, result_dest);
		if (maybe_result.has_value())
		{
			return maybe_result.get();
		}
	}
	else if (func_call.func_body->is_libc_macro())
	{
		auto const &return_type = func_call.func_body->return_type;
		auto const macro_name = context.get_libc_macro_name(*func_call.func_body);
		if (return_type.is_any_reference())
		{
			// TODO: use temporary
			auto const value_string = bz::format("&{}", macro_name);
			auto const pointer_value = context.add_value_expression(value_string, get_type(return_type, context));
			return context.create_dereference(pointer_value);
		}
		else if (result_dest.has_value())
		{
			context.create_assignment(result_dest.get(), macro_name);
			return result_dest.get();
		}
		else
		{
			return context.add_value_expression(macro_name, get_type(return_type, context));
		}
	}

	if (func_call.func_body->is_only_consteval())
	{
		context.create_trap();
		if (result_dest.has_value())
		{
			return result_dest.get();
		}
		else if (func_call.func_body->return_type.is_any_reference())
		{
			auto const pointer_type = get_type(func_call.func_body->return_type, context);
			auto const pointer_value = context.add_uninitialized_value(pointer_type);
			return context.create_dereference(pointer_value);
		}
		else if (func_call.func_body->return_type.is<ast::ts_void>())
		{
			return context.get_void_value();
		}
		else
		{
			auto const result_type = get_type(func_call.func_body->return_type, context);
			return context.add_uninitialized_value(result_type);
		}
	}

	bz::vector<expr_value> arg_values;
	arg_values.reserve(func_call.params.size());

	if (func_call.param_resolve_order == ast::resolve_order::regular)
	{
		for (auto const arg_index : bz::iota(0, func_call.params.size()))
		{
			if (ast::is_generic_parameter(func_call.func_body->params[arg_index]))
			{
				continue;
			}

			auto const &param_type = func_call.func_body->params[arg_index].get_type();
			auto const arg_value = generate_expression(func_call.params[arg_index], context, {});
			if (param_type.is_any_reference())
			{
				arg_values.push_back(context.create_address_of(arg_value));
			}
			else if (ast::is_trivially_relocatable(param_type))
			{
				arg_values.push_back(arg_value);
			}
			else
			{
				arg_values.push_back(context.create_address_of(arg_value));
			}
		}
	}
	else
	{
		for (auto const arg_index : bz::iota(0, func_call.params.size()).reversed())
		{
			if (ast::is_generic_parameter(func_call.func_body->params[arg_index]))
			{
				continue;
			}

			auto const &param_type = func_call.func_body->params[arg_index].get_type();
			auto const arg_value = generate_expression(func_call.params[arg_index], context, {});
			if (param_type.is_any_reference())
			{
				arg_values.push_front(context.create_address_of(arg_value));
			}
			else if (ast::is_trivially_relocatable(param_type))
			{
				arg_values.push_front(arg_value);
			}
			else
			{
				arg_values.push_front(context.create_address_of(arg_value));
			}
		}
	}

	if (func_call.func_body->is_intrinsic() && func_call.func_body->intrinsic_kind == ast::function_body::builtin_call_main)
	{
		auto const &main_func = context.get_main_func_body();
		bz_assert(arg_values.size() == func_call.params.size());
		bz_assert(main_func.params.size() <= arg_values.size());
		arg_values.resize(main_func.params.size());
	}

	auto const &func = context.get_function(*func_call.func_body);
	return generate_function_call(
		func.name,
		arg_values,
		func_call.func_body->return_type,
		context,
		result_dest
	);
}

static expr_value generate_expression(
	ast::expr_indirect_function_call const &indirect_function_call,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	auto const called_type = indirect_function_call.called.get_expr_type().remove_any_mut();
	bz_assert(called_type.is<ast::ts_function>() || called_type.is_optional_function());
	auto const &fn_type = called_type.is<ast::ts_function>()
		? called_type.get<ast::ts_function>()
		: called_type.get_optional_function();
	auto const func_value = generate_expression(indirect_function_call.called, context, {});
	auto const func_string = context.to_string_unary(func_value, precedence::suffix);

	bz::vector<expr_value> arg_values;
	arg_values.reserve(indirect_function_call.params.size());

	for (auto const arg_index : bz::iota(0, indirect_function_call.params.size()))
	{
		auto const &param_type = fn_type.param_types[arg_index];
		auto const arg_value = generate_expression(indirect_function_call.params[arg_index], context, {});
		if (param_type.is_any_reference())
		{
			arg_values.push_back(context.create_address_of(arg_value));
		}
		else if (ast::is_trivially_relocatable(param_type))
		{
			arg_values.push_back(arg_value);
		}
		else
		{
			arg_values.push_back(context.create_address_of(arg_value));
		}
	}

	return generate_function_call(
		func_string,
		arg_values,
		fn_type.return_type,
		context,
		result_dest
	);
}

static expr_value generate_expression(
	ast::expr_cast const &cast,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	auto const expr_t = cast.expr.get_expr_type().remove_mut_reference();
	auto const dest_t = cast.type.remove_any_mut();

	if (expr_t.is<ast::ts_array>() && dest_t.is<ast::ts_array_slice>())
	{
		auto const expr_value = generate_expression(cast.expr, context, {});
		bz_assert(context.is_array(expr_value.get_type()));
		auto const array_size = context.maybe_get_array(expr_value.get_type())->size;
		auto const begin_ptr = context.create_struct_gep_pointer(expr_value, 0);
		auto const end_ptr   = context.create_struct_gep_pointer(expr_value, array_size);

		if (result_dest.has_value())
		{
			auto const &result_value = result_dest.get();
			auto const slice_literal = context.to_string_struct_literal(result_value.get_type(), { begin_ptr, end_ptr });
			context.create_assignment(result_value, slice_literal);
			return result_value;
		}
		else
		{
			auto const result_type = get_type(dest_t, context);
			return context.create_struct_literal(result_type, { begin_ptr, end_ptr });
		}
	}
	else
	{
		auto const expr_value = generate_expression(cast.expr, context, {});
		auto const result_type = get_type(dest_t, context);
		auto const result_value = context.create_cast(expr_value, result_type);
		return value_or_result_dest(result_value, result_dest, context);
	}
}

static expr_value generate_expression(
	ast::expr_bit_cast const &bit_cast,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	if (!result_dest.has_value())
	{
		result_dest = context.add_uninitialized_value(get_type(bit_cast.type, context));
	}

	auto const &result_value = result_dest.get();
	auto const value = generate_expression(bit_cast.expr, context, {});

	auto const dest = context.create_address_of(result_value);
	auto const src = context.create_address_of(value);
	auto const size = context.create_sizeof(result_value.get_type());

	context.add_libc_header("string.h");
	generate_trivial_function_call("memcpy", { dest, src, size }, context.get_void(), context, {});

	return result_value;
}

static expr_value generate_expression(
	ast::expr_optional_cast const &optional_cast,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	if (optional_cast.type.is_optional_reference())
	{
		return context.create_address_of(generate_expression(optional_cast.expr, context, result_dest));
	}
	else if (optional_cast.type.is_optional_pointer_like())
	{
		return generate_expression(optional_cast.expr, context, result_dest);
	}
	else
	{
		if (!result_dest.has_value())
		{
			result_dest = context.add_uninitialized_value(get_type(optional_cast.type, context));
		}
		auto const &result_value = result_dest.get();

		auto const value_result_dest = get_optional_value(result_value, context);
		generate_expression(optional_cast.expr, context, value_result_dest);
		set_optional_has_value(result_value, true, context);

		return result_value;
	}
}

static expr_value generate_expression(
	ast::expr_take_reference const &take_reference,
	codegen_context &context
)
{
	return generate_expression(take_reference.expr, context, {});
}

static expr_value generate_expression(
	ast::expr_take_move_reference const &take_move_reference,
	codegen_context &context
)
{
	return generate_expression(take_move_reference.expr, context, {});
}

static expr_value generate_expression(
	ast::expr_aggregate_init const &aggregate_init,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	if (!result_dest.has_value())
	{
		result_dest = context.add_uninitialized_value(get_type(aggregate_init.type, context));
	}

	auto const &result_value = result_dest.get();
	for (auto const i : bz::iota(0, aggregate_init.exprs.size()))
	{
		auto const member_ptr = context.create_struct_gep(result_value, i);
		if (aggregate_init.exprs[i].get_expr_type().is_reference())
		{
			auto const ref = generate_expression(aggregate_init.exprs[i], context, {});
			context.create_assignment(member_ptr, context.create_address_of(ref));
		}
		else
		{
			generate_expression(aggregate_init.exprs[i], context, member_ptr);
		}
	}
	return result_value;
}

static expr_value generate_expression(
	ast::expr_array_value_init const &array_value_init,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	bz_assert(array_value_init.type.is<ast::ts_array>());
	auto const size = array_value_init.type.get<ast::ts_array>().size;

	if (!result_dest.has_value())
	{
		result_dest = context.add_uninitialized_value(get_type(array_value_init.type, context));
	}
	auto const &result_value = result_dest.get();

	auto const value = generate_expression(array_value_init.value, context, {});
	auto const prev_value = context.push_value_reference(value);

	auto const loop_info = create_loop_start(size, context);

	auto const elem_result_dest = context.create_array_gep(result_value, loop_info.index);
	generate_expression(array_value_init.copy_expr, context, elem_result_dest);

	create_loop_end(loop_info, context);

	context.pop_value_reference(prev_value);

	return result_value;
}

static expr_value generate_expression(
	ast::expr_aggregate_default_construct const &aggregate_default_construct,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	if (!result_dest.has_value())
	{
		result_dest = context.add_uninitialized_value(get_type(aggregate_default_construct.type, context));
	}

	auto const &result_value = result_dest.get();
	for (auto const i : bz::iota(0, aggregate_default_construct.default_construct_exprs.size()))
	{
		bz_assert(!aggregate_default_construct.default_construct_exprs[i].get_expr_type().is_any_reference());
		auto const member = context.create_struct_gep(result_value, i);
		generate_expression(aggregate_default_construct.default_construct_exprs[i], context, member);
	}
	return result_value;
}

static expr_value generate_expression(
	ast::expr_array_default_construct const &array_default_construct,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	bz_assert(array_default_construct.type.is<ast::ts_array>());
	auto const size = array_default_construct.type.get<ast::ts_array>().size;

	if (!result_dest.has_value())
	{
		result_dest = context.add_uninitialized_value(get_type(array_default_construct.type, context));
	}
	auto const &result_value = result_dest.get();

	auto const loop_info = create_loop_start(size, context);

	auto const elem_result_dest = context.create_array_gep(result_value, loop_info.index);
	generate_expression(array_default_construct.default_construct_expr, context, elem_result_dest);

	create_loop_end(loop_info, context);

	return result_value;
}

static expr_value generate_expression(
	ast::expr_optional_default_construct const &optional_default_construct,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	auto const type = get_type(optional_default_construct.type, context);
	auto const init_expr = optional_default_construct.type.is_optional_pointer_like()
		? "0"
		: bz::format("({}){}", context.to_string(type), "{0}");

	return value_or_result_dest(init_expr, type, result_dest, context);
}

static expr_value generate_expression(
	ast::expr_builtin_default_construct const &builtin_default_construct,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	bz_assert(builtin_default_construct.type.is<ast::ts_array_slice>());
	auto const type = get_type(builtin_default_construct.type, context);
	auto const init_expr = bz::format("({}){}", context.to_string(type), "{0}");

	return value_or_result_dest(init_expr, type, result_dest, context);
}

static expr_value generate_expression(
	ast::expr_aggregate_copy_construct const &aggregate_copy_construct,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	auto const copied_value = generate_expression(aggregate_copy_construct.copied_value, context, {});
	if (!result_dest.has_value())
	{
		result_dest = context.add_uninitialized_value(copied_value.get_type());
	}

	auto const &result_value = result_dest.get();
	for (auto const i : bz::iota(0, aggregate_copy_construct.copy_exprs.size()))
	{
		auto const result_member_value = context.create_struct_gep(result_value, i);
		auto const member_value = context.create_struct_gep(copied_value, i);
		auto const prev_value = context.push_value_reference(member_value);
		generate_expression(aggregate_copy_construct.copy_exprs[i], context, result_member_value);
		context.pop_value_reference(prev_value);
	}
	return result_value;
}

static expr_value generate_expression(
	ast::expr_array_copy_construct const &array_copy_construct,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	auto const copied_value = generate_expression(array_copy_construct.copied_value, context, {});

	if (!result_dest.has_value())
	{
		result_dest = context.add_uninitialized_value(copied_value.get_type());
	}
	auto const &result_value = result_dest.get();

	bz_assert(context.is_array(copied_value.get_type()));
	auto const size = context.maybe_get_array(copied_value.get_type())->size;
	auto const loop_info = create_loop_start(size, context);

	auto const elem_result_dest = context.create_array_gep(result_value, loop_info.index);
	auto const copied_elem = context.create_array_gep(copied_value, loop_info.index);
	auto const prev_value = context.push_value_reference(copied_elem);
	generate_expression(array_copy_construct.copy_expr, context, elem_result_dest);
	context.pop_value_reference(prev_value);

	create_loop_end(loop_info, context);

	return result_value;
}

static expr_value generate_expression(
	ast::expr_optional_copy_construct const &optional_copy_construct,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	auto const copied_value = generate_expression(optional_copy_construct.copied_value, context, {});

	if (!result_dest.has_value())
	{
		result_dest = context.add_uninitialized_value(copied_value.get_type());
	}
	auto const &result_value = result_dest.get();

	auto const has_value = get_optional_has_value(copied_value, context);
	set_optional_has_value(result_value, has_value, context);

	auto const prev_if_info = context.begin_if(has_value);

	auto const result_opt_value = get_optional_value(result_value, context);
	auto const prev_value = context.push_value_reference(get_optional_value(copied_value, context));
	generate_expression(optional_copy_construct.value_copy_expr, context, result_opt_value);
	context.pop_value_reference(prev_value);

	context.end_if(prev_if_info);

	return result_value;
}

static expr_value generate_expression(
	ast::expr_trivial_copy_construct const &trivial_copy_construct,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	auto const copied_value = generate_expression(trivial_copy_construct.copied_value, context, {});
	if (result_dest.has_value())
	{
		auto const &result_value = result_dest.get();
		context.create_assignment(result_value, copied_value);
		return result_value;
	}
	else
	{
		return context.create_trivial_copy(copied_value);
	}
}

static expr_value generate_expression(
	ast::expr_aggregate_move_construct const &aggregate_move_construct,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	auto const moved_value = generate_expression(aggregate_move_construct.moved_value, context, {});
	if (!result_dest.has_value())
	{
		result_dest = context.add_uninitialized_value(moved_value.get_type());
	}

	auto const &result_value = result_dest.get();
	for (auto const i : bz::iota(0, aggregate_move_construct.move_exprs.size()))
	{
		auto const result_member_value = context.create_struct_gep(result_value, i);
		auto const member_value = context.create_struct_gep(moved_value, i);
		auto const prev_info = context.push_expression_scope();
		auto const prev_value = context.push_value_reference(member_value);
		generate_expression(aggregate_move_construct.move_exprs[i], context, result_member_value);
		context.pop_value_reference(prev_value);
		context.pop_expression_scope(prev_info);
	}
	return result_value;
}

static expr_value generate_expression(
	ast::expr_array_move_construct const &array_move_construct,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	auto const moved_value = generate_expression(array_move_construct.moved_value, context, {});

	if (!result_dest.has_value())
	{
		result_dest = context.add_uninitialized_value(moved_value.get_type());
	}
	auto const &result_value = result_dest.get();

	bz_assert(context.is_array(moved_value.get_type()));
	auto const size = context.maybe_get_array(moved_value.get_type())->size;
	auto const loop_info = create_loop_start(size, context);

	auto const elem_result_dest = context.create_array_gep(result_value, loop_info.index);
	auto const moved_elem = context.create_array_gep(moved_value, loop_info.index);
	auto const prev_value = context.push_value_reference(moved_elem);
	generate_expression(array_move_construct.move_expr, context, elem_result_dest);
	context.pop_value_reference(prev_value);

	create_loop_end(loop_info, context);

	return result_value;
}

static expr_value generate_expression(
	ast::expr_optional_move_construct const &optional_move_construct,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	auto const moved_value = generate_expression(optional_move_construct.moved_value, context, {});

	if (!result_dest.has_value())
	{
		result_dest = context.add_uninitialized_value(moved_value.get_type());
	}
	auto const &result_value = result_dest.get();

	auto const has_value = get_optional_has_value(moved_value, context);
	set_optional_has_value(result_value, has_value, context);

	auto const prev_if_info = context.begin_if(has_value);

	auto const result_opt_value = get_optional_value(result_value, context);
	auto const prev_value = context.push_value_reference(get_optional_value(moved_value, context));
	generate_expression(optional_move_construct.value_move_expr, context, result_opt_value);
	context.pop_value_reference(prev_value);

	context.end_if(prev_if_info);

	return result_value;
}

static expr_value generate_expression(
	ast::expr_trivial_relocate const &trivial_relocate,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	bz_assert(!trivial_relocate.value.get_expr_type().is_reference());
	auto const value = generate_expression(trivial_relocate.value, context, {});
	if (result_dest.has_value())
	{
		auto const &result_value = result_dest.get();
		context.create_assignment(result_value, value);
		return result_value;
	}
	else
	{
		return context.create_trivial_copy(value);
	}
}

static expr_value generate_expression(
	ast::expr_aggregate_destruct const &aggregate_destruct,
	codegen_context &context
)
{
	auto const value = generate_expression(aggregate_destruct.value, context, {});

	for (auto const i : bz::iota(0, aggregate_destruct.elem_destruct_calls.size()).reversed())
	{
		if (aggregate_destruct.elem_destruct_calls[i].not_null())
		{
			auto const elem_value = context.create_struct_gep(value, i);
			auto const prev_value = context.push_value_reference(elem_value);
			generate_expression(aggregate_destruct.elem_destruct_calls[i], context, {});
			context.pop_value_reference(prev_value);
		}
	}

	return context.get_void_value();
}

static expr_value generate_expression(
	ast::expr_array_destruct const &array_destruct,
	codegen_context &context
)
{
	auto const value = generate_expression(array_destruct.value, context, {});

	bz_assert(context.is_array(value.get_type()));
	auto const size = context.maybe_get_array(value.get_type())->size;
	auto const loop_info = create_loop_start(size, context);

	auto const elem_value = context.create_array_gep(value, loop_info.index);
	auto const prev_value = context.push_value_reference(elem_value);
	generate_expression(array_destruct.elem_destruct_call, context, {});
	context.pop_value_reference(prev_value);

	create_loop_end(loop_info, context);

	return context.get_void_value();
}

static expr_value generate_expression(
	ast::expr_optional_destruct const &optional_destruct,
	codegen_context &context
)
{
	auto const value = generate_expression(optional_destruct.value, context, {});

	auto const has_value = get_optional_has_value(value, context);
	auto const prev_if_info = context.begin_if(has_value);

	auto const prev_value = context.push_value_reference(get_optional_value(value, context));
	generate_expression(optional_destruct.value_destruct_call, context, {});
	context.pop_value_reference(prev_value);

	context.end_if(prev_if_info);

	return context.get_void_value();
}

static expr_value generate_expression(
	ast::expr_base_type_destruct const &base_type_destruct,
	codegen_context &context
)
{
	auto const value = generate_expression(base_type_destruct.value, context, {});

	if (base_type_destruct.destruct_call.not_null())
	{
		auto const prev_value = context.push_value_reference(value);
		generate_expression(base_type_destruct.destruct_call, context, {});
		context.pop_value_reference(prev_value);
	}

	for (auto const i : bz::iota(0, base_type_destruct.member_destruct_calls.size()).reversed())
	{
		if (base_type_destruct.member_destruct_calls[i].not_null())
		{
			auto const elem_value = context.create_struct_gep(value, i);
			auto const prev_value = context.push_value_reference(elem_value);
			generate_expression(base_type_destruct.member_destruct_calls[i], context, {});
			context.pop_value_reference(prev_value);
		}
	}

	return context.get_void_value();
}

static expr_value generate_expression(
	ast::expr_destruct_value const &destruct_value,
	codegen_context &context
)
{
	auto const value = generate_expression(destruct_value.value, context, {});
	if (destruct_value.destruct_call.not_null())
	{
		auto const prev_value = context.push_value_reference(value);
		generate_expression(destruct_value.destruct_call, context, {});
		context.pop_value_reference(prev_value);
	}

	return context.get_void_value();
}

struct pointer_compare_info_t
{
	codegen_context::if_info_t prev_if_info;
	bool compare_needed;
};

[[nodiscard]] static pointer_compare_info_t create_pointer_compare_begin(
	expr_value const &lhs,
	expr_value const &rhs,
	codegen_context &context
)
{
	if (lhs.get_type() == rhs.get_type() && !lhs.is_rvalue && !rhs.is_rvalue)
	{
		auto const prev_if_info = context.begin_if(context.create_not_equals(
			context.create_address_of(lhs),
			context.create_address_of(rhs)
		));
		return { prev_if_info, true };
	}
	else
	{
		return { .prev_if_info = {}, .compare_needed = false };
	}
}

static void create_pointer_compare_end(pointer_compare_info_t pointer_compare_info, codegen_context &context)
{
	if (pointer_compare_info.compare_needed)
	{
		context.end_if(pointer_compare_info.prev_if_info);
	}
}

static expr_value generate_expression(
	ast::expr_aggregate_swap const &aggregate_swap,
	codegen_context &context
)
{
	auto const lhs = generate_expression(aggregate_swap.lhs, context, {});
	auto const rhs = generate_expression(aggregate_swap.rhs, context, {});
	auto const compare_info = create_pointer_compare_begin(lhs, rhs, context);

	for (auto const i : bz::iota(0, aggregate_swap.swap_exprs.size()))
	{
		auto const lhs_member = context.create_struct_gep(lhs, i);
		auto const rhs_member = context.create_struct_gep(rhs, i);

		auto const prev_info = context.push_expression_scope();
		auto const lhs_prev_value = context.push_value_reference(lhs_member);
		auto const rhs_prev_value = context.push_value_reference(rhs_member);
		generate_expression(aggregate_swap.swap_exprs[i], context, {});
		context.pop_value_reference(rhs_prev_value);
		context.pop_value_reference(lhs_prev_value);
		context.pop_expression_scope(prev_info);
	}

	create_pointer_compare_end(compare_info, context);
	return context.get_void_value();
}

static expr_value generate_expression(
	ast::expr_array_swap const &array_swap,
	codegen_context &context
)
{
	auto const lhs = generate_expression(array_swap.lhs, context, {});
	auto const rhs = generate_expression(array_swap.rhs, context, {});
	auto const compare_info = create_pointer_compare_begin(lhs, rhs, context);

	bz_assert(context.is_array(lhs.get_type()));
	auto const size = context.maybe_get_array(lhs.get_type())->size;
	auto const loop_info = create_loop_start(size, context);

	auto const lhs_element = context.create_array_gep(lhs, loop_info.index);
	auto const rhs_element = context.create_array_gep(rhs, loop_info.index);
	auto const lhs_prev_value = context.push_value_reference(lhs_element);
	auto const rhs_prev_value = context.push_value_reference(rhs_element);
	generate_expression(array_swap.swap_expr, context, {});
	context.pop_value_reference(rhs_prev_value);
	context.pop_value_reference(lhs_prev_value);

	create_loop_end(loop_info, context);

	create_pointer_compare_end(compare_info, context);
	return context.get_void_value();
}

static expr_value generate_expression(
	ast::expr_optional_swap const &optional_swap,
	codegen_context &context
)
{
	auto const lhs = generate_expression(optional_swap.lhs, context, {});
	auto const rhs = generate_expression(optional_swap.rhs, context, {});
	auto const compare_info = create_pointer_compare_begin(lhs, rhs, context);

	auto const lhs_has_value = get_optional_has_value(lhs, context);
	auto const rhs_has_value = get_optional_has_value(rhs, context);
	auto const both_has_value = context.create_logical_and(lhs_has_value, rhs_has_value);

	auto const prev_if_info = context.begin_if(both_has_value);
	{
		auto const prev_info = context.push_expression_scope();
		auto const lhs_prev_value = context.push_value_reference(get_optional_value(lhs, context));
		auto const rhs_prev_value = context.push_value_reference(get_optional_value(rhs, context));
		generate_expression(optional_swap.value_swap_expr, context, {});
		context.pop_value_reference(rhs_prev_value);
		context.pop_value_reference(lhs_prev_value);
		context.pop_expression_scope(prev_info);
	}

	context.begin_else_if(lhs_has_value);
	{
		auto const prev_info = context.push_expression_scope();
		auto const rhs_value = get_optional_value(rhs, context);
		auto const prev_value = context.push_value_reference(get_optional_value(lhs, context));
		generate_expression(optional_swap.lhs_move_expr, context, rhs_value);
		context.pop_value_reference(prev_value);

		set_optional_has_value(lhs, false, context);
		set_optional_has_value(rhs, true, context);
		context.pop_expression_scope(prev_info);
	}

	context.begin_else_if(rhs_has_value);
	{
		auto const prev_info = context.push_expression_scope();
		auto const lhs_value = get_optional_value(lhs, context);
		auto const prev_value = context.push_value_reference(get_optional_value(rhs, context));
		generate_expression(optional_swap.rhs_move_expr, context, lhs_value);
		context.pop_value_reference(prev_value);

		set_optional_has_value(lhs, true, context);
		set_optional_has_value(rhs, false, context);
		context.pop_expression_scope(prev_info);
	}

	context.end_if(prev_if_info);

	create_pointer_compare_end(compare_info, context);

	return context.get_void_value();
}

static expr_value generate_expression(
	ast::expr_base_type_swap const &base_type_swap,
	codegen_context &context
)
{
	auto const lhs = generate_expression(base_type_swap.lhs, context, {});
	auto const rhs = generate_expression(base_type_swap.rhs, context, {});
	auto const compare_info = create_pointer_compare_begin(lhs, rhs, context);

	bz_assert(lhs.get_type() == rhs.get_type());
	auto const temp = context.add_uninitialized_value(lhs.get_type());

	// temp = move lhs
	{
		auto const prev_info = context.push_expression_scope();
		auto const prev_value = context.push_value_reference(lhs);
		generate_expression(base_type_swap.lhs_move_expr, context, temp);
		context.pop_value_reference(prev_value);
		context.pop_expression_scope(prev_info);
	}
	// lhs = move rhs
	{
		auto const prev_info = context.push_expression_scope();
		auto const prev_value = context.push_value_reference(rhs);
		generate_expression(base_type_swap.rhs_move_expr, context, lhs);
		context.pop_value_reference(prev_value);
		context.pop_expression_scope(prev_info);
	}
	// rhs = move temp
	{
		auto const prev_info = context.push_expression_scope();
		auto const prev_value = context.push_value_reference(temp);
		generate_expression(base_type_swap.temp_move_expr, context, rhs);
		context.pop_value_reference(prev_value);
		context.pop_expression_scope(prev_info);
	}

	create_pointer_compare_end(compare_info, context);
	return context.get_void_value();
}

static expr_value generate_expression(
	ast::expr_trivial_swap const &trivial_swap,
	codegen_context &context
)
{
	auto const lhs = generate_expression(trivial_swap.lhs, context, {});
	auto const rhs = generate_expression(trivial_swap.rhs, context, {});
	bz_assert(lhs.get_type() == rhs.get_type());
	auto const compare_info = create_pointer_compare_begin(lhs, rhs, context);

	auto const temp = context.add_uninitialized_value(lhs.get_type());
	context.create_assignment(temp, lhs);
	context.create_assignment(lhs, rhs);
	context.create_assignment(rhs, temp);

	create_pointer_compare_end(compare_info, context);
	return context.get_void_value();
}

static expr_value generate_expression(
	ast::expr_aggregate_assign const &aggregate_assign,
	codegen_context &context
)
{
	auto const rhs = generate_expression(aggregate_assign.rhs, context, {});
	auto const lhs = generate_expression(aggregate_assign.lhs, context, {});

	auto const compare_info = create_pointer_compare_begin(lhs, rhs, context);

	for (auto const i : bz::iota(0, aggregate_assign.assign_exprs.size()))
	{
		auto const lhs_member = context.create_struct_gep(lhs, i);
		auto const rhs_member = context.create_struct_gep(rhs, i);

		auto const prev_info = context.push_expression_scope();
		auto const lhs_prev_value = context.push_value_reference(lhs_member);
		auto const rhs_prev_value = context.push_value_reference(rhs_member);
		generate_expression(aggregate_assign.assign_exprs[i], context, {});
		context.pop_value_reference(rhs_prev_value);
		context.pop_value_reference(lhs_prev_value);
		context.pop_expression_scope(prev_info);
	}

	create_pointer_compare_end(compare_info, context);

	return lhs;
}

static expr_value generate_expression(
	ast::expr_array_assign const &array_assign,
	codegen_context &context
)
{
	auto const rhs = generate_expression(array_assign.rhs, context, {});
	auto const lhs = generate_expression(array_assign.lhs, context, {});
	auto const compare_info = create_pointer_compare_begin(lhs, rhs, context);

	bz_assert(context.is_array(lhs.get_type()));
	auto const size = context.maybe_get_array(lhs.get_type())->size;
	auto const loop_info = create_loop_start(size, context);

	auto const lhs_element = context.create_array_gep(lhs, loop_info.index);
	auto const rhs_element = context.create_array_gep(rhs, loop_info.index);
	auto const lhs_prev_value = context.push_value_reference(lhs_element);
	auto const rhs_prev_value = context.push_value_reference(rhs_element);
	generate_expression(array_assign.assign_expr, context, {});
	context.pop_value_reference(rhs_prev_value);
	context.pop_value_reference(lhs_prev_value);

	create_loop_end(loop_info, context);

	create_pointer_compare_end(compare_info, context);
	return context.get_void_value();
}

static expr_value generate_expression(
	ast::expr_optional_assign const &optional_assign,
	codegen_context &context
)
{
	auto const rhs = generate_expression(optional_assign.rhs, context, {});
	auto const lhs = generate_expression(optional_assign.lhs, context, {});
	auto const compare_info = create_pointer_compare_begin(lhs, rhs, context);

	auto const lhs_has_value = get_optional_has_value(lhs, context);
	auto const rhs_has_value = get_optional_has_value(rhs, context);
	auto const both_has_value = context.create_logical_and(lhs_has_value, rhs_has_value);

	auto const prev_if_info = context.begin_if(both_has_value);
	{
		auto const prev_info = context.push_expression_scope();
		auto const lhs_prev_value = context.push_value_reference(get_optional_value(lhs, context));
		auto const rhs_prev_value = context.push_value_reference(get_optional_value(rhs, context));
		generate_expression(optional_assign.value_assign_expr, context, {});
		context.pop_value_reference(rhs_prev_value);
		context.pop_value_reference(lhs_prev_value);
		context.pop_expression_scope(prev_info);
	}

	context.begin_else_if(lhs_has_value);
	{
		auto const prev_value = context.push_value_reference(get_optional_value(lhs, context));
		generate_expression(optional_assign.value_destruct_expr, context, {});
		context.pop_value_reference(prev_value);

		set_optional_has_value(lhs, false, context);
	}

	context.begin_else_if(rhs_has_value);
	{
		auto const prev_info = context.push_expression_scope();
		auto const lhs_value = get_optional_value(lhs, context);
		auto const prev_value = context.push_value_reference(get_optional_value(rhs, context));
		generate_expression(optional_assign.value_construct_expr, context, lhs_value);
		context.pop_value_reference(prev_value);

		set_optional_has_value(lhs, true, context);
		context.pop_expression_scope(prev_info);
	}

	context.end_if(prev_if_info);

	create_pointer_compare_end(compare_info, context);

	return context.get_void_value();
}

static expr_value generate_expression(
	ast::expr_optional_null_assign const &optional_null_assign,
	codegen_context &context
)
{
	generate_expression(optional_null_assign.rhs, context, {});
	auto const lhs = generate_expression(optional_null_assign.lhs, context, {});

	if (context.is_pointer_or_function(lhs.get_type()))
	{
		context.create_assignment(lhs, "0");
	}
	else if (optional_null_assign.value_destruct_expr.not_null())
	{
		auto const has_value = get_optional_has_value(lhs, context);

		auto const prev_if_info = context.begin_if(has_value);

		auto const prev_value = context.push_value_reference(get_optional_value(lhs, context));
		generate_expression(optional_null_assign.value_destruct_expr, context, {});
		context.pop_value_reference(prev_value);

		set_optional_has_value(lhs, false, context);

		context.end_if(prev_if_info);
	}
	else
	{
		set_optional_has_value(lhs, false, context);
	}

	return lhs;
}

static expr_value generate_expression(
	ast::expr_optional_value_assign const &optional_value_assign,
	codegen_context &context
)
{
	auto const rhs = generate_expression(optional_value_assign.rhs, context, {});
	auto const lhs = generate_expression(optional_value_assign.lhs, context, {});

	if (context.is_pointer_or_function(lhs.get_type()))
	{
		context.create_assignment(lhs, rhs);
		return lhs;
	}

	auto const has_value = get_optional_has_value(lhs, context);

	auto const prev_if_info = context.begin_if(has_value);
	{
		auto const prev_info = context.push_expression_scope();
		auto const lhs_prev_value = context.push_value_reference(get_optional_value(lhs, context));
		auto const rhs_prev_value = context.push_value_reference(rhs);
		generate_expression(optional_value_assign.value_assign_expr, context, {});
		context.pop_value_reference(rhs_prev_value);
		context.pop_value_reference(lhs_prev_value);
		context.pop_expression_scope(prev_info);
	}

	context.begin_else();
	{
		auto const prev_info = context.push_expression_scope();
		auto const prev_value = context.push_value_reference(rhs);
		auto const lhs_value = get_optional_value(lhs, context);
		generate_expression(optional_value_assign.value_construct_expr, context, lhs_value);
		context.pop_value_reference(prev_value);
		context.pop_expression_scope(prev_info);

		set_optional_has_value(lhs, true, context);
	}

	context.end_if(prev_if_info);

	return lhs;
}

static expr_value generate_expression(
	ast::expr_optional_reference_value_assign const &optional_reference_value_assign,
	codegen_context &context
)
{
	auto const rhs = generate_expression(optional_reference_value_assign.rhs, context, {});
	auto const lhs = generate_expression(optional_reference_value_assign.lhs, context, {});

	auto const rhs_reference_value = context.create_address_of(rhs);
	context.create_assignment(lhs, rhs_reference_value);

	return lhs;
}

static expr_value generate_expression(
	ast::expr_base_type_assign const &base_type_assign,
	codegen_context &context
)
{
	auto const rhs = generate_expression(base_type_assign.rhs, context, {});
	auto const lhs = generate_expression(base_type_assign.lhs, context, {});

	auto const compare_info = create_pointer_compare_begin(lhs, rhs, context);

	// lhs destruct
	{
		auto const prev_info = context.push_expression_scope();
		auto const prev_value = context.push_value_reference(lhs);
		generate_expression(base_type_assign.lhs_destruct_expr, context, {});
		context.pop_value_reference(prev_value);
		context.pop_expression_scope(prev_info);
	}

	// rhs copy
	{
		auto const prev_info = context.push_expression_scope();
		auto const prev_value = context.push_value_reference(rhs);
		generate_expression(base_type_assign.rhs_copy_expr, context, lhs);
		context.pop_value_reference(prev_value);
		context.pop_expression_scope(prev_info);
	}

	create_pointer_compare_end(compare_info, context);

	return lhs;
}

static expr_value generate_expression(
	ast::expr_trivial_assign const &trivial_assign,
	codegen_context &context
)
{
	auto const rhs = generate_expression(trivial_assign.rhs, context, {});
	auto const lhs = generate_expression(trivial_assign.lhs, context, {});

	context.create_assignment(lhs, rhs);

	return lhs;
}

static expr_value generate_expression(
	ast::expr_member_access const &member_access,
	codegen_context &context
)
{
	auto const base = generate_expression(member_access.base, context, {});

	bz_assert(member_access.base.get_expr_type().remove_mut_reference().is<ast::ts_base_type>());
	auto const info = member_access.base.get_expr_type().remove_mut_reference().get<ast::ts_base_type>().info;
	auto const &accessed_type = info->member_variables[member_access.index]->get_type();
	if (accessed_type.is_reference())
	{
		return context.create_dereference(context.create_struct_gep_value(base, member_access.index));
	}
	else
	{
		return context.create_struct_gep(base, member_access.index);
	}
}

static expr_value generate_expression(
	lex::src_tokens const &src_tokens,
	ast::expr_optional_extract_value const &optional_extract_value,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	auto const optional_value = generate_expression(optional_extract_value.optional_value, context, {});
	generate_optional_get_value_check(src_tokens, optional_value, context);

	auto const optional_value_type = optional_extract_value.optional_value.get_expr_type().remove_any_mut();
	if (optional_value_type.is_optional_reference())
	{
		auto const pointer_value = get_optional_value(optional_value, context);
		bz_assert(!result_dest.has_value());
		return context.create_dereference(pointer_value);
	}
	else
	{
		auto const prev_info = context.push_expression_scope();
		auto const prev_value = context.push_value_reference(get_optional_value(optional_value, context));
		auto const result_value = generate_expression(optional_extract_value.value_move_expr, context, result_dest);
		context.pop_value_reference(prev_value);
		context.pop_expression_scope(prev_info);

		return result_value;
	}
}

static expr_value generate_expression(
	ast::expr_rvalue_member_access const &rvalue_member_access,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	auto const base = generate_expression(rvalue_member_access.base, context, {});

	bz_assert(rvalue_member_access.base.get_expr_type().remove_mut_reference().is<ast::ts_base_type>());
	auto const info = rvalue_member_access.base.get_expr_type().remove_mut_reference().get<ast::ts_base_type>().info;
	auto const &accessed_type = info->member_variables[rvalue_member_access.index]->get_type();
	bz_assert(!result_dest.has_value() || !accessed_type.is_reference());

	auto const prev_info = context.push_expression_scope();
	expr_value result = {};
	for (auto const i : bz::iota(0, rvalue_member_access.member_refs.size()))
	{
		if (rvalue_member_access.member_refs[i].is_null())
		{
			continue;
		}

		auto const member_value = [&]() {
			if (i == rvalue_member_access.index && accessed_type.is_reference())
			{
				auto const ref_ref = context.create_struct_gep_value(base, i);
				bz_assert(context.is_pointer(ref_ref.get_type()));
				return context.create_dereference(ref_ref);
			}
			else
			{
				return context.create_struct_gep(base, i);
			}
		}();

		auto const prev_value = context.push_value_reference(member_value);
		if (i == rvalue_member_access.index)
		{
			auto const prev_info = context.push_expression_scope();
			result = generate_expression(rvalue_member_access.member_refs[i], context, result_dest);
			context.pop_expression_scope(prev_info);
		}
		else
		{
			generate_expression(rvalue_member_access.member_refs[i], context, {});
		}
		context.pop_value_reference(prev_value);
	}
	context.pop_expression_scope(prev_info);

	return result;
}

static expr_value generate_expression(
	ast::expr_type_member_access const &type_member_access,
	codegen_context &context
)
{
	return context.get_variable(*type_member_access.var_decl);
}

static expr_value generate_expression(
	ast::expr_compound const &compound_expr,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	auto const prev_info = context.push_expression_scope();
	for (auto const &stmt : compound_expr.statements)
	{
		generate_statement(stmt, context);
	}

	if (compound_expr.final_expr.is_null())
	{
		context.pop_expression_scope(prev_info);
		bz_assert(!result_dest.has_value());
		return context.get_void_value();
	}
	else
	{
		auto const result = generate_expression(compound_expr.final_expr, context, result_dest);
		context.pop_expression_scope(prev_info);
		return result;
	}
}

static expr_value generate_expression(
	ast::expr_if const &if_expr,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	auto const condition_prev_info = context.push_expression_scope();
	auto const condition = generate_expression(if_expr.condition, context, {});
	context.pop_expression_scope(condition_prev_info);

	bz::optional<expr_value> reference_result = {};
	if (if_expr.then_block.get_expr_type().is_any_reference())
	{
		bz_assert(!result_dest.has_value());
		reference_result = context.add_uninitialized_value(get_type(if_expr.then_block.get_expr_type(), context));
	}
	else if (if_expr.else_block.get_expr_type().is_any_reference())
	{
		bz_assert(!result_dest.has_value());
		reference_result = context.add_uninitialized_value(get_type(if_expr.else_block.get_expr_type(), context));
	}

	auto const prev_if_info = context.begin_if(condition);
	// then
	{
		auto const prev_info = context.push_expression_scope();
		auto const value = generate_expression(if_expr.then_block, context, result_dest);
		if (reference_result.has_value())
		{
			context.create_assignment(reference_result.get(), context.create_address_of(value));
		}
		context.pop_expression_scope(prev_info);
	}

	// else
	if (if_expr.else_block.not_null())
	{
		context.begin_else();
		auto const prev_info = context.push_expression_scope();
		auto const value = generate_expression(if_expr.else_block, context, result_dest);
		if (reference_result.has_value())
		{
			context.create_assignment(reference_result.get(), context.create_address_of(value));
		}
		context.pop_expression_scope(prev_info);
	}
	context.end_if(prev_if_info);

	if (result_dest.has_value())
	{
		return result_dest.get();
	}
	else if (reference_result.has_value())
	{
		return context.create_dereference(reference_result.get());
	}
	else
	{
		return context.get_void_value();
	}
}

static expr_value generate_expression(
	ast::expr_if_consteval const &if_consteval_expr,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	bz_assert(if_consteval_expr.condition.is_constant());
	bz_assert(if_consteval_expr.condition.get_constant_value().is_boolean());
	auto const condition = if_consteval_expr.condition.get_constant_value().get_boolean();
	if (condition)
	{
		return generate_expression(if_consteval_expr.then_block, context, result_dest);
	}
	else if (if_consteval_expr.else_block.not_null())
	{
		return generate_expression(if_consteval_expr.else_block, context, result_dest);
	}
	else
	{
		bz_assert(!result_dest.has_value());
		return context.get_void_value();
	}
}

static expr_value generate_integral_switch(
	lex::src_tokens const &src_tokens,
	ast::expr_switch const &switch_expr,
	expr_value matched_value,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	bz::optional<expr_value> reference_result = {};
	for (auto const &[case_vals, case_expr] : switch_expr.cases)
	{
		if (case_expr.get_expr_type().is_any_reference())
		{
			bz_assert(!result_dest.has_value());
			reference_result = context.add_uninitialized_value(get_type(case_expr.get_expr_type(), context));
			break;
		}
	}
	if (!reference_result.has_value() && switch_expr.default_case.get_expr_type().is_any_reference())
	{
		bz_assert(!result_dest.has_value());
		reference_result = context.add_uninitialized_value(get_type(switch_expr.default_case.get_expr_type(), context));
	}

	auto const prev_switch_info = context.begin_switch(matched_value);

	for (auto const &[case_vals, case_expr] : switch_expr.cases)
	{
		for (auto const &expr : case_vals)
		{
			bz_assert(expr.is_constant());
			auto const &value = expr.get_constant_value();
			switch (value.kind())
			{
			static_assert(ast::constant_value::variant_count == 19);
			case ast::constant_value_kind::sint:
			{
				bz::u8string buffer = "";
				write_sint(buffer, value.get_sint());
				context.add_case_label(buffer);
				break;
			}
			case ast::constant_value_kind::uint:
				context.add_case_label(bz::format("{}u", value.get_uint()));
				break;
			case ast::constant_value_kind::u8char:
				context.add_case_label(bz::format("{}u", value.get_u8char()));
				break;
			case ast::constant_value_kind::boolean:
				context.add_case_label(value.get_boolean() ? "1" : "0");
				break;
			case ast::constant_value_kind::enum_:
			{
				auto const enum_value = value.get_enum();
				bz_assert(enum_value.decl->underlying_type.is<ast::ts_base_type>());
				auto const type_kind = enum_value.decl->underlying_type.get<ast::ts_base_type>().info->kind;
				if (ast::is_signed_integer_kind(type_kind))
				{
					bz::u8string buffer = "";
					write_sint(buffer, static_cast<int64_t>(enum_value.value));
					context.add_case_label(buffer);
				}
				else
				{
					context.add_case_label(bz::format("{}", enum_value.value));
				}
				break;
			}
			default:
				bz_unreachable;
			}
		}

		context.begin_case();
		auto const prev_info = context.push_expression_scope();
		auto const value = generate_expression(case_expr, context, result_dest);
		if (reference_result.has_value() && case_expr.get_expr_type().is_any_reference())
		{
			context.create_assignment(reference_result.get(), context.create_address_of(value));
		}
		context.pop_expression_scope(prev_info);
		context.end_case();
	}

	if (switch_expr.default_case.not_null())
	{
		context.begin_default_case();
		auto const prev_info = context.push_expression_scope();
		auto const value = generate_expression(switch_expr.default_case, context, result_dest);
		if (reference_result.has_value() && switch_expr.default_case.get_expr_type().is_any_reference())
		{
			context.create_assignment(reference_result.get(), context.create_address_of(value));
		}
		context.pop_expression_scope(prev_info);
		context.end_case();
	}
	else if (switch_expr.is_complete)
	{
		context.begin_default_case();
		if (global_data::panic_on_invalid_switch)
		{
			generate_panic_call(src_tokens, "invalid value used in 'switch'", context);
		}
		else
		{
			context.create_unreachable();
		}
		context.end_case();
	}

	context.end_switch(prev_switch_info);

	if (result_dest.has_value())
	{
		return result_dest.get();
	}
	else if (reference_result.has_value())
	{
		return context.create_dereference(reference_result.get());
	}
	else
	{
		return context.get_void_value();
	}
}

struct string_switch_case_info_t
{
	bz::u8string_view string_value;
	size_t case_index;
};

static uint64_t get_string_int_value(bz::u8string_view str, codegen_context &context)
{
	bz_assert(str.size() <= 8);
	uint64_t result = 0;
	if (context.is_little_endian())
	{
		for (auto const i : bz::iota(0, str.size()))
		{
			auto const c = static_cast<uint64_t>(static_cast<uint8_t>(*(str.data() + i)));
			result |= c << (i * 8);
		}
	}
	else
	{
		for (auto const i : bz::iota(0, str.size()))
		{
			auto const c = static_cast<uint64_t>(static_cast<uint8_t>(*(str.data() + i)));
			result |= c << ((7 - i) * 8);
		}
	}

	return result;
}

static expr_value are_strings_equal(expr_value begin_ptr, bz::u8string_view str, codegen_context &context)
{
	if (str.size() == 0)
	{
		return context.get_true_value();
	}

	auto const &lhs = begin_ptr;
	auto const rhs_type = context.add_const_pointer(context.get_uint8());
	auto const rhs = context.add_temporary_expression(
		context.create_cstring(str),
		rhs_type,
		false,
		false,
		true,
		precedence::identifier
	);
	auto const size = context.get_unsigned_value(str.size(), context.get_usize());
	// size is never zero here, otherwise memcmp has undefined behaviour
	bz_assert(str.size() != 0);
	auto const memcmp_result = generate_trivial_function_call("memcmp", { lhs, rhs, size }, context.get_c_int(), context, {});
	return context.create_equals(memcmp_result, context.get_signed_zero_value(memcmp_result.get_type()));
}

static expr_value generate_string_switch(
	ast::expr_switch const &switch_expr,
	expr_value begin_ptr,
	expr_value end_ptr,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	bz::optional<expr_value> reference_result = {};
	for (auto const &[case_vals, case_expr] : switch_expr.cases)
	{
		if (case_expr.get_expr_type().is_any_reference())
		{
			bz_assert(!result_dest.has_value());
			reference_result = context.add_uninitialized_value(get_type(case_expr.get_expr_type(), context));
			break;
		}
	}
	if (!reference_result.has_value() && switch_expr.default_case.get_expr_type().is_any_reference())
	{
		bz_assert(!result_dest.has_value());
		reference_result = context.add_uninitialized_value(get_type(switch_expr.default_case.get_expr_type(), context));
	}

	// switching on strings is done in two main steps:
	//   1. do a switch on the size of the string to narrow it down
	//   2. for each unique size in the cases determine the matching case

	auto const case_count = switch_expr.cases.transform([](auto const &case_val) {
		return case_val.values.size();
	}).sum();
	auto case_infos = ast::arena_vector<string_switch_case_info_t>();
	case_infos.reserve(case_count);
	for (auto const case_index : bz::iota(0, switch_expr.cases.size()))
	{
		for (auto const &value : switch_expr.cases[case_index].values)
		{
			bz_assert(value.is_constant() && value.get_constant_value().is_string());
			auto const string_value = value.get_constant_value().get_string();
			case_infos.push_back({ string_value, case_index });
		}
	}
	auto const default_case_index = switch_expr.cases.size();

	// sort in terms of string size
	case_infos.sort([](auto const &lhs, auto const &rhs) {
		return lhs.string_value.size() < rhs.string_value.size();
	});
	auto const size = context.create_minus(end_ptr, begin_ptr, context.get_isize());

	// store the case index in this variable
	auto const case_index_value = context.add_uninitialized_value(context.get_usize());
	if (case_infos.not_empty())
	{
		context.add_libc_header("string.h"); // memcpy, memcmp
		auto const size_prev_switch_info = context.begin_switch(size);

		size_t value_index = 0;
		while (value_index < case_infos.size())
		{
			auto const current_size = case_infos[value_index].string_value.size();
			context.add_case_label(bz::format("{}u", current_size));
			context.begin_case();

			// if the string is less than 8 bytes we copy them into an integer and do a switch on that,
			// otherwise we do an if-else chain (if chain with breaks in each case)

			// special case for size 0, because otherwise the source pointer in memcpy may be invalid, which is undefined behaviour
			if (current_size == 0)
			{
				bz_assert(value_index + 1 == case_infos.size() || case_infos[value_index + 1].string_value.size() != current_size);
				auto const case_index = case_infos[value_index].case_index;
				context.create_assignment(case_index_value, bz::format("{}u", case_index));
				++value_index;
			}
			else if (current_size <= 8)
			{
				auto const string_int_value = context.add_value_expression("0u", context.get_uint64());
				auto const size_value = context.get_unsigned_value(current_size, context.get_usize());
				generate_trivial_function_call(
					"memcpy",
					{ context.create_address_of(string_int_value), begin_ptr, size_value },
					context.get_void(),
					context,
					{}
				);

				auto const value_prev_switch_info = context.begin_switch(string_int_value);

				// this will do at least one iteration, so the outside loop will terminate
				for (; case_infos[value_index].string_value.size() == current_size; ++value_index)
				{
					auto const &[string_value, case_index] = case_infos[value_index];
					auto const case_string_int_value = get_string_int_value(string_value, context);
					context.add_case_label(bz::format("{}u", case_string_int_value));
					context.begin_case();
					context.create_assignment(case_index_value, bz::format("{}u", case_index));
					context.end_case();
				}

				// default case
				context.begin_default_case();
				context.create_assignment(case_index_value, bz::format("{}u", default_case_index));
				context.end_case();

				context.end_switch(value_prev_switch_info);
			}
			else
			{
				// this will do at least one iteration, so the outside loop will terminate
				for (; case_infos[value_index].string_value.size() == current_size; ++value_index)
				{
					auto const &[string_value, case_index] = case_infos[value_index];
					auto const prev_if_info = context.begin_if(are_strings_equal(begin_ptr, string_value, context));
					context.create_assignment(case_index_value, bz::format("{}u", case_index));
					context.add_expression("break");
					context.end_if(prev_if_info);
				}

				context.create_assignment(case_index_value, bz::format("{}u", default_case_index));
			}

			context.end_case();
		}

		context.end_switch(size_prev_switch_info);
	}
	else
	{
		context.create_assignment(case_index_value, bz::format("{}u", default_case_index));
	}


	auto const prev_switch_info = context.begin_switch(case_index_value);
	for (auto const case_index : bz::iota(0, switch_expr.cases.size()))
	{
		auto const &case_expr = switch_expr.cases[case_index].expr;
		context.add_case_label(bz::format("{}u", case_index));

		context.begin_case();
		auto const prev_info = context.push_expression_scope();
		auto const value = generate_expression(case_expr, context, result_dest);
		if (reference_result.has_value() && case_expr.get_expr_type().is_any_reference())
		{
			context.create_assignment(reference_result.get(), context.create_address_of(value));
		}
		context.pop_expression_scope(prev_info);
		context.end_case();
	}

	if (switch_expr.default_case.not_null())
	{
		context.begin_default_case();
		auto const prev_info = context.push_expression_scope();
		auto const value = generate_expression(switch_expr.default_case, context, result_dest);
		if (reference_result.has_value() && switch_expr.default_case.get_expr_type().is_any_reference())
		{
			context.create_assignment(reference_result.get(), context.create_address_of(value));
		}
		context.pop_expression_scope(prev_info);
		context.end_case();
	}
	context.end_switch(prev_switch_info);

	if (result_dest.has_value())
	{
		return result_dest.get();
	}
	else if (reference_result.has_value())
	{
		return context.create_dereference(reference_result.get());
	}
	else
	{
		return context.get_void_value();
	}
}

static expr_value generate_expression(
	lex::src_tokens const &src_tokens,
	ast::expr_switch const &switch_expr,
	codegen_context &context,
	bz::optional<expr_value> result_dest
)
{
	auto const matched_value_prev_info = context.push_expression_scope();
	auto const matched_value = generate_expression(switch_expr.matched_expr, context, {});
	auto const is_string = switch_expr.matched_expr.get_expr_type().is<ast::ts_base_type>()
		&& switch_expr.matched_expr.get_expr_type().get<ast::ts_base_type>().info->kind == ast::type_info::str_;

	if (is_string)
	{
		auto const begin_ptr = context.create_struct_gep_value(matched_value, 0);
		auto const end_ptr   = context.create_struct_gep_value(matched_value, 1);
		context.pop_expression_scope(matched_value_prev_info);

		return generate_string_switch(switch_expr, begin_ptr, end_ptr, context, result_dest);
	}
	else
	{
		context.pop_expression_scope(matched_value_prev_info);

		return generate_integral_switch(src_tokens, switch_expr, matched_value, context, result_dest);
	}
}

static expr_value generate_expression(
	ast::expr_break const &,
	codegen_context &context
)
{
	context.generate_loop_destruct_operations();
	if (auto const goto_index = context.get_break_goto_index(); goto_index.has_value())
	{
		context.add_expression(bz::format("goto {}", context.make_goto_label(goto_index.get())));
	}
	else
	{
		context.add_expression("break");
	}
	return context.get_void_value();
}

static expr_value generate_expression(
	ast::expr_continue const &,
	codegen_context &context
)
{
	context.generate_loop_destruct_operations();
	if (auto const goto_index = context.get_continue_goto_index(); goto_index.has_value())
	{
		context.add_expression(bz::format("goto {}", context.make_goto_label(goto_index.get())));
	}
	else
	{
		context.add_expression("continue");
	}
	return context.get_void_value();
}

static expr_value generate_expression(
	ast::expr_unreachable const &unreachable,
	codegen_context &context
)
{
	if (global_data::panic_on_unreachable)
	{
		generate_expression(unreachable.panic_fn_call, context, {});
	}
	else
	{
		context.create_unreachable();
	}
	return context.get_void_value();
}

static expr_value generate_expression(
	ast::expr_generic_type_instantiation const &,
	codegen_context &context
)
{
	// this is always a constant expression
	bz_unreachable;
}

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
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_function_name>(), context);
	case ast::expr_t::index<ast::expr_function_alias_name>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_function_alias_name>(), context);
	case ast::expr_t::index<ast::expr_function_overload_set>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_function_overload_set>(), context);
	case ast::expr_t::index<ast::expr_struct_name>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_struct_name>(), context);
	case ast::expr_t::index<ast::expr_enum_name>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_enum_name>(), context);
	case ast::expr_t::index<ast::expr_type_alias_name>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_type_alias_name>(), context);
	case ast::expr_t::index<ast::expr_integer_literal>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_integer_literal>(), context);
	case ast::expr_t::index<ast::expr_null_literal>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_null_literal>(), context);
	case ast::expr_t::index<ast::expr_enum_literal>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_enum_literal>(), context);
	case ast::expr_t::index<ast::expr_typed_literal>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_typed_literal>(), context);
	case ast::expr_t::index<ast::expr_placeholder_literal>:
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_placeholder_literal>(), context);
	case ast::expr_t::index<ast::expr_typename_literal>:
		bz_assert(!result_dest.has_value());
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
		return generate_expression(original_expr.src_tokens, expr.get<ast::expr_optional_extract_value>(), context, result_dest);
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
		return generate_expression(original_expr.src_tokens, expr.get<ast::expr_switch>(), context, result_dest);
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
		bz_assert(!result_dest.has_value());
		return generate_expression(expr.get<ast::expr_generic_type_instantiation>(), context);
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
	if (const_expr.kind == ast::expression_type_kind::lvalue)
	{
		return generate_expression(original_expr, const_expr.expr, context, result_dest);
	}
	else if (const_expr.kind == ast::expression_type_kind::none)
	{
		return context.get_void_value();
	}
	else if (const_expr.kind == ast::expression_type_kind::type_name)
	{
		return context.get_void_value();
	}

	auto value_string = generate_constant_value_string(const_expr.value, const_expr.type, context);

	if (const_expr.type.is_optional() && !const_expr.value.is_null_constant())
	{
		if (!result_dest.has_value())
		{
			result_dest = context.add_uninitialized_value(get_type(const_expr.type, context));
		}
		auto const &result_value = result_dest.get();
		context.create_assignment(get_optional_value(result_value, context), value_string);
		set_optional_has_value(result_value, true, context);
		return result_value;
	}
	else if (result_dest.has_value())
	{
		auto const &result_value = result_dest.get();
		context.create_assignment(result_value, value_string);
		return result_value;
	}
	else if (const_expr.type.is_optional_pointer_like() && const_expr.value.is_null_constant())
	{
		auto const expr_type = get_type(const_expr.type, context);
		bz_assert(value_string == "0");
		value_string = bz::format("({})0", context.to_string(expr_type));
		// this value is either a numeric literal or a compound literal, so we use the lower precedence from the two
		return context.add_temporary_expression(std::move(value_string), expr_type, false, false, true, precedence::prefix);
	}
	else
	{
		auto const expr_type = get_type(const_expr.type, context);
		// this value is either a numeric literal or a compound literal, so we use the lower precedence from the two
		return context.add_temporary_expression(std::move(value_string), expr_type, false, false, true, precedence::prefix);
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
		&& !dyn_expr.type.is_any_reference()
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

	if (dyn_expr.kind == ast::expression_type_kind::noreturn)
	{
		result_dest.clear();
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
	auto const prev_while_info = context.begin_while("1");

	// condition
	{
		auto const prev_info = context.push_expression_scope();
		auto const condition = generate_expression(while_stmt.condition, context, {});
		context.pop_expression_scope(prev_info);

		auto const prev_if_info = context.begin_if_not(condition);
		context.add_expression("break");
		context.end_if(prev_if_info);
	}

	// body
	{
		auto const prev_info = context.push_expression_scope();
		generate_expression(while_stmt.while_block, context, {});
		context.pop_expression_scope(prev_info);
	}

	context.end_while(prev_while_info);
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
	auto const prev_while_info = context.begin_while("1", true);

	// condition
	if (for_stmt.condition.not_null())
	{
		auto const prev_info = context.push_expression_scope();
		auto const condition = generate_expression(for_stmt.condition, context, {});
		context.pop_expression_scope(prev_info);

		auto const prev_if_info = context.begin_if_not(condition);
		context.add_expression("break");
		context.end_if(prev_if_info);
	}

	// body
	{
		auto const prev_info = context.push_expression_scope();
		generate_expression(for_stmt.for_block, context, {});
		context.pop_expression_scope(prev_info);
	}

	if (context.current_function_info.while_info.needs_continue_goto)
	{
		auto const index = context.current_function_info.while_info.continue_goto_index;
		context.add_expression(bz::format("{}:", context.make_goto_label(index)));
	}

	// iteration
	if (for_stmt.iteration.not_null())
	{
		auto const prev_info = context.push_expression_scope();
		generate_expression(for_stmt.iteration, context, {});
		context.pop_expression_scope(prev_info);
	}

	context.end_while(prev_while_info);
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
	auto const prev_while_info = context.begin_while("1", true);

	// condition
	{
		auto const prev_info = context.push_expression_scope();
		auto const condition = generate_expression(foreach_stmt.condition, context, {});
		context.pop_expression_scope(prev_info);

		auto const prev_if_info = context.begin_if_not(condition);
		context.add_expression("break");
		context.end_if(prev_if_info);
	}

	// body
	{
		auto const iter_prev_info = context.push_expression_scope();
		generate_statement(foreach_stmt.iter_deref_var_decl, context);
		generate_expression(foreach_stmt.for_block, context, {});
		context.pop_expression_scope(iter_prev_info);
	}

	if (context.current_function_info.while_info.needs_continue_goto)
	{
		auto const index = context.current_function_info.while_info.continue_goto_index;
		context.add_expression(bz::format("{}:", context.make_goto_label(index)));
	}

	// iteration
	{
		auto const prev_info = context.push_expression_scope();
		generate_expression(foreach_stmt.iteration, context, {});
		context.pop_expression_scope(prev_info);
	}

	context.end_while(prev_while_info);
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
	else if (context.current_function_info.func_body->return_type.is_any_reference())
	{
		auto const prev_info = context.push_expression_scope();
		auto const return_value_ref = generate_expression(return_stmt.expr, context, context.current_function_info.return_value);
		auto const return_value = context.create_address_of(return_value_ref);
		context.pop_expression_scope(prev_info);
		context.generate_all_destruct_operations();
		context.add_return(return_value);
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
	if (expr_stmt.expr.is<ast::expanded_variadic_expression>())
	{
		for (auto &expr : expr_stmt.expr.get<ast::expanded_variadic_expression>().exprs)
		{
			auto const prev_info = context.push_expression_scope();
			generate_expression(expr, context, {});
			context.pop_expression_scope(prev_info);
		}
	}
	else
	{
		auto const prev_info = context.push_expression_scope();
		generate_expression(expr_stmt.expr, context, {});
		context.pop_expression_scope(prev_info);
	}
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
		for (auto const &[elem_decl, i] : var_decl.tuple_decls.enumerate())
		{
			auto const elem_ref = elem_decl.get_type().is_any_reference()
				? context.create_dereference(context.create_struct_gep_value(value, i))
				: context.create_struct_gep(value, i);
			add_variable_helper(elem_decl, elem_ref, context);
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

		if (var_decl.tuple_decls.not_empty())
		{
			return;
		}

		auto const &info = context.get_global_variable(var_decl);
		auto value = context.add_temporary_expression(
			info.expr_string,
			info.var_type,
			!var_decl.get_type().is_mut(),
			true,
			false,
			precedence::suffix // info.expr_string is either an identifier, member access or subscript
		);
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
		if (var_decl.get_type().is_any_reference())
		{
			auto const prev_info = context.push_expression_scope();
			auto const ref_value = generate_expression(var_decl.init_expr, context, {});
			auto const init_value = context.create_address_of(ref_value);
			context.create_assignment(alloca, init_value);
			context.pop_expression_scope(prev_info);
			add_variable_helper(var_decl, context.create_dereference(alloca), context);
		}
		else
		{
			if (var_decl.init_expr.not_null())
			{
				auto const prev_info = context.push_expression_scope();
				generate_expression(var_decl.init_expr, context, alloca);
				context.pop_expression_scope(prev_info);
			}
			add_variable_helper(var_decl, alloca, context);
		}
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

static void generate_function_declaration(ast::function_body &func_body, codegen_context &context)
{
	bz_assert(func_body.body.is_null());
	bz_assert(!func_body.is_bitcode_emitted());
	func_body.flags |= ast::function_body::bitcode_emitted;

	if (func_body.is_libc_function() || func_body.is_intrinsic())
	{
		return;
	}

	auto const &func_name = context.get_function(func_body).name;
	auto const return_by_pointer = !func_body.return_type.is<ast::ts_void>()
		&& !func_body.return_type.is_any_reference()
		&& !ast::is_trivially_relocatable(func_body.return_type);
	auto const static_prefix = func_body.is_external_linkage() ? "" : "static ";

	auto const return_type = return_by_pointer ? context.get_void() : get_type(func_body.return_type, context);
	auto &decls_string = context.function_declarations_string;
	auto const return_type_string = context.to_string(return_type);
	decls_string += bz::format("{}{} {}(", static_prefix, return_type_string, func_name);

	if (return_by_pointer)
	{
		auto const return_value_type = get_type(func_body.return_type, context);
		auto const type_string = context.to_string(context.add_pointer(return_value_type));
		decls_string += type_string;
		decls_string += " restrict";
	}

	bool first = !return_by_pointer;
	for (auto const &param : func_body.params)
	{
		if (ast::is_generic_parameter(param))
		{
			continue;
		}
		// main can be only a declaration in freestanding mode, so this is still needed
		//
		// second parameter of main would be generated as 'unsigned char const * const *', which is invalid
		if (func_body.is_external_linkage() && func_body.symbol_name == "main" && !first)
		{
			decls_string += ", char const * const *";
			continue;
		}

		auto const param_type = get_type(param.get_type(), context);

		if (first)
		{
			first = false;
		}
		else
		{
			decls_string += ", ";
		}

		if (param.get_type().is_any_reference())
		{
			auto const param_type_string = context.to_string(param_type);
			decls_string += param_type_string;
		}
		else if (ast::is_trivially_relocatable(param.get_type()))
		{
			auto const param_type_string = context.to_string(param_type);
			decls_string += param_type_string;
		}
		else
		{
			auto const param_type_string = context.to_string(context.add_pointer(param_type));
			decls_string += param_type_string;
		}
	}

	// if no parameters were emitted, we need to add 'void'
	if (first)
	{
		decls_string += "void";
	}

	decls_string += ");\n";
}

static void generate_function(ast::function_body &func_body, codegen_context &context)
{
	bz_assert(func_body.body.not_null());
	bz_assert(!func_body.is_bitcode_emitted());
	func_body.flags |= ast::function_body::bitcode_emitted;
	context.reset_current_function(func_body);

	auto const &func_name = context.get_function(func_body).name;
	if (func_body.is_libc_function())
	{
		return;
	}

	auto const return_by_pointer = !func_body.return_type.is<ast::ts_void>()
		&& !func_body.return_type.is_any_reference()
		&& !ast::is_trivially_relocatable(func_body.return_type);
	auto const static_prefix = func_body.is_external_linkage() ? "" : "static ";

	auto const return_type = return_by_pointer ? context.get_void()
		: func_body.is_main() ? context.get_int32()
		: get_type(func_body.return_type, context);
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
		bodies_string += bz::format("{} restrict {}", type_string, name);
		decls_string += type_string;
		decls_string += " restrict";
		context.current_function_info.return_value = context.make_reference_expression(index, return_value_type, false);
	}

	bool first = !return_by_pointer;
	for (auto const &param : func_body.params)
	{
		if (ast::is_generic_parameter(param))
		{
			generate_statement(param, context);
			continue;
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

		if (param.get_type().is_any_reference())
		{
			auto const param_type_string = context.to_string(param_type);
			bodies_string += bz::format("{} {}", param_type_string, name);
			decls_string += param_type_string;
			auto const [ref_type, modifier] = context.remove_pointer(param_type);
			auto const var_value = context.make_reference_expression(index, ref_type, modifier == type_modifier::const_pointer);
			add_variable_helper(param, var_value, context);
		}
		else if (ast::is_trivially_relocatable(param.get_type()))
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

	if (func_body.is_main())
	{
		context.add_expression("return 0");
	}

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
		if (func_body->body.is_null())
		{
			generate_function_declaration(*func_body, context);
		}
		else
		{
			generate_function(*func_body, context);
		}
	}
}

static void generate_rvalue_array_destruct(
	ast::expression const &elem_destruct_expr,
	expr_value array_value,
	expr_value rvalue_array_elem_ptr_value,
	codegen_context &context
)
{
	bz_assert(context.is_array(array_value.get_type()));
	auto const array_type = context.maybe_get_array(array_value.get_type());
	auto const size = array_type->size;

	auto const begin_elem_ptr = context.create_struct_gep_pointer(array_value, 0);
	auto const end_elem_ptr = context.create_struct_gep_pointer(array_value, size);

	auto const it_value = context.create_trivial_copy(end_elem_ptr);
	auto const condition = context.create_not_equals(it_value, begin_elem_ptr);
	auto const prev_while_info = context.begin_while(condition);

	context.create_prefix_unary_operation(it_value, "--");

	auto const skip_elem = context.create_equals(it_value, rvalue_array_elem_ptr_value);
	auto const prev_if_info = context.begin_if(skip_elem);
	context.add_expression("continue");
	context.end_if(prev_if_info);

	auto const elem_value = context.create_dereference(it_value);
	auto const prev_value = context.push_value_reference(elem_value);
	generate_expression(elem_destruct_expr, context, {});
	context.pop_value_reference(prev_value);

	context.end_while(prev_while_info);
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
		codegen_context::if_info_t prev_if_info = {};
		if (condition.has_value())
		{
			prev_if_info = context.begin_if(condition.get());
		}

		auto const prev_info = context.push_expression_scope();
		generate_expression(*destruct_op.get<ast::destruct_variable>().destruct_call, context, {});
		context.pop_expression_scope(prev_info);

		if (condition.has_value())
		{
			context.end_if(prev_if_info);
		}
	}
	else if (destruct_op.is<ast::destruct_self>())
	{
		codegen_context::if_info_t prev_if_info = {};
		if (condition.has_value())
		{
			prev_if_info = context.begin_if(condition.get());
		}

		auto const &value = destruct_op_info.value;
		auto const prev_info = context.push_expression_scope();
		auto const prev_value = context.push_value_reference(value);
		generate_expression(*destruct_op.get<ast::destruct_self>().destruct_call, context, {});
		context.pop_value_reference(prev_value);
		context.pop_expression_scope(prev_info);

		if (condition.has_value())
		{
			context.end_if(prev_if_info);
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
		codegen_context::if_info_t prev_if_info = {};
		if (condition.has_value())
		{
			prev_if_info = context.begin_if(condition.get());
		}

		bz_assert(destruct_op_info.rvalue_array_elem_ptr.has_value());
		generate_rvalue_array_destruct(
			*destruct_op.get<ast::destruct_rvalue_array>().elem_destruct_call,
			destruct_op_info.value,
			destruct_op_info.rvalue_array_elem_ptr.get(),
			context
		);

		if (condition.has_value())
		{
			context.end_if(prev_if_info);
		}
	}
	else
	{
		static_assert(ast::destruct_operation::variant_count == 5);
	}

	if (move_destruct_indicator.has_value())
	{
		context.create_assignment(move_destruct_indicator.get(), "0");
	}
}

} // namespace codegen::c