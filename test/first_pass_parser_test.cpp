#include "first_pass_parser.cpp"
#include "test.h"

void get_tokens_in_curly_test(void)
{
	bz::vector<error> errors = {};

#define x(str, it_pos)                                \
do {                                                  \
    bz::string_view const file = str;                 \
    auto const tokens = get_tokens(file, "", errors); \
    assert_true(errors.empty());                      \
    auto it = tokens.begin();                         \
    get_tokens_in_curly<token::semi_colon>(           \
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
    get_tokens_in_curly<token::semi_colon>(           \
        it, tokens.end(), errors                      \
    );                                                \
    assert_false(errors.empty());                     \
    errors.clear();                                   \
    assert_eq(it, it_pos);                            \
} while (false)

	x("", tokens.begin());
	x("x + 3;", tokens.begin() + 3);
	//      ^ tokens.begin() + 3
	x("(asdf++--);", tokens.begin() + 5);
	//           ^ tokens.begin() + 6
	x("() => { std::print(\"hello\"); };", tokens.begin() + 12);
	//                                 ^ tokens.begin() + 12
	x("((((;);)));", tokens.begin() + 10);
	//           ^ tokens.begin() + 10
	x("[0, 1, 2];", tokens.begin() + 7);
	//          ^ tokens.begin() + 7
	x("&const int32;", tokens.begin() + 3);
	//             ^ tokens.begin() + 3

	x_err("{ a + b; return 3;", tokens.begin() + 8);
	//                       ^ tokens.begin() + 8

#undef x
#undef x_err
}

void get_expression_or_type_tokens_test(void)
{
	bz::vector<error> errors = {};

#define x(str, it_pos)                                \
do {                                                  \
    bz::string_view const file = str;                 \
    auto const tokens = get_tokens(file, "", errors); \
    assert_true(errors.empty());                      \
    auto it = tokens.begin();                         \
    get_expression_or_type_tokens<token::semi_colon>( \
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
    get_expression_or_type_tokens<token::semi_colon>( \
        it, tokens.end(), errors                      \
    );                                                \
    assert_false(errors.empty());                     \
    errors.clear();                                   \
    assert_eq(it, it_pos);                            \
} while (false)

	x("x + 3;", tokens.begin() + 3);
	//      ^ tokens.begin() + 3
	x("(asdf++--);", tokens.begin() + 5);
	//           ^ tokens.begin() + 6
	x("() => { std::print(\"hello\"); };", tokens.begin() + 12);
	//                                 ^ tokens.begin() + 12
	x("(((())));", tokens.begin() + 8);
	//         ^ tokens.begin() + 8
	x("[0, 1, 2];", tokens.begin() + 7);
	//          ^ tokens.begin() + 7
	x("&const int32;", tokens.begin() + 3);
	//             ^ tokens.begin() + 3

	x_err("(", tokens.begin() + 1);
	x_err("[1, 2, [asdf, x];", tokens.begin() + 10);
	//                     ^ tokens.begin() + 10
	x_err("[(1, 2, 3 + 4];", tokens.begin() + 10);
	//                   ^ tokens.begin() + 10

#undef x
#undef x_err
}

#define xx(fn, str, it_pos)                           \
do {                                                  \
    bz::string_view const file = str;                 \
    auto const tokens = get_tokens(file, "", errors); \
    assert_true(errors.empty());                      \
    auto it = tokens.begin();                         \
    fn(it, tokens.end(), errors);                     \
    assert_true(errors.empty());                      \
    assert_eq(it, it_pos);                            \
} while (false)

#define xx_err(fn, str, it_pos)                       \
do {                                                  \
    bz::string_view const file = str;                 \
    auto const tokens = get_tokens(file, "", errors); \
    assert_true(errors.empty());                      \
    auto it = tokens.begin();                         \
    fn(it, tokens.end(), errors);                     \
    assert_false(errors.empty());                     \
    errors.clear();                                   \
    assert_eq(it, it_pos);                            \
} while (false)

void get_function_params_test(void)
{
	bz::vector<error> errors = {};

#define x(str, it_pos) xx(get_function_params, str, it_pos)
#define x_err(str, it_pos) xx_err(get_function_params, str, it_pos)

	x("()", tokens.end() - 1);
	x("(a: int32, b: [int32, [[]], std::string])", tokens.end() - 1);
	x("(: [..]float64)", tokens.end() - 1);

	x_err("(", tokens.end() - 1);
	x_err("(: [[], a: int32) { return 3; }", tokens.begin() + 10);
	//                       ^ tokens.begin() + 10
	x_err("( -> int32 { return 0; }", tokens.begin() + 8);
	//                             ^ tokens.begin() + 8 (eof)
	x_err("(: [[], a: int32)", tokens.end() - 1);

#undef x
#undef x_err
}

void get_stmt_compound_test(void)
{
	bz::vector<error> errors = {};

#define x(str, it_pos) xx(get_stmt_compound, str, it_pos)
#define x_err(str, it_pos) xx_err(get_stmt_compound, str, it_pos)

	x("{}", tokens.begin() + 2);
	//   ^ tokens.begin() + 2 (eof)
	x("{ return 0; if (asdf) return 1; else return 2; } else {}", tokens.begin() + 16);
	//                                                  ^ tokens.begin() + 16

	x_err("{", tokens.begin() + 1);
	x_err("{ a + b; return 0; function hello() { return 2; }", tokens.begin() + 17);
	//                                                      ^ tokens.begin() + 17 (eof)

#undef x
#undef x_err
}

void get_stmt_compound_ptr_test(void)
{
	bz::vector<error> errors = {};

#define x(str, it_pos) xx(get_stmt_compound_ptr, str, it_pos)
#define x_err(str, it_pos) xx_err(get_stmt_compound_ptr, str, it_pos)

	x("{}", tokens.begin() + 2);
	//   ^ tokens.begin() + 2 (eof)
	x("{ return 0; if (asdf) return 1; else return 2; } else {}", tokens.begin() + 16);
	//                                                  ^ tokens.begin() + 16

	x_err("{", tokens.begin() + 1);
	x_err("{ a + b; return 0; function hello() { return 2; }", tokens.begin() + 17);
	//                                                      ^ tokens.begin() + 17 (eof)

#undef x
#undef x_err
}

void parse_if_statement_test(void)
{
	bz::vector<error> errors = {};

#define x(str, it_pos) xx(parse_if_statement, str, it_pos)
#define x_err(str, it_pos) xx_err(parse_if_statement, str, it_pos)

	x("if (true) { return; }", tokens.begin() + 8);
	//                      ^ tokens.begin() + 8 (eof)
	x("if (false) x = 3; else x = 4;", tokens.begin() + 13);
	//                              ^ tokens.begin() + 13 (eof)
	x("if (true) {} else {} return 0;", tokens.begin() + 9);
	//                      ^ tokens.begin() + 9

	x_err("if true x = 3;", tokens.begin() + 6);
	//                   ^ tokens.begin() + 6
	x_err("if; return 0;", tokens.begin() + 2);
	//         ^ tokens.begin() + 2
	x_err("if (1 + 2 == 3 return 3;", tokens.begin() + 10);
	//                             ^ tokens.begin() + 10

#undef x
#undef x_err
}

void parse_while_statement_test(void)
{
	bz::vector<error> errors = {};

#define x(str, it_pos) xx(parse_while_statement, str, it_pos)
#define x_err(str, it_pos) xx_err(parse_while_statement, str, it_pos)

	x("while (true) { return; }", tokens.begin() + 8);
	//                         ^ tokens.begin() + 8 (eof)
	x("while (false) x = 3;", tokens.begin() + 8);
	//                     ^ tokens.begin() + 8 (eof)
	x("while (true) {} return 0;", tokens.begin() + 6);
	//                 ^ tokens.begin() + 6
	x("while (++stream->kind != token::eof); return 2;", tokens.begin() + 12);
	//                                       ^ tokens.begin() + 12

	x_err("while true x = 3;", tokens.begin() + 6);
	//                      ^ tokens.begin() + 6
	x_err("while; return 0;", tokens.begin() + 2);
	//          ^ tokens.begin() + 2
	x_err("while (1 + 2 == 3 return 3;", tokens.begin() + 10);
	//                             ^ tokens.begin() + 10

#undef x
#undef x_err
}

void parse_for_statement_test(void)
{
	throw test_fail_exception("");
}

void parse_return_statement_test(void)
{
	bz::vector<error> errors = {};

#define x(str, it_pos) xx(parse_return_statement, str, it_pos)
#define x_err(str, it_pos) xx_err(parse_return_statement, str, it_pos)

	x("return 3; }", tokens.begin() + 3);
	//           ^ tokens.begin() + 3
	x("return a + b - c; }", tokens.begin() + 7);
	//                   ^ tokens.begin() + 7
	x("return () => { return 3; }; }", tokens.begin() + 10);
	//                             ^ tokens.begin() + 10

	x_err("return 3 }", tokens.begin() + 2);
	//              ^ tokens.begin() + 2
	x_err("return [a, b, [c]; }", tokens.begin() + 10);
	//                        ^ tokens.begin() + 10

#undef x
#undef x_err
}

void parse_no_op_statement_test(void)
{
	bz::vector<error> errors = {};

#define x(str, it_pos) xx(parse_no_op_statement, str, it_pos)
#define x_err(str, it_pos) xx_err(parse_no_op_statement, str, it_pos)

	x("; a", tokens.begin() + 1);

	// parse_no_op_statement cannot give errors

#undef x
#undef x_err
}

void parse_expression_statement_test(void)
{
	bz::vector<error> errors = {};

#define x(str, it_pos) xx(parse_expression_statement, str, it_pos)
#define x_err(str, it_pos) xx_err(parse_expression_statement, str, it_pos)

	x("x = y + 3; a", tokens.begin() + 6);
	//            ^ tokens.begin() + 6
	x("[a, b] = [b, a]; a", tokens.begin() + 12);
	//                  ^ tokens.begin() + 12

	// this is just a first pass parse, so this is legal
	x("x = y a + 3; a", tokens.begin() + 7);
	//              ^ tokens.begin() + 7
	x("1 + 3 3; a", tokens.begin() + 5);
	//          ^ tokens.begin() + 5

	x_err("y = x }", tokens.begin() + 3);
	//           ^ tokens.begin() + 3
	x_err("y = x return y; }", tokens.begin() + 3);
	//           ^ tokens.begin() + 3
	x_err("[a, b] = [b, a; a", tokens.begin() + 11);
	//                     ^ tokens.begin() + 11
	x_err("[a, b] = b, a]; a", tokens.begin() + 11);
	//                     ^ tokens.begin() + 11
	x_err("[(1, 2, 3 + 4]; [a, b] = [c, d];", tokens.begin() + 11);
	//                     ^ tokens.begin() + 11

#undef x
#undef x_err
}

test_result first_pass_parser_test(void)
{
	test_begin();

	test(get_tokens_in_curly_test);
	test(get_expression_or_type_tokens_test);
	test(get_function_params_test);
	test(get_stmt_compound_test);
	test(get_stmt_compound_ptr_test);
	test(parse_if_statement_test);
	test(parse_while_statement_test);
//	test(parse_for_statement_test);
	test(parse_return_statement_test);
	test(parse_no_op_statement_test);
	test(parse_expression_statement_test);

	test_end();
}
