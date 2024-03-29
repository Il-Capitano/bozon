#include "statement.h"
#include "token_info.h"

namespace ast
{

bz::u8string function_body::get_signature(void) const
{
	auto const is_op = this->function_name_or_operator_kind.is<uint32_t>();
	bz::u8string result = "";

	if (this->is_constructor())
	{
		auto const info = this->get_constructor_of();
		result += type_info::decode_symbol_name(info->symbol_name);
		result += "::constructor(";
	}
	else if (this->is_destructor())
	{
		auto const info = this->get_destructor_of();
		result += type_info::decode_symbol_name(info->symbol_name);
		result += "::destructor(";
	}
	else if (is_op)
	{
		result += "operator ";
		auto const kind = this->function_name_or_operator_kind.get<uint32_t>();
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
		result += this->function_name_or_operator_kind.get<identifier>().format_as_unqualified();
		result += '(';
	}

	bool first = true;
	for (auto &p : this->params)
	{
		if (first)
		{
			first = false;
			result += bz::format("{}: {}", p.get_id().as_string(), p.get_type());
		}
		else
		{
			result += bz::format(", {}: {}", p.get_id().as_string(), p.get_type());
		}

		if (p.get_type().is_typename() && p.init_expr.is_typename())
		{
			result += bz::format(" = {}", p.init_expr.get_typename());
		}
		else if (p.get_type().is<ast::ts_consteval>() && p.init_expr.is_constant())
		{
			result += bz::format(" = {}", get_value_string(p.init_expr.get_constant_value()));
		}
	}

	if (this->is_constructor() || this->is_destructor())
	{
		result += ')';
	}
	else
	{
		result += ") -> ";
		result += bz::format("{}", this->return_type);
	}
	return result;
}

bz::u8string function_body::get_symbol_name(void) const
{
	if (this->is_destructor())
	{
		if (this->params[0].get_type().is<ast::ts_move_reference>())
		{
			return bz::format("move_dtor.{}", ast::type_info::decode_symbol_name(this->get_destructor_of()->symbol_name));
		}
		else
		{
			return bz::format("dtor.{}", ast::type_info::decode_symbol_name(this->get_destructor_of()->symbol_name));
		}
	}
	else if (this->is_constructor())
	{
		auto symbol_name = bz::format("ctor.{}", ast::type_info::decode_symbol_name(this->get_constructor_of()->symbol_name));
		symbol_name += bz::format("..{}", this->params.size());
		for (auto &p : this->params)
		{
			symbol_name += '.';
			symbol_name += p.get_type().get_symbol_name();
			if (p.get_type().is<ts_consteval>())
			{
				bz_assert(p.init_expr.is_constant());
				symbol_name += '.';
				constant_value::encode_for_symbol_name(symbol_name, p.init_expr.get_constant_value());
			}
		}
		return symbol_name;
	}
	else
	{
		bz_assert(this->function_name_or_operator_kind.not_null());
		auto const is_op = this->function_name_or_operator_kind.is<uint32_t>();
		auto symbol_name = is_op
			? bz::format("op.{}", this->function_name_or_operator_kind.get<uint32_t>())
			: bz::format("func.{}", this->function_name_or_operator_kind.get<identifier>().format_for_symbol());
		symbol_name += bz::format("..{}", this->params.size());
		for (auto &p : this->params)
		{
			symbol_name += '.';
			symbol_name += p.get_type().get_symbol_name();
			if (ast::is_generic_parameter(p))
			{
				bz_assert(p.init_expr.is_constant());
				symbol_name += '.';
				constant_value::encode_for_symbol_name(symbol_name, p.init_expr.get_constant_value());
			}
		}
		symbol_name += '.';
		symbol_name += this->return_type.get_symbol_name();
		return symbol_name;
	}
}

bz::u8string function_body::get_candidate_message(void) const
{
	if (this->is_default_op_assign())
	{
		return bz::format("candidate: (the default copy assignment operator) '{}'", this->get_signature());
	}
	else if (this->is_default_op_move_assign())
	{
		return bz::format("candidate: (the default move assignment operator) '{}'", this->get_signature());
	}
	else if (this->is_default_default_constructor())
	{
		return bz::format("candidate: (the default default constructor) '{}'", this->get_signature());
	}
	else if (this->is_default_copy_constructor())
	{
		return bz::format("candidate: (the default copy constructor) '{}'", this->get_signature());
	}
	else if (this->is_default_move_constructor())
	{
		return bz::format("candidate (the default move constructor) '{}'", this->get_signature());
	}
	else
	{
		return bz::format("candidate: '{}'", this->get_signature());
	}
}

arena_vector<decl_variable> function_body::get_params_copy_for_generic_specialization(void)
{
	return this->params;
}

std::pair<function_body *, bz::u8string> function_body::add_specialized_body(
	arena_vector<decl_variable> params,
	arena_vector<generic_required_from_t> required_from
)
{
	bz_assert(params.size() == this->params.size() || (!this->params.empty() && this->params.back().get_type().is<ast::ts_variadic>()));

	if (params.is_any([](auto const &param) { return is_generic_parameter(param) && param.init_expr.is_error(); }))
	{
		return { nullptr, bz::u8string() };
	}

	if (auto const it = this->generic_specializations_map.find(params); it != this->generic_specializations_map.end())
	{
		return { it->second, bz::u8string() };
	}

	this->generic_specializations.emplace_back(make_ast_unique<function_body>(*this, generic_copy_t{}));
	auto const func_body = this->generic_specializations.back().get();
	func_body->params = std::move(params);
	func_body->generic_parent = this;
	func_body->generic_required_from.append(std::move(required_from));

	this->generic_specializations_map.insert({ func_body->params, func_body });
	return { func_body, bz::u8string() };
}

enclosing_scope_t function_body::get_enclosing_scope(void) const noexcept
{
	bz_assert(this->scope.is_local());
	return this->scope.get_local().parent;
}

bz::u8string function_body::decode_symbol_name(
	bz::u8string_view::const_iterator &it,
	bz::u8string_view::const_iterator end
)
{
	constexpr bz::u8string_view destructor_ = "dtor.";
	constexpr bz::u8string_view constructor_ = "ctor.";
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
		auto func_name = bz::u8string_view(name_begin, name_end);
		for (auto it = func_name.find('.'); it != func_name.end(); it = func_name.find('.'))
		{
			auto const scope_name = bz::u8string_view(func_name.begin(), it);
			func_name = bz::u8string_view(it + 1, func_name.end());
			result += scope_name;
			constexpr auto scope = lex::get_token_value(lex::token::scope);
			result += scope;
		}
		result += func_name;
		result += '(';
		it = bz::u8string_view::const_iterator(name_end.data() + 2);
	}
	else if (symbol_name.starts_with(destructor_))
	{
		auto type_begin = bz::u8string_view::const_iterator(it.data() + destructor_.size());
		auto const type = typespec::decode_symbol_name(type_begin, end);
		return bz::format("{}::destructor(: &mut {})", type, type);
	}
	else if (symbol_name.starts_with(constructor_))
	{
		auto type_begin = bz::u8string_view::const_iterator(it.data() + constructor_.size());
		auto const type_end = symbol_name.find(type_begin, "..");
		auto const type = typespec::decode_symbol_name(type_begin, type_end);
		it = bz::u8string_view::const_iterator(type_end.data() + 2);
		result += type;
		result += "::constructor(";
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
		}
		else
		{
			result += ", : ";
		}
		auto const type = typespec::decode_symbol_name(it, end);
		result += type;
		if (type.starts_with("consteval") || type.contains("typename"))
		{
			bz_assert(it != end);
			bz_assert(*it == '.');
			++it;
			result += " = ";
			result += constant_value::decode_from_symbol_name(it, end);
		}
	}

	if (it == end)
	{
		result += ")";
	}
	else
	{
		bz_assert(it != end && *it == '.');
		++it;

		result += ") -> ";
		result += typespec::decode_symbol_name(it, end);
	}

	return result;
}

