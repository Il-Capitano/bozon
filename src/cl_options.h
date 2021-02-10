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
	bz::array<ctcli::group_element_t, ctx::warning_infos.size()> result{};

	size_t i = 0;
	for (i = 0; i < ctx::warning_infos.size(); ++i)
	{
		bz_assert(static_cast<size_t>(ctx::warning_infos[i].kind) == i);
		result[i] = ctcli::create_group_element(ctx::warning_infos[i].name, ctx::warning_infos[i].description);
	}

	bz_assert(i == result.size());
	return result;
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
inline constexpr bz::array ctcli::command_line_options<ctcli::options_id_t::def> = {
	ctcli::create_option("-V, --version",                    "Print compiler version"),
	ctcli::create_option("-I, --import-dir <dir>",           "Add <dir> as an import directory", ctcli::arg_type::string),
	ctcli::create_option("-o, --output <file>",              "Write output to <file>", ctcli::arg_type::string),
	ctcli::create_option("--emit={obj|asm|llvm-bc|llvm-ir}", "Emit the specified code type (default=obj)", ctcli::arg_type::none),
	ctcli::create_option("--target=<target-triple>",         "Set compilation target to <target-triple>", ctcli::arg_type::string),

	ctcli::create_hidden_option("--x86-asm-syntax={att|intel}",   "Assembly syntax used for x86 (default=att)", ctcli::arg_type::none),
	ctcli::create_hidden_option("--profile",                      "Measure time for compilation steps"),
	ctcli::create_hidden_option("--debug-ir-output",              "Emit an LLVM IR file alongside the regular output"),
	ctcli::create_hidden_option("--no-error-highlight",           "Disable printing of highlighted source in error messages"),
	ctcli::create_hidden_option("--error-report-tab-size=<size>", "Set tab size in error reporting (default=4)", ctcli::arg_type::uint64),

	ctcli::create_group_option("-W, --warn <warning>",      "Enable the specified <warning>", warning_group_id, "warnings"),
	ctcli::create_group_option("-O, --opt <optimization>", "Enable the specified <optimization>", opt_group_id, "optimizations"),
};

template<>
inline constexpr bool ctcli::add_verbose_option<ctcli::options_id_t::def> = true;

template<>
inline constexpr bool ctcli::is_array_like<ctcli::option("--import-dir")> = true;

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

#endif // CL_OPTIONS_H
