#include "clparser.h"
#include "ctx/warnings.h"
#include "global_data.h"
#include "ctx/command_parse_context.h"

namespace cl
{

bz::vector<bz::u8string_view> get_args(int argc, char const **argv)
{
	bz_assert(argc >= 1);
	bz::vector<bz::u8string_view> result = {};
	result.reserve(argc - 1);
	for (int i = 1; i < argc; ++i)
	{
		result.push_back(argv[i]);
	}
	return result;
}


using iter_t = bz::array_view<bz::u8string_view const>::const_iterator;

static void parse_warnings(iter_t &it, iter_t end, ctx::command_parse_context &context)
{
	bz_assert(it->substring(0, 2) == "-W");
	bz_assert(it != end);

	if (*it == "-Wall")
	{
		enable_Wall();
	}
	// disabling warnings: -Wno-<warning-name>
	else if (it->substring(2, 5) == "no-")
	{
		auto const warning_name = it->substring(5);
		auto const warning_kind = ctx::get_warning_kind(warning_name);
		if (warning_kind == ctx::warning_kind::_last)
		{
			context.report_error(it, bz::format("unknown warning '{}'", warning_name));
		}
		else
		{
			disable_warning(warning_kind);
		}
	}
	else
	{
		auto const warning_name = it->substring(2);
		auto const warning_kind = ctx::get_warning_kind(warning_name);
		if (warning_kind == ctx::warning_kind::_last)
		{
			context.report_error(it, bz::format("unknown warning '{}'", warning_name));
		}
		else
		{
			enable_warning(warning_kind);
		}
	}
	++it;
}

static void check_output_file_name(iter_t it, ctx::command_parse_context &context)
{
	if (!it->ends_with(".o"))
	{
		context.report_error(it, "output file must end in '.o'");
	}
}

template<
	bz::u8string *dest,
	void (*check_fn)(iter_t it, ctx::command_parse_context &context)
>
void default_string_argument_parser(iter_t &it, iter_t end, ctx::command_parse_context &context)
{
	bz_assert(it != end);
	auto const arg = *it;
	++it;

	if (it == end)
	{
		context.report_error(it, bz::format("expected an argument after '{}'", arg));
	}
	else
	{
		check_fn(it, context);
		*dest = *it;
		++it;
	}
}

template<bool *dest, bool value>
void default_flag_parser(iter_t &it, iter_t end, ctx::command_parse_context &)
{
	bz_assert(it != end);
	*dest = value;
	++it;
}

static void check_source_file_name(iter_t it, ctx::command_parse_context &context)
{
	if (!it->ends_with(".bz"))
	{
		context.report_error(it, "source files must end in '.bz'");
	}
}


using parse_fn_t = void (*)(iter_t &it, iter_t end, ctx::command_parse_context &context);

struct prefix_parser
{
	bz::u8string_view prefix;
	bz::u8string_view usage;
	bz::u8string_view help;
	parse_fn_t        parse_fn;
};

struct argument_parser
{
	bz::u8string_view arg_name;
	bz::u8string_view usage;
	bz::u8string_view help;
	parse_fn_t        parse_fn;
};

struct flag_parser
{
	bz::u8string_view flag_name;
	// usage is the same as flag_name
	bz::u8string_view help;
	parse_fn_t        parse_fn;
};

struct flag_with_alternate_parser
{
	bz::u8string_view both_names;
	// usage is the same as both_names
	bz::u8string_view help;
	parse_fn_t        parse_fn;
};

constexpr std::pair<bz::u8string_view, bz::u8string_view> get_both_names(
	flag_with_alternate_parser const &flag
)
{
	auto const comma = flag.both_names.find(',');
	bz_assert(comma != flag.both_names.end());
	bz_assert(comma + 1 != flag.both_names.end());
	bz_assert(*(comma + 1) == ' ');
	bz_assert(comma + 2 != flag.both_names.end());
	bz_assert(*(comma + 2) == '-');
	return {
		bz::u8string_view(flag.both_names.begin(), comma),
		bz::u8string_view(comma + 2, flag.both_names.end())
	};
}


constexpr auto prefix_parsers = []() {
	using T = prefix_parser;

	std::array result = {
		T{ "-W",    "-W<warning>",    "Enable the specified warning",  &parse_warnings },
		T{ "-Wno-", "-Wno-<warning>", "Disable the specified warning", &parse_warnings },
	};

	// need to sort the array, so longer prefixes are checked first
	constexpr_bubble_sort(
		result,
		[](auto const &lhs, auto const &rhs) {
			return lhs.prefix.length() > rhs.prefix.length();
		},
		[](auto &lhs, auto &rhs) {
			auto const tmp = lhs;
			lhs = rhs;
			rhs = tmp;
		}
	);

	return result;
}();

constexpr auto argument_parsers = []() {
	using T = argument_parser;

	std::array result = {
		T{ "-o", "-o <file>", "Write output to <file>", &default_string_argument_parser<&output_file_name, &check_output_file_name> },
	};

	return result;
}();

constexpr auto flag_parsers = []() {
	using T = flag_parser;

	std::array result = {
		T{ "--profile",        "Measure time for compilation steps", &default_flag_parser<&do_profile,       true> },
//		T{ "--verbose-errors", "Print more verbose error messages",  &default_flag_parser<&do_verbose_error, true> },
	};

	return result;
}();

constexpr auto flag_with_alternate_parsers = []() {
	using T = flag_with_alternate_parser;

	std::array result = {
		T{ "-h, --help",    "Display this help page", &default_flag_parser<&display_help,    true> },
		T{ "-V, --version", "Print compiler version", &default_flag_parser<&display_version, true> },
	};

	return result;
}();



static_assert([]() {
	for (auto const &prefix : prefix_parsers)
	{
		if (!prefix.prefix.starts_with("-"))
		{
			return false;
		}
	}
	return true;
}(), "a prefix doesn't start with '-'");

static_assert([]() {
	for (auto const &arg : argument_parsers)
	{
		if (!arg.arg_name.starts_with("-"))
		{
			return false;
		}
	}
	return true;
}(), "an argument option doesn't start with '-'");

static_assert([]() {
	for (auto const &flag : flag_parsers)
	{
		if (!flag.flag_name.starts_with("-"))
		{
			return false;
		}
	}
	return true;
}(), "a flag doesn't start with '-'");

static_assert([]() {
	for (auto const &flag : flag_with_alternate_parsers)
	{
		auto const [first, second] = get_both_names(flag);
		if (!first.starts_with("-") || !second.starts_with("-"))
		{
			return false;
		}
	}
	return true;
}(), "a flag doesn't start with '-'");

static_assert([]() {
	for (auto const &flag : flag_with_alternate_parsers)
	{
		auto const [first, second] = get_both_names(flag);
		if (!(first.starts_with("-") && !first.starts_with("--") && second.starts_with("--")))
		{
			return false;
		}
	}
	return true;
}(), "flag with alternate form doesn't follow the form of '-x, --long-x'");



enum class parser_kind
{
	none,
	prefix,
	argument,
	flag,
	flag_with_alternate,
};

struct parser_variants_t
{
	prefix_parser              prefix              = { bz::u8string_view{}, bz::u8string_view{}, bz::u8string_view{}, nullptr };
	argument_parser            argument            = { bz::u8string_view{}, bz::u8string_view{}, bz::u8string_view{}, nullptr };
	flag_parser                flag                = { bz::u8string_view{}, bz::u8string_view{}, nullptr };
	flag_with_alternate_parser flag_with_alternate = { bz::u8string_view{}, bz::u8string_view{}, nullptr };
};

struct command_line_parser
{
	parser_kind       kind = parser_kind::none;
	parser_variants_t variants{};
};

constexpr auto command_line_parsers = []() {
	using result_t = std::array<
		command_line_parser,
		prefix_parsers.size() + argument_parsers.size()
		+ flag_parsers.size() + flag_with_alternate_parsers.size()
	>;

	result_t result{};

	size_t i = 0;
	for (auto const &flag : flag_with_alternate_parsers)
	{
		result[i].kind = parser_kind::flag_with_alternate;
		result[i].variants.flag_with_alternate = flag;
		++i;
	}

	for (auto const &flag : flag_parsers)
	{
		result[i].kind = parser_kind::flag;
		result[i].variants.flag = flag;
		++i;
	}

	for (auto const &arg : argument_parsers)
	{
		result[i].kind = parser_kind::argument;
		result[i].variants.argument = arg;
		++i;
	}

	for (auto const &prefix : prefix_parsers)
	{
		result[i].kind = parser_kind::prefix;
		result[i].variants.prefix = prefix;
		++i;
	}

	return result;
}();


static void do_parse(iter_t &it, iter_t end, ctx::command_parse_context &context)
{
	bz_assert(it != end);
	bool good = false;
	[&]<size_t ...Ns>(bz::meta::index_sequence<Ns...>) {
		(((void)(good || ([&]() {
			constexpr auto parser = command_line_parsers[Ns];
			if constexpr (parser.kind == parser_kind::prefix)
			{
				constexpr auto prefix   = parser.variants.prefix.prefix;
				constexpr auto parse_fn = parser.variants.prefix.parse_fn;

				if (it->starts_with(prefix))
				{
					parse_fn(it, end, context);
					good = true;
				}
			}
			else if constexpr (parser.kind == parser_kind::argument)
			{
				constexpr auto arg      = parser.variants.argument.arg_name;
				constexpr auto parse_fn = parser.variants.argument.parse_fn;

				if (*it == arg)
				{
					parse_fn(it, end, context);
					good = true;
				}
			}
			else if constexpr (parser.kind == parser_kind::flag)
			{
				constexpr auto flag     = parser.variants.flag.flag_name;
				constexpr auto parse_fn = parser.variants.flag.parse_fn;

				if (*it == flag)
				{
					parse_fn(it, end, context);
					good = true;
				}
			}
			else if constexpr (parser.kind == parser_kind::flag_with_alternate)
			{
				constexpr auto both_names = get_both_names(parser.variants.flag_with_alternate);
				constexpr auto parse_fn = parser.variants.flag_with_alternate.parse_fn;

				if (*it == both_names.first || *it == both_names.second)
				{
					parse_fn(it, end, context);
					good = true;
				}
			}
			else
			{
				static_assert(
					bz::meta::always_false<decltype(sizeof...(Ns))>,
					"invalid kind for command_line_parser"
				);
			}
		}(), true))), ...);
	}(bz::meta::make_index_sequence<command_line_parsers.size()>{});

	if (!good && it->starts_with("-"))
	{
		context.report_error(it, bz::format("unknown command line option '{}'", *it));
		++it;
	}
	else if (!good)
	{
		check_source_file_name(it, context);
		if (source_file != "")
		{
			context.report_error(it, "only one source file can be provided");
		}
		else
		{
			source_file = *it;
		}
		++it;
	}
}


void parse_command_line(ctx::command_parse_context &context)
{
	auto it = context.args.begin();
	auto const end = context.args.end();
	while (it != end)
	{
		do_parse(it, end, context);
	}
}


constexpr bz::u8string_view get_usage_string(command_line_parser const &parser)
{
	switch (parser.kind)
	{
	case parser_kind::prefix:
		return parser.variants.prefix.usage;
	case parser_kind::argument:
		return parser.variants.argument.usage;
	case parser_kind::flag:
		return parser.variants.flag.flag_name;
	case parser_kind::flag_with_alternate:
		return parser.variants.flag_with_alternate.both_names;
	default:
		bz_assert(false);
		return "";
	}
}

bz::u8string get_indented_usage_string(command_line_parser const &parser)
{
	auto const usage_string = get_usage_string(parser);
	if (usage_string.starts_with("--"))
	{
		// four spaces to pad '-x, ' strings in flags having alternatives
		bz::u8string result = "    ";
		result += usage_string;
		return result;
	}
	else
	{
		return usage_string;
	}
}

constexpr bz::u8string_view get_help_string(command_line_parser const &parser)
{
	switch (parser.kind)
	{
	case parser_kind::prefix:
		return parser.variants.prefix.help;
	case parser_kind::argument:
		return parser.variants.argument.help;
	case parser_kind::flag:
		return parser.variants.flag.help;
	case parser_kind::flag_with_alternate:
		return parser.variants.flag_with_alternate.help;
	default:
		bz_assert(false);
		return "";
	}
}

constexpr bz::u8string_view get_usage_string_without_dashes(command_line_parser const &parser)
{
	bz::u8string_view usage = get_usage_string(parser);
	if (usage.starts_with("--"))
	{
		return usage.substring(2);
	}
	else
	{
		bz_assert(usage.starts_with("-"));
		return usage.substring(1);
	}
}

constexpr bool alphabetical_compare(bz::u8string_view lhs, bz::u8string_view rhs)
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
			return c + ('A' - 'a');
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

constexpr auto sorted_command_line_parsers = []() {
	auto result = command_line_parsers;

	constexpr_bubble_sort(
		result,
		[](auto const &lhs, auto const &rhs) {
			return alphabetical_compare(
				get_usage_string_without_dashes(lhs),
				get_usage_string_without_dashes(rhs)
			);
		},
		[](auto &lhs, auto &rhs) {
			auto const tmp = lhs;
			lhs = rhs;
			rhs = tmp;
		}
	);

	return result;
}();

static bz::u8string build_help_string()
{
	constexpr bz::u8string_view initial_indent = "  ";
	constexpr size_t pad_to = 24;

	bz::u8string result = "Usage: bozon [options] file\nOptions:\n";

	for (auto const &parser : sorted_command_line_parsers)
	{
		auto const usage = get_indented_usage_string(parser);
		auto const help = get_help_string(parser);

		if (usage.length() >= pad_to)
		{
			result += initial_indent;
			result += usage;
			result += bz::format("\n{:{}}{}\n", pad_to + initial_indent.length(), "", help);
		}
		else
		{
			result += bz::format("{}{:{}}{}\n", initial_indent, pad_to, usage, help);
		}
	}

	return result;
}

void display_help_screen(void)
{
	auto const help_string = build_help_string();
	bz::print(help_string);
}

void print_version_info(void)
{
	constexpr bz::u8string_view version_info = "bozon 0.0.0";
	bz::print("{}\n", version_info);
}

} // namespace cl
