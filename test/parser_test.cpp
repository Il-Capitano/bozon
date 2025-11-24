#include "parse/parse_common.cpp"
#include "parse/expression_parser.cpp"
#include "parse/statement_parser.cpp"
#include "resolve/attribute_resolver.cpp"
#include "resolve/expression_resolver.cpp"
#include "resolve/statement_resolver.cpp"
#include "resolve/match_common.cpp"
#include "resolve/match_to_type.cpp"
#include "resolve/match_expression.cpp"

#include "test.h"
#include "lex/lexer.h"
#include "ctx/global_context.h"
#include "ctx/lex_context.h"
#include "ctx/parse_context.h"

using namespace parse;
using namespace resolve;

#define xxx(fn, str, it_pos, error_assert, custom_assert)  \
do {                                                       \
    bz::u8string_view const file = str;                    \
    auto const tokens = lex::get_tokens(file, 0, lex_ctx); \
    assert_false(global_ctx.has_errors_or_warnings());     \
    auto it = tokens.begin();                              \
    auto res = fn(it, tokens.end() - 1, parse_ctx);        \
    assert_eq(it, it_pos);                                 \
    assert_true(custom_assert);                            \
    assert_true(error_assert);                             \
    global_ctx.clear_errors_and_warnings();                \
} while (false)

#define xx(fn, str, it_pos, custom_assert)                                \
xxx(fn, str, it_pos, !global_ctx.has_errors_or_warnings(), custom_assert)

#define xx_compiles(fn, str, it_pos, custom_assert)           \
xxx(fn, str, it_pos, !global_ctx.has_errors(), custom_assert)

#define xx_warn(fn, str, it_pos, custom_assert)                                            \
xxx(fn, str, it_pos, !global_ctx.has_errors() && global_ctx.has_warnings(), custom_assert)

#define xx_err(fn, str, it_pos, custom_assert)               \
xxx(fn, str, it_pos, global_ctx.has_errors(), custom_assert)


#define declare_var(id_str, type_str, init_expr_str)                                 \
do {                                                                                 \
    static std::list<bz::vector<lex::token>> var_tokens;                             \
    static bz::vector<ast::statement> var_decls;                                     \
    var_tokens.emplace_back(lex::get_tokens(id_str, 0, lex_ctx));                    \
    auto const &name_tokens = var_tokens.back();                                     \
    assert_eq(name_tokens.size(), 2);                                                \
    assert_eq(name_tokens[0].kind, lex::token::identifier);                          \
    auto const id = name_tokens.begin();                                             \
    var_tokens.emplace_back(lex::get_tokens(type_str, 0, lex_ctx));                  \
    auto const &type_tokens = var_tokens.back();                                     \
    assert_false(global_ctx.has_errors());                                           \
    auto const init_expr_tokens = lex::get_tokens(init_expr_str, 0, lex_ctx);        \
    var_tokens.push_back(init_expr_tokens);                                          \
    auto init_expr = sizeof init_expr_str == 1                                       \
        ? ast::expression()                                                          \
        : ast::make_unresolved_expression({                                          \
            init_expr_tokens.begin(),                                                \
            init_expr_tokens.begin(),                                                \
            init_expr_tokens.end() - 1,                                              \
        });                                                                          \
    lex::src_tokens type_src_tokens = {                                              \
        type_tokens.begin(),                                                         \
        type_tokens.begin(),                                                         \
        type_tokens.end() - 1,                                                       \
    };                                                                               \
    lex::token_range type_token_range = {                                            \
        type_src_tokens.begin,                                                       \
        type_src_tokens.end,                                                         \
    };                                                                               \
    auto decl = ast::make_decl_variable(                                             \
        lex::src_tokens{id, id, id + 1},                                             \
        lex::token_range{},                                                          \
        ast::var_id_and_type(                                                        \
            ast::make_identifier(id),                                                \
            parse_ctx.type_as_expression(                                            \
                lex::src_tokens::from_range(type_token_range),                       \
                ast::make_unresolved_typespec(type_token_range)                      \
            )                                                                        \
        ),                                                                           \
        std::move(init_expr),                                                        \
        parse_ctx.get_current_enclosing_scope()                                      \
    );                                                                               \
    auto &var_decl = decl.get<ast::decl_variable>();                                 \
    resolve_variable_impl(var_decl, parse_ctx);                                      \
    var_decls.emplace_back(std::move(decl));                                         \
    assert_false(global_ctx.has_errors());                                           \
    parse_ctx.add_local_variable(var_decl);                                          \
    assert_false(global_ctx.has_errors());                                           \
} while (false)

static bz::optional<bz::u8string> get_paren_matched_range_test(ctx::global_context &global_ctx)
{
	ctx::lex_context lex_ctx(global_ctx);
	ctx::parse_context parse_ctx(global_ctx);
	parse_ctx.current_global_scope = global_ctx._builtin_global_scope;

#define x(str, it_pos)                                     \
do {                                                       \
    bz::u8string_view const file = str;                    \
    auto const tokens = lex::get_tokens(file, 0, lex_ctx); \
    assert_false(global_ctx.has_errors());                 \
    auto it = tokens.begin() + 1;                          \
    get_paren_matched_range(it, tokens.end(), parse_ctx);  \
    assert_eq(it, it_pos);                                 \
} while (false)

	// the function expects that the leading parenthesis has been consumed

	// the opening parenthesis will be skipped
	x("() a", tokens.begin() + 2);
	//    ^ tokens.begin() + 2
	x("[] a", tokens.begin() + 2);
	//    ^ tokens.begin() + 2
	x("(()) a", tokens.begin() + 4);
	//      ^ tokens.begin() + 4
	x("[(())[][]{{}}] a", tokens.begin() + 14);
	//                ^ tokens.begin() + 14

	return {};

#undef x
}

static constexpr bool operator == (ast::internal::null_t, ast::internal::null_t) noexcept
{ return true; }

