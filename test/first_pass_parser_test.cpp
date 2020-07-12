#include "first_pass_parser.cpp"
#include "test.h"

#include "lex/lexer.h"
#include "ctx/global_context.h"
#include "ctx/first_pass_parse_context.h"

static void get_tokens_in_curly_test(void)
{
	ctx::global_context global_ctx;
	ctx::lex_context lex_ctx(global_ctx);
	ctx::first_pass_parse_context context(global_ctx);

#define x(str, it_pos)                                      \
do {                                                        \
    bz::u8string_view const file = str;                     \
    auto const tokens = lex::get_tokens(file, "", lex_ctx); \
    assert_false(global_ctx.has_errors());                  \
    auto it = tokens.begin();                               \
    ++it;                                                   \
    get_tokens_in_curly<lex::token::curly_close>(           \
        it, tokens.end(), context                           \
    );                                                      \
    assert_false(global_ctx.has_errors());                  \
    assert_eq(it, it_pos);                                  \
} while (false)

#define x_err(str, it_pos)                                  \
do {                                                        \
    bz::u8string_view const file = str;                     \
    auto const tokens = lex::get_tokens(file, "", lex_ctx); \
    assert_false(global_ctx.has_errors());                  \
    auto it = tokens.begin();                               \
    ++it;                                                   \
    get_tokens_in_curly<lex::token::curly_close>(           \
        it, tokens.end(), context                           \
    );                                                      \
    assert_true(global_ctx.has_errors());                   \
    global_ctx.clear_errors_and_warnings();                 \
    assert_eq(it, it_pos);                                  \
} while (false)

	x("{}", tokens.end() - 1);
	x("{ x += 3; }", tokens.end() - 1);
	x("{ i can write anything here... as long as it's' tokenizable. ;; +-+3++-- }", tokens.end() - 1);
	x("{ { } }", tokens.end() - 1);
	x("{ { } { } }", tokens.end() - 1);
	x("{ =>=>=> }", tokens.end() - 1);
	x("{ ([[(]) }", tokens.end() - 1);

	x_err("{", tokens.end() - 1);
	x_err("{{   }", tokens.end() - 1);
	x_err("{{{", tokens.end() - 1);

//	x("", tokens.end() - 1);
//	x("", tokens.end() - 1);
//	x("", tokens.end() - 1);
//	x("", tokens.end() - 1);

/*
	x("{ }", tokens.begin() + 2);
	x("{x + 3;", tokens.begin() + 3);
	//      ^ tokens.begin() + 3
	x("{(asdf++--);", tokens.begin() + 5);
	//           ^ tokens.begin() + 6
	x("{() => { std::print(\"hello\"); };", tokens.begin() + 12);
	//                                 ^ tokens.begin() + 12
	x("{((((;);)));", tokens.begin() + 10);
	//           ^ tokens.begin() + 10
	x("{[0, 1, 2];", tokens.begin() + 7);
	//          ^ tokens.begin() + 7
	x("{&const int32;", tokens.begin() + 3);
	//             ^ tokens.begin() + 3

	x_err("{ a + b; return 3;", tokens.begin() + 8);
	//                       ^ tokens.begin() + 8 (eof)
*/

#undef x
#undef x_err
}

