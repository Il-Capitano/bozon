#ifndef CL_CLPARSER_H
#define CL_CLPARSER_H

#include <bz/u8string_view.h>
#include <bz/vector.h>
#include <bz/array_view.h>
#include <bz/optional.h>
#include <bz/format.h>
#include <bz/result.h>
#include <array>

namespace cl
{

using iter_t = bz::array_view<bz::u8string_view const>::const_iterator;

using parse_fn_t = bz::optional<bz::u8string> (*)(iter_t begin, iter_t end, iter_t &stream);

enum class parser_kind
{
	flag,
	prefix,
	argument,
};

struct parser
{
	parser_kind       kind{};

	bz::u8string_view flag_name{};
	bz::u8string_view alternate_flag_name{};
	bz::u8string_view usage{};
	bz::u8string_view help{};
	parse_fn_t        parse_fn{};
	bool              hidden{};
};

template<typename T>
struct arg_parser;

template<>
struct arg_parser<bool>
{
	static bz::result<bool, bz::u8string> parse(bz::u8string_view arg);
};

template<>
struct arg_parser<bz::u8string>
{
	static bz::result<bz::u8string_view, bz::u8string> parse(bz::u8string_view arg);
};

template<>
struct arg_parser<size_t>
{
	static bz::result<size_t, bz::u8string> parse(bz::u8string_view arg);
};

template<auto *output>
constexpr bool is_array_like = false;

namespace internal
{

constexpr bool is_valid_flag_char(bz::u8char c)
{
	return c > ' '
		&& c != ','
		&& c != '='
		&& c != '|'
		&& c != '<' && c != '>'
		&& c != '[' && c != ']'
		&& c != '(' && c != ')'
		&& c != '{' && c != '}';
}

template<bool *output>
bz::optional<bz::u8string> default_flag_parser(iter_t begin, iter_t end, iter_t &stream)
{
	static_assert(!is_array_like<output>);
	bz_assert(stream != end);
	static iter_t init_iter = nullptr;
	if (init_iter == nullptr)
	{
		init_iter = stream;
		++stream;
		*output = true;
		return {};
	}
	else
	{
		auto const flag_iter = stream;
		++stream;
		return bz::format("option '{}' has already been set by argument '{}', at position {}", *flag_iter, *init_iter, init_iter - begin);
	}
};

constexpr void consume_flag_name(bz::u8iterator &it, bz::u8iterator end)
{
	while (it != end && is_valid_flag_char(*it))
	{
		++it;
	}
}

constexpr void consume_value(bz::u8iterator &it, bz::u8iterator end)
{
	bz_assert(it != end && (*it == '<' || *it == '{'), "value doesn't start with '<' or '{'");
	if (*it == '<')
	{
		++it; // '<'
		consume_flag_name(it, end);
		bz_assert(it != end && *it == '>', "value doesn't end with '>'");
		++it; // '>'
	}
	else // if (*it == '{')
	{
		++it; // '{'
		do
		{
			auto const val_begin = it;
			consume_flag_name(it, end);
			auto const val_end = it;
			auto const val = bz::u8string_view(val_begin, val_end);
			bz_assert(bz::constexpr_not_equals(val, ""), "value option must not be an empty string");
		} while (it != end && *it == '|' && ((void)++it, true));
		bz_assert(it != end && *it == '}', "value doesn't end with '}'");
		++it; // '}'
	}
}

// -f <value>
// --flag-name <value>
// --flag-name {val1|val2|val3}
constexpr void check_argument_parser_syntax(bz::u8string_view usage)
{
	auto it = usage.begin();
	auto const end = usage.end();

	bz_assert(it != end && *it == '-', "usage doesn't follow the syntax --flag-name <value> (flag name doesn't start with '-')");
	consume_flag_name(it, end);

	bz_assert(it != end && *it == ' ', "usage doesn't follow the syntax --flag-name <value> (no space after '--flag-name')");
	++it; // ' '
	consume_value(it, end);
	bz_assert(it == end, "usage doesn't follow the syntax --flag-name <value> (usage doesn't end after '>')");
}

template<auto *output>
bz::optional<bz::u8string> default_argument_parser(iter_t begin, iter_t end, iter_t &stream)
{
	bz_assert(stream != end);
	if constexpr (is_array_like<output>)
	{
		auto const flag_name = *stream;
		++stream;
		if (stream == end)
		{
			return bz::format("expected an argument after '{}'", flag_name);
		}
		auto const arg = *stream;
		++stream;
		auto const result = arg_parser<typename bz::meta::remove_reference<decltype(*output)>::value_type>::parse(arg);
		if (result.has_error())
		{
			return result.get_error();
		}
		else
		{
			output->emplace_back(std::move(result.get_result()));
			return {};
		}
	}
	else
	{
		static iter_t init_iter = nullptr;
		auto const flag_iter = stream;
		if (init_iter == nullptr)
		{
			auto const flag_name = *stream;
			++stream;
			if (stream == end)
			{
				return bz::format("expected an argument after '{}'", flag_name);
			}
			auto const arg = *stream;
			++stream;
			auto const result = arg_parser<bz::meta::remove_reference<decltype(*output)>>::parse(arg);
			if (result.has_error())
			{
				return result.get_error();
			}
			else
			{
				init_iter = flag_iter;
				*output = std::move(result.get_result());
				return {};
			}
		}
		else
		{
			++stream;
			if (stream != end)
			{
				++stream;
			}
			return bz::format("option '{}' has already been set by argument '{}', at position {}", *flag_iter, *init_iter, init_iter - begin);
		}
	}
}

template<
	auto *output,
	bz::optional<bz::meta::remove_pointer<decltype(output)>> (*value_parser)(bz::u8string_view value)
>
bz::optional<bz::u8string> choice_argument_parser(iter_t begin, iter_t end, iter_t &stream)
{
	bz_assert(stream != end);
	if constexpr (is_array_like<output>)
	{
		auto const flag_name = *stream;
		++stream;
		if (stream == end)
		{
			return bz::format("expected an argument after '{}'", flag_name);
		}
		auto const arg = *stream;
		++stream;
		auto const result = value_parser(arg);
		if (result.has_value())
		{
			output->emplace_back(std::move(*result));
			return {};
		}
		else
		{
			return bz::format("invalid argument '{}' for '{}'", arg, flag_name);
		}
	}
	else
	{
		static iter_t init_iter = nullptr;
		auto const flag_iter = stream;
		if (init_iter == nullptr)
		{
			auto const flag_name = *stream;
			++stream;
			if (stream == end)
			{
				return bz::format("expected an argument after '{}'", flag_name);
			}
			auto const arg = *stream;
			++stream;
			auto const result = value_parser(arg);
			if (result.has_value())
			{
				init_iter = flag_iter;
				*output = std::move(*result);
				return {};
			}
			else
			{
				return bz::format("invalid argument '{}' for '{}'", arg, flag_name);
			}
		}
		else
		{
			++stream;
			if (stream != end)
			{
				++stream;
			}
			return bz::format("option '{}' has already been set by argument '{}', at position {}", *flag_iter, *init_iter, init_iter - begin);
		}
	}
}

// -f=<value>
// --flag-name=<value>
// --flag-name={val1|val2|val3}
constexpr void check_equals_parser_syntax(bz::u8string_view usage)
{
	auto it = usage.begin();
	auto const end = usage.end();

	bz_assert(it != end && *it == '-', "usage doesn't follow the syntax --flag-name=<value> (flag name doesn't start with '-')");
	consume_flag_name(it, end);

	bz_assert(it != end && *it == '=', "usage doesn't follow the syntax --flag-name=<value> (missing '=' after '--flag-name')");
	++it; // '='
	consume_value(it, end);
	bz_assert(it == end, "usage doesn't follow the syntax --flag-name=<value> (usage doesn't end after '>')");
}

template<auto *output>
bz::optional<bz::u8string> default_equals_parser(iter_t begin, iter_t end, iter_t &stream)
{
	bz_assert(stream != end);
	if constexpr (is_array_like<output>)
	{
		auto const stream_value = *stream;
		++stream;
		auto const it = stream_value.find('=') + 1;
		auto const arg = bz::u8string_view(it, stream_value.end());
		if (arg == "")
		{
			return bz::format("expected an argument after '{}'", bz::u8string_view(stream_value.begin(), it));
		}
		auto const result = arg_parser<typename bz::meta::remove_reference<decltype(*output)>::value_type>::parse(arg);
		if (result.has_error())
		{
			return result.get_error();
		}
		else
		{
			output->emplace_back(std::move(result.get_result()));
			return {};
		}
	}
	else
	{
		static iter_t init_iter = nullptr;
		auto const flag_iter = stream;
		if (init_iter == nullptr)
		{
			auto const stream_value = *stream;
			++stream;
			auto const it = stream_value.find('=') + 1;
			auto const arg = bz::u8string_view(it, stream_value.end());
			if (arg == "")
			{
				return bz::format("expected an argument after '{}'", bz::u8string_view(stream_value.begin(), it));
			}
			auto const result = arg_parser<bz::meta::remove_reference<decltype(*output)>>::parse(arg);
			if (result.has_error())
			{
				return result.get_error();
			}
			else
			{
				init_iter = flag_iter;
				*output = std::move(result.get_result());
				return {};
			}
		}
		else
		{
			auto const flag_name = bz::u8string_view(flag_iter->begin(), flag_iter->find('='));
			++stream;
			return bz::format("option '{}' has already been set by argument '{}', at position {}", flag_name, *init_iter, init_iter - begin);
		}
	}
}

template<
	auto *output,
	bz::optional<bz::meta::remove_pointer<decltype(output)>> (*value_parser)(bz::u8string_view value)
>
bz::optional<bz::u8string> choice_equals_parser(iter_t begin, iter_t end, iter_t &stream)
{
	bz_assert(stream != end);
	if constexpr (is_array_like<output>)
	{
		auto const stream_value = *stream;
		++stream;
		auto const it = stream_value.find('=');
		auto const flag_name = bz::u8string_view(stream_value.begin(), it);
		auto const arg = bz::u8string_view(it + 1, stream_value.end());
		if (arg == "")
		{
			return bz::format("expected an argument after '{}'", bz::u8string_view(stream_value.begin(), it + 1));
		}
		auto const result = value_parser(arg);
		if (result.has_value())
		{
			output->emplace_back(std::move(*result));
			return {};
		}
		else
		{
			return bz::format("invalid argument '{}' for '{}'", arg, flag_name);
		}
	}
	else
	{
		static iter_t init_iter = nullptr;
		auto const flag_iter = stream;
		if (init_iter == nullptr)
		{
			auto const stream_value = *stream;
			++stream;
			auto const it = stream_value.find('=');
			auto const flag_name = bz::u8string_view(stream_value.begin(), it);
			auto const arg = bz::u8string_view(it + 1, stream_value.end());
			if (arg == "")
			{
				return bz::format("expected an argument after '{}'", bz::u8string_view(stream_value.begin(), it + 1));
			}
			auto const result = value_parser(arg);
			if (result.has_value())
			{
				init_iter = flag_iter;
				*output = std::move(*result);
				return {};
			}
			else
			{
				return bz::format("invalid argument '{}' for '{}'", arg, flag_name);
			}
		}
		else
		{
			auto const flag_name = bz::u8string_view(flag_iter->begin(), flag_iter->find('='));
			++stream;
			return bz::format("option '{}' has already been set by argument '{}', at position {}", flag_name, *init_iter, init_iter - begin);
		}
	}
}

constexpr void check_multiple_flag_parser_syntax(bz::u8string_view usage)
{
	bz_assert(usage.starts_with("-"), "usage doesn't follow the syntax '-f, --flag-name' or '-f, --flag-name <value>' (usage doesn't start with '-')");
	bz_assert(!usage.starts_with("--"), "usage doesn't follow the syntax '-f, --flag-name' or '-f, --flag-name <value>' (usage starts with '--')");
	auto it = usage.begin();
	auto const end = usage.end();
	++it; // '-'

	bz_assert(it != end && is_valid_flag_char(*it), "usage doesn't follow the syntax '-f, --flag-name' or '-f, --flag-name <value>' (missing character after '-')");
	++it;
	bz_assert(it != end && *it == ',', "usage doesn't follow the syntax '-f, --flag-name' or '-f, --flag-name <value>' (missing ',' after first flag name)");
	++it;
	bz_assert(it != end && *it == ' ', "usage doesn't follow the syntax '-f, --flag-name' or '-f, --flag-name <value>' (missing ' ' after ',')");
	++it;
	bz_assert(it != end && *it == '-', "usage doesn't follow the syntax '-f, --flag-name' or '-f, --flag-name <value>' (missing the second flag)");
	++it;
	bz_assert(it != end && *it == '-', "usage doesn't follow the syntax '-f, --flag-name' or '-f, --flag-name <value>' (missing second '-' in second flag)");
	++it;

	consume_flag_name(it, end);

	if (it == end)
	{
		return;
	}

	bz_assert(it != end && *it == ' ', "usage doesn't follow the syntax '-f, --flag-name <value>' (missing ' ' after second flag name)");
	++it;
	consume_value(it, end);
	bz_assert(it == end, "usage doesn't follow the syntax '-f, --flag-name <value>' (usage doesn't end after '>')");
}

constexpr std::pair<bz::u8string_view, bz::u8string_view> get_multiple_flag_names(bz::u8string_view usage)
{
	auto it = usage.begin();
	auto const end = usage.end();

	auto const first_flag_name_begin = it;
	while (it != end && is_valid_flag_char(*it))
	{
		++it;
	}
	bz_assert(it != end);
	auto const first_flag_name = bz::u8string_view(first_flag_name_begin, it);

	bz_assert(*it == ',');
	++it;
	bz_assert(it != end && *it == ' ');
	++it;
	bz_assert(it != end && *it == '-');

	auto const second_flag_name_begin = it;
	while (it != end && is_valid_flag_char(*it))
	{
		++it;
	}
	auto const second_flag_name = bz::u8string_view(second_flag_name_begin, it);

	return { first_flag_name, second_flag_name };
}

constexpr bool is_multiple_flag_argument(bz::u8string_view usage)
{
	auto const [first, second] = get_multiple_flag_names(usage);
	return usage.length() > (first.length() + second.length() + 2);
}

constexpr bool is_choice_flag(bz::u8string_view usage)
{
	return usage.contains('{');
}

constexpr size_t get_choice_count(bz::u8string_view usage)
{
	return usage.count_chars('|') + 1;
}

} // namespace internal

// syntax:
// -f
// --flag-name             simple bool flag
// -f <value>
// --flag-name <value>     argument flag
// -f=<value>
// --flag-name=<value>     equals flag
// -f, --flag-name         multiple option simple bool flag
// -f, --flag-name <value> multiple option argument flag

template<
	auto *output,
	bz::optional<bz::meta::remove_pointer<decltype(output)>> (*choice_parse_fn)(bz::u8string_view value) = nullptr
>
constexpr parser create_parser(bz::u8string_view usage, bz::u8string_view help, bool hidden = false)
{
	static_assert(output != nullptr);
	bz_assert(usage.starts_with("-"));

	auto const begin = usage.begin();
	auto const end = usage.end();
	auto it = begin;
	++it; // '-'
	bz_assert(it != end);
	if (*it == '-')
	{
		++it;
		bz_assert(it != end && internal::is_valid_flag_char(*it) && *it != '-', "expected a flag character after '--'");
	}
	else
	{
		bz_assert(internal::is_valid_flag_char(*it), "expected a flag character after '-'");
		bz_assert(it + 1 == end || !internal::is_valid_flag_char(*(it + 1)), "flag starting with a single '-' must be one character long");
	}

	while (it != end && internal::is_valid_flag_char(*it))
	{
		++it;
	}

	if (it == end)
	{
		// assert that the output is bool
		bz_assert((bz::meta::is_same<decltype(output), bool *>), "a flag with no argument must have an output type of 'bool'");
		if constexpr (bz::meta::is_same<decltype(output), bool *>)
		{
			return parser{ parser_kind::flag, usage, "", usage, help, &internal::default_flag_parser<output>, hidden };
		}
	}

	switch (*it)
	{
	case ' ':
	{
		internal::check_argument_parser_syntax(usage);
		auto const flag_name = bz::u8string_view(begin, it);
		if constexpr (choice_parse_fn != nullptr)
		{
			bz_assert(internal::is_choice_flag(usage));
			auto const parse_fn = &internal::choice_argument_parser<output, choice_parse_fn>;
			return parser{ parser_kind::argument, flag_name, "", usage, help, parse_fn, hidden };
		}
		else
		{
			bz_assert(!internal::is_choice_flag(usage));
			auto const parse_fn = &internal::default_argument_parser<output>;
			return parser{ parser_kind::argument, flag_name, "", usage, help, parse_fn, hidden };
		}
	}
	case '=':
	{
		++it;
		internal::check_equals_parser_syntax(usage);
		auto const prefix_name = bz::u8string_view(begin, it);
		if constexpr (choice_parse_fn != nullptr)
		{
			bz_assert(internal::is_choice_flag(usage));
			auto const parse_fn = &internal::choice_equals_parser<output, choice_parse_fn>;
			return parser{ parser_kind::prefix, prefix_name, "", usage, help, parse_fn, hidden };
		}
		else
		{
			bz_assert(!internal::is_choice_flag(usage));
			auto const parse_fn = &internal::default_equals_parser<output>;
			return parser{ parser_kind::prefix, prefix_name, "", usage, help, parse_fn, hidden };
		}
	}
	case ',':
	{
		internal::check_multiple_flag_parser_syntax(usage);
		auto const [first_flag_name, second_flag_name] = internal::get_multiple_flag_names(usage);
		auto const is_argument = internal::is_multiple_flag_argument(usage);

		if (is_argument)
		{
			if constexpr (choice_parse_fn != nullptr)
			{
				bz_assert(internal::is_choice_flag(usage));
				auto const parse_fn = &internal::choice_argument_parser<output, choice_parse_fn>;
				return parser{ parser_kind::argument, first_flag_name, second_flag_name, usage, help, parse_fn, hidden };
			}
			else
			{
				bz_assert(!internal::is_choice_flag(usage));
				auto const parse_fn = &internal::default_argument_parser<output>;
				return parser{ parser_kind::argument, first_flag_name, second_flag_name, usage, help, parse_fn, hidden };
			}
		}
		else
		{
			bz_assert((bz::meta::is_same<decltype(output), bool *>), "a flag with no argument must have an output type of 'bool'");
			if constexpr (bz::meta::is_same<decltype(output), bool *>)
			{
				auto const parse_fn = &internal::default_flag_parser<output>;
				return parser{ parser_kind::flag, first_flag_name, second_flag_name, usage, help, parse_fn, hidden };
			}
			else
			{
				bz_unreachable;
			}
		}
	}
	default:
		bz_unreachable;
	}
}

struct group_element_t
{
	bz::u8string_view            flag_name{};
	bz::u8string_view            usage{};
	bz::u8string_view            help{};
	size_t                       index;
	bz::array_view<size_t const> enable_group{};
	parse_fn_t                   parse_fn;
};

// syntax:
// item-name    simple bool flag
constexpr group_element_t create_group_element(bz::u8string_view usage, bz::u8string_view help, size_t index)
{
	bz_assert(bz::constexpr_not_equals(usage, "help"), "'help' is reserved in groups");
	auto it = usage.begin();
	auto const end = usage.end();
	bz_assert(it != end, "usage doesn't follow the syntax 'item-name' (usage is empty)");
	while (it != end && internal::is_valid_flag_char(*it))
	{
		++it;
	}
	bz_assert(it == end, "item doesn't follow the syntax 'item-name' (usage doesn't end after name)");
	return group_element_t{ usage, usage, help, index, {}, nullptr };
}

// syntax:
// item-name    simple bool flag
template<size_t N>
constexpr group_element_t create_group_element(bz::u8string_view usage, bz::u8string_view help, std::array<size_t, N> const &enable_group)
{
	bz_assert(bz::constexpr_not_equals(usage, "help"), "'help' is reserved in groups");
	auto it = usage.begin();
	auto const end = usage.end();
	bz_assert(it != end, "usage doesn't follow the syntax 'item-name' (usage is empty)");
	while (it != end && internal::is_valid_flag_char(*it))
	{
		++it;
	}
	bz_assert(it == end, "item doesn't follow the syntax 'item-name' (usage doesn't end after name)");
	return group_element_t{ usage, usage, help, size_t(-1), { enable_group.data(), enable_group.data() + enable_group.size() }, nullptr };
}

// syntax:
// item-name=<value>    equals flag
template<auto *output>
constexpr group_element_t create_group_element(bz::u8string_view usage, bz::u8string_view help)
{
	auto const begin = usage.begin();
	auto const end   = usage.end();
	auto it = begin;
	bz_assert(it != end, "usage doesn't follow the syntax 'item-name=<value>' (usage is empty)");
	while (it != end && internal::is_valid_flag_char(*it))
	{
		++it;
	}
	auto const item_name = bz::u8string_view(begin, it);
	bz_assert(bz::constexpr_not_equals(item_name, "help"), "'help' is reserved in groups");
	bz_assert(it != end && *it == '=', "usage doesn't follow the syntax 'item-name=<value>' (missing '=' after item name)");
	++it;
	bz_assert(it != end && *it == '<', "usage doesn't follow the syntax 'item-name=<value>' (missing '<' after '=')");
	++it;
	while (it != end && internal::is_valid_flag_char(*it))
	{
		++it;
	}
	bz_assert(it != end && *it == '>', "usage doesn't follow the syntax 'item-name=<value>' (missing '>' after value name)");
	++it;
	bz_assert(it == end, "usage doesn't follow the syntax 'item-name=<value>' (usage doesn't end after '>')");
	auto const parse_fn = +[](iter_t begin, iter_t end, iter_t &stream) -> bz::optional<bz::u8string> {
		bz_assert(stream != end);
		static iter_t init_iter = nullptr;
		auto const flag_iter = stream;
		if (init_iter == nullptr)
		{
			auto const stream_value = *stream;
			++stream;
			auto const it = stream_value.find('=') + 1;
			auto const arg = bz::u8string_view(it, stream_value.end());
			if (arg == "")
			{
				auto const flag_name = bz::u8string_view(stream_value.begin() + 2, it);
				return bz::format("expected an argument after '{}'", flag_name);
			}
			auto const result = arg_parser<bz::meta::remove_reference<decltype(*output)>>::parse(arg);
			if (result.has_error())
			{
				return result.get_error();
			}
			else
			{
				*output = result.get_result();
				init_iter = flag_iter;
				return {};
			}
		}
		else
		{
			auto const flag_name = bz::u8string_view(flag_iter->begin() + 2, flag_iter->find('='));
			++stream;
			return bz::format("option '{}' has already been set by argument '{}', at position {}", flag_name, *init_iter, init_iter - begin);
		}
	};
	return group_element_t{ item_name, usage, help, size_t(-1), {}, parse_fn };
}

namespace internal
{

template<typename T>
constexpr bool is_array = false;

template<typename T, size_t N>
constexpr bool is_array<std::array<T, N>> = true;

} // namespace internal

// syntax:
// -F<item>           simple bool flag enabler
// -F<item>=<value>   items with a <value> argument
// -F<item>[=<value>] only some items have <value> argument
// -Fno-<item>        disabler (only bool flags)
template<auto &group_, auto &bool_output_, bool *help_out>
constexpr parser create_group_parser(bz::u8string_view usage, bz::u8string_view help, bool hidden = false)
{
	using group_t = bz::meta::remove_cv_reference<decltype(group_)>;
	using bool_output_t = bz::meta::remove_reference<decltype(bool_output_)>;
	static_assert(internal::is_array<group_t>);
	static_assert(internal::is_array<bool_output_t>);
	static_assert(bz::meta::is_same<group_t, std::array<group_element_t, group_.size()>>);
	static_assert(bz::meta::is_same<bool_output_t, std::array<bool, bool_output_.size()>>);

	group_t const &group = group_;
	bool_output_t &bool_output = bool_output_;

	constexpr auto group_bool_count = [&]() {
		size_t count = 0;
		for (auto const &g : group)
		{
			if (g.enable_group.empty() && g.parse_fn == nullptr)
			{
				++count;
			}
		}
		return count;
	}();
	constexpr auto group_equals_count = [&]() {
		size_t count = 0;
		for (auto const &g : group)
		{
			if (g.parse_fn != nullptr)
			{
				++count;
			}
		}
		return count;
	}();
	static_assert(group_bool_count == bool_output.size(), "number of bool group flags doesn't match the output array size");

	auto const begin = usage.begin();
	auto const end = usage.end();
	auto it = begin;
	bz_assert(it != end && *it == '-', "flag doesn't follow the syntax '-f<item>' (flag doesn't start with '-')");
	++it;
	bz_assert(it != end && [](bz::u8char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
	}(*it), "flag doesn't follow the syntax '-f<item>' (second character must be a letter)");
	++it;
	bz_assert(it != end && *it == '<', "flag doesn't follow the syntax '-f<item>' (missing '<')");
	++it;
	while (it != end && internal::is_valid_flag_char(*it))
	{
		++it;
	}
	bz_assert(it != end && *it == '>', "flag doesn't follow the syntax '-f<item>' (missing '>')");
	++it;
	if constexpr (group_bool_count == 0)
	{
		static_assert(group_equals_count != 0);
		bz_assert(it != end && *it == '=', "flag doesn't follow the syntax '-f<item>=<value>' (missing '=')");
		++it;
		bz_assert(it != end && *it == '<', "flag doesn't follow the syntax '-f<item>=<value>' (missing '<' after '=')");
		++it;
		while (it != end && internal::is_valid_flag_char(*it))
		{
			++it;
		}
		bz_assert(it != end && *it == '>', "flag doesn't follow the syntax '-f<item>=<value>' (missing '>' after value name)");
		++it;
		bz_assert(it == end, "flag doesn't follow the syntax '-f<item>=<value>' (usage doesn't end after '>')");
	}
	else if constexpr (group_equals_count != 0)
	{
		bz_assert(it != end && *it == '[', "flag doesn't follow the syntax '-f<item>[=<value>]' (missing '[')");
		++it;
		bz_assert(it != end && *it == '=', "flag doesn't follow the syntax '-f<item>[=<value>]' (missing '=')");
		++it;
		bz_assert(it != end && *it == '<', "flag doesn't follow the syntax '-f<item>[=<value>]' (missing '<' after '=')");
		++it;
		while (it != end && internal::is_valid_flag_char(*it))
		{
			++it;
		}
		bz_assert(it != end && *it == '>', "flag doesn't follow the syntax '-f<item>[=<value>]' (missing '>' after value name)");
		++it;
		bz_assert(it != end && *it == ']', "flag doesn't follow the syntax '-f<item>[=<value>]' (missing ']' after '>')");
		++it;
		bz_assert(it == end, "flag doesn't follow the syntax '-f<item>[=<value>]' (usage doesn't end after ']')");
	}
	else
	{
		bz_assert(it == end, "flag doesn't follow the syntax '-f<item>' (usage doesn't end after '>')");
	}

	auto const parse_fn = +[](iter_t begin, iter_t end, iter_t &stream) -> bz::optional<bz::u8string> {
		bz_assert(stream != end);
		auto const full_flag_val = *stream;
		auto const equals_it = full_flag_val.find('=');
		auto flag_val = bz::u8string_view(full_flag_val.begin() + 2, equals_it);
		static std::array<std::pair<int, iter_t>, bool_output.size()> enables;

		auto const modify_flag = [
			full_flag_val, equals_it,
			begin, end, &stream
		](bz::u8string_view flag_val, bool value) -> bz::optional<bz::u8string> {
			auto const it = std::find_if(
				group.begin(), group.end(),
				[flag_val](auto const &flag) {
					return flag_val == flag.flag_name;
				}
			);

			if (it == group.end())
			{
				++stream;
				return bz::format("unknown option '{}' for '{}'", flag_val, full_flag_val.substring(0, value ? 2 : 5));
			}
			else if (it->index != size_t(-1))
			{
				auto const flag_it = stream;
				++stream;
				auto const index = it->index;
				switch (enables[index].first)
				{
				case -2:
					return bz::format(
						"option '{}' has already been disabled by argument '{}', at position {}",
						flag_val, *enables[index].second, enables[index].second - begin
					);
				case -1:
					bz_unreachable;
				case  0:
				case  1:
					enables[index] = { value ? 2 : -2, flag_it };
					bool_output[index] = value;
					return {};
				case  2:
					return bz::format(
						"option '{}' has already been enabled by argument '{}', at position {}",
						flag_val, *enables[index].second, enables[index].second - begin
					);
				default:
					bz_unreachable;
				}
			}
			else if (value && it->parse_fn != nullptr)
			{
				if (equals_it == full_flag_val.end())
				{
					++stream;
					return bz::format("expected an argument for option '{}'", flag_val);
				}
				else
				{
					return it->parse_fn(begin, end, stream);
				}
			}
			else if (value)
			{
				auto const flag_it = stream;
				++stream;
				bz_assert(it->enable_group.size() > 0);
				for (auto const index : it->enable_group)
				{
					switch (enables[index].first)
					{
					case -2:
						break;
					case -1:
						bz_unreachable;
					case  0:
						enables[index] = { 1, flag_it };
						bool_output[index] = true;
						break;
					case  1:
						break;
					case  2:
						break;
					default:
						bz_unreachable;
					}
				}
				return {};
			}
			else
			{
				++stream;
				return bz::format("unknown option '{}' for '{}'", flag_val, full_flag_val.substring(0, value ? 2 : 5));
			}
		};

		if (flag_val == "help")
		{
			static iter_t help_it = nullptr;
			if (help_it == nullptr)
			{
				*help_out = true;
				help_it = stream;
				++stream;
				return {};
			}
			else
			{
				return bz::format(
					"option '{}' has already been enabled by argument '{}', at position {}",
					full_flag_val, *help_it, help_it - begin
				);
			}
		}
		else if (flag_val.starts_with("no-"))
		{
			flag_val = flag_val.substring(3);
			return modify_flag(flag_val, false);
		}
		else
		{
			return modify_flag(flag_val, true);
		}
	};
	return parser{ parser_kind::prefix, usage.substring(0, 2), "", usage, help, parse_fn, hidden };
}

template<auto &parsers>
constexpr auto create_parser_function(void)
{
	using parsers_t = bz::meta::remove_cv_reference<decltype(parsers)>;
	static_assert(internal::is_array<parsers_t>);
	static_assert(bz::meta::is_same<parsers_t, std::array<parser, parsers.size()>>);

	return +[](iter_t begin, iter_t end, iter_t &stream) -> bz::optional<bz::u8string> {
		bz::optional<bz::u8string> result{};
		auto const flag_val = *stream;
		[&]<size_t ...Ns>(bz::meta::index_sequence<Ns...>) {
			bool done = false;
			(((void)(done || ([&]() {
				if constexpr (parsers[Ns].kind == parser_kind::flag)
				{
					constexpr auto first_flag_name  = parsers[Ns].flag_name;
					constexpr auto second_flag_name = parsers[Ns].alternate_flag_name;
					constexpr auto parse_fn         = parsers[Ns].parse_fn;
					constexpr bool has_second_flag  = bz::constexpr_not_equals(second_flag_name, "");

					if (
						flag_val == first_flag_name
						|| (has_second_flag && flag_val == second_flag_name)
					)
					{
						result = parse_fn(begin, end, stream);
						done = true;
					}
				}
				else if constexpr (parsers[Ns].kind == parser_kind::prefix)
				{
					constexpr auto flag_name = parsers[Ns].flag_name;
					constexpr auto parse_fn  = parsers[Ns].parse_fn;
					static_assert(bz::constexpr_equals(parsers[Ns].alternate_flag_name, ""));

					if (flag_val.starts_with(flag_name))
					{
						result = parse_fn(begin, end, stream);
						done = true;
					}
				}
				else if constexpr (parsers[Ns].kind == parser_kind::argument)
				{
					constexpr auto first_flag_name  = parsers[Ns].flag_name;
					constexpr auto second_flag_name = parsers[Ns].alternate_flag_name;
					constexpr auto parse_fn         = parsers[Ns].parse_fn;
					constexpr bool has_second_flag  = bz::constexpr_not_equals(second_flag_name, "");

					if (
						flag_val == first_flag_name
						|| (has_second_flag && flag_val == second_flag_name)
					)
					{
						result = parse_fn(begin, end, stream);
						done = true;
					}
				}
			}(), true))), ...);
			if (!done)
			{
				result = bz::format("unknown command line option '{}'", flag_val);
				++stream;
			}
		}(bz::meta::make_index_sequence<parsers.size()>{});
		return result;
	};
}


bz::vector<bz::u8string_view> get_args(int argc, char const **argv);

bz::u8string get_help_string(
	bz::array_view<bz::u8string const> usages,
	bz::array_view<bz::u8string const> helps,
	size_t initial_indent_width,
	size_t usage_width,
	size_t column_limit
);

bool alphabetical_compare(bz::u8string_view lhs, bz::u8string_view rhs);

template<auto &parsers>
bz::u8string get_help_string(
	size_t initial_indent_width,
	size_t usage_width,
	size_t column_limit,
	bool is_verbose = false
)
{
	using parsers_t = bz::meta::remove_cv_reference<decltype(parsers)>;
	static_assert(internal::is_array<parsers_t>);
	static_assert(bz::meta::is_same<parsers_t, std::array<parser, parsers.size()>>);

	bz::vector<size_t> indicies = {};
	indicies.resize(parsers.size());
	for (size_t i = 0; i < indicies.size(); ++i)
	{
		indicies[i] = i;
	}

	std::sort(indicies.begin(), indicies.end(), [](size_t lhs, size_t rhs) {
		if (parsers[lhs].flag_name == "-h" || parsers[lhs].flag_name == "--help")
		{
			return true;
		}
		else if (parsers[rhs].flag_name == "-h" || parsers[rhs].flag_name == "--help")
		{
			return false;
		}
		auto lhs_usage = parsers[lhs].usage;
		auto rhs_usage = parsers[rhs].usage;

		if (lhs_usage.starts_with("--"))
		{
			lhs_usage = lhs_usage.substring(2);
		}
		else
		{
			bz_assert(lhs_usage.starts_with("-"));
			lhs_usage = lhs_usage.substring(1);
		}

		if (rhs_usage.starts_with("--"))
		{
			rhs_usage = rhs_usage.substring(2);
		}
		else
		{
			bz_assert(rhs_usage.starts_with("-"));
			rhs_usage = rhs_usage.substring(1);
		}

		return alphabetical_compare(lhs_usage, rhs_usage);
	});

	bz::vector<bz::u8string> usages = {};
	bz::vector<bz::u8string> helps = {};
	usages.reserve(parsers.size());
	helps.reserve(parsers.size());
	for (auto const index : indicies)
	{
		if (is_verbose || !parsers[index].hidden)
		{
			auto const usage = parsers[index].usage;
			if (usage.starts_with("--"))
			{
				usages.emplace_back(bz::format("    {}", usage));
			}
			else
			{
				usages.emplace_back(usage);
			}
			helps.emplace_back(parsers[index].help);
		}
	}

	return get_help_string(usages, helps, initial_indent_width, usage_width, column_limit);
}

template<auto &parsers>
bz::u8string get_additional_help_string(
	size_t initial_indent_width,
	size_t usage_width,
	size_t column_limit,
	bool is_verbose = false
)
{
	using parsers_t = bz::meta::remove_cv_reference<decltype(parsers)>;
	static_assert(internal::is_array<parsers_t>);
	static_assert(bz::meta::is_same<parsers_t, std::array<parser, parsers.size()>>);

	bz::vector<size_t> indicies = {};
	indicies.resize(parsers.size());
	for (size_t i = 0; i < indicies.size(); ++i)
	{
		indicies[i] = i;
	}

	std::sort(indicies.begin(), indicies.end(), [](size_t lhs, size_t rhs) {
		if (parsers[lhs].flag_name == "-h" || parsers[lhs].flag_name == "--help")
		{
			return true;
		}
		else if (parsers[rhs].flag_name == "-h" || parsers[rhs].flag_name == "--help")
		{
			return false;
		}
		auto lhs_usage = parsers[lhs].usage;
		auto rhs_usage = parsers[rhs].usage;

		if (lhs_usage.starts_with("--"))
		{
			lhs_usage = lhs_usage.substring(2);
		}
		else
		{
			bz_assert(lhs_usage.starts_with("-"));
			lhs_usage = lhs_usage.substring(1);
		}

		if (rhs_usage.starts_with("--"))
		{
			rhs_usage = rhs_usage.substring(2);
		}
		else
		{
			bz_assert(rhs_usage.starts_with("-"));
			rhs_usage = rhs_usage.substring(1);
		}

		return alphabetical_compare(lhs_usage, rhs_usage);
	});

	bz::vector<bz::u8string> usages = {};
	bz::vector<bz::u8string> helps = {};

	auto const verbose_it = std::find_if(
		parsers.begin(), parsers.end(),
		[](auto const &p) {
			return (p.flag_name == "-v" && p.alternate_flag_name == "--verbose")
				|| (p.flag_name == "-v" && p.alternate_flag_name == "")
				|| (p.flag_name == "--verbose" && p.alternate_flag_name == "");
		}
	);

	auto const has_verbose = verbose_it != parsers.end();
	auto const verbose_flag_name = has_verbose ? verbose_it->flag_name : "";

	for (auto const index : indicies)
	{
		auto const &parser = parsers[index];
		if (!is_verbose && has_verbose)
		{
			if (parser.flag_name == "-h" && parser.alternate_flag_name == "--help")
			{
				usages.emplace_back(bz::format("--help {}", verbose_flag_name));
				helps.emplace_back("Display all available options");
			}
			else if (parser.flag_name == "-h" && parser.alternate_flag_name == "")
			{
				usages.emplace_back(bz::format("-h {}", verbose_flag_name));
				helps.emplace_back("Display all available options");
			}
			else if (parser.flag_name == "--help" && parser.alternate_flag_name == "")
			{
				usages.emplace_back(bz::format("--help {}", verbose_flag_name));
				helps.emplace_back("Display all available options");
			}
		}

		if (is_verbose || !parser.hidden)
		{
			auto const usage = parser.usage;
			auto it = usage.begin();
			auto const end = usage.end();
			if (
				it != end && *it == '-' && (++it, true)
				&& it != end && internal::is_valid_flag_char(*it) && *it != '-' && (++it, true)
				&& it != end && *it == '<'
			)
			{
				auto const prefix = usage.substring(0, 2);
				usages.emplace_back(bz::format("{}help", prefix));
				helps.emplace_back(bz::format("Display available options for '{}'", prefix));
			}
		}
	}

	return get_help_string(usages, helps, initial_indent_width, usage_width, column_limit);
}

template<auto &group>
bz::u8string get_group_help_string(
	size_t initial_indent_width,
	size_t usage_width,
	size_t column_limit
)
{
	using group_t = bz::meta::remove_cv_reference<decltype(group)>;
	static_assert(internal::is_array<group_t>);
	static_assert(bz::meta::is_same<group_t, std::array<group_element_t, group.size()>>);

	bz::vector<size_t> indicies = {};
	indicies.resize(group.size());
	for (size_t i = 0; i < indicies.size(); ++i)
	{
		indicies[i] = i;
	}

	std::sort(indicies.begin(), indicies.end(), [](size_t lhs, size_t rhs) {
		auto const lhs_flag = group[lhs].usage;
		auto const rhs_flag = group[rhs].usage;
		auto const lhs_is_group = !group[lhs].enable_group.empty();
		auto const rhs_is_group = !group[rhs].enable_group.empty();

		if (lhs_is_group == rhs_is_group)
		{
			return alphabetical_compare(lhs_flag, rhs_flag);
		}
		else if (lhs_is_group)
		{
			return true;
		}
		else
		{
			return false;
		}
	});

	bool is_group = !group[indicies.front()].enable_group.empty();
	bz::vector<bz::u8string> usages = {};
	bz::vector<bz::u8string> helps = {};
	usages.reserve(group.size() + (is_group ? 1 : 0));
	helps.reserve(group.size() + (is_group ? 1 : 0));

	for (auto const index : indicies)
	{
		if (is_group && group[index].enable_group.empty())
		{
			is_group = false;
			usages.emplace_back("");
			helps.emplace_back("");
		}
		usages.emplace_back(group[index].usage);
		helps.emplace_back(group[index].help);
	}

	return get_help_string(usages, helps, initial_indent_width, usage_width, column_limit);
}

} // namespace cl

#endif // CL_CLPARSER_H
