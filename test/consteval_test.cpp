#include "parse/consteval.cpp"

#include "test.h"
#include "ctx/global_context.h"
#include "ctx/lex_context.h"
#include "ctx/parse_context.h"
#include "parse/expression_parser.h"

using namespace parse;

static bz::optional<bz::u8string> consteval_try_test(ctx::global_context &global_ctx)
{
	ctx::lex_context lex_ctx(global_ctx);
	ctx::parse_context parse_ctx(global_ctx);
	ctx::decl_set global_decls = ctx::get_default_decls();
	parse_ctx.global_decls = &global_decls;

#define x(str, kind, res_value)                                                 \
do {                                                                            \
    bz::u8string_view const file = str;                                         \
    auto const tokens = lex::get_tokens(file, 0, lex_ctx);                      \
    assert_false(global_ctx.has_errors_or_warnings());                          \
    auto it = tokens.begin();                                                   \
    auto res = parse_expression(it, tokens.end() - 1, parse_ctx, precedence{}); \
    consteval_try(res, parse_ctx);                                              \
    assert_eq(it, tokens.end() - 1);                                            \
    assert_false(res.is_error());                                               \
    assert_true(res.is<ast::constant_expression>());                            \
    ast::constant_value value;                                                  \
    value.emplace<kind>(res_value);                                             \
    assert_eq(res.get<ast::constant_expression>().value, value);                \
    global_ctx.clear_errors_and_warnings();                                     \
    global_ctx._comptime_executor.functions_to_compile.clear();                 \
} while (false)

#define x_fail(str)                                                             \
do {                                                                            \
    bz::u8string_view const file = str;                                         \
    auto const tokens = lex::get_tokens(file, 0, lex_ctx);                      \
    assert_false(global_ctx.has_errors_or_warnings());                          \
    auto it = tokens.begin();                                                   \
    auto res = parse_expression(it, tokens.end() - 1, parse_ctx, precedence{}); \
    consteval_try(res, parse_ctx);                                              \
    assert_eq(it, tokens.end() - 1);                                            \
    assert_false(res.is_error());                                               \
    assert_true(res.has_consteval_failed());                                    \
    global_ctx.clear_errors_and_warnings();                                     \
    global_ctx._comptime_executor.functions_to_compile.clear();                 \
} while (false)

	x("'a'", ast::constant_value::u8char, 'a');
	x("123", ast::constant_value::sint, 123);
	x("123u8", ast::constant_value::uint, 123);
	x("\"hello\"", ast::constant_value::string, "hello");
	x("__builtin_exp_f64(1.5)",        ast::constant_value::float64, std::exp(1.5));
	x("{ 0; __builtin_exp_f64(1.5) }", ast::constant_value::float64, std::exp(1.5));
	x("__builtin_exp_f32(1.5f32)",        ast::constant_value::float32, std::exp(1.5f));
	x("{ 0; __builtin_exp_f32(1.5f32) }", ast::constant_value::float32, std::exp(1.5f));
	x("__builtin_sinh_f32(1.6f32)",        ast::constant_value::float32, std::sinh(1.6f));
	x("{ 0; __builtin_sinh_f32(1.6f32) }", ast::constant_value::float32, std::sinh(1.6f));
	x(R"({
		function factorial(n) -> typeof n
		{
			if (n <= 1 as typeof n)
			{
				return n;
			}
			return n * factorial(n - 1 as typeof n);
		}
		factorial(10)
	})", ast::constant_value::sint, 3628800);
	x(R"({
		function factorial(n) -> typeof n
		{
			if (n <= 1 as typeof n)
			{
				return n;
			}
			return n * factorial(n - 1 as typeof n);
		}
		factorial(10u)
	})", ast::constant_value::uint, 3628800);
	x(R"({
		function factorial(n) -> typeof n
		{
			type T = typeof n;
			let result = 1 as T;
			for (let i = 1 as T; i <= n; ++i)
			{
				result *= i;
			}
			return result;
		}
		factorial(10)
	})", ast::constant_value::sint, 3628800);
	x(R"({
		function factorial(n) -> typeof n
		{
			type T = typeof n;
			let result = 1 as T;
			for (let i = 1 as T; i <= n; ++i)
			{
				result *= i;
			}
			return result;
		}
		factorial(10u)
	})", ast::constant_value::uint, 3628800);

	x_fail(R"asdf({
		function foo() -> int32
		{
			__builtin_println_stdout("hello from foo()");
			return 0;
		}
		foo()
	})asdf");

	return {};

#undef x
#undef x_fail
}

test_result consteval_test(void)
{
	test_begin();

	ctx::global_context global_ctx;

	if (!global_ctx.initialize_llvm())
	{
		global_ctx.report_and_clear_errors_and_warnings();
		bz_unreachable;
	}
	if (!global_ctx.initialize_builtins())
	{
		global_ctx.report_and_clear_errors_and_warnings();
		bz_unreachable;
	}
	if (!global_ctx.parse())
	{
		global_ctx.report_and_clear_errors_and_warnings();
		bz_unreachable;
	}

	test_fn(consteval_try_test, global_ctx);

	test_end();
}
