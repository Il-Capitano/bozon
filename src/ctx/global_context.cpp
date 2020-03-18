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

global_context::global_context(void)
	: _decls(), _types(get_default_types())
{}


void global_context::report_error(error &&err)
{
	this->_errors.emplace_back(std::move(err));
}

bool global_context::has_errors(void) const
{
	return !this->_errors.empty();
}

void global_context::clear_errors(void)
{
	this->_errors.clear();
}

void global_context::report_ambiguous_id_error(bz::string_view scope, lex::token_pos id)
{
	auto &func_sets = this->_decls[scope].func_sets;

	auto set = std::find_if(
		func_sets.begin(), func_sets.end(),
		[id = id->value](auto const &set) {
			return id == set.id;
		}
	);
	bz::printf("{}: {}\n", id->src_pos.line, id->value);
	assert(set != func_sets.end());

	bz::vector<note> notes = {};
	for (auto &decl : set->func_decls)
	{
		notes.emplace_back(make_note(
			decl->identifier, "candidate:"
		));
	}

	this->_errors.emplace_back(make_error(id, "identifier is ambiguous", std::move(notes)));
}



auto global_context::add_global_declaration(bz::string_view scope, ast::declaration &decl)
	-> bz::result<int, error>
{
	switch (decl.kind())
	{
	case ast::declaration::index<ast::decl_variable>:
		return this->add_global_variable(scope, *decl.get<ast::decl_variable_ptr>());
	case ast::declaration::index<ast::decl_function>:
		return this->add_global_function(scope, *decl.get<ast::decl_function_ptr>());
	case ast::declaration::index<ast::decl_operator>:
		return this->add_global_operator(scope, *decl.get<ast::decl_operator_ptr>());
	case ast::declaration::index<ast::decl_struct>:
		return this->add_global_struct(scope, *decl.get<ast::decl_struct_ptr>());
	default:
		assert(false);
		return 1;
	}
}

auto global_context::add_global_variable(bz::string_view scope, ast::decl_variable &var_decl)
	-> bz::result<int, error>
{
	auto &var_decls = this->_decls[scope].var_decls;
	auto const it = std::find_if(
		var_decls.begin(), var_decls.end(),
		[id = var_decl.identifier->value](auto const &v) {
			return id == v->identifier->value;
		}
	);
	if (it != var_decls.end())
	{
		return ctx::make_error(
			var_decl.identifier,
			bz::format("variable '{}' has already been declared", (*it)->identifier->value),
			{ ctx::make_note((*it)->identifier, "previous declaration:") }
		);
	}
	else
	{
		var_decls.push_back(&var_decl);
		return 0;
	}
}

auto global_context::add_global_function(bz::string_view scope, ast::decl_function &func_decl)
	-> bz::result<int, error>
{
	auto &func_sets = this->_decls[scope].func_sets;
	auto set = std::find_if(
		func_sets.begin(), func_sets.end(),
		[id = func_decl.identifier->value](auto const &fn_set) {
			return id == fn_set.id;
		}
	);
	if (set == func_sets.end())
	{
		func_sets.push_back({ func_decl.identifier->value, { &func_decl } });
	}
	else
	{
		// TODO: we need to check for conflicts
		set->func_decls.push_back(&func_decl);
	}
	return 0;
}

auto global_context::add_global_operator(bz::string_view scope, ast::decl_operator &op_decl)
	-> bz::result<int, error>
{
	auto &op_sets = this->_decls[scope].op_sets;
	auto set = std::find_if(
		op_sets.begin(), op_sets.end(),
		[op = op_decl.op->kind](auto const &op_set) {
			return op == op_set.op;
		}
	);
	if (set == op_sets.end())
	{
		op_sets.push_back({ op_decl.op->kind, { &op_decl } });
	}
	else
	{
		// TODO: we need to check for conflicts
		set->op_decls.push_back(&op_decl);
	}
	return 0;
}

auto global_context::add_global_struct(bz::string_view scope, ast::decl_struct &struct_decl)
	-> bz::result<int, error>
{
	auto &struct_decls = this->_decls[scope].struct_decls;
	auto const it = std::find_if(
		struct_decls.begin(), struct_decls.end(),
		[id = struct_decl.identifier->value](auto const &s) {
			return id == s->identifier->value;
		}
	);
	if (it != struct_decls.end())
	{
		return ctx::make_error(
			struct_decl.identifier,
			bz::format("struct '{}' has already been declared", struct_decl.identifier->value),
			{ ctx::make_note((*it)->identifier, "previous declaration:") }
		);
	}
	else
	{
		struct_decls.push_back(&struct_decl);
		return 0;
	}
}


auto global_context::get_identifier_type(bz::string_view scope, lex::token_pos id)
	-> bz::result<ast::expression::expr_type_t, error>
{
	auto &var_decls = this->_decls[scope].var_decls;
	auto it = std::find_if(
		var_decls.begin(), var_decls.end(),
		[id = id->value](auto const &var) {
			return id == var->identifier->value;
		}
	);
	if (it != var_decls.end())
	{
		return ast::expression::expr_type_t{ ast::expression::lvalue, (*it)->var_type };
	}

	auto &func_sets = this->_decls[scope].func_sets;
	auto set = std::find_if(
		func_sets.begin(), func_sets.end(),
		[id = id->value](auto const &set) {
			return id == set.id;
		}
	);
	if (set == func_sets.end())
	{
		return ctx::make_error(id, "undeclared identifier");
	}
	else if (set->func_decls.size() == 1)
	{
		resolve_symbol(set->func_decls[0]->body, scope, *this);
		auto &ret_type = set->func_decls[0]->body.return_type;
		bz::vector<ast::typespec> param_types = {};
		for (auto &p : set->func_decls[0]->body.params)
		{
			param_types.emplace_back(p.var_type);
		}
		return ast::expression::expr_type_t{
			ast::expression::function_name,
			ast::make_ts_function(ret_type, std::move(param_types))
		};
	}
	else
	{
		return ast::expression::expr_type_t{ ast::expression::function_name, ast::typespec() };
	}
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

ast::type_info const *global_context::get_type_info(bz::string_view, bz::string_view id)
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
