#include "statement.h"
#include "token_info.h"

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

			static_assert(_builtin_last - _builtin_first == 77);
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
		lex::src_tokens{}, lex::token_pos{}, lex::token_range{},
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
	auto const bool_type    = make_base_type_typespec({}, get_builtin_type_info(type_info::bool_));
	auto const uint64_type  = make_base_type_typespec({}, get_builtin_type_info(type_info::uint64_));
	auto const uint8_type   = make_base_type_typespec({}, get_builtin_type_info(type_info::uint8_));
	auto const float32_type = make_base_type_typespec({}, get_builtin_type_info(type_info::float32_));
	auto const float64_type = make_base_type_typespec({}, get_builtin_type_info(type_info::float64_));
	auto const str_type     = make_base_type_typespec({}, get_builtin_type_info(type_info::str_));
	auto const void_type    = make_void_typespec(nullptr);
	auto const void_ptr_type = [&]() {
		typespec result = make_void_typespec(nullptr);
		result.add_layer<ts_pointer>(nullptr);
		return result;
	}();
	auto const void_const_ptr_type = [&]() {
		typespec result = make_void_typespec(nullptr);
		result.add_layer<ts_pointer>(nullptr);
		result.add_layer<ts_const>(nullptr);
		return result;
	}();
	auto const uint8_const_ptr_type = [&]() {
		typespec result = make_base_type_typespec({}, get_builtin_type_info(type_info::uint8_));
		result.add_layer<ts_const>(nullptr);
		result.add_layer<ts_pointer>(nullptr);
		return result;
	}();
	auto const auto_ptr_type = [&]() {
		typespec result = make_auto_typespec({});
		result.add_layer<ts_pointer>(nullptr);
		return result;
	}();
	auto const auto_const_ptr_type = [&]() {
		typespec result = make_auto_typespec({});
		result.add_layer<ts_const>(nullptr);
		result.add_layer<ts_pointer>(nullptr);
		return result;
	}();
	auto const slice_auto_type = [&]() {
		typespec auto_t = make_auto_typespec({});
		return make_array_slice_typespec({}, std::move(auto_t));
	}();
	auto const slice_const_auto_type = [&]() {
		typespec auto_t = make_auto_typespec({});
		auto_t.add_layer<ts_const>(nullptr);
		return make_array_slice_typespec({}, std::move(auto_t));
	}();