type_info::decl_operator_ptr type_info::make_default_op_assign(lex::src_tokens const &src_tokens, type_info &info)
{
	auto lhs_t = [&]() {
		typespec result = make_base_type_typespec({}, &info);
		result.add_layer<ts_mut>();
		result.add_layer<ts_lvalue_reference>();
		return result;
	}();
	auto rhs_t = [&]() {
		typespec result = make_base_type_typespec({}, &info);
		result.add_layer<ts_lvalue_reference>();
		return result;
	}();
	auto ret_t = [&]() {
		typespec result = make_base_type_typespec({}, &info);
		result.add_layer<ts_mut>();
		result.add_layer<ts_lvalue_reference>();
		return result;
	}();

	auto result = make_ast_unique<decl_operator>();
	result->body.params.reserve(2);
	result->body.params.emplace_back(
		lex::src_tokens{},
		lex::token_range{},
		var_id_and_type(identifier{}, std::move(lhs_t)),
		enclosing_scope_t{}
	);
	result->body.params.emplace_back(
		lex::src_tokens{},
		lex::token_range{},
		var_id_and_type(identifier{}, std::move(rhs_t)),
		enclosing_scope_t{}
	);
	result->body.return_type = std::move(ret_t);
	result->body.function_name_or_operator_kind = lex::token::assign;
	result->body.src_tokens = src_tokens;
	result->body.state = resolve_state::symbol;
	result->body.flags |= function_body::default_op_assign;
	result->body.flags |= function_body::module_export;
	return result;
}

