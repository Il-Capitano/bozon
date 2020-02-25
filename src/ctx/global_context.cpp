#include "global_context.h"
#include "parser.h"

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
		ast::type_info{ ast::type_info::type_kind::void_,    "void",    0,  0, ast::type_info::built_in, {} },
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
	-> bz::result<ast::typespec, error>
{
	auto &var_decls = this->_decls[scope].var_decls;
	auto it = std::find_if(
		var_decls.begin(), var_decls.end(),
		[id = id->value](auto const &var) {
			return id == var->identifier->value;
		}
	);
	if (it == var_decls.end())
	{
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
			return ast::make_ts_function(ret_type, std::move(param_types));
		}
		else
		{
			return ast::typespec();
		}
	}
	else
	{
		return (*it)->var_type;
	}
}

static bool are_directly_matchable_types(
	ast::expression::expr_type_t const &from,
	ast::typespec const &to
)
{
	if (
		to.kind() == ast::typespec::index<ast::ts_reference>
		&& from.type_kind != ast::expression::expr_type_kind::lvalue
		&& from.type_kind != ast::expression::expr_type_kind::lvalue_reference
	)
	{
		return false;
	}

	ast::typespec const *to_it = [&]() -> ast::typespec const * {
		if (to.kind() == ast::typespec::index<ast::ts_reference>)
		{
			return &to.get<ast::ts_reference_ptr>()->base;
		}
		else if (to.kind() == ast::typespec::index<ast::ts_constant>)
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
			to_it->kind() == ast::typespec::index<ast::ts_base_type>
			&& from_it->kind() == ast::typespec::index<ast::ts_base_type>
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
		else if (to_it->kind() == ast::typespec::index<ast::ts_constant>)
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

static bool is_integer_kind(ast::type_info::type_kind kind)
{
	return kind >= ast::type_info::type_kind::int8_
		&& kind <= ast::type_info::type_kind::uint64_;
}

static bool is_unsigned_integer_kind(ast::type_info::type_kind kind)
{
	return kind >= ast::type_info::type_kind::uint8_
		&& kind <= ast::type_info::type_kind::uint64_;
}

static bool is_signed_integer_kind(ast::type_info::type_kind kind)
{
	return kind >= ast::type_info::type_kind::int8_
		&& kind <= ast::type_info::type_kind::int64_;
}

static bool is_floating_point_kind(ast::type_info::type_kind kind)
{
	return kind == ast::type_info::type_kind::float32_
		|| kind == ast::type_info::type_kind::float64_;
}

static bool is_arithmetic_kind(ast::type_info::type_kind kind)
{
	return kind >= ast::type_info::type_kind::int8_
		&& kind <= ast::type_info::type_kind::float64_;
}

static auto get_built_in_operation_type(ast::expr_unary_op const &unary_op)
	-> bz::result<ast::typespec, error>
{
	auto const get_underlying_type = [](ast::typespec const &ts) -> auto const &
	{
		switch (ts.kind())
		{
		case ast::typespec::index<ast::ts_constant>:
			return ts.get<ast::ts_constant_ptr>()->base;
		default:
			return ts;
		}
	};

	switch (unary_op.op->kind)
	{
	case lex::token::plus:
	{
		auto &t = get_underlying_type(unary_op.expr.expr_type.expr_type);
		if (t.kind() != ast::typespec::index<ast::ts_base_type>)
		{
			return ast::typespec();
		}
		auto const kind = t.get<ast::ts_base_type_ptr>()->info->kind;
		if (is_arithmetic_kind(kind))
		{
			return t;
		}
		else
		{
			return ast::typespec();
		}
	}
	case lex::token::minus:
	{
		auto &t = get_underlying_type(unary_op.expr.expr_type.expr_type);
		if (t.kind() != ast::typespec::index<ast::ts_base_type>)
		{
			return ast::typespec();
		}
		auto const kind = t.get<ast::ts_base_type_ptr>()->info->kind;
		if (is_signed_integer_kind(kind) || is_floating_point_kind(kind))
		{
			return t;
		}
		else
		{
			return ast::typespec();
		}
	}
	case lex::token::dereference:
	{
		auto &t = get_underlying_type(unary_op.expr.expr_type.expr_type);
		if (t.kind() != ast::typespec::index<ast::ts_pointer>)
		{
			return ast::typespec();
		}
		return ast::make_ts_reference(t.get<ast::ts_pointer_ptr>()->base);
	}
	case lex::token::ampersand:
		if (
			unary_op.expr.expr_type.type_kind == ast::expression::lvalue
			|| unary_op.expr.expr_type.type_kind == ast::expression::lvalue_reference
		)
		{
			return ast::make_ts_pointer(unary_op.expr.expr_type.expr_type);
		}
		else
		{
			return ctx::make_error(unary_op, "cannot take address of an rvalue");
		}
	case lex::token::bit_not:
	{
		auto &t = get_underlying_type(unary_op.expr.expr_type.expr_type);
		if (t.kind() != ast::typespec::index<ast::ts_base_type>)
		{
			return ast::typespec();
		}
		auto const kind = t.get<ast::ts_base_type_ptr>()->info->kind;
		if (is_unsigned_integer_kind(kind))
		{
			return t;
		}
		else
		{
			return ast::typespec();
		}
	}
	case lex::token::bool_not:
	{
		auto &t = get_underlying_type(unary_op.expr.expr_type.expr_type);
		if (t.kind() != ast::typespec::index<ast::ts_base_type>)
		{
			return ast::typespec();
		}
		auto const kind = t.get<ast::ts_base_type_ptr>()->info->kind;
		if (kind == ast::type_info::type_kind::bool_)
		{
			return t;
		}
		else
		{
			return ast::typespec();
		}
	}

	default:
		assert(false);
		return ast::typespec();
	}
}

auto global_context::get_operation_type(bz::string_view scope, ast::expr_unary_op const &unary_op)
	-> bz::result<ast::typespec, error>
{
	auto &op_sets = this->_decls[scope].op_sets;

	if (is_built_in_type(unary_op.expr.expr_type.expr_type))
	{
		auto type = get_built_in_operation_type(unary_op);
		if (type.has_error() || type.get_result().kind() != ast::typespec::null)
		{
			return type;
		}
	}

	auto const set = std::find_if(
		op_sets.begin(), op_sets.end(),
		[&](auto &set) {
			return set.op == unary_op.op->kind;
		}
	);

	if (set == op_sets.end())
	{
		return ctx::make_error(
			unary_op,
			bz::format(
				"undeclared unary operator {} with type '{}'",
				unary_op.op->value, unary_op.expr.expr_type.expr_type
			)
		);
	}

	for (auto op : set->op_decls)
	{
		if (op->params.size() != 1)
		{
			continue;
		}

		if (are_directly_matchable_types(unary_op.expr.expr_type, op->params[0].var_type))
		{
			return op->return_type;
		}
	}

	return ctx::make_error(
		unary_op,
		bz::format("undeclared unary operator {}", unary_op.op->value)
	);
}

auto global_context::get_operation_type(bz::string_view scope, ast::expr_binary_op const &binary_op)
	-> bz::result<ast::typespec, error>
{
	auto const get_undeclared_error = [&]()
	{
		if (binary_op.op->kind == lex::token::square_open)
		{
			return ctx::make_error(binary_op, "undeclared binary operator []");
		}
		else
		{
			return ctx::make_error(
				binary_op,
				bz::format("undeclared binary operator {}", binary_op.op->value)
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
			return op->return_type;
		}
	}

	return get_undeclared_error();
}

auto global_context::get_function_call_type(bz::string_view scope, ast::expr_function_call const &func_call)
	-> bz::result<ast::typespec, error>
{
	if (func_call.called.expr_type.type_kind == ast::expression::function_name)
	{
		if (func_call.called.expr_type.expr_type.kind() != ast::typespec::null)
		{
			auto &fn_t = *func_call.called.expr_type.expr_type.get<ast::ts_function_ptr>();
			auto &func_sets = this->_decls[scope].func_sets;
			auto original_decl_set = std::find_if(
				func_sets.begin(), func_sets.end(), [
					id = func_call.called.get<ast::expr_identifier_ptr>()->identifier->value
				](auto const &set) {
					return id == set.id;
				}
			);
			assert(original_decl_set != func_sets.end());
			assert(original_decl_set->func_decls.size() == 1);

			if (fn_t.argument_types.size() != func_call.params.size())
			{
				return ctx::make_error(
					func_call,
					bz::format(
						"expected {} arguments for call to '{}', but was given {}",
						fn_t.argument_types.size(), original_decl_set->id, func_call.params.size()
					),
					{
						ctx::make_note(
							original_decl_set->func_decls[0]->identifier,
							bz::format("see declaration of '{}':", original_decl_set->id)
						)
					}
				);
			}

			resolve_symbol(*original_decl_set->func_decls[0], scope, *this);
			auto types_it = fn_t.argument_types.begin();
			auto call_it  = func_call.params.begin();
			auto const types_end = fn_t.argument_types.end();
//			auto const call_end  = func_call.params.end();
			for (; types_it != types_end; ++call_it, ++types_it)
			{
				if (!are_directly_matchable_types(call_it->expr_type, *types_it))
				{
					return ctx::make_error(
						*call_it,
						bz::format(
							"unable to convert argument {} from '{}' to '{}'",
							(call_it - func_call.params.begin()) + 1,
							call_it->expr_type.expr_type, *types_it
						)
					);
				}
			}
			return fn_t.return_type;
		}
		// call to an overload set with more than one members
		else
		{
			assert(false);
			return ast::typespec();
		}
	}
	else
	{
		// function call operator
		assert(false);
		return ast::typespec();
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
