#ifndef CL_OPTIONS_H
#define CL_OPTIONS_H

#include "cl/clparser.h"
#include "ctx/warnings.h"
#include "global_data.h"
#include "ctx/command_parse_context.h"

constexpr std::array Wall_indicies = {
	static_cast<size_t>(ctx::warning_kind::int_overflow),
	static_cast<size_t>(ctx::warning_kind::int_divide_by_zero),
	static_cast<size_t>(ctx::warning_kind::float_overflow),
	static_cast<size_t>(ctx::warning_kind::float_divide_by_zero),
	static_cast<size_t>(ctx::warning_kind::unknown_attribute),
	static_cast<size_t>(ctx::warning_kind::null_pointer_dereference),
	static_cast<size_t>(ctx::warning_kind::unused_value),
	static_cast<size_t>(ctx::warning_kind::unclosed_comment),
	static_cast<size_t>(ctx::warning_kind::mismatched_brace_indent),
	static_cast<size_t>(ctx::warning_kind::unused_variable),
	static_cast<size_t>(ctx::warning_kind::greek_question_mark),
};

inline bz::u8string dummy_flag_value;

constexpr auto warning_group = []() {
	std::array<cl::group_element_t, ctx::warning_infos.size() + 1> result{};

	size_t i = 0;
	for (i = 0; i < ctx::warning_infos.size(); ++i)
	{
		bz_assert(static_cast<size_t>(ctx::warning_infos[i].kind) == i);
		result[i] = cl::create_group_element(ctx::warning_infos[i].name, ctx::warning_infos[i].description, i);
	}

	result[i++] = cl::create_group_element("all", "Enable all warnings", Wall_indicies);

	bz_assert(i == result.size());
	return result;
}();

template<>
constexpr bool cl::is_array_like<&import_dirs> = true;

constexpr std::array clparsers = {
	cl::create_parser<&display_help>                    ("-h, --help",                          "Display this help page"),
	cl::create_parser<&do_verbose>                      ("-v, --verbose",                       "Do verbose output"),
	cl::create_parser<&display_version>                 ("-V, --version",                       "Print compiler version"),
	cl::create_parser<&import_dirs>                     ("-I, --import-dir <dir>",              "Add <dir> as an import directory"),
	cl::create_parser<&output_file_name>                ("-o, --output <file>",                 "Write output to <file>"),
	cl::create_parser<&emit_file_type, &parse_emit_type>("--emit={object|asm|llvm-bc|llvm-ir}", "Emit the specified code type (default=object)"),
	cl::create_parser<&do_profile>                      ("--profile",                           "Measure time for compilation steps", true),
	cl::create_parser<&no_error_highlight>              ("--no-error-highlight",                "Disable printing of highlighted source in error messages", true),
	cl::create_parser<&tab_size>                        ("--error-report-tab-size=<size>",      "Set tab size in error reporting (default=4)", true),
	cl::create_parser<&target>                          ("--target=<target-triple>",            "Set compilation target to <target-triple>"),

	cl::create_group_parser<warning_group, warnings, &display_warning_help>("-W<warning>", "Enable the specified <warning>"),
};

inline void parse_command_line(ctx::command_parse_context &context)
{
	auto const args = context.args;
	bz_assert(args.size() > 1);

	auto const begin = args.begin();
	auto const end   = args.end();
	auto it = begin;
	++it;

	constexpr auto parse_fn = cl::create_parser_function<clparsers>();

	while (it != end)
	{
		if (it->starts_with("-"))
		{
			auto const start_it = it;
			auto result = parse_fn(begin, end, it);
			if (result.has_value())
			{
				context.report_error(start_it, std::move(*result));
			}
		}
		else
		{
			if (source_file != "")
			{
				context.report_error(it, "only one source file can be provided");
			}
			else if (it->ends_with(".bz"))
			{
				source_file = *it;
			}
			else
			{
				context.report_error(it, "source file name must end in '.bz'");
			}
			++it;
		}
	}
}

inline void print_help(void)
{
	bz::u8string help_string = "Usage: bozon [options] file\n\nOptions:\n";
	help_string += cl::get_help_string<clparsers>(2, 24, 80);
	help_string += "\nAdditional help:\n";
	help_string += cl::get_additional_help_string<clparsers>(2, 24, 80);
	bz::print(help_string);
}

inline void print_verbose_help(void)
{
	bz::u8string help_string = "Usage: bozon [options] file\n\nOptions:\n";
	help_string += cl::get_help_string<clparsers>(2, 24, 80, true);
	help_string += "\nAdditional help:\n";
	help_string += cl::get_additional_help_string<clparsers>(2, 24, 80, true);
	bz::print(help_string);
}

inline void print_warning_help(void)
{
	bz::u8string help_string = "Available warnings:\n";
	help_string += cl::get_group_help_string<warning_group>(2, 24, 80);
	bz::print(help_string);
}

#endif // CL_OPTIONS_H
