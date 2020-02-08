#include "test.h"

test_result lexer_test(void);
test_result first_pass_parser_test(void);

int main(void)
{
	size_t test_count = 0;
	size_t passed_count = 0;

	auto add_to_total = [&](test_result res)
	{
		test_count += res.test_count;
		passed_count += res.passed_count;
	};

	add_to_total(lexer_test());
	add_to_total(first_pass_parser_test());

	std::cout << "\nFinished running all tests\n"
		<< passed_count << '/' << test_count << " tests passed";

	return 0;
}
