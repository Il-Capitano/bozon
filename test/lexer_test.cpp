#include "lexer.cpp"
#include "test.h"

// multi_char_tokens and keywords arrays are sorted in terms of token length
template<typename T>
constexpr bool is_reverse_sorted(T const &arr)
{
	if (arr.size() == 0)
	{
		return true;
	}
	auto it = arr.begin();
	auto trail = it;
	++it;
	auto const end = arr.end();
	for (; it != end; ++it, ++trail)
	{
		if (it->first.length() > trail->first.length())
		{
			return false;
		}
	}
	return true;
}

static_assert(is_reverse_sorted(multi_char_tokens), "multi_char_tokens is not sorted");
static_assert(is_reverse_sorted(keywords), "keywords is not sorted");

void file_iterator_test(void)
{
	bz::string_view file = "\nthis is line #2\n";
	file_iterator it = {
		file.begin(), "<source>"
	};
	assert_eq(it.line, 1);
	assert_eq(it.column, 1);
	assert_eq(it.file, "<source>");
	assert_eq(it.it, file.begin());
	++it;
	assert_eq(it.line, 2);
	assert_eq(it.column, 1);
	++it;
	assert_eq(it.line, 2);
	assert_eq(it.column, 2);
	++it;
	assert_eq(it.line, 2);
	assert_eq(it.column, 3);
}

void get_token_value_test(void)
{
	for (auto t : multi_char_tokens)
	{
		assert_eq(t.first, get_token_value(t.second));
	}

	for (auto kw : keywords)
	{
		assert_eq(kw.first, get_token_value(kw.second));
	}

	for (unsigned char c = ' '; c != 128; ++c)
	{
		assert_eq(bz::string(c), get_token_value(c));
	}

	assert_eq(get_token_value(token::identifier), "identifier");
	assert_eq(get_token_value(token::number_literal), "number literal");
	assert_eq(get_token_value(token::string_literal), "string literal");
	assert_eq(get_token_value(token::character_literal), "character literal");
}

void bad_char_test(void)
{
	bz::string_view file = "\nthis is line #2\n";
	auto it = file.begin();
	file_iterator file_it = {
		it, "<source>"
	};

	{
		auto err = bad_char(file_it, "this is an error message");
		assert_eq(err.file, "<source>");
		assert_eq(err.line, 1);
		assert_eq(err.column, 1);
		assert_eq(err.message, "this is an error message");
		assert_eq(err.notes.size(), 0);
		assert_eq(err.src_begin, it);
		assert_eq(err.src_pivot, it);
		assert_eq(err.src_end, it + 1);
	}

	{
		auto err = bad_char("filename", 123, 456, "this is another error message");
		assert_eq(err.file, "filename");
		assert_eq(err.line, 123);
		assert_eq(err.column, 456);
		assert_eq(err.message, "this is another error message");
		assert_eq(err.notes.size(), 0);
		assert_eq(err.src_begin, nullptr);
		assert_eq(err.src_pivot, nullptr);
		assert_eq(err.src_end, nullptr);
	}
}

void skip_comments_and_whitespace_test(void)
{
	{
		bz::string_view file = "";
		assert_eq(file.begin(), file.end());
		file_iterator it = { file.begin(), "" };
		skip_comments_and_whitespace(it, file.end());
		assert_eq(it.it, file.end());
	}

#define x(str)                           \
bz::string_view file = str;              \
file_iterator it = { file.begin(), "" }; \
skip_comments_and_whitespace(it, file.end())

	{
		x("this is not whitespace");
		assert_eq(it.it, file.begin());
	}

	{
		x("     \t\t\n\t\n\n\n   \n\t\t\n    ");
		assert_eq(it.it, file.end());
	}

	{
		x("//\nthis is not whitespace");
		assert_neq(it.it, file.end());
		assert_eq(it.it, file.begin() + 3);
	}

	{
		x("// this is a comment\nthis is not whitespace");
		assert_neq(it.it, file.end());
		assert_eq(it.it, std::find(file.begin(), file.end(), '\n') + 1);
	}

	{
		x("// this is a comment\n\tthis is not whitespace");
		assert_neq(it.it, file.end());
		assert_eq(it.it, std::find(file.begin(), file.end(), '\n') + 2); // extra tab here
	}

	{
		x("// comment\n\n\n");
		assert_eq(it.it, std::find(file.begin(), file.end(), '\n') + 3);
		assert_eq(it.it, file.end());
	}

	{
		x("/**/");
		assert_eq(it.it, file.end());
	}

	{
		x("/*              \n\n this is a comment...\t\t */    \n\n\t   \t");
		assert_eq(it.it, file.end());
	}

	{
		x("/* this /* is a nested */ comment */  a");
		assert_eq(it.it, file.end() - 1);
	}

	{
		x("/*");
		assert_eq(it.it, file.end());
	}

	{
		x("/ * */");
		assert_eq(it.it, file.begin());
	}

	{
		x("/* / * */ */");
		assert_eq(it.it, file.end() - 2);
	}
}

void lexer_test(void)
{
	test_begin();

	test(file_iterator_test);
	test(get_token_value_test);
	test(bad_char_test);
	test(skip_comments_and_whitespace_test);

	test_end();
}
