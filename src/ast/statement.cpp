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

bz::u8string function_body::get_signature(void) const
{
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

	auto const first_char = *this->function_name.begin();
	auto const is_op = first_char >= '1' && first_char <= '9';
	bz::u8string result = "";

	if (is_op)
	{
		result += "operator ";
		auto const kind = parse_int(this->function_name);
		if (kind == lex::token::paren_open)
		{
			result += "() (";
		}
		else if (kind == lex::token::square_open)
		{
			result += "[] (";
		}
		else
		{
			result += token_info[kind].token_value;
			result += " (";
		}
	}
	else
	{
		result += "function ";
		result += this->function_name;
		result += '(';
	}
	bool first = true;
	for (auto &p : this->params)
	{
		if (first)
		{
			first = false;
			result += bz::format(": {}", p.var_type);
		}
		else
		{
			result += bz::format(", : {}", p.var_type);
		}
	}
	result += ") -> ";
	result += bz::format("{}", this->return_type);
	return result;
}

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

std::unique_ptr<function_body> function_body::get_copy_for_generic_specialization(bz::vector<std::pair<lex::src_tokens, function_body *>> required_from)
{
	auto result = std::make_unique<function_body>(*this);
	result->flags &= ~generic;
	result->flags |= generic_specialization;
	result->generic_required_from.append(std::move(required_from));
	return result;
}

function_body *function_body::add_specialized_body(std::unique_ptr<function_body> body)
{
	bz_assert(body->is_generic_specialization());
	bz_assert(body->params.size() == this->params.size());
	auto const is_equal_params = [](auto const &lhs, auto const &rhs) {
		for (auto const &[lhs_param, rhs_param] : bz::zip(lhs, rhs))
		{
			if (lhs_param.var_type != rhs_param.var_type)
			{
				return false;
			}
		}
		return true;
	};
	auto const it = std::find_if(
		this->generic_specializations.begin(), this->generic_specializations.end(),
		[&](auto const &specialization) {
			return is_equal_params(specialization->params, body->params);
		}
	);
	if (it == this->generic_specializations.end())
	{
		this->generic_specializations.push_back(std::move(body));
		return this->generic_specializations.back().get();
	}
	else
	{
		return it->get();
	}
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
	else if (symbol_name.starts_with(function_))
	{
		auto const name_begin = bz::u8string_view::const_iterator(it.data() + function_.size());
		result += "function ";
		auto const name_end = symbol_name.find(name_begin, "..");
		result += bz::u8string_view(name_begin, name_end);
		result += '(';
		it = bz::u8string_view::const_iterator(name_end.data() + 2);
	}
	else if (bz::u8string_view(it, end) == "main")
	{
		return "function main() -> int32";
	}
	else
	{
		// unknown prefix (extern probably)
		return bz::u8string(it, end);
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
