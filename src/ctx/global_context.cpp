#include "global_context.h"
#include "bitcode_context.h"
#include "ast/statement.h"
#include "cl_options.h"
#include "bc/emit_bitcode.h"
#include "colors.h"
#include "comptime/codegen_context.h"
#include "comptime/codegen.h"

#include <cassert>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/LegacyPassManager.h>

#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Analysis/AliasAnalysisEvaluator.h>
#include <llvm/Analysis/BasicAliasAnalysis.h>
#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/Analysis/TargetTransformInfo.h>
#include <llvm/Analysis/TypeBasedAliasAnalysis.h>
#include <llvm/Analysis/ScopedNoAliasAA.h>
#include <llvm/Analysis/AssumptionCache.h>
#include <llvm/Analysis/ProfileSummaryInfo.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/LazyBranchProbabilityInfo.h>
#include <llvm/Analysis/LazyBlockFrequencyInfo.h>
#include <llvm/Analysis/LazyValueInfo.h>
#include <llvm/Analysis/OptimizationRemarkEmitter.h>
#include <llvm/Analysis/CallGraph.h>
#include <llvm/Analysis/GlobalsModRef.h>
#include <llvm/Analysis/MemorySSA.h>
#include <llvm/Analysis/PostDominators.h>
#include <llvm/Analysis/BranchProbabilityInfo.h>
#include <llvm/Analysis/BlockFrequencyInfo.h>
#include <llvm/Analysis/LoopPass.h>
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/Analysis/PhiValues.h>
#include <llvm/Analysis/MemoryDependenceAnalysis.h>
#include <llvm/Analysis/DemandedBits.h>
#include <llvm/Analysis/LoopAccessAnalysis.h>

#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/AggressiveInstCombine/AggressiveInstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Transforms/Utils.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/IPO/ForceFunctionAttrs.h>
#include <llvm/Transforms/IPO/InferFunctionAttrs.h>
#include <llvm/Transforms/IPO/FunctionAttrs.h>
#include <llvm/Transforms/IPO/AlwaysInliner.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/Transforms/Instrumentation.h>
#include <llvm/Transforms/Vectorize.h>
#include <llvm/Transforms/Instrumentation.h>

#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/MC/MCAsmInfo.h>
#include <llvm/MC/TargetRegistry.h>

#if LLVM_VERSION_MAJOR < 14
#error LLVM 14 is required
#endif // LLVM 14

