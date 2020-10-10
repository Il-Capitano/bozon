#include "constant_value.h"
#include "escape_sequences.h"

namespace ast
{

bz::u8string get_value_string(constant_value const &value)
{
	switch (value.kind())
	{
	case ast::constant_value::sint:
		return bz::format("{}", value.get<ast::constant_value::sint>());
	case ast::constant_value::uint:
		return bz::format("{}", value.get<ast::constant_value::uint>());
	case ast::constant_value::float32:
		return bz::format("{}", value.get<ast::constant_value::float32>());
	case ast::constant_value::float64:
		return bz::format("{}", value.get<ast::constant_value::float64>());
	case ast::constant_value::u8char:
		return bz::format("'{}'", add_escape_sequences(value.get<ast::constant_value::u8char>()));
	case ast::constant_value::string:
		return bz::format("\"{}\"", add_escape_sequences(value.get<ast::constant_value::string>()));
	case ast::constant_value::boolean:
		return bz::format("{}", value.get<ast::constant_value::boolean>());
	case ast::constant_value::null:
		return "null";
	case ast::constant_value::array:
	case ast::constant_value::tuple:
	case ast::constant_value::function:
	case ast::constant_value::function_set_id:
		return "";
	case ast::constant_value::type:
		return bz::format("{}", value.get<ast::constant_value::type>());
	case ast::constant_value::aggregate:
	default:
		return "";
	}
}

} // namespace ast
