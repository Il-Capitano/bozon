#include "statement.h"
#include "token_info.h"
#include "ctx/global_context.h"

namespace ast
{

lex::token_pos decl_variable::get_tokens_begin(void) const
{ return this->src_tokens.begin; }

lex::token_pos decl_variable::get_tokens_pivot(void) const
{ return this->src_tokens.pivot; }

lex::token_pos decl_variable::get_tokens_end(void) const
{ return this->src_tokens.end; }

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
		else if (p.get_type().is<ast::ts_consteval>() && p.init_expr.is<ast::constant_expression>())
		{
			result += bz::format(" = {}", get_value_string(p.init_expr.get<ast::constant_expression>().value));
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
		return bz::format("dtor.{}", ast::type_info::decode_symbol_name(this->get_destructor_of()->symbol_name));
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
				bz_assert(p.init_expr.is<constant_expression>());
				symbol_name += '.';
				constant_value::encode_for_symbol_name(symbol_name, p.init_expr.get<constant_expression>().value);
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
			if (p.get_type().is<ts_consteval>())
			{
				bz_assert(p.init_expr.is<constant_expression>());
				symbol_name += '.';
				constant_value::encode_for_symbol_name(symbol_name, p.init_expr.get<constant_expression>().value);
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
		return bz::format("candidate (the default copy assignment operator) '{}'", this->get_signature());
	}
	else if (this->is_default_op_move_assign())
	{
		return bz::format("candidate (the default move assignment operator) '{}'", this->get_signature());
	}
	else if (this->is_default_default_constructor())
	{
		return bz::format("candidate (the default default constructor) '{}'", this->get_signature());
	}
	else if (this->is_default_copy_constructor())
	{
		return bz::format("candidate (the default copy constructor) '{}'", this->get_signature());
	}
	else
	{
		return bz::format("candidate '{}'", this->get_signature());
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
				bz_assert(lhs_param.init_expr.template is<ast::constant_expression>());
				bz_assert(rhs_param.init_expr.template is<ast::constant_expression>());
				auto const &lhs_val = lhs_param.init_expr.template get<ast::constant_expression>().value;
				auto const &rhs_val = rhs_param.init_expr.template get<ast::constant_expression>().value;
				if (lhs_val != rhs_val)
				{
					return false;
				}
			}
		}
		return true;
	};
	auto const it = std::find_if(
		this->generic_specializations.begin(), this->generic_specializations.end(),
		[&](auto const &specialization) {
			return is_equal_params(specialization->params, params);
		}
	);

	if (it != this->generic_specializations.end())
	{
		return { it->get(), bz::u8string() };
	}

	this->generic_specializations.emplace_back(make_ast_unique<function_body>(*this, generic_copy_t{}));
	auto const func_body = this->generic_specializations.back().get();
	func_body->params = std::move(params);
	func_body->generic_parent = this;
	func_body->generic_required_from.append(std::move(required_from));
	if (false && func_body->is_intrinsic())
	{
		bz_unreachable;
		switch (func_body->intrinsic_kind)
		{
		case builtin_slice_begin_ptr:
		case builtin_slice_begin_const_ptr:
		case builtin_slice_end_ptr:
		case builtin_slice_end_const_ptr:
		{
			bz_assert(func_body->params.size() == 1);
			auto const arg_type = func_body->params[0].get_type().as_typespec_view();
			bz_assert(arg_type.is<ts_array_slice>());
			func_body->return_type = arg_type.get<ts_array_slice>().elem_type;
			func_body->return_type.add_layer<ts_pointer>();
			break;
		}
		case builtin_slice_from_ptrs:
		case builtin_slice_from_const_ptrs:
		{
			bz_assert(func_body->params.size() == 2);
			auto const first_arg_type = func_body->params[0].get_type().as_typespec_view();
			auto const second_arg_type = func_body->params[1].get_type().as_typespec_view();
			if (first_arg_type != second_arg_type)
			{
				return {
					nullptr,
					bz::format(
						"different types given to {}, '{}' and '{}'",
						func_body->function_name_or_operator_kind.get<identifier>().format_as_unqualified(),
						first_arg_type, second_arg_type
					)
				};
			}
			bz_assert(first_arg_type.is<ts_pointer>());
			func_body->return_type = make_array_slice_typespec({}, first_arg_type.get<ts_pointer>());
			break;
		}
		case builtin_pointer_cast:
		{
			bz_assert(func_body->params.size() == 2);
			bz_assert(func_body->params[0].init_expr.is_typename());
			func_body->return_type = func_body->params[0].init_expr.get_typename();
			break;
		}
		case builtin_int_to_pointer:
		{
			bz_assert(func_body->params.size() == 2);
			bz_assert(func_body->params[0].init_expr.is_typename());
			func_body->return_type = func_body->params[0].init_expr.get_typename();
			break;
		}
		case comptime_malloc_type:
		{
			bz_assert(func_body->params.size() == 2);
			bz_assert(func_body->params[0].init_expr.is_typename());
			auto result_type = func_body->params[0].init_expr.get_typename();
			if (result_type.is<ts_lvalue_reference>())
			{
				result_type.remove_layer();
			}
			result_type.add_layer<ts_pointer>();
			func_body->return_type = std::move(result_type);
			break;
		}

		case builtin_unary_plus:
		case builtin_unary_minus:
			bz_unreachable;
		case builtin_unary_dereference:
			bz_assert(func_body->params.size() == 1);
			bz_assert(func_body->params[0].get_type().is<ts_pointer>());
			func_body->return_type = func_body->params[0].get_type();
			func_body->return_type.nodes[0] = ts_lvalue_reference{};
			break;
		case builtin_unary_bit_not:
		case builtin_unary_bool_not:
			bz_unreachable;
		case builtin_unary_plus_plus:
		case builtin_unary_minus_minus:
			bz_assert(func_body->params.size() == 1);
			bz_assert(func_body->params[0].get_type().is<ts_lvalue_reference>());
			func_body->return_type = func_body->params[0].get_type();
			break;

		case builtin_binary_assign:
			bz_assert(func_body->params.size() == 2);
			bz_assert(func_body->params[0].get_type().is<ts_lvalue_reference>());
			if (!func_body->params[1].get_type().is<ts_base_type>() /* != null_t */)
			{
				auto const lhs_type = func_body->params[0].get_type().get<ts_lvalue_reference>();
				auto const rhs_type = func_body->params[1].get_type().as_typespec_view();
				if (lhs_type != rhs_type)
				{
					if (lhs_type.is<ts_pointer>())
					{
						bz_assert(rhs_type.is<ts_pointer>());
						return {
							nullptr,
							bz::format(
								"mismatched pointer types in 'operator =', '{}' and '{}'",
								lhs_type, rhs_type
							)
						};
					}
					else
					{
						bz_assert(lhs_type.is<ts_array_slice>());
						bz_assert(rhs_type.is<ts_array_slice>());
						return {
							nullptr,
							bz::format(
								"mismatched array slice types in 'operator =', '{}' and '{}'",
								lhs_type, rhs_type
							)
						};
					}
				}
			}
			func_body->return_type = func_body->params[0].get_type();
			break;
		case builtin_binary_plus:
			bz_assert(func_body->params.size() == 2);
			if (func_body->params[0].get_type().is<ts_pointer>())
			{
				func_body->return_type = func_body->params[0].get_type();
			}
			else
			{
				bz_assert(func_body->params[1].get_type().is<ts_pointer>());
				func_body->return_type = func_body->params[1].get_type();
			}
			break;
		case builtin_binary_plus_eq:
			bz_assert(func_body->params.size() == 2);
			bz_assert(func_body->params[0].get_type().is<ts_lvalue_reference>());
			func_body->return_type = func_body->params[0].get_type();
			break;
		case builtin_binary_minus:
			bz_assert(func_body->params.size() == 2);
			if (func_body->params[0].get_type().is<ts_pointer>() && func_body->params[1].get_type().is<ts_pointer>())
			{
				auto const lhs_type = func_body->params[0].get_type().as_typespec_view();
				auto const rhs_type = func_body->params[1].get_type().as_typespec_view();
				if (lhs_type != rhs_type)
				{
					return {
						nullptr,
						bz::format(
							"mismatched pointer types in 'operator -', '{}' and '{}'",
							lhs_type, rhs_type
						)
					};
				}
			}
			else if (func_body->params[0].get_type().is<ts_pointer>())
			{
				func_body->return_type = func_body->params[0].get_type();
			}
			else
			{
				bz_assert(func_body->params[1].get_type().is<ts_pointer>());
				func_body->return_type = func_body->params[1].get_type();
			}
			break;
		case builtin_binary_minus_eq:
			bz_assert(func_body->params.size() == 2);
			bz_assert(func_body->params[0].get_type().is<ts_lvalue_reference>());
			func_body->return_type = func_body->params[0].get_type();
			break;
		case builtin_binary_multiply:
		case builtin_binary_multiply_eq:
		case builtin_binary_divide:
		case builtin_binary_divide_eq:
		case builtin_binary_modulo:
		case builtin_binary_modulo_eq:
			bz_unreachable;
		case builtin_binary_equals:
		case builtin_binary_not_equals:
		case builtin_binary_less_than:
		case builtin_binary_less_than_eq:
		case builtin_binary_greater_than:
		case builtin_binary_greater_than_eq:
		{
			bz_assert(func_body->params.size() == 2);
			auto const lhs_type = func_body->params[0].get_type().as_typespec_view();
			auto const rhs_type = func_body->params[1].get_type().as_typespec_view();
			if (lhs_type.is<ts_pointer>() && rhs_type.is<ts_pointer>() && lhs_type != rhs_type)
			{
				return {
					nullptr,
					bz::format(
						"mismatched types in 'operator {}', '{}' and '{}'",
						lex::get_token_value(func_body->function_name_or_operator_kind.get<uint32_t>()),
						lhs_type, rhs_type
					)
				};
			}
			break;
		}
		case builtin_binary_bit_and:
		case builtin_binary_bit_and_eq:
		case builtin_binary_bit_xor:
		case builtin_binary_bit_xor_eq:
		case builtin_binary_bit_or:
		case builtin_binary_bit_or_eq:
		case builtin_binary_bit_left_shift:
		case builtin_binary_bit_left_shift_eq:
		case builtin_binary_bit_right_shift:
		case builtin_binary_bit_right_shift_eq:
			bz_unreachable;

		static_assert(_builtin_last - _builtin_first == 125);
		static_assert(_builtin_unary_operator_last - _builtin_unary_operator_first == 7);
		static_assert(_builtin_binary_operator_last - _builtin_binary_operator_first == 27);
		default:
			break;
		}
	}
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
		return bz::format("{}::destructor(: &{})", type, type);
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
		if (type.starts_with("consteval"))
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

