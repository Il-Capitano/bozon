#include "lex/lexer.cpp"

#include "test.h"
#include "ctx/global_context.h"
#include "ctx/lex_context.h"

using namespace lex;

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

static bz::optional<bz::u8string> file_iterator_test(void)
{
	bz::u8string_view file = "\nthis is line #2\n";
	file_iterator it = {
		file.begin(), 0
	};
	assert_eq(it.line, 1);
	assert_eq(it.file_id, 0);
	assert_eq(it.it, file.begin());
	++it;
	assert_eq(it.line, 2);
	++it;
	assert_eq(it.line, 2);
	++it;
	assert_eq(it.line, 2);

	return {};
}

static bz::optional<bz::u8string> get_token_value_test(void)
{
	for (auto t : multi_char_tokens)
	{
		assert_eq(t.first, get_token_value(t.second));
	}

	for (auto kw : keywords)
	{
		assert_eq(kw.first, get_token_value(kw.second));
	}

//	for (unsigned char c = ' '; c != 128; ++c)
//	{
//		assert_eq(bz::u8string(1, c), get_token_value(c));
//	}

	assert_eq(get_token_value(token::identifier), "identifier");
	assert_eq(get_token_value(token::integer_literal), "integer literal");
	assert_eq(get_token_value(token::floating_point_literal), "floating-point literal");
	assert_eq(get_token_value(token::hex_literal), "hexadecimal literal");
	assert_eq(get_token_value(token::oct_literal), "octal literal");
	assert_eq(get_token_value(token::bin_literal), "binary literal");
	assert_eq(get_token_value(token::string_literal), "string literal");
	assert_eq(get_token_value(token::character_literal), "character literal");

	return {};
}

static bz::optional<bz::u8string> skip_comments_and_whitespace_test(ctx::global_context &global_ctx)
{
	ctx::lex_context lex_ctx(global_ctx);

	{
		bz::u8string_view file = "";
		assert_eq(file.begin(), file.end());
		file_iterator it = { file.begin(), 0 };
		skip_comments_and_whitespace(it, file.end(), lex_ctx);
		assert_eq(it.it, file.end());
	}

#define x(str)                                        \
bz::u8string_view file = str;                         \
file_iterator it = { file.begin(), 0 };               \
skip_comments_and_whitespace(it, file.end(), lex_ctx)

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
		//                                       ^ file.begin() + 38
		assert_eq(it.it, file.begin() + 38);
	}

	{
		x("/* comment ");
		assert_eq(it.it, file.end());
		assert_true(global_ctx.has_warnings());
	}

	{
		x("/ * not a comment */");
		assert_eq(it.it, file.begin());
	}

	{
		x("/* / * comment */ not nested */");
		//                   ^ file.begin() + 18
		assert_eq(it.it, file.begin() + 18);
	}

	{
		x("/* // */\n   a");
		//              ^ file.begin() + 12
		assert_eq(it.it, file.begin() + 12);
	}

	{
		x("/* // \n */");
		assert_eq(it.it, file.end());
	}

	{
		x("// /*\n */");
		//         ^ file.begin() + 7
		assert_eq(it.it, file.begin() + 7);
	}

	return {};

#undef x
}

