#include "clparser.h"
#include "ctx/warnings.h"
#include "global_data.h"

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
using parse_fn_t = void (*)(iter_t &it, iter_t end);

struct command_line_parser
{
	bz::u8string_view prefix;
	bz::u8string_view arg;
	parse_fn_t        parse_fn;
};

static void parse_warnings(iter_t &it, iter_t end)
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
		bz_assert(warning_kind != ctx::warning_kind::_last);
		disable_warning(warning_kind);
	}
	else
	{
		auto const warning_name = it->substring(2);
		auto const warning_kind = ctx::get_warning_kind(warning_name);
		bz_assert(warning_kind != ctx::warning_kind::_last);
		enable_warning(warning_kind);
	}
	++it;
}

constexpr auto command_line_parsers = []() {
	using T = command_line_parser;

	std::array result = {
		T{ "-W", "", &parse_warnings },
	};

	return result;
}();


static void do_parse(iter_t &it, iter_t end)
{
	bool good = false;
	[&]<size_t ...Ns>(bz::meta::index_sequence<Ns...>) {
		((
			[&]() {
				constexpr auto prefix   = command_line_parsers[Ns].prefix;
				constexpr auto arg      = command_line_parsers[Ns].arg;
				constexpr auto parse_fn = command_line_parsers[Ns].parse_fn;

				static_assert(!(
					bz::constexpr_equals(prefix, "")
					&& bz::constexpr_equals(arg, "")
				));

				if constexpr (
					bz::constexpr_equals(prefix, "")
					&& bz::constexpr_not_equals(arg, "")
				)
				{
					if (*it == arg)
					{
						parse_fn(it, end);
						good = true;
					}
				}
				else if constexpr (
					bz::constexpr_not_equals(prefix, "")
					&& bz::constexpr_equals(arg, "")
				)
				{
					if (it->substring(0, prefix.length()) == prefix)
					{
						parse_fn(it, end);
						good = true;
					}
				}
				else // if constexpr (prefix != "" && arg != "")
				{
					bz_assert(false);
				}
			}()
		), ...);
	}(bz::meta::make_index_sequence<command_line_parsers.size()>{});

	if (!good)
	{
		bz_assert(false);
	}
}

void parse_command_line(bz::array_view<bz::u8string_view const> args)
{
	bz::print("parsing command line arguments...\n");

	auto it = args.begin();
	auto const end = args.end();
	while (it != end)
	{
		do_parse(it, end);
	}
}

} // namespace cl
