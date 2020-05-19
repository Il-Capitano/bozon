#include "parser.cpp"
#include "test.h"

#include "lex/lexer.h"
#include "ctx/global_context.h"
#include "ctx/lex_context.h"
#include "ctx/parse_context.h"

#define xx_global(fn, str, it_pos, custom_assert)           \
do {                                                        \
    bz::u8string_view const file = str;                       \
    auto const tokens = lex::get_tokens(file, "", lex_ctx); \
    assert_false(global_ctx.has_errors());                  \
    auto it = tokens.begin();                               \
    auto res = fn(it, tokens.end() - 1, global_ctx);        \
    assert_false(global_ctx.has_errors());                  \
    assert_eq(it, it_pos);                                  \
    assert_true(custom_assert);                             \
} while (false)

#define xx_gloabal_err(fn, str, it_pos, custom_assert)      \
do {                                                        \
    bz::u8string_view const file = str;                       \
    auto const tokens = lex::get_tokens(file, "", lex_ctx); \
    assert_false(global_ctx.has_errors());                  \
    auto it = tokens.begin();                               \
    auto res = fn(it, tokens.end() - 1, global_ctx);        \
    assert_true(global_ctx.has_errors());                   \
    global_ctx.clear_errors();                              \
    assert_eq(it, it_pos);                                  \
} while (false)

#define xx_parse(fn, str, it_pos, custom_assert)            \
do {                                                        \
    bz::u8string_view const file = str;                       \
    auto const tokens = lex::get_tokens(file, "", lex_ctx); \
    assert_false(global_ctx.has_errors());                  \
    auto it = tokens.begin();                               \
    auto res = fn(it, tokens.end() - 1, parse_ctx);         \
    assert_false(global_ctx.has_errors());                  \
    assert_eq(it, it_pos);                                  \
    assert_true(custom_assert);                             \
} while (false)

#define xx_parse_err(fn, str, it_pos, custom_assert)        \
do {                                                        \
    bz::u8string_view const file = str;                       \
    auto const tokens = lex::get_tokens(file, "", lex_ctx); \
    assert_false(global_ctx.has_errors());                  \
    auto it = tokens.begin();                               \
    auto res = fn(it, tokens.end() - 1, parse_ctx);         \
    assert_true(global_ctx.has_errors());                   \
    global_ctx.clear_errors();                              \
    assert_eq(it, it_pos);                                  \
    assert_true(custom_assert);                             \
} while (false)

