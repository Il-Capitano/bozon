#include "constant_value.h"
#include "escape_sequences.h"

namespace ast
{

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
	case constant_value::tuple:
	case constant_value::function:
	case constant_value::unqualified_function_set_id:
	case constant_value::qualified_function_set_id:
		return "";
	case constant_value::type:
		return bz::format("{}", value.get<constant_value::type>());
	case constant_value::aggregate:
	default:
		return "";
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
		return lhs.get<constant_value::unqualified_function_set_id>() == rhs.get<constant_value::unqualified_function_set_id>();
	case constant_value::qualified_function_set_id:
		return lhs.get<constant_value::qualified_function_set_id>() == rhs.get<constant_value::qualified_function_set_id>();
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