type_info::decl_operator_ptr type_info::make_default_op_assign(lex::src_tokens src_tokens, type_info &info)
{
	auto lhs_t = [&]() {
		typespec result = make_base_type_typespec({}, &info);
		result.add_layer<ts_lvalue_reference>();
		return result;
	}();
	auto rhs_t = [&]() {
		typespec result = make_base_type_typespec({}, &info);
		result.add_layer<ts_const>();
		result.add_layer<ts_lvalue_reference>();
		return result;
	}();
	auto ret_t = [&]() {
		typespec result = make_base_type_typespec({}, &info);
		result.add_layer<ts_lvalue_reference>();
		return result;
	}();

	auto result = make_ast_unique<decl_operator>();
	result->body.params.reserve(2);
	result->body.params.emplace_back(
		lex::src_tokens{},
		lex::token_range{},
		var_id_and_type(identifier{}, type_as_expression(std::move(lhs_t))),
		enclosing_scope_t{}
	);
	result->body.params.emplace_back(
		lex::src_tokens{},
		lex::token_range{},
		var_id_and_type(identifier{}, type_as_expression(std::move(rhs_t))),
		enclosing_scope_t{}
	);
	result->body.return_type = std::move(ret_t);
	result->body.function_name_or_operator_kind = lex::token::assign;
	result->body.src_tokens = src_tokens;
	result->body.state = resolve_state::symbol;
	result->body.flags |= function_body::default_op_assign;
	return result;
}

type_info::decl_operator_ptr type_info::make_default_op_move_assign(lex::src_tokens src_tokens, type_info &info)
{
	auto lhs_t = [&]() {
		typespec result = make_base_type_typespec({}, &info);
		result.add_layer<ts_lvalue_reference>();
		return result;
	}();
	auto rhs_t = make_base_type_typespec({}, &info);
	auto ret_t = [&]() {
		typespec result = make_base_type_typespec({}, &info);
		result.add_layer<ts_lvalue_reference>();
		return result;
	}();

	auto result = make_ast_unique<decl_operator>();
	result->body.params.reserve(2);
	result->body.params.emplace_back(
		lex::src_tokens{},
		lex::token_range{},
		var_id_and_type(identifier{}, type_as_expression(std::move(lhs_t))),
		enclosing_scope_t{}
	);
	result->body.params.emplace_back(
		lex::src_tokens{},
		lex::token_range{},
		var_id_and_type(identifier{}, type_as_expression(std::move(rhs_t))),
		enclosing_scope_t{}
	);
	result->body.return_type = std::move(ret_t);
	result->body.function_name_or_operator_kind = lex::token::assign;
	result->body.src_tokens = src_tokens;
	result->body.state = resolve_state::symbol;
	result->body.flags |= function_body::default_op_move_assign;
	return result;
}

type_info::decl_function_ptr type_info::make_default_copy_constructor(lex::src_tokens src_tokens, type_info &info)
{
	auto param_t = make_base_type_typespec({}, &info);
	param_t.add_layer<ts_const>();
	param_t.add_layer<ts_lvalue_reference>();

	auto ret_t = make_base_type_typespec({}, &info);

	auto result = make_ast_unique<decl_function>();
	result->body.params.emplace_back(
		lex::src_tokens{},
		lex::token_range{},
		var_id_and_type(identifier{}, type_as_expression(std::move(param_t))),
		enclosing_scope_t{}
	);
	result->body.return_type = std::move(ret_t);
	result->body.src_tokens = src_tokens;
	result->body.state = resolve_state::symbol;
	result->body.flags |= function_body::default_copy_constructor;
	result->body.flags |= function_body::constructor;
	result->body.constructor_or_destructor_of = &info;
	return result;
}

type_info::decl_function_ptr type_info::make_default_default_constructor(lex::src_tokens src_tokens, type_info &info)
{
	auto param_t = make_base_type_typespec({}, &info);
	param_t.add_layer<ts_const>();
	param_t.add_layer<ts_lvalue_reference>();

	auto ret_t = make_base_type_typespec({}, &info);

	auto result = make_ast_unique<decl_function>();
	result->body.return_type = std::move(ret_t);
	result->body.src_tokens = src_tokens;
	result->body.state = resolve_state::symbol;
	result->body.flags |= function_body::default_default_constructor;
	result->body.flags |= function_body::constructor;
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
				bz_assert(lhs_param.init_expr.template is<ast::constant_expression>());
				bz_assert(rhs_param.init_expr.template is<ast::constant_expression>());
				auto const &lhs_val = lhs_param.init_expr.template get<ast::constant_expression>().value;
				auto const &rhs_val = rhs_param.init_expr.template get<ast::constant_expression>().value;
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
	switch (this->kind)
	{
	case aggregate:
	case forward_declaration:
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
				bz_assert(param.init_expr.is<ast::constant_expression>());
				result += get_value_string(param.init_expr.get<ast::constant_expression>().value);
			}
			result += '>';
			return result;
		}
		else
		{
			return this->type_name.format_as_unqualified();
		}
	default:
		return decode_symbol_name(this->symbol_name);
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

static type_info::decl_function_ptr make_default_constructor(type_info *info)
{
	auto result = make_ast_unique<decl_function>();
	result->body.return_type = make_base_type_typespec({}, info);
	switch (info->kind)
	{
		case type_info::int8_:
			result->body.intrinsic_kind = function_body::i8_default_constructor;
			break;
		case type_info::int16_:
			result->body.intrinsic_kind = function_body::i16_default_constructor;
			break;
		case type_info::int32_:
			result->body.intrinsic_kind = function_body::i32_default_constructor;
			break;
		case type_info::int64_:
			result->body.intrinsic_kind = function_body::i64_default_constructor;
			break;
		case type_info::uint8_:
			result->body.intrinsic_kind = function_body::u8_default_constructor;
			break;
		case type_info::uint16_:
			result->body.intrinsic_kind = function_body::u16_default_constructor;
			break;
		case type_info::uint32_:
			result->body.intrinsic_kind = function_body::u32_default_constructor;
			break;
		case type_info::uint64_:
			result->body.intrinsic_kind = function_body::u64_default_constructor;
			break;
		case type_info::float32_:
			result->body.intrinsic_kind = function_body::f32_default_constructor;
			break;
		case type_info::float64_:
			result->body.intrinsic_kind = function_body::f64_default_constructor;
			break;
		case type_info::char_:
			result->body.intrinsic_kind = function_body::char_default_constructor;
			break;
		case type_info::str_:
			result->body.intrinsic_kind = function_body::str_default_constructor;
			break;
		case type_info::bool_:
			result->body.intrinsic_kind = function_body::bool_default_constructor;
			break;
		case type_info::null_t_:
			result->body.intrinsic_kind = function_body::null_t_default_constructor;
			break;
		default:
			bz_unreachable;
	}
	result->body.flags = function_body::intrinsic
		| function_body::constructor
		| function_body::default_default_constructor;
	result->body.constructor_or_destructor_of = info;
	result->body.state = resolve_state::symbol;
	result->body.symbol_name = result->body.get_symbol_name();
	return result;
}

bz::vector<type_info> make_builtin_type_infos(void)
{
	bz::vector<type_info> result;
	result.reserve(type_info::null_t_ + 1);
	result.push_back(type_info::make_builtin("int8",     type_info::int8_));
	result.push_back(type_info::make_builtin("int16",    type_info::int16_));
	result.push_back(type_info::make_builtin("int32",    type_info::int32_));
	result.push_back(type_info::make_builtin("int64",    type_info::int64_));
	result.push_back(type_info::make_builtin("uint8",    type_info::uint8_));
	result.push_back(type_info::make_builtin("uint16",   type_info::uint16_));
	result.push_back(type_info::make_builtin("uint32",   type_info::uint32_));
	result.push_back(type_info::make_builtin("uint64",   type_info::uint64_));
	result.push_back(type_info::make_builtin("float32",  type_info::float32_));
	result.push_back(type_info::make_builtin("float64",  type_info::float64_));
	result.push_back(type_info::make_builtin("char",     type_info::char_));
	result.push_back(type_info::make_builtin("str",      type_info::str_));
	result.push_back(type_info::make_builtin("bool",     type_info::bool_));
	result.push_back(type_info::make_builtin("__null_t", type_info::null_t_));
	for (auto &info : result)
	{
		info.default_default_constructor = make_default_constructor(&info);
		info.constructors.push_back(info.default_default_constructor.get());
	}
	return result;
}

bz::vector<type_and_name_pair> make_builtin_types(bz::array_view<type_info> builtin_type_infos, size_t pointer_size)
{
	bz::vector<type_and_name_pair> result;
	result.reserve(builtin_type_infos.size() + 3);
	for (auto &type_info : builtin_type_infos)
	{
		result.emplace_back(make_base_type_typespec({}, &type_info), type_info::decode_symbol_name(type_info.symbol_name));
	}
	switch (pointer_size)
	{
	case 8:
		result.emplace_back(make_base_type_typespec({}, &builtin_type_infos[type_info::uint64_]), "usize");
		result.emplace_back(make_base_type_typespec({}, &builtin_type_infos[type_info::int64_]),  "isize");
		break;
	case 4:
		result.emplace_back(make_base_type_typespec({}, &builtin_type_infos[type_info::uint32_]), "usize");
		result.emplace_back(make_base_type_typespec({}, &builtin_type_infos[type_info::int32_]),  "isize");
		break;
	case 2:
		result.emplace_back(make_base_type_typespec({}, &builtin_type_infos[type_info::uint16_]), "usize");
		result.emplace_back(make_base_type_typespec({}, &builtin_type_infos[type_info::int16_]),  "isize");
		break;
	case 1:
		result.emplace_back(make_base_type_typespec({}, &builtin_type_infos[type_info::uint8_]), "usize");
		result.emplace_back(make_base_type_typespec({}, &builtin_type_infos[type_info::int8_]),  "isize");
		break;
	default:
		bz_unreachable;
	}
	result.emplace_back(make_void_typespec(nullptr), "void");
	return result;
}

