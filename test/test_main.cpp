#include "test.h"
#include "timer.h"

test_result lexer_test(void);
test_result first_pass_parser_test(void);
test_result parser_test(void);

int main(void)
{
	size_t test_count = 0;
	size_t passed_count = 0;

	auto add_to_total = [&](test_result res)
	{
		test_count += res.test_count;
		passed_count += res.passed_count;
	};

	try
	{
		auto const begin = timer::now();
		add_to_total(lexer_test());
		add_to_total(first_pass_parser_test());
		add_to_total(parser_test());
		auto const end = timer::now();

		auto const passed_percentage = 100 * (static_cast<double>(passed_count) / test_count);
		auto const highlight_color =
			passed_count == test_count
			? colors::bright_green
			: colors::bright_red;

		bz::print(
			"\nFinished running all tests in {:.3f}ms\n"
			"{}{}/{}{} ({}{:.2f}%{}) tests passed\n",
			(end - begin).count() * 1000.0,
			highlight_color,
			passed_count, test_count,
			colors::clear,
			highlight_color,
			passed_percentage,
			colors::clear
		);
	}
	catch (std::exception &e)
	{
		bz::print("\nan exception occurred: {}", e.what());
	}

	return 0;
}
