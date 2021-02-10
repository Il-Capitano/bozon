#ifndef CL_OPTIONS_H
#define CL_OPTIONS_H

#include "ctcli/ctcli.h"
#include "ctx/warnings.h"
#include "bc/optimizations.h"
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

constexpr auto Oall_indicies = []() {
	using result_t = std::array<size_t, bc::optimization_infos.size()>;
	result_t result{};

	size_t i = 0;
	for (auto const &info : bc::optimization_infos)
	{
		result[i++] = static_cast<size_t>(info.kind);
	}
	bz_assert(i == result.size());

	return result;
}();

static constexpr auto warning_group_id = ctcli::group_id_t::_0;
static constexpr auto opt_group_id     = ctcli::group_id_t::_1;

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
	bz::array<ctcli::group_element_t, bc::optimization_infos.size() + 1> result{};

	size_t i = 0;
	for (i = 0; i < bc::optimization_infos.size(); ++i)
	{
		bz_assert(static_cast<size_t>(bc::optimization_infos[i].kind) == i);
		result[i] = ctcli::create_group_element(bc::optimization_infos[i].name, bc::optimization_infos[i].description);
	}

	static_assert(bz::meta::is_same<size_t, uint64_t>);
	result[i++] = ctcli::create_group_element("max-iter-count=<count>", "Control the maximum number of pass iterations (default=32)", ctcli::arg_type::uint64);

	bz_assert(i == result.size());
	return result;
}();

template<>
inline constexpr bz::array ctcli::option_group_multiple<opt_group_id> = []() {
	return []<size_t ...Is>(bz::meta::index_sequence<Is...>) {
		return bz::array{
			ctcli::create_multiple_group_element<opt_group_id>("all", "Enable all optimizations", { bc::optimization_infos[Is].name... }),
		};
	}(bz::meta::make_index_sequence<bc::optimization_infos.size()>{});
}();

template<>
inline constexpr bz::array ctcli::command_line_options<ctcli::options_id_t::def> = {
	ctcli::create_option("-V, --version",                    "Print compiler version"),
	ctcli::create_option("-I, --import-dir <dir>",           "Add <dir> as an import directory", ctcli::arg_type::string),
	ctcli::create_option("-o, --output <file>",              "Write output to <file>", ctcli::arg_type::string),
	ctcli::create_option("--emit={obj|asm|llvm-bc|llvm-ir}", "Emit the specified code type (default=obj)"),
	ctcli::create_option("--target=<target-triple>",         "Set compilation target to <target-triple>", ctcli::arg_type::string),

	ctcli::create_hidden_option("--x86-asm-syntax={att|intel}",   "Assembly syntax used for x86 (default=att)"),
	ctcli::create_hidden_option("--profile",                      "Measure time for compilation steps"),
	ctcli::create_hidden_option("--debug-ir-output",              "Emit an LLVM IR file alongside the regular output"),
	ctcli::create_hidden_option("--no-error-highlight",           "Disable printing of highlighted source in error messages"),
	ctcli::create_hidden_option("--error-report-tab-size=<size>", "Set tab size in error reporting (default=4)", ctcli::arg_type::uint64),

	ctcli::create_group_option("-W, --warn <warning>",     "Enable the specified <warning>",      warning_group_id, "warnings"),
	ctcli::create_group_option("-O, --opt <optimization>", "Enable the specified <optimization>", opt_group_id,     "optimizations"),
};

template<>
inline constexpr bool ctcli::add_verbose_option<ctcli::options_id_t::def> = true;

template<>
inline constexpr bool ctcli::is_array_like<ctcli::option("--import-dir")> = true;

template<>
inline constexpr bool ctcli::is_array_like<ctcli::group_element("--warn error")> = true;

template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::option("--version")>               = &display_version;
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::option("--output")>                = &output_file_name;
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::option("--profile")>               = &do_profile;
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::option("--debug-ir-output")>       = &debug_ir_output;
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::option("--verbose")>               = &do_verbose;
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::option("--target")>                = &target;
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::option("--emit")>                  = &emit_file_type;
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::option("--x86-asm-syntax")>        = &x86_asm_syntax;
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::option("--error-report-tab-size")> = &tab_size;
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::option("--no-error-highlight")>    = &no_error_highlight;
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::option("--import-dir")>            = &import_dirs;

template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--warn int-overflow")>             = &warnings[static_cast<size_t>(ctx::warning_kind::int_overflow)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--warn int-divide-by-zero")>       = &warnings[static_cast<size_t>(ctx::warning_kind::int_divide_by_zero)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--warn float-overflow")>           = &warnings[static_cast<size_t>(ctx::warning_kind::float_overflow)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--warn float-divide-by-zero")>     = &warnings[static_cast<size_t>(ctx::warning_kind::float_divide_by_zero)];
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
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--warn bad-float-math")>           = &warnings[static_cast<size_t>(ctx::warning_kind::bad_float_math)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--warn binary-stdout")>            = &warnings[static_cast<size_t>(ctx::warning_kind::binary_stdout)];
static_assert(static_cast<size_t>(ctx::warning_kind::_last) == 18);

template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--opt instcombine")>            = &optimizations[static_cast<size_t>(bc::optimization_kind::instcombine)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--opt mem2reg")>                = &optimizations[static_cast<size_t>(bc::optimization_kind::mem2reg)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--opt simplifycfg")>            = &optimizations[static_cast<size_t>(bc::optimization_kind::simplifycfg)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--opt reassociate")>            = &optimizations[static_cast<size_t>(bc::optimization_kind::reassociate)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--opt gvn")>                    = &optimizations[static_cast<size_t>(bc::optimization_kind::gvn)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--opt inline")>                 = &optimizations[static_cast<size_t>(bc::optimization_kind::inline_)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--opt dce")>                    = &optimizations[static_cast<size_t>(bc::optimization_kind::dce)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--opt adce")>                   = &optimizations[static_cast<size_t>(bc::optimization_kind::adce)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--opt sccp")>                   = &optimizations[static_cast<size_t>(bc::optimization_kind::sccp)];
template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--opt aggressive-instcombine")> = &optimizations[static_cast<size_t>(bc::optimization_kind::aggressive_instcombine)];
static_assert(static_cast<size_t>(bc::optimization_kind::_last) == 10);

template<> inline constexpr auto *ctcli::value_storage_ptr<ctcli::group_element("--opt max-iter-count")> = &max_opt_iter_count;

template<>
inline constexpr auto ctcli::argument_parse_function<ctcli::option("--emit")> = [](bz::u8string_view arg) -> std::optional<emit_type> {
	if (arg == "obj")
	{
		return emit_type::obj;
	}
	else if (arg == "asm")
	{
		return emit_type::asm_;
	}
	else if (arg == "llvm-bc")
	{
		return emit_type::llvm_bc;
	}
	else if (arg == "llvm-ir")
	{
		return emit_type::llvm_ir;
	}
	else
	{
		return {};
	}
};

template<>
inline constexpr auto ctcli::argument_parse_function<ctcli::option("--x86-asm-syntax")> = [](bz::u8string_view arg) -> std::optional<x86_asm_syntax_kind> {
	if (arg == "att")
	{
		return x86_asm_syntax_kind::att;
	}
	else if (arg == "intel")
	{
		return x86_asm_syntax_kind::intel;
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
