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
		auto const func_body = this->generic_specializations.back().get();
		if (func_body->is_intrinsic())
		{
			switch (func_body->intrinsic_kind)
			{
			case builtin_slice_begin_ptr:
			case builtin_slice_begin_const_ptr:
			case builtin_slice_end_ptr:
			case builtin_slice_end_const_ptr:
			{
				bz_assert(func_body->params.size() == 1);
				typespec_view const arg_type = func_body->params[0].var_type;
				bz_assert(arg_type.is<ts_array_slice>());
				func_body->return_type = arg_type.get<ast::ts_array_slice>().elem_type;
				func_body->return_type.add_layer<ts_pointer>();
				break;
			}
			case builtin_slice_from_ptrs:
			case builtin_slice_from_const_ptrs:
			{
				bz_assert(func_body->params.size() == 2);
				typespec_view const arg_type = func_body->params[0].var_type;
				bz_assert(arg_type.is<ts_pointer>());
				func_body->return_type = make_array_slice_typespec({}, arg_type.get<ts_pointer>());
				break;
			}

			static_assert(_builtin_last - _builtin_first == 14);
			}
		}
		return func_body;
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

static_assert(ast::type_info::int8_    ==  0);
static_assert(ast::type_info::int16_   ==  1);
static_assert(ast::type_info::int32_   ==  2);
static_assert(ast::type_info::int64_   ==  3);
static_assert(ast::type_info::uint8_   ==  4);
static_assert(ast::type_info::uint16_  ==  5);
static_assert(ast::type_info::uint32_  ==  6);
static_assert(ast::type_info::uint64_  ==  7);
static_assert(ast::type_info::float32_ ==  8);
static_assert(ast::type_info::float64_ ==  9);
static_assert(ast::type_info::char_    == 10);
static_assert(ast::type_info::str_     == 11);
static_assert(ast::type_info::bool_    == 12);
static_assert(ast::type_info::null_t_  == 13);


static auto get_builtin_type_infos(void)
{
	return bz::array{
		ast::type_info::make_built_in("int8",     ast::type_info::int8_),
		ast::type_info::make_built_in("int16",    ast::type_info::int16_),
		ast::type_info::make_built_in("int32",    ast::type_info::int32_),
		ast::type_info::make_built_in("int64",    ast::type_info::int64_),
		ast::type_info::make_built_in("uint8",    ast::type_info::uint8_),
		ast::type_info::make_built_in("uint16",   ast::type_info::uint16_),
		ast::type_info::make_built_in("uint32",   ast::type_info::uint32_),
		ast::type_info::make_built_in("uint64",   ast::type_info::uint64_),
		ast::type_info::make_built_in("float32",  ast::type_info::float32_),
		ast::type_info::make_built_in("float64",  ast::type_info::float64_),
		ast::type_info::make_built_in("char",     ast::type_info::char_),
		ast::type_info::make_built_in("str",      ast::type_info::str_),
		ast::type_info::make_built_in("bool",     ast::type_info::bool_),
		ast::type_info::make_built_in("__null_t", ast::type_info::null_t_),
	};
}

static auto builtin_type_infos = get_builtin_type_infos();

template<typename ...Ts>
static ast::function_body create_builtin_function(
	uint32_t kind,
	bz::u8string_view symbol_name,
	ast::typespec return_type,
	Ts ...arg_types
)
{
	static_assert((bz::meta::is_same<Ts, ast::typespec> && ...));
	bz::vector<ast::decl_variable> params;
	params.reserve(sizeof... (Ts));
	((params.emplace_back(
		lex::token_range{}, lex::token_pos{}, lex::token_range{},
		std::move(arg_types)
	)), ...);
	auto const is_generic = [&]() {
		for (auto const &param : params)
		{
			if (!ast::is_complete(param.var_type))
			{
				return true;
			}
		}
		return false;
	}();
	ast::function_body result;
	result.params = std::move(params);
	result.return_type = std::move(return_type);
	result.symbol_name = symbol_name;
	result.state = ast::resolve_state::symbol;
	result.cc = abi::calling_convention::c;
	result.flags =
		ast::function_body::external_linkage
		| ast::function_body::intrinsic
		| (is_generic ? ast::function_body::generic : 0);
	result.intrinsic_kind = kind;
	return result;
}

