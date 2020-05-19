#include "parse_context.h"
#include "global_context.h"
#include "built_in_operators.h"
#include "parser.h"

namespace ctx
{

parse_context::parse_context(global_context &_global_ctx)
	: global_ctx(_global_ctx), scope_decls{}
{}

void parse_context::report_error(lex::token_pos it) const
{
	this->global_ctx.report_error(ctx::make_error(it));
}

void parse_context::report_error(
	lex::token_pos it,
	bz::u8string message,
	bz::vector<ctx::note> notes, bz::vector<ctx::suggestion> suggestions
) const
{
	this->global_ctx.report_error(ctx::make_error(
		it, std::move(message),
		std::move(notes), std::move(suggestions)
	));
}

void parse_context::report_error(
	lex::src_tokens src_tokens,
	bz::u8string message,
	bz::vector<ctx::note> notes, bz::vector<ctx::suggestion> suggestions
) const
{
	this->global_ctx.report_error(ctx::make_error(
		src_tokens.begin, src_tokens.pivot, src_tokens.end, std::move(message),
		std::move(notes), std::move(suggestions)
	));
}

void parse_context::report_warning(lex::token_pos it) const
{
	this->global_ctx.report_warning(ctx::make_warning(it));
}

void parse_context::report_warning(
	lex::token_pos it,
	bz::u8string message,
	bz::vector<ctx::note> notes, bz::vector<ctx::suggestion> suggestions
) const
{
	this->global_ctx.report_warning(ctx::make_warning(
		it, std::move(message),
		std::move(notes), std::move(suggestions)
	));
}

void parse_context::report_warning(
	lex::src_tokens src_tokens,
	bz::u8string message,
	bz::vector<ctx::note> notes, bz::vector<ctx::suggestion> suggestions
) const
{
	this->global_ctx.report_warning(ctx::make_warning(
		src_tokens.begin, src_tokens.pivot, src_tokens.end, std::move(message),
		std::move(notes), std::move(suggestions)
	));
}

bool parse_context::has_errors(void) const
{
	return this->global_ctx.has_errors();
}

lex::token_pos parse_context::assert_token(lex::token_pos &stream, uint32_t kind) const
{
	if (stream->kind != kind)
	{
		this->global_ctx.report_error(ctx::make_error(
			stream,
			stream->kind == lex::token::eof
			? bz::format("expected {} before end-of-file", lex::get_token_name_for_message(kind))
			: bz::format("expected {}", lex::get_token_name_for_message(kind))
		));
		return stream;
	}
	else
	{
		auto t = stream;
		++stream;
		return t;
	}
}

lex::token_pos parse_context::assert_token(lex::token_pos &stream, uint32_t kind1, uint32_t kind2) const
{
	if (stream->kind != kind1 && stream->kind != kind2)
	{
		this->global_ctx.report_error(ctx::make_error(
			stream,
			stream->kind == lex::token::eof
			? bz::format(
				"expected {} or {} before end-of-file",
				lex::get_token_name_for_message(kind1), lex::get_token_name_for_message(kind2)
			)
			: bz::format(
				"expected {} or {}",
				lex::get_token_name_for_message(kind1), lex::get_token_name_for_message(kind2)
			)
		));
		return stream;
	}
	else
	{
		auto t = stream;
		++stream;
		return t;
	}
}

void parse_context::report_ambiguous_id_error(lex::token_pos id) const
{
	bz_assert(false);
}


void parse_context::add_scope(void)
{
	this->scope_decls.push_back({});
}

void parse_context::remove_scope(void)
{
	bz_assert(!this->scope_decls.empty());
	this->scope_decls.pop_back();
}


void parse_context::add_local_variable(ast::decl_variable &var_decl)
{
	bz_assert(this->scope_decls.size() != 0);
	this->scope_decls.back().var_decls.push_back(&var_decl);
}

void parse_context::add_local_function(ast::decl_function &func_decl)
{
	bz_assert(this->scope_decls.size() != 0);
	auto &sets = this->scope_decls.back().func_sets;
	auto const set = std::find_if(
		sets.begin(), sets.end(),
		[id = func_decl.identifier->value](auto const &set) {
			return id == set.id;
		}
	);
	if (set == sets.end())
	{
		sets.push_back({ func_decl.identifier->value, { &func_decl } });
	}
	else
	{
		// TODO: check for conflicts
		set->func_decls.push_back(&func_decl);
	}
	this->global_ctx.add_compile_function(func_decl);
}

void parse_context::add_local_operator(ast::decl_operator &op_decl)
{
	bz_assert(this->scope_decls.size() != 0);
	auto &sets = this->scope_decls.back().op_sets;
	auto const set = std::find_if(
		sets.begin(), sets.end(),
		[op = op_decl.op->kind](auto const &set) {
			return op == set.op;
		}
	);
	if (set == sets.end())
	{
		sets.push_back({ op_decl.op->kind, { &op_decl } });
	}
	else
	{
		// TODO: check for conflicts
		set->op_decls.push_back(&op_decl);
	}
	this->global_ctx.add_compile_operator(op_decl);
}

void parse_context::add_local_struct(ast::decl_struct &struct_decl)
{
	bz_assert(this->scope_decls.size() != 0);
	this->scope_decls.back().type_infos.push_back({ struct_decl.identifier->value, &struct_decl.info });
}

/*
auto parse_context::get_identifier_decl(lex::token_pos id) const
	-> bz::variant<ast::decl_variable const *, ast::decl_function const *>
{
	// ==== local decls ====
	// we go in reverse through the scopes and the variables
	// in case there's shadowing
	for (
		auto scope = this->scope_decls.rbegin();
		scope != this->scope_decls.rend();
		++scope
	)
	{
		auto const var = std::find_if(
			scope->var_decls.rbegin(), scope->var_decls.rend(),
			[id = id->value](auto const &var) {
				return var->identifier->value == id;
			}
		);
		if (var != scope->var_decls.rend())
		{
			return *var;
		}

		auto const fn_set = std::find_if(
			scope->func_sets.begin(), scope->func_sets.end(),
			[id = id->value](auto const &fn_set) {
				return fn_set.id == id;
			}
		);
		if (fn_set != scope->func_sets.end())
		{
			if (fn_set->func_decls.size() == 1)
			{
				return fn_set->func_decls[0];
			}
			else
			{
				bz_assert(!fn_set->func_decls.empty());
				return static_cast<ast::decl_function const *>(nullptr);
			}
		}
	}

	// ==== export (global) decls ====
	auto &export_decls = this->global_ctx._export_decls;
	auto const var = std::find_if(
		export_decls.var_decls.begin(), export_decls.var_decls.end(),
		[id = id->value](auto const &var) {
			return id == var->identifier->value;
		}
	);
	if (var != export_decls.var_decls.end())
	{
		return *var;
	}

	auto const fn_set = std::find_if(
		export_decls.func_sets.begin(), export_decls.func_sets.end(),
		[id = id->value](auto const &fn_set) {
			return id == fn_set.id;
		}
	);
	if (fn_set != export_decls.func_sets.end())
	{
		if (fn_set->func_decls.size() == 1)
		{
			return fn_set->func_decls[0];
		}
		else
		{
			bz_assert(!fn_set->func_decls.empty());
			return static_cast<ast::decl_function const *>(nullptr);
		}
	}
	return {};
}
*/
/*
ast::expression::expr_type_t parse_context::get_identifier_type(lex::token_pos id) const
{
	auto decl = this->get_identifier_decl(id);
	switch (decl.index())
	{
	case decl.index_of<ast::decl_variable const *>:
	{
		auto const var = decl.get<ast::decl_variable const *>();
		return {
			var->var_type.is<ast::ts_reference>()
			? ast::expression::lvalue_reference
			: ast::expression::lvalue,
			ast::remove_lvalue_reference(var->var_type)
		};
	}
	case decl.index_of<ast::decl_function const *>:
	{
		auto const fn = decl.get<ast::decl_function const *>();
		auto fn_t = [&]() -> ast::typespec {
			if (fn == nullptr)
			{
				return ast::typespec();
			}
			bz::vector<ast::typespec> arg_types = {};
			for (auto &p : fn->body.params)
			{
				arg_types.emplace_back(p.var_type);
			}
			return ast::make_ts_function({ nullptr, nullptr }, nullptr, fn->body.return_type, arg_types);
		}();
		return { ast::expression::function_name, fn_t };
	}
	case decl.null:
		this->report_error(id, "undeclared identifier");
		return { ast::expression::rvalue, ast::typespec() };
	default:
		bz_assert(false);
		return {};
	}
}
*/

ast::expression parse_context::make_identifier_expression(lex::token_pos id) const
{
	// ==== local decls ====
	// we go in reverse through the scopes and the variables
	// in case there's shadowing
	lex::src_tokens const src_tokens = { id, id, id + 1 };
	for (
		auto scope = this->scope_decls.rbegin();
		scope != this->scope_decls.rend();
		++scope
	)
	{
		auto const var = std::find_if(
			scope->var_decls.rbegin(), scope->var_decls.rend(),
			[id = id->value](auto const &var) {
				return var->identifier->value == id;
			}
		);
		if (var != scope->var_decls.rend())
		{
			auto id_type_kind = ast::expression_type_kind::lvalue;
			auto const *id_type_ptr = &(*var)->var_type;
			if ((*var)->var_type.is<ast::ts_reference>())
			{
				id_type_kind = ast::expression_type_kind::lvalue_reference;
				id_type_ptr = &(*var)->var_type.get<ast::ts_reference_ptr>()->base;
			}
			return ast::make_dynamic_expression(
				src_tokens,
				id_type_kind, *id_type_ptr,
				ast::make_expr_identifier(id, *var)
			);
		}

		auto const fn_set = std::find_if(
			scope->func_sets.begin(), scope->func_sets.end(),
			[id = id->value](auto const &fn_set) {
				return fn_set.id == id;
			}
		);
		if (fn_set != scope->func_sets.end())
		{
			auto const id_type_kind = ast::expression_type_kind::function_name;
			if (fn_set->func_decls.size() == 1)
			{
				auto const decl = fn_set->func_decls[0];
				auto const &return_type = decl->body.return_type;
				bz::vector<ast::typespec> param_types;
				param_types.reserve(decl->body.params.size());
				for (auto &p : decl->body.params)
				{
					param_types.emplace_back(p.var_type);
				}
				return ast::make_constant_expression(
					src_tokens,
					id_type_kind,
					ast::make_ts_function({}, return_type, std::move(param_types)),
					ast::constant_value(&decl->body),
					ast::make_expr_identifier(id, decl)
				);
			}
			else
			{
				auto const decl = static_cast<ast::decl_function const *>(nullptr);
				return ast::make_constant_expression(
					src_tokens,
					id_type_kind, ast::typespec(),
					ast::constant_value(fn_set->id.as_string_view()),
					ast::make_expr_identifier(id, decl)
				);
			}
		}
	}

	// ==== export (global) decls ====
	auto &export_decls = this->global_ctx._export_decls;
	auto const var = std::find_if(
		export_decls.var_decls.begin(), export_decls.var_decls.end(),
		[id = id->value](auto const &var) {
			return id == var->identifier->value;
		}
	);
	if (var != export_decls.var_decls.end())
	{
		auto id_type_kind = ast::expression_type_kind::lvalue;
		auto const *id_type_ptr = &(*var)->var_type;
		if ((*var)->var_type.is<ast::ts_reference>())
		{
			id_type_kind = ast::expression_type_kind::lvalue_reference;
			id_type_ptr = &(*var)->var_type.get<ast::ts_reference_ptr>()->base;
		}
		return ast::make_dynamic_expression(
			src_tokens,
			id_type_kind, *id_type_ptr,
			ast::make_expr_identifier(id, *var)
		);
	}

	auto const fn_set = std::find_if(
		export_decls.func_sets.begin(), export_decls.func_sets.end(),
		[id = id->value](auto const &fn_set) {
			return id == fn_set.id;
		}
	);
	if (fn_set != export_decls.func_sets.end())
	{
		auto const id_type_kind = ast::expression_type_kind::function_name;
		if (fn_set->func_decls.size() == 1)
		{
			auto const decl = fn_set->func_decls[0];
			auto &return_type = decl->body.return_type;
			bz::vector<ast::typespec> param_types;
			param_types.reserve(decl->body.params.size());
			for (auto &p : decl->body.params)
			{
				param_types.emplace_back(p.var_type);
			}
			return ast::make_constant_expression(
				src_tokens,
				id_type_kind,
				ast::make_ts_function({}, return_type, std::move(param_types)),
				ast::constant_value(&decl->body),
				ast::make_expr_identifier(id, decl)
			);
		}
		else
		{
			auto const decl = static_cast<ast::decl_function const *>(nullptr);
			return ast::make_constant_expression(
				src_tokens,
				id_type_kind, ast::typespec(),
				ast::constant_value(fn_set->id.as_string_view()),
				ast::make_expr_identifier(id, decl)
			);
		}
	}

	this->report_error(id, bz::format("undeclared identifier '{}'", id->value));
	return ast::expression(src_tokens);
}

static bz::u8char get_character(bz::u8string_view::const_iterator &it)
{
	auto const get_hex_value = [](bz::u8char c)
	{
		bz_assert((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
		if (c >= '0' && c <= '9')
		{
			return c - '0';
		}
		else if (c >= 'a' && c <= 'f')
		{
			return c - 'a' + 10;
		}
		else
		{
			return c - 'A' + 10;
		}
	};
	switch (*it)
	{
	case '\\':
		++it;
		switch (*it)
		{
		case '\\':
			++it;
			return '\\';
		case '\'':
			++it;
			return '\'';
		case '\"':
			++it;
			return '\"';
		case 'n':
			++it;
			return '\n';
		case 't':
			++it;
			return '\t';
		case 'x':
		{
			++it;
			bz_assert(*it >= '0' && *it <= '7');
			uint8_t val = (*it - '0') << 4;
			++it;
			val |= get_hex_value(*it);
			++it;
			return static_cast<bz::u8char>(val);
		}
		case 'u':
		{
			++it;
			bz::u8char val = 0;
			for (int i = 0; i < 4; ++i, ++it)
			{
				val <<= 4;
				val |= get_hex_value(*it);
			}
			return val;
		}
		case 'U':
		{
			++it;
			bz::u8char val = 0;
			for (int i = 0; i < 8; ++i, ++it)
			{
				val <<= 4;
				val |= get_hex_value(*it);
			}
			return val;
		}
		default:
			bz_assert(false);
			return '\0';
		}

	default:
	{
		auto const res = *it;
		++it;
		return res;
	}
	}
}

ast::expression parse_context::make_literal(lex::token_pos literal) const
{
	lex::src_tokens const src_tokens = { literal, literal, literal + 1 };
	auto const get_int_value_and_type_info = [this, literal]<size_t N>(
		bz::u8string_view postfix,
		uint64_t num,
		bz::meta::index_constant<N>
	) -> std::pair<ast::constant_value, ast::type_info *> {
		constexpr auto default_type_info = static_cast<uint32_t>(N);
		using default_type = ast::type_from_type_info_t<default_type_info>;
		static_assert(
			bz::meta::is_same<default_type, int32_t>
			|| bz::meta::is_same<default_type, uint32_t>
		);
		using wide_default_type = ast::type_from_type_info_t<default_type_info + 1>;
		static_assert(
			bz::meta::is_same<wide_default_type, int64_t>
			|| bz::meta::is_same<wide_default_type, uint64_t>
		);

		std::pair<ast::constant_value, ast::type_info *> return_value{};
		auto &value = return_value.first;
		auto &type_info = return_value.second;
		if (postfix == "")
		{
			if (num <= static_cast<uint64_t>(std::numeric_limits<default_type>::max()))
			{
				value = static_cast<wide_default_type>(num);
				type_info = this->get_base_type_info(default_type_info);
			}
			else if (num <= static_cast<uint64_t>(std::numeric_limits<wide_default_type>::max()))
			{
				value = static_cast<wide_default_type>(num);
				type_info = this->get_base_type_info(default_type_info + 1);
			}
			else
			{
				value = num;
				type_info = this->get_base_type_info(ast::type_info::uint64_);
			}
		}
#define postfix_check(postfix_str, type, wide_type)                                           \
if (postfix == postfix_str)                                                                   \
{                                                                                             \
    if (num > static_cast<uint64_t>(std::numeric_limits<type##_t>::max()))                    \
    {                                                                                         \
        this->report_error(literal, "literal value is too large to fit in type '" #type "'"); \
    }                                                                                         \
    value = static_cast<wide_type>(static_cast<type##_t>(num));                               \
    type_info = this->get_base_type_info(ast::type_info::type##_);                            \
}
		else postfix_check("i8",  int8,   int64_t)
		else postfix_check("i16", int16,  int64_t)
		else postfix_check("i32", int32,  int64_t)
		else postfix_check("i64", int64,  int64_t)
		else postfix_check("u8",  uint8,  uint64_t)
		else postfix_check("u16", uint16, uint64_t)
		else postfix_check("u32", uint32, uint64_t)
		else postfix_check("u64", uint64, uint64_t)
#undef postfix_check
		else
		{
			if (num <= static_cast<uint64_t>(std::numeric_limits<default_type>::max()))
			{
				value = static_cast<wide_default_type>(num);
				type_info = this->get_base_type_info(default_type_info);
			}
			else if (num <= static_cast<uint64_t>(std::numeric_limits<wide_default_type>::max()))
			{
				value = static_cast<wide_default_type>(num);
				type_info = this->get_base_type_info(default_type_info + 1);
			}
			else
			{
				value = num;
				type_info = this->get_base_type_info(ast::type_info::uint64_);
			}
			this->report_error(literal, bz::format("unknown postfix '{}'", postfix));
		}
		return return_value;
	};

	bz_assert(literal->kind != lex::token::string_literal);
	switch (literal->kind)
	{
	case lex::token::integer_literal:
	{
		auto const number_string = literal->value;
		uint64_t num = 0;
		for (auto c : number_string)
		{
			if (c == '\'')
			{
				continue;
			}

			bz_assert(c >= '0' && c <= '9');
			auto const num10 = num * 10;
			if (
				num > std::numeric_limits<uint64_t>::max() / 10
				|| num10 > std::numeric_limits<uint64_t>::max() - (c - '0')
			)
			{
				this->report_error(literal, "literal value is too large, even for 'uint64'");
				break;
			}
			num = num10 + (c - '0');
		}

		auto const postfix = literal->postfix;
		auto const [value, type_info] = get_int_value_and_type_info(
			postfix,
			num,
			bz::meta::index_constant<ast::type_info::int32_>{}
		);

		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			ast::make_ts_base_type({}, type_info),
			value,
			ast::make_expr_literal(literal)
		);
	}
	case lex::token::floating_point_literal:
	{
		auto const number_string = literal->value;
		double num = 0.0;
		bool decimal_state = false;
		double decimal_multiplier = 1.0;
		for (auto const c : number_string)
		{
			if (c == '\'')
			{
				continue;
			}

			if (c == '.')
			{
				decimal_state = true;
				continue;
			}

			if (decimal_state)
			{
				bz_assert(c >= '0' && c <= '9');
				decimal_multiplier /= 10.0;
				num += static_cast<double>(c - '0') * decimal_multiplier;
			}
			else
			{
				bz_assert(c >= '0' && c <= '9');
				num *= 10.0;
				num += static_cast<double>(c - '0');
			}
		}

		auto const postfix = literal->postfix;
		ast::constant_value value{};
		ast::type_info *type_info;
		if (postfix == "" || postfix == "f64")
		{
			value = num;
			type_info = this->get_base_type_info(ast::type_info::float64_);
		}
		else if (postfix == "f32")
		{
			value = static_cast<float32_t>(num);
			type_info = this->get_base_type_info(ast::type_info::float32_);
		}
		else
		{
			value = num;
			type_info = this->get_base_type_info(ast::type_info::float64_);
			this->report_error(literal, bz::format("unknown postfix '{}'", postfix));
		}

		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			ast::make_ts_base_type({}, type_info),
			value,
			ast::make_expr_literal(literal)
		);
	}
	case lex::token::hex_literal:
	{
		// number_string_ contains the leading 0x or 0X
		auto const number_string_ = literal->value;
		bz_assert(number_string_.substring(0, 2) == "0x" || number_string_.substring(0, 2) == "0X");
		auto const number_string = number_string_.substring(2, size_t(-1));
		uint64_t num = 0;
		for (auto c : number_string)
		{
			if (c == '\'')
			{
				continue;
			}

			bz_assert((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
			auto const num16 = num * 16;
			auto const char_value = (c >= '0' && c <= '9') ? c - '0'
				: (c >= 'a' && c <= 'f') ? (c - 'a') + 10
				:/* (c >= 'A' && c <= 'F') ? */(c - 'A') + 10;
			if (
				num > std::numeric_limits<uint64_t>::max() / 16
				|| num16 > std::numeric_limits<uint64_t>::max() - char_value
			)
			{
				this->report_error(literal, "literal value is too large, even for 'uint64'");
				break;
			}
			num = num16 + char_value;
		}

		auto const postfix = literal->postfix;
		auto const [value, type_info] = get_int_value_and_type_info(
			postfix,
			num,
			bz::meta::index_constant<ast::type_info::uint32_>{}
		);

		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			ast::make_ts_base_type({}, type_info),
			value,
			ast::make_expr_literal(literal)
		);
	}
	case lex::token::oct_literal:
	{
		// number_string_ contains the leading 0o or 0O
		auto const number_string_ = literal->value;
		bz_assert(number_string_.substring(0, 2) == "0o" || number_string_.substring(0, 2) == "0O");
		auto const number_string = number_string_.substring(2, size_t(-1));
		uint64_t num = 0;
		for (auto c : number_string)
		{
			if (c == '\'')
			{
				continue;
			}

			bz_assert(c >= '0' && c <= '7');
			auto const num8 = num * 8;
			auto const char_value = c - '0';
			if (
				num > std::numeric_limits<uint64_t>::max() / 8
				|| num8 > std::numeric_limits<uint64_t>::max() - char_value
			)
			{
				this->report_error(literal, "literal value is too large, even for 'uint64'");
				break;
			}
			num = num8 + char_value;
		}

		auto const postfix = literal->postfix;
		auto const [value, type_info] = get_int_value_and_type_info(
			postfix,
			num,
			bz::meta::index_constant<ast::type_info::uint32_>{}
		);

		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			ast::make_ts_base_type({}, type_info),
			value,
			ast::make_expr_literal(literal)
		);
	}
	case lex::token::bin_literal:
	{
		// number_string_ contains the leading 0b or 0B
		auto const number_string_ = literal->value;
		bz_assert(number_string_.substring(0, 2) == "0b" || number_string_.substring(0, 2) == "0B");
		auto const number_string = number_string_.substring(2, size_t(-1));
		uint64_t num = 0;
		for (auto c : number_string)
		{
			if (c == '\'')
			{
				continue;
			}

			bz_assert(c >= '0' && c <= '1');
			auto const num2 = num * 2;
			auto const char_value = c - '0';
			if (
				num > std::numeric_limits<uint64_t>::max() / 2
				|| num2 > std::numeric_limits<uint64_t>::max() - char_value
			)
			{
				this->report_error(literal, "literal value is too large, even for 'uint64'");
				break;
			}
			num = num2 + char_value;
		}

		auto const postfix = literal->postfix;
		auto const [value, type_info] = get_int_value_and_type_info(
			postfix,
			num,
			bz::meta::index_constant<ast::type_info::uint32_>{}
		);

		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			ast::make_ts_base_type({}, type_info),
			value,
			ast::make_expr_literal(literal)
		);
	}
	case lex::token::character_literal:
	{
		auto const char_string = literal->value;
		auto it = char_string.begin();
		auto const end = char_string.end();
		auto const value = get_character(it);
		bz_assert(it == end);

		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			ast::make_ts_base_type({}, this->get_base_type_info(ast::type_info::char_)),
			ast::constant_value(value),
			ast::make_expr_literal(literal)
		);
	}
	case lex::token::kw_true:
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			ast::make_ts_base_type({}, this->get_base_type_info(ast::type_info::bool_)),
			ast::constant_value(true),
			ast::make_expr_literal(literal)
		);
	case lex::token::kw_false:
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			ast::make_ts_base_type({}, this->get_base_type_info(ast::type_info::bool_)),
			ast::constant_value(false),
			ast::make_expr_literal(literal)
		);
	case lex::token::kw_null:
		return ast::make_constant_expression(
			src_tokens,
			ast::expression_type_kind::rvalue,
			ast::make_ts_base_type({}, this->get_base_type_info(ast::type_info::null_t_)),
			ast::constant_value(ast::internal::null_t{}),
			ast::make_expr_literal(literal)
		);
	default:
		bz_assert(false);
		break;
	}
}

ast::expression parse_context::make_string_literal(lex::token_pos const begin, lex::token_pos const end) const
{
	bz_assert(end > begin);
	auto it = begin;
	auto const get_string_value = [](lex::token_pos token) -> bz::u8string {
		bz::u8string result = "";
		auto const value = token->value;
		auto it = value.begin();
		auto const end = value.end();

		while (it != end)
		{
			auto const slash = value.find(it, '\\');
			result += bz::u8string_view(it, slash);
			if (slash == end)
			{
				break;
			}
			it = slash;
			result += get_character(it);
		}

		return result;
	};

	bz::u8string result = "";
	for (; it != end; ++it)
	{
		result += get_string_value(it);
	}

	return ast::make_constant_expression(
		{ begin, end },
		ast::expression_type_kind::rvalue,
		ast::make_ts_base_type({}, this->get_base_type_info(ast::type_info::str_)),
		ast::constant_value(result),
		ast::make_expr_literal(lex::token_range{ begin, end })
	);
}

ast::expression parse_context::make_tuple(lex::src_tokens src_tokens, bz::vector<ast::expression> elems) const
{
	bz_assert(false);
	return ast::expression();
}

static bool is_built_in_type(ast::typespec const &ts)
{
	switch (ts.kind())
	{
	case ast::typespec::index<ast::ts_constant>:
		return is_built_in_type(ts.get<ast::ts_constant_ptr>()->base);
	case ast::typespec::index<ast::ts_base_type>:
	{
		auto &base = *ts.get<ast::ts_base_type_ptr>();
		return (base.info->flags & ast::type_info::built_in) != 0;
	}
	case ast::typespec::index<ast::ts_pointer>:
	case ast::typespec::index<ast::ts_function>:
	case ast::typespec::index<ast::ts_tuple>:
		return true;
	default:
		return false;
	}
}

static int get_type_match_level(
	ast::typespec const &dest,
	ast::expression const &expr,
	parse_context &context
)
{
	int result = 0;

	bz_assert(!expr.is<ast::tuple_expression>());
	auto const [expr_type, expr_type_kind] = expr.get_expr_type_and_kind();

	auto const *dest_it = &ast::remove_const(dest);
	auto const *src_it = &expr_type;

	if (dest_it->is<ast::ts_base_type>())
	{
		src_it = &ast::remove_const(*src_it);
		// if the argument is just a base type, return 1 if there's a conversion, and -1 otherwise
		// TODO: use is_convertible here...
		if (
			src_it->is<ast::ts_base_type>()
			&& src_it->get<ast::ts_base_type_ptr>()->info == dest_it->get<ast::ts_base_type_ptr>()->info
		)
		{
			return 1;
		}
		else
		{
			return -1;
		}
	}
	else if (dest_it->is<ast::ts_void>())
	{
		bz_assert(false);
	}
	else if (dest_it->is<ast::ts_tuple>())
	{
		bz_assert(false);
	}
	else if (dest_it->is<ast::ts_function>())
	{
		bz_assert(false);
	}
	else if (dest_it->is<ast::ts_reference>())
	{
		// if the source is not an lvalue, return -1
		if (
			expr_type_kind != ast::expression_type_kind::lvalue
			&& expr_type_kind != ast::expression_type_kind::lvalue_reference
		)
		{
			return -1;
		}

		dest_it = &ast::remove_lvalue_reference(*dest_it);
		// if the dest is not a &const return 0 if the src and dest types match, -1 otherwise
		if (!dest_it->is<ast::ts_constant>())
		{
			return *dest_it == *src_it ? 0 : -1;
		}
		else
		{
			dest_it = &dest_it->get<ast::ts_constant_ptr>()->base;
			// if the source is not const increment the result
			if (src_it->is<ast::ts_constant>())
			{
				src_it = &src_it->get<ast::ts_constant_ptr>()->base;
			}
			else
			{
				++result;
			}
		}
	}
	else // if (dest_it->is<ast::ts_pointer>())
	{
		result = 1; // not a reference, so result starts at 1
		// advance src_it if it's const
		src_it = &ast::remove_const(*src_it);
		// return -1 if the source is not a pointer
		if (!src_it->is<ast::ts_pointer>())
		{
			return -1;
		}

		// advance src_it and dest_it
		src_it = &src_it->get<ast::ts_pointer_ptr>()->base;
		dest_it = &dest_it->get<ast::ts_pointer_ptr>()->base;

		// if the dest is not a *const return 1 if the src and dest types match, -1 otherwise
		if (!dest_it->is<ast::ts_constant>())
		{
			return *dest_it == *src_it ? 0 : -1;
		}
		else
		{
			dest_it = &dest_it->get<ast::ts_constant_ptr>()->base;
			// if the source is not const increment the result
			if (src_it->is<ast::ts_constant>())
			{
				src_it = &src_it->get<ast::ts_constant_ptr>()->base;
			}
			else
			{
				++result;
			}
		}
	}

	// we can only get here if the dest type is &const or *const
	auto const advance = [](ast::typespec const *&ts)
	{
		switch (ts->kind())
		{
		case ast::typespec::index<ast::ts_constant>:
			ts = &ts->get<ast::ts_constant_ptr>()->base;
			break;
		case ast::typespec::index<ast::ts_pointer>:
			ts = &ts->get<ast::ts_pointer_ptr>()->base;
			break;
		default:
			bz_assert(false);
			break;
		}
	};

	while (true)
	{
		if (dest_it->is<ast::ts_base_type>())
		{
			if (
				src_it->is<ast::ts_base_type>()
				&& src_it->get<ast::ts_base_type_ptr>()->info == dest_it->get<ast::ts_base_type_ptr>()->info
			)
			{
				return result;
			}
			else
			{
				return -1;
			}
		}
		else if (dest_it->is<ast::ts_void>())
		{
			bz_assert(false);
		}
		else if (dest_it->is<ast::ts_function>())
		{
			bz_assert(false);
		}
		else if (dest_it->is<ast::ts_tuple>())
		{
			bz_assert(false);
		}
		else if (dest_it->kind() == src_it->kind())
		{
			advance(dest_it);
			advance(src_it);
		}
		else if (dest_it->is<ast::ts_constant>())
		{
			++result;
			advance(dest_it);
		}
		else
		{
			return -1;
		}
	}
}

struct match_level
{
	int min;
	int sum;
};

static match_level get_function_call_match_level(
	ast::function_body &func_body,
	bz::vector<ast::expression> const &params,
	parse_context &context
)
{
	if (func_body.params.size() != params.size())
	{
		return { -1, -1 };
	}

	resolve_symbol(func_body, context);

	match_level result = { std::numeric_limits<int>::max(), 0 };

	auto const add_to_result = [&](int match)
	{
		if (result.min == -1)
		{
			return;
		}
		else if (match == -1)
		{
			result = { -1, -1 };
		}
		else
		{
			if (match < result.min)
			{
				result.min = match;
			}
			result.sum += match;
		}
	};

	auto params_it = func_body.params.begin();
	auto call_it  = params.begin();
	auto const types_end = func_body.params.end();
	for (; params_it != types_end; ++call_it, ++params_it)
	{
		add_to_result(get_type_match_level(params_it->var_type, *call_it, context));
	}
	return result;
}

static match_level get_function_call_match_level(
	ast::function_body &func_body,
	ast::expression const &expr,
	parse_context &context
)
{
	if (func_body.params.size() != 1)
	{
		return { -1, -1 };
	}

	resolve_symbol(func_body, context);

	auto const match_level = get_type_match_level(func_body.params[0].var_type, expr, context);
	return { match_level, match_level };
}

static match_level get_function_call_match_level(
	ast::function_body &func_body,
	ast::expression const &lhs,
	ast::expression const &rhs,
	parse_context &context
)
{
	if (func_body.params.size() != 2)
	{
		return { -1, -1 };
	}

	resolve_symbol(func_body, context);

	auto const lhs_level = get_type_match_level(func_body.params[0].var_type, lhs, context);
	auto const rhs_level = get_type_match_level(func_body.params[0].var_type, rhs, context);
	if (lhs_level == -1 || rhs_level == -1)
	{
		return { -1, -1 };
	}
	return { std::min(lhs_level, rhs_level), lhs_level + rhs_level };
}

/*
static error get_bad_call_error(
	ast::decl_function *func,
	ast::expr_function_call const &func_call
)
{
	if (func->body.params.size() != func_call.params.size())
	{
		return make_error(
			func_call,
			bz::format(
				"expected {} {} for call to '{}', but was given {}",
				func->body.params.size(), func->body.params.size() == 1 ? "argument" : "arguments",
				func->identifier->value, func_call.params.size()
			),
			{
				make_note(
					func->identifier,
					bz::format("see declaration of '{}':", func->identifier->value)
				)
			}
		);
	}

	auto params_it = func->body.params.begin();
	auto call_it  = func_call.params.begin();
	auto const types_end = func->body.params.end();
//	auto const call_end  = func_call.params.end();
	for (; params_it != types_end; ++call_it, ++params_it)
	{
		bz_assert(!call_it->is<ast::tuple_expression>());
		if (get_type_match_level(params_it->var_type, *call_it) == -1)
		{
			auto const [call_it_type, _] = call_it->get_expr_type_and_kind();
			return make_error(
				*call_it,
				bz::format(
					"unable to convert argument {} from '{}' to '{}'",
					(call_it - func_call.params.begin()) + 1,
					call_it_type, params_it->var_type
				)
			);
		}
	}

	bz_assert(false);
	return make_error(func->identifier, "");
}
*/

static auto find_best_match(
	bz::vector<std::pair<match_level, ast::function_body *>> const &possible_funcs,
	size_t scope_decl_count
) -> std::pair<match_level, ast::function_body *>
{
	std::pair<match_level, ast::function_body *> min_scope_match = { { -1, -1 }, nullptr };

	{
		auto it = possible_funcs.begin();
		auto const end = possible_funcs.begin() + scope_decl_count;
		for (; it != end; ++it)
		{
			if (
				min_scope_match.first.min == -1
				|| it->first.min < min_scope_match.first.min
				|| (it->first.min == min_scope_match.first.min && it->first.sum < min_scope_match.first.sum)
			)
			{
				min_scope_match = *it;
			}
		}
	}

	std::pair<match_level, ast::function_body *> min_global_match = { { -1, -1 }, nullptr };
	{
		auto it = possible_funcs.begin() + scope_decl_count;
		auto const end = possible_funcs.end();
		for (; it != end; ++it)
		{
			if (
				min_global_match.first.min == -1
				|| it->first.min < min_global_match.first.min
				|| (it->first.min == min_global_match.first.min && it->first.sum < min_global_match.first.sum)
			)
			{
				min_global_match = *it;
			}
		}
	}

	if (min_scope_match.first.min == -1 && min_global_match.first.min == -1)
	{
		return { { -1, -1 }, nullptr };
	}
	// there's no global match
	else if (min_global_match.first.min == -1)
	{
		return min_scope_match;
	}
	// there's a better scope match
	else if (
		min_scope_match.first.min != -1
		&& (
			min_scope_match.first.min < min_global_match.first.min
			|| (min_scope_match.first.min == min_global_match.first.min && min_scope_match.first.sum <= min_global_match.first.sum)
		)
	)
	{
		return min_scope_match;
	}
	// the global match is the best
	else
	{
		// we need to check for ambiguity here
		bz::vector<std::pair<match_level, ast::function_body *>> possible_min_matches = {};
		for (
			auto it = possible_funcs.begin() + scope_decl_count;
			it != possible_funcs.end();
			++it
		)
		{
			if (it->first.min == min_global_match.first.min && it->first.sum == min_global_match.first.sum)
			{
				possible_min_matches.push_back(*it);
			}
		}

		bz_assert(possible_min_matches.size() != 0);
		if (possible_min_matches.size() == 1)
		{
			return min_global_match;
		}
		else
		{
			// TODO: report ambiguous call error somehow
			bz_assert(false);
			return { { -1, -1 }, nullptr };
		}
	}
}

ast::expression parse_context::make_unary_operator_expression(
	lex::src_tokens src_tokens,
	lex::token_pos op,
	ast::expression expr
)
{
	bz_assert(!expr.is<ast::tuple_expression>());
	if (expr.is_null())
	{
		bz_assert(this->has_errors());
		return ast::expression(src_tokens);
	}

	if (!lex::is_overloadable_unary_operator(op->kind))
	{
		auto result = make_non_overloadable_operation(op, std::move(expr), *this);
		result.src_tokens = src_tokens;
		return result;
	}

	auto [type, type_kind] = expr.get_expr_type_and_kind();

	if (is_built_in_type(ast::remove_const(type)))
	{
		auto result = make_built_in_operation(op, std::move(expr), *this);
		result.src_tokens = src_tokens;
		return result;
	}

	auto const report_undeclared_error = [&, this, type = &type]()
	{
		this->report_error(
			src_tokens,
			bz::format(
				"undeclared unary operator {} with type '{}'",
				op->value, *type
			)
		);
	};

	bz::vector<std::pair<match_level, ast::function_body *>> possible_funcs = {};

	// we go through the scope decls for a matching declaration
	for (
		auto scope = this->scope_decls.rbegin();
		scope != this->scope_decls.rend();
		++scope
	)
	{
		auto const set = std::find_if(
			scope->op_sets.begin(), scope->op_sets.end(),
			[op = op->kind](auto const &op_set) {
				return op == op_set.op;
			}
		);
		if (set != scope->op_sets.end())
		{
			for (auto &op : set->op_decls)
			{
				auto const match_level = get_function_call_match_level(op->body, expr, *this);
				if (match_level.min != -1)
				{
					possible_funcs.push_back({ match_level, &op->body });
				}
			}
		}
	}

	auto const scope_decl_count = possible_funcs.size();

	auto &export_decls = this->global_ctx._export_decls;
	auto const global_set = std::find_if(
		export_decls.op_sets.begin(), export_decls.op_sets.end(),
		[op = op->kind](auto const &op_set) {
			return op == op_set.op;
		}
	);
	if (global_set != export_decls.op_sets.end())
	{
		for (auto &op : global_set->op_decls)
		{
			auto const match_level = get_function_call_match_level(op->body, expr, *this);
			if (match_level.min != -1)
			{
				possible_funcs.push_back({ match_level, &op->body });
			}
		}
	}

	auto const best = find_best_match(possible_funcs, scope_decl_count);
	if (best.first.min == -1)
	{
		report_undeclared_error();
		return ast::expression(src_tokens);
	}
	else
	{
		auto &ret_t = best.second->return_type;
		auto return_type_kind = ast::expression_type_kind::rvalue;
		auto return_type = &ast::remove_const(ret_t);
		if (ret_t.is<ast::ts_reference>())
		{
			return_type_kind = ast::expression_type_kind::lvalue_reference;
			return_type = &ret_t.get<ast::ts_reference_ptr>()->base;
		}
		bz::vector<ast::expression> params = {};
		params.emplace_back(std::move(expr));
		return ast::make_dynamic_expression(
			src_tokens,
			return_type_kind, *return_type,
			ast::make_expr_function_call(src_tokens, std::move(params), best.second)
		);
	}
}

ast::expression parse_context::make_binary_operator_expression(
	lex::src_tokens src_tokens,
	lex::token_pos op,
	ast::expression lhs,
	ast::expression rhs
)
{
	bz_assert(!lhs.is<ast::tuple_expression>());
	bz_assert(!rhs.is<ast::tuple_expression>());
	if (lhs.is_null() || rhs.is_null())
	{
		bz_assert(this->has_errors());
		return ast::expression(src_tokens);
	}

	if (!lex::is_overloadable_binary_operator(op->kind))
	{
		auto result = make_non_overloadable_operation(op, std::move(lhs), std::move(rhs), *this);
		result.src_tokens = src_tokens;
		return result;
	}

	auto const [lhs_type, lhs_type_kind] = lhs.get_expr_type_and_kind();
	auto const [rhs_type, rhs_type_kind] = rhs.get_expr_type_and_kind();

	if (
		is_built_in_type(ast::remove_const(lhs_type))
		&& is_built_in_type(ast::remove_const(rhs_type))
	)
	{
		auto result = make_built_in_operation(op, std::move(lhs), std::move(rhs), *this);
		result.src_tokens = src_tokens;
		return result;
	}

	auto const report_undeclared_error = [&, this, lhs_type = &lhs_type, rhs_type = &rhs_type]()
	{
		if (op->kind == lex::token::square_open)
		{
			this->report_error(
				src_tokens,
				bz::format(
					"undeclared binary operator [] with types '{}' and '{}'",
					*lhs_type, *rhs_type
				)
			);
		}
		else
		{
			this->report_error(
				src_tokens,
				bz::format(
					"undeclared binary operator {} with types '{}' and '{}'",
					op->value, *lhs_type, *rhs_type
				)
			);
		}
	};

	bz::vector<std::pair<match_level, ast::function_body *>> possible_funcs = {};

	// we go through the scope decls for a matching declaration
	for (
		auto scope = this->scope_decls.rbegin();
		scope != this->scope_decls.rend();
		++scope
	)
	{
		auto const set = std::find_if(
			scope->op_sets.begin(), scope->op_sets.end(),
			[op = op->kind](auto const &op_set) {
				return op == op_set.op;
			}
		);
		if (set != scope->op_sets.end())
		{
			for (auto &op : set->op_decls)
			{
				auto const match_level = get_function_call_match_level(op->body, lhs, rhs, *this);
				if (match_level.min != -1)
				{
					possible_funcs.push_back({ match_level, &op->body });
				}
			}
		}
	}

	auto const scope_decl_count = possible_funcs.size();

	auto &export_decls = this->global_ctx._export_decls;
	auto const global_set = std::find_if(
		export_decls.op_sets.begin(), export_decls.op_sets.end(),
		[op = op->kind](auto const &op_set) {
			return op == op_set.op;
		}
	);
	if (global_set != export_decls.op_sets.end())
	{
		for (auto &op : global_set->op_decls)
		{
			auto const match_level = get_function_call_match_level(op->body, lhs, rhs, *this);
			if (match_level.min != -1)
			{
				possible_funcs.push_back({ match_level, &op->body });
			}
		}
	}

	auto const best = find_best_match(possible_funcs, scope_decl_count);
	if (best.first.min == -1)
	{
		report_undeclared_error();
		return ast::expression(src_tokens);
	}
	else
	{
		auto &ret_t = best.second->return_type;
		auto return_type_kind = ast::expression_type_kind::rvalue;
		auto return_type = &ast::remove_const(ret_t);
		if (ret_t.is<ast::ts_reference>())
		{
			return_type_kind = ast::expression_type_kind::lvalue_reference;
			return_type = &ret_t.get<ast::ts_reference_ptr>()->base;
		}
		bz::vector<ast::expression> params = {};
		params.reserve(2);
		params.emplace_back(std::move(lhs));
		params.emplace_back(std::move(rhs));
		return ast::make_dynamic_expression(
			src_tokens,
			return_type_kind, *return_type,
			ast::make_expr_function_call(src_tokens, std::move(params), best.second)
		);
	}
}

ast::expression parse_context::make_function_call_expression(
	lex::src_tokens src_tokens,
	ast::expression called,
	bz::vector<ast::expression> params
)
{
	if (called.is<ast::tuple_expression>())
	{
		this->report_error(
			src_tokens,
			"undeclared function call operator with types '...' (not yet implemented)"
		);
		return ast::expression(src_tokens);
	}

	auto const [called_type, called_type_kind] = called.get_expr_type_and_kind();

	if (called_type_kind == ast::expression_type_kind::function_name)
	{
		bz_assert(called.is<ast::constant_expression>());
		auto &const_called = called.get<ast::constant_expression>();
		if (const_called.value.kind() == ast::constant_value::function)
		{
			auto const func_body = const_called.value.get<ast::constant_value::function>();
			if (get_function_call_match_level(*func_body, params, *this).sum == -1)
			{
				this->report_error(
					src_tokens,
					"couldn't match the function call to any of the overloads"
				);
			}
			auto &ret_t = func_body->return_type;
			auto return_type_kind = ast::expression_type_kind::rvalue;
			auto return_type = &ast::remove_const(ret_t);
			if (ret_t.is<ast::ts_reference>())
			{
				return_type_kind = ast::expression_type_kind::lvalue_reference;
				return_type = &ret_t.get<ast::ts_reference_ptr>()->base;
			}
			return ast::make_dynamic_expression(
				src_tokens,
				return_type_kind, *return_type,
				ast::make_expr_function_call(src_tokens, std::move(params), func_body)
			);
		}
		else
		{
			bz_assert(const_called.value.kind() == ast::constant_value::function_set_id);
			auto const id = const_called.value.get<ast::constant_value::function_set_id>();
			bz::vector<std::pair<match_level, ast::function_body *>> possible_funcs = {};
			// we go through the scope decls for a matching declaration
			for (
				auto scope = this->scope_decls.rbegin();
				scope != this->scope_decls.rend();
				++scope
			)
			{
				auto const set = std::find_if(
					scope->func_sets.begin(), scope->func_sets.end(),
					[id](auto const &fn_set) {
						return id == fn_set.id;
					}
				);
				if (set != scope->func_sets.end())
				{
					for (auto &fn : set->func_decls)
					{
						auto const match_level = get_function_call_match_level(fn->body, params, *this);
						if (match_level.min != -1)
						{
							possible_funcs.push_back({ match_level, &fn->body });
						}
					}
				}
			}

			auto const scope_decl_count = possible_funcs.size();

			auto &export_decls = this->global_ctx._export_decls;
			auto const global_set = std::find_if(
				export_decls.func_sets.begin(), export_decls.func_sets.end(),
				[id](auto const &fn_set) {
					return id == fn_set.id;
				}
			);
			if (global_set != export_decls.func_sets.end())
			{
				for (auto &fn : global_set->func_decls)
				{
					auto const match_level = get_function_call_match_level(fn->body, params, *this);
					if (match_level.min != -1)
					{
						possible_funcs.push_back({ match_level, &fn->body });
					}
				}
			}

			auto const best = find_best_match(possible_funcs, scope_decl_count);
			if (best.first.min == -1)
			{
				this->report_error(
					src_tokens,
					"couldn't match the function call to any of the overloads"
				);
				return ast::expression(src_tokens);
			}
			else
			{
				auto &ret_t = best.second->return_type;
				auto return_type_kind = ast::expression_type_kind::rvalue;
				auto return_type = &ast::remove_const(ret_t);
				if (ret_t.is<ast::ts_reference>())
				{
					return_type_kind = ast::expression_type_kind::lvalue_reference;
					return_type = &ret_t.get<ast::ts_reference_ptr>()->base;
				}
				return ast::make_dynamic_expression(
					src_tokens,
					return_type_kind, *return_type,
					ast::make_expr_function_call(src_tokens, std::move(params), best.second)
				);
			}
		}
	}
	else
	{
		// function call operator
		bz_assert(false);
	}
}

ast::expression parse_context::make_cast_expression(
	lex::src_tokens src_tokens,
	lex::token_pos op,
	ast::expression expr,
	ast::typespec type
)
{
	if (expr.is<ast::tuple_expression>())
	{
		this->report_error(src_tokens, "invalid cast");
		return ast::expression(src_tokens);
	}

	auto const [expr_type, expr_type_kind] = expr.get_expr_type_and_kind();

	if (is_built_in_type(expr_type))
	{
		auto result = make_built_in_cast(op, std::move(expr), std::move(type), *this);
		result.src_tokens = src_tokens;
		return result;
	}
	else
	{
		this->report_error(src_tokens, "invalid cast");
		return ast::expression(src_tokens);
	}
}

void parse_context::match_expression_to_type(
	ast::expression &expr,
	ast::typespec &dest_type
)
{
	bz_assert(!expr.is<ast::tuple_expression>());
	if (expr.is_null())
	{
		bz_assert(this->has_errors());
		return;
	}
	auto [expr_type, expr_type_kind] = expr.get_expr_type_and_kind();
	if (!ast::is_complete(expr_type))
	{
		return;
	}
	auto *expr_it = &expr_type;
	auto *dest_it = &ast::remove_const(dest_type);

	auto const strict_match = [&, &expr_type = expr_type]() {
		bool loop = true;
		while (loop)
		{
			switch (dest_it->kind())
			{
			case ast::typespec::index<ast::ts_base_type>:
				if (*expr_it != *dest_it)
				{
					this->report_error(
						expr,
						bz::format("cannot convert expression from type '{}' to '{}'", expr_type, dest_type)
					);
				}
				loop = false;
				break;
			case ast::typespec::index<ast::ts_constant>:
				dest_it = &dest_it->get<ast::ts_constant_ptr>()->base;
				if (!expr_it->is<ast::ts_constant>())
				{
					this->report_error(
						expr,
						bz::format("cannot convert expression from type '{}' to '{}'", expr_type, dest_type)
					);
					loop = false;
				}
				else
				{
					expr_it = &expr_it->get<ast::ts_constant_ptr>()->base;
				}
				break;
			case ast::typespec::index<ast::ts_pointer>:
				dest_it = &dest_it->get<ast::ts_pointer_ptr>()->base;
				if (!expr_it->is<ast::ts_pointer_ptr>())
				{
					this->report_error(
						expr,
						bz::format("cannot convert expression from type '{}' to '{}'", expr_type, dest_type)
					);
					loop = false;
				}
				else
				{
					expr_it = &expr_it->get<ast::ts_pointer_ptr>()->base;
				}
				break;
			default:
				*dest_it = *expr_it;
				loop = false;
				break;
			}
		}
	};

	if (dest_it->is<ast::ts_reference>())
	{
		if (
			expr_type_kind != ast::expression_type_kind::lvalue
			&& expr_type_kind != ast::expression_type_kind::lvalue_reference
		)
		{
			this->report_error(expr, "cannot bind an rvalue to an lvalue reference");
			return;
		}

		if (dest_it->is<ast::ts_constant>())
		{
			dest_it = &dest_it->get<ast::ts_constant_ptr>()->base;
			expr_it = &ast::remove_const(*expr_it);
		}
		else if (expr_it->is<ast::ts_constant>())
		{
			this->report_error(
				expr,
				bz::format(
					"cannot bind an expression of type '{}' to a reference of type '{}'",
					*expr_it, *dest_it
				)
			);
			return;
		}

		strict_match();
	}
	else if (dest_it->is<ast::ts_pointer>())
	{
		expr_it = &ast::remove_const(*expr_it);
		dest_it = &dest_it->get<ast::ts_pointer_ptr>()->base;
		if (!expr_it->is<ast::ts_pointer>())
		{
			// check if expr is a null literal
			if (
				expr_it->is<ast::ts_base_type>()
				&& expr_it->get<ast::ts_base_type_ptr>()->info->kind == ast::type_info::null_t_
			)
			{
				bz_assert(expr.is<ast::constant_expression>());
				if (!ast::is_complete(dest_type))
				{
					this->report_error(expr, "variable type cannot be deduced");
					return;
				}
				else
				{
					expr.get<ast::constant_expression>().type = ast::remove_const(dest_type);
					return;
				}
			}
			else
			{
				this->report_error(
					expr,
					bz::format("cannot convert expression from type '{}' to '{}'", expr_type, dest_type)
				);
				return;
			}
		}
		else
		{
			expr_it = &expr_it->get<ast::ts_pointer_ptr>()->base;
			if (dest_it->is<ast::ts_constant>())
			{
				dest_it = &dest_it->get<ast::ts_constant_ptr>()->base;
				expr_it = &ast::remove_const(*expr_it);
			}
			else if (expr_it->is<ast::ts_constant>())
			{
				this->report_error(
					expr,
					bz::format("cannot convert expression from type '{}' to '{}'", expr_type, dest_type)
				);
				return;
			}
		}

		strict_match();
	}
	else if (dest_it->is<ast::ts_base_type>())
	{
		expr_it = &ast::remove_const(*expr_it);
		if (!expr_it->is<ast::ts_base_type>())
		{
			this->report_error(
				expr,
				bz::format("cannot convert expression from type '{}' to '{}'", expr_type, dest_type)
			);
			return;
		}

		auto const dest_info = dest_it->get<ast::ts_base_type_ptr>()->info;
		auto const expr_info = expr_it->get<ast::ts_base_type_ptr>()->info;

		if (dest_info != expr_info)
		{
			this->report_error(
				expr,
				bz::format("cannot convert expression from type '{}' to '{}'", expr_type, dest_type)
			);
			return;
		}
	}
	else
	{
		bz_assert(dest_it->is_null());
		expr_it = &ast::remove_const(*expr_it);
		*dest_it = *expr_it;
	}
}

/*
auto parse_context::get_cast_body_and_type(ast::expr_cast const &cast)
	-> std::pair<ast::function_body *, ast::expression::expr_type_t>
{
	auto res = get_built_in_cast_type(cast.expr.expr_type, cast.type, *this);
	if (res.expr_type.kind() == ast::typespec::null)
	{
		this->report_error(cast, bz::format("invalid cast from '{}' to '{}'", cast.expr.expr_type.expr_type, cast.type));
	}
	return { nullptr, std::move(res) };
}
*/
/*
bool parse_context::is_convertible(ast::expression::expr_type_t const &from, ast::typespec const &to)
{
	return are_directly_matchable_types(from, to);
}
*/

ast::type_info *parse_context::get_base_type_info(uint32_t kind) const
{ return this->global_ctx.get_base_type_info(kind); }

ast::type_info *parse_context::get_type_info(bz::u8string_view id) const
{
	for (
		auto scope = this->scope_decls.rbegin();
		scope != this->scope_decls.rend();
		++scope
	)
	{
		auto const it = std::find_if(
			scope->type_infos.rbegin(), scope->type_infos.rend(),
			[id](auto const &info) {
				return id == info.id;
			}
		);
		if (it != scope->type_infos.rend())
		{
			return it->info;
		}
	}

	auto &export_decls = this->global_ctx._export_decls;
	auto const global_it = std::find_if(
		export_decls.type_infos.begin(), export_decls.type_infos.end(),
		[id](auto const &info) {
			return id == info.id;
		}
	);

	if (global_it != export_decls.type_infos.end())
	{
		return global_it->info;
	}
	else
	{
		return nullptr;
	}
}

} // namespace ctx
