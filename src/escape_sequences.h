#ifndef ESCAPE_SEQUENCES_H
#define ESCAPE_SEQUENCES_H

#include "core.h"
#include "colors.h"
#include "global_data.h"
#include "ctx/lex_context.h"

constexpr bool is_hex_char(bz::u8char c)
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

constexpr bz::u8char get_hex_value(bz::u8char c)
{
	bz_assert((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
	if (c >= '0' && c <= '9')
	{
		return c - '0';
	}
	else if (c >= 'a' && c <= 'f')
	{
		return c - 'a' + 10;
	}
	else
	{
		return c - 'A' + 10;
	}
}


struct escape_sequence_parser
{
	bz::u8char c;
	bz::u8string_view help;
	void       (*verify)(file_iterator &stream, ctx::char_pos end, ctx::lex_context &context);
	bz::u8char (*get)(bz::u8iterator &it);
};

inline void verify_backslash(file_iterator &stream, ctx::char_pos end, ctx::lex_context &)
{
	bz_assert(stream.it != end);
	bz_assert(*stream.it == '\\');
	++stream;
}

inline bz::u8char get_backslash(bz::u8iterator &it)
{
	bz_assert(*it == '\\');
	++it;
	return '\\';
}

inline void verify_single_quote(file_iterator &stream, ctx::char_pos end, ctx::lex_context &)
{
	bz_assert(stream.it != end);
	bz_assert(*stream.it == '\'');
	++stream;
}

inline bz::u8char get_single_quote(bz::u8iterator &it)
{
	bz_assert(*it == '\'');
	++it;
	return '\'';
}

inline void verify_double_quote(file_iterator &stream, ctx::char_pos end, ctx::lex_context &)
{
	bz_assert(stream.it != end);
	bz_assert(*stream.it == '\"');
	++stream;
}

inline bz::u8char get_double_quote(bz::u8iterator &it)
{
	bz_assert(*it == '\"');
	++it;
	return '\"';
}

inline void verify_new_line(file_iterator &stream, ctx::char_pos end, ctx::lex_context &)
{
	bz_assert(stream.it != end);
	bz_assert(*stream.it == 'n');
	++stream;
}

inline bz::u8char get_new_line(bz::u8iterator &it)
{
	bz_assert(*it == 'n');
	++it;
	return '\n';
}

inline void verify_tab(file_iterator &stream, ctx::char_pos end, ctx::lex_context &)
{
	bz_assert(stream.it != end);
	bz_assert(*stream.it == 't');
	++stream;
}

inline bz::u8char get_tab(bz::u8iterator &it)
{
	bz_assert(*it == 't');
	++it;
	return '\t';
}

inline void verify_carriage_return(file_iterator &stream, ctx::char_pos end, ctx::lex_context &)
{
	bz_assert(stream.it != end);
	bz_assert(*stream.it == 'r');
	++stream;
}

inline bz::u8char get_carriage_return(bz::u8iterator &it)
{
	bz_assert(*it == 'r');
	++it;
	return '\r';
}

inline void verify_hex_char(file_iterator &stream, ctx::char_pos end, ctx::lex_context &context)
{
	bz_assert(stream.it != end);
	bz_assert(*stream.it == 'x');

	static_assert('\\' <= 127);
	auto const escape_char = ctx::char_pos(stream.it.data() - 1);

	++stream;
	if (stream.it == end || !is_hex_char(*stream.it))
	{
		context.bad_char(stream, "\\x must be followed by two hex characters");
		return;
	}
	auto const first_char = stream.it;
	auto const first_char_val = *first_char;
	++stream;
	if (stream.it == end || !is_hex_char(*stream.it))
	{
		context.bad_char(
			stream,
			"\\x must be followed by two hex characters (one byte)",
			{},
			{
				context.make_suggestion(
					stream.file_id, stream.line, first_char, "0",
					bz::format("did you mean '\\x0{:c}'?", first_char_val)
				)
			}
		);
		return;
	}
	auto const second_char_val = *stream.it;
	++stream;
	auto const hex_end = stream.it;

	// we need to restrict the value to 0x7f, so it's a valid UTF-8 code point
	if (!(first_char_val >= '0' && first_char_val <= '7'))
	{
		context.bad_chars(
			stream.file_id, stream.line,
			first_char, first_char, hex_end,
			bz::format(
				"the value 0x{:c}{:c} is too large for a hex character, it must be at most 0x7f",
				first_char_val, second_char_val
			),
			{},
			{
				context.make_suggestion(
					stream.file_id, stream.line, escape_char,
					escape_char, hex_end,
					bz::format("\\u00{:c}{:c}", first_char_val, second_char_val),
					bz::format("use \\u00{:c}{:c} instead", first_char_val, second_char_val)
				)
			}
		);
	}
}

inline bz::u8char get_hex_char(bz::u8iterator &it)
{
	bz_assert(*it == 'x');
	++it; // x

	auto const first_hex_char = *it;
	bz_assert(is_hex_char(first_hex_char));
	++it; // first hex

	auto const second_hex_char = *it;
	bz_assert(is_hex_char(second_hex_char));
	++it; // second hex

	return (get_hex_value(first_hex_char) << 4) | get_hex_value(second_hex_char);
}

inline void verify_unicode_small(file_iterator &stream, ctx::char_pos end, ctx::lex_context &context)
{
	bz_assert(stream.it != end);
	bz_assert(*stream.it == 'u');
	++stream; // u
	auto const first_char = stream.it;

	bz::u8char val = 0;
	// we expect four hex characters
	for (int i = 0; i < 4; ++i, ++stream)
	{
		auto const c = *stream.it;
		if (stream.it == end || !is_hex_char(c))
		{
			context.bad_char(stream, "\\u must be followed by four hex characters (two bytes)");
			break;
		}
		val <<= 4;
		val |= get_hex_value(c);
	}
	if (!bz::is_valid_unicode_value(val))
	{
		context.bad_chars(
			stream.file_id, stream.line,
			first_char, first_char, stream.it,
			bz::format("U+{:04X} is not a valid unicode codepoint", val)
		);
	}
}

inline bz::u8char get_unicode_small(bz::u8iterator &it)
{
	bz_assert(*it == 'u');

	++it; // u
	bz::u8char val = 0;
	for (int i = 0; i < 4; ++i, ++it)
	{
		auto const it_val = *it;
		bz_assert(is_hex_char(it_val));
		val <<= 4;
		val |= get_hex_value(it_val);
	}
	return val;
}

inline void verify_unicode_big(file_iterator &stream, ctx::char_pos end, ctx::lex_context &context)
{
	bz_assert(stream.it != end);
	bz_assert(*stream.it == 'U');
	++stream; // U
	auto const first_char = stream.it;

	bz::u8char val = 0;
	// we expect eight hex characters
	for (int i = 0; i < 8; ++i, ++stream)
	{
		auto const c = *stream.it;
		if (stream.it == end || !is_hex_char(c))
		{
			context.bad_char(stream, "\\U must be followed by eight hex characters (four bytes)");
			return;
		}
		val <<= 4;
		val |= get_hex_value(c);
	}
	if (!bz::is_valid_unicode_value(val))
	{
		context.bad_chars(
			stream.file_id, stream.line,
			first_char, first_char, stream.it,
			bz::format("U+{:04X} is not a valid unicode codepoint", val)
		);
	}
}

inline bz::u8char get_unicode_big(bz::u8iterator &it)
{
	bz_assert(*it == 'U');

	++it; // U
	bz::u8char val = 0;
	for (int i = 0; i < 8; ++i, ++it)
	{
		auto const it_val = *it;
		bz_assert(is_hex_char(it_val));
		val <<= 4;
		val |= get_hex_value(it_val);
	}
	return val;
}


constexpr std::array escape_sequence_parsers = {
	escape_sequence_parser{ '\\', "\\\\",        &verify_backslash,       &get_backslash       },
	escape_sequence_parser{ '\'', "\\\'",        &verify_single_quote,    &get_single_quote    },
	escape_sequence_parser{ '\"', "\\\"",        &verify_double_quote,    &get_double_quote    },
	escape_sequence_parser{ 'n',  "\\n",         &verify_new_line,        &get_new_line        },
	escape_sequence_parser{ 't',  "\\t",         &verify_tab,             &get_tab             },
	escape_sequence_parser{ 'r',  "\\r",         &verify_carriage_return, &get_carriage_return },
	escape_sequence_parser{ 'x',  "\\xXX",       &verify_hex_char,        &get_hex_char        },
	escape_sequence_parser{ 'u',  "\\uXXXX",     &verify_unicode_small,   &get_unicode_small   },
	escape_sequence_parser{ 'U',  "\\UXXXXXXXX", &verify_unicode_big,     &get_unicode_big     },
};

inline bz::u8string get_available_escape_sequences_message(void)
{
	bz::u8string message = "Available escape sequences are ";
	for (auto const &parser : escape_sequence_parsers)
	{
		message += bz::format("'{}', ", parser.help);
	}
	message += "where X is a hex character";
	return message;
}

inline void verify_escape_sequence(file_iterator &stream, ctx::char_pos end, ctx::lex_context &context)
{
	if (stream.it == end)
	{
		context.bad_eof(stream, "expected an escape sequence before end-of-file");
		return;
	}
	auto const c = *stream.it;
	bool good = false;
	// the generated code should be the same as with a switch:
	// https://godbolt.org/z/b6WM5b
	[&]<size_t ...Ns>(bz::meta::index_sequence<Ns...>) {
		((
			c == escape_sequence_parsers[Ns].c
			? (void)((good = true), escape_sequence_parsers[Ns].verify(stream, end, context))
			: (void)0
		), ...);
	}(bz::meta::make_index_sequence<escape_sequence_parsers.size()>{});

	if (!good)
	{
		static_assert('\\' <= 127);
		auto const escape_char = ctx::char_pos(stream.it.data() - 1);
		auto const get_char = [](bz::u8char c) -> bz::u8string {
			if (c >= ' ')
			{
				return bz::u8string(1, c);
			}
			else
			{
				switch (c)
				{
				case '\t': return bz::format("{}\\t{}", colors::bright_black, colors::clear);
				case '\n': return bz::format("{}\\n{}", colors::bright_black, colors::clear);
				case '\r': return bz::format("{}\\r{}", colors::bright_black, colors::clear);
				default:
					return bz::format(
						"{}\\x{:02x}{}",
						colors::bright_black,
						static_cast<uint32_t>(c),
						colors::clear
					);
				}
			}
		};

		bz::vector<ctx::note> notes = {};
		if (do_verbose)
		{
			notes.push_back(context.make_note(
				stream.file_id, stream.line,
				get_available_escape_sequences_message()
			));
		}

		context.bad_chars(
			stream.file_id, stream.line,
			escape_char, escape_char, stream.it + 1,
			bz::format("invalid escape sequence '\\{}'", get_char(c)),
			std::move(notes), { context.make_suggestion(
				stream.file_id, stream.line,
				escape_char, "\\", "did you mean to escape the backslash?"
			) }
		);
		++stream;
	}
}

inline bz::u8char get_escape_sequence(bz::u8iterator &it)
{
	// generated code should be the same as with a switch:
	// https://godbolt.org/z/b6WM5b
	auto const c = *it;
	return [&]<size_t ...Ns>(bz::meta::index_sequence<Ns...>) {
		bz::u8char result = 0;
		((
			c == escape_sequence_parsers[Ns].c
			? (void)(result = escape_sequence_parsers[Ns].get(it))
			: (void)0
		), ...);
		return result;
	}(bz::meta::make_index_sequence<escape_sequence_parsers.size()>{});
	// no error reporting needed here
}

#endif // ESCAPE_SEQUENCES_H
