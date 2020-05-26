#include "parser.cpp"
#include "test.h"

#include "lex/lexer.h"
#include "ctx/global_context.h"
#include "ctx/lex_context.h"
#include "ctx/parse_context.h"

#define xxx(fn, str, it_pos, error_assert, custom_assert)   \
do {                                                        \
    bz::u8string_view const file = str;                     \
    auto const tokens = lex::get_tokens(file, "", lex_ctx); \
    assert_false(global_ctx.has_errors_or_warnings());      \
    auto it = tokens.begin();                               \
    auto res = fn(it, tokens.end() - 1, parse_ctx);         \
    assert_eq(it, it_pos);                                  \
    assert_true(error_assert);                              \
    assert_true(custom_assert);                             \
    global_ctx.clear_errors_and_warnings();                 \
} while (false)

#define xx(fn, str, it_pos, custom_assert)                                \
xxx(fn, str, it_pos, !global_ctx.has_errors_or_warnings(), custom_assert)

#define xx_compiles(fn, str, it_pos, custom_assert)           \
xxx(fn, str, it_pos, !global_ctx.has_errors(), custom_assert)

#define xx_warn(fn, str, it_pos, custom_assert)                                            \
xxx(fn, str, it_pos, !global_ctx.has_errors() && global_ctx.has_warnings(), custom_assert)

#define xx_err(fn, str, it_pos, custom_assert)               \
xxx(fn, str, it_pos, global_ctx.has_errors(), custom_assert)

std::list<bz::vector<lex::token>> var_tokens = {};
bz::vector<ast::decl_variable_ptr> var_decls = {};

#define declare_var(id_str, type_str)                                          \
do {                                                                           \
    var_tokens.emplace_back(lex::get_tokens(id_str, "", lex_ctx));             \
    auto const &name_tokens = var_tokens.back();                               \
    assert_eq(name_tokens.size(), 2);                                          \
    assert_eq(name_tokens[0].kind, lex::token::identifier);                    \
    auto const id = name_tokens.begin();                                       \
    var_tokens.emplace_back(lex::get_tokens(type_str, "", lex_ctx));           \
    auto const &type_tokens = var_tokens.back();                               \
    auto type_stream = type_tokens.begin();                                    \
    auto type = parse_typespec(type_stream, type_tokens.end() - 1, parse_ctx); \
    assert_false(global_ctx.has_errors());                                     \
    assert_eq(type_stream, type_tokens.end() - 1);                             \
    auto decl = ast::make_decl_variable(                                       \
        lex::token_range{id, id + 1}, id, ast::typespec{}, std::move(type)     \
    );                                                                         \
    resolve(decl, parse_ctx);                                                  \
    var_decls.emplace_back(std::move(decl.get<ast::decl_variable_ptr>()));     \
    assert_false(global_ctx.has_errors());                                     \
    parse_ctx.add_local_variable(*var_decls.back());                           \
    assert_false(global_ctx.has_errors());                                     \
} while (false)





static void get_paren_matched_range_test(void)
{
	ctx::global_context global_ctx;
	ctx::lex_context lex_ctx(global_ctx);

#define x(str, it_pos)                                      \
do {                                                        \
    bz::u8string_view const file = str;                     \
    auto const tokens = lex::get_tokens(file, "", lex_ctx); \
    assert_false(global_ctx.has_errors());                  \
    auto it = tokens.begin();                               \
    get_paren_matched_range(it, tokens.end());              \
    assert_eq(it, it_pos);                                  \
} while (false)

	// the function expects that the leading parenthesis has been consumed

	x(") a", tokens.begin() + 1);
	//   ^ tokens.begin() + 1
	x("] a", tokens.begin() + 1);
	//   ^ tokens.begin() + 1
	x("} a", tokens.begin() + 1);
	//   ^ tokens.begin() + 1
	x("()) a", tokens.begin() + 3);
	//     ^ tokens.begin() + 3
	x("(())[][]{{}}] a", tokens.begin() + 13);
	//               ^ tokens.begin() + 13

#undef x
}

static constexpr bool operator == (ast::internal::null_t, ast::internal::null_t) noexcept
{ return true; }