type_info *get_builtin_type_info(uint32_t kind)
{
	bz_assert(kind < builtin_type_infos.size());
	return &builtin_type_infos[kind];
}

static auto builtin_functions = []() {
	auto const bool_type   = ast::make_base_type_typespec({}, get_builtin_type_info(ast::type_info::bool_));
	auto const uint64_type = ast::make_base_type_typespec({}, get_builtin_type_info(ast::type_info::uint64_));
	auto const str_type    = ast::make_base_type_typespec({}, get_builtin_type_info(ast::type_info::str_));
	auto const uint8_const_ptr_type = [&]() {
		ast::typespec result = ast::make_base_type_typespec({}, get_builtin_type_info(ast::type_info::uint8_));
		result.add_layer<ast::ts_const>(nullptr);
		result.add_layer<ast::ts_pointer>(nullptr);
		return result;
	}();
	auto const auto_ptr_type = [&]() {
		ast::typespec result = ast::make_auto_typespec({});
		result.add_layer<ast::ts_pointer>(nullptr);
		return result;
	}();
	auto const auto_const_ptr_type = [&]() {
		ast::typespec result = ast::make_auto_typespec({});
		result.add_layer<ast::ts_const>(nullptr);
		result.add_layer<ast::ts_pointer>(nullptr);
		return result;
	}();
	auto const slice_auto_type = [&]() {
		ast::typespec auto_t = ast::make_auto_typespec({});
		return ast::make_array_slice_typespec({}, std::move(auto_t));
	}();
	auto const slice_const_auto_type = [&]() {
		ast::typespec auto_t = ast::make_auto_typespec({});
		auto_t.add_layer<ast::ts_const>(nullptr);
		return ast::make_array_slice_typespec({}, std::move(auto_t));
	}();

#define add_builtin(pos, kind, symbol_name, ...) \
((void)([]() { static_assert(kind == pos); }), create_builtin_function(kind, symbol_name, __VA_ARGS__))
	bz::array<
		ast::function_body,
		ast::function_body::_builtin_last - ast::function_body::_builtin_first
	> result = {
		add_builtin( 0, ast::function_body::builtin_str_eq,     "__bozon_builtin_str_eq",     bool_type,   str_type, str_type),
		add_builtin( 1, ast::function_body::builtin_str_neq,    "__bozon_builtin_str_neq",    bool_type,   str_type, str_type),
		add_builtin( 2, ast::function_body::builtin_str_length, "__bozon_builtin_str_length", uint64_type, str_type),

		add_builtin( 3, ast::function_body::builtin_str_begin_ptr,         "", uint8_const_ptr_type, str_type),
		add_builtin( 4, ast::function_body::builtin_str_end_ptr,           "", uint8_const_ptr_type, str_type),
		add_builtin( 5, ast::function_body::builtin_str_size,              "", uint64_type, str_type),
		add_builtin( 6, ast::function_body::builtin_str_from_ptrs,         "", str_type, uint8_const_ptr_type, uint8_const_ptr_type),

		add_builtin( 7, ast::function_body::builtin_slice_begin_ptr,       "", {}, slice_auto_type),
		add_builtin( 8, ast::function_body::builtin_slice_begin_const_ptr, "", {}, slice_const_auto_type),
		add_builtin( 9, ast::function_body::builtin_slice_end_ptr,         "", {}, slice_auto_type),
		add_builtin(10, ast::function_body::builtin_slice_end_const_ptr,   "", {}, slice_const_auto_type),
		add_builtin(11, ast::function_body::builtin_slice_size,            "", uint64_type, slice_const_auto_type),
		add_builtin(12, ast::function_body::builtin_slice_from_ptrs,       "", {}, auto_ptr_type, auto_ptr_type),
		add_builtin(13, ast::function_body::builtin_slice_from_const_ptrs, "", {}, auto_const_ptr_type, auto_const_ptr_type),
	};
#undef add_builtin
	static_assert(function_body::_builtin_last - function_body::_builtin_first == 14);
	return result;
}();

function_body *get_builtin_function(uint32_t kind)
{
	bz_assert(kind < builtin_functions.size());
	return &builtin_functions[kind];
}

} // namespace ast
