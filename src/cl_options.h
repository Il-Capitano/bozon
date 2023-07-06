#ifndef CL_OPTIONS_H
#define CL_OPTIONS_H

#include "ctcli/ctcli.h"
#include "ctx/warnings.h"
#include "codegen/optimizations.h"
#include "global_data.h"

constexpr auto Wall_indicies = []() {
	using result_t = std::array<size_t, ctx::warning_infos.size()>;
	result_t result{};

	size_t i = 0;
	for (auto const &info : ctx::warning_infos)
	{
		result[i++] = static_cast<size_t>(info.kind);
	}
	bz_assert(i == result.size());

	return result;
}();

static constexpr auto warning_group_id  = ctcli::group_id_t::_0;
static constexpr auto opt_group_id      = ctcli::group_id_t::_1;
static constexpr auto code_gen_group_id = ctcli::group_id_t::_2;

template<>
inline constexpr bz::array ctcli::option_group<warning_group_id> = []() {
	bz::array<ctcli::group_element_t, ctx::warning_infos.size() + 1> result{};

	size_t i = 0;
	for (i = 0; i < ctx::warning_infos.size(); ++i)
	{
		bz_assert(static_cast<size_t>(ctx::warning_infos[i].kind) == i);
		result[i] = ctcli::create_group_element(ctx::warning_infos[i].name, ctx::warning_infos[i].description);
	}

	result[i++] = ctcli::create_group_element("error=<warning>", "Treat <warning> as an error", ctcli::arg_type::string);

	bz_assert(i == result.size());
	return result;
}();

template<>
inline constexpr bz::array ctcli::option_group_multiple<warning_group_id> = []() {
	return []<size_t ...Is>(bz::meta::index_sequence<Is...>) {
		return bz::array{
			ctcli::create_multiple_group_element<warning_group_id>("all", "Enable all warnings", { ctx::warning_infos[Is].name... }),
		};
	}(bz::meta::make_index_sequence<ctx::warning_infos.size()>{});
}();

template<>
inline constexpr bz::array ctcli::option_group<opt_group_id> = []() {
	bz::array<ctcli::group_element_t, codegen::optimization_infos.size() + 4> result{};

	size_t i = 0;
	for (i = 0; i < codegen::optimization_infos.size(); ++i)
	{
		bz_assert(static_cast<size_t>(codegen::optimization_infos[i].kind) == i);
		result[i] = ctcli::create_hidden_group_element(codegen::optimization_infos[i].name, codegen::optimization_infos[i].description);
	}

	static_assert(bz::meta::is_same<size_t, uint64_t>);
	result[i++] = ctcli::create_group_element("max-iter-count=<count>", "Control the maximum number of pass iterations (default=1)", ctcli::arg_type::uint64);
	result[i++] = ctcli::create_group_element("opt-level=<level>",      "Set optimization level (0-3) (default=0)",                  ctcli::arg_type::uint32);
	result[i++] = ctcli::create_group_element("size-opt-level=<level>", "Set size optimization level (0-2) (default=0)",             ctcli::arg_type::uint32);

	result[i++] = ctcli::create_hidden_group_element("machine-code-opt-level=<level>", "Manually set optimization level for machine code generation (0-3)", ctcli::arg_type::uint32);

	bz_assert(i == result.size());
	return result;
}();

template<>
inline constexpr bz::array ctcli::option_group_alias<opt_group_id> = {
	ctcli::create_alias_group_element<opt_group_id>("0", "No optimizations (same as opt-level=0)",               "opt-level=0"),
	ctcli::create_alias_group_element<opt_group_id>("1", "Enable basic optimizations (same as opt-level=1)",     "opt-level=1"),
	ctcli::create_alias_group_element<opt_group_id>("2", "Enable more optimizations (same as opt-level=2)",      "opt-level=2"),
	ctcli::create_alias_group_element<opt_group_id>("3", "Enable even more optimizations (same as opt-level=3)", "opt-level=3"),
	ctcli::create_alias_group_element<opt_group_id>("s", "Optimize for size (same as size-opt-level=1)",         "size-opt-level=1"),
	ctcli::create_alias_group_element<opt_group_id>("z", "Optimize more for size (same as size-opt-level=2)",    "size-opt-level=2"),
};

template<>
inline constexpr bz::array ctcli::option_group<code_gen_group_id> = {
	ctcli::create_group_element("panic-on-unreachable",             "Call '__builtin_panic' if unreachable is hit (default=true)"),
	ctcli::create_group_element("panic-on-null-dereference",        "Call '__builtin_panic' if null is dereferenced (default=true)"),
	ctcli::create_group_element("panic-on-null-pointer-arithmetic", "Call '__builtin_panic' if null is used in pointer arithmetic (default=true)"),
	ctcli::create_group_element("panic-on-null-get-value",          "Call '__builtin_panic' if 'get_value' is called with a null value (default=true)"),
	ctcli::create_group_element("panic-on-invalid-switch",          "Call '__builtin_panic' if an invalid enum value is used in a 'switch' expression (default=true)"),
	ctcli::create_group_element("discard-llvm-value-names",         "Discard values names for LLVM bitcode (default=true)"),
	ctcli::create_group_element("freestanding",                     "Generate code with no external dependencies (default=false)"),
};

