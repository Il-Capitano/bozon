#include "parse_context.h"
#include "global_context.h"
#include "built_in_operators.h"
#include "parser.h"

namespace ctx
{

void parse_context::report_error(lex::token_pos it) const
{
	this->global_ctx.report_error(ctx::make_error(it));
}

void parse_context::report_error(
	lex::token_pos it,
	bz::string message, bz::vector<ctx::note> notes
) const
{
	this->global_ctx.report_error(ctx::make_error(
		it, std::move(message), std::move(notes)
	));
}

void parse_context::report_error(
	lex::token_pos begin, lex::token_pos pivot, lex::token_pos end,
	bz::string message, bz::vector<ctx::note> notes
) const
{
	this->global_ctx.report_error(ctx::make_error(
		begin, pivot, end, std::move(message), std::move(notes)
	));
}

bool parse_context::has_errors(void) const
{
	return this->global_ctx.has_errors();
}

lex::token_pos parse_context::assert_token(lex::token_pos &stream, uint32_t kind) const
{
	if (stream->kind != kind)
	{
		this->global_ctx.report_error(ctx::make_error(
			stream,
			stream->kind == lex::token::eof
			? bz::format("expected {} before end-of-file", lex::get_token_name_for_message(kind))
			: bz::format("expected {}", lex::get_token_name_for_message(kind))
		));
		return stream;
	}
	else
	{
		auto t = stream;
		++stream;
		return t;
	}
}

lex::token_pos parse_context::assert_token(lex::token_pos &stream, uint32_t kind1, uint32_t kind2) const
{
	if (stream->kind != kind1 && stream->kind != kind2)
	{
		this->global_ctx.report_error(ctx::make_error(
			stream,
			stream->kind == lex::token::eof
			? bz::format(
				"expected {} or {} before end-of-file",
				lex::get_token_name_for_message(kind1), lex::get_token_name_for_message(kind2)
			)
			: bz::format(
				"expected {} or {}",
				lex::get_token_name_for_message(kind1), lex::get_token_name_for_message(kind2)
			)
		));
		return stream;
	}
	else
	{
		auto t = stream;
		++stream;
		return t;
	}
}

void parse_context::report_ambiguous_id_error(lex::token_pos id) const
{
	this->global_ctx.report_ambiguous_id_error(this->scope, id);
}


void parse_context::add_scope(void)
{
	this->scope_variables.push_back({});
}

void parse_context::remove_scope(void)
{
	this->scope_variables.pop_back();
}


void parse_context::add_global_declaration(ast::declaration &decl)
{
	auto res = this->global_ctx.add_global_declaration(this->scope, decl);
	if (res.has_error())
	{
		this->global_ctx.report_error(std::move(res.get_error()));
	}
}

void parse_context::add_global_variable(ast::decl_variable &var_decl)
{
	auto res = this->global_ctx.add_global_variable(this->scope, var_decl);
	if (res.has_error())
	{
		this->global_ctx.report_error(std::move(res.get_error()));
	}
}

void parse_context::add_global_function(ast::decl_function &func_decl)
{
	auto res = this->global_ctx.add_global_function(this->scope, func_decl);
	if (res.has_error())
	{
		this->global_ctx.report_error(std::move(res.get_error()));
	}
}

void parse_context::add_global_operator(ast::decl_operator &op_decl)
{
	auto res = this->global_ctx.add_global_operator(this->scope, op_decl);
	if (res.has_error())
	{
		this->global_ctx.report_error(std::move(res.get_error()));
	}
}

void parse_context::add_global_struct(ast::decl_struct &struct_decl)
{
	auto res = this->global_ctx.add_global_struct(this->scope, struct_decl);
	if (res.has_error())
	{
		this->global_ctx.report_error(std::move(res.get_error()));
	}
}


void parse_context::add_local_variable(ast::decl_variable const &var_decl)
{
	assert(this->scope_variables.size() != 0);
	this->scope_variables.back().push_back(&var_decl);
}


auto parse_context::get_identifier_decl(lex::token_pos id) const
	-> bz::variant<ast::decl_variable const *, ast::decl_function const *>
{
	// we go in reverse through the scopes and the variables
	// in case there's shadowing
	for (
		auto scope = this->scope_variables.rbegin();
		scope != this->scope_variables.rend();
		++scope
	)
	{
		auto const var = std::find_if(
			scope->rbegin(), scope->rend(),
			[&](auto const &var) {
				return var->identifier->value == id->value;
			}
		);
		if (var != scope->rend())
		{
			return *var;
		}
	}
	// TODO: get declarations for function names
	return {};
}


ast::expression::expr_type_t parse_context::get_identifier_type(lex::token_pos id) const
{
	// we go in reverse through the scopes and the variables
	// in case there's shadowing
	for (
		auto scope = this->scope_variables.rbegin();
		scope != this->scope_variables.rend();
		++scope
	)
	{
		auto const var = std::find_if(
			scope->rbegin(), scope->rend(),
			[&](auto const &var) {
				return var->identifier->value == id->value;
			}
		);
		if (var != scope->rend())
		{
			auto const var_ptr = *var;
			if (var_ptr->var_type.is<ast::ts_reference>())
			{
				return { ast::expression::lvalue_reference, var_ptr->var_type.get<ast::ts_reference_ptr>()->base };
			}
			else
			{
				return { ast::expression::lvalue, var_ptr->var_type };
			}
		}
	}

	auto res = this->global_ctx.get_identifier_type(this->scope, id);
	if (res.has_error())
	{
		this->global_ctx.report_error(std::move(res.get_error()));
		return { ast::expression::rvalue, ast::typespec() };
	}
	else
	{
		return std::move(res.get_result());
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

struct match_level
{
	int min;
	int sum;
};

static match_level get_function_call_match_level(
	ast::decl_function *func,
	ast::expr_function_call const &func_call
)
{
	if (func->body.params.size() != func_call.params.size())
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

	auto params_it = func->body.params.begin();
	auto call_it  = func_call.params.begin();
	auto const types_end = func->body.params.end();
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
	if (func->body.params.size() != func_call.params.size())
	{
		return make_error(
			func_call,
			bz::format(
				"expected {} {} for call to '{}', but was given {}",
				func->body.params.size(), func->body.params.size() == 1 ? "argument" : "arguments",
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

	auto params_it = func->body.params.begin();
	auto call_it  = func_call.params.begin();
	auto const types_end = func->body.params.end();
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


auto parse_context::get_operation_body_and_type(ast::expr_unary_op const &unary_op)
	-> std::pair<ast::function_body *, ast::expression::expr_type_t>
{
	std::pair<ast::function_body *, ast::expression::expr_type_t> error_result = {
		nullptr, { ast::expression::rvalue, ast::typespec() }
	};

	if (!lex::is_overloadable_unary_operator(unary_op.op->kind))
	{
		auto type = get_non_overloadable_operation_type(
			unary_op.expr.expr_type, unary_op.op->kind,
			*this
		);
		if (type.has_error())
		{
			this->report_error(unary_op, std::move(type.get_error()));
			return error_result;
		}
		else
		{
			return { nullptr, std::move(type.get_result()) };
		}
	}

	if (is_built_in_type(ast::remove_const(unary_op.expr.expr_type.expr_type)))
	{
		auto type = get_built_in_operation_type(
			unary_op.expr.expr_type, unary_op.op->kind,
			*this
		);
		if (type.expr_type.kind() != ast::typespec::null)
		{
			return { nullptr, type };
		}
	}

	auto const report_undeclared_error = [&, this]()
	{
		this->report_error(
			unary_op,
			bz::format(
				"undeclared unary operator {} with type '{}'",
				unary_op.op->value,
				unary_op.expr.expr_type.expr_type
			)
		);
	};

	auto &op_sets = this->global_ctx.get_local_decls(this->scope).op_sets;
	auto const set = std::find_if(
		op_sets.begin(), op_sets.end(),
		[&](auto &set) {
			return set.op == unary_op.op->kind;
		}
	);

	if (set == op_sets.end())
	{
		report_undeclared_error();
		return error_result;
	}

	for (auto op : set->op_decls)
	{
		if (op->body.params.size() != 1)
		{
			continue;
		}

		if (are_directly_matchable_types(unary_op.expr.expr_type, op->body.params[0].var_type))
		{
			auto &t = op->body.return_type;
			if (t.is<ast::ts_reference>())
			{
				return {
					&op->body,
					{
						ast::expression::lvalue_reference,
						t.get<ast::ts_reference_ptr>()->base
					}
				};
			}
			else
			{
				return { &op->body, { ast::expression::rvalue, t } };
			}
		}
	}

	report_undeclared_error();
	return error_result;
}

auto parse_context::get_operation_body_and_type(ast::expr_binary_op const &binary_op)
	-> std::pair<ast::function_body *, ast::expression::expr_type_t>
{
	std::pair<ast::function_body *, ast::expression::expr_type_t> error_result = {
		nullptr, { ast::expression::rvalue, ast::typespec() }
	};

	if (!lex::is_overloadable_binary_operator(binary_op.op->kind))
	{
		auto type = get_non_overloadable_operation_type(
			binary_op.lhs.expr_type, binary_op.rhs.expr_type,
			binary_op.op->kind,
			*this
		);
		if (type.has_error())
		{
			this->report_error(binary_op, std::move(type.get_error()));
			return error_result;
		}
		else
		{
			return { nullptr, std::move(type.get_result()) };
		}
	}

	if (
		is_built_in_type(ast::remove_const(binary_op.lhs.expr_type.expr_type))
		&& is_built_in_type(ast::remove_const(binary_op.rhs.expr_type.expr_type))
	)
	{
		auto type = get_built_in_operation_type(
			binary_op.lhs.expr_type, binary_op.rhs.expr_type,
			binary_op.op->kind,
			*this
		);
		if (type.expr_type.kind() != ast::typespec::null)
		{
			return { nullptr, type };
		}
	}

	auto const report_undeclared_error = [&, this]()
	{
		if (binary_op.op->kind == lex::token::square_open)
		{
			this->report_error(
				binary_op,
				bz::format(
					"undeclared binary operator [] with types '{}' and '{}'",
					binary_op.lhs.expr_type.expr_type, binary_op.rhs.expr_type.expr_type
				)
			);
		}
		else
		{
			this->report_error(
				binary_op,
				bz::format(
					"undeclared binary operator {} with types '{}' and '{}'",
					binary_op.op->value,
					binary_op.lhs.expr_type.expr_type, binary_op.rhs.expr_type.expr_type
				)
			);
		}
	};

	auto &op_sets = this->global_ctx.get_local_decls(this->scope).op_sets;
	auto const set = std::find_if(
		op_sets.begin(),
		op_sets.end(),
		[&](auto &set) {
			return set.op == binary_op.op->kind;
		}
	);

	if (set == op_sets.end())
	{
		report_undeclared_error();
		return error_result;
	}

	for (auto op : set->op_decls)
	{
		if (op->body.params.size() != 2)
		{
			continue;
		}

		if (
			are_directly_matchable_types(binary_op.lhs.expr_type, op->body.params[0].var_type)
			&& are_directly_matchable_types(binary_op.rhs.expr_type, op->body.params[1].var_type)
		)
		{
			auto &t = op->body.return_type;
			if (t.is<ast::ts_reference>())
			{
				return {
					&op->body,
					{
						ast::expression::lvalue_reference,
						t.get<ast::ts_reference_ptr>()->base
					}
				};
			}
			else
			{
				return { &op->body, { ast::expression::rvalue, t } };
			}
		}
	}

	report_undeclared_error();
	return error_result;
}

auto parse_context::get_function_call_body_and_type(ast::expr_function_call const &func_call)
	-> std::pair<ast::function_body *, ast::expression::expr_type_t>
{
	std::pair<ast::function_body *, ast::expression::expr_type_t> error_result = {
		nullptr, { ast::expression::rvalue, ast::typespec() }
	};

	auto const get_return_type = [](ast::decl_function *func)
	{
		auto &t = func->body.return_type;
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
		auto &func_sets = this->global_ctx.get_local_decls(this->scope).func_sets;
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
			resolve_symbol(set->func_decls[0]->body, this->scope, this->global_ctx);
			auto const func = set->func_decls[0];

			if (get_function_call_match_level(func, func_call).sum == -1)
			{
				this->global_ctx.report_error(get_bad_call_error(func, func_call));
				return error_result;
			}
			else
			{
				return { &func->body, get_return_type(func) };
			}
		}
		// call to an overload set with more than one members
		else
		{
			assert(set->func_decls.size() > 1);

			bz::vector<std::pair<match_level, ast::decl_function *>> possible_funcs = {};
			for (auto func : set->func_decls)
			{
				if (func->body.params.size() != func_call.params.size())
				{
					continue;
				}

				resolve_symbol(func->body, scope, this->global_ctx);
				auto match_level = get_function_call_match_level(func, func_call);
				if (match_level.sum != -1)
				{
					possible_funcs.push_back({ match_level, func });
				}
			}

			if (possible_funcs.size() == 0)
			{
				this->report_error(func_call, "couldn't call any of the functions in the overload set");
				return error_result;
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
					return { &min_match_it->second->body, get_return_type(min_match_it->second) };
				}
				else
				{
					this->report_error(func_call, "function call is ambiguous");
					return error_result;
				}
			}
		}
	}
	else
	{
		// function call operator
		assert(false);
		return error_result;
	}
}

/*
ast::expression::expr_type_t parse_context::get_operation_type(ast::expr_unary_op const &unary_op)
{
	if (!lex::is_overloadable_unary_operator(unary_op.op->kind))
	{
		auto type = get_non_overloadable_operation_type(
			unary_op.expr.expr_type, unary_op.op->kind,
			*this
		);
		if (type.has_error())
		{
			this->report_error(unary_op, std::move(type.get_error()));
			return { ast::expression::rvalue, ast::typespec() };
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
			*this
		);
		if (type.expr_type.kind() != ast::typespec::null)
		{
			return type;
		}
	}

	auto const report_undeclared_error = [&, this]()
	{
		this->report_error(
			unary_op,
			bz::format(
				"undeclared unary operator {} with type '{}'",
				unary_op.op->value,
				unary_op.expr.expr_type.expr_type
			)
		);
	};

	auto &op_sets = this->global_ctx.get_local_decls(this->scope).op_sets;
	auto const set = std::find_if(
		op_sets.begin(), op_sets.end(),
		[&](auto &set) {
			return set.op == unary_op.op->kind;
		}
	);

	if (set == op_sets.end())
	{
		report_undeclared_error();
		return { ast::expression::rvalue, ast::typespec() };
	}

	for (auto op : set->op_decls)
	{
		if (op->body.params.size() != 1)
		{
			continue;
		}

		if (are_directly_matchable_types(unary_op.expr.expr_type, op->body.params[0].var_type))
		{
			auto &t = op->body.return_type;
			if (t.is<ast::ts_reference>())
			{
				return {
					ast::expression::lvalue_reference,
					t.get<ast::ts_reference_ptr>()->base
				};
			}
			else
			{
				return { ast::expression::rvalue, t };
			}
		}
	}

	report_undeclared_error();
	return { ast::expression::rvalue, ast::typespec() };
}

ast::expression::expr_type_t parse_context::get_operation_type(ast::expr_binary_op const &binary_op)
{
	if (!lex::is_overloadable_binary_operator(binary_op.op->kind))
	{
		auto type = get_non_overloadable_operation_type(
			binary_op.lhs.expr_type, binary_op.rhs.expr_type,
			binary_op.op->kind,
			*this
		);
		if (type.has_error())
		{
			this->report_error(binary_op, std::move(type.get_error()));
			return { ast::expression::rvalue, ast::typespec() };
		}
		else
		{
			return std::move(type.get_result());
		}
	}

	if (
		is_built_in_type(ast::remove_const(binary_op.lhs.expr_type.expr_type))
		&& is_built_in_type(ast::remove_const(binary_op.rhs.expr_type.expr_type))
	)
	{
		auto type = get_built_in_operation_type(
			binary_op.lhs.expr_type, binary_op.rhs.expr_type,
			binary_op.op->kind,
			*this
		);
		if (type.expr_type.kind() != ast::typespec::null)
		{
			return type;
		}
	}

	auto const report_undeclared_error = [&, this]()
	{
		if (binary_op.op->kind == lex::token::square_open)
		{
			this->report_error(
				binary_op,
				bz::format(
					"undeclared binary operator [] with types '{}' and '{}'",
					binary_op.lhs.expr_type.expr_type, binary_op.rhs.expr_type.expr_type
				)
			);
		}
		else
		{
			this->report_error(
				binary_op,
				bz::format(
					"undeclared binary operator {} with types '{}' and '{}'",
					binary_op.op->value,
					binary_op.lhs.expr_type.expr_type, binary_op.rhs.expr_type.expr_type
				)
			);
		}
	};

	auto &op_sets = this->global_ctx.get_local_decls(this->scope).op_sets;
	auto const set = std::find_if(
		op_sets.begin(),
		op_sets.end(),
		[&](auto &set) {
			return set.op == binary_op.op->kind;
		}
	);

	if (set == op_sets.end())
	{
		report_undeclared_error();
		return { ast::expression::rvalue, ast::typespec() };
	}

	for (auto op : set->op_decls)
	{
		if (op->body.params.size() != 2)
		{
			continue;
		}

		if (
			are_directly_matchable_types(binary_op.lhs.expr_type, op->body.params[0].var_type)
			&& are_directly_matchable_types(binary_op.rhs.expr_type, op->body.params[1].var_type)
		)
		{
			auto &t = op->body.return_type;
			if (t.is<ast::ts_reference>())
			{
				return {
					ast::expression::lvalue_reference,
					t.get<ast::ts_reference_ptr>()->base
				};
			}
			else
			{
				return { ast::expression::rvalue, t };
			}
		}
	}

	report_undeclared_error();
	return { ast::expression::rvalue, ast::typespec() };
}



ast::expression::expr_type_t parse_context::get_function_call_type(ast::expr_function_call const &func_call) const
{
	auto res = this->global_ctx.get_function_call_type(this->scope, func_call);
	if (res.has_error())
	{
		this->global_ctx.report_error(std::move(res.get_error()));
		return { ast::expression::rvalue, ast::typespec() };
	}
	else
	{
		return std::move(res.get_result());
	}
}
*/

bool parse_context::is_convertible(ast::expression::expr_type_t const &from, ast::typespec const &to)
{
	return are_directly_matchable_types(from, to);
}

ast::type_info const *parse_context::get_type_info(bz::string_view id) const
{
	return this->global_ctx.get_type_info(this->scope, id);
}

} // namespace ctx
