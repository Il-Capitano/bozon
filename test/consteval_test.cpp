#include "resolve/consteval.cpp"

#include "test.h"
#include "ctx/global_context.h"
#include "ctx/lex_context.h"
#include "ctx/parse_context.h"
#include "parse/expression_parser.h"
#include "resolve/expression_resolver.h"
#include "resolve/match_expression.h"

using namespace parse;
using namespace resolve;

static bz::optional<bz::u8string> consteval_guaranteed_test(ctx::global_context &global_ctx)
{
	ctx::lex_context lex_ctx(global_ctx);
	ctx::parse_context parse_ctx(global_ctx);
	parse_ctx.current_global_scope = global_ctx._builtin_global_scope;

#define x(str, kind, res_value)                                                 \
do {                                                                            \
    bz::u8string_view const file = str;                                         \
    auto const tokens = lex::get_tokens(file, 0, lex_ctx);                      \
    assert_false(global_ctx.has_errors_or_warnings());                          \
    auto it = tokens.begin();                                                   \
    auto res = parse_expression(it, tokens.end() - 1, parse_ctx, precedence{}); \
    resolve_expression(res, parse_ctx);                                         \
    consteval_guaranteed(res, parse_ctx);                                       \
    assert_eq(it, tokens.end() - 1);                                            \
    assert_false(res.is_error());                                               \
    assert_true(res.is<ast::constant_expression>());                            \
    ast::constant_value value;                                                  \
    value.emplace<kind>(res_value);                                             \
    assert_eq(res.get<ast::constant_expression>().value, value);                \
    global_ctx.clear_errors_and_warnings();                                     \
} while (false)

#define x_fail(str)                                                             \
do {                                                                            \
    bz::u8string_view const file = str;                                         \
    auto const tokens = lex::get_tokens(file, 0, lex_ctx);                      \
    assert_false(global_ctx.has_errors_or_warnings());                          \
    auto it = tokens.begin();                                                   \
    auto res = parse_expression(it, tokens.end() - 1, parse_ctx, precedence{}); \
    resolve_expression(res, parse_ctx);                                         \
    consteval_guaranteed(res, parse_ctx);                                       \
    assert_eq(it, tokens.end() - 1);                                            \
    assert_false(res.is_error());                                               \
    assert_false(res.is<ast::constant_expression>());                           \
    global_ctx.clear_errors_and_warnings();                                     \
} while (false)

	x("'a'", ast::constant_value_kind::u8char, 'a');
	x("123", ast::constant_value_kind::sint, 123);
	x("123u8", ast::constant_value_kind::uint, 123);
	x("\"hello\"", ast::constant_value_kind::string, "hello");

	// x("__builtin_exp_f64(1.5)", ast::constant_value_kind::float64, std::exp(1.5));
	// x("__builtin_exp_f32(1.5f32)", ast::constant_value_kind::float32, std::exp(1.5f));
	// x("__builtin_sinh_f32(1.6f32)", ast::constant_value_kind::float32, std::sinh(1.6f));

	x_fail("{ 0; __builtin_exp_f64(1.5) }");
	x_fail("{ 0; __builtin_exp_f32(1.5f32) }");
	x_fail("{ 0; __builtin_sinh_f32(1.6f32) }");

	x("3 + 4", ast::constant_value_kind::sint, 7);
	x("3 / 4", ast::constant_value_kind::sint, 0);
	x_fail("3 / 0");
	x_fail("3u32 << 32");

	return {};

#undef x
#undef x_fail
}