type_info::decl_operator_ptr type_info::make_default_op_move_assign(lex::src_tokens const &src_tokens, type_info &info)
{
	auto lhs_t = [&]() {
		typespec result = make_base_type_typespec({}, &info);
		result.add_layer<ts_mut>();
		result.add_layer<ts_lvalue_reference>();
		return result;
	}();
	auto rhs_t = make_base_type_typespec({}, &info);
	auto ret_t = [&]() {
		typespec result = make_base_type_typespec({}, &info);
		result.add_layer<ts_mut>();
		result.add_layer<ts_lvalue_reference>();
		return result;
	}();

	auto result = make_ast_unique<decl_operator>();
	result->body.params.reserve(2);
	result->body.params.emplace_back(
		lex::src_tokens{},
		lex::token_range{},
		var_id_and_type(identifier{}, std::move(lhs_t)),
		enclosing_scope_t{}
	);
	result->body.params.emplace_back(
		lex::src_tokens{},
		lex::token_range{},
		var_id_and_type(identifier{}, std::move(rhs_t)),
		enclosing_scope_t{}
	);
	result->body.return_type = std::move(ret_t);
	result->body.function_name_or_operator_kind = lex::token::assign;
	result->body.src_tokens = src_tokens;
	result->body.state = resolve_state::symbol;
	result->body.flags |= function_body::default_op_move_assign;
	result->body.flags |= function_body::module_export;
	return result;
}

type_info::decl_function_ptr type_info::make_default_copy_constructor(lex::src_tokens const &src_tokens, type_info &info)
{
	auto param_t = make_base_type_typespec({}, &info);
	param_t.add_layer<ts_lvalue_reference>();

	auto ret_t = make_base_type_typespec({}, &info);

	auto result = make_ast_unique<decl_function>();
	result->body.params.emplace_back(
		lex::src_tokens{},
		lex::token_range{},
		var_id_and_type(identifier{}, std::move(param_t)),
		enclosing_scope_t{}
	);
	result->body.return_type = std::move(ret_t);
	result->body.src_tokens = src_tokens;
	result->body.state = resolve_state::symbol;
	result->body.flags |= function_body::constructor;
	result->body.flags |= function_body::copy_constructor;
	result->body.flags |= function_body::default_copy_constructor;
	result->body.constructor_or_destructor_of = &info;
	return result;
}

type_info::decl_function_ptr type_info::make_default_move_constructor(lex::src_tokens const &src_tokens, type_info &info)
{
	auto param_t = make_base_type_typespec({}, &info);
	param_t.add_layer<ts_move_reference>();

	auto ret_t = make_base_type_typespec({}, &info);

	auto result = make_ast_unique<decl_function>();
	result->body.params.emplace_back(
		lex::src_tokens{},
		lex::token_range{},
		var_id_and_type(identifier{}, std::move(param_t)),
		enclosing_scope_t{}
	);
	result->body.return_type = std::move(ret_t);
	result->body.src_tokens = src_tokens;
	result->body.state = resolve_state::symbol;
	result->body.flags |= function_body::constructor;
	result->body.flags |= function_body::move_constructor;
	result->body.flags |= function_body::default_move_constructor;
	result->body.constructor_or_destructor_of = &info;
	return result;
}

type_info::decl_function_ptr type_info::make_default_default_constructor(lex::src_tokens const &src_tokens, type_info &info)
{
	auto ret_t = make_base_type_typespec({}, &info);

	auto result = make_ast_unique<decl_function>();
	result->body.return_type = std::move(ret_t);
	result->body.src_tokens = src_tokens;
	result->body.state = resolve_state::symbol;
	result->body.flags |= function_body::constructor;
	result->body.flags |= function_body::default_constructor;
	result->body.flags |= function_body::default_default_constructor;
	result->body.constructor_or_destructor_of = &info;
	return result;
}

arena_vector<decl_variable> type_info::get_params_copy_for_generic_instantiation(void)
{
	return this->generic_parameters;
}