template<typename ...Ts>
static function_body create_builtin_function(
	uint32_t kind,
	bz::u8string_view symbol_name,
	typespec return_type,
	Ts ...arg_types
)
{
	static_assert((bz::meta::is_same<Ts, typespec> && ...));
	arena_vector<decl_variable> params;
	params.reserve(sizeof... (Ts));
	((params.emplace_back(
		lex::src_tokens{}, lex::token_range{},
		var_id_and_type(identifier{}, type_as_expression(std::move(arg_types))),
		enclosing_scope_t{}
	)), ...);
	auto const is_generic = [&]() {
		for (auto const &param : params)
		{
			if (is_generic_parameter(param))
			{
				return true;
			}
		}
		return false;
	}();
	function_body result;
	result.params = std::move(params);
	result.return_type = std::move(return_type);
	bz_assert(intrinsic_info[kind].kind == kind);
	result.function_name_or_operator_kind = identifier{ {}, { intrinsic_info[kind].func_name }, true };
	result.symbol_name = symbol_name;
	result.state = resolve_state::symbol;
	result.cc = abi::calling_convention::c;
	result.flags =
		(symbol_name != "" ? function_body::external_linkage : 0)
		| function_body::intrinsic
		| (is_generic ? function_body::generic : 0);
	result.intrinsic_kind = kind;
	return result;
}

bz::vector<decl_function> make_builtin_functions(bz::array_view<type_info> builtin_type_infos, size_t pointer_size)
{
	auto const bool_type     = make_base_type_typespec({}, &builtin_type_infos[type_info::bool_]);
	auto const uint8_type    = make_base_type_typespec({}, &builtin_type_infos[type_info::uint8_]);
	auto const uint16_type   = make_base_type_typespec({}, &builtin_type_infos[type_info::uint16_]);
	auto const uint32_type   = make_base_type_typespec({}, &builtin_type_infos[type_info::uint32_]);
	auto const uint64_type   = make_base_type_typespec({}, &builtin_type_infos[type_info::uint64_]);
	auto const float32_type  = make_base_type_typespec({}, &builtin_type_infos[type_info::float32_]);
	auto const float64_type  = make_base_type_typespec({}, &builtin_type_infos[type_info::float64_]);
	auto const str_type      = make_base_type_typespec({}, &builtin_type_infos[type_info::str_]);
	auto const void_type     = make_void_typespec(nullptr);
	auto const typename_type = make_typename_typespec(nullptr);
	auto const auto_type     = make_auto_typespec(nullptr);
	auto const usize_type = [&]() {
		switch (pointer_size)
		{
		case 8:
			return uint64_type;
		case 4:
			return uint32_type;
		case 2:
			return uint16_type;
		case 1:
			return uint8_type;
		default:
			bz_unreachable;
		}
	}();
	auto const void_ptr_type = [&]() {
		typespec result = make_void_typespec(nullptr);
		result.add_layer<ts_pointer>();
		return result;
	}();
	auto const void_const_ptr_type = [&]() {
		typespec result = make_void_typespec(nullptr);
		result.add_layer<ts_const>();
		result.add_layer<ts_pointer>();
		return result;
	}();
	auto const uint8_const_ptr_type = [&]() {
		typespec result = make_base_type_typespec({}, &builtin_type_infos[type_info::uint8_]);
		result.add_layer<ts_const>();
		result.add_layer<ts_pointer>();
		return result;
	}();
	auto const auto_ptr_type = [&]() {
		typespec result = make_auto_typespec(nullptr);
		result.add_layer<ts_pointer>();
		return result;
	}();
	auto const auto_const_ptr_type = [&]() {
		typespec result = make_auto_typespec(nullptr);
		result.add_layer<ts_const>();
		result.add_layer<ts_pointer>();
		return result;
	}();
	auto const slice_auto_type = [&]() {
		typespec auto_t = make_auto_typespec(nullptr);
		return make_array_slice_typespec({}, std::move(auto_t));
	}();
	auto const slice_const_auto_type = [&]() {
		typespec auto_t = make_auto_typespec(nullptr);
		auto_t.add_layer<ts_const>();
		return make_array_slice_typespec({}, std::move(auto_t));
	}();
	auto const typename_ptr_type = [&]() {
		typespec result = make_typename_typespec(nullptr);
		result.add_layer<ts_pointer>();
		return result;
	}();
	auto const ref_auto_type = [&]() {
		typespec result = make_auto_typespec(nullptr);
		result.add_layer<ts_lvalue_reference>();
		return result;
	}();

	size_t pos = 0;

#define add_builtin(func_kind, symbol_name, ...)                       \
(result.emplace_back(                                                  \
    make_identifier(intrinsic_info[pos].func_name),                    \
    (                                                                  \
        [&]() { bz_assert(func_kind == intrinsic_info[pos].kind); }(), \
        create_builtin_function(func_kind, symbol_name, __VA_ARGS__)   \
    )                                                                  \
), ++pos)

	bz::vector<decl_function> result;
	result.reserve(intrinsic_info.size());

	add_builtin(function_body::builtin_str_eq,          "", bool_type,  str_type, str_type);
	add_builtin(function_body::builtin_str_neq,         "", bool_type,  str_type, str_type);
	add_builtin(function_body::builtin_str_length,      "", usize_type, str_type);
	add_builtin(function_body::builtin_str_starts_with, "", bool_type,  str_type, str_type);
	add_builtin(function_body::builtin_str_ends_with,   "", bool_type,  str_type, str_type);

	add_builtin(function_body::builtin_str_begin_ptr,         "", uint8_const_ptr_type, str_type);
	add_builtin(function_body::builtin_str_end_ptr,           "", uint8_const_ptr_type, str_type);
	add_builtin(function_body::builtin_str_size,              "", usize_type, str_type);
	add_builtin(function_body::builtin_str_from_ptrs,         "", str_type, uint8_const_ptr_type, uint8_const_ptr_type);

	add_builtin(function_body::builtin_slice_begin_ptr,       "", {}, slice_auto_type);
	add_builtin(function_body::builtin_slice_begin_const_ptr, "", {}, slice_const_auto_type);
	add_builtin(function_body::builtin_slice_end_ptr,         "", {}, slice_auto_type);
	add_builtin(function_body::builtin_slice_end_const_ptr,   "", {}, slice_const_auto_type);
	add_builtin(function_body::builtin_slice_size,            "", usize_type, slice_const_auto_type);
	add_builtin(function_body::builtin_slice_from_ptrs,       "", {}, auto_ptr_type, auto_ptr_type);
	add_builtin(function_body::builtin_slice_from_const_ptrs, "", {}, auto_const_ptr_type, auto_const_ptr_type);

	add_builtin(function_body::builtin_pointer_cast,   "", {}, typename_ptr_type, void_const_ptr_type);
	add_builtin(function_body::builtin_pointer_to_int, "", usize_type, void_const_ptr_type);
	add_builtin(function_body::builtin_int_to_pointer, "", {}, typename_ptr_type, usize_type);

	add_builtin(function_body::builtin_call_destructor,   "", void_type, ref_auto_type);
	add_builtin(function_body::builtin_inplace_construct, "", void_type, auto_ptr_type, auto_type);

	add_builtin(function_body::builtin_is_comptime,   "", bool_type);
	add_builtin(function_body::builtin_is_option_set, "", bool_type, str_type);
	add_builtin(function_body::builtin_panic,         "__bozon_builtin_panic", void_type);

	add_builtin(function_body::print_stdout,   "__bozon_builtin_print_stdout",   void_type, str_type);
	add_builtin(function_body::println_stdout, "__bozon_builtin_println_stdout", void_type, str_type);
	add_builtin(function_body::print_stderr,   "__bozon_builtin_print_stderr",   void_type, str_type);
	add_builtin(function_body::println_stderr, "__bozon_builtin_println_stderr", void_type, str_type);

	add_builtin(function_body::comptime_malloc,      "__bozon_builtin_comptime_malloc", void_ptr_type, usize_type);
	add_builtin(function_body::comptime_malloc_type, "", {}, typename_type, usize_type);
	add_builtin(function_body::comptime_free,        "__bozon_builtin_comptime_free",   void_type, void_ptr_type);

	add_builtin(function_body::comptime_compile_error,   "", void_type, str_type);
	add_builtin(function_body::comptime_compile_warning, "", void_type, str_type);

	add_builtin(function_body::comptime_compile_error_src_tokens,   "", void_type, str_type, uint64_type, uint64_type, uint64_type);
	add_builtin(function_body::comptime_compile_warning_src_tokens, "", void_type, str_type, uint64_type, uint64_type, uint64_type);

	add_builtin(function_body::comptime_create_global_string, "", str_type, str_type);

	add_builtin(function_body::typename_as_str, "", str_type, typename_type);

	add_builtin(function_body::memcpy,  "llvm.memcpy.p0i8.p0i8.i64",  void_type, void_ptr_type, void_const_ptr_type, uint64_type);
	add_builtin(function_body::memmove, "llvm.memmove.p0i8.p0i8.i64", void_type, void_ptr_type, void_const_ptr_type, uint64_type);
	add_builtin(function_body::memset,  "llvm.memset.p0i8.i64", void_type, void_ptr_type, uint8_type, uint64_type);

	add_builtin(function_body::exp_f32,   "llvm.exp.f32",   float32_type, float32_type);
	add_builtin(function_body::exp_f64,   "llvm.exp.f64",   float64_type, float64_type);
	add_builtin(function_body::exp2_f32,  "llvm.exp2.f32",  float32_type, float32_type);
	add_builtin(function_body::exp2_f64,  "llvm.exp2.f64",  float64_type, float64_type);
	add_builtin(function_body::expm1_f32, "expm1f",         float32_type, float32_type);
	add_builtin(function_body::expm1_f64, "expm1",          float64_type, float64_type);
	add_builtin(function_body::log_f32,   "llvm.log.f32",   float32_type, float32_type);
	add_builtin(function_body::log_f64,   "llvm.log.f64",   float64_type, float64_type);
	add_builtin(function_body::log10_f32, "llvm.log10.f32", float32_type, float32_type);
	add_builtin(function_body::log10_f64, "llvm.log10.f64", float64_type, float64_type);
	add_builtin(function_body::log2_f32,  "llvm.log2.f32",  float32_type, float32_type);
	add_builtin(function_body::log2_f64,  "llvm.log2.f64",  float64_type, float64_type);
	add_builtin(function_body::log1p_f32, "log1pf",         float32_type, float32_type);
	add_builtin(function_body::log1p_f64, "log1p",          float64_type, float64_type);

	add_builtin(function_body::sqrt_f32,  "llvm.sqrt.f32", float32_type, float32_type);
	add_builtin(function_body::sqrt_f64,  "llvm.sqrt.f64", float64_type, float64_type);
	add_builtin(function_body::pow_f32,   "llvm.pow.f32",  float32_type, float32_type, float32_type);
	add_builtin(function_body::pow_f64,   "llvm.pow.f64",  float64_type, float64_type, float64_type);
	add_builtin(function_body::cbrt_f32,  "cbrtf",         float32_type, float32_type);
	add_builtin(function_body::cbrt_f64,  "cbrt",          float64_type, float64_type);
	add_builtin(function_body::hypot_f32, "hypotf",        float32_type, float32_type, float32_type);
	add_builtin(function_body::hypot_f64, "hypot",         float64_type, float64_type, float64_type);

	add_builtin(function_body::sin_f32,   "llvm.sin.f32", float32_type, float32_type);
	add_builtin(function_body::sin_f64,   "llvm.sin.f64", float64_type, float64_type);
	add_builtin(function_body::cos_f32,   "llvm.cos.f32", float32_type, float32_type);
	add_builtin(function_body::cos_f64,   "llvm.cos.f64", float64_type, float64_type);
	add_builtin(function_body::tan_f32,   "tanf",         float32_type, float32_type);
	add_builtin(function_body::tan_f64,   "tan",          float64_type, float64_type);
	add_builtin(function_body::asin_f32,  "asinf",        float32_type, float32_type);
	add_builtin(function_body::asin_f64,  "asin",         float64_type, float64_type);
	add_builtin(function_body::acos_f32,  "acosf",        float32_type, float32_type);
	add_builtin(function_body::acos_f64,  "acos",         float64_type, float64_type);
	add_builtin(function_body::atan_f32,  "atanf",        float32_type, float32_type);
	add_builtin(function_body::atan_f64,  "atan",         float64_type, float64_type);
	add_builtin(function_body::atan2_f32, "atan2f",       float32_type, float32_type, float32_type);
	add_builtin(function_body::atan2_f64, "atan2",        float64_type, float64_type, float64_type);

	add_builtin(function_body::sinh_f32,  "sinhf",  float32_type, float32_type);
	add_builtin(function_body::sinh_f64,  "sinh",   float64_type, float64_type);
	add_builtin(function_body::cosh_f32,  "coshf",  float32_type, float32_type);
	add_builtin(function_body::cosh_f64,  "cosh",   float64_type, float64_type);
	add_builtin(function_body::tanh_f32,  "tanhf",  float32_type, float32_type);
	add_builtin(function_body::tanh_f64,  "tanh",   float64_type, float64_type);
	add_builtin(function_body::asinh_f32, "asinhf", float32_type, float32_type);
	add_builtin(function_body::asinh_f64, "asinh",  float64_type, float64_type);
	add_builtin(function_body::acosh_f32, "acoshf", float32_type, float32_type);
	add_builtin(function_body::acosh_f64, "acosh",  float64_type, float64_type);
	add_builtin(function_body::atanh_f32, "atanhf", float32_type, float32_type);
	add_builtin(function_body::atanh_f64, "atanh",  float64_type, float64_type);

	add_builtin(function_body::erf_f32,    "erff",    float32_type, float32_type);
	add_builtin(function_body::erf_f64,    "erf",     float64_type, float64_type);
	add_builtin(function_body::erfc_f32,   "erfcf",   float32_type, float32_type);
	add_builtin(function_body::erfc_f64,   "erfc",    float64_type, float64_type);
	add_builtin(function_body::tgamma_f32, "tgammaf", float32_type, float32_type);
	add_builtin(function_body::tgamma_f64, "tgamma",  float64_type, float64_type);
	add_builtin(function_body::lgamma_f32, "lgammaf", float32_type, float32_type);
	add_builtin(function_body::lgamma_f64, "lgamma",  float64_type, float64_type);

	add_builtin(function_body::bitreverse_u8,  "llvm.bitreverse.i8",  uint8_type,  uint8_type);
	add_builtin(function_body::bitreverse_u16, "llvm.bitreverse.i16", uint16_type, uint16_type);
	add_builtin(function_body::bitreverse_u32, "llvm.bitreverse.i32", uint32_type, uint32_type);
	add_builtin(function_body::bitreverse_u64, "llvm.bitreverse.i64", uint64_type, uint64_type);
	add_builtin(function_body::popcount_u8,    "llvm.ctpop.i8",  uint8_type,  uint8_type);
	add_builtin(function_body::popcount_u16,   "llvm.ctpop.i16", uint16_type, uint16_type);
	add_builtin(function_body::popcount_u32,   "llvm.ctpop.i32", uint32_type, uint32_type);
	add_builtin(function_body::popcount_u64,   "llvm.ctpop.i64", uint64_type, uint64_type);
	add_builtin(function_body::byteswap_u16,   "llvm.bswap.i16", uint16_type, uint16_type);
	add_builtin(function_body::byteswap_u32,   "llvm.bswap.i32", uint32_type, uint32_type);
	add_builtin(function_body::byteswap_u64,   "llvm.bswap.i64", uint64_type, uint64_type);

	add_builtin(function_body::clz_u8,   "llvm.ctlz.i8",  uint8_type,  uint8_type);
	add_builtin(function_body::clz_u16,  "llvm.ctlz.i16", uint16_type, uint16_type);
	add_builtin(function_body::clz_u32,  "llvm.ctlz.i32", uint32_type, uint32_type);
	add_builtin(function_body::clz_u64,  "llvm.ctlz.i64", uint64_type, uint64_type);
	add_builtin(function_body::ctz_u8,   "llvm.cttz.i8",  uint8_type,  uint8_type);
	add_builtin(function_body::ctz_u16,  "llvm.cttz.i16", uint16_type, uint16_type);
	add_builtin(function_body::ctz_u32,  "llvm.cttz.i32", uint32_type, uint32_type);
	add_builtin(function_body::ctz_u64,  "llvm.cttz.i64", uint64_type, uint64_type);
	add_builtin(function_body::fshl_u8,  "llvm.fshl.i8",  uint8_type,  uint8_type,  uint8_type,  uint8_type);
	add_builtin(function_body::fshl_u16, "llvm.fshl.i16", uint16_type, uint16_type, uint16_type, uint16_type);
	add_builtin(function_body::fshl_u32, "llvm.fshl.i32", uint32_type, uint32_type, uint32_type, uint32_type);
	add_builtin(function_body::fshl_u64, "llvm.fshl.i64", uint64_type, uint64_type, uint64_type, uint64_type);
	add_builtin(function_body::fshr_u8,  "llvm.fshr.i8",  uint8_type,  uint8_type,  uint8_type,  uint8_type);
	add_builtin(function_body::fshr_u16, "llvm.fshr.i16", uint16_type, uint16_type, uint16_type, uint16_type);
	add_builtin(function_body::fshr_u32, "llvm.fshr.i32", uint32_type, uint32_type, uint32_type, uint32_type);
	add_builtin(function_body::fshr_u64, "llvm.fshr.i64", uint64_type, uint64_type, uint64_type, uint64_type);
#undef add_builtin
	bz_assert(result.size() == intrinsic_info.size());

	result[function_body::builtin_is_option_set              ].body.flags |= function_body::only_consteval;
	result[function_body::comptime_compile_error             ].body.flags |= function_body::only_consteval;
	result[function_body::comptime_compile_warning           ].body.flags |= function_body::only_consteval;
	result[function_body::comptime_compile_error_src_tokens  ].body.flags |= function_body::only_consteval;
	result[function_body::comptime_compile_warning_src_tokens].body.flags |= function_body::only_consteval;

	return result;
}