static bz::optional<bz::u8string> consteval_try_test(ctx::global_context &global_ctx)
{
	ctx::lex_context lex_ctx(global_ctx);
	ctx::parse_context parse_ctx(global_ctx);
	parse_ctx.current_global_scope = global_ctx._builtin_global_scope;

#define x(str, kind, res_value)                                                 \
do {                                                                            \
    bz::u8string_view const file = str;                                         \
    auto const tokens = lex::get_tokens(file, 0, lex_ctx);                      \
    assert_false(global_ctx.has_errors_or_warnings());                          \
    auto it = tokens.begin();                                                   \
    auto res = parse_expression(it, tokens.end() - 1, parse_ctx, precedence{}); \
    resolve_expression(res, parse_ctx);                                         \
    auto auto_type = ast::make_auto_typespec(nullptr);                          \
    resolve::match_expression_to_type(res, auto_type, parse_ctx);               \
    consteval_try(res, parse_ctx);                                              \
    assert_eq(it, tokens.end() - 1);                                            \
    assert_false(res.is_error());                                               \
    assert_true(res.is<ast::constant_expression>());                            \
    ast::constant_value value;                                                  \
    value.emplace<kind>(res_value);                                             \
    assert_eq(res.get<ast::constant_expression>().value, value);                \
    global_ctx.clear_errors_and_warnings();                                     \
} while (false)

#define x_fail(str)                                                             \
do {                                                                            \
    bz::u8string_view const file = str;                                         \
    auto const tokens = lex::get_tokens(file, 0, lex_ctx);                      \
    assert_false(global_ctx.has_errors_or_warnings());                          \
    auto it = tokens.begin();                                                   \
    auto res = parse_expression(it, tokens.end() - 1, parse_ctx, precedence{}); \
    resolve_expression(res, parse_ctx);                                         \
    auto auto_type = ast::make_auto_typespec(nullptr);                          \
    resolve::match_expression_to_type(res, auto_type, parse_ctx);               \
    consteval_try(res, parse_ctx);                                              \
    assert_eq(it, tokens.end() - 1);                                            \
    assert_true(res.has_consteval_failed());                                    \
    assert_true(global_ctx.has_errors());                                       \
    global_ctx.clear_errors_and_warnings();                                     \
} while (false)

	x("'a'", ast::constant_value_kind::u8char, 'a');
	x("123", ast::constant_value_kind::sint, 123);
	x("123u8", ast::constant_value_kind::uint, 123);
	x("\"hello\"", ast::constant_value_kind::string, "hello");
	x("__builtin_exp_f64(1.5)",        ast::constant_value_kind::float64, std::exp(1.5));
	x("{ 0; __builtin_exp_f64(1.5) }", ast::constant_value_kind::float64, std::exp(1.5));
	x("__builtin_exp_f32(1.5f32)",        ast::constant_value_kind::float32, std::exp(1.5f));
	x("{ 0; __builtin_exp_f32(1.5f32) }", ast::constant_value_kind::float32, std::exp(1.5f));
	x("__builtin_sinh_f32(1.6f32)",        ast::constant_value_kind::float32, std::sinh(1.6f));
	x("{ 0; __builtin_sinh_f32(1.6f32) }", ast::constant_value_kind::float32, std::sinh(1.6f));
	x(R"({
		function factorial(n) -> typeof n
		{
			type T = typeof n;
			mut result = 1 as T;
			for (mut i = 1 as T; i <= n; ++i)
			{
				result *= i;
			}
			return result;
		}
		factorial(10)
	})", ast::constant_value_kind::sint, 3628800);
	x(R"({
		function factorial(n) -> typeof n
		{
			type T = typeof n;
			mut result = 1 as T;
			for (mut i = 1 as T; i <= n; ++i)
			{
				result *= i;
			}
			return result;
		}
		factorial(10u)
	})", ast::constant_value_kind::uint, 3628800);
	x(R"({
		function foo() -> [10: int32]
		{
			mut result: [10: int32];
			for (mut i = 0; i < 10; ++i)
			{
				result[i] = i;
			}
			return result;
		}
		consteval vals = foo();
		vals[3]
	})", ast::constant_value_kind::sint, 3);

	x_fail(R"({ if (x) { 0 } else { 1 } })");
	x_fail(R"asdf({
		function foo() -> int32
		{
			__builtin_println_stdout("hello from foo()");
			return 0;
		}
		foo()
	})asdf");
	x_fail(R"({
		mut arr: [4: int32] = [ 1, 2, 3, 4 ];
		let index = -1;
		arr[index] = 3;
		0
	})");
	x_fail(R"({
		mut arr: [4: int32] = [ 1, 2, 3, 4 ];
		let index = 4;
		arr[index] = 3;
		0
	})");
	x_fail(R"({
		mut arr: [4: int32] = [ 1, 2, 3, 4 ];
		let index = 4u;
		arr[index] = 3;
		0
	})");
	x_fail(R"({
		@symbol_name("exp") function my_exp(x: float64) -> float64;
		let e = my_exp(1.0);
		0
	})");
	x_fail(R"({
		unreachable;
	})");

	return {};

#undef x
#undef x_fail
}

test_result consteval_test(ctx::global_context &global_ctx)
{
	test_begin();

	test_fn(consteval_guaranteed_test, global_ctx);
	global_ctx.report_and_clear_errors_and_warnings();
	test_fn(consteval_try_test, global_ctx);
	global_ctx.report_and_clear_errors_and_warnings();

	test_end();
}
