#include "global_context.h"
#include "parser.h"
#include "built_in_operators.h"

namespace ctx
{

static std::list<ast::type_info> get_default_types(void)
{
	return {
		ast::type_info{ ast::type_info::type_kind::int8_,    "int8",    1,  1, ast::type_info::built_in, {} },
		ast::type_info{ ast::type_info::type_kind::int16_,   "int16",   2,  2, ast::type_info::built_in, {} },
		ast::type_info{ ast::type_info::type_kind::int32_,   "int32",   4,  4, ast::type_info::built_in, {} },
		ast::type_info{ ast::type_info::type_kind::int64_,   "int64",   8,  8, ast::type_info::built_in, {} },
		ast::type_info{ ast::type_info::type_kind::uint8_,   "uint8",   1,  1, ast::type_info::built_in, {} },
		ast::type_info{ ast::type_info::type_kind::uint16_,  "uint16",  2,  2, ast::type_info::built_in, {} },
		ast::type_info{ ast::type_info::type_kind::uint32_,  "uint32",  4,  4, ast::type_info::built_in, {} },
		ast::type_info{ ast::type_info::type_kind::uint64_,  "uint64",  8,  8, ast::type_info::built_in, {} },
		ast::type_info{ ast::type_info::type_kind::float32_, "float32", 4,  4, ast::type_info::built_in, {} },
		ast::type_info{ ast::type_info::type_kind::float64_, "float64", 8,  8, ast::type_info::built_in, {} },
		ast::type_info{ ast::type_info::type_kind::char_,    "char",    4,  4, ast::type_info::built_in, {} },
		ast::type_info{ ast::type_info::type_kind::str_,     "str",     16, 8, ast::type_info::built_in, {} },
		ast::type_info{ ast::type_info::type_kind::bool_,    "bool",    1,  1, ast::type_info::built_in, {} },
		ast::type_info{ ast::type_info::type_kind::null_t_,  "null_t",  0,  0, ast::type_info::built_in, {} },
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
		resolve_symbol(*set->func_decls[0], scope, *this);
		auto &ret_type = set->func_decls[0]->return_type;
		bz::vector<ast::typespec> param_types = {};
		for (auto &p : set->func_decls[0]->params)
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

static bool are_directly_matchable_types(
	ast::expression::expr_type_t const &from,
	ast::typespec const &to
)
{
	if (
		to.is<ast::ts_reference>()
		&& from.type_kind != ast::expression::expr_type_kind::lvalue
		&& from.type_kind != ast::expression::expr_type_kind::lvalue_reference
	)
	{
		return false;
	}

	ast::typespec const *to_it = [&]() -> ast::typespec const * {
		if (to.is<ast::ts_reference>())
		{
			return &to.get<ast::ts_reference_ptr>()->base;
		}
		else if (to.is<ast::ts_constant>())
		{
			return &to.get<ast::ts_constant_ptr>()->base;
		}
		else
		{
			return &to;
		}
	}();

	auto from_it = &from.expr_type;

	auto const advance = [](ast::typespec const *&it)
	{
		switch (it->kind())
		{
		case ast::typespec::index<ast::ts_pointer>:
			it = &it->get<ast::ts_pointer_ptr>()->base;
			break;
		case ast::typespec::index<ast::ts_constant>:
			it = &it->get<ast::ts_constant_ptr>()->base;
			break;
		default:
			assert(false);
			break;
		}
	};

	while (true)
	{
		if (
			to_it->is<ast::ts_base_type>()
			&& from_it->is<ast::ts_base_type>()
		)
		{
			return to_it->get<ast::ts_base_type_ptr>()->info
				== from_it->get<ast::ts_base_type_ptr>()->info;
		}
		else if (to_it->kind() == from_it->kind())
		{
			advance(to_it);
			advance(from_it);
		}
		else if (to_it->is<ast::ts_constant>())
		{
			advance(to_it);
		}
		else
		{
			return false;
		}
	}

	return false;
}

static bool is_built_in_type(ast::typespec const &ts)
{
	switch (ts.kind())
	{
	case ast::typespec::index<ast::ts_constant>:
		return is_built_in_type(ts.get<ast::ts_constant_ptr>()->base);
	case ast::typespec::index<ast::ts_base_type>:
	{
		auto &base = *ts.get<ast::ts_base_type_ptr>();
		return (base.info->flags & ast::type_info::built_in) != 0;
	}
	case ast::typespec::index<ast::ts_pointer>:
	case ast::typespec::index<ast::ts_function>:
	case ast::typespec::index<ast::ts_tuple>:
		return true;
	default:
		return false;
	}
}


auto global_context::get_operation_type(bz::string_view scope, ast::expr_unary_op const &unary_op)
	-> bz::result<ast::expression::expr_type_t, error>
{
	parse_context parse_ctx(scope, *this);
	if (!lex::is_overloadable_unary_operator(unary_op.op->kind))
	{
		auto type = get_non_overloadable_operation_type(
			unary_op.expr.expr_type, unary_op.op->kind,
			parse_ctx
		);
		if (type.has_error())
		{
			return make_error(unary_op, std::move(type.get_error()));
		}
		else
		{
			return std::move(type.get_result());
		}
	}

	if (is_built_in_type(ast::remove_const(unary_op.expr.expr_type.expr_type)))
	{
		auto type = get_built_in_operation_type(
			unary_op.expr.expr_type, unary_op.op->kind,
			parse_ctx
		);
		if (type.expr_type.kind() != ast::typespec::null)
		{
			return type;
		}
	}

	auto const get_undeclared_error = [&]()
	{
		return ctx::make_error(
			unary_op,
			bz::format(
				"undeclared unary operator {} with type '{}'",
				unary_op.op->value,
				unary_op.expr.expr_type.expr_type
			)
		);
	};

	auto &op_sets = this->_decls[scope].op_sets;
	auto const set = std::find_if(
		op_sets.begin(), op_sets.end(),
		[&](auto &set) {
			return set.op == unary_op.op->kind;
		}
	);

	if (set == op_sets.end())
	{
		return get_undeclared_error();
	}

	for (auto op : set->op_decls)
	{
		if (op->params.size() != 1)
		{
			continue;
		}

		if (are_directly_matchable_types(unary_op.expr.expr_type, op->params[0].var_type))
		{
			auto &t = op->return_type;
			if (t.is<ast::ts_reference>())
			{
				return ast::expression::expr_type_t{
					ast::expression::lvalue_reference,
					t.get<ast::ts_reference_ptr>()->base
				};
			}
			else
			{
				return ast::expression::expr_type_t{ ast::expression::rvalue, t };
			}
		}
	}

	return get_undeclared_error();
}

auto global_context::get_operation_type(bz::string_view scope, ast::expr_binary_op const &binary_op)
	-> bz::result<ast::expression::expr_type_t, error>
{
	parse_context parse_ctx(scope, *this);
	if (!lex::is_overloadable_binary_operator(binary_op.op->kind))
	{
		assert(false);
	}

	if (
		is_built_in_type(ast::remove_const(binary_op.lhs.expr_type.expr_type))
		&& is_built_in_type(ast::remove_const(binary_op.rhs.expr_type.expr_type))
	)
	{
		auto type = get_built_in_operation_type(
			binary_op.lhs.expr_type, binary_op.rhs.expr_type,
			binary_op.op->kind,
			parse_ctx
		);
		if (type.expr_type.kind() != ast::typespec::null)
		{
			return type;
		}
	}

	auto const get_undeclared_error = [&]()
	{
		if (binary_op.op->kind == lex::token::square_open)
		{
			return ctx::make_error(
				binary_op,
				bz::format(
					"undeclared binary operator [] with types '{}' and '{}'",
					binary_op.lhs.expr_type.expr_type, binary_op.rhs.expr_type.expr_type
				)
			);
		}
		else
		{
			return ctx::make_error(
				binary_op,
				bz::format(
					"undeclared binary operator {} with types '{}' and '{}'",
					binary_op.op->value,
					binary_op.lhs.expr_type.expr_type, binary_op.rhs.expr_type.expr_type
				)
			);
		}
	};

	auto &op_sets = this->_decls[scope].op_sets;
	auto const set = std::find_if(
		op_sets.begin(),
		op_sets.end(),
		[&](auto &set) {
			return set.op == binary_op.op->kind;
		}
	);

	if (set == op_sets.end())
	{
		return get_undeclared_error();
	}

	for (auto op : set->op_decls)
	{
		if (op->params.size() != 2)
		{
			continue;
		}

		if (
			are_directly_matchable_types(binary_op.lhs.expr_type, op->params[0].var_type)
			&& are_directly_matchable_types(binary_op.rhs.expr_type, op->params[1].var_type)
		)
		{
			auto &t = op->return_type;
			if (t.is<ast::ts_reference>())
			{
				return ast::expression::expr_type_t{
					ast::expression::lvalue_reference,
					t.get<ast::ts_reference_ptr>()->base
				};
			}
			else
			{
				return ast::expression::expr_type_t{ ast::expression::rvalue, t };
			}
		}
	}

	return get_undeclared_error();
}

struct match_level
{
	int min;
	int sum;
};
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

static int get_type_match_level(
	ast::typespec const &dest,
	ast::expression::expr_type_t const &src
)
{
	int result = 0;

	auto const *dest_it = dest.is<ast::ts_constant>()
		? &dest.get<ast::ts_constant_ptr>()->base
		: &dest;
	auto const *src_it = &src.expr_type;

	if (!dest_it->is<ast::ts_reference>())
	{
		++result;
	}

	if (dest_it->is<ast::ts_base_type>())
	{
		// TODO: use is_convertible here...
		if (
			src_it->is<ast::ts_base_type>()
			&& src_it->get<ast::ts_base_type_ptr>()->info == dest_it->get<ast::ts_base_type_ptr>()->info
		)
		{
			return result;
		}
		else
		{
			return -1;
		}
	}
	else if (
		dest_it->is<ast::ts_reference>()
		|| dest_it->is<ast::ts_pointer>()
	)
	{
		if (
			dest_it->is<ast::ts_reference>()
			&& src.type_kind != ast::expression::lvalue
			&& src.type_kind != ast::expression::lvalue_reference
		)
		{
			return -1;
		}

		dest_it = dest_it->is<ast::ts_reference>()
			? &dest_it->get<ast::ts_reference_ptr>()->base
			: &dest_it->get<ast::ts_pointer_ptr>()->base;
		if (dest_it->is<ast::ts_pointer>() && src_it->is<ast::ts_constant>())
		{
			src_it = &src_it->get<ast::ts_constant_ptr>()->base;
		}
		if (!src_it->is<ast::ts_pointer>())
		{
			return -1;
		}
		src_it = &src_it->get<ast::ts_pointer_ptr>()->base;

		if (!dest_it->is<ast::ts_constant>())
		{
			if (src_it->is<ast::ts_constant>())
			{
				return -1;
			}
			else
			{
				return *dest_it == *src_it ? result : -1;
			}
		}
		else if (!src_it->is<ast::ts_constant>())
		{
			++result;
		}
	}

	// we can only get here if the dest type is &const T or *const T
	auto const advance = [](ast::typespec const *&ts)
	{
		switch (ts->kind())
		{
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

	while (true)
	{
		if (dest_it->is<ast::ts_base_type>())
		{
			if (
				src_it->is<ast::ts_base_type>()
				&& src_it->get<ast::ts_base_type_ptr>()->info == dest_it->get<ast::ts_base_type_ptr>()->info
			)
			{
				return result;
			}
			else
			{
				return -1;
			}
		}
		else if (dest_it->kind() == src_it->kind())
		{
			advance(dest_it);
			advance(src_it);
		}
		else if (dest_it->is<ast::ts_constant>())
		{
			++result;
			advance(dest_it);
		}
		else
		{
			return -1;
		}
	}
}

static match_level get_function_call_match_level(
	ast::decl_function *func,
	ast::expr_function_call const &func_call
)
{
	if (func->params.size() != func_call.params.size())
	{
		return { -1, -1 };
	}

	match_level result = { std::numeric_limits<int>::max(), 0 };

	auto const add_to_result = [&](int match)
	{
		if (result.min == -1)
		{
			return;
		}
		else if (match == -1)
		{
			result = { -1, -1 };
		}
		else
		{
			if (match < result.min)
			{
				result.min = match;
			}
			result.sum += match;
		}
	};

	auto params_it = func->params.begin();
	auto call_it  = func_call.params.begin();
	auto const types_end = func->params.end();
	for (; params_it != types_end; ++call_it, ++params_it)
	{
		add_to_result(get_type_match_level(params_it->var_type, call_it->expr_type));
	}
	return result;
}

static error get_bad_call_error(
	ast::decl_function *func,
	ast::expr_function_call const &func_call
)
{
	if (func->params.size() != func_call.params.size())
	{
		return make_error(
			func_call,
			bz::format(
				"expected {} {} for call to '{}', but was given {}",
				func->params.size(), func->params.size() == 1 ? "argument" : "arguments",
				func->identifier->value, func_call.params.size()
			),
			{
				make_note(
					func->identifier,
					bz::format("see declaration of '{}':", func->identifier->value)
				)
			}
		);
	}

	auto params_it = func->params.begin();
	auto call_it  = func_call.params.begin();
	auto const types_end = func->params.end();
//	auto const call_end  = func_call.params.end();
	for (; params_it != types_end; ++call_it, ++params_it)
	{
		if (get_type_match_level(params_it->var_type, call_it->expr_type) == -1)
		{
			return make_error(
				*call_it,
				bz::format(
					"unable to convert argument {} from '{}' to '{}'",
					(call_it - func_call.params.begin()) + 1,
					call_it->expr_type.expr_type, params_it->var_type
				)
			);
		}
	}

	assert(false);
	return make_error(func->identifier, "");
}

static bz::vector<note> get_bad_call_candidate_notes(
	ast::decl_function *func,
	ast::expr_function_call const &func_call
)
{
	if (func->params.size() != func_call.params.size())
	{
		return {
			make_note(
				func_call,
				bz::format(
					"expected {} {} for call to '{}', but was given {}",
					func->params.size(), func->params.size() == 1 ? "argument" : "arguments",
					func->identifier->value, func_call.params.size()
				)
			),
			make_note(
				func->identifier,
				bz::format("see declaration of '{}':", func->identifier->value)
			)
		};
	}

	auto params_it = func->params.begin();
	auto call_it  = func_call.params.begin();
	auto const types_end = func->params.end();
//	auto const call_end  = func_call.params.end();
	for (; params_it != types_end; ++call_it, ++params_it)
	{
		if (!are_directly_matchable_types(call_it->expr_type, params_it->var_type))
		{
			return { make_note(
				*call_it,
				bz::format(
					"unable to convert argument {} from '{}' to '{}'",
					(call_it - func_call.params.begin()) + 1,
					call_it->expr_type.expr_type, params_it->var_type
				)
			) };
		}
	}

	assert(false);
	return {};
}

auto global_context::get_function_call_type(bz::string_view scope, ast::expr_function_call const &func_call)
	-> bz::result<ast::expression::expr_type_t, error>
{
	auto const get_return_value = [](ast::decl_function *func)
	{
		auto &t = func->return_type;
		if (t.is<ast::ts_reference>())
		{
			return ast::expression::expr_type_t{
				ast::expression::lvalue_reference,
				t.get<ast::ts_reference_ptr>()->base
			};
		}
		else
		{
			return ast::expression::expr_type_t{ ast::expression::rvalue, t };
		}
	};

	if (func_call.called.expr_type.type_kind == ast::expression::function_name)
	{
		assert(func_call.called.is<ast::expr_identifier>());
		auto &func_sets = this->_decls[scope].func_sets;
		auto const set = std::find_if(
			func_sets.begin(), func_sets.end(), [
				id = func_call.called.get<ast::expr_identifier_ptr>()->identifier->value
			](auto const &set) {
				return id == set.id;
			}
		);
		assert(set != func_sets.end());

		if (func_call.called.expr_type.expr_type.kind() != ast::typespec::null)
		{
			assert(set->func_decls.size() == 1);
			resolve_symbol(*set->func_decls[0], scope, *this);
			auto const func = set->func_decls[0];

			if (get_function_call_match_level(func, func_call).sum == -1)
			{
				return get_bad_call_error(func, func_call);
			}
			else
			{
				return get_return_value(func);
			}
		}
		// call to an overload set with more than one members
		else
		{
			assert(set->func_decls.size() > 1);

			bz::vector<std::pair<match_level, ast::decl_function *>> possible_funcs = {};
			for (auto func : set->func_decls)
			{
				if (func->params.size() != func_call.params.size())
				{
					continue;
				}

				resolve_symbol(*func, scope, *this);
				auto match_level = get_function_call_match_level(func, func_call);
				if (match_level.sum != -1)
				{
					possible_funcs.push_back({ match_level, func });
				}
			}

			if (possible_funcs.size() == 0)
			{
				return make_error(func_call, "couldn't call any of the functions in the overload set");
			}
			else
			{
				auto const min_match_it = std::min_element(
					possible_funcs.begin(), possible_funcs.end(),
					[](auto const &lhs, auto const &rhs) {
						return lhs.first.min < rhs.first.min
							|| (lhs.first.min == rhs.first.min && lhs.first.sum < rhs.first.sum);
					}
				);
				if (std::count_if(
					possible_funcs.begin(), possible_funcs.end(), [&](auto const &p) {
						return p.first.min == min_match_it->first.min
							&& p.first.sum == min_match_it->first.sum;
					}
				) == 1)
				{
					return get_return_value(min_match_it->second);
				}
				else
				{
					return make_error(func_call, "function call is ambiguous");
				}
			}
		}
	}
	else
	{
		// function call operator
		bz::printf("{}\n", func_call.called.get_tokens_pivot()->src_pos.line);
		bz::printf("{}\n", func_call.called.expr_type.type_kind == ast::expression::function_name);
		assert(false);
		return ast::expression::expr_type_t{ ast::expression::rvalue, ast::typespec() };
	}
}

bool global_context::is_convertible(
	bz::string_view /*scope*/,
	ast::expression::expr_type_t const &from,
	ast::typespec const &to
)
{
	return are_directly_matchable_types(from, to);
}


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