bz::vector<universal_function_set> make_builtin_universal_functions(void)
{
	return {
		{ "length",      { function_body::builtin_str_length                                                    } },
		{ "size",        { function_body::builtin_str_size,        function_body::builtin_slice_size            } },
		{ "begin",       { function_body::builtin_slice_begin_ptr, function_body::builtin_slice_begin_const_ptr } },
		{ "end",         { function_body::builtin_slice_end_ptr,   function_body::builtin_slice_end_const_ptr   } },
		{ "begin_ptr",   { function_body::builtin_str_begin_ptr                                                 } },
		{ "end_ptr",     { function_body::builtin_str_end_ptr                                                   } },
		{ "starts_with", { function_body::builtin_str_starts_with                                               } },
		{ "ends_with",   { function_body::builtin_str_ends_with                                                 } },
	};
}


enum class generic_arg_kind : uint8_t
{
	none,
	ptr,
	ptr_const,
	ref,
	ref_const,
	slice,
	slice_const,
};

constexpr uint8_t unary_intrinsic_kind_from_op_kind(uint32_t op_kind)
{
	switch (op_kind)
	{
	case lex::token::plus:        return function_body::builtin_unary_plus;
	case lex::token::minus:       return function_body::builtin_unary_minus;
	case lex::token::dereference: return function_body::builtin_unary_dereference;
	case lex::token::bit_not:     return function_body::builtin_unary_bit_not;
	case lex::token::bool_not:    return function_body::builtin_unary_bool_not;
	case lex::token::plus_plus:   return function_body::builtin_unary_plus_plus;
	case lex::token::minus_minus: return function_body::builtin_unary_minus_minus;
	default: bz_unreachable;
	}
}

struct builtin_unary_operator_table_entry_t
{
	uint32_t op_kind;
	uint8_t intrinsic_kind;
	uint8_t expr_t;
	uint8_t res_t;
	generic_arg_kind expr_generic_t;

	constexpr builtin_unary_operator_table_entry_t(uint32_t op_kind, uint8_t expr_t, uint8_t res_t)
		: op_kind(op_kind), intrinsic_kind(unary_intrinsic_kind_from_op_kind(op_kind)), expr_t(expr_t), res_t(res_t), expr_generic_t(generic_arg_kind::none)
	{}

	constexpr builtin_unary_operator_table_entry_t(uint32_t op_kind, generic_arg_kind expr_generic_t, uint8_t res_t = type_info::aggregate)
		: op_kind(op_kind), intrinsic_kind(unary_intrinsic_kind_from_op_kind(op_kind)), expr_t(), res_t(res_t), expr_generic_t(expr_generic_t)
	{
		bz_assert(this->expr_generic_t != generic_arg_kind::none);
	}
};

#define unary_arithmetic_entry(op_kind, type_kind) \
builtin_unary_operator_table_entry_t(lex::token::op_kind, type_info::type_kind, type_info::type_kind)