template<>
inline constexpr bz::array ctcli::command_line_options<ctcli::options_id_t::def> = {
	ctcli::create_option("-V, --version",                         "Print compiler version"),
	ctcli::create_option("-I, --import-dir <dir>",                "Add <dir> as an import directory", ctcli::arg_type::string),
	ctcli::create_option("-o, --output <file>",                   "Write output to <file>", ctcli::arg_type::string),
	ctcli::create_option("-D, --define <option>",                 "Set <option> for compilation", ctcli::arg_type::string),
	ctcli::create_option("--emit={obj|asm|llvm-bc|llvm-ir|null}", "Emit the specified code type or nothing (default=obj)"),
	ctcli::create_option("--target=<target-triple>",              "Set compilation target to <target-triple>", ctcli::arg_type::string),

	ctcli::create_hidden_option("--stdlib-dir <dir>",             "Specify the standard library directory", ctcli::arg_type::string),
	ctcli::create_hidden_option("--x86-asm-syntax={att|intel}",   "Assembly syntax used for x86 (default=att)"),
	ctcli::create_hidden_option("--profile",                      "Measure time for compilation steps"),
	ctcli::create_hidden_option("--no-main",                      "Don't provide a default 'main' function"),
	ctcli::create_hidden_option("--no-error-highlight",           "Disable printing of highlighted source in error messages"),
	ctcli::create_hidden_option("--error-report-tab-size=<size>", "Set tab size in error reporting (default=4)", ctcli::arg_type::uint64),
	ctcli::create_hidden_option("--enable-comptime-print",        "Enable the usage of '__builtin_comptime_print'"),

	ctcli::create_undocumented_option("--return-zero-on-error", "Return 0 exit code even if there were build errors"),
#ifndef NDEBUG
	ctcli::create_undocumented_option("--debug-ir-output", "Emit an LLVM IR file alongside the regular output"),
	ctcli::create_undocumented_option("--debug-comptime-print-functions", ""),
	ctcli::create_undocumented_option("--debug-comptime-print-instructions", ""),
#endif // !NDEBUG
#ifdef BOZON_PROFILE_COMPTIME
	ctcli::create_undocumented_option("--debug-comptime-print-instruction-counts", ""),
#endif // BOZON_PROFILE_COMPTIME

	ctcli::create_group_option("-W, --warn <warning>",     "Enable the specified <warning>",      warning_group_id,  "warnings"),
	ctcli::create_group_option("-O, --opt <optimization>", "Enable the specified <optimization>", opt_group_id,      "optimizations"),
	ctcli::create_group_option("-C, --code-gen <option>",  "Set code generation option",          code_gen_group_id, "code generation options"),
};

template<> inline constexpr bool ctcli::add_verbose_option<ctcli::options_id_t::def> = true;

template<> inline constexpr bool ctcli::is_array_like<ctcli::option("--import-dir")> = true;
template<> inline constexpr bool ctcli::is_array_like<ctcli::option("--define")>     = true;
template<> inline constexpr bool ctcli::is_array_like<ctcli::option("--opt")>        = true;
template<> inline constexpr bool ctcli::is_array_like<ctcli::group_element("--warn error")> = true;

template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::option("--version")>                  = &display_version;
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::option("--import-dir")>               = &import_dirs;
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::option("--output")>                   = &output_file_name;
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::option("--define")>                   = &defines;
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::option("--emit")>                     = &emit_file_type;
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::option("--target")>                   = &target;
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::option("--stdlib-dir")>               = &stdlib_dir;
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::option("--x86-asm-syntax")>           = &x86_asm_syntax;
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::option("--profile")>                  = &do_profile;
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::option("--no-main")>                  = &no_main;
#ifndef NDEBUG
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::option("--debug-ir-output")>          = &debug_ir_output;
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::option("--debug-comptime-print-functions")>    = &debug_comptime_print_functions;
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::option("--debug-comptime-print-instructions")> = &debug_comptime_print_instructions;
#endif // !NDEBUG
#ifdef BOZON_PROFILE_COMPTIME
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::option("--debug-comptime-print-instruction-counts")> = &debug_comptime_print_instruction_counts;
#endif // BOZON_PROFILE_COMPTIME
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::option("--no-error-highlight")>       = &no_error_highlight;
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::option("--error-report-tab-size")>    = &tab_size;
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::option("--enable-comptime-print")>    = &enable_comptime_print;
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::option("--return-zero-on-error")>     = &return_zero_on_error;
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::option("--verbose")>                  = &do_verbose;