static bz::optional<bz::u8string> get_identifier_or_keyword_token_test(ctx::global_context &global_ctx)
{
	ctx::lex_context context(global_ctx);

#define x_id(str)                                                        \
bz::u8string_view const file = str;                                      \
file_iterator it = { file.begin(), 0 };                                  \
auto const t = get_identifier_or_keyword_token(it, file.end(), context); \
assert_false(global_ctx.has_errors());                                   \
assert_eq(t.kind, token::identifier)

#define x_kw(str, kw_kind)                                               \
bz::u8string_view const file = str;                                      \
file_iterator it = { file.begin(), 0 };                                  \
auto const t = get_identifier_or_keyword_token(it, file.end(), context); \
assert_false(global_ctx.has_errors());                                   \
assert_eq(t.kind, kw_kind)

#define xx_kw(str, kw_kind) do { x_kw(str, kw_kind); } while (false)


	{
		x_id("asdfjkl");
		assert_eq(it.it, file.end());
		assert_eq(t.value, "asdfjkl");
	}

	{
		x_id("____");
		assert_eq(it.it, file.end());
		assert_eq(t.value, "____");
	}

	{
//		x_id("0123");
	}

	{
		x_id("a0123");
		assert_eq(it.it, file.end());
		assert_eq(t.value, "a0123");
	}

	{
		x_id("_0123");
		assert_eq(it.it, file.end());
		assert_eq(t.value, "_0123");
	}

	{
		x_id("asdf ");
		//        ^ file.begin() + 4
		assert_eq(it.it, file.begin() + 4);
		assert_eq(t.value, "asdf");
	}

	{
		x_id("asdf+");
		//        ^ file.begin() + 4
		assert_eq(it.it, file.begin() + 4);
		assert_eq(t.value, "asdf");
	}

	xx_kw("namespace", token::kw_namespace);
	xx_kw("function", token::kw_function);
	xx_kw("operator", token::kw_operator);
	xx_kw("typename", token::kw_typename);
	xx_kw("return", token::kw_return);
	xx_kw("struct", token::kw_struct);
	xx_kw("sizeof", token::kw_sizeof);
	xx_kw("typeof", token::kw_typeof);
	xx_kw("while", token::kw_while);
	xx_kw("class", token::kw_class);
	xx_kw("using", token::kw_using);
	xx_kw("const", token::kw_const);
	xx_kw("false", token::kw_false);
	xx_kw("else", token::kw_else);
	xx_kw("auto", token::kw_auto);
	xx_kw("true", token::kw_true);
	xx_kw("null", token::kw_null);
	xx_kw("for", token::kw_for);
	xx_kw("let", token::kw_let);
	xx_kw("if", token::kw_if);

	{
		x_id("False");
		assert_eq(t.value, "False");
	}

	return {};

#undef x_id
#undef x_kw
}

static bz::optional<bz::u8string> get_character_token_test(ctx::global_context &global_ctx)
{
	ctx::lex_context context(global_ctx);

#define x(str, c, it_pos)                                        \
do {                                                             \
    bz::u8string_view const file = str;                          \
    file_iterator it = { file.begin(), 0 };                      \
    auto const t = get_character_token(it, file.end(), context); \
    assert_false(global_ctx.has_errors());                       \
    assert_eq(t.kind, token::character_literal);                 \
    assert_eq(t.value, c);                                       \
    assert_eq(it.it, it_pos);                                    \
} while (false)

#define x_err(str, it_pos)                        \
do {                                              \
    bz::u8string_view const file = str;           \
    file_iterator it = { file.begin(), 0 };       \
    get_character_token(it, file.end(), context); \
    assert_true(global_ctx.has_errors());         \
    global_ctx.clear_errors_and_warnings();       \
    assert_eq(it.it, it_pos);                     \
} while (false)

	x("'a'", "a", file.end());
	x("'0'", "0", file.end());
	x("'a' ", "a", file.begin() + 3);
	//    ^ file.begin() + 3
	x("'\\''", "\\'", file.end());
	x("'\"'", "\"", file.end());
	x("'\\x00'", "\\x00", file.end());
	x("'\\x7f'", "\\x7f", file.end());

	x_err("'", file.end());
	x_err("''", file.end());
	x_err("'missing closing ' that's not at the end", file.begin() + 8);
	//             ^ file.begin() + 2
	// the 'issing' is the postfix here
	x_err("'\\j'", file.end());
	x_err("'\\", file.end());
	x_err("'\\x'", file.end());
	x_err("'\\x0'", file.end());
	x_err("'\\x80'", file.end());

	return {};

#undef x
#undef x_err
}