static void get_paren_matched_range_test(void)
{
	ctx::global_context global_ctx;
	ctx::lex_context lex_ctx(global_ctx);

#define x(str, it_pos)                                      \
do {                                                        \
    bz::u8string_view const file = str;                       \
    auto const tokens = lex::get_tokens(file, "", lex_ctx); \
    assert_false(global_ctx.has_errors());                  \
    auto it = tokens.begin();                               \
    get_paren_matched_range(it, tokens.end());              \
    assert_eq(it, it_pos);                                  \
} while (false)

	// the function expects that we are already in parens

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

static void resolve_literal_test(void)
{
	ctx::global_context global_ctx;
	ctx::lex_context lex_ctx(global_ctx);
	ctx::parse_context parse_ctx(global_ctx);

#define x(str, kind_)                                                                      \
do {                                                                                       \
    bz::u8string_view const file = str;                                                    \
    auto const tokens = lex::get_tokens(file, "", lex_ctx);                                \
    assert_false(global_ctx.has_errors());                                                 \
    auto it = tokens.begin();                                                              \
    auto expr = parse_expression(                                                          \
        it, tokens.end(),                                                                  \
        parse_ctx, precedence{}                                                            \
    );                                                                                     \
    assert_eq(it, tokens.end() - 1);                                                       \
    assert_true(expr.is<ast::constant_expression>());                                      \
    assert_true(expr.get<ast::constant_expression>().type.is<ast::ts_base_type>());        \
    assert_eq(                                                                             \
        ast::type_info::kind_,                                                             \
        expr.get<ast::constant_expression>().type.get<ast::ts_base_type_ptr>()->info->kind \
    );                                                                                     \
} while (false)

	x("0", int32_);
	x("0.0", float64_);
	x("'c'", char_);
	x("\"hello\"", str_);
	x("false", bool_);
	x("true", bool_);
	x("null", null_t_);

#undef x
}

static void parse_primary_expression_test(void)
{
	ctx::global_context global_ctx;
	ctx::lex_context lex_ctx(global_ctx);
	ctx::parse_context parse_ctx(global_ctx);

#define x(str) xx_parse(parse_primary_expression, str, tokens.end() - 1, true)
#define x_err(str, it_pos) xx_parse_err(parse_primary_expression, str, it_pos, true)

	x("1");
	x("+3");
	x("!!!!!!true");
	x("(0)");
	x("((((!false))))");
	x("+ + - - 0");
//	x("sizeof 0");

	x_err("++3", tokens.begin() + 2);
	x_err("&0", tokens.begin() + 2);
	x_err("a", tokens.begin() + 1);

#undef x
#undef x_err
}

static void parse_expression_comma_list_test(void)
{
	ctx::global_context global_ctx;
	ctx::lex_context lex_ctx(global_ctx);
	ctx::parse_context parse_ctx(global_ctx);

#define x(str, res_size) xx_parse(parse_expression_comma_list, str, tokens.end() - 1, res.size() == res_size)

	x("0, 1, 2, \"hello\"", 4);
	x("(0, 0, 0), 1, 2", 3);
	x("('a', 'b', 0, 1.5), 'a'", 2);

#undef x
#undef x_err
}

static void parse_expression_test(void)
{
	ctx::global_context global_ctx;
	ctx::lex_context lex_ctx(global_ctx);
	ctx::parse_context parse_ctx(global_ctx);

#define x(str)                                                   \
do {                                                             \
    bz::u8string_view const file = str;                            \
    auto const tokens = lex::get_tokens(file, "", lex_ctx);      \
    assert_false(global_ctx.has_errors());                       \
    auto it = tokens.begin();                                    \
    parse_expression(it, tokens.end(), parse_ctx, precedence{}); \
    assert_false(global_ctx.has_errors());                       \
    assert_eq(it, tokens.end() - 1);                             \
} while (false)

#define x_err(str, it_pos)                                       \
do {                                                             \
    bz::u8string_view const file = str;                          \
    auto const tokens = lex::get_tokens(file, "", lex_ctx);      \
    assert_false(global_ctx.has_errors());                       \
    auto it = tokens.begin();                                    \
    parse_expression(it, tokens.end(), parse_ctx, precedence{}); \
    assert_true(global_ctx.has_errors());                        \
    global_ctx.clear_errors();                                   \
    assert_eq(it, it_pos);                                       \
} while (false)

	x("-1");
	x("(((0)))");
	x("1 + 2 + 4 * 7 / 1");
	x("(1.0 - 2.1) / +4.5");
	x("- - - - - -1234");

	x_err("", tokens.begin());
	x_err("a + 3", tokens.begin() + 1);
	//       ^ tokens.begin() + 1
	x_err("3 - * 4 a", tokens.begin() + 4);
	//             ^ tokens.begin() + 3

#undef x
#undef x_err
}

static void parse_typespec_test(void)
{
	ctx::global_context global_ctx;
	ctx::lex_context lex_ctx(global_ctx);
	ctx::parse_context parse_ctx(global_ctx);

#define x(str, it_pos, kind_) xx_parse(parse_typespec, str, it_pos, res.kind() == kind_)
#define x_err(str, it_pos, kind_) xx_parse_err(parse_typespec, str, it_pos, res.kind() == kind_)

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
	test(resolve_literal_test);
	test(parse_primary_expression_test);
	test(parse_expression_comma_list_test);
	test(parse_expression_test);
	test(parse_typespec_test);

	test_end();
}