#define unary_kind_entry(op_kind, expr_kind, res_kind) \
builtin_unary_operator_table_entry_t(lex::token::op_kind, type_info::expr_kind, type_info::res_kind)

static constexpr bz::array builtin_unary_operator_table = {
	// ================
	// operator +
	// ================
	unary_arithmetic_entry(plus, int8_),
	unary_arithmetic_entry(plus, int16_),
	unary_arithmetic_entry(plus, int32_),
	unary_arithmetic_entry(plus, int64_),
	unary_arithmetic_entry(plus, uint8_),
	unary_arithmetic_entry(plus, uint16_),
	unary_arithmetic_entry(plus, uint32_),
	unary_arithmetic_entry(plus, uint64_),
	unary_arithmetic_entry(plus, float32_),
	unary_arithmetic_entry(plus, float64_),

	// ================
	// operator -
	// ================
	unary_arithmetic_entry(minus, int8_),
	unary_arithmetic_entry(minus, int16_),
	unary_arithmetic_entry(minus, int32_),
	unary_arithmetic_entry(minus, int64_),
	unary_arithmetic_entry(minus, float32_),
	unary_arithmetic_entry(minus, float64_),

	// ================
	// operator *
	// ================
	builtin_unary_operator_table_entry_t(lex::token::dereference, generic_arg_kind::ptr),
	builtin_unary_operator_table_entry_t(lex::token::dereference, generic_arg_kind::ptr_const),

	// ================
	// operator ~
	// ================
	unary_arithmetic_entry(bit_not, uint8_),
	unary_arithmetic_entry(bit_not, uint16_),
	unary_arithmetic_entry(bit_not, uint32_),
	unary_arithmetic_entry(bit_not, uint64_),
	unary_arithmetic_entry(bit_not, bool_),

	// ================
	// operator !
	// ================
	unary_arithmetic_entry(bool_not, bool_),

	// ================
	// operator ++
	// ================
	unary_arithmetic_entry(plus_plus, int8_),
	unary_arithmetic_entry(plus_plus, int16_),
	unary_arithmetic_entry(plus_plus, int32_),
	unary_arithmetic_entry(plus_plus, int64_),
	unary_arithmetic_entry(plus_plus, uint8_),
	unary_arithmetic_entry(plus_plus, uint16_),
	unary_arithmetic_entry(plus_plus, uint32_),
	unary_arithmetic_entry(plus_plus, uint64_),
	unary_arithmetic_entry(plus_plus, char_),
	builtin_unary_operator_table_entry_t(lex::token::plus_plus, generic_arg_kind::ptr),
	builtin_unary_operator_table_entry_t(lex::token::plus_plus, generic_arg_kind::ptr_const),

	// ================
	// operator --
	// ================
	unary_arithmetic_entry(minus_minus, int8_),
	unary_arithmetic_entry(minus_minus, int16_),
	unary_arithmetic_entry(minus_minus, int32_),
	unary_arithmetic_entry(minus_minus, int64_),
	unary_arithmetic_entry(minus_minus, uint8_),
	unary_arithmetic_entry(minus_minus, uint16_),
	unary_arithmetic_entry(minus_minus, uint32_),
	unary_arithmetic_entry(minus_minus, uint64_),
	unary_arithmetic_entry(minus_minus, char_),
	builtin_unary_operator_table_entry_t(lex::token::minus_minus, generic_arg_kind::ptr),
	builtin_unary_operator_table_entry_t(lex::token::minus_minus, generic_arg_kind::ptr_const),
};

#undef unary_arithmetic_entry
#undef unary_kind_entry


constexpr uint8_t binary_intrinsic_kind_from_op_kind(uint32_t op_kind)
{
	switch (op_kind)
	{
	case lex::token::assign:             return function_body::builtin_binary_assign;
	case lex::token::plus:               return function_body::builtin_binary_plus;
	case lex::token::plus_eq:            return function_body::builtin_binary_plus_eq;
	case lex::token::minus:              return function_body::builtin_binary_minus;
	case lex::token::minus_eq:           return function_body::builtin_binary_minus_eq;
	case lex::token::multiply:           return function_body::builtin_binary_multiply;
	case lex::token::multiply_eq:        return function_body::builtin_binary_multiply_eq;
	case lex::token::divide:             return function_body::builtin_binary_divide;
	case lex::token::divide_eq:          return function_body::builtin_binary_divide_eq;
	case lex::token::modulo:             return function_body::builtin_binary_modulo;
	case lex::token::modulo_eq:          return function_body::builtin_binary_modulo_eq;
	case lex::token::equals:             return function_body::builtin_binary_equals;
	case lex::token::not_equals:         return function_body::builtin_binary_not_equals;
	case lex::token::less_than:          return function_body::builtin_binary_less_than;
	case lex::token::less_than_eq:       return function_body::builtin_binary_less_than_eq;
	case lex::token::greater_than:       return function_body::builtin_binary_greater_than;
	case lex::token::greater_than_eq:    return function_body::builtin_binary_greater_than_eq;
	case lex::token::bit_and:            return function_body::builtin_binary_bit_and;
	case lex::token::bit_and_eq:         return function_body::builtin_binary_bit_and_eq;
	case lex::token::bit_xor:            return function_body::builtin_binary_bit_xor;
	case lex::token::bit_xor_eq:         return function_body::builtin_binary_bit_xor_eq;
	case lex::token::bit_or:             return function_body::builtin_binary_bit_or;
	case lex::token::bit_or_eq:          return function_body::builtin_binary_bit_or_eq;
	case lex::token::bit_left_shift:     return function_body::builtin_binary_bit_left_shift;
	case lex::token::bit_left_shift_eq:  return function_body::builtin_binary_bit_left_shift_eq;
	case lex::token::bit_right_shift:    return function_body::builtin_binary_bit_right_shift;
	case lex::token::bit_right_shift_eq: return function_body::builtin_binary_bit_right_shift_eq;
	default: bz_unreachable;
	}
}

struct builtin_binary_operator_table_entry_t
{
	uint32_t op_kind;
	uint8_t intrinsic_kind;
	uint8_t lhs_t;
	uint8_t rhs_t;
	uint8_t res_t;
	generic_arg_kind lhs_generic_t;
	generic_arg_kind rhs_generic_t;

	constexpr builtin_binary_operator_table_entry_t(uint32_t op_kind, uint8_t lhs_t, uint8_t rhs_t, uint8_t res_t)
		: op_kind(op_kind), intrinsic_kind(binary_intrinsic_kind_from_op_kind(op_kind)), lhs_t(lhs_t), rhs_t(rhs_t), res_t(res_t), lhs_generic_t(generic_arg_kind::none), rhs_generic_t(generic_arg_kind::none)
	{}

	constexpr builtin_binary_operator_table_entry_t(uint32_t op_kind, generic_arg_kind lhs_generic_t, generic_arg_kind rhs_generic_t, uint8_t res_t = type_info::aggregate)
		: op_kind(op_kind), intrinsic_kind(binary_intrinsic_kind_from_op_kind(op_kind)), lhs_t(), rhs_t(), res_t(res_t), lhs_generic_t(lhs_generic_t), rhs_generic_t(rhs_generic_t)
	{
		bz_assert(this->lhs_generic_t != generic_arg_kind::none && this->rhs_generic_t != generic_arg_kind::none);
	}

	constexpr builtin_binary_operator_table_entry_t(uint32_t op_kind, generic_arg_kind lhs_generic_t, uint8_t rhs_t, uint8_t res_t = type_info::aggregate)
		: op_kind(op_kind), intrinsic_kind(binary_intrinsic_kind_from_op_kind(op_kind)), lhs_t(), rhs_t(rhs_t), res_t(res_t), lhs_generic_t(lhs_generic_t), rhs_generic_t(generic_arg_kind::none)
	{
		bz_assert(this->lhs_generic_t != generic_arg_kind::none);
	}

	constexpr builtin_binary_operator_table_entry_t(uint32_t op_kind, uint8_t lhs_t, generic_arg_kind rhs_generic_t, uint8_t res_t = type_info::aggregate)
		: op_kind(op_kind), intrinsic_kind(binary_intrinsic_kind_from_op_kind(op_kind)), lhs_t(lhs_t), rhs_t(), res_t(res_t), lhs_generic_t(generic_arg_kind::none), rhs_generic_t(rhs_generic_t)
	{
		bz_assert(this->rhs_generic_t != generic_arg_kind::none);
	}
};


#define binary_arithmetic_entry(op_kind, type_kind) \
builtin_binary_operator_table_entry_t(lex::token::op_kind, type_info::type_kind, type_info::type_kind, type_info::type_kind)

#define binary_kind_entry(op_kind, lhs_kind, rhs_kind, res_kind) \
builtin_binary_operator_table_entry_t(lex::token::op_kind, type_info::lhs_kind, type_info::rhs_kind, type_info::res_kind)

