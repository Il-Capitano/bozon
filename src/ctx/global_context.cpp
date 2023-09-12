#include "global_context.h"
#include "ast/statement.h"
#include "cl_options.h"
#include "colors.h"
#include "comptime/codegen_context.h"
#include "comptime/codegen.h"

namespace ctx
{

ast::scope_t get_default_decls(ast::scope_t *builtin_global_scope)
{
	ast::scope_t result;
	auto &global_scope = result.emplace<ast::global_scope_t>();
	global_scope.parent = { builtin_global_scope, 0 };
	return result;
}

global_context::global_context(void)
	: _compile_decls{},
	  _errors{},
	  _builtin_universal_functions(ast::make_builtin_universal_functions()),
	  _builtin_type_infos{},
	  _builtin_functions{},
	  _builtin_operators{},
	  _builtin_global_scope()
{}

global_context::~global_context(void) noexcept = default;

src_file *global_context::get_src_file(fs::path const &file_path)
{
	bz_assert(fs::canonical(file_path).make_preferred() == file_path);
	auto const it = this->_src_files_map.find(file_path);
	if (it != this->_src_files_map.end())
	{
		return it->second;
	}
	else
	{
		return nullptr;
	}
}

bz::array_view<bz::u8string_view const> global_context::get_scope_in_persistent_storage(bz::array_view<bz::u8string const> scope)
{
	auto &result = this->_src_scopes_storage.emplace_back();

	for (auto const &fragment : scope)
	{
		auto &fragment_storage = this->_src_scope_fragments.push_back(std::make_unique<char[]>(fragment.size()));
		std::memcpy(fragment_storage.get(), fragment.data_as_char_ptr(), fragment.size());
		result.push_back(bz::u8string_view(fragment_storage.get(), fragment_storage.get() + fragment.size()));
	}

	return result;
}

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
	bz_assert(this->_builtin_functions[kind] != nullptr || kind == ast::function_body::builtin_panic_handler);

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

ast::decl_operator *global_context::get_builtin_operator(uint32_t op_kind_, uint8_t expr_type_kind_)
{
	using op_enum_t = builtin_type_arithmetic_operator_table_t::unary_array_t::index_type;
	using kind_enum_t = arithmetic_operator_table_t::index_type;

	auto const op_kind = static_cast<op_enum_t>(op_kind_);
	auto const expr_type_kind = static_cast<kind_enum_t>(expr_type_kind_);

	if (
		this->_arithmetic_operators.is_valid_index(expr_type_kind)
		&& this->_arithmetic_operators[expr_type_kind].unary_ops.is_valid_index(op_kind)
	)
	{
		return this->_arithmetic_operators[expr_type_kind].unary_ops[op_kind];
	}
	else if (
		this->_bitwise_operators.is_valid_index(expr_type_kind)
		&& this->_bitwise_operators[expr_type_kind].unary_ops.is_valid_index(op_kind)
	)
	{
		return this->_bitwise_operators[expr_type_kind].unary_ops[op_kind];
	}
	else
	{
		return nullptr;
	}
}

ast::decl_operator *global_context::get_builtin_operator(uint32_t op_kind_, uint8_t lhs_type_kind_, uint8_t rhs_type_kind_)
{
	using op_enum_t = builtin_type_basic_operator_table_t::binary_array_t::index_type;
	using kind_enum_t = basic_operator_table_t::index_type;

	auto const op_kind = static_cast<op_enum_t>(op_kind_);
	auto const lhs_type_kind = static_cast<kind_enum_t>(lhs_type_kind_);
	auto const rhs_type_kind = static_cast<kind_enum_t>(rhs_type_kind_);

	if (op_kind == lex::token::bit_left_shift)
	{
		bz_assert(this->_bitwise_operators.is_valid_index(lhs_type_kind));
		if (ast::is_signed_integer_kind(rhs_type_kind))
		{
			return this->_bitwise_operators[lhs_type_kind].signed_bit_left_shift;
		}
		else
		{
			return this->_bitwise_operators[lhs_type_kind].unsigned_bit_left_shift;
		}
	}
	else if (op_kind == lex::token::bit_left_shift_eq)
	{
		bz_assert(this->_bitwise_operators.is_valid_index(lhs_type_kind));
		if (ast::is_signed_integer_kind(rhs_type_kind))
		{
			return this->_bitwise_operators[lhs_type_kind].signed_bit_left_shift_eq;
		}
		else
		{
			return this->_bitwise_operators[lhs_type_kind].unsigned_bit_left_shift_eq;
		}
	}
	else if (op_kind == lex::token::bit_right_shift)
	{
		bz_assert(this->_bitwise_operators.is_valid_index(lhs_type_kind));
		if (ast::is_signed_integer_kind(rhs_type_kind))
		{
			return this->_bitwise_operators[lhs_type_kind].signed_bit_right_shift;
		}
		else
		{
			return this->_bitwise_operators[lhs_type_kind].unsigned_bit_right_shift;
		}
	}
	else if (op_kind == lex::token::bit_right_shift_eq)
	{
		bz_assert(this->_bitwise_operators.is_valid_index(lhs_type_kind));
		if (ast::is_signed_integer_kind(rhs_type_kind))
		{
			return this->_bitwise_operators[lhs_type_kind].signed_bit_right_shift_eq;
		}
		else
		{
			return this->_bitwise_operators[lhs_type_kind].unsigned_bit_right_shift_eq;
		}
	}
	else if (
		lhs_type_kind == rhs_type_kind
		&& this->_basic_operators.is_valid_index(lhs_type_kind)
		&& this->_basic_operators[lhs_type_kind].binary_ops.is_valid_index(op_kind)
	)
	{
		return this->_basic_operators[lhs_type_kind].binary_ops[op_kind];
	}
	else if (
		lhs_type_kind == rhs_type_kind
		&& this->_arithmetic_operators.is_valid_index(lhs_type_kind)
		&& this->_arithmetic_operators[lhs_type_kind].binary_ops.is_valid_index(op_kind)
	)
	{
		return this->_arithmetic_operators[lhs_type_kind].binary_ops[op_kind];
	}
	else if (
		lhs_type_kind == rhs_type_kind
		&& this->_bitwise_operators.is_valid_index(lhs_type_kind)
		&& this->_bitwise_operators[lhs_type_kind].binary_ops.is_valid_index(op_kind)
	)
	{
		return this->_bitwise_operators[lhs_type_kind].binary_ops[op_kind];
	}
	else
	{
		return nullptr;
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
	auto &file = *this->_src_files[file_id];
	bz_assert(file._file_id == file_id);
	return file;
}

src_file const &global_context::get_src_file(uint32_t file_id) const noexcept
{
	auto const &file = *this->_src_files[file_id];
	bz_assert(file._file_id == file_id);
	return file;
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

static std::pair<fs::path, bool> search_for_source_file(
	ast::identifier const &id,
	fs::path const &current_path,
	bz::array_view<fs::path const> import_dirs
)
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
		bz_assert(!fs::exists(same_dir_module) || fs::canonical(same_dir_module).make_preferred() == same_dir_module);
		if (fs::exists(same_dir_module))
		{
			return { std::move(same_dir_module), false };
		}
		auto same_dir_module_folder = current_path / module_folder_name_sv;
		bz_assert(!fs::exists(same_dir_module_folder) || fs::canonical(same_dir_module_folder).make_preferred() == same_dir_module_folder);
		if (fs::exists(same_dir_module_folder) && fs::is_directory(same_dir_module_folder))
		{
			return { std::move(same_dir_module_folder), false };
		}
	}

	if (allow_library)
	{
		for (auto const &import_dir : import_dirs)
		{
			auto library_module = import_dir / module_file_name_sv;
			bz_assert(!fs::exists(library_module) || fs::canonical(library_module).make_preferred() == library_module);
			if (fs::exists(library_module))
			{
				return { std::move(library_module), true };
			}
			auto library_module_folder = import_dir / module_folder_name_sv;
			bz_assert(!fs::exists(library_module_folder) || fs::canonical(library_module_folder).make_preferred() == library_module_folder);
			if (fs::exists(library_module_folder) && fs::is_directory(library_module_folder))
			{
				return { std::move(library_module_folder), true };
			}
		}
	}
	return {};
}

static uint32_t add_module_file(
	src_file const &current_file,
	fs::path module_path,
	bool is_library_file,
	bz::vector<bz::u8string> scope,
	global_context &context
)
{
	auto &file = [&]() -> auto & {
		auto const file_it = context.get_src_file(module_path);
		if (file_it == nullptr)
		{
			return context.emplace_src_file(
				std::move(module_path), context._src_files.size(), std::move(scope), is_library_file || current_file._is_library_file
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

static bz::vector<global_context::module_info_t> add_module_folder(
	src_file const &current_file,
	fs::path module_path,
	bool is_library_folder,
	bz::vector<bz::u8string> &scope,
	global_context &context
)
{
	bz::vector<global_context::module_info_t> result;
	bz_assert(fs::is_directory(module_path));
	for (auto const &p : bz::basic_range(fs::directory_iterator(module_path), fs::directory_iterator()))
	{
		auto const filename = p.path().filename().generic_string();
		if (is_library_folder && filename.starts_with('_'))
		{
			continue;
		}

		if (p.is_directory())
		{
			auto const &folder_name_s = filename;
			auto const folder_name = bz::u8string_view(folder_name_s.data(), folder_name_s.data() + folder_name_s.size());
			bz_assert(folder_name.size() != 0);
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
		else if (filename.ends_with(".bz"))
		{
			auto path = p.path();
			path.make_preferred();
			auto const id = add_module_file(current_file, std::move(path), is_library_folder, scope, context);
			if (id != std::numeric_limits<uint32_t>::max())
			{
				result.push_back({
					.id = id,
					.scope = context.get_scope_in_persistent_storage(scope),
				});
			}
		}
	}
	return result;
}

bz::vector<global_context::module_info_t> global_context::add_module(uint32_t current_file_id, ast::identifier const &id)
{
	auto &current_file = this->get_src_file(current_file_id);
	auto [module_path, is_library_path] = search_for_source_file(
		id,
		current_file.get_file_path().parent_path(),
		this->_import_dirs
	);
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
		return {};
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
		auto const result = add_module_file(current_file, std::move(module_path), is_library_path, std::move(scope), *this);
		if (result == std::numeric_limits<uint32_t>::max())
		{
			return {};
		}
		else
		{
			return { module_info_t{ result, id.values.slice(0, id.values.size() - 1) } };
		}
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
		return add_module_folder(current_file, std::move(module_path), is_library_path, scope, *this);
	}
}

ast::scope_t &global_context::get_file_global_scope(uint32_t file_id)
{
	return this->get_src_file(file_id)._global_scope;
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

		if (op_decl->body.params[0].get_type().remove_mut_reference().is<ast::ts_base_type>())
		{
			using kind_enum_t = arithmetic_operator_table_t::index_type;
			using op_enum_t = builtin_type_arithmetic_operator_table_t::unary_array_t::index_type;

			auto const kind = static_cast<kind_enum_t>(
				op_decl->body.params[0]
					.get_type()
					.remove_mut_reference()
					.get<ast::ts_base_type>().info->kind
			);
			auto const op_kind = static_cast<op_enum_t>(op);

			if (
				this->_arithmetic_operators.is_valid_index(kind)
				&& this->_arithmetic_operators[kind].unary_ops.is_valid_index(op_kind)
			)
			{
				bz_assert(this->_arithmetic_operators[kind].unary_ops[op_kind] == nullptr);
				this->_arithmetic_operators[kind].unary_ops[op_kind] = op_decl;
			}
			else if (
				this->_bitwise_operators.is_valid_index(kind)
				&& this->_bitwise_operators[kind].unary_ops.is_valid_index(op_kind)
			)
			{
				bz_assert(this->_bitwise_operators[kind].unary_ops[op_kind] == nullptr);
				this->_bitwise_operators[kind].unary_ops[op_kind] = op_decl;
			}
		}

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

		if (
			op_decl->body.params[0].get_type().remove_mut_reference().is<ast::ts_base_type>()
			&& op_decl->body.params[1].get_type().is<ast::ts_base_type>()
		)
		{
			using kind_enum_t = basic_operator_table_t::index_type;
			using op_enum_t = builtin_type_basic_operator_table_t::binary_array_t::index_type;

			auto const lhs_kind = static_cast<kind_enum_t>(
				op_decl->body.params[0]
					.get_type()
					.remove_mut_reference()
					.get<ast::ts_base_type>().info->kind
			);
			auto const rhs_kind = static_cast<kind_enum_t>(
				op_decl->body.params[1]
					.get_type()
					.get<ast::ts_base_type>().info->kind
			);
			auto const op_kind = static_cast<op_enum_t>(op);

			if (op_kind == lex::token::bit_left_shift)
			{
				bz_assert(this->_bitwise_operators.is_valid_index(lhs_kind));
				if (ast::is_signed_integer_kind(rhs_kind))
				{
					bz_assert(this->_bitwise_operators[lhs_kind].signed_bit_left_shift == nullptr);
					this->_bitwise_operators[lhs_kind].signed_bit_left_shift = op_decl;
				}
				else
				{
					bz_assert(this->_bitwise_operators[lhs_kind].unsigned_bit_left_shift == nullptr);
					this->_bitwise_operators[lhs_kind].unsigned_bit_left_shift = op_decl;
				}
			}
			else if (op_kind == lex::token::bit_left_shift_eq)
			{
				bz_assert(this->_bitwise_operators.is_valid_index(lhs_kind));
				if (ast::is_signed_integer_kind(rhs_kind))
				{
					bz_assert(this->_bitwise_operators[lhs_kind].signed_bit_left_shift_eq == nullptr);
					this->_bitwise_operators[lhs_kind].signed_bit_left_shift_eq = op_decl;
				}
				else
				{
					bz_assert(this->_bitwise_operators[lhs_kind].unsigned_bit_left_shift_eq == nullptr);
					this->_bitwise_operators[lhs_kind].unsigned_bit_left_shift_eq = op_decl;
				}
			}
			else if (op_kind == lex::token::bit_right_shift)
			{
				bz_assert(this->_bitwise_operators.is_valid_index(lhs_kind));
				if (ast::is_signed_integer_kind(rhs_kind))
				{
					bz_assert(this->_bitwise_operators[lhs_kind].signed_bit_right_shift == nullptr);
					this->_bitwise_operators[lhs_kind].signed_bit_right_shift = op_decl;
				}
				else
				{
					bz_assert(this->_bitwise_operators[lhs_kind].unsigned_bit_right_shift == nullptr);
					this->_bitwise_operators[lhs_kind].unsigned_bit_right_shift = op_decl;
				}
			}
			else if (op_kind == lex::token::bit_right_shift_eq)
			{
				bz_assert(this->_bitwise_operators.is_valid_index(lhs_kind));
				if (ast::is_signed_integer_kind(rhs_kind))
				{
					bz_assert(this->_bitwise_operators[lhs_kind].signed_bit_right_shift_eq == nullptr);
					this->_bitwise_operators[lhs_kind].signed_bit_right_shift_eq = op_decl;
				}
				else
				{
					bz_assert(this->_bitwise_operators[lhs_kind].unsigned_bit_right_shift_eq == nullptr);
					this->_bitwise_operators[lhs_kind].unsigned_bit_right_shift_eq = op_decl;
				}
			}
			else if (
				lhs_kind == rhs_kind
				&& this->_basic_operators.is_valid_index(lhs_kind)
				&& this->_basic_operators[lhs_kind].binary_ops.is_valid_index(op_kind)
			)
			{
				bz_assert(this->_basic_operators[lhs_kind].binary_ops[op_kind] == nullptr);
				this->_basic_operators[lhs_kind].binary_ops[op_kind] = op_decl;
			}
			else if (
				lhs_kind == rhs_kind
				&& this->_arithmetic_operators.is_valid_index(lhs_kind)
				&& this->_arithmetic_operators[lhs_kind].binary_ops.is_valid_index(op_kind)
			)
			{
				bz_assert(this->_arithmetic_operators[lhs_kind].binary_ops[op_kind] == nullptr);
				this->_arithmetic_operators[lhs_kind].binary_ops[op_kind] = op_decl;
			}
			else if (
				lhs_kind == rhs_kind
				&& this->_bitwise_operators.is_valid_index(lhs_kind)
				&& this->_bitwise_operators[lhs_kind].binary_ops.is_valid_index(op_kind)
			)
			{
				bz_assert(this->_bitwise_operators[lhs_kind].binary_ops[op_kind] == nullptr);
				this->_bitwise_operators[lhs_kind].binary_ops[op_kind] = op_decl;
			}
		}

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
	auto const pointer_size = this->comptime_codegen_context->machine_parameters.pointer_size;
	bz_assert(pointer_size == 8 || pointer_size == 4);
	return pointer_size == 8
		? this->get_builtin_type_info(ast::type_info::uint64_)
		: this->get_builtin_type_info(ast::type_info::uint32_);
}

ast::type_info *global_context::get_isize_type_info_for_builtin_alias(void) const
{
	auto const pointer_size = this->comptime_codegen_context->machine_parameters.pointer_size;
	bz_assert(pointer_size == 8 || pointer_size == 4);
	return pointer_size == 8
		? this->get_builtin_type_info(ast::type_info::int64_)
		: this->get_builtin_type_info(ast::type_info::int32_);
}

size_t global_context::get_pointer_size(void) const
{
	return this->comptime_codegen_context->machine_parameters.pointer_size;
}

bool global_context::is_aggressive_consteval_enabled(void) const
{
	auto const &optimizations = ctcli::option_value<ctcli::option("--opt")>;
	return optimizations.contains(ctcli::group_element("--opt aggressive-consteval"));
}

bz::optional<uint32_t> global_context::get_machine_code_opt_level(void) const
{
	if (ctcli::is_option_set<ctcli::group_element("--opt machine-code-opt-level")>())
	{
		return global_data::machine_code_opt_level;
	}
	else
	{
		return {};
	}
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
		global_data::compile_until = compilation_phase::parse_command_line;
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
		global_data::compile_until = compilation_phase::parse_command_line;
		return true;
	}
	else if (global_data::display_version)
	{
		print_version_info();
		global_data::compile_until = compilation_phase::parse_command_line;
		return true;
	}

	auto &positional_args = ctcli::positional_arguments<ctcli::options_id_t::def>;
	if (positional_args.size() >= 2)
	{
		this->report_error("only one source file may be provided");
	}

	if (positional_args.size() == 1)
	{
		global_data::source_file = positional_args[0];
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

[[nodiscard]] bool global_context::initialize_target_info(void)
{
	this->target_triple = codegen::target_triple::parse(global_data::target);

	auto const target_properties = this->target_triple.get_target_properties();

	bool error = false;

	if (
		!target_properties.pointer_size.has_value()
		&& !ctcli::is_option_set<ctcli::group_element("--code-gen target-pointer-size")>()
	)
	{
		this->report_error(bz::format(
			"unable to infer target pointer size from triple '{}'; provide the command line option '-C pointer-size=<size>'",
			this->target_triple.triple
		));
		error = true;
	}
	else if (
		target_properties.pointer_size.has_value()
		&& ctcli::is_option_set<ctcli::group_element("--code-gen target-pointer-size")>()
		&& target_properties.pointer_size.get() != global_data::target_pointer_size
	)
	{
		this->report_error(bz::format(
			"inferred and explicitly provided target pointer sizes of {} and {} are different for triple '{}'",
			target_properties.pointer_size.get(), global_data::target_pointer_size, this->target_triple.triple
		));
		error = true;
	}

	if (
		!target_properties.endianness.has_value()
		&& !ctcli::is_option_set<ctcli::group_element("--code-gen target-endianness")>()
	)
	{
		this->report_error(bz::format(
			"unable to infer target endianness from triple '{}'; provide the command line option '-C target-endianness={}'",
			this->target_triple.triple, "{little|big}" // needed here because it contains '{'
		));
		error = true;
	}
	else if (
		target_properties.endianness.has_value()
		&& ctcli::is_option_set<ctcli::group_element("--code-gen target-endianness")>()
		&& ![&]() {
			auto const inferred = target_properties.endianness.get();
			auto const provided = global_data::target_endianness;
			return (inferred == comptime::memory::endianness_kind::little && provided == target_endianness_kind::little)
				|| (inferred == comptime::memory::endianness_kind::big && provided == target_endianness_kind::big);
		}()
	)
	{
		this->report_error(bz::format(
			"inferred and explicitly provided target endianness kinds are different for triple '{}'",
			this->target_triple.triple
		));
		error = true;
	}

	if (
		global_data::emit_file_type == emit_type::c
		&& !target_properties.c_short_size.has_value()
		&& !ctcli::is_option_set<ctcli::group_element("--code-gen target-c-short-size")>()
	)
	{
		this->report_error(bz::format(
			"unable to infer the size of 'short' from triple '{}'; provide the command line option '-C target-c-short-size={}'",
			this->target_triple.triple, "{little|big}" // needed here because it contains '{'
		));
		error = true;
	}
	else if (
		target_properties.c_short_size.has_value()
		&& ctcli::is_option_set<ctcli::group_element("--code-gen target-c-short-size")>()
		&& target_properties.c_short_size.get() != global_data::target_c_short_size
	)
	{
		this->report_error(bz::format(
			"inferred and explicitly provided 'short' sizes of {} and {} are different for triple '{}'",
			target_properties.c_short_size.get(), global_data::target_c_short_size, this->target_triple.triple
		));
		error = true;
	}

	if (
		global_data::emit_file_type == emit_type::c
		&& !target_properties.c_int_size.has_value()
		&& !ctcli::is_option_set<ctcli::group_element("--code-gen target-c-int-size")>()
	)
	{
		this->report_error(bz::format(
			"unable to infer the size of 'int' from triple '{}'; provide the command line option '-C target-c-int-size={}'",
			this->target_triple.triple, "{little|big}" // needed here because it contains '{'
		));
		error = true;
	}
	else if (
		target_properties.c_int_size.has_value()
		&& ctcli::is_option_set<ctcli::group_element("--code-gen target-c-int-size")>()
		&& target_properties.c_int_size.get() != global_data::target_c_int_size
	)
	{
		this->report_error(bz::format(
			"inferred and explicitly provided 'int' sizes of {} and {} are different for triple '{}'",
			target_properties.c_int_size.get(), global_data::target_c_int_size, this->target_triple.triple
		));
		error = true;
	}

	if (
		global_data::emit_file_type == emit_type::c
		&& !target_properties.c_long_size.has_value()
		&& !ctcli::is_option_set<ctcli::group_element("--code-gen target-c-long-size")>()
	)
	{
		this->report_error(bz::format(
			"unable to infer the size of 'long' from triple '{}'; provide the command line option '-C target-c-long-size={}'",
			this->target_triple.triple, "{little|big}" // needed here because it contains '{'
		));
		error = true;
	}
	else if (
		target_properties.c_long_size.has_value()
		&& ctcli::is_option_set<ctcli::group_element("--code-gen target-c-long-size")>()
		&& target_properties.c_long_size.get() != global_data::target_c_long_size
	)
	{
		this->report_error(bz::format(
			"inferred and explicitly provided 'long' sizes of {} and {} are different for triple '{}'",
			target_properties.c_long_size.get(), global_data::target_c_long_size, this->target_triple.triple
		));
		error = true;
	}

	if (
		global_data::emit_file_type == emit_type::c
		&& !target_properties.c_long_long_size.has_value()
		&& !ctcli::is_option_set<ctcli::group_element("--code-gen target-c-long-long-size")>()
	)
	{
		this->report_error(bz::format(
			"unable to infer the size of 'long long' from triple '{}'; provide the command line option '-C target-c-long-long-size={}'",
			this->target_triple.triple, "{little|big}" // needed here because it contains '{'
		));
		error = true;
	}
	else if (
		target_properties.c_long_long_size.has_value()
		&& ctcli::is_option_set<ctcli::group_element("--code-gen target-c-long-long-size")>()
		&& target_properties.c_long_long_size.get() != global_data::target_c_long_long_size
	)
	{
		this->report_error(bz::format(
			"inferred and explicitly provided 'long long' sizes of {} and {} are different for triple '{}'",
			target_properties.c_long_long_size.get(), global_data::target_c_long_long_size, this->target_triple.triple
		));
		error = true;
	}

	if (error)
	{
		return false;
	}

	auto const pointer_size = target_properties.pointer_size.has_value() ? target_properties.pointer_size.get() : global_data::target_pointer_size;
	auto const endianness = [&]() {
		if (target_properties.endianness.has_value())
		{
			return target_properties.endianness.get();
		}
		else if (global_data::target_endianness == target_endianness_kind::little)
		{
			return comptime::memory::endianness_kind::little;
		}
		else
		{
			return comptime::memory::endianness_kind::big;
		}
	}();

	if (pointer_size != 8 && pointer_size != 4)
	{
		this->report_error(bz::format("target pointer size of {} is not supported", pointer_size));
		return false;
	}

	auto const machine_parameters = comptime::machine_parameters_t{
		.pointer_size = static_cast<size_t>(pointer_size),
		.endianness = endianness,
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

	auto const normalized_target_triple = this->target_triple.get_normalized_target();
	std::error_code ec;
	auto stdlib_dir_path = fs::canonical(
		fs::path(std::string_view(
			global_data::stdlib_dir.data_as_char_ptr(),
			global_data::stdlib_dir.size()
		), fs::path::native_format),
		ec
	);
	stdlib_dir_path.make_preferred();
	if (ec || !fs::exists(stdlib_dir_path))
	{
		this->report_error(bz::format("invalid path '{}' specified for '--stdlib-dir'", global_data::stdlib_dir));
		return false;
	}

	auto common_dir = stdlib_dir_path / "common";
	auto target_dir = stdlib_dir_path / std::string_view(normalized_target_triple.data_as_char_ptr(), normalized_target_triple.size());
	if (fs::exists(common_dir))
	{
		this->_import_dirs.push_back(std::move(common_dir));
	}

	if (!global_data::freestanding)
	{
		if (fs::exists(target_dir))
		{
			this->_import_dirs.push_back(target_dir);
		}
		else if (auto generic_target_dir = stdlib_dir_path / "generic"; fs::exists(generic_target_dir))
		{
			target_dir = std::move(generic_target_dir);
			this->_import_dirs.push_back(target_dir);
		}
	}

	for (auto const &import_dir : global_data::import_dirs)
	{
		std::string_view import_dir_sv(import_dir.data_as_char_ptr(), import_dir.size());
		// avoid throws
		std::error_code ec;
		auto import_dir_path = fs::canonical(fs::path(import_dir_sv), ec);
		import_dir_path.make_preferred();
		if (!ec && fs::exists(import_dir_path))
		{
			this->_import_dirs.push_back(std::move(import_dir_path));
		}
	}

	{
		auto const builtins_file_path = stdlib_dir_path / "compiler/__builtins.bz";
		if (fs::exists(builtins_file_path) && fs::is_regular_file(builtins_file_path))
		{
			auto &builtins_file = this->emplace_src_file(
				builtins_file_path, this->_src_files.size(), bz::vector<bz::u8string>(), true
			);
			this->_builtin_global_scope = &builtins_file._global_scope;
			if (!builtins_file.parse_global_symbols(*this))
			{
				return false;
			}

			if (!builtins_file.parse_builtins(*this))
			{
				return false;
			}
		}
	}

	if (!global_data::freestanding)
	{
		auto const builtins_file_path = target_dir / "__builtins.bz";
		if (fs::exists(builtins_file_path) && fs::is_regular_file(builtins_file_path))
		{
			auto &builtins_file = this->emplace_src_file(
				builtins_file_path, this->_src_files.size(), bz::vector<bz::u8string>(), true
			);
			if (!builtins_file.parse_global_symbols(*this))
			{
				return false;
			}

			if (!builtins_file.parse(*this))
			{
				return false;
			}
		}

		if (!global_data::no_main)
		{
			auto const main_file_path = target_dir / "__main.bz";
			if (fs::exists(main_file_path) && fs::is_regular_file(main_file_path))
			{
				auto &main_file = this->emplace_src_file(
					main_file_path, this->_src_files.size(), bz::vector<bz::u8string>(), true
				);
				if (!main_file.parse_global_symbols(*this))
				{
					return false;
				}

				if (!main_file.parse(*this))
				{
					return false;
				}
			}
		}
	}

	return true;
}

[[nodiscard]] bool global_context::parse_global_symbols(void)
{
	if (global_data::source_file == "")
	{
		this->report_error("no source file was provided");
		return false;
	}
	else if (global_data::source_file != "-" && !global_data::source_file.ends_with(".bz"))
	{
		this->report_error("source file name must end in '.bz'");
		return false;
	}

	auto source_file_path = fs::canonical(fs::path(std::string_view(global_data::source_file.data_as_char_ptr(), global_data::source_file.size())));
	source_file_path.make_preferred();
	if (!fs::exists(source_file_path))
	{
		this->report_error(bz::format("invalid source file '{}': file does not exist", global_data::source_file));
		return false;
	}
	else if (!fs::is_regular_file(source_file_path))
	{
		this->report_error(bz::format("invalid source file '{}': file is not a regular file", global_data::source_file));
		return false;
	}

	auto &file = this->emplace_src_file(
		std::move(source_file_path), this->_src_files.size(), bz::vector<bz::u8string>(), false
	);
	if (!file.parse_global_symbols(*this))
	{
		return false;
	}

	return true;
}

[[nodiscard]] bool global_context::parse(void)
{
	for (auto const &file : this->_src_files)
	{
		if (!file->parse(*this))
		{
			return false;
		}
	}

	return true;
}

[[nodiscard]] bool global_context::initialize_backend(void)
{
	if (global_data::emit_file_type == emit_type::null)
	{
		return true;
	}

	this->backend_context = codegen::create_backend_context(*this);
	return this->backend_context != nullptr;
}

[[nodiscard]] bool global_context::generate_and_output_code(void)
{
	if (global_data::emit_file_type == emit_type::null || this->backend_context == nullptr)
	{
		return true;
	}
#ifndef NDEBUG
	else if (global_data::debug_no_emit_file)
	{
		return this->backend_context->generate_and_output_code(*this, {});
	}
#endif // !NDEBUG
	else if (global_data::output_file_name != "")
	{
		return this->backend_context->generate_and_output_code(*this, global_data::output_file_name);
	}
	else
	{
		auto const file_extension = [&]() -> bz::u8string_view {
			switch (global_data::emit_file_type)
			{
			case emit_type::obj:
				return ".o";
			case emit_type::asm_:
				return ".s";
			case emit_type::llvm_bc:
				return ".bc";
			case emit_type::llvm_ir:
				return ".ll";
			case emit_type::c:
				return ".c";
			case emit_type::null:
				bz_unreachable;
			}
		}();

		auto const slash_it = global_data::source_file.rfind_any("/\\");
		auto const dot = global_data::source_file.rfind('.');
		bz_assert(dot != bz::u8iterator{});
		auto const output_path = bz::format(
			"{}{}",
			bz::u8string(
				slash_it == bz::u8iterator{} ? global_data::source_file.begin() : slash_it + 1,
				dot
			),
			file_extension
		);
		return this->backend_context->generate_and_output_code(*this, output_path);
	}
}

} // namespace ctx
