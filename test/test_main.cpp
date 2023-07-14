#include "test.h"
#include "timer.h"
#include "global_data.h"
#include "ctx/global_context.h"
#include "crash_handling.h"

test_result ryu_test();
test_result lexer_test(ctx::global_context &global_ctx);
test_result parser_test(ctx::global_context &global_ctx);
test_result consteval_test(ctx::global_context &global_ctx);

int main(int argc, char const **argv)
{
	register_crash_handlers();
	global_data::import_dirs.push_back("bozon-stdlib");

	ctx::global_context global_ctx;

	if (!global_ctx.parse_command_line(argc, argv))
	{
		global_ctx.report_and_clear_errors_and_warnings();
		bz_unreachable;
	}
	if (!global_ctx.initialize_target_info())
	{
		global_ctx.report_and_clear_errors_and_warnings();
		bz_unreachable;
	}
	if (!global_ctx.initialize_backend())
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

	if (global_ctx.has_errors())
	{
		global_ctx.report_and_clear_errors_and_warnings();
		bz_unreachable;
	}

	global_ctx.report_and_clear_errors_and_warnings();

	auto in_ms = [](auto time)
	{
		return static_cast<double>(
			std::chrono::duration_cast<std::chrono::nanoseconds>(time).count()
		) * 1e-6;
	};

	// enable all warnings
	for (auto &warning : global_data::warnings)
	{
		warning = true;
	}

	size_t test_count = 0;
	size_t passed_count = 0;

	auto add_to_total = [&](test_result res)
	{
		test_count += res.test_count;
		passed_count += res.passed_count;
	};

	auto const begin = timer::now();
	add_to_total(ryu_test());
	add_to_total(lexer_test(global_ctx));
	add_to_total(parser_test(global_ctx));
	add_to_total(consteval_test(global_ctx));
	auto const end = timer::now();

	auto const passed_percentage = 100 * (static_cast<double>(passed_count) / test_count);
	auto const highlight_color =
		passed_count == test_count
		? colors::bright_green
		: colors::bright_red;

	bz::print(
		"\nFinished running all tests in {:.3f}ms\n"
		"{}{}/{}{} ({}{:.2f}%{}) tests passed\n",
		in_ms(end - begin),
		highlight_color,
		passed_count, test_count,
		colors::clear,
		highlight_color,
		passed_percentage,
		colors::clear
	);

	global_ctx.report_and_clear_errors_and_warnings();

// see explanation in src/main.cpp
#ifdef _WIN32
	std::exit(passed_count == test_count ? 0 : 1);
#else
	return passed_count == test_count ? 0 : 1;
#endif // windows
}