static constexpr bz::array builtin_binary_operator_table = {
	// ================
	// operator =
	// ================
	binary_arithmetic_entry(assign, int8_),
	binary_arithmetic_entry(assign, int16_),
	binary_arithmetic_entry(assign, int32_),
	binary_arithmetic_entry(assign, int64_),
	binary_arithmetic_entry(assign, uint8_),
	binary_arithmetic_entry(assign, uint16_),
	binary_arithmetic_entry(assign, uint32_),
	binary_arithmetic_entry(assign, uint64_),
	binary_arithmetic_entry(assign, float32_),
	binary_arithmetic_entry(assign, float64_),
	binary_arithmetic_entry(assign, char_),
	binary_arithmetic_entry(assign, str_),
	binary_arithmetic_entry(assign, bool_),
	binary_arithmetic_entry(assign, null_t_),
	builtin_binary_operator_table_entry_t(lex::token::assign, generic_arg_kind::ptr, generic_arg_kind::ptr),
	builtin_binary_operator_table_entry_t(lex::token::assign, generic_arg_kind::ptr_const, generic_arg_kind::ptr_const),
	builtin_binary_operator_table_entry_t(lex::token::assign, generic_arg_kind::ptr, type_info::null_t_),
	builtin_binary_operator_table_entry_t(lex::token::assign, generic_arg_kind::ptr_const, type_info::null_t_),
	builtin_binary_operator_table_entry_t(lex::token::assign, generic_arg_kind::slice, generic_arg_kind::slice),
	builtin_binary_operator_table_entry_t(lex::token::assign, generic_arg_kind::slice_const, generic_arg_kind::slice_const),

	// ================
	// operator +
	// ================
	binary_arithmetic_entry(plus, int8_),
	binary_arithmetic_entry(plus, int16_),
	binary_arithmetic_entry(plus, int32_),
	binary_arithmetic_entry(plus, int64_),
	binary_arithmetic_entry(plus, uint8_),
	binary_arithmetic_entry(plus, uint16_),
	binary_arithmetic_entry(plus, uint32_),
	binary_arithmetic_entry(plus, uint64_),
	binary_arithmetic_entry(plus, float32_),
	binary_arithmetic_entry(plus, float64_),
	binary_kind_entry(plus, char_, int64_,  char_),
	binary_kind_entry(plus, char_, uint64_, char_),
	binary_kind_entry(plus, int64_,  char_, char_),
	binary_kind_entry(plus, uint64_, char_, char_),
	builtin_binary_operator_table_entry_t(lex::token::plus, generic_arg_kind::ptr, type_info::int64_),
	builtin_binary_operator_table_entry_t(lex::token::plus, generic_arg_kind::ptr, type_info::uint64_),
	builtin_binary_operator_table_entry_t(lex::token::plus, type_info::int64_,  generic_arg_kind::ptr),
	builtin_binary_operator_table_entry_t(lex::token::plus, type_info::uint64_, generic_arg_kind::ptr),
	builtin_binary_operator_table_entry_t(lex::token::plus, generic_arg_kind::ptr_const, type_info::int64_),
	builtin_binary_operator_table_entry_t(lex::token::plus, generic_arg_kind::ptr_const, type_info::uint64_),
	builtin_binary_operator_table_entry_t(lex::token::plus, type_info::int64_,  generic_arg_kind::ptr_const),
	builtin_binary_operator_table_entry_t(lex::token::plus, type_info::uint64_, generic_arg_kind::ptr_const),

	// ================
	// operator +=
	// ================
	binary_arithmetic_entry(plus_eq, int8_),
	binary_arithmetic_entry(plus_eq, int16_),
	binary_arithmetic_entry(plus_eq, int32_),
	binary_arithmetic_entry(plus_eq, int64_),
	binary_arithmetic_entry(plus_eq, uint8_),
	binary_arithmetic_entry(plus_eq, uint16_),
	binary_arithmetic_entry(plus_eq, uint32_),
	binary_arithmetic_entry(plus_eq, uint64_),
	binary_arithmetic_entry(plus_eq, float32_),
	binary_arithmetic_entry(plus_eq, float64_),
	binary_kind_entry(plus_eq, char_, int64_,  char_),
	binary_kind_entry(plus_eq, char_, uint64_, char_),
	builtin_binary_operator_table_entry_t(lex::token::plus_eq, generic_arg_kind::ptr, type_info::int64_),
	builtin_binary_operator_table_entry_t(lex::token::plus_eq, generic_arg_kind::ptr, type_info::uint64_),
	builtin_binary_operator_table_entry_t(lex::token::plus_eq, generic_arg_kind::ptr_const, type_info::int64_),
	builtin_binary_operator_table_entry_t(lex::token::plus_eq, generic_arg_kind::ptr_const, type_info::uint64_),

	// ================
	// operator -
	// ================
	binary_arithmetic_entry(minus, int8_),
	binary_arithmetic_entry(minus, int16_),
	binary_arithmetic_entry(minus, int32_),
	binary_arithmetic_entry(minus, int64_),
	binary_arithmetic_entry(minus, uint8_),
	binary_arithmetic_entry(minus, uint16_),
	binary_arithmetic_entry(minus, uint32_),
	binary_arithmetic_entry(minus, uint64_),
	binary_arithmetic_entry(minus, float32_),
	binary_arithmetic_entry(minus, float64_),
	binary_kind_entry(minus, char_, int64_,  char_),
	binary_kind_entry(minus, char_, uint64_, char_),
	binary_kind_entry(minus, char_, char_, int32_),
	builtin_binary_operator_table_entry_t(lex::token::minus, generic_arg_kind::ptr, type_info::int64_),
	builtin_binary_operator_table_entry_t(lex::token::minus, generic_arg_kind::ptr, type_info::uint64_),
	builtin_binary_operator_table_entry_t(lex::token::minus, generic_arg_kind::ptr_const, type_info::int64_),
	builtin_binary_operator_table_entry_t(lex::token::minus, generic_arg_kind::ptr_const, type_info::uint64_),
	builtin_binary_operator_table_entry_t(lex::token::minus, generic_arg_kind::ptr_const, generic_arg_kind::ptr_const, type_info::int64_),

	// ================
	// operator -=
	// ================
	binary_arithmetic_entry(minus_eq, int8_),
	binary_arithmetic_entry(minus_eq, int16_),
	binary_arithmetic_entry(minus_eq, int32_),
	binary_arithmetic_entry(minus_eq, int64_),
	binary_arithmetic_entry(minus_eq, uint8_),
	binary_arithmetic_entry(minus_eq, uint16_),
	binary_arithmetic_entry(minus_eq, uint32_),
	binary_arithmetic_entry(minus_eq, uint64_),
	binary_arithmetic_entry(minus_eq, float32_),
	binary_arithmetic_entry(minus_eq, float64_),
	binary_kind_entry(minus_eq, char_, int64_,  char_),
	binary_kind_entry(minus_eq, char_, uint64_, char_),
	builtin_binary_operator_table_entry_t(lex::token::minus_eq, generic_arg_kind::ptr, type_info::int64_),
	builtin_binary_operator_table_entry_t(lex::token::minus_eq, generic_arg_kind::ptr, type_info::uint64_),
	builtin_binary_operator_table_entry_t(lex::token::minus_eq, generic_arg_kind::ptr_const, type_info::int64_),
	builtin_binary_operator_table_entry_t(lex::token::minus_eq, generic_arg_kind::ptr_const, type_info::uint64_),

	// ================
	// operator *
	// ================
	binary_arithmetic_entry(multiply, int8_),
	binary_arithmetic_entry(multiply, int16_),
	binary_arithmetic_entry(multiply, int32_),
	binary_arithmetic_entry(multiply, int64_),
	binary_arithmetic_entry(multiply, uint8_),
	binary_arithmetic_entry(multiply, uint16_),
	binary_arithmetic_entry(multiply, uint32_),
	binary_arithmetic_entry(multiply, uint64_),
	binary_arithmetic_entry(multiply, float32_),
	binary_arithmetic_entry(multiply, float64_),

	// ================
	// operator *=
	// ================
	binary_arithmetic_entry(multiply_eq, int8_),
	binary_arithmetic_entry(multiply_eq, int16_),
	binary_arithmetic_entry(multiply_eq, int32_),
	binary_arithmetic_entry(multiply_eq, int64_),
	binary_arithmetic_entry(multiply_eq, uint8_),
	binary_arithmetic_entry(multiply_eq, uint16_),
	binary_arithmetic_entry(multiply_eq, uint32_),
	binary_arithmetic_entry(multiply_eq, uint64_),
	binary_arithmetic_entry(multiply_eq, float32_),
	binary_arithmetic_entry(multiply_eq, float64_),

	// ================
	// operator /
	// ================
	binary_arithmetic_entry(divide, int8_),
	binary_arithmetic_entry(divide, int16_),
	binary_arithmetic_entry(divide, int32_),
	binary_arithmetic_entry(divide, int64_),
	binary_arithmetic_entry(divide, uint8_),
	binary_arithmetic_entry(divide, uint16_),
	binary_arithmetic_entry(divide, uint32_),
	binary_arithmetic_entry(divide, uint64_),
	binary_arithmetic_entry(divide, float32_),
	binary_arithmetic_entry(divide, float64_),

	// ================
	// operator /=
	// ================
	binary_arithmetic_entry(divide_eq, int8_),
	binary_arithmetic_entry(divide_eq, int16_),
	binary_arithmetic_entry(divide_eq, int32_),
	binary_arithmetic_entry(divide_eq, int64_),
	binary_arithmetic_entry(divide_eq, uint8_),
	binary_arithmetic_entry(divide_eq, uint16_),
	binary_arithmetic_entry(divide_eq, uint32_),
	binary_arithmetic_entry(divide_eq, uint64_),
	binary_arithmetic_entry(divide_eq, float32_),
	binary_arithmetic_entry(divide_eq, float64_),

	// ================
	// operator %
	// ================
	binary_arithmetic_entry(modulo, int8_),
	binary_arithmetic_entry(modulo, int16_),
	binary_arithmetic_entry(modulo, int32_),
	binary_arithmetic_entry(modulo, int64_),
	binary_arithmetic_entry(modulo, uint8_),
	binary_arithmetic_entry(modulo, uint16_),
	binary_arithmetic_entry(modulo, uint32_),
	binary_arithmetic_entry(modulo, uint64_),

	// ================
	// operator %=
	// ================
	binary_arithmetic_entry(modulo_eq, int8_),
	binary_arithmetic_entry(modulo_eq, int16_),
	binary_arithmetic_entry(modulo_eq, int32_),
	binary_arithmetic_entry(modulo_eq, int64_),
	binary_arithmetic_entry(modulo_eq, uint8_),
	binary_arithmetic_entry(modulo_eq, uint16_),
	binary_arithmetic_entry(modulo_eq, uint32_),
	binary_arithmetic_entry(modulo_eq, uint64_),

	// ================
	// operator ==
	// ================
	binary_kind_entry(equals, int8_,   int8_,   bool_),
	binary_kind_entry(equals, int16_,  int16_,  bool_),
	binary_kind_entry(equals, int32_,  int32_,  bool_),
	binary_kind_entry(equals, int64_,  int64_,  bool_),
	binary_kind_entry(equals, uint8_,  uint8_,  bool_),
	binary_kind_entry(equals, uint16_, uint16_, bool_),
	binary_kind_entry(equals, uint32_, uint32_, bool_),
	binary_kind_entry(equals, uint64_, uint64_, bool_),
	binary_kind_entry(equals, float32_, float32_, bool_),
	binary_kind_entry(equals, float64_, float64_, bool_),
	binary_kind_entry(equals, char_, char_, bool_),
	binary_kind_entry(equals, str_, str_, bool_),
	binary_kind_entry(equals, bool_, bool_, bool_),
	binary_kind_entry(equals, null_t_, null_t_, bool_),
	builtin_binary_operator_table_entry_t(lex::token::equals, generic_arg_kind::ptr_const, generic_arg_kind::ptr_const, type_info::bool_),
	builtin_binary_operator_table_entry_t(lex::token::equals, type_info::null_t_, generic_arg_kind::ptr_const, type_info::bool_),
	builtin_binary_operator_table_entry_t(lex::token::equals, generic_arg_kind::ptr_const, type_info::null_t_, type_info::bool_),

	// ================
	// operator !=
	// ================
	binary_kind_entry(not_equals, int8_,   int8_,   bool_),
	binary_kind_entry(not_equals, int16_,  int16_,  bool_),
	binary_kind_entry(not_equals, int32_,  int32_,  bool_),
	binary_kind_entry(not_equals, int64_,  int64_,  bool_),
	binary_kind_entry(not_equals, uint8_,  uint8_,  bool_),
	binary_kind_entry(not_equals, uint16_, uint16_, bool_),
	binary_kind_entry(not_equals, uint32_, uint32_, bool_),
	binary_kind_entry(not_equals, uint64_, uint64_, bool_),
	binary_kind_entry(not_equals, float32_, float32_, bool_),
	binary_kind_entry(not_equals, float64_, float64_, bool_),
	binary_kind_entry(not_equals, char_, char_, bool_),
	binary_kind_entry(not_equals, str_, str_, bool_),
	binary_kind_entry(not_equals, bool_, bool_, bool_),
	binary_kind_entry(not_equals, null_t_, null_t_, bool_),
	builtin_binary_operator_table_entry_t(lex::token::not_equals, generic_arg_kind::ptr_const, generic_arg_kind::ptr_const, type_info::bool_),
	builtin_binary_operator_table_entry_t(lex::token::not_equals, type_info::null_t_, generic_arg_kind::ptr_const, type_info::bool_),
	builtin_binary_operator_table_entry_t(lex::token::not_equals, generic_arg_kind::ptr_const, type_info::null_t_, type_info::bool_),

	// ================
	// operator <
	// ================
	binary_kind_entry(less_than, int8_,   int8_,   bool_),
	binary_kind_entry(less_than, int16_,  int16_,  bool_),
	binary_kind_entry(less_than, int32_,  int32_,  bool_),
	binary_kind_entry(less_than, int64_,  int64_,  bool_),
	binary_kind_entry(less_than, uint8_,  uint8_,  bool_),
	binary_kind_entry(less_than, uint16_, uint16_, bool_),
	binary_kind_entry(less_than, uint32_, uint32_, bool_),
	binary_kind_entry(less_than, uint64_, uint64_, bool_),
	binary_kind_entry(less_than, float32_, float32_, bool_),
	binary_kind_entry(less_than, float64_, float64_, bool_),
	binary_kind_entry(less_than, char_, char_, bool_),
	builtin_binary_operator_table_entry_t(lex::token::less_than, generic_arg_kind::ptr_const, generic_arg_kind::ptr_const, type_info::bool_),

	// ================
	// operator <=
	// ================
	binary_kind_entry(less_than_eq, int8_,   int8_,   bool_),
	binary_kind_entry(less_than_eq, int16_,  int16_,  bool_),
	binary_kind_entry(less_than_eq, int32_,  int32_,  bool_),
	binary_kind_entry(less_than_eq, int64_,  int64_,  bool_),
	binary_kind_entry(less_than_eq, uint8_,  uint8_,  bool_),
	binary_kind_entry(less_than_eq, uint16_, uint16_, bool_),
	binary_kind_entry(less_than_eq, uint32_, uint32_, bool_),
	binary_kind_entry(less_than_eq, uint64_, uint64_, bool_),
	binary_kind_entry(less_than_eq, float32_, float32_, bool_),
	binary_kind_entry(less_than_eq, float64_, float64_, bool_),
	binary_kind_entry(less_than_eq, char_, char_, bool_),
	builtin_binary_operator_table_entry_t(lex::token::less_than_eq, generic_arg_kind::ptr_const, generic_arg_kind::ptr_const, type_info::bool_),

	// ================
	// operator >
	// ================
	binary_kind_entry(greater_than, int8_,   int8_,   bool_),
	binary_kind_entry(greater_than, int16_,  int16_,  bool_),
	binary_kind_entry(greater_than, int32_,  int32_,  bool_),
	binary_kind_entry(greater_than, int64_,  int64_,  bool_),
	binary_kind_entry(greater_than, uint8_,  uint8_,  bool_),
	binary_kind_entry(greater_than, uint16_, uint16_, bool_),
	binary_kind_entry(greater_than, uint32_, uint32_, bool_),
	binary_kind_entry(greater_than, uint64_, uint64_, bool_),
	binary_kind_entry(greater_than, float32_, float32_, bool_),
	binary_kind_entry(greater_than, float64_, float64_, bool_),
	binary_kind_entry(greater_than, char_, char_, bool_),
	builtin_binary_operator_table_entry_t(lex::token::greater_than, generic_arg_kind::ptr_const, generic_arg_kind::ptr_const, type_info::bool_),

	// ================
	// operator >=
	// ================
	binary_kind_entry(greater_than_eq, int8_,   int8_,   bool_),
	binary_kind_entry(greater_than_eq, int16_,  int16_,  bool_),
	binary_kind_entry(greater_than_eq, int32_,  int32_,  bool_),
	binary_kind_entry(greater_than_eq, int64_,  int64_,  bool_),
	binary_kind_entry(greater_than_eq, uint8_,  uint8_,  bool_),
	binary_kind_entry(greater_than_eq, uint16_, uint16_, bool_),
	binary_kind_entry(greater_than_eq, uint32_, uint32_, bool_),
	binary_kind_entry(greater_than_eq, uint64_, uint64_, bool_),
	binary_kind_entry(greater_than_eq, float32_, float32_, bool_),
	binary_kind_entry(greater_than_eq, float64_, float64_, bool_),
	binary_kind_entry(greater_than_eq, char_, char_, bool_),
	builtin_binary_operator_table_entry_t(lex::token::greater_than_eq, generic_arg_kind::ptr_const, generic_arg_kind::ptr_const, type_info::bool_),

	// ================
	// operator &
	// ================
	binary_arithmetic_entry(bit_and, uint8_),
	binary_arithmetic_entry(bit_and, uint16_),
	binary_arithmetic_entry(bit_and, uint32_),
	binary_arithmetic_entry(bit_and, uint64_),
	binary_arithmetic_entry(bit_and, bool_),

	// ================
	// operator &=
	// ================
	binary_arithmetic_entry(bit_and_eq, uint8_),
	binary_arithmetic_entry(bit_and_eq, uint16_),
	binary_arithmetic_entry(bit_and_eq, uint32_),
	binary_arithmetic_entry(bit_and_eq, uint64_),
	binary_arithmetic_entry(bit_and_eq, bool_),

	// ================
	// operator ^
	// ================
	binary_arithmetic_entry(bit_xor, uint8_),
	binary_arithmetic_entry(bit_xor, uint16_),
	binary_arithmetic_entry(bit_xor, uint32_),
	binary_arithmetic_entry(bit_xor, uint64_),
	binary_arithmetic_entry(bit_xor, bool_),

	// ================
	// operator ^=
	// ================
	binary_arithmetic_entry(bit_xor_eq, uint8_),
	binary_arithmetic_entry(bit_xor_eq, uint16_),
	binary_arithmetic_entry(bit_xor_eq, uint32_),
	binary_arithmetic_entry(bit_xor_eq, uint64_),
	binary_arithmetic_entry(bit_xor_eq, bool_),

	// ================
	// operator |
	// ================
	binary_arithmetic_entry(bit_or, uint8_),
	binary_arithmetic_entry(bit_or, uint16_),
	binary_arithmetic_entry(bit_or, uint32_),
	binary_arithmetic_entry(bit_or, uint64_),
	binary_arithmetic_entry(bit_or, bool_),

	// ================
	// operator |=
	// ================
	binary_arithmetic_entry(bit_or_eq, uint8_),
	binary_arithmetic_entry(bit_or_eq, uint16_),
	binary_arithmetic_entry(bit_or_eq, uint32_),
	binary_arithmetic_entry(bit_or_eq, uint64_),
	binary_arithmetic_entry(bit_or_eq, bool_),

	// ================
	// operator <<
	// ================
	binary_kind_entry(bit_left_shift, uint8_,   int64_, uint8_),
	binary_kind_entry(bit_left_shift, uint8_,  uint64_, uint8_),
	binary_kind_entry(bit_left_shift, uint16_,  int64_, uint16_),
	binary_kind_entry(bit_left_shift, uint16_, uint64_, uint16_),
	binary_kind_entry(bit_left_shift, uint32_,  int64_, uint32_),
	binary_kind_entry(bit_left_shift, uint32_, uint64_, uint32_),
	binary_kind_entry(bit_left_shift, uint64_,  int64_, uint64_),
	binary_kind_entry(bit_left_shift, uint64_, uint64_, uint64_),

	// ================
	// operator <<=
	// ================
	binary_kind_entry(bit_left_shift_eq, uint8_,   int64_, uint8_),
	binary_kind_entry(bit_left_shift_eq, uint8_,  uint64_, uint8_),
	binary_kind_entry(bit_left_shift_eq, uint16_,  int64_, uint16_),
	binary_kind_entry(bit_left_shift_eq, uint16_, uint64_, uint16_),
	binary_kind_entry(bit_left_shift_eq, uint32_,  int64_, uint32_),
	binary_kind_entry(bit_left_shift_eq, uint32_, uint64_, uint32_),
	binary_kind_entry(bit_left_shift_eq, uint64_,  int64_, uint64_),
	binary_kind_entry(bit_left_shift_eq, uint64_, uint64_, uint64_),

	// ================
	// operator >>
	// ================
	binary_kind_entry(bit_right_shift, uint8_,   int64_, uint8_),
	binary_kind_entry(bit_right_shift, uint8_,  uint64_, uint8_),
	binary_kind_entry(bit_right_shift, uint16_,  int64_, uint16_),
	binary_kind_entry(bit_right_shift, uint16_, uint64_, uint16_),
	binary_kind_entry(bit_right_shift, uint32_,  int64_, uint32_),
	binary_kind_entry(bit_right_shift, uint32_, uint64_, uint32_),
	binary_kind_entry(bit_right_shift, uint64_,  int64_, uint64_),
	binary_kind_entry(bit_right_shift, uint64_, uint64_, uint64_),

	// ================
	// operator >>=
	// ================
	binary_kind_entry(bit_right_shift_eq, uint8_,   int64_, uint8_),
	binary_kind_entry(bit_right_shift_eq, uint8_,  uint64_, uint8_),
	binary_kind_entry(bit_right_shift_eq, uint16_,  int64_, uint16_),
	binary_kind_entry(bit_right_shift_eq, uint16_, uint64_, uint16_),
	binary_kind_entry(bit_right_shift_eq, uint32_,  int64_, uint32_),
	binary_kind_entry(bit_right_shift_eq, uint32_, uint64_, uint32_),
	binary_kind_entry(bit_right_shift_eq, uint64_,  int64_, uint64_),
	binary_kind_entry(bit_right_shift_eq, uint64_, uint64_, uint64_),
};