static bz::optional<bz::u8string> parse_primary_expression_test(ctx::global_context &global_ctx)
{
	ctx::lex_context lex_ctx(global_ctx);
	ctx::parse_context parse_ctx(global_ctx);
	parse_ctx.current_global_scope = global_ctx._builtin_global_scope;

#define x(str) xx(parse_primary_expression, str, tokens.end() - 1, (consteval_guaranteed(res, parse_ctx), true))
#define x_warn(str) xx_warn(parse_primary_expression, str, tokens.end() - 1, (consteval_guaranteed(res, parse_ctx), true))
#define x_err(str) xx_err(parse_primary_expression, str, tokens.end() - 1, (consteval_guaranteed(res, parse_ctx), true))

#define x_const_expr(str, _type, const_expr_kind, const_expr_value)                                 \
xx_compiles(                                                                                        \
    parse_primary_expression,                                                                       \
    str,                                                                                            \
    tokens.end() - 1,                                                                               \
    (consteval_guaranteed(res, parse_ctx), (                                                        \
        res.is<ast::constant_expression>()                                                          \
        && res.get<ast::constant_expression>().type.is<ast::ts_base_type>()                         \
        && res.get<ast::constant_expression>().type.get<ast::ts_base_type>().info->kind == _type    \
        && res.get<ast::constant_expression>().value.kind() == const_expr_kind                      \
        && (res.get<ast::constant_expression>().value.get<const_expr_kind>()) == (const_expr_value) \
    ))                                                                                              \
)

	// add scope to allow variables
	auto local_scope = ast::make_local_scope(parse_ctx.get_current_enclosing_scope(), false);
	parse_ctx.push_local_scope(&local_scope);


	x_err("");

	declare_var("a", "mut i32", "");
	x("a");
	x_err("this_doesnt_exist");

	x_const_expr("42", ast::type_info::i32_, ast::constant_value_kind::sint, 42);
	auto const min_int64_val = static_cast<int64_t>(std::numeric_limits<int32_t>::max()) + 1;
	auto const min_int64_str = bz::format("{}", min_int64_val);
	x_const_expr(min_int64_str, ast::type_info::i64_, ast::constant_value_kind::sint, min_int64_val);
	auto const min_uint64_val = static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + 1;
	auto const min_uint64_str = bz::format("{}", min_uint64_val);
	x_const_expr(min_uint64_str, ast::type_info::u64_, ast::constant_value_kind::uint, min_uint64_val);
	x_err("999999999999999999999999999");

	x_err("32i123456");

	x_const_expr("42i8",  ast::type_info::i8_,  ast::constant_value_kind::sint, 42);
	x_const_expr("42i16", ast::type_info::i16_, ast::constant_value_kind::sint, 42);
	x_const_expr("42i32", ast::type_info::i32_, ast::constant_value_kind::sint, 42);
	x_const_expr("42i64", ast::type_info::i64_, ast::constant_value_kind::sint, 42);
	x_const_expr("42u8",  ast::type_info::u8_,  ast::constant_value_kind::uint, 42);
	x_const_expr("42u16", ast::type_info::u16_, ast::constant_value_kind::uint, 42);
	x_const_expr("42u32", ast::type_info::u32_, ast::constant_value_kind::uint, 42);
	x_const_expr("42u64", ast::type_info::u64_, ast::constant_value_kind::uint, 42);
	x_err("128i8");

	x_const_expr("1.5", ast::type_info::float64_, ast::constant_value_kind::float64, 1.5);
	x_err("1.5f123456");

	x_const_expr("1.5f32", ast::type_info::float32_, ast::constant_value_kind::float32, 1.5f);
	x_const_expr("1.5f64", ast::type_info::float64_, ast::constant_value_kind::float64, 1.5);

	x_const_expr("0x42", ast::type_info::u32_, ast::constant_value_kind::uint, 0x42);
	x_const_expr("0x1234'5678'90ab'cdef", ast::type_info::u64_, ast::constant_value_kind::uint, 0x1234'5678'90ab'cdef);
	x_err("0x1'1234'5678'90ab'cdef");
	x_const_expr("0x42i8", ast::type_info::i8_, ast::constant_value_kind::sint, 0x42);
	x_err("0xffi8");
	x_err("0x1'ffff'ffff'ffff'ffff");

	x_const_expr("0o42", ast::type_info::u32_, ast::constant_value_kind::uint, (4 * 8 + 2));
	auto const min_uint64_oct_str = bz::format("0o{:o}", uint64_t(1) << 32);
	x_const_expr(min_uint64_oct_str, ast::type_info::u64_, ast::constant_value_kind::uint, static_cast<uint64_t>(1ull << 32));
	x_const_expr("0o42i8", ast::type_info::i8_, ast::constant_value_kind::sint, (4 * 8 + 2));
	x_err("0o200i8");

	x_const_expr("0b1010'0101", ast::type_info::u32_, ast::constant_value_kind::uint, 0b1010'0101);
	x_const_expr("0b'1'0000'0000'0000'0000'0000'0000'0000'0000", ast::type_info::u64_, ast::constant_value_kind::uint, 1ull << 32);
	x_const_expr("0b0110'0101'i8", ast::type_info::i8_, ast::constant_value_kind::sint, 0b0110'0101);
	x_err("0b1000'0000'i8");
	x_err("0b'1''0000'0000''0000'0000''0000'0000''0000'0000''''0000'0000''0000'0000''0000'0000''0000'0000");

	x_const_expr("'a'", ast::type_info::char_, ast::constant_value_kind::u8char, 'a');
	x_err("'a'asdf");
	x_const_expr("'\\x7f'", ast::type_info::char_, ast::constant_value_kind::u8char, 0x7f);
	x_const_expr("'\\u0470'", ast::type_info::char_, ast::constant_value_kind::u8char, 0x470);
	x_const_expr("'\\U00000470'", ast::type_info::char_, ast::constant_value_kind::u8char, 0x470);
	x_const_expr("'Ѱ'", ast::type_info::char_, ast::constant_value_kind::u8char, 0x470);
//	x_err("'\\U000110000'", 1); // this is handled while lexing

	x_const_expr("true", ast::type_info::bool_, ast::constant_value_kind::boolean, true);
	x_const_expr("false", ast::type_info::bool_, ast::constant_value_kind::boolean, false);
	x_const_expr("null", ast::type_info::null_t_, ast::constant_value_kind::null, ast::internal::null_t{});

	x_const_expr(R"( "" )", ast::type_info::str_, ast::constant_value_kind::string, "");
	x_const_expr(R"( "hello!!" )", ast::type_info::str_, ast::constant_value_kind::string, "hello!!");
	x_const_expr(R"( "hello	!!" )", ast::type_info::str_, ast::constant_value_kind::string, "hello\t!!");
	x_const_expr(R"( "hello\t!!" )", ast::type_info::str_, ast::constant_value_kind::string, "hello\t!!");
	x_const_expr(R"( "hello!!\u0470" )", ast::type_info::str_, ast::constant_value_kind::string, "hello!!Ѱ");
	x_const_expr(R"( "hello" " again" " and again!" )", ast::type_info::str_, ast::constant_value_kind::string, "hello again and again!");


	x("(0)");
	x_err("(0 0)");
	x_err("()");

	x_const_expr("+42", ast::type_info::i32_, ast::constant_value_kind::sint, 42);
	x_err("+ 'a'");

	x_const_expr("-42", ast::type_info::i32_, ast::constant_value_kind::sint, -42);
	x_err("-42u32");
	x_warn("-(-128 as i8)");
	auto const max_int64 = std::numeric_limits<int64_t>::max();
	auto const test_str = bz::format("-(-{}i64 - 1)", max_int64);
	x_warn(test_str);
	x_const_expr(test_str, ast::type_info::i64_, ast::constant_value_kind::sint, std::numeric_limits<int64_t>::min());

	declare_var("a", "mut i32", "");
	x("++a");
	declare_var("p", "mut *i32", "&a");
	x("++p");
	declare_var("c", "mut char", "");
	x("++c");
	declare_var("b", "mut bool", "");
	x_err("++b");
	declare_var("const_a", "i32", "0");
	x_err("++const_a");
	x_err("++0");

	x("--a");
	x("--p");
	x("--c");
	x_err("--b");
	x_err("--const_a");
	x_err("--0");

	x_const_expr("~0u8", ast::type_info::u8_, ast::constant_value_kind::uint, 255);
	x_const_expr("~1u32", ast::type_info::u32_, ast::constant_value_kind::uint, std::numeric_limits<uint32_t>::max() - 1);
	x_const_expr("~0b1100'0011u8", ast::type_info::u8_, ast::constant_value_kind::uint, 0b0011'1100u);
	x_const_expr("~false", ast::type_info::bool_, ast::constant_value_kind::boolean, true);
	x_err("~0i32");
	x_err("~0");
	x_err("~' '");

	x_const_expr("!true", ast::type_info::bool_, ast::constant_value_kind::boolean, false);
	x_const_expr("!!true", ast::type_info::bool_, ast::constant_value_kind::boolean, true);
	x_err("!0");
	x_err("!null");
	x_err("!' '");
	x_err("!\"\"");

	x("&a");
	x_err("&0");
	x_err("&(a + 1)");

	x("*&a");
	// x_warn("*(null as *i32)");
	x_err("*a");
	x_err("*0");




	x_const_expr("+3", ast::type_info::i32_, ast::constant_value_kind::sint, 3);
	x_const_expr("!!!!!!true", ast::type_info::bool_, ast::constant_value_kind::boolean, true);
	x_const_expr("(0)", ast::type_info::i32_, ast::constant_value_kind::sint, 0);
	x_const_expr("((((!true))))", ast::type_info::bool_, ast::constant_value_kind::boolean, false);
	x_const_expr("+ + - - 42i8", ast::type_info::i8_, ast::constant_value_kind::sint, 42);
//	x("sizeof 0");


	x_err("++3");
	x_err("&0");

	return {};

#undef x
#undef x_err
#undef x_warn
#undef x_const_expr
}

static bz::optional<bz::u8string> parse_expression_comma_list_test(ctx::global_context &global_ctx)
{
	ctx::lex_context lex_ctx(global_ctx);
	ctx::parse_context parse_ctx(global_ctx);
	parse_ctx.current_global_scope = global_ctx._builtin_global_scope;

#define x(str, res_size) xx(parse_expression_comma_list, str, tokens.end() - 1, res.size() == res_size)
#define x_warn(str, res_size) xx_warn(parse_expression_comma_list, str, tokens.end() - 1, res.size() == res_size)

	x("0, 1, 2, \"hello\"", 4);
	// there's a warning because the lhs of a comma expression has no effect
	// x_warn("(0, 0, 0), 1, 2", 3);
	// x_warn("('a', 'b', 0, 1.5), 'a'", 2);

	return {};

#undef x
#undef x_warn
#undef x_err
}

static auto parse_expression_alt(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	auto result = parse_expression(stream, end, context, precedence{});
	resolve_expression(result, context);
	consteval_guaranteed(result, context);
	return result;
}

//	/*
static bz::optional<bz::u8string> parse_expression_test(ctx::global_context &global_ctx)
{
	ctx::lex_context lex_ctx(global_ctx);
	ctx::parse_context parse_ctx(global_ctx);
	parse_ctx.current_global_scope = global_ctx._builtin_global_scope;

#define x(str) xx(parse_expression_alt, str, tokens.end() - 1, true)
#define x_warn(str) xx_warn(parse_expression_alt, str, tokens.end() - 1, true)
#define x_err(str) xx_err(parse_expression_alt, str, tokens.end() - 1, true)

#define x_base_t(str, type_kind)                                              \
xx(                                                                           \
    parse_expression_alt,                                                     \
    str,                                                                      \
    tokens.end() - 1,                                                         \
    ([&]() {                                                                  \
        if (res.is<ast::constant_expression>())                               \
        {                                                                     \
            auto &const_expr = res.get<ast::constant_expression>();           \
            auto const type = const_expr.type.remove_mut_reference();         \
            return type.is<ast::ts_base_type>()                               \
                && type.get<ast::ts_base_type>().info->kind == (type_kind);   \
        }                                                                     \
        else if (res.is<ast::dynamic_expression>())                           \
        {                                                                     \
            auto &dyn_expr = res.get<ast::dynamic_expression>();              \
            auto const type = dyn_expr.type.remove_mut_reference();           \
            return type.is<ast::ts_base_type>()                               \
                && type.get<ast::ts_base_type>().info->kind == (type_kind);   \
        }                                                                     \
        else                                                                  \
        {                                                                     \
            return false;                                                     \
        }                                                                     \
    }())                                                                      \
)

	using pair_t = std::pair<bz::u8string_view, uint32_t>;
	bz::array signed_vars = {
		pair_t{ "i8_",  ast::type_info::i8_  },
		pair_t{ "i16_", ast::type_info::i16_ },
		pair_t{ "i32_", ast::type_info::i32_ },
		pair_t{ "i64_", ast::type_info::i64_ },
	};
	bz::array unsigned_vars = {
		pair_t{ "u8_",  ast::type_info::u8_  },
		pair_t{ "u16_", ast::type_info::u16_ },
		pair_t{ "u32_", ast::type_info::u32_ },
		pair_t{ "u64_", ast::type_info::u64_ },
	};
	bz::array floating_point_vars = {
		pair_t{ "f32", ast::type_info::float32_ },
		pair_t{ "f64", ast::type_info::float64_ },
	};
	bz::array char_vars = {
		pair_t{ "c", ast::type_info::char_ },
	};


	// add scope to allow variables
	auto local_scope = ast::make_local_scope(parse_ctx.get_current_enclosing_scope(), false);
	parse_ctx.push_local_scope(&local_scope);

	declare_var("i8_",  "mut i8", "");
	declare_var("i16_", "mut i16", "");
	declare_var("i32_", "mut i32", "");
	declare_var("i64_", "mut i64", "");
	declare_var("u8_",  "mut u8", "");
	declare_var("u16_", "mut u16", "");
	declare_var("u32_", "mut u32", "");
	declare_var("u64_", "mut u64", "");
	declare_var("f32", "mut float32", "");
	declare_var("f64", "mut float64", "");
	declare_var("c", "mut char", "");
	declare_var("s", "mut str", "");
	declare_var("p", "mut *i32", "&i32_");
	declare_var("op", "mut ?*i32", "");

	x_err("");

#define test_vars(var1, var2, op, cmp, result_kind)                             \
do {                                                                            \
    for (auto &a : var1)                                                        \
    {                                                                           \
        for (auto &b : var2)                                                    \
        {                                                                       \
            auto const test_str = bz::format("{} " op " {}", a.first, b.first); \
            if (cmp)                                                            \
            {                                                                   \
                x_base_t(test_str, result_kind);                                \
            }                                                                   \
            else                                                                \
            {                                                                   \
                x_err(test_str);                                                \
            }                                                                   \
        }                                                                       \
    }                                                                           \
} while (false)

#define x_ii(op, cmp, result_kind) test_vars(signed_vars,         signed_vars, op, cmp, result_kind)
#define x_iu(op, cmp, result_kind) test_vars(signed_vars,       unsigned_vars, op, cmp, result_kind)
#define x_if(op, cmp, result_kind) test_vars(signed_vars, floating_point_vars, op, cmp, result_kind)
#define x_ic(op, cmp, result_kind) test_vars(signed_vars,           char_vars, op, cmp, result_kind)

#define x_ui(op, cmp, result_kind) test_vars(unsigned_vars,         signed_vars, op, cmp, result_kind)
#define x_uu(op, cmp, result_kind) test_vars(unsigned_vars,       unsigned_vars, op, cmp, result_kind)
#define x_uf(op, cmp, result_kind) test_vars(unsigned_vars, floating_point_vars, op, cmp, result_kind)
#define x_uc(op, cmp, result_kind) test_vars(unsigned_vars,           char_vars, op, cmp, result_kind)

#define x_fi(op, cmp, result_kind) test_vars(floating_point_vars,         signed_vars, op, cmp, result_kind)
#define x_fu(op, cmp, result_kind) test_vars(floating_point_vars,       unsigned_vars, op, cmp, result_kind)
#define x_ff(op, cmp, result_kind) test_vars(floating_point_vars, floating_point_vars, op, cmp, result_kind)
#define x_fc(op, cmp, result_kind) test_vars(floating_point_vars,           char_vars, op, cmp, result_kind)

#define x_ci(op, cmp, result_kind) test_vars(char_vars,         signed_vars, op, cmp, result_kind)
#define x_cu(op, cmp, result_kind) test_vars(char_vars,       unsigned_vars, op, cmp, result_kind)
#define x_cf(op, cmp, result_kind) test_vars(char_vars, floating_point_vars, op, cmp, result_kind)
#define x_cc(op, cmp, result_kind) test_vars(char_vars,           char_vars, op, cmp, result_kind)

	x_ii("=", a.second >= b.second, a.second);
	x_iu("=", false, 0);
	x_if("=", false, 0);
	x_ic("=", false, 0);
	x_ui("=", false, 0);
	x_uu("=", a.second >= b.second, a.second);
	x_uf("=", false, 0);
	x_uc("=", false, 0);
	x_fi("=", false, 0);
	x_fu("=", false, 0);
	x_ff("=", a.second == b.second, a.second);
	x_fc("=", false, 0);
	x_ci("=", false, 0);
	x_cu("=", false, 0);
	x_cf("=", false, 0);
	x_cc("=", true, a.second);


	x_base_t("c = 'a'", ast::type_info::char_);
	x_err("p = null");
	x("op = null");

	x_ii("+", true, std::max(a.second, b.second));
	x_iu("+", false, 0);
	x_if("+", false, 0);
	x_ic("+", true, b.second);
	x_ui("+", false, 0);
	x_uu("+", true, std::max(a.second, b.second));
	x_uf("+", false, 0);
	x_uc("+", true, b.second);
	x_fi("+", false, 0);
	x_fu("+", false, 0);
	x_ff("+", a.second == b.second, a.second);
	x_fc("+", false, 0);
	x_ci("+", true, a.second);
	x_cu("+", true, a.second);
	x_cf("+", false, 0);
	x_cc("+", false, 0);

	x_ii("+=", a.second >= b.second, a.second);
	x_iu("+=", false, 0);
	x_if("+=", false, 0);
	x_ic("+=", false, 0);
	x_ui("+=", false, 0);
	x_uu("+=", a.second >= b.second, a.second);
	x_uf("+=", false, 0);
	x_uc("+=", false, 0);
	x_fi("+=", false, 0);
	x_fu("+=", false, 0);
	x_ff("+=", a.second == b.second, a.second);
	x_fc("+=", false, 0);
	x_ci("+=", true, a.second);
	x_cu("+=", true, a.second);
	x_cf("+=", false, 0);
	x_cc("+=", false, 0);

	x_ii("-", true, std::max(a.second, b.second));
	x_iu("-", false, 0);
	x_if("-", false, 0);
	x_ic("-", false, 0);
	x_ui("-", false, 0);
	x_uu("-", true, std::max(a.second, b.second));
	x_uf("-", false, 0);
	x_uc("-", false, 0);
	x_fi("-", false, 0);
	x_fu("-", false, 0);
	x_ff("-", a.second == b.second, a.second);
	x_fc("-", false, 0);
	x_ci("-", true, a.second);
	x_cu("-", true, a.second);
	x_cf("-", false, 0);
	x_cc("-", true, ast::type_info::i32_);

	x_ii("-=", a.second >= b.second, a.second);
	x_iu("-=", false, 0);
	x_if("-=", false, 0);
	x_ic("-=", false, 0);
	x_ui("-=", false, 0);
	x_uu("-=", a.second >= b.second, a.second);
	x_uf("-=", false, 0);
	x_uc("-=", false, 0);
	x_fi("-=", false, 0);
	x_fu("-=", false, 0);
	x_ff("-=", a.second == b.second, a.second);
	x_fc("-=", false, 0);
	x_ci("-=", true, a.second);
	x_cu("-=", true, a.second);
	x_cf("-=", false, 0);
	x_cc("-=", false, 0);

	x_ii("*", true, std::max(a.second, b.second));
	x_iu("*", false, 0);
	x_if("*", false, 0);
	x_ic("*", false, 0);
	x_ui("*", false, 0);
	x_uu("*", true, std::max(a.second, b.second));
	x_uf("*", false, 0);
	x_uc("*", false, 0);
	x_fi("*", false, 0);
	x_fu("*", false, 0);
	x_ff("*", a.second == b.second, a.second);
	x_fc("*", false, 0);
	x_ci("*", false, 0);
	x_cu("*", false, 0);
	x_cf("*", false, 0);
	x_cc("*", false, 0);

	x_ii("*=", a.second >= b.second, a.second);
	x_iu("*=", false, 0);
	x_if("*=", false, 0);
	x_ic("*=", false, 0);
	x_ui("*=", false, 0);
	x_uu("*=", a.second >= b.second, a.second);
	x_uf("*=", false, 0);
	x_uc("*=", false, 0);
	x_fi("*=", false, 0);
	x_fu("*=", false, 0);
	x_ff("*=", a.second == b.second, a.second);
	x_fc("*=", false, 0);
	x_ci("*=", false, 0);
	x_cu("*=", false, 0);
	x_cf("*=", false, 0);
	x_cc("*=", false, 0);

	x_ii("/", true, std::max(a.second, b.second));
	x_iu("/", false, 0);
	x_if("/", false, 0);
	x_ic("/", false, 0);
	x_ui("/", false, 0);
	x_uu("/", true, std::max(a.second, b.second));
	x_uf("/", false, 0);
	x_uc("/", false, 0);
	x_fi("/", false, 0);
	x_fu("/", false, 0);
	x_ff("/", a.second == b.second, a.second);
	x_fc("/", false, 0);
	x_ci("/", false, 0);
	x_cu("/", false, 0);
	x_cf("/", false, 0);
	x_cc("/", false, 0);

	x_ii("/=", a.second >= b.second, a.second);
	x_iu("/=", false, 0);
	x_if("/=", false, 0);
	x_ic("/=", false, 0);
	x_ui("/=", false, 0);
	x_uu("/=", a.second >= b.second, a.second);
	x_uf("/=", false, 0);
	x_uc("/=", false, 0);
	x_fi("/=", false, 0);
	x_fu("/=", false, 0);
	x_ff("/=", a.second == b.second, a.second);
	x_fc("/=", false, 0);
	x_ci("/=", false, 0);
	x_cu("/=", false, 0);
	x_cf("/=", false, 0);
	x_cc("/=", false, 0);

	x_ii("%", true, std::max(a.second, b.second));
	x_iu("%", false, 0);
	x_if("%", false, 0);
	x_ic("%", false, 0);
	x_ui("%", false, 0);
	x_uu("%", true, std::max(a.second, b.second));
	x_uf("%", false, 0);
	x_uc("%", false, 0);
	x_fi("%", false, 0);
	x_fu("%", false, 0);
	x_ff("%", false, 0);
	x_fc("%", false, 0);
	x_ci("%", false, 0);
	x_cu("%", false, 0);
	x_cf("%", false, 0);
	x_cc("%", false, 0);

	x_ii("%=", a.second >= b.second, a.second);
	x_iu("%=", false, 0);
	x_if("%=", false, 0);
	x_ic("%=", false, 0);
	x_ui("%=", false, 0);
	x_uu("%=", a.second >= b.second, a.second);
	x_uf("%=", false, 0);
	x_uc("%=", false, 0);
	x_fi("%=", false, 0);
	x_fu("%=", false, 0);
	x_ff("%=", false, 0);
	x_fc("%=", false, 0);
	x_ci("%=", false, 0);
	x_cu("%=", false, 0);
	x_cf("%=", false, 0);
	x_cc("%=", false, 0);

	x_ii("==", true, ast::type_info::bool_);
	x_iu("==", false, 0);
	x_if("==", false, 0);
	x_ic("==", false, 0);
	x_ui("==", false, 0);
	x_uu("==", true, ast::type_info::bool_);
	x_uf("==", false, 0);
	x_uc("==", false, 0);
	x_fi("==", false, 0);
	x_fu("==", false, 0);
	x_ff("==", a.second == b.second, ast::type_info::bool_);
	x_fc("==", false, 0);
	x_ci("==", false, 0);
	x_cu("==", false, 0);
	x_cf("==", false, 0);
	x_cc("==", true, ast::type_info::bool_);

	x_ii("!=", true, ast::type_info::bool_);
	x_iu("!=", false, 0);
	x_if("!=", false, 0);
	x_ic("!=", false, 0);
	x_ui("!=", false, 0);
	x_uu("!=", true, ast::type_info::bool_);
	x_uf("!=", false, 0);
	x_uc("!=", false, 0);
	x_fi("!=", false, 0);
	x_fu("!=", false, 0);
	x_ff("!=", a.second == b.second, ast::type_info::bool_);
	x_fc("!=", false, 0);
	x_ci("!=", false, 0);
	x_cu("!=", false, 0);
	x_cf("!=", false, 0);
	x_cc("!=", true, ast::type_info::bool_);

	x_ii("<", true, ast::type_info::bool_);
	x_iu("<", false, 0);
	x_if("<", false, 0);
	x_ic("<", false, 0);
	x_ui("<", false, 0);
	x_uu("<", true, ast::type_info::bool_);
	x_uf("<", false, 0);
	x_uc("<", false, 0);
	x_fi("<", false, 0);
	x_fu("<", false, 0);
	x_ff("<", a.second == b.second, ast::type_info::bool_);
	x_fc("<", false, 0);
	x_ci("<", false, 0);
	x_cu("<", false, 0);
	x_cf("<", false, 0);
	x_cc("<", true, ast::type_info::bool_);

	x_ii("<=", true, ast::type_info::bool_);
	x_iu("<=", false, 0);
	x_if("<=", false, 0);
	x_ic("<=", false, 0);
	x_ui("<=", false, 0);
	x_uu("<=", true, ast::type_info::bool_);
	x_uf("<=", false, 0);
	x_uc("<=", false, 0);
	x_fi("<=", false, 0);
	x_fu("<=", false, 0);
	x_ff("<=", a.second == b.second, ast::type_info::bool_);
	x_fc("<=", false, 0);
	x_ci("<=", false, 0);
	x_cu("<=", false, 0);
	x_cf("<=", false, 0);
	x_cc("<=", true, ast::type_info::bool_);

	x_ii(">", true, ast::type_info::bool_);
	x_iu(">", false, 0);
	x_if(">", false, 0);
	x_ic(">", false, 0);
	x_ui(">", false, 0);
	x_uu(">", true, ast::type_info::bool_);
	x_uf(">", false, 0);
	x_uc(">", false, 0);
	x_fi(">", false, 0);
	x_fu(">", false, 0);
	x_ff(">", a.second == b.second, ast::type_info::bool_);
	x_fc(">", false, 0);
	x_ci(">", false, 0);
	x_cu(">", false, 0);
	x_cf(">", false, 0);
	x_cc(">", true, ast::type_info::bool_);

	x_ii(">=", true, ast::type_info::bool_);
	x_iu(">=", false, 0);
	x_if(">=", false, 0);
	x_ic(">=", false, 0);
	x_ui(">=", false, 0);
	x_uu(">=", true, ast::type_info::bool_);
	x_uf(">=", false, 0);
	x_uc(">=", false, 0);
	x_fi(">=", false, 0);
	x_fu(">=", false, 0);
	x_ff(">=", a.second == b.second, ast::type_info::bool_);
	x_fc(">=", false, 0);
	x_ci(">=", false, 0);
	x_cu(">=", false, 0);
	x_cf(">=", false, 0);
	x_cc(">=", true, ast::type_info::bool_);

	x_ii("&", false, 0);
	x_iu("&", false, 0);
	x_if("&", false, 0);
	x_ic("&", false, 0);
	x_ui("&", false, 0);
	x_uu("&", true, std::max(a.second, b.second));
	x_uf("&", false, 0);
	x_uc("&", false, 0);
	x_fi("&", false, 0);
	x_fu("&", false, 0);
	x_ff("&", false, 0);
	x_fc("&", false, 0);
	x_ci("&", false, 0);
	x_cu("&", false, 0);
	x_cf("&", false, 0);
	x_cc("&", false, 0);

	x_ii("^", false, 0);
	x_iu("^", false, 0);
	x_if("^", false, 0);
	x_ic("^", false, 0);
	x_ui("^", false, 0);
	x_uu("^", true, std::max(a.second, b.second));
	x_uf("^", false, 0);
	x_uc("^", false, 0);
	x_fi("^", false, 0);
	x_fu("^", false, 0);
	x_ff("^", false, 0);
	x_fc("^", false, 0);
	x_ci("^", false, 0);
	x_cu("^", false, 0);
	x_cf("^", false, 0);
	x_cc("^", false, 0);

	x_ii("|", false, 0);
	x_iu("|", false, 0);
	x_if("|", false, 0);
	x_ic("|", false, 0);
	x_ui("|", false, 0);
	x_uu("|", true, std::max(a.second, b.second));
	x_uf("|", false, 0);
	x_uc("|", false, 0);
	x_fi("|", false, 0);
	x_fu("|", false, 0);
	x_ff("|", false, 0);
	x_fc("|", false, 0);
	x_ci("|", false, 0);
	x_cu("|", false, 0);
	x_cf("|", false, 0);
	x_cc("|", false, 0);

	x_ii("&=", false, 0);
	x_iu("&=", false, 0);
	x_if("&=", false, 0);
	x_ic("&=", false, 0);
	x_ui("&=", false, 0);
	x_uu("&=", a.second >= b.second, a.second);
	x_uf("&=", false, 0);
	x_uc("&=", false, 0);
	x_fi("&=", false, 0);
	x_fu("&=", false, 0);
	x_ff("&=", false, 0);
	x_fc("&=", false, 0);
	x_ci("&=", false, 0);
	x_cu("&=", false, 0);
	x_cf("&=", false, 0);
	x_cc("&=", false, 0);

	x_ii("^=", false, 0);
	x_iu("^=", false, 0);
	x_if("^=", false, 0);
	x_ic("^=", false, 0);
	x_ui("^=", false, 0);
	x_uu("^=", a.second >= b.second, a.second);
	x_uf("^=", false, 0);
	x_uc("^=", false, 0);
	x_fi("^=", false, 0);
	x_fu("^=", false, 0);
	x_ff("^=", false, 0);
	x_fc("^=", false, 0);
	x_ci("^=", false, 0);
	x_cu("^=", false, 0);
	x_cf("^=", false, 0);
	x_cc("^=", false, 0);

	x_ii("|=", false, 0);
	x_iu("|=", false, 0);
	x_if("|=", false, 0);
	x_ic("|=", false, 0);
	x_ui("|=", false, 0);
	x_uu("|=", a.second >= b.second, a.second);
	x_uf("|=", false, 0);
	x_uc("|=", false, 0);
	x_fi("|=", false, 0);
	x_fu("|=", false, 0);
	x_ff("|=", false, 0);
	x_fc("|=", false, 0);
	x_ci("|=", false, 0);
	x_cu("|=", false, 0);
	x_cf("|=", false, 0);
	x_cc("|=", false, 0);

	x_ii("<<", false, 0);
	x_iu("<<", false, 0);
	x_if("<<", false, 0);
	x_ic("<<", false, 0);
	x_ui("<<", true, a.second);
	x_uu("<<", true, a.second);
	x_uf("<<", false, 0);
	x_uc("<<", false, 0);
	x_fi("<<", false, 0);
	x_fu("<<", false, 0);
	x_ff("<<", false, 0);
	x_fc("<<", false, 0);
	x_ci("<<", false, 0);
	x_cu("<<", false, 0);
	x_cf("<<", false, 0);
	x_cc("<<", false, 0);

	x_ii(">>", false, 0);
	x_iu(">>", false, 0);
	x_if(">>", false, 0);
	x_ic(">>", false, 0);
	x_ui(">>", true, a.second);
	x_uu(">>", true, a.second);
	x_uf(">>", false, 0);
	x_uc(">>", false, 0);
	x_fi(">>", false, 0);
	x_fu(">>", false, 0);
	x_ff(">>", false, 0);
	x_fc(">>", false, 0);
	x_ci(">>", false, 0);
	x_cu(">>", false, 0);
	x_cf(">>", false, 0);
	x_cc(">>", false, 0);

	x_ii("<<=", false, 0);
	x_iu("<<=", false, 0);
	x_if("<<=", false, 0);
	x_ic("<<=", false, 0);
	x_ui("<<=", true, a.second);
	x_uu("<<=", true, a.second);
	x_uf("<<=", false, 0);
	x_uc("<<=", false, 0);
	x_fi("<<=", false, 0);
	x_fu("<<=", false, 0);
	x_ff("<<=", false, 0);
	x_fc("<<=", false, 0);
	x_ci("<<=", false, 0);
	x_cu("<<=", false, 0);
	x_cf("<<=", false, 0);
	x_cc("<<=", false, 0);

	x_ii(">>=", false, 0);
	x_iu(">>=", false, 0);
	x_if(">>=", false, 0);
	x_ic(">>=", false, 0);
	x_ui(">>=", true, a.second);
	x_uu(">>=", true, a.second);
	x_uf(">>=", false, 0);
	x_uc(">>=", false, 0);
	x_fi(">>=", false, 0);
	x_fu(">>=", false, 0);
	x_ff(">>=", false, 0);
	x_fc(">>=", false, 0);
	x_ci(">>=", false, 0);
	x_cu(">>=", false, 0);
	x_cf(">>=", false, 0);
	x_cc(">>=", false, 0);

	x_ii("&&", false, 0);
	x_iu("&&", false, 0);
	x_if("&&", false, 0);
	x_ic("&&", false, 0);
	x_ui("&&", false, 0);
	x_uu("&&", false, 0);
	x_uf("&&", false, 0);
	x_uc("&&", false, 0);
	x_fi("&&", false, 0);
	x_fu("&&", false, 0);
	x_ff("&&", false, 0);
	x_fc("&&", false, 0);
	x_ci("&&", false, 0);
	x_cu("&&", false, 0);
	x_cf("&&", false, 0);
	x_cc("&&", false, 0);

	x_ii("^^", false, 0);
	x_iu("^^", false, 0);
	x_if("^^", false, 0);
	x_ic("^^", false, 0);
	x_ui("^^", false, 0);
	x_uu("^^", false, 0);
	x_uf("^^", false, 0);
	x_uc("^^", false, 0);
	x_fi("^^", false, 0);
	x_fu("^^", false, 0);
	x_ff("^^", false, 0);
	x_fc("^^", false, 0);
	x_ci("^^", false, 0);
	x_cu("^^", false, 0);
	x_cf("^^", false, 0);
	x_cc("^^", false, 0);

	x_ii("||", false, 0);
	x_iu("||", false, 0);
	x_if("||", false, 0);
	x_ic("||", false, 0);
	x_ui("||", false, 0);
	x_uu("||", false, 0);
	x_uf("||", false, 0);
	x_uc("||", false, 0);
	x_fi("||", false, 0);
	x_fu("||", false, 0);
	x_ff("||", false, 0);
	x_fc("||", false, 0);
	x_ci("||", false, 0);
	x_cu("||", false, 0);
	x_cf("||", false, 0);
	x_cc("||", false, 0);

	return {};

#undef x
#undef x_err
#undef x_warn
#undef x_base_t
}
//	*/

static bz::optional<bz::u8string> constant_expression_test(ctx::global_context &global_ctx)
{
	ctx::lex_context lex_ctx(global_ctx);
	ctx::parse_context parse_ctx(global_ctx);
	parse_ctx.current_global_scope = global_ctx._builtin_global_scope;

#define x(str) xx(parse_expression_alt, str, tokens.end() - 1, true)
#define x_err(str) xx_err(parse_expression_alt, str, tokens.end() - 1, true)
#define x_warn(str) xx_warn(parse_expression_alt, str, tokens.end() - 1, true)

#define x_const_expr(str, const_expr_kind, const_expr_value)                                        \
xx_compiles(                                                                                        \
    parse_expression_alt,                                                                           \
    str,                                                                                            \
    tokens.end() - 1,                                                                               \
    (                                                                                               \
        res.is<ast::constant_expression>()                                                          \
        && res.get<ast::constant_expression>().value.kind() == const_expr_kind                      \
        && (res.get<ast::constant_expression>().value.get<const_expr_kind>()) == (const_expr_value) \
    )                                                                                               \
)

#define x_const_expr_bool(str, value)                                         \
x_const_expr(str, ast::type_info::bool_, ast::constant_value_kind::boolean, value)


	x_const_expr("40 + 2", ast::constant_value_kind::sint, 42);
	x_const_expr("40u32 + 2u32", ast::constant_value_kind::uint, 42u);
	x_const_expr("255u8 + 3u8", ast::constant_value_kind::uint, uint8_t(258));
	x_warn("255u8 + 3u8");
	x("((255u8 + 3u8))");
	x_const_expr("~0u64", ast::constant_value_kind::uint, std::numeric_limits<uint64_t>::max());

/*
	x_const_expr("42", ast::type_info::i32_, ast::constant_value_kind::sint, 42);
	x_const_expr("40 + 2", ast::type_info::i32_, ast::constant_value_kind::sint, 42);
	x_const_expr("40 as u32", ast::type_info::u32_, ast::constant_value_kind::uint, 40);
	x_const_expr("257 as u8", ast::type_info::u8_, ast::constant_value_kind::uint, 1);

	x_const_expr("255u8 + 1u16", ast::type_info::u16_, ast::constant_value_kind::uint, 256);
	x_const_expr("500 * 500", ast::type_info::i32_, ast::constant_value_kind::sint, 250'000);
	x_const_expr("500u32 * 100u8", ast::type_info::u32_, ast::constant_value_kind::uint, 50'000);
	x_const_expr("100u8 * 500u32", ast::type_info::u32_, ast::constant_value_kind::uint, 50'000);
	x_const_expr("500 / 500", ast::type_info::i32_, ast::constant_value_kind::sint, 1);
	x_const_expr("500u32 / 100u8", ast::type_info::u32_, ast::constant_value_kind::uint, 5);
	x_const_expr("100u8 / 500u32", ast::type_info::u32_, ast::constant_value_kind::uint, 0);
	x_const_expr("500 % 500", ast::type_info::i32_, ast::constant_value_kind::sint, 0);
	x_const_expr("13 % 9", ast::type_info::i32_, ast::constant_value_kind::sint, 4);
	x_const_expr("500u32 % 100u8", ast::type_info::u32_, ast::constant_value_kind::uint, 0);
	x_const_expr("100u8 % 500u32", ast::type_info::u32_, ast::constant_value_kind::uint, 100);
	x_const_expr_bool("0 == 0", true);
	x_const_expr_bool("0 == 1", false);
	x_const_expr_bool("0u8 == 300u16", false);
	x_const_expr_bool("-127 == (0i8 - 127i8)", true);
	x_const_expr_bool("1.5 == 3.0 / 2.0", true);
	x_const_expr_bool("0.1f32 == 0.1f32", true);
	x_const_expr_bool("1.0 == 5.1 / 123.4", false);
	x_const_expr_bool("-1.0 == 1.0", false);
	x_const_expr_bool("-0.0 == 0.0", true);
	x_const_expr_bool("'a' == 'a'", true);
	x_const_expr_bool("'a' == 'b'", false);
	x_const_expr_bool("0 != 0", false);
	x_const_expr_bool("0 != 1", true);
	x_const_expr_bool("0u8 != 300u16", true);
	x_const_expr_bool("-127 != (0i8 - 127i8)", false);
	x_const_expr_bool("1.5 != 3.0 / 2.0", false);
	x_const_expr_bool("0.1f32 != 0.1f32", false);
	x_const_expr_bool("1.0 != 5.1 / 123.4", true);
	x_const_expr_bool("-1.0 != 1.0", true);
	x_const_expr_bool("-0.0 != 0.0", false);
	x_const_expr_bool("'a' != 'a'", false);
	x_const_expr_bool("'a' != 'b'", true);
*/

	return {};
#undef x
#undef x_err
#undef x_warn
#undef x_const_expr
#undef x_const_expr_bool
}

static bz::optional<bz::u8string> parse_typespec_test(ctx::global_context &global_ctx)
{
	ctx::lex_context lex_ctx(global_ctx);
	ctx::parse_context parse_ctx(global_ctx);
	parse_ctx.current_global_scope = global_ctx._builtin_global_scope;

#define x(str, it_pos, kind_) xx(parse_expression_alt, str, it_pos, res.is_typename() && res.get_typename().is<kind_>())
#define x_err(str, it_pos) xx_err(parse_expression_alt, str, it_pos, res.is_error())

	x("i32", tokens.begin() + 1, ast::ts_base_type);
	x("i32 a", tokens.begin() + 1, ast::ts_base_type);
	x("void", tokens.begin() + 1, ast::ts_void);

	x("*i32", tokens.begin() + 2, ast::ts_pointer);

	x("mut i32", tokens.begin() + 2, ast::ts_mut);

	x("&i32", tokens.begin() + 2, ast::ts_lvalue_reference);

	x("[]", tokens.begin() + 2, ast::ts_tuple);
	x("[i32, float64, __null_t]", tokens.begin() + 7, ast::ts_tuple);

	// x("function() -> void", tokens.begin() + 5, ast::ts_function);
	// x("function(i32, i32) -> void", tokens.begin() + 8, ast::ts_function);

	x_err("", tokens.begin());
	x_err("foo", tokens.begin() + 1);
	x_err("*foo", tokens.begin() + 2);
	// x_err("function()", tokens.begin() + 3);
	// x_err("function(,) -> void", tokens.begin() + 6);
	// x_err("function(, i32) -> void", tokens.begin() + 7);

	return {};

#undef x
#undef x_err
}

test_result parser_test(ctx::global_context &global_ctx)
{
	test_begin();

	test_fn(get_paren_matched_range_test, global_ctx);
	global_ctx.report_and_clear_errors_and_warnings();
	test_fn(parse_primary_expression_test, global_ctx);
	global_ctx.report_and_clear_errors_and_warnings();
	test_fn(parse_expression_comma_list_test, global_ctx);
	global_ctx.report_and_clear_errors_and_warnings();
	test_fn(parse_expression_test, global_ctx);
	global_ctx.report_and_clear_errors_and_warnings();
	test_fn(constant_expression_test, global_ctx);
	global_ctx.report_and_clear_errors_and_warnings();
	test_fn(parse_typespec_test, global_ctx);
	global_ctx.report_and_clear_errors_and_warnings();

	test_end();
}
