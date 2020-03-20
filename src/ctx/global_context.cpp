#include "global_context.h"
#include "parser.h"

namespace ctx
{

static std::list<ast::type_info> get_default_types(void)
{
	return {
		ast::type_info{ ast::type_info::type_kind::int8_,    "int8",    1,  1, ast::type_info::default_built_in_flags, nullptr },
		ast::type_info{ ast::type_info::type_kind::int16_,   "int16",   2,  2, ast::type_info::default_built_in_flags, nullptr },
		ast::type_info{ ast::type_info::type_kind::int32_,   "int32",   4,  4, ast::type_info::default_built_in_flags, nullptr },
		ast::type_info{ ast::type_info::type_kind::int64_,   "int64",   8,  8, ast::type_info::default_built_in_flags, nullptr },
		ast::type_info{ ast::type_info::type_kind::uint8_,   "uint8",   1,  1, ast::type_info::default_built_in_flags, nullptr },
		ast::type_info{ ast::type_info::type_kind::uint16_,  "uint16",  2,  2, ast::type_info::default_built_in_flags, nullptr },
		ast::type_info{ ast::type_info::type_kind::uint32_,  "uint32",  4,  4, ast::type_info::default_built_in_flags, nullptr },
		ast::type_info{ ast::type_info::type_kind::uint64_,  "uint64",  8,  8, ast::type_info::default_built_in_flags, nullptr },
		ast::type_info{ ast::type_info::type_kind::float32_, "float32", 4,  4, ast::type_info::default_built_in_flags, nullptr },
		ast::type_info{ ast::type_info::type_kind::float64_, "float64", 8,  8, ast::type_info::default_built_in_flags, nullptr },
		ast::type_info{ ast::type_info::type_kind::char_,    "char",    4,  4, ast::type_info::default_built_in_flags, nullptr },
		ast::type_info{ ast::type_info::type_kind::str_,     "str",     16, 8, ast::type_info::default_built_in_flags, nullptr },
		ast::type_info{ ast::type_info::type_kind::bool_,    "bool",    1,  1, ast::type_info::default_built_in_flags, nullptr },
		ast::type_info{ ast::type_info::type_kind::null_t_,  "null_t",  0,  0, ast::type_info::default_built_in_flags, nullptr },
	};
}


file_decls &global_context::get_decls(uint32_t file_id)
{
	auto const it = std::find_if(
		this->_file_decls.begin(), this->_file_decls.end(),
		[file_id](auto const &file_decls) {
			return file_id == file_decls.file_id;
		}
	);
	assert(it != this->_file_decls.end());
	return *it;
}

global_context::global_context(void)
	: _file_decls(), _types(get_default_types())
{}

uint32_t global_context::get_file_id(bz::string_view file)
{
	if (this->_file_decls.empty())
	{
		this->_file_decls.push_back({ 0, file, {}, {} });
		return 0;
	}

	auto const it = std::find_if(
		this->_file_decls.begin(), this->_file_decls.end(),
		[file](auto const &file_decls) {
			return file == file_decls.file_name;
		}
	);
	if (it == this->_file_decls.end())
	{
		uint32_t const new_id = this->_file_decls.size();
		this->_file_decls.push_back({ new_id, file, {}, {} });
		return new_id;
	}

	return it->file_id;
}


void global_context::report_ambiguous_id_error(uint32_t file_id, lex::token_pos id)
{
	auto &file_decls = this->get_decls(file_id);

	auto export_set = std::find_if(
		file_decls.export_decls.func_sets.begin(), file_decls.export_decls.func_sets.end(),
		[id = id->value](auto const &set) {
			return id == set.id;
		}
	);
	auto const internal_set = std::find_if(
		file_decls.internal_decls.func_sets.begin(), file_decls.internal_decls.func_sets.end(),
		[id = id->value](auto const &set) {
			return id == set.id;
		}
	);
	assert(
		(export_set != file_decls.export_decls.func_sets.end())
		|| (internal_set != file_decls.internal_decls.func_sets.end())
	);

	bz::vector<note> notes = {};
	if (export_set != file_decls.export_decls.func_sets.end())
	{
		for (auto &decl : export_set->func_decls)
		{
			notes.emplace_back(make_note(
				decl->identifier, "candidate:"
			));
		}
	}
	if (internal_set != file_decls.internal_decls.func_sets.end())
	{
		for (auto &decl : internal_set->func_decls)
		{
			notes.emplace_back(make_note(
				decl->identifier, "candidate:"
			));
		}
	}

	this->_errors.emplace_back(make_error(id, "identifier is ambiguous", std::move(notes)));
}



auto global_context::add_export_declaration(uint32_t file_id, ast::declaration &decl)
	-> bz::result<int, error>
{
	switch (decl.kind())
	{
	case ast::declaration::index<ast::decl_variable>:
		return this->add_export_variable(file_id, *decl.get<ast::decl_variable_ptr>());
	case ast::declaration::index<ast::decl_function>:
		return this->add_export_function(file_id, *decl.get<ast::decl_function_ptr>());
	case ast::declaration::index<ast::decl_operator>:
		return this->add_export_operator(file_id, *decl.get<ast::decl_operator_ptr>());
	case ast::declaration::index<ast::decl_struct>:
		return this->add_export_struct(file_id, *decl.get<ast::decl_struct_ptr>());
	default:
		assert(false);
		return 1;
	}
}

static auto check_for_id_redeclaration(
	file_decls const &file_decls,
	lex::token_pos id,
	bool check_all
) -> bz::result<int, error>
{
	// check global variables
	{
		auto const export_it = std::find_if(
			file_decls.export_decls.var_decls.begin(), file_decls.export_decls.var_decls.end(),
			[id = id->value](auto const &v) {
				return id == v->identifier->value;
			}
		);
		if (export_it != file_decls.export_decls.var_decls.end())
		{
			return make_error(
				id,
				bz::format("redeclaration of a global name '{}'", id->value),
				{ make_note((*export_it)->identifier, "previous declaration:") }
			);
		}

		auto const internal_it = std::find_if(
			file_decls.internal_decls.var_decls.begin(), file_decls.internal_decls.var_decls.end(),
			[id = id->value](auto const &v) {
				return id == v->identifier->value;
			}
		);
		if (internal_it != file_decls.internal_decls.var_decls.end())
		{
			return make_error(
				id,
				bz::format("redeclaration of a global name '{}'", id->value),
				{ make_note((*internal_it)->identifier, "previous declaration:") }
			);
		}
	}

	// check global functions if check_all is true
	if (check_all)
	{
		bz::vector<note> notes = {};

		auto const export_it = std::find_if(
			file_decls.export_decls.func_sets.begin(), file_decls.export_decls.func_sets.end(),
			[id = id->value](auto const &v) {
				return id == v.id;
			}
		);
		if (export_it != file_decls.export_decls.func_sets.end())
		{
			for (auto &decl : export_it->func_decls)
			{
				notes.emplace_back(make_note(decl->identifier, "previous declaration:"));
			}
		}

		auto const internal_it = std::find_if(
			file_decls.internal_decls.func_sets.begin(), file_decls.internal_decls.func_sets.end(),
			[id = id->value](auto const &v) {
				return id == v.id;
			}
		);
		if (internal_it != file_decls.internal_decls.func_sets.end())
		{
			for (auto &decl : internal_it->func_decls)
			{
				notes.emplace_back(make_note(decl->identifier, "previous declaration:"));
			}
		}

		if (!notes.empty())
		{
			return make_error(
				id,
				bz::format("redeclaration of a global name '{}'", id->value),
				std::move(notes)
			);
		}
	}

	return 0;
}

auto global_context::add_export_variable(uint32_t file_id, ast::decl_variable &var_decl)
	-> bz::result<int, error>
{
	auto &file_decls = this->get_decls(file_id);

	auto res = check_for_id_redeclaration(file_decls, var_decl.identifier, true);
	if (!res.has_error())
	{
		file_decls.export_decls.var_decls.push_back(&var_decl);
	}

	return res;
}

auto global_context::add_internal_variable(uint32_t file_id, ast::decl_variable &var_decl)
	-> bz::result<int, error>
{
	auto &file_decls = this->get_decls(file_id);

	auto res = check_for_id_redeclaration(file_decls, var_decl.identifier, true);
	if (!res.has_error())
	{
		file_decls.internal_decls.var_decls.push_back(&var_decl);
	}

	return res;
}

auto global_context::add_export_function(uint32_t file_id, ast::decl_function &func_decl)
	-> bz::result<int, error>
{
	auto &file_decls = this->get_decls(file_id);

	auto res = check_for_id_redeclaration(file_decls, func_decl.identifier, false);
	if (!res.has_error())
	{
		auto &func_sets = file_decls.export_decls.func_sets;
		auto set = std::find_if(
			func_sets.begin(), func_sets.end(),
			[id = func_decl.identifier->value](auto const &overload_set) {
				return id == overload_set.id;
			}
		);
		if (set == func_sets.end())
		{
			func_sets.push_back({ func_decl.identifier->value, { &func_decl } });
		}
		else
		{
			// TODO: check for conflicts
			set->func_decls.push_back(&func_decl);
		}
	}

	return res;
}

auto global_context::add_internal_function(uint32_t file_id, ast::decl_function &func_decl)
	-> bz::result<int, error>
{
	auto &file_decls = this->get_decls(file_id);

	auto res = check_for_id_redeclaration(file_decls, func_decl.identifier, false);
	if (!res.has_error())
	{
		auto &func_sets = file_decls.internal_decls.func_sets;
		auto set = std::find_if(
			func_sets.begin(), func_sets.end(),
			[id = func_decl.identifier->value](auto const &overload_set) {
				return id == overload_set.id;
			}
		);
		if (set == func_sets.end())
		{
			func_sets.push_back({ func_decl.identifier->value, { &func_decl } });
		}
		else
		{
			// TODO: check for conflicts
			set->func_decls.push_back(&func_decl);
		}
	}

	return res;
}

auto global_context::add_export_operator(uint32_t file_id, ast::decl_operator &op_decl)
	-> bz::result<int, error>
{
	auto &file_decls = this->get_decls(file_id);

	// TODO: check for redeclaration here
	auto &op_sets = file_decls.export_decls.op_sets;
	auto set = std::find_if(
		op_sets.begin(), op_sets.end(),
		[op = op_decl.op->kind](auto const &overload_set) {
			return op == overload_set.op;
		}
	);
	if (set == op_sets.end())
	{
		op_sets.push_back({ op_decl.op->kind, { &op_decl } });
	}
	else
	{
		// TODO: check for conflicts
		set->op_decls.push_back(&op_decl);
	}

	return 0;
}

auto global_context::add_internal_operator(uint32_t file_id, ast::decl_operator &op_decl)
	-> bz::result<int, error>
{
	auto &file_decls = this->get_decls(file_id);

	// TODO: check for redeclaration here
	auto &op_sets = file_decls.internal_decls.op_sets;
	auto set = std::find_if(
		op_sets.begin(), op_sets.end(),
		[op = op_decl.op->kind](auto const &overload_set) {
			return op == overload_set.op;
		}
	);
	if (set == op_sets.end())
	{
		op_sets.push_back({ op_decl.op->kind, { &op_decl } });
	}
	else
	{
		// TODO: check for conflicts
		set->op_decls.push_back(&op_decl);
	}

	return 0;
}

auto global_context::add_export_struct(uint32_t file_id, ast::decl_struct &struct_decl)
	-> bz::result<int, error>
{
	auto &file_decls = this->get_decls(file_id);

	auto const export_decl = std::find_if(
		file_decls.export_decls.struct_decls.begin(), file_decls.export_decls.struct_decls.end(),
		[id = struct_decl.identifier->value](auto const &decl) {
			return id == decl->identifier->value;
		}
	);
	if (export_decl != file_decls.export_decls.struct_decls.end())
	{
		return make_error(
			struct_decl.identifier,
			bz::format("redeclaration of type '{}'", struct_decl.identifier->value),
			{ make_note((*export_decl)->identifier, "previous declaration:") }
		);
	}

	auto const internal_decl = std::find_if(
		file_decls.internal_decls.struct_decls.begin(), file_decls.internal_decls.struct_decls.end(),
		[id = struct_decl.identifier->value](auto const &decl) {
			return id == decl->identifier->value;
		}
	);
	if (internal_decl != file_decls.internal_decls.struct_decls.end())
	{
		return make_error(
			struct_decl.identifier,
			bz::format("redeclaration of type '{}'", struct_decl.identifier->value),
			{ make_note((*internal_decl)->identifier, "previous declaration:") }
		);
	}

	file_decls.export_decls.struct_decls.push_back(&struct_decl);
	return 0;
}

auto global_context::add_internal_struct(uint32_t file_id, ast::decl_struct &struct_decl)
	-> bz::result<int, error>
{
	auto &file_decls = this->get_decls(file_id);

	auto const export_decl = std::find_if(
		file_decls.export_decls.struct_decls.begin(), file_decls.export_decls.struct_decls.end(),
		[id = struct_decl.identifier->value](auto const &decl) {
			return id == decl->identifier->value;
		}
	);
	if (export_decl != file_decls.export_decls.struct_decls.end())
	{
		return make_error(
			struct_decl.identifier,
			bz::format("redeclaration of type '{}'", struct_decl.identifier->value),
			{ make_note((*export_decl)->identifier, "previous declaration:") }
		);
	}

	auto const internal_decl = std::find_if(
		file_decls.internal_decls.struct_decls.begin(), file_decls.internal_decls.struct_decls.end(),
		[id = struct_decl.identifier->value](auto const &decl) {
			return id == decl->identifier->value;
		}
	);
	if (internal_decl != file_decls.internal_decls.struct_decls.end())
	{
		return make_error(
			struct_decl.identifier,
			bz::format("redeclaration of type '{}'", struct_decl.identifier->value),
			{ make_note((*internal_decl)->identifier, "previous declaration:") }
		);
	}

	file_decls.internal_decls.struct_decls.push_back(&struct_decl);
	return 0;
}

/*
static int get_type_match_level(
	ast::typespec const &dest,
	ast::expression::expr_type_t const &src
)
{
	int result = 0;

	auto dest_it = &dest;
	auto src_it = &src.expr_type;

	auto const advance = [](ast::typespec const *&ts)
	{
		switch (ts->kind())
		{
		case ast::typespec::index<ast::ts_reference>:
			ts = &ts->get<ast::ts_reference_ptr>()->base;
			break;
		case ast::typespec::index<ast::ts_constant>:
			ts = &ts->get<ast::ts_constant_ptr>()->base;
			break;
		case ast::typespec::index<ast::ts_pointer>:
			ts = &ts->get<ast::ts_pointer_ptr>()->base;
			break;
		default:
			assert(false);
			break;
		}
	};

	if (dest_it->kind() != ast::typespec::index<ast::ts_reference>)
	{
		++result;
		if (dest_it->kind() == ast::typespec::index<ast::ts_constant>)
		{
			advance(dest_it);
		}
		if (src_it->kind() == ast::typespec::index<ast::ts_constant>)
		{
			advance(src_it);
		}

		if (dest_it->kind() == ast::typespec::index<ast::ts_base_type>)
		{
			// TODO: use is_convertible
			if (
				src_it->kind() != ast::typespec::index<ast::ts_base_type>
				|| src_it->get<ast::ts_base_type_ptr>()->info != dest_it->get<ast::ts_base_type_ptr>()->info
			)
			{
				return -1;
			}
			else
			{
				return result;
			}
		}
	}
	// TODO: rvalue references...
	else
	{
		if (
			src.type_kind != ast::expression::lvalue
			&& src.type_kind != ast::expression::lvalue_reference
		)
		{
			return -1;
		}
		advance(dest_it);
	}

	while (true)
	{
		if (dest_it->kind() == ast::typespec::index<ast::ts_base_type>)
		{
			if (
				src_it->kind() != ast::typespec::index<ast::ts_base_type>
				|| src_it->get<ast::ts_base_type_ptr>()->info != dest_it->get<ast::ts_base_type_ptr>()->info
			)
			{
				return -1;
			}
			else
			{
				return result;
			}
		}
		else if (dest_it->kind() == src_it->kind())
		{
			advance(dest_it);
			advance(src_it);
		}
		else if (dest_it->kind() == ast::typespec::index<ast::ts_constant>)
		{
			advance(dest_it);
			++result;
		}
		else
		{
			return -1;
		}
	}
}
*/

ast::type_info const *global_context::get_type_info(uint32_t, bz::string_view id)
{
	auto const it = std::find_if(
		this->_types.begin(),
		this->_types.end(),
		[id](auto const &info) {
			return id == info.identifier;
		}
	);
	if (it == this->_types.end())
	{
		return nullptr;
	}
	else
	{
		return it.operator->();
	}
}

} // namespace ctx