static void parse_primary_expression_test(void)
{
	ctx::global_context global_ctx;
	ctx::lex_context lex_ctx(global_ctx);
	ctx::parse_context parse_ctx(global_ctx);

#define x(str) xx(parse_primary_expression, str, tokens.end() - 1, true)
#define x_warn(str) xx_warn(parse_primary_expression, str, tokens.end() - 1, true)
#define x_err(str) xx_err(parse_primary_expression, str, tokens.end() - 1, true)

#define x_const_expr(str, _type, const_expr_kind, const_expr_value)                                   \
xx_compiles(                                                                                          \
    parse_primary_expression,                                                                         \
    str,                                                                                              \
    tokens.end() - 1,                                                                                 \
    (                                                                                                 \
        res.is<ast::constant_expression>()                                                            \
        && res.get<ast::constant_expression>().type.is<ast::ts_base_type>()                           \
        && res.get<ast::constant_expression>().type.get<ast::ts_base_type_ptr>()->info->kind == _type \
        && res.get<ast::constant_expression>().value.kind() == const_expr_kind                        \
        && (res.get<ast::constant_expression>().value.get<const_expr_kind>()) == (const_expr_value)   \
    )                                                                                                 \
)

	// add scope to allow variables
	parse_ctx.add_scope();


	x_err("");

	declare_var("a", "int32");
	x("a");
	x_err("this_doesnt_exist");

	x_const_expr("42", ast::type_info::int32_, ast::constant_value::sint, 42);
	auto const min_int64_val = static_cast<int64_t>(std::numeric_limits<int32_t>::max()) + 1;
	auto const min_int64_str = bz::format("{}", min_int64_val);
	x_const_expr(min_int64_str, ast::type_info::int64_, ast::constant_value::sint, min_int64_val);
	auto const min_uint64_val = static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + 1;
	auto const min_uint64_str = bz::format("{}", min_uint64_val);
	x_const_expr(min_uint64_str, ast::type_info::uint64_, ast::constant_value::uint, min_uint64_val);
	x_err("999999999999999999999999999");

	x_err("32i123456");

	x_const_expr("42i8",  ast::type_info::int8_,  ast::constant_value::sint, 42);
	x_const_expr("42i16", ast::type_info::int16_, ast::constant_value::sint, 42);
	x_const_expr("42i32", ast::type_info::int32_, ast::constant_value::sint, 42);
	x_const_expr("42i64", ast::type_info::int64_, ast::constant_value::sint, 42);
	x_const_expr("42u8",  ast::type_info::uint8_,  ast::constant_value::uint, 42);
	x_const_expr("42u16", ast::type_info::uint16_, ast::constant_value::uint, 42);
	x_const_expr("42u32", ast::type_info::uint32_, ast::constant_value::uint, 42);
	x_const_expr("42u64", ast::type_info::uint64_, ast::constant_value::uint, 42);
	x_err("128i8");

	x_const_expr("1.5", ast::type_info::float64_, ast::constant_value::float64, 1.5);
	x_err("1.5f123456");

	x_const_expr("1.5f32", ast::type_info::float32_, ast::constant_value::float32, 1.5f);
	x_const_expr("1.5f64", ast::type_info::float64_, ast::constant_value::float64, 1.5);

	x_const_expr("0x42", ast::type_info::uint32_, ast::constant_value::uint, 0x42);
	x_const_expr("0x1234'5678'90ab'cdef", ast::type_info::uint64_, ast::constant_value::uint, 0x1234'5678'90ab'cdef);
	x_err("0x1'1234'5678'90ab'cdef");
	x_const_expr("0x42i8", ast::type_info::int8_, ast::constant_value::sint, 0x42);
	x_err("0xffi8");
	x_err("0x1'ffff'ffff'ffff'ffff");

	x_const_expr("0o42", ast::type_info::uint32_, ast::constant_value::uint, (4 * 8 + 2));
	auto const min_uint64_oct_str = bz::format("0o{:o}", 1ull << 32);
	x_const_expr(min_uint64_oct_str, ast::type_info::uint64_, ast::constant_value::uint, static_cast<uint64_t>(1ull << 32));
	x_const_expr("0o42i8", ast::type_info::int8_, ast::constant_value::sint, (4 * 8 + 2));
	x_err("0o200i8");

	x_const_expr("0b1010'0101", ast::type_info::uint32_, ast::constant_value::uint, 0b1010'0101);
	x_const_expr("0b'1'0000'0000'0000'0000'0000'0000'0000'0000", ast::type_info::uint64_, ast::constant_value::uint, 1ull << 32);
	x_const_expr("0b0110'0101'i8", ast::type_info::int8_, ast::constant_value::sint, 0b0110'0101);
	x_err("0b1000'0000'i8");
	x_err("0b'1''0000'0000''0000'0000''0000'0000''0000'0000''''0000'0000''0000'0000''0000'0000''0000'0000");

	x_const_expr("'a'", ast::type_info::char_, ast::constant_value::u8char, 'a');
	x_err("'a'asdf");
	x_const_expr("'\\x7f'", ast::type_info::char_, ast::constant_value::u8char, 0x7f);
	x_const_expr("'\\u0470'", ast::type_info::char_, ast::constant_value::u8char, 0x470);
	x_const_expr("'\\U00000470'", ast::type_info::char_, ast::constant_value::u8char, 0x470);
	x_const_expr("'Ѱ'", ast::type_info::char_, ast::constant_value::u8char, 0x470);
//	x_err("'\\U000110000'", 1); // this is handled while lexing

	x_const_expr("true", ast::type_info::bool_, ast::constant_value::boolean, true);
	x_const_expr("false", ast::type_info::bool_, ast::constant_value::boolean, false);
	x_const_expr("null", ast::type_info::null_t_, ast::constant_value::null, ast::internal::null_t{});

	x_const_expr(R"( "" )", ast::type_info::str_, ast::constant_value::string, "");
	x_const_expr(R"( "hello!!" )", ast::type_info::str_, ast::constant_value::string, "hello!!");
	x_const_expr(R"( "hello	!!" )", ast::type_info::str_, ast::constant_value::string, "hello\t!!");
	x_const_expr(R"( "hello\t!!" )", ast::type_info::str_, ast::constant_value::string, "hello\t!!");
	x_const_expr(R"( "hello!!\u0470" )", ast::type_info::str_, ast::constant_value::string, "hello!!Ѱ");
	x_const_expr(R"( "hello" " again" " and again!" )", ast::type_info::str_, ast::constant_value::string, "hello again and again!");


	x("(0)");
	x_err("(0 0)");
	x_err("()");

	x_const_expr("+42", ast::type_info::int32_, ast::constant_value::sint, 42);
	x_err("+ 'a'");

	x_const_expr("-42", ast::type_info::int32_, ast::constant_value::sint, -42);
	x_err("-42u32");
	x_warn("-(-128 as int8)");
	auto const max_int64 = std::numeric_limits<int64_t>::max();
	auto const test_str = bz::format("-(-{} - 1)", max_int64);
	x_warn(test_str);
	x_const_expr(test_str, ast::type_info::int64_, ast::constant_value::sint, std::numeric_limits<int64_t>::min());

	declare_var("a", "int32");
	x("++a");
	declare_var("p", "*int32");
	x("++p");
	declare_var("c", "char");
	x("++c");
	declare_var("b", "bool");
	x_err("++b");
	declare_var("const_a", "const int32");
	x_err("++const_a");
	x_err("++0");

	x("--a");
	x("--p");
	x("--c");
	x_err("--b");
	x_err("--const_a");
	x_err("--0");

	x_const_expr("~0u8", ast::type_info::uint8_, ast::constant_value::uint, 255);
	x_const_expr("~1u32", ast::type_info::uint32_, ast::constant_value::uint, std::numeric_limits<uint32_t>::max() - 1);
	x_const_expr("~0b1100'0011u8", ast::type_info::uint8_, ast::constant_value::uint, 0b0011'1100u);
	x_const_expr("~false", ast::type_info::bool_, ast::constant_value::boolean, true);
	x_err("~0i32");
	x_err("~0");
	x_err("~' '");

	x_const_expr("!true", ast::type_info::bool_, ast::constant_value::boolean, false);
	x_const_expr("!!true", ast::type_info::bool_, ast::constant_value::boolean, true);
	x_err("!0");
	x_err("!null");
	x_err("!' '");
	x_err("!\"\"");

	x("&a");
	x_err("&0");
	x_err("&(a + 1)");

	x("*&a");
	x_warn("*(null as *int32)");
	x_err("*a");
	x_err("*0");




	x_const_expr("+3", ast::type_info::int32_, ast::constant_value::sint, 3);
	x_const_expr("!!!!!!true", ast::type_info::bool_, ast::constant_value::boolean, true);
	x_const_expr("(0)", ast::type_info::int32_, ast::constant_value::sint, 0);
	x_const_expr("((((!true))))", ast::type_info::bool_, ast::constant_value::boolean, false);
	x_const_expr("+ + - - 42i8", ast::type_info::int8_, ast::constant_value::sint, 42);
//	x("sizeof 0");


	x_err("++3");
	x_err("&0");

#undef x
#undef x_err
#undef x_warn
#undef x_const_expr
}

static void parse_expression_comma_list_test(void)
{
	ctx::global_context global_ctx;
	ctx::lex_context lex_ctx(global_ctx);
	ctx::parse_context parse_ctx(global_ctx);

#define x(str, res_size) xx(parse_expression_comma_list, str, tokens.end() - 1, res.size() == res_size)

	x("0, 1, 2, \"hello\"", 4);
	x("(0, 0, 0), 1, 2", 3);
	x("('a', 'b', 0, 1.5), 'a'", 2);

#undef x
#undef x_err
}

static auto parse_expression_alt(
	lex::token_pos &stream, lex::token_pos end,
	ctx::parse_context &context
)
{
	return parse_expression(stream, end, context, precedence{});
}

static void parse_expression_test(void)
{
	ctx::global_context global_ctx;
	ctx::lex_context lex_ctx(global_ctx);
	ctx::parse_context parse_ctx(global_ctx);

#define x(str) xx(parse_expression_alt, str, tokens.end() - 1, true)
#define x_warn(str) xx_warn(parse_expression_alt, str, tokens.end() - 1, true)
#define x_err(str) xx_err(parse_expression_alt, str, tokens.end() - 1, true)

#define x_const_expr(str, _type, const_expr_kind, const_expr_value)                                   \
xx_compiles(                                                                                          \
    parse_expression_alt,                                                                             \
    str,                                                                                              \
    tokens.end() - 1,                                                                                 \
    (                                                                                                 \
        res.is<ast::constant_expression>()                                                            \
        && res.get<ast::constant_expression>().type.is<ast::ts_base_type>()                           \
        && res.get<ast::constant_expression>().type.get<ast::ts_base_type_ptr>()->info->kind == _type \
        && res.get<ast::constant_expression>().value.kind() == const_expr_kind                        \
        && (res.get<ast::constant_expression>().value.get<const_expr_kind>()) == (const_expr_value)   \
    )                                                                                                 \
)

#define x_base_t(str, type_kind)                                                          \
xx(                                                                                       \
    parse_expression_alt,                                                                 \
    str,                                                                                  \
    tokens.end() - 1,                                                                     \
    ([&res]() {                                                                           \
        if (res.is<ast::constant_expression>())                                           \
        {                                                                                 \
            auto &const_expr = res.get<ast::constant_expression>();                       \
            auto base_type = const_expr.type.get_if<ast::ts_base_type_ptr>();             \
            return base_type && (*base_type)->info->kind == ast::type_info::type_kind##_; \
        }                                                                                 \
        else if (res.is<ast::dynamic_expression>())                                       \
        {                                                                                 \
            auto &dyn_expr = res.get<ast::dynamic_expression>();                          \
            auto base_type = dyn_expr.type.get_if<ast::ts_base_type_ptr>();               \
            return base_type && (*base_type)->info->kind == ast::type_info::type_kind##_; \
        }                                                                                 \
        else                                                                              \
        {                                                                                 \
            return false;                                                                 \
        }                                                                                 \
    }())                                                                                  \
)


	// add scope to allow variables
	parse_ctx.add_scope();

	declare_var("i8",  "int8");
	declare_var("i16", "int16");
	declare_var("i32", "int32");
	declare_var("i64", "int64");
	declare_var("u8",  "uint8");
	declare_var("u16", "uint16");
	declare_var("u32", "uint32");
	declare_var("u64", "uint64");
	declare_var("f32", "float32");
	declare_var("f64", "float64");
	declare_var("c", "char");
	declare_var("s", "str");
	declare_var("p", "*int32");

	x_err("");

	x_const_expr("42", ast::type_info::int32_, ast::constant_value::sint, 42);
	x_const_expr("40 + 2", ast::type_info::int32_, ast::constant_value::sint, 42);
	x_const_expr("40 as uint32", ast::type_info::uint32_, ast::constant_value::uint, 40);
	x_const_expr("257 as uint8", ast::type_info::uint8_, ast::constant_value::uint, 1);

	x_base_t("i8 = i8", int8);
	x_base_t("i16 = i8", int16);
	x_base_t("f32 = f32", float32);
	x_err("i32 = i64");
	x_err("f64 = f32");
	x_base_t("c = 'a'", char);
	x("p = null");

	x_base_t("i8  + i8", int8);
	x_base_t("i16 + i8", int16);
	x_base_t("i8  + i16", int16);
	x_base_t("i32 + i32", int32);
	x_base_t("i16 + i64", int64);
	x_base_t("u8  + u8", uint8);
	x_base_t("u16 + u8", uint16);
	x_base_t("u8  + u16", uint16);
	x_base_t("u32 + u32", uint32);
	x_base_t("u16 + u64", uint64);
	x_base_t("f32 + f32", float32);
	x_base_t("f64 + f64", float64);
	x_err("i32 + u32");
	x_err("f64 + f32");
	x_err("f64 + i32");
	x_err("c + c");
	x_base_t("c + i32", char);
	x_base_t("c + u32", char);
	x_base_t("c + i64", char);
	x_base_t("c + u64", char);
	x("p + i32");
	x_err("p + c");
	x_warn("255u8 + 1u8");
	x_const_expr("255u8 + 1u16", ast::type_info::uint16_, ast::constant_value::uint, 256);

	x_base_t("i8 += i8", int8);
	x_err("i8 += i16");
	x_base_t("i32 += i16", int32);
	x_err("i32 += u16");
	x_base_t("f32 += f32", float32);
	x_err("f64 += f32");
	x_base_t("c += i64", char);
	x_base_t("c += u64", char);
	x("p += i32");
	x("p += u32");
	x_err("p += c");

	x_base_t("i8  - i8", int8);
	x_base_t("i16 - i8", int16);
	x_base_t("i8  - i16", int16);
	x_base_t("i32 - i32", int32);
	x_base_t("i16 - i64", int64);
	x_base_t("u8  - u8", uint8);
	x_base_t("u16 - u8", uint16);
	x_base_t("u8  - u16", uint16);
	x_base_t("u32 - u32", uint32);
	x_base_t("u16 - u64", uint64);
	x_base_t("f32 - f32", float32);
	x_base_t("f64 - f64", float64);
	x_err("i32 - u32");
	x_err("f64 - f32");
	x_err("f64 - i32");
	x_warn("0u8 - 1u8");
	auto const str = bz::format("0i32 - {} as int32", std::numeric_limits<int32_t>::min());
	x_warn(str);
	x_base_t("c - c", int32);
	x_base_t("c - i32", char);
	x_base_t("c - u32", char);
	x_base_t("c - i64", char);
	x_base_t("c - u64", char);
	x_err("i32 - c");
	x_base_t("p - p", int64);
	x("p - i32");
	x("p - u32");

	x_base_t("i8 -= i8", int8);
	x_err("i8 -= i16");
	x_base_t("i32 -= i16", int32);
	x_err("i32 -= u16");
	x_base_t("f32 -= f32", float32);
	x_err("f64 -= f32");
	x_base_t("c -= i64", char);
	x_base_t("c -= u64", char);
	x("p -= i32");
	x("p -= u32");
	x_err("p -= c");

	x_const_expr("500 * 500", ast::type_info::int32_, ast::constant_value::sint, 250'000);
	x_const_expr("500u32 * 100u8", ast::type_info::uint32_, ast::constant_value::uint, 50'000);
	x_const_expr("100u8 * 500u32", ast::type_info::uint32_, ast::constant_value::uint, 50'000);
	x_base_t("i8  * i8", int8);
	x_base_t("i16 * i8", int16);
	x_base_t("i8  * i16", int16);
	x_base_t("i32 * i32", int32);
	x_base_t("i16 * i64", int64);
	x_base_t("u8  * u8", uint8);
	x_base_t("u16 * u8", uint16);
	x_base_t("u8  * u16", uint16);
	x_base_t("u32 * u32", uint32);
	x_base_t("u16 * u64", uint64);
	x_base_t("f32 * f32", float32);
	x_base_t("f64 * f64", float64);
	x_err("i32 * u32");
	x_err("f64 * f32");
	x_err("f64 * i32");
	x_warn("255u8 * 2u8");

	x_const_expr("500 / 500", ast::type_info::int32_, ast::constant_value::sint, 1);
	x_const_expr("500u32 / 100u8", ast::type_info::uint32_, ast::constant_value::uint, 5);
	x_const_expr("100u8 / 500u32", ast::type_info::uint32_, ast::constant_value::uint, 0);
	x_base_t("i8  / i8", int8);
	x_base_t("i16 / i8", int16);
	x_base_t("i8  / i16", int16);
	x_base_t("i32 / i32", int32);
	x_base_t("i16 / i64", int64);
	x_base_t("u8  / u8", uint8);
	x_base_t("u16 / u8", uint16);
	x_base_t("u8  / u16", uint16);
	x_base_t("u32 / u32", uint32);
	x_base_t("u16 / u64", uint64);
	x_base_t("f32 / f32", float32);
	x_base_t("f64 / f64", float64);
	x_err("i32 / u32");
	x_err("f64 / f32");
	x_err("f64 / i32");
	x_err("255u8 / 0u8");
	x_warn("i32 / 0");
	x_warn("u32 / 0u32");


	x("-1");
	x("(((0)))");
	x("1 + 2 + 4 * 7 / 1");
	x("(1.0 - 2.1) / +4.5");
	x("- - - - - -1234");

	x_err("a + 3");

#undef x
#undef x_err
#undef x_warn
#undef x_const_expr
#undef x_base_t
}

static void parse_typespec_test(void)
{
	ctx::global_context global_ctx;
	ctx::lex_context lex_ctx(global_ctx);
	ctx::parse_context parse_ctx(global_ctx);

#define x(str, it_pos, kind_) xx(parse_typespec, str, it_pos, res.kind() == kind_)
#define x_err(str, it_pos, kind_) xx_err(parse_typespec, str, it_pos, res.kind() == kind_)

	x("int32", tokens.begin() + 1, ast::typespec::index<ast::ts_base_type>);
	x("int32 a", tokens.begin() + 1, ast::typespec::index<ast::ts_base_type>);
	x("void", tokens.begin() + 1, ast::typespec::index<ast::ts_void>);

	x("*int32", tokens.begin() + 2, ast::typespec::index<ast::ts_pointer>);

	x("const int32", tokens.begin() + 2, ast::typespec::index<ast::ts_constant>);

	x("&int32", tokens.begin() + 2, ast::typespec::index<ast::ts_reference>);

	x("[]", tokens.begin() + 2, ast::typespec::index<ast::ts_tuple>);
	x("[int32, float64, null_t]", tokens.begin() + 7, ast::typespec::index<ast::ts_tuple>);

	x("function() -> void", tokens.begin() + 5, ast::typespec::index<ast::ts_function>);
	x("function(int32, int32) -> void", tokens.begin() + 8, ast::typespec::index<ast::ts_function>);

	x_err("", tokens.begin(), size_t(-1));
	x_err("foo", tokens.begin() + 1, size_t(-1));
	x_err("*foo", tokens.begin() + 2, ast::typespec::index<ast::ts_pointer>);
	x_err("function()", tokens.begin() + 3, ast::typespec::index<ast::ts_function>);
	x_err("function(,) -> void", tokens.begin() + 6, ast::typespec::index<ast::ts_function>);
	x_err("function(, int32) -> void", tokens.begin() + 7, ast::typespec::index<ast::ts_function>);

#undef x
#undef x_err
}

test_result parser_test(void)
{
	test_begin();

	test(get_paren_matched_range_test);
	test(parse_primary_expression_test);
	test(parse_expression_comma_list_test);
	test(parse_expression_test);
	test(parse_typespec_test);

	test_end();
}
