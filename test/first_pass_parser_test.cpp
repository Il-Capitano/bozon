#include "ast/type.cpp"
#include "ast/expression.cpp"
#include "ast/statement.cpp"
#include "first_pass_parser.cpp"
#include "test.h"

void get_expression_or_type_test(void)
{
	bz::vector<error> errors = {};

#define x
}

test_result first_pass_parser_test(void)
{
	test_begin();

	test(get_expression_or_type_test);

	test_end();
}