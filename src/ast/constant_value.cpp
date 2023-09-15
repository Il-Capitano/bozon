#include "constant_value.h"
#include "escape_sequences.h"
#include "statement.h"

namespace ast
{

static_assert(std::is_trivially_copy_constructible_v<constant_value_base_t>);

static_assert(constant_value::variant_count == 19);
static_assert(static_cast<uint64_t>(constant_value_kind::sint) == constant_value_base_t::index_of<int64_t>);
static_assert(static_cast<uint64_t>(constant_value_kind::uint) == constant_value_base_t::index_of<uint64_t>);
static_assert(static_cast<uint64_t>(constant_value_kind::float32) == constant_value_base_t::index_of<float32_t>);
static_assert(static_cast<uint64_t>(constant_value_kind::float64) == constant_value_base_t::index_of<float64_t>);
static_assert(static_cast<uint64_t>(constant_value_kind::u8char) == constant_value_base_t::index_of<bz::u8char>);
static_assert(static_cast<uint64_t>(constant_value_kind::string) == constant_value_base_t::index_of<bz::u8string_view>);
static_assert(static_cast<uint64_t>(constant_value_kind::boolean) == constant_value_base_t::index_of<bool>);
static_assert(static_cast<uint64_t>(constant_value_kind::null) == constant_value_base_t::index_of<internal::null_t>);
static_assert(static_cast<uint64_t>(constant_value_kind::void_) == constant_value_base_t::index_of<internal::void_t>);
static_assert(static_cast<uint64_t>(constant_value_kind::enum_) == constant_value_base_t::index_of<internal::enum_t>);
// static_assert(static_cast<uint64_t>(constant_value_kind::array) == constant_value_base_t::index_of<bz::array_view<constant_value const>>);
static_assert(static_cast<uint64_t>(constant_value_kind::sint_array) == constant_value_base_t::index_of<bz::array_view<int64_t const>>);
static_assert(static_cast<uint64_t>(constant_value_kind::uint_array) == constant_value_base_t::index_of<bz::array_view<uint64_t const>>);
static_assert(static_cast<uint64_t>(constant_value_kind::float32_array) == constant_value_base_t::index_of<bz::array_view<float32_t const>>);
static_assert(static_cast<uint64_t>(constant_value_kind::float64_array) == constant_value_base_t::index_of<bz::array_view<float64_t const>>);
// static_assert(static_cast<uint64_t>(constant_value_kind::tuple) == constant_value_base_t::index_of<bz::array_view<constant_value const>>);
static_assert(static_cast<uint64_t>(constant_value_kind::function) == constant_value_base_t::index_of<function_body *>);
static_assert(static_cast<uint64_t>(constant_value_kind::type) == constant_value_base_t::index_of<typespec_view>);
// static_assert(static_cast<uint64_t>(constant_value_kind::aggregate) == constant_value_base_t::index_of<bz::array_view<constant_value const>>);
static_assert(bz::meta::is_same<constant_value_base_t::value_type<static_cast<uint64_t>(constant_value_kind::array)>, bz::array_view<constant_value const>>);
static_assert(bz::meta::is_same<constant_value_base_t::value_type<static_cast<uint64_t>(constant_value_kind::tuple)>, bz::array_view<constant_value const>>);
static_assert(bz::meta::is_same<constant_value_base_t::value_type<static_cast<uint64_t>(constant_value_kind::aggregate)>, bz::array_view<constant_value const>>);
static_assert(
	constant_value_kind::array != constant_value_kind::tuple
	&& constant_value_kind::array != constant_value_kind::aggregate
	&& constant_value_kind::tuple != constant_value_kind::aggregate
);

static bz::u8string get_aggregate_like_value_string(bz::array_view<constant_value const> values)
{
	if (values.empty())
	{
		return "[]";
	}
	bz::u8string result = "[";
	bool first = true;
	for (auto const &value : values)
	{
		if (first)
		{
			first = false;
			result += ' ';
			result += get_value_string(value);
		}
		else
		{
			result += ", ";
			result += get_value_string(value);
		}
	}
	result += " ]";
	return result;
}

template<typename T>
static bz::u8string get_aggregate_like_value_string(bz::array_view<T const> values)
{
	if (values.empty())
	{
		return "[]";
	}
	bz::u8string result = "[";
	bool first = true;
	for (auto const &value : values)
	{
		if (first)
		{
			first = false;
			result += ' ';
			result += bz::format("{}", value);
		}
		else
		{
			result += ", ";
			result += bz::format("{}", value);
		}
	}
	result += " ]";
	return result;
}

bz::u8string get_value_string(constant_value const &value)
{
	switch (value.kind())
	{
	static_assert(constant_value::variant_count == 19);
	case constant_value_kind::sint:
		return bz::format("{}", value.get_sint());
	case constant_value_kind::uint:
		return bz::format("{}", value.get_uint());
	case constant_value_kind::float32:
		return bz::format("{}", value.get_float32());
	case constant_value_kind::float64:
		return bz::format("{}", value.get_float64());
	case constant_value_kind::u8char:
		return bz::format("'{}'", add_escape_sequences(value.get_u8char()));
	case constant_value_kind::string:
		return bz::format("\"{}\"", add_escape_sequences(value.get_string()));
	case constant_value_kind::boolean:
		return bz::format("{}", value.get_boolean());
	case constant_value_kind::null:
		return "null";
	case constant_value_kind::void_:
		return "void()";
	case constant_value_kind::enum_:
	{
		auto const [decl, enum_value] = value.get_enum();
		auto const value_name = decl->get_value_name(enum_value);
		auto const type_name = decl->id.format_as_unqualified();
		if (value_name != "")
		{
			return bz::format("{}.{}", type_name, value_name);
		}
		else
		{
			auto const kind = decl->underlying_type.get<ast::ts_base_type>().info->kind;
			if (ast::is_signed_integer_kind(kind))
			{
				return bz::format("{}({})", type_name, bit_cast<int64_t>(enum_value));
			}
			else
			{
				return bz::format("{}({})", type_name, enum_value);
			}
		}
	}
	case constant_value_kind::array:
		return get_aggregate_like_value_string(value.get_array());
	case constant_value_kind::sint_array:
		return get_aggregate_like_value_string(value.get_sint_array());
	case constant_value_kind::uint_array:
		return get_aggregate_like_value_string(value.get_uint_array());
	case constant_value_kind::float32_array:
		return get_aggregate_like_value_string(value.get_float32_array());
	case constant_value_kind::float64_array:
		return get_aggregate_like_value_string(value.get_float64_array());
	case constant_value_kind::tuple:
		return get_aggregate_like_value_string(value.get_tuple());
	case constant_value_kind::function:
		return "";
	case constant_value_kind::type:
		return bz::format("{}", value.get_type());
	case constant_value_kind::aggregate:
		return get_aggregate_like_value_string(value.get_aggregate());
	default:
		return "";
	}
}

static void encode_array_like(bz::u8string &out, bz::array_view<constant_value const> values)
{
	out += bz::format("{}", values.size());
	for (auto const &value : values)
	{
		out += '.';
		constant_value::encode_for_symbol_name(out, value);
	}
}

static void encode_array_like(bz::u8string &out, bz::array_view<int64_t const> values)
{
	out += bz::format("{}", values.size());
	for (auto const &value : values)
	{
		out += '.';
		out += bz::format("{}", bit_cast<uint64_t>(value));
	}
}

static void encode_array_like(bz::u8string &out, bz::array_view<uint64_t const> values)
{
	out += bz::format("{}", values.size());
	for (auto const &value : values)
	{
		out += '.';
		out += bz::format("{}", value);
	}
}

static void encode_array_like(bz::u8string &out, bz::array_view<float32_t const> values)
{
	out += bz::format("{}", values.size());
	for (auto const &value : values)
	{
		out += '.';
		out += bz::format("{:08x}", bit_cast<uint32_t>(value));
	}
}

static void encode_array_like(bz::u8string &out, bz::array_view<float64_t const> values)
{
	out += bz::format("{}", values.size());
	for (auto const &value : values)
	{
		out += '.';
		out += bz::format("{:016x}", bit_cast<uint64_t>(value));
	}
}

void constant_value::encode_for_symbol_name(bz::u8string &out, constant_value const &value)
{
	switch (value.kind())
	{
	static_assert(constant_value::variant_count == 19);
	case constant_value_kind::sint:
		out += 'i';
		out += bz::format("{}", bit_cast<uint64_t>(value.get_sint()));
		break;
	case constant_value_kind::uint:
		out += 'u';
		out += bz::format("{}", value.get_uint());
		break;
	case constant_value_kind::float32:
		out += 'f';
		out += bz::format("{:08x}", bit_cast<uint32_t>(value.get_float32()));
		break;
	case constant_value_kind::float64:
		out += 'd';
		out += bz::format("{:016x}", bit_cast<uint64_t>(value.get_float64()));
		break;
	case constant_value_kind::u8char:
		out += 'c';
		out += bz::format("{}", bit_cast<uint32_t>(value.get_u8char()));
		break;
	case constant_value_kind::string:
	{
		auto const str = value.get_string();
		out += 's';
		out += bz::format("{}", str.size());
		out += '.';
		out += str;
		break;
	}
	case constant_value_kind::boolean:
		out += 'b';
		out += value.get_boolean() ? '1' : '0';
		break;
	case constant_value_kind::null:
		out += 'n';
		break;
	case constant_value_kind::void_:
		out += 'v';
		break;
	case constant_value_kind::enum_:
	{
		auto const [decl, enum_value] = value.get_enum();
		out += 'e';
		out += decl->id.format_as_unqualified();
		out += '.';
		auto const value_name = decl->get_value_name(enum_value);
		if (value_name != "")
		{
			out += bz::format("{}.{}", value_name.size(), value_name);
		}
		else
		{
			auto const is_signed = is_signed_integer_kind(decl->underlying_type.get<ast::ts_base_type>().info->kind);
			if (is_signed)
			{
				out += bz::format("0{}", enum_value);
			}
			else
			{
				out += bz::format("{}", enum_value);
			}
		}
		break;
	}
	case constant_value_kind::array:
		out += 'A';
		encode_array_like(out, value.get_array());
		break;
	case constant_value_kind::sint_array:
		out += 'I';
		encode_array_like(out, value.get_sint_array());
		break;
	case constant_value_kind::uint_array:
		out += 'U';
		encode_array_like(out, value.get_uint_array());
		break;
	case constant_value_kind::float32_array:
		out += 'G';
		encode_array_like(out, value.get_float32_array());
		break;
	case constant_value_kind::float64_array:
		out += 'D';
		encode_array_like(out, value.get_float64_array());
		break;
	case constant_value_kind::tuple:
		out += 'T';
		encode_array_like(out, value.get_tuple());
		break;
	case constant_value_kind::function:
	{
		auto const body = value.get_function();
		auto const func_symbol = body->symbol_name.as_string_view();
		out += 'F';
		out += bz::format("{}", func_symbol.size());
		out += '.';
		out += func_symbol;
		break;
	}
	case constant_value_kind::type:
	{
		auto const symbol = value.get_type().get_symbol_name();
		out += 't';
		out += bz::format("{}", symbol.size());
		out += '.';
		out += symbol;
		break;
	}
	case constant_value_kind::aggregate:
		out += 'a';
		encode_array_like(out, value.get_aggregate());
		break;
	default:
		bz_unreachable;
	}
}

template<typename Int>
static Int parse_int(bz::u8string_view::const_iterator &it, bz::u8string_view::const_iterator end)
{
	Int result = 0;
	for (; it != end; ++it)
	{
		auto const c = *it;
		if (c < '0' || c > '9')
		{
			break;
		}
		result *= 10;
		result += c - '0';
	}
	return result;
}

template<typename Int>
static Int parse_hex(bz::u8string_view::const_iterator &it, bz::u8string_view::const_iterator end)
{
	Int result = 0;
	for (; it != end; ++it)
	{
		auto const c = *it;
		if ((c < '0' || c > '9') && (c < 'a' || c > 'f'))
		{
			break;
		}
		result *= 16;
		if (c >= '0' && c <= '9')
		{
			result += c - '0';
		}
		else
		{
			result += c - 'a' + 10;
		}
	}
	return result;
}

static void decode_array_like(
	bz::u8string_view::const_iterator &it,
	bz::u8string_view::const_iterator end,
	bz::u8string &out
)
{
	auto const size = parse_int<size_t>(it, end);
	out += "[ ";
	for (auto const i : bz::iota(0, size))
	{
		if (i != 0)
		{
			out += ", ";
		}
		bz_assert(*it == '.');
		++it;
		out += constant_value::decode_from_symbol_name(it, end);
	}
	out += " ]";
}

static void decode_sint_array(
	bz::u8string_view::const_iterator &it,
	bz::u8string_view::const_iterator end,
	bz::u8string &out
)
{
	auto const size = parse_int<size_t>(it, end);
	out += "[ ";
	for (auto const i : bz::iota(0, size))
	{
		if (i != 0)
		{
			out += ", ";
		}
		bz_assert(*it == '.');
		++it;
		out += bz::format("{}", bit_cast<int64_t>(parse_int<uint64_t>(it, end)));
	}
	out += " ]";
}

static void decode_uint_array(
	bz::u8string_view::const_iterator &it,
	bz::u8string_view::const_iterator end,
	bz::u8string &out
)
{
	auto const size = parse_int<size_t>(it, end);
	out += "[ ";
	for (auto const i : bz::iota(0, size))
	{
		if (i != 0)
		{
			out += ", ";
		}
		bz_assert(*it == '.');
		++it;
		out += bz::format("{}", parse_int<uint64_t>(it, end));
	}
	out += " ]";
}

static void decode_float32_array(
	bz::u8string_view::const_iterator &it,
	bz::u8string_view::const_iterator end,
	bz::u8string &out
)
{
	auto const size = parse_int<size_t>(it, end);
	out += "[ ";
	for (auto const i : bz::iota(0, size))
	{
		if (i != 0)
		{
			out += ", ";
		}
		bz_assert(*it == '.');
		++it;
		out += bz::format("{}", bit_cast<float32_t>(parse_hex<uint32_t>(it, end)));
	}
	out += " ]";
}

static void decode_float64_array(
	bz::u8string_view::const_iterator &it,
	bz::u8string_view::const_iterator end,
	bz::u8string &out
)
{
	auto const size = parse_int<size_t>(it, end);
	out += "[ ";
	for (auto const i : bz::iota(0, size))
	{
		if (i != 0)
		{
			out += ", ";
		}
		bz_assert(*it == '.');
		++it;
		out += bz::format("{}", bit_cast<float64_t>(parse_hex<uint64_t>(it, end)));
	}
	out += " ]";
}

bz::u8string constant_value::decode_from_symbol_name(bz::u8string_view::const_iterator &it, bz::u8string_view::const_iterator end)
{
	static_assert(constant_value::variant_count == 19);
	switch (*it)
	{
	case 'i':
		++it;
		return bz::format("{}", bit_cast<int64_t>(parse_int<uint64_t>(it, end)));
	case 'u':
		++it;
		return bz::format("{}", parse_int<uint64_t>(it, end));
	case 'f':
		++it;
		return bz::format("{}", bit_cast<float32_t>(parse_hex<uint32_t>(it, end)));
	case 'd':
		++it;
		return bz::format("{}", bit_cast<float64_t>(parse_hex<uint64_t>(it, end)));
	case 'c':
		++it;
		return bz::format("'{:c}'", parse_int<bz::u8char>(it, end));
	case 's':
	{
		++it;
		auto const size = parse_int<size_t>(it, end);
		bz_assert(it != end);
		bz_assert(*it == '.');
		++it;
		auto const str_end = bz::u8string_view::const_iterator(it.data() + size);
		auto const result = bz::u8string_view(it, str_end);
		it = str_end;
		return result;
	}
	case 'b':
	{
		++it;
		auto const c = *it;
		++it;
		return c == '1' ? "true" : "false";
	}
	case 'n':
		return "null";
	case 'v':
		return "void()";
	case 'e':
	{
		++it;
		auto const dot = bz::u8string_view(it, end).find('.');
		auto const type_name = bz::u8string_view(it, dot);
		it = dot + 1;
		auto const c = *it;
		if (c >= '0' && c <= '9')
		{
			auto const value = parse_int<uint64_t>(it, end);
			if (c == '0')
			{
				return bz::format("{}({})", type_name, bit_cast<int64_t>(value));
			}
			else
			{
				return bz::format("{}({})", type_name, value);
			}
		}
		else
		{
			auto const value_name_size = parse_int<size_t>(it, end);
			auto const value_name_end_it = bz::u8string_view::const_iterator(it.data() + value_name_size);
			auto const value_name = bz::u8string_view(it, value_name_end_it);
			it = value_name_end_it;
			return bz::format("{}.{}", type_name, value_name);
		}
	}
	case 'A':
	{
		++it;
		bz::u8string result;
		decode_array_like(it, end, result);
		return result;
	}
	case 'I':
	{
		++it;
		bz::u8string result;
		decode_sint_array(it, end, result);
		return result;
	}
	case 'U':
	{
		++it;
		bz::u8string result;
		decode_uint_array(it, end, result);
		return result;
	}
	case 'G':
	{
		++it;
		bz::u8string result;
		decode_float32_array(it, end, result);
		return result;
	}
	case 'D':
	{
		++it;
		bz::u8string result;
		decode_float64_array(it, end, result);
		return result;
	}
	case 'T':
	{
		++it;
		bz::u8string result;
		decode_array_like(it, end, result);
		return result;
	}
	case 'F':
	{
		++it;
		auto const size = parse_int<size_t>(it, end);
		bz_assert(it != end);
		bz_assert(*it == '.');
		auto const str_end = bz::u8string_view::const_iterator(it.data() + size);
		auto const result_symbol = bz::u8string_view(it, str_end);
		it = str_end;
		return ast::function_body::decode_symbol_name(result_symbol);
	}
	case 't':
	{
		++it;
		auto const size = parse_int<size_t>(it, end);
		bz_assert(it != end);
		bz_assert(*it == '.');
		auto const str_end = bz::u8string_view::const_iterator(it.data() + size);
		auto const result_symbol = bz::u8string_view(it, str_end);
		it = str_end;
		return ast::typespec::decode_symbol_name(result_symbol);
	}
	case 'a':
	{
		++it;
		bz::u8string result;
		decode_array_like(it, end, result);
		return result;
	}
	default:
		bz_unreachable;
	}
}

bool operator == (constant_value const &lhs, constant_value const &rhs) noexcept
{
	if (lhs.kind() != rhs.kind())
	{
		return false;
	}
	switch (lhs.kind())
	{
	static_assert(constant_value::variant_count == 19);
	case constant_value_kind::sint:
		return lhs.get_sint() == rhs.get_sint();
	case constant_value_kind::uint:
		return lhs.get_uint() == rhs.get_uint();
	case constant_value_kind::float32:
		return lhs.get_float32() == rhs.get_float32();
	case constant_value_kind::float64:
		return lhs.get_float64() == rhs.get_float64();
	case constant_value_kind::u8char:
		return lhs.get_u8char() == rhs.get_u8char();
	case constant_value_kind::string:
		return lhs.get_string() == rhs.get_string();
	case constant_value_kind::boolean:
		return lhs.get_boolean() == rhs.get_boolean();
	case constant_value_kind::null:
		return true;
	case constant_value_kind::void_:
		return true;
	case constant_value_kind::enum_:
	{
		auto const [lhs_decl, lhs_value] = lhs.get_enum();
		auto const [rhs_decl, rhs_value] = rhs.get_enum();
		return lhs_decl == rhs_decl && lhs_value == rhs_value;
	}
	case constant_value_kind::array:
		return lhs.get_array() == rhs.get_array();
	case constant_value_kind::sint_array:
		return lhs.get_sint_array() == rhs.get_sint_array();
	case constant_value_kind::uint_array:
		return lhs.get_uint_array() == rhs.get_uint_array();
	case constant_value_kind::float32_array:
		return lhs.get_float32_array() == rhs.get_float32_array();
	case constant_value_kind::float64_array:
		return lhs.get_float64_array() == rhs.get_float64_array();
	case constant_value_kind::tuple:
		return lhs.get_tuple() == rhs.get_tuple();
	case constant_value_kind::function:
		return lhs.get_function() == rhs.get_function();
	case constant_value_kind::type:
		return lhs.get_type() == rhs.get_type();
	case constant_value_kind::aggregate:
		return lhs.get_aggregate() == rhs.get_aggregate();
	default:
		return false;
	}
}

bool operator != (constant_value const &lhs, constant_value const &rhs) noexcept
{
	return !(lhs == rhs);
}

} // namespace ast
