#include "global_context.h"
#include "parser.h"

namespace ctx
{

static_assert(ast::type_info::int8_    == 0);
static_assert(ast::type_info::int16_   == 1);
static_assert(ast::type_info::int32_   == 2);
static_assert(ast::type_info::int64_   == 3);
static_assert(ast::type_info::uint8_   == 4);
static_assert(ast::type_info::uint16_  == 5);
static_assert(ast::type_info::uint32_  == 6);
static_assert(ast::type_info::uint64_  == 7);
static_assert(ast::type_info::float32_ == 8);
static_assert(ast::type_info::float64_ == 9);
static_assert(ast::type_info::char_    == 10);
static_assert(ast::type_info::str_     == 11);
static_assert(ast::type_info::bool_    == 12);
static_assert(ast::type_info::null_t_  == 13);


std::array<ast::type_info, ast::type_info::null_t_ + 1> get_default_type_infos(void)
{
	return {
		ast::type_info{ ast::type_info::int8_,    ast::type_info::default_built_in_flags, "int8",    {} },
		ast::type_info{ ast::type_info::int16_,   ast::type_info::default_built_in_flags, "int16",   {} },
		ast::type_info{ ast::type_info::int32_,   ast::type_info::default_built_in_flags, "int32",   {} },
		ast::type_info{ ast::type_info::int64_,   ast::type_info::default_built_in_flags, "int64",   {} },
		ast::type_info{ ast::type_info::uint8_,   ast::type_info::default_built_in_flags, "uint8",   {} },
		ast::type_info{ ast::type_info::uint16_,  ast::type_info::default_built_in_flags, "uint16",  {} },
		ast::type_info{ ast::type_info::uint32_,  ast::type_info::default_built_in_flags, "uint32",  {} },
		ast::type_info{ ast::type_info::uint64_,  ast::type_info::default_built_in_flags, "uint64",  {} },
		ast::type_info{ ast::type_info::float32_, ast::type_info::default_built_in_flags, "float32", {} },
		ast::type_info{ ast::type_info::float64_, ast::type_info::default_built_in_flags, "float64", {} },
		ast::type_info{ ast::type_info::char_,    ast::type_info::default_built_in_flags, "char",    {} },
		ast::type_info{ ast::type_info::str_,     ast::type_info::default_built_in_flags, "str",     {} },
		ast::type_info{ ast::type_info::bool_,    ast::type_info::default_built_in_flags, "bool",    {} },
		ast::type_info{ ast::type_info::null_t_,  ast::type_info::default_built_in_flags, "null_t",  {} },
	};
}

static auto default_type_infos = get_default_type_infos();

static bz::vector<type_info_with_name> get_default_types(void)
{
	using tiwn = type_info_with_name;
	return {
		tiwn{ "int8",    &default_type_infos[ast::type_info::int8_]    },
		tiwn{ "int16",   &default_type_infos[ast::type_info::int16_]   },
		tiwn{ "int32",   &default_type_infos[ast::type_info::int32_]   },
		tiwn{ "int64",   &default_type_infos[ast::type_info::int64_]   },
		tiwn{ "uint8",   &default_type_infos[ast::type_info::uint8_]   },
		tiwn{ "uint16",  &default_type_infos[ast::type_info::uint16_]  },
		tiwn{ "uint32",  &default_type_infos[ast::type_info::uint32_]  },
		tiwn{ "uint64",  &default_type_infos[ast::type_info::uint64_]  },
		tiwn{ "float32", &default_type_infos[ast::type_info::float32_] },
		tiwn{ "float64", &default_type_infos[ast::type_info::float64_] },
		tiwn{ "char",    &default_type_infos[ast::type_info::char_]    },
		tiwn{ "str",     &default_type_infos[ast::type_info::str_]     },
		tiwn{ "bool",    &default_type_infos[ast::type_info::bool_]    },
		tiwn{ "null_t",  &default_type_infos[ast::type_info::null_t_]  },
	};
}


global_context::global_context(void)
	: _export_decls{
		  {}, // var_decls
		  {}, // func_sets
		  {}, // op_sets
		  get_default_types() // type_infos
	  },
	  _compile_decls{},
	  _errors{}
{}


bool global_context::has_errors(void) const
{
	for (auto &err : this->_errors)
	{
		if (!err.is_warning)
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
		if (err.is_warning)
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
		if (!err.is_warning)
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
		if (err.is_warning)
		{
			++count;
		}
	}
	return count;
}

void global_context::add_export_declaration(ast::declaration &decl)
{
	switch (decl.kind())
	{
	case ast::declaration::index<ast::decl_variable>:
		this->add_export_variable(*decl.get<ast::decl_variable_ptr>());
		break;
	case ast::declaration::index<ast::decl_function>:
		this->add_export_function(*decl.get<ast::decl_function_ptr>());
		break;
	case ast::declaration::index<ast::decl_operator>:
		this->add_export_operator(*decl.get<ast::decl_operator_ptr>());
		break;
	case ast::declaration::index<ast::decl_struct>:
		this->add_export_struct(*decl.get<ast::decl_struct_ptr>());
		break;
	default:
		bz_assert(false);
		break;
	}
}

void global_context::add_export_variable(ast::decl_variable &var_decl)
{
	this->_export_decls.var_decls.push_back(&var_decl);
	this->_compile_decls.var_decls.push_back(&var_decl);
}

void global_context::add_compile_variable(ast::decl_variable &var_decl)
{
	this->_compile_decls.var_decls.push_back(&var_decl);
}

void global_context::add_export_function(ast::decl_function &func_decl)
{
	auto &func_sets = this->_export_decls.func_sets;
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
		// we don't check for conflicts just yet
		// these should be added after the first pass parsing stage
		// and resolved after, where redeclaration checks are made
		set->func_decls.push_back(&func_decl);
	}

	this->_compile_decls.func_decls.push_back(&func_decl);
}

void global_context::add_compile_function(ast::decl_function &func_decl)
{
	this->_compile_decls.func_decls.push_back(&func_decl);
}

void global_context::add_export_operator(ast::decl_operator &op_decl)
{
	auto &op_sets = this->_export_decls.op_sets;
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
		// we don't check for conflicts just yet
		// these should be added after the first pass parsing stage
		// and resolved after, where redeclaration checks are made
		set->op_decls.push_back(&op_decl);
	}

	this->_compile_decls.op_decls.push_back(&op_decl);
}

void global_context::add_compile_operator(ast::decl_operator &op_decl)
{
	this->_compile_decls.op_decls.push_back(&op_decl);
}

void global_context::add_export_struct(ast::decl_struct &struct_decl)
{
	this->_export_decls.type_infos.push_back({ struct_decl.identifier->value, &struct_decl.info });
	this->_compile_decls.type_infos.push_back(&struct_decl.info);
}

void global_context::add_compile_struct(ast::decl_struct &struct_decl)
{
	this->_compile_decls.type_infos.push_back(&struct_decl.info);
}

ast::type_info *global_context::get_base_type_info(uint32_t kind) const
{
	bz_assert(kind <= ast::type_info::null_t_);
	return &default_type_infos[kind];
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
			bz_assert(false);
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

} // namespace ctx