template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--warn int-overflow")>             = &warnings[static_cast<size_t>(ctx::warning_kind::int_overflow)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--warn int-divide-by-zero")>       = &warnings[static_cast<size_t>(ctx::warning_kind::int_divide_by_zero)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--warn float-overflow")>           = &warnings[static_cast<size_t>(ctx::warning_kind::float_overflow)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--warn float-divide-by-zero")>     = &warnings[static_cast<size_t>(ctx::warning_kind::float_divide_by_zero)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--warn float-nan-math")>           = &warnings[static_cast<size_t>(ctx::warning_kind::float_nan_math)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--warn unknown-attribute")>        = &warnings[static_cast<size_t>(ctx::warning_kind::unknown_attribute)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--warn null-pointer-dereference")> = &warnings[static_cast<size_t>(ctx::warning_kind::null_pointer_dereference)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--warn unused-value")>             = &warnings[static_cast<size_t>(ctx::warning_kind::unused_value)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--warn unclosed-comment")>         = &warnings[static_cast<size_t>(ctx::warning_kind::unclosed_comment)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--warn mismatched-brace-indent")>  = &warnings[static_cast<size_t>(ctx::warning_kind::mismatched_brace_indent)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--warn unused-variable")>          = &warnings[static_cast<size_t>(ctx::warning_kind::unused_variable)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--warn greek-question-mark")>      = &warnings[static_cast<size_t>(ctx::warning_kind::greek_question_mark)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--warn bad-file-extension")>       = &warnings[static_cast<size_t>(ctx::warning_kind::bad_file_extension)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--warn unknown-target")>           = &warnings[static_cast<size_t>(ctx::warning_kind::unknown_target)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--warn invalid-unicode")>          = &warnings[static_cast<size_t>(ctx::warning_kind::invalid_unicode)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--warn nan-compare")>              = &warnings[static_cast<size_t>(ctx::warning_kind::nan_compare)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--warn out-of-bounds-index")>      = &warnings[static_cast<size_t>(ctx::warning_kind::out_of_bounds_index)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--warn math-domain-error")>        = &warnings[static_cast<size_t>(ctx::warning_kind::math_domain_error)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--warn binary-stdout")>            = &warnings[static_cast<size_t>(ctx::warning_kind::binary_stdout)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--warn is-comptime-always-true")>  = &warnings[static_cast<size_t>(ctx::warning_kind::is_comptime_always_true)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--warn non-exhaustive-switch")>    = &warnings[static_cast<size_t>(ctx::warning_kind::non_exhaustive_switch)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--warn unneeded-else")>            = &warnings[static_cast<size_t>(ctx::warning_kind::unneeded_else)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--warn assign-in-condition")>      = &warnings[static_cast<size_t>(ctx::warning_kind::assign_in_condition)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--warn get-value-null")>           = &warnings[static_cast<size_t>(ctx::warning_kind::get_value_null)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--warn enum-value-overflow")>      = &warnings[static_cast<size_t>(ctx::warning_kind::enum_value_overflow)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--warn comptime-warning")>         = &warnings[static_cast<size_t>(ctx::warning_kind::comptime_warning)];
static_assert(static_cast<size_t>(ctx::warning_kind::_last) == 26);

template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--opt max-iter-count")>         = &max_opt_iter_count;
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--opt opt-level")>              = &opt_level;
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--opt size-opt-level")>         = &size_opt_level;
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--opt machine-code-opt-level")> = &machine_code_opt_level;

template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--code-gen panic-on-unreachable")>             = &panic_on_unreachable;
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--code-gen panic-on-null-dereference")>        = &panic_on_null_dereference;
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--code-gen panic-on-null-pointer-arithmetic")> = &panic_on_null_pointer_arithmetic;
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--code-gen panic-on-null-get-value")>          = &panic_on_null_get_value;
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--code-gen panic-on-invalid-switch")>          = &panic_on_invalid_switch;
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--code-gen discard-llvm-value-names")>         = &discard_llvm_value_names;
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--code-gen freestanding")>                     = &freestanding;

template<>
inline constexpr auto ctcli::argument_parse_function<ctcli::option("--emit")> = [](bz::u8string_view arg) -> std::optional<emit_type> {
	auto const result = parse_emit_type(arg);
	if (result.has_value())
	{
		return result.get();
	}
	else
	{
		return {};
	}
};

template<>
inline constexpr auto ctcli::argument_parse_function<ctcli::option("--x86-asm-syntax")> = [](bz::u8string_view arg) -> std::optional<x86_asm_syntax_kind> {
	auto const result = parse_x86_asm_syntax(arg);
	if (result.has_value())
	{
		return result.get();
	}
	else
	{
		return {};
	}
};

template<>
inline constexpr auto ctcli::argument_parse_function<ctcli::group_element("--warn error")> = [](bz::u8string_view arg) -> std::optional<bz::u8string_view> {
	auto const it = std::find_if(
		ctx::warning_infos.begin(), ctx::warning_infos.end(),
		[arg](auto const &info) {
			return info.name == arg;
		}
	);
	if (it == ctx::warning_infos.end())
	{
		if (arg == "all")
		{
			for (auto &val : error_warnings)
			{
				val = true;
			}
			return arg;
		}
		return {};
	}
	else
	{
		auto const index = static_cast<size_t>(it - ctx::warning_infos.begin());
		error_warnings[index] = true;
		return arg;
	}
};

#endif // CL_OPTIONS_H
