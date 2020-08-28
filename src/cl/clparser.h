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
	multiple_flag,
	prefix,
	argument,
	group,
};

struct parser
{
	parser_kind       kind{};

	bz::u8string_view flag_name{};
	bz::u8string_view alternate_flag_name{};
	bz::u8string_view usage{};
	bz::u8string_view help{};
	parse_fn_t        parse_fn{};
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

template<bool *output>
bz::optional<bz::u8string> default_flag_parser(iter_t begin, iter_t end, iter_t &stream)
{
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

// syntax:
// --flag-name             a simple bool flag
// --flag-name=<value>     equals flag
// --flag-name <value>
// -h, --help              multiple option simple bool flag
template<auto *output>
constexpr parser create_parser(bz::u8string_view usage, bz::u8string_view help)
{
	bz_assert(usage.starts_with("-"));
	auto const is_valid_char = [](bz::u8char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-';
	};

	auto const begin = usage.begin();
	auto const end = usage.end();
	auto it = begin;
	++it;
	bz_assert(it != end);
	if (*it == '-')
	{
		++it;
	}

	while (it != end && is_valid_char(*it))
	{
		++it;
	}

	if (it == end)
	{
		if constexpr (bz::meta::is_same<decltype(output), bool *>)
		{
			// assert that the output is bool
			bz_assert((bz::meta::is_same<decltype(output), bool *>));
			return parser{ parser_kind::flag, usage, "", usage, help, &default_flag_parser<output> };
		}
		else
		{
			bz_unreachable;
		}
	}

	switch (*it)
	{
	case ' ':
	{
		auto const flag_name = bz::u8string_view(begin, it);
		++it;
		bz_assert(it != end && *it == '<', "flag doesn't follow the syntax '--flag-name <value>' (missing '<')");
		++it;
		while (it != end && is_valid_char(*it))
		{
			++it;
		}
		bz_assert(it != end && *it == '>', "flag doesn't follow the syntax '--flag-name <value>' (missing '>')");
		++it;
		bz_assert(it == end, "flag doesn't follow the syntax '--flag-name <value>' (string doesn't end after '>')");
		auto const parse_fn = +[](iter_t begin, iter_t end, iter_t &stream) -> bz::optional<bz::u8string> {
			bz_assert(stream != end);
			static iter_t init_iter = nullptr;
			if (init_iter == nullptr)
			{
				init_iter = stream;
				auto const flag_name = *stream;
				++stream;
				if (stream == end)
				{
					return bz::format("expected an argument after '{}'", flag_name);
				}
				auto const arg = *stream;
				auto const result = arg_parser<bz::meta::remove_reference<decltype(*output)>>::parse(arg);
				if (result.has_error())
				{
					return result.get_error();
				}
				else
				{
					*output = result.get_result();
					return {};
				}
			}
			else
			{
				auto const flag_iter = stream;
				++stream;
				if (stream != end)
				{
					++stream;
				}
				return bz::format("option '{}' has already been set by argument '{}', at position {}", *flag_iter, *init_iter, init_iter - begin);
			}
		};
		return parser{ parser_kind::argument, flag_name, "", usage, help, parse_fn };
	}
	case '=':
	{
		++it;
		auto const prefix_name = bz::u8string_view(begin, it);
		bz_assert(it != end && *it == '<', "flag doesn't follow the syntax '--flag-name=<value>' (missing '<')");
		++it;
		while (it != end && is_valid_char(*it))
		{
			++it;
		}
		bz_assert(it != end && *it == '>', "flag doesn't follow the syntax '--flag-name=<value>' (missing '>')");
		++it;
		bz_assert(it == end, "flag doesn't follow the syntax '--flag-name=<value>' (flag doesn't end after '>')");
		auto const parse_fn = +[](iter_t begin, iter_t end, iter_t &stream) -> bz::optional<bz::u8string> {
			bz_assert(stream != end);
			static iter_t init_iter = nullptr;
			if (init_iter == nullptr)
			{
				auto const stream_value = *stream;
				++stream;
				auto const it = stream_value.find('=');
				bz_assert(it != stream_value.end());
				auto const arg = bz::u8string_view(it + 1, stream_value.end());
				auto const result = arg_parser<bz::meta::remove_reference<decltype(*output)>>::parse(arg);
				if (result.has_error())
				{
					return result.get_error();
				}
				else
				{
					*output = result.get_result();
					return {};
				}
			}
			else
			{
				auto const flag_iter = stream;
				++stream;
				return bz::format("option '{}' has already been set by argument '{}', at position {}", *flag_iter, *init_iter, init_iter - begin);
			}
		};
		return parser{ parser_kind::prefix, prefix_name, "", usage, help, parse_fn };
	}
	case ',':
	{
		if constexpr (bz::meta::is_same<decltype(output), bool *>)
		{
			bz_assert(!usage.starts_with("--"), "flag doesn't follow the syntax '-f, --flag-name' (first flag starts with '--')");
			auto const first_flag_name = bz::u8string_view(begin, it);
			bz_assert(first_flag_name.length() == 2, "flag doesn't follow the syntax '-f, --flag-name' (first flag is not a single character)");
			++it;
			bz_assert(it != end && *it == ' ', "flag doesn't follow the syntax '-f, --flag-name' (missing space after comma)");
			++it;
			auto const second_flag_begin = it;
			bz_assert(bz::u8string_view(it, end).starts_with("--"), "flag doesn't follow the syntax '-f, --flag-name' (second flag doesn't start with '--')");
			++it; ++it;
			while (it != end && is_valid_char(*it))
			{
				++it;
			}
			bz_assert(it == end, "flag doesn't follow the syntax '-f, --flag-name' (second flag doesn't end after the name)");
			auto const second_flag_name = bz::u8string_view(second_flag_begin, end);
			bz_assert(second_flag_name.length() >= 3, "flag doesn't follow the syntax '-f, --flag-name' (second flag has no name)");
			return parser{ parser_kind::multiple_flag, first_flag_name, second_flag_name, usage, help, &default_flag_parser<output> };
		}
		else
		{
			bz_unreachable;
		}
	}
	default:
		bz_unreachable;
	}
}

struct group_element_t
{
	bz::u8string_view flag_name{};
	bz::u8string_view help{};
	bz::array_view<size_t const> enable_group{};
};

constexpr group_element_t create_group_element(bz::u8string_view usage, bz::u8string_view help)
{
	for (auto const c : usage)
	{
		bz_assert((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-', "flag must be of the form 'flag-name'");
	}
	bz_assert(bz::constexpr_not_equals(usage, "help"), "'help' is reserved in groups");
	return group_element_t{ usage, help, {} };
}

template<size_t N>
constexpr group_element_t create_group_element(bz::u8string_view usage, bz::u8string_view help, std::array<size_t, N> const &enable_group)
{
	for (auto const c : usage)
	{
		bz_assert((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-', "flag must be of the form 'flag-name'");
	}
	bz_assert(bz::constexpr_not_equals(usage, "help"), "'help' is reserved in groups");
	return group_element_t{ usage, help, { enable_group.data(), enable_group.data() + enable_group.size() } };
}

namespace internal
{

template<typename T>
constexpr bool is_array = false;

template<typename T, size_t N>
constexpr bool is_array<std::array<T, N>> = true;

} // namespace internal

template<auto &group, auto &output, bool *help_out>
constexpr parser create_group_parser(bz::u8string_view usage, bz::u8string_view help)
{
	using group_t = bz::meta::remove_cv_reference<decltype(group)>;
	using output_t = bz::meta::remove_reference<decltype(output)>;
	static_assert(internal::is_array<group_t>);
	static_assert(internal::is_array<output_t>);
	static_assert(bz::meta::is_same<group_t, std::array<group_element_t, group.size()>>);
	static_assert(bz::meta::is_same<output_t, std::array<bool, output.size()>>);
	auto const is_valid_char = [](bz::u8char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-';
	};

	auto const begin = usage.begin();
	auto const end = usage.end();
	auto it = begin;
	bz_assert(it != end && *it == '-', "flag doesn't follow the syntax '-f<value>' (flag doesn't start with '-')");
	++it;
	bz_assert(it != end && [](bz::u8char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
	}(*it), "flag doesn't follow the syntax '-f<value>' (second character must be a letter)");
	++it;
	bz_assert(it != end && *it == '<', "flag doesn't follow the syntax '-f<value>' (missing '<')");
	++it;
	while (it != end && is_valid_char(*it))
	{
		++it;
	}
	bz_assert(it != end && *it == '>', "flag doesn't follow the syntax '-f<value>' (missing '>')");
	++it;
	bz_assert(it == end, "flag doesn't follow the syntax '-f<value>' (flag must end after '>')");

	using T = bz::meta::integral_constant<size_t, output.size()>;

	auto const parser = +[](iter_t begin, iter_t end, iter_t &stream) -> bz::optional<bz::u8string> {
		bz_assert(stream != end);
		auto const flag_it = stream;
		auto const full_flag_val = *stream;
		++stream;
		auto flag_val = full_flag_val.substring(2);
		static std::array<std::pair<int, iter_t>, T::value> enables;

		auto const modify_flag = [=](bz::u8string_view flag_val, bool value) -> bz::optional<bz::u8string> {
			auto const it = std::find_if(
				group.begin(), group.end(),
				[flag_val](auto const &flag) {
					return flag_val == flag.flag_name;
				}
			);

			if (it == group.end())
			{
				return bz::format("unknown option '{}' for '{}'", flag_val, full_flag_val.substring(0, 2));
			}
			else if (it->enable_group.empty())
			{
				auto const index = static_cast<size_t>(it - group.begin());
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
					output[index] = value;
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
			else if (value)
			{
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
						output[index] = true;
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
				return bz::format("unknown option '{}' for '{}'", flag_val, full_flag_val.substring(0, 2));
			}
		};

		if (flag_val == "help")
		{
			static iter_t help_it = nullptr;
			if (help_it == nullptr)
			{
				*help_out = true;
				help_it = flag_it;
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
	return { parser_kind::prefix, usage.substring(0, 2), "", usage, help, parser };
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
					constexpr auto flag_name = parsers[Ns].flag_name;
					constexpr auto parse_fn  = parsers[Ns].parse_fn;

					if (flag_val == flag_name)
					{
						result = parse_fn(begin, end, stream);
						done = true;
					}
				}
				else if constexpr (parsers[Ns].kind == parser_kind::multiple_flag)
				{
					constexpr auto first_flag_name  = parsers[Ns].flag_name;
					constexpr auto second_flag_name = parsers[Ns].alternate_flag_name;
					constexpr auto parse_fn  = parsers[Ns].parse_fn;

					if (flag_val == first_flag_name || flag_val == second_flag_name)
					{
						result = parse_fn(begin, end, stream);
						done = true;
					}
				}
				else if constexpr (parsers[Ns].kind == parser_kind::prefix)
				{
					constexpr auto flag_name = parsers[Ns].flag_name;
					constexpr auto parse_fn  = parsers[Ns].parse_fn;

					if (flag_val.starts_with(flag_name))
					{
						result = parse_fn(begin, end, stream);
						done = true;
					}
				}
				else if constexpr (parsers[Ns].kind == parser_kind::argument)
				{
					constexpr auto flag_name = parsers[Ns].flag_name;
					constexpr auto parse_fn  = parsers[Ns].parse_fn;

					if (flag_val == flag_name)
					{
						result = parse_fn(begin, end, stream);
						done = true;
					}
				}
				else if constexpr (parsers[Ns].kind == parser_kind::group)
				{
					constexpr auto flag_name = parsers[Ns].flag_name;
					constexpr auto parse_fn  = parsers[Ns].parse_fn;

					if (flag_val.starts_with(flag_name))
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
	size_t column_limit
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
		auto const lhs_flag = group[lhs].flag_name;
		auto const rhs_flag = group[rhs].flag_name;
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
		usages.emplace_back(group[index].flag_name);
		helps.emplace_back(group[index].help);
	}

	return get_help_string(usages, helps, initial_indent_width, usage_width, column_limit);
}

} // namespace cl

#endif // CL_CLPARSER_H
