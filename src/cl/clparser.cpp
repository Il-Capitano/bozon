#include "clparser.h"

namespace cl
{

bz::result<bool, bz::u8string> arg_parser<bool>::parse(bz::u8string_view arg)
{
	if (arg == "true")
	{
		return true;
	}
	else if (arg == "false")
	{
		return false;
	}
	else
	{
		return bz::format("invalid bool input '{}'", arg);
	}
}

bz::result<bz::u8string_view, bz::u8string> arg_parser<bz::u8string>::parse(bz::u8string_view arg)
{
	return arg;
}

bz::result<size_t, bz::u8string> arg_parser<size_t>::parse(bz::u8string_view arg)
{
	size_t result = 0;
	for (auto const c : arg)
	{
		if (c >= '0' && c <= '9')
		{
			result *= 10;
			result += c - '0';
		}
		else
		{
			return bz::format("invalid number input '{}'", arg);
		}
	}
	return result;
}

bz::vector<bz::u8string_view> get_args(int argc, char const **argv)
{
	bz::vector<bz::u8string_view> result;
	result.reserve(static_cast<size_t>(argc));
	for (int i = 0; i < argc; ++i)
	{
		result.emplace_back(argv[i]);
	}
	return result;
}

bool alphabetical_compare(bz::u8string_view lhs, bz::u8string_view rhs)
{
	auto lhs_it = lhs.begin();
	auto rhs_it = rhs.begin();
	auto const lhs_end = lhs.end();
	auto const rhs_end = rhs.end();

	auto const is_upper = [](bz::u8char c) {
		return c >= 'A' && c <= 'Z';
	};
	auto const to_upper = [](bz::u8char c) {
		if (c >= 'a' && c <= 'z')
		{
			static_assert('a' > 'A');
			return c - ('a' - 'A');
		}
		else
		{
			return c;
		}
	};

	for (; lhs_it != lhs_end && rhs_it != rhs_end; ++lhs_it, ++rhs_it)
	{
		auto const lhs_c = to_upper(*lhs_it);
		auto const rhs_c = to_upper(*rhs_it);
		auto const lhs_is_upper = is_upper(*lhs_it);
		auto const rhs_is_upper = is_upper(*rhs_it);

		if (lhs_c < rhs_c || (lhs_c == rhs_c && !lhs_is_upper && rhs_is_upper))
		{
			return true;
		}
		else if (lhs_c > rhs_c || (lhs_c == rhs_c && lhs_is_upper && !rhs_is_upper))
		{
			return false;
		}
		bz_assert(lhs_c == rhs_c && lhs_is_upper == rhs_is_upper);
	}

	return lhs_it == lhs_end && rhs_it != rhs_end;
}

static bz::vector<bz::u8string_view> split_words(bz::u8string_view str)
{
	bz::vector<bz::u8string_view> result = {};
	auto it = str.begin();
	auto const end = str.end();
	while (it != end)
	{
		auto const next_space = str.find(it, ' ');
		result.emplace_back(it, next_space);
		if (next_space == end)
		{
			break;
		}

		it = next_space + 1;
	}

	return result;
}

static bz::u8string format_long_help_string(
	bz::u8string_view help_str,
	size_t initial_indent_width,
	size_t usage_width,
	size_t column_limit
)
{
	auto const next_line_indent_width = initial_indent_width + usage_width;
	auto const help_str_width = column_limit - next_line_indent_width;
	auto const indentation = bz::format("{:{}}", next_line_indent_width, "");
	bz_assert(help_str.length() > help_str_width);
	auto const words = split_words(help_str);

	bz::u8string result = "";
	size_t column = 0;
	bool first = true;
	for (auto const word : words)
	{
		bz_assert(column <= help_str_width);
		auto const len = word.length();
		// -1 because of the space in the front
		if (column != 0 && len + column > help_str_width - 1)
		{
			result += '\n';
			result += indentation;
			column = 0;
		}
		else if (!first)
		{
			result += ' ';
			column += 1;
		}

		if (len > help_str_width)
		{
			auto const lines_count = len / help_str_width + 1;
			auto const last_column = len % help_str_width;
			column = last_column;
			for (size_t i = 0; i < lines_count; ++i)
			{
				if (i != 0)
				{
					result += '\n';
					result += indentation;
				}
				result += word.substring(i * help_str_width, (i + 1) * help_str_width);
			}
		}
		else
		{
			result += word;
			column += len;
		}

		first = false;
	}

	return result;
}

bz::u8string get_help_string(
	bz::array_view<bz::u8string const> usages,
	bz::array_view<bz::u8string const> helps,
	size_t initial_indent_width,
	size_t usage_width,
	size_t column_limit
)
{
	auto const initial_indent = bz::format("{:{}}", initial_indent_width, "");

	bz::u8string result = "";

	bz_assert(usages.size() == helps.size());
	for (auto const &[usage, help] : bz::zip(usages, helps))
	{
		auto const formatted_help = help.length() > (column_limit - usage_width - initial_indent_width)
			? format_long_help_string(help, initial_indent_width, usage_width, column_limit)
			: help;

		if (usage.length() >= usage_width)
		{
			result += initial_indent;
			result += usage;
			result += bz::format("\n{:{}}{}\n", initial_indent_width + usage_width, "", formatted_help);
		}
		else
		{
			result += bz::format("{}{:{}}{}\n", initial_indent, usage_width, usage, help);
		}
	}

	return result;
}

} // namespace cl