type_info *type_info::add_generic_instantiation(
	arena_vector<decl_variable> generic_params,
	arena_vector<generic_required_from_t> required_from
)
{
	bz_assert(
		generic_params.size() == this->generic_parameters.size()
		|| (!this->generic_parameters.empty() && this->generic_parameters.back().get_type().is<ast::ts_variadic>())
	);
	auto const is_equal_params = [](auto const &lhs, auto const &rhs) {
		if (lhs.size() != rhs.size())
		{
			return false;
		}
		for (auto const &[lhs_param, rhs_param] : bz::zip(lhs, rhs))
		{
			if (lhs_param.get_type() != rhs_param.get_type())
			{
				return false;
			}
			else if (is_generic_parameter(lhs_param))
			{
				bz_assert(lhs_param.init_expr.is_constant());
				bz_assert(rhs_param.init_expr.is_constant());
				auto const &lhs_val = lhs_param.init_expr.get_constant_value();
				auto const &rhs_val = rhs_param.init_expr.get_constant_value();
				if (lhs_val != rhs_val)
				{
					return false;
				}
			}
		}
		return true;
	};
	auto const it = std::find_if(
		this->generic_instantiations.begin(), this->generic_instantiations.end(),
		[&](auto const &instantiation) {
			return is_equal_params(instantiation->generic_parameters, generic_params);
		}
	);

	if (it != this->generic_instantiations.end())
	{
		return it->get();
	}

	this->generic_instantiations.emplace_back(make_ast_unique<type_info>(*this, generic_copy_t{}));
	auto const info = this->generic_instantiations.back().get();
	info->generic_parameters = std::move(generic_params);
	info->generic_parent = this;
	info->generic_required_from.append(std::move(required_from));
	return info;
}

bz::u8string type_info::get_typename_as_string(void) const
{
	if (this->is_generic_instantiation())
	{
		auto result = this->type_name.format_as_unqualified();
		result += '<';
		bool first = true;
		for (auto const &param : this->generic_parameters)
		{
			if (first)
			{
				first = false;
			}
			else
			{
				result += ", ";
			}
			bz_assert(param.init_expr.is_constant());
			result += get_value_string(param.init_expr.get_constant_value());
		}
		result += '>';
		return result;
	}
	else
	{
		return this->type_name.format_as_unqualified();
	}
}

enclosing_scope_t type_info::get_scope(void) noexcept
{
	return { &this->scope, 0 };
}

enclosing_scope_t type_info::get_enclosing_scope(void) const noexcept
{
	bz_assert(this->scope.is_global());
	return this->scope.get_global().parent;
}

static_assert(type_info::int8_    ==  0);
static_assert(type_info::int16_   ==  1);
static_assert(type_info::int32_   ==  2);
static_assert(type_info::int64_   ==  3);
static_assert(type_info::uint8_   ==  4);
static_assert(type_info::uint16_  ==  5);
static_assert(type_info::uint32_  ==  6);
static_assert(type_info::uint64_  ==  7);
static_assert(type_info::float32_ ==  8);
static_assert(type_info::float64_ ==  9);
static_assert(type_info::char_    == 10);
static_assert(type_info::str_     == 11);
static_assert(type_info::bool_    == 12);
static_assert(type_info::null_t_  == 13);

uint64_t decl_enum::get_unique_values_count(void) const
{
	if (this->values.empty())
	{
		return 0;
	}

	bz_assert(this->values[0].value.not_null());
	auto values_vec = [&]() -> arena_vector<uint64_t> {
		if (this->values[0].value.is<int64_t>())
		{
			return this->values
				.transform([](auto const &value) { return static_cast<uint64_t>(value.value.template get<int64_t>()); })
				.collect<arena_vector>();
		}
		else
		{
			return this->values
				.transform([](auto const &value) { return value.value.template get<uint64_t>(); })
				.collect<arena_vector>();
		}
	}();

	std::sort(values_vec.begin(), values_vec.end());
	return static_cast<uint64_t>(std::unique(values_vec.begin(), values_vec.end()) - values_vec.begin());
}

bz::u8string_view decl_enum::get_value_name(uint64_t value) const
{
	if (this->values.empty())
	{
		return "";
	}

	auto const is_signed = this->values[0].value.is<int64_t>();
	if (is_signed)
	{
		auto const it = std::find_if(
			this->values.begin(), this->values.end(),
			[signed_value = bit_cast<int64_t>(value)](auto const &enum_value) {
				return enum_value.value.template get<int64_t>() == signed_value;
			}
		);
		if (it == this->values.end())
		{
			return "";
		}
		else
		{
			return it->id->value;
		}
	}
	else
	{
		auto const it = std::find_if(
			this->values.begin(), this->values.end(),
			[value](auto const &enum_value) {
				return enum_value.value.template get<uint64_t>() == value;
			}
		);
		if (it == this->values.end())
		{
			return "";
		}
		else
		{
			return it->id->value;
		}
	}
}

