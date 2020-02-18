#include "test.h"

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
		add_to_total(lexer_test());
		add_to_total(first_pass_parser_test());
		add_to_total(parser_test());

		auto const passed_percentage = 100 * (static_cast<double>(passed_count) / test_count);
		auto const highlight_color =
			passed_count == test_count
			? colors::bright_green
			: colors::bright_red;

		bz::printf(
			"\nFinished running all tests\n"
			"{}{}/{}{} ({}{:.2f}%{}) tests passed\n",
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
		bz::printf("\nan exception occurred: {}", e.what());
	}

	return 0;
}
