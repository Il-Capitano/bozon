#include "parser.cpp"
#include "test.h"

#include "ctx/global_context.h"
#include "ctx/parse_context.h"

#define xx(fn, str, it_pos)                                \
do {                                                       \
    bz::string_view const file = str;                      \
    auto const tokens = lex::get_tokens(file, "", errors); \
    assert_true(errors.empty());                           \
    auto it = tokens.begin();                              \
    fn(it, tokens.end(), context);                         \
    assert_false(global_ctx.has_errors());                 \
    assert_eq(it, it_pos);                                 \
} while (false)

#define xx_err(fn, str, it_pos)                            \
do {                                                       \
    bz::string_view const file = str;                      \
    auto const tokens = lex::get_tokens(file, "", errors); \
    assert_true(errors.empty());                           \
    auto it = tokens.begin();                              \
    fn(it, tokens.end(), context);                         \
    assert_true(global_ctx.has_errors());                  \
    errors.clear();                                        \
    assert_eq(it, it_pos);                                 \
} while (false)

static void parse_primary_expression_test(void)
{
	ctx::global_context global_ctx;
	ctx::parse_context context("", global_ctx);
	bz::vector<ctx::error> errors = {};

#define x(str) xx(parse_primary_expression, str, tokens.end() - 1)
#define x_err(str, it_pos) xx_err(parse_primary_expression, str, it_pos)

	x("1");
	x("+3");

#undef x
#undef x_err
}

test_result parser_test(void)
{
	test_begin();

	test(parse_primary_expression_test);

	test_end();
}
