#include "parser.cpp"
#include "test.h"

#include "lex/lexer.h"
#include "ctx/global_context.h"
#include "ctx/lex_context.h"
#include "ctx/parse_context.h"

#define xx_global(fn, str, it_pos)                          \
do {                                                        \
    bz::string_view const file = str;                       \
    auto const tokens = lex::get_tokens(file, "", lex_ctx); \
    assert_false(global_ctx.has_errors());                  \
    auto it = tokens.begin();                               \
    fn(it, tokens.end(), parse_ctx);                        \
    assert_false(global_ctx.has_errors());                  \
    assert_eq(it, it_pos);                                  \
} while (false)

#define xx_gloabal_err(fn, str, it_pos)                     \
do {                                                        \
    bz::string_view const file = str;                       \
    auto const tokens = lex::get_tokens(file, "", lex_ctx); \
    assert_false(global_ctx.has_errors());                  \
    auto it = tokens.begin();                               \
    fn(it, tokens.end(), parse_ctx);                        \
    assert_true(global_ctx.has_errors());                   \
    global_ctx.clear_errors();                              \
    assert_eq(it, it_pos);                                  \
} while (false)

#define xx_parse(fn, str, it_pos)                           \
do {                                                        \
    bz::string_view const file = str;                       \
    auto const tokens = lex::get_tokens(file, "", lex_ctx); \
    assert_false(global_ctx.has_errors());                  \
    auto it = tokens.begin();                               \
    fn(it, tokens.end(), parse_ctx);                        \
    assert_false(global_ctx.has_errors());                  \
    assert_eq(it, it_pos);                                  \
} while (false)

#define xx_parse_err(fn, str, it_pos)                       \
do {                                                        \
    bz::string_view const file = str;                       \
    auto const tokens = lex::get_tokens(file, "", lex_ctx); \
    assert_false(global_ctx.has_errors());                  \
    auto it = tokens.begin();                               \
    fn(it, tokens.end(), parse_ctx);                        \
    assert_true(global_ctx.has_errors());                   \
    global_ctx.clear_errors();                              \
    assert_eq(it, it_pos);                                  \
} while (false)

static void get_paren_matched_range_test(void)
{
	ctx::global_context global_ctx;
	ctx::lex_context lex_ctx(global_ctx);

#define x(str, it_pos)                                      \
do {                                                        \
    bz::string_view const file = str;                       \
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

static std::ostream &operator << (std::ostream &os, ast::type_info::type_kind kind)
{
	os << static_cast<uint32_t>(kind);
	return os;
}

static void resolve_literal_test(void)
{
	ctx::global_context global_ctx;
	ctx::lex_context lex_ctx(global_ctx);
	ctx::parse_context parse_ctx("", global_ctx);

#define x(str, kind_)                                                     \
do {                                                                      \
    bz::string_view const file = str;                                     \
    auto const tokens = lex::get_tokens(file, "", lex_ctx);               \
    assert_false(global_ctx.has_errors());                                \
    auto it = tokens.begin();                                             \
    auto expr = parse_expression(                                         \
        it, tokens.end(),                                                 \
        parse_ctx, precedence{}                                           \
    );                                                                    \
    assert_eq(it, tokens.end() - 1);                                      \
    resolve_literal(expr, parse_ctx);                                     \
    assert_eq(                                                            \
        ast::type_info::type_kind::kind_,                                 \
        expr.expr_type.expr_type.get<ast::ts_base_type_ptr>()->info->kind \
    );                                                                    \
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
	ctx::parse_context parse_ctx("", global_ctx);

#define x(str) xx_parse(parse_primary_expression, str, tokens.end() - 1)
#define x_err(str, it_pos) xx_parse_err(parse_primary_expression, str, it_pos)

	x("1");
	x("+3");
	x("!!!!!!true");
	x("(0)");
	x("((((!false))))");
	x("+ + - - 0");
	x("sizeof 0");

	x_err("++3", tokens.begin() + 2);
	x_err("&0", tokens.begin() + 2);
	x_err("a", tokens.begin() + 1);

#undef x
#undef x_err
}

static void parse_expression_test(void)
{
	ctx::global_context global_ctx;
	ctx::lex_context lex_ctx(global_ctx);
	ctx::parse_context parse_ctx("", global_ctx);

#define x(str) \
do {                                                             \
    bz::string_view const file = str;                            \
    auto const tokens = lex::get_tokens(file, "", lex_ctx);      \
    assert_false(global_ctx.has_errors());                       \
    auto it = tokens.begin();                                    \
    parse_expression(it, tokens.end(), parse_ctx, precedence{}); \
    assert_false(global_ctx.has_errors());                       \
    assert_eq(it, tokens.end() - 1);                             \
} while (false)

	x("-1");

#undef x
}

test_result parser_test(void)
{
	test_begin();

	test(get_paren_matched_range_test);
	test(resolve_literal_test);
	test(parse_primary_expression_test);
	test(parse_expression_test);

	test_end();
}
