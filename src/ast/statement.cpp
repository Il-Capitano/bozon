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
	else
	{
		return bz::format("candidate '{}'", this->get_signature());
	}
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
	bz_assert(!body->is_generic());
	bz_assert(body->params.size() == this->params.size() || (!this->params.empty() && this->params.back().get_type().is<ast::ts_variadic>()));
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
			return is_equal_params(specialization->params, body->params);
		}
	);

	if (it == this->generic_specializations.end())
	{
		body->generic_parent = this;
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
				typespec_view const arg_type = func_body->params[0].get_type();
				bz_assert(arg_type.is<ts_array_slice>());
				func_body->return_type = arg_type.get<ts_array_slice>().elem_type;
				func_body->return_type.add_layer<ts_pointer>();
				break;
			}
			case builtin_slice_from_ptrs:
			case builtin_slice_from_const_ptrs:
			{
				bz_assert(func_body->params.size() == 2);
				typespec_view const arg_type = func_body->params[0].get_type();
				bz_assert(arg_type.is<ts_pointer>());
				func_body->return_type = make_array_slice_typespec({}, arg_type.get<ts_pointer>());
				break;
			}
			case builtin_pointer_cast:
			{
				bz_assert(func_body->params.size() == 2);
				bz_assert(func_body->params[0].init_expr.is_typename());
				auto const result_type = func_body->params[0].init_expr.get_typename().as_typespec_view();
				func_body->return_type = result_type;
				break;
			}
			case builtin_int_to_pointer:
			{
				bz_assert(func_body->params.size() == 2);
				bz_assert(func_body->params[0].init_expr.is_typename());
				auto const result_type = func_body->params[0].init_expr.get_typename().as_typespec_view();
				func_body->return_type = result_type;
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

			static_assert(_builtin_last - _builtin_first == 121);
			default:
				break;
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

type_info::function_body_ptr type_info::make_default_op_assign(lex::src_tokens src_tokens, type_info &info)
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

	auto result = make_ast_unique<function_body>();
	result->params.reserve(2);
	result->params.emplace_back(
		lex::src_tokens{},
		lex::token_range{},
		var_id_and_type(identifier{}, type_as_expression(std::move(lhs_t)))
	);
	result->params.emplace_back(
		lex::src_tokens{},
		lex::token_range{},
		var_id_and_type(identifier{}, type_as_expression(std::move(rhs_t)))
	);
	result->return_type = std::move(ret_t);
	result->function_name_or_operator_kind = lex::token::assign;
	result->src_tokens = src_tokens;
	result->state = resolve_state::symbol;
	result->flags |= function_body::default_op_assign;
	return result;
}

type_info::function_body_ptr type_info::make_default_op_move_assign(lex::src_tokens src_tokens, type_info &info)
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

	auto result = make_ast_unique<function_body>();
	result->params.reserve(2);
	result->params.emplace_back(
		lex::src_tokens{},
		lex::token_range{},
		var_id_and_type(identifier{}, type_as_expression(std::move(lhs_t)))
	);
	result->params.emplace_back(
		lex::src_tokens{},
		lex::token_range{},
		var_id_and_type(identifier{}, type_as_expression(std::move(rhs_t)))
	);
	result->return_type = std::move(ret_t);
	result->function_name_or_operator_kind = lex::token::assign;
	result->src_tokens = src_tokens;
	result->state = resolve_state::symbol;
	result->flags |= function_body::default_op_move_assign;
	return result;
}

type_info::function_body_ptr type_info::make_default_copy_constructor(lex::src_tokens src_tokens, type_info &info)
{
	auto param_t = make_base_type_typespec({}, &info);
	param_t.add_layer<ts_const>();
	param_t.add_layer<ts_lvalue_reference>();

	auto ret_t = make_base_type_typespec({}, &info);

	auto result = make_ast_unique<function_body>();
	result->params.emplace_back(
		lex::src_tokens{},
		lex::token_range{},
		var_id_and_type(identifier{}, type_as_expression(std::move(param_t)))
	);
	result->return_type = std::move(ret_t);
	result->src_tokens = src_tokens;
	result->state = resolve_state::symbol;
	result->flags |= function_body::default_copy_constructor;
	result->flags |= function_body::constructor;
	result->constructor_or_destructor_of = &info;
	return result;
}