decl_enum::decl_operator_ptr decl_enum::make_default_op_assign(lex::src_tokens const &src_tokens, decl_enum &decl)
{
	auto lhs_t = [&]() {
		typespec result = make_enum_typespec({}, &decl);
		result.add_layer<ts_mut>();
		result.add_layer<ts_lvalue_reference>();
		return result;
	}();
	auto rhs_t = make_enum_typespec({}, &decl);
	auto ret_t = lhs_t;

	auto result = make_ast_unique<decl_operator>();
	result->body.params.reserve(2);
	result->body.params.emplace_back(
		lex::src_tokens{},
		lex::token_range{},
		var_id_and_type(identifier{}, std::move(lhs_t)),
		enclosing_scope_t{}
	);
	result->body.params.emplace_back(
		lex::src_tokens{},
		lex::token_range{},
		var_id_and_type(identifier{}, std::move(rhs_t)),
		enclosing_scope_t{}
	);
	result->body.return_type = std::move(ret_t);
	result->body.function_name_or_operator_kind = lex::token::assign;
	result->body.src_tokens = src_tokens;
	result->body.state = resolve_state::symbol;
	result->body.flags |= function_body::default_op_assign;
	result->body.flags |= function_body::module_export;
	return result;
}

decl_enum::decl_operator_ptr decl_enum::make_default_compare_op(
	lex::src_tokens const &src_tokens,
	decl_enum &decl,
	uint32_t op_kind,
	typespec result_type
)
{
	auto lhs_t = make_enum_typespec({}, &decl);
	auto rhs_t = make_enum_typespec({}, &decl);

	auto result = make_ast_unique<decl_operator>();
	result->body.params.reserve(2);
	result->body.params.emplace_back(
		lex::src_tokens{},
		lex::token_range{},
		var_id_and_type(identifier{}, std::move(lhs_t)),
		enclosing_scope_t{}
	);
	result->body.params.emplace_back(
		lex::src_tokens{},
		lex::token_range{},
		var_id_and_type(identifier{}, std::move(rhs_t)),
		enclosing_scope_t{}
	);
	result->body.return_type = std::move(result_type);
	result->body.function_name_or_operator_kind = op_kind;
	result->body.src_tokens = src_tokens;
	result->body.state = resolve_state::symbol;
	result->body.flags |= function_body::intrinsic;
	result->body.flags |= function_body::builtin_operator;
	result->body.flags |= function_body::module_export;
	switch (op_kind)
	{
	case lex::token::equals:
		result->body.intrinsic_kind = function_body::builtin_binary_equals;
		break;
	case lex::token::not_equals:
		result->body.intrinsic_kind = function_body::builtin_binary_not_equals;
		break;
	case lex::token::less_than:
		result->body.intrinsic_kind = function_body::builtin_binary_less_than;
		break;
	case lex::token::less_than_eq:
		result->body.intrinsic_kind = function_body::builtin_binary_less_than_eq;
		break;
	case lex::token::greater_than:
		result->body.intrinsic_kind = function_body::builtin_binary_greater_than;
		break;
	case lex::token::greater_than_eq:
		result->body.intrinsic_kind = function_body::builtin_binary_greater_than_eq;
		break;
	default:
		bz_unreachable;
	}
	return result;
}

bz::vector<universal_function_set> make_builtin_universal_functions(void)
{
	return {
		{ "size", {
			function_body::builtin_slice_size,
			function_body::builtin_array_size,
		}},
		{ "begin", {
			function_body::builtin_slice_begin_ptr,
			function_body::builtin_slice_begin_mut_ptr,
			function_body::builtin_array_begin_ptr,
			function_body::builtin_array_begin_mut_ptr,
		}},
		{ "end", {
			function_body::builtin_slice_end_ptr,
			function_body::builtin_slice_end_mut_ptr,
			function_body::builtin_array_end_ptr,
			function_body::builtin_array_end_mut_ptr,
		}},
		{ "get_value", {
			function_body::builtin_optional_get_value_ref,
			function_body::builtin_optional_get_mut_value_ref,
			function_body::builtin_optional_get_value,
		}},
	};
}

} // namespace ast
