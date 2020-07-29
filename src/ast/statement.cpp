#include "statement.h"
#include "token_info.h"

namespace ast
{

lex::token_pos decl_variable::get_tokens_begin(void) const
{ return this->tokens.begin; }

lex::token_pos decl_variable::get_tokens_pivot(void) const
{ return this->identifier; }

lex::token_pos decl_variable::get_tokens_end(void) const
{ return this->tokens.end; }

bz::u8string function_body::get_symbol_name(void) const
{
	bz_assert(this->function_name.size() != 0);
	auto const first_char = *this->function_name.begin();
	auto const is_op = first_char >= '1' && first_char <= '9';
	auto symbol_name = bz::format("{}.{}", is_op ? "op" : "func", this->function_name);
	symbol_name += bz::format("..{}", this->params.size());
	for (auto &p : this->params)
	{
		symbol_name += '.';
		symbol_name += p.var_type.get_symbol_name();
	}
	symbol_name += '.';
	symbol_name += this->return_type.get_symbol_name();
	return symbol_name;
}

bz::u8string function_body::decode_symbol_name(
	bz::u8string_view::const_iterator &it,
	bz::u8string_view::const_iterator end
)
{
	constexpr bz::u8string_view function_ = "func.";
	constexpr bz::u8string_view operator_ = "op.";

	auto const parse_int = [](bz::u8string_view str) {
		uint64_t result = 0;
		for (auto const c : str)
		{
			bz_assert(c >= '0' && c <= '9');
			result *= 10;
			result += c - '0';
		}
		return result;
	};

	bz::u8string result = "";

	auto const symbol_name = bz::u8string_view(it, end);
	if (symbol_name.starts_with(operator_))
	{
		auto const op_begin = bz::u8string_view::const_iterator(it.data() + operator_.size());
		result += "operator ";
		auto const op_end = symbol_name.find(op_begin, "..");
		auto const op_kind = parse_int(bz::u8string_view(op_begin, op_end));
		result += token_info[op_kind].token_value;
		result += " (";
		it = bz::u8string_view::const_iterator(op_end.data() + 2);
	}
	else
	{
		bz_assert(symbol_name.starts_with(function_));
		auto const name_begin = bz::u8string_view::const_iterator(it.data() + function_.size());
		result += "function ";
		auto const name_end = symbol_name.find(name_begin, "..");
		result += bz::u8string_view(name_begin, name_end);
		result += '(';
		it = bz::u8string_view::const_iterator(name_end.data() + 2);
	}

	auto const param_count_begin = it;
	auto const param_count_end = symbol_name.find(param_count_begin, '.');
	auto const param_count = parse_int(bz::u8string_view(param_count_begin, param_count_end));
	it = param_count_end;

	for (uint64_t i = 0; i < param_count; ++i)
	{
		bz_assert(it != end && *it == '.');
		++it;
		if (i == 0)
		{
			result += ": ";
			result += typespec::decode_symbol_name(it, end);
		}
		else
		{
			result += ", : ";
			result += typespec::decode_symbol_name(it, end);
		}
	}

	bz_assert(it != end && *it == '.');
	++it;

	result += ") -> ";
	result += typespec::decode_symbol_name(it, end);

	return result;
}

} // namespace ast