#undef binary_arithmetic_entry
#undef binary_kind_entry


static constexpr bz::array builtin_unary_operators = {
	lex::token::plus,
	lex::token::minus,
	lex::token::address_of,
	lex::token::dereference,
	lex::token::bit_not,
	lex::token::bool_not,
	lex::token::plus_plus,
	lex::token::minus_minus,
};

static constexpr bz::array builtin_binary_operators = {
	lex::token::assign,
	lex::token::plus,
	lex::token::plus_eq,
	lex::token::minus,
	lex::token::minus_eq,
	lex::token::multiply,
	lex::token::multiply_eq,
	lex::token::divide,
	lex::token::divide_eq,
	lex::token::modulo,
	lex::token::modulo_eq,
	lex::token::equals,
	lex::token::not_equals,
	lex::token::less_than,
	lex::token::less_than_eq,
	lex::token::greater_than,
	lex::token::greater_than_eq,
	lex::token::bit_and,
	lex::token::bit_and_eq,
	lex::token::bit_xor,
	lex::token::bit_xor_eq,
	lex::token::bit_or,
	lex::token::bit_or_eq,
	lex::token::bit_left_shift,
	lex::token::bit_left_shift_eq,
	lex::token::bit_right_shift,
	lex::token::bit_right_shift_eq,
};

