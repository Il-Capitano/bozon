#include "constant_value.h"
#include "escape_sequences.h"
#include "statement.h"

namespace ast
{

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

bz::u8string get_value_string(constant_value const &value)
{
	switch (value.kind())
	{
	case constant_value::sint:
		return bz::format("{}", value.get<constant_value::sint>());
	case constant_value::uint:
		return bz::format("{}", value.get<constant_value::uint>());
	case constant_value::float32:
		return bz::format("{}", value.get<constant_value::float32>());
	case constant_value::float64:
		return bz::format("{}", value.get<constant_value::float64>());
	case constant_value::u8char:
		return bz::format("'{}'", add_escape_sequences(value.get<constant_value::u8char>()));
	case constant_value::string:
		return bz::format("\"{}\"", add_escape_sequences(value.get<constant_value::string>()));
	case constant_value::boolean:
		return bz::format("{}", value.get<constant_value::boolean>());
	case constant_value::null:
		return "null";
	case constant_value::array:
		return get_aggregate_like_value_string(value.get<constant_value::array>());
	case constant_value::tuple:
		return get_aggregate_like_value_string(value.get<constant_value::tuple>());
	case constant_value::function:
		return "";
	case constant_value::unqualified_function_set_id:
	{
		bz::u8string result;
		bool first = true;
		for (auto const id : value.get<constant_value::qualified_function_set_id>().id)
		{
			if (first)
			{
				first = false;
			}
			else
			{
				result += "::";
			}
			result += id;
		}
		return result;
	}
	case constant_value::qualified_function_set_id:
	{
		bz::u8string result;
		for (auto const id : value.get<constant_value::qualified_function_set_id>().id)
		{
			result += "::";
			result += id;
		}
		return result;
	}
	case constant_value::type:
		return bz::format("{}", value.get<constant_value::type>());
	case constant_value::aggregate:
		return get_aggregate_like_value_string(value.get<constant_value::aggregate>());
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

void constant_value::encode_for_symbol_name(bz::u8string &out, constant_value const &value)
{
	switch (value.kind())
	{
	case sint:
		out += 'i';
		out += bz::format("{}", bit_cast<uint64_t>(value.get<sint>()));
		break;
	case uint:
		out += 'u';
		out += bz::format("{}", value.get<uint>());
		break;
	case float32:
		out += 'f';
		out += bz::format("{:8x}", bit_cast<uint32_t>(value.get<float32>()));
		break;
	case float64:
		out += 'd';
		out += bz::format("{:16x}", bit_cast<uint64_t>(value.get<float64>()));
		break;
	case u8char:
		out += 'c';
		out += bz::format("{}", bit_cast<uint32_t>(value.get<u8char>()));
		break;
	case string:
	{
		auto const str = value.get<string>().as_string_view();
		out += 's';
		out += bz::format("{}", str.size());
		out += '.';
		out += str;
		break;
	}
	case boolean:
		out += 'b';
		out += value.get<boolean>() ? '1' : '0';
		break;
	case null:
		out += 'n';
		break;
	case void_:
		out += 'v';
		break;
	case array:
		out += 'A';
		encode_array_like(out, value.get<array>());
		break;
	case tuple:
		out += 'T';
		encode_array_like(out, value.get<tuple>());
		break;
	case function:
	{
		auto const func = value.get<function>();
		auto const func_symbol = func->body.symbol_name.as_string_view();
		out += 'F';
		out += bz::format("{}", func_symbol.size());
		out += '.';
		out += func_symbol;
		break;
	}
	case unqualified_function_set_id:
	case qualified_function_set_id:
		bz_unreachable;
	case type:
	{
		auto const symbol = value.get<type>().get_symbol_name();
		out += 't';
		out += bz::format("{}", symbol.size());
		out += '.';
		out += symbol;
		break;
	}
	case aggregate:
		out += 'a';
		encode_array_like(out, value.get<aggregate>());
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

bz::u8string constant_value::decode_from_symbol_name(bz::u8string_view::const_iterator &it, bz::u8string_view::const_iterator end)
{
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
		return "void";
	case 'A':
	{
		++it;
		bz::u8string result;
		decode_array_like(it, end, result);
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
	case 'U':
	case 'Q':
		bz_unreachable;
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
	case constant_value::sint:
		return lhs.get<constant_value::sint>() == rhs.get<constant_value::sint>();
	case constant_value::uint:
		return lhs.get<constant_value::uint>() == rhs.get<constant_value::uint>();
	case constant_value::float32:
		return lhs.get<constant_value::float32>() == rhs.get<constant_value::float32>();
	case constant_value::float64:
		return lhs.get<constant_value::float64>() == rhs.get<constant_value::float64>();
	case constant_value::u8char:
		return lhs.get<constant_value::u8char>() == rhs.get<constant_value::u8char>();
	case constant_value::string:
		return lhs.get<constant_value::string>() == rhs.get<constant_value::string>();
	case constant_value::boolean:
		return lhs.get<constant_value::boolean>() == rhs.get<constant_value::boolean>();
	case constant_value::null:
		return true;
	case constant_value::array:
		return lhs.get<constant_value::array>() == rhs.get<constant_value::array>();
	case constant_value::tuple:
		return lhs.get<constant_value::tuple>() == rhs.get<constant_value::tuple>();
	case constant_value::function:
		return lhs.get<constant_value::function>() == rhs.get<constant_value::function>();
	case constant_value::unqualified_function_set_id:
	{
		auto const &[lhs_id, lhs_bodies] = lhs.get<constant_value::unqualified_function_set_id>();
		auto const &[rhs_id, rhs_bodies] = rhs.get<constant_value::unqualified_function_set_id>();
		if (lhs_bodies.empty() && rhs_bodies.empty())
		{
			return lhs_id == rhs_id;
		}
		else if (lhs_bodies.not_empty() && rhs_bodies.not_empty())
		{
			return lhs_id == rhs_id && lhs_bodies == rhs_bodies;
		}
		else
		{
			return false;
		}
	}
	case constant_value::qualified_function_set_id:
	{
		auto const &[lhs_id, lhs_bodies] = lhs.get<constant_value::qualified_function_set_id>();
		auto const &[rhs_id, rhs_bodies] = rhs.get<constant_value::qualified_function_set_id>();
		if (lhs_bodies.empty() && rhs_bodies.empty())
		{
			return lhs_id == rhs_id;
		}
		else if (lhs_bodies.not_empty() && rhs_bodies.not_empty())
		{
			return lhs_id == rhs_id && lhs_bodies == rhs_bodies;
		}
		else
		{
			return false;
		}
	}
	case constant_value::type:
		return lhs.get<constant_value::type>() == rhs.get<constant_value::type>();
	case constant_value::aggregate:
		return lhs.get<constant_value::aggregate>() == rhs.get<constant_value::aggregate>();
	default:
		return false;
	}
}

bool operator != (constant_value const &lhs, constant_value const &rhs) noexcept
{
	return !(lhs == rhs);
}

} // namespace ast