#define add_builtin(pos, kind, symbol_name, ...) \
((void)([]() { static_assert(kind == pos); }), create_builtin_function(kind, symbol_name, __VA_ARGS__))
	bz::array result = {
		add_builtin( 0, function_body::builtin_str_eq,     "__bozon_builtin_str_eq",     bool_type,   str_type, str_type),
		add_builtin( 1, function_body::builtin_str_neq,    "__bozon_builtin_str_neq",    bool_type,   str_type, str_type),
		add_builtin( 2, function_body::builtin_str_length, "__bozon_builtin_str_length", uint64_type, str_type),

		add_builtin( 3, function_body::builtin_str_begin_ptr,         "", uint8_const_ptr_type, str_type),
		add_builtin( 4, function_body::builtin_str_end_ptr,           "", uint8_const_ptr_type, str_type),
		add_builtin( 5, function_body::builtin_str_size,              "", uint64_type, str_type),
		add_builtin( 6, function_body::builtin_str_from_ptrs,         "", str_type, uint8_const_ptr_type, uint8_const_ptr_type),

		add_builtin( 7, function_body::builtin_slice_begin_ptr,       "", {}, slice_auto_type),
		add_builtin( 8, function_body::builtin_slice_begin_const_ptr, "", {}, slice_const_auto_type),
		add_builtin( 9, function_body::builtin_slice_end_ptr,         "", {}, slice_auto_type),
		add_builtin(10, function_body::builtin_slice_end_const_ptr,   "", {}, slice_const_auto_type),
		add_builtin(11, function_body::builtin_slice_size,            "", uint64_type, slice_const_auto_type),
		add_builtin(12, function_body::builtin_slice_from_ptrs,       "", {}, auto_ptr_type, auto_ptr_type),
		add_builtin(13, function_body::builtin_slice_from_const_ptrs, "", {}, auto_const_ptr_type, auto_const_ptr_type),

		add_builtin(14, function_body::print_stdout,   "__bozon_builtin_print_stdout",   void_type, str_type),
		add_builtin(15, function_body::println_stdout, "__bozon_builtin_println_stdout", void_type, str_type),
		add_builtin(16, function_body::print_stderr,   "__bozon_builtin_print_stderr",   void_type, str_type),
		add_builtin(17, function_body::println_stderr, "__bozon_builtin_println_stderr", void_type, str_type),

		add_builtin(18, function_body::memcpy,  "llvm.memcpy.p0i8.p0i8.i64",  void_type, void_ptr_type, void_const_ptr_type, uint64_type, bool_type),
		add_builtin(19, function_body::memmove, "llvm.memmove.p0i8.p0i8.i64", void_type, void_ptr_type, void_const_ptr_type, uint64_type, bool_type),
		add_builtin(20, function_body::memset,  "llvm.memset.p0i8.i64", void_type, void_ptr_type, uint8_type, uint64_type, bool_type),

		add_builtin(21, function_body::exp_f32,   "llvm.exp.f32",   float32_type, float32_type),
		add_builtin(22, function_body::exp_f64,   "llvm.exp.f64",   float64_type, float64_type),
		add_builtin(23, function_body::exp2_f32,  "llvm.exp2.f32",  float32_type, float32_type),
		add_builtin(24, function_body::exp2_f64,  "llvm.exp2.f64",  float64_type, float64_type),
		add_builtin(25, function_body::expm1_f32, "expm1f",         float32_type, float32_type),
		add_builtin(26, function_body::expm1_f64, "expm1",          float64_type, float64_type),
		add_builtin(27, function_body::log_f32,   "llvm.log.f32",   float32_type, float32_type),
		add_builtin(28, function_body::log_f64,   "llvm.log.f64",   float64_type, float64_type),
		add_builtin(29, function_body::log10_f32, "llvm.log10.f32", float32_type, float32_type),
		add_builtin(30, function_body::log10_f64, "llvm.log10.f64", float64_type, float64_type),
		add_builtin(31, function_body::log2_f32,  "llvm.log2.f32",  float32_type, float32_type),
		add_builtin(32, function_body::log2_f64,  "llvm.log2.f64",  float64_type, float64_type),
		add_builtin(33, function_body::log1p_f32, "log1pf",         float32_type, float32_type),
		add_builtin(34, function_body::log1p_f64, "log1p",          float64_type, float64_type),

		add_builtin(35, function_body::sqrt_f32,  "llvm.sqrt.f32", float32_type, float32_type),
		add_builtin(36, function_body::sqrt_f64,  "llvm.sqrt.f64", float64_type, float64_type),
		add_builtin(37, function_body::pow_f32,   "llvm.pow.f32",  float32_type, float32_type, float32_type),
		add_builtin(38, function_body::pow_f64,   "llvm.pow.f64",  float64_type, float64_type, float64_type),
		add_builtin(39, function_body::cbrt_f32,  "cbrtf",         float32_type, float32_type),
		add_builtin(40, function_body::cbrt_f64,  "cbrt",          float64_type, float64_type),
		add_builtin(41, function_body::hypot_f32, "hypotf",        float32_type, float32_type, float32_type),
		add_builtin(42, function_body::hypot_f64, "hypot",         float64_type, float64_type, float64_type),

		add_builtin(43, function_body::sin_f32,   "llvm.sin.f32", float32_type, float32_type),
		add_builtin(44, function_body::sin_f64,   "llvm.sin.f64", float64_type, float64_type),
		add_builtin(45, function_body::cos_f32,   "llvm.cos.f32", float32_type, float32_type),
		add_builtin(46, function_body::cos_f64,   "llvm.cos.f64", float64_type, float64_type),
		add_builtin(47, function_body::tan_f32,   "tanf",         float32_type, float32_type),
		add_builtin(48, function_body::tan_f64,   "tan",          float64_type, float64_type),
		add_builtin(49, function_body::asin_f32,  "asinf",        float32_type, float32_type),
		add_builtin(50, function_body::asin_f64,  "asin",         float64_type, float64_type),
		add_builtin(51, function_body::acos_f32,  "acosf",        float32_type, float32_type),
		add_builtin(52, function_body::acos_f64,  "acos",         float64_type, float64_type),
		add_builtin(53, function_body::atan_f32,  "atanf",        float32_type, float32_type),
		add_builtin(54, function_body::atan_f64,  "atan",         float64_type, float64_type),
		add_builtin(55, function_body::atan2_f32, "atan2f",       float32_type, float32_type, float32_type),
		add_builtin(56, function_body::atan2_f64, "atan2",        float64_type, float64_type, float64_type),

		add_builtin(57, function_body::sinh_f32,  "sinhf",  float32_type, float32_type),
		add_builtin(58, function_body::sinh_f64,  "sinh",   float64_type, float64_type),
		add_builtin(59, function_body::cosh_f32,  "coshf",  float32_type, float32_type),
		add_builtin(60, function_body::cosh_f64,  "cosh",   float64_type, float64_type),
		add_builtin(61, function_body::tanh_f32,  "tanhf",  float32_type, float32_type),
		add_builtin(62, function_body::tanh_f64,  "tanh",   float64_type, float64_type),
		add_builtin(63, function_body::asinh_f32, "asinhf", float32_type, float32_type),
		add_builtin(64, function_body::asinh_f64, "asinh",  float64_type, float64_type),
		add_builtin(65, function_body::acosh_f32, "acoshf", float32_type, float32_type),
		add_builtin(66, function_body::acosh_f64, "acosh",  float64_type, float64_type),
		add_builtin(67, function_body::atanh_f32, "atanhf", float32_type, float32_type),
		add_builtin(68, function_body::atanh_f64, "atanh",  float64_type, float64_type),

		add_builtin(69, function_body::erf_f32,    "erff",    float32_type, float32_type),
		add_builtin(70, function_body::erf_f64,    "erf",     float64_type, float64_type),
		add_builtin(71, function_body::erfc_f32,   "erfcf",   float32_type, float32_type),
		add_builtin(72, function_body::erfc_f64,   "erfc",    float64_type, float64_type),
		add_builtin(73, function_body::tgamma_f32, "tgammaf", float32_type, float32_type),
		add_builtin(74, function_body::tgamma_f64, "tgamma",  float64_type, float64_type),
		add_builtin(75, function_body::lgamma_f32, "lgammaf", float32_type, float32_type),
		add_builtin(76, function_body::lgamma_f64, "lgamma",  float64_type, float64_type),
	};
#undef add_builtin
	static_assert(result.size() == intrinsic_info.size());
	return result;
}();

function_body *get_builtin_function(uint32_t kind)
{
	bz_assert(kind < builtin_functions.size());
	return &builtin_functions[kind];
}

} // namespace ast
