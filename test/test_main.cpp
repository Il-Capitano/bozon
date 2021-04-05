#include "test.h"
#include "timer.h"
#include "global_data.h"

test_result lexer_test(void);
test_result parser_test(void);
test_result consteval_test(void);

int main(void)
{
	debug_comptime_ir_output = true;
	use_interpreter = true;

	auto in_ms = [](auto time)
	{
		return static_cast<double>(
			std::chrono::duration_cast<std::chrono::nanoseconds>(time).count()
		) * 1e-6;
	};

	// enable all warnings
	for (auto &warning : warnings)
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
	add_to_total(lexer_test());
	add_to_total(parser_test());
	add_to_total(consteval_test());
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

	return 0;
}
