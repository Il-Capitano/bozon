#include "first_pass_parser.cpp"
#include "test.h"

void get_expression_or_type_test(void)
{
	bz::vector<error> errors = {};

#define x(str, it_pos)                                \
do {                                                  \
    bz::string_view const file = str;                 \
    auto const tokens = get_tokens(file, "", errors); \
    assert_true(errors.empty());                      \
    auto it = tokens.begin();                         \
    get_expression_or_type<token::semi_colon>(        \
        it, tokens.end(), errors                      \
    );                                                \
    assert_true(errors.empty());                      \
    assert_eq(it, it_pos);                            \
} while (false)

#define x_err(str, it_pos)                            \
do {                                                  \
    bz::string_view const file = str;                 \
    auto const tokens = get_tokens(file, "", errors); \
    assert_true(errors.empty());                      \
    auto it = tokens.begin();                         \
    get_expression_or_type<token::semi_colon>(        \
        it, tokens.end(), errors                      \
    );                                                \
    assert_false(errors.empty());                     \
    assert_eq(it, it_pos);                            \
    errors.clear();                                   \
} while (false)

	x("x+3;", tokens.begin() + 3);
	x("(asdf;++--);", tokens.begin() + 6);
	x("() => { std::print(\"hello\"); };", tokens.end() - 1);
	x_err("(", tokens.end()); // why +2 ???

#undef x
}

test_result first_pass_parser_test(void)
{
	test_begin();

	test(get_expression_or_type_test);

	test_end();
}