static bz::optional<bz::u8string> get_string_token_test(ctx::global_context &global_ctx)
{
	ctx::lex_context context(global_ctx);

#define x(str, c, it_pos)                                     \
do {                                                          \
    bz::u8string_view const file = str;                       \
    file_iterator it = { file.begin(), 0 };                   \
    auto const t = get_string_token(it, file.end(), context); \
    assert_false(global_ctx.has_errors());                    \
    assert_eq(t.kind, token::string_literal);                 \
    assert_eq(t.value, c);                                    \
    assert_eq(it.it, it_pos);                                 \
} while (false)

#define x_err(str, it_pos)                     \
do {                                           \
    bz::u8string_view const file = str;        \
    file_iterator it = { file.begin(), 0 };    \
    get_string_token(it, file.end(), context); \
    assert_true(global_ctx.has_errors());      \
    global_ctx.clear_errors_and_warnings();    \
    assert_eq(it.it, it_pos);                  \
} while (false)

	x(R"("")", "", file.end());
	x(R"("this is a string")", "this is a string", file.end());
	x(R"("" )", "", file.begin() + 2);
	//     ^ file.begin() + 2
	x(R"("'")", "'", file.end());
	x(R"("\'")", "\\'", file.end());
	x(R"("\"")", "\\\"", file.end());
	x(R"("this is a string" and this is not)", "this is a string", file.begin() + 18);
	//                     ^ file.begin() + 18

	x_err(R"("     )", file.end());
	x_err(R"("\j")", file.end());
	x_err(R"("\)", file.end());

	return {};

#undef x
#undef x_err
}

static bz::optional<bz::u8string> get_number_token_test(ctx::global_context &global_ctx)
{
	ctx::lex_context context(global_ctx);

#define x(str, it_pos)                                        \
do {                                                          \
    bz::u8string_view const file = str;                       \
    file_iterator it = { file.begin(), 0 };                   \
    auto const t = get_number_token(it, file.end(), context); \
    assert_false(global_ctx.has_errors());                    \
    assert_true(                                              \
        t.kind == token::integer_literal                      \
        || t.kind == token::floating_point_literal            \
        || t.kind == token::hex_literal                       \
        || t.kind == token::oct_literal                       \
        || t.kind == token::bin_literal                       \
    );                                                        \
    assert_eq(it.it, it_pos);                                 \
} while (false)

	x("1234", file.end());
	x("1234 ", file.begin() + 4);
	x("1234-", file.begin() + 4);
	x("1'2'3'4", file.end());
	x("1'''''''''2'3'4'''''''", file.end());
	x("1'''''''''2'3'4'''' '''", file.begin() + 19);
	//                    ^ file.begin() + 19

	x("1e1", file.end());
	x("1.0e1", file.end());
	x("1e+1", file.end());
	x("1e-1", file.end());
	x("1e-10", file.end());
	x("1.0e-10", file.end());
	x("1''.0e-10", file.end());
	x("1.0e 10", file.begin() + 4);
	//     ^
	x("1.0e'10", file.begin() + 4);
	//     ^
	x("1.e'10", file.begin() + 1);
	//  ^

	x("1.1", file.end());
	x("1.1.1", file.begin() + 3);
	//    ^ file.begin() + 3
	x("1'''2'''2323'1'.2124213''4512''", file.end());
	x("1'''2'''2323'1'.'''''2124213''4512''.''123", file.begin() + 15);
	//                ^ file.begin() + 15

	x("0x0123456789abcdef", file.end());
	x("0x0123456789ABCDEF", file.end());
	x("0X0123456789abcdef", file.end());
	x("0X0123456789ABCDEF", file.end());
	x("0x0123'4567'89ab'cdef", file.end());
	x("0o01234567", file.end());
	x("0O01234567", file.end());
	x("0b01010101", file.end());
	x("0B01010101", file.end());

	return {};

#undef x
}

static bz::optional<bz::u8string> get_single_char_token_test(ctx::global_context &global_ctx)
{
	ctx::lex_context context(global_ctx);

	for (unsigned char c = ' '; c != 128; ++c)
	{
		bz::u8string const _file(1, c);
		bz::u8string_view const file = _file;
		file_iterator it = { file.begin(), 0 };
		auto const t = get_single_char_token(it, file.end(), context);
		assert_eq(t.kind, c);
		assert_eq(t.value, file);
		assert_false(global_ctx.has_errors());
	}

	return {};
}

static bz::optional<bz::u8string> get_next_token_test(ctx::global_context &global_ctx)
{
	ctx::lex_context context(global_ctx);

#define x(str, token_kind)                                  \
do {                                                        \
    bz::u8string_view const file = str;                     \
    file_iterator it = { file.begin(), 0 };                 \
    auto const t = get_next_token(it, file.end(), context); \
    assert_false(global_ctx.has_errors());                  \
    assert_eq(t.kind, token_kind);                          \
} while (false)

#define x_id(str) x(str, token::identifier)
#define x_int(str) x(str, token::integer_literal)
#define x_float(str) x(str, token::floating_point_literal)
#define x_hex(str) x(str, token::hex_literal)
#define x_oct(str) x(str, token::oct_literal)
#define x_bin(str) x(str, token::bin_literal)
#define x_str(str) x(str, token::string_literal)
#define x_char(str) x(str, token::character_literal)

	x("", token::eof);

	x_id("some_id");
	x_id("_asdf");
	x_id("_1234");
	x_id("a1234");
	x_id("_++-.,");

	x_int("1234");
	x_float("1.1");
	x_float("1e3");
	x_float("1.3423e-33");
	x_float("1''''2''.1''''''");
	x_hex("0x0'''123''4567''''''''89abcdef");
	x_oct("0o01'234'567");
	x_bin("0b'0101'0101'0101");

	x_str(R"("this is a string")");
	x_str(R"("")");
	x_str(R"("\"")");

	x_char("'a'");
	x_char("'\\t'");

	for (auto &mc : multi_char_tokens)
	{
		x(mc.first, mc.second);
	}

	for (auto &kw : keywords)
	{
		x(kw.first, kw.second);
	}

	return {};

#undef x
}

static bz::optional<bz::u8string> get_tokens_test(ctx::global_context &global_ctx)
{
	ctx::lex_context context(global_ctx);

#define assert_eqs                                                                                \
([](bz::vector<token> const &ts, bz::vector<uint32_t> const &kinds) -> bz::optional<bz::u8string> \
{                                                                                                 \
    assert_eq(ts.size(), kinds.size() + 1);                                                       \
    for (size_t i = 0; i < kinds.size(); ++i)                                                     \
    {                                                                                             \
        assert_eq(ts[i].kind, kinds[i]);                                                          \
    }                                                                                             \
    assert_eq(ts.back().kind, token::eof);                                                        \
                                                                                                  \
	return {};                                                                                    \
})

#define x(str, ...)                                      \
do {                                                     \
    bz::u8string_view const file = str;                  \
    auto const ts = get_tokens(file, 0, context);        \
    assert_false(global_ctx.has_errors());               \
    auto const result = assert_eqs(ts, { __VA_ARGS__ }); \
    if (result.has_value())                              \
    {                                                    \
        return result;                                   \
    }                                                    \
} while (false)

	x("");
	x("+-+-", token::plus, token::minus, token::plus, token::minus);
	x("+++", token::plus_plus, token::plus);
	x(
		"function main() {}",
		token::kw_function, token::identifier,
		token::paren_open, token::paren_close,
		token::curly_open, token::curly_close
	);
	x(".....", token::dot_dot_dot, token::dot_dot);
	x(
		"...auto, hello",
		token::dot_dot_dot, token::kw_auto,
		token::comma, token::identifier
	);
	x(
		"comment: /* asdfasdfasdf */asdf",
		token::identifier, token::colon,
		token::identifier
	);
	x("./**/.", token::dot, token::dot);

	return {};

#undef assert_eqs
#undef x
}

test_result lexer_test(ctx::global_context &global_ctx)
{
	test_begin();

	test_fn(file_iterator_test);
	test_fn(get_token_value_test);
	test_fn(skip_comments_and_whitespace_test, global_ctx);
	test_fn(get_identifier_or_keyword_token_test, global_ctx);
	test_fn(get_character_token_test, global_ctx);
	test_fn(get_string_token_test, global_ctx);
	test_fn(get_number_token_test, global_ctx);
	test_fn(get_single_char_token_test, global_ctx);
	test_fn(get_next_token_test, global_ctx);
	test_fn(get_tokens_test, global_ctx);

	test_end();
}