static void get_expression_or_type_tokens_test(void)
{
	ctx::global_context global_ctx;
	ctx::lex_context lex_ctx(global_ctx);
	ctx::first_pass_parse_context context(global_ctx);

#define x(str, it_pos)                                      \
do {                                                        \
    bz::u8string_view const file = str;                     \
    auto const tokens = lex::get_tokens(file, "", lex_ctx); \
    assert_false(global_ctx.has_errors());                  \
    auto it = tokens.begin();                               \
    get_expression_or_type_tokens<lex::token::semi_colon>(  \
        it, tokens.end(), context                           \
    );                                                      \
    assert_false(global_ctx.has_errors());                  \
    assert_eq(it, it_pos);                                  \
} while (false)

#define x_err(str, it_pos)                                  \
do {                                                        \
    bz::u8string_view const file = str;                     \
    auto const tokens = lex::get_tokens(file, "", lex_ctx); \
    assert_false(global_ctx.has_errors());                  \
    auto it = tokens.begin();                               \
    get_expression_or_type_tokens<lex::token::semi_colon>(  \
        it, tokens.end(), context                           \
    );                                                      \
    assert_true(global_ctx.has_errors());                   \
    global_ctx.clear_errors_and_warnings();                 \
    assert_eq(it, it_pos);                                  \
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
	x("stream->kind == token::eof;", tokens.begin() + 7);
	//                           ^ tokens.begin() + 7

	x_err("(", tokens.begin() + 1);
	x_err("[1, 2, [asdf, x];", tokens.begin() + 10);
	//                     ^ tokens.begin() + 10
	x_err("[(1, 2, 3 + 4];", tokens.begin() + 10);
	//                   ^ tokens.begin() + 10

#undef x
#undef x_err
}

#define xx(fn, str, it_pos)                                 \
do {                                                        \
    bz::u8string_view const file = str;                     \
    auto const tokens = lex::get_tokens(file, "", lex_ctx); \
    assert_false(global_ctx.has_errors());                  \
    auto it = tokens.begin();                               \
    fn(it, tokens.end(), context);                          \
    assert_false(global_ctx.has_errors());                  \
    assert_eq(it, it_pos);                                  \
} while (false)

#define xx_err(fn, str, it_pos)                             \
do {                                                        \
    bz::u8string_view const file = str;                     \
    auto const tokens = lex::get_tokens(file, "", lex_ctx); \
    assert_false(global_ctx.has_errors());                  \
    auto it = tokens.begin();                               \
    fn(it, tokens.end(), context);                          \
    assert_true(global_ctx.has_errors());                   \
    global_ctx.clear_errors_and_warnings();                 \
    assert_eq(it, it_pos);                                  \
} while (false)

static void get_function_params_test(void)
{
	ctx::global_context global_ctx;
	ctx::lex_context lex_ctx(global_ctx);
	ctx::first_pass_parse_context context(global_ctx);

#define x(str, it_pos) xx(get_function_params, str, it_pos)
#define x_err(str, it_pos) xx_err(get_function_params, str, it_pos)

	x("()", tokens.end() - 1);
	x("(a: int32, b: [int32, [[]], std::string])", tokens.end() - 1);
	x("(: [..]float64)", tokens.end() - 1);
	x("(: functor, : int32, : int32)", tokens.end() - 1);

	x_err("(", tokens.end() - 1);
	x_err("(: [[], a: int32) { return 3; }", tokens.begin() + 10);
	//                       ^ tokens.begin() + 10
	x_err("( -> int32 { return 0; }", tokens.begin() + 3);
	//                ^ tokens.begin() + 3
	x_err("(: [[], a: int32)", tokens.end() - 1);

#undef x
#undef x_err
}

/*
static void get_stmt_compound_test(void)
{
	ctx::global_context global_ctx;
	ctx::lex_context lex_ctx(global_ctx);
	ctx::first_pass_parse_context context(global_ctx);

#define x(str, it_pos) xx(get_stmt_compound, str, it_pos)
#define x_err(str, it_pos) xx_err(get_stmt_compound, str, it_pos)

	x("{}", tokens.begin() + 2);
	//   ^ tokens.begin() + 2 (eof)
	x("{ return 0; if (asdf) return 1; else return 2; } else {}", tokens.begin() + 16);
	//                                                  ^ tokens.begin() + 16

	x_err("{ (a, b; } else {}", tokens.begin() + 7);
	//            ^ tokens.begin() + 7
	x_err("{", tokens.begin() + 1);
	x_err("{ a + b; return 0; function hello() { return 2; }", tokens.begin() + 17);
	//                                                      ^ tokens.begin() + 17 (eof)

#undef x
#undef x_err
}
*/

static void get_stmt_compound_ptr_test(void)
{
	ctx::global_context global_ctx;
	ctx::lex_context lex_ctx(global_ctx);
	ctx::first_pass_parse_context context(global_ctx);

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

static void parse_if_statement_test(void)
{
	ctx::global_context global_ctx;
	ctx::lex_context lex_ctx(global_ctx);
	ctx::first_pass_parse_context context(global_ctx);

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

static void parse_while_statement_test(void)
{
	ctx::global_context global_ctx;
	ctx::lex_context lex_ctx(global_ctx);
	ctx::first_pass_parse_context context(global_ctx);

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

// static void parse_for_statement_test(void)
// {
// 	throw test_fail_exception("");
// }

static void parse_return_statement_test(void)
{
	ctx::global_context global_ctx;
	ctx::lex_context lex_ctx(global_ctx);
	ctx::first_pass_parse_context context(global_ctx);

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

static void parse_no_op_statement_test(void)
{
	ctx::global_context global_ctx;
	ctx::lex_context lex_ctx(global_ctx);
	ctx::first_pass_parse_context context(global_ctx);

#define x(str, it_pos) xx(parse_no_op_statement, str, it_pos)
#define x_err(str, it_pos) xx_err(parse_no_op_statement, str, it_pos)

	x("; a", tokens.begin() + 1);

	// parse_no_op_statement cannot give errors

#undef x
#undef x_err
}

static void parse_expression_statement_test(void)
{
	ctx::global_context global_ctx;
	ctx::lex_context lex_ctx(global_ctx);
	ctx::first_pass_parse_context context(global_ctx);

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
	x("() => { std::print(\"hello\"); return 0; }; a", tokens.begin() + 16);
	//                                             ^ tokens.begin() + 16

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
	x_err("else return 3; }", tokens.begin() + 4);
	//                    ^ tokens.begin() + 4

#undef x
#undef x_err
}

static void parse_variable_declaration_test(void)
{
	ctx::global_context global_ctx;
	ctx::lex_context lex_ctx(global_ctx);
	ctx::first_pass_parse_context context(global_ctx);

#define x(str, it_pos) xx(parse_variable_declaration, str, it_pos)
#define x_err(str, it_pos) xx_err(parse_variable_declaration, str, it_pos)

	x("let a: int32; a", tokens.begin() + 5);
	//               ^ tokens.begin() + 5
	x("let a = 0; a", tokens.begin() + 5);
	//            ^ tokens.begin() + 5
	x("let a: int32 = 0; a", tokens.begin() + 7);
	//                   ^ tokens.begin() + 7
	x("let &a: int32 = b; a", tokens.begin() + 8);
	//                    ^ tokens.begin() + 8
	x("let const a = 0; a", tokens.begin() + 6);
	//                  ^ tokens.begin() + 6
	x("const a = 0; a", tokens.begin() + 5);
	//              ^ tokens.begin() + 5
	x("let &const **const *const a = 0; a", tokens.begin() + 12);
	//                                  ^ tokens.begin() + 12
	x("let a = 0.0; a", tokens.begin() + 5);
	//              ^ tokens.begin() + 5

	x_err("let a: [int32, float64 = [0, 1.3]; a", tokens.begin() + 14);
	//                                        ^ tokens.begin() + 14
	x_err("const &a: int32 = b; a", tokens.begin() + 8);
	//                          ^ tokens.begin() + 8
	x_err("let &const **&const *const a = 0; a", tokens.begin() + 13);
	//                                       ^ tokens.begin() + 13
	x_err("const const a = 0; a", tokens.begin() + 6);
	//                        ^ tokens.begin() + 6

#undef x
#undef x_err
}

static void parse_struct_definition_test(void)
{
	ctx::global_context global_ctx;
	ctx::lex_context lex_ctx(global_ctx);
	ctx::first_pass_parse_context context(global_ctx);

#define x(str, it_pos) xx(parse_struct_definition, str, it_pos)
#define x_err(str, it_pos) xx_err(parse_struct_definition, str, it_pos)

	x("struct vec2d { x: float64; y: float64; } a", tokens.begin() + 12);
	//                                          ^ tokens.begin() + 12
	x("struct foo {} a", tokens.begin() + 4);
	//               ^ tokens.begin() + 4

	x_err("struct foo { x: float64; y: float64 } a", tokens.begin() + 11);
	//                                           ^ tokens.begin() + 11
	x_err("struct foo { a int32; } a", tokens.begin() + 7);
	//                             ^ tokens.begin() + 7
	x_err("struct { a: int32; } a", tokens.begin() + 7);
	//                          ^ tokens.begin() + 7
	x_err("struct foo a: int32; } a", tokens.begin() + 7);
	//                            ^ tokens.begin() + 7
	x_err("struct bz::formatter<ast::expression> {} a", tokens.begin() + 12);
	//                                               ^ tokens.begin() + 12 (eof)

#undef x
#undef x_err
}

static void parse_function_definition_test(void)
{
	ctx::global_context global_ctx;
	ctx::lex_context lex_ctx(global_ctx);
	ctx::first_pass_parse_context context(global_ctx);

#define x(str, it_pos) xx(parse_function_definition, str, it_pos)
#define x_err(str, it_pos) xx_err(parse_function_definition, str, it_pos)

	x("function main() -> int32 { return 0; } a", tokens.begin() + 11);
	//                                        ^ tokens.begin() + 11
	x("function factorial(n: int32) -> int32"
	  "{ if (n <= 1) return 1; return n * factorial(n - 1); } a", tokens.begin() + 30);
	//                                                        ^ tokens.begin() + 30
	x("function foo() -> { return 0; } a", tokens.begin() + 10);
	//                                 ^ tokens.begin() + 10

	x_err("function foo() { return 0; } a", tokens.begin() + 9);
	//                                  ^ tokens.begin() + 9
	x_err("function () -> int32 { return 0; } a", tokens.begin() + 10);
	//                                        ^ tokens.begin() + 10
	x_err("function foo() -> int32 { return 0;", tokens.begin() + 10);
	//                                        ^ tokens.begin() + 10 (eof)
	x_err("function foo() -> int32 return 0; } a", tokens.begin() + 6);
	//                             ^ tokens.begin() + 6

#undef x
#undef x_err
}

static void parse_operator_definition_test(void)
{
	ctx::global_context global_ctx;
	ctx::lex_context lex_ctx(global_ctx);
	ctx::first_pass_parse_context context(global_ctx);

#define x(str, it_pos) xx(parse_operator_definition, str, it_pos)
#define x_err(str, it_pos) xx_err(parse_operator_definition, str, it_pos)

	x("operator + (a: int32) -> int32 { return a; } a", tokens.begin() + 14);
	//                                              ^ tokens.begin() + 14
	x("operator .. (a: std::string, b: &const std::string) -> std::string"
	  "{ if (a.empty()) return b; else return a.append(b); } a", tokens.begin() + 43);
	//                                                       ^ tokens.begin() + 43
	x("operator << (: int32, : float32) -> void {} a", tokens.begin() + 13);
	//                                             ^ tokens.begin() + 13
	x("operator () (: functor, : int32, : int32) -> int32 { return 0; } a", tokens.begin() + 20);
	//                                                                  ^ tokens.begin() + 20

	x_err("operator foo() { return 0; } a", tokens.begin() + 9);
	//                                  ^ tokens.begin() + 9
	x_err("operator () -> int32 { return 0; } a", tokens.begin() + 10);
	//                                        ^ tokens.begin() + 10
	x_err("operator foo() -> int32 { return 0;", tokens.begin() + 10);
	//                                        ^ tokens.begin() + 10 (eof)
	x_err("operator foo() -> int32 return 0; } a", tokens.begin() + 6);
	//                             ^ tokens.begin() + 6

#undef x
#undef x_err
}

static void parse_declaration_test(void)
{
	ctx::global_context global_ctx;
	ctx::lex_context lex_ctx(global_ctx);
	ctx::first_pass_parse_context context(global_ctx);

#define x(str, decl_type)                                       \
do {                                                            \
    bz::u8string_view const file = str;                         \
    auto const tokens = lex::get_tokens(file, "", lex_ctx);     \
    assert_false(global_ctx.has_errors());                      \
    auto it = tokens.begin();                                   \
    auto const decl = parse_declaration(                        \
        it, tokens.end(), context                               \
    );                                                          \
    assert_false(global_ctx.has_errors());                      \
    assert_eq(decl.kind(), ast::declaration::index<decl_type>); \
    assert_eq(it, tokens.end() - 1);                            \
} while (false)

	x("let a: int32;", ast::decl_variable);
	x("const *a = &b;", ast::decl_variable);
	x("struct foo {}", ast::decl_struct);
	x("function foo() -> void {}", ast::decl_function);
	x("operator + (: int32, : int32) -> void {}", ast::decl_operator);

#undef x
}

static void parse_statement_test(void)
{
	ctx::global_context global_ctx;
	ctx::lex_context lex_ctx(global_ctx);
	ctx::first_pass_parse_context context(global_ctx);

#define x(str, decl_type)                                     \
do {                                                          \
    bz::u8string_view const file = str;                       \
    auto const tokens = lex::get_tokens(file, "", lex_ctx);   \
    assert_false(global_ctx.has_errors());                    \
    auto it = tokens.begin();                                 \
    auto const stmt = parse_statement(                        \
        it, tokens.end(), context                             \
    );                                                        \
    assert_false(global_ctx.has_errors());                    \
    assert_eq(stmt.kind(), ast::statement::index<decl_type>); \
    assert_eq(it, tokens.end() - 1);                          \
} while (false)

	x("if (a == b) {}", ast::stmt_if);
	x("if (a == b) {} else {}", ast::stmt_if);
	x("while (it != end) { ++it; }", ast::stmt_while);
	x("return a + b / 2;", ast::stmt_return);
	x(";", ast::stmt_no_op);
	x("{ let b = 0; }", ast::stmt_compound);
	x("let a: int32;", ast::decl_variable);
	x("const *a = &b;", ast::decl_variable);
	x("struct foo {}", ast::decl_struct);
	x("function foo() -> void {}", ast::decl_function);
	x("operator + (: int32, : int32) -> void {}", ast::decl_operator);
	x("a = b / 2;", ast::stmt_expression);

#undef x
}

test_result first_pass_parser_test(void)
{
	test_begin();

	test_fn(get_tokens_in_curly_test);
	test_fn(get_expression_or_type_tokens_test);
	test_fn(get_function_params_test);
//	test_fn(get_stmt_compound_test);
	test_fn(get_stmt_compound_ptr_test);
	test_fn(parse_if_statement_test);
	test_fn(parse_while_statement_test);
//	test_fn(parse_for_statement_test);
	test_fn(parse_return_statement_test);
	test_fn(parse_no_op_statement_test);
	test_fn(parse_expression_statement_test);
	test_fn(parse_variable_declaration_test);
//	test_fn(parse_struct_definition_test);
	test_fn(parse_function_definition_test);
	test_fn(parse_operator_definition_test);
	test_fn(parse_declaration_test);
	test_fn(parse_statement_test);

	test_end();
}