static constexpr auto unique_builtin_operators = []() {
	constexpr auto unique_operator_count = builtin_binary_operators.size() + builtin_unary_operators.size()
		- builtin_unary_operators.filter([](auto const kind) { return builtin_binary_operators.contains(kind); }).count();
	bz::array<uint32_t, unique_operator_count> result{};

	for (size_t i = 0; i < builtin_binary_operators.size(); ++i)
	{
		result[i] = builtin_binary_operators[i];
	}

	for (
		auto const [unary_kind, index]
			: builtin_unary_operators.filter([](auto const kind) {
				return !builtin_binary_operators.contains(kind);
			}).enumerate()
	)
	{
		result[builtin_binary_operators.size() + index] = unary_kind;
	}

	return result;
}();


static bz::array_view<builtin_unary_operator_table_entry_t const> get_builtin_unary_operator_range(uint32_t kind)
{
	auto it = builtin_unary_operator_table.begin();
	auto const end = builtin_unary_operator_table.end();

	while (it != end && it->op_kind != kind)
	{
		++it;
	}
	if (it == end)
	{
		return {};
	}
	auto const start_it = it;
	while (it != end && it->op_kind == kind)
	{
		++it;
	}
	auto const end_it = it;
	return { start_it, end_it };
}

static bz::array_view<builtin_binary_operator_table_entry_t const> get_builtin_binary_operator_range(uint32_t kind)
{
	auto it = builtin_binary_operator_table.begin();
	auto const end = builtin_binary_operator_table.end();

	while (it != end && it->op_kind != kind)
	{
		++it;
	}
	if (it == end)
	{
		return {};
	}
	auto const start_it = it;
	while (it != end && it->op_kind == kind)
	{
		++it;
	}
	auto const end_it = it;
	return { start_it, end_it };
}

static bool is_unary_op_reference_like(uint32_t op_kind)
{
	switch (op_kind)
	{
	case lex::token::plus_plus:
	case lex::token::minus_minus:
		return true;
	default:
		return false;
	}
}

static bool is_binary_op_assign_like(uint32_t op_kind)
{
	switch (op_kind)
	{
	case lex::token::assign:
	case lex::token::plus_eq:
	case lex::token::minus_eq:
	case lex::token::multiply_eq:
	case lex::token::divide_eq:
	case lex::token::modulo_eq:
	case lex::token::bit_and_eq:
	case lex::token::bit_xor_eq:
	case lex::token::bit_or_eq:
	case lex::token::bit_left_shift_eq:
	case lex::token::bit_right_shift_eq:
		return true;
	default:
		return false;
	}
}

static typespec generic_arg_kind_as_type(generic_arg_kind kind)
{
	typespec result = make_auto_typespec(nullptr);
	switch (kind)
	{
	case generic_arg_kind::none:
		bz_unreachable;
	case generic_arg_kind::ptr:
		result.add_layer<ts_pointer>();
		break;
	case generic_arg_kind::ptr_const:
		result.add_layer<ts_const>();
		result.add_layer<ts_pointer>();
		break;
	case generic_arg_kind::ref:
		result.add_layer<ts_lvalue_reference>();
		break;
	case generic_arg_kind::ref_const:
		result.add_layer<ts_const>();
		result.add_layer<ts_lvalue_reference>();
		break;
	case generic_arg_kind::slice:
		result = make_array_slice_typespec({}, std::move(result));
		break;
	case generic_arg_kind::slice_const:
		result.add_layer<ts_const>();
		result = make_array_slice_typespec({}, std::move(result));
		break;
	}
	return result;
}

static function_body make_builtin_operator_function_body(
	builtin_unary_operator_table_entry_t const &builtin_unary_op,
	bz::array_view<type_info> builtin_type_infos
)
{
	function_body result;
	result.params.resize(1);
	result.params[0].id_and_type.var_type = type_as_expression([&]() {
		if (builtin_unary_op.expr_generic_t == generic_arg_kind::none)
		{
			return make_base_type_typespec({}, &builtin_type_infos[builtin_unary_op.expr_t]);
		}
		else
		{
			return generic_arg_kind_as_type(builtin_unary_op.expr_generic_t);
		}
	}());
	result.params[0].state = resolve_state::symbol;
	result.return_type = [&]() {
		if (builtin_unary_op.res_t != type_info::aggregate)
		{
			return make_base_type_typespec({}, &builtin_type_infos[builtin_unary_op.res_t]);
		}
		else
		{
			return typespec();
		}
	}();
	if (is_unary_op_reference_like(builtin_unary_op.op_kind))
	{
		result.params[0].get_type().add_layer<ts_lvalue_reference>();
		if (!result.return_type.is_empty())
		{
			result.return_type.add_layer<ts_lvalue_reference>();
		}
	}
	result.function_name_or_operator_kind = builtin_unary_op.op_kind;
	result.intrinsic_kind = builtin_unary_op.intrinsic_kind;
	result.flags |= function_body::intrinsic | function_body::builtin_operator;
	if (is_generic_parameter(result.params[0]))
	{
		result.state = resolve_state::parameters;
		result.flags |= function_body::generic;
	}
	else
	{
		result.state = resolve_state::symbol;
		result.resolve_symbol_name();
	}

	result.scope = make_local_scope({});

	return result;
}

static function_body make_builtin_operator_function_body(
	builtin_binary_operator_table_entry_t const &builtin_binary_op,
	bz::array_view<type_info> builtin_type_infos
)
{
	function_body result;
	result.params.resize(2);
	result.params[0].id_and_type.var_type = type_as_expression([&]() {
		if (builtin_binary_op.lhs_generic_t == generic_arg_kind::none)
		{
			return make_base_type_typespec({}, &builtin_type_infos[builtin_binary_op.lhs_t]);
		}
		else
		{
			return generic_arg_kind_as_type(builtin_binary_op.lhs_generic_t);
		}
	}());
	result.params[1].id_and_type.var_type = type_as_expression([&]() {
		if (builtin_binary_op.rhs_generic_t == generic_arg_kind::none)
		{
			return make_base_type_typespec({}, &builtin_type_infos[builtin_binary_op.rhs_t]);
		}
		else
		{
			return generic_arg_kind_as_type(builtin_binary_op.rhs_generic_t);
		}
	}());
	result.params[0].state = resolve_state::symbol;
	result.return_type = [&]() {
		if (builtin_binary_op.res_t != type_info::aggregate)
		{
			return make_base_type_typespec({}, &builtin_type_infos[builtin_binary_op.res_t]);
		}
		else
		{
			return typespec();
		}
	}();
	if (is_binary_op_assign_like(builtin_binary_op.op_kind))
	{
		result.params[0].get_type().add_layer<ts_lvalue_reference>();
		if (!result.return_type.is_empty())
		{
			result.return_type.add_layer<ts_lvalue_reference>();
		}
	}
	result.function_name_or_operator_kind = builtin_binary_op.op_kind;
	result.state = resolve_state::symbol;
	result.intrinsic_kind = builtin_binary_op.intrinsic_kind;
	result.flags |= function_body::intrinsic | function_body::builtin_operator;
	if (is_generic_parameter(result.params[0]) || is_generic_parameter(result.params[1]))
	{
		result.state = resolve_state::parameters;
		result.flags |= function_body::generic;
	}
	else
	{
		result.state = resolve_state::symbol;
		result.resolve_symbol_name();
	}

	return result;
}

static builtin_operator make_builtin_operator(uint32_t kind, bz::array_view<type_info> builtin_type_infos)
{
	auto const unary_range  = get_builtin_unary_operator_range(kind);
	auto const binary_range = get_builtin_binary_operator_range(kind);

	builtin_operator result;
	result.op = kind;

	result.decls.reserve(unary_range.size() + binary_range.size());
	for (auto const &builtin_unary_op : unary_range)
	{
		result.decls.emplace_back(
			bz::vector<bz::u8string_view>{},
			nullptr,
			make_builtin_operator_function_body(builtin_unary_op, builtin_type_infos)
		);
	}
	for (auto const &builtin_binary_op : binary_range)
	{
		result.decls.emplace_back(
			bz::vector<bz::u8string_view>{},
			nullptr,
			make_builtin_operator_function_body(builtin_binary_op, builtin_type_infos)
		);
	}

	return result;
}


bz::vector<builtin_operator> make_builtin_operators(bz::array_view<type_info> builtin_type_infos)
{
	auto result = unique_builtin_operators.transform([builtin_type_infos](auto const kind) {
		return make_builtin_operator(kind, builtin_type_infos);
	}).collect();

	bz_assert(result.is_all([](auto const &builtin_op) {
		return builtin_op.decls.is_all([op = builtin_op.op](auto const &decl) {
			return decl.body.function_name_or_operator_kind == op;
		});
	}));

	return result;
}

} // namespace ast