type_info::function_body_ptr type_info::make_default_default_constructor(lex::src_tokens src_tokens, type_info &info)
{
	auto param_t = make_base_type_typespec({}, &info);
	param_t.add_layer<ts_const>();
	param_t.add_layer<ts_lvalue_reference>();

	auto ret_t = make_base_type_typespec({}, &info);

	auto result = make_ast_unique<function_body>();
	result->return_type = std::move(ret_t);
	result->src_tokens = src_tokens;
	result->state = resolve_state::symbol;
	result->flags |= function_body::default_default_constructor;
	result->flags |= function_body::constructor;
	result->constructor_or_destructor_of = &info;
	return result;
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
	bz::vector<decl_variable> params;
	params.reserve(sizeof... (Ts));
	((params.emplace_back(
		lex::src_tokens{}, lex::token_range{},
		var_id_and_type(identifier{}, type_as_expression(std::move(arg_types)))
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

bz::vector<function_body> make_builtin_functions(bz::array_view<type_info> builtin_type_infos, size_t pointer_size)
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

#define add_builtin(pos, kind, symbol_name, ...) \
((void)([]() { static_assert(kind == pos); }), create_builtin_function(kind, symbol_name, __VA_ARGS__))
	bz::vector result = {
		add_builtin( 0, function_body::builtin_str_eq,          "", bool_type,  str_type, str_type),
		add_builtin( 1, function_body::builtin_str_neq,         "", bool_type,  str_type, str_type),
		add_builtin( 2, function_body::builtin_str_length,      "", usize_type, str_type),
		add_builtin( 3, function_body::builtin_str_starts_with, "", bool_type,  str_type, str_type),
		add_builtin( 4, function_body::builtin_str_ends_with,   "", bool_type,  str_type, str_type),

		add_builtin( 5, function_body::builtin_str_begin_ptr,         "", uint8_const_ptr_type, str_type),
		add_builtin( 6, function_body::builtin_str_end_ptr,           "", uint8_const_ptr_type, str_type),
		add_builtin( 7, function_body::builtin_str_size,              "", usize_type, str_type),
		add_builtin( 8, function_body::builtin_str_from_ptrs,         "", str_type, uint8_const_ptr_type, uint8_const_ptr_type),

		add_builtin( 9, function_body::builtin_slice_begin_ptr,       "", {}, slice_auto_type),
		add_builtin(10, function_body::builtin_slice_begin_const_ptr, "", {}, slice_const_auto_type),
		add_builtin(11, function_body::builtin_slice_end_ptr,         "", {}, slice_auto_type),
		add_builtin(12, function_body::builtin_slice_end_const_ptr,   "", {}, slice_const_auto_type),
		add_builtin(13, function_body::builtin_slice_size,            "", usize_type, slice_const_auto_type),
		add_builtin(14, function_body::builtin_slice_from_ptrs,       "", {}, auto_ptr_type, auto_ptr_type),
		add_builtin(15, function_body::builtin_slice_from_const_ptrs, "", {}, auto_const_ptr_type, auto_const_ptr_type),

		add_builtin(16, function_body::builtin_pointer_cast,   "", {}, typename_ptr_type, void_const_ptr_type),
		add_builtin(17, function_body::builtin_pointer_to_int, "", usize_type, void_const_ptr_type),
		add_builtin(18, function_body::builtin_int_to_pointer, "", {}, typename_ptr_type, usize_type),

		add_builtin(19, function_body::builtin_call_destructor,   "", void_type, ref_auto_type),
		add_builtin(20, function_body::builtin_inplace_construct, "", void_type, auto_ptr_type, auto_type),

		add_builtin(21, function_body::builtin_is_comptime, "", bool_type),
		add_builtin(22, function_body::builtin_panic,       "__bozon_builtin_panic", void_type),

		add_builtin(23, function_body::print_stdout,   "__bozon_builtin_print_stdout",   void_type, str_type),
		add_builtin(24, function_body::println_stdout, "__bozon_builtin_println_stdout", void_type, str_type),
		add_builtin(25, function_body::print_stderr,   "__bozon_builtin_print_stderr",   void_type, str_type),
		add_builtin(26, function_body::println_stderr, "__bozon_builtin_println_stderr", void_type, str_type),

		add_builtin(27, function_body::comptime_malloc,      "__bozon_builtin_comptime_malloc", void_ptr_type, usize_type),
		add_builtin(28, function_body::comptime_malloc_type, "", {}, typename_type, usize_type),
		add_builtin(29, function_body::comptime_free,        "__bozon_builtin_comptime_free",   void_type, void_ptr_type),

		add_builtin(30, function_body::comptime_compile_error,   "", void_type, str_type),
		add_builtin(31, function_body::comptime_compile_warning, "", void_type, str_type),

		add_builtin(32, function_body::comptime_compile_error_src_tokens,   "", void_type, str_type, uint64_type, uint64_type, uint64_type),
		add_builtin(33, function_body::comptime_compile_warning_src_tokens, "", void_type, str_type, uint64_type, uint64_type, uint64_type),

		add_builtin(34, function_body::comptime_create_global_string, "", str_type, str_type),

		add_builtin(35, function_body::memcpy,  "llvm.memcpy.p0i8.p0i8.i64",  void_type, void_ptr_type, void_const_ptr_type, uint64_type),
		add_builtin(36, function_body::memmove, "llvm.memmove.p0i8.p0i8.i64", void_type, void_ptr_type, void_const_ptr_type, uint64_type),
		add_builtin(37, function_body::memset,  "llvm.memset.p0i8.i64", void_type, void_ptr_type, uint8_type, uint64_type),

		add_builtin(38, function_body::exp_f32,   "llvm.exp.f32",   float32_type, float32_type),
		add_builtin(39, function_body::exp_f64,   "llvm.exp.f64",   float64_type, float64_type),
		add_builtin(40, function_body::exp2_f32,  "llvm.exp2.f32",  float32_type, float32_type),
		add_builtin(41, function_body::exp2_f64,  "llvm.exp2.f64",  float64_type, float64_type),
		add_builtin(42, function_body::expm1_f32, "expm1f",         float32_type, float32_type),
		add_builtin(43, function_body::expm1_f64, "expm1",          float64_type, float64_type),
		add_builtin(44, function_body::log_f32,   "llvm.log.f32",   float32_type, float32_type),
		add_builtin(45, function_body::log_f64,   "llvm.log.f64",   float64_type, float64_type),
		add_builtin(46, function_body::log10_f32, "llvm.log10.f32", float32_type, float32_type),
		add_builtin(47, function_body::log10_f64, "llvm.log10.f64", float64_type, float64_type),
		add_builtin(48, function_body::log2_f32,  "llvm.log2.f32",  float32_type, float32_type),
		add_builtin(49, function_body::log2_f64,  "llvm.log2.f64",  float64_type, float64_type),
		add_builtin(50, function_body::log1p_f32, "log1pf",         float32_type, float32_type),
		add_builtin(51, function_body::log1p_f64, "log1p",          float64_type, float64_type),

		add_builtin(52, function_body::sqrt_f32,  "llvm.sqrt.f32", float32_type, float32_type),
		add_builtin(53, function_body::sqrt_f64,  "llvm.sqrt.f64", float64_type, float64_type),
		add_builtin(54, function_body::pow_f32,   "llvm.pow.f32",  float32_type, float32_type, float32_type),
		add_builtin(55, function_body::pow_f64,   "llvm.pow.f64",  float64_type, float64_type, float64_type),
		add_builtin(56, function_body::cbrt_f32,  "cbrtf",         float32_type, float32_type),
		add_builtin(57, function_body::cbrt_f64,  "cbrt",          float64_type, float64_type),
		add_builtin(58, function_body::hypot_f32, "hypotf",        float32_type, float32_type, float32_type),
		add_builtin(59, function_body::hypot_f64, "hypot",         float64_type, float64_type, float64_type),

		add_builtin(60, function_body::sin_f32,   "llvm.sin.f32", float32_type, float32_type),
		add_builtin(61, function_body::sin_f64,   "llvm.sin.f64", float64_type, float64_type),
		add_builtin(62, function_body::cos_f32,   "llvm.cos.f32", float32_type, float32_type),
		add_builtin(63, function_body::cos_f64,   "llvm.cos.f64", float64_type, float64_type),
		add_builtin(64, function_body::tan_f32,   "tanf",         float32_type, float32_type),
		add_builtin(65, function_body::tan_f64,   "tan",          float64_type, float64_type),
		add_builtin(66, function_body::asin_f32,  "asinf",        float32_type, float32_type),
		add_builtin(67, function_body::asin_f64,  "asin",         float64_type, float64_type),
		add_builtin(68, function_body::acos_f32,  "acosf",        float32_type, float32_type),
		add_builtin(69, function_body::acos_f64,  "acos",         float64_type, float64_type),
		add_builtin(70, function_body::atan_f32,  "atanf",        float32_type, float32_type),
		add_builtin(71, function_body::atan_f64,  "atan",         float64_type, float64_type),
		add_builtin(72, function_body::atan2_f32, "atan2f",       float32_type, float32_type, float32_type),
		add_builtin(73, function_body::atan2_f64, "atan2",        float64_type, float64_type, float64_type),

		add_builtin(74, function_body::sinh_f32,  "sinhf",  float32_type, float32_type),
		add_builtin(75, function_body::sinh_f64,  "sinh",   float64_type, float64_type),
		add_builtin(76, function_body::cosh_f32,  "coshf",  float32_type, float32_type),
		add_builtin(77, function_body::cosh_f64,  "cosh",   float64_type, float64_type),
		add_builtin(78, function_body::tanh_f32,  "tanhf",  float32_type, float32_type),
		add_builtin(79, function_body::tanh_f64,  "tanh",   float64_type, float64_type),
		add_builtin(80, function_body::asinh_f32, "asinhf", float32_type, float32_type),
		add_builtin(81, function_body::asinh_f64, "asinh",  float64_type, float64_type),
		add_builtin(82, function_body::acosh_f32, "acoshf", float32_type, float32_type),
		add_builtin(83, function_body::acosh_f64, "acosh",  float64_type, float64_type),
		add_builtin(84, function_body::atanh_f32, "atanhf", float32_type, float32_type),
		add_builtin(85, function_body::atanh_f64, "atanh",  float64_type, float64_type),

		add_builtin(86, function_body::erf_f32,    "erff",    float32_type, float32_type),
		add_builtin(87, function_body::erf_f64,    "erf",     float64_type, float64_type),
		add_builtin(88, function_body::erfc_f32,   "erfcf",   float32_type, float32_type),
		add_builtin(89, function_body::erfc_f64,   "erfc",    float64_type, float64_type),
		add_builtin(90, function_body::tgamma_f32, "tgammaf", float32_type, float32_type),
		add_builtin(91, function_body::tgamma_f64, "tgamma",  float64_type, float64_type),
		add_builtin(92, function_body::lgamma_f32, "lgammaf", float32_type, float32_type),
		add_builtin(93, function_body::lgamma_f64, "lgamma",  float64_type, float64_type),

		add_builtin( 94, function_body::bitreverse_u8,  "llvm.bitreverse.i8",  uint8_type,  uint8_type),
		add_builtin( 95, function_body::bitreverse_u16, "llvm.bitreverse.i16", uint16_type, uint16_type),
		add_builtin( 96, function_body::bitreverse_u32, "llvm.bitreverse.i32", uint32_type, uint32_type),
		add_builtin( 97, function_body::bitreverse_u64, "llvm.bitreverse.i64", uint64_type, uint64_type),
		add_builtin( 98, function_body::popcount_u8,    "llvm.ctpop.i8",  uint8_type,  uint8_type),
		add_builtin( 99, function_body::popcount_u16,   "llvm.ctpop.i16", uint16_type, uint16_type),
		add_builtin(100, function_body::popcount_u32,   "llvm.ctpop.i32", uint32_type, uint32_type),
		add_builtin(101, function_body::popcount_u64,   "llvm.ctpop.i64", uint64_type, uint64_type),
		add_builtin(102, function_body::byteswap_u16,   "llvm.bswap.i16", uint16_type, uint16_type),
		add_builtin(103, function_body::byteswap_u32,   "llvm.bswap.i32", uint32_type, uint32_type),
		add_builtin(104, function_body::byteswap_u64,   "llvm.bswap.i64", uint64_type, uint64_type),

		add_builtin(105, function_body::clz_u8,   "llvm.ctlz.i8",  uint8_type,  uint8_type),
		add_builtin(106, function_body::clz_u16,  "llvm.ctlz.i16", uint16_type, uint16_type),
		add_builtin(107, function_body::clz_u32,  "llvm.ctlz.i32", uint32_type, uint32_type),
		add_builtin(108, function_body::clz_u64,  "llvm.ctlz.i64", uint64_type, uint64_type),
		add_builtin(109, function_body::ctz_u8,   "llvm.cttz.i8",  uint8_type,  uint8_type),
		add_builtin(110, function_body::ctz_u16,  "llvm.cttz.i16", uint16_type, uint16_type),
		add_builtin(111, function_body::ctz_u32,  "llvm.cttz.i32", uint32_type, uint32_type),
		add_builtin(112, function_body::ctz_u64,  "llvm.cttz.i64", uint64_type, uint64_type),
		add_builtin(113, function_body::fshl_u8,  "llvm.fshl.i8",  uint8_type,  uint8_type,  uint8_type,  uint8_type),
		add_builtin(114, function_body::fshl_u16, "llvm.fshl.i16", uint16_type, uint16_type, uint16_type, uint16_type),
		add_builtin(115, function_body::fshl_u32, "llvm.fshl.i32", uint32_type, uint32_type, uint32_type, uint32_type),
		add_builtin(116, function_body::fshl_u64, "llvm.fshl.i64", uint64_type, uint64_type, uint64_type, uint64_type),
		add_builtin(117, function_body::fshr_u8,  "llvm.fshr.i8",  uint8_type,  uint8_type,  uint8_type,  uint8_type),
		add_builtin(118, function_body::fshr_u16, "llvm.fshr.i16", uint16_type, uint16_type, uint16_type, uint16_type),
		add_builtin(119, function_body::fshr_u32, "llvm.fshr.i32", uint32_type, uint32_type, uint32_type, uint32_type),
		add_builtin(120, function_body::fshr_u64, "llvm.fshr.i64", uint64_type, uint64_type, uint64_type, uint64_type),
	};
#undef add_builtin
	bz_assert(result.size() == intrinsic_info.size());

	result[function_body::comptime_compile_error]             .flags |= function_body::only_consteval;
	result[function_body::comptime_compile_warning]           .flags |= function_body::only_consteval;
	result[function_body::comptime_compile_error_src_tokens]  .flags |= function_body::only_consteval;
	result[function_body::comptime_compile_warning_src_tokens].flags |= function_body::only_consteval;

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

} // namespace ast