namespace ctx
{

static bz::array<llvm::Type *, static_cast<int>(ast::type_info::null_t_) + 1>
get_llvm_builtin_types(llvm::LLVMContext &context)
{
	auto const i8_ptr = llvm::Type::getInt8PtrTy(context);
	auto const str_t  = llvm::StructType::create("builtin.str", i8_ptr, i8_ptr);
	auto const null_t = llvm::StructType::create(context, {}, "builtin.__null_t");
	return {
		llvm::Type::getInt8Ty(context),   // int8_
		llvm::Type::getInt16Ty(context),  // int16_
		llvm::Type::getInt32Ty(context),  // int32_
		llvm::Type::getInt64Ty(context),  // int64_
		llvm::Type::getInt8Ty(context),   // uint8_
		llvm::Type::getInt16Ty(context),  // uint16_
		llvm::Type::getInt32Ty(context),  // uint32_
		llvm::Type::getInt64Ty(context),  // uint64_
		llvm::Type::getFloatTy(context),  // float32_
		llvm::Type::getDoubleTy(context), // float64_
		llvm::Type::getInt32Ty(context),  // char_
		str_t,                            // str_
		llvm::Type::getInt1Ty(context),   // bool_
		null_t,                           // null_t_
	};
}

static llvm::PassBuilder get_pass_builder(llvm::TargetMachine *tm)
{
	auto tuning_options = llvm::PipelineTuningOptions();
	// we could later add command line options to set different tuning options
	return llvm::PassBuilder(tm, tuning_options);
}

ast::scope_t get_default_decls(ast::scope_t *builtin_global_scope, bz::array_view<bz::u8string_view const> id_scope)
{
	ast::scope_t result;
	auto &global_scope = result.emplace<ast::global_scope_t>();
	global_scope.parent = { builtin_global_scope, 0 };
	global_scope.id_scope = id_scope;
	return result;
}

global_context::global_context(void)
	: _compile_decls{},
	  _errors{},
	  _builtin_universal_functions(ast::make_builtin_universal_functions()),
	  _builtin_type_infos{},
	  _builtin_functions{},
	  _builtin_operators{},
	  _builtin_global_scope(),
	  _llvm_context(),
	  _module("test", this->_llvm_context),
	  _target(nullptr),
	  _target_machine(nullptr),
	  _data_layout(),
	  _llvm_builtin_types(get_llvm_builtin_types(this->_llvm_context))
{}

global_context::~global_context(void) noexcept = default;

ast::type_info *global_context::get_builtin_type_info(uint32_t kind) const
{
	bz_assert(kind <= ast::type_info::null_t_);
	bz_assert(this->_builtin_type_infos[kind] != nullptr);
	return this->_builtin_type_infos[kind];
}

ast::type_info *global_context::get_usize_type_info(void) const
{
	auto const usize_alias = this->_builtin_usize_type_alias;
	bz_assert(usize_alias != nullptr);
	bz_assert(usize_alias->get_type().is<ast::ts_base_type>());
	auto const info = usize_alias->get_type().get<ast::ts_base_type>().info;
	bz_assert(ast::is_unsigned_integer_kind(info->kind));
	return info;
}

ast::type_info *global_context::get_isize_type_info(void) const
{
	auto const isize_alias = this->_builtin_isize_type_alias;
	bz_assert(isize_alias != nullptr);
	bz_assert(isize_alias->get_type().is<ast::ts_base_type>());
	auto const info = isize_alias->get_type().get<ast::ts_base_type>().info;
	bz_assert(ast::is_signed_integer_kind(info->kind));
	return info;
}

ast::decl_function *global_context::get_builtin_function(uint32_t kind)
{
	bz_assert(kind < this->_builtin_functions.size());
	bz_assert(this->_builtin_functions[kind] != nullptr);

	return this->_builtin_functions[kind];
}

bz::array_view<uint32_t const> global_context::get_builtin_universal_functions(bz::u8string_view id)
{
	auto const it = std::find_if(
		this->_builtin_universal_functions.begin(), this->_builtin_universal_functions.end(),
		[id](auto const &set) {
			return id == set.id;
		}
	);
	if (it != this->_builtin_universal_functions.end())
	{
		return it->func_kinds;
	}
	else
	{
		return {};
	}
}

resolve::attribute_info_t *global_context::get_builtin_attribute(bz::u8string_view name)
{
	auto const it = std::find_if(
		this->_builtin_attributes.begin(), this->_builtin_attributes.end(),
		[name](auto const &attr) {
			return attr.name == name;
		}
	);
	if (it == this->_builtin_attributes.end())
	{
		return nullptr;
	}
	else
	{
		return &*it;
	}
}

size_t global_context::get_sizeof(ast::typespec_view ts)
{
	bz_assert(this->comptime_codegen_context != nullptr);
	return comptime::get_type(ts, *this->comptime_codegen_context)->size;
}

size_t global_context::get_alignof(ast::typespec_view ts)
{
	bz_assert(this->comptime_codegen_context != nullptr);
	return comptime::get_type(ts, *this->comptime_codegen_context)->align;
}

comptime::codegen_context &global_context::get_codegen_context(void)
{
	bz_assert(this->comptime_codegen_context != nullptr);
	return *this->comptime_codegen_context;
}

void global_context::report_error_or_warning(error &&err)
{
	this->_errors.emplace_back(std::move(err));
}

void global_context::report_error(error &&err)
{
	bz_assert(err.is_error());
	this->report_error_or_warning(std::move(err));
}

void global_context::report_error(bz::u8string message, bz::vector<source_highlight> notes, bz::vector<source_highlight> suggestions)
{
	this->report_error_or_warning(error{
		warning_kind::_last,
		{
			global_context::compiler_file_id, 0,
			char_pos(), char_pos(), char_pos(),
			suggestion_range{}, suggestion_range{},
			std::move(message),
		},
		std::move(notes), std::move(suggestions)
	});
}

void global_context::report_error(
	lex::src_tokens const &src_tokens, bz::u8string message,
	bz::vector<source_highlight> notes,
	bz::vector<source_highlight> suggestions
)
{
	this->_errors.push_back(error{
		warning_kind::_last,
		{
			src_tokens.pivot->src_pos.file_id, src_tokens.pivot->src_pos.line,
			src_tokens.begin->src_pos.begin, src_tokens.pivot->src_pos.begin, (src_tokens.end - 1)->src_pos.end,
			suggestion_range{}, suggestion_range{},
			std::move(message),
		},
		std::move(notes), std::move(suggestions)
	});
}

void global_context::report_warning(error &&err)
{
	bz_assert(err.is_warning());
	if (is_warning_enabled(err.kind))
	{
		this->report_error_or_warning(std::move(err));
	}
}

void global_context::report_warning(warning_kind kind, bz::u8string message)
{
	if (is_warning_enabled(kind))
	{
		this->report_error_or_warning(error{
			kind,
			{
				global_context::compiler_file_id, 0,
				char_pos(), char_pos(), char_pos(),
				suggestion_range{}, suggestion_range{},
				std::move(message),
			},
			{}, {}
		});
	}
}

[[nodiscard]] source_highlight global_context::make_note(bz::u8string message)
{
	return source_highlight{
		global_context::compiler_file_id, 0,
		char_pos(), char_pos(), char_pos(),
		{}, {}, std::move(message)
	};
}

[[nodiscard]] source_highlight global_context::make_note(lex::src_tokens const &src_tokens, bz::u8string message)
{
	return source_highlight{
		src_tokens.pivot->src_pos.file_id, src_tokens.pivot->src_pos.line,
		src_tokens.begin->src_pos.begin, src_tokens.pivot->src_pos.begin, (src_tokens.end - 1)->src_pos.end,
		{}, {},
		std::move(message)
	};
}

src_file &global_context::get_src_file(uint32_t file_id) noexcept
{
	auto const it = std::find_if(
		this->_src_files.begin(), this->_src_files.end(),
		[&](auto const &src_file) {
			return file_id == src_file._file_id;
		}
	);
	bz_assert(it != this->_src_files.end());
	return *it;
}

src_file const &global_context::get_src_file(uint32_t file_id) const noexcept
{
	auto const it = std::find_if(
		this->_src_files.begin(), this->_src_files.end(),
		[&](auto const &src_file) {
			return file_id == src_file._file_id;
		}
	);
	bz_assert(it != this->_src_files.end());
	return *it;
}

char_pos global_context::get_file_begin(uint32_t file_id) const noexcept
{
	if (file_id == compiler_file_id || file_id == command_line_file_id)
	{
		return char_pos();
	}
	bz_assert(file_id < this->_src_files.size());
	return this->get_src_file(file_id)._file.begin();
}

std::pair<char_pos, char_pos> global_context::get_file_begin_and_end(uint32_t file_id) const noexcept
{
	if (file_id == compiler_file_id || file_id == command_line_file_id)
	{
		return { char_pos(), char_pos() };
	}
	bz_assert(file_id < this->_src_files.size());
	auto const &src_file = this->get_src_file(file_id);
	return { src_file._file.begin(), src_file._file.end() };
}

bool global_context::is_library_file(uint32_t file_id) const noexcept
{
	return file_id == compiler_file_id || file_id == command_line_file_id || this->get_src_file(file_id)._is_library_file;
}


bool global_context::has_errors(void) const
{
	for (auto &err : this->_errors)
	{
		if (err.is_error() || (is_warning_error(err.kind)))
		{
			return true;
		}
	}
	return false;
}

bool global_context::has_warnings(void) const
{
	for (auto &err : this->_errors)
	{
		if (err.is_warning())
		{
			return true;
		}
	}
	return false;
}

size_t global_context::get_error_count(void) const
{
	size_t count = 0;
	for (auto &err : this->_errors)
	{
		if (err.is_error())
		{
			++count;
		}
	}
	return count;
}

size_t global_context::get_warning_count(void) const
{
	size_t count = 0;
	for (auto &err : this->_errors)
	{
		if (err.is_warning())
		{
			++count;
		}
	}
	return count;
}

void global_context::add_compile_function(ast::function_body &func_body)
{
	this->_compile_decls.funcs.push_back(&func_body);
}

static std::pair<fs::path, bool> search_for_source_file(ast::identifier const &id, fs::path const &current_path)
{
	bz::u8string module_file_name;
	bool allow_library = true;
	bool first = true;
	for (auto const value : id.values)
	{
		if (first)
		{
			first = false;
		}
		else
		{
			module_file_name += '/';
		}
		module_file_name += value;
		allow_library &= !value.starts_with('_');
	}
	module_file_name += ".bz";
	std::string_view module_folder_name_sv(module_file_name.data_as_char_ptr(), module_file_name.size() - 3);
	std::string_view module_file_name_sv(module_file_name.data_as_char_ptr(), module_file_name.size());
	if (!id.is_qualified)
	{
		auto same_dir_module = current_path / module_file_name_sv;
		if (fs::exists(same_dir_module))
		{
			return { std::move(same_dir_module), false };
		}
		auto same_dir_module_folder = current_path / module_folder_name_sv;
		if (fs::exists(same_dir_module_folder) && fs::is_directory(same_dir_module_folder))
		{
			return { std::move(same_dir_module_folder), false };
		}
	}

	if (allow_library)
	{
		for (auto const &import_dir : import_dirs)
		{
			std::string_view import_dir_sv(import_dir.data_as_char_ptr(), import_dir.size());
			auto library_module = fs::path(import_dir_sv) / module_file_name_sv;
			if (fs::exists(library_module))
			{
				return { std::move(library_module), true };
			}
			auto library_module_folder = fs::path(import_dir_sv) / module_folder_name_sv;
			if (fs::exists(library_module_folder) && fs::is_directory(library_module_folder))
			{
				return { std::move(library_module_folder), true };
			}
		}
	}
	return {};
}

static uint32_t add_module_file(
	src_file &current_file,
	fs::path const &module_path,
	bool is_library_file,
	bz::vector<bz::u8string> scope,
	global_context &context
)
{
	auto &file = [&]() -> auto & {
		auto const file_it = std::find_if(
			context._src_files.begin(), context._src_files.end(),
			[&](auto const &src_file) {
				return fs::equivalent(src_file.get_file_path(), module_path);
			}
		);
		if (file_it == context._src_files.end())
		{
			return context._src_files.emplace_back(
				module_path, context._src_files.size(), std::move(scope), is_library_file || current_file._is_library_file
			);
		}
		else
		{
			return *file_it;
		}
	}();

	if (file._stage < src_file::parsed_global_symbols)
	{
		if (!file.parse_global_symbols(context))
		{
			return std::numeric_limits<uint32_t>::max();
		}
	}
	return file._file_id;
}

static bz::vector<uint32_t> add_module_folder(
	src_file &current_file,
	fs::path const &module_path,
	bool is_library_folder,
	bz::vector<bz::u8string> &scope,
	global_context &context
)
{
	bz::vector<uint32_t> result;
	bz_assert(fs::is_directory(module_path));
	for (auto const &p : bz::basic_range(fs::directory_iterator(module_path), fs::directory_iterator()))
	{
		if (is_library_folder && p.path().filename().generic_string().starts_with('_'))
		{
			continue;
		}

		if (p.is_directory())
		{
			auto const folder_name_s = p.path().filename().generic_string();
			auto const folder_name = bz::u8string_view(folder_name_s.data(), folder_name_s.data() + folder_name_s.size());
			auto const is_identifier = [&]() {
				auto const first_char = *folder_name.begin();
				auto const is_valid_first_char =
					(first_char >= 'a' && first_char <= 'z')
					|| (first_char >= 'A' && first_char <= 'Z')
					|| first_char == '_';
				return is_valid_first_char && bz::to_range(folder_name).is_all([](bz::u8char c) {
					return (c >= 'a' && c <= 'z')
					|| (c >= 'A' && c <= 'Z')
					|| (c >= '0' && c <= '9')
					|| c == '_';
				});
			}();
			if (is_identifier)
			{
				scope.push_back(folder_name);
				result.append(add_module_folder(current_file, p.path(), is_library_folder, scope, context));
				scope.pop_back();
			}
		}
		else if (p.path().filename().generic_string().ends_with(".bz"))
		{
			result.push_back(add_module_file(current_file, p.path(), is_library_folder, scope, context));
		}
	}
	return result;
}

bz::vector<uint32_t> global_context::add_module(uint32_t current_file_id, ast::identifier const &id)
{
	auto &current_file = this->get_src_file(current_file_id);
	auto const [module_path, is_library_path] = search_for_source_file(id, current_file.get_file_path().parent_path());
	if (module_path.empty())
	{
		this->report_error(error{
			warning_kind::_last,
			{
				id.tokens.begin->src_pos.file_id, id.tokens.begin->src_pos.line,
				id.tokens.begin->src_pos.begin, id.tokens.begin->src_pos.begin, (id.tokens.end - 1)->src_pos.end,
				suggestion_range{}, suggestion_range{},
				bz::format("unable to find module '{}'", id.as_string()),
			},
			{}, {}
		});
		return { std::numeric_limits<uint32_t>::max() };
	}

	if (module_path.filename().generic_string().ends_with(".bz"))
	{
		auto scope = [&, is_library_path = is_library_path]() {
			if (is_library_path)
			{
				auto result = bz::vector<bz::u8string>();
				result.append(bz::array_view(id.values.begin(), id.values.end() - 1));
				return result;
			}
			else
			{
				auto result = this->get_src_file(current_file_id)._scope_container;
				result.append(bz::array_view(id.values.begin(), id.values.end() - 1));
				return result;
			}
		}();
		return { add_module_file(current_file, module_path, is_library_path, std::move(scope), *this) };
	}
	else
	{
		auto scope = [&, is_library_path = is_library_path]() {
			if (is_library_path)
			{
				auto result = bz::vector<bz::u8string>();
				result.append(bz::array_view(id.values.begin(), id.values.end()));
				return result;
			}
			else
			{
				auto result = this->get_src_file(current_file_id)._scope_container;
				result.append(bz::array_view(id.values.begin(), id.values.end()));
				return result;
			}
		}();
		return add_module_folder(current_file, module_path, is_library_path, scope, *this);
	}
}

ast::scope_t *global_context::get_file_export_decls(uint32_t file_id)
{
	return &this->get_src_file(file_id)._export_decls;
}

bz::u8string global_context::get_file_name(uint32_t file_id)
{
	if (file_id == command_line_file_id)
	{
		return "<command-line>";
	}
	else
	{
		bz_assert(file_id != compiler_file_id);
		return this->get_src_file(file_id).get_file_path().generic_string().c_str();
	}
}

bz::u8string global_context::get_location_string(lex::token_pos t)
{
	bz_assert(t != nullptr);
	return bz::format("{}:{}", this->get_file_name(t->src_pos.file_id), t->src_pos.line);
}

bool global_context::add_builtin_function(ast::decl_function *func_decl)
{
	if (!func_decl->id.is_qualified || func_decl->id.values.size() != 1)
	{
		return false;
	}

	auto const id = func_decl->id.values[0];

	auto const it = std::find_if(
		ast::intrinsic_info.begin(), ast::intrinsic_info.end(),
		[id](auto const &info) {
			return info.func_name == id;
		}
	);

	if (it == ast::intrinsic_info.end() || this->_builtin_functions[it->kind] != nullptr)
	{
		return false;
	}

	func_decl->body.intrinsic_kind = it->kind;
	this->_builtin_functions[it->kind] = func_decl;
	return true;
}

bool global_context::add_builtin_operator(ast::decl_operator *op_decl)
{
	auto const op = op_decl->op->kind;
	if (op_decl->body.params.size() == 1)
	{
		auto const it = std::find_if(
			ast::builtin_unary_operator_info.begin(), ast::builtin_unary_operator_info.end(),
			[op](auto const &info) {
				return op == info.op;
			}
		);
		if (it == ast::builtin_unary_operator_info.end())
		{
			return false;
		}

		op_decl->body.intrinsic_kind = it->kind;
		this->_builtin_operators.push_back(op_decl);
		return true;
	}
	else if (op_decl->body.params.size() == 2)
	{
		auto const it = std::find_if(
			ast::builtin_binary_operator_info.begin(), ast::builtin_binary_operator_info.end(),
			[op](auto const &info) {
				return op == info.op;
			}
		);
		if (it == ast::builtin_binary_operator_info.end())
		{
			return false;
		}

		op_decl->body.intrinsic_kind = it->kind;
		this->_builtin_operators.push_back(op_decl);
		return true;
	}
	else
	{
		return false;
	}
}

bool global_context::add_builtin_type_alias(ast::decl_type_alias *alias_decl)
{
	if (alias_decl->id.values.back() == "isize" && this->_builtin_isize_type_alias == nullptr)
	{
		this->_builtin_isize_type_alias = alias_decl;
		return true;
	}
	else if (alias_decl->id.values.back() == "usize" && this->_builtin_usize_type_alias == nullptr)
	{
		this->_builtin_usize_type_alias = alias_decl;
		return true;
	}
	else
	{
		return false;
	}
}

struct builtin_type_info_info_t
{
	bz::u8string_view name;
	uint8_t kind;
};

static constexpr bz::array builtin_type_info_infos = {
	builtin_type_info_info_t{ "int8",     ast::type_info::int8_    },
	builtin_type_info_info_t{ "int16",    ast::type_info::int16_   },
	builtin_type_info_info_t{ "int32",    ast::type_info::int32_   },
	builtin_type_info_info_t{ "int64",    ast::type_info::int64_   },
	builtin_type_info_info_t{ "uint8",    ast::type_info::uint8_   },
	builtin_type_info_info_t{ "uint16",   ast::type_info::uint16_  },
	builtin_type_info_info_t{ "uint32",   ast::type_info::uint32_  },
	builtin_type_info_info_t{ "uint64",   ast::type_info::uint64_  },
	builtin_type_info_info_t{ "float32",  ast::type_info::float32_ },
	builtin_type_info_info_t{ "float64",  ast::type_info::float64_ },
	builtin_type_info_info_t{ "char",     ast::type_info::char_    },
	builtin_type_info_info_t{ "str",      ast::type_info::str_     },
	builtin_type_info_info_t{ "bool",     ast::type_info::bool_    },
	builtin_type_info_info_t{ "__null_t", ast::type_info::null_t_  },
};

bool global_context::add_builtin_type_info(ast::type_info *info)
{
	auto const name = info->type_name.values.back();
	auto const it = std::find_if(
		builtin_type_info_infos.begin(), builtin_type_info_infos.end(),
		[name](auto const &info) {
			return info.name == name;
		}
	);

	if (it == builtin_type_info_infos.end() || this->_builtin_type_infos[it->kind] != nullptr)
	{
		return false;
	}

	info->kind = it->kind;
	this->_builtin_type_infos[it->kind] = info;


	// attributes are initialized after all the builtin types have been resolved, which should be now
	if (it->kind == ast::type_info::null_t_)
	{
		bz_assert(this->_builtin_type_infos.is_all([](auto const info) { return info != nullptr; }));
		this->_builtin_attributes = resolve::make_attribute_infos(this->_builtin_type_infos);
	}

	return true;
}

ast::type_info *global_context::get_usize_type_info_for_builtin_alias(void) const
{
	auto const pointer_size = this->_data_layout->getPointerSize();
	bz_assert(pointer_size == 8 || pointer_size == 4);
	return pointer_size == 8
		? this->get_builtin_type_info(ast::type_info::uint64_)
		: this->get_builtin_type_info(ast::type_info::uint32_);
}

ast::type_info *global_context::get_isize_type_info_for_builtin_alias(void) const
{
	auto const pointer_size = this->_data_layout->getPointerSize();
	bz_assert(pointer_size == 8 || pointer_size == 4);
	return pointer_size == 8
		? this->get_builtin_type_info(ast::type_info::int64_)
		: this->get_builtin_type_info(ast::type_info::int32_);
}

bool global_context::is_aggressive_consteval_enabled(void) const
{
	auto const &optimizations = ctcli::option_value<ctcli::option("--opt")>;
	return optimizations.contains(ctcli::group_element("--opt aggressive-consteval"));
}


void global_context::report_and_clear_errors_and_warnings(void)
{
	for (auto &err : this->_errors)
	{
		print_error_or_warning(err, *this);
	}
	this->clear_errors_and_warnings();
}

[[nodiscard]] bool global_context::parse_command_line(int argc, char const * const*argv)
{
	if (argc == 1)
	{
		ctcli::print_options_help<>("bozon", "source-file", 2, 24, 80);
		compile_until = compilation_phase::parse_command_line;
		return true;
	}

	auto const errors = ctcli::parse_command_line(argc, argv);
	for (auto const &err : errors)
	{
		this->report_error(error{
			warning_kind::_last,
			{
				command_line_file_id,
				static_cast<uint32_t>(err.flag_position),
				char_pos(), char_pos(), char_pos(),
				suggestion_range{}, suggestion_range{},
				err.message,
			},
			{}, {}
		});
	}
	if (errors.not_empty())
	{
		return false;
	}

	if (ctcli::print_help_if_needed("bozon", "source-file", 2, 24, 80))
	{
		compile_until = compilation_phase::parse_command_line;
		return true;
	}
	else if (display_version)
	{
		print_version_info();
		compile_until = compilation_phase::parse_command_line;
		return true;
	}

	auto &positional_args = ctcli::positional_arguments<ctcli::options_id_t::def>;
	if (positional_args.size() >= 2)
	{
		this->report_error("only one source file may be provided");
	}

	if (positional_args.size() == 1)
	{
		source_file = positional_args[0];
	}

	if (this->has_errors())
	{
		return false;
	}
	else
	{
		return true;
	}
}

[[nodiscard]] bool global_context::initialize_llvm(void)
{
	this->_llvm_context.setDiscardValueNames(discard_llvm_value_names);

	auto const is_native_target = target == "" || target == "native";
	auto const target_triple = is_native_target
		? llvm::Triple::normalize(llvm::sys::getDefaultTargetTriple())
		: llvm::Triple::normalize(std::string(target.data_as_char_ptr(), target.size()));
	llvm::InitializeAllDisassemblers();
	llvm::InitializeAllTargetInfos();
	llvm::InitializeAllTargets();
	llvm::InitializeAllTargetMCs();
	llvm::InitializeAllAsmParsers();
	llvm::InitializeAllAsmPrinters();

	{
		// set --x86-asm-syntax for LLVM
		char const *args[] = {
			"bozon",
			x86_asm_syntax == x86_asm_syntax_kind::att ? "--x86-asm-syntax=att" : "--x86-asm-syntax=intel"
		};
		if (!llvm::cl::ParseCommandLineOptions(std::size(args), args))
		{
			bz_unreachable;
		}
	}

	std::string target_error = "";
	this->_target = llvm::TargetRegistry::lookupTarget(target_triple, target_error);
	if (this->_target == nullptr)
	{
		constexpr std::string_view default_start = "No available targets are compatible with triple \"";
		bz::vector<source_highlight> notes;
		if (do_verbose)
		{
			bz::u8string message = "available targets are: ";
			bool is_first = true;
			for (auto const &target : llvm::TargetRegistry::targets())
			{
				if (is_first)
				{
					message += bz::format("'{}'", target.getName());
					is_first = false;
				}
				else
				{
					message += bz::format(", '{}'", target.getName());
				}
			}
			notes.emplace_back(this->make_note(std::move(message)));
		}
		if (target_error.substr(0, default_start.length()) == default_start)
		{
			this->report_error(bz::format(
				"'{}' is not an available target", target_triple.c_str()
			), std::move(notes));
		}
		else
		{
			this->report_error(target_error.c_str(), std::move(notes));
		}
		return false;
	}

	auto const triple = llvm::Triple(target_triple);
	auto const os = triple.getOS();
	auto const arch = triple.getArch();

	if (os == llvm::Triple::Win32 && arch == llvm::Triple::x86_64)
	{
		this->_platform_abi = abi::platform_abi::microsoft_x64;
	}
	else if (os == llvm::Triple::Linux && arch == llvm::Triple::x86_64)
	{
		this->_platform_abi = abi::platform_abi::systemv_amd64;
	}
	else
	{
		this->_platform_abi = abi::platform_abi::generic;
	}

	if (this->_platform_abi == abi::platform_abi::generic)
	{
		this->report_warning(
			warning_kind::unknown_target,
			bz::format(
				"target '{}' has limited support right now, external function calls may not work as intended",
				target_triple.c_str()
			)
		);
	}

	auto const cpu = "generic";
	auto const features = "";

	llvm::TargetOptions options;
	auto rm = llvm::Optional<llvm::Reloc::Model>(llvm::Reloc::Model::PIC_);
	this->_target_machine.reset(this->_target->createTargetMachine(target_triple, cpu, features, options, rm));
	bz_assert(this->_target_machine);
	switch (opt_level)
	{
	case 0:
		this->_target_machine->setOptLevel(llvm::CodeGenOpt::None);
		break;
	case 1:
		this->_target_machine->setOptLevel(llvm::CodeGenOpt::Less);
		break;
	case 2:
		this->_target_machine->setOptLevel(llvm::CodeGenOpt::Default);
		break;
	default:
		this->_target_machine->setOptLevel(llvm::CodeGenOpt::Aggressive);
		break;
	}
	this->_data_layout = this->_target_machine->createDataLayout();
	this->_module.setDataLayout(*this->_data_layout);
	this->_module.setTargetTriple(target_triple);

	auto const machine_parameters = comptime::machine_parameters_t{
		.pointer_size = this->get_data_layout().getPointerSize(),
		.endianness = this->get_data_layout().isLittleEndian()
			? comptime::memory::endianness_kind::little
			: comptime::memory::endianness_kind::big,
	};

	this->type_prototype_set = std::make_unique<ast::type_prototype_set_t>(machine_parameters.pointer_size);
	this->comptime_codegen_context = std::make_unique<comptime::codegen_context>(*this->type_prototype_set, machine_parameters);

	return true;
}

[[nodiscard]] bool global_context::initialize_builtins(void)
{
	this->_builtin_type_infos.resize(ast::type_info::null_t_ + 1, nullptr);
	this->_builtin_functions.resize(ast::function_body::_builtin_last - ast::function_body::_builtin_first, nullptr);

	if (!ctcli::is_option_set<ctcli::option("--stdlib-dir")>())
	{
		this->report_error("option '--stdlib-dir' is required");
		return false;
	}

	auto const &target_triple = this->_target_machine->getTargetTriple().str();
	auto const stdlib_dir_path = fs::path(std::string_view(stdlib_dir.data_as_char_ptr(), stdlib_dir.size()));
	auto const common_str = bz::u8string((stdlib_dir_path / "common").generic_string().c_str());
	auto const target_str = bz::u8string((stdlib_dir_path / target_triple).generic_string().c_str());
	import_dirs.push_front(target_str);
	import_dirs.push_front(common_str);

	auto const builtins_file_path = stdlib_dir_path / "compiler/__builtins.bz";
	auto &builtins_file = this->_src_files.emplace_back(
		builtins_file_path, this->_src_files.size(), bz::vector<bz::u8string>(), true
	);
	this->_builtin_global_scope = &builtins_file._export_decls;
	if (!builtins_file.parse_global_symbols(*this))
	{
		return false;
	}

	if (!builtins_file.parse(*this))
	{
		return false;
	}

	return true;
}

[[nodiscard]] bool global_context::parse_global_symbols(void)
{
	if (source_file == "")
	{
		this->report_error("no source file was provided");
		return false;
	}
	else if (source_file != "-" && !source_file.ends_with(".bz"))
	{
		this->report_error("source file name must end in '.bz'");
		return false;
	}

	auto const source_file_path = fs::path(std::string_view(source_file.data_as_char_ptr(), source_file.size()));
	auto &file = this->_src_files.emplace_back(source_file_path, this->_src_files.size(), bz::vector<bz::u8string>(), false);
	if (!file.parse_global_symbols(*this))
	{
		return false;
	}

	return true;
}

[[nodiscard]] bool global_context::parse(void)
{
	for (auto &file : this->_src_files)
	{
		if (!file.parse(*this))
		{
			return false;
		}
	}

	return true;
}

static auto filter_struct_decls(bz::array_view<ast::statement const> decls)
{
	return decls
		.filter([](auto const &stmt) { return stmt.template is<ast::decl_struct>(); })
		.transform([](auto const &stmt) -> ast::decl_struct const & { return stmt.template get<ast::decl_struct>(); });
}

static auto filter_var_decls(bz::array_view<ast::statement const> decls)
{
	return decls
		.filter([](auto const &stmt) { return stmt.template is<ast::decl_variable>(); })
		.transform([](auto const &stmt) -> ast::decl_variable const & { return stmt.template get<ast::decl_variable>(); });
}

static void emit_struct_symbols_helper(bz::array_view<ast::statement const> decls, bitcode_context &context)
{
	for (auto const &struct_decl : filter_struct_decls(decls))
	{
		bc::emit_global_type_symbol(struct_decl.info, context);

		if (struct_decl.info.kind == ast::type_info::aggregate)
		{
			if (struct_decl.info.is_generic())
			{
				for (auto const &instantiation_decl : struct_decl.info.generic_instantiations)
				{
					if (instantiation_decl->state == ast::resolve_state::all)
					{
						emit_struct_symbols_helper(instantiation_decl->body.get<bz::vector<ast::statement>>(), context);
					}
				}
			}
			else
			{
				if (struct_decl.info.state == ast::resolve_state::all)
				{
					emit_struct_symbols_helper(struct_decl.info.body.get<bz::vector<ast::statement>>(), context);
				}
			}
		}
	}
}

static void emit_structs_helper(bz::array_view<ast::statement const> decls, bitcode_context &context)
{
	for (auto const &struct_decl : filter_struct_decls(decls))
	{
		bc::emit_global_type(struct_decl.info, context);

		if (struct_decl.info.kind == ast::type_info::aggregate)
		{
			if (struct_decl.info.is_generic())
			{
				for (auto const &instantiation_decl : struct_decl.info.generic_instantiations)
				{
					if (instantiation_decl->state == ast::resolve_state::all)
					{
						emit_structs_helper(instantiation_decl->body.get<bz::vector<ast::statement>>(), context);
					}
				}
			}
			else
			{
				if (struct_decl.info.state == ast::resolve_state::all)
				{
					emit_structs_helper(struct_decl.info.body.get<bz::vector<ast::statement>>(), context);
				}
			}
		}
	}
}

static void emit_variables_helper(bz::array_view<ast::statement const> decls, bitcode_context &context)
{
	for (auto const &var_decl : filter_var_decls(decls))
	{
		if (var_decl.is_global())
		{
			bc::emit_global_variable(var_decl, context);
		}
	}

	for (auto const &struct_decl : filter_struct_decls(decls))
	{
		if (struct_decl.info.kind == ast::type_info::aggregate)
		{
			if (struct_decl.info.is_generic())
			{
				for (auto const &instantiation_decl : struct_decl.info.generic_instantiations)
				{
					if (instantiation_decl->state == ast::resolve_state::all)
					{
						emit_variables_helper(instantiation_decl->body.get<bz::vector<ast::statement>>(), context);
					}
				}
			}
			else
			{
				if (struct_decl.info.state == ast::resolve_state::all)
				{
					emit_variables_helper(struct_decl.info.body.get<bz::vector<ast::statement>>(), context);
				}
			}
		}
	}
}

[[nodiscard]] bool global_context::emit_bitcode(void)
{
	bitcode_context context(*this, &this->_module);

	llvm::LoopAnalysisManager loop_analysis_manager;
	llvm::FunctionAnalysisManager function_analysis_manager;
	llvm::CGSCCAnalysisManager cgscc_analysis_manager;
	llvm::ModuleAnalysisManager module_analysis_manager;

	auto builder = get_pass_builder(this->_target_machine.get());

	if (opt_level != 0 || size_opt_level != 0)
	{
		builder.registerModuleAnalyses(module_analysis_manager);
		builder.registerCGSCCAnalyses(cgscc_analysis_manager);
		builder.registerFunctionAnalyses(function_analysis_manager);
		builder.registerLoopAnalyses(loop_analysis_manager);
		builder.crossRegisterProxies(
			loop_analysis_manager,
			function_analysis_manager,
			cgscc_analysis_manager,
			module_analysis_manager
		);
	}

	auto const llvm_opt_level = [&]() {
		if (size_opt_level != 0)
		{
			return size_opt_level == 1 ? llvm::OptimizationLevel::Os : llvm::OptimizationLevel::Oz;
		}
		else if (opt_level != 0)
		{
			return
				opt_level == 1 ? llvm::OptimizationLevel::O1 :
				opt_level == 2 ? llvm::OptimizationLevel::O2 :
				llvm::OptimizationLevel::O3;
		}
		else
		{
			return llvm::OptimizationLevel::O0;
		}
	}();

	auto function_pass_manager = llvm_opt_level != llvm::OptimizationLevel::O0
		? builder.buildFunctionSimplificationPipeline(llvm_opt_level, llvm::ThinOrFullLTOPhase::None)
		: llvm::FunctionPassManager();

	if (opt_level != 0 || size_opt_level != 0)
	{
		context.function_analysis_manager = &function_analysis_manager;
		context.function_pass_manager = &function_pass_manager;
	}

	// add declarations to the module
	bz_assert(this->_compile_decls.var_decls.size() == 0);
	for (auto const &file : this->_src_files)
	{
		emit_struct_symbols_helper(file._declarations, context);
	}
	for (auto const &file : this->_src_files)
	{
		emit_structs_helper(file._declarations, context);
	}
	for (auto const &file : this->_src_files)
	{
		emit_variables_helper(file._declarations, context);
	}
	for (auto const func : this->_compile_decls.funcs)
	{
		if (
			func->is_external_linkage()
			&& !(
				this->_main == nullptr
				&& func->symbol_name == "main"
			)
		)
		{
			context.ensure_function_emission(func);
		}
	}

	bc::emit_necessary_functions(context);

	return !this->has_errors();
}

[[nodiscard]] bool global_context::emit_file(void)
{
	// only here for debug purposes, the '--emit' option does not control this
	if (debug_ir_output)
	{
		std::error_code ec;
		llvm::raw_fd_ostream file("output.ll", ec, llvm::sys::fs::OF_Text);
		if (!ec)
		{
			this->_module.print(file, nullptr);
		}
		else
		{
			bz::print(stderr, "{}unable to write output.ll{}\n", colors::bright_red, colors::clear);
		}
	}

	switch (emit_file_type)
	{
	case emit_type::obj:
		return this->emit_obj();
	case emit_type::asm_:
		return this->emit_asm();
	case emit_type::llvm_bc:
		return this->emit_llvm_bc();
	case emit_type::llvm_ir:
		return this->emit_llvm_ir();
	case emit_type::null:
		return true;
	}
	bz_unreachable;
}

bool global_context::emit_obj(void)
{
	bz::u8string const &output_file = output_file_name != ""
		? output_file_name
		: []() {
			auto const slash_it = source_file.rfind_any("/\\");
			auto const dot = source_file.rfind('.');
			bz_assert(dot != bz::u8iterator{});
			return bz::format("{}.o", bz::u8string(
				slash_it == bz::u8iterator{} ? source_file.begin() : slash_it + 1,
				dot
			));
		}();

	if (output_file != "-" && !output_file.ends_with(".o"))
	{
		this->report_warning(
			warning_kind::bad_file_extension,
			bz::format("object output file '{}' doesn't have the file extension '.o'", output_file)
		);
	}

	// passing "-" to raw_fd_ostream should output to stdout and not create a new file
	// http://llvm.org/doxygen/classllvm_1_1raw__fd__ostream.html#af5462bc0fe5a61eccc662708da280e64
	std::error_code ec;
	llvm::raw_fd_ostream dest(
		llvm::StringRef(output_file.data_as_char_ptr(), output_file.size()),
		ec, llvm::sys::fs::OF_None
	);

	if (ec)
	{
		this->report_error(bz::format(
			"unable to open output file '{}', reason: '{}'",
			output_file, ec.message().c_str()
		));
		return false;
	}

	if (output_file == "-")
	{
		this->report_warning(
			warning_kind::binary_stdout,
			"outputting binary file to stdout"
		);
	}

	auto &module = this->_module;
	llvm::legacy::PassManager pass_manager;
	auto const target_machine = this->_target_machine.get();
	auto const res = target_machine->addPassesToEmitFile(pass_manager, dest, nullptr, llvm::CGFT_ObjectFile);
	if (res)
	{
		this->report_error("object file emission is not supported");
		return false;
	}

	pass_manager.run(module);
	return true;
}

bool global_context::emit_asm(void)
{
	bz::u8string const &output_file = output_file_name != ""
		? output_file_name
		: []() {
			auto const slash_it = source_file.rfind('/');
			auto const dot = source_file.rfind('.');
			bz_assert(dot != bz::u8iterator{});
			return bz::format("{}.s", bz::u8string(
				slash_it == bz::u8iterator{} ? source_file.begin() : slash_it + 1,
				dot
			));
		}();

	if (output_file != "-" && !output_file.ends_with(".s"))
	{
		this->report_warning(
			warning_kind::bad_file_extension,
			bz::format("assembly output file '{}' doesn't have the file extension '.s'", output_file)
		);
	}

	// passing "-" to raw_fd_ostream should output to stdout and not create a new file
	// http://llvm.org/doxygen/classllvm_1_1raw__fd__ostream.html#af5462bc0fe5a61eccc662708da280e64
	std::error_code ec;
	llvm::raw_fd_ostream dest(
		llvm::StringRef(output_file.data_as_char_ptr(), output_file.size()),
		ec, llvm::sys::fs::OF_None
	);

	if (ec)
	{
		this->report_error(bz::format(
			"unable to open output file '{}', reason: '{}'",
			output_file, ec.message().c_str()
		));
		return false;
	}

	auto &module = this->_module;
	llvm::legacy::PassManager pass_manager;
	auto const target_machine = this->_target_machine.get();
	auto const res = target_machine->addPassesToEmitFile(pass_manager, dest, nullptr, llvm::CGFT_AssemblyFile);
	if (res)
	{
		this->report_error("assembly file emission is not supported");
		return false;
	}

	pass_manager.run(module);
	dest.flush();

	return true;
}

bool global_context::emit_llvm_bc(void)
{
	auto &module = this->_module;
	bz::u8string const &output_file = output_file_name != ""
		? output_file_name
		: []() {
			auto const slash_it = source_file.rfind('/');
			auto const dot = source_file.rfind('.');
			bz_assert(dot != bz::u8iterator{});
			return bz::format("{}.bc", bz::u8string(
				slash_it == bz::u8iterator{} ? source_file.begin() : slash_it + 1,
				dot
			));
		}();

	if (output_file != "-" && !output_file.ends_with(".bc"))
	{
		this->report_warning(
			warning_kind::bad_file_extension,
			bz::format("LLVM bitcode output file '{}' doesn't have the file extension '.bc'", output_file)
		);
	}

	// passing "-" to raw_fd_ostream should output to stdout and not create a new file
	// http://llvm.org/doxygen/classllvm_1_1raw__fd__ostream.html#af5462bc0fe5a61eccc662708da280e64
	std::error_code ec;
	llvm::raw_fd_ostream dest(
		llvm::StringRef(output_file.data_as_char_ptr(), output_file.size()),
		ec, llvm::sys::fs::OF_None
	);

	if (ec)
	{
		this->report_error(bz::format(
			"unable to open output file '{}', reason: '{}'",
			output_file, ec.message().c_str()
		));
		return false;
	}

	if (output_file == "-")
	{
		this->report_warning(
			warning_kind::binary_stdout,
			"outputting binary file to stdout"
		);
	}

	llvm::WriteBitcodeToFile(module, dest);
	return true;
}

bool global_context::emit_llvm_ir(void)
{
	auto &module = this->_module;
	bz::u8string const &output_file = output_file_name != ""
		? output_file_name
		: []() {
			auto const slash_it = source_file.rfind('/');
			auto const dot = source_file.rfind('.');
			bz_assert(dot != bz::u8iterator{});
			return bz::format("{}.ll", bz::u8string(
				slash_it == bz::u8iterator{} ? source_file.begin() : slash_it + 1,
				dot
			));
		}();

	if (output_file != "-" && !output_file.ends_with(".ll"))
	{
		this->report_warning(
			warning_kind::bad_file_extension,
			bz::format("LLVM IR output file '{}' doesn't have the file extension '.ll'", output_file)
		);
	}

	// passing "-" to raw_fd_ostream should output to stdout and not create a new file
	// http://llvm.org/doxygen/classllvm_1_1raw__fd__ostream.html#af5462bc0fe5a61eccc662708da280e64
	std::error_code ec;
	llvm::raw_fd_ostream dest(
		llvm::StringRef(output_file.data_as_char_ptr(), output_file.size()),
		ec, llvm::sys::fs::OF_None
	);

	if (ec)
	{
		this->report_error(bz::format(
			"unable to open output file '{}', reason: '{}'",
			output_file, ec.message().c_str()
		));
		return false;
	}

	module.print(dest, nullptr);
	return true;
}

[[nodiscard]] bool global_context::optimize(void)
{
	auto const &optimizations = ctcli::option_value<ctcli::option("--opt")>;
	if ((opt_level == 0 && optimizations.empty()) || max_opt_iter_count == 0)
	{
		return true;
	}

	auto &module = this->_module;

	auto const llvm_opt_level = [&]() {
		if (size_opt_level != 0)
		{
			return size_opt_level == 1 ? llvm::OptimizationLevel::Os : llvm::OptimizationLevel::Oz;
		}
		else
		{
			switch (opt_level)
			{
			case 1:
				return llvm::OptimizationLevel::O1;
			case 2:
				return llvm::OptimizationLevel::O2;
			default:
				return llvm::OptimizationLevel::O3;
			}
		}
	}();

	for ([[maybe_unused]] auto const _ : bz::iota(0, max_opt_iter_count))
	{
		llvm::LoopAnalysisManager loop_analysis_manager;
		llvm::FunctionAnalysisManager function_analysis_manager;
		llvm::CGSCCAnalysisManager cgscc_analysis_manager;
		llvm::ModuleAnalysisManager module_analysis_manager;

		auto builder = get_pass_builder(this->_target_machine.get());

		builder.registerModuleAnalyses(module_analysis_manager);
		builder.registerCGSCCAnalyses(cgscc_analysis_manager);
		builder.registerFunctionAnalyses(function_analysis_manager);
		builder.registerLoopAnalyses(loop_analysis_manager);
		builder.crossRegisterProxies(
			loop_analysis_manager,
			function_analysis_manager,
			cgscc_analysis_manager,
			module_analysis_manager
		);

		auto pass_manager = builder.buildPerModuleDefaultPipeline(llvm_opt_level);

		pass_manager.run(module, module_analysis_manager);
	}

	// always return true
	return true;
}

} // namespace ctx